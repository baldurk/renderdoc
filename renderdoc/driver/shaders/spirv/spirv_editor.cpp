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

#define SPV_ENABLE_UTILITY_CODE

#include "spirv_editor.h"
#include <algorithm>
#include <utility>
#include "common/common.h"
#include "serialise/serialiser.h"

static const uint32_t FirstRealWord = 5;

SPIRVScalar::SPIRVScalar(rdcspv::Iter it)
{
  type = it.opcode();

  if(type == spv::OpTypeInt || type == spv::OpTypeFloat)
    width = it.word(2);
  else
    width = 0;

  if(type == spv::OpTypeInt)
    signedness = it.word(3) == 1;
  else
    signedness = false;
}

rdcspv::Operation SPIRVVector::decl(SPIRVEditor &editor) const
{
  return rdcspv::Operation(spv::OpTypeVector, {0U, editor.DeclareType(scalar).value(), count});
}

rdcspv::Operation SPIRVMatrix::decl(SPIRVEditor &editor) const
{
  return rdcspv::Operation(spv::OpTypeMatrix, {0U, editor.DeclareType(vector).value(), count});
}

rdcspv::Operation SPIRVPointer::decl(SPIRVEditor &editor) const
{
  return rdcspv::Operation(spv::OpTypePointer, {0U, (uint32_t)storage, baseId.value()});
}

rdcspv::Operation SPIRVImage::decl(SPIRVEditor &editor) const
{
  return rdcspv::Operation(spv::OpTypeImage, {0U, editor.DeclareType(retType).value(), (uint32_t)dim,
                                              depth, arrayed, ms, sampled, (uint32_t)format});
}

rdcspv::Operation SPIRVSampler::decl(SPIRVEditor &editor) const
{
  return rdcspv::Operation(spv::OpTypeSampler, {0U});
}

rdcspv::Operation SPIRVSampledImage::decl(SPIRVEditor &editor) const
{
  return rdcspv::Operation(spv::OpTypeSampledImage, {0U, baseId.value()});
}

rdcspv::Operation SPIRVFunction::decl(SPIRVEditor &editor) const
{
  std::vector<uint32_t> words;

  words.push_back(0U);
  words.push_back(returnId.value());
  for(rdcspv::Id id : argumentIds)
    words.push_back(id.value());

  return rdcspv::Operation(spv::OpTypeFunction, words);
}

