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

#ifndef INTERCEPTOR_DISASSEMBLER_H_
#define INTERCEPTOR_DISASSEMBLER_H_

#include <memory>

#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"

namespace interceptor {

class Disassembler {
 public:
  static Disassembler *Create(const llvm::Triple &triple);

  bool GetInstruction(const void *data, size_t offset, llvm::MCInst &inst,
                      uint64_t &inst_size);

 private:
  Disassembler(const llvm::Triple &triple);

  bool Initialize();

  llvm::Triple triple_;
  std::unique_ptr<llvm::MCRegisterInfo> mri_;
  std::unique_ptr<llvm::MCSubtargetInfo> sti_;
  std::unique_ptr<llvm::MCContext> ctx_;
  std::unique_ptr<llvm::MCDisassembler> dis_;
};

}  // end of namespace interceptor

#endif  // INTERCEPTOR_DISASSEMBLER_H_
