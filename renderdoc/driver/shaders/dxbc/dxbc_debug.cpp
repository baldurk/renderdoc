/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "dxbc_debug.h"
#include <algorithm>
#include "common/formatting.h"
#include "driver/dxgi/dxgi_common.h"
#include "maths/formatpacking.h"
#include "replay/replay_driver.h"
#include "dxbc_bytecode.h"
#include "dxbc_container.h"

using namespace DXBCBytecode;

namespace DXBCDebug
{
static float round_ne(float x)
{
  if(!RDCISFINITE(x))
    return x;

  float rem = remainderf(x, 1.0f);

  return x - rem;
}

static float flush_denorm(const float f)
{
  uint32_t x;
  memcpy(&x, &f, sizeof(f));

  // if any bit is set in the exponent, it's not denormal
  if(x & 0x7F800000)
    return f;

  // keep only the sign bit
  x &= 0x80000000;
  float ret;
  memcpy(&ret, &x, sizeof(ret));
  return ret;
}

VarType OperationType(const DXBCBytecode::OpcodeType &op)
{
  switch(op)
  {
    // non typed operations, just return float
    case OPCODE_LOOP:
    case OPCODE_CONTINUE:
    case OPCODE_CONTINUEC:
    case OPCODE_ENDLOOP:
    case OPCODE_SWITCH:
    case OPCODE_CASE:
    case OPCODE_DEFAULT:
    case OPCODE_ENDSWITCH:
    case OPCODE_ELSE:
    case OPCODE_ENDIF:
    case OPCODE_RET:
    case OPCODE_RETC:
    case OPCODE_DISCARD:
    case OPCODE_NOP:
    case OPCODE_CUSTOMDATA:
    case OPCODE_OPAQUE_CUSTOMDATA:
    case OPCODE_SHADER_MESSAGE:
    case OPCODE_DCL_IMMEDIATE_CONSTANT_BUFFER:
    case OPCODE_SYNC:
    case OPCODE_STORE_UAV_TYPED:
    case OPCODE_STORE_RAW:
    case OPCODE_STORE_STRUCTURED: return VarType::Float;

    // operations that can be either type, also just return float (fixed up later)
    case OPCODE_SAMPLE:
    case OPCODE_SAMPLE_L:
    case OPCODE_SAMPLE_B:
    case OPCODE_SAMPLE_C:
    case OPCODE_SAMPLE_C_LZ:
    case OPCODE_GATHER4:
    case OPCODE_GATHER4_C:
    case OPCODE_GATHER4_PO:
    case OPCODE_GATHER4_PO_C:
    case OPCODE_SAMPLE_D:
    case OPCODE_RESINFO:
    case OPCODE_BUFINFO:
    case OPCODE_SAMPLE_INFO:
    case OPCODE_SAMPLE_POS:
    case OPCODE_EVAL_CENTROID:
    case OPCODE_EVAL_SAMPLE_INDEX:
    case OPCODE_EVAL_SNAPPED:
    case OPCODE_LOD:
    case OPCODE_LD:
    case OPCODE_LD_MS: return VarType::Float;

    case OPCODE_ADD:
    case OPCODE_MUL:
    case OPCODE_DIV:
    case OPCODE_MOV:
    case OPCODE_MOVC:
    case OPCODE_MAX:
    case OPCODE_MIN:
    case OPCODE_MAD:
    case OPCODE_DP2:
    case OPCODE_DP3:
    case OPCODE_DP4:
    case OPCODE_SINCOS:
    case OPCODE_F16TOF32:
    case OPCODE_F32TOF16:
    case OPCODE_FRC:
    case OPCODE_FTOI:
    case OPCODE_FTOU:
    case OPCODE_FTOD:
    case OPCODE_ROUND_PI:
    case OPCODE_ROUND_Z:
    case OPCODE_ROUND_NE:
    case OPCODE_ROUND_NI:
    case OPCODE_RCP:
    case OPCODE_RSQ:
    case OPCODE_SQRT:
    case OPCODE_LOG:
    case OPCODE_EXP:
    case OPCODE_LT:
    case OPCODE_GE:
    case OPCODE_EQ:
    case OPCODE_NE:
    case OPCODE_DERIV_RTX:
    case OPCODE_DERIV_RTX_COARSE:
    case OPCODE_DERIV_RTX_FINE:
    case OPCODE_DERIV_RTY:
    case OPCODE_DERIV_RTY_COARSE:
    case OPCODE_DERIV_RTY_FINE: return VarType::Float;

    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_IADD:
    case OPCODE_IMUL:
    case OPCODE_IMAD:
    case OPCODE_ISHL:
    case OPCODE_IGE:
    case OPCODE_IEQ:
    case OPCODE_ILT:
    case OPCODE_ISHR:
    case OPCODE_IBFE:
    case OPCODE_INE:
    case OPCODE_INEG:
    case OPCODE_IMAX:
    case OPCODE_IMIN:
    case OPCODE_SWAPC:
    case OPCODE_BREAK:
    case OPCODE_BREAKC:
    case OPCODE_IF:
    case OPCODE_ITOF:
    case OPCODE_DTOI: return VarType::SInt;

    case OPCODE_ATOMIC_IADD:
    case OPCODE_ATOMIC_IMAX:
    case OPCODE_ATOMIC_IMIN:
    case OPCODE_IMM_ATOMIC_IADD:
    case OPCODE_IMM_ATOMIC_IMAX:
    case OPCODE_IMM_ATOMIC_IMIN: return VarType::SInt;
    case OPCODE_ATOMIC_AND:
    case OPCODE_ATOMIC_OR:
    case OPCODE_ATOMIC_XOR:
    case OPCODE_ATOMIC_CMP_STORE:
    case OPCODE_ATOMIC_UMAX:
    case OPCODE_ATOMIC_UMIN:
    case OPCODE_IMM_ATOMIC_AND:
    case OPCODE_IMM_ATOMIC_OR:
    case OPCODE_IMM_ATOMIC_XOR:
    case OPCODE_IMM_ATOMIC_EXCH:
    case OPCODE_IMM_ATOMIC_CMP_EXCH:
    case OPCODE_IMM_ATOMIC_UMAX:
    case OPCODE_IMM_ATOMIC_UMIN: return VarType::UInt;

    case OPCODE_BFREV:
    case OPCODE_COUNTBITS:
    case OPCODE_FIRSTBIT_HI:
    case OPCODE_FIRSTBIT_LO:
    case OPCODE_FIRSTBIT_SHI:
    case OPCODE_UADDC:
    case OPCODE_USUBB:
    case OPCODE_UMAD:
    case OPCODE_UMUL:
    case OPCODE_UMIN:
    case OPCODE_IMM_ATOMIC_ALLOC:
    case OPCODE_IMM_ATOMIC_CONSUME:
    case OPCODE_UMAX:
    case OPCODE_UDIV:
    case OPCODE_UTOF:
    case OPCODE_USHR:
    case OPCODE_ULT:
    case OPCODE_UGE:
    case OPCODE_BFI:
    case OPCODE_UBFE:
    case OPCODE_NOT:
    case OPCODE_XOR:
    case OPCODE_LD_RAW:
    case OPCODE_LD_UAV_TYPED:
    case OPCODE_LD_STRUCTURED:
    case OPCODE_DTOU: return VarType::UInt;

    case OPCODE_DADD:
    case OPCODE_DMAX:
    case OPCODE_DMIN:
    case OPCODE_DMUL:
    case OPCODE_DEQ:
    case OPCODE_DNE:
    case OPCODE_DGE:
    case OPCODE_DLT:
    case OPCODE_DMOV:
    case OPCODE_DMOVC:
    case OPCODE_DTOF:
    case OPCODE_DDIV:
    case OPCODE_DFMA:
    case OPCODE_DRCP:
    case OPCODE_ITOD:
    case OPCODE_UTOD: return VarType::Double;

    case OPCODE_AMD_U64_ATOMIC:
    case OPCODE_NV_U64_ATOMIC: return VarType::UInt;

    default: RDCERR("Unhandled operation %d in shader debugging", op); return VarType::Float;
  }
}

bool OperationFlushing(const DXBCBytecode::OpcodeType &op)
{
  switch(op)
  {
    // float mathematical operations all flush denorms
    case OPCODE_ADD:
    case OPCODE_MUL:
    case OPCODE_DIV:
    case OPCODE_MAX:
    case OPCODE_MIN:
    case OPCODE_MAD:
    case OPCODE_DP2:
    case OPCODE_DP3:
    case OPCODE_DP4:
    case OPCODE_SINCOS:
    case OPCODE_FRC:
    case OPCODE_ROUND_PI:
    case OPCODE_ROUND_Z:
    case OPCODE_ROUND_NE:
    case OPCODE_ROUND_NI:
    case OPCODE_RCP:
    case OPCODE_RSQ:
    case OPCODE_SQRT:
    case OPCODE_LOG:
    case OPCODE_EXP:
    case OPCODE_LT:
    case OPCODE_GE:
    case OPCODE_EQ:
    case OPCODE_NE: return true;

    // can't generate denorms, or denorm inputs are implicitly rounded to 0, so don't bother
    // flushing
    case OPCODE_ITOF:
    case OPCODE_UTOF:
    case OPCODE_FTOI:
    case OPCODE_FTOU: return false;

    // we have to flush this manually since the input is halves encoded in uints
    case OPCODE_F16TOF32:
    case OPCODE_F32TOF16: return false;

    // implementation defined if this should flush or not, we choose not.
    case OPCODE_DTOF:
    case OPCODE_FTOD: return false;

    // any I/O or data movement operation that does not manipulate the data, such as using the
    // ld(22.4.6) instruction to access Resource data, or executing mov instruction or conditional
    // move/swap instruction (excluding min or max instructions), must not alter data at all (so a
    // denorm remains denorm).
    case OPCODE_MOV:
    case OPCODE_MOVC:
    case OPCODE_LD:
    case OPCODE_LD_MS: return false;

    // sample operations flush denorms
    case OPCODE_SAMPLE:
    case OPCODE_SAMPLE_L:
    case OPCODE_SAMPLE_B:
    case OPCODE_SAMPLE_C:
    case OPCODE_SAMPLE_C_LZ:
    case OPCODE_SAMPLE_D:
    case OPCODE_GATHER4:
    case OPCODE_GATHER4_C:
    case OPCODE_GATHER4_PO:
    case OPCODE_GATHER4_PO_C: return true;

    // don't flush eval ops as some inputs may be uint
    case OPCODE_EVAL_CENTROID:
    case OPCODE_EVAL_SAMPLE_INDEX:
    case OPCODE_EVAL_SNAPPED: return false;

    // don't flush samplepos since an operand is scalar
    case OPCODE_SAMPLE_POS: return false;

    // unclear if these flush and it's unlikely denorms will come up, so conservatively flush
    case OPCODE_SAMPLE_INFO:
    case OPCODE_LOD:
    case OPCODE_DERIV_RTX:
    case OPCODE_DERIV_RTX_COARSE:
    case OPCODE_DERIV_RTX_FINE:
    case OPCODE_DERIV_RTY:
    case OPCODE_DERIV_RTY_COARSE:
    case OPCODE_DERIV_RTY_FINE: return true;

    // operations that don't work on floats don't flush
    case OPCODE_RESINFO:
    case OPCODE_BUFINFO:
    case OPCODE_LOOP:
    case OPCODE_CONTINUE:
    case OPCODE_CONTINUEC:
    case OPCODE_ENDLOOP:
    case OPCODE_SWITCH:
    case OPCODE_CASE:
    case OPCODE_DEFAULT:
    case OPCODE_ENDSWITCH:
    case OPCODE_ELSE:
    case OPCODE_ENDIF:
    case OPCODE_RET:
    case OPCODE_RETC:
    case OPCODE_DISCARD:
    case OPCODE_NOP:
    case OPCODE_CUSTOMDATA:
    case OPCODE_OPAQUE_CUSTOMDATA:
    case OPCODE_SHADER_MESSAGE:
    case OPCODE_DCL_IMMEDIATE_CONSTANT_BUFFER:
    case OPCODE_SYNC:
    case OPCODE_STORE_UAV_TYPED:
    case OPCODE_STORE_RAW:
    case OPCODE_STORE_STRUCTURED: return false;

    // integer operations don't flush
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_IADD:
    case OPCODE_IMUL:
    case OPCODE_IMAD:
    case OPCODE_ISHL:
    case OPCODE_IGE:
    case OPCODE_IEQ:
    case OPCODE_ILT:
    case OPCODE_ISHR:
    case OPCODE_IBFE:
    case OPCODE_INE:
    case OPCODE_INEG:
    case OPCODE_IMAX:
    case OPCODE_IMIN:
    case OPCODE_SWAPC:
    case OPCODE_BREAK:
    case OPCODE_BREAKC:
    case OPCODE_IF:
    case OPCODE_DTOI:
    case OPCODE_ATOMIC_IADD:
    case OPCODE_ATOMIC_IMAX:
    case OPCODE_ATOMIC_IMIN:
    case OPCODE_IMM_ATOMIC_IADD:
    case OPCODE_IMM_ATOMIC_IMAX:
    case OPCODE_IMM_ATOMIC_IMIN:
    case OPCODE_ATOMIC_AND:
    case OPCODE_ATOMIC_OR:
    case OPCODE_ATOMIC_XOR:
    case OPCODE_ATOMIC_CMP_STORE:
    case OPCODE_ATOMIC_UMAX:
    case OPCODE_ATOMIC_UMIN:
    case OPCODE_IMM_ATOMIC_AND:
    case OPCODE_IMM_ATOMIC_OR:
    case OPCODE_IMM_ATOMIC_XOR:
    case OPCODE_IMM_ATOMIC_EXCH:
    case OPCODE_IMM_ATOMIC_CMP_EXCH:
    case OPCODE_IMM_ATOMIC_UMAX:
    case OPCODE_IMM_ATOMIC_UMIN:
    case OPCODE_BFREV:
    case OPCODE_COUNTBITS:
    case OPCODE_FIRSTBIT_HI:
    case OPCODE_FIRSTBIT_LO:
    case OPCODE_FIRSTBIT_SHI:
    case OPCODE_UADDC:
    case OPCODE_USUBB:
    case OPCODE_UMAD:
    case OPCODE_UMUL:
    case OPCODE_UMIN:
    case OPCODE_IMM_ATOMIC_ALLOC:
    case OPCODE_IMM_ATOMIC_CONSUME:
    case OPCODE_UMAX:
    case OPCODE_UDIV:
    case OPCODE_USHR:
    case OPCODE_ULT:
    case OPCODE_UGE:
    case OPCODE_BFI:
    case OPCODE_UBFE:
    case OPCODE_NOT:
    case OPCODE_XOR:
    case OPCODE_LD_RAW:
    case OPCODE_LD_UAV_TYPED:
    case OPCODE_LD_STRUCTURED:
    case OPCODE_DTOU: return false;

    // doubles do not flush
    case OPCODE_DADD:
    case OPCODE_DMAX:
    case OPCODE_DMIN:
    case OPCODE_DMUL:
    case OPCODE_DEQ:
    case OPCODE_DNE:
    case OPCODE_DGE:
    case OPCODE_DLT:
    case OPCODE_DMOV:
    case OPCODE_DMOVC:
    case OPCODE_DDIV:
    case OPCODE_DFMA:
    case OPCODE_DRCP:
    case OPCODE_ITOD:
    case OPCODE_UTOD: return false;

    case OPCODE_AMD_U64_ATOMIC:
    case OPCODE_NV_U64_ATOMIC: return false;

    default: RDCERR("Unhandled operation %d in shader debugging", op); break;
  }

  return false;
}

bool OperandSwizzle(const Operation &op, const Operand &oper)
{
  switch(op.operation)
  {
    case OPCODE_SAMPLE:
    case OPCODE_SAMPLE_L:
    case OPCODE_SAMPLE_B:
    case OPCODE_SAMPLE_D:
    case OPCODE_SAMPLE_C:
    case OPCODE_SAMPLE_C_LZ:
    case OPCODE_LD:
    case OPCODE_LD_MS:
    case OPCODE_GATHER4:
    case OPCODE_GATHER4_C:
    case OPCODE_GATHER4_PO:
    case OPCODE_GATHER4_PO_C:
    case OPCODE_LOD:
    {
      // Per HLSL docs, "the swizzle on srcResource determines how to swizzle the 4-component result
      // coming back from the texture sample/filter". That swizzle should not be handled when
      // fetching the src operand
      if(oper.type == TYPE_RESOURCE)
        return false;
    }
    break;

    default: break;
  }

  return true;
}

void DoubleSet(ShaderVariable &var, const double in[2])
{
  var.value.f64v[0] = in[0];
  var.value.f64v[1] = in[1];
  var.type = VarType::Double;
}

void DoubleGet(const ShaderVariable &var, double out[2])
{
  out[0] = var.value.f64v[0];
  out[1] = var.value.f64v[1];
}

void TypedUAVStore(GlobalState::ViewFmt &fmt, byte *d, ShaderVariable var)
{
  if(fmt.byteWidth == 10)
  {
    uint32_t u = 0;

    if(fmt.fmt == CompType::UInt)
    {
      u |= (var.value.u32v[0] & 0x3ff) << 0;
      u |= (var.value.u32v[1] & 0x3ff) << 10;
      u |= (var.value.u32v[2] & 0x3ff) << 20;
      u |= (var.value.u32v[3] & 0x3) << 30;
    }
    else if(fmt.fmt == CompType::UNorm)
    {
      u = ConvertToR10G10B10A2(
          Vec4f(var.value.f32v[0], var.value.f32v[1], var.value.f32v[2], var.value.f32v[3]));
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
    memcpy(d, &u, sizeof(uint32_t));
  }
  else if(fmt.byteWidth == 11)
  {
    uint32_t u = ConvertToR11G11B10(Vec3f(var.value.f32v[0], var.value.f32v[1], var.value.f32v[2]));
    memcpy(d, &u, sizeof(uint32_t));
  }
  else if(fmt.byteWidth == 4)
  {
    uint32_t *u = (uint32_t *)d;

    for(int c = 0; c < fmt.numComps; c++)
      u[c] = var.value.u32v[c];
  }
  else if(fmt.byteWidth == 2)
  {
    if(fmt.fmt == CompType::Float)
    {
      uint16_t *u = (uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = ConvertToHalf(var.value.f32v[c]);
    }
    else if(fmt.fmt == CompType::UInt)
    {
      uint16_t *u = (uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = var.value.u32v[c] & 0xffff;
    }
    else if(fmt.fmt == CompType::SInt)
    {
      int16_t *i = (int16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        i[c] = (int16_t)RDCCLAMP(var.value.s32v[c], (int32_t)INT16_MIN, (int32_t)INT16_MAX);
    }
    else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
    {
      uint16_t *u = (uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(var.value.f32v[c], 0.0f, 1.0f) * float(0xffff) + 0.5f;
        u[c] = uint16_t(f);
      }
    }
    else if(fmt.fmt == CompType::SNorm)
    {
      int16_t *i = (int16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(var.value.f32v[c], -1.0f, 1.0f) * 0x7fff;

        if(f < 0.0f)
          i[c] = int16_t(f - 0.5f);
        else
          i[c] = int16_t(f + 0.5f);
      }
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }
  else if(fmt.byteWidth == 1)
  {
    if(fmt.fmt == CompType::UInt)
    {
      uint8_t *u = (uint8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = var.value.u32v[c] & 0xff;
    }
    else if(fmt.fmt == CompType::SInt)
    {
      int8_t *i = (int8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        i[c] = (int8_t)RDCCLAMP(var.value.s32v[c], (int32_t)INT8_MIN, (int32_t)INT8_MAX);
    }
    else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
    {
      uint8_t *u = (uint8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(var.value.f32v[c], 0.0f, 1.0f) * float(0xff) + 0.5f;
        u[c] = uint8_t(f);
      }
    }
    else if(fmt.fmt == CompType::SNorm)
    {
      int8_t *i = (int8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(var.value.f32v[c], -1.0f, 1.0f) * 0x7f;

        if(f < 0.0f)
          i[c] = int8_t(f - 0.5f);
        else
          i[c] = int8_t(f + 0.5f);
      }
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }
}

ShaderVariable TypedUAVLoad(GlobalState::ViewFmt &fmt, const byte *d)
{
  ShaderVariable result("", 0.0f, 0.0f, 0.0f, 0.0f);

  if(fmt.byteWidth == 10)
  {
    uint32_t u;
    memcpy(&u, d, sizeof(uint32_t));

    if(fmt.fmt == CompType::UInt)
    {
      result.value.u32v[0] = (u >> 0) & 0x3ff;
      result.value.u32v[1] = (u >> 10) & 0x3ff;
      result.value.u32v[2] = (u >> 20) & 0x3ff;
      result.value.u32v[3] = (u >> 30) & 0x003;
    }
    else if(fmt.fmt == CompType::UNorm)
    {
      Vec4f res = ConvertFromR10G10B10A2(u);
      result.value.f32v[0] = res.x;
      result.value.f32v[1] = res.y;
      result.value.f32v[2] = res.z;
      result.value.f32v[3] = res.w;
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }
  else if(fmt.byteWidth == 11)
  {
    uint32_t u;
    memcpy(&u, d, sizeof(uint32_t));

    Vec3f res = ConvertFromR11G11B10(u);
    result.value.f32v[0] = res.x;
    result.value.f32v[1] = res.y;
    result.value.f32v[2] = res.z;
    result.value.f32v[3] = 1.0f;
  }
  else
  {
    if(fmt.byteWidth == 4)
    {
      const uint32_t *u = (const uint32_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        result.value.u32v[c] = u[c];
    }
    else if(fmt.byteWidth == 2)
    {
      if(fmt.fmt == CompType::Float)
      {
        const uint16_t *u = (const uint16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.value.f32v[c] = ConvertFromHalf(u[c]);
      }
      else if(fmt.fmt == CompType::UInt)
      {
        const uint16_t *u = (const uint16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.value.u32v[c] = u[c];
      }
      else if(fmt.fmt == CompType::SInt)
      {
        const int16_t *in = (const int16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.value.s32v[c] = in[c];
      }
      else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
      {
        const uint16_t *u = (const uint16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.value.f32v[c] = float(u[c]) / float(0xffff);
      }
      else if(fmt.fmt == CompType::SNorm)
      {
        const int16_t *in = (const int16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
        {
          // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
          if(in[c] == -32768)
            result.value.f32v[c] = -1.0f;
          else
            result.value.f32v[c] = float(in[c]) / 32767.0f;
        }
      }
      else
      {
        RDCERR("Unexpected format type on buffer resource");
      }
    }
    else if(fmt.byteWidth == 1)
    {
      if(fmt.fmt == CompType::UInt)
      {
        const uint8_t *u = (const uint8_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.value.u32v[c] = u[c];
      }
      else if(fmt.fmt == CompType::SInt)
      {
        const int8_t *in = (const int8_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.value.s32v[c] = in[c];
      }
      else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
      {
        const uint8_t *u = (const uint8_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.value.f32v[c] = float(u[c]) / float(0xff);
      }
      else if(fmt.fmt == CompType::SNorm)
      {
        const int8_t *in = (const int8_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
        {
          // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
          if(in[c] == -128)
            result.value.f32v[c] = -1.0f;
          else
            result.value.f32v[c] = float(in[c]) / 127.0f;
        }
      }
      else
      {
        RDCERR("Unexpected format type on buffer resource");
      }
    }

    // fill in alpha with 1.0 or 1 as appropriate
    if(fmt.numComps < 4)
    {
      if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB ||
         fmt.fmt == CompType::SNorm || fmt.fmt == CompType::Float)
        result.value.f32v[3] = 1.0f;
      else
        result.value.u32v[3] = 1;
    }
  }

  return result;
}

// "NaN has special handling. If one source operand is NaN, then the other source operand is
// returned and the choice is made per-component. If both are NaN, any NaN representation is
// returned."

float dxbc_min(float a, float b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a < b ? a : b;
}

double dxbc_min(double a, double b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a < b ? a : b;
}

float dxbc_max(float a, float b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a >= b ? a : b;
}

double dxbc_max(double a, double b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a >= b ? a : b;
}

ShaderVariable sat(const ShaderVariable &v, const VarType type)
{
  ShaderVariable r = v;

  switch(type)
  {
    case VarType::SInt:
    {
      for(size_t i = 0; i < v.columns; i++)
        r.value.s32v[i] = v.value.s32v[i] < 0 ? 0 : (v.value.s32v[i] > 1 ? 1 : v.value.s32v[i]);
      break;
    }
    case VarType::UInt:
    {
      for(size_t i = 0; i < v.columns; i++)
        r.value.u32v[i] = v.value.u32v[i] ? 1 : 0;
      break;
    }
    case VarType::Float:
    {
      // "The saturate instruction result modifier performs the following operation on the result
      // values(s) from a floating point arithmetic operation that has _sat applied to it:
      //
      // min(1.0f, max(0.0f, value))
      //
      // where min() and max() in the above expression behave in the way min, max, dmin, or dmax
      // operate. "

      for(size_t i = 0; i < v.columns; i++)
      {
        r.value.f32v[i] = dxbc_min(1.0f, dxbc_max(0.0f, v.value.f32v[i]));
      }

      break;
    }
    case VarType::Double:
    {
      double src[2];
      DoubleGet(v, src);

      double dst[2];
      dst[0] = dxbc_min(1.0, dxbc_max(0.0, src[0]));
      dst[1] = dxbc_min(1.0, dxbc_max(0.0, src[1]));

      DoubleSet(r, dst);
      break;
    }
    default:
      RDCFATAL(
          "Unsupported type of variable %d in math operation.\n"
          "This is likely a bug in the asm extraction as such code isn't likely to be produced by "
          "fxc.",
          type);
  }

  r.type = type;

  return r;
}

ShaderVariable abs(const ShaderVariable &v, const VarType type)
{
  ShaderVariable r = v;

  switch(type)
  {
    case VarType::SInt:
    {
      for(size_t i = 0; i < v.columns; i++)
        r.value.s32v[i] = v.value.s32v[i] > 0 ? v.value.s32v[i] : -v.value.s32v[i];
      break;
    }
    case VarType::UInt:
    {
      break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < v.columns; i++)
        r.value.f32v[i] = v.value.f32v[i] > 0 ? v.value.f32v[i] : -v.value.f32v[i];
      break;
    }
    case VarType::Double:
    {
      double src[2];
      DoubleGet(v, src);

      double dst[2];
      dst[0] = src[0] > 0 ? src[0] : -src[0];
      dst[1] = src[1] > 0 ? src[1] : -src[1];

      DoubleSet(r, dst);
      break;
    }
    default:
      RDCFATAL(
          "Unsupported type of variable %d in math operation.\n"
          "This is likely a bug in the asm extraction as such code isn't likely to be produced by "
          "fxc.",
          type);
  }

  r.type = type;

  return r;
}

ShaderVariable neg(const ShaderVariable &v, const VarType type)
{
  ShaderVariable r = v;

  switch(type)
  {
    case VarType::SInt:
    {
      for(size_t i = 0; i < v.columns; i++)
        r.value.s32v[i] = -v.value.s32v[i];
      break;
    }
    case VarType::UInt:
    {
      break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < v.columns; i++)
        r.value.f32v[i] = -v.value.f32v[i];
      break;
    }
    case VarType::Double:
    {
      double src[2];
      DoubleGet(v, src);

      double dst[2];
      dst[0] = -src[0];
      dst[1] = -src[1];

      DoubleSet(r, dst);
      break;
    }
    default:
      RDCFATAL(
          "Unsupported type of variable %d in math operation.\n"
          "This is likely a bug in the asm extraction as such code isn't likely to be produced by "
          "fxc.",
          type);
  }

  r.type = type;

  return r;
}

ShaderVariable mul(const ShaderVariable &a, const ShaderVariable &b, const VarType type)
{
  ShaderVariable r = a;

  switch(type)
  {
    case VarType::SInt:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.s32v[i] = a.value.s32v[i] * b.value.s32v[i];
      break;
    }
    case VarType::UInt:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.u32v[i] = a.value.u32v[i] * b.value.u32v[i];
      break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.f32v[i] = a.value.f32v[i] * b.value.f32v[i];
      break;
    }
    case VarType::Double:
    {
      double src0[2], src1[2];
      DoubleGet(a, src0);
      DoubleGet(b, src1);

      double dst[2];
      dst[0] = src0[0] * src1[0];
      dst[1] = src0[1] * src1[1];

      DoubleSet(r, dst);
      break;
    }
    default:
      RDCFATAL(
          "Unsupported type of variable %d in math operation.\n"
          "This is likely a bug in the asm extraction as such code isn't likely to be produced by "
          "fxc.",
          type);
  }

  r.type = type;

  return r;
}

ShaderVariable div(const ShaderVariable &a, const ShaderVariable &b, const VarType type)
{
  ShaderVariable r = a;

  switch(type)
  {
    case VarType::SInt:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.s32v[i] = a.value.s32v[i] / b.value.s32v[i];
      break;
    }
    case VarType::UInt:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.u32v[i] = a.value.u32v[i] / b.value.u32v[i];
      break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.f32v[i] = a.value.f32v[i] / b.value.f32v[i];
      break;
    }
    case VarType::Double:
    {
      double src0[2], src1[2];
      DoubleGet(a, src0);
      DoubleGet(b, src1);

      double dst[2];
      dst[0] = src0[0] / src1[0];
      dst[1] = src0[1] / src1[1];

      DoubleSet(r, dst);
      break;
    }
    default:
      RDCFATAL(
          "Unsupported type of variable %d in math operation.\n"
          "This is likely a bug in the asm extraction as such code isn't likely to be produced by "
          "fxc.",
          type);
  }

  r.type = type;

  return r;
}

ShaderVariable add(const ShaderVariable &a, const ShaderVariable &b, const VarType type)
{
  ShaderVariable r = a;

  switch(type)
  {
    case VarType::SInt:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.s32v[i] = a.value.s32v[i] + b.value.s32v[i];
      break;
    }
    case VarType::UInt:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.u32v[i] = a.value.u32v[i] + b.value.u32v[i];
      break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.f32v[i] = a.value.f32v[i] + b.value.f32v[i];
      break;
    }
    case VarType::Double:
    {
      double src0[2], src1[2];
      DoubleGet(a, src0);
      DoubleGet(b, src1);

      double dst[2];
      dst[0] = src0[0] + src1[0];
      dst[1] = src0[1] + src1[1];

      DoubleSet(r, dst);
      break;
    }
    default:
      RDCFATAL(
          "Unsupported type of variable %d in math operation.\n"
          "This is likely a bug in the asm extraction as such code isn't likely to be produced by "
          "fxc.",
          type);
  }

  r.type = type;

  return r;
}

ShaderVariable sub(const ShaderVariable &a, const ShaderVariable &b, const VarType type)
{
  return add(a, neg(b, type), type);
}

ThreadState::ThreadState(int workgroupIdx, GlobalState &globalState, const DXBC::DXBCContainer *dxbc)
    : global(globalState)
{
  workgroupIndex = workgroupIdx;
  done = false;
  nextInstruction = 0;
  reflection = dxbc->GetReflection();
  program = dxbc->GetDXBCByteCode();
  debug = dxbc->GetDebugInfo();
  RDCEraseEl(semantics);

  program->SetupRegisterFile(variables);
}

bool ThreadState::Finished() const
{
  return program && (done || nextInstruction >= (int)program->GetNumInstructions());
}

ShaderEvents ThreadState::AssignValue(ShaderVariable &dst, uint32_t dstIndex,
                                      const ShaderVariable &src, uint32_t srcIndex, bool flushDenorm)
{
  ShaderEvents flags = ShaderEvents::NoEvent;

  if(src.type == VarType::Float)
  {
    float ft = src.value.f32v[srcIndex];
    if(!RDCISFINITE(ft))
      flags |= ShaderEvents::GeneratedNanOrInf;
  }
  else if(src.type == VarType::Double)
  {
    double dt = src.value.f64v[srcIndex];
    if(!RDCISFINITE(dt))
      flags |= ShaderEvents::GeneratedNanOrInf;
  }

  dst.value.u32v[dstIndex] = src.value.u32v[srcIndex];

  if(flushDenorm && src.type == VarType::Float)
    dst.value.f32v[dstIndex] = flush_denorm(dst.value.f32v[dstIndex]);

  return flags;
}

void ThreadState::SetDst(ShaderDebugState *state, const Operand &dstoper, const Operation &op,
                         const ShaderVariable &val)
{
  ShaderVariable *v = NULL;

  uint32_t indices[4] = {0};

  RDCASSERT(dstoper.indices.size() <= 4);

  for(size_t i = 0; i < dstoper.indices.size(); i++)
  {
    if(dstoper.indices[i].absolute)
      indices[i] = (uint32_t)dstoper.indices[i].index;
    else
      indices[i] = 0;

    if(dstoper.indices[i].relative)
    {
      ShaderVariable idx = GetSrc(dstoper.indices[i].operand, op, false);

      indices[i] += idx.value.s32v[0];
    }
  }

  switch(dstoper.type)
  {
    case TYPE_TEMP:
    case TYPE_INDEXABLE_TEMP:
    case TYPE_OUTPUT:
    case TYPE_OUTPUT_DEPTH:
    case TYPE_OUTPUT_DEPTH_LESS_EQUAL:
    case TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
    case TYPE_OUTPUT_STENCIL_REF:
    case TYPE_OUTPUT_COVERAGE_MASK:
    {
      uint32_t idx = program->GetRegisterIndex(dstoper.type, indices[0]);

      if(idx < variables.size())
        v = &variables[idx];
      break;
    }
    case TYPE_INPUT:
    case TYPE_CONSTANT_BUFFER:
    {
      RDCERR(
          "Attempt to write to read-only operand (input, cbuffer, etc).\n"
          "This is likely a bug in the asm extraction as such code isn't likely to be produced by "
          "fxc.");
      break;
    }
    case TYPE_NULL:
    {
      // nothing to do! silently return here
      return;
    }
    default:
    {
      RDCERR("Currently unsupported destination operand type!", dstoper.type);
      break;
    }
  }

  ShaderVariable *changeVar = v;

  if(dstoper.type == TYPE_INDEXABLE_TEMP)
  {
    RDCASSERTEQUAL(dstoper.indices.size(), 2);
    if(dstoper.indices.size() == 2)
    {
      RDCASSERT(indices[1] < (uint32_t)v->members.size(), indices[1], v->members.size());
      if(indices[1] < (uint32_t)v->members.size())
        v = &v->members[indices[1]];
    }
  }

  if(!v)
  {
    RDCERR("Couldn't get output register %s %u, write will be lost", ToStr(dstoper.type).c_str(),
           indices[0]);
    return;
  }

  ShaderVariable right = val;

  RDCASSERT(v->rows == 1 && right.rows == 1);
  RDCASSERT(right.columns <= 4);

  bool flushDenorm = OperationFlushing(op.operation);

  // behaviour for scalar and vector masks are slightly different.
  // in a scalar operation like r0.z = r4.x + r6.y
  // then when doing the set to dest we must write into the .z
  // from the only component - x - since the result is scalar.
  // in a vector operation like r0.zw = r4.xxxy + r6.yyyz
  // then we must write from matching component to matching component

  if(op.saturate())
    right = sat(right, OperationType(op.operation));

  ShaderVariableChange change = {*changeVar};

  ShaderEvents flags = ShaderEvents::NoEvent;

  if(dstoper.comps[0] != 0xff && dstoper.comps[1] == 0xff && dstoper.comps[2] == 0xff &&
     dstoper.comps[3] == 0xff)
  {
    RDCASSERT(dstoper.comps[0] != 0xff);

    flags |= AssignValue(*v, dstoper.comps[0], right, 0, flushDenorm);
  }
  else
  {
    int compsWritten = 0;
    for(size_t i = 0; i < 4; i++)
    {
      // if comps value is 0xff, we should not write to this component
      if(dstoper.comps[i] != 0xff)
      {
        RDCASSERT(dstoper.comps[i] < v->columns);
        flags |= AssignValue(*v, dstoper.comps[i], right, dstoper.comps[i], flushDenorm);
        compsWritten++;
      }
    }

    if(compsWritten == 0)
      flags |= AssignValue(*v, 0, right, 0, flushDenorm);
  }

  if(state)
  {
    state->flags |= flags;
    change.after = *changeVar;
    state->changes.push_back(change);
  }
}

ShaderVariable ThreadState::DDX(bool fine, const rdcarray<ThreadState> &quad,
                                const DXBCBytecode::Operand &oper,
                                const DXBCBytecode::Operation &op) const
{
  ShaderVariable ret;

  VarType optype = OperationType(op.operation);

  int quadIndex = workgroupIndex;

  if(!fine)
  {
    // use top-left pixel's neighbours
    ret = sub(quad[1].GetSrc(oper, op), quad[0].GetSrc(oper, op), optype);
  }
  // find direct neighbours - left pixel in the quad
  else if(quadIndex % 2 == 0)
  {
    ret = sub(quad[quadIndex + 1].GetSrc(oper, op), quad[quadIndex].GetSrc(oper, op), optype);
  }
  else
  {
    ret = sub(quad[quadIndex].GetSrc(oper, op), quad[quadIndex - 1].GetSrc(oper, op), optype);
  }

  return ret;
}

ShaderVariable ThreadState::DDY(bool fine, const rdcarray<ThreadState> &quad,
                                const DXBCBytecode::Operand &oper,
                                const DXBCBytecode::Operation &op) const
{
  ShaderVariable ret;

  VarType optype = OperationType(op.operation);

  int quadIndex = workgroupIndex;

  if(!fine)
  {
    // use top-left pixel's neighbours
    ret = sub(quad[2].GetSrc(oper, op), quad[0].GetSrc(oper, op), optype);
  }
  // find direct neighbours - top pixel in the quad
  else if(quadIndex / 2 == 0)
  {
    ret = sub(quad[quadIndex + 2].GetSrc(oper, op), quad[quadIndex].GetSrc(oper, op), optype);
  }
  else
  {
    ret = sub(quad[quadIndex].GetSrc(oper, op), quad[quadIndex - 2].GetSrc(oper, op), optype);
  }

  return ret;
}

ShaderVariable ThreadState::GetSrc(const Operand &oper, const Operation &op, bool allowFlushing) const
{
  ShaderVariable v;

  uint32_t indices[4] = {0};

  RDCASSERT(oper.indices.size() <= 4);

  for(size_t i = 0; i < oper.indices.size(); i++)
  {
    if(oper.indices[i].absolute)
      indices[i] = (uint32_t)oper.indices[i].index;
    else
      indices[i] = 0;

    if(oper.indices[i].relative)
    {
      ShaderVariable idx = GetSrc(oper.indices[i].operand, op, false);

      indices[i] += idx.value.s32v[0];
    }
  }

  // is this type a flushable input (for float operations)
  bool flushable = allowFlushing;

  switch(oper.type)
  {
    case TYPE_TEMP:
    case TYPE_INDEXABLE_TEMP:
    case TYPE_OUTPUT:
    {
      uint32_t idx = program->GetRegisterIndex(oper.type, indices[0]);

      if(idx < variables.size())
      {
        if(oper.type == TYPE_INDEXABLE_TEMP)
        {
          RDCASSERTEQUAL(oper.indices.size(), 2);
          RDCASSERT(indices[1] < variables[idx].members.size(), indices[1],
                    variables[idx].members.size());
          if(oper.indices.size() == 2 && indices[1] < variables[idx].members.size())
          {
            v = variables[idx].members[indices[1]];
          }
          else
          {
            v = ShaderVariable(rdcstr(), indices[0], indices[0], indices[0], indices[0]);
          }
        }
        else
        {
          v = variables[idx];
        }
      }
      else
      {
        v = ShaderVariable(rdcstr(), indices[0], indices[0], indices[0], indices[0]);
      }

      break;
    }
    case TYPE_INPUT:
    {
      RDCASSERT(indices[0] < (uint32_t)inputs.size());

      if(indices[0] < (uint32_t)inputs.size())
        v = inputs[indices[0]];
      else
        v = ShaderVariable(rdcstr(), indices[0], indices[0], indices[0], indices[0]);

      break;
    }

    // instructions referencing group shared memory handle it specially (the operand
    // itself just names the groupshared memory region, there's a separate dst address
    // operand).
    case TYPE_THREAD_GROUP_SHARED_MEMORY:

    case TYPE_RESOURCE:
    case TYPE_SAMPLER:
    case TYPE_UNORDERED_ACCESS_VIEW:
    case TYPE_NULL:
    case TYPE_RASTERIZER:
    {
      // should be handled specially by instructions that expect these types of
      // argument but let's be sane and include the indices. For indexing into
      // resource arrays, indices[0] will contain the logical identifier, and
      // indices[1] will contain the shader register correlating to the resource
      // array index requested (in absolute terms, not relative to the start register)
      v = ShaderVariable(rdcstr(), indices[0], indices[1], indices[2], indices[3]);
      flushable = false;
      break;
    }
    case TYPE_IMMEDIATE32:
    case TYPE_IMMEDIATE64:
    {
      v.name = "Immediate";

      flushable = false;

      if(oper.numComponents == NUMCOMPS_1)
      {
        v.rows = 1;
        v.columns = 1;
      }
      else if(oper.numComponents == NUMCOMPS_4)
      {
        v.rows = 1;
        v.columns = 4;
      }
      else
      {
        RDCFATAL("N-wide vectors not supported (per hlsl spec)");
      }

      if(oper.type == TYPE_IMMEDIATE32)
      {
        for(size_t i = 0; i < v.columns; i++)
        {
          v.value.s32v[i] = (int32_t)oper.values[i];
        }
      }
      else
      {
        RDCUNIMPLEMENTED(
            "Encountered immediate 64bit value!");    // need to figure out what to do here.
      }

      break;
    }
    case TYPE_CONSTANT_BUFFER:
    {
      int cb = -1;

      // For this operand, index 0 is always the logical identifier that we can use to
      // lookup the correct cbuffer. The location of the access entry differs for SM5.1,
      // as index 1 stores the shader register.
      bool isCBArray = false;
      uint32_t cbIdentifier = indices[0];
      uint32_t cbArrayIndex = ~0U;
      uint32_t cbLookup = program->IsShaderModel51() ? indices[2] : indices[1];
      for(size_t i = 0; i < reflection->CBuffers.size(); i++)
      {
        if(reflection->CBuffers[i].identifier == cbIdentifier)
        {
          cb = (int)i;
          cbArrayIndex = indices[1] - reflection->CBuffers[i].reg;
          isCBArray = reflection->CBuffers[i].bindCount > 1;
          break;
        }
      }

      RDCASSERTMSG("Invalid cbuffer lookup", cb != -1 && cb < global.constantBlocks.count(), cb,
                   global.constantBlocks.count());

      if(cb >= 0 && cb < global.constantBlocks.count())
      {
        RDCASSERTMSG("Invalid cbuffer array index",
                     !isCBArray || cbArrayIndex < (uint32_t)global.constantBlocks[cb].members.count(),
                     isCBArray, cb, global.constantBlocks[cb].members.count());

        if(!isCBArray || cbArrayIndex < (uint32_t)global.constantBlocks[cb].members.count())
        {
          const rdcarray<ShaderVariable> &targetVars =
              isCBArray ? global.constantBlocks[cb].members[cbArrayIndex].members
                        : global.constantBlocks[cb].members;
          RDCASSERTMSG("Out of bounds cbuffer lookup", cbLookup < (uint32_t)targetVars.count(),
                       cbLookup, targetVars.count());

          if(cbLookup < (uint32_t)targetVars.count())
            v = targetVars[cbLookup];
          else
            v = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
        }
        else
        {
          v = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
        }
      }
      else
      {
        v = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
      }

      break;
    }
    case TYPE_IMMEDIATE_CONSTANT_BUFFER:
    {
      v = ShaderVariable(rdcstr(), 0, 0, 0, 0);

      const rdcarray<uint32_t> &icb = program->GetImmediateConstantBuffer();

      // if this Vec4f is entirely in the ICB
      if(indices[0] <= icb.size() / 4 - 1)
      {
        for(size_t i = 0; i < 4; i++)
          v.value.u32v[i] = icb[indices[0] * 4 + i];
      }
      else
      {
        // ICBs are always a multiple of Vec4fs, so no need to do a partial read (like in a normal
        // CB)
        RDCWARN(
            "Shader read off the end of an immediate constant buffer. Bug in shader or simulation? "
            "Clamping to 0s");
      }

      break;
    }
    case TYPE_INPUT_THREAD_GROUP_ID:
    {
      v = ShaderVariable("vThreadGroupID", semantics.GroupID[0], semantics.GroupID[1],
                         semantics.GroupID[2], (uint32_t)0);

      break;
    }
    case TYPE_INPUT_THREAD_ID:
    {
      uint32_t numthreads[3] = {0, 0, 0};

      for(size_t i = 0; i < program->GetNumDeclarations(); i++)
      {
        const Declaration &decl = program->GetDeclaration(i);

        if(decl.declaration == OPCODE_DCL_THREAD_GROUP)
        {
          numthreads[0] = decl.groupSize[0];
          numthreads[1] = decl.groupSize[1];
          numthreads[2] = decl.groupSize[2];
        }
      }

      RDCASSERT(numthreads[0] >= 1 && numthreads[0] <= 1024);
      RDCASSERT(numthreads[1] >= 1 && numthreads[1] <= 1024);
      RDCASSERT(numthreads[2] >= 1 && numthreads[2] <= 64);
      RDCASSERT(numthreads[0] * numthreads[1] * numthreads[2] <= 1024);

      v = ShaderVariable("vThreadID", semantics.GroupID[0] * numthreads[0] + semantics.ThreadID[0],
                         semantics.GroupID[1] * numthreads[1] + semantics.ThreadID[1],
                         semantics.GroupID[2] * numthreads[2] + semantics.ThreadID[2], (uint32_t)0);

      break;
    }
    case TYPE_INPUT_THREAD_ID_IN_GROUP:
    {
      v = ShaderVariable("vThreadIDInGroup", semantics.ThreadID[0], semantics.ThreadID[1],
                         semantics.ThreadID[2], (uint32_t)0);

      break;
    }
    case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
    {
      uint32_t numthreads[3] = {0, 0, 0};

      for(size_t i = 0; i < program->GetNumDeclarations(); i++)
      {
        const Declaration &decl = program->GetDeclaration(i);

        if(decl.declaration == OPCODE_DCL_THREAD_GROUP)
        {
          numthreads[0] = decl.groupSize[0];
          numthreads[1] = decl.groupSize[1];
          numthreads[2] = decl.groupSize[2];
        }
      }

      RDCASSERT(numthreads[0] >= 1 && numthreads[0] <= 1024);
      RDCASSERT(numthreads[1] >= 1 && numthreads[1] <= 1024);
      RDCASSERT(numthreads[2] >= 1 && numthreads[2] <= 64);
      RDCASSERT(numthreads[0] * numthreads[1] * numthreads[2] <= 1024);

      uint32_t flattened = semantics.ThreadID[2] * numthreads[0] * numthreads[1] +
                           semantics.ThreadID[1] * numthreads[0] + semantics.ThreadID[0];

      v = ShaderVariable("vThreadIDInGroupFlattened", flattened, flattened, flattened, flattened);
      break;
    }
    case TYPE_INPUT_COVERAGE_MASK:
    {
      v = ShaderVariable("vCoverage", semantics.coverage, semantics.coverage, semantics.coverage,
                         semantics.coverage);
      break;
    }
    case TYPE_INPUT_PRIMITIVEID:
    {
      v = ShaderVariable("vPrimitiveID", semantics.primID, semantics.primID, semantics.primID,
                         semantics.primID);
      break;
    }
    default:
    {
      RDCERR("Currently unsupported operand type %d!", oper.type);

      v = ShaderVariable("vUnsupported", (uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0);

      break;
    }
  }

  if(OperandSwizzle(op, oper))
  {
    ShaderValue s = v.value;

    // perform swizzling
    v.value.u32v[0] = s.u32v[oper.comps[0] == 0xff ? 0 : oper.comps[0]];
    v.value.u32v[1] = s.u32v[oper.comps[1] == 0xff ? 1 : oper.comps[1]];
    v.value.u32v[2] = s.u32v[oper.comps[2] == 0xff ? 2 : oper.comps[2]];
    v.value.u32v[3] = s.u32v[oper.comps[3] == 0xff ? 3 : oper.comps[3]];

    if(oper.comps[0] != 0xff && oper.comps[1] == 0xff && oper.comps[2] == 0xff &&
       oper.comps[3] == 0xff)
      v.columns = 1;
    else
      v.columns = 4;
  }
  else
  {
    v.columns = 4;
  }

  if(oper.flags & Operand::FLAG_ABS)
  {
    v = abs(v, OperationType(op.operation));
  }

  if(oper.flags & Operand::FLAG_NEG)
  {
    v = neg(v, OperationType(op.operation));
  }

  if(OperationFlushing(op.operation) && flushable)
  {
    for(int i = 0; i < 4; i++)
      v.value.f32v[i] = flush_denorm(v.value.f32v[i]);
  }

  return v;
}

static uint32_t BitwiseReverseLSB16(uint32_t x)
{
  // Reverse the bits in x, then discard the lower half
  // https://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel
  x = ((x >> 1) & 0x55555555) | ((x & 0x55555555) << 1);
  x = ((x >> 2) & 0x33333333) | ((x & 0x33333333) << 2);
  x = ((x >> 4) & 0x0F0F0F0F) | ((x & 0x0F0F0F0F) << 4);
  x = ((x >> 8) & 0x00FF00FF) | ((x & 0x00FF00FF) << 8);
  return x << 16;
}

static uint32_t PopCount(uint32_t x)
{
  // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

void FlattenSingleVariable(const rdcstr &cbufferName, uint32_t byteOffset, const rdcstr &basename,
                           const ShaderVariable &v, rdcarray<ShaderVariable> &outvars,
                           rdcarray<SourceVariableMapping> &sourcevars)
{
  size_t outIdx = byteOffset / 16;
  size_t outComp = (byteOffset % 16) / 4;

  if(v.RowMajor())
    outvars.resize(RDCMAX(outIdx + v.rows, outvars.size()));
  else
    outvars.resize(RDCMAX(outIdx + v.columns, outvars.size()));

  if(outvars[outIdx].columns > 0)
  {
    // if we already have a variable in this slot, just copy the data for this variable and add the
    // source mapping.
    // We should not overlap into the next register as that's not allowed.
    memcpy(&outvars[outIdx].value.u32v[outComp], &v.value.u32v[0], sizeof(uint32_t) * v.columns);

    SourceVariableMapping mapping;
    mapping.name = basename;
    mapping.type = v.type;
    mapping.rows = v.rows;
    mapping.columns = v.columns;
    mapping.offset = byteOffset;
    mapping.variables.resize(v.columns);

    for(int i = 0; i < v.columns; i++)
    {
      mapping.variables[i].type = DebugVariableType::Constant;
      mapping.variables[i].name = StringFormat::Fmt("%s[%u]", cbufferName.c_str(), outIdx);
      mapping.variables[i].component = uint16_t(outComp + i);
    }

    sourcevars.push_back(mapping);
  }
  else
  {
    const uint32_t numRegisters = v.RowMajor() ? v.rows : v.columns;
    for(uint32_t reg = 0; reg < numRegisters; reg++)
    {
      outvars[outIdx + reg].rows = 1;
      outvars[outIdx + reg].type = VarType::Unknown;
      outvars[outIdx + reg].columns = v.columns;
      outvars[outIdx + reg].flags = v.flags;
    }

    if(v.RowMajor())
    {
      for(size_t ri = 0; ri < v.rows; ri++)
        memcpy(&outvars[outIdx + ri].value.u32v[0], &v.value.u32v[ri * v.columns],
               sizeof(uint32_t) * v.columns);
    }
    else
    {
      // if we have a matrix stored in column major order, we need to transpose it back so we can
      // unroll it into vectors.
      for(size_t ci = 0; ci < v.columns; ci++)
        for(size_t ri = 0; ri < v.rows; ri++)
          outvars[outIdx + ci].value.u32v[ri] = v.value.u32v[ri * v.columns + ci];
    }

    SourceVariableMapping mapping;
    mapping.name = basename;
    mapping.type = v.type;
    mapping.rows = v.rows;
    mapping.columns = v.columns;
    mapping.offset = byteOffset;
    mapping.variables.resize(v.rows * v.columns);

    RDCASSERT(outComp == 0 || v.rows == 1, outComp, v.rows);

    size_t i = 0;
    for(uint8_t r = 0; r < v.rows; r++)
    {
      for(uint8_t c = 0; c < v.columns; c++)
      {
        size_t regIndex = outIdx + (v.RowMajor() ? r : c);
        size_t compIndex = outComp + (v.RowMajor() ? c : r);

        mapping.variables[i].type = DebugVariableType::Constant;
        mapping.variables[i].name = StringFormat::Fmt("%s[%zu]", cbufferName.c_str(), regIndex);
        mapping.variables[i].component = uint16_t(compIndex);
        i++;
      }
    }

    sourcevars.push_back(mapping);
  }
}

void FlattenVariables(const rdcstr &cbufferName, const rdcarray<ShaderConstant> &constants,
                      const rdcarray<ShaderVariable> &invars, rdcarray<ShaderVariable> &outvars,
                      const rdcstr &prefix, uint32_t baseOffset,
                      rdcarray<SourceVariableMapping> &sourceVars)
{
  RDCASSERTEQUAL(constants.size(), invars.size());

  for(size_t i = 0; i < constants.size(); i++)
  {
    const ShaderConstant &c = constants[i];
    const ShaderVariable &v = invars[i];

    uint32_t byteOffset = baseOffset + c.byteOffset;

    rdcstr basename = prefix + rdcstr(v.name);

    if(v.type == VarType::Struct)
    {
      // check if this is an array of structs or not
      if(c.type.elements == 1)
      {
        FlattenVariables(cbufferName, c.type.members, v.members, outvars, basename + ".",
                         byteOffset, sourceVars);
      }
      else
      {
        for(int m = 0; m < v.members.count(); m++)
        {
          FlattenVariables(cbufferName, c.type.members, v.members[m].members, outvars,
                           StringFormat::Fmt("%s[%zu].", basename.c_str(), m),
                           byteOffset + m * c.type.arrayByteStride, sourceVars);
        }
      }
    }
    else if(c.type.elements > 1 || (v.rows == 0 && v.columns == 0) || !v.members.empty())
    {
      for(int m = 0; m < v.members.count(); m++)
      {
        FlattenSingleVariable(cbufferName, byteOffset + m * c.type.arrayByteStride,
                              StringFormat::Fmt("%s[%zu]", basename.c_str(), m), v.members[m],
                              outvars, sourceVars);
      }
    }
    else
    {
      FlattenSingleVariable(cbufferName, byteOffset, basename, v, outvars, sourceVars);
    }
  }
}

void ThreadState::MarkResourceAccess(ShaderDebugState *state, DXBCBytecode::OperandType type,
                                     const BindingSlot &slot)
{
  if(state == NULL)
    return;

  if(type != DXBCBytecode::TYPE_RESOURCE && type != DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
    return;

  state->changes.push_back(ShaderVariableChange());
  ShaderVariableChange &change = state->changes.back();
  change.after.rows = change.after.columns = 1;
  change.after.type = (type == DXBCBytecode::TYPE_RESOURCE) ? VarType::ReadOnlyResource
                                                            : VarType::ReadWriteResource;

  uint32_t reg = slot.shaderRegister;
  uint32_t arrIdx = 0;

  const rdcarray<DXBC::ShaderInputBind> &shaderBinds =
      (type == DXBCBytecode::TYPE_RESOURCE) ? reflection->SRVs : reflection->UAVs;
  for(uint32_t i = 0; i < shaderBinds.size(); ++i)
  {
    const DXBC::ShaderInputBind &bind = shaderBinds[i];
    if(bind.space == slot.registerSpace && bind.reg <= slot.shaderRegister &&
       (bind.bindCount == ~0U || slot.shaderRegister < bind.reg + bind.bindCount))
    {
      reg = bind.reg;
      arrIdx = slot.shaderRegister - bind.reg;

      char prefix = (type == DXBCBytecode::TYPE_RESOURCE) ? 't' : 'u';
      if(program->IsShaderModel51())
        prefix = (char)toupper(prefix);

      uint32_t resIdx = GetLogicalIdentifierForBindingSlot(*program, type, slot);

      if(bind.bindCount == 1)
        change.after.name = StringFormat::Fmt("%c%u", prefix, resIdx);
      else
        change.after.name = StringFormat::Fmt("%c%u[%u]", prefix, resIdx, arrIdx);

      change.after.SetBindIndex(ShaderBindIndex((type == DXBCBytecode::TYPE_RESOURCE)
                                                    ? DescriptorCategory::ReadOnlyResource
                                                    : DescriptorCategory::ReadWriteResource,
                                                i, arrIdx));

      break;
    }
  }

  // Check whether this resource was visited before
  bool found = false;
  ShaderBindIndex bp = change.after.GetBindIndex();
  rdcarray<ShaderBindIndex> &accessed =
      (type == DXBCBytecode::TYPE_RESOURCE) ? m_accessedSRVs : m_accessedUAVs;
  for(size_t i = 0; i < accessed.size(); ++i)
  {
    if(accessed[i] == bp)
    {
      found = true;
      break;
    }
  }

  if(found)
    change.before = change.after;
  else
    accessed.push_back(bp);
}

void ThreadState::PrepareInitial(ShaderDebugState &initial)
{
  for(const ShaderVariable &v : variables)
    initial.changes.push_back({ShaderVariable(), v});

  if(debug)
  {
    const Operation &nextOp = program->GetInstruction(0);
    debug->GetCallstack(0, nextOp.offset, initial.callstack);
  }
}

void ThreadState::StepNext(ShaderDebugState *state, DebugAPIWrapper *apiWrapper,
                           const rdcarray<ThreadState> &prevWorkgroup)
{
  if(nextInstruction >= program->GetNumInstructions())
    return;

  const Operation &op = program->GetInstruction((size_t)nextInstruction);

  apiWrapper->SetCurrentInstruction(nextInstruction);
  nextInstruction++;

  if(nextInstruction >= program->GetNumInstructions())
    nextInstruction--;

  if(state && debug)
  {
    const Operation &nextOp = program->GetInstruction((size_t)nextInstruction);
    debug->GetCallstack(nextInstruction, nextOp.offset, state->callstack);
  }

  rdcarray<ShaderVariable> srcOpers;

  VarType optype = OperationType(op.operation);

  for(size_t i = 1; i < op.operands.size(); i++)
    srcOpers.push_back(GetSrc(op.operands[i], op));

  switch(op.operation)
  {
      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // Math operations

    case OPCODE_DADD:
    case OPCODE_IADD:
    case OPCODE_ADD:
      SetDst(state, op.operands[0], op, add(srcOpers[0], srcOpers[1], optype));
      break;
    case OPCODE_DDIV:
    case OPCODE_DIV:
      SetDst(state, op.operands[0], op, div(srcOpers[0], srcOpers[1], optype));
      break;
    case OPCODE_UDIV:
    {
      ShaderVariable quot("", (uint32_t)0xffffffff, (uint32_t)0xffffffff, (uint32_t)0xffffffff,
                          (uint32_t)0xffffffff);
      ShaderVariable rem("", (uint32_t)0xffffffff, (uint32_t)0xffffffff, (uint32_t)0xffffffff,
                         (uint32_t)0xffffffff);

      for(size_t i = 0; i < 4; i++)
      {
        if(srcOpers[2].value.u32v[i] != 0)
        {
          quot.value.u32v[i] = srcOpers[1].value.u32v[i] / srcOpers[2].value.u32v[i];
          rem.value.u32v[i] =
              srcOpers[1].value.u32v[i] - (quot.value.u32v[i] * srcOpers[2].value.u32v[i]);
        }
        else
        {
          if(state)
            state->flags |= ShaderEvents::GeneratedNanOrInf;
        }
      }

      if(op.operands[0].type != TYPE_NULL)
      {
        SetDst(state, op.operands[0], op, quot);
      }
      if(op.operands[1].type != TYPE_NULL)
      {
        SetDst(state, op.operands[1], op, rem);
      }
      break;
    }
    case OPCODE_BFREV:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        ret.value.u32v[i] = BitwiseReverseLSB16(srcOpers[0].value.u32v[i]);
      }

      SetDst(state, op.operands[0], op, ret);

      break;
    }
    case OPCODE_COUNTBITS:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        ret.value.u32v[i] = PopCount(srcOpers[0].value.u32v[i]);
      }

      SetDst(state, op.operands[0], op, ret);
      break;
    }
    case OPCODE_FIRSTBIT_HI:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        unsigned char found = BitScanReverse((DWORD *)&ret.value.u32v[i], srcOpers[0].value.u32v[i]);
        if(found == 0)
        {
          ret.value.u32v[i] = ~0U;
        }
        else
        {
          // firstbit_hi counts index 0 as the MSB, BitScanReverse counts index 0 as the LSB. So we
          // need to invert
          ret.value.u32v[i] = 31 - ret.value.u32v[i];
        }
      }

      SetDst(state, op.operands[0], op, ret);
      break;
    }
    case OPCODE_FIRSTBIT_LO:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        unsigned char found = BitScanForward((DWORD *)&ret.value.u32v[i], srcOpers[0].value.u32v[i]);
        if(found == 0)
          ret.value.u32v[i] = ~0U;
      }

      SetDst(state, op.operands[0], op, ret);
      break;
    }
    case OPCODE_FIRSTBIT_SHI:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        uint32_t u = srcOpers[0].value.u32v[i];
        if(srcOpers[0].value.s32v[i] < 0)
          u = ~u;

        unsigned char found = BitScanReverse((DWORD *)&ret.value.u32v[i], u);

        if(found == 0)
        {
          ret.value.u32v[i] = ~0U;
        }
        else
        {
          // firstbit_shi counts index 0 as the MSB, BitScanReverse counts index 0 as the LSB. So we
          // need to invert
          ret.value.u32v[i] = 31 - ret.value.u32v[i];
        }
      }

      SetDst(state, op.operands[0], op, ret);
      break;
    }
    case OPCODE_IMUL:
    case OPCODE_UMUL:
    {
      ShaderVariable hi("", 0U, 0U, 0U, 0U);
      ShaderVariable lo("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        if(op.operation == OPCODE_UMUL)
        {
          uint64_t res = uint64_t(srcOpers[1].value.u32v[i]) * uint64_t(srcOpers[2].value.u32v[i]);

          hi.value.u32v[i] = uint32_t((res >> 32) & 0xffffffff);
          lo.value.u32v[i] = uint32_t(res & 0xffffffff);
        }
        else if(op.operation == OPCODE_IMUL)
        {
          int64_t res = int64_t(srcOpers[1].value.s32v[i]) * int64_t(srcOpers[2].value.s32v[i]);

          hi.value.u32v[i] = uint32_t((res >> 32) & 0xffffffff);
          lo.value.u32v[i] = uint32_t(res & 0xffffffff);
        }
      }

      if(op.operands[0].type != TYPE_NULL)
      {
        SetDst(state, op.operands[0], op, hi);
      }
      if(op.operands[1].type != TYPE_NULL)
      {
        SetDst(state, op.operands[1], op, lo);
      }
      break;
    }
    case OPCODE_DMUL:
    case OPCODE_MUL:
      SetDst(state, op.operands[0], op, mul(srcOpers[0], srcOpers[1], optype));
      break;
    case OPCODE_UADDC:
    {
      uint64_t src[4];
      for(int i = 0; i < 4; i++)
        src[i] = (uint64_t)srcOpers[1].value.u32v[i];
      for(int i = 0; i < 4; i++)
        src[i] = (uint64_t)srcOpers[2].value.u32v[i];

      // set the rounded result
      uint32_t dst[4];

      for(int i = 0; i < 4; i++)
        dst[i] = (uint32_t)(src[i] & 0xffffffff);

      SetDst(state, op.operands[0], op, ShaderVariable(rdcstr(), dst[0], dst[1], dst[2], dst[3]));

      // if not null, set the carry bits
      if(op.operands[1].type != TYPE_NULL)
        SetDst(state, op.operands[1], op,
               ShaderVariable(rdcstr(), src[0] > 0xffffffff ? 1U : 0U, src[1] > 0xffffffff ? 1U : 0U,
                              src[2] > 0xffffffff ? 1U : 0U, src[3] > 0xffffffff ? 1U : 0U));

      break;
    }
    case OPCODE_USUBB:
    {
      uint64_t src0[4];
      uint64_t src1[4];

      // add on a 'borrow' bit
      for(int i = 0; i < 4; i++)
        src0[i] = 0x100000000 | (uint64_t)srcOpers[1].value.u32v[i];
      for(int i = 0; i < 4; i++)
        src1[i] = (uint64_t)srcOpers[2].value.u32v[i];

      // do the subtract
      uint64_t result[4];
      for(int i = 0; i < 4; i++)
        result[i] = src0[i] - src1[i];

      uint32_t dst[4];
      for(int i = 0; i < 4; i++)
        dst[i] = (uint32_t)(result[0] & 0xffffffff);

      SetDst(state, op.operands[0], op, ShaderVariable(rdcstr(), dst[0], dst[1], dst[2], dst[3]));

      // if not null, mark where the borrow bits were used
      if(op.operands[1].type != TYPE_NULL)
        SetDst(state, op.operands[1], op,
               ShaderVariable(rdcstr(), result[0] <= 0xffffffff ? 1U : 0U,
                              result[1] <= 0xffffffff ? 1U : 0U, result[2] <= 0xffffffff ? 1U : 0U,
                              result[3] <= 0xffffffff ? 1U : 0U));

      break;
    }
    case OPCODE_IMAD:
    case OPCODE_UMAD:
    case OPCODE_MAD:
    case OPCODE_DFMA:
      SetDst(state, op.operands[0], op,
             add(mul(srcOpers[0], srcOpers[1], optype), srcOpers[2], optype));
      break;
    case OPCODE_DP2:
    case OPCODE_DP3:
    case OPCODE_DP4:
    {
      ShaderVariable dot = mul(srcOpers[0], srcOpers[1], optype);

      float sum = dot.value.f32v[0];
      sum += dot.value.f32v[1];
      if(op.operation >= OPCODE_DP3)
        sum += dot.value.f32v[2];
      if(op.operation >= OPCODE_DP4)
        sum += dot.value.f32v[3];

      SetDst(state, op.operands[0], op, ShaderVariable(rdcstr(), sum, sum, sum, sum));
      break;
    }
    case OPCODE_F16TOF32:
    {
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            flush_denorm(ConvertFromHalf(srcOpers[0].value.u32v[0] & 0xffff)),
                            flush_denorm(ConvertFromHalf(srcOpers[0].value.u32v[1] & 0xffff)),
                            flush_denorm(ConvertFromHalf(srcOpers[0].value.u32v[2] & 0xffff)),
                            flush_denorm(ConvertFromHalf(srcOpers[0].value.u32v[3] & 0xffff))));
      break;
    }
    case OPCODE_F32TOF16:
    {
      SetDst(
          state, op.operands[0], op,
          ShaderVariable(rdcstr(), (uint32_t)ConvertToHalf(flush_denorm(srcOpers[0].value.f32v[0])),
                         (uint32_t)ConvertToHalf(flush_denorm(srcOpers[0].value.f32v[1])),
                         (uint32_t)ConvertToHalf(flush_denorm(srcOpers[0].value.f32v[2])),
                         (uint32_t)ConvertToHalf(flush_denorm(srcOpers[0].value.f32v[3]))));
      break;
    }
    case OPCODE_FRC:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), srcOpers[0].value.f32v[0] - floorf(srcOpers[0].value.f32v[0]),
                            srcOpers[0].value.f32v[1] - floorf(srcOpers[0].value.f32v[1]),
                            srcOpers[0].value.f32v[2] - floorf(srcOpers[0].value.f32v[2]),
                            srcOpers[0].value.f32v[3] - floorf(srcOpers[0].value.f32v[3])));
      break;
    // positive infinity
    case OPCODE_ROUND_PI:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), ceilf(srcOpers[0].value.f32v[0]),
                            ceilf(srcOpers[0].value.f32v[1]), ceilf(srcOpers[0].value.f32v[2]),
                            ceilf(srcOpers[0].value.f32v[3])));
      break;
    // negative infinity
    case OPCODE_ROUND_NI:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), floorf(srcOpers[0].value.f32v[0]),
                            floorf(srcOpers[0].value.f32v[1]), floorf(srcOpers[0].value.f32v[2]),
                            floorf(srcOpers[0].value.f32v[3])));
      break;
    // towards zero
    case OPCODE_ROUND_Z:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            srcOpers[0].value.f32v[0] < 0 ? ceilf(srcOpers[0].value.f32v[0])
                                                          : floorf(srcOpers[0].value.f32v[0]),
                            srcOpers[0].value.f32v[1] < 0 ? ceilf(srcOpers[0].value.f32v[1])
                                                          : floorf(srcOpers[0].value.f32v[1]),
                            srcOpers[0].value.f32v[2] < 0 ? ceilf(srcOpers[0].value.f32v[2])
                                                          : floorf(srcOpers[0].value.f32v[2]),
                            srcOpers[0].value.f32v[3] < 0 ? ceilf(srcOpers[0].value.f32v[3])
                                                          : floorf(srcOpers[0].value.f32v[3])));
      break;
    // to nearest even int (banker's rounding)
    case OPCODE_ROUND_NE:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), round_ne(srcOpers[0].value.f32v[0]),
                            round_ne(srcOpers[0].value.f32v[1]), round_ne(srcOpers[0].value.f32v[2]),
                            round_ne(srcOpers[0].value.f32v[3])));
      break;
    case OPCODE_INEG: SetDst(state, op.operands[0], op, neg(srcOpers[0], optype)); break;
    case OPCODE_IMIN:
      SetDst(state, op.operands[0], op,
             ShaderVariable(
                 "",
                 srcOpers[0].value.s32v[0] < srcOpers[1].value.s32v[0] ? srcOpers[0].value.s32v[0]
                                                                       : srcOpers[1].value.s32v[0],
                 srcOpers[0].value.s32v[1] < srcOpers[1].value.s32v[1] ? srcOpers[0].value.s32v[1]
                                                                       : srcOpers[1].value.s32v[1],
                 srcOpers[0].value.s32v[2] < srcOpers[1].value.s32v[2] ? srcOpers[0].value.s32v[2]
                                                                       : srcOpers[1].value.s32v[2],
                 srcOpers[0].value.s32v[3] < srcOpers[1].value.s32v[3] ? srcOpers[0].value.s32v[3]
                                                                       : srcOpers[1].value.s32v[3]));
      break;
    case OPCODE_UMIN:
      SetDst(state, op.operands[0], op,
             ShaderVariable(
                 "",
                 srcOpers[0].value.u32v[0] < srcOpers[1].value.u32v[0] ? srcOpers[0].value.u32v[0]
                                                                       : srcOpers[1].value.u32v[0],
                 srcOpers[0].value.u32v[1] < srcOpers[1].value.u32v[1] ? srcOpers[0].value.u32v[1]
                                                                       : srcOpers[1].value.u32v[1],
                 srcOpers[0].value.u32v[2] < srcOpers[1].value.u32v[2] ? srcOpers[0].value.u32v[2]
                                                                       : srcOpers[1].value.u32v[2],
                 srcOpers[0].value.u32v[3] < srcOpers[1].value.u32v[3] ? srcOpers[0].value.u32v[3]
                                                                       : srcOpers[1].value.u32v[3]));
      break;
    case OPCODE_DMIN:
    {
      double src0[2], src1[2];
      DoubleGet(srcOpers[0], src0);
      DoubleGet(srcOpers[1], src1);

      double dst[2];
      dst[0] = dxbc_min(src0[0], src1[0]);
      dst[1] = dxbc_min(src0[1], src1[1]);

      ShaderVariable r("", 0U, 0U, 0U, 0U);
      DoubleSet(r, dst);

      SetDst(state, op.operands[0], op, r);
      break;
    }
    case OPCODE_MIN:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), dxbc_min(srcOpers[0].value.f32v[0], srcOpers[1].value.f32v[0]),
                            dxbc_min(srcOpers[0].value.f32v[1], srcOpers[1].value.f32v[1]),
                            dxbc_min(srcOpers[0].value.f32v[2], srcOpers[1].value.f32v[2]),
                            dxbc_min(srcOpers[0].value.f32v[3], srcOpers[1].value.f32v[3])));
      break;
    case OPCODE_UMAX:
      SetDst(state, op.operands[0], op,
             ShaderVariable(
                 "",
                 srcOpers[0].value.u32v[0] >= srcOpers[1].value.u32v[0] ? srcOpers[0].value.u32v[0]
                                                                        : srcOpers[1].value.u32v[0],
                 srcOpers[0].value.u32v[1] >= srcOpers[1].value.u32v[1] ? srcOpers[0].value.u32v[1]
                                                                        : srcOpers[1].value.u32v[1],
                 srcOpers[0].value.u32v[2] >= srcOpers[1].value.u32v[2] ? srcOpers[0].value.u32v[2]
                                                                        : srcOpers[1].value.u32v[2],
                 srcOpers[0].value.u32v[3] >= srcOpers[1].value.u32v[3] ? srcOpers[0].value.u32v[3]
                                                                        : srcOpers[1].value.u32v[3]));
      break;
    case OPCODE_IMAX:
      SetDst(state, op.operands[0], op,
             ShaderVariable(
                 "",
                 srcOpers[0].value.s32v[0] >= srcOpers[1].value.s32v[0] ? srcOpers[0].value.s32v[0]
                                                                        : srcOpers[1].value.s32v[0],
                 srcOpers[0].value.s32v[1] >= srcOpers[1].value.s32v[1] ? srcOpers[0].value.s32v[1]
                                                                        : srcOpers[1].value.s32v[1],
                 srcOpers[0].value.s32v[2] >= srcOpers[1].value.s32v[2] ? srcOpers[0].value.s32v[2]
                                                                        : srcOpers[1].value.s32v[2],
                 srcOpers[0].value.s32v[3] >= srcOpers[1].value.s32v[3] ? srcOpers[0].value.s32v[3]
                                                                        : srcOpers[1].value.s32v[3]));
      break;
    case OPCODE_DMAX:
    {
      double src0[2], src1[2];
      DoubleGet(srcOpers[0], src0);
      DoubleGet(srcOpers[1], src1);

      double dst[2];
      dst[0] = dxbc_max(src0[0], src1[0]);
      dst[1] = dxbc_max(src0[1], src1[1]);

      ShaderVariable r("", 0U, 0U, 0U, 0U);
      DoubleSet(r, dst);

      SetDst(state, op.operands[0], op, r);
      break;
    }
    case OPCODE_MAX:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), dxbc_max(srcOpers[0].value.f32v[0], srcOpers[1].value.f32v[0]),
                            dxbc_max(srcOpers[0].value.f32v[1], srcOpers[1].value.f32v[1]),
                            dxbc_max(srcOpers[0].value.f32v[2], srcOpers[1].value.f32v[2]),
                            dxbc_max(srcOpers[0].value.f32v[3], srcOpers[1].value.f32v[3])));
      break;
    case OPCODE_SQRT:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), sqrtf(srcOpers[0].value.f32v[0]),
                            sqrtf(srcOpers[0].value.f32v[1]), sqrtf(srcOpers[0].value.f32v[2]),
                            sqrtf(srcOpers[0].value.f32v[3])));
      break;
    case OPCODE_DRCP:
    {
      double ds[2] = {0.0, 0.0};
      DoubleGet(srcOpers[0], ds);
      ds[0] = 1.0f / ds[0];
      ds[1] = 1.0f / ds[1];

      ShaderVariable r("", 0U, 0U, 0U, 0U);
      DoubleSet(r, ds);

      SetDst(state, op.operands[0], op, r);
      break;
    }

    case OPCODE_IBFE:
    {
      // bottom 5 bits
      ShaderVariable width("", (int32_t)(srcOpers[0].value.s32v[0] & 0x1f),
                           (int32_t)(srcOpers[0].value.s32v[1] & 0x1f),
                           (int32_t)(srcOpers[0].value.s32v[2] & 0x1f),
                           (int32_t)(srcOpers[0].value.s32v[3] & 0x1f));
      ShaderVariable offset("", (int32_t)(srcOpers[1].value.s32v[0] & 0x1f),
                            (int32_t)(srcOpers[1].value.s32v[1] & 0x1f),
                            (int32_t)(srcOpers[1].value.s32v[2] & 0x1f),
                            (int32_t)(srcOpers[1].value.s32v[3] & 0x1f));

      ShaderVariable dest("", (int32_t)0, (int32_t)0, (int32_t)0, (int32_t)0);

      for(int comp = 0; comp < 4; comp++)
      {
        if(width.value.s32v[comp] == 0)
        {
          dest.value.s32v[comp] = 0;
        }
        else if(width.value.s32v[comp] + offset.value.s32v[comp] < 32)
        {
          dest.value.s32v[comp] = srcOpers[2].value.s32v[comp]
                                  << (32 - (width.value.s32v[comp] + offset.value.s32v[comp]));
          dest.value.s32v[comp] = dest.value.s32v[comp] >> (32 - width.value.s32v[comp]);
        }
        else
        {
          dest.value.s32v[comp] = srcOpers[2].value.s32v[comp] >> offset.value.s32v[comp];
        }
      }

      SetDst(state, op.operands[0], op, dest);
      break;
    }
    case OPCODE_UBFE:
    {
      // bottom 5 bits
      ShaderVariable width("", (uint32_t)(srcOpers[0].value.u32v[0] & 0x1f),
                           (uint32_t)(srcOpers[0].value.u32v[1] & 0x1f),
                           (uint32_t)(srcOpers[0].value.u32v[2] & 0x1f),
                           (uint32_t)(srcOpers[0].value.u32v[3] & 0x1f));
      ShaderVariable offset("", (uint32_t)(srcOpers[1].value.u32v[0] & 0x1f),
                            (uint32_t)(srcOpers[1].value.u32v[1] & 0x1f),
                            (uint32_t)(srcOpers[1].value.u32v[2] & 0x1f),
                            (uint32_t)(srcOpers[1].value.u32v[3] & 0x1f));

      ShaderVariable dest("", (uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0);

      for(int comp = 0; comp < 4; comp++)
      {
        if(width.value.u32v[comp] == 0)
        {
          dest.value.u32v[comp] = 0;
        }
        else if(width.value.u32v[comp] + offset.value.u32v[comp] < 32)
        {
          dest.value.u32v[comp] = srcOpers[2].value.u32v[comp]
                                  << (32 - (width.value.u32v[comp] + offset.value.u32v[comp]));
          dest.value.u32v[comp] = dest.value.u32v[comp] >> (32 - width.value.u32v[comp]);
        }
        else
        {
          dest.value.u32v[comp] = srcOpers[2].value.u32v[comp] >> offset.value.u32v[comp];
        }
      }

      SetDst(state, op.operands[0], op, dest);
      break;
    }
    case OPCODE_BFI:
    {
      // bottom 5 bits
      ShaderVariable width("", (uint32_t)(srcOpers[0].value.u32v[0] & 0x1f),
                           (uint32_t)(srcOpers[0].value.u32v[1] & 0x1f),
                           (uint32_t)(srcOpers[0].value.u32v[2] & 0x1f),
                           (uint32_t)(srcOpers[0].value.u32v[3] & 0x1f));
      ShaderVariable offset("", (uint32_t)(srcOpers[1].value.u32v[0] & 0x1f),
                            (uint32_t)(srcOpers[1].value.u32v[1] & 0x1f),
                            (uint32_t)(srcOpers[1].value.u32v[2] & 0x1f),
                            (uint32_t)(srcOpers[1].value.u32v[3] & 0x1f));

      ShaderVariable dest("", (uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0);

      for(int comp = 0; comp < 4; comp++)
      {
        uint32_t bitmask =
            (((1 << width.value.u32v[comp]) - 1) << offset.value.u32v[comp]) & 0xffffffff;
        dest.value.u32v[comp] =
            (uint32_t)(((srcOpers[2].value.u32v[comp] << offset.value.u32v[comp]) & bitmask) |
                       (srcOpers[3].value.u32v[comp] & ~bitmask));
      }

      SetDst(state, op.operands[0], op, dest);
      break;
    }
    case OPCODE_ISHL:
    {
      uint32_t shifts[] = {
          srcOpers[1].value.u32v[0] & 0x1f,
          srcOpers[1].value.u32v[1] & 0x1f,
          srcOpers[1].value.u32v[2] & 0x1f,
          srcOpers[1].value.u32v[3] & 0x1f,
      };

      // if we were only given a single component, it's the form that shifts all components
      // by the same amount
      if(op.operands[2].numComponents == NUMCOMPS_1 ||
         (op.operands[2].comps[2] < 4 && op.operands[2].comps[2] == 0xff))
        shifts[3] = shifts[2] = shifts[1] = shifts[0];

      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), srcOpers[0].value.s32v[0] << shifts[0],
                            srcOpers[0].value.s32v[1] << shifts[1],
                            srcOpers[0].value.s32v[2] << shifts[2],
                            srcOpers[0].value.s32v[3] << shifts[3]));
      break;
    }
    case OPCODE_USHR:
    {
      uint32_t shifts[] = {
          srcOpers[1].value.u32v[0] & 0x1f,
          srcOpers[1].value.u32v[1] & 0x1f,
          srcOpers[1].value.u32v[2] & 0x1f,
          srcOpers[1].value.u32v[3] & 0x1f,
      };

      // if we were only given a single component, it's the form that shifts all components
      // by the same amount
      if(op.operands[2].numComponents == NUMCOMPS_1 ||
         (op.operands[2].comps[2] < 4 && op.operands[2].comps[2] == 0xff))
        shifts[3] = shifts[2] = shifts[1] = shifts[0];

      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), srcOpers[0].value.u32v[0] >> shifts[0],
                            srcOpers[0].value.u32v[1] >> shifts[1],
                            srcOpers[0].value.u32v[2] >> shifts[2],
                            srcOpers[0].value.u32v[3] >> shifts[3]));
      break;
    }
    case OPCODE_ISHR:
    {
      uint32_t shifts[] = {
          srcOpers[1].value.u32v[0] & 0x1f,
          srcOpers[1].value.u32v[1] & 0x1f,
          srcOpers[1].value.u32v[2] & 0x1f,
          srcOpers[1].value.u32v[3] & 0x1f,
      };

      // if we were only given a single component, it's the form that shifts all components
      // by the same amount
      if(op.operands[2].numComponents == NUMCOMPS_1 ||
         (op.operands[2].comps[2] < 4 && op.operands[2].comps[2] == 0xff))
        shifts[3] = shifts[2] = shifts[1] = shifts[0];

      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), srcOpers[0].value.s32v[0] >> shifts[0],
                            srcOpers[0].value.s32v[1] >> shifts[1],
                            srcOpers[0].value.s32v[2] >> shifts[2],
                            srcOpers[0].value.s32v[3] >> shifts[3]));
      break;
    }
    case OPCODE_AND:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), srcOpers[0].value.s32v[0] & srcOpers[1].value.s32v[0],
                            srcOpers[0].value.s32v[1] & srcOpers[1].value.s32v[1],
                            srcOpers[0].value.s32v[2] & srcOpers[1].value.s32v[2],
                            srcOpers[0].value.s32v[3] & srcOpers[1].value.s32v[3]));
      break;
    case OPCODE_OR:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), srcOpers[0].value.s32v[0] | srcOpers[1].value.s32v[0],
                            srcOpers[0].value.s32v[1] | srcOpers[1].value.s32v[1],
                            srcOpers[0].value.s32v[2] | srcOpers[1].value.s32v[2],
                            srcOpers[0].value.s32v[3] | srcOpers[1].value.s32v[3]));
      break;
    case OPCODE_XOR:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), srcOpers[0].value.u32v[0] ^ srcOpers[1].value.u32v[0],
                            srcOpers[0].value.u32v[1] ^ srcOpers[1].value.u32v[1],
                            srcOpers[0].value.u32v[2] ^ srcOpers[1].value.u32v[2],
                            srcOpers[0].value.u32v[3] ^ srcOpers[1].value.u32v[3]));
      break;
    case OPCODE_NOT:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), ~srcOpers[0].value.u32v[0], ~srcOpers[0].value.u32v[1],
                            ~srcOpers[0].value.u32v[2], ~srcOpers[0].value.u32v[3]));
      break;

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // transcendental functions with loose ULP requirements, so we pass them to the GPU to get
      // more accurate (well, LESS accurate but more representative) answers.

    case OPCODE_RCP:
    case OPCODE_RSQ:
    case OPCODE_EXP:
    case OPCODE_LOG:
    {
      ShaderVariable calcResultA("calcA", 0.0f, 0.0f, 0.0f, 0.0f);
      ShaderVariable calcResultB("calcB", 0.0f, 0.0f, 0.0f, 0.0f);
      if(apiWrapper->CalculateMathIntrinsic(op.operation, srcOpers[0], calcResultA, calcResultB))
      {
        SetDst(state, op.operands[0], op, calcResultA);
      }
      else
      {
        return;
      }
      break;
    }
    case OPCODE_SINCOS:
    {
      ShaderVariable calcResultA("calcA", 0.0f, 0.0f, 0.0f, 0.0f);
      ShaderVariable calcResultB("calcB", 0.0f, 0.0f, 0.0f, 0.0f);
      if(apiWrapper->CalculateMathIntrinsic(OPCODE_SINCOS, srcOpers[1], calcResultA, calcResultB))
      {
        if(op.operands[0].type != TYPE_NULL)
          SetDst(state, op.operands[0], op, calcResultA);
        if(op.operands[1].type != TYPE_NULL)
          SetDst(state, op.operands[1], op, calcResultB);
      }
      else
      {
        return;
      }
      break;
    }

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // Misc

    case OPCODE_NOP:
    case OPCODE_CUSTOMDATA:
    case OPCODE_OPAQUE_CUSTOMDATA:
    case OPCODE_SHADER_MESSAGE:
    case OPCODE_DCL_IMMEDIATE_CONSTANT_BUFFER: break;
    case OPCODE_SYNC:    // might never need to implement this. Who knows!
      break;
    case OPCODE_DMOV:
    case OPCODE_MOV: SetDst(state, op.operands[0], op, srcOpers[0]); break;
    case OPCODE_DMOVC:
      SetDst(
          state, op.operands[0], op,
          ShaderVariable(
              "", srcOpers[0].value.u32v[0] ? srcOpers[1].value.u32v[0] : srcOpers[2].value.u32v[0],
              srcOpers[0].value.u32v[0] ? srcOpers[1].value.u32v[1] : srcOpers[2].value.u32v[1],
              srcOpers[0].value.u32v[1] ? srcOpers[1].value.u32v[2] : srcOpers[2].value.u32v[2],
              srcOpers[0].value.u32v[1] ? srcOpers[1].value.u32v[3] : srcOpers[2].value.u32v[3]));
      break;
    case OPCODE_MOVC:
      SetDst(
          state, op.operands[0], op,
          ShaderVariable(
              "", srcOpers[0].value.s32v[0] ? srcOpers[1].value.s32v[0] : srcOpers[2].value.s32v[0],
              srcOpers[0].value.s32v[1] ? srcOpers[1].value.s32v[1] : srcOpers[2].value.s32v[1],
              srcOpers[0].value.s32v[2] ? srcOpers[1].value.s32v[2] : srcOpers[2].value.s32v[2],
              srcOpers[0].value.s32v[3] ? srcOpers[1].value.s32v[3] : srcOpers[2].value.s32v[3]));
      break;
    case OPCODE_SWAPC:
      SetDst(
          state, op.operands[0], op,
          ShaderVariable(
              "", srcOpers[1].value.s32v[0] ? srcOpers[3].value.s32v[0] : srcOpers[2].value.s32v[0],
              srcOpers[1].value.s32v[1] ? srcOpers[3].value.s32v[1] : srcOpers[2].value.s32v[1],
              srcOpers[1].value.s32v[2] ? srcOpers[3].value.s32v[2] : srcOpers[2].value.s32v[2],
              srcOpers[1].value.s32v[3] ? srcOpers[3].value.s32v[3] : srcOpers[2].value.s32v[3]));

      SetDst(
          state, op.operands[1], op,
          ShaderVariable(
              "", srcOpers[1].value.s32v[0] ? srcOpers[2].value.s32v[0] : srcOpers[3].value.s32v[0],
              srcOpers[1].value.s32v[1] ? srcOpers[2].value.s32v[1] : srcOpers[3].value.s32v[1],
              srcOpers[1].value.s32v[2] ? srcOpers[2].value.s32v[2] : srcOpers[3].value.s32v[2],
              srcOpers[1].value.s32v[3] ? srcOpers[2].value.s32v[3] : srcOpers[3].value.s32v[3]));
      break;
    case OPCODE_ITOF:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), (float)srcOpers[0].value.s32v[0],
                            (float)srcOpers[0].value.s32v[1], (float)srcOpers[0].value.s32v[2],
                            (float)srcOpers[0].value.s32v[3]));
      break;
    case OPCODE_UTOF:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), (float)srcOpers[0].value.u32v[0],
                            (float)srcOpers[0].value.u32v[1], (float)srcOpers[0].value.u32v[2],
                            (float)srcOpers[0].value.u32v[3]));
      break;
    case OPCODE_FTOI:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), (int)srcOpers[0].value.f32v[0], (int)srcOpers[0].value.f32v[1],
                            (int)srcOpers[0].value.f32v[2], (int)srcOpers[0].value.f32v[3]));
      break;
    case OPCODE_FTOU:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(), (uint32_t)srcOpers[0].value.f32v[0],
                            (uint32_t)srcOpers[0].value.f32v[1], (uint32_t)srcOpers[0].value.f32v[2],
                            (uint32_t)srcOpers[0].value.f32v[3]));
      break;
    case OPCODE_ITOD:
    case OPCODE_UTOD:
    case OPCODE_FTOD:
    {
      double res[2];

      if(op.operation == OPCODE_ITOD)
      {
        res[0] = (double)srcOpers[0].value.s32v[0];
        res[1] = (double)srcOpers[0].value.s32v[1];
      }
      else if(op.operation == OPCODE_UTOD)
      {
        res[0] = (double)srcOpers[0].value.u32v[0];
        res[1] = (double)srcOpers[0].value.u32v[1];
      }
      else if(op.operation == OPCODE_FTOD)
      {
        res[0] = (double)srcOpers[0].value.f32v[0];
        res[1] = (double)srcOpers[0].value.f32v[1];
      }

      // if we only did a 1-wide double op, copy .xy into .zw so we can then
      // swizzle into .xy or .zw freely on the destination operand.
      // e.g. ftod r0.zw, r0.z - if we didn't do this, there'd be nothing valid in .zw
      if(op.operands[1].comps[2] == 0xff)
        res[1] = res[0];

      ShaderVariable r("", 0U, 0U, 0U, 0U);
      DoubleSet(r, res);

      SetDst(state, op.operands[0], op, r);
      break;
    }
    case OPCODE_DTOI:
    case OPCODE_DTOU:
    case OPCODE_DTOF:
    {
      double src[2];
      DoubleGet(srcOpers[0], src);

      // special behaviour for dest mask. if it's .xz then first goes into .x, second into .z.
      // if the mask is .y then the first goes into .y and second goes nowhere.
      // so we need to check the dest mask and put the results into the right place

      ShaderVariable r("", 0U, 0U, 0U, 0U);

      if(op.operation == OPCODE_DTOU)
      {
        if(op.operands[0].comps[1] == 0xff)    // only one mask
        {
          r.value.u32v[op.operands[0].comps[0]] = uint32_t(src[0]);
        }
        else
        {
          r.value.u32v[op.operands[0].comps[0]] = uint32_t(src[0]);
          r.value.u32v[op.operands[0].comps[1]] = uint32_t(src[1]);
        }
      }
      else if(op.operation == OPCODE_DTOI)
      {
        if(op.operands[0].comps[1] == 0xff)    // only one mask
        {
          r.value.s32v[op.operands[0].comps[0]] = int32_t(src[0]);
        }
        else
        {
          r.value.s32v[op.operands[0].comps[0]] = int32_t(src[0]);
          r.value.s32v[op.operands[0].comps[1]] = int32_t(src[1]);
        }
      }
      else if(op.operation == OPCODE_DTOF)
      {
        if(op.operands[0].comps[1] == 0xff)    // only one mask
        {
          r.value.f32v[op.operands[0].comps[0]] = float(src[0]);
        }
        else
        {
          r.value.f32v[op.operands[0].comps[0]] = float(src[0]);
          r.value.f32v[op.operands[0].comps[1]] = float(src[1]);
        }
      }

      SetDst(state, op.operands[0], op, r);
      break;
    }

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // Comparison

    case OPCODE_EQ:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.f32v[0] == srcOpers[1].value.f32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[1] == srcOpers[1].value.f32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[2] == srcOpers[1].value.f32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[3] == srcOpers[1].value.f32v[3] ? ~0u : 0u)));
      break;
    case OPCODE_NE:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.f32v[0] != srcOpers[1].value.f32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[1] != srcOpers[1].value.f32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[2] != srcOpers[1].value.f32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[3] != srcOpers[1].value.f32v[3] ? ~0u : 0u)));
      break;
    case OPCODE_LT:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.f32v[0] < srcOpers[1].value.f32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[1] < srcOpers[1].value.f32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[2] < srcOpers[1].value.f32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[3] < srcOpers[1].value.f32v[3] ? ~0u : 0u)));
      break;
    case OPCODE_GE:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.f32v[0] >= srcOpers[1].value.f32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[1] >= srcOpers[1].value.f32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[2] >= srcOpers[1].value.f32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.f32v[3] >= srcOpers[1].value.f32v[3] ? ~0u : 0u)));
      break;
    case OPCODE_DEQ:
    case OPCODE_DNE:
    case OPCODE_DGE:
    case OPCODE_DLT:
    {
      double src0[2], src1[2];
      DoubleGet(srcOpers[0], src0);
      DoubleGet(srcOpers[1], src1);

      uint32_t cmp1 = 0;
      uint32_t cmp2 = 0;

      switch(op.operation)
      {
        case OPCODE_DEQ:
          cmp1 = (src0[0] == src1[0] ? ~0l : 0l);
          cmp2 = (src0[1] == src1[1] ? ~0l : 0l);
          break;
        case OPCODE_DNE:
          cmp1 = (src0[0] != src1[0] ? ~0l : 0l);
          cmp2 = (src0[1] != src1[1] ? ~0l : 0l);
          break;
        case OPCODE_DGE:
          cmp1 = (src0[0] >= src1[0] ? ~0l : 0l);
          cmp2 = (src0[1] >= src1[1] ? ~0l : 0l);
          break;
        case OPCODE_DLT:
          cmp1 = (src0[0] < src1[0] ? ~0l : 0l);
          cmp2 = (src0[1] < src1[1] ? ~0l : 0l);
          break;
        default: break;
      }

      // special behaviour for dest mask. if it's .xz then first comparison goes into .x, second
      // into .z.
      // if the mask is .y then the first comparison goes into .y and second goes nowhere.
      // so we need to check the dest mask and put the comparison results into the right place

      ShaderVariable r("", 0U, 0U, 0U, 0U);

      if(op.operands[0].comps[1] == 0xff)    // only one mask
      {
        r.value.u32v[op.operands[0].comps[0]] = cmp1;
      }
      else
      {
        r.value.u32v[op.operands[0].comps[0]] = cmp1;
        r.value.u32v[op.operands[0].comps[1]] = cmp2;
      }

      SetDst(state, op.operands[0], op, r);
      break;
    }
    case OPCODE_IEQ:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.s32v[0] == srcOpers[1].value.s32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[1] == srcOpers[1].value.s32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[2] == srcOpers[1].value.s32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[3] == srcOpers[1].value.s32v[3] ? ~0u : 0u)));
      break;
    case OPCODE_INE:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.s32v[0] != srcOpers[1].value.s32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[1] != srcOpers[1].value.s32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[2] != srcOpers[1].value.s32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[3] != srcOpers[1].value.s32v[3] ? ~0u : 0u)));
      break;
    case OPCODE_IGE:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.s32v[0] >= srcOpers[1].value.s32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[1] >= srcOpers[1].value.s32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[2] >= srcOpers[1].value.s32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[3] >= srcOpers[1].value.s32v[3] ? ~0u : 0u)));
      break;
    case OPCODE_ILT:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.s32v[0] < srcOpers[1].value.s32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[1] < srcOpers[1].value.s32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[2] < srcOpers[1].value.s32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.s32v[3] < srcOpers[1].value.s32v[3] ? ~0u : 0u)));
      break;
    case OPCODE_ULT:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.u32v[0] < srcOpers[1].value.u32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.u32v[1] < srcOpers[1].value.u32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.u32v[2] < srcOpers[1].value.u32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.u32v[3] < srcOpers[1].value.u32v[3] ? ~0u : 0u)));
      break;
    case OPCODE_UGE:
      SetDst(state, op.operands[0], op,
             ShaderVariable(rdcstr(),
                            (srcOpers[0].value.u32v[0] >= srcOpers[1].value.u32v[0] ? ~0u : 0u),
                            (srcOpers[0].value.u32v[1] >= srcOpers[1].value.u32v[1] ? ~0u : 0u),
                            (srcOpers[0].value.u32v[2] >= srcOpers[1].value.u32v[2] ? ~0u : 0u),
                            (srcOpers[0].value.u32v[3] >= srcOpers[1].value.u32v[3] ? ~0u : 0u)));
      break;

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // Atomic instructions

    case OPCODE_IMM_ATOMIC_ALLOC:
    {
      BindingSlot slot = GetBindingSlotForIdentifier(*program, TYPE_UNORDERED_ACCESS_VIEW,
                                                     srcOpers[0].value.u32v[0]);
      GlobalState::UAVIterator uav = global.uavs.find(slot);
      if(uav == global.uavs.end())
      {
        apiWrapper->FetchUAV(slot);
        uav = global.uavs.find(slot);
      }

      MarkResourceAccess(state, TYPE_UNORDERED_ACCESS_VIEW, slot);

      // if it's not a buffer or the buffer is empty this UAV is NULL/invalid, return 0 for the
      // counter
      uint32_t count = uav->second.data.empty() ? 0 : uav->second.hiddenCounter++;
      SetDst(state, op.operands[0], op, ShaderVariable(rdcstr(), count, count, count, count));
      break;
    }

    case OPCODE_IMM_ATOMIC_CONSUME:
    {
      BindingSlot slot = GetBindingSlotForIdentifier(*program, TYPE_UNORDERED_ACCESS_VIEW,
                                                     srcOpers[0].value.u32v[0]);
      GlobalState::UAVIterator uav = global.uavs.find(slot);
      if(uav == global.uavs.end())
      {
        apiWrapper->FetchUAV(slot);
        uav = global.uavs.find(slot);
      }

      MarkResourceAccess(state, TYPE_UNORDERED_ACCESS_VIEW, slot);

      // if it's not a buffer or the buffer is empty this UAV is NULL/invalid, return 0 for the
      // counter
      uint32_t count = uav->second.data.empty() ? 0 : --uav->second.hiddenCounter;
      SetDst(state, op.operands[0], op, ShaderVariable(rdcstr(), count, count, count, count));
      break;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Derivative instructions

    // don't differentiate, coarse, fine, whatever. The spec lets us implement it all as fine.
    case OPCODE_DERIV_RTX:
    case OPCODE_DERIV_RTX_COARSE:
    case OPCODE_DERIV_RTX_FINE:
      if(program->GetShaderType() != DXBC::ShaderType::Pixel || prevWorkgroup.size() != 4)
        RDCERR(
            "Attempt to use derivative instruction not in pixel shader. Undefined results will "
            "occur!");
      else
        SetDst(state, op.operands[0], op,
               DDX(op.operation == OPCODE_DERIV_RTX_FINE, prevWorkgroup, op.operands[1], op));
      break;
    case OPCODE_DERIV_RTY:
    case OPCODE_DERIV_RTY_COARSE:
    case OPCODE_DERIV_RTY_FINE:
      if(program->GetShaderType() != DXBC::ShaderType::Pixel || prevWorkgroup.size() != 4)
        RDCERR(
            "Attempt to use derivative instruction not in pixel shader. Undefined results will "
            "occur!");
      else
        SetDst(state, op.operands[0], op,
               DDY(op.operation == OPCODE_DERIV_RTY_FINE, prevWorkgroup, op.operands[1], op));
      break;

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Buffer/Texture load and store

    // handle atomic operations all together
    case OPCODE_ATOMIC_IADD:
    case OPCODE_ATOMIC_IMAX:
    case OPCODE_ATOMIC_IMIN:
    case OPCODE_ATOMIC_AND:
    case OPCODE_ATOMIC_OR:
    case OPCODE_ATOMIC_XOR:
    case OPCODE_ATOMIC_CMP_STORE:
    case OPCODE_ATOMIC_UMAX:
    case OPCODE_ATOMIC_UMIN:
    case OPCODE_IMM_ATOMIC_IADD:
    case OPCODE_IMM_ATOMIC_IMAX:
    case OPCODE_IMM_ATOMIC_IMIN:
    case OPCODE_IMM_ATOMIC_AND:
    case OPCODE_IMM_ATOMIC_OR:
    case OPCODE_IMM_ATOMIC_XOR:
    case OPCODE_IMM_ATOMIC_EXCH:
    case OPCODE_IMM_ATOMIC_CMP_EXCH:
    case OPCODE_IMM_ATOMIC_UMAX:
    case OPCODE_IMM_ATOMIC_UMIN:
    {
      Operand beforeResult;
      uint32_t resIndex = 0;
      ShaderVariable *dstAddress = NULL;
      ShaderVariable *src0 = NULL;
      ShaderVariable *src1 = NULL;
      bool gsm = false;

      if(op.operation == OPCODE_IMM_ATOMIC_IADD || op.operation == OPCODE_IMM_ATOMIC_IMAX ||
         op.operation == OPCODE_IMM_ATOMIC_IMIN || op.operation == OPCODE_IMM_ATOMIC_AND ||
         op.operation == OPCODE_IMM_ATOMIC_OR || op.operation == OPCODE_IMM_ATOMIC_XOR ||
         op.operation == OPCODE_IMM_ATOMIC_EXCH || op.operation == OPCODE_IMM_ATOMIC_CMP_EXCH ||
         op.operation == OPCODE_IMM_ATOMIC_UMAX || op.operation == OPCODE_IMM_ATOMIC_UMIN)
      {
        beforeResult = op.operands[0];
        resIndex = (uint32_t)op.operands[1].indices[0].index;
        gsm = (op.operands[1].type == TYPE_THREAD_GROUP_SHARED_MEMORY);
        dstAddress = &srcOpers[1];
        src0 = &srcOpers[2];
        if(srcOpers.size() > 3)
          src1 = &srcOpers[3];
      }
      else
      {
        beforeResult.type = TYPE_NULL;
        resIndex = (uint32_t)op.operands[0].indices[0].index;
        gsm = (op.operands[0].type == TYPE_THREAD_GROUP_SHARED_MEMORY);
        dstAddress = &srcOpers[0];
        src0 = &srcOpers[1];
        if(srcOpers.size() > 2)
          src1 = &srcOpers[2];
      }

      uint32_t stride = 4;
      uint32_t offset = 0;
      uint32_t numElems = 0;
      bool structured = false;

      byte *data = NULL;

      if(gsm)
      {
        offset = 0;
        if(resIndex > global.groupshared.size())
        {
          numElems = 0;
          stride = 4;
          data = NULL;
        }
        else
        {
          numElems = global.groupshared[resIndex].count;
          stride = global.groupshared[resIndex].bytestride;
          data = &global.groupshared[resIndex].data[0];
          structured = global.groupshared[resIndex].structured;
        }
      }
      else
      {
        BindingSlot slot =
            GetBindingSlotForIdentifier(*program, TYPE_UNORDERED_ACCESS_VIEW, resIndex);
        GlobalState::UAVIterator uav = global.uavs.find(slot);
        if(uav == global.uavs.end())
        {
          apiWrapper->FetchUAV(slot);
          uav = global.uavs.find(slot);
        }

        MarkResourceAccess(state, TYPE_UNORDERED_ACCESS_VIEW, slot);

        offset = uav->second.firstElement;
        numElems = uav->second.numElements;
        data = &uav->second.data[0];

        const DXBCBytecode::Declaration *pDecl =
            program->FindDeclaration(TYPE_UNORDERED_ACCESS_VIEW, resIndex);
        if(pDecl)
        {
          if(pDecl->declaration == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW)
          {
            stride = 4;
            structured = false;
          }
          else if(pDecl->declaration == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED)
          {
            stride = pDecl->structured.stride;
            structured = true;
          }
        }
      }

      RDCASSERT(data);

      // seems like .x is element index, and .y is byte address, in the dstAddress operand
      //
      // "Out of bounds addressing on u# causes nothing to be written to memory, except if the
      //  u# is structured, and byte offset into the struct (second component of the address) is
      //  causing the out of bounds access, then the entire contents of the UAV become undefined."
      //
      // "The number of components taken from the address is determined by the dimensionality of dst
      // u# or g#."

      if(data)
      {
        data += (offset + dstAddress->value.u32v[0]) * stride;
        if(structured)
          data += dstAddress->value.u32v[1];
      }

      // if out of bounds, undefined result is returned to dst0 for immediate operands,
      // so we only need to care about the in-bounds case.
      // Also helper/inactive pixels are not allowed to modify UAVs
      if(data && offset + dstAddress->value.u32v[0] < numElems && !Finished())
      {
        uint32_t *udst = (uint32_t *)data;
        int32_t *idst = (int32_t *)data;

        if(beforeResult.type != TYPE_NULL)
        {
          SetDst(state, beforeResult, op, ShaderVariable(rdcstr(), *udst, *udst, *udst, *udst));
        }

        // not verified below since by definition the operations that expect usrc1 will have it
        uint32_t *usrc0 = src0->value.u32v.data();
        uint32_t *usrc1 = src1->value.u32v.data();

        int32_t *isrc0 = src0->value.s32v.data();

        switch(op.operation)
        {
          case OPCODE_IMM_ATOMIC_IADD:
          case OPCODE_ATOMIC_IADD: *udst = *udst + *usrc0; break;
          case OPCODE_IMM_ATOMIC_IMAX:
          case OPCODE_ATOMIC_IMAX: *idst = RDCMAX(*idst, *isrc0); break;
          case OPCODE_IMM_ATOMIC_IMIN:
          case OPCODE_ATOMIC_IMIN: *idst = RDCMIN(*idst, *isrc0); break;
          case OPCODE_IMM_ATOMIC_AND:
          case OPCODE_ATOMIC_AND: *udst = *udst & *usrc0; break;
          case OPCODE_IMM_ATOMIC_OR:
          case OPCODE_ATOMIC_OR: *udst = *udst | *usrc0; break;
          case OPCODE_IMM_ATOMIC_XOR:
          case OPCODE_ATOMIC_XOR: *udst = *udst ^ *usrc0; break;
          case OPCODE_IMM_ATOMIC_EXCH: *udst = *usrc0; break;
          case OPCODE_IMM_ATOMIC_CMP_EXCH:
          case OPCODE_ATOMIC_CMP_STORE:
            if(*udst == *usrc1)
              *udst = *usrc0;
            break;
          case OPCODE_IMM_ATOMIC_UMAX:
          case OPCODE_ATOMIC_UMAX: *udst = RDCMAX(*udst, *usrc0); break;
          case OPCODE_IMM_ATOMIC_UMIN:
          case OPCODE_ATOMIC_UMIN: *udst = RDCMIN(*udst, *usrc0); break;
          default: break;
        }
      }

      break;
    }

    // store and load paths are mostly identical
    case OPCODE_STORE_UAV_TYPED:
    case OPCODE_STORE_RAW:
    case OPCODE_STORE_STRUCTURED:

    case OPCODE_LD_RAW:
    case OPCODE_LD_UAV_TYPED:
    case OPCODE_LD_STRUCTURED:
    {
      uint32_t resIndex = 0;
      uint32_t structOffset = 0;
      uint32_t elemIdx = 0;

      uint32_t texCoords[3] = {0, 0, 0};

      uint32_t stride = 0;

      bool srv = true;
      bool gsm = false;

      bool load = true;

      uint8_t resComps[4] = {0, 1, 2, 3};

      if(op.operation == OPCODE_STORE_UAV_TYPED || op.operation == OPCODE_STORE_RAW ||
         op.operation == OPCODE_STORE_STRUCTURED)
      {
        load = false;
      }

      if(load && state)
        state->flags |= ShaderEvents::SampleLoadGather;

      if(op.operation == OPCODE_LD_STRUCTURED || op.operation == OPCODE_STORE_STRUCTURED)
      {
        if(load)
        {
          resIndex = (uint32_t)op.operands[3].indices[0].index;
          srv = (op.operands[3].type == TYPE_RESOURCE);
          gsm = (op.operands[3].type == TYPE_THREAD_GROUP_SHARED_MEMORY);
          memcpy(resComps, op.operands[3].comps, sizeof(resComps));

          stride = op.stride;
        }
        else
        {
          resIndex = (uint32_t)op.operands[0].indices[0].index;
          srv = false;
          gsm = (op.operands[0].type == TYPE_THREAD_GROUP_SHARED_MEMORY);
        }

        if(stride == 0)
        {
          if(gsm && resIndex < global.groupshared.size())
          {
            stride = global.groupshared[resIndex].bytestride;
          }
          else if(!gsm)
          {
            OperandType declType = srv ? TYPE_RESOURCE : TYPE_UNORDERED_ACCESS_VIEW;
            OpcodeType declOpcode =
                srv ? OPCODE_DCL_RESOURCE_STRUCTURED : OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED;
            const DXBCBytecode::Declaration *pDecl = program->FindDeclaration(declType, resIndex);
            if(pDecl && pDecl->declaration == declOpcode)
              stride = pDecl->structured.stride;
          }
        }

        structOffset = srcOpers[1].value.u32v[0];
        elemIdx = srcOpers[0].value.u32v[0];
      }
      else if(op.operation == OPCODE_LD_UAV_TYPED || op.operation == OPCODE_STORE_UAV_TYPED)
      {
        if(load)
        {
          resIndex = (uint32_t)op.operands[2].indices[0].index;
          gsm = (op.operands[2].type == TYPE_THREAD_GROUP_SHARED_MEMORY);
          memcpy(resComps, op.operands[2].comps, sizeof(resComps));
        }
        else
        {
          resIndex = (uint32_t)op.operands[0].indices[0].index;
          gsm = (op.operands[0].type == TYPE_THREAD_GROUP_SHARED_MEMORY);
        }

        elemIdx = srcOpers[0].value.u32v[0];

        // could be a tex load
        texCoords[0] = srcOpers[0].value.u32v[0];
        texCoords[1] = srcOpers[0].value.u32v[1];
        texCoords[2] = srcOpers[0].value.u32v[2];

        stride = 4;
        srv = false;
      }
      else if(op.operation == OPCODE_LD_RAW || op.operation == OPCODE_STORE_RAW)
      {
        if(load)
        {
          resIndex = (uint32_t)op.operands[2].indices[0].index;
          srv = (op.operands[2].type == TYPE_RESOURCE);
          gsm = (op.operands[2].type == TYPE_THREAD_GROUP_SHARED_MEMORY);
          memcpy(resComps, op.operands[2].comps, sizeof(resComps));
        }
        else
        {
          resIndex = (uint32_t)op.operands[0].indices[0].index;
          srv = false;
          gsm = (op.operands[0].type == TYPE_THREAD_GROUP_SHARED_MEMORY);
        }

        // the index is supposed to be a multiple of 4 but the behaviour seems to be to round down
        elemIdx = (srcOpers[0].value.u32v[0] & ~0x3);
        stride = 1;
      }

      RDCASSERT(stride != 0);

      byte *data = NULL;
      size_t dataSize = 0;
      bool texData = false;
      uint32_t rowPitch = 0;
      uint32_t depthPitch = 0;
      uint32_t firstElem = 0;
      uint32_t numElems = 0;
      GlobalState::ViewFmt fmt;

      if(gsm)
      {
        firstElem = 0;
        if(resIndex > global.groupshared.size())
        {
          numElems = 0;
          stride = 4;
          data = NULL;
        }
        else
        {
          numElems = global.groupshared[resIndex].count;
          stride = global.groupshared[resIndex].bytestride;
          data = global.groupshared[resIndex].data.data();
          dataSize = global.groupshared[resIndex].data.size();
          fmt.fmt = CompType::UInt;
          fmt.byteWidth = 4;
          fmt.numComps = global.groupshared[resIndex].bytestride / 4;
          fmt.stride = 0;
        }
        texData = false;
      }
      else
      {
        BindingSlot slot = GetBindingSlotForIdentifier(
            *program, srv ? TYPE_RESOURCE : TYPE_UNORDERED_ACCESS_VIEW, resIndex);

        if(srv)
        {
          GlobalState::SRVIterator srvIter = global.srvs.find(slot);
          if(srvIter == global.srvs.end())
          {
            apiWrapper->FetchSRV(slot);
            srvIter = global.srvs.find(slot);
          }

          MarkResourceAccess(state, TYPE_RESOURCE, slot);

          data = srvIter->second.data.data();
          dataSize = srvIter->second.data.size();
          firstElem = srvIter->second.firstElement;
          numElems = srvIter->second.numElements;
          fmt = srvIter->second.format;
        }
        else
        {
          GlobalState::UAVIterator uavIter = global.uavs.find(slot);
          if(uavIter == global.uavs.end())
          {
            apiWrapper->FetchUAV(slot);
            uavIter = global.uavs.find(slot);
          }

          MarkResourceAccess(state, TYPE_UNORDERED_ACCESS_VIEW, slot);

          data = uavIter->second.data.data();
          dataSize = uavIter->second.data.size();
          texData = uavIter->second.tex;
          rowPitch = uavIter->second.rowPitch;
          depthPitch = uavIter->second.depthPitch;
          firstElem = uavIter->second.firstElement;
          numElems = uavIter->second.numElements;
          fmt = uavIter->second.format;
        }

        if(op.operation == OPCODE_LD_UAV_TYPED || op.operation == OPCODE_STORE_UAV_TYPED)
          stride = fmt.Stride();
      }

      // indexing for raw views is in bytes, but firstElement/numElements is in format-sized
      // units. Multiply up by stride
      if(op.operation == OPCODE_LD_RAW || op.operation == OPCODE_STORE_RAW)
      {
        firstElem *= RDCMIN(4, fmt.byteWidth);
        numElems *= RDCMIN(4, fmt.byteWidth);
      }

      RDCASSERT(data);

      size_t dataOffset = 0;

      if(texData)
      {
        dataOffset += texCoords[0] * fmt.Stride();
        dataOffset += texCoords[1] * rowPitch;
        dataOffset += texCoords[2] * depthPitch;
      }
      else
      {
        dataOffset += (firstElem + elemIdx) * stride;
        dataOffset += structOffset;
      }

      if(!data || (!texData && elemIdx >= numElems) || (texData && dataOffset >= dataSize))
      {
        if(load)
          SetDst(state, op.operands[0], op, ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U));
      }
      else
      {
        data += dataOffset;

        int maxIndex = fmt.numComps;

        uint32_t srcIdx = 1;
        if(op.operation == OPCODE_STORE_STRUCTURED || op.operation == OPCODE_LD_STRUCTURED)
        {
          srcIdx = 2;
          maxIndex = (stride - structOffset) / sizeof(uint32_t);
          fmt.byteWidth = 4;
          fmt.numComps = 4;
          if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
             op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
            fmt.numComps = 1;
          fmt.fmt = CompType::UInt;
        }
        // raw loads/stores can come from any component (as long as it's within range of the data!)
        if(op.operation == OPCODE_LD_RAW || op.operation == OPCODE_STORE_RAW)
        {
          fmt.byteWidth = 4;

          // normally we can read 4 elements
          fmt.numComps = 4;
          // clamp to out of bounds based on numElems
          fmt.numComps = RDCMIN(fmt.numComps, int(numElems - elemIdx) / 4);
          maxIndex = fmt.numComps;

          if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
             op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
            fmt.numComps = 1;
          fmt.fmt = CompType::UInt;
        }

        if(load)
        {
          ShaderVariable result = TypedUAVLoad(fmt, data);

          // apply the swizzle on the resource operand
          ShaderVariable fetch("", 0U, 0U, 0U, 0U);

          for(int c = 0; c < 4; c++)
          {
            uint8_t comp = resComps[c];
            if(comp == 0xff)
              comp = 0;

            fetch.value.u32v[c] = result.value.u32v[comp];
          }

          if(op.operation != OPCODE_LD_RAW && op.operation != OPCODE_LD_STRUCTURED)
          {
            // if we are assigning into a scalar, SetDst expects the result to be in .x (as normally
            // we are assigning FROM a scalar also).
            // to match this expectation, propogate the component across.
            if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
               op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
              fetch.value.u32v[0] = fetch.value.u32v[op.operands[0].comps[0]];
          }

          SetDst(state, op.operands[0], op, fetch);
        }
        else if(!Finished())    // helper/inactive pixels can't modify UAVs
        {
          for(int i = 0; i < 4; i++)
          {
            uint8_t comp = op.operands[0].comps[i];
            // masks must be contiguous from x, if we reach the 'end' we're done
            if(comp == 0xff || comp >= maxIndex)
              break;

            TypedUAVStore(fmt, data, srcOpers[srcIdx]);
          }
        }
      }

      break;
    }

    case OPCODE_EVAL_CENTROID:
    case OPCODE_EVAL_SAMPLE_INDEX:
    case OPCODE_EVAL_SNAPPED:
    {
      // opcodes only seem to be supported for regular inputs
      RDCASSERT(op.operands[1].type == TYPE_INPUT);

      GlobalState::SampleEvalCacheKey key;

      RDCASSERT(program->GetShaderType() == DXBC::ShaderType::Pixel);

      key.quadIndex = workgroupIndex;

      // if this is TYPE_INPUT we can look up the index directly
      key.inputRegisterIndex = (int32_t)op.operands[1].indices[0].index;

      for(int c = 0; c < 4; c++)
      {
        if(op.operands[0].comps[c] == 0xff)
          break;

        key.numComponents = c + 1;
      }

      key.firstComponent = op.operands[1].comps[op.operands[0].comps[0]];

      if(op.operation == OPCODE_EVAL_SAMPLE_INDEX)
      {
        key.sample = srcOpers[1].value.s32v[0];
      }
      else if(op.operation == OPCODE_EVAL_SNAPPED)
      {
        key.offsetx = RDCCLAMP(srcOpers[1].value.s32v[0], -8, 7);
        key.offsety = RDCCLAMP(srcOpers[1].value.s32v[1], -8, 7);
      }
      else if(op.operation == OPCODE_EVAL_CENTROID)
      {
        // OPCODE_EVAL_CENTROID is the default, -1 sample and 0,0 offset
      }

      // look up this combination in the cache, if we get a hit then return that value.
      auto it = global.sampleEvalCache.find(key);
      if(it != global.sampleEvalCache.end())
      {
        // perform source operand swizzling
        ShaderVariable var = it->second;

        for(int i = 0; i < 4; i++)
          if(op.operands[1].comps[i] < 4)
            var.value.u32v[i] = it->second.value.u32v[op.operands[1].comps[i]];

        SetDst(state, op.operands[0], op, var);
      }
      else
      {
        // if we got here, either the cache is empty (we're not rendering MSAA at all) so we should
        // just return the interpolant, or something went wrong and the item we want isn't cached so
        // the best we can do is return the interpolant.

        if(!global.sampleEvalCache.empty())
        {
          apiWrapper->AddDebugMessage(
              MessageCategory::Shaders, MessageSeverity::Medium, MessageSource::RuntimeWarning,
              StringFormat::Fmt(
                  "Shader debugging %d: %s\n"
                  "No sample evaluate found in cache. Possible out-of-bounds sample index",
                  nextInstruction - 1, op.str.c_str()));
        }

        SetDst(state, op.operands[0], op, srcOpers[0]);
      }

      break;
    }

    case OPCODE_SAMPLE_INFO:
    case OPCODE_SAMPLE_POS:
    {
      size_t numIndices = program->IsShaderModel51() ? 2 : 1;
      bool isAbsoluteResource =
          (op.operands[1].indices.size() == numIndices && op.operands[1].indices[0].absolute &&
           !op.operands[1].indices[0].relative);
      BindingSlot slot;
      if(op.operands[1].type != TYPE_RASTERIZER)
      {
        UINT identifier = (UINT)(op.operands[1].indices[0].index & 0xffffffff);
        slot = GetBindingSlotForIdentifier(*program, op.operands[1].type, identifier);

        MarkResourceAccess(state, op.operands[1].type, slot);
      }
      ShaderVariable result =
          apiWrapper->GetSampleInfo(op.operands[1].type, isAbsoluteResource, slot, op.str.c_str());

      // "If there is no resource bound to the specified slot, 0 is returned."

      // lookup sample pos if we got a count from above
      if(op.operation == OPCODE_SAMPLE_POS && result.value.u32v[0] > 0 &&
         (op.operands[2].type == TYPE_IMMEDIATE32 || op.operands[2].type == TYPE_TEMP))
      {
        // assume standard sample pattern - this might not hold in all cases
        // http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx

        uint32_t sampleIndex = srcOpers[1].value.u32v[0];
        uint32_t sampleCount = result.value.u32v[0];

        if(sampleIndex >= sampleCount)
        {
          // Per HLSL docs, if sampleIndex is out of bounds a zero vector is returned
          RDCWARN("sample index %u is out of bounds on resource bound to sample_pos (%u samples)",
                  sampleIndex, sampleCount);
          result.value.f32v[0] = 0.0f;
          result.value.f32v[1] = 0.0f;
          result.value.f32v[2] = 0.0f;
          result.value.f32v[3] = 0.0f;
        }
        else
        {
          const float *sample_pattern = NULL;

// co-ordinates are given as (i,j) in 16ths of a pixel
#define _SMP(c) ((c) / 16.0f)

          if(sampleCount == 1)
          {
            RDCWARN("Non-multisampled texture being passed to sample_pos");

            apiWrapper->AddDebugMessage(
                MessageCategory::Shaders, MessageSeverity::Medium, MessageSource::RuntimeWarning,
                StringFormat::Fmt(
                    "Shader debugging %d: %s\nNon-multisampled texture being passed to sample_pos",
                    nextInstruction - 1, op.str.c_str()));

            sample_pattern = NULL;
          }
          else if(sampleCount == 2)
          {
            static const float pattern_2x[] = {
                _SMP(4.0f),
                _SMP(4.0f),
                _SMP(-4.0f),
                _SMP(-4.0f),
            };

            sample_pattern = &pattern_2x[0];
          }
          else if(sampleCount == 4)
          {
            static const float pattern_4x[] = {
                _SMP(-2.0f), _SMP(-6.0f), _SMP(6.0f), _SMP(-2.0f),
                _SMP(-6.0f), _SMP(2.0f),  _SMP(2.0f), _SMP(6.0f),
            };

            sample_pattern = &pattern_4x[0];
          }
          else if(sampleCount == 8)
          {
            static const float pattern_8x[] = {
                _SMP(1.0f),  _SMP(-3.0f), _SMP(-1.0f), _SMP(3.0f),  _SMP(5.0f),  _SMP(1.0f),
                _SMP(-3.0f), _SMP(-5.0f), _SMP(-5.0f), _SMP(5.0f),  _SMP(-7.0f), _SMP(-1.0f),
                _SMP(3.0f),  _SMP(7.0f),  _SMP(7.0f),  _SMP(-7.0f),
            };

            sample_pattern = &pattern_8x[0];
          }
          else if(sampleCount == 16)
          {
            static const float pattern_16x[] = {
                _SMP(1.0f),  _SMP(1.0f),  _SMP(-1.0f), _SMP(-3.0f), _SMP(-3.0f), _SMP(2.0f),
                _SMP(4.0f),  _SMP(-1.0f), _SMP(-5.0f), _SMP(-2.0f), _SMP(2.0f),  _SMP(5.0f),
                _SMP(5.0f),  _SMP(3.0f),  _SMP(3.0f),  _SMP(-5.0f), _SMP(-2.0f), _SMP(6.0f),
                _SMP(0.0f),  _SMP(-7.0f), _SMP(-4.0f), _SMP(-6.0f), _SMP(-6.0f), _SMP(4.0f),
                _SMP(-8.0f), _SMP(0.0f),  _SMP(7.0f),  _SMP(-4.0f), _SMP(6.0f),  _SMP(7.0f),
                _SMP(-7.0f), _SMP(-8.0f),
            };

            sample_pattern = &pattern_16x[0];
          }
          else    // unsupported sample count
          {
            RDCERR("Unsupported sample count on resource for sample_pos: %u", result.value.u32v[0]);

            sample_pattern = NULL;
          }

          if(sample_pattern == NULL)
          {
            result.value.f32v[0] = 0.0f;
            result.value.f32v[1] = 0.0f;
          }
          else
          {
            result.value.f32v[0] = sample_pattern[sampleIndex * 2 + 0];
            result.value.f32v[1] = sample_pattern[sampleIndex * 2 + 1];
          }
        }

#undef _SMP
      }

      // apply swizzle
      ShaderVariable swizzled("", 0.0f, 0.0f, 0.0f, 0.0f);

      for(int i = 0; i < 4; i++)
      {
        if(op.operands[1].comps[i] == 0xff)
          swizzled.value.u32v[i] = result.value.u32v[0];
        else
          swizzled.value.u32v[i] = result.value.u32v[op.operands[1].comps[i]];
      }

      // apply ret type
      if(op.operation == OPCODE_SAMPLE_POS)
      {
        result = swizzled;
        result.type = VarType::Float;
      }
      else if(op.infoRetType == RETTYPE_FLOAT)
      {
        result.value.f32v[0] = (float)swizzled.value.u32v[0];
        result.value.f32v[1] = (float)swizzled.value.u32v[1];
        result.value.f32v[2] = (float)swizzled.value.u32v[2];
        result.value.f32v[3] = (float)swizzled.value.u32v[3];
        result.type = VarType::Float;
      }
      else
      {
        result = swizzled;
        result.type = VarType::UInt;
      }

      // if we are assigning into a scalar, SetDst expects the result to be in .x (as normally we
      // are assigning FROM a scalar also).
      // to match this expectation, propogate the component across.
      if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
         op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
        result.value.u32v[0] = result.value.u32v[op.operands[0].comps[0]];

      SetDst(state, op.operands[0], op, result);

      break;
    }

    case OPCODE_BUFINFO:
    {
      size_t numIndices = program->IsShaderModel51() ? 2 : 1;
      if(op.operands[1].indices.size() == numIndices && op.operands[1].indices[0].absolute &&
         !op.operands[1].indices[0].relative)
      {
        UINT identifier = (UINT)(op.operands[1].indices[0].index & 0xffffffff);
        BindingSlot slot = GetBindingSlotForIdentifier(*program, op.operands[1].type, identifier);
        ShaderVariable result = apiWrapper->GetBufferInfo(op.operands[1].type, slot, op.str.c_str());

        MarkResourceAccess(state, op.operands[1].type, slot);

        // apply swizzle
        ShaderVariable swizzled("", 0.0f, 0.0f, 0.0f, 0.0f);

        for(int i = 0; i < 4; i++)
        {
          if(op.operands[1].comps[i] == 0xff)
            swizzled.value.u32v[i] = result.value.u32v[0];
          else
            swizzled.value.u32v[i] = result.value.u32v[op.operands[1].comps[i]];
        }

        result = swizzled;
        result.type = VarType::UInt;

        // if we are assigning into a scalar, SetDst expects the result to be in .x (as normally we
        // are assigning FROM a scalar also).
        // to match this expectation, propogate the component across.
        if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
           op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
          result.value.u32v[0] = result.value.u32v[op.operands[0].comps[0]];

        SetDst(state, op.operands[0], op, result);
      }
      else
      {
        RDCERR("Unexpected relative addressing");
        SetDst(state, op.operands[0], op, ShaderVariable(rdcstr(), 0.0f, 0.0f, 0.0f, 0.0f));
      }

      break;
    }

    case OPCODE_RESINFO:
    {
      // spec says "srcMipLevel is read as an unsigned integer scalar"
      uint32_t mipLevel = srcOpers[0].value.u32v[0];

      size_t numIndices = program->IsShaderModel51() ? 2 : 1;
      if(op.operands[2].indices.size() == numIndices && op.operands[2].indices[0].absolute &&
         !op.operands[2].indices[0].relative)
      {
        int dim = 0;
        UINT identifier = (UINT)(op.operands[2].indices[0].index & 0xffffffff);
        BindingSlot slot = GetBindingSlotForIdentifier(*program, op.operands[2].type, identifier);
        ShaderVariable result = apiWrapper->GetResourceInfo(op.operands[2].type, slot, mipLevel, dim);

        MarkResourceAccess(state, op.operands[2].type, slot);

        // need a valid dimension even if the resource was unbound, so
        // search for the declaration
        if(dim == 0)
        {
          const Declaration *pDecl =
              program->FindDeclaration(TYPE_RESOURCE, (uint32_t)op.operands[2].indices[0].index);
          if(pDecl && pDecl->declaration == OPCODE_DCL_RESOURCE)
          {
            switch(pDecl->resource.dim)
            {
              default:
              case RESOURCE_DIMENSION_UNKNOWN:
              case NUM_DIMENSIONS:
              case RESOURCE_DIMENSION_BUFFER:
              case RESOURCE_DIMENSION_RAW_BUFFER:
              case RESOURCE_DIMENSION_STRUCTURED_BUFFER:
              case RESOURCE_DIMENSION_TEXTURE1D:
              case RESOURCE_DIMENSION_TEXTURE1DARRAY: dim = 1; break;
              case RESOURCE_DIMENSION_TEXTURE2D:
              case RESOURCE_DIMENSION_TEXTURE2DMS:
              case RESOURCE_DIMENSION_TEXTURE2DARRAY:
              case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
              case RESOURCE_DIMENSION_TEXTURECUBE:
              case RESOURCE_DIMENSION_TEXTURECUBEARRAY: dim = 2; break;
              case RESOURCE_DIMENSION_TEXTURE3D: dim = 3; break;
            }
          }
        }

        // apply swizzle
        ShaderVariable swizzled("", 0.0f, 0.0f, 0.0f, 0.0f);

        for(int i = 0; i < 4; i++)
        {
          if(op.operands[2].comps[i] == 0xff)
            swizzled.value.u32v[i] = result.value.u32v[0];
          else
            swizzled.value.u32v[i] = result.value.u32v[op.operands[2].comps[i]];
        }

        // apply ret type
        if(op.infoRetType == RETTYPE_FLOAT)
        {
          result.value.f32v[0] = (float)swizzled.value.u32v[0];
          result.value.f32v[1] = (float)swizzled.value.u32v[1];
          result.value.f32v[2] = (float)swizzled.value.u32v[2];
          result.value.f32v[3] = (float)swizzled.value.u32v[3];
          result.type = VarType::Float;
        }
        else if(op.infoRetType == RETTYPE_RCPFLOAT)
        {
          // only width/height/depth values we set are reciprocated, other values
          // are just left as is
          if(dim <= 1)
            result.value.f32v[0] = 1.0f / (float)swizzled.value.u32v[0];
          else
            result.value.f32v[0] = (float)swizzled.value.u32v[0];

          if(dim <= 2)
            result.value.f32v[1] = 1.0f / (float)swizzled.value.u32v[1];
          else
            result.value.f32v[1] = (float)swizzled.value.u32v[1];

          if(dim <= 3)
            result.value.f32v[2] = 1.0f / (float)swizzled.value.u32v[2];
          else
            result.value.f32v[2] = (float)swizzled.value.u32v[2];

          result.value.f32v[3] = (float)swizzled.value.u32v[3];
          result.type = VarType::Float;
        }
        else if(op.infoRetType == RETTYPE_UINT)
        {
          result = swizzled;
          result.type = VarType::UInt;
        }

        // if we are assigning into a scalar, SetDst expects the result to be in .x (as normally we
        // are assigning FROM a scalar also).
        // to match this expectation, propogate the component across.
        if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
           op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
          result.value.u32v[0] = result.value.u32v[op.operands[0].comps[0]];

        SetDst(state, op.operands[0], op, result);
      }
      else
      {
        RDCERR("Unexpected relative addressing");
        SetDst(state, op.operands[0], op, ShaderVariable(rdcstr(), 0.0f, 0.0f, 0.0f, 0.0f));
      }

      break;
    }
    case OPCODE_SAMPLE:
    case OPCODE_SAMPLE_L:
    case OPCODE_SAMPLE_B:
    case OPCODE_SAMPLE_D:
    case OPCODE_SAMPLE_C:
    case OPCODE_SAMPLE_C_LZ:
    case OPCODE_LD:
    case OPCODE_LD_MS:
    case OPCODE_GATHER4:
    case OPCODE_GATHER4_C:
    case OPCODE_GATHER4_PO:
    case OPCODE_GATHER4_PO_C:
    case OPCODE_LOD:
    {
      if(op.operation != OPCODE_LOD && state)
        state->flags |= ShaderEvents::SampleLoadGather;

      DXBCBytecode::SamplerMode samplerMode = NUM_SAMPLERS;
      DXBCBytecode::ResourceDimension resourceDim = RESOURCE_DIMENSION_UNKNOWN;
      DXBC::ResourceRetType resourceRetType = DXBC::RETURN_TYPE_UNKNOWN;
      int sampleCount = 0;

      // Default assumptions for bindings
      Operand destOperand = op.operands[0];
      Operand resourceOperand = op.operands[2];
      Operand samplerOperand;
      if(op.operands.size() > 3)
        samplerOperand = op.operands[3];
      if(op.operation == OPCODE_GATHER4_PO || op.operation == OPCODE_GATHER4_PO_C)
      {
        resourceOperand = op.operands[3];
        samplerOperand = op.operands[4];
      }

      BindingSlot resourceBinding((uint32_t)resourceOperand.indices[0].index, 0);
      BindingSlot samplerBinding(0, 0);

      for(size_t i = 0; i < program->GetNumDeclarations(); i++)
      {
        const Declaration &decl = program->GetDeclaration(i);

        if(decl.declaration == OPCODE_DCL_SAMPLER && decl.operand.sameResource(samplerOperand))
        {
          samplerMode = decl.samplerMode;
          samplerBinding = GetBindingSlotForDeclaration(*program, decl);
        }
        if(op.operation == OPCODE_LD && decl.declaration == OPCODE_DCL_RESOURCE &&
           decl.resource.dim == RESOURCE_DIMENSION_BUFFER &&
           decl.operand.sameResource(resourceOperand))
        {
          resourceDim = decl.resource.dim;

          resourceBinding = GetBindingSlotForDeclaration(*program, decl);
          GlobalState::SRVIterator srv = global.srvs.find(resourceBinding);
          if(srv == global.srvs.end())
          {
            apiWrapper->FetchSRV(resourceBinding);
            srv = global.srvs.find(resourceBinding);
          }

          const byte *data = &srv->second.data[0];
          uint32_t offset = srv->second.firstElement;
          uint32_t numElems = srv->second.numElements;

          GlobalState::ViewFmt fmt = srv->second.format;

          data += fmt.Stride() * offset;

          ShaderVariable result;

          {
            result = ShaderVariable(rdcstr(), 0.0f, 0.0f, 0.0f, 0.0f);

            if(srcOpers[0].value.u32v[0] < numElems &&
               data + srcOpers[0].value.u32v[0] * fmt.Stride() <= srv->second.data.end())
              result = TypedUAVLoad(fmt, data + srcOpers[0].value.u32v[0] * fmt.Stride());
          }

          ShaderVariable fetch("", 0U, 0U, 0U, 0U);

          for(int c = 0; c < 4; c++)
          {
            uint8_t comp = resourceOperand.comps[c];
            if(resourceOperand.comps[c] == 0xff)
              comp = 0;

            fetch.value.u32v[c] = result.value.u32v[comp];
          }

          // if we are assigning into a scalar, SetDst expects the result to be in .x (as normally
          // we are assigning FROM a scalar also).
          // to match this expectation, propogate the component across.
          if(destOperand.comps[0] != 0xff && destOperand.comps[1] == 0xff &&
             destOperand.comps[2] == 0xff && destOperand.comps[3] == 0xff)
            fetch.value.u32v[0] = fetch.value.u32v[destOperand.comps[0]];

          SetDst(state, destOperand, op, fetch);

          MarkResourceAccess(state, TYPE_RESOURCE, resourceBinding);

          return;
        }
        if(decl.declaration == OPCODE_DCL_RESOURCE && decl.operand.sameResource(resourceOperand))
        {
          resourceDim = decl.resource.dim;
          resourceRetType = decl.resource.resType[0];
          sampleCount = decl.resource.sampleCount;

          resourceBinding = GetBindingSlotForDeclaration(*program, decl);

          // With SM5.1, resource arrays need to offset the shader register by the array index
          if(program->IsShaderModel51())
            resourceBinding.shaderRegister = srcOpers[1].value.u32v[1];

          // doesn't seem like these are ever less than four components, even if the texture is
          // declared <float3> for example.
          // shouldn't matter though is it just comes out in the wash.
          RDCASSERT(decl.resource.resType[0] == decl.resource.resType[1] &&
                    decl.resource.resType[1] == decl.resource.resType[2] &&
                    decl.resource.resType[2] == decl.resource.resType[3]);
          RDCASSERT(decl.resource.resType[0] != DXBC::RETURN_TYPE_CONTINUED &&
                    decl.resource.resType[0] != DXBC::RETURN_TYPE_UNUSED &&
                    decl.resource.resType[0] != DXBC::RETURN_TYPE_MIXED &&
                    decl.resource.resType[0] >= 0 &&
                    decl.resource.resType[0] < DXBC::NUM_RETURN_TYPES);
        }
      }

      // for lod operation, it's only defined for certain resources - otherwise just returns 0
      if(op.operation == OPCODE_LOD && resourceDim != RESOURCE_DIMENSION_TEXTURE1D &&
         resourceDim != RESOURCE_DIMENSION_TEXTURE1DARRAY &&
         resourceDim != RESOURCE_DIMENSION_TEXTURE2D &&
         resourceDim != RESOURCE_DIMENSION_TEXTURE2DARRAY &&
         resourceDim != RESOURCE_DIMENSION_TEXTURE3D && resourceDim != RESOURCE_DIMENSION_TEXTURECUBE)
      {
        ShaderVariable invalidResult("tex", 0.0f, 0.0f, 0.0f, 0.0f);

        SetDst(state, destOperand, op, invalidResult);
        break;
      }

      ShaderVariable uv = srcOpers[0];
      ShaderVariable ddxCalc;
      ShaderVariable ddyCalc;

      // these ops need DDX/DDY
      if(op.operation == OPCODE_SAMPLE || op.operation == OPCODE_SAMPLE_B ||
         op.operation == OPCODE_SAMPLE_C || op.operation == OPCODE_LOD)
      {
        if(program->GetShaderType() != DXBC::ShaderType::Pixel || prevWorkgroup.size() != 4)
        {
          RDCERR(
              "Attempt to use derivative instruction not in pixel shader. Undefined results will "
              "occur!");
        }
        else
        {
          // texture samples use coarse derivatives
          ddxCalc = DDX(false, prevWorkgroup, op.operands[1], op);
          ddyCalc = DDY(false, prevWorkgroup, op.operands[1], op);
        }
      }
      else if(op.operation == OPCODE_SAMPLE_D)
      {
        ddxCalc = srcOpers[3];
        ddyCalc = srcOpers[4];
      }

      int multisampleIndex = 0;
      if(srcOpers.size() >= 3)
        multisampleIndex = srcOpers[2].value.s32v[0];
      float lodOrCompareValue = 0.0f;
      if(srcOpers.size() >= 4)
        lodOrCompareValue = srcOpers[3].value.f32v[0];
      if(op.operation == OPCODE_GATHER4_PO_C)
        lodOrCompareValue = srcOpers[5].value.f32v[0];

      uint8_t swizzle[4] = {0};
      for(int i = 0; i < 4; i++)
      {
        if(resourceOperand.comps[i] == 0xff)
          swizzle[i] = 0;
        else
          swizzle[i] = resourceOperand.comps[i];
      }

      GatherChannel gatherChannel = GatherChannel::Red;
      if(op.operation == OPCODE_GATHER4 || op.operation == OPCODE_GATHER4_C ||
         op.operation == OPCODE_GATHER4_PO || op.operation == OPCODE_GATHER4_PO_C)
      {
        gatherChannel = (GatherChannel)samplerOperand.comps[0];
      }

      // for bias instruction we can't do a SampleGradBias, so add the bias into the sampler state.
      float samplerBias = 0.0f;
      if(op.operation == OPCODE_SAMPLE_B)
        samplerBias = srcOpers[3].value.f32v[0];

      SampleGatherResourceData resourceData;
      resourceData.dim = resourceDim;
      resourceData.retType = resourceRetType;
      resourceData.sampleCount = sampleCount;
      resourceData.binding = resourceBinding;

      SampleGatherSamplerData samplerData;
      samplerData.mode = samplerMode;
      samplerData.binding = samplerBinding;
      samplerData.bias = samplerBias;

      MarkResourceAccess(state, TYPE_RESOURCE, resourceBinding);

      ShaderVariable lookupResult("tex", 0.0f, 0.0f, 0.0f, 0.0f);
      if(apiWrapper->CalculateSampleGather(op.operation, resourceData, samplerData, uv, ddxCalc,
                                           ddyCalc, op.texelOffset, multisampleIndex,
                                           lodOrCompareValue, swizzle, gatherChannel,
                                           op.str.c_str(), lookupResult))
      {
        // should be a better way of doing this
        if(destOperand.comps[1] == 0xff)
          lookupResult.value.s32v[0] = lookupResult.value.s32v[destOperand.comps[0]];

        SetDst(state, destOperand, op, lookupResult);
      }
      else
      {
        return;
      }
      break;
    }

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // Flow control

    case OPCODE_SWITCH:
    {
      uint32_t switchValue = GetSrc(op.operands[0], op).value.u32v[0];

      int depth = 0;

      uint32_t jumpLocation = 0;

      uint32_t search = nextInstruction;

      for(; search < (uint32_t)program->GetNumInstructions(); search++)
      {
        const Operation &nextOp = program->GetInstruction((size_t)search);

        // track nested switch statements to ensure we don't accidentally pick the case from a
        // different switch
        if(nextOp.operation == OPCODE_SWITCH)
        {
          depth++;
        }
        else if(depth > 0 && nextOp.operation == OPCODE_ENDSWITCH)
        {
          depth--;
        }
        else if(depth == 0)
        {
          // note the default: location as jumpLocation if we haven't found a matching
          // case yet. If we find one later, this will be overridden
          if(nextOp.operation == OPCODE_DEFAULT)
            jumpLocation = search;

          // reached end of our switch statement
          if(nextOp.operation == OPCODE_ENDSWITCH)
            break;

          if(nextOp.operation == OPCODE_CASE)
          {
            uint32_t caseValue = GetSrc(nextOp.operands[0], nextOp).value.u32v[0];

            // comparison is defined to be bitwise
            if(caseValue == switchValue)
            {
              // we've found our case, break out
              jumpLocation = search;
              break;
            }
          }
        }
      }

      // jumpLocation points to the case we're taking, either a matching case or default

      if(jumpLocation == 0)
      {
        RDCERR("Didn't find matching case or default: for switch(%u)!", switchValue);
      }
      else
      {
        // skip straight past any case or default labels as we don't want to step to them, we want
        // next instruction to point
        // at the next excutable instruction (which might be a break if we're doing nothing)
        for(; jumpLocation < (uint32_t)program->GetNumInstructions(); jumpLocation++)
        {
          const Operation &nextOp = program->GetInstruction(jumpLocation);

          if(nextOp.operation != OPCODE_CASE && nextOp.operation != OPCODE_DEFAULT)
            break;
        }

        nextInstruction = jumpLocation;
      }

      break;
    }
    case OPCODE_CASE:
    case OPCODE_DEFAULT:
    case OPCODE_LOOP:
    case OPCODE_ENDSWITCH:
    case OPCODE_ENDIF:
      // do nothing. Basically just an anonymous label that is used elsewhere
      // (IF/ELSE/SWITCH/ENDLOOP/BREAK)
      break;
    case OPCODE_CONTINUE:
    case OPCODE_CONTINUEC:
    case OPCODE_ENDLOOP:
    {
      int depth = 0;

      int32_t test = op.operation == OPCODE_CONTINUEC ? GetSrc(op.operands[0], op).value.s32v[0] : 0;

      if(op.operation == OPCODE_CONTINUE || op.operation == OPCODE_CONTINUEC)
        depth = 1;

      if((test == 0 && !op.nonzero()) || (test != 0 && op.nonzero()) ||
         op.operation == OPCODE_CONTINUE || op.operation == OPCODE_ENDLOOP)
      {
        // skip back one to the endloop that we're processing
        nextInstruction--;

        for(; nextInstruction >= 0; nextInstruction--)
        {
          if(program->GetInstruction(nextInstruction).operation == OPCODE_ENDLOOP)
            depth++;
          if(program->GetInstruction(nextInstruction).operation == OPCODE_LOOP)
            depth--;

          if(depth == 0)
          {
            break;
          }
        }

        RDCASSERT(nextInstruction >= 0);
      }

      break;
    }
    case OPCODE_BREAK:
    case OPCODE_BREAKC:
    {
      int32_t test = op.operation == OPCODE_BREAKC ? GetSrc(op.operands[0], op).value.s32v[0] : 0;

      if((test == 0 && !op.nonzero()) || (test != 0 && op.nonzero()) || op.operation == OPCODE_BREAK)
      {
        // break out (jump to next endloop/endswitch)
        int depth = 1;

        for(; nextInstruction < (int)program->GetNumInstructions(); nextInstruction++)
        {
          if(program->GetInstruction(nextInstruction).operation == OPCODE_LOOP ||
             program->GetInstruction(nextInstruction).operation == OPCODE_SWITCH)
            depth++;
          if(program->GetInstruction(nextInstruction).operation == OPCODE_ENDLOOP ||
             program->GetInstruction(nextInstruction).operation == OPCODE_ENDSWITCH)
            depth--;

          if(depth == 0)
          {
            break;
          }
        }

        RDCASSERT(program->GetInstruction(nextInstruction).operation == OPCODE_ENDLOOP ||
                  program->GetInstruction(nextInstruction).operation == OPCODE_ENDSWITCH);

        // don't want to process the endloop and jump again!
        nextInstruction++;
      }

      break;
    }
    case OPCODE_IF:
    {
      int32_t test = GetSrc(op.operands[0], op).value.s32v[0];

      if((test == 0 && !op.nonzero()) || (test != 0 && op.nonzero()))
      {
        // nothing, we go into the if.
      }
      else
      {
        // jump to after the next matching else/endif
        int depth = 0;

        // skip back one to the if that we're processing
        nextInstruction--;

        for(; nextInstruction < (int)program->GetNumInstructions(); nextInstruction++)
        {
          if(program->GetInstruction(nextInstruction).operation == OPCODE_IF)
            depth++;
          // only step out on an else if it's the matching depth to our starting if (depth == 1)
          if(depth == 1 && program->GetInstruction(nextInstruction).operation == OPCODE_ELSE)
            depth--;
          if(program->GetInstruction(nextInstruction).operation == OPCODE_ENDIF)
            depth--;

          if(depth == 0)
          {
            break;
          }
        }

        RDCASSERT(program->GetInstruction(nextInstruction).operation == OPCODE_ELSE ||
                  program->GetInstruction(nextInstruction).operation == OPCODE_ENDIF);

        // step to next instruction after the else/endif (processing an else would skip that block)
        nextInstruction++;
      }

      break;
    }
    case OPCODE_ELSE:
    {
      // if we hit an else then we've just processed the if() bracket and need to break out (jump to
      // next endif)
      int depth = 1;

      for(; nextInstruction < (int)program->GetNumInstructions(); nextInstruction++)
      {
        if(program->GetInstruction(nextInstruction).operation == OPCODE_IF)
          depth++;
        if(program->GetInstruction(nextInstruction).operation == OPCODE_ENDIF)
          depth--;

        if(depth == 0)
        {
          break;
        }
      }

      RDCASSERT(program->GetInstruction(nextInstruction).operation == OPCODE_ENDIF);

      // step to next instruction after the else/endif (for consistency with handling in the if
      // block)
      nextInstruction++;

      break;
    }
    case OPCODE_DISCARD:
    {
      int32_t test = GetSrc(op.operands[0], op).value.s32v[0];

      if((test != 0 && !op.nonzero()) || (test == 0 && op.nonzero()))
      {
        // don't discard
        break;
      }

      // discarding.
      done = true;
      break;
    }
    case OPCODE_RET:
    case OPCODE_RETC:
    {
      int32_t test = op.operation == OPCODE_RETC ? GetSrc(op.operands[0], op).value.s32v[0] : 0;

      if((test == 0 && !op.nonzero()) || (test != 0 && op.nonzero()) || op.operation == OPCODE_RET)
      {
        // assumes not in a function call
        done = true;
      }
      break;
    }

    //////////////////////////////////////////////////////////////////////////
    // Vendor extensions
    //////////////////////////////////////////////////////////////////////////
    case OPCODE_AMD_U64_ATOMIC:
    case OPCODE_NV_U64_ATOMIC:
    {
      VendorAtomicOp atomicOp = (VendorAtomicOp)op.preciseValues;

      uint32_t resIndex = (uint32_t)op.operands[2].indices[0].index;
      ShaderVariable dstAddress, compare, value;

      int param = 2;

      if(op.texelOffset[0] == 1)
      {
        // single operand for address - simple
        dstAddress = srcOpers[param++];
      }
      else if(op.texelOffset[0] == 2)
      {
        dstAddress = srcOpers[param++];
        dstAddress.value.u32v[1] = srcOpers[param++].value.u32v[0];
        dstAddress.value.u32v[2] = srcOpers[param++].value.u32v[2];
      }
      else
      {
        RDCERR("Unexpected parameter compression value %d ", op.texelOffset[0]);
        break;
      }

      if(atomicOp == ATOMIC_OP_CAS)
      {
        if(op.texelOffset[1] == 1)
        {
          compare = srcOpers[param++];
        }
        else if(op.texelOffset[1] == 2)
        {
          compare = srcOpers[param++];
          compare.value.u32v[1] = srcOpers[param++].value.u32v[0];
          compare.value.u32v[2] = srcOpers[param++].value.u32v[2];
        }
        else
        {
          RDCERR("Unexpected parameter compression value %d ", op.texelOffset[1]);
          break;
        }
      }

      if(op.texelOffset[2] == 1)
      {
        value = srcOpers[param++];
      }
      else if(op.texelOffset[2] == 2)
      {
        value = srcOpers[param++];
        value.value.u32v[1] = srcOpers[param++].value.u32v[0];
        value.value.u32v[2] = srcOpers[param++].value.u32v[2];
      }
      else
      {
        RDCERR("Unexpected parameter compression value %d ", op.texelOffset[2]);
        break;
      }

      BindingSlot slot = GetBindingSlotForIdentifier(*program, TYPE_UNORDERED_ACCESS_VIEW, resIndex);
      GlobalState::UAVIterator uav = global.uavs.find(slot);
      if(uav == global.uavs.end())
      {
        apiWrapper->FetchUAV(slot);
        uav = global.uavs.find(slot);
      }

      MarkResourceAccess(state, TYPE_UNORDERED_ACCESS_VIEW, slot);

      const uint32_t stride = sizeof(uint64_t);
      byte *data = &uav->second.data[0];

      RDCASSERT(data);

      if(data)
      {
        if(uav->second.tex)
        {
          data += dstAddress.value.u32v[0] * stride;
          data += dstAddress.value.u32v[1] * uav->second.rowPitch;
          data += dstAddress.value.u32v[2] * uav->second.depthPitch;
        }
        else
        {
          data += uav->second.firstElement * stride + dstAddress.value.u32v[0];
        }
      }

      if(data && data < uav->second.data.end() && !Finished())
      {
        ShaderVariable result(rdcstr(), 0U, 0U, 0U, 0U);

        uint64_t *data64 = (uint64_t *)data;

        result.value.u32v[0] = uint32_t(*data64);
        SetDst(state, op.operands[0], op, result);
        result.value.u32v[0] = uint32_t((*data64) >> 32U);
        SetDst(state, op.operands[1], op, result);

        uint64_t compare64 = compare.value.u64v[0];
        uint64_t value64 = value.value.u64v[0];

        switch(atomicOp)
        {
          case ATOMIC_OP_NONE: break;
          case ATOMIC_OP_AND: *data64 = *data64 & value64; break;
          case ATOMIC_OP_OR: *data64 = *data64 | value64; break;
          case ATOMIC_OP_XOR: *data64 = *data64 ^ value64; break;
          case ATOMIC_OP_ADD: *data64 = *data64 + value64; break;
          case ATOMIC_OP_MAX: *data64 = RDCMAX(*data64, value64); break;
          case ATOMIC_OP_MIN: *data64 = RDCMIN(*data64, value64); break;
          case ATOMIC_OP_SWAP: *data64 = value64; break;
          case ATOMIC_OP_CAS:
            if(*data64 == compare64)
              *data64 = value64;
            break;
        }
      }
      break;
    }

    //////////////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////////////
    default:
    {
      RDCERR("Unsupported operation %d in assembly debugging", op.operation);
      break;
    }
  }
}

