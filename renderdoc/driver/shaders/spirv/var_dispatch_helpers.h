/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Baldur Karlsson
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
  uint16_t *comp = &var.value.u16v[c];
  memcpy(&ret, &comp, sizeof(ret));
  return *ret;
}

template <>
inline float &comp<float>(ShaderVariable &var, uint32_t c)
{
  return var.value.fv[c];
}

template <>
inline double &comp<double>(ShaderVariable &var, uint32_t c)
{
  return var.value.dv[c];
}

template <typename T>
inline T comp(const ShaderVariable &val, uint32_t c);

template <>
inline half_float::half comp<half_float::half>(const ShaderVariable &var, uint32_t c)
{
  half_float::half ret;
  uint16_t comp = var.value.u16v[c];
  memcpy(&ret, &comp, sizeof(ret));
  return ret;
}

template <>
inline float comp<float>(const ShaderVariable &var, uint32_t c)
{
  return var.value.fv[c];
}

template <>
inline double comp<double>(const ShaderVariable &var, uint32_t c)
{
  return var.value.dv[c];
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
INT_COMP(uint32_t, uv);
INT_COMP(int32_t, iv);
INT_COMP(uint64_t, u64v);
INT_COMP(int64_t, s64v);

// helpers for when we just want the float value, up or downcasted
inline float floatComp(const ShaderVariable &var, uint32_t c)
{
  if(var.type == VarType::Float)
    return var.value.fv[c];
  else if(var.type == VarType::Half)
    return ConvertFromHalf(var.value.u16v[c]);
  else if(var.type == VarType::Double)
    return (float)var.value.dv[c];
  else
    return 0.0f;
}

inline uint32_t uintComp(const ShaderVariable &var, uint32_t c)
{
  uint32_t byteSize = VarTypeByteSize(var.type);
  if(byteSize == 4)
    return var.value.uv[c];
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
    return var.value.iv[c];
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
    var.value.fv[c] = f;
  else if(var.type == VarType::Half)
    var.value.u16v[c] = ConvertToHalf(f);
  else if(var.type == VarType::Double)
    var.value.dv[c] = f;
}

inline void setUintComp(ShaderVariable &var, uint32_t c, uint32_t u)
{
  uint32_t byteSize = VarTypeByteSize(var.type);
  if(byteSize == 4)
    var.value.uv[c] = u;
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
    var.value.iv[c] = i;
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
    result.value.fv[3] = 1.0f;
  else if(result.type == VarType::Half)
    result.value.u16v[3] = ConvertToHalf(1.0f);
  else if(result.type == VarType::Double)
    result.value.dv[3] = 1.0;
  else
    setUintComp(result, 3, 1);
}

inline void copyComp(ShaderVariable &dst, uint32_t dstComp, const ShaderVariable &src,
                     uint32_t srcComp, VarType type = VarType::Unknown)
{
  if(type == VarType::Unknown)
  {
    RDCASSERTEQUAL(dst.type, src.type);
    type = src.type;
  }
  const uint32_t sz = VarTypeByteSize(type);
  memcpy(((byte *)dst.value.u64v) + sz * dstComp, ((byte *)src.value.u64v) + sz * srcComp, sz);
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
