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

#include "spirv_debug.h"
#include <math.h>
#include "maths/half_convert.h"
#include "maths/matrix.h"
#include "os/os_specific.h"

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
    float x = var.value.fv[c];
    if(RDCISFINITE(x))
      var.value.fv[c] = x - remainderf(x, 1.0f);
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
    var.value.fv[c] = truncf(var.value.fv[c]);

  return var;
}

ShaderVariable FAbs(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = fabsf(var.value.fv[c]);

  return var;
}

ShaderVariable SAbs(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.iv[c] = abs(var.value.iv[c]);

  return var;
}

ShaderVariable FSign(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
    if(var.value.fv[c] > 0.0f)
      var.value.fv[c] = 1.0f;
    else if(var.value.fv[c] < 0.0f)
      var.value.fv[c] = -1.0f;
  }

  return var;
}

ShaderVariable SSign(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
    if(var.value.iv[c] > 0)
      var.value.iv[c] = 1;
    else if(var.value.iv[c] < 0)
      var.value.iv[c] = -1;
    // 0 is left alone
  }

  return var;
}

ShaderVariable Floor(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = floorf(var.value.fv[c]);

  return var;
}

ShaderVariable Ceil(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = ceilf(var.value.fv[c]);

  return var;
}

ShaderVariable Fract(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = var.value.fv[c] - floorf(var.value.fv[c]);

  return var;
}

static const float piOver180 = 3.14159265358979323846f / 180.0f;
static const float piUnder180 = 180.0f / 3.14159265358979323846f;

ShaderVariable Radians(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = var.value.fv[c] * piOver180;

  return var;
}

ShaderVariable Degrees(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = var.value.fv[c] * piUnder180;

  return var;
}

ShaderVariable Determinant(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable m = state.GetSrc(params[0]);

  RDCASSERTEQUAL(m.rows, m.columns);

  if(m.rows == 4)
  {
    Matrix4f mat;
    mat.SetFrom(m.value.fv);
    m.value.fv[0] = mat.Determinant();
  }
  else if(m.rows == 3)
  {
    Matrix3f mat;
    mat.SetFrom(m.value.fv);
    m.value.fv[0] = mat.Determinant();
  }
  else if(m.rows == 2)
  {
    Matrix2f mat;
    mat.SetFrom(m.value.fv);
    m.value.fv[0] = mat.Determinant();
  }
  m.rows = m.columns = 1;

  return m;
}

ShaderVariable MatrixInverse(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable m = state.GetSrc(params[0]);

  RDCASSERTEQUAL(m.rows, m.columns);

  if(m.rows == 4)
  {
    Matrix4f mat;
    mat.SetFrom(m.value.fv);
    memcpy(m.value.fv, mat.Inverse().Data(), sizeof(mat));
  }
  else if(m.rows == 3)
  {
    Matrix3f mat;
    mat.SetFrom(m.value.fv);
    memcpy(m.value.fv, mat.Inverse().Data(), sizeof(mat));
  }
  else if(m.rows == 2)
  {
    Matrix2f mat;
    mat.SetFrom(m.value.fv);
    memcpy(m.value.fv, mat.Inverse().Data(), sizeof(mat));
  }

  return m;
}

ShaderVariable Modf(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable x = state.GetSrc(params[0]);
  Id iptr = params[1];

  ShaderVariable whole = x;

  for(uint32_t c = 0; c < x.columns; c++)
    x.value.fv[c] = modff(x.value.fv[c], &whole.value.fv[c]);

  state.WritePointerValue(iptr, whole);

  return x;
}

ShaderVariable ModfStruct(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable x = state.GetSrc(params[0]);

  ShaderVariable ret;
  ret.rows = 1;
  ret.columns = 1;
  ret.isStruct = true;
  ret.members = {x, x};
  ret.members[0].name = "_child0";
  ret.members[1].name = "_child1";

  for(uint32_t c = 0; c < x.columns; c++)
    ret.members[0].value.fv[c] = modff(x.value.fv[c], &ret.members[1].value.fv[c]);

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

ShaderVariable FMax(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
    if(var.type == VarType::Double)
      var.value.dv[c] = GLSLMax(var.value.dv[c], y.value.dv[c]);
    else
      var.value.fv[c] = GLSLMax(var.value.fv[c], y.value.fv[c]);
  }

  return var;
}

