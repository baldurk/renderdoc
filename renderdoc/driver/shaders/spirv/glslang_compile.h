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
#include "spirv_compile.h"

namespace glslang
{
class TShader;
class TProgram;
};

glslang::TShader *CompileShaderForReflection(SPIRVShaderStage stage,
                                             const std::vector<std::string> &sources);
glslang::TProgram *LinkProgramForReflection(const std::vector<glslang::TShader *> &shaders);

struct TBuiltInResource;
extern TBuiltInResource *GetDefaultResources();

enum class ReflectionInterface
{
  Input,
  Output,
  Uniform,
  UniformBlock,
  ShaderStorageBlock,
  AtomicCounterBuffer,
  BufferVariable,
};

enum class ReflectionProperty
{
  ActiveResources,
  BufferBinding,
  TopLevelArrayStride,
  BlockIndex,
  ArraySize,
  IsRowMajor,
  NumActiveVariables,
  BufferDataSize,
  NameLength,
  Type,
  LocationComponent,
  ReferencedByVertexShader,
  ReferencedByTessControlShader,
  ReferencedByTessEvaluationShader,
  ReferencedByGeometryShader,
  ReferencedByFragmentShader,
  ReferencedByComputeShader,
  Internal_Binding,
  AtomicCounterBufferIndex,
  Offset,
  ArrayStride,
  MatrixStride,
  Location,
};

void glslangGetProgramInterfaceiv(glslang::TProgram *program, ReflectionInterface programInterface,
                                  ReflectionProperty pname, int32_t *params);

void glslangGetProgramResourceiv(glslang::TProgram *program, ReflectionInterface programInterface,
                                 uint32_t index, const std::vector<ReflectionProperty> &props,
                                 int32_t bufSize, int32_t *length, int32_t *params);
uint32_t glslangGetProgramResourceIndex(glslang::TProgram *program, const char *name);

const char *glslangGetProgramResourceName(glslang::TProgram *program,
                                          ReflectionInterface programInterface, uint32_t index);
