/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "spirv_editor.h"
#include <algorithm>
#include <utility>
#include "common/common.h"
#include "serialise/serialiser.h"
#include "spirv_op_helpers.h"

SPIRVScalar::SPIRVScalar(rdcspv::Iter it)
{
  type = it.opcode();

  if(type == rdcspv::Op::TypeInt)
  {
    rdcspv::OpTypeInt decoded(it);
    width = decoded.width;
    signedness = decoded.signedness == 1;
  }
  else if(type == rdcspv::Op::TypeFloat)
  {
    rdcspv::OpTypeFloat decoded(it);
    width = decoded.width;
    signedness = false;
  }
  else
  {
    width = 0;
    signedness = false;
  }
}

rdcspv::Operation SPIRVVector::decl(SPIRVEditor &editor) const
{
  return rdcspv::OpTypeVector(rdcspv::Id(), editor.DeclareType(scalar), count);
}

rdcspv::Operation SPIRVMatrix::decl(SPIRVEditor &editor) const
{
  return rdcspv::OpTypeMatrix(rdcspv::Id(), editor.DeclareType(vector), count);
}

rdcspv::Operation SPIRVPointer::decl(SPIRVEditor &editor) const
{
  return rdcspv::OpTypePointer(rdcspv::Id(), storage, baseId);
}

rdcspv::Operation SPIRVImage::decl(SPIRVEditor &editor) const
{
  return rdcspv::OpTypeImage(rdcspv::Id(), editor.DeclareType(retType), dim, depth, arrayed, ms,
                             sampled, format);
}

rdcspv::Operation SPIRVSampler::decl(SPIRVEditor &editor) const
{
  return rdcspv::OpTypeSampler(rdcspv::Id());
}

rdcspv::Operation SPIRVSampledImage::decl(SPIRVEditor &editor) const
{
  return rdcspv::OpTypeSampledImage(rdcspv::Id(), baseId);
}

rdcspv::Operation SPIRVFunction::decl(SPIRVEditor &editor) const
{
  return rdcspv::OpTypeFunction(rdcspv::Id(), returnId, argumentIds);
}