SPIRVEditor::SPIRVEditor(std::vector<uint32_t> &spirvWords) : spirv(spirvWords)
{
  if(spirv.size() < FirstRealWord || spirv[0] != spv::MagicNumber)
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

  for(rdcspv::Iter it(spirv, FirstRealWord); it; it++)
  {
    spv::Op opcode = it.opcode();

    if(opcode == spv::OpCapability)
    {
      START_SECTION(SPIRVSection::Capabilities);
    }
    else if(opcode == spv::OpExtension)
    {
      START_SECTION(SPIRVSection::Extensions);
    }
    else if(opcode == spv::OpExtInstImport)
    {
      START_SECTION(SPIRVSection::ExtInst);
    }
    else if(opcode == spv::OpMemoryModel)
    {
      START_SECTION(SPIRVSection::MemoryModel);
    }
    else if(opcode == spv::OpEntryPoint)
    {
      START_SECTION(SPIRVSection::EntryPoints);
    }
    else if(opcode == spv::OpExecutionMode || opcode == spv::OpExecutionModeId)
    {
      START_SECTION(SPIRVSection::ExecutionMode);
    }
    else if(opcode == spv::OpString || opcode == spv::OpSource ||
            opcode == spv::OpSourceContinued || opcode == spv::OpSourceExtension ||
            opcode == spv::OpName || opcode == spv::OpMemberName || opcode == spv::OpModuleProcessed)
    {
      START_SECTION(SPIRVSection::Debug);
    }
    else if(opcode == spv::OpDecorate || opcode == spv::OpMemberDecorate ||
            opcode == spv::OpGroupDecorate || opcode == spv::OpGroupMemberDecorate ||
            opcode == spv::OpDecorationGroup || opcode == spv::OpDecorateStringGOOGLE ||
            opcode == spv::OpMemberDecorateStringGOOGLE)
    {
      START_SECTION(SPIRVSection::Annotations);
    }
    else if(opcode == spv::OpFunction)
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
  RDCASSERTEQUAL(sections[SPIRVSection::First].startOffset, FirstRealWord);

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
  for(size_t i = FirstRealWord; i < spirv.size();)
  {
    while(spirv[i] == rdcspv::OpNopWord)
    {
      spirv.erase(spirv.begin() + i);
      addWords(i, -1);
    }

    uint32_t len = spirv[i] >> spv::WordCountShift;

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

  rdcspv::Operation op(spv::OpName, uintName);

  rdcspv::Iter it;

  // OpName must be before OpModuleProcessed.
  for(it = Begin(SPIRVSection::Debug); it < End(SPIRVSection::Debug); ++it)
  {
    if(it.opcode() == spv::OpModuleProcessed)
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

void SPIRVEditor::AddCapability(spv::Capability cap)
{
  // don't add duplicate capabilities
  if(capabilities.find(cap) != capabilities.end())
    return;

  // insert the operation at the very start
  rdcspv::Operation op(spv::OpCapability, {(uint32_t)cap});
  op.insertInto(spirv, FirstRealWord);
  RegisterOp(rdcspv::Iter(spirv, FirstRealWord));
  addWords(FirstRealWord, op.size());
}

void SPIRVEditor::AddExtension(const std::string &extension)
{
  // don't add duplicate extensions
  if(extensions.find(extension) != extensions.end())
    return;

  // start at the beginning
  rdcspv::Iter it(spirv, FirstRealWord);

  // skip past any capabilities
  while(it.opcode() == spv::OpCapability)
    it++;

  // insert the extension instruction
  size_t sz = extension.size();
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], extension.c_str(), sz);

  rdcspv::Operation op(spv::OpExtension, uintName);
  op.insertInto(spirv, it.offs());
  RegisterOp(it);
  addWords(it.offs(), op.size());
}

void SPIRVEditor::AddExecutionMode(rdcspv::Id entry, spv::ExecutionMode mode,
                                   std::vector<uint32_t> params)
{
  size_t offset = sections[SPIRVSection::ExecutionMode].endOffset;

  params.insert(params.begin(), (uint32_t)mode);
  params.insert(params.begin(), entry.value());

  rdcspv::Operation op(spv::OpExecutionMode, params);
  op.insertInto(spirv, offset);
  RegisterOp(rdcspv::Iter(spirv, offset));
  addWords(offset, op.size());
}

rdcspv::Id SPIRVEditor::ImportExtInst(const char *setname)
{
  rdcspv::Id ret = extSets[setname];

  if(ret)
    return ret;

  // start at the beginning
  rdcspv::Iter it(spirv, FirstRealWord);

  // skip past any capabilities and extensions
  while(it.opcode() == spv::OpCapability || it.opcode() == spv::OpExtension)
    it++;

  // insert the import instruction
  ret = MakeId();

  size_t sz = strlen(setname);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], setname, sz);

  uintName.insert(uintName.begin(), ret.value());

  rdcspv::Operation op(spv::OpExtInstImport, uintName);
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
    if(it.word(2) == id.value())
      return it;
    it++;
  }

  return rdcspv::Iter();
}