BindingSlot GetBindingSlotForDeclaration(const Program &program, const DXBCBytecode::Declaration &decl)
{
  uint32_t baseRegister = program.IsShaderModel51() ? (uint32_t)decl.operand.indices[1].index
                                                    : (uint32_t)decl.operand.indices[0].index;

  return BindingSlot(baseRegister, decl.space);
}

BindingSlot GetBindingSlotForIdentifier(const Program &program, OperandType declType,
                                        uint32_t identifier)
{
  // A note on matching declarations: with SM 5.0 or lower, the declaration will have a single
  // operand index, which corresponds to the bound slot (e.g., t0 for a SRV). With SM 5.1, the
  // declaration has three operand indices: the logical binding slot, the start register, and
  // the end register. In addition, the register space is specified.

  // In order to match a declaration, we use the logical binding slot. This is identical for
  // all cases - with SM 5.1 the compiler translates each binding from shader register(s) &
  // register space into a unique identifier that is used to reference it in other instructions.
  // For example, an SRV specified with (t0, space2) could be given T1 as its identifier.

  // When matching declarations, use operand index 0 to match with the instruction operand.
  // When fetching data for a resource, with SM 5.0 or lower, use operand 0 as the shader
  // register, and decl.space (which will always be 0) for the register space. With SM 5.1,
  // use operand index 1 and 2 to get the shader register and use decl.space for the
  // register space (which can be any value, as specified in HLSL and the root signature).

  // TODO: Need to test resource arrays to ensure correct behavior with SM 5.1 here

  if(program.IsShaderModel51())
  {
    const Declaration *pDecl = program.FindDeclaration(declType, identifier);
    if(pDecl)
      return GetBindingSlotForDeclaration(program, *pDecl);

    RDCERR("Unable to find matching declaration for identifier %u", identifier);
  }

  return BindingSlot(identifier, 0);
}

