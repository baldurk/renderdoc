/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "driver/dxgi/dxgi_common.h"
#include "maths/formatpacking.h"
#include "replay/replay_driver.h"
#include "dxbc_bytecode.h"
#include "dxbc_container.h"

using namespace DXBCBytecode;

namespace ShaderDebug
{
static float round_ne(float x)
{
  if(!_finite(x) || _isnan(x))
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

VarType State::OperationType(const DXBCBytecode::OpcodeType &op) const
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
    case OPCODE_SYNC:
    case OPCODE_STORE_UAV_TYPED:
    case OPCODE_STORE_RAW:
    case OPCODE_STORE_STRUCTURED:
      return VarType::Float;

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

    default: RDCERR("Unhandled operation %d in shader debugging", op); return VarType::Float;
  }
}

bool State::OperationFlushing(const DXBCBytecode::OpcodeType &op) const
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
    case OPCODE_NE:
      return true;

    // can't generate denorms, or denorm inputs are implicitly rounded to 0, so don't bother
    // flushing
    case OPCODE_ITOF:
    case OPCODE_UTOF:
    case OPCODE_FTOI:
    case OPCODE_FTOU:
      return false;

    // we have to flush this manually since the input is halves encoded in uints
    case OPCODE_F16TOF32:
    case OPCODE_F32TOF16:
      return false;

    // implementation defined if this should flush or not, we choose not.
    case OPCODE_DTOF:
    case OPCODE_FTOD:
      return false;

    // any I/O or data movement operation that does not manipulate the data, such as using the
    // ld(22.4.6) instruction to access Resource data, or executing mov instruction or conditional
    // move/swap instruction (excluding min or max instructions), must not alter data at all (so a
    // denorm remains denorm).
    case OPCODE_MOV:
    case OPCODE_MOVC:
    case OPCODE_LD:
    case OPCODE_LD_MS:
      return false;

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
    case OPCODE_GATHER4_PO_C:
      return true;

    // unclear if these flush and it's unlikely denorms will come up, so conservatively flush
    case OPCODE_SAMPLE_INFO:
    case OPCODE_SAMPLE_POS:
    case OPCODE_EVAL_CENTROID:
    case OPCODE_EVAL_SAMPLE_INDEX:
    case OPCODE_EVAL_SNAPPED:
    case OPCODE_LOD:
    case OPCODE_DERIV_RTX:
    case OPCODE_DERIV_RTX_COARSE:
    case OPCODE_DERIV_RTX_FINE:
    case OPCODE_DERIV_RTY:
    case OPCODE_DERIV_RTY_COARSE:
    case OPCODE_DERIV_RTY_FINE:
      return true;

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
    case OPCODE_SYNC:
    case OPCODE_STORE_UAV_TYPED:
    case OPCODE_STORE_RAW:
    case OPCODE_STORE_STRUCTURED:
      return false;

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
    case OPCODE_DTOU:
      return false;

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

    default: RDCERR("Unhandled operation %d in shader debugging", op); break;
  }

  return false;
}

void DoubleSet(ShaderVariable &var, const double in[2])
{
  var.value.d.x = in[0];
  var.value.d.y = in[1];
  var.type = VarType::Double;
}

void DoubleGet(const ShaderVariable &var, double out[2])
{
  out[0] = var.value.d.x;
  out[1] = var.value.d.y;
}

