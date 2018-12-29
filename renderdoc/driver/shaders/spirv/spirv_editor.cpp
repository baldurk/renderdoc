/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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
#include <utility>
#include "common/common.h"
#include "serialise/serialiser.h"

static const uint32_t FirstRealWord = 5;

template <>
std::string DoStringise(const SPIRVId &el)
{
  return StringFormat::Fmt("%u", el.id);
}

void SPIRVOperation::nopRemove(size_t idx, size_t count)
{
  RDCASSERT(idx >= 1);
  size_t oldSize = size();

  if(count == 0)
    count = oldSize - idx;

  // reduce the size of this op
  *iter = MakeHeader(iter.opcode(), oldSize - count);

  if(idx + count < oldSize)
  {
    // move any words on the end into the middle, then nop them
    for(size_t i = 0; i < count; i++)
    {
      iter.word(idx + i) = iter.word(idx + count + i);
      iter.word(oldSize - i - 1) = SPV_NOP;
    }
  }
  else
  {
    for(size_t i = 0; i < count; i++)
    {
      iter.word(idx + i) = SPV_NOP;
    }
  }
}

void SPIRVOperation::nopRemove()
{
  for(size_t i = 0, sz = size(); i < sz; i++)
    iter.word(i) = SPV_NOP;
}

SPIRVScalar::SPIRVScalar(SPIRVIterator it)
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

SPIRVOperation SPIRVVector::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypeVector, {0U, editor.DeclareType(scalar), count});
}

SPIRVOperation SPIRVMatrix::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypeMatrix, {0U, editor.DeclareType(vector), count});
}

SPIRVOperation SPIRVPointer::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypePointer, {0U, (uint32_t)storage, baseId});
}

SPIRVOperation SPIRVImage::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypeImage, {0U, editor.DeclareType(retType), (uint32_t)dim, depth,
                                           arrayed, ms, sampled, (uint32_t)format});
}

SPIRVOperation SPIRVSampledImage::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypeSampledImage, {0U, baseId});
}

SPIRVOperation SPIRVFunction::decl(SPIRVEditor &editor) const
{
  std::vector<uint32_t> words;

  words.push_back(0U);
  words.push_back(returnId);
  for(SPIRVId id : argumentIds)
    words.push_back(id);

  return SPIRVOperation(spv::OpTypeFunction, words);
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
    sections[section].startOffset = it.offset;

  for(SPIRVIterator it(spirv, FirstRealWord); it; it++)
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
      spirv.insert(spirv.begin() + sections[s].startOffset, SPV_NOP);
      sections[s].endOffset++;

      for(uint32_t t = s + 1; t < SPIRVSection::Count; t++)
      {
        sections[t].startOffset++;
        sections[t].endOffset++;
      }
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
    while(spirv[i] == SPV_NOP)
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

SPIRVId SPIRVEditor::MakeId()
{
  uint32_t ret = spirv[3];
  spirv[3]++;
  idOffsets.resize(spirv[3]);
  return ret;
}

void SPIRVEditor::SetName(uint32_t id, const char *name)
{
  size_t sz = strlen(name);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], name, sz);

  uintName.insert(uintName.begin(), id);

  SPIRVOperation op(spv::OpName, uintName);

  SPIRVIterator it;

  // OpName must be before OpModuleProcessed.
  for(it = Begin(SPIRVSection::Debug); it < End(SPIRVSection::Debug); ++it)
  {
    if(it.opcode() == spv::OpModuleProcessed)
      break;
  }

  spirv.insert(spirv.begin() + it.offs(), op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, it.offs()));
  addWords(it.offs(), op.size());
}

void SPIRVEditor::AddDecoration(const SPIRVOperation &op)
{
  size_t offs = sections[SPIRVSection::Annotations].endOffset;

  spirv.insert(spirv.begin() + offs, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offs));
  addWords(offs, op.size());
}

