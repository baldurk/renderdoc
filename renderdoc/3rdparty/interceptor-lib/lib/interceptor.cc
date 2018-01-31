/*
 * Copyright (C) 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "interceptor.h"

#include <sys/mman.h>
#include <unistd.h>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>

#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"

#include "code_generator.h"
#include "linker.h"
#include "memory_manager.h"
#include "target.h"

#if defined(__arm__)
#include "ARM/target_arm.h"
#elif defined(__arm64__) || defined(__aarch64__)
#include "AArch64/target_aarch64.h"
#elif defined(__i386__)
#include "X86/target_x86.h"
#endif

using namespace interceptor;

namespace interceptor {

class InterceptorImpl {
 public:
  InterceptorImpl();
  ~InterceptorImpl();

  Error InterceptFunction(void *old_function, void *new_function,
                          void **callback_function);

  Error InterceptFunction(const char *symbol_name, void *new_function,
                          void **callback_function);

  void *FindFunctionByName(const char *symbol_name);

 private:
  Error WriteMemory(void *target, const void *source, size_t num,
                    bool is_executable);

  Error GetTrampolineSize(const TrampolineConfig &config, void *old_function,
                          void *new_function, size_t &trampoline_size);

  Error InstallTrampoline(const TrampolineConfig &config, void *old_function,
                          void *new_function);

  Error RewriteInstructions(void *old_function, size_t rewrite_size,
                            std::unique_ptr<CodeGenerator> &codegen);

  Error CreateCompensationFunction(void *old_function, size_t rewrite_size,
                                   void **callback_function);

  Linker linker_;
  std::unique_ptr<Target> target_;
  std::unique_ptr<MemoryManager> executable_memory_;
  std::unordered_map<void *, std::vector<uint8_t>> original_codes_;
};

}  // end of namespace interceptor

extern "C" {

void *InitializeInterceptor() { return new InterceptorImpl(); }

void TerminateInterceptor(void *interceptor) {
  delete static_cast<InterceptorImpl *>(interceptor);
}

void *FindFunctionByName(void *interceptor, const char *symbol_name) {
  return static_cast<InterceptorImpl *>(interceptor)
      ->FindFunctionByName(symbol_name);
}

bool InterceptFunction(void *interceptor, void *old_function,
                       void *new_function, void **callback_function,
                       void (*error_callback)(void *, const char *),
                       void *error_callback_baton) {
  Error error =
      static_cast<InterceptorImpl *>(interceptor)
          ->InterceptFunction(old_function, new_function, callback_function);
  if (error_callback && error.Fail()) {
    std::ostringstream oss;
    oss << "Intercepting function at " << old_function
        << " failed: " << error.GetMessage();
    error_callback(error_callback_baton, oss.str().c_str());
  }
  return error.Success();
}

bool InterceptSymbol(void *interceptor, const char *symbol_name,
                     void *new_function, void **callback_function,
                     void (*error_callback)(void *, const char *),
                     void *error_callback_baton) {
  Error error =
      static_cast<InterceptorImpl *>(interceptor)
          ->InterceptFunction(symbol_name, new_function, callback_function);
  if (error_callback && error.Fail()) {
    std::ostringstream oss;
    oss << "Intercepting '" << symbol_name
        << "' failed: " << error.GetMessage();
    error_callback(error_callback_baton, oss.str().c_str());
  }
  return error.Success();
}

}  // extern "C"

static void InitializeLLVM() {
  static std::once_flag flag;

  std::call_once(flag, []() {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllDisassemblers();
  });
}

static Error ChangePageProtection(void *ptr, size_t size, int prot) {
  uintptr_t page_size = getpagesize();
  uintptr_t page_mask = ~(page_size - 1);
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t base = ptr_val & page_mask;
  uintptr_t end = (ptr_val + size + page_size - 1) & page_mask;
  if (mprotect(reinterpret_cast<void *>(base), end - base, prot) != 0)
    return Error("Failed to change protection for %p to %x", ptr, prot);
  return Error();
}

InterceptorImpl::InterceptorImpl()
    : executable_memory_(new MemoryManager(PROT_EXEC | PROT_READ,
                                           MAP_PRIVATE | MAP_ANONYMOUS)) {
  InitializeLLVM();

#if defined(__arm__)
  target_.reset(new TargetARM());
#elif defined(__arm64__) || defined(__aarch64__)
  target_.reset(new TargetAARCH64());
#elif defined(__i386__)
  target_.reset(new TargetX86());
#else
  assert(false && "Not supported architecture!");
#endif
}

InterceptorImpl::~InterceptorImpl() {
  for (const auto &code : original_codes_)
    WriteMemory(code.first, code.second.data(), code.second.size(), true);
}

Error InterceptorImpl::WriteMemory(void *target, const void *source, size_t num,
                                   bool is_executable) {
  int prot = PROT_READ;
  if (is_executable) prot |= PROT_EXEC;

  Error error = ChangePageProtection(target, num, prot | PROT_WRITE);
  if (error.Fail()) return error;

  memcpy(target, source, num);

  error = ChangePageProtection(target, num, prot);
  if (error.Fail()) return error;

  return error;
}

static size_t GetCodeAligment(Target *target, void *function) {
  uintptr_t load_address =
      reinterpret_cast<uintptr_t>(target->GetLoadAddress(function));
  return load_address % target->GetCodeAlignment();
}

Error InterceptorImpl::GetTrampolineSize(const TrampolineConfig &config,
                                         void *old_function, void *new_function,
                                         size_t &trampoline_size) {
  size_t initial_alignment = GetCodeAligment(target_.get(), old_function);
  std::unique_ptr<CodeGenerator> codegen(
      target_->GetCodeGenerator(old_function, initial_alignment));
  if (!codegen) return Error("Failed to create a code generator!");

  Error error =
      target_->EmitTrampoline(config, *codegen, old_function, new_function);
  if (error.Fail()) return error;
  trampoline_size = codegen->LayoutCode();
  return Error();
}

Error InterceptorImpl::InstallTrampoline(const TrampolineConfig &config,
                                         void *old_function,
                                         void *new_function) {
  size_t initial_alignment = GetCodeAligment(target_.get(), old_function);
  std::unique_ptr<CodeGenerator> codegen(
      target_->GetCodeGenerator(old_function, initial_alignment));
  if (!codegen) return Error("Failed to create a code generator!");

  Error error =
      target_->EmitTrampoline(config, *codegen, old_function, new_function);
  if (error.Fail()) return error;

  codegen->LayoutCode();

  void *load_address = target_->GetLoadAddress(old_function);
  codegen->LinkCode(reinterpret_cast<uintptr_t>(load_address));

  const llvm::SmallVectorImpl<char> &trampoline = codegen->GetCode();

  std::vector<uint8_t> original_code(trampoline.size());
  memcpy(original_code.data(), load_address, trampoline.size());

  error = WriteMemory(load_address, trampoline.data(), trampoline.size(), true);
  if (error.Fail()) return error;

  original_codes_.emplace(load_address, std::move(original_code));
  return Error();
}

Error InterceptorImpl::RewriteInstructions(
    void *old_function, size_t rewrite_size,
    std::unique_ptr<CodeGenerator> &codegen) {
  codegen.reset(target_->GetCodeGenerator(old_function, 0));
  if (!codegen) return Error("Failed to create codegen");

  std::unique_ptr<Disassembler> disassembler(
      target_->CreateDisassembler(old_function));
  if (!disassembler) return Error("Failed to create disassembler");

  size_t offset = 0;
  void *func_addr = target_->GetLoadAddress(old_function);
  bool reached_end_of_function = false;
  while (offset < rewrite_size && !reached_end_of_function) {
    llvm::MCInst inst;
    uint64_t inst_size = 0;
    if (!disassembler->GetInstruction(func_addr, offset, inst, inst_size))
      return Error("Failed to disassemble instruction at %p + %zd", func_addr,
                   offset);

    Error error = target_->RewriteInstruction(inst, *codegen, func_addr, offset,
                                              reached_end_of_function);
    if (error.Fail()) return error;

    offset += inst_size;
  }

  if (offset < rewrite_size)
    return Error(
        "End of function reached after %zd byte when rewriting %zd bytes",
        offset, rewrite_size);

  uint8_t *target_addr = static_cast<uint8_t *>(old_function) + offset;
  TrampolineConfig full_config = target_->GetFullTrampolineConfig();
  Error error =
      target_->EmitTrampoline(full_config, *codegen, nullptr, target_addr);
  if (error.Fail()) return error;

  return Error();
}

Error InterceptorImpl::CreateCompensationFunction(void *old_function,
                                                  size_t rewrite_size,
                                                  void **callback_function) {
  std::unique_ptr<CodeGenerator> codegen;
  Error error = RewriteInstructions(old_function, rewrite_size, codegen);
  if (error.Fail()) return error;

  size_t code_size = codegen->LayoutCode();
  void *target =
      executable_memory_->Allocate(code_size, target_->GetCodeAlignment());
  if (!target) return Error("Failed to allocate executable memory");

  error = codegen->LinkCode(reinterpret_cast<uintptr_t>(target));
  if (error.Fail()) return error;

  const llvm::SmallVectorImpl<char> &instructions = codegen->GetCode();
  error = WriteMemory(target, instructions.data(), instructions.size(), true);
  if (error.Fail()) return error;

  *callback_function = target_->FixupCallbackFunction(old_function, target);
  return Error();
}

Error InterceptorImpl::InterceptFunction(void *old_function, void *new_function,
                                         void **callback_function) {
  if (!callback_function) {
    // TODO: Verify that the function is long enough for placing a trampoline
    //       inside it. If it isn't then currently we are overwriting the
    //       beginning of the next function as well causing potential SIGILL.

    // We don't have to set up a callback function so installing a trampoline
    // without generating compensation instructions is sufficient.
    TrampolineConfig full_config = target_->GetFullTrampolineConfig();
    return InstallTrampoline(full_config, old_function, new_function);
  }

  uintptr_t old_address = reinterpret_cast<uintptr_t>(old_function);

  size_t aligned_full_trampoline_size = 0;
  TrampolineConfig full_config = target_->GetFullTrampolineConfig();
  Error error = GetTrampolineSize(full_config, nullptr, new_function,
                                  aligned_full_trampoline_size);
  if (error.Fail()) return error;

  std::vector<TrampolineConfig> configs =
      target_->GetTrampolineConfigs(old_address);
  for (const auto &config : configs) {
    if (config.IsFullTrampoline()) {
      size_t trampoline_size = 0;
      error = GetTrampolineSize(config, old_function, new_function,
                                trampoline_size);
      if (error.Fail()) return error;

      error = CreateCompensationFunction(old_function, trampoline_size,
                                         callback_function);
      if (error.Fail()) return error;

      return InstallTrampoline(config, old_function, new_function);
    } else {
      void *intermediate_trampoline = executable_memory_->Allocate(
          aligned_full_trampoline_size, target_->GetCodeAlignment(),
          config.start_address, config.end_address);
      if (!intermediate_trampoline) continue;

      size_t trampoline_size = 0;
      error = GetTrampolineSize(config, old_function, intermediate_trampoline,
                                trampoline_size);
      if (error.Fail()) return error;

      error = CreateCompensationFunction(old_function, trampoline_size,
                                         callback_function);
      if (error.Fail()) return error;

      error =
          InstallTrampoline(full_config, intermediate_trampoline, new_function);
      if (error.Fail()) return error;

      return InstallTrampoline(config, old_function, intermediate_trampoline);
    }
  }
  return Error("Failed to find a suitable trampoline");
}

Error InterceptorImpl::InterceptFunction(const char *symbol_name,
                                         void *new_function,
                                         void **callback_function) {
  linker_.RefreshSymbolList();

  std::vector<Linker::Symbol> symbols = linker_.FindSymbols(symbol_name);
  if (symbols.empty())
    return Error("Failed to find symbol with name '%s'", symbol_name);
  if (symbols.size() > 1)
    return Error("More then 1 symbol found with name '%s'", symbol_name);

  const Linker::Symbol &symbol = symbols.front();
  return InterceptFunction(reinterpret_cast<void *>(symbol.address),
                           new_function, callback_function);
}

void *InterceptorImpl::FindFunctionByName(const char *symbol_name) {
  if (!symbol_name) return nullptr;

  linker_.RefreshSymbolList();

  std::vector<Linker::Symbol> symbols = linker_.FindSymbols(symbol_name);
  if (symbols.empty()) return nullptr;
  if (symbols.size() > 1) return nullptr;

  return reinterpret_cast<void *>(symbols.front().address);
}
