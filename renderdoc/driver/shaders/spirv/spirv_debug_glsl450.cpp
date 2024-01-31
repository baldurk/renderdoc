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

#include "spirv_debug.h"
#include <math.h>
#include "maths/half_convert.h"
#include "maths/matrix.h"
#include "os/os_specific.h"
#include "var_dispatch_helpers.h"

// add some overloads we'll use to avoid the mess of math function definitions across compilers and
// C/C++
inline int8_t RDCABS(int8_t v)
{
  return v < 0 ? -v : v;
}
inline int16_t RDCABS(int16_t v)
{
  return v < 0 ? -v : v;
}
inline int32_t RDCABS(int32_t v)
{
  return v < 0 ? -v : v;
}
inline int64_t RDCABS(int64_t v)
{
  return v < 0 ? -v : v;
}

inline float RDCMODF(float a, float *b)
{
  return modff(a, b);
}
inline double RDCMODF(double a, double *b)
{
  return modf(a, b);
}
inline half_float::half RDCMODF(half_float::half a, half_float::half *b)
{
  return half_float::modf(a, b);
}

namespace rdcspv
{
namespace glsl
{
#define CHECK_PARAMS(n)                                                               \
  if(params.size() != n)                                                              \
  {                                                                                   \
    RDCERR("Unexpected number of parameters (%zu) to %s, expected %u", params.size(), \
           __PRETTY_FUNCTION_SIGNATURE__, n);                                         \
    return ShaderVariable();                                                          \
  }

ShaderVariable RoundEven(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T)         \
  T x = comp<T>(var, c); \
  if(RDCISFINITE(x))     \
    comp<T>(var, c) = x - remainder(x, (T)1.0);

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable Round(ThreadState &state, uint32_t instruction, const rdcarray<Id> &params)
{
  // for now do as the spec allows and implement this as RoundEven
  return RoundEven(state, instruction, params);
}

ShaderVariable Trunc(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = trunc(comp<T>(var, c))

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable FAbs(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = fabs(comp<T>(var, c))

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable SAbs(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(var, c) = RDCABS(comp<S>(var, c))

    IMPL_FOR_INT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable FSign(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T)           \
  T val = comp<T>(var, c); \
  if(val > 0.0)            \
    comp<T>(var, c) = 1.0; \
  else if(val < 0.0)       \
    comp<T>(var, c) = -1.0;

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable SSign(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U)     \
  S val = comp<S>(var, c); \
  if(val > 0)              \
    comp<S>(var, c) = 1;   \
  else if(val < 0)         \
    comp<S>(var, c) = -1;

    IMPL_FOR_INT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable Floor(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = floor(comp<T>(var, c))

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable Ceil(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = ceil(comp<T>(var, c))

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable Fract(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = comp<T>(var, c) - floor(comp<T>(var, c))

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

static const float piOver180 = 3.14159265358979323846f / 180.0f;
static const float piUnder180 = 180.0f / 3.14159265358979323846f;

ShaderVariable Radians(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = comp<T>(var, c) * piOver180;

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable Degrees(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = comp<T>(var, c) * piUnder180;

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable Determinant(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable m = var;

  if(var.type != VarType::Float)
  {
    static bool warned = false;
    if(!warned)
    {
      warned = true;
      RDCLOG("Calculating determinant at float precision instead of %s", ToStr(m.type).c_str());
    }

    for(uint8_t c = 0; c < m.rows * m.columns; c++)
      m.value.f32v[c] = floatComp(var, c);
  }

  RDCASSERTEQUAL(m.rows, m.columns);

  if(m.rows == 4)
  {
    Matrix4f mat;
    mat.SetFrom(m.value.f32v.data());
    m.value.f32v[0] = mat.Determinant();
  }
  else if(m.rows == 3)
  {
    Matrix3f mat;
    mat.SetFrom(m.value.f32v.data());
    m.value.f32v[0] = mat.Determinant();
  }
  else if(m.rows == 2)
  {
    Matrix2f mat;
    mat.SetFrom(m.value.f32v.data());
    m.value.f32v[0] = mat.Determinant();
  }
  m.rows = m.columns = 1;

  if(var.type != VarType::Float)
  {
    float f = m.value.f32v[0];
    setFloatComp(m, 0, f);
  }

  return m;
}

ShaderVariable MatrixInverse(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable m = var;

  if(var.type != VarType::Float)
  {
    static bool warned = false;
    if(!warned)
    {
      warned = true;
      RDCLOG("Calculating matrix inverse at float precision instead of %s", ToStr(m.type).c_str());
    }

    for(uint8_t c = 0; c < m.rows * m.columns; c++)
      m.value.f32v[c] = floatComp(var, c);
  }

  RDCASSERTEQUAL(m.rows, m.columns);

  if(m.rows == 4)
  {
    Matrix4f mat;
    mat.SetFrom(m.value.f32v.data());
    memcpy(m.value.f32v.data(), mat.Inverse().Data(), sizeof(mat));
  }
  else if(m.rows == 3)
  {
    Matrix3f mat;
    mat.SetFrom(m.value.f32v.data());
    memcpy(m.value.f32v.data(), mat.Inverse().Data(), sizeof(mat));
  }
  else if(m.rows == 2)
  {
    Matrix2f mat;
    mat.SetFrom(m.value.f32v.data());
    memcpy(m.value.f32v.data(), mat.Inverse().Data(), sizeof(mat));
  }

  if(var.type != VarType::Float)
  {
    var = m;
    for(uint8_t c = 0; c < m.rows * m.columns; c++)
      setFloatComp(m, c, var.value.f32v[c]);
  }

  return m;
}

ShaderVariable Modf(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  Id iptr = params[1];

  ShaderVariable whole = var;

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = RDCMODF(comp<T>(var, c), &comp<T>(whole, c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  state.WritePointerValue(iptr, whole);

  return var;
}

ShaderVariable ModfStruct(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  ShaderVariable ret;
  ret.rows = 1;
  ret.columns = 1;
  ret.type = VarType::Struct;
  ret.members = {var, var};
  ret.members[0].name = "_child0";
  ret.members[1].name = "_child1";

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(ret.members[0], c) = RDCMODF(comp<T>(var, c), &comp<T>(ret.members[1], c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return ret;
}

template <typename T>
static T GLSLMax(T x, T y)
{
  return x < y ? y : x;
}

template <typename T>
static T GLSLMin(T x, T y)
{
  return y < x ? y : x;
}

template <>
float GLSLMax(float x, float y)
{
  const bool xnan = RDCISNAN(x);
  const bool ynan = RDCISNAN(y);
  if(xnan && !ynan)
    return y;
  else if(!xnan && ynan)
    return x;
  else
    return x < y ? y : x;
}

template <>
float GLSLMin(float x, float y)
{
  const bool xnan = RDCISNAN(x);
  const bool ynan = RDCISNAN(y);
  if(xnan && !ynan)
    return y;
  else if(!xnan && ynan)
    return x;
  else
    return y < x ? y : x;
}

template <>
double GLSLMax(double x, double y)
{
  const bool xnan = RDCISNAN(x);
  const bool ynan = RDCISNAN(y);
  if(xnan && !ynan)
    return y;
  else if(!xnan && ynan)
    return x;
  else
    return x < y ? y : x;
}

template <>
double GLSLMin(double x, double y)
{
  const bool xnan = RDCISNAN(x);
  const bool ynan = RDCISNAN(y);
  if(xnan && !ynan)
    return y;
  else if(!xnan && ynan)
    return x;
  else
    return y < x ? y : x;
}

template <>
half_float::half GLSLMax(half_float::half x, half_float::half y)
{
  const bool xnan = RDCISNAN(x);
  const bool ynan = RDCISNAN(y);
  if(xnan && !ynan)
    return y;
  else if(!xnan && ynan)
    return x;
  else
    return x < y ? y : x;
}

template <>
half_float::half GLSLMin(half_float::half x, half_float::half y)
{
  const bool xnan = RDCISNAN(x);
  const bool ynan = RDCISNAN(y);
  if(xnan && !ynan)
    return y;
  else if(!xnan && ynan)
    return x;
  else
    return y < x ? y : x;
}

ShaderVariable FMax(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = GLSLMax(comp<T>(var, c), comp<T>(y, c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable UMax(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = GLSLMax(comp<U>(var, c), comp<U>(y, c));

    IMPL_FOR_INT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable SMax(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(var, c) = GLSLMax(comp<S>(var, c), comp<S>(y, c));

    IMPL_FOR_INT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable FMin(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = GLSLMin(comp<T>(var, c), comp<T>(y, c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable UMin(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = GLSLMin(comp<U>(var, c), comp<U>(y, c));

    IMPL_FOR_INT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable SMin(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(var, c) = GLSLMin(comp<S>(var, c), comp<S>(y, c));

    IMPL_FOR_INT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable FClamp(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable minVal = state.GetSrc(params[1]);
  ShaderVariable maxVal = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) \
  comp<T>(var, c) = GLSLMin(GLSLMax(comp<T>(var, c), comp<T>(minVal, c)), comp<T>(maxVal, c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable UClamp(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable minVal = state.GetSrc(params[1]);
  ShaderVariable maxVal = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U) \
  comp<U>(var, c) = GLSLMin(GLSLMax(comp<U>(var, c), comp<U>(minVal, c)), comp<U>(maxVal, c));

    IMPL_FOR_INT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable SClamp(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable minVal = state.GetSrc(params[1]);
  ShaderVariable maxVal = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U) \
  comp<S>(var, c) = GLSLMin(GLSLMax(comp<S>(var, c), comp<S>(minVal, c)), comp<S>(maxVal, c));

    IMPL_FOR_INT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable FMix(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);
  ShaderVariable a = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T)          \
  T xf = comp<T>(var, c); \
  T yf = comp<T>(y, c);   \
  T af = comp<T>(a, c);   \
                          \
  comp<T>(var, c) = xf * (1 - af) + yf * af;

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable Step(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable edge = state.GetSrc(params[0]);
  ShaderVariable var = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = comp<T>(var, c) < comp<T>(edge, c) ? T(0.0) : T(1.0);

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable SmoothStep(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable edge0 = state.GetSrc(params[0]);
  ShaderVariable edge1 = state.GetSrc(params[1]);
  ShaderVariable var = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T)                                                             \
  T edge0f = comp<T>(edge0, c);                                              \
  T edge1f = comp<T>(edge1, c);                                              \
  T xf = comp<T>(var, c);                                                    \
                                                                             \
  T t = GLSLMin(GLSLMax((xf - edge0f) / (edge1f - edge0f), T(0.0)), T(1.0)); \
                                                                             \
  comp<T>(var, c) = t * t * (T(3.0) - T(2.0) * t);

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable Frexp(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  Id iptr = params[1];

  ShaderVariable whole = var;

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = frexp(comp<T>(var, c), &comp<int32_t>(whole, c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  state.WritePointerValue(iptr, whole);

  return var;
}

ShaderVariable FrexpStruct(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  ShaderVariable ret;
  ret.rows = 1;
  ret.columns = 1;
  ret.type = VarType::Struct;
  ret.members = {var, var};
  ret.members[0].name = "_child0";
  ret.members[1].name = "_child1";
  // member 1 must be a scalar or vector with integer type, and 32-bit width
  ret.members[1].type = VarType::SInt;

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) \
  comp<T>(ret.members[0], c) = frexp(comp<T>(var, c), &comp<int32_t>(ret.members[1], c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return ret;
}

ShaderVariable Ldexp(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable exp = state.GetSrc(params[1]);

  for(uint8_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = ldexp(comp<T>(var, c), comp<int32_t>(exp, c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable PackSnorm4x8(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  // The v operand must be a vector of 4 components whose type is a 32-bit floating-point.
  packed |= (int32_t(RDCCLAMP(v.value.f32v[0], -1.0f, 1.0f) * 127.0f) & 0xff) << 0;
  packed |= (int32_t(RDCCLAMP(v.value.f32v[1], -1.0f, 1.0f) * 127.0f) & 0xff) << 8;
  packed |= (int32_t(RDCCLAMP(v.value.f32v[2], -1.0f, 1.0f) * 127.0f) & 0xff) << 16;
  packed |= (int32_t(RDCCLAMP(v.value.f32v[3], -1.0f, 1.0f) * 127.0f) & 0xff) << 24;

  v.value.u32v[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackUnorm4x8(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  // The v operand must be a vector of 4 components whose type is a 32-bit floating-point.
  packed |= (uint32_t(RDCCLAMP(v.value.f32v[0], 0.0f, 1.0f) * 255.0f) & 0xff) << 0;
  packed |= (uint32_t(RDCCLAMP(v.value.f32v[1], 0.0f, 1.0f) * 255.0f) & 0xff) << 8;
  packed |= (uint32_t(RDCCLAMP(v.value.f32v[2], 0.0f, 1.0f) * 255.0f) & 0xff) << 16;
  packed |= (uint32_t(RDCCLAMP(v.value.f32v[3], 0.0f, 1.0f) * 255.0f) & 0xff) << 24;

  v.value.u32v[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackSnorm2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  // The v operand must be a vector of 4 components whose type is a 32-bit floating-point.
  packed |= (int32_t(RDCCLAMP(v.value.f32v[0], -1.0f, 1.0f) * 32767.0f) & 0xffff) << 0;
  packed |= (int32_t(RDCCLAMP(v.value.f32v[1], -1.0f, 1.0f) * 32767.0f) & 0xffff) << 16;

  v.value.u32v[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackUnorm2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  // The v operand must be a vector of 4 components whose type is a 32-bit floating-point.
  packed |= (uint32_t(RDCCLAMP(v.value.f32v[0], 0.0f, 1.0f) * 65535.0f) & 0xffff) << 0;
  packed |= (uint32_t(RDCCLAMP(v.value.f32v[1], 0.0f, 1.0f) * 65535.0f) & 0xffff) << 16;

  v.value.u32v[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackHalf2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  // The v operand must be a vector of 4 components whose type is a 32-bit floating-point.
  packed |= ConvertToHalf(v.value.f32v[0]) << 0;
  packed |= ConvertToHalf(v.value.f32v[1]) << 16;

  v.value.u32v[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackDouble2x32(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  // u64 is aliased with the double, so we just OR together
  v.value.u64v[0] = uint64_t(v.value.u32v[0]) | (uint64_t(v.value.u32v[1]) << 32);

  v.type = VarType::Double;
  v.columns = 1;

  return v;
}

ShaderVariable UnpackSnorm4x8(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.u32v[0];

  // The v operand must be a vector of 4 components whose type is a 32-bit floating-point.
  v.value.f32v[0] = RDCCLAMP(float(int8_t((packed >> 0) & 0xff)) / 127.0f, -1.0f, 1.0f);
  v.value.f32v[1] = RDCCLAMP(float(int8_t((packed >> 8) & 0xff)) / 127.0f, -1.0f, 1.0f);
  v.value.f32v[2] = RDCCLAMP(float(int8_t((packed >> 16) & 0xff)) / 127.0f, -1.0f, 1.0f);
  v.value.f32v[3] = RDCCLAMP(float(int8_t((packed >> 24) & 0xff)) / 127.0f, -1.0f, 1.0f);

  v.type = VarType::Float;
  v.columns = 4;

  return v;
}

ShaderVariable UnpackUnorm4x8(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.u32v[0];

  v.value.f32v[0] = float((packed >> 0) & 0xff) / 255.0f;
  v.value.f32v[1] = float((packed >> 8) & 0xff) / 255.0f;
  v.value.f32v[2] = float((packed >> 16) & 0xff) / 255.0f;
  v.value.f32v[3] = float((packed >> 24) & 0xff) / 255.0f;

  v.type = VarType::Float;
  v.columns = 4;

  return v;
}

ShaderVariable UnpackSnorm2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.u32v[0];

  v.value.f32v[0] = RDCCLAMP(float(int16_t((packed >> 0) & 0xffff)) / 32767.0f, -1.0f, 1.0f);
  v.value.f32v[1] = RDCCLAMP(float(int16_t((packed >> 16) & 0xffff)) / 32767.0f, -1.0f, 1.0f);

  v.type = VarType::Float;
  v.columns = 2;

  return v;
}

ShaderVariable UnpackUnorm2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.u32v[0];

  v.value.f32v[0] = float((packed >> 0) & 0xffff) / 65535.0f;
  v.value.f32v[1] = float((packed >> 16) & 0xffff) / 65535.0f;

  v.type = VarType::Float;
  v.columns = 2;

  return v;
}

ShaderVariable UnpackHalf2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.u32v[0];

  v.value.f32v[0] = ConvertFromHalf((packed >> 0) & 0xffff);
  v.value.f32v[1] = ConvertFromHalf((packed >> 16) & 0xffff);

  v.type = VarType::Float;
  v.columns = 2;

  return v;
}

ShaderVariable UnpackDouble2x32(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  // u64 is aliased with the double, so we just OR together
  uint64_t doubleUint = v.value.u64v[0];
  v.value.u32v[0] = (doubleUint >> 0) & 0xFFFFFFFFU;
  v.value.u32v[1] = (doubleUint >> 32) & 0xFFFFFFFFU;

  v.type = VarType::UInt;
  v.columns = 2;

  return v;
}

ShaderVariable Cross(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable x = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  RDCASSERT(x.columns == 3 && y.columns == 3, x.columns, y.columns);

  ShaderVariable var = x;

#undef _IMPL
#define _IMPL(T)                                                                   \
  comp<T>(var, 0) = comp<T>(x, 1) * comp<T>(y, 2) - comp<T>(y, 1) * comp<T>(x, 2); \
  comp<T>(var, 1) = comp<T>(x, 2) * comp<T>(y, 0) - comp<T>(y, 2) * comp<T>(x, 0); \
  comp<T>(var, 2) = comp<T>(x, 0) * comp<T>(y, 1) - comp<T>(y, 0) * comp<T>(x, 1);

  IMPL_FOR_FLOAT_TYPES(_IMPL);

  return var;
}

ShaderVariable FaceForward(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable N = state.GetSrc(params[0]);
  ShaderVariable I = state.GetSrc(params[1]);
  ShaderVariable Nref = state.GetSrc(params[2]);

  ShaderVariable var = N;

#undef _IMPL
#define _IMPL(T)                             \
  T dot(0.0);                                \
  for(uint8_t c = 0; c < var.columns; c++)   \
    dot += comp<T>(Nref, c) * comp<T>(I, c); \
  if(dot >= 0.0)                             \
  {                                          \
    for(uint8_t c = 0; c < var.columns; c++) \
      comp<T>(var, c) = -comp<T>(N, c);      \
  }

  IMPL_FOR_FLOAT_TYPES(_IMPL);

  return var;
}

ShaderVariable Reflect(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable I = state.GetSrc(params[0]);
  ShaderVariable N = state.GetSrc(params[1]);

  ShaderVariable var = N;

#undef _IMPL
#define _IMPL(T)                           \
  T dot(0.0);                              \
  for(uint8_t c = 0; c < var.columns; c++) \
    dot += comp<T>(N, c) * comp<T>(I, c);  \
                                           \
  for(uint8_t c = 0; c < var.columns; c++) \
    comp<T>(var, c) = comp<T>(I, c) - T(2.0) * dot * comp<T>(N, c);

  IMPL_FOR_FLOAT_TYPES(_IMPL);

  return var;
}

ShaderVariable FindILsb(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable x = state.GetSrc(params[0]);

  // This instruction is currently limited to 32-bit width components.
  for(uint8_t c = 0; c < x.columns; c++)
    x.value.s32v[c] = x.value.u32v[c] == 0 ? -1 : Bits::CountTrailingZeroes(x.value.u32v[c]);

  return x;
}

ShaderVariable FindSMsb(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable x = state.GetSrc(params[0]);

  // This instruction is currently limited to 32-bit width components.
  for(uint8_t c = 0; c < x.columns; c++)
  {
    if(x.value.s32v[c] == 0 || x.value.s32v[c] == -1)
      x.value.s32v[c] = -1;
    else if(x.value.s32v[c] >= 0)
      x.value.u32v[c] = 31 - Bits::CountLeadingZeroes(x.value.u32v[c]);
    else
      x.value.u32v[c] = 31 - Bits::CountLeadingZeroes(~x.value.u32v[c]);
  }

  return x;
}

ShaderVariable FindUMsb(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable x = state.GetSrc(params[0]);

  // This instruction is currently limited to 32-bit width components.
  for(uint8_t c = 0; c < x.columns; c++)
  {
    x.value.s32v[c] = x.value.s32v[c] == 0 ? -1 : 31 - Bits::CountLeadingZeroes(x.value.u32v[c]);
  }

  return x;
}

ShaderVariable NMin(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = GLSLMin(comp<T>(var, c), comp<T>(y, c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable NMax(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = GLSLMax(comp<T>(var, c), comp<T>(y, c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable NClamp(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable minVal = state.GetSrc(params[1]);
  ShaderVariable maxVal = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) \
  comp<T>(var, c) = GLSLMin(GLSLMax(comp<T>(var, c), comp<T>(minVal, c)), comp<T>(maxVal, c));

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
}

ShaderVariable GPUOp(ThreadState &state, uint32_t instruction, const rdcarray<Id> &params)
{
  rdcarray<ShaderVariable> paramVars;
  for(Id id : params)
    paramVars.push_back(state.GetSrc(id));

  ShaderVariable ret = paramVars[0];

  if(!state.debugger.GetAPIWrapper()->CalculateMathOp(state, (GLSLstd450)instruction, paramVars, ret))
    memset(&ret.value, 0, sizeof(ret.value));

  return ret;
}

};    // namespace glsl

void ConfigureGLSLStd450(ExtInstDispatcher &extinst)
{
  extinst.names.resize((size_t)GLSLstd450::Max);
  for(size_t i = 0; i < extinst.names.size(); i++)
    extinst.names[i] = ToStr(GLSLstd450(i));

  extinst.functions.resize(extinst.names.size());

#define EXT(func)                                              \
  extinst.functions[(uint32_t)GLSLstd450::func] = &glsl::func; \
  uint32_t noduplicate##func;                                  \
  (void)noduplicate##func;
  EXT(Round);
  EXT(RoundEven);
  EXT(Trunc);
  EXT(FAbs);
  EXT(SAbs);
  EXT(FSign);
  EXT(SSign);
  EXT(Floor);
  EXT(Ceil);
  EXT(Fract);
  EXT(Radians);
  EXT(Degrees);
  EXT(Determinant);
  EXT(MatrixInverse);
  EXT(Modf);
  EXT(ModfStruct);
  EXT(FMin);
  EXT(UMin);
  EXT(SMin);
  EXT(FMax);
  EXT(UMax);
  EXT(SMax);
  EXT(FClamp);
  EXT(UClamp);
  EXT(SClamp);
  EXT(FMix);
  EXT(Step);
  EXT(SmoothStep);
  EXT(Frexp);
  EXT(FrexpStruct);
  EXT(Ldexp);
  EXT(PackSnorm4x8);
  EXT(PackUnorm4x8);
  EXT(PackSnorm2x16);
  EXT(PackUnorm2x16);
  EXT(PackHalf2x16);
  EXT(PackDouble2x32);
  EXT(UnpackSnorm2x16);
  EXT(UnpackUnorm2x16);
  EXT(UnpackHalf2x16);
  EXT(UnpackSnorm4x8);
  EXT(UnpackUnorm4x8);
  EXT(UnpackDouble2x32);
  EXT(Cross);
  EXT(FaceForward);
  EXT(Reflect);
  EXT(FindILsb);
  EXT(FindSMsb);
  EXT(FindUMsb);
  EXT(NMin);
  EXT(NMax);
  EXT(NClamp);

// transcendentals and other operations that will likely be less accurate on GPU we run on the GPU
// to be more faithful to the real execution
#define GPU_EXT(func)                                           \
  extinst.functions[(uint32_t)GLSLstd450::func] = &glsl::GPUOp; \
  uint32_t noduplicate##func;                                   \
  (void)noduplicate##func;
  GPU_EXT(Sin)
  GPU_EXT(Cos)
  GPU_EXT(Tan)
  GPU_EXT(Asin)
  GPU_EXT(Acos)
  GPU_EXT(Atan)
  GPU_EXT(Sinh)
  GPU_EXT(Cosh)
  GPU_EXT(Tanh)
  GPU_EXT(Asinh)
  GPU_EXT(Acosh)
  GPU_EXT(Atanh)
  GPU_EXT(Atan2)
  GPU_EXT(Pow)
  GPU_EXT(Exp)
  GPU_EXT(Log)
  GPU_EXT(Exp2)
  GPU_EXT(Log2)
  GPU_EXT(Sqrt)
  GPU_EXT(InverseSqrt)
  GPU_EXT(Fma)
  GPU_EXT(Length);
  GPU_EXT(Distance);
  GPU_EXT(Normalize);
  GPU_EXT(Refract);
}
};    // namespace rdcspv
