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

#include "spirv_processor.h"
#include "spirv_op_helpers.h"

namespace rdcspv
{
Processor::Processor()
{
}

void Processor::Parse(const std::vector<uint32_t> &spirvWords)
{
  m_SPIRV = spirvWords;

  if(m_SPIRV.size() < FirstRealWord || m_SPIRV[0] != MagicNumber)
  {
    RDCERR("Empty or invalid SPIR-V module");
    m_SPIRV.clear();
    return;
  }

  // [4] is reserved
  RDCASSERT(m_SPIRV[4] == 0);

  PreParse(m_SPIRV[3]);

  // simple state machine to track which section we're in.
  // Note that a couple of sections are optional and could be skipped over, at which point we insert
  // a dummy OpNop so they're not empty (which will be stripped later) and record them as in
  // between.
  //
  // We only handle single-shader modules at the moment, so some things are required by virtue of
  // being required in a shader - e.g. at least the Shader capability, at least one entry point, etc
  //
  // Capabilities: REQUIRED (we assume - must declare Shader capability)
  // Extensions: OPTIONAL
  // ExtInst: OPTIONAL
  // MemoryModel: REQUIRED (required by spec)
  // EntryPoints: REQUIRED (we assume)
  // ExecutionMode: OPTIONAL
  // Debug: OPTIONAL
  // Annotations: OPTIONAL (in theory - would require empty shader)
  // TypesVariables: REQUIRED (must at least have the entry point function type)
  // Functions: REQUIRED (must have the entry point)

  // set the book-ends: start of the first section and end of the last
  m_Sections[Section::Count - 1].endOffset = m_SPIRV.size();

#define START_SECTION(section)             \
  if(m_Sections[section].startOffset == 0) \
    m_Sections[section].startOffset = it.offs();

  for(Iter it(m_SPIRV, FirstRealWord); it; it++)
  {
    Op opcode = it.opcode();

    if(opcode == Op::Capability)
    {
      START_SECTION(Section::Capabilities);
    }
    else if(opcode == Op::Extension)
    {
      START_SECTION(Section::Extensions);
    }
    else if(opcode == Op::ExtInstImport)
    {
      START_SECTION(Section::ExtInst);
    }
    else if(opcode == Op::MemoryModel)
    {
      START_SECTION(Section::MemoryModel);
    }
    else if(opcode == Op::EntryPoint)
    {
      START_SECTION(Section::EntryPoints);
    }
    else if(opcode == Op::ExecutionMode || opcode == Op::ExecutionModeId)
    {
      START_SECTION(Section::ExecutionMode);
    }
    else if(opcode == Op::String || opcode == Op::Source || opcode == Op::SourceContinued ||
            opcode == Op::SourceExtension || opcode == Op::Name || opcode == Op::MemberName ||
            opcode == Op::ModuleProcessed)
    {
      START_SECTION(Section::Debug);
    }
    else if(opcode == Op::Decorate || opcode == Op::MemberDecorate || opcode == Op::GroupDecorate ||
            opcode == Op::GroupMemberDecorate || opcode == Op::DecorationGroup ||
            opcode == Op::DecorateStringGOOGLE || opcode == Op::MemberDecorateStringGOOGLE)
    {
      START_SECTION(Section::Annotations);
    }
    else if(opcode == Op::Function)
    {
      START_SECTION(Section::Functions);
    }
    else
    {
      // if we've reached another instruction, check if we've reached the function section yet. If
      // we have then assume it's an instruction inside a function and ignore. If we haven't, assume
      // it's a type/variable/constant type instruction
      if(m_Sections[Section::Functions].startOffset == 0)
      {
        START_SECTION(Section::TypesVariablesConstants);
      }
    }

    RegisterOp(it);
  }

#undef START_SECTION

  PostParse();

  // ensure we got everything right. First section should start at the beginning
  RDCASSERTEQUAL(m_Sections[Section::First].startOffset, FirstRealWord);

  // we now set the endOffset of each section to the start of the next. Any empty sections
  // temporarily have startOffset set to endOffset, we'll pad them with a nop below.
  for(int s = Section::Count - 1; s > 0; s--)
  {
    RDCASSERTEQUAL(m_Sections[s - 1].endOffset, 0);
    m_Sections[s - 1].endOffset = m_Sections[s].startOffset;
    if(m_Sections[s - 1].startOffset == 0)
      m_Sections[s - 1].startOffset = m_Sections[s - 1].endOffset;
  }
}

void Processor::PreParse(uint32_t maxId)
{
  idOffsets.resize(maxId);
  idTypes.resize(maxId);
}

void Processor::RegisterOp(Iter it)
{
  OpDecoder opdata(it);
  if(opdata.result != Id() && opdata.resultType != Id())
  {
    RDCASSERT(opdata.result.value() < idTypes.size());
    idTypes[opdata.result.value()] = opdata.resultType;
  }

  if(opdata.result != Id())
    idOffsets[opdata.result.value()] = it.offs();
  if(opdata.op == Op::Capability)
  {
    OpCapability decoded(it);
    capabilities.insert(decoded.capability);
  }
  else if(opdata.op == Op::Extension)
  {
    OpExtension decoded(it);
    extensions.insert(decoded.name);
  }
  else if(opdata.op == Op::ExtInstImport)
  {
    OpExtInstImport decoded(it);
    extSets[decoded.name] = decoded.result;
  }
  else if(opdata.op == Op::Function)
  {
    functions.push_back(opdata.result);
  }
  else if(opdata.op == Op::EntryPoint)
  {
    OpEntryPoint decoded(it);
    entries.push_back(EntryPoint(decoded.executionModel, decoded.entryPoint, decoded.name));
  }
  else if(opdata.op == Op::Variable)
  {
    OpVariable decoded(it);

    // only register global variables here
    if(decoded.storageClass != rdcspv::StorageClass::Function)
      globals.push_back(Variable(decoded.resultType, decoded.result, decoded.storageClass));
  }
  else if(opdata.op == Op::TypeVoid || opdata.op == Op::TypeBool || opdata.op == Op::TypeInt ||
          opdata.op == Op::TypeFloat)
  {
    scalarTypes[opdata.result] = Scalar(it);
  }
  else if(opdata.op == Op::TypeVector)
  {
    OpTypeVector decoded(it);

    vectorTypes[opdata.result] = Vector(scalarTypes[decoded.componentType], decoded.componentCount);
  }
  else if(opdata.op == Op::TypeMatrix)
  {
    OpTypeMatrix decoded(it);

    matrixTypes[opdata.result] = Matrix(vectorTypes[decoded.columnType], decoded.columnCount);
  }
  else if(opdata.op == Op::TypeImage)
  {
    OpTypeImage decoded(it);

    imageTypes[opdata.result] =
        Image(scalarTypes[decoded.sampledType], decoded.dim, decoded.depth, decoded.arrayed,
              decoded.mS, decoded.sampled, decoded.imageFormat);
  }
  else if(opdata.op == Op::TypeSampler)
  {
    samplerTypes[opdata.result] = Sampler();
  }
  else if(opdata.op == Op::TypeSampledImage)
  {
    OpTypeSampledImage decoded(it);

    sampledImageTypes[decoded.result] = SampledImage(decoded.imageType);
  }
  else if(opdata.op == Op::TypePointer)
  {
    OpTypePointer decoded(it);

    pointerTypes[decoded.result] = Pointer(decoded.type, decoded.storageClass);
  }
  else if(opdata.op == Op::TypeFunction)
  {
    OpTypeFunction decoded(it);

    functionTypes[decoded.result] = FunctionType(decoded.returnType, decoded.parameters);
  }
}

void Processor::UnregisterOp(Iter it)
{
  OpDecoder opdata(it);
  if(opdata.result != Id() && opdata.resultType != Id())
    idTypes[opdata.result.value()] = Id();

  if(opdata.result != Id())
    idOffsets[opdata.result.value()] = 0;

  if(opdata.op == Op::Capability)
  {
    OpCapability decoded(it);
    capabilities.erase(decoded.capability);
  }
  else if(opdata.op == Op::Extension)
  {
    OpExtension decoded(it);
    extensions.erase(decoded.name);
  }
  else if(opdata.op == Op::ExtInstImport)
  {
    OpExtInstImport decoded(it);
    extSets.erase(decoded.name);
  }
  else if(opdata.op == Op::Function)
  {
    for(auto funcIt = functions.begin(); funcIt != functions.end(); ++funcIt)
    {
      if(*funcIt == opdata.result)
      {
        functions.erase(funcIt);
        break;
      }
    }
  }
  else if(opdata.op == Op::EntryPoint)
  {
    OpEntryPoint decoded(it);

    for(auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
      if(entryIt->id == decoded.entryPoint)
      {
        entries.erase(entryIt);
        break;
      }
    }
  }
  else if(opdata.op == Op::Variable)
  {
    for(auto varIt = globals.begin(); varIt != globals.end(); ++varIt)
    {
      if(varIt->id == opdata.result)
      {
        globals.erase(varIt);
        break;
      }
    }
  }
  else if(opdata.op == Op::TypeVoid || opdata.op == Op::TypeBool || opdata.op == Op::TypeInt ||
          opdata.op == Op::TypeFloat)
  {
    scalarTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeVector)
  {
    vectorTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeMatrix)
  {
    matrixTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeImage)
  {
    imageTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeSampler)
  {
    samplerTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeSampledImage)
  {
    sampledImageTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypePointer)
  {
    pointerTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeFunction)
  {
    functionTypes.erase(opdata.result);
  }
}

void Processor::PostParse()
{
}

};    // namespace rdcspv