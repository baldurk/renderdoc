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

#ifndef INTERCEPTOR_TARGET_H_
#define INTERCEPTOR_TARGET_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "llvm/MC/MCInst.h"

#include "code_generator.h"
#include "disassembler.h"
#include "error.h"

namespace interceptor {

struct TrampolineConfig {
  uint32_t type;            // Architecture specific value
  bool require_source;      // Using relative or absolute jump
  uintptr_t start_address;  // First address it can jump to
  uintptr_t end_address;    // Last address it can jump to

  // Returns true if the trampoline can jump from any address in the address
  // space to any other address and false otherwise.
  bool IsFullTrampoline() const {
    return !require_source &&
           start_address == std::numeric_limits<uintptr_t>::min() &&
           end_address == std::numeric_limits<uintptr_t>::max();
  }
};

class Target {
 public:
  virtual ~Target() = default;

  // Create a new code generator with the specified start alignment what can
  // generate code with the same ISA pointed by the address (e.g. thumb vs arm).
  virtual CodeGenerator *GetCodeGenerator(void *address,
                                          size_t start_alignment) = 0;

  // Create a disassebler what can disassamble code coming from the specified
  // address.
  virtual Disassembler *CreateDisassembler(void *address) = 0;

  // Get the maximum alignment required by the target by any instruction in any
  // of the supported ISA.
  virtual size_t GetCodeAlignment() const = 0;

  // Return a load address from a function pointer. Have to be implemented for
  // architectures where some bits of the function pointers contain meta-data
  // (e.g. thumb bit)
  virtual void *GetLoadAddress(void *addr) { return addr; }

  // Returns the full list of avaiulable trampolines on the given architecture
  // sorted to increasing order by the total size of instructions (including
  // data) inside the trampoline.
  virtual std::vector<TrampolineConfig> GetTrampolineConfigs(
      uintptr_t start_address) const = 0;

  // Return the configuartion of the full trampoline what have to be able to
  // jump to any address inside the process's address space.
  TrampolineConfig GetFullTrampolineConfig() const;

  // Emit a trampoline with the given config into the code generator what will
  // jump to the specified target address if it is placed into the location
  // specified by the source address.
  virtual Error EmitTrampoline(const TrampolineConfig &config,
                               CodeGenerator &codegen, void *source,
                               void *target) = 0;

  // Rewrite the specified instruction read from "data + offset "into the code
  // generator with a set of instructions with the exact same effect but without
  // any limitation about the location they have to be placed at. Additionally
  // it sets the "possible_end_of_function" flag to true if the instruction can
  // be the last one inside a function and to false otherwise.
  virtual Error RewriteInstruction(const llvm::MCInst &inst,
                                   CodeGenerator &codegen, void *data,
                                   size_t offset,
                                   bool &possible_end_of_function) = 0;

  // Convert the pointer specified by "new_function" from a memory load address
  // to a function pointer with the same ISA as the function pointed by
  // "old_function". Have to be implemented for architectures where some bits of
  // the function pointers contain meta-data (e.g. thumb bit).
  virtual void *FixupCallbackFunction(void *old_function, void *new_function) {
    return new_function;
  }
};

}  // end of namespace interceptor

#endif  // INTERCEPTOR_TARGET_H_
