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

#include "constant_pool_data_expr.h"

using namespace interceptor;

ConstantPoolDataExpr::ConstantPoolDataExpr(const llvm::MCExpr *expr,
                                           size_t size, size_t alignment)
    : expr_(expr),
      size_(size),
      alignment_(alignment),
      allocated_(false),
      base_location_(0) {}

const ConstantPoolDataExpr *ConstantPoolDataExpr::Create(
    const llvm::MCExpr *expr, size_t size, size_t alignment,
    llvm::MCContext &ctx) {
  return new (ctx) ConstantPoolDataExpr(expr, size, alignment);
}

void ConstantPoolDataExpr::printImpl(llvm::raw_ostream &,
                                     const llvm::MCAsmInfo *) const {}

void ConstantPoolDataExpr::fixELFSymbolsInTLSFixups(llvm::MCAssembler &) const {
}

void ConstantPoolDataExpr::visitUsedExpr(llvm::MCStreamer &Streamer) const {
  Streamer.visitUsedExpr(*expr_);
}

llvm::MCFragment *ConstantPoolDataExpr::findAssociatedFragment() const {
  llvm_unreachable("FIXME: what goes here?");
}

bool ConstantPoolDataExpr::evaluateAsRelocatableImpl(
    llvm::MCValue &res, const llvm::MCAsmLayout *,
    const llvm::MCFixup *) const {
  if (!allocated_) return false;
  res = llvm::MCValue::get(base_location_);
  return true;
}

bool ConstantPoolDataExpr::allocate(llvm::raw_ostream &data) {
  llvm::MCValue val;
  if (!expr_->evaluateAsRelocatable(val, nullptr, nullptr)) return false;

  base_location_ += data.tell();
  while (base_location_ % alignment_ != 0) {
    data.write((char)0);
    base_location_++;
  }

  switch (size_) {
    case 1: {
      uint8_t value = val.getConstant();
      data.write((char *)&value, sizeof(value));
      break;
    }
    case 2: {
      uint16_t value = val.getConstant();
      data.write((char *)&value, sizeof(value));
      break;
    }
    case 4: {
      uint32_t value = val.getConstant();
      data.write((char *)&value, sizeof(value));
      break;
    }
    case 8: {
      uint64_t value = val.getConstant();
      data.write((char *)&value, sizeof(value));
      break;
    }
    default: {
      assert(false && "Unsupported alignment");
      return false;
    }
  }

  allocated_ = true;
  return true;
}

void ConstantPoolDataExpr::setBaseLocation(uintptr_t location) {
  base_location_ += location;
}
