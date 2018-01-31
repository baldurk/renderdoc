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

#include "target_x86.h"

#include <memory>

#include "MCTargetDesc/X86MCTargetDesc.h"
#include "llvm/ADT/Triple.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"

#include "code_generator.h"
#include "disassembler.h"

using namespace interceptor;

static llvm::Triple GetTriple() {
  llvm::Triple triple(llvm::sys::getProcessTriple());
  assert(triple.getArch() == llvm::Triple::x86 &&
         "Invalid default host triple for target");
  return triple;
}

CodeGenerator *TargetX86::GetCodeGenerator(void *address,
                                           size_t start_alignment) {
  return CodeGenerator::Create(GetTriple(), start_alignment);
}

Disassembler *TargetX86::CreateDisassembler(void *address) {
  return Disassembler::Create(GetTriple());
}

std::vector<TrampolineConfig> TargetX86::GetTrampolineConfigs(
    uintptr_t start_address) const {
  std::vector<TrampolineConfig> configs;
  configs.push_back({FULL_TRAMPOLINE, false, 0, 0xffffffff});
  return configs;
}

Error TargetX86::EmitTrampoline(const TrampolineConfig &config,
                                    CodeGenerator &codegen, void *source,
                                    void *target) {
  switch (config.type) {
    case FULL_TRAMPOLINE: {
      uintptr_t target_addr = reinterpret_cast<uintptr_t>(target);
      codegen.AddInstruction(
          llvm::MCInstBuilder(llvm::X86::JMP_4).addImm(target_addr));
      return Error();
    }
  }
  return Error("Unsupported trampoline type");
}

static uintptr_t calculatePcRelativeAddress(void *data, int64_t pc_offset,
                                            size_t offset, size_t instr_size) {
  uintptr_t data_addr = reinterpret_cast<uintptr_t>(data);
  data_addr += pc_offset;
  data_addr += offset;
  data_addr += instr_size;
  return data_addr;
}

// Checks if the given address is the address of the __x86.get_pc_thunk.bx
// function with comparing the instruction sequence to the following opcodes:
// MOVL (%esp), %ebx; RETL
// Additionally we detect functions with NOP instructions between the above
// sepcified instructions.
static Error IsGetPcThunk(TargetX86 *target, void *address, bool &res) {
  std::unique_ptr<Disassembler> disassembler(
      target->CreateDisassembler(address));
  if (!disassembler) return Error("Failed to create disassembler");

  // The decode state represent the next instruction we are expecting
  enum class DetectState : uint8_t {
    MOV,
    RET
  };
  DetectState state = DetectState::MOV;

  size_t offset = 0;
  while (true) {
    llvm::MCInst inst;
    uint64_t inst_size = 0;
    if (!disassembler->GetInstruction(address, offset, inst, inst_size))
      return Error("Failed to disassemble instruction at %p + %zd", address,
                   offset);

    switch (inst.getOpcode()) {
      case llvm::X86::NOOP:
      case llvm::X86::NOOPL:
      case llvm::X86::NOOPW:
        break;
      case llvm::X86::RET:
      case llvm::X86::RETL:
      case llvm::X86::RETW:
        res = (state == DetectState::RET);
        return Error();
      case llvm::X86::MOV32rm:
        if (state != DetectState::MOV) {
          res = false;
          return Error();
        } else if (inst.getNumOperands() < 2) {
          res = false;
          return Error();
        }
        const llvm::MCOperand &op0 = inst.getOperand(0),
                              op1 = inst.getOperand(1);
        if (!op0.isReg() || op0.getReg() != llvm::X86::EBX || !op1.isReg() ||
            op1.getReg() != llvm::X86::ESP) {
          res = false;
          return Error();
        }
        state = DetectState::RET;
        break;
    }
    offset += inst_size;
  }
}

Error TargetX86::RewriteInstruction(const llvm::MCInst &inst,
                                    CodeGenerator &codegen, void *data,
                                    size_t offset,
                                    bool &possible_end_of_function) {
  switch (inst.getOpcode()) {
    case llvm::X86::AND32rr:
    case llvm::X86::AND32mr:
    case llvm::X86::AND32ri8:
    case llvm::X86::AND32mi8:
    case llvm::X86::AND32i32:
    case llvm::X86::AND32ri:
    case llvm::X86::AND32mi:
    case llvm::X86::AND32rm:
    case llvm::X86::LEA32r:
    case llvm::X86::MOV32ao32:
    case llvm::X86::MOV32rm:
    case llvm::X86::MOV32rr:
    case llvm::X86::PUSH32r:
    case llvm::X86::SUB32ri:
    case llvm::X86::SUB32ri8: {
      possible_end_of_function = false;
      codegen.AddInstruction(inst);
      break;
    }
    case llvm::X86::CALLpcrel32: {
      if (inst.getNumOperands() != 1)
        return Error("CALL <rel32> not supported when has more then 1 operand");
      possible_end_of_function = false;
      uintptr_t target_addr = calculatePcRelativeAddress(
          data, inst.getOperand(0).getImm(), offset, 5);

      bool is_get_pc_thunk = false;
      Error error = IsGetPcThunk(this, reinterpret_cast<void *>(target_addr),
                                 is_get_pc_thunk);
      if (error.Fail()) return error;

      if (is_get_pc_thunk) {
        uintptr_t return_value = calculatePcRelativeAddress(data, 0, offset, 5);
        codegen.AddInstruction(llvm::MCInstBuilder(llvm::X86::MOV32ri)
                                   .addReg(llvm::X86::EBX)
                                   .addImm(return_value));
      } else {
        codegen.AddInstruction(
            llvm::MCInstBuilder(llvm::X86::CALLpcrel32).addImm(target_addr));
      }
      break;
    }
    default: {
      possible_end_of_function = true;
      return Error("Unhandled instruction: %s (OpcodeId: %d)",
                   codegen.PrintInstruction(inst).c_str(), inst.getOpcode());
    }
  }
  return Error();
}
