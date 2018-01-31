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

#ifndef INTERCEPTOR_CONSTNT_POOL_DATA_EXPR_H_
#define INTERCEPTOR_CONSTNT_POOL_DATA_EXPR_H_

#include <memory>
#include <string>
#include <vector>

#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"

#include "constant_pool_data_expr.h"
#include "error.h"

namespace interceptor {

class CodeGenerator {
 public:
  static CodeGenerator *Create(const llvm::Triple &triple,
                               size_t start_alignment = 0);

  void AddInstruction(const llvm::MCInst &inst);

  size_t LayoutCode();

  Error LinkCode(uintptr_t location);

  const llvm::SmallVectorImpl<char> &GetCode() const;

  uint32_t GetAlignmentOffset(uint32_t alignment_base) const;

  template <typename T, size_t A = alignof(T)>
  const llvm::MCExpr *CreateDataExpr(T value, size_t alignment = A);

  const llvm::MCSubtargetInfo &GetSubtargetInfo() const;

  std::string PrintInstruction(const llvm::MCInst &inst);

 private:
  CodeGenerator(const llvm::Triple &triple, size_t start_alignment);

  bool Initialize();

  size_t start_alignment_;

  llvm::Triple triple_;
  std::unique_ptr<llvm::MCRegisterInfo> mri_;
  std::unique_ptr<llvm::MCSubtargetInfo> sti_;
  std::unique_ptr<llvm::MCInstrInfo> mii_;
  std::unique_ptr<llvm::MCInstPrinter> ipr_;
  std::unique_ptr<llvm::MCAsmBackend> asmb_;
  std::unique_ptr<llvm::MCAsmInfo> asmi_;
  std::unique_ptr<llvm::MCContext> ctx_;
  std::unique_ptr<llvm::MCCodeEmitter> codegen_;

  std::vector<llvm::MCInst> instructions_;
  std::vector<const ConstantPoolDataExpr *> const_pool_exprs_;

  llvm::SmallVector<char, 32> code_;
  llvm::raw_svector_ostream code_stream_;
  llvm::SmallVector<llvm::MCFixup, 8> fixups_;
};

template <typename T, size_t A>
const llvm::MCExpr *CodeGenerator::CreateDataExpr(T value, size_t alignment) {
  const llvm::MCExpr *value_exp = llvm::MCConstantExpr::create(value, *ctx_);
  const_pool_exprs_.emplace_back(
      ConstantPoolDataExpr::Create(value_exp, sizeof(T), alignment, *ctx_));
  return const_pool_exprs_.back();
}

}  // end of namespace interceptor

#endif  // INTERCEPTOR_CONSTNT_POOL_DATA_EXPR_H_