void GlobalState::PopulateGroupshared(const DXBCBytecode::Program *pBytecode)
{
  for(size_t i = 0; i < pBytecode->GetNumDeclarations(); i++)
  {
    const DXBCBytecode::Declaration &decl = pBytecode->GetDeclaration(i);

    if(decl.declaration == DXBCBytecode::OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW ||
       decl.declaration == DXBCBytecode::OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED)
    {
      uint32_t slot = (uint32_t)decl.operand.indices[0].index;

      if(groupshared.size() <= slot)
      {
        groupshared.resize(slot + 1);

        groupsharedMem &mem = groupshared[slot];

        mem.structured =
            (decl.declaration == DXBCBytecode::OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED);

        if(mem.structured)
        {
          mem.count = decl.tsgm_structured.count;
          mem.bytestride = decl.tsgm_structured.stride;
        }
        else
        {
          mem.count = decl.tgsmCount;
          mem.bytestride = 4;    // raw groupshared is implicitly uint32s
        }

        mem.data.resize(mem.bytestride * mem.count);
      }
    }
  }
}

uint32_t GetLogicalIdentifierForBindingSlot(const DXBCBytecode::Program &program,
                                            OperandType declType, const DXBCDebug::BindingSlot &slot)
{
  uint32_t idx = slot.shaderRegister;
  if(program.IsShaderModel51())
  {
    // Need to lookup the logical identifier from the declarations
    size_t numDeclarations = program.GetNumDeclarations();
    for(size_t d = 0; d < numDeclarations; ++d)
    {
      const DXBCBytecode::Declaration &decl = program.GetDeclaration(d);
      if(decl.operand.type == declType && decl.space == slot.registerSpace &&
         decl.operand.indices[1].index <= slot.shaderRegister &&
         decl.operand.indices[2].index >= slot.shaderRegister)
      {
        idx = (uint32_t)decl.operand.indices[0].index;
        break;
      }
    }
  }

  return idx;
}