void TypedUAVStore(GlobalState::ViewFmt &fmt, byte *d, ShaderVariable var)
{
  if(fmt.byteWidth == 10)
  {
    uint32_t u = 0;

    if(fmt.fmt == CompType::UInt)
    {
      u |= (var.value.u.x & 0x3ff) << 0;
      u |= (var.value.u.y & 0x3ff) << 10;
      u |= (var.value.u.z & 0x3ff) << 20;
      u |= (var.value.u.w & 0x3) << 30;
    }
    else if(fmt.fmt == CompType::UNorm)
    {
      u = ConvertToR10G10B10A2(Vec4f(var.value.f.x, var.value.f.y, var.value.f.z, var.value.f.w));
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
    memcpy(d, &u, sizeof(uint32_t));
  }
  else if(fmt.byteWidth == 11)
  {
    uint32_t u = ConvertToR11G11B10(Vec3f(var.value.f.x, var.value.f.y, var.value.f.z));
    memcpy(d, &u, sizeof(uint32_t));
  }
  else if(fmt.byteWidth == 4)
  {
    uint32_t *u = (uint32_t *)d;

    for(int c = 0; c < fmt.numComps; c++)
      u[c] = var.value.uv[c];
  }
  else if(fmt.byteWidth == 2)
  {
    if(fmt.fmt == CompType::Float)
    {
      uint16_t *u = (uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = ConvertToHalf(var.value.fv[c]);
    }
    else if(fmt.fmt == CompType::UInt)
    {
      uint16_t *u = (uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = var.value.uv[c] & 0xffff;
    }
    else if(fmt.fmt == CompType::SInt)
    {
      int16_t *i = (int16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        i[c] = (int16_t)RDCCLAMP(var.value.iv[c], (int32_t)INT16_MIN, (int32_t)INT16_MAX);
    }
    else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
    {
      uint16_t *u = (uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(var.value.fv[c], 0.0f, 1.0f) * float(0xffff) + 0.5f;
        u[c] = uint16_t(f);
      }
    }
    else if(fmt.fmt == CompType::SNorm)
    {
      int16_t *i = (int16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(var.value.fv[c], -1.0f, 1.0f) * 0x7fff;

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
        u[c] = var.value.uv[c] & 0xff;
    }
    else if(fmt.fmt == CompType::SInt)
    {
      int8_t *i = (int8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        i[c] = (int8_t)RDCCLAMP(var.value.iv[c], (int32_t)INT8_MIN, (int32_t)INT8_MAX);
    }
    else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
    {
      uint8_t *u = (uint8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(var.value.fv[c], 0.0f, 1.0f) * float(0xff) + 0.5f;
        u[c] = uint8_t(f);
      }
    }
    else if(fmt.fmt == CompType::SNorm)
    {
      int8_t *i = (int8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(var.value.fv[c], -1.0f, 1.0f) * 0x7f;

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
      result.value.u.x = (u >> 0) & 0x3ff;
      result.value.u.y = (u >> 10) & 0x3ff;
      result.value.u.z = (u >> 20) & 0x3ff;
      result.value.u.w = (u >> 30) & 0x003;
    }
    else if(fmt.fmt == CompType::UNorm)
    {
      Vec4f res = ConvertFromR10G10B10A2(u);
      result.value.f.x = res.x;
      result.value.f.y = res.y;
      result.value.f.z = res.z;
      result.value.f.w = res.w;
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
    result.value.f.x = res.x;
    result.value.f.y = res.y;
    result.value.f.z = res.z;
    result.value.f.w = 1.0f;
  }
  else if(fmt.byteWidth == 4)
  {
    const uint32_t *u = (const uint32_t *)d;

    for(int c = 0; c < fmt.numComps; c++)
      result.value.uv[c] = u[c];
  }
  else if(fmt.byteWidth == 2)
  {
    if(fmt.fmt == CompType::Float)
    {
      const uint16_t *u = (const uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        result.value.fv[c] = ConvertFromHalf(u[c]);
    }
    else if(fmt.fmt == CompType::UInt)
    {
      const uint16_t *u = (const uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        result.value.uv[c] = u[c];
    }
    else if(fmt.fmt == CompType::SInt)
    {
      const int16_t *in = (const int16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        result.value.iv[c] = in[c];
    }
    else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
    {
      const uint16_t *u = (const uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        result.value.fv[c] = float(u[c]) / float(0xffff);
    }
    else if(fmt.fmt == CompType::SNorm)
    {
      const int16_t *in = (const int16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
        if(in[c] == -32768)
          result.value.fv[c] = -1.0f;
        else
          result.value.fv[c] = float(in[c]) / 32767.0f;
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
        result.value.uv[c] = u[c];
    }
    else if(fmt.fmt == CompType::SInt)
    {
      const int8_t *in = (const int8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        result.value.iv[c] = in[c];
    }
    else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
    {
      const uint8_t *u = (const uint8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        result.value.fv[c] = float(u[c]) / float(0xff);
    }
    else if(fmt.fmt == CompType::SNorm)
    {
      const int8_t *in = (const int8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
        if(in[c] == -128)
          result.value.fv[c] = -1.0f;
        else
          result.value.fv[c] = float(in[c]) / 127.0f;
      }
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }

  return result;
}

// "NaN has special handling. If one source operand is NaN, then the other source operand is
// returned and the choice is made per-component. If both are NaN, any NaN representation is
// returned."

float dxbc_min(float a, float b)
{
  if(_isnan(a))
    return b;

  if(_isnan(b))
    return a;

  return a < b ? a : b;
}

double dxbc_min(double a, double b)
{
  if(_isnan(a))
    return b;

  if(_isnan(b))
    return a;

  return a < b ? a : b;
}

float dxbc_max(float a, float b)
{
  if(_isnan(a))
    return b;

  if(_isnan(b))
    return a;

  return a >= b ? a : b;
}

double dxbc_max(double a, double b)
{
  if(_isnan(a))
    return b;

  if(_isnan(b))
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
        r.value.iv[i] = v.value.iv[i] < 0 ? 0 : (v.value.iv[i] > 1 ? 1 : v.value.iv[i]);
      break;
    }
    case VarType::UInt:
    {
      for(size_t i = 0; i < v.columns; i++)
        r.value.uv[i] = v.value.uv[i] ? 1 : 0;
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
        r.value.fv[i] = dxbc_min(1.0f, dxbc_max(0.0f, v.value.fv[i]));
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
        r.value.iv[i] = v.value.iv[i] > 0 ? v.value.iv[i] : -v.value.iv[i];
      break;
    }
    case VarType::UInt: { break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < v.columns; i++)
        r.value.fv[i] = v.value.fv[i] > 0 ? v.value.fv[i] : -v.value.fv[i];
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
        r.value.iv[i] = -v.value.iv[i];
      break;
    }
    case VarType::UInt: { break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < v.columns; i++)
        r.value.fv[i] = -v.value.fv[i];
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
        r.value.iv[i] = a.value.iv[i] * b.value.iv[i];
      break;
    }
    case VarType::UInt:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.uv[i] = a.value.uv[i] * b.value.uv[i];
      break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.fv[i] = a.value.fv[i] * b.value.fv[i];
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
        r.value.iv[i] = a.value.iv[i] / b.value.iv[i];
      break;
    }
    case VarType::UInt:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.uv[i] = a.value.uv[i] / b.value.uv[i];
      break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.fv[i] = a.value.fv[i] / b.value.fv[i];
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
        r.value.iv[i] = a.value.iv[i] + b.value.iv[i];
      break;
    }
    case VarType::UInt:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.uv[i] = a.value.uv[i] + b.value.uv[i];
      break;
    }
    case VarType::Float:
    {
      for(size_t i = 0; i < a.columns; i++)
        r.value.fv[i] = a.value.fv[i] + b.value.fv[i];
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

void State::Init()
{
  std::vector<uint32_t> indexTempSizes;

  for(size_t i = 0; i < program->GetNumDeclarations(); i++)
  {
    const DXBCBytecode::Declaration &decl = program->GetDeclaration(i);

    if(decl.declaration == OPCODE_DCL_TEMPS)
    {
      registers.reserve(decl.numTemps);

      for(uint32_t t = 0; t < decl.numTemps; t++)
      {
        char buf[64] = {0};

        StringFormat::snprintf(buf, 63, "r%d", t);

        registers.push_back(ShaderVariable(buf, 0l, 0l, 0l, 0l));
      }
    }
    if(decl.declaration == OPCODE_DCL_INDEXABLE_TEMP)
    {
      uint32_t reg = decl.tempReg;
      uint32_t size = decl.numTemps;
      if(reg >= indexTempSizes.size())
        indexTempSizes.resize(reg + 1);

      indexTempSizes[reg] = size;
    }
  }

  if(indexTempSizes.size())
  {
    indexableTemps.resize(indexTempSizes.size());

    for(int32_t i = 0; i < (int32_t)indexTempSizes.size(); i++)
    {
      if(indexTempSizes[i] > 0)
      {
        indexableTemps[i].members.resize(indexTempSizes[i]);
        for(uint32_t t = 0; t < indexTempSizes[i]; t++)
        {
          char buf[64] = {0};

          StringFormat::snprintf(buf, 63, "x%u[%u]", i, t);

          indexableTemps[i].members[t] = ShaderVariable(buf, 0l, 0l, 0l, 0l);
        }
      }
    }
  }
}

bool State::Finished() const
{
  return program && (done || nextInstruction >= (int)program->GetNumInstructions());
}

bool State::AssignValue(ShaderVariable &dst, uint32_t dstIndex, const ShaderVariable &src,
                        uint32_t srcIndex, bool flushDenorm)
{
  if(src.type == VarType::Float)
  {
    float ft = src.value.fv[srcIndex];
    if(!_finite(ft) || _isnan(ft))
      flags |= ShaderEvents::GeneratedNanOrInf;
  }
  else if(src.type == VarType::Double)
  {
    double dt = src.value.dv[srcIndex];
    if(!_finite(dt) || _isnan(dt))
      flags |= ShaderEvents::GeneratedNanOrInf;
  }

  bool ret = (dst.value.uv[dstIndex] != src.value.uv[srcIndex]);

  dst.value.uv[dstIndex] = src.value.uv[srcIndex];

  if(flushDenorm && src.type == VarType::Float)
    dst.value.fv[dstIndex] = flush_denorm(dst.value.fv[dstIndex]);

  return ret;
}

void State::SetDst(const Operand &dstoper, const Operation &op, const ShaderVariable &val)
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

      indices[i] += idx.value.i.x;
    }
  }

  RegisterRange range;
  range.index = uint16_t(indices[0]);

  switch(dstoper.type)
  {
    case TYPE_TEMP:
    {
      range.type = RegisterType::Temporary;
      RDCASSERT(indices[0] < (uint32_t)registers.size());
      if(indices[0] < (uint32_t)registers.size())
        v = &registers[(size_t)indices[0]];
      break;
    }
    case TYPE_INDEXABLE_TEMP:
    {
      range.type = RegisterType::IndexedTemporary;
      RDCASSERT(dstoper.indices.size() == 2);

      if(dstoper.indices.size() == 2)
      {
        RDCASSERT(indices[0] < (uint32_t)indexableTemps.size());
        if(indices[0] < (uint32_t)indexableTemps.size())
        {
          RDCASSERT(indices[1] < (uint32_t)indexableTemps[indices[0]].members.size());
          if(indices[1] < (uint32_t)indexableTemps[indices[0]].members.size())
          {
            v = &indexableTemps[indices[0]].members[indices[1]];
          }
        }
      }
      break;
    }
    case TYPE_OUTPUT:
    {
      range.type = RegisterType::Output;
      RDCASSERT(indices[0] < (uint32_t)outputs.size());
      if(indices[0] < (uint32_t)outputs.size())
        v = &outputs[(size_t)indices[0]];
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
      // nothing to do!
      return;
    }
    case TYPE_OUTPUT_DEPTH:
    case TYPE_OUTPUT_DEPTH_LESS_EQUAL:
    case TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
    case TYPE_OUTPUT_STENCIL_REF:
    case TYPE_OUTPUT_COVERAGE_MASK:
    {
      // handle all semantic outputs together
      ShaderBuiltin builtin = ShaderBuiltin::Count;
      switch(dstoper.type)
      {
        case TYPE_OUTPUT_DEPTH: builtin = ShaderBuiltin::DepthOutput; break;
        case TYPE_OUTPUT_DEPTH_LESS_EQUAL: builtin = ShaderBuiltin::DepthOutputLessEqual; break;
        case TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
          builtin = ShaderBuiltin::DepthOutputGreaterEqual;
          break;
        case TYPE_OUTPUT_STENCIL_REF: builtin = ShaderBuiltin::StencilReference; break;
        case TYPE_OUTPUT_COVERAGE_MASK: builtin = ShaderBuiltin::MSAACoverage; break;
        default: RDCERR("Invalid dest operand!"); break;
      }

      for(size_t i = 0; i < reflection->OutputSig.size(); i++)
      {
        if(reflection->OutputSig[i].systemValue == builtin)
        {
          v = &outputs[i];
          break;
        }
      }

      if(!v)
      {
        RDCERR("Couldn't find type %d by semantic matching, falling back to string match",
               dstoper.type);

        std::string name = dstoper.toString(reflection, ToString::ShowSwizzle);
        for(size_t i = 0; i < outputs.size(); i++)
        {
          if(outputs[i].name == name)
          {
            v = &outputs[i];
            break;
          }
        }

        if(v)
          break;
      }

      break;
    }
    default:
    {
      RDCERR("Currently unsupported destination operand type %d!", dstoper.type);

      std::string name = dstoper.toString(reflection, ToString::ShowSwizzle);
      for(size_t i = 0; i < outputs.size(); i++)
      {
        if(outputs[i].name == name)
        {
          v = &outputs[i];
          break;
        }
      }

      if(v)
        break;

      break;
    }
  }

  RDCASSERT(v);

  if(v)
  {
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

    if(op.saturate)
      right = sat(right, OperationType(op.operation));

    if(dstoper.comps[0] != 0xff && dstoper.comps[1] == 0xff && dstoper.comps[2] == 0xff &&
       dstoper.comps[3] == 0xff)
    {
      RDCASSERT(dstoper.comps[0] != 0xff);

      bool changed = AssignValue(*v, dstoper.comps[0], right, 0, flushDenorm);

      if(changed && range.type != RegisterType::Undefined)
      {
        range.component = dstoper.comps[0];
        modified.push_back(range);
      }
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
          bool changed = AssignValue(*v, dstoper.comps[i], right, dstoper.comps[i], flushDenorm);
          compsWritten++;

          if(changed && range.type != RegisterType::Undefined)
          {
            range.component = dstoper.comps[i];
            modified.push_back(range);
          }
        }
      }

      if(compsWritten == 0)
      {
        bool changed = AssignValue(*v, 0, right, 0, flushDenorm);

        if(changed && range.type != RegisterType::Undefined)
        {
          range.component = 0;
          modified.push_back(range);
        }
      }
    }
  }
}

ShaderVariable State::DDX(bool fine, State quad[4], const DXBCBytecode::Operand &oper,
                          const DXBCBytecode::Operation &op) const
{
  ShaderVariable ret;

  VarType optype = OperationType(op.operation);

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

ShaderVariable State::DDY(bool fine, State quad[4], const DXBCBytecode::Operand &oper,
                          const DXBCBytecode::Operation &op) const
{
  ShaderVariable ret;

  VarType optype = OperationType(op.operation);

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

ShaderVariable State::GetSrc(const Operand &oper, const Operation &op, bool allowFlushing) const
{
  ShaderVariable v, s;

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

      indices[i] += idx.value.i.x;
    }
  }

  // is this type a flushable input (for float operations)
  bool flushable = allowFlushing;

  switch(oper.type)
  {
    case TYPE_TEMP:
    {
      // we assume we never write to an uninitialised register
      RDCASSERT(indices[0] < (uint32_t)registers.size());

      if(indices[0] < (uint32_t)registers.size())
        v = s = registers[indices[0]];
      else
        v = s = ShaderVariable("", indices[0], indices[0], indices[0], indices[0]);

      break;
    }
    case TYPE_INDEXABLE_TEMP:
    {
      RDCASSERT(oper.indices.size() == 2);

      if(oper.indices.size() == 2)
      {
        RDCASSERT(indices[0] < (uint32_t)indexableTemps.size());
        if(indices[0] < (uint32_t)indexableTemps.size())
        {
          RDCASSERT(indices[1] < (uint32_t)indexableTemps[indices[0]].members.size());
          if(indices[1] < (uint32_t)indexableTemps[indices[0]].members.size())
          {
            v = s = indexableTemps[indices[0]].members[indices[1]];
          }
        }
      }
      break;
    }
    case TYPE_INPUT:
    {
      RDCASSERT(indices[0] < (uint32_t)trace->inputs.size());

      if(indices[0] < (uint32_t)trace->inputs.size())
        v = s = trace->inputs[indices[0]];
      else
        v = s = ShaderVariable("", indices[0], indices[0], indices[0], indices[0]);

      break;
    }
    case TYPE_OUTPUT:
    {
      RDCASSERT(indices[0] < (uint32_t)outputs.size());

      if(indices[0] < (uint32_t)outputs.size())
        v = s = outputs[indices[0]];
      else
        v = s = ShaderVariable("", indices[0], indices[0], indices[0], indices[0]);

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
      // argument but let's be sane and include the index
      v = s = ShaderVariable("", indices[0], indices[0], indices[0], indices[0]);
      flushable = false;
      break;
    }
    case TYPE_IMMEDIATE32:
    case TYPE_IMMEDIATE64:
    {
      s.name = "Immediate";

      flushable = false;

      if(oper.numComponents == NUMCOMPS_1)
      {
        s.rows = 1;
        s.columns = 1;
      }
      else if(oper.numComponents == NUMCOMPS_4)
      {
        s.rows = 1;
        s.columns = 4;
      }
      else
      {
        RDCFATAL("N-wide vectors not supported (per hlsl spec)");
      }

      if(oper.type == TYPE_IMMEDIATE32)
      {
        for(size_t i = 0; i < s.columns; i++)
        {
          s.value.iv[i] = (int32_t)oper.values[i];
        }
      }
      else
      {
        RDCUNIMPLEMENTED(
            "Encountered immediate 64bit value!");    // need to figure out what to do here.
      }

      v = s;

      break;
    }
    case TYPE_CONSTANT_BUFFER:
    {
      int cb = -1;

      for(size_t i = 0; i < reflection->CBuffers.size(); i++)
      {
        if(reflection->CBuffers[i].reg == indices[0])
        {
          cb = (int)i;
          break;
        }
      }

      RDCASSERTMSG("Invalid cbuffer lookup", cb != -1 && cb < trace->constantBlocks.count(), cb,
                   trace->constantBlocks.count());

      if(cb >= 0 && cb < trace->constantBlocks.count())
      {
        RDCASSERTMSG("Out of bounds cbuffer lookup",
                     indices[1] < (uint32_t)trace->constantBlocks[cb].members.count(), indices[1],
                     trace->constantBlocks[cb].members.count());

        if(indices[1] < (uint32_t)trace->constantBlocks[cb].members.count())
          v = s = trace->constantBlocks[cb].members[indices[1]];
        else
          v = s = ShaderVariable("", 0U, 0U, 0U, 0U);
      }
      else
      {
        v = s = ShaderVariable("", 0U, 0U, 0U, 0U);
      }

      break;
    }
    case TYPE_IMMEDIATE_CONSTANT_BUFFER:
    {
      v = s = ShaderVariable("", 0, 0, 0, 0);

      const std::vector<uint32_t> &icb = program->GetImmediateConstantBuffer();

      // if this Vec4f is entirely in the ICB
      if(indices[0] <= icb.size() / 4 - 1)
      {
        memcpy(s.value.uv, &icb[indices[0] * 4], sizeof(Vec4f));
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
      v = s = ShaderVariable("vThreadGroupID", semantics.GroupID[0], semantics.GroupID[1],
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

      v = s =
          ShaderVariable("vThreadID", semantics.GroupID[0] * numthreads[0] + semantics.ThreadID[0],
                         semantics.GroupID[1] * numthreads[1] + semantics.ThreadID[1],
                         semantics.GroupID[2] * numthreads[2] + semantics.ThreadID[2], (uint32_t)0);

      break;
    }
    case TYPE_INPUT_THREAD_ID_IN_GROUP:
    {
      v = s = ShaderVariable("vThreadIDInGroup", semantics.ThreadID[0], semantics.ThreadID[1],
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

      v = s = ShaderVariable("vThreadIDInGroupFlattened", flattened, flattened, flattened, flattened);
      break;
    }
    case TYPE_INPUT_COVERAGE_MASK:
    {
      v = s = ShaderVariable("vCoverage", semantics.coverage, semantics.coverage,
                             semantics.coverage, semantics.coverage);
      break;
    }
    case TYPE_INPUT_PRIMITIVEID:
    {
      v = s = ShaderVariable("vPrimitiveID", semantics.primID, semantics.primID, semantics.primID,
                             semantics.primID);
      break;
    }
    default:
    {
      RDCERR("Currently unsupported operand type %d!", oper.type);

      v = s = ShaderVariable("vUnsupported", (uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0);

      break;
    }
  }

  // perform swizzling
  v.value.uv[0] = s.value.uv[oper.comps[0] == 0xff ? 0 : oper.comps[0]];
  v.value.uv[1] = s.value.uv[oper.comps[1] == 0xff ? 1 : oper.comps[1]];
  v.value.uv[2] = s.value.uv[oper.comps[2] == 0xff ? 2 : oper.comps[2]];
  v.value.uv[3] = s.value.uv[oper.comps[3] == 0xff ? 3 : oper.comps[3]];

  if(oper.comps[0] != 0xff && oper.comps[1] == 0xff && oper.comps[2] == 0xff && oper.comps[3] == 0xff)
    v.columns = 1;
  else
    v.columns = 4;

  if(oper.modifier == OPERAND_MODIFIER_ABS || oper.modifier == OPERAND_MODIFIER_ABSNEG)
  {
    v = abs(v, OperationType(op.operation));
  }

  if(oper.modifier == OPERAND_MODIFIER_NEG || oper.modifier == OPERAND_MODIFIER_ABSNEG)
  {
    v = neg(v, OperationType(op.operation));
  }

  if(OperationFlushing(op.operation) && flushable)
  {
    for(int i = 0; i < 4; i++)
      v.value.fv[i] = flush_denorm(v.value.fv[i]);
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

State State::GetNext(GlobalState &global, DebugAPIWrapper *apiWrapper, State quad[4]) const
{
  State s = *this;

  s.modified.clear();

  if(s.nextInstruction >= s.program->GetNumInstructions())
    return s;

  const Operation &op = s.program->GetInstruction((size_t)s.nextInstruction);

  apiWrapper->SetCurrentInstruction(s.nextInstruction);
  s.nextInstruction++;
  s.flags = ShaderEvents::NoEvent;

  std::vector<ShaderVariable> srcOpers;

  VarType optype = OperationType(op.operation);

  for(size_t i = 1; i < op.operands.size(); i++)
    srcOpers.push_back(GetSrc(op.operands[i], op));

  switch(op.operation)
  {
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Math operations

    case OPCODE_DADD:
    case OPCODE_IADD:
    case OPCODE_ADD: s.SetDst(op.operands[0], op, add(srcOpers[0], srcOpers[1], optype)); break;
    case OPCODE_DDIV:
    case OPCODE_DIV: s.SetDst(op.operands[0], op, div(srcOpers[0], srcOpers[1], optype)); break;
    case OPCODE_UDIV:
    {
      ShaderVariable quot("", (uint32_t)0xffffffff, (uint32_t)0xffffffff, (uint32_t)0xffffffff,
                          (uint32_t)0xffffffff);
      ShaderVariable rem("", (uint32_t)0xffffffff, (uint32_t)0xffffffff, (uint32_t)0xffffffff,
                         (uint32_t)0xffffffff);

      for(size_t i = 0; i < 4; i++)
      {
        if(srcOpers[2].value.uv[i] != 0)
        {
          quot.value.uv[i] = srcOpers[1].value.uv[i] / srcOpers[2].value.uv[i];
          rem.value.uv[i] = srcOpers[1].value.uv[i] - (quot.value.uv[i] * srcOpers[2].value.uv[i]);
        }
      }

      if(op.operands[0].type != TYPE_NULL)
      {
        s.SetDst(op.operands[0], op, quot);
      }
      if(op.operands[1].type != TYPE_NULL)
      {
        s.SetDst(op.operands[1], op, rem);
      }
      break;
    }
    case OPCODE_BFREV:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        ret.value.uv[i] = BitwiseReverseLSB16(srcOpers[0].value.uv[i]);
      }

      s.SetDst(op.operands[0], op, ret);

      break;
    }
    case OPCODE_COUNTBITS:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        ret.value.uv[i] = PopCount(srcOpers[0].value.uv[i]);
      }

      s.SetDst(op.operands[0], op, ret);
      break;
    }
    case OPCODE_FIRSTBIT_HI:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        unsigned char found = BitScanReverse((DWORD *)&ret.value.uv[i], srcOpers[0].value.uv[i]);
        if(found == 0)
        {
          ret.value.uv[i] = ~0U;
        }
        else
        {
          // firstbit_hi counts index 0 as the MSB, BitScanReverse counts index 0 as the LSB. So we
          // need to invert
          ret.value.uv[i] = 31 - ret.value.uv[i];
        }
      }

      s.SetDst(op.operands[0], op, ret);
      break;
    }
    case OPCODE_FIRSTBIT_LO:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        unsigned char found = BitScanForward((DWORD *)&ret.value.uv[i], srcOpers[0].value.uv[i]);
        if(found == 0)
          ret.value.uv[i] = ~0U;
      }

      s.SetDst(op.operands[0], op, ret);
      break;
    }
    case OPCODE_FIRSTBIT_SHI:
    {
      ShaderVariable ret("", 0U, 0U, 0U, 0U);

      for(size_t i = 0; i < 4; i++)
      {
        uint32_t u = srcOpers[0].value.uv[i];
        if(srcOpers[0].value.iv[i] < 0)
          u = ~u;

        unsigned char found = BitScanReverse((DWORD *)&ret.value.uv[i], u);

        if(found == 0)
        {
          ret.value.uv[i] = ~0U;
        }
        else
        {
          // firstbit_shi counts index 0 as the MSB, BitScanReverse counts index 0 as the LSB. So we
          // need to invert
          ret.value.uv[i] = 31 - ret.value.uv[i];
        }
      }

      s.SetDst(op.operands[0], op, ret);
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
          uint64_t res = uint64_t(srcOpers[1].value.uv[i]) * uint64_t(srcOpers[2].value.uv[i]);

          hi.value.uv[i] = uint32_t((res >> 32) & 0xffffffff);
          lo.value.uv[i] = uint32_t(res & 0xffffffff);
        }
        else if(op.operation == OPCODE_IMUL)
        {
          int64_t res = int64_t(srcOpers[1].value.iv[i]) * int64_t(srcOpers[2].value.iv[i]);

          hi.value.uv[i] = uint32_t((res >> 32) & 0xffffffff);
          lo.value.uv[i] = uint32_t(res & 0xffffffff);
        }
      }

      if(op.operands[0].type != TYPE_NULL)
      {
        s.SetDst(op.operands[0], op, hi);
      }
      if(op.operands[1].type != TYPE_NULL)
      {
        s.SetDst(op.operands[1], op, lo);
      }
      break;
    }
    case OPCODE_DMUL:
    case OPCODE_MUL: s.SetDst(op.operands[0], op, mul(srcOpers[0], srcOpers[1], optype)); break;
    case OPCODE_UADDC:
    {
      uint64_t src[4];
      for(int i = 0; i < 4; i++)
        src[i] = (uint64_t)srcOpers[1].value.uv[i];
      for(int i = 0; i < 4; i++)
        src[i] = (uint64_t)srcOpers[2].value.uv[i];

      // set the rounded result
      uint32_t dst[4];

      for(int i = 0; i < 4; i++)
        dst[i] = (uint32_t)(src[i] & 0xffffffff);

      s.SetDst(op.operands[0], op, ShaderVariable("", dst[0], dst[1], dst[2], dst[3]));

      // if not null, set the carry bits
      if(op.operands[1].type != TYPE_NULL)
        s.SetDst(op.operands[1], op,
                 ShaderVariable("", src[0] > 0xffffffff ? 1U : 0U, src[1] > 0xffffffff ? 1U : 0U,
                                src[2] > 0xffffffff ? 1U : 0U, src[3] > 0xffffffff ? 1U : 0U));

      break;
    }
    case OPCODE_USUBB:
    {
      uint64_t src0[4];
      uint64_t src1[4];

      // add on a 'borrow' bit
      for(int i = 0; i < 4; i++)
        src0[i] = 0x100000000 | (uint64_t)srcOpers[1].value.uv[i];
      for(int i = 0; i < 4; i++)
        src1[i] = (uint64_t)srcOpers[2].value.uv[i];

      // do the subtract
      uint64_t result[4];
      for(int i = 0; i < 4; i++)
        result[i] = src0[i] - src1[i];

      uint32_t dst[4];
      for(int i = 0; i < 4; i++)
        dst[i] = (uint32_t)(result[0] & 0xffffffff);

      s.SetDst(op.operands[0], op, ShaderVariable("", dst[0], dst[1], dst[2], dst[3]));

      // if not null, mark where the borrow bits were used
      if(op.operands[1].type != TYPE_NULL)
        s.SetDst(
            op.operands[1], op,
            ShaderVariable("", result[0] <= 0xffffffff ? 1U : 0U, result[1] <= 0xffffffff ? 1U : 0U,
                           result[2] <= 0xffffffff ? 1U : 0U, result[3] <= 0xffffffff ? 1U : 0U));

      break;
    }
    case OPCODE_IMAD:
    case OPCODE_UMAD:
    case OPCODE_MAD:
    case OPCODE_DFMA:
      s.SetDst(op.operands[0], op, add(mul(srcOpers[0], srcOpers[1], optype), srcOpers[2], optype));
      break;
    case OPCODE_DP2:
    case OPCODE_DP3:
    case OPCODE_DP4:
    {
      ShaderVariable dot = mul(srcOpers[0], srcOpers[1], optype);

      float sum = dot.value.f.x;
      if(op.operation >= OPCODE_DP2)
        sum += dot.value.f.y;
      if(op.operation >= OPCODE_DP3)
        sum += dot.value.f.z;
      if(op.operation >= OPCODE_DP4)
        sum += dot.value.f.w;

      s.SetDst(op.operands[0], op, ShaderVariable("", sum, sum, sum, sum));
      break;
    }
    case OPCODE_F16TOF32:
    {
      s.SetDst(op.operands[0], op,
               ShaderVariable("", flush_denorm(ConvertFromHalf(srcOpers[0].value.u.x & 0xffff)),
                              flush_denorm(ConvertFromHalf(srcOpers[0].value.u.y & 0xffff)),
                              flush_denorm(ConvertFromHalf(srcOpers[0].value.u.z & 0xffff)),
                              flush_denorm(ConvertFromHalf(srcOpers[0].value.u.w & 0xffff))));
      break;
    }
    case OPCODE_F32TOF16:
    {
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (uint32_t)ConvertToHalf(flush_denorm(srcOpers[0].value.f.x)),
                              (uint32_t)ConvertToHalf(flush_denorm(srcOpers[0].value.f.y)),
                              (uint32_t)ConvertToHalf(flush_denorm(srcOpers[0].value.f.z)),
                              (uint32_t)ConvertToHalf(flush_denorm(srcOpers[0].value.f.w))));
      break;
    }
    case OPCODE_FRC:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", srcOpers[0].value.f.x - floor(srcOpers[0].value.f.x),
                              srcOpers[0].value.f.y - floor(srcOpers[0].value.f.y),
                              srcOpers[0].value.f.z - floor(srcOpers[0].value.f.z),
                              srcOpers[0].value.f.w - floor(srcOpers[0].value.f.w)));
      break;
    // positive infinity
    case OPCODE_ROUND_PI:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", ceil(srcOpers[0].value.f.x), ceil(srcOpers[0].value.f.y),
                              ceil(srcOpers[0].value.f.z), ceil(srcOpers[0].value.f.w)));
      break;
    // negative infinity
    case OPCODE_ROUND_NI:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", floor(srcOpers[0].value.f.x), floor(srcOpers[0].value.f.y),
                              floor(srcOpers[0].value.f.z), floor(srcOpers[0].value.f.w)));
      break;
    // towards zero
    case OPCODE_ROUND_Z:
      s.SetDst(
          op.operands[0], op,
          ShaderVariable(
              "",
              srcOpers[0].value.f.x < 0 ? ceil(srcOpers[0].value.f.x) : floor(srcOpers[0].value.f.x),
              srcOpers[0].value.f.y < 0 ? ceil(srcOpers[0].value.f.y) : floor(srcOpers[0].value.f.y),
              srcOpers[0].value.f.z < 0 ? ceil(srcOpers[0].value.f.z) : floor(srcOpers[0].value.f.z),
              srcOpers[0].value.f.w < 0 ? ceil(srcOpers[0].value.f.w) : floor(srcOpers[0].value.f.w)));
      break;
    // to nearest even int (banker's rounding)
    case OPCODE_ROUND_NE:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", round_ne(srcOpers[0].value.f.x), round_ne(srcOpers[0].value.f.y),
                              round_ne(srcOpers[0].value.f.z), round_ne(srcOpers[0].value.f.w)));
      break;
    case OPCODE_INEG: s.SetDst(op.operands[0], op, neg(srcOpers[0], optype)); break;
    case OPCODE_IMIN:
      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[0].value.i.x < srcOpers[1].value.i.x ? srcOpers[0].value.i.x
                                                                           : srcOpers[1].value.i.x,
                         srcOpers[0].value.i.y < srcOpers[1].value.i.y ? srcOpers[0].value.i.y
                                                                       : srcOpers[1].value.i.y,
                         srcOpers[0].value.i.z < srcOpers[1].value.i.z ? srcOpers[0].value.i.z
                                                                       : srcOpers[1].value.i.z,
                         srcOpers[0].value.i.w < srcOpers[1].value.i.w ? srcOpers[0].value.i.w
                                                                       : srcOpers[1].value.i.w));
      break;
    case OPCODE_UMIN:
      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[0].value.u.x < srcOpers[1].value.u.x ? srcOpers[0].value.u.x
                                                                           : srcOpers[1].value.u.x,
                         srcOpers[0].value.u.y < srcOpers[1].value.u.y ? srcOpers[0].value.u.y
                                                                       : srcOpers[1].value.u.y,
                         srcOpers[0].value.u.z < srcOpers[1].value.u.z ? srcOpers[0].value.u.z
                                                                       : srcOpers[1].value.u.z,
                         srcOpers[0].value.u.w < srcOpers[1].value.u.w ? srcOpers[0].value.u.w
                                                                       : srcOpers[1].value.u.w));
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

      s.SetDst(op.operands[0], op, r);
      break;
    }
    case OPCODE_MIN:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", dxbc_min(srcOpers[0].value.f.x, srcOpers[1].value.f.x),
                              dxbc_min(srcOpers[0].value.f.y, srcOpers[1].value.f.y),
                              dxbc_min(srcOpers[0].value.f.z, srcOpers[1].value.f.z),
                              dxbc_min(srcOpers[0].value.f.w, srcOpers[1].value.f.w)));
      break;
    case OPCODE_UMAX:
      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[0].value.u.x >= srcOpers[1].value.u.x ? srcOpers[0].value.u.x
                                                                            : srcOpers[1].value.u.x,
                         srcOpers[0].value.u.y >= srcOpers[1].value.u.y ? srcOpers[0].value.u.y
                                                                        : srcOpers[1].value.u.y,
                         srcOpers[0].value.u.z >= srcOpers[1].value.u.z ? srcOpers[0].value.u.z
                                                                        : srcOpers[1].value.u.z,
                         srcOpers[0].value.u.w >= srcOpers[1].value.u.w ? srcOpers[0].value.u.w
                                                                        : srcOpers[1].value.u.w));
      break;
    case OPCODE_IMAX:
      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[0].value.i.x >= srcOpers[1].value.i.x ? srcOpers[0].value.i.x
                                                                            : srcOpers[1].value.i.x,
                         srcOpers[0].value.i.y >= srcOpers[1].value.i.y ? srcOpers[0].value.i.y
                                                                        : srcOpers[1].value.i.y,
                         srcOpers[0].value.i.z >= srcOpers[1].value.i.z ? srcOpers[0].value.i.z
                                                                        : srcOpers[1].value.i.z,
                         srcOpers[0].value.i.w >= srcOpers[1].value.i.w ? srcOpers[0].value.i.w
                                                                        : srcOpers[1].value.i.w));
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

      s.SetDst(op.operands[0], op, r);
      break;
    }
    case OPCODE_MAX:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", dxbc_max(srcOpers[0].value.f.x, srcOpers[1].value.f.x),
                              dxbc_max(srcOpers[0].value.f.y, srcOpers[1].value.f.y),
                              dxbc_max(srcOpers[0].value.f.z, srcOpers[1].value.f.z),
                              dxbc_max(srcOpers[0].value.f.w, srcOpers[1].value.f.w)));
      break;
    case OPCODE_SQRT:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", sqrtf(srcOpers[0].value.f.x), sqrtf(srcOpers[0].value.f.y),
                              sqrtf(srcOpers[0].value.f.z), sqrtf(srcOpers[0].value.f.w)));
      break;
    case OPCODE_DRCP:
    {
      double ds[2] = {0.0, 0.0};
      DoubleGet(srcOpers[0], ds);
      ds[0] = 1.0f / ds[0];
      ds[1] = 1.0f / ds[1];

      ShaderVariable r("", 0U, 0U, 0U, 0U);
      DoubleSet(r, ds);

      s.SetDst(op.operands[0], op, r);
      break;
    }

    case OPCODE_IBFE:
    {
      // bottom 5 bits
      ShaderVariable width(
          "", (int32_t)(srcOpers[0].value.i.x & 0x1f), (int32_t)(srcOpers[0].value.i.y & 0x1f),
          (int32_t)(srcOpers[0].value.i.z & 0x1f), (int32_t)(srcOpers[0].value.i.w & 0x1f));
      ShaderVariable offset(
          "", (int32_t)(srcOpers[1].value.i.x & 0x1f), (int32_t)(srcOpers[1].value.i.y & 0x1f),
          (int32_t)(srcOpers[1].value.i.z & 0x1f), (int32_t)(srcOpers[1].value.i.w & 0x1f));

      ShaderVariable dest("", (int32_t)0, (int32_t)0, (int32_t)0, (int32_t)0);

      for(int comp = 0; comp < 4; comp++)
      {
        if(width.value.iv[comp] == 0)
        {
          dest.value.iv[comp] = 0;
        }
        else if(width.value.iv[comp] + offset.value.iv[comp] < 32)
        {
          dest.value.iv[comp] = srcOpers[2].value.iv[comp]
                                << (32 - (width.value.iv[comp] + offset.value.iv[comp]));
          dest.value.iv[comp] = dest.value.iv[comp] >> (32 - width.value.iv[comp]);
        }
        else
        {
          dest.value.iv[comp] = srcOpers[2].value.iv[comp] >> offset.value.iv[comp];
        }
      }

      s.SetDst(op.operands[0], op, dest);
      break;
    }
    case OPCODE_UBFE:
    {
      // bottom 5 bits
      ShaderVariable width(
          "", (uint32_t)(srcOpers[0].value.u.x & 0x1f), (uint32_t)(srcOpers[0].value.u.y & 0x1f),
          (uint32_t)(srcOpers[0].value.u.z & 0x1f), (uint32_t)(srcOpers[0].value.u.w & 0x1f));
      ShaderVariable offset(
          "", (uint32_t)(srcOpers[1].value.u.x & 0x1f), (uint32_t)(srcOpers[1].value.u.y & 0x1f),
          (uint32_t)(srcOpers[1].value.u.z & 0x1f), (uint32_t)(srcOpers[1].value.u.w & 0x1f));

      ShaderVariable dest("", (uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0);

      for(int comp = 0; comp < 4; comp++)
      {
        if(width.value.uv[comp] == 0)
        {
          dest.value.uv[comp] = 0;
        }
        else if(width.value.uv[comp] + offset.value.uv[comp] < 32)
        {
          dest.value.uv[comp] = srcOpers[2].value.uv[comp]
                                << (32 - (width.value.uv[comp] + offset.value.uv[comp]));
          dest.value.uv[comp] = dest.value.uv[comp] >> (32 - width.value.uv[comp]);
        }
        else
        {
          dest.value.uv[comp] = srcOpers[2].value.uv[comp] >> offset.value.uv[comp];
        }
      }

      s.SetDst(op.operands[0], op, dest);
      break;
    }
    case OPCODE_BFI:
    {
      // bottom 5 bits
      ShaderVariable width(
          "", (uint32_t)(srcOpers[0].value.u.x & 0x1f), (uint32_t)(srcOpers[0].value.u.y & 0x1f),
          (uint32_t)(srcOpers[0].value.u.z & 0x1f), (uint32_t)(srcOpers[0].value.u.w & 0x1f));
      ShaderVariable offset(
          "", (uint32_t)(srcOpers[1].value.u.x & 0x1f), (uint32_t)(srcOpers[1].value.u.y & 0x1f),
          (uint32_t)(srcOpers[1].value.u.z & 0x1f), (uint32_t)(srcOpers[1].value.u.w & 0x1f));

      ShaderVariable dest("", (uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0);

      for(int comp = 0; comp < 4; comp++)
      {
        uint32_t bitmask = (((1 << width.value.uv[comp]) - 1) << offset.value.uv[comp]) & 0xffffffff;
        dest.value.uv[comp] =
            (uint32_t)(((srcOpers[2].value.uv[comp] << offset.value.uv[comp]) & bitmask) |
                       (srcOpers[3].value.uv[comp] & ~bitmask));
      }

      s.SetDst(op.operands[0], op, dest);
      break;
    }
    case OPCODE_ISHL:
    {
      uint32_t shifts[] = {
          srcOpers[1].value.u.x & 0x1f, srcOpers[1].value.u.y & 0x1f, srcOpers[1].value.u.z & 0x1f,
          srcOpers[1].value.u.w & 0x1f,
      };

      // if we were only given a single component, it's the form that shifts all components
      // by the same amount
      if(op.operands[2].numComponents == NUMCOMPS_1 ||
         (op.operands[2].comps[2] < 4 && op.operands[2].comps[2] == 0xff))
        shifts[3] = shifts[2] = shifts[1] = shifts[0];

      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[0].value.i.x << shifts[0], srcOpers[0].value.i.y << shifts[1],
                         srcOpers[0].value.i.z << shifts[2], srcOpers[0].value.i.w << shifts[3]));
      break;
    }
    case OPCODE_USHR:
    {
      uint32_t shifts[] = {
          srcOpers[1].value.u.x & 0x1f, srcOpers[1].value.u.y & 0x1f, srcOpers[1].value.u.z & 0x1f,
          srcOpers[1].value.u.w & 0x1f,
      };

      // if we were only given a single component, it's the form that shifts all components
      // by the same amount
      if(op.operands[2].numComponents == NUMCOMPS_1 ||
         (op.operands[2].comps[2] < 4 && op.operands[2].comps[2] == 0xff))
        shifts[3] = shifts[2] = shifts[1] = shifts[0];

      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[0].value.u.x >> shifts[0], srcOpers[0].value.u.y >> shifts[1],
                         srcOpers[0].value.u.z >> shifts[2], srcOpers[0].value.u.w >> shifts[3]));
      break;
    }
    case OPCODE_ISHR:
    {
      uint32_t shifts[] = {
          srcOpers[1].value.u.x & 0x1f, srcOpers[1].value.u.y & 0x1f, srcOpers[1].value.u.z & 0x1f,
          srcOpers[1].value.u.w & 0x1f,
      };

      // if we were only given a single component, it's the form that shifts all components
      // by the same amount
      if(op.operands[2].numComponents == NUMCOMPS_1 ||
         (op.operands[2].comps[2] < 4 && op.operands[2].comps[2] == 0xff))
        shifts[3] = shifts[2] = shifts[1] = shifts[0];

      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[0].value.i.x >> shifts[0], srcOpers[0].value.i.y >> shifts[1],
                         srcOpers[0].value.i.z >> shifts[2], srcOpers[0].value.i.w >> shifts[3]));
      break;
    }
    case OPCODE_AND:
      s.SetDst(op.operands[0], op, ShaderVariable("", srcOpers[0].value.i.x & srcOpers[1].value.i.x,
                                                  srcOpers[0].value.i.y & srcOpers[1].value.i.y,
                                                  srcOpers[0].value.i.z & srcOpers[1].value.i.z,
                                                  srcOpers[0].value.i.w & srcOpers[1].value.i.w));
      break;
    case OPCODE_OR:
      s.SetDst(op.operands[0], op, ShaderVariable("", srcOpers[0].value.i.x | srcOpers[1].value.i.x,
                                                  srcOpers[0].value.i.y | srcOpers[1].value.i.y,
                                                  srcOpers[0].value.i.z | srcOpers[1].value.i.z,
                                                  srcOpers[0].value.i.w | srcOpers[1].value.i.w));
      break;
    case OPCODE_XOR:
      s.SetDst(op.operands[0], op, ShaderVariable("", srcOpers[0].value.u.x ^ srcOpers[1].value.u.x,
                                                  srcOpers[0].value.u.y ^ srcOpers[1].value.u.y,
                                                  srcOpers[0].value.u.z ^ srcOpers[1].value.u.z,
                                                  srcOpers[0].value.u.w ^ srcOpers[1].value.u.w));
      break;
    case OPCODE_NOT:
      s.SetDst(op.operands[0], op, ShaderVariable("", ~srcOpers[0].value.u.x, ~srcOpers[0].value.u.y,
                                                  ~srcOpers[0].value.u.z, ~srcOpers[0].value.u.w));
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
        s.SetDst(op.operands[0], op, calcResultA);
      }
      else
      {
        return s;
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
          s.SetDst(op.operands[0], op, calcResultA);
        if(op.operands[1].type != TYPE_NULL)
          s.SetDst(op.operands[1], op, calcResultB);
      }
      else
      {
        return s;
      }
      break;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Misc

    case OPCODE_NOP:
    case OPCODE_CUSTOMDATA:
    case OPCODE_SYNC:    // might never need to implement this. Who knows!
      break;
    case OPCODE_DMOV:
    case OPCODE_MOV: s.SetDst(op.operands[0], op, srcOpers[0]); break;
    case OPCODE_DMOVC:
      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[0].value.u.x ? srcOpers[1].value.u.x : srcOpers[2].value.u.x,
                         srcOpers[0].value.u.x ? srcOpers[1].value.u.y : srcOpers[2].value.u.y,
                         srcOpers[0].value.u.y ? srcOpers[1].value.u.z : srcOpers[2].value.u.z,
                         srcOpers[0].value.u.y ? srcOpers[1].value.u.w : srcOpers[2].value.u.w));
      break;
    case OPCODE_MOVC:
      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[0].value.i.x ? srcOpers[1].value.i.x : srcOpers[2].value.i.x,
                         srcOpers[0].value.i.y ? srcOpers[1].value.i.y : srcOpers[2].value.i.y,
                         srcOpers[0].value.i.z ? srcOpers[1].value.i.z : srcOpers[2].value.i.z,
                         srcOpers[0].value.i.w ? srcOpers[1].value.i.w : srcOpers[2].value.i.w));
      break;
    case OPCODE_SWAPC:
      s.SetDst(
          op.operands[0], op,
          ShaderVariable("", srcOpers[1].value.i.x ? srcOpers[3].value.i.x : srcOpers[2].value.i.x,
                         srcOpers[1].value.i.y ? srcOpers[3].value.i.y : srcOpers[2].value.i.y,
                         srcOpers[1].value.i.z ? srcOpers[3].value.i.z : srcOpers[2].value.i.z,
                         srcOpers[1].value.i.w ? srcOpers[3].value.i.w : srcOpers[2].value.i.w));

      s.SetDst(
          op.operands[1], op,
          ShaderVariable("", srcOpers[1].value.i.x ? srcOpers[2].value.i.x : srcOpers[3].value.i.x,
                         srcOpers[1].value.i.y ? srcOpers[2].value.i.y : srcOpers[3].value.i.y,
                         srcOpers[1].value.i.z ? srcOpers[2].value.i.z : srcOpers[3].value.i.z,
                         srcOpers[1].value.i.w ? srcOpers[2].value.i.w : srcOpers[3].value.i.w));
      break;
    case OPCODE_ITOF:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (float)srcOpers[0].value.i.x, (float)srcOpers[0].value.i.y,
                              (float)srcOpers[0].value.i.z, (float)srcOpers[0].value.i.w));
      break;
    case OPCODE_UTOF:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (float)srcOpers[0].value.u.x, (float)srcOpers[0].value.u.y,
                              (float)srcOpers[0].value.u.z, (float)srcOpers[0].value.u.w));
      break;
    case OPCODE_FTOI:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (int)srcOpers[0].value.f.x, (int)srcOpers[0].value.f.y,
                              (int)srcOpers[0].value.f.z, (int)srcOpers[0].value.f.w));
      break;
    case OPCODE_FTOU:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (uint32_t)srcOpers[0].value.f.x, (uint32_t)srcOpers[0].value.f.y,
                              (uint32_t)srcOpers[0].value.f.z, (uint32_t)srcOpers[0].value.f.w));
      break;
    case OPCODE_ITOD:
    case OPCODE_UTOD:
    case OPCODE_FTOD:
    {
      double res[2];

      if(op.operation == OPCODE_ITOD)
      {
        res[0] = (double)srcOpers[0].value.i.x;
        res[1] = (double)srcOpers[0].value.i.y;
      }
      else if(op.operation == OPCODE_UTOD)
      {
        res[0] = (double)srcOpers[0].value.u.x;
        res[1] = (double)srcOpers[0].value.u.y;
      }
      else if(op.operation == OPCODE_FTOD)
      {
        res[0] = (double)srcOpers[0].value.f.x;
        res[1] = (double)srcOpers[0].value.f.y;
      }

      // if we only did a 1-wide double op, copy .xy into .zw so we can then
      // swizzle into .xy or .zw freely on the destination operand.
      // e.g. ftod r0.zw, r0.z - if we didn't do this, there'd be nothing valid in .zw
      if(op.operands[1].comps[2] == 0xff)
        res[1] = res[0];

      ShaderVariable r("", 0U, 0U, 0U, 0U);
      DoubleSet(r, res);

      s.SetDst(op.operands[0], op, r);
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
          r.value.uv[op.operands[0].comps[0]] = uint32_t(src[0]);
        }
        else
        {
          r.value.uv[op.operands[0].comps[0]] = uint32_t(src[0]);
          r.value.uv[op.operands[0].comps[1]] = uint32_t(src[1]);
        }
      }
      else if(op.operation == OPCODE_DTOI)
      {
        if(op.operands[0].comps[1] == 0xff)    // only one mask
        {
          r.value.iv[op.operands[0].comps[0]] = int32_t(src[0]);
        }
        else
        {
          r.value.iv[op.operands[0].comps[0]] = int32_t(src[0]);
          r.value.iv[op.operands[0].comps[1]] = int32_t(src[1]);
        }
      }
      else if(op.operation == OPCODE_DTOF)
      {
        if(op.operands[0].comps[1] == 0xff)    // only one mask
        {
          r.value.fv[op.operands[0].comps[0]] = float(src[0]);
        }
        else
        {
          r.value.fv[op.operands[0].comps[0]] = float(src[0]);
          r.value.fv[op.operands[0].comps[1]] = float(src[1]);
        }
      }

      s.SetDst(op.operands[0], op, r);
      break;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Comparison

    case OPCODE_EQ:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.f.x == srcOpers[1].value.f.x ? ~0l : 0l),
                              (srcOpers[0].value.f.y == srcOpers[1].value.f.y ? ~0l : 0l),
                              (srcOpers[0].value.f.z == srcOpers[1].value.f.z ? ~0l : 0l),
                              (srcOpers[0].value.f.w == srcOpers[1].value.f.w ? ~0l : 0l)));
      break;
    case OPCODE_NE:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.f.x != srcOpers[1].value.f.x ? ~0l : 0l),
                              (srcOpers[0].value.f.y != srcOpers[1].value.f.y ? ~0l : 0l),
                              (srcOpers[0].value.f.z != srcOpers[1].value.f.z ? ~0l : 0l),
                              (srcOpers[0].value.f.w != srcOpers[1].value.f.w ? ~0l : 0l)));
      break;
    case OPCODE_LT:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.f.x < srcOpers[1].value.f.x ? ~0l : 0l),
                              (srcOpers[0].value.f.y < srcOpers[1].value.f.y ? ~0l : 0l),
                              (srcOpers[0].value.f.z < srcOpers[1].value.f.z ? ~0l : 0l),
                              (srcOpers[0].value.f.w < srcOpers[1].value.f.w ? ~0l : 0l)));
      break;
    case OPCODE_GE:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.f.x >= srcOpers[1].value.f.x ? ~0l : 0l),
                              (srcOpers[0].value.f.y >= srcOpers[1].value.f.y ? ~0l : 0l),
                              (srcOpers[0].value.f.z >= srcOpers[1].value.f.z ? ~0l : 0l),
                              (srcOpers[0].value.f.w >= srcOpers[1].value.f.w ? ~0l : 0l)));
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
      }

      // special behaviour for dest mask. if it's .xz then first comparison goes into .x, second
      // into .z.
      // if the mask is .y then the first comparison goes into .y and second goes nowhere.
      // so we need to check the dest mask and put the comparison results into the right place

      ShaderVariable r("", 0U, 0U, 0U, 0U);

      if(op.operands[0].comps[1] == 0xff)    // only one mask
      {
        r.value.uv[op.operands[0].comps[0]] = cmp1;
      }
      else
      {
        r.value.uv[op.operands[0].comps[0]] = cmp1;
        r.value.uv[op.operands[0].comps[1]] = cmp2;
      }

      s.SetDst(op.operands[0], op, r);
      break;
    }
    case OPCODE_IEQ:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.i.x == srcOpers[1].value.i.x ? ~0l : 0l),
                              (srcOpers[0].value.i.y == srcOpers[1].value.i.y ? ~0l : 0l),
                              (srcOpers[0].value.i.z == srcOpers[1].value.i.z ? ~0l : 0l),
                              (srcOpers[0].value.i.w == srcOpers[1].value.i.w ? ~0l : 0l)));
      break;
    case OPCODE_INE:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.i.x != srcOpers[1].value.i.x ? ~0l : 0l),
                              (srcOpers[0].value.i.y != srcOpers[1].value.i.y ? ~0l : 0l),
                              (srcOpers[0].value.i.z != srcOpers[1].value.i.z ? ~0l : 0l),
                              (srcOpers[0].value.i.w != srcOpers[1].value.i.w ? ~0l : 0l)));
      break;
    case OPCODE_IGE:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.i.x >= srcOpers[1].value.i.x ? ~0l : 0l),
                              (srcOpers[0].value.i.y >= srcOpers[1].value.i.y ? ~0l : 0l),
                              (srcOpers[0].value.i.z >= srcOpers[1].value.i.z ? ~0l : 0l),
                              (srcOpers[0].value.i.w >= srcOpers[1].value.i.w ? ~0l : 0l)));
      break;
    case OPCODE_ILT:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.i.x < srcOpers[1].value.i.x ? ~0l : 0l),
                              (srcOpers[0].value.i.y < srcOpers[1].value.i.y ? ~0l : 0l),
                              (srcOpers[0].value.i.z < srcOpers[1].value.i.z ? ~0l : 0l),
                              (srcOpers[0].value.i.w < srcOpers[1].value.i.w ? ~0l : 0l)));
      break;
    case OPCODE_ULT:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.u.x < srcOpers[1].value.u.x ? ~0l : 0l),
                              (srcOpers[0].value.u.y < srcOpers[1].value.u.y ? ~0l : 0l),
                              (srcOpers[0].value.u.z < srcOpers[1].value.u.z ? ~0l : 0l),
                              (srcOpers[0].value.u.w < srcOpers[1].value.u.w ? ~0l : 0l)));
      break;
    case OPCODE_UGE:
      s.SetDst(op.operands[0], op,
               ShaderVariable("", (srcOpers[0].value.u.x >= srcOpers[1].value.u.x ? ~0l : 0l),
                              (srcOpers[0].value.u.y >= srcOpers[1].value.u.y ? ~0l : 0l),
                              (srcOpers[0].value.u.z >= srcOpers[1].value.u.z ? ~0l : 0l),
                              (srcOpers[0].value.u.w >= srcOpers[1].value.u.w ? ~0l : 0l)));
      break;

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Atomic instructions

    case OPCODE_IMM_ATOMIC_ALLOC:
    {
      uint32_t count = global.uavs[srcOpers[0].value.u.x].hiddenCounter++;
      s.SetDst(op.operands[0], op, ShaderVariable("", count, count, count, count));
      break;
    }

    case OPCODE_IMM_ATOMIC_CONSUME:
    {
      uint32_t count = --global.uavs[srcOpers[0].value.u.x].hiddenCounter;
      s.SetDst(op.operands[0], op, ShaderVariable("", count, count, count, count));
      break;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Derivative instructions

    // don't differentiate, coarse, fine, whatever. The spec lets us implement it all as fine.
    case OPCODE_DERIV_RTX:
    case OPCODE_DERIV_RTX_COARSE:
    case OPCODE_DERIV_RTX_FINE:
      if(quad == NULL)
        RDCERR(
            "Attempt to use derivative instruction not in pixel shader. Undefined results will "
            "occur!");
      else
        s.SetDst(op.operands[0], op,
                 s.DDX(op.operation == OPCODE_DERIV_RTX_FINE, quad, op.operands[1], op));
      break;
    case OPCODE_DERIV_RTY:
    case OPCODE_DERIV_RTY_COARSE:
    case OPCODE_DERIV_RTY_FINE:
      if(quad == NULL)
        RDCERR(
            "Attempt to use derivative instruction not in pixel shader. Undefined results will "
            "occur!");
      else
        s.SetDst(op.operands[0], op,
                 s.DDY(op.operation == OPCODE_DERIV_RTY_FINE, quad, op.operands[1], op));
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
        offset = global.uavs[resIndex].firstElement;
        numElems = global.uavs[resIndex].numElements;
        data = &global.uavs[resIndex].data[0];

        for(size_t i = 0; i < s.program->GetNumDeclarations(); i++)
        {
          const DXBCBytecode::Declaration &decl = program->GetDeclaration(i);

          if(decl.operand.type == TYPE_UNORDERED_ACCESS_VIEW &&
             decl.operand.indices[0].index == resIndex)
          {
            if(decl.declaration == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW)
            {
              stride = 4;
              structured = false;
              break;
            }
            else if(decl.declaration == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED)
            {
              stride = decl.stride;
              structured = true;
              break;
            }
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
        data += (offset + dstAddress->value.u.x) * stride;
        if(structured)
          data += dstAddress->value.u.y;
      }

      // if out of bounds, undefined result is returned to dst0 for immediate operands,
      // so we only need to care about the in-bounds case.
      // Also helper/inactive pixels are not allowed to modify UAVs
      if(data && offset + dstAddress->value.u.x < numElems && !Finished())
      {
        uint32_t *udst = (uint32_t *)data;
        int32_t *idst = (int32_t *)data;

        if(beforeResult.type != TYPE_NULL)
        {
          s.SetDst(beforeResult, op, ShaderVariable("", *udst, *udst, *udst, *udst));
        }

        // not verified below since by definition the operations that expect usrc1 will have it
        uint32_t *usrc0 = src0 ? src0->value.uv : NULL;
        uint32_t *usrc1 = src1 ? src1->value.uv : NULL;

        int32_t *isrc0 = src0 ? src0->value.iv : NULL;

        switch(op.operation)
        {
          case OPCODE_IMM_ATOMIC_IADD:
          case OPCODE_ATOMIC_IADD: *udst = *udst + *usrc0; break;
          case OPCODE_IMM_ATOMIC_IMAX:
          case OPCODE_ATOMIC_IMAX: *idst = std::max(*idst, *isrc0); break;
          case OPCODE_IMM_ATOMIC_IMIN:
          case OPCODE_ATOMIC_IMIN: *idst = std::min(*idst, *isrc0); break;
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
          case OPCODE_ATOMIC_UMAX: *udst = std::max(*udst, *usrc0); break;
          case OPCODE_IMM_ATOMIC_UMIN:
          case OPCODE_ATOMIC_UMIN: *udst = std::min(*udst, *usrc0); break;
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
      uint32_t elemOffset = 0;
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

      if(load)
        s.flags = ShaderEvents::SampleLoadGather;

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
            for(size_t i = 0; i < s.program->GetNumDeclarations(); i++)
            {
              const DXBCBytecode::Declaration &decl = program->GetDeclaration(i);

              if(decl.operand.type == TYPE_UNORDERED_ACCESS_VIEW && !srv &&
                 decl.operand.indices[0].index == resIndex &&
                 decl.declaration == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED)
              {
                stride = decl.stride;
                break;
              }
              if(decl.operand.type == TYPE_RESOURCE && srv &&
                 decl.operand.indices[0].index == resIndex &&
                 decl.declaration == OPCODE_DCL_RESOURCE_STRUCTURED)
              {
                stride = decl.stride;
                break;
              }
            }
          }
        }

        elemOffset = srcOpers[1].value.u.x;
        elemIdx = srcOpers[0].value.u.x;
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

        elemIdx = srcOpers[0].value.u.x;

        // could be a tex load
        texCoords[0] = srcOpers[0].value.u.x;
        texCoords[1] = srcOpers[0].value.u.y;
        texCoords[2] = srcOpers[0].value.u.z;

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
        }
        else
        {
          resIndex = (uint32_t)op.operands[0].indices[0].index;
          srv = false;
          gsm = (op.operands[0].type == TYPE_THREAD_GROUP_SHARED_MEMORY);
        }

        // the index is supposed to be a multiple of 4 but the behaviour seems to be to round down
        elemIdx = (srcOpers[0].value.u.x & ~0x3);
        stride = 1;
      }

      RDCASSERT(stride != 0);

      uint32_t offset = srv ? global.srvs[resIndex].firstElement : global.uavs[resIndex].firstElement;
      uint32_t numElems = srv ? global.srvs[resIndex].numElements : global.uavs[resIndex].numElements;
      GlobalState::ViewFmt fmt = srv ? global.srvs[resIndex].format : global.uavs[resIndex].format;

      // indexing for raw views is in bytes, but firstElement/numElements is in format-sized
      // units. Multiply up by stride
      if(op.operation == OPCODE_LD_RAW || op.operation == OPCODE_STORE_RAW)
      {
        offset *= RDCMIN(4, fmt.byteWidth);
        numElems *= RDCMIN(4, fmt.byteWidth);
      }

      byte *data = srv ? &global.srvs[resIndex].data[0] : &global.uavs[resIndex].data[0];
      bool texData = srv ? false : global.uavs[resIndex].tex;
      uint32_t rowPitch = srv ? 0 : global.uavs[resIndex].rowPitch;
      uint32_t depthPitch = srv ? 0 : global.uavs[resIndex].depthPitch;

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
          fmt.fmt = CompType::UInt;
          fmt.byteWidth = 4;
          fmt.numComps = global.groupshared[resIndex].bytestride / 4;
          fmt.stride = 0;
        }
        texData = false;
      }

      RDCASSERT(data);

      size_t texOffset = 0;

      if(texData)
      {
        texOffset += texCoords[0] * fmt.Stride();
        texOffset += texCoords[1] * rowPitch;
        texOffset += texCoords[2] * depthPitch;
      }

      if(!data || (!texData && elemIdx >= numElems) ||
         (texData && texOffset >= global.uavs[resIndex].data.size()))
      {
        if(load)
          s.SetDst(op.operands[0], op, ShaderVariable("", 0U, 0U, 0U, 0U));
      }
      else
      {
        if(gsm || !texData)
        {
          data += (offset + elemIdx) * stride;
          data += elemOffset;
        }
        else
        {
          data += texOffset;
        }

        int maxIndex = fmt.numComps;

        uint32_t srcIdx = 1;
        if(op.operation == OPCODE_STORE_STRUCTURED || op.operation == OPCODE_LD_STRUCTURED)
        {
          srcIdx = 2;
          maxIndex = (stride - elemOffset) / sizeof(uint32_t);
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

            fetch.value.uv[c] = result.value.uv[comp];
          }

          if(op.operation != OPCODE_LD_RAW && op.operation != OPCODE_LD_STRUCTURED)
          {
            // if we are assigning into a scalar, SetDst expects the result to be in .x (as normally
            // we are assigning FROM a scalar also).
            // to match this expectation, propogate the component across.
            if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
               op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
              fetch.value.uv[0] = fetch.value.uv[op.operands[0].comps[0]];
          }

          s.SetDst(op.operands[0], op, fetch);
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

      key.quadIndex = quadIndex;

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
        key.sample = srcOpers[1].value.i.x;
      }
      else if(op.operation == OPCODE_EVAL_SNAPPED)
      {
        key.offsetx = RDCCLAMP(srcOpers[1].value.i.x, -8, 7);
        key.offsety = RDCCLAMP(srcOpers[1].value.i.y, -8, 7);
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
            var.value.uv[i] = it->second.value.uv[op.operands[1].comps[i]];

        s.SetDst(op.operands[0], op, var);
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
                  s.nextInstruction - 1, op.str.c_str()));
        }

        s.SetDst(op.operands[0], op, srcOpers[0]);
      }

      break;
    }

    case OPCODE_SAMPLE_INFO:
    case OPCODE_SAMPLE_POS:
    {
      bool isAbsoluteResource =
          (op.operands[1].indices.size() == 1 && op.operands[1].indices[0].absolute &&
           !op.operands[1].indices[0].relative);
      UINT slot = (UINT)(op.operands[1].indices[0].index & 0xffffffff);
      ShaderVariable result =
          apiWrapper->GetSampleInfo(op.operands[1].type, isAbsoluteResource, slot, op.str.c_str());

      // "If there is no resource bound to the specified slot, 0 is returned."

      // lookup sample pos if we got a count from above
      if(op.operation == OPCODE_SAMPLE_POS && result.value.u.x > 0 &&
         op.operands[2].type == TYPE_IMMEDIATE32)
      {
        // assume standard sample pattern - this might not hold in all cases
        // http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx

        uint32_t sampleIndex = op.operands[2].values[0];
        uint32_t sampleCount = result.value.u.x;

        if(sampleIndex >= sampleCount)
        {
          RDCWARN("sample index %u is out of bounds on resource bound to sample_pos (%u samples)",
                  sampleIndex, sampleCount);
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
                    s.nextInstruction - 1, op.str.c_str()));

            sample_pattern = NULL;
          }
          else if(sampleCount == 2)
          {
            static const float pattern_2x[] = {
                _SMP(4.0f), _SMP(4.0f), _SMP(-4.0f), _SMP(-4.0f),
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
            RDCERR("Unsupported sample count on resource for sample_pos: %u", result.value.u.x);

            sample_pattern = NULL;
          }

          if(sample_pattern == NULL)
          {
            result.value.f.x = 0.0f;
            result.value.f.y = 0.0f;
          }
          else
          {
            result.value.f.x = sample_pattern[sampleIndex * 2 + 0];
            result.value.f.y = sample_pattern[sampleIndex * 2 + 1];
          }
        }