ShaderVariable UMax(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.uv[c] = GLSLMax(var.value.uv[c], y.value.uv[c]);

  return var;
}

ShaderVariable SMax(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.iv[c] = GLSLMax(var.value.iv[c], y.value.iv[c]);

  return var;
}

ShaderVariable FMin(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
    if(var.type == VarType::Double)
      var.value.dv[c] = GLSLMin(var.value.dv[c], y.value.dv[c]);
    else
      var.value.fv[c] = GLSLMin(var.value.fv[c], y.value.fv[c]);
  }

  return var;
}

ShaderVariable UMin(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.uv[c] = GLSLMin(var.value.uv[c], y.value.uv[c]);

  return var;
}

ShaderVariable SMin(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.iv[c] = GLSLMin(var.value.iv[c], y.value.iv[c]);

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
    if(var.type == VarType::Double)
      var.value.dv[c] = GLSLMin(GLSLMax(var.value.dv[c], minVal.value.dv[c]), maxVal.value.dv[c]);
    else
      var.value.fv[c] = GLSLMin(GLSLMax(var.value.fv[c], minVal.value.fv[c]), maxVal.value.fv[c]);
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
    var.value.uv[c] = GLSLMin(GLSLMax(var.value.uv[c], minVal.value.uv[c]), maxVal.value.uv[c]);

  return var;
}

ShaderVariable SClamp(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable minVal = state.GetSrc(params[1]);
  ShaderVariable maxVal = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.iv[c] = GLSLMin(GLSLMax(var.value.iv[c], minVal.value.iv[c]), maxVal.value.iv[c]);

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
    float xf = var.value.fv[c];
    float yf = y.value.fv[c];
    float af = a.value.fv[c];

    var.value.fv[c] = xf * (1 - af) + yf * af;
  }

  return var;
}

ShaderVariable Step(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable edge = state.GetSrc(params[0]);
  ShaderVariable x = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < x.columns; c++)
    x.value.fv[c] = x.value.fv[c] < edge.value.fv[c] ? 0.0f : 1.0f;

  return x;
}

ShaderVariable SmoothStep(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable edge0 = state.GetSrc(params[0]);
  ShaderVariable edge1 = state.GetSrc(params[1]);
  ShaderVariable x = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < x.columns; c++)
  {
    if(x.type == VarType::Double)
    {
      const double edge0f = edge0.value.dv[c];
      const double edge1f = edge1.value.dv[c];
      const double xf = x.value.dv[c];

      const double t = GLSLMin(GLSLMax((xf - edge0f) / (edge1f - edge0f), 0.0), 1.0);

      x.value.dv[c] = t * t * (3 - 2 * t);
    }
    else
    {
      const float edge0f = edge0.value.fv[c];
      const float edge1f = edge1.value.fv[c];
      const float xf = x.value.fv[c];

      const float t = GLSLMin(GLSLMax((xf - edge0f) / (edge1f - edge0f), 0.0f), 1.0f);

      x.value.fv[c] = t * t * (3 - 2 * t);
    }
  }

  return x;
}

ShaderVariable Frexp(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable x = state.GetSrc(params[0]);
  Id iptr = params[1];

  ShaderVariable whole = x;

  for(uint32_t c = 0; c < x.columns; c++)
    x.value.fv[c] = frexpf(x.value.fv[c], &whole.value.iv[c]);

  state.WritePointerValue(iptr, whole);

  return x;
}

ShaderVariable FrexpStruct(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable x = state.GetSrc(params[0]);

  ShaderVariable ret;
  ret.rows = 1;
  ret.columns = 1;
  ret.isStruct = true;
  ret.members = {x, x};
  ret.members[0].name = "_child0";
  ret.members[1].name = "_child1";

  for(uint32_t c = 0; c < x.columns; c++)
    ret.members[0].value.fv[c] = frexpf(x.value.fv[c], &ret.members[1].value.iv[c]);

  return ret;
}

ShaderVariable Ldexp(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable x = state.GetSrc(params[0]);
  ShaderVariable exp = state.GetSrc(params[1]);

  for(uint8_t c = 0; c < x.columns; c++)
    x.value.fv[c] = ldexpf(x.value.fv[c], exp.value.iv[c]);

  return x;
}