SPIRVEditor::SPIRVEditor(std::vector<uint32_t> &spirvWords) : spirv(spirvWords)
{
  if(spirv.size() < rdcspv::FirstRealWord || spirv[0] != rdcspv::MagicNumber)
  {
    RDCERR("Empty or invalid SPIR-V module");
    return;
  }

  moduleVersion.major = uint8_t((spirv[1] & 0x00ff0000) >> 16);
  moduleVersion.minor = uint8_t((spirv[1] & 0x0000ff00) >> 8);
  generator = spirv[2];
  idOffsets.resize(spirv[3]);
  idTypes.resize(spirv[3]);

  // [4] is reserved
  RDCASSERT(spirv[4] == 0);

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
  sections[SPIRVSection::Count - 1].endOffset = spirvWords.size();

#define START_SECTION(section)           \
  if(sections[section].startOffset == 0) \
    sections[section].startOffset = it.offs();

  for(rdcspv::Iter it(spirv, rdcspv::FirstRealWord); it; it++)
  {
    rdcspv::Op opcode = it.opcode();

    if(opcode == rdcspv::Op::Capability)
    {
      START_SECTION(SPIRVSection::Capabilities);
    }
    else if(opcode == rdcspv::Op::Extension)
    {
      START_SECTION(SPIRVSection::Extensions);
    }
    else if(opcode == rdcspv::Op::ExtInstImport)
    {
      START_SECTION(SPIRVSection::ExtInst);
    }
    else if(opcode == rdcspv::Op::MemoryModel)
    {
      START_SECTION(SPIRVSection::MemoryModel);
    }
    else if(opcode == rdcspv::Op::EntryPoint)
    {
      START_SECTION(SPIRVSection::EntryPoints);
    }
    else if(opcode == rdcspv::Op::ExecutionMode || opcode == rdcspv::Op::ExecutionModeId)
    {
      START_SECTION(SPIRVSection::ExecutionMode);
    }
    else if(opcode == rdcspv::Op::String || opcode == rdcspv::Op::Source ||
            opcode == rdcspv::Op::SourceContinued || opcode == rdcspv::Op::SourceExtension ||
            opcode == rdcspv::Op::Name || opcode == rdcspv::Op::MemberName ||
            opcode == rdcspv::Op::ModuleProcessed)
    {
      START_SECTION(SPIRVSection::Debug);
    }
    else if(opcode == rdcspv::Op::Decorate || opcode == rdcspv::Op::MemberDecorate ||
            opcode == rdcspv::Op::GroupDecorate || opcode == rdcspv::Op::GroupMemberDecorate ||
            opcode == rdcspv::Op::DecorationGroup || opcode == rdcspv::Op::DecorateStringGOOGLE ||
            opcode == rdcspv::Op::MemberDecorateStringGOOGLE)
    {
      START_SECTION(SPIRVSection::Annotations);
    }
    else if(opcode == rdcspv::Op::Function)
    {
      START_SECTION(SPIRVSection::Functions);
    }
    else
    {
      // if we've reached another instruction, check if we've reached the function section yet. If
      // we have then assume it's an instruction inside a function and ignore. If we haven't, assume
      // it's a type/variable/constant type instruction
      if(sections[SPIRVSection::Functions].startOffset == 0)
      {
        START_SECTION(SPIRVSection::TypesVariablesConstants);
      }
    }

    RegisterOp(it);
  }

#undef START_SECTION

  // ensure we got everything right. First section should start at the beginning
  RDCASSERTEQUAL(sections[SPIRVSection::First].startOffset, rdcspv::FirstRealWord);

  // we now set the endOffset of each section to the start of the next. Any empty sections
  // temporarily have startOffset set to endOffset, we'll pad them with a nop below.
  for(int s = SPIRVSection::Count - 1; s > 0; s--)
  {
    RDCASSERTEQUAL(sections[s - 1].endOffset, 0);
    sections[s - 1].endOffset = sections[s].startOffset;
    if(sections[s - 1].startOffset == 0)
      sections[s - 1].startOffset = sections[s - 1].endOffset;
  }

  // find any empty sections and insert a nop into the stream there. We need to fixup later section
  // offsets by hand as addWords doesn't handle empty sections properly (it thinks we're inserting
  // into the later section by offset since the offsets overlap). That's why we're adding these
  // padding nops in the first place!
  for(uint32_t s = 0; s < SPIRVSection::Count; s++)
  {
    if(sections[s].startOffset == sections[s].endOffset)
    {
      spirv.insert(spirv.begin() + sections[s].startOffset, rdcspv::OpNopWord);
      sections[s].endOffset++;

      for(uint32_t t = s + 1; t < SPIRVSection::Count; t++)
      {
        sections[t].startOffset++;
        sections[t].endOffset++;
      }

      // look through every id, and update its offset
      for(size_t &o : idOffsets)
        if(o >= sections[s].startOffset)
          o++;
    }
  }

  // each section should now precisely match each other end-to-end and not be empty
  for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
  {
    RDCASSERTNOTEQUAL(sections[s].startOffset, 0);
    RDCASSERTNOTEQUAL(sections[s].endOffset, 0);

    RDCASSERT(sections[s].endOffset - sections[s].startOffset > 0, sections[s].startOffset,
              sections[s].endOffset);

    if(s != 0)
      RDCASSERTEQUAL(sections[s - 1].endOffset, sections[s].startOffset);

    if(s + 1 < SPIRVSection::Count)
      RDCASSERTEQUAL(sections[s].endOffset, sections[s + 1].startOffset);
  }
}