#undef _SMP
      }

      // apply swizzle
      ShaderVariable swizzled("", 0.0f, 0.0f, 0.0f, 0.0f);

      for(int i = 0; i < 4; i++)
      {
        if(op.operands[1].comps[i] == 0xff)
          swizzled.value.uv[i] = result.value.uv[0];
        else
          swizzled.value.uv[i] = result.value.uv[op.operands[1].comps[i]];
      }

      // apply ret type
      if(op.operation == OPCODE_SAMPLE_POS)
      {
        result = swizzled;
        result.type = VarType::Float;
      }
      else if(op.resinfoRetType == RETTYPE_FLOAT)
      {
        result.value.f.x = (float)swizzled.value.u.x;
        result.value.f.y = (float)swizzled.value.u.y;
        result.value.f.z = (float)swizzled.value.u.z;
        result.value.f.w = (float)swizzled.value.u.w;
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
        result.value.uv[0] = result.value.uv[op.operands[0].comps[0]];

      s.SetDst(op.operands[0], op, result);

      break;
    }

    case OPCODE_BUFINFO:
    {
      if(op.operands[1].indices.size() == 1 && op.operands[1].indices[0].absolute &&
         !op.operands[1].indices[0].relative)
      {
        UINT slot = (UINT)(op.operands[1].indices[0].index & 0xffffffff);
        ShaderVariable result = apiWrapper->GetBufferInfo(op.operands[1].type, slot, op.str.c_str());

        // apply swizzle
        ShaderVariable swizzled("", 0.0f, 0.0f, 0.0f, 0.0f);

        for(int i = 0; i < 4; i++)
        {
          if(op.operands[1].comps[i] == 0xff)
            swizzled.value.uv[i] = result.value.uv[0];
          else
            swizzled.value.uv[i] = result.value.uv[op.operands[1].comps[i]];
        }

        result = swizzled;
        result.type = VarType::UInt;

        // if we are assigning into a scalar, SetDst expects the result to be in .x (as normally we
        // are assigning FROM a scalar also).
        // to match this expectation, propogate the component across.
        if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
           op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
          result.value.uv[0] = result.value.uv[op.operands[0].comps[0]];

        s.SetDst(op.operands[0], op, result);
      }
      else
      {
        RDCERR("Unexpected relative addressing");
        s.SetDst(op.operands[0], op, ShaderVariable("", 0.0f, 0.0f, 0.0f, 0.0f));
      }

      break;
    }

    case OPCODE_RESINFO:
    {
      // spec says "srcMipLevel is read as an unsigned integer scalar"
      uint32_t mipLevel = srcOpers[0].value.u.x;

      if(op.operands[2].indices.size() == 1 && op.operands[2].indices[0].absolute &&
         !op.operands[2].indices[0].relative)
      {
        int dim = 0;
        UINT slot = (UINT)(op.operands[2].indices[0].index & 0xffffffff);
        ShaderVariable result = apiWrapper->GetResourceInfo(op.operands[2].type, slot, mipLevel, dim);

        // need a valid dimension even if the resource was unbound, so
        // search for the declaration
        if(dim == 0)
        {
          for(size_t i = 0; i < s.program->GetNumDeclarations(); i++)
          {
            const DXBCBytecode::Declaration &decl = s.program->GetDeclaration(i);

            if(decl.declaration == OPCODE_DCL_RESOURCE && decl.operand.type == TYPE_RESOURCE &&
               decl.operand.indices.size() == 1 &&
               decl.operand.indices[0] == op.operands[2].indices[0])
            {
              switch(decl.dim)
              {
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
              break;
            }
          }
        }

        // apply swizzle
        ShaderVariable swizzled("", 0.0f, 0.0f, 0.0f, 0.0f);

        for(int i = 0; i < 4; i++)
        {
          if(op.operands[2].comps[i] == 0xff)
            swizzled.value.uv[i] = result.value.uv[0];
          else
            swizzled.value.uv[i] = result.value.uv[op.operands[2].comps[i]];
        }

        // apply ret type
        if(op.resinfoRetType == RETTYPE_FLOAT)
        {
          result.value.f.x = (float)swizzled.value.u.x;
          result.value.f.y = (float)swizzled.value.u.y;
          result.value.f.z = (float)swizzled.value.u.z;
          result.value.f.w = (float)swizzled.value.u.w;
          result.type = VarType::Float;
        }
        else if(op.resinfoRetType == RETTYPE_RCPFLOAT)
        {
          // only width/height/depth values we set are reciprocated, other values
          // are just left as is
          if(dim <= 1)
            result.value.f.x = 1.0f / (float)swizzled.value.u.x;
          else
            result.value.f.x = (float)swizzled.value.u.x;

          if(dim <= 2)
            result.value.f.y = 1.0f / (float)swizzled.value.u.y;
          else
            result.value.f.y = (float)swizzled.value.u.y;

          if(dim <= 3)
            result.value.f.z = 1.0f / (float)swizzled.value.u.z;
          else
            result.value.f.z = (float)swizzled.value.u.z;

          result.value.f.w = (float)swizzled.value.u.w;
          result.type = VarType::Float;
        }
        else if(op.resinfoRetType == RETTYPE_UINT)
        {
          result = swizzled;
          result.type = VarType::UInt;
        }

        // if we are assigning into a scalar, SetDst expects the result to be in .x (as normally we
        // are assigning FROM a scalar also).
        // to match this expectation, propogate the component across.
        if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
           op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
          result.value.uv[0] = result.value.uv[op.operands[0].comps[0]];

        s.SetDst(op.operands[0], op, result);
      }
      else
      {
        RDCERR("Unexpected relative addressing");
        s.SetDst(op.operands[0], op, ShaderVariable("", 0.0f, 0.0f, 0.0f, 0.0f));
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
      if(op.operation != OPCODE_LOD)
        s.flags = ShaderEvents::SampleLoadGather;

      SamplerMode samplerMode = NUM_SAMPLERS;
      ResourceDimension resourceDim = RESOURCE_DIMENSION_UNKNOWN;
      DXBC::ResourceRetType resourceRetType = DXBC::RETURN_TYPE_UNKNOWN;
      int sampleCount = 0;

      for(size_t i = 0; i < program->GetNumDeclarations(); i++)
      {
        const Declaration &decl = program->GetDeclaration(i);

        if(decl.declaration == OPCODE_DCL_SAMPLER && op.operands.size() > 3 &&
           decl.operand.indices == op.operands[3].indices)
        {
          samplerMode = decl.samplerMode;
        }
        if(decl.dim == RESOURCE_DIMENSION_BUFFER && op.operation == OPCODE_LD &&
           decl.declaration == OPCODE_DCL_RESOURCE && decl.operand.type == TYPE_RESOURCE &&
           decl.operand.indices.size() == 1 && decl.operand.indices[0] == op.operands[2].indices[0])
        {
          resourceDim = decl.dim;

          uint32_t resIndex = (uint32_t)decl.operand.indices[0].index;

          const byte *data = &global.srvs[resIndex].data[0];
          uint32_t offset = global.srvs[resIndex].firstElement;
          uint32_t numElems = global.srvs[resIndex].numElements;

          GlobalState::ViewFmt fmt = global.srvs[resIndex].format;

          data += fmt.Stride() * offset;

          ShaderVariable result;

          {
            result = ShaderVariable("", 0.0f, 0.0f, 0.0f, 0.0f);

            if(srcOpers[0].value.uv[0] < numElems)
              result = TypedUAVLoad(fmt, data + srcOpers[0].value.uv[0] * fmt.Stride());
          }

          ShaderVariable fetch("", 0U, 0U, 0U, 0U);

          for(int c = 0; c < 4; c++)
          {
            uint8_t comp = op.operands[2].comps[c];
            if(op.operands[2].comps[c] == 0xff)
              comp = 0;

            fetch.value.uv[c] = result.value.uv[comp];
          }

          // if we are assigning into a scalar, SetDst expects the result to be in .x (as normally
          // we are assigning FROM a scalar also).
          // to match this expectation, propogate the component across.
          if(op.operands[0].comps[0] != 0xff && op.operands[0].comps[1] == 0xff &&
             op.operands[0].comps[2] == 0xff && op.operands[0].comps[3] == 0xff)
            fetch.value.uv[0] = fetch.value.uv[op.operands[0].comps[0]];

          s.SetDst(op.operands[0], op, fetch);

          return s;
        }
        if(decl.declaration == OPCODE_DCL_RESOURCE && decl.operand.type == TYPE_RESOURCE &&
           decl.operand.indices.size() == 1 && decl.operand.indices[0] == op.operands[2].indices[0])
        {
          resourceDim = decl.dim;
          resourceRetType = decl.resType[0];
          sampleCount = decl.sampleCount;

          // doesn't seem like these are ever less than four components, even if the texture is
          // declared <float3> for example.
          // shouldn't matter though is it just comes out in the wash.
          RDCASSERT(decl.resType[0] == decl.resType[1] && decl.resType[1] == decl.resType[2] &&
                    decl.resType[2] == decl.resType[3]);
          RDCASSERT(decl.resType[0] != DXBC::RETURN_TYPE_CONTINUED &&
                    decl.resType[0] != DXBC::RETURN_TYPE_UNUSED &&
                    decl.resType[0] != DXBC::RETURN_TYPE_MIXED && decl.resType[0] >= 0 &&
                    decl.resType[0] < DXBC::NUM_RETURN_TYPES);
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

        s.SetDst(op.operands[0], op, invalidResult);
        break;
      }

      ShaderVariable uv = srcOpers[0];
      ShaderVariable ddxCalc;
      ShaderVariable ddyCalc;

      // these ops need DDX/DDY
      if(op.operation == OPCODE_SAMPLE || op.operation == OPCODE_SAMPLE_B ||
         op.operation == OPCODE_SAMPLE_C || op.operation == OPCODE_LOD)
      {
        if(quad == NULL)
        {
          RDCERR(
              "Attempt to use derivative instruction not in pixel shader. Undefined results will "
              "occur!");
        }
        else
        {
          // texture samples use coarse derivatives
          ddxCalc = s.DDX(false, quad, op.operands[1], op);
          ddyCalc = s.DDY(false, quad, op.operands[1], op);
        }
      }
      else if(op.operation == OPCODE_SAMPLE_D)
      {
        ddxCalc = srcOpers[3];
        ddyCalc = srcOpers[4];
      }

      UINT texSlot = (UINT)op.operands[2].indices[0].index;
      UINT samplerSlot = 0;

      for(size_t i = 0; i < op.operands.size(); i++)
      {
        const Operand &operand = op.operands[i];
        if(operand.type == OperandType::TYPE_SAMPLER)
          samplerSlot = (UINT)operand.indices[0].index;
      }

      int multisampleIndex = srcOpers[2].value.i.x;
      float lodOrCompareValue = srcOpers[3].value.f.x;
      if(op.operation == OPCODE_GATHER4_PO_C)
        lodOrCompareValue = srcOpers[4].value.f.x;

      uint8_t swizzle[4] = {0};
      for(int i = 0; i < 4; i++)
      {
        if(op.operands[2].comps[i] == 0xff)
          swizzle[i] = 0;
        else
          swizzle[i] = op.operands[2].comps[i];
      }

      GatherChannel gatherChannel = GatherChannel::Red;
      if(op.operation == OPCODE_GATHER4 || op.operation == OPCODE_GATHER4_C ||
         op.operation == OPCODE_GATHER4_PO || op.operation == OPCODE_GATHER4_PO_C)
      {
        gatherChannel = (GatherChannel)op.operands[3].comps[0];
      }

      // for bias instruction we can't do a SampleGradBias, so add the bias into the sampler state.
      float samplerBias = 0.0f;
      if(op.operation == OPCODE_SAMPLE_B)
      {
        samplerSlot = (UINT)srcOpers[2].value.u.x;
        samplerBias = srcOpers[3].value.f.x;
      }

      SampleGatherResourceData resourceData;
      resourceData.dim = resourceDim;
      resourceData.retType = resourceRetType;
      resourceData.sampleCount = sampleCount;
      resourceData.slot = texSlot;

      SampleGatherSamplerData samplerData;
      samplerData.mode = samplerMode;
      samplerData.slot = samplerSlot;
      samplerData.bias = samplerBias;

      ShaderVariable lookupResult("tex", 0.0f, 0.0f, 0.0f, 0.0f);
      if(apiWrapper->CalculateSampleGather(op.operation, resourceData, samplerData, uv, ddxCalc,
                                           ddyCalc, op.texelOffset, multisampleIndex,
                                           lodOrCompareValue, swizzle, gatherChannel,
                                           op.str.c_str(), lookupResult))
      {
        // should be a better way of doing this
        if(op.operands[0].comps[1] == 0xff)
          lookupResult.value.iv[0] = lookupResult.value.iv[op.operands[0].comps[0]];

        s.SetDst(op.operands[0], op, lookupResult);
      }
      else
      {
        return s;
      }
      break;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Flow control

    case OPCODE_SWITCH:
    {
      uint32_t switchValue = GetSrc(op.operands[0], op).value.u.x;

      int depth = 0;

      uint32_t jumpLocation = 0;

      uint32_t search = s.nextInstruction;

      for(; search < (uint32_t)program->GetNumInstructions(); search++)
      {
        const Operation &nextOp = s.program->GetInstruction((size_t)search);

        // track nested switch statements to ensure we don't accidentally pick the case from a
        // different switch
        if(nextOp.operation == OPCODE_SWITCH)
        {
          depth++;
        }
        else if(nextOp.operation == OPCODE_ENDSWITCH)
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
            uint32_t caseValue = GetSrc(nextOp.operands[0], nextOp).value.u.x;

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
          const Operation &nextOp = s.program->GetInstruction(jumpLocation);

          if(nextOp.operation != OPCODE_CASE && nextOp.operation != OPCODE_DEFAULT)
            break;
        }

        s.nextInstruction = jumpLocation;
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

      int32_t test = op.operation == OPCODE_CONTINUEC ? GetSrc(op.operands[0], op).value.i.x : 0;

      if(op.operation == OPCODE_CONTINUE || op.operation == OPCODE_CONTINUEC)
        depth = 1;

      if((test == 0 && !op.nonzero) || (test != 0 && op.nonzero) ||
         op.operation == OPCODE_CONTINUE || op.operation == OPCODE_ENDLOOP)
      {
        // skip back one to the endloop that we're processing
        s.nextInstruction--;

        for(; s.nextInstruction >= 0; s.nextInstruction--)
        {
          if(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ENDLOOP)
            depth++;
          if(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_LOOP)
            depth--;

          if(depth == 0)
          {
            break;
          }
        }

        RDCASSERT(s.nextInstruction >= 0);
      }

      break;
    }
    case OPCODE_BREAK:
    case OPCODE_BREAKC:
    {
      int32_t test = op.operation == OPCODE_BREAKC ? GetSrc(op.operands[0], op).value.i.x : 0;

      if((test == 0 && !op.nonzero) || (test != 0 && op.nonzero) || op.operation == OPCODE_BREAK)
      {
        // break out (jump to next endloop/endswitch)
        int depth = 1;

        for(; s.nextInstruction < (int)program->GetNumInstructions(); s.nextInstruction++)
        {
          if(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_LOOP ||
             s.program->GetInstruction(s.nextInstruction).operation == OPCODE_SWITCH)
            depth++;
          if(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ENDLOOP ||
             s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ENDSWITCH)
            depth--;

          if(depth == 0)
          {
            break;
          }
        }

        RDCASSERT(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ENDLOOP ||
                  s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ENDSWITCH);

        // don't want to process the endloop and jump again!
        s.nextInstruction++;
      }

      break;
    }
    case OPCODE_IF:
    {
      int32_t test = GetSrc(op.operands[0], op).value.i.x;

      if((test == 0 && !op.nonzero) || (test != 0 && op.nonzero))
      {
        // nothing, we go into the if.
      }
      else
      {
        // jump to after the next matching else/endif
        int depth = 0;

        // skip back one to the if that we're processing
        s.nextInstruction--;

        for(; s.nextInstruction < (int)program->GetNumInstructions(); s.nextInstruction++)
        {
          if(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_IF)
            depth++;
          // only step out on an else if it's the matching depth to our starting if (depth == 1)
          if(depth == 1 && s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ELSE)
            depth--;
          if(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ENDIF)
            depth--;

          if(depth == 0)
          {
            break;
          }
        }

        RDCASSERT(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ELSE ||
                  s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ENDIF);

        // step to next instruction after the else/endif (processing an else would skip that block)
        s.nextInstruction++;
      }

      break;
    }
    case OPCODE_ELSE:
    {
      // if we hit an else then we've just processed the if() bracket and need to break out (jump to
      // next endif)
      int depth = 1;

      for(; s.nextInstruction < (int)program->GetNumInstructions(); s.nextInstruction++)
      {
        if(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_IF)
          depth++;
        if(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ENDIF)
          depth--;

        if(depth == 0)
        {
          break;
        }
      }

      RDCASSERT(s.program->GetInstruction(s.nextInstruction).operation == OPCODE_ENDIF);

      // step to next instruction after the else/endif (for consistency with handling in the if
      // block)
      s.nextInstruction++;

      break;
    }
    case OPCODE_DISCARD:
    {
      int32_t test = GetSrc(op.operands[0], op).value.i.x;

      if((test != 0 && !op.nonzero) || (test == 0 && op.nonzero))
      {
        // don't discard
        break;
      }

      // discarding.
      s.done = true;
      break;
    }
    case OPCODE_RET:
    case OPCODE_RETC:
    {
      int32_t test = op.operation == OPCODE_RETC ? GetSrc(op.operands[0], op).value.i.x : 0;

      if((test == 0 && !op.nonzero) || (test != 0 && op.nonzero) || op.operation == OPCODE_RET)
      {
        // assumes not in a function call
        s.done = true;
      }
      break;
    }
    default:
    {
      RDCERR("Unsupported operation %d in assembly debugging", op.operation);
      break;
    }
  }

  return s;
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

        mem.count = decl.count;
        if(mem.structured)
          mem.bytestride = decl.stride;
        else
          mem.bytestride = 4;    // raw groupshared is implicitly uint32s

        mem.data.resize(mem.bytestride * mem.count);
      }
    }
  }
}

