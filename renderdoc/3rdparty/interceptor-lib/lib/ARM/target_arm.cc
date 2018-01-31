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

#include "target_arm.h"

#include <memory>

#include "MCTargetDesc/ARMBaseInfo.h"
#include "MCTargetDesc/ARMMCTargetDesc.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"

#include "code_generator.h"
#include "disassembler.h"

using namespace interceptor;

static bool IsThumb(void *ptr) { return reinterpret_cast<uintptr_t>(ptr) & 1; }

static bool IsThumb(const CodeGenerator &codegen) {
  llvm::Triple::ArchType arch =
      codegen.GetSubtargetInfo().getTargetTriple().getArch();
  return arch == llvm::Triple::thumb || arch == llvm::Triple::thumbeb;
}

static llvm::Triple GetTriple(void *addr) {
  llvm::Triple triple(llvm::sys::getProcessTriple());
  assert((triple.getArch() == llvm::Triple::arm ||
          triple.getArch() == llvm::Triple::thumb) &&
         "Invalid default host triple for target");
  if (IsThumb(addr)) {
    llvm::StringRef arm_name = triple.getArchName();
    std::string thumb_name("thumb");
    thumb_name += arm_name.substr(3);
    triple.setArchName(thumb_name);
  }
  return triple;
}

CodeGenerator *TargetARM::GetCodeGenerator(void *address,
                                           size_t start_alignment) {
  return CodeGenerator::Create(GetTriple(address), start_alignment);
}

Disassembler *TargetARM::CreateDisassembler(void *address) {
  return Disassembler::Create(GetTriple(address));
}

void *TargetARM::GetLoadAddress(void *addr) {
  uintptr_t addr_val = reinterpret_cast<uintptr_t>(addr);
  addr_val &= ~1;
  return reinterpret_cast<void *>(addr_val);
}

void *TargetARM::FixupCallbackFunction(void *old_function, void *new_function) {
  if (IsThumb(old_function)) {
    uintptr_t new_func_addr = reinterpret_cast<uintptr_t>(new_function);
    new_func_addr |= 1;
    return reinterpret_cast<void *>(new_func_addr);
  }
  return new_function;
}

std::vector<TrampolineConfig> TargetARM::GetTrampolineConfigs(
    uintptr_t start_address) const {
  std::vector<TrampolineConfig> configs;
  configs.push_back({FULL_TRAMPOLINE, false, 0, 0xffffffff});
  return configs;
}

Error TargetARM::EmitTrampoline(const TrampolineConfig &config,
                                    CodeGenerator &codegen, void *source,
                                    void *target) {
  switch (config.type) {
    case FULL_TRAMPOLINE: {
      uint32_t target_addr = (uintptr_t)target;
      if (IsThumb(codegen)) {
        codegen.AddInstruction(
            llvm::MCInstBuilder(llvm::ARM::t2LDRpci)
                .addReg(llvm::ARM::PC)
                .addExpr(codegen.CreateDataExpr(target_addr)));
      } else {
        codegen.AddInstruction(llvm::MCInstBuilder(llvm::ARM::LDRi12)
                                   .addReg(llvm::ARM::PC)
                                   .addExpr(codegen.CreateDataExpr(target_addr))
                                   .addImm(0)
                                   .addImm(llvm::ARMCC::AL)
                                   .addImm(0));
      }
      return Error();
    }
  }
  return Error("Unsupported trampoline type");
}

static void *calculatePcRelativeAddressArm(void *data, size_t pc_offset,
                                           size_t offset) {
  uintptr_t data_addr = reinterpret_cast<uintptr_t>(data);
  assert((data_addr & 3) == 0 && "Unaligned data address");
  assert((pc_offset & 3) == 0 && "Unaligned PC offset");

  data_addr += pc_offset;  // Add the PC
  data_addr += 8;          // Add the 8 byte implicit offset
  data_addr += offset;     // Add the offset
  return reinterpret_cast<void *>(data_addr);
}

static void *calculatePcRelativeAddressThumb(void *data, size_t pc_offset,
                                             size_t offset, bool align) {
  uintptr_t data_addr = reinterpret_cast<uintptr_t>(data);
  assert((data_addr & 1) == 0 && "Unaligned data address");
  assert((pc_offset & 1) == 0 && "Unaligned PC offset");

  data_addr += pc_offset;      // Add the PC
  data_addr += 1;              // Add 1 for the thumb bit
  data_addr += 4;              // Add the 4 byte implicit offset
  if (align) data_addr &= ~3;  // Align to 4 byte
  data_addr += offset;         // Add the offset
  return reinterpret_cast<void *>(data_addr);
}

static uint32_t getThumbPc(void *data, size_t offset) {
  uintptr_t data_addr = reinterpret_cast<uintptr_t>(data);
  data_addr += offset;
  data_addr += 4;
  data_addr &= ~1;
  return data_addr;
}