void SPIRVEditor::StripNops()
{
  for(size_t i = rdcspv::FirstRealWord; i < spirv.size();)
  {
    while(spirv[i] == rdcspv::OpNopWord)
    {
      spirv.erase(spirv.begin() + i);
      addWords(i, -1);
    }

    uint32_t len = spirv[i] >> rdcspv::WordCountShift;

    if(len == 0)
    {
      RDCERR("Malformed SPIR-V");
      break;
    }

    i += len;
  }
}

rdcspv::Id SPIRVEditor::MakeId()
{
  uint32_t ret = spirv[3];
  spirv[3]++;
  idOffsets.resize(spirv[3]);
  idTypes.resize(spirv[3]);
  return rdcspv::Id::fromWord(ret);
}

void SPIRVEditor::SetName(rdcspv::Id id, const char *name)
{
  size_t sz = strlen(name);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], name, sz);

  uintName.insert(uintName.begin(), id.value());

  rdcspv::Operation op(rdcspv::Op::Name, uintName);

  rdcspv::Iter it;

  // OpName must be before OpModuleProcessed.
  for(it = Begin(SPIRVSection::Debug); it < End(SPIRVSection::Debug); ++it)
  {
    if(it.opcode() == rdcspv::Op::ModuleProcessed)
      break;
  }

  op.insertInto(spirv, it.offs());
  RegisterOp(rdcspv::Iter(spirv, it.offs()));
  addWords(it.offs(), op.size());
}

void SPIRVEditor::AddDecoration(const rdcspv::Operation &op)
{
  size_t offset = sections[SPIRVSection::Annotations].endOffset;
  op.insertInto(spirv, offset);
  RegisterOp(rdcspv::Iter(spirv, offset));
  addWords(offset, op.size());
}

void SPIRVEditor::AddCapability(rdcspv::Capability cap)
{
  // don't add duplicate capabilities
  if(capabilities.find(cap) != capabilities.end())
    return;

  // insert the operation at the very start
  rdcspv::Operation op(rdcspv::Op::Capability, {(uint32_t)cap});
  op.insertInto(spirv, rdcspv::FirstRealWord);
  RegisterOp(rdcspv::Iter(spirv, rdcspv::FirstRealWord));
  addWords(rdcspv::FirstRealWord, op.size());
}

void SPIRVEditor::AddExtension(const rdcstr &extension)
{
  // don't add duplicate extensions
  if(extensions.find(extension) != extensions.end())
    return;

  // start at the beginning
  rdcspv::Iter it(spirv, rdcspv::FirstRealWord);

  // skip past any capabilities
  while(it.opcode() == rdcspv::Op::Capability)
    it++;

  // insert the extension instruction
  size_t sz = extension.size();
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], extension.c_str(), sz);

  rdcspv::Operation op(rdcspv::Op::Extension, uintName);
  op.insertInto(spirv, it.offs());
  RegisterOp(it);
  addWords(it.offs(), op.size());
}

void SPIRVEditor::AddExecutionMode(const rdcspv::Operation &mode)
{
  size_t offset = sections[SPIRVSection::ExecutionMode].endOffset;

  mode.insertInto(spirv, offset);
  RegisterOp(rdcspv::Iter(spirv, offset));
  addWords(offset, mode.size());
}

rdcspv::Id SPIRVEditor::ImportExtInst(const char *setname)
{
  rdcspv::Id ret = extSets[setname];

  if(ret)
    return ret;

  // start at the beginning
  rdcspv::Iter it(spirv, rdcspv::FirstRealWord);

  // skip past any capabilities and extensions
  while(it.opcode() == rdcspv::Op::Capability || it.opcode() == rdcspv::Op::Extension)
    it++;

  // insert the import instruction
  ret = MakeId();

  size_t sz = strlen(setname);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], setname, sz);

  uintName.insert(uintName.begin(), ret.value());

  rdcspv::Operation op(rdcspv::Op::ExtInstImport, uintName);
  op.insertInto(spirv, it.offs());
  RegisterOp(it);
  addWords(it.offs(), op.size());

  extSets[setname] = ret;

  return ret;
}

