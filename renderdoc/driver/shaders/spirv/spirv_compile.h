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

#pragma once

#include "api/replay/rdcarray.h"
#include "api/replay/rdcstr.h"

namespace rdcspv
{
enum class ShaderStage
{
  Vertex,
  TessControl,
  TessEvaluation,
  Geometry,
  Fragment,
  Compute,
  // gap of 6 for RT shaders
  Task = Compute + 7,
  Mesh,
  Invalid,
};

enum class InputLanguage
{
  Unknown,
  OpenGLGLSL,
  VulkanGLSL,
  VulkanHLSL,
};

struct CompilationSettings
{
  CompilationSettings(InputLanguage l, ShaderStage s) : stage(s), lang(l) {}
  CompilationSettings() = default;

  ShaderStage stage = ShaderStage::Invalid;
  InputLanguage lang = InputLanguage::Unknown;
  bool debugInfo = false;
  bool gles = false;
  rdcstr entryPoint;
};

void Init();
void Shutdown();

rdcstr Compile(const CompilationSettings &settings, const rdcarray<rdcstr> &sources,
               rdcarray<uint32_t> &spirv);

};    // namespace rdcspv
