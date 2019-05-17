/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2019 Baldur Karlsson
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

#include "replay/replay_driver.h"
#include "gl_dispatch_table.h"

class WrappedOpenGL;

enum class FFVertexOutput : uint32_t
{
  // Core members of gl_PerVertex
  PointSize,
  First = PointSize,
  ClipDistance,

  // Compatibility implicit varyings, generally only comes back from glslang's reflection
  ClipVertex,
  FrontColor,
  BackColor,
  FrontSecondaryColor,
  BackSecondaryColor,
  TexCoord,
  FogFragCoord,
  Count,
};

DECLARE_REFLECTION_ENUM(FFVertexOutput);
ITERABLE_OPERATORS(FFVertexOutput);

struct FixedFunctionVertexOutputs
{
  bool used[arraydim<FFVertexOutput>()] = {};
};

int ParseVersionStatement(const char *version);
void MakeShaderReflection(GLenum shadType, GLuint sepProg, ShaderReflection &refl,
                          const FixedFunctionVertexOutputs &outputUsage);
GLuint MakeSeparableShaderProgram(WrappedOpenGL &drv, GLenum type, std::vector<std::string> sources,
                                  std::vector<std::string> *includepaths);
void CheckVertexOutputUses(const std::vector<std::string> &sources,
                           FixedFunctionVertexOutputs &outputUsage);