rdcspv::Id SPIRVEditor::AddType(const rdcspv::Operation &op)
{
  size_t offset = sections[SPIRVSection::Types].endOffset;

  rdcspv::Id id = rdcspv::Id::fromWord(op[1]);
  idOffsets[id.value()] = offset;
  op.insertInto(spirv, offset);
  RegisterOp(rdcspv::Iter(spirv, offset));
  addWords(offset, op.size());
  return id;
}

rdcspv::Id SPIRVEditor::AddVariable(const rdcspv::Operation &op)
{
  size_t offset = sections[SPIRVSection::Variables].endOffset;

  rdcspv::Id id = rdcspv::Id::fromWord(op[2]);
  idOffsets[id.value()] = offset;
  op.insertInto(spirv, offset);
  RegisterOp(rdcspv::Iter(spirv, offset));
  addWords(offset, op.size());
  return id;
}

rdcspv::Id SPIRVEditor::AddConstant(const rdcspv::Operation &op)
{
  size_t offset = sections[SPIRVSection::Constants].endOffset;

  rdcspv::Id id = rdcspv::Id::fromWord(op[2]);
  idOffsets[id.value()] = offset;
  op.insertInto(spirv, offset);
  RegisterOp(rdcspv::Iter(spirv, offset));
  addWords(offset, op.size());
  return id;
}

void SPIRVEditor::AddFunction(const rdcspv::Operation *ops, size_t count)
{
  idOffsets[ops[0][2]] = spirv.size();

  for(size_t i = 0; i < count; i++)
    ops[i].appendTo(spirv);

  RegisterOp(rdcspv::Iter(spirv, idOffsets[ops[0][2]]));
}

rdcspv::Iter SPIRVEditor::GetID(rdcspv::Id id)
{
  size_t offs = idOffsets[id.value()];

  if(offs)
    return rdcspv::Iter(spirv, offs);

  return rdcspv::Iter();
}

rdcspv::Iter SPIRVEditor::GetEntry(rdcspv::Id id)
{
  rdcspv::Iter it(spirv, sections[SPIRVSection::EntryPoints].startOffset);
  rdcspv::Iter end(spirv, sections[SPIRVSection::EntryPoints].endOffset);

  while(it && it < end)
  {
    rdcspv::OpEntryPoint entry(it);

    if(entry.entryPoint == id.value())
      return it;
    it++;
  }

  return rdcspv::Iter();
}

rdcspv::Id SPIRVEditor::DeclareStructType(const std::vector<rdcspv::Id> &members)
{
  rdcspv::Id typeId = MakeId();
  AddType(rdcspv::OpTypeStruct(typeId, members));
  return typeId;
}

void SPIRVEditor::AddOperation(rdcspv::Iter iter, const rdcspv::Operation &op)
{
  if(!iter)
    return;

  // add op
  op.insertInto(spirv, iter.offs());

  // update offsets
  addWords(iter.offs(), op.size());
}

