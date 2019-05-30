/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include <string>
#include <vector>

enum class SPIRVShaderStage
{
  Vertex,
  TessControl,
  TessEvaluation,
  Geometry,
  Fragment,
  Compute,
  Invalid,
};

enum class SPIRVSourceLanguage
{
  Unknown,
  OpenGLGLSL,
  VulkanGLSL,
  VulkanHLSL,
};

struct SPIRVCompilationSettings
{
  SPIRVCompilationSettings(SPIRVSourceLanguage l, SPIRVShaderStage s) : stage(s), lang(l) {}
  SPIRVCompilationSettings() = default;

  SPIRVShaderStage stage = SPIRVShaderStage::Invalid;
  SPIRVSourceLanguage lang = SPIRVSourceLanguage::Unknown;
  std::string entryPoint;
};

void InitSPIRVCompiler();
void ShutdownSPIRVCompiler();

std::string CompileSPIRV(const SPIRVCompilationSettings &settings,
                         const std::vector<std::string> &sources, std::vector<uint32_t> &spirv);