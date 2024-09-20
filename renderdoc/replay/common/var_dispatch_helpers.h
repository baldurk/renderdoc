/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#pragma once

#include "3rdparty/half/half.hpp"
#include "api/replay/replay_enums.h"
#include "maths/half_convert.h"

inline bool RDCISFINITE(half_float::half input)
{
  return half_float::isfinite(input);
}
inline bool RDCISNAN(half_float::half input)
{
  return half_float::isnan(input);
}
inline bool RDCISINF(half_float::half input)
{
  return half_float::isinf(input);
}

template <typename T>
inline T &comp(ShaderVariable &val, uint32_t c);

template <>
inline half_float::half &comp<half_float::half>(ShaderVariable &var, uint32_t c)
{
  half_float::half *ret;
  rdhalf *comp = &var.value.f16v[c];
  memcpy(&ret, &comp, sizeof(ret));
  return *ret;
}

template <>
inline float &comp<float>(ShaderVariable &var, uint32_t c)
{
  return var.value.f32v[c];
}

template <>
inline double &comp<double>(ShaderVariable &var, uint32_t c)
{
  return var.value.f64v[c];
}

template <typename T>
inline T comp(const ShaderVariable &val, uint32_t c);

template <>
inline half_float::half comp<half_float::half>(const ShaderVariable &var, uint32_t c)
{
  half_float::half ret;
  rdhalf comp = var.value.f16v[c];
  memcpy(&ret, &comp, sizeof(ret));
  return ret;
}

template <>
inline float comp<float>(const ShaderVariable &var, uint32_t c)
{
  return var.value.f32v[c];
}

template <>
inline double comp<double>(const ShaderVariable &var, uint32_t c)
{
  return var.value.f64v[c];
}

#define INT_COMP(type, member)                                  \
  template <>                                                   \
  inline type &comp<type>(ShaderVariable & var, uint32_t c)     \
  {                                                             \
    return var.value.member[c];                                 \
  }                                                             \
  template <>                                                   \
  inline type comp<type>(const ShaderVariable &var, uint32_t c) \
  {                                                             \
    return var.value.member[c];                                 \
  }

INT_COMP(uint8_t, u8v);
INT_COMP(int8_t, s8v);
INT_COMP(uint16_t, u16v);
INT_COMP(int16_t, s16v);
INT_COMP(uint32_t, u32v);
INT_COMP(int32_t, s32v);
INT_COMP(uint64_t, u64v);
INT_COMP(int64_t, s64v);

// helpers for when we just want the float value, up or downcasted
inline float floatComp(const ShaderVariable &var, uint32_t c)
{
  if(var.type == VarType::Float)
    return var.value.f32v[c];
  else if(var.type == VarType::Half)
    return (float)var.value.f16v[c];
  else if(var.type == VarType::Double)
    return (float)var.value.f64v[c];
  else
    return 0.0f;
}

inline uint32_t uintComp(const ShaderVariable &var, uint32_t c)
{
  uint32_t byteSize = VarTypeByteSize(var.type);
  if(byteSize == 4)
    return var.value.u32v[c];
  else if(byteSize == 2)
    return var.value.u16v[c];
  else if(byteSize == 8)
    return (uint32_t)var.value.u64v[c];
  else if(byteSize == 1)
    return var.value.u8v[c];
  else
    return 0;
}

inline int32_t intComp(const ShaderVariable &var, uint32_t c)
{
  uint32_t byteSize = VarTypeByteSize(var.type);
  if(byteSize == 4)
    return var.value.s32v[c];
  else if(byteSize == 2)
    return var.value.s16v[c];
  else if(byteSize == 8)
    return (int32_t)var.value.s64v[c];
  else if(byteSize == 1)
    return var.value.s8v[c];
  else
    return 0;
}

inline void setFloatComp(ShaderVariable &var, uint32_t c, float f)
{
  if(var.type == VarType::Float)
    var.value.f32v[c] = f;
  else if(var.type == VarType::Half)
    var.value.f16v[c].set(f);
  else if(var.type == VarType::Double)
    var.value.f64v[c] = f;
}

inline void setUintComp(ShaderVariable &var, uint32_t c, uint32_t u)
{
  uint32_t byteSize = VarTypeByteSize(var.type);
  if(byteSize == 4)
    var.value.u32v[c] = u;
  else if(byteSize == 2)
    var.value.u16v[c] = u & 0xffffu;
  else if(byteSize == 8)
    var.value.u64v[c] = u;
  else if(byteSize == 1)
    var.value.u8v[c] = u & 0xffu;
}

inline void setIntComp(ShaderVariable &var, uint32_t c, int32_t i)
{
  uint32_t byteSize = VarTypeByteSize(var.type);
  if(byteSize == 4)
    var.value.s32v[c] = i;
  else if(byteSize == 2)
    var.value.s16v[c] = (int16_t)i;
  else if(byteSize == 8)
    var.value.s64v[c] = i;
  else if(byteSize == 1)
    var.value.s8v[c] = (int8_t)i;
}

