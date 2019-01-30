/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "spirv_common.h"
#include "common/common.h"
#include "maths/formatpacking.h"
#include "replay/replay_driver.h"

#undef min
#undef max

#include "3rdparty/glslang/glslang/Include/Types.h"
#include "3rdparty/glslang/glslang/Public/ShaderLang.h"

static bool inited = false;
std::vector<glslang::TShader *> allocatedShaders;
std::vector<glslang::TProgram *> allocatedPrograms;

void InitSPIRVCompiler()
{
  if(!inited)
  {
    glslang::InitializeProcess();
    inited = true;
  }
}

void ShutdownSPIRVCompiler()
{
  if(inited)
  {
    // programs must be deleted before shaders
    for(glslang::TProgram *program : allocatedPrograms)
      delete program;

    for(glslang::TShader *shader : allocatedShaders)
      delete shader;

    allocatedPrograms.clear();
    allocatedShaders.clear();

    glslang::FinalizeProcess();
  }
}

void FillSpecConstantVariables(const rdcarray<ShaderConstant> &invars,
                               rdcarray<ShaderVariable> &outvars,
                               const std::vector<SpecConstant> &specInfo)
{
  StandardFillCBufferVariables(invars, outvars, bytebuf());

  RDCASSERTEQUAL(invars.size(), outvars.size());

  for(size_t v = 0; v < invars.size() && v < outvars.size(); v++)
  {
    outvars[v].value.uv[0] = (invars[v].defaultValue & 0xFFFFFFFF);
    outvars[v].value.uv[1] = ((invars[v].defaultValue >> 32) & 0xFFFFFFFF);
  }

  // find any actual values specified
  for(size_t i = 0; i < specInfo.size(); i++)
  {
    for(size_t v = 0; v < invars.size() && v < outvars.size(); v++)
    {
      if(specInfo[i].specID == invars[v].byteOffset)
      {
        memcpy(outvars[v].value.uv, specInfo[i].data.data(),
               RDCMIN(specInfo[i].data.size(), sizeof(outvars[v].value.uv)));
      }
    }
  }
}

void glslangGetProgramInterfaceiv(glslang::TProgram *program, ReflectionInterface programInterface,
                                  ReflectionProperty pname, int32_t *params)
{
  *params = 0;

  if(pname == ReflectionProperty::ActiveResources)
  {
    switch(programInterface)
    {
      case ReflectionInterface::Input: *params = program->getNumPipeInputs(); break;
      case ReflectionInterface::Output: *params = program->getNumPipeOutputs(); break;
      case ReflectionInterface::Uniform: *params = program->getNumUniformVariables(); break;
      case ReflectionInterface::UniformBlock: *params = program->getNumUniformBlocks(); break;
      case ReflectionInterface::BufferVariable: *params = program->getNumBufferVariables(); break;
      case ReflectionInterface::ShaderStorageBlock: *params = program->getNumBufferBlocks(); break;
      case ReflectionInterface::AtomicCounterBuffer:
        *params = program->getNumAtomicCounters();
        break;
    }
  }
  else
  {
    RDCERR("Unsupported reflection property %d", pname);
  }
}