void SPIRVEditor::RegisterOp(rdcspv::Iter it)
{
  rdcspv::Op opcode = it.opcode();

  rdcspv::OpDecoder opdata(it);
  if(opdata.result != rdcspv::Id() && opdata.resultType != rdcspv::Id())
  {
    RDCASSERT(opdata.result.value() < idTypes.size());
    idTypes[opdata.result.value()] = opdata.resultType;
  }

  if(opdata.result != rdcspv::Id())
    idOffsets[opdata.result.value()] = it.offs();

  if(opcode == rdcspv::Op::EntryPoint)
  {
    entries.push_back(rdcspv::OpEntryPoint(it));
  }
  else if(opcode == rdcspv::Op::MemoryModel)
  {
    rdcspv::OpMemoryModel decoded(it);
    addressmodel = decoded.addressingModel;
    memorymodel = decoded.memoryModel;
  }
  else if(opcode == rdcspv::Op::Capability)
  {
    rdcspv::OpCapability decoded(it);
    capabilities.insert(decoded.capability);
  }
  else if(opcode == rdcspv::Op::Extension)
  {
    rdcspv::OpExtension decoded(it);
    extensions.insert(decoded.name);
  }
  else if(opcode == rdcspv::Op::ExtInstImport)
  {
    rdcspv::OpExtInstImport decoded(it);
    extSets[decoded.name] = decoded.result;
  }
  else if(opcode == rdcspv::Op::Function)
  {
    functions.push_back(opdata.result);
  }
  else if(opcode == rdcspv::Op::Variable)
  {
    variables.push_back(rdcspv::OpVariable(it));
  }
  else if(opcode == rdcspv::Op::Decorate)
  {
    rdcspv::OpDecorate decorate(it);

    auto it = std::lower_bound(
        decorations.begin(), decorations.end(), decorate,
        [](const rdcspv::OpDecorate &a, const rdcspv::OpDecorate &b) { return a < b; });
    decorations.insert(it, decorate);

    if(decorate.decoration == rdcspv::Decoration::DescriptorSet)
      bindings[decorate.target].set = decorate.decoration.descriptorSet;
    if(decorate.decoration == rdcspv::Decoration::Binding)
      bindings[decorate.target].binding = decorate.decoration.binding;
  }
  else if(opcode == rdcspv::Op::TypeVoid || opcode == rdcspv::Op::TypeBool ||
          opcode == rdcspv::Op::TypeInt || opcode == rdcspv::Op::TypeFloat)
  {
    SPIRVScalar scalar(it);
    scalarTypes[scalar] = opdata.result;
  }
  else if(opcode == rdcspv::Op::TypeVector)
  {
    rdcspv::OpTypeVector decoded(it);

    rdcspv::Iter scalarIt = GetID(decoded.componentType);

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", decoded.componentType);
      return;
    }

    vectorTypes[SPIRVVector(scalarIt, decoded.componentCount)] = decoded.result;
  }
  else if(opcode == rdcspv::Op::TypeMatrix)
  {
    rdcspv::OpTypeMatrix decodedMatrix(it);

    rdcspv::Iter vectorIt = GetID(decodedMatrix.columnType);

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", decodedMatrix.columnType);
      return;
    }

    rdcspv::OpTypeVector decodedVector(vectorIt);

    rdcspv::Iter scalarIt = GetID(decodedVector.componentType);

    matrixTypes[SPIRVMatrix(SPIRVVector(scalarIt, decodedVector.componentCount),
                            decodedMatrix.columnCount)] = decodedMatrix.result;
  }
  else if(opcode == rdcspv::Op::TypeImage)
  {
    rdcspv::OpTypeImage decoded(it);

    rdcspv::Iter scalarIt = GetID(decoded.sampledType);

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", decoded.sampledType);
      return;
    }

    imageTypes[SPIRVImage(scalarIt, decoded.dim, decoded.depth, decoded.arrayed, decoded.mS,
                          decoded.sampled, decoded.imageFormat)] = decoded.result;
  }
  else if(opcode == rdcspv::Op::TypeSampler)
  {
    samplerTypes[SPIRVSampler()] = opdata.result;
  }
  else if(opcode == rdcspv::Op::TypeSampledImage)
  {
    rdcspv::OpTypeSampledImage decoded(it);

    sampledImageTypes[SPIRVSampledImage(decoded.imageType)] = decoded.result;
  }
  else if(opcode == rdcspv::Op::TypePointer)
  {
    rdcspv::OpTypePointer decoded(it);

    pointerTypes[SPIRVPointer(decoded.type, decoded.storageClass)] = decoded.result;
  }
  else if(opcode == rdcspv::Op::TypeStruct)
  {
    structTypes.insert(opdata.result);
  }
  else if(opcode == rdcspv::Op::TypeFunction)
  {
    rdcspv::OpTypeFunction decoded(it);

    functionTypes[SPIRVFunction(decoded.returnType, decoded.parameters)] = decoded.result;
  }
}

