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

#include "code_generator.h"

using namespace interceptor;

CodeGenerator::CodeGenerator(const llvm::Triple &triple, size_t start_alignment)
    : start_alignment_(start_alignment), triple_(triple), code_stream_(code_) {
  // Set the start alignment of the stream to match with the specified value
  for (size_t i = 0; i < start_alignment_; ++i) code_stream_.write((char)0);
}

CodeGenerator *CodeGenerator::Create(const llvm::Triple &triple,
                                     size_t start_alignment) {
  std::unique_ptr<CodeGenerator> codegen(
      new CodeGenerator(triple, start_alignment));
  if (codegen->Initialize()) return codegen.release();
  return nullptr;
}

void CodeGenerator::AddInstruction(const llvm::MCInst &inst) {
  size_t offset = code_.size();
  llvm::SmallVector<llvm::MCFixup, 4> new_fixups;
  codegen_->encodeInstruction(inst, code_stream_, new_fixups, *sti_);

  // We have to offset the fixups with the offset of the generated instruction
  // in the data stream because encodeInstruction emits them as if it is
  // writing the new instructions from the beginning of the stream.
  for (llvm::MCFixup &fixup : new_fixups)
    fixup.setOffset(fixup.getOffset() + offset);

  fixups_.insert(fixups_.end(), new_fixups.begin(), new_fixups.end());
}

bool CodeGenerator::Initialize() {
  std::string error;

  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(triple_.str().c_str(), error);
  if (!target) return false;

  mri_.reset(target->createMCRegInfo(triple_.str().c_str()));
  if (!mri_) return false;

  sti_.reset(target->createMCSubtargetInfo(triple_.str().c_str(), "", ""));
  if (!sti_) return false;

  mii_.reset(target->createMCInstrInfo());
  if (!mii_) return false;

  llvm::MCTargetOptions options;
  asmb_.reset(target->createMCAsmBackend(*mri_, triple_.str(), "", options));
  if (!asmb_) return false;

  ctx_.reset(new llvm::MCContext(nullptr, mri_.get(), nullptr));
  codegen_.reset(target->createMCCodeEmitter(*mii_, *mri_, *ctx_));
  if (!codegen_) return false;

  // These are used only for logging and error reporting. Don't fail if we
  // haven't managed to create them.
  asmi_.reset(target->createMCAsmInfo(*mri_, triple_.str().c_str()));
  if (asmi_) {
    ipr_.reset(target->createMCInstPrinter(llvm::Triple(triple_), 0, *asmi_,
                                           *mii_, *mri_));
  }

  return true;
}

size_t CodeGenerator::LayoutCode() {
  for (const ConstantPoolDataExpr *pool : const_pool_exprs_)
    const_cast<ConstantPoolDataExpr *>(pool)->allocate(code_stream_);
  return code_.size() - start_alignment_;
}

Error CodeGenerator::LinkCode(uintptr_t location) {
  for (const ConstantPoolDataExpr *pool : const_pool_exprs_)
    const_cast<ConstantPoolDataExpr *>(pool)->setBaseLocation(location);

  for (const llvm::MCFixup &fixup : fixups_) {
    const llvm::MCExpr *expr = fixup.getValue();

    llvm::MCValue mc_value;
    uint64_t value = 0;
    if (!expr->evaluateAsRelocatable(mc_value, nullptr, &fixup))
      return Error("Failed to evalue the value of an MCFixup");
    value = mc_value.getConstant();

    int flags = asmb_->getFixupKindInfo(fixup.getKind()).Flags;
    bool pc_rel = flags & llvm::MCFixupKindInfo::FKF_IsPCRel;
    bool align_pc = flags & llvm::MCFixupKindInfo::FKF_IsAlignedDownTo32Bits;
    if (pc_rel) {
      uint64_t offset = fixup.getOffset();
      if (align_pc) offset &= ~0x3;
      value -= offset;
      value -= location;
    }

    asmb_->applyFixup(fixup, code_.data(), code_.size(), value, pc_rel);
  }

  if (start_alignment_ != 0)
    code_.erase(code_.begin(), code_.begin() + start_alignment_);

  return Error();
}

const llvm::SmallVectorImpl<char> &CodeGenerator::GetCode() const {
  return code_;
}

const llvm::MCSubtargetInfo &CodeGenerator::GetSubtargetInfo() const {
  return *sti_;
}

uint32_t CodeGenerator::GetAlignmentOffset(uint32_t alignment_base) const {
  return code_.size() % alignment_base;
}

std::string CodeGenerator::PrintInstruction(const llvm::MCInst &inst) {
  std::string s;
  llvm::raw_string_ostream os(s);
  if (ipr_)
    ipr_->printInst(&inst, os, "", *sti_);
  else
    inst.dump_pretty(os, nullptr, " ");
  return os.str();
}
