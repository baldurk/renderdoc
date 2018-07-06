/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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

#undef min
#undef max

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

void SPIRVFillCBufferVariables(const rdcarray<ShaderConstant> &invars,
                               vector<ShaderVariable> &outvars, const bytebuf &data,
                               size_t baseOffset)
{
  for(size_t v = 0; v < invars.size(); v++)
  {
    std::string basename = invars[v].name;

    uint32_t rows = invars[v].type.descriptor.rows;
    uint32_t cols = invars[v].type.descriptor.columns;
    uint32_t elems = RDCMAX(1U, invars[v].type.descriptor.elements);
    bool rowMajor = invars[v].type.descriptor.rowMajorStorage != 0;
    bool isArray = elems > 1;

    size_t dataOffset =
        baseOffset + invars[v].reg.vec * sizeof(float) * 4 + invars[v].reg.comp * sizeof(float);

    if(!invars[v].type.members.empty() || (rows == 0 && cols == 0))
    {
      ShaderVariable var;
      var.name = basename;
      var.rows = var.columns = 0;
      var.type = VarType::Float;
      var.rowMajor = rowMajor;

      vector<ShaderVariable> varmembers;

      if(isArray)
      {
        for(uint32_t i = 0; i < elems; i++)
        {
          ShaderVariable vr;
          vr.name = StringFormat::Fmt("%s[%u]", basename.c_str(), i);
          vr.rows = vr.columns = 0;
          vr.type = VarType::Float;
          vr.rowMajor = rowMajor;

          vector<ShaderVariable> mems;

          SPIRVFillCBufferVariables(invars[v].type.members, mems, data, dataOffset);

          dataOffset += invars[v].type.descriptor.arrayByteStride;

          vr.isStruct = true;

          vr.members = mems;

          varmembers.push_back(vr);
        }

        var.isStruct = false;
      }
      else
      {
        var.isStruct = true;

        SPIRVFillCBufferVariables(invars[v].type.members, varmembers, data, dataOffset);
      }

      {
        var.members = varmembers;
        outvars.push_back(var);
      }

      continue;
    }

    size_t outIdx = outvars.size();
    outvars.resize(outvars.size() + 1);

    {
      outvars[outIdx].name = basename;
      outvars[outIdx].rows = 1;
      outvars[outIdx].type = invars[v].type.descriptor.type;
      outvars[outIdx].isStruct = false;
      outvars[outIdx].columns = cols;
      outvars[outIdx].rowMajor = rowMajor;

      size_t elemByteSize = 4;
      if(outvars[outIdx].type == VarType::Double)
        elemByteSize = 8;

      ShaderVariable &var = outvars[outIdx];

      if(!isArray)
      {
        outvars[outIdx].rows = rows;

        if(dataOffset < data.size())
        {
          const byte *d = &data[dataOffset];

          RDCASSERT(rows <= 4 && rows * cols <= 16, rows, cols);

          if(!rowMajor)
          {
            uint32_t tmp[16] = {0};

            for(uint32_t c = 0; c < cols; c++)
            {
              size_t srcoffs = 4 * elemByteSize * c;
              size_t dstoffs = rows * elemByteSize * c;
              memcpy((byte *)(tmp) + dstoffs, d + srcoffs,
                     RDCMIN(data.size() - dataOffset + srcoffs, elemByteSize * rows));
            }

            // transpose
            for(size_t r = 0; r < rows; r++)
              for(size_t c = 0; c < cols; c++)
                outvars[outIdx].value.uv[r * cols + c] = tmp[c * rows + r];
          }
          else
          {
            for(uint32_t r = 0; r < rows; r++)
            {
              size_t srcoffs = 4 * elemByteSize * r;
              size_t dstoffs = cols * elemByteSize * r;
              memcpy((byte *)(&outvars[outIdx].value.uv[0]) + dstoffs, d + srcoffs,
                     RDCMIN(data.size() - dataOffset + srcoffs, elemByteSize * cols));
            }
          }
        }
      }
      else
      {
        var.name = outvars[outIdx].name;
        var.rows = 0;
        var.columns = 0;

        bool isMatrix = rows > 1 && cols > 1;

        vector<ShaderVariable> varmembers;
        varmembers.resize(elems);

        std::string base = outvars[outIdx].name;

        // primary is the 'major' direction
        // so we copy secondaryDim number of primaryDim-sized elements
        uint32_t primaryDim = cols;
        uint32_t secondaryDim = rows;
        if(isMatrix && rowMajor)
        {
          primaryDim = rows;
          secondaryDim = cols;
        }

        for(uint32_t e = 0; e < elems; e++)
        {
          varmembers[e].name = StringFormat::Fmt("%s[%u]", base.c_str(), e);
          varmembers[e].rows = rows;
          varmembers[e].type = invars[v].type.descriptor.type;
          varmembers[e].isStruct = false;
          varmembers[e].columns = cols;
          varmembers[e].rowMajor = rowMajor;

          size_t rowDataOffset = dataOffset;

          dataOffset += invars[v].type.descriptor.arrayByteStride;

          if(rowDataOffset < data.size())
          {
            const byte *d = &data[rowDataOffset];

            // each primary element (row or column) is stored in a float4.
            // we copy some padding here, but that will come out in the wash
            // when we transpose
            for(uint32_t s = 0; s < secondaryDim; s++)
            {
              uint32_t matStride = primaryDim;
              if(matStride == 3)
                matStride = 4;
              memcpy(&(varmembers[e].value.uv[primaryDim * s]), d + matStride * elemByteSize * s,
                     RDCMIN(data.size() - rowDataOffset, elemByteSize * primaryDim));
            }

            if(!rowMajor)
            {
              ShaderVariable tmp = varmembers[e];
              // transpose
              for(size_t ri = 0; ri < rows; ri++)
                for(size_t ci = 0; ci < cols; ci++)
                  varmembers[e].value.uv[ri * cols + ci] = tmp.value.uv[ci * rows + ri];
            }
          }
        }

        {
          var.isStruct = false;
          var.members = varmembers;
        }
      }
    }
  }
}