ShaderVariable PackSnorm4x8(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  packed |= (int32_t(RDCCLAMP(v.value.fv[0], -1.0f, 1.0f) * 127.0f) & 0xff) << 0;
  packed |= (int32_t(RDCCLAMP(v.value.fv[1], -1.0f, 1.0f) * 127.0f) & 0xff) << 8;
  packed |= (int32_t(RDCCLAMP(v.value.fv[2], -1.0f, 1.0f) * 127.0f) & 0xff) << 16;
  packed |= (int32_t(RDCCLAMP(v.value.fv[3], -1.0f, 1.0f) * 127.0f) & 0xff) << 24;

  v.value.uv[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackUnorm4x8(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  packed |= (uint32_t(RDCCLAMP(v.value.fv[0], 0.0f, 1.0f) * 255.0f) & 0xff) << 0;
  packed |= (uint32_t(RDCCLAMP(v.value.fv[1], 0.0f, 1.0f) * 255.0f) & 0xff) << 8;
  packed |= (uint32_t(RDCCLAMP(v.value.fv[2], 0.0f, 1.0f) * 255.0f) & 0xff) << 16;
  packed |= (uint32_t(RDCCLAMP(v.value.fv[3], 0.0f, 1.0f) * 255.0f) & 0xff) << 24;

  v.value.uv[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackSnorm2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  packed |= (int32_t(RDCCLAMP(v.value.fv[0], -1.0f, 1.0f) * 32767.0f) & 0xffff) << 0;
  packed |= (int32_t(RDCCLAMP(v.value.fv[1], -1.0f, 1.0f) * 32767.0f) & 0xffff) << 16;

  v.value.uv[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackUnorm2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  packed |= (uint32_t(RDCCLAMP(v.value.fv[0], 0.0f, 1.0f) * 65535.0f) & 0xffff) << 0;
  packed |= (uint32_t(RDCCLAMP(v.value.fv[1], 0.0f, 1.0f) * 65535.0f) & 0xffff) << 16;

  v.value.uv[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackHalf2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = 0;

  packed |= ConvertToHalf(v.value.fv[0]) << 0;
  packed |= ConvertToHalf(v.value.fv[1]) << 16;

  v.value.uv[0] = packed;

  v.type = VarType::UInt;
  v.columns = 1;

  return v;
}

ShaderVariable PackDouble2x32(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  // u64 is aliased with the double, so we just OR together
  v.value.u64v[0] = uint64_t(v.value.uv[0]) | (uint64_t(v.value.uv[1]) << 32);

  v.type = VarType::Double;
  v.columns = 1;

  return v;
}

ShaderVariable UnpackSnorm4x8(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.uv[0];

  v.value.fv[0] = RDCCLAMP(float(int8_t((packed >> 0) & 0xff)) / 127.0f, -1.0f, 1.0f);
  v.value.fv[1] = RDCCLAMP(float(int8_t((packed >> 8) & 0xff)) / 127.0f, -1.0f, 1.0f);
  v.value.fv[2] = RDCCLAMP(float(int8_t((packed >> 16) & 0xff)) / 127.0f, -1.0f, 1.0f);
  v.value.fv[3] = RDCCLAMP(float(int8_t((packed >> 24) & 0xff)) / 127.0f, -1.0f, 1.0f);

  v.type = VarType::Float;
  v.columns = 4;

  return v;
}

ShaderVariable UnpackUnorm4x8(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.uv[0];

  v.value.fv[0] = float((packed >> 0) & 0xff) / 255.0f;
  v.value.fv[1] = float((packed >> 8) & 0xff) / 255.0f;
  v.value.fv[2] = float((packed >> 16) & 0xff) / 255.0f;
  v.value.fv[3] = float((packed >> 24) & 0xff) / 255.0f;

  v.type = VarType::Float;
  v.columns = 4;

  return v;
}

ShaderVariable UnpackSnorm2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.uv[0];

  v.value.fv[0] = RDCCLAMP(float(int16_t((packed >> 0) & 0xffff)) / 32767.0f, -1.0f, 1.0f);
  v.value.fv[1] = RDCCLAMP(float(int16_t((packed >> 16) & 0xffff)) / 32767.0f, -1.0f, 1.0f);

  v.type = VarType::Float;
  v.columns = 2;

  return v;
}

ShaderVariable UnpackUnorm2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.uv[0];

  v.value.fv[0] = float((packed >> 0) & 0xffff) / 65535.0f;
  v.value.fv[1] = float((packed >> 16) & 0xffff) / 65535.0f;

  v.type = VarType::Float;
  v.columns = 2;

  return v;
}

ShaderVariable UnpackHalf2x16(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable v = state.GetSrc(params[0]);

  uint32_t packed = v.value.uv[0];

  v.value.fv[0] = ConvertFromHalf((packed >> 0) & 0xffff);
  v.value.fv[1] = ConvertFromHalf((packed >> 16) & 0xffff);

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
  v.value.uv[0] = (doubleUint >> 0) & 0xFFFFFFFFU;
  v.value.uv[1] = (doubleUint >> 32) & 0xFFFFFFFFU;

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

  var.value.fv[0] = x.value.fv[1] * y.value.fv[2] - y.value.fv[1] * x.value.fv[2];
  var.value.fv[1] = x.value.fv[2] * y.value.fv[0] - y.value.fv[2] * x.value.fv[0];
  var.value.fv[2] = x.value.fv[0] * y.value.fv[1] - y.value.fv[0] * x.value.fv[1];

  return var;
}

ShaderVariable FaceForward(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable N = state.GetSrc(params[0]);
  ShaderVariable I = state.GetSrc(params[1]);
  ShaderVariable Nref = state.GetSrc(params[2]);

  float dot = 0;
  for(uint8_t c = 0; c < Nref.columns; c++)
    dot += Nref.value.fv[c] * I.value.fv[c];

  if(dot >= 0.0f)
  {
    for(uint8_t c = 0; c < Nref.columns; c++)
      N.value.fv[c] = -N.value.fv[c];
  }

  return N;
}

ShaderVariable Reflect(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable I = state.GetSrc(params[0]);
  ShaderVariable N = state.GetSrc(params[1]);

  float dot = 0;
  for(uint8_t c = 0; c < N.columns; c++)
    dot += N.value.fv[c] * I.value.fv[c];

  for(uint8_t c = 0; c < N.columns; c++)
    N.value.fv[c] = I.value.fv[c] - 2.0f * dot * N.value.fv[c];

  return N;
}

ShaderVariable FindILsb(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable x = state.GetSrc(params[0]);

  for(uint8_t c = 0; c < x.columns; c++)
    x.value.iv[c] = x.value.uv[c] == 0 ? -1 : Bits::CountTrailingZeroes(x.value.uv[c]);

  return x;
}

ShaderVariable FindSMsb(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable x = state.GetSrc(params[0]);

  for(uint8_t c = 0; c < x.columns; c++)
  {
    if(x.value.iv[c] == 0 || x.value.iv[c] == -1)
      x.value.iv[c] = -1;
    else if(x.value.iv[c] >= 0)
      x.value.uv[c] = 31 - Bits::CountLeadingZeroes(x.value.uv[c]);
    else
      x.value.uv[c] = 31 - Bits::CountLeadingZeroes(~x.value.uv[c]);
  }

  return x;
}

ShaderVariable FindUMsb(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable x = state.GetSrc(params[0]);

  for(uint8_t c = 0; c < x.columns; c++)
  {
    x.value.iv[c] = x.value.iv[c] == 0 ? -1 : 31 - Bits::CountLeadingZeroes(x.value.uv[c]);
  }

  return x;
}

ShaderVariable NMin(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = GLSLMin(var.value.fv[c], y.value.fv[c]);

  return var;
}

ShaderVariable NMax(ThreadState &state, uint32_t, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
    if(var.type == VarType::Double)
      var.value.dv[c] = GLSLMax(var.value.dv[c], y.value.dv[c]);
    else
      var.value.fv[c] = GLSLMax(var.value.fv[c], y.value.fv[c]);
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
    if(var.type == VarType::Double)
      var.value.dv[c] = GLSLMin(GLSLMax(var.value.dv[c], minVal.value.dv[c]), maxVal.value.dv[c]);
    else
      var.value.fv[c] = GLSLMin(GLSLMax(var.value.fv[c], minVal.value.fv[c]), maxVal.value.fv[c]);
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
    memset(ret.value.u64v, 0, sizeof(ret.value.u64v));

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