void SPIRVEditor::AddCapability(spv::Capability cap)
{
  // don't add duplicate capabilities
  if(capabilities.find(cap) != capabilities.end())
    return;

  // insert the operation at the very start
  SPIRVOperation op(spv::OpCapability, {(uint32_t)cap});
  spirv.insert(spirv.begin() + FirstRealWord, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, FirstRealWord));
  addWords(FirstRealWord, op.size());
}

void SPIRVEditor::AddExtension(const std::string &extension)
{
  // don't add duplicate extensions
  if(extensions.find(extension) != extensions.end())
    return;

  // start at the beginning
  SPIRVIterator it(spirv, FirstRealWord);

  // skip past any capabilities
  while(it.opcode() == spv::OpCapability)
    it++;

  // insert the extension instruction
  size_t sz = extension.size();
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], extension.c_str(), sz);

  SPIRVOperation op(spv::OpExtension, uintName);
  spirv.insert(spirv.begin() + it.offset, op.begin(), op.end());
  RegisterOp(it);
  addWords(it.offset, op.size());
}

void SPIRVEditor::AddExecutionMode(SPIRVId entry, spv::ExecutionMode mode,
                                   std::vector<uint32_t> params)
{
  size_t offset = sections[SPIRVSection::ExecutionMode].endOffset;

  params.insert(params.begin(), (uint32_t)mode);
  params.insert(params.begin(), (uint32_t)entry);

  SPIRVOperation op(spv::OpExecutionMode, params);
  spirv.insert(spirv.begin() + offset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offset));
  addWords(offset, op.size());
}

SPIRVId SPIRVEditor::ImportExtInst(const char *setname)
{
  SPIRVId ret = extSets[setname];

  if(ret)
    return ret;

  // start at the beginning
  SPIRVIterator it(spirv, FirstRealWord);

  // skip past any capabilities and extensions
  while(it.opcode() == spv::OpCapability || it.opcode() == spv::OpExtension)
    it++;

  // insert the import instruction
  ret = MakeId();

  size_t sz = strlen(setname);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], setname, sz);

  uintName.insert(uintName.begin(), ret);

  SPIRVOperation op(spv::OpExtInstImport, uintName);
  spirv.insert(spirv.begin() + it.offset, op.begin(), op.end());
  RegisterOp(it);
  addWords(it.offset, op.size());

  extSets[setname] = ret;

  return ret;
}

SPIRVId SPIRVEditor::AddType(const SPIRVOperation &op)
{
  size_t offset = sections[SPIRVSection::Types].endOffset;

  SPIRVId id = op[1];
  idOffsets[id] = offset;
  spirv.insert(spirv.begin() + offset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offset));
  addWords(offset, op.size());
  return id;
}

SPIRVId SPIRVEditor::AddVariable(const SPIRVOperation &op)
{
  size_t offset = sections[SPIRVSection::Variables].endOffset;

  SPIRVId id = op[2];
  idOffsets[id] = offset;
  spirv.insert(spirv.begin() + offset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offset));
  addWords(offset, op.size());
  return id;
}

SPIRVId SPIRVEditor::AddConstant(const SPIRVOperation &op)
{
  size_t offset = sections[SPIRVSection::Constants].endOffset;

  SPIRVId id = op[2];
  idOffsets[id] = offset;
  spirv.insert(spirv.begin() + offset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offset));
  addWords(offset, op.size());
  return id;
}

void SPIRVEditor::AddFunction(const SPIRVOperation *ops, size_t count)
{
  idOffsets[ops[0][2]] = spirv.size();

  for(size_t i = 0; i < count; i++)
    spirv.insert(spirv.end(), ops[i].begin(), ops[i].end());

  RegisterOp(SPIRVIterator(spirv, idOffsets[ops[0][2]]));
}

SPIRVIterator SPIRVEditor::GetID(SPIRVId id)
{
  size_t offs = idOffsets[id];

  if(offs)
    return SPIRVIterator(spirv, offs);

  return SPIRVIterator();
}