void SPIRVEditor::UnregisterOp(rdcspv::Iter it)
{
  rdcspv::Op opcode = it.opcode();

  rdcspv::OpDecoder opdata(it);
  if(opdata.result != rdcspv::Id() && opdata.resultType != rdcspv::Id())
    idTypes[opdata.result.value()] = rdcspv::Id();

  if(opdata.result != rdcspv::Id())
    idOffsets[opdata.result.value()] = 0;

  if(opcode == rdcspv::Op::EntryPoint)
  {
    rdcspv::OpEntryPoint decoded(it);

    for(auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
      if(entryIt->entryPoint == decoded.entryPoint)
      {
        entries.erase(entryIt);
        break;
      }
    }
  }
  else if(opcode == rdcspv::Op::Function)
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
  else if(opcode == rdcspv::Op::Variable)
  {
    for(auto varIt = variables.begin(); varIt != variables.end(); ++varIt)
    {
      if(varIt->result == opdata.result)
      {
        variables.erase(varIt);
        break;
      }
    }
  }
  else if(opcode == rdcspv::Op::Decorate)
  {
    rdcspv::OpDecorate decorate(it);

    auto it = std::lower_bound(
        decorations.begin(), decorations.end(), decorate,
        [](const rdcspv::OpDecorate &a, const rdcspv::OpDecorate &b) { return a < b; });
    if(it != decorations.end() && *it == decorate)
      decorations.erase(it);

    if(decorate.decoration == rdcspv::Decoration::DescriptorSet)
      bindings[decorate.target].set = SPIRVBinding().set;
    if(decorate.decoration == rdcspv::Decoration::Binding)
      bindings[decorate.target].binding = SPIRVBinding().binding;
  }
  else if(opcode == rdcspv::Op::Capability)
  {
    rdcspv::OpCapability decoded(it);
    capabilities.erase(decoded.capability);
  }
  else if(opcode == rdcspv::Op::Extension)
  {
    rdcspv::OpExtension decoded(it);
    extensions.erase(decoded.name);
  }
  else if(opcode == rdcspv::Op::ExtInstImport)
  {
    rdcspv::OpExtInstImport decoded(it);
    extSets.erase(decoded.name);
  }
  else if(opcode == rdcspv::Op::TypeVoid || opcode == rdcspv::Op::TypeBool ||
          opcode == rdcspv::Op::TypeInt || opcode == rdcspv::Op::TypeFloat)
  {
    SPIRVScalar scalar(it);
    scalarTypes.erase(scalar);
  }
  else if(opcode == rdcspv::Op::TypeVector)
  {
    rdcspv::OpTypeVector decoded(it);

    rdcspv::Iter scalarIt = GetID(decoded.componentType);

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", decoded.componentType);
      return;
    }

    vectorTypes.erase(SPIRVVector(scalarIt, decoded.componentCount));
  }
  else if(opcode == rdcspv::Op::TypeMatrix)
  {
    rdcspv::OpTypeMatrix decodedMatrix(it);

    rdcspv::Iter vectorIt = GetID(decodedMatrix.columnType);

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", decodedMatrix.columnType);
      return;
    }

    rdcspv::OpTypeVector decodedVector(vectorIt);

    rdcspv::Iter scalarIt = GetID(decodedVector.componentType);

    matrixTypes.erase(SPIRVMatrix(SPIRVVector(scalarIt, decodedVector.componentCount),
                                  decodedMatrix.columnCount));
  }
  else if(opcode == rdcspv::Op::TypeImage)
  {
    rdcspv::OpTypeImage decoded(it);

    rdcspv::Iter scalarIt = GetID(decoded.sampledType);

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", decoded.sampledType);
      return;
    }

    imageTypes.erase(SPIRVImage(scalarIt, decoded.dim, decoded.depth, decoded.arrayed, decoded.mS,
                                decoded.sampled, decoded.imageFormat));
  }
  else if(opcode == rdcspv::Op::TypeSampler)
  {
    samplerTypes.erase(SPIRVSampler());
  }
  else if(opcode == rdcspv::Op::TypeSampledImage)
  {
    rdcspv::OpTypeSampledImage decoded(it);

    sampledImageTypes.erase(SPIRVSampledImage(decoded.imageType));
  }
  else if(opcode == rdcspv::Op::TypePointer)
  {
    rdcspv::OpTypePointer decoded(it);

    pointerTypes.erase(SPIRVPointer(decoded.type, decoded.storageClass));
  }
  else if(opcode == rdcspv::Op::TypeStruct)
  {
    structTypes.erase(opdata.result);
  }
  else if(opcode == rdcspv::Op::TypeFunction)
  {
    rdcspv::OpTypeFunction decoded(it);

    functionTypes.erase(SPIRVFunction(decoded.returnType, decoded.parameters));
  }
}