void AddCBufferToGlobalState(const DXBCBytecode::Program &program, GlobalState &global,
                             rdcarray<SourceVariableMapping> &sourceVars,
                             const ShaderReflection &refl, const BindingSlot &slot, bytebuf &cbufData)
{
  // Find the identifier
  size_t numCBs = refl.constantBlocks.size();
  for(size_t i = 0; i < numCBs; ++i)
  {
    const ConstantBlock &cb = refl.constantBlocks[i];
    if(slot.registerSpace == (uint32_t)cb.fixedBindSetOrSpace &&
       slot.shaderRegister >= (uint32_t)cb.fixedBindNumber &&
       slot.shaderRegister < (uint32_t)(cb.fixedBindNumber + cb.bindArraySize))
    {
      uint32_t arrayIndex = slot.shaderRegister - cb.fixedBindNumber;

      rdcarray<ShaderVariable> &targetVars =
          cb.bindArraySize > 1 ? global.constantBlocks[i].members[arrayIndex].members
                               : global.constantBlocks[i].members;
      RDCASSERTMSG("Reassigning previously filled cbuffer", targetVars.empty());

      uint32_t cbufferIndex =
          GetLogicalIdentifierForBindingSlot(program, DXBCBytecode::TYPE_CONSTANT_BUFFER, slot);

      global.constantBlocks[i].name =
          program.GetRegisterName(DXBCBytecode::TYPE_CONSTANT_BUFFER, cbufferIndex);

      SourceVariableMapping cbSourceMapping;
      cbSourceMapping.name = refl.constantBlocks[i].name;
      cbSourceMapping.variables.push_back(
          DebugVariableReference(DebugVariableType::Constant, global.constantBlocks[i].name));
      sourceVars.push_back(cbSourceMapping);

      rdcstr identifierPrefix = global.constantBlocks[i].name;
      rdcstr variablePrefix = refl.constantBlocks[i].name;
      if(cb.bindArraySize > 1)
      {
        identifierPrefix =
            StringFormat::Fmt("%s[%u]", global.constantBlocks[i].name.c_str(), arrayIndex);
        variablePrefix = StringFormat::Fmt("%s[%u]", refl.constantBlocks[i].name.c_str(), arrayIndex);

        // The above sourceVar is for the logical identifier, and FlattenVariables adds the
        // individual elements of the constant buffer. For CB arrays, add an extra source
        // var for the CB array index
        SourceVariableMapping cbArrayMapping;
        global.constantBlocks[i].members[arrayIndex].name = StringFormat::Fmt("[%u]", arrayIndex);
        cbArrayMapping.name = variablePrefix;
        cbArrayMapping.variables.push_back(
            DebugVariableReference(DebugVariableType::Constant, identifierPrefix));
        sourceVars.push_back(cbArrayMapping);
      }
      const rdcarray<ShaderConstant> &constants =
          (cb.bindArraySize > 1) ? refl.constantBlocks[i].variables[0].type.members
                                 : refl.constantBlocks[i].variables;

      rdcarray<ShaderVariable> vars;
      StandardFillCBufferVariables(refl.resourceId, constants, vars, cbufData);
      FlattenVariables(identifierPrefix, constants, vars, targetVars, variablePrefix + ".", 0,
                       sourceVars);

      for(size_t c = 0; c < targetVars.size(); c++)
      {
        targetVars[c].name = StringFormat::Fmt("[%u]", (uint32_t)c);
      }

      return;
    }
  }
}

