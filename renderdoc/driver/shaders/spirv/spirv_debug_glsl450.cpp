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
#include "maths/matrix.h"

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

ShaderVariable RoundEven(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
  {
    float x = var.value.fv[c];
    if(!isinf(x) && !isnan(x))
      var.value.fv[c] = x - remainderf(x, 1.0f);
  }

  return var;
}

ShaderVariable Round(ThreadState &state, const rdcarray<Id> &params)
{
  // for now do as the spec allows and implement this as RoundEven
  return RoundEven(state, params);
}

ShaderVariable Trunc(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = truncf(var.value.fv[c]);

  return var;
}

ShaderVariable FAbs(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = fabsf(var.value.fv[c]);

  return var;
}

ShaderVariable SAbs(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.iv[c] = abs(var.value.iv[c]);

  return var;
}

ShaderVariable FSign(ThreadState &state, const rdcarray<Id> &params)
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

ShaderVariable SSign(ThreadState &state, const rdcarray<Id> &params)
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

ShaderVariable Floor(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = floorf(var.value.fv[c]);

  return var;
}

ShaderVariable Ceil(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = ceilf(var.value.fv[c]);

  return var;
}

ShaderVariable Fract(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = var.value.fv[c] - floorf(var.value.fv[c]);

  return var;
}

static const float piOver180 = 3.14159265358979323846f / 180.0f;
static const float piUnder180 = 180.0f / 3.14159265358979323846f;

ShaderVariable Radians(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = var.value.fv[c] * piOver180;

  return var;
}

ShaderVariable Degrees(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = var.value.fv[c] * piUnder180;

  return var;
}

ShaderVariable Pow(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = powf(var.value.fv[c], y.value.fv[c]);

  return var;
}

ShaderVariable Determinant(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable m = state.GetSrc(params[0]);

  RDCASSERTEQUAL(m.rows, m.columns);

  if(m.rows == 4)
  {
    Matrix4f mat;
    mat.SetFrom(m.value.fv);
    m.value.f.x = mat.Determinant();
  }
  else if(m.rows == 3)
  {
    Matrix3f mat;
    mat.SetFrom(m.value.fv);
    m.value.f.x = mat.Determinant();
  }
  else if(m.rows == 2)
  {
    Matrix2f mat;
    mat.SetFrom(m.value.fv);
    m.value.f.x = mat.Determinant();
  }
  m.rows = m.columns = 1;

  return m;
}

ShaderVariable MatrixInverse(ThreadState &state, const rdcarray<Id> &params)
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

ShaderVariable FMax(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = GLSLMax(var.value.fv[c], y.value.fv[c]);

  return var;
}

ShaderVariable UMax(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.uv[c] = GLSLMax(var.value.uv[c], y.value.uv[c]);

  return var;
}

ShaderVariable SMax(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.iv[c] = GLSLMax(var.value.iv[c], y.value.iv[c]);

  return var;
}

ShaderVariable FMin(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = GLSLMin(var.value.fv[c], y.value.fv[c]);

  return var;
}

ShaderVariable UMin(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.uv[c] = GLSLMin(var.value.uv[c], y.value.uv[c]);

  return var;
}

ShaderVariable SMin(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.iv[c] = GLSLMin(var.value.iv[c], y.value.iv[c]);

  return var;
}

ShaderVariable FClamp(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable minVal = state.GetSrc(params[1]);
  ShaderVariable maxVal = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = GLSLMin(GLSLMax(var.value.fv[c], minVal.value.fv[c]), maxVal.value.fv[c]);

  return var;
}

ShaderVariable UClamp(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable minVal = state.GetSrc(params[1]);
  ShaderVariable maxVal = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.uv[c] = GLSLMin(GLSLMax(var.value.uv[c], minVal.value.uv[c]), maxVal.value.uv[c]);

  return var;
}

ShaderVariable SClamp(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable minVal = state.GetSrc(params[1]);
  ShaderVariable maxVal = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.iv[c] = GLSLMin(GLSLMax(var.value.iv[c], minVal.value.iv[c]), maxVal.value.iv[c]);

  return var;
}

ShaderVariable FMix(ThreadState &state, const rdcarray<Id> &params)
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

ShaderVariable Normalize(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  float sqrlength = 0.0f;

  for(uint32_t c = 0; c < var.columns; c++)
    sqrlength += (var.value.fv[c] * var.value.fv[c]);

  float invlength = 1.0f / sqrtf(sqrlength);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] *= invlength;

  return var;
}

ShaderVariable Cross(ThreadState &state, const rdcarray<Id> &params)
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

static float GLSLNMax(float x, float y)
{
  const bool xnan = isnan(x);
  const bool ynan = isnan(y);
  if(xnan && !ynan)
    return y;
  else if(!xnan && ynan)
    return x;
  else
    return x < y ? y : x;
}

static float GLSLNMin(float x, float y)
{
  const bool xnan = isnan(x);
  const bool ynan = isnan(y);
  if(xnan && !ynan)
    return y;
  else if(!xnan && ynan)
    return x;
  else
    return y < x ? y : x;
}

ShaderVariable NMin(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = GLSLNMin(var.value.fv[c], y.value.fv[c]);

  return var;
}

ShaderVariable NMax(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(2);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable y = state.GetSrc(params[1]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = GLSLNMax(var.value.fv[c], y.value.fv[c]);

  return var;
}

ShaderVariable NClamp(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(3);

  ShaderVariable var = state.GetSrc(params[0]);
  ShaderVariable minVal = state.GetSrc(params[1]);
  ShaderVariable maxVal = state.GetSrc(params[2]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = GLSLNMin(GLSLNMax(var.value.fv[c], minVal.value.fv[c]), maxVal.value.fv[c]);

  return var;
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
  EXT(Pow);
  EXT(Determinant);
  EXT(MatrixInverse);
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
  EXT(Cross);
  EXT(Normalize);
  EXT(NMin);
  EXT(NMax);
  EXT(NClamp);
}
};    // namespace rdcspv