void glslangGetProgramResourceiv(glslang::TProgram *program, ReflectionInterface programInterface,
                                 uint32_t index, const std::vector<ReflectionProperty> &props,
                                 int32_t bufSize, int32_t *length, int32_t *params)
{
  // all of our properties are single-element values, so we just loop up to buffer size or number of
  // properties, whichever comes first.
  for(size_t i = 0; i < RDCMIN((size_t)bufSize, props.size()); i++)
  {
    switch(props[i])
    {
      case ReflectionProperty::ActiveResources:
        RDCERR("Unhandled reflection property ActiveResources");
        params[i] = 0;
        break;
      case ReflectionProperty::BufferBinding:
      {
        if(programInterface == ReflectionInterface::UniformBlock)
          params[i] = program->getUniformBlock(index).getBinding();
        else if(programInterface == ReflectionInterface::ShaderStorageBlock)
          params[i] = program->getBufferBlock(index).getBinding();
        else
          RDCERR("Unsupported interface for BufferBinding query");
        break;
      }
      case ReflectionProperty::BlockIndex:
      {
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniform(index).index;
        else if(programInterface == ReflectionInterface::BufferVariable)
          params[i] = program->getBufferVariable(index).index;
        else
          RDCERR("Unsupported interface for BlockIndex query");
        break;
      }
      case ReflectionProperty::ArraySize:
      {
        if(programInterface == ReflectionInterface::Uniform)
        {
          params[i] = program->getUniform(index).size;
        }
        else if(programInterface == ReflectionInterface::BufferVariable)
        {
          params[i] = program->getBufferVariable(index).size;
        }
        else if(programInterface == ReflectionInterface::Input)
        {
          const glslang::TType *type = program->getPipeInput(index).getType();
          if(type->isArray())
            params[i] = type->getOuterArraySize();
          else
            params[i] = 1;
        }
        else if(programInterface == ReflectionInterface::Output)
        {
          const glslang::TType *type = program->getPipeOutput(index).getType();
          if(type->isArray())
            params[i] = type->getOuterArraySize();
          else
            params[i] = 1;
        }
        else
        {
          RDCERR("Unsupported interface for ArraySize query");
        }
        break;
      }
      case ReflectionProperty::IsRowMajor:
      {
        const glslang::TType *ttype = NULL;

        if(programInterface == ReflectionInterface::Uniform)
          ttype = program->getUniform(index).getType();
        else if(programInterface == ReflectionInterface::BufferVariable)
          ttype = program->getBufferVariable(index).getType();
        else
          RDCERR("Unsupported interface for RowMajor query");

        if(ttype)
          params[i] = (ttype->getQualifier().layoutMatrix == glslang::ElmRowMajor);
        else
          params[i] = 0;
        break;
      }
      case ReflectionProperty::MatrixStride:
      {
        // From documentation of std140:
        //
        // 5. "If the member is a column-major matrix with C columns and R rows, the matrix is
        // stored identically to an array of C column vectors with R components each, according to
        // rule (4)."
        // 7. "If the member is a row-major matrix with C columns and R rows, the matrix is stored
        // identically to an array of R row vectors with C components each, according to rule (4)."
        //
        // So in std140 the matrix stride is always at least 16-bytes unless the matrix is doubles.
        // In std430, because the rule (4) array alignment is relaxed, it can be less.
        if(programInterface == ReflectionInterface::Uniform)
        {
          params[i] = 16;
        }
        else if(programInterface == ReflectionInterface::BufferVariable)
        {
          const glslang::TType *ttype = program->getBufferVariable(index).getType();

          if(ttype->getQualifier().layoutMatrix == glslang::ElmRowMajor)
            params[i] = ttype->getMatrixCols() * sizeof(float);
          else
            params[i] = ttype->getMatrixRows() * sizeof(float);
        }
        else
        {
          RDCERR("Unsupported interface for RowMajor query");
        }

        break;
      }
      case ReflectionProperty::NumActiveVariables:
      {
        if(programInterface == ReflectionInterface::UniformBlock)
          params[i] = program->getUniformBlock(index).numMembers;
        else if(programInterface == ReflectionInterface::ShaderStorageBlock)
          params[i] = program->getBufferBlock(index).numMembers;
        else
          RDCERR("Unsupported interface for NumActiveVariables query");
        break;
      }
      case ReflectionProperty::BufferDataSize:
        RDCASSERT(programInterface == ReflectionInterface::UniformBlock);
        params[i] = program->getUniformBlock(index).size;
        break;
      case ReflectionProperty::NameLength:
      {
        // The name length includes a terminating null character.
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = (int32_t)program->getUniform(index).name.size() + 1;
        else if(programInterface == ReflectionInterface::BufferVariable)
          params[i] = (int32_t)program->getBufferVariable(index).name.size() + 1;
        else if(programInterface == ReflectionInterface::UniformBlock)
          params[i] = (int32_t)program->getUniformBlock(index).name.size() + 1;
        else if(programInterface == ReflectionInterface::Input)
          params[i] = (int32_t)program->getPipeInput(index).name.size() + 1;
        else if(programInterface == ReflectionInterface::Output)
          params[i] = (int32_t)program->getPipeOutput(index).name.size() + 1;
        else if(programInterface == ReflectionInterface::AtomicCounterBuffer)
          params[i] = (int32_t)program->getAtomicCounter(index).name.size() + 1;
        else if(programInterface == ReflectionInterface::ShaderStorageBlock)
          params[i] = (int32_t)program->getBufferBlock(index).name.size() + 1;
        else
          RDCERR("Unsupported interface for NameLength query");
        break;
      }
      case ReflectionProperty::Type:
      {
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniform(index).glDefineType;
        else if(programInterface == ReflectionInterface::BufferVariable)
          params[i] = program->getBufferVariable(index).glDefineType;
        else if(programInterface == ReflectionInterface::Input)
          params[i] = program->getPipeInput(index).glDefineType;
        else if(programInterface == ReflectionInterface::Output)
          params[i] = program->getPipeOutput(index).glDefineType;
        else
          RDCERR("Unsupported interface for Type query");

        if(params[i] == 0)
          params[i] = 0x1406;    // GL_FLOAT

        break;
      }
      case ReflectionProperty::LocationComponent:
      {
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniform(index).getType()->getQualifier().layoutComponent;
        else if(programInterface == ReflectionInterface::BufferVariable)
          params[i] = program->getBufferVariable(index).getType()->getQualifier().layoutComponent;
        else if(programInterface == ReflectionInterface::Input)
          params[i] = program->getPipeInput(index).getType()->getQualifier().layoutComponent;
        else if(programInterface == ReflectionInterface::Output)
          params[i] = program->getPipeOutput(index).getType()->getQualifier().layoutComponent;
        else
          RDCERR("Unsupported interface for LocationComponent query");

        if(params[i] == glslang::TQualifier::layoutComponentEnd)
          params[i] = 0;
        break;
      }
      case ReflectionProperty::ReferencedByVertexShader:
      case ReflectionProperty::ReferencedByTessControlShader:
      case ReflectionProperty::ReferencedByTessEvaluationShader:
      case ReflectionProperty::ReferencedByGeometryShader:
      case ReflectionProperty::ReferencedByFragmentShader:
      case ReflectionProperty::ReferencedByComputeShader:
      {
        EShLanguageMask mask = {};
        switch(props[i])
        {
          case ReflectionProperty::ReferencedByVertexShader: mask = EShLangVertexMask; break;
          case ReflectionProperty::ReferencedByTessControlShader:
            mask = EShLangTessControlMask;
            break;
          case ReflectionProperty::ReferencedByTessEvaluationShader:
            mask = EShLangTessEvaluationMask;
            break;
          case ReflectionProperty::ReferencedByGeometryShader: mask = EShLangGeometryMask; break;
          case ReflectionProperty::ReferencedByFragmentShader: mask = EShLangFragmentMask; break;
          case ReflectionProperty::ReferencedByComputeShader: mask = EShLangComputeMask; break;
          default: break;
        }

        if(programInterface == ReflectionInterface::Uniform)
          params[i] = (program->getUniform(index).stages & mask) != 0;
        else if(programInterface == ReflectionInterface::UniformBlock)
          params[i] = (program->getUniformBlock(index).stages & mask) != 0;
        else if(programInterface == ReflectionInterface::BufferVariable)
          params[i] = (program->getBufferVariable(index).stages & mask) != 0;
        else if(programInterface == ReflectionInterface::ShaderStorageBlock)
          params[i] = (program->getBufferBlock(index).stages & mask) != 0;
        else if(programInterface == ReflectionInterface::Input)
          params[i] = (program->getPipeInput(index).stages & mask) != 0;
        else if(programInterface == ReflectionInterface::Output)
          params[i] = (program->getPipeOutput(index).stages & mask) != 0;
        else if(programInterface == ReflectionInterface::AtomicCounterBuffer)
          params[i] = (program->getAtomicCounter(index).stages & mask) != 0;
        else
          RDCERR("Unexpected interface being queried for referenced-by");

        break;
      }
      case ReflectionProperty::Internal_Binding:
      {
        if(programInterface == ReflectionInterface::UniformBlock)
        {
          params[i] = program->getUniformBlock(index).getType()->getQualifier().layoutBinding;
          break;
        }

        // deliberate fall-through
      }
      case ReflectionProperty::AtomicCounterBufferIndex:
      {
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniform(index).getType()->getQualifier().layoutBinding;
        else if(programInterface == ReflectionInterface::AtomicCounterBuffer)
          params[i] = program->getAtomicCounter(index).getType()->getQualifier().layoutBinding;
        else
          RDCERR("Unexpected interface being queried for AtomicCounterBufferIndex");
        break;
      }
      case ReflectionProperty::Offset:
      {
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniform(index).offset;
        else if(programInterface == ReflectionInterface::BufferVariable)
          params[i] = program->getBufferVariable(index).offset;
        else
          RDCERR("Unsupported interface for Offset query");
        break;
      }
      case ReflectionProperty::TopLevelArrayStride:
      {
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniform(index).topLevelArrayStride;
        else if(programInterface == ReflectionInterface::BufferVariable)
          params[i] = program->getBufferVariable(index).topLevelArrayStride;
        else
          RDCERR("Unsupported interface for ArrayStride query");
        break;
      }
      case ReflectionProperty::ArrayStride:
      {
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniform(index).arrayStride;
        else if(programInterface == ReflectionInterface::BufferVariable)
          params[i] = program->getBufferVariable(index).arrayStride;
        else
          RDCERR("Unsupported interface for ArrayStride query");
        break;
      }
      case ReflectionProperty::Location:
      {
        // want to query the actual implementation for bare uniform locations, which is handled
        // elsewhere. So we always return either -1 for uniforms that don't have a location (i.e.
        // are in a block) or 0 for bare uniforms
        if(programInterface == ReflectionInterface::Uniform)
        {
          params[i] = program->getUniform(index).index >= 0 ? -1 : 0;
        }
        // for program inputs/outputs for a vertex/fragment shader respectively, we want to do the
        // same as above and always query when possible, however for fragment inputs e.g. we want to
        // keep the locations that might be present in the shader. So we do the reverse - return -1
        // when it's a vertex input to force a query, and otherwise return the layout set.
        else if(programInterface == ReflectionInterface::Input)
        {
          params[i] = program->getPipeInput(index).getType()->getQualifier().layoutLocation;

          if(params[i] == glslang::TQualifier::layoutLocationEnd)
            params[i] = -1;

          if(program->getPipeInput(index).stages == EShLangVertexMask)
            params[i] = -1;
        }
        else if(programInterface == ReflectionInterface::Output)
        {
          params[i] = program->getPipeOutput(index).getType()->getQualifier().layoutLocation;

          if(params[i] == glslang::TQualifier::layoutLocationEnd)
            params[i] = -1;

          if(program->getPipeOutput(index).stages == EShLangFragmentMask)
            params[i] = -1;
        }
        break;
      }
    }
  }
}