inline void set0001(ShaderVariable &result)
{
  RDCEraseEl(result.value);

  if(result.type == VarType::Float)
    result.value.f32v[3] = 1.0f;
  else if(result.type == VarType::Half)
    result.value.f16v[3].set(1.0f);
  else if(result.type == VarType::Double)
    result.value.f64v[3] = 1.0;
  else
    setUintComp(result, 3, 1);
}

inline void copyComp(ShaderVariable &dst, uint32_t dstComp, const ShaderVariable &src,
                     uint32_t srcComp)
{
  // fast path for same-sized inputs, which is common (e.g. float declared variables with float
  // inputs)
  if(dst.type == src.type)
  {
    const uint32_t sz = VarTypeByteSize(src.type);
    memcpy(((byte *)dst.value.u8v.data()) + sz * dstComp,
           ((byte *)src.value.u8v.data()) + sz * srcComp, sz);
    return;
  }
  else
  {
    // otherwise we convert the component here
    const uint32_t srcSz = VarTypeByteSize(src.type);
    const uint32_t dstSz = VarTypeByteSize(dst.type);

    if(srcSz <= 4 && dstSz <= 4)
    {
      // if the types are no more than 4-byte, we can use the helpers above without truncation
      if(VarTypeCompType(src.type) == CompType::Float)
        setFloatComp(dst, dstComp, floatComp(src, srcComp));
      else if(VarTypeCompType(src.type) == CompType::SInt)
        setIntComp(dst, dstComp, intComp(src, srcComp));
      else
        setUintComp(dst, dstComp, uintComp(src, srcComp));
    }
    else
    {
      // if there's a 64-bit type somewhere we need to go through double/int64
      double d = 0.0;
      uint64_t u = 0;
      int64_t i = 0;

      switch(src.type)
      {
        case VarType::Float:
        case VarType::Half:
        {
          d = floatComp(src, srcComp);
          break;
        }
        case VarType::Double:
        {
          d = src.value.f64v[srcComp];
          break;
        }
        case VarType::SInt:
        case VarType::SShort:
        case VarType::SByte:
        {
          i = intComp(src, srcComp);
          break;
        }
        case VarType::SLong:
        {
          i = src.value.s64v[srcComp];
          break;
        }
        case VarType::ULong:
        {
          u = src.value.u64v[srcComp];
          break;
        }
        default:
        {
          // all other case are uints or invalid types
          u = uintComp(src, srcComp);
          break;
        }
      }

      // valid SPIR-V should match the base type in any case where we're copying components,
      // conversions between are done separately. So we just assume that d/u/i was filled above and
      // read from it to the output
      switch(src.type)
      {
        case VarType::Float:
        case VarType::Half:
        {
          setFloatComp(dst, dstComp, float(d));
          break;
        }
        case VarType::Double:
        {
          dst.value.f64v[dstComp] = d;
          break;
        }
        case VarType::SInt:
        case VarType::SShort:
        case VarType::SByte:
        {
          setIntComp(dst, dstComp, int32_t(i));
          break;
        }
        case VarType::SLong:
        {
          dst.value.s64v[dstComp] = i;
          break;
        }
        case VarType::ULong:
        {
          dst.value.u64v[dstComp] = u;
          break;
        }
        default:
        {
          // all other case are uints or invalid types
          setUintComp(dst, dstComp, uint32_t(u));
          break;
        }
      }
    }
  }
}

#define IMPL_FOR_FLOAT_TYPES_FOR_TYPE(impl, type) \
  if(type == VarType::Float)                      \
  {                                               \
    impl(float);                                  \
  }                                               \
  else if(type == VarType::Half)                  \
  {                                               \
    impl(half_float::half);                       \
  }                                               \
  else if(type == VarType::Double)                \
  {                                               \
    impl(double);                                 \
  }

#define IMPL_FOR_INT_TYPES_FOR_TYPE(impl, type)           \
  if(type == VarType::UByte)                              \
  {                                                       \
    impl(uint8_t, int8_t, uint8_t);                       \
  }                                                       \
  else if(type == VarType::SByte)                         \
  {                                                       \
    impl(int8_t, int8_t, uint8_t);                        \
  }                                                       \
  else if(type == VarType::UShort)                        \
  {                                                       \
    impl(uint16_t, int16_t, uint16_t);                    \
  }                                                       \
  else if(type == VarType::SShort)                        \
  {                                                       \
    impl(int16_t, int16_t, uint16_t);                     \
  }                                                       \
  else if(type == VarType::UInt || type == VarType::Bool) \
  {                                                       \
    impl(uint32_t, int32_t, uint32_t);                    \
  }                                                       \
  else if(type == VarType::SInt)                          \
  {                                                       \
    impl(int32_t, int32_t, uint32_t);                     \
  }                                                       \
  else if(type == VarType::ULong)                         \
  {                                                       \
    impl(uint64_t, int64_t, uint64_t);                    \
  }                                                       \
  else if(type == VarType::SLong)                         \
  {                                                       \
    impl(int64_t, int64_t, uint64_t);                     \
  }

#define IMPL_FOR_FLOAT_TYPES(impl) IMPL_FOR_FLOAT_TYPES_FOR_TYPE(impl, var.type)
#define IMPL_FOR_INT_TYPES(impl) IMPL_FOR_INT_TYPES_FOR_TYPE(impl, var.type)