rdcspv::Id SPIRVEditor::DeclareStructType(const std::vector<rdcspv::Id> &members)
{
  std::vector<uint32_t> words(members.size());
  memcpy(words.data(), members.data(), words.size() * sizeof(uint32_t));
  rdcspv::Id typeId = MakeId();
  words.insert(words.begin(), typeId.value());
  AddType(rdcspv::Operation(spv::OpTypeStruct, words));
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
  spv::Op opcode = it.opcode();

  {
    bool hasResult = false, hasResultType = false;
    spv::HasResultAndType(opcode, &hasResult, &hasResultType);

    if(hasResult && hasResultType)
    {
      RDCASSERT(it.word(2) < idTypes.size());
      idTypes[it.word(2)] = rdcspv::Id::fromWord(it.word(1));
    }
  }

  if(opcode == spv::OpEntryPoint)
  {
    SPIRVEntry entry;
    entry.id = rdcspv::Id::fromWord(it.word(2));
    entry.name = (const char *)&it.word(3);

    entries.push_back(entry);
  }
  else if(opcode == spv::OpMemoryModel)
  {
    addressmodel = (spv::AddressingModel)it.word(2);
    memorymodel = (spv::MemoryModel)it.word(3);
  }
  else if(opcode == spv::OpCapability)
  {
    capabilities.insert((spv::Capability)it.word(1));
  }
  else if(opcode == spv::OpExtension)
  {
    const char *name = (const char *)&it.word(1);
    extensions.insert(name);
  }
  else if(opcode == spv::OpExtInstImport)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    const char *name = (const char *)&it.word(2);
    extSets[name] = id;
  }
  else if(opcode == spv::OpFunction)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(2));
    idOffsets[id.value()] = it.offs();

    functions.push_back(id);
  }
  else if(opcode == spv::OpVariable)
  {
    SPIRVVariable var;
    var.type = rdcspv::Id::fromWord(it.word(1));
    var.id = rdcspv::Id::fromWord(it.word(2));
    var.storageClass = (spv::StorageClass)it.word(3);
    if(it.size() > 4)
      var.init = rdcspv::Id::fromWord(it.word(4));

    variables.push_back(var);
  }
  else if(opcode == spv::OpDecorate)
  {
    SPIRVDecoration decoration;
    decoration.id = rdcspv::Id::fromWord(it.word(1));
    decoration.dec = (spv::Decoration)it.word(2);

    RDCASSERTMSG("Too many parameters in decoration", it.size() <= 7, it.size());

    for(size_t i = 0; i + 3 < it.size() && i < ARRAY_COUNT(decoration.parameters); i++)
      decoration.parameters[i] = it.word(i + 3);

    auto it = std::lower_bound(decorations.begin(), decorations.end(), decoration);
    decorations.insert(it, decoration);

    if(decoration.dec == spv::DecorationDescriptorSet)
      bindings[decoration.id].set = decoration.parameters[0];
    if(decoration.dec == spv::DecorationBinding)
      bindings[decoration.id].binding = decoration.parameters[0];
  }
  else if(opcode == spv::OpTypeVoid || opcode == spv::OpTypeBool || opcode == spv::OpTypeInt ||
          opcode == spv::OpTypeFloat)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    idOffsets[id.value()] = it.offs();

    SPIRVScalar scalar(it);
    scalarTypes[scalar] = id;
  }
  else if(opcode == spv::OpTypeVector)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    idOffsets[id.value()] = it.offs();

    rdcspv::Iter scalarIt = GetID(rdcspv::Id::fromWord(it.word(2)));

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    vectorTypes[SPIRVVector(scalarIt, it.word(3))] = id;
  }
  else if(opcode == spv::OpTypeMatrix)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    idOffsets[id.value()] = it.offs();

    rdcspv::Iter vectorIt = GetID(rdcspv::Id::fromWord(it.word(2)));

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", it.word(2));
      return;
    }

    rdcspv::Iter scalarIt = GetID(rdcspv::Id::fromWord(vectorIt.word(2)));
    uint32_t vectorDim = vectorIt.word(3);

    matrixTypes[SPIRVMatrix(SPIRVVector(scalarIt, vectorDim), it.word(3))] = id;
  }
  else if(opcode == spv::OpTypeImage)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    idOffsets[id.value()] = it.offs();

    rdcspv::Iter scalarIt = GetID(rdcspv::Id::fromWord(it.word(2)));

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    imageTypes[SPIRVImage(scalarIt, (spv::Dim)it.word(3), it.word(4), it.word(5), it.word(6),
                          it.word(7), (spv::ImageFormat)it.word(8))] = id;
  }
  else if(opcode == spv::OpTypeSampler)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    idOffsets[id.value()] = it.offs();

    samplerTypes[SPIRVSampler()] = id;
  }
  else if(opcode == spv::OpTypeSampledImage)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    idOffsets[id.value()] = it.offs();

    rdcspv::Id base = rdcspv::Id::fromWord(it.word(2));

    sampledImageTypes[SPIRVSampledImage(base)] = id;
  }
  else if(opcode == spv::OpTypePointer)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    idOffsets[id.value()] = it.offs();

    pointerTypes[SPIRVPointer(rdcspv::Id::fromWord(it.word(3)), (spv::StorageClass)it.word(2))] = id;
  }
  else if(opcode == spv::OpTypeStruct)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    idOffsets[id.value()] = it.offs();

    structTypes.insert(id);
  }
  else if(opcode == spv::OpTypeFunction)
  {
    rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
    idOffsets[id.value()] = it.offs();

    std::vector<rdcspv::Id> args;

    for(size_t i = 3; i < it.size(); i++)
      args.push_back(rdcspv::Id::fromWord(it.word(i)));

    functionTypes[SPIRVFunction(rdcspv::Id::fromWord(it.word(2)), args)] = id;
  }
}