static bool hasPcOperand(const llvm::MCInst &inst) {
  for (size_t i = 0; i < inst.getNumOperands(); ++i) {
    const llvm::MCOperand &op = inst.getOperand(i);
    if (op.isReg() && op.getReg() == llvm::ARM::PC) return true;
  }
  return false;
}

Error TargetARM::RewriteInstruction(const llvm::MCInst &inst,
                                    CodeGenerator &codegen, void *data,
                                    size_t offset,
                                    bool &possible_end_of_function) {
  switch (inst.getOpcode()) {
    case llvm::ARM::tADDspi:
    case llvm::ARM::tSUBspi: {
      possible_end_of_function = false;
      codegen.AddInstruction(inst);
      break;
    }
    case llvm::ARM::MRC:
    case llvm::ARM::MOVi16:
    case llvm::ARM::tMOVi8: {
      uint32_t RdRt = inst.getOperand(0).getReg();
      possible_end_of_function = (RdRt == llvm::ARM::PC);
      codegen.AddInstruction(inst);
      break;
    }
    case llvm::ARM::t2LDMIA_UPD: {
      possible_end_of_function = hasPcOperand(inst);
      codegen.AddInstruction(inst);
      break;
    }
    case llvm::ARM::CMPrr:
    case llvm::ARM::LDR_PRE_IMM:
    case llvm::ARM::LDR_PRE_REG:
    case llvm::ARM::LDR_POST_IMM:
    case llvm::ARM::LDR_POST_REG:
    case llvm::ARM::LDRH_PRE:
    case llvm::ARM::LDRH_POST:
    case llvm::ARM::LDRH:
    case llvm::ARM::LDRB_PRE_IMM:
    case llvm::ARM::LDRB_PRE_REG:
    case llvm::ARM::LDRB_POST_IMM:
    case llvm::ARM::LDRB_POST_REG:
    case llvm::ARM::LDRBi12:
    case llvm::ARM::LDRSH_PRE:
    case llvm::ARM::LDRSH_POST:
    case llvm::ARM::LDRSH:
    case llvm::ARM::LDRSB_PRE:
    case llvm::ARM::LDRSB_POST:
    case llvm::ARM::LDRSB:
    case llvm::ARM::STR_PRE_IMM:
    case llvm::ARM::STR_PRE_REG:
    case llvm::ARM::STR_POST_IMM:
    case llvm::ARM::STR_POST_REG:
    case llvm::ARM::STRi12:
    case llvm::ARM::STRH_PRE:
    case llvm::ARM::STRH_POST:
    case llvm::ARM::STRH:
    case llvm::ARM::STRB_PRE_IMM:
    case llvm::ARM::STRB_PRE_REG:
    case llvm::ARM::STRB_POST_IMM:
    case llvm::ARM::STRB_POST_REG:
    case llvm::ARM::STRBi12:
    case llvm::ARM::MOVr:
    case llvm::ARM::STMDA_UPD:
    case llvm::ARM::STMDB_UPD:
    case llvm::ARM::STRD:
    case llvm::ARM::STRD_PRE:
    case llvm::ARM::SUBri:
    case llvm::ARM::tADDi3:
    case llvm::ARM::tADDi8:
    case llvm::ARM::tADDrSP:
    case llvm::ARM::tADDrSPi:
    case llvm::ARM::tBIC:
    case llvm::ARM::tCMPi8:
    case llvm::ARM::tLDRi:
    case llvm::ARM::tLDRspi:
    case llvm::ARM::tLSRri:
    case llvm::ARM::tMOVr:
    case llvm::ARM::tPUSH:
    case llvm::ARM::tSTRspi:
    case llvm::ARM::tSUBrr:
    case llvm::ARM::t2ADDri:
    case llvm::ARM::t2ADDri12:
    case llvm::ARM::t2BICri:
    case llvm::ARM::t2BICrr:
    case llvm::ARM::t2CMPri:
    case llvm::ARM::t2LDRi12:
    case llvm::ARM::t2LDRDi8:
    case llvm::ARM::t2LDRD_PRE:
    case llvm::ARM::t2LDRD_POST:
    case llvm::ARM::t2MOVi:
    case llvm::ARM::t2MOVr:
    case llvm::ARM::t2MOVTi16:
    case llvm::ARM::t2STMDB_UPD:
    case llvm::ARM::t2STR_PRE:
    case llvm::ARM::t2STRDi8:
    case llvm::ARM::t2STRD_PRE:
    case llvm::ARM::t2STRD_POST:
    case llvm::ARM::t2SUBri:
    case llvm::ARM::VSTMDDB_UPD: {
      if (hasPcOperand(inst))
        return Error(
            "Instruction not handled yet when one of the operand is PC");
      possible_end_of_function = false;
      codegen.AddInstruction(inst);
      break;
    }
    case llvm::ARM::tADDhirr: {
      uint32_t Rdn = inst.getOperand(0).getReg();
      uint32_t Rm = inst.getOperand(2).getReg();
      possible_end_of_function = (Rdn == llvm::ARM::PC);

      if (Rm == llvm::ARM::PC) {
        if (Rdn == llvm::ARM::PC) return Error("'add pc, pc' is UNPREDICTABLE");

        uint32_t pc_value = getThumbPc(data, offset);
        uint32_t scratch_reg =
            Rdn != llvm::ARM::R0 ? llvm::ARM::R0 : llvm::ARM::R1;
        codegen.AddInstruction(
            llvm::MCInstBuilder(llvm::ARM::tPUSH).addImm(0).addImm(0).addReg(
                scratch_reg));
        codegen.AddInstruction(llvm::MCInstBuilder(llvm::ARM::tLDRpci)
                                   .addReg(scratch_reg)
                                   .addExpr(codegen.CreateDataExpr(pc_value)));
        codegen.AddInstruction(llvm::MCInstBuilder(llvm::ARM::tADDhirr)
                                   .addReg(Rdn)
                                   .addImm(0)
                                   .addReg(scratch_reg));
        codegen.AddInstruction(
            llvm::MCInstBuilder(llvm::ARM::tPOP).addImm(0).addImm(0).addReg(
                scratch_reg));
      } else {
        codegen.AddInstruction(inst);
      }
      break;
    }
    case llvm::ARM::LDRi12: {
      uint32_t Rt = inst.getOperand(0).getReg();
      uint32_t Rn = inst.getOperand(1).getReg();
      int64_t imm = inst.getOperand(2).getImm();
      int64_t p   = inst.getOperand(3).getImm();
      possible_end_of_function = (Rt == llvm::ARM::PC);

      if (Rn == llvm::ARM::PC) {
        void *load_source =
            calculatePcRelativeAddressArm(data, offset, imm);
        uint32_t load_data = 0;
        memcpy(&load_data, load_source, sizeof(uint32_t));
        codegen.AddInstruction(llvm::MCInstBuilder(llvm::ARM::LDRi12)
                                   .addReg(Rt)
                                   .addExpr(codegen.CreateDataExpr(load_data))
                                   .addImm(0)
                                   .addImm(p)
                                   .addImm(0));
      } else {
        codegen.AddInstruction(inst);
      }
      break;
    }
    case llvm::ARM::tLDRpci:
    case llvm::ARM::t2LDRpci: {
      uint32_t Rt = inst.getOperand(0).getReg();
      int64_t imm = inst.getOperand(1).getImm();
      possible_end_of_function = (Rt == llvm::ARM::PC);

      void *load_source =
          calculatePcRelativeAddressThumb(data, offset, imm, true);
      uint32_t load_data = 0;
      memcpy(&load_data, load_source, sizeof(uint32_t));
      llvm::MCInst new_inst = inst;  // TODO: Use MCInstBuilder
      new_inst.getOperand(1) =
          llvm::MCOperand::createExpr(codegen.CreateDataExpr(load_data));
      codegen.AddInstruction(new_inst);
      break;
    }
    case llvm::ARM::Bcc: {
      uint32_t pred = inst.getOperand(0).getImm();
      uint32_t imm = inst.getOperand(1).getImm();
      possible_end_of_function = true;

      void *target = calculatePcRelativeAddressArm(data, offset, imm);
      uint32_t target_addr = reinterpret_cast<uintptr_t>(target);
      codegen.AddInstruction(llvm::MCInstBuilder(llvm::ARM::LDRi12)
                                 .addReg(llvm::ARM::PC)
                                 .addExpr(codegen.CreateDataExpr(target_addr))
                                 .addImm(0)
                                 .addImm(pred)
                                 .addImm(0));
      break;
    }
    case llvm::ARM::t2B: {
      uint32_t imm = inst.getOperand(0).getImm();
      possible_end_of_function = true;

      void *target = calculatePcRelativeAddressThumb(data, offset, imm, false);
      uint32_t target_addr = reinterpret_cast<uintptr_t>(target);
      codegen.AddInstruction(llvm::MCInstBuilder(llvm::ARM::t2LDRpci)
                                 .addReg(llvm::ARM::PC)
                                 .addExpr(codegen.CreateDataExpr(target_addr)));
      break;
    }
    case llvm::ARM::tBL: {
      uint32_t imm = inst.getOperand(2).getImm();
      possible_end_of_function = false;

      uint32_t lr_offset = 5 + codegen.GetAlignmentOffset(4);
      codegen.AddInstruction(llvm::MCInstBuilder(llvm::ARM::t2ADDri12)
                                 .addReg(llvm::ARM::LR)    // Rd
                                 .addReg(llvm::ARM::PC)    // Rn
                                 .addImm(lr_offset)        // imm
                                 .addImm(0)                // -
                                 .addImm(0)                // -
                                 .addReg(llvm::ARM::R0));  // S

      void *target = calculatePcRelativeAddressThumb(data, offset, imm, false);
      uint32_t target_addr = reinterpret_cast<uintptr_t>(target);
      codegen.AddInstruction(llvm::MCInstBuilder(llvm::ARM::t2LDRpci)
                                 .addReg(llvm::ARM::PC)
                                 .addExpr(codegen.CreateDataExpr(target_addr)));
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