SPIRVIterator SPIRVEditor::GetEntry(SPIRVId id)
{
  SPIRVIterator it(spirv, sections[SPIRVSection::EntryPoints].startOffset);
  SPIRVIterator end(spirv, sections[SPIRVSection::EntryPoints].endOffset);

  while(it && it < end)
  {
    if(it.word(2) == id)
      return it;
    it++;
  }

  return SPIRVIterator();
}

SPIRVId SPIRVEditor::DeclareStructType(std::vector<uint32_t> members)
{
  SPIRVId typeId = MakeId();
  members.insert(members.begin(), typeId);
  AddType(SPIRVOperation(spv::OpTypeStruct, members));
  return typeId;
}

void SPIRVEditor::AddWord(SPIRVIterator iter, uint32_t word)
{
  if(!iter)
    return;

  // if it's just pointing at a SPIRVOperation, we can just push_back immediately
  if(iter.words != &spirv)
  {
    iter.words->push_back(word);
    return;
  }

  // add word
  spirv.insert(spirv.begin() + iter.offset + iter.size(), word);

  // fix up header
  iter.word(0) = SPIRVOperation::MakeHeader(iter.opcode(), iter.size() + 1);

  // update offsets
  addWords(iter.offset + iter.size(), 1);
}

void SPIRVEditor::AddOperation(SPIRVIterator iter, const SPIRVOperation &op)
{
  if(!iter)
    return;

  // if it's just pointing at a SPIRVOperation, this is invalid
  if(iter.words != &spirv)
    return;

  // add op
  spirv.insert(spirv.begin() + iter.offset, op.begin(), op.end());

  // update offsets
  addWords(iter.offset, op.size());
}

void SPIRVEditor::RegisterOp(SPIRVIterator it)
{
  spv::Op opcode = it.opcode();

  if(opcode == spv::OpEntryPoint)
  {
    SPIRVEntry entry;
    entry.id = it.word(2);
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
    SPIRVId id = it.word(1);
    const char *name = (const char *)&it.word(2);
    extSets[name] = id;
  }
  else if(opcode == spv::OpFunction)
  {
    SPIRVId id = it.word(2);
    idOffsets[id] = it.offset;

    functions.push_back(id);
  }
  else if(opcode == spv::OpTypeVoid || opcode == spv::OpTypeBool || opcode == spv::OpTypeInt ||
          opcode == spv::OpTypeFloat)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVScalar scalar(it);
    scalarTypes[scalar] = id;
  }
  else if(opcode == spv::OpTypeVector)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVIterator scalarIt = GetID(it.word(2));

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    vectorTypes[SPIRVVector(scalarIt, it.word(3))] = id;
  }
  else if(opcode == spv::OpTypeMatrix)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVIterator vectorIt = GetID(it.word(2));

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", it.word(2));
      return;
    }

    SPIRVIterator scalarIt = GetID(vectorIt.word(2));
    uint32_t vectorDim = vectorIt.word(3);

    matrixTypes[SPIRVMatrix(SPIRVVector(scalarIt, vectorDim), it.word(3))] = id;
  }
  else if(opcode == spv::OpTypeImage)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVIterator scalarIt = GetID(it.word(2));

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    imageTypes[SPIRVImage(scalarIt, (spv::Dim)it.word(3), it.word(4), it.word(5), it.word(6),
                          it.word(7), (spv::ImageFormat)it.word(8))] = id;
  }
  else if(opcode == spv::OpTypeSampledImage)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVId base = it.word(2);

    sampledImageTypes[SPIRVSampledImage(base)] = id;
  }
  else if(opcode == spv::OpTypePointer)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    pointerTypes[SPIRVPointer(it.word(3), (spv::StorageClass)it.word(2))] = id;
  }
  else if(opcode == spv::OpTypeStruct)
  {
    idOffsets[it.word(1)] = it.offset;
  }
  else if(opcode == spv::OpTypeFunction)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    std::vector<SPIRVId> args;

    for(size_t i = 3; i < it.size(); i++)
      args.push_back(it.word(i));

    functionTypes[SPIRVFunction(it.word(2), args)] = id;
  }
}