void ApplyDerivatives(GlobalState &global, rdcarray<ThreadState> &quad, int reg, int element,
                      int numWords, float *data, float signmul, int32_t quadIdxA, int32_t quadIdxB)
{
  for(int w = 0; w < numWords; w++)
  {
    quad[quadIdxA].inputs[reg].value.f32v[element + w] += signmul * data[w];
    if(quadIdxB >= 0)
      quad[quadIdxB].inputs[reg].value.f32v[element + w] += signmul * data[w];
  }

  // quick check to see if this register was evaluated
  if(global.sampleEvalRegisterMask & (1ULL << reg))
  {
    // apply derivative to any cached sample evaluations on these quad indices
    for(auto it = global.sampleEvalCache.begin(); it != global.sampleEvalCache.end(); ++it)
    {
      if((it->first.quadIndex == quadIdxA || it->first.quadIndex == quadIdxB) &&
         reg == it->first.inputRegisterIndex)
      {
        for(int w = 0; w < numWords; w++)
          it->second.value.f32v[element + w] += data[w];
      }
    }
  }
}

void ApplyAllDerivatives(GlobalState &global, rdcarray<ThreadState> &quad, int destIdx,
                         const rdcarray<PSInputElement> &initialValues, float *data)
{
  // We make the assumption that the coarse derivatives are generated from (0,0) in the quad, and
  // fine derivatives are generated from the destination index and its neighbours in X and Y.
  // This isn't spec'd but we must assume something and this will hopefully get us closest to
  // reproducing actual results.
  //
  // For debugging, we need members of the quad to be able to generate coarse and fine
  // derivatives.
  //
  // For (0,0) we only need the coarse derivatives to get our neighbours (1,0) and (0,1) which
  // will give us coarse and fine derivatives being identical.
  //
  // For the others we will need to use a combination of coarse and fine derivatives to get the
  // diagonal element in the quad. In the examples below, remember that the quad indices are:
  //
  // +---+---+
  // | 0 | 1 |
  // +---+---+
  // | 2 | 3 |
  // +---+---+
  //
  // And that we have definitions of the derivatives:
  //
  // ddx_coarse = (1,0) - (0,0)
  // ddy_coarse = (0,1) - (0,0)
  //
  // i.e. the same for all members of the quad
  //
  // ddx_fine   = (x,y) - (1-x,y)
  // ddy_fine   = (x,y) - (x,1-y)
  //
  // i.e. the difference to the neighbour of our desired invocation (the one we have the actual
  // inputs for, from gathering above).
  //
  // So e.g. if our thread is at (1,1) destIdx = 3
  //
  // (1,0) = (1,1) - ddx_fine
  // (0,1) = (1,1) - ddy_fine
  // (0,0) = (1,1) - ddy_fine - ddx_coarse
  //
  // and ddy_coarse is unused. For (1,0) destIdx = 1:
  //
  // (1,1) = (1,0) + ddy_fine
  // (0,1) = (1,0) - ddx_coarse + ddy_coarse
  // (0,0) = (1,0) - ddx_coarse
  //
  // and ddx_fine is unused (it's identical to ddx_coarse anyway)

  // this is the value of input[1] - input[0]
  float *ddx_coarse = (float *)data;

  for(size_t i = 0; i < initialValues.size(); i++)
  {
    if(!initialValues[i].included)
      continue;

    if(initialValues[i].reg >= 0)
    {
      if(destIdx == 0)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddx_coarse, 1.0f, 1, 3);
      else if(destIdx == 1)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddx_coarse, -1.0f, 0, 2);
      else if(destIdx == 2)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddx_coarse, 1.0f, 1, -1);
      else if(destIdx == 3)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddx_coarse, -1.0f, 0, -1);
    }

    ddx_coarse += initialValues[i].numwords;
  }

  // this is the value of input[2] - input[0]
  float *ddy_coarse = ddx_coarse;

  for(size_t i = 0; i < initialValues.size(); i++)
  {
    if(!initialValues[i].included)
      continue;

    if(initialValues[i].reg >= 0)
    {
      if(destIdx == 0)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddy_coarse, 1.0f, 2, 3);
      else if(destIdx == 1)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddy_coarse, 1.0f, 2, -1);
      else if(destIdx == 2)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddy_coarse, -1.0f, 0, 1);
    }

    ddy_coarse += initialValues[i].numwords;
  }

  float *ddxfine = ddy_coarse;

  for(size_t i = 0; i < initialValues.size(); i++)
  {
    if(!initialValues[i].included)
      continue;

    if(initialValues[i].reg >= 0)
    {
      if(destIdx == 2)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddxfine, 1.0f, 3, -1);
      else if(destIdx == 3)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddxfine, -1.0f, 2, -1);
    }

    ddxfine += initialValues[i].numwords;
  }

  float *ddyfine = ddxfine;

  for(size_t i = 0; i < initialValues.size(); i++)
  {
    if(!initialValues[i].included)
      continue;

    if(initialValues[i].reg >= 0)
    {
      if(destIdx == 1)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddyfine, 1.0f, 3, -1);
      else if(destIdx == 3)
        ApplyDerivatives(global, quad, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddyfine, -1.0f, 0, 1);
    }

    ddyfine += initialValues[i].numwords;
  }
}