void SPIRVEditor::addWords(size_t offs, int32_t num)
{
  // look through every section, any that are >= this point, adjust the offsets
  // note that if we're removing words then any offsets pointing directly to the removed words
  // will go backwards - but they no longer have anywhere valid to point.
  for(LogicalSection &section : sections)
  {
    // we have three cases to consider: either the offset matches start, is within (up to and
    // including end) or is outside the section.
    // We ensured during parsing that all sections were non-empty by adding nops if necessary, so we
    // don't have to worry about the situation where we can't decide if an insert is at the end of
    // one section or inside the next. Note this means we don't support inserting at the start of a
    // section.

    if(offs == section.startOffset)
    {
      // if the offset matches the start, we're appending at the end of the previous section so move
      // both
      section.startOffset += num;
      section.endOffset += num;
    }
    else if(offs > section.startOffset && offs <= section.endOffset)
    {
      // if the offset is in the section (up to and including the end) then we're inserting in this
      // section, so move the end only
      section.endOffset += num;
    }
    else if(section.startOffset >= offs)
    {
      // otherwise move both or neither depending on which side the offset is.
      section.startOffset += num;
      section.endOffset += num;
    }
  }

  // look through every id, and do the same
  for(size_t &o : idOffsets)
    if(o >= offs)
      o += num;
}

#define TYPETABLE(StructType, variable)                                             \
  template <>                                                                       \
  std::map<StructType, rdcspv::Id> &SPIRVEditor::GetTable<StructType>()             \
  {                                                                                 \
    return variable;                                                                \
  }                                                                                 \
  template <>                                                                       \
  const std::map<StructType, rdcspv::Id> &SPIRVEditor::GetTable<StructType>() const \
  {                                                                                 \
    return variable;                                                                \
  }

TYPETABLE(SPIRVScalar, scalarTypes);
TYPETABLE(SPIRVVector, vectorTypes);
TYPETABLE(SPIRVMatrix, matrixTypes);
TYPETABLE(SPIRVPointer, pointerTypes);
TYPETABLE(SPIRVImage, imageTypes);
TYPETABLE(SPIRVSampler, samplerTypes);
TYPETABLE(SPIRVSampledImage, sampledImageTypes);
TYPETABLE(SPIRVFunction, functionTypes);

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"
#include "core/core.h"
#include "spirv_common.h"
#include "spirv_compile.h"