void CreateShaderDebugStateAndTrace(ShaderDebug::State &initialState, ShaderDebugTrace &trace,
                                    int quadIdx, DXBC::DXBCContainer *dxbc,
                                    const ShaderReflection &refl, bytebuf *cbufData)
{
  initialState = ShaderDebug::State(quadIdx, &trace, dxbc->GetReflection(), dxbc->GetDXBCByteCode());

  size_t numInputs = dxbc->GetReflection()->InputSig.size();
  size_t numOutputs = dxbc->GetReflection()->OutputSig.size();

  int32_t maxReg = -1;
  for(size_t i = 0; i < numInputs; i++)
    maxReg = RDCMAX(maxReg, (int32_t)dxbc->GetReflection()->InputSig[i].regIndex);

  bool inputCoverage = false;

  // Check if the shader uses the coverage mask
  for(size_t i = 0; i < dxbc->GetDXBCByteCode()->GetNumDeclarations(); i++)
  {
    const Declaration &decl = dxbc->GetDXBCByteCode()->GetDeclaration(i);

    if(decl.declaration == OPCODE_DCL_INPUT && decl.operand.type == TYPE_INPUT_COVERAGE_MASK)
    {
      inputCoverage = true;
      break;
    }
  }

  // Add inputs to the shader trace
  if(maxReg >= 0 || inputCoverage)
  {
    trace.inputs.resize(maxReg + 1 + (inputCoverage ? 1 : 0));
    for(size_t i = 0; i < numInputs; i++)
    {
      const SigParameter &sig = dxbc->GetReflection()->InputSig[i];

      ShaderVariable v;

      v.name = StringFormat::Fmt("v%d (%s)", sig.regIndex, sig.semanticIdxName.c_str());
      v.rows = 1;
      v.columns = sig.regChannelMask & 0x8 ? 4 : sig.regChannelMask & 0x4
                                                     ? 3
                                                     : sig.regChannelMask & 0x2
                                                           ? 2
                                                           : sig.regChannelMask & 0x1 ? 1 : 0;

      if(sig.compType == CompType::UInt)
        v.type = VarType::UInt;
      else if(sig.compType == CompType::SInt)
        v.type = VarType::SInt;

      if(trace.inputs[sig.regIndex].columns == 0)
        trace.inputs[sig.regIndex] = v;
      else
        trace.inputs[sig.regIndex].columns = RDCMAX(trace.inputs[sig.regIndex].columns, v.columns);
    }

    // Put the coverage mask at the end
    if(inputCoverage)
    {
      trace.inputs[maxReg + 1] = ShaderVariable("vCoverage", 0U, 0U, 0U, 0U);
      trace.inputs[maxReg + 1].columns = 1;
    }
  }

  // Add outputs to the shader state
  uint32_t specialOutputs = 0;
  maxReg = -1;
  for(size_t i = 0; i < numOutputs; i++)
  {
    if(dxbc->GetReflection()->OutputSig[i].regIndex == ~0U)
      specialOutputs++;
    else
      maxReg = RDCMAX(maxReg, (int32_t)dxbc->GetReflection()->OutputSig[i].regIndex);
  }

  if(maxReg >= 0 || specialOutputs > 0)
  {
    initialState.outputs.resize(maxReg + 1 + specialOutputs);
    for(size_t i = 0; i < numOutputs; i++)
    {
      const SigParameter &sig = dxbc->GetReflection()->OutputSig[i];

      if(sig.regIndex == ~0U)
        continue;

      ShaderVariable v;

      v.name = StringFormat::Fmt("o%d (%s)", sig.regIndex, sig.semanticIdxName.c_str());
      v.rows = 1;
      v.columns = sig.regChannelMask & 0x8 ? 4 : sig.regChannelMask & 0x4
                                                     ? 3
                                                     : sig.regChannelMask & 0x2
                                                           ? 2
                                                           : sig.regChannelMask & 0x1 ? 1 : 0;

      if(initialState.outputs[sig.regIndex].columns == 0)
        initialState.outputs[sig.regIndex] = v;
      else
        initialState.outputs[sig.regIndex].columns =
            RDCMAX(initialState.outputs[sig.regIndex].columns, v.columns);
    }

    int32_t outIdx = maxReg + 1;

    for(size_t i = 0; i < numOutputs; i++)
    {
      const SigParameter &sig = dxbc->GetReflection()->OutputSig[i];

      if(sig.regIndex != ~0U)
        continue;

      ShaderVariable v;

      if(sig.systemValue == ShaderBuiltin::OutputControlPointIndex)
        v.name = "vOutputControlPointID";
      else if(sig.systemValue == ShaderBuiltin::DepthOutput)
        v.name = "oDepth";
      else if(sig.systemValue == ShaderBuiltin::DepthOutputLessEqual)
        v.name = "oDepthLessEqual";
      else if(sig.systemValue == ShaderBuiltin::DepthOutputGreaterEqual)
        v.name = "oDepthGreaterEqual";
      else if(sig.systemValue == ShaderBuiltin::MSAACoverage)
        v.name = "oMask";
      else if(sig.systemValue == ShaderBuiltin::StencilReference)
        v.name = "oStencilRef";
      else
      {
        RDCERR("Unhandled output: %s (%d)", sig.semanticName.c_str(), sig.systemValue);
        continue;
      }

      v.rows = 1;
      v.columns = sig.regChannelMask & 0x8 ? 4 : sig.regChannelMask & 0x4
                                                     ? 3
                                                     : sig.regChannelMask & 0x2
                                                           ? 2
                                                           : sig.regChannelMask & 0x1 ? 1 : 0;

      initialState.outputs[outIdx++] = v;
    }
  }

  // Fill constant buffers and add them to the trace
  size_t numCBuffers = dxbc->GetReflection()->CBuffers.size();
  trace.constantBlocks.resize(numCBuffers);
  for(size_t i = 0; i < numCBuffers; i++)
  {
    rdcarray<ShaderVariable> vars;

    // Fetch cbuffers into vars, which will be 'natural': structs with members, non merged vectors
    StandardFillCBufferVariables(refl.resourceId, refl.constantBlocks[i].variables, vars,
                                 cbufData[dxbc->GetReflection()->CBuffers[i].reg]);

    FlattenVariables(refl.constantBlocks[i].variables, vars, trace.constantBlocks[i].members);

    for(size_t c = 0; c < trace.constantBlocks[i].members.size(); c++)
    {
      trace.constantBlocks[i].members[c].name =
          StringFormat::Fmt("cb%u[%u] (%s)", dxbc->GetReflection()->CBuffers[i].reg, (uint32_t)c,
                            trace.constantBlocks[i].members[c].name.c_str());
    }
  }

  initialState.Init();
}