void FillSpecConstantVariables(const rdcarray<ShaderConstant> &invars,
                               std::vector<ShaderVariable> &outvars,
                               const std::vector<SpecConstant> &specInfo)
{
  outvars.resize(invars.size());
  for(size_t v = 0; v < invars.size(); v++)
  {
    outvars[v].rows = invars[v].type.descriptor.rows;
    outvars[v].columns = invars[v].type.descriptor.columns;
    outvars[v].isStruct = !invars[v].type.members.empty();
    RDCASSERT(!outvars[v].isStruct);
    outvars[v].name = invars[v].name;
    outvars[v].type = invars[v].type.descriptor.type;

    outvars[v].value.uv[0] = (invars[v].defaultValue & 0xFFFFFFFF);
    outvars[v].value.uv[1] = ((invars[v].defaultValue >> 32) & 0xFFFFFFFF);
  }

  // find any actual values specified
  for(size_t i = 0; i < specInfo.size(); i++)
  {
    for(size_t v = 0; v < invars.size(); v++)
    {
      if(specInfo[i].specID == invars[v].reg.vec)
      {
        memcpy(outvars[v].value.uv, specInfo[i].data.data(),
               RDCMIN(specInfo[i].data.size(), sizeof(outvars[v].value.uv)));
        break;
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
      case ReflectionInterface::Input: *params = program->getNumLiveAttributes(); break;
      case ReflectionInterface::Output:
        // unsupported
        *params = 0;
        break;
      case ReflectionInterface::Uniform: *params = program->getNumLiveUniformVariables(); break;
      case ReflectionInterface::UniformBlock: *params = program->getNumLiveUniformBlocks(); break;
      case ReflectionInterface::ShaderStorageBlock:
        // unsupported
        *params = 0;
        break;
      case ReflectionInterface::AtomicCounterBuffer:
        // unsupported
        *params = 0;
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
  if(programInterface == ReflectionInterface::Output ||
     programInterface == ReflectionInterface::ShaderStorageBlock ||
     programInterface == ReflectionInterface::AtomicCounterBuffer)
  {
    RDCWARN("unsupported program interface");
  }

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
        RDCASSERT(programInterface == ReflectionInterface::UniformBlock);
        params[i] = program->getUniformBlockBinding(index);
        break;
      case ReflectionProperty::TopLevelArrayStride:
        // TODO glslang doesn't give us this
        params[i] = 16;
        break;
      case ReflectionProperty::BlockIndex:
        RDCASSERT(programInterface == ReflectionInterface::Uniform);
        params[i] = program->getUniformBlockIndex(index);
        break;
      case ReflectionProperty::ArraySize:
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniformArraySize(index);
        else if(programInterface == ReflectionInterface::Input)
          // TODO assuming all inputs are non-arrayed
          params[i] = 1;
        else
          RDCERR("Unsupported interface for ArraySize query");
        break;
      case ReflectionProperty::IsRowMajor:
        // TODO glslang doesn't expose this, assume column major.
        params[i] = 0;
        break;
      case ReflectionProperty::NumActiveVariables:
        // TODO glslang doesn't give us this
        params[i] = 1;
        break;
      case ReflectionProperty::BufferDataSize:
        RDCASSERT(programInterface == ReflectionInterface::UniformBlock);
        params[i] = program->getUniformBlockSize(index);
        break;
      case ReflectionProperty::NameLength:
        // The name length includes a terminating null character.
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = (int32_t)strlen(program->getUniformName(index)) + 1;
        else if(programInterface == ReflectionInterface::UniformBlock)
          params[i] = (int32_t)strlen(program->getUniformBlockName(index)) + 1;
        else if(programInterface == ReflectionInterface::Input)
          params[i] = (int32_t)strlen(program->getAttributeName(index)) + 1;
        else
          RDCERR("Unsupported interface for NameLEngth query");
        break;
      case ReflectionProperty::Type:
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniformType(index);
        else if(programInterface == ReflectionInterface::Input)
          params[i] = program->getAttributeType(index);
        else
          RDCERR("Unsupported interface for Type query");
        break;
      case ReflectionProperty::LocationComponent:
        // TODO glslang doesn't give us this information
        params[i] = 0;
        break;
      case ReflectionProperty::ReferencedByVertexShader:
      case ReflectionProperty::ReferencedByTessControlShader:
      case ReflectionProperty::ReferencedByTessEvaluationShader:
      case ReflectionProperty::ReferencedByGeometryShader:
      case ReflectionProperty::ReferencedByFragmentShader:
      case ReflectionProperty::ReferencedByComputeShader:
        // TODO glslang doesn't give us this information
        params[i] = 1;
        break;
      case ReflectionProperty::AtomicCounterBufferIndex:
        RDCERR("Atomic counters not supported");
        break;
      case ReflectionProperty::Offset:
        RDCASSERT(programInterface == ReflectionInterface::Uniform);
        params[i] = program->getUniformBufferOffset(index);
        break;
      case ReflectionProperty::MatrixStride:
        RDCASSERT(programInterface == ReflectionInterface::Uniform);
        // TODO glslang doesn't give us this information
        params[i] = 64;
        break;
      case ReflectionProperty::ArrayStride:
        RDCASSERT(programInterface == ReflectionInterface::Uniform);
        // TODO glslang doesn't give us this information
        params[i] = 64;
        break;
      case ReflectionProperty::Location:
        // have to query the actual implementation, which is handled elsewhere. We return either -1
        // for uniforms that don't have a location (i.e. are in a block) or 0 for bare uniforms
        if(programInterface == ReflectionInterface::Uniform)
          params[i] = program->getUniformBlockIndex(index) >= 0 ? -1 : 0;
        else if(programInterface == ReflectionInterface::Input)
          params[i] = index;
        break;
    }
  }
}

const char *glslangGetProgramResourceName(glslang::TProgram *program,
                                          ReflectionInterface programInterface, uint32_t index)
{
  const char *fetchedName = "";

  switch(programInterface)
  {
    case ReflectionInterface::Input: fetchedName = program->getAttributeName(index); break;
    case ReflectionInterface::Output: RDCWARN("Output attributes unsupported"); break;
    case ReflectionInterface::Uniform: fetchedName = program->getUniformName(index); break;
    case ReflectionInterface::UniformBlock:
      fetchedName = program->getUniformBlockName(index);
      break;
    case ReflectionInterface::ShaderStorageBlock:
      RDCWARN("shader storage blocks unsupported");
      break;
    case ReflectionInterface::AtomicCounterBuffer:
      RDCWARN("atomic counter buffers unsupported");
      break;
  }

  return fetchedName;
}