static void RemoveSection(std::vector<uint32_t> &spirv, size_t offsets[SPIRVSection::Count][2],
                          SPIRVSection::Type section)
{
  SPIRVEditor ed(spirv);

  for(rdcspv::Iter it = ed.Begin(section), end = ed.End(section); it < end; it++)
    ed.Remove(it);

  size_t oldLength = offsets[section][1] - offsets[section][0];

  // section will still contain a nop
  offsets[section][1] = offsets[section][0] + 4;

  // subsequent sections will be shorter by the length - 4, because a nop will still be inserted
  // as padding to ensure no section is truly empty.
  size_t delta = oldLength - 4;

  for(uint32_t s = section + 1; s < SPIRVSection::Count; s++)
  {
    offsets[s][0] -= delta;
    offsets[s][1] -= delta;
  }
}

static void CheckSPIRV(SPIRVEditor &ed, size_t offsets[SPIRVSection::Count][2])
{
  for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
  {
    INFO("Section " << s);
    CHECK(ed.Begin((SPIRVSection::Type)s).offs() == offsets[s][0] / sizeof(uint32_t));
    CHECK(ed.End((SPIRVSection::Type)s).offs() == offsets[s][1] / sizeof(uint32_t));
  }

  // should only be one entry point
  REQUIRE(ed.GetEntries().size() == 1);

  rdcspv::Id entryId = ed.GetEntries()[0].entryPoint;

  // check that the iterator places us precisely at the start of the functions section
  CHECK(ed.GetID(entryId).offs() == ed.Begin(SPIRVSection::Functions).offs());
}

TEST_CASE("Test SPIR-V editor section handling", "[spirv]")
{
  InitSPIRVCompiler();
  RenderDoc::Inst().RegisterShutdownFunction(&ShutdownSPIRVCompiler);

  SPIRVCompilationSettings settings;
  settings.entryPoint = "main";
  settings.lang = SPIRVSourceLanguage::VulkanGLSL;
  settings.stage = SPIRVShaderStage::Fragment;

  // simple shader that has at least something in every section
  std::vector<std::string> sources = {
      R"(#version 450 core

#extension GL_EXT_shader_16bit_storage : require

layout(binding = 0) uniform block {
	float16_t val;
};

layout(location = 0) out vec4 col;

void main() {
  col = vec4(sin(gl_FragCoord.x)*float(val), 0, 0, 1);
}
)",
  };

  std::vector<uint32_t> spirv;
  std::string errors = CompileSPIRV(settings, sources, spirv);

  INFO("SPIR-V compilation - " << errors);

  // ensure that compilation succeeded
  REQUIRE(spirv.size() > 0);

  // these offsets may change if the compiler changes above. Verify manually with spirv-dis that
  // they should be updated.
  // For convenience the offsets are in bytes (which spirv-dis uses) and are converted in the loop
  // in CheckSPIRV.
  size_t offsets[SPIRVSection::Count][2] = {
      // Capabilities
      {0x14, 0x24},
      // Extensions
      {0x24, 0x40},
      // ExtInst
      {0x40, 0x58},
      // MemoryModel
      {0x58, 0x64},
      // EntryPoints
      {0x64, 0x80},
      // ExecutionMode
      {0x80, 0x8c},
      // Debug
      {0x8c, 0x118},
      // Annotations
      {0x118, 0x178},
      // TypesVariables
      {0x178, 0x2a0},
      // Functions
      {0x2a0, 0x370},
  };

  SECTION("Check that SPIR-V is correct with no changes")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  // we remove all sections we consider optional in arbitrary order. We don't care about keeping the
  // SPIR-V valid all we're testing is the section offsets are correct.
  RemoveSection(spirv, offsets, SPIRVSection::Extensions);

  SECTION("Check with extensions removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, SPIRVSection::Debug);

  SECTION("Check with debug removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, SPIRVSection::ExtInst);

  SECTION("Check with extension imports removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, SPIRVSection::ExecutionMode);

  SECTION("Check with execution mode removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, SPIRVSection::Annotations);

  SECTION("Check with annotations removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }
}

#endif