bool PromptDebugTimeout(uint32_t cycleCounter)
{
  std::string msg = StringFormat::Fmt(
      "RenderDoc's shader debugging has been running for over %u cycles, which indicates either a "
      "very long-running loop, or possibly an infinite loop. Continuing could lead to extreme "
      "memory allocations, slow UI or even crashes. Would you like to abort debugging to see what "
      "has run so far?\n\n"
      "Hit yes to abort debugging. Note that loading the resulting trace could take several "
      "minutes.",
      cycleCounter);

  int ret = MessageBoxA(NULL, msg.c_str(), "Shader debugging timeout", MB_YESNO | MB_ICONWARNING);

  if(ret == IDYES)
    return true;

  return false;
}

void ApplyDerivatives(ShaderDebug::GlobalState &global, ShaderDebugTrace traces[4], int reg,
                      int element, int numWords, float *data, float signmul, int32_t quadIdxA,
                      int32_t quadIdxB)
{
  for(int w = 0; w < numWords; w++)
  {
    traces[quadIdxA].inputs[reg].value.fv[element + w] += signmul * data[w];
    if(quadIdxB >= 0)
      traces[quadIdxB].inputs[reg].value.fv[element + w] += signmul * data[w];
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
          it->second.value.fv[element + w] += data[w];
      }
    }
  }
}