void FillViewFmt(DXGI_FORMAT format, GlobalState::ViewFmt &viewFmt)
{
  if(format != DXGI_FORMAT_UNKNOWN)
  {
    ResourceFormat fmt = MakeResourceFormat(format);

    viewFmt.byteWidth = fmt.compByteWidth;
    viewFmt.numComps = fmt.compCount;
    viewFmt.fmt = fmt.compType;

    if(format == DXGI_FORMAT_R11G11B10_FLOAT)
      viewFmt.byteWidth = 11;
    else if(format == DXGI_FORMAT_R10G10B10A2_UINT || format == DXGI_FORMAT_R10G10B10A2_UNORM)
      viewFmt.byteWidth = 10;
  }
}

void LookupSRVFormatFromShaderReflection(const DXBC::Reflection &reflection,
                                         const BindingSlot &slot, GlobalState::ViewFmt &viewFmt)
{
  for(const DXBC::ShaderInputBind &bind : reflection.SRVs)
  {
    if(bind.reg == slot.shaderRegister && bind.space == slot.registerSpace &&
       bind.dimension == DXBC::ShaderInputBind::DIM_BUFFER &&
       bind.retType < DXBC::RETURN_TYPE_MIXED && bind.retType != DXBC::RETURN_TYPE_UNKNOWN)
    {
      viewFmt.byteWidth = 4;
      viewFmt.numComps = bind.numComps;

      if(bind.retType == DXBC::RETURN_TYPE_UNORM)
        viewFmt.fmt = CompType::UNorm;
      else if(bind.retType == DXBC::RETURN_TYPE_SNORM)
        viewFmt.fmt = CompType::SNorm;
      else if(bind.retType == DXBC::RETURN_TYPE_UINT)
        viewFmt.fmt = CompType::UInt;
      else if(bind.retType == DXBC::RETURN_TYPE_SINT)
        viewFmt.fmt = CompType::SInt;
      else
        viewFmt.fmt = CompType::Float;

      break;
    }
  }
}

