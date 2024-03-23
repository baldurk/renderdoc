/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

enum class ShaderType
{
  GLSL,
  GLSLES,
  Vulkan,
  GLSPIRV,
};

#include <functional>
#include "api/replay/rdcstr.h"

rdcstr GenerateGLSLShader(const rdcstr &shader, ShaderType type, int version,
                          const rdcstr &defines = "");

rdcstr InsertSnippetAfterVersion(ShaderType type, const char *source, int len, const char *snippet);

// for unit tests
struct ShaderReflection;
enum class ShaderStage : uint8_t;
using ReflectionMaker = std::function<void(ShaderStage stage, const rdcstr &source,
                                           const rdcstr &entryPoint, ShaderReflection &refl)>;
void TestGLSLReflection(ShaderType testType, ReflectionMaker compile);