void ApplyAllDerivatives(ShaderDebug::GlobalState &global, ShaderDebugTrace traces[4], int destIdx,
                         const std::vector<PSInputElement> &initialValues, float *data)
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
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddx_coarse, 1.0f, 1, 3);
      else if(destIdx == 1)
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddx_coarse, -1.0f, 0, 2);
      else if(destIdx == 2)
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddx_coarse, 1.0f, 1, -1);
      else if(destIdx == 3)
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
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
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddy_coarse, 1.0f, 2, 3);
      else if(destIdx == 1)
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddy_coarse, 1.0f, 2, -1);
      else if(destIdx == 2)
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
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
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddxfine, 1.0f, 3, -1);
      else if(destIdx == 3)
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
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
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddyfine, 1.0f, 3, -1);
      else if(destIdx == 3)
        ApplyDerivatives(global, traces, initialValues[i].reg, initialValues[i].elem,
                         initialValues[i].numwords, ddyfine, -1.0f, 0, 1);
    }

    ddyfine += initialValues[i].numwords;
  }
}

void FlattenSingleVariable(uint32_t byteOffset, const std::string &basename,
                           const ShaderVariable &v, rdcarray<ShaderVariable> &outvars)
{
  size_t outIdx = byteOffset / 16;
  size_t outComp = (byteOffset % 16) / 4;

  if(v.rowMajor)
    outvars.resize(RDCMAX(outIdx + v.rows, outvars.size()));
  else
    outvars.resize(RDCMAX(outIdx + v.columns, outvars.size()));

  if(!outvars[outIdx].name.empty())
  {
    // if we already have a variable in this slot, just append this variable to it. We should not
    // overlap into the next register as that's not allowed.
    outvars[outIdx].name = std::string(outvars[outIdx].name) + ", " + basename;
    outvars[outIdx].rows = 1;
    outvars[outIdx].isStruct = false;
    outvars[outIdx].columns += v.columns;

    RDCASSERT(outvars[outIdx].columns <= 4, outvars[outIdx].columns);

    memcpy(&outvars[outIdx].value.uv[outComp], &v.value.uv[0], sizeof(uint32_t) * v.columns);
  }
  else
  {
    const uint32_t numRegisters = v.rowMajor ? v.rows : v.columns;
    const char *regName = v.rowMajor ? "row" : "col";
    for(uint32_t reg = 0; reg < numRegisters; reg++)
    {
      if(numRegisters > 1)
        outvars[outIdx + reg].name = StringFormat::Fmt("%s.%s%u", basename.c_str(), regName, reg);
      else
        outvars[outIdx + reg].name = basename;
      outvars[outIdx + reg].rows = 1;
      outvars[outIdx + reg].type = v.type;
      outvars[outIdx + reg].isStruct = false;
      outvars[outIdx + reg].columns = v.columns;
      outvars[outIdx + reg].rowMajor = v.rowMajor;
    }

    if(v.rowMajor)
    {
      for(size_t ri = 0; ri < v.rows; ri++)
        memcpy(&outvars[outIdx + ri].value.uv[0], &v.value.uv[ri * v.columns],
               sizeof(uint32_t) * v.columns);
    }
    else
    {
      // if we have a matrix stored in column major order, we need to transpose it back so we can
      // unroll it into vectors.
      for(size_t ci = 0; ci < v.columns; ci++)
        for(size_t ri = 0; ri < v.rows; ri++)
          outvars[outIdx + ci].value.uv[ri] = v.value.uv[ri * v.columns + ci];
    }
  }
}