DXBCBytecode::InterpolationMode GetInterpolationModeForInputParam(const SigParameter &sig,
                                                                  const DXBC::Reflection &psDxbc,
                                                                  const DXBCBytecode::Program *program)
{
  if(sig.varType == VarType::SInt || sig.varType == VarType::UInt)
    return DXBCBytecode::InterpolationMode::INTERPOLATION_CONSTANT;

  if(sig.varType == VarType::Float)
  {
    // if we're packed with ints on either side, we must be nointerpolation
    size_t numInputs = psDxbc.InputSig.size();
    for(size_t j = 0; j < numInputs; j++)
    {
      if(sig.regIndex == psDxbc.InputSig[j].regIndex && psDxbc.InputSig[j].varType != VarType::Float)
        return DXBCBytecode::InterpolationMode::INTERPOLATION_CONSTANT;
    }

    DXBCBytecode::InterpolationMode interpolation = DXBCBytecode::INTERPOLATION_UNDEFINED;

    if(program)
    {
      for(size_t d = 0; d < program->GetNumDeclarations(); d++)
      {
        const DXBCBytecode::Declaration &decl = program->GetDeclaration(d);

        if(decl.declaration == DXBCBytecode::OPCODE_DCL_INPUT_PS &&
           decl.operand.indices[0].absolute && decl.operand.indices[0].index == sig.regIndex)
        {
          interpolation = decl.inputOutput.inputInterpolation;
          break;
        }
      }
    }

    return interpolation;
  }

  RDCERR("Unexpected input signature type: %s", ToStr(sig.varType).c_str());
  return DXBCBytecode::InterpolationMode::INTERPOLATION_UNDEFINED;
}

void GatherPSInputDataForInitialValues(const DXBC::DXBCContainer *dxbc,
                                       const DXBC::Reflection &prevStageDxbc,
                                       rdcarray<PSInputElement> &initialValues,
                                       rdcarray<rdcstr> &floatInputs, rdcarray<rdcstr> &inputVarNames,
                                       rdcstr &psInputDefinition, int &structureStride)
{
  const DXBC::Reflection &psDxbc = *dxbc->GetReflection();
  const DXBCBytecode::Program *program = dxbc->GetDXBCByteCode();

  // When debugging a pixel shader, we need to get the initial values of each pixel shader
  // input for the pixel that we are debugging, from whichever the previous shader stage was
  // configured in the pipeline. This function returns the input element definitions, other
  // associated data, the HLSL definition to use when gathering pixel shader initial values,
  // and the stride of that HLSL structure.

  // This function does not provide any HLSL definitions for additional metadata that may be
  // needed for gathering initial values, such as primitive ID, and also does not provide the
  // shader function body.

  initialValues.clear();
  floatInputs.clear();
  inputVarNames.clear();
  psInputDefinition = "struct PSInput\n{\n";
  structureStride = 0;

  if(psDxbc.InputSig.empty())
  {
    psInputDefinition += "float4 input_dummy : SV_Position;\n";

    initialValues.push_back(PSInputElement(-1, 0, 4, ShaderBuiltin::Undefined, true));

    structureStride += 4;
  }

  // name, pair<start semantic index, end semantic index>
  rdcarray<rdcpair<rdcstr, rdcpair<uint32_t, uint32_t>>> arrays;

  uint32_t nextreg = 0;

  size_t numInputs = psDxbc.InputSig.size();
  inputVarNames.resize(numInputs);

  for(size_t i = 0; i < numInputs; i++)
  {
    const SigParameter &sig = psDxbc.InputSig[i];

    psInputDefinition += "  ";

    bool included = true;

    // handled specially to account for SV_ ordering
    if(sig.systemValue == ShaderBuiltin::MSAACoverage ||
       sig.systemValue == ShaderBuiltin::IsFrontFace ||
       sig.systemValue == ShaderBuiltin::MSAASampleIndex)
    {
      psInputDefinition += "//";
      included = false;
    }

    // it seems sometimes primitive ID can be included within inputs and isn't subject to the SV_
    // ordering restrictions - possibly to allow for geometry shaders to output the primitive ID as
    // an interpolant. Only comment it out if it's the last input.
    if(i + 1 == numInputs && sig.systemValue == ShaderBuiltin::PrimitiveIndex)
    {
      psInputDefinition += "//";
      included = false;
    }

    int arrayIndex = -1;

    for(size_t a = 0; a < arrays.size(); a++)
    {
      if(sig.semanticName == arrays[a].first && arrays[a].second.first <= sig.semanticIndex &&
         arrays[a].second.second >= sig.semanticIndex)
      {
        psInputDefinition += "//";
        included = false;
        arrayIndex = sig.semanticIndex - arrays[a].second.first;
      }
    }

    int missingreg = int(sig.regIndex) - int(nextreg);

    // fill in holes from output sig of previous shader if possible, to try and
    // ensure the same register order
    for(int dummy = 0; dummy < missingreg; dummy++)
    {
      bool filled = false;

      size_t numPrevOutputs = prevStageDxbc.OutputSig.size();
      for(size_t os = 0; os < numPrevOutputs; os++)
      {
        if(prevStageDxbc.OutputSig[os].regIndex == nextreg + dummy)
        {
          filled = true;

          if(prevStageDxbc.OutputSig[os].varType == VarType::Float)
            psInputDefinition += "float";
          else if(prevStageDxbc.OutputSig[os].varType == VarType::SInt)
            psInputDefinition += "int";
          else if(prevStageDxbc.OutputSig[os].varType == VarType::UInt)
            psInputDefinition += "uint";
          else
            RDCERR("Unexpected input signature type: %s",
                   ToStr(prevStageDxbc.OutputSig[os].varType).c_str());

          int numCols = (prevStageDxbc.OutputSig[os].regChannelMask & 0x1 ? 1 : 0) +
                        (prevStageDxbc.OutputSig[os].regChannelMask & 0x2 ? 1 : 0) +
                        (prevStageDxbc.OutputSig[os].regChannelMask & 0x4 ? 1 : 0) +
                        (prevStageDxbc.OutputSig[os].regChannelMask & 0x8 ? 1 : 0);

          structureStride += 4 * numCols;

          initialValues.push_back(PSInputElement(-1, 0, numCols, ShaderBuiltin::Undefined, true));

          rdcstr name = prevStageDxbc.OutputSig[os].semanticIdxName;

          psInputDefinition += ToStr((uint32_t)numCols) + " input_" + name + " : " + name + ";\n";
        }
      }

      if(!filled)
      {
        rdcstr dummy_reg = "dummy_register";
        dummy_reg += ToStr((uint32_t)nextreg + dummy);
        psInputDefinition += "float4 var_" + dummy_reg + " : semantic_" + dummy_reg + ";\n";

        initialValues.push_back(PSInputElement(-1, 0, 4, ShaderBuiltin::Undefined, true));

        structureStride += 4 * sizeof(float);
      }
    }

    nextreg = sig.regIndex + 1;

    DXBCBytecode::InterpolationMode interpolation =
        GetInterpolationModeForInputParam(sig, psDxbc, program);
    if(interpolation != DXBCBytecode::INTERPOLATION_UNDEFINED)
      psInputDefinition += ToStr(interpolation) + " ";
    psInputDefinition += ToStr(sig.varType);

    int numCols = (sig.regChannelMask & 0x1 ? 1 : 0) + (sig.regChannelMask & 0x2 ? 1 : 0) +
                  (sig.regChannelMask & 0x4 ? 1 : 0) + (sig.regChannelMask & 0x8 ? 1 : 0);

    rdcstr name = sig.semanticIdxName;

    // arrays of interpolators are handled really weirdly. They use cbuffer
    // packing rules where each new value is in a new register (rather than
    // e.g. 2 x float2 in a single register), but that's pointless because
    // you can't dynamically index into input registers.
    // If we declare those elements as a non-array, the float2s or floats
    // will be packed into registers and won't match up to the previous
    // shader.
    // HOWEVER to add an extra bit of fun, fxc will happily pack other
    // parameters not in the array into spare parts of the registers.
    //
    // So I think the upshot is that we can detect arrays reliably by
    // whenever we encounter a float or float2 at the start of a register,
    // search forward to see if the next register has an element that is the
    // same semantic name and one higher semantic index. If so, there's an
    // array, so keep searching to enumerate its length.
    // I think this should be safe if the packing just happens to place those
    // registers together.

    int arrayLength = 0;

    if(included && numCols <= 2 && (sig.regChannelMask & 0x1))
    {
      uint32_t nextIdx = sig.semanticIndex + 1;

      for(size_t j = i + 1; j < numInputs; j++)
      {
        const SigParameter &jSig = psDxbc.InputSig[j];

        // if we've found the 'next' semantic
        if(sig.semanticName == jSig.semanticName && nextIdx == jSig.semanticIndex)
        {
          int jNumCols = (jSig.regChannelMask & 0x1 ? 1 : 0) + (jSig.regChannelMask & 0x2 ? 1 : 0) +
                         (jSig.regChannelMask & 0x4 ? 1 : 0) + (jSig.regChannelMask & 0x8 ? 1 : 0);

          DXBCBytecode::InterpolationMode jInterp =
              GetInterpolationModeForInputParam(jSig, psDxbc, program);

          // if it's the same size, type, and interpolation mode, then it could potentially be
          // packed into an array. Check if it's using the first channel component to tell whether
          // it's tightly packed with another semantic.
          if(jNumCols == numCols && interpolation == jInterp && sig.varType == jSig.varType &&
             jSig.regChannelMask & 0x1)
          {
            if(arrayLength == 0)
              arrayLength = 2;
            else
              arrayLength++;

            // continue searching now
            nextIdx++;
            j = i + 1;
            continue;
          }
        }
      }

      if(arrayLength > 0)
        arrays.push_back(
            make_rdcpair(sig.semanticName, make_rdcpair((uint32_t)sig.semanticIndex, nextIdx - 1)));
    }

    if(included)
    {
      // in UAV structs, arrays are packed tightly, so just multiply by arrayLength
      structureStride += 4 * numCols * RDCMAX(1, arrayLength);
    }

    // as another side effect of the above, an element declared as a 1-length array won't be
    // detected but it WILL be put in its own register (not packed together), so detect this
    // case too.
    // Note we have to search *backwards* because we need to know if this register should have
    // been packed into the previous register, but wasn't. float/float2/float3 can be packed after
    // an array just fine, so long as the sum of their components doesn't exceed a register width
    if(included && i > 0 && arrayLength == 0)
    {
      const SigParameter &prev = psDxbc.InputSig[i - 1];

      if(prev.regIndex != sig.regIndex && prev.compCount + sig.compCount <= 4)
        arrayLength = 1;
    }

    // The compiler is also really annoying and will go to great lengths to rearrange elements
    // and screw up our declaration, to pack things together. E.g.:
    // float2 a : TEXCOORD1;
    // float4 b : TEXCOORD2;
    // float4 c : TEXCOORD3;
    // float2 d : TEXCOORD4;
    // the compiler will move d up and pack it into the last two components of a.
    // To prevent this, we look forward and backward to check that we aren't expecting to pack
    // with anything, and if not then we just make it a 1-length array to ensure no packing.
    // Note the regChannelMask & 0x1 means it is using .x, so it's not the tail-end of a pack
    if(included && arrayLength == 0 && numCols <= 2 && (sig.regChannelMask & 0x1))
    {
      if(i == numInputs - 1)
      {
        // the last element is never packed
        arrayLength = 1;
      }
      else
      {
        // if the next reg is using .x, it wasn't packed with us
        if(psDxbc.InputSig[i + 1].regChannelMask & 0x1)
          arrayLength = 1;
      }
    }

    psInputDefinition += ToStr((uint32_t)numCols) + " input_" + name;
    if(arrayLength > 0)
      psInputDefinition += "[" + ToStr(arrayLength) + "]";
    psInputDefinition += " : " + name;

    inputVarNames[i] = "input_" + name;
    if(arrayLength > 0)
      inputVarNames[i] += StringFormat::Fmt("[%d]", RDCMAX(0, arrayIndex));

    if(included && sig.varType == VarType::Float)
    {
      if(arrayLength == 0)
      {
        floatInputs.push_back("input_" + name);
      }
      else
      {
        for(int a = 0; a < arrayLength; a++)
          floatInputs.push_back("input_" + name + "[" + ToStr(a) + "]");
      }
    }

    psInputDefinition += ";\n";

    int firstElem = sig.regChannelMask & 0x1   ? 0
                    : sig.regChannelMask & 0x2 ? 1
                    : sig.regChannelMask & 0x4 ? 2
                    : sig.regChannelMask & 0x8 ? 3
                                               : -1;

    // arrays get added all at once (because in the struct data, they are contiguous even if
    // in the input signature they're not).
    if(arrayIndex < 0)
    {
      if(arrayLength == 0)
      {
        initialValues.push_back(
            PSInputElement(sig.regIndex, firstElem, numCols, sig.systemValue, included));
      }
      else
      {
        for(int a = 0; a < arrayLength; a++)
        {
          initialValues.push_back(
              PSInputElement(sig.regIndex + a, firstElem, numCols, sig.systemValue, included));
        }
      }
    }
  }

  psInputDefinition += "};\n\n";
}

