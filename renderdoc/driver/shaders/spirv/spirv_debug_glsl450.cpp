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

#if defined(_MSC_VER)
#define finite _finite
#endif

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

ShaderVariable FAbs(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = fabs(var.value.fv[c]);

  return var;
}

ShaderVariable Floor(ThreadState &state, const rdcarray<Id> &params)
{
  CHECK_PARAMS(1);

  ShaderVariable var = state.GetSrc(params[0]);

  for(uint32_t c = 0; c < var.columns; c++)
    var.value.fv[c] = floor(var.value.fv[c]);

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

  float invlength = 1.0f / sqrt(sqrlength);

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
};    // namespace glsl

void ConfigureGLSLStd450(ExtInstDispatcher &extinst)
{
  extinst.names.resize((size_t)GLSLstd450::Max);
  for(size_t i = 0; i < extinst.names.size(); i++)
    extinst.names[i] = ToStr(GLSLstd450(i));

  extinst.functions.resize(extinst.names.size());

#define EXT(func) extinst.functions[(uint32_t)GLSLstd450::func] = &glsl::func;
  EXT(FAbs);
  EXT(Floor);
  EXT(Pow);
  EXT(FMix);
  EXT(Cross);
  EXT(Normalize);
}
};    // namespace rdcspv