void FlattenVariables(const rdcarray<ShaderConstant> &constants,
                      const rdcarray<ShaderVariable> &invars, rdcarray<ShaderVariable> &outvars,
                      const std::string &prefix, uint32_t baseOffset)
{
  RDCASSERTEQUAL(constants.size(), invars.size());

  for(size_t i = 0; i < constants.size(); i++)
  {
    const ShaderConstant &c = constants[i];
    const ShaderVariable &v = invars[i];

    uint32_t byteOffset = baseOffset + c.byteOffset;

    std::string basename = prefix + std::string(v.name);

    if(!v.members.empty())
    {
      if(v.isStruct)
      {
        FlattenVariables(c.type.members, v.members, outvars, basename + ".", byteOffset);
      }
      else
      {
        if(c.type.members.empty())
        {
          // if there are no members in this type, it means it's a basic array - unroll directly

          for(int m = 0; m < v.members.count(); m++)
          {
            FlattenSingleVariable(byteOffset + m * c.type.descriptor.arrayByteStride,
                                  StringFormat::Fmt("%s[%zu]", basename.c_str(), m), v.members[m],
                                  outvars);
          }
        }
        else
        {
          // otherwise we recurse into each member and flatten

          for(int m = 0; m < v.members.count(); m++)
          {
            FlattenVariables(c.type.members, v.members[m].members, outvars,
                             StringFormat::Fmt("%s[%zu].", basename.c_str(), m),
                             byteOffset + m * c.type.descriptor.arrayByteStride);
          }
        }
      }

      continue;
    }

    FlattenSingleVariable(byteOffset, basename, v, outvars);
  }
}