uint32_t glslangGetProgramResourceIndex(glslang::TProgram *program, const char *name)
{
  uint32_t idx = program->getReflectionIndex(name);

  // Additionally, if <name> would exactly match the name string of an active
  // resource if "[0]" were appended to <name>, the index of the matched
  // resource is returned.
  if(idx == ~0U)
  {
    std::string arraysuffixed = name;
    arraysuffixed += "[0]";
    idx = program->getReflectionIndex(arraysuffixed.c_str());
  }

  return idx;
}

const char *glslangGetProgramResourceName(glslang::TProgram *program,
                                          ReflectionInterface programInterface, uint32_t index)
{
  const char *fetchedName = "";

  switch(programInterface)
  {
    case ReflectionInterface::Input: fetchedName = program->getPipeInput(index).name.c_str(); break;
    case ReflectionInterface::Output:
      fetchedName = program->getPipeOutput(index).name.c_str();
      break;
    case ReflectionInterface::Uniform: fetchedName = program->getUniform(index).name.c_str(); break;
    case ReflectionInterface::UniformBlock:
      fetchedName = program->getUniformBlock(index).name.c_str();
      break;
    case ReflectionInterface::BufferVariable:
      fetchedName = program->getBufferVariable(index).name.c_str();
      break;
    case ReflectionInterface::ShaderStorageBlock:
      fetchedName = program->getBufferBlock(index).name.c_str();
      break;
    case ReflectionInterface::AtomicCounterBuffer:
      fetchedName = program->getAtomicCounter(index).name.c_str();
      break;
  }

  return fetchedName;
}