void SPIRVEditor::UnregisterOp(rdcspv::Iter it)
{
  spv::Op opcode = it.opcode();

  {
    bool hasResult = false, hasResultType = false;
    spv::HasResultAndType(opcode, &hasResult, &hasResultType);

    if(hasResult && hasResultType)
      idTypes[it.word(2)] = rdcspv::Id();
  }

  rdcspv::Id id;

  if(opcode == spv::OpEntryPoint)
  {
    for(auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
      if(entryIt->id == it.word(2))
      {
        entries.erase(entryIt);
        break;
      }
    }
  }
  else if(opcode == spv::OpFunction)
  {
    id = rdcspv::Id::fromWord(it.word(2));
    for(auto funcIt = functions.begin(); funcIt != functions.end(); ++funcIt)
    {
      if(*funcIt == id)
      {
        functions.erase(funcIt);
        break;
      }
    }
  }
  else if(opcode == spv::OpVariable)
  {
    id = rdcspv::Id::fromWord(it.word(2));
    for(auto varIt = variables.begin(); varIt != variables.end(); ++varIt)
    {
      if(varIt->id == id)
      {
        variables.erase(varIt);
        break;
      }
    }
  }
  else if(opcode == spv::OpDecorate)
  {
    SPIRVDecoration decoration;
    decoration.id = rdcspv::Id::fromWord(it.word(1));
    decoration.dec = (spv::Decoration)it.word(2);

    RDCASSERTMSG("Too many parameters in decoration", it.size() <= 7, it.size());

    for(size_t i = 0; i + 3 < it.size() && i < ARRAY_COUNT(decoration.parameters); i++)
      decoration.parameters[i] = it.word(i + 3);

    auto it = std::find(decorations.begin(), decorations.end(), decoration);
    if(it != decorations.end())
      decorations.erase(it);

    if(decoration.dec == spv::DecorationDescriptorSet)
      bindings[decoration.id].set = SPIRVBinding().set;
    if(decoration.dec == spv::DecorationBinding)
      bindings[decoration.id].binding = SPIRVBinding().binding;
  }
  else if(opcode == spv::OpCapability)
  {
    capabilities.erase((spv::Capability)it.word(1));
  }
  else if(opcode == spv::OpExtension)
  {
    const char *name = (const char *)&it.word(1);
    extensions.erase(name);
  }
  else if(opcode == spv::OpExtInstImport)
  {
    const char *name = (const char *)&it.word(2);
    extSets.erase(name);
  }
  else if(opcode == spv::OpTypeVoid || opcode == spv::OpTypeBool || opcode == spv::OpTypeInt ||
          opcode == spv::OpTypeFloat)
  {
    id = rdcspv::Id::fromWord(it.word(1));

    SPIRVScalar scalar(it);
    scalarTypes.erase(scalar);
  }
  else if(opcode == spv::OpTypeVector)
  {
    id = rdcspv::Id::fromWord(it.word(1));

    rdcspv::Iter scalarIt = GetID(rdcspv::Id::fromWord(it.word(2)));

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    vectorTypes.erase(SPIRVVector(scalarIt, it.word(3)));
  }
  else if(opcode == spv::OpTypeMatrix)
  {
    id = rdcspv::Id::fromWord(it.word(1));

    rdcspv::Iter vectorIt = GetID(rdcspv::Id::fromWord(it.word(2)));

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", it.word(2));
      return;
    }

    rdcspv::Iter scalarIt = GetID(rdcspv::Id::fromWord(vectorIt.word(2)));
    uint32_t vectorDim = vectorIt.word(3);

    matrixTypes.erase(SPIRVMatrix(SPIRVVector(scalarIt, vectorDim), it.word(3)));
  }
  else if(opcode == spv::OpTypeImage)
  {
    id = rdcspv::Id::fromWord(it.word(1));

    rdcspv::Iter scalarIt = GetID(rdcspv::Id::fromWord(it.word(2)));

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    imageTypes.erase(SPIRVImage(scalarIt, (spv::Dim)it.word(3), it.word(4), it.word(5), it.word(6),
                                it.word(7), (spv::ImageFormat)it.word(8)));
  }
  else if(opcode == spv::OpTypeSampler)
  {
    id = rdcspv::Id::fromWord(it.word(1));

    samplerTypes.erase(SPIRVSampler());
  }
  else if(opcode == spv::OpTypeSampledImage)
  {
    id = rdcspv::Id::fromWord(it.word(1));

    rdcspv::Id base = rdcspv::Id::fromWord(it.word(2));

    sampledImageTypes.erase(SPIRVSampledImage(base));
  }
  else if(opcode == spv::OpTypePointer)
  {
    id = rdcspv::Id::fromWord(it.word(1));

    pointerTypes.erase(SPIRVPointer(rdcspv::Id::fromWord(it.word(3)), (spv::StorageClass)it.word(2)));
  }
  else if(opcode == spv::OpTypeStruct)
  {
    id = rdcspv::Id::fromWord(it.word(1));

    structTypes.erase(id);
  }
  else if(opcode == spv::OpTypeFunction)
  {
    id = rdcspv::Id::fromWord(it.word(1));

    std::vector<rdcspv::Id> args;

    for(size_t i = 3; i < it.size(); i++)
      args.push_back(rdcspv::Id::fromWord(it.word(i)));

    functionTypes.erase(SPIRVFunction(rdcspv::Id::fromWord(it.word(2)), args));
  }

  if(id)
    idOffsets[id.value()] = 0;
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

  rdcspv::Id entryId = ed.GetEntries()[0].id;

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