void FlattenVariables(const rdcarray<ShaderConstant> &constants,
                      const rdcarray<ShaderVariable> &invars, rdcarray<ShaderVariable> &outvars)
{
  FlattenVariables(constants, invars, outvars, "", 0);
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
                                         uint32_t shaderRegister, GlobalState::ViewFmt &viewFmt)
{
  for(const DXBC::ShaderInputBind &bind : reflection.SRVs)
  {
    if(bind.reg == shaderRegister && bind.dimension == DXBC::ShaderInputBind::DIM_BUFFER &&
       bind.retType < DXBC::RETURN_TYPE_MIXED && bind.retType != DXBC::RETURN_TYPE_UNKNOWN)
    {
      viewFmt.byteWidth = 4;
      viewFmt.numComps = bind.numSamples;

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

void GatherPSInputDataForInitialValues(const DXBC::Reflection &psDxbc,
                                       const DXBC::Reflection &prevStageDxbc,
                                       std::vector<PSInputElement> &initialValues,
                                       std::vector<std::string> &floatInputs,
                                       std::vector<std::string> &inputVarNames,
                                       std::string &psInputDefinition, int &structureStride)
{
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
  std::vector<rdcpair<rdcstr, rdcpair<uint32_t, uint32_t>>> arrays;

  uint32_t nextreg = 0;

  size_t numInputs = psDxbc.InputSig.size();
  inputVarNames.resize(numInputs);

  for(size_t i = 0; i < numInputs; i++)
  {
    const SigParameter &sig = psDxbc.InputSig[i];

    psInputDefinition += "  ";

    bool included = true;

    // handled specially to account for SV_ ordering
    if(sig.systemValue == ShaderBuiltin::PrimitiveIndex ||
       sig.systemValue == ShaderBuiltin::MSAACoverage ||
       sig.systemValue == ShaderBuiltin::IsFrontFace ||
       sig.systemValue == ShaderBuiltin::MSAASampleIndex)
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

          if(prevStageDxbc.OutputSig[os].compType == CompType::Float)
            psInputDefinition += "float";
          else if(prevStageDxbc.OutputSig[os].compType == CompType::SInt)
            psInputDefinition += "int";
          else if(prevStageDxbc.OutputSig[os].compType == CompType::UInt)
            psInputDefinition += "uint";
          else
            RDCERR("Unexpected input signature type: %d", prevStageDxbc.OutputSig[os].compType);

          int numCols = (prevStageDxbc.OutputSig[os].regChannelMask & 0x1 ? 1 : 0) +
                        (prevStageDxbc.OutputSig[os].regChannelMask & 0x2 ? 1 : 0) +
                        (prevStageDxbc.OutputSig[os].regChannelMask & 0x4 ? 1 : 0) +
                        (prevStageDxbc.OutputSig[os].regChannelMask & 0x8 ? 1 : 0);

          structureStride += 4 * numCols;

          initialValues.push_back(PSInputElement(-1, 0, numCols, ShaderBuiltin::Undefined, true));

          std::string name = prevStageDxbc.OutputSig[os].semanticIdxName;

          psInputDefinition += ToStr((uint32_t)numCols) + " input_" + name + " : " + name + ";\n";
        }
      }

      if(!filled)
      {
        std::string dummy_reg = "dummy_register";
        dummy_reg += ToStr((uint32_t)nextreg + dummy);
        psInputDefinition += "float4 var_" + dummy_reg + " : semantic_" + dummy_reg + ";\n";

        initialValues.push_back(PSInputElement(-1, 0, 4, ShaderBuiltin::Undefined, true));

        structureStride += 4 * sizeof(float);
      }
    }

    nextreg = sig.regIndex + 1;

    if(sig.compType == CompType::Float)
    {
      // if we're packed with ints on either side, we must be nointerpolation
      bool nointerp = false;
      for(size_t j = 0; j < numInputs; j++)
      {
        if(sig.regIndex == psDxbc.InputSig[j].regIndex &&
           psDxbc.InputSig[j].compType != CompType::Float)
        {
          nointerp = true;
          break;
        }
      }

      if(nointerp)
        psInputDefinition += "nointerpolation ";

      psInputDefinition += "float";
    }
    else if(sig.compType == CompType::SInt)
      psInputDefinition += "nointerpolation int";
    else if(sig.compType == CompType::UInt)
      psInputDefinition += "nointerpolation uint";
    else
      RDCERR("Unexpected input signature type: %d", sig.compType);

    int numCols = (sig.regChannelMask & 0x1 ? 1 : 0) + (sig.regChannelMask & 0x2 ? 1 : 0) +
                  (sig.regChannelMask & 0x4 ? 1 : 0) + (sig.regChannelMask & 0x8 ? 1 : 0);

    std::string name = sig.semanticIdxName;

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
        // if we've found the 'next' semantic
        if(sig.semanticName == psDxbc.InputSig[j].semanticName &&
           nextIdx == psDxbc.InputSig[j].semanticIndex)
        {
          int jNumCols = (sig.regChannelMask & 0x1 ? 1 : 0) + (sig.regChannelMask & 0x2 ? 1 : 0) +
                         (sig.regChannelMask & 0x4 ? 1 : 0) + (sig.regChannelMask & 0x8 ? 1 : 0);

          // if it's the same size, and it's at the start of the next register
          if(jNumCols == numCols && psDxbc.InputSig[j].regChannelMask <= 0x3)
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
        arrays.push_back(make_rdcpair(sig.semanticName, make_rdcpair(sig.semanticIndex, nextIdx - 1)));
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
    // been packed into the previous register, but wasn't. float/float2 can be packed after an
    // array just fine.
    if(included && i > 0 && arrayLength == 0 && numCols <= 2 && sig.regChannelMask <= 0x3)
    {
      const SigParameter &prev = psDxbc.InputSig[i - 1];

      if(prev.regIndex != sig.regIndex && prev.compCount <= 2 && prev.regChannelMask <= 0x3)
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

    if(included && sig.compType == CompType::Float)
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

    int firstElem = sig.regChannelMask & 0x1 ? 0 : sig.regChannelMask & 0x2
                                                       ? 1
                                                       : sig.regChannelMask & 0x4
                                                             ? 2
                                                             : sig.regChannelMask & 0x8 ? 3 : -1;

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

};    // namespace ShaderDebug

#if ENABLED(ENABLE_UNIT_TESTS)

#include <limits>
#include "3rdparty/catch/catch.hpp"

using namespace ShaderDebug;

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
    CHECK(_isnan(dxbc_min(nan, nan)));
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
    CHECK(_isnan(dxbc_max(nan, nan)));
  };

  SECTION("sat/abs/neg on NaNs")
  {
    ShaderVariable v("a", b, nan, neginf, posinf);

    ShaderVariable v2 = sat(v, VarType::Float);

    CHECK(v2.value.f.x == 1.0f);
    CHECK(v2.value.f.y == 0.0f);
    CHECK(v2.value.f.z == 0.0f);
    CHECK(v2.value.f.w == 1.0f);

    v2 = neg(v, VarType::Float);

    CHECK(v2.value.f.x == -b);
    CHECK(_isnan(v2.value.f.y));
    CHECK(v2.value.f.z == posinf);
    CHECK(v2.value.f.w == neginf);

    v2 = abs(v, VarType::Float);

    CHECK(v2.value.f.x == b);
    CHECK(_isnan(v2.value.f.y));
    CHECK(v2.value.f.z == posinf);
    CHECK(v2.value.f.w == posinf);
  };

  SECTION("test denorm flushing")
  {
    float foo = 3.141f;

    // check normal values
    CHECK(flush_denorm(0.0f) == 0.0f);
    CHECK(flush_denorm(foo) == foo);
    CHECK(flush_denorm(-foo) == -foo);

    // check NaN/inf values
    CHECK(_isnan(flush_denorm(nan)));
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