void SPIRVEditor::UnregisterOp(SPIRVIterator it)
{
  spv::Op opcode = it.opcode();

  SPIRVId id;

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
    id = it.word(2);
    for(auto funcIt = functions.begin(); funcIt != functions.end(); ++funcIt)
    {
      if(*funcIt == id)
      {
        functions.erase(funcIt);
        break;
      }
    }
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
    id = it.word(1);

    SPIRVScalar scalar(it);
    scalarTypes.erase(scalar);
  }
  else if(opcode == spv::OpTypeVector)
  {
    id = it.word(1);

    SPIRVIterator scalarIt = GetID(it.word(2));

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    vectorTypes.erase(SPIRVVector(scalarIt, it.word(3)));
  }
  else if(opcode == spv::OpTypeMatrix)
  {
    id = it.word(1);

    SPIRVIterator vectorIt = GetID(it.word(2));

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", it.word(2));
      return;
    }

    SPIRVIterator scalarIt = GetID(vectorIt.word(2));
    uint32_t vectorDim = vectorIt.word(3);

    matrixTypes.erase(SPIRVMatrix(SPIRVVector(scalarIt, vectorDim), it.word(3)));
  }
  else if(opcode == spv::OpTypeImage)
  {
    id = it.word(1);

    SPIRVIterator scalarIt = GetID(it.word(2));

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    imageTypes.erase(SPIRVImage(scalarIt, (spv::Dim)it.word(3), it.word(4), it.word(5), it.word(6),
                                it.word(7), (spv::ImageFormat)it.word(8)));
  }
  else if(opcode == spv::OpTypeSampledImage)
  {
    id = it.word(1);

    SPIRVId base = it.word(2);

    sampledImageTypes.erase(SPIRVSampledImage(base));
  }
  else if(opcode == spv::OpTypePointer)
  {
    id = it.word(1);

    pointerTypes.erase(SPIRVPointer(it.word(3), (spv::StorageClass)it.word(2)));
  }
  else if(opcode == spv::OpTypeStruct)
  {
    id = it.word(1);
  }
  else if(opcode == spv::OpTypeFunction)
  {
    id = it.word(1);

    std::vector<SPIRVId> args;

    for(size_t i = 3; i < it.size(); i++)
      args.push_back(it.word(i));

    functionTypes.erase(SPIRVFunction(it.word(2), args));
  }

  if(id)
    idOffsets[id] = 0;
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

template <>
std::map<SPIRVScalar, SPIRVId> &SPIRVEditor::GetTable<SPIRVScalar>()
{
  return scalarTypes;
}

template <>
std::map<SPIRVVector, SPIRVId> &SPIRVEditor::GetTable<SPIRVVector>()
{
  return vectorTypes;
}

template <>
std::map<SPIRVMatrix, SPIRVId> &SPIRVEditor::GetTable<SPIRVMatrix>()
{
  return matrixTypes;
}

template <>
std::map<SPIRVPointer, SPIRVId> &SPIRVEditor::GetTable<SPIRVPointer>()
{
  return pointerTypes;
}

template <>
std::map<SPIRVImage, SPIRVId> &SPIRVEditor::GetTable<SPIRVImage>()
{
  return imageTypes;
}

template <>
std::map<SPIRVSampledImage, SPIRVId> &SPIRVEditor::GetTable<SPIRVSampledImage>()
{
  return sampledImageTypes;
}

template <>
std::map<SPIRVFunction, SPIRVId> &SPIRVEditor::GetTable<SPIRVFunction>()
{
  return functionTypes;
}

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"
#include "core/core.h"
#include "spirv_common.h"

