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

#ifndef INTERCEPTOR_CONSTANT_POOL_DATA_EXPR_H_
#define INTERCEPTOR_CONSTANT_POOL_DATA_EXPR_H_

#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/raw_ostream.h"

namespace interceptor {

class ConstantPoolDataExpr : public llvm::MCTargetExpr {
 public:
  void printImpl(llvm::raw_ostream &, const llvm::MCAsmInfo *) const override;

  void visitUsedExpr(llvm::MCStreamer &Streamer) const override;

  llvm::MCFragment *findAssociatedFragment() const override;

  bool evaluateAsRelocatableImpl(llvm::MCValue &res, const llvm::MCAsmLayout *,
                                 const llvm::MCFixup *) const override;

  void fixELFSymbolsInTLSFixups(llvm::MCAssembler &) const override;

  bool allocate(llvm::raw_ostream &data);

  void setBaseLocation(uintptr_t location);

  static const ConstantPoolDataExpr *Create(const llvm::MCExpr *expr,
                                            size_t size, size_t alignment,
                                            llvm::MCContext &ctx);

  static bool classof(const MCExpr *E) {
    return E->getKind() == MCExpr::Target;
  }

 private:
  ConstantPoolDataExpr(const llvm::MCExpr *expr, size_t size, size_t alignment);

  const llvm::MCExpr *expr_;
  const size_t size_;
  const size_t alignment_;

  bool allocated_;
  uintptr_t base_location_;
};

}  // end of namespace interceptor

#endif  // INTERCEPTOR_CONSTANT_POOL_DATA_EXPR_H_