ShaderDebugTrace *InterpretDebugger::BeginDebug(const DXBC::DXBCContainer *dxbcContainer,
                                                const ShaderReflection &refl, int activeIndex)
{
  ShaderDebugTrace *ret = new ShaderDebugTrace;
  ret->debugger = this;
  ret->stage = refl.stage;

  this->dxbc = dxbcContainer;
  this->activeLaneIndex = activeIndex;

  int workgroupSize = dxbc->m_Type == DXBC::ShaderType::Pixel ? 4 : 1;
  for(int i = 0; i < workgroupSize; i++)
    workgroup.push_back(ThreadState(i, global, dxbc));

  if(dxbc->m_Type == DXBC::ShaderType::Compute)
    global.PopulateGroupshared(dxbc->GetDXBCByteCode());

  ThreadState &state = activeLane();

  int32_t maxReg = -1;
  for(const SigParameter &sig : dxbc->GetReflection()->InputSig)
  {
    if(sig.regIndex != ~0U)
      maxReg = RDCMAX(maxReg, (int32_t)sig.regIndex);
  }

  const bool inputCoverage = dxbc->GetDXBCByteCode()->HasCoverageInput();

  // Add inputs to the shader trace
  if(maxReg >= 0 || inputCoverage)
  {
    state.inputs.resize(maxReg + 1 + (inputCoverage ? 1 : 0));
    for(int32_t sigIdx = 0; sigIdx < dxbc->GetReflection()->InputSig.count(); sigIdx++)
    {
      const SigParameter &sig = dxbc->GetReflection()->InputSig[sigIdx];

      ShaderVariable v;

      v.name = dxbc->GetDXBCByteCode()->GetRegisterName(DXBCBytecode::TYPE_INPUT, sig.regIndex);
      v.rows = 1;
      v.columns = sig.regChannelMask & 0x8   ? 4
                  : sig.regChannelMask & 0x4 ? 3
                  : sig.regChannelMask & 0x2 ? 2
                  : sig.regChannelMask & 0x1 ? 1
                                             : 0;
      v.type = sig.varType;

      ShaderVariable &dst = state.inputs[sig.regIndex];

      // if the variable hasn't been initialised, just assign. If it has, we're in a situation where
      // two input parameters are assigned to the same variable overlapping, so just update the
      // number of columns to the max of both. The source mapping (either from debug info or our own
      // below) will handle distinguishing better.
      if(dst.name.empty())
        dst = v;
      else
        dst.columns = RDCMAX(dst.columns, v.columns);

      {
        SourceVariableMapping sourcemap;
        sourcemap.name = sig.semanticIdxName;
        if(sourcemap.name.empty() && sig.systemValue != ShaderBuiltin::Undefined)
          sourcemap.name = ToStr(sig.systemValue);
        sourcemap.type = v.type;
        sourcemap.rows = 1;
        sourcemap.columns = sig.compCount;
        sourcemap.variables.reserve(sig.compCount);
        sourcemap.signatureIndex = sigIdx;

        for(uint16_t c = 0; c < 4; c++)
        {
          if(sig.regChannelMask & (1 << c))
          {
            DebugVariableReference ref;
            ref.name = v.name;
            ref.type = DebugVariableType::Input;
            ref.component = c;
            sourcemap.variables.push_back(ref);
          }
        }

        ret->sourceVars.push_back(sourcemap);
      }
    }

    // Put the coverage mask at the end
    if(inputCoverage)
    {
      state.inputs.back() = ShaderVariable(
          dxbc->GetDXBCByteCode()->GetRegisterName(DXBCBytecode::TYPE_INPUT_COVERAGE_MASK, 0), 0U,
          0U, 0U, 0U);
      state.inputs.back().columns = 1;

      {
        SourceVariableMapping sourcemap;
        sourcemap.name = "SV_Coverage";
        sourcemap.type = VarType::UInt;
        sourcemap.rows = 1;
        sourcemap.columns = 1;
        // no corresponding signature element for this - maybe we should generate one?
        sourcemap.signatureIndex = -1;
        DebugVariableReference ref;
        ref.type = DebugVariableType::Input;
        ref.name = state.inputs.back().name;
        sourcemap.variables.push_back(ref);

        ret->sourceVars.push_back(sourcemap);
      }
    }
  }

  // Set up outputs in the shader state
  for(int32_t sigIdx = 0; sigIdx < dxbc->GetReflection()->OutputSig.count(); sigIdx++)
  {
    const SigParameter &sig = dxbc->GetReflection()->OutputSig[sigIdx];

    DXBCBytecode::OperandType type = DXBCBytecode::TYPE_OUTPUT;

    if(sig.systemValue == ShaderBuiltin::DepthOutput)
      type = DXBCBytecode::TYPE_OUTPUT_DEPTH;
    else if(sig.systemValue == ShaderBuiltin::DepthOutputLessEqual)
      type = DXBCBytecode::TYPE_OUTPUT_DEPTH_LESS_EQUAL;
    else if(sig.systemValue == ShaderBuiltin::DepthOutputGreaterEqual)
      type = DXBCBytecode::TYPE_OUTPUT_DEPTH_GREATER_EQUAL;
    else if(sig.systemValue == ShaderBuiltin::MSAACoverage)
      type = DXBCBytecode::TYPE_OUTPUT_COVERAGE_MASK;
    else if(sig.systemValue == ShaderBuiltin::StencilReference)
      type = DXBCBytecode::TYPE_OUTPUT_STENCIL_REF;

    if(type == DXBCBytecode::TYPE_OUTPUT && sig.regIndex == ~0U)
    {
      RDCERR("Unhandled output: %s (%s)", sig.semanticName.c_str(), ToStr(sig.systemValue).c_str());
      continue;
    }

    uint32_t idx = dxbc->GetDXBCByteCode()->GetRegisterIndex(type, sig.regIndex);

    if(idx >= state.variables.size())
      continue;

    ShaderVariable v;

    v.name = dxbc->GetDXBCByteCode()->GetRegisterName(type, sig.regIndex);
    v.rows = 1;
    v.columns = sig.regChannelMask & 0x8   ? 4
                : sig.regChannelMask & 0x4 ? 3
                : sig.regChannelMask & 0x2 ? 2
                : sig.regChannelMask & 0x1 ? 1
                                           : 0;
    v.type = sig.varType;

    ShaderVariable &dst = state.variables[idx];

    // if the variable hasn't been initialised, just assign. If it has, we're in a situation where
    // two input parameters are assigned to the same variable overlapping, so just update the
    // number of columns to the max of both. The source mapping (either from debug info or our own
    // below) will handle distinguishing better.
    if(dst.name.empty())
      dst = v;
    else
      dst.columns = RDCMAX(dst.columns, v.columns);

    if(type == DXBCBytecode::TYPE_OUTPUT)
    {
      SourceVariableMapping sourcemap;
      sourcemap.name = sig.semanticIdxName;
      if(sourcemap.name.empty() && sig.systemValue != ShaderBuiltin::Undefined)
        sourcemap.name = ToStr(sig.systemValue);
      sourcemap.type = v.type;
      sourcemap.rows = 1;
      sourcemap.columns = sig.compCount;
      sourcemap.signatureIndex = sigIdx;
      sourcemap.variables.reserve(sig.compCount);

      for(uint16_t c = 0; c < 4; c++)
      {
        if(sig.regChannelMask & (1 << c))
        {
          DebugVariableReference ref;
          ref.type = DebugVariableType::Variable;
          ref.name = v.name;
          ref.component = c;
          sourcemap.variables.push_back(ref);
        }
      }

      ret->sourceVars.push_back(sourcemap);
    }
    else
    {
      SourceVariableMapping sourcemap;

      if(sig.systemValue == ShaderBuiltin::DepthOutput)
      {
        sourcemap.name = "SV_Depth";
        sourcemap.type = VarType::Float;
      }
      else if(sig.systemValue == ShaderBuiltin::DepthOutputLessEqual)
      {
        sourcemap.name = "SV_DepthLessEqual";
        sourcemap.type = VarType::Float;
      }
      else if(sig.systemValue == ShaderBuiltin::DepthOutputGreaterEqual)
      {
        sourcemap.name = "SV_DepthGreaterEqual";
        sourcemap.type = VarType::Float;
      }
      else if(sig.systemValue == ShaderBuiltin::MSAACoverage)
      {
        sourcemap.name = "SV_Coverage";
        sourcemap.type = VarType::UInt;
      }
      else if(sig.systemValue == ShaderBuiltin::StencilReference)
      {
        sourcemap.name = "SV_StencilRef";
        sourcemap.type = VarType::UInt;
      }

      // all these variables are 1 scalar component
      sourcemap.rows = 1;
      sourcemap.columns = 1;
      sourcemap.signatureIndex = sigIdx;
      DebugVariableReference ref;
      ref.type = DebugVariableType::Variable;
      ref.name = v.name;
      sourcemap.variables.push_back(ref);

      ret->sourceVars.push_back(sourcemap);
    }
  }

  // Set the number of constant buffers in the trace, but assignment happens later
  size_t numCBuffers = dxbc->GetReflection()->CBuffers.size();
  global.constantBlocks.resize(numCBuffers);
  for(size_t i = 0; i < numCBuffers; ++i)
  {
    uint32_t bindCount = dxbc->GetReflection()->CBuffers[i].bindCount;
    if(bindCount > 1)
    {
      // With a CB array, we use a nesting structure for the trace
      global.constantBlocks[i].members.resize(bindCount);
    }
  }

  struct ResList
  {
    VarType varType;
    DebugVariableType debugVarType;
    DescriptorCategory category;
    const rdcarray<ShaderResource> &resources;
    const char *regChars;
    rdcarray<ShaderVariable> &dst;
  };

  ResList lists[2] = {
      {
          VarType::ReadOnlyResource,
          DebugVariableType::ReadOnlyResource,
          DescriptorCategory::ReadOnlyResource,
          refl.readOnlyResources,
          "tT",
          ret->readOnlyResources,
      },
      {
          VarType::ReadWriteResource,
          DebugVariableType::ReadWriteResource,
          DescriptorCategory::ReadWriteResource,
          refl.readWriteResources,
          "uU",
          ret->readWriteResources,
      },
  };

  for(ResList &list : lists)
  {
    // add the registers for the resources that are used
    list.dst.reserve(list.resources.size());
    for(uint32_t i = 0; i < list.resources.size(); i++)
    {
      const ShaderResource &r = list.resources[i];

      rdcstr identifier;

      if(dxbc->GetDXBCByteCode()->IsShaderModel51())
        identifier = StringFormat::Fmt("%c%zu", list.regChars[1], i);
      else
        identifier = StringFormat::Fmt("%c%u", list.regChars[0], r.fixedBindNumber);

      ShaderVariable reg(identifier, 0U, 0U, 0U, 0U);
      reg.rows = 1;
      reg.columns = 1;

      reg.SetBindIndex(ShaderBindIndex(list.category, i, 0));

      SourceVariableMapping sourcemap;
      sourcemap.name = r.name;
      sourcemap.type = list.varType;
      sourcemap.rows = 1;
      sourcemap.columns = 1;
      sourcemap.offset = 0;
      DebugVariableReference ref;
      ref.type = list.debugVarType;
      ref.name = reg.name;
      sourcemap.variables.push_back(ref);

      ret->sourceVars.push_back(sourcemap);
      list.dst.push_back(reg);
    }
  }

  ret->samplers.reserve(refl.samplers.size());
  for(uint32_t i = 0; i < refl.samplers.size(); i++)
  {
    const ShaderSampler &s = refl.samplers[i];

    rdcstr identifier;

    if(dxbc->GetDXBCByteCode()->IsShaderModel51())
      identifier = StringFormat::Fmt("S%zu", i);
    else
      identifier = StringFormat::Fmt("s%u", s.fixedBindNumber);

    ShaderVariable reg(identifier, 0U, 0U, 0U, 0U);
    reg.rows = 1;
    reg.columns = 1;

    reg.SetBindIndex(ShaderBindIndex(DescriptorCategory::Sampler, i, 0));

    SourceVariableMapping sourcemap;
    sourcemap.name = s.name;
    sourcemap.type = VarType::Sampler;
    sourcemap.rows = 1;
    sourcemap.columns = 1;
    sourcemap.offset = 0;
    DebugVariableReference ref;
    ref.type = DebugVariableType::Sampler;
    ref.name = reg.name;
    sourcemap.variables.push_back(ref);

    ret->sourceVars.push_back(sourcemap);
    ret->samplers.push_back(reg);
  }

  return ret;
}

void InterpretDebugger::CalcActiveMask(rdcarray<bool> &activeMask)
{
  // one bool per workgroup thread
  activeMask.resize(workgroup.size());

  // start as active, then if necessary turn off threads that are running diverged
  for(bool &active : activeMask)
    active = true;

  // only pixel shaders automatically converge workgroups, compute shaders need explicit sync
  if(dxbc->m_Type != DXBC::ShaderType::Pixel)
    return;

  // otherwise we need to make sure that control flow which converges stays in lockstep so that
  // derivatives etc are still valid. While diverged, we don't have to keep threads in lockstep
  // since using derivatives is invalid.
  //
  // Threads diverge either in ifs, loops, or switches. Due to the nature of the bytecode, all
  // threads *must* pass through the same exit instruction for each, there's no jumping around with
  // gotos. Note also for the same reason, the only time threads are on earlier instructions is if
  // they are still catching up to a thread that has exited the control flow.
  //
  // So the scheme is as follows:
  // * If all threads have the same nextInstruction, just continue we are still in lockstep.
  // * If threads are out of lockstep, find any thread which has nextInstruction pointing
  //   immediately *after* an ENDIF, ENDLOOP or ENDSWITCH. Pointing directly at one is not an
  //   indication the thread is done, as the next step for an ENDLOOP will jump back to the matching
  //   LOOP and continue iterating.
  // * Pause any thread matching the above until all threads are pointing to the same instruction.
  //   By the assumption above, all threads will eventually pass through this terminating
  //   instruction so we just pause any other threads and don't do anything until the control flow
  //   has converged and we can continue stepping in lockstep.

  // all threads as active.
  // if we've converged, or we were never diverged, this keeps everything ticking

  // see if we've diverged
  bool differentNext = false;
  for(size_t i = 1; i < workgroup.size(); i++)
    differentNext |= (workgroup[0].nextInstruction != workgroup[i].nextInstruction);

  if(differentNext)
  {
    // this isn't *perfect* but it will still eventually continue. We look for the most advanced
    // thread, and check to see if it's just finished a control flow. If it has then we assume it's
    // at the convergence point and wait for every other thread to catch up, pausing any threads
    // that reach the convergence point before others.

    // Note this might mean we don't have any threads paused even within divergent flow. This is
    // fine and all we care about is pausing to make sure threads don't run ahead into code that
    // should be lockstep. We don't care at all about what they do within the code that is
    // divergent.

    // The reason this isn't perfect is that the most advanced thread could be on an inner loop or
    // inner if, not the convergence point, and we could be pausing it fruitlessly. Worse still - it
    // could be on a branch none of the other threads will take so they will never reach that exact
    // instruction.
    // But we know that all threads will eventually go through the convergence point, so even in
    // that worst case if we didn't pick the right waiting point, another thread will overtake and
    // become the new most advanced thread and the previous waiting thread will resume. So in this
    // case we caused a thread to wait more than it should have but that's not a big deal as it's
    // within divergent flow so they don't have to stay in lockstep. Also if all threads will
    // eventually pass that point we picked, we just waited to converge even in technically
    // divergent code which is also harmless.

    // Phew!

    uint32_t convergencePoint = 0;

    // find which thread is most advanced
    for(size_t i = 0; i < workgroup.size(); i++)
      if(workgroup[i].nextInstruction > convergencePoint)
        convergencePoint = workgroup[i].nextInstruction;

    if(convergencePoint > 0)
    {
      DXBCBytecode::OpcodeType op =
          dxbc->GetDXBCByteCode()->GetInstruction(convergencePoint - 1).operation;

      // if the most advnaced thread hasn't just finished control flow, then all
      // threads are still running, so don't converge
      if(op != OPCODE_ENDIF && op != OPCODE_ENDLOOP && op != OPCODE_ENDSWITCH)
        convergencePoint = 0;
    }

    // pause any threads at that instruction (could be none)
    for(size_t i = 0; i < workgroup.size(); i++)
      if(workgroup[i].nextInstruction == convergencePoint)
        activeMask[i] = false;
  }
}

rdcarray<ShaderDebugState> InterpretDebugger::ContinueDebug(DXBCDebug::DebugAPIWrapper *apiWrapper)
{
  DXBCDebug::ThreadState &active = activeLane();

  rdcarray<ShaderDebugState> ret;

  // if we've finished, return an empty set to signify that
  if(active.Finished())
    return ret;

  // initialise a blank set of shader variable changes in the first ShaderDebugState
  if(steps == 0)
  {
    ShaderDebugState initial;

    active.PrepareInitial(initial);

    ret.push_back(std::move(initial));

    steps++;
  }

  rdcarray<DXBCDebug::ThreadState> oldworkgroup = workgroup;

  rdcarray<bool> activeMask;

  // continue stepping until we have 100 target steps completed in a chunk. This may involve doing
  // more steps if our target thread is inactive
  for(int stepEnd = steps + 100; steps < stepEnd;)
  {
    if(active.Finished())
      break;

    // set up the old workgroup so that cross-workgroup/cross-quad operations (e.g. DDX/DDY) get
    // consistent results even when we step the quad out of order. Otherwise if an operation reads
    // and writes from the same register we'd trash data needed for other workgroup elements.
    for(size_t i = 0; i < oldworkgroup.size(); i++)
      oldworkgroup[i].variables = workgroup[i].variables;

    // calculate the current mask of which threads are active
    CalcActiveMask(activeMask);

    // step all active members of the workgroup
    for(int i = 0; i < workgroup.count(); i++)
    {
      if(activeMask[i])
      {
        if(i == activeLaneIndex)
        {
          ShaderDebugState state;
          workgroup[i].StepNext(&state, apiWrapper, oldworkgroup);
          state.stepIndex = steps;
          state.nextInstruction = workgroup[i].nextInstruction;
          ret.push_back(std::move(state));

          steps++;
        }
        else
        {
          workgroup[i].StepNext(NULL, apiWrapper, oldworkgroup);
        }
      }
    }
  }

  return ret;
}

};    // namespace ShaderDebug

#if ENABLED(ENABLE_UNIT_TESTS)

#include <limits>
#include "catch/catch.hpp"

using namespace DXBCDebug;

TEST_CASE("DXBC debugging helpers", "[program]")
{
  const float posinf = std::numeric_limits<float>::infinity();
  const float neginf = -std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float a = 1.0f;
  const float b = 2.0f;

  SECTION("dxbc_min")
  {
    CHECK(dxbc_min(neginf, neginf) == neginf);
    CHECK(dxbc_min(neginf, a) == neginf);
    CHECK(dxbc_min(neginf, posinf) == neginf);
    CHECK(dxbc_min(neginf, nan) == neginf);
    CHECK(dxbc_min(a, neginf) == neginf);
    CHECK(dxbc_min(a, b) == a);
    CHECK(dxbc_min(a, posinf) == a);
    CHECK(dxbc_min(a, nan) == a);
    CHECK(dxbc_min(posinf, neginf) == neginf);
    CHECK(dxbc_min(posinf, a) == a);
    CHECK(dxbc_min(posinf, posinf) == posinf);
    CHECK(dxbc_min(posinf, nan) == posinf);
    CHECK(dxbc_min(nan, neginf) == neginf);
    CHECK(dxbc_min(nan, a) == a);
    CHECK(dxbc_min(nan, posinf) == posinf);
    CHECK(RDCISNAN(dxbc_min(nan, nan)));
  };

  SECTION("dxbc_max")
  {
    CHECK(dxbc_max(neginf, neginf) == neginf);
    CHECK(dxbc_max(neginf, a) == a);
    CHECK(dxbc_max(neginf, posinf) == posinf);
    CHECK(dxbc_max(neginf, nan) == neginf);
    CHECK(dxbc_max(a, neginf) == a);
    CHECK(dxbc_max(a, b) == b);
    CHECK(dxbc_max(a, posinf) == posinf);
    CHECK(dxbc_max(a, nan) == a);
    CHECK(dxbc_max(posinf, neginf) == posinf);
    CHECK(dxbc_max(posinf, a) == posinf);
    CHECK(dxbc_max(posinf, posinf) == posinf);
    CHECK(dxbc_max(posinf, nan) == posinf);
    CHECK(dxbc_max(nan, neginf) == neginf);
    CHECK(dxbc_max(nan, a) == a);
    CHECK(dxbc_max(nan, posinf) == posinf);
    CHECK(RDCISNAN(dxbc_max(nan, nan)));
  };

  SECTION("sat/abs/neg on NaNs")
  {
    ShaderVariable v("a", b, nan, neginf, posinf);

    ShaderVariable v2 = sat(v, VarType::Float);

    CHECK(v2.value.f32v[0] == 1.0f);
    CHECK(v2.value.f32v[1] == 0.0f);
    CHECK(v2.value.f32v[2] == 0.0f);
    CHECK(v2.value.f32v[3] == 1.0f);

    v2 = neg(v, VarType::Float);

    CHECK(v2.value.f32v[0] == -b);
    CHECK(RDCISNAN(v2.value.f32v[1]));
    CHECK(v2.value.f32v[2] == posinf);
    CHECK(v2.value.f32v[3] == neginf);

    v2 = abs(v, VarType::Float);

    CHECK(v2.value.f32v[0] == b);
    CHECK(RDCISNAN(v2.value.f32v[1]));
    CHECK(v2.value.f32v[2] == posinf);
    CHECK(v2.value.f32v[3] == posinf);
  };

  SECTION("test denorm flushing")
  {
    float foo = 3.141f;

    // check normal values
    CHECK(flush_denorm(0.0f) == 0.0f);
    CHECK(flush_denorm(foo) == foo);
    CHECK(flush_denorm(-foo) == -foo);

    // check NaN/inf values
    CHECK(RDCISNAN(flush_denorm(nan)));
    CHECK(flush_denorm(neginf) == neginf);
    CHECK(flush_denorm(posinf) == posinf);

    // check zero sign bit - bit more complex
    uint32_t negzero = 0x80000000U;
    float negzerof;
    memcpy(&negzerof, &negzero, sizeof(negzero));

    float flushed = flush_denorm(negzerof);
    CHECK(memcmp(&flushed, &negzerof, sizeof(negzerof)) == 0);

    // check that denormal values are flushed, preserving sign
    foo = 1.12104e-44f;
    CHECK(flush_denorm(foo) != foo);
    CHECK(flush_denorm(-foo) != -foo);
    CHECK(flush_denorm(foo) == 0.0f);
    flushed = flush_denorm(-foo);
    CHECK(memcmp(&flushed, &negzerof, sizeof(negzerof)) == 0);
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