static void RemoveSection(std::vector<uint32_t> &spirv, size_t offsets[SPIRVSection::Count][2],
                          SPIRVSection::Type section)
{
  SPIRVEditor ed(spirv);

  for(SPIRVIterator it = ed.Begin(section), end = ed.End(section); it < end; it++)
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

  INFO("SPIR-V compilation" << errors);

  // ensure that compilation succeeded
  REQUIRE(spirv.size() > 0);

  // these offsets may change if the compiler changes above. Verify manually with spirv-dis that
  // they should be updated.
  // For convenience the offsets are in bytes (which spirv-dis uses) and are converted in the loops
  // below.
  size_t offsets[SPIRVSection::Count][2] = {
      // Capabilities
      {0x14, 0x2c},
      // Extensions
      {0x2c, 0x6c},
      // ExtInst
      {0x6c, 0x84},
      // MemoryModel
      {0x84, 0x90},
      // EntryPoints
      {0x90, 0xac},
      // ExecutionMode
      {0xac, 0xb8},
      // Debug
      {0xb8, 0x144},
      // Annotations
      {0x144, 0x1a4},
      // TypesVariables
      {0x1a4, 0x2cc},
      // Functions
      {0x2cc, 0x39c},
  };

  SECTION("Check that SPIR-V is correct with no changes")
  {
    SPIRVEditor ed(spirv);

    for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
    {
      INFO("Section " << s);
      CHECK(ed.Begin((SPIRVSection::Type)s).offs() == offsets[s][0] / sizeof(uint32_t));
      CHECK(ed.End((SPIRVSection::Type)s).offs() == offsets[s][1] / sizeof(uint32_t));
    }
  }

  // we remove all sections we consider optional in arbitrary order. We don't care about keeping the
  // SPIR-V valid all we're testing is the section offsets are correct.
  RemoveSection(spirv, offsets, SPIRVSection::Extensions);

  SECTION("Check with extensions removed")
  {
    SPIRVEditor ed(spirv);

    for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
    {
      INFO("Section " << s);
      CHECK(ed.Begin((SPIRVSection::Type)s).offs() == offsets[s][0] / sizeof(uint32_t));
      CHECK(ed.End((SPIRVSection::Type)s).offs() == offsets[s][1] / sizeof(uint32_t));
    }
  }

  RemoveSection(spirv, offsets, SPIRVSection::Debug);

  SECTION("Check with debug removed")
  {
    SPIRVEditor ed(spirv);

    for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
    {
      INFO("Section " << s);
      CHECK(ed.Begin((SPIRVSection::Type)s).offs() == offsets[s][0] / sizeof(uint32_t));
      CHECK(ed.End((SPIRVSection::Type)s).offs() == offsets[s][1] / sizeof(uint32_t));
    }
  }

  RemoveSection(spirv, offsets, SPIRVSection::ExtInst);

  SECTION("Check with extension imports removed")
  {
    SPIRVEditor ed(spirv);

    for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
    {
      INFO("Section " << s);
      CHECK(ed.Begin((SPIRVSection::Type)s).offs() == offsets[s][0] / sizeof(uint32_t));
      CHECK(ed.End((SPIRVSection::Type)s).offs() == offsets[s][1] / sizeof(uint32_t));
    }
  }

  RemoveSection(spirv, offsets, SPIRVSection::ExecutionMode);

  SECTION("Check with execution mode removed")
  {
    SPIRVEditor ed(spirv);

    for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
    {
      INFO("Section " << s);
      CHECK(ed.Begin((SPIRVSection::Type)s).offs() == offsets[s][0] / sizeof(uint32_t));
      CHECK(ed.End((SPIRVSection::Type)s).offs() == offsets[s][1] / sizeof(uint32_t));
    }
  }

  RemoveSection(spirv, offsets, SPIRVSection::Annotations);

  SECTION("Check with annotations removed")
  {
    SPIRVEditor ed(spirv);

    for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
    {
      INFO("Section " << s);
      CHECK(ed.Begin((SPIRVSection::Type)s).offs() == offsets[s][0] / sizeof(uint32_t));
      CHECK(ed.End((SPIRVSection::Type)s).offs() == offsets[s][1] / sizeof(uint32_t));
    }
  }
}

#endif