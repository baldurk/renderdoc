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
  enum class SectionState
  {
    Preamble,          // OpCapability, OpExtension, anything before OpEntryPoint
    EntryPoints,       // REQUIRED: OpEntryPoint
    ExecutionMode,     // OpExecutionMode
    Debug,             // OPTIONAL: debug instructions (OpString, OpSource*, Op*Name, etc)
    Decoration,        // OPTIONAL (technically): Op*Decorate*
    TypeVar,           // REQUIRED: OpType*, OpVariable
    FunctionBodies,    // REQUIRED: OpFunction*
  } section;

  section = SectionState::Preamble;

  for(SPIRVIterator it(spirv, FirstRealWord); it; it++)
  {
    spv::Op opcode = it.opcode();

    if(opcode == spv::OpEntryPoint)
    {
      if(section != SectionState::Preamble && section != SectionState::EntryPoints)
        RDCERR("Unexpected current section when encountering OpEntryPoint: %d", section);

      if(section != SectionState::EntryPoints)
        entryPointSection.startOffset = it.offset;

      section = SectionState::EntryPoints;
    }
    else if(opcode == spv::OpExecutionMode)
    {
      if(section != SectionState::EntryPoints && section != SectionState::ExecutionMode)
        RDCERR("Unexpected current section when encountering OpExecutionMode: %d", section);

      if(section == SectionState::EntryPoints)
        entryPointSection.endOffset = it.offset;

      section = SectionState::ExecutionMode;
    }
    else if(opcode == spv::OpString || opcode == spv::OpSource || opcode == spv::OpSourceContinued ||
            opcode == spv::OpSourceExtension || opcode == spv::OpName || opcode == spv::OpMemberName)
    {
      if(section != SectionState::EntryPoints && section != SectionState::ExecutionMode &&
         section != SectionState::Debug)
        RDCERR("Unexpected current section when encountering debug instruction %s: %d",
               ToStr(opcode).c_str(), section);

      if(section == SectionState::EntryPoints)
        entryPointSection.endOffset = it.offset;

      if(section != SectionState::Debug)
        debugSection.startOffset = it.offset;

      section = SectionState::Debug;
    }
    else if(opcode == spv::OpDecorate || opcode == spv::OpMemberDecorate ||
            opcode == spv::OpGroupDecorate || opcode == spv::OpGroupMemberDecorate ||
            opcode == spv::OpDecorationGroup)
    {
      if(section != SectionState::EntryPoints && section != SectionState::ExecutionMode &&
         section != SectionState::Debug && section != SectionState::Decoration)
        RDCERR("Unexpected current section when encountering decoration instruction %s: %d",
               ToStr(opcode).c_str(), section);

      if(section == SectionState::EntryPoints)
        entryPointSection.endOffset = it.offset;

      if(section == SectionState::Debug)
      {
        debugSection.endOffset = it.offset;
      }
      else if(section != SectionState::Decoration)
      {
        // coming from some other section that isn't debug, insert a dummy debug section
        RDCDEBUG("Debug section is empty, inserting OpNop which will later be stripped");
        spirv.insert(spirv.begin() + it.offset, SPV_NOP);

        debugSection.startOffset = it.offset;
        it.offset++;
        debugSection.endOffset = it.offset;
      }

      if(section != SectionState::Decoration)
        decorationSection.startOffset = it.offset;

      section = SectionState::Decoration;
    }
    else if(opcode == spv::OpFunction)
    {
      if(section != SectionState::FunctionBodies)
      {
        if(section != SectionState::TypeVar)
          RDCERR("Unexpected current section when encountering OpFunction: %d", section);

        // we've now met the function bodies
        section = SectionState::FunctionBodies;

        typeVarSection.endOffset = it.offset;

        if(typeVarSection.startOffset == typeVarSection.endOffset || typeVarSection.startOffset == 0)
          RDCERR("No types found in this shader! There should be at least one for the entry point");
      }
    }
    else
    {
      // if we've reached another instruction, ignore it if we've reached the function bodies. Also
      // ignore during preamble
      if(section != SectionState::FunctionBodies && section != SectionState::Preamble)
      {
        // if it's an instruction not covered above, and we haven't hit the functions, it's a
        // type/variable/constant instruction.
        if(section != SectionState::EntryPoints && section != SectionState::ExecutionMode &&
           section != SectionState::Debug && section != SectionState::Decoration &&
           section != SectionState::TypeVar)
          RDCERR("Unexpected current section when encountering type/variable instruction %s: %d",
                 ToStr(opcode).c_str(), section);

        if(section == SectionState::EntryPoints)
          entryPointSection.endOffset = it.offset;

        if(section == SectionState::Decoration)
        {
          // if we're coming from decoration, all is well. We inserted a dummy debug section if
          // needed above before starting it.
          decorationSection.endOffset = it.offset;
        }
        else if(section == SectionState::Debug)
        {
          debugSection.endOffset = it.offset;

          // if we're coming straight from debug, we need to insert a dummy decoration section
          RDCDEBUG("Decoration section is empty, inserting OpNop which will later be stripped");
          spirv.insert(spirv.begin() + it.offset, SPV_NOP);

          decorationSection.startOffset = it.offset;
          it.offset++;
          decorationSection.endOffset = it.offset;
        }
        else if(section != SectionState::TypeVar)
        {
          // coming from some other section that isn't debug or decoration, insert a dummy debug
          // AND a dummy decoration section.
          RDCDEBUG("Debug/decoration sections empty, inserting OpNops to later be stripped");
          spirv.insert(spirv.begin() + it.offset, SPV_NOP);

          debugSection.startOffset = it.offset;
          it.offset++;
          debugSection.endOffset = it.offset;

          spirv.insert(spirv.begin() + it.offset, SPV_NOP);

          decorationSection.startOffset = it.offset;
          it.offset++;
          decorationSection.endOffset = it.offset;
        }

        if(section != SectionState::TypeVar)
          typeVarSection.startOffset = it.offset;

        section = SectionState::TypeVar;
      }
    }

    RegisterOp(it);
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

  spirv.insert(spirv.begin() + debugSection.endOffset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, debugSection.endOffset));
  addWords(debugSection.endOffset, op.size());
}

void SPIRVEditor::AddDecoration(const SPIRVOperation &op)
{
  spirv.insert(spirv.begin() + decorationSection.endOffset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, decorationSection.endOffset));
  addWords(decorationSection.endOffset, op.size());
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
  SPIRVId id = op[1];
  idOffsets[id] = typeVarSection.endOffset;
  spirv.insert(spirv.begin() + typeVarSection.endOffset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, typeVarSection.endOffset));
  addWords(typeVarSection.endOffset, op.size());
  return id;
}

SPIRVId SPIRVEditor::AddVariable(const SPIRVOperation &op)
{
  SPIRVId id = op[2];
  idOffsets[id] = typeVarSection.endOffset;
  spirv.insert(spirv.begin() + typeVarSection.endOffset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, typeVarSection.endOffset));
  addWords(typeVarSection.endOffset, op.size());
  return id;
}

SPIRVId SPIRVEditor::AddConstant(const SPIRVOperation &op)
{
  SPIRVId id = op[2];
  idOffsets[id] = typeVarSection.endOffset;
  spirv.insert(spirv.begin() + typeVarSection.endOffset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, typeVarSection.endOffset));
  addWords(typeVarSection.endOffset, op.size());
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
  SPIRVIterator it(spirv, entryPointSection.startOffset);
  SPIRVIterator end(spirv, entryPointSection.endOffset);

  while(it && it < end)
  {
    if(it.word(2) == id)
      return it;
    it++;
  }

  return SPIRVIterator();
}

SPIRVIterator SPIRVEditor::BeginEntries()
{
  return SPIRVIterator(spirv, entryPointSection.startOffset);
}

SPIRVIterator SPIRVEditor::BeginDebug()
{
  return SPIRVIterator(spirv, debugSection.startOffset);
}

SPIRVIterator SPIRVEditor::BeginDecorations()
{
  return SPIRVIterator(spirv, decorationSection.startOffset);
}

SPIRVIterator SPIRVEditor::BeginTypes()
{
  return SPIRVIterator(spirv, typeVarSection.startOffset);
}

SPIRVIterator SPIRVEditor::BeginFunctions()
{
  return SPIRVIterator(spirv, typeVarSection.endOffset);
}

SPIRVIterator SPIRVEditor::EndEntries()
{
  return SPIRVIterator(spirv, entryPointSection.endOffset);
}

SPIRVIterator SPIRVEditor::EndDebug()
{
  return SPIRVIterator(spirv, debugSection.endOffset);
}

SPIRVIterator SPIRVEditor::EndDecorations()
{
  return SPIRVIterator(spirv, decorationSection.endOffset);
}

SPIRVIterator SPIRVEditor::EndTypes()
{
  return SPIRVIterator(spirv, typeVarSection.endOffset);
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
  else if(opcode == spv::OpCapability)
  {
    capabilities.insert((spv::Capability)it.word(1));
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
  for(LogicalSection *section :
      {&entryPointSection, &debugSection, &decorationSection, &typeVarSection})
  {
    // we have three cases to consider: either the offset matches start, is within (up to and
    // including end) or is outside the section.
    // We ensured during parsing that all sections were non-empty by adding nops if necessary, so we
    // don't have to worry about the situation where we can't decide if an insert is at the end of
    // one section or inside the next. Note this means we don't support inserting at the start of a
    // section.

    if(offs == section->startOffset)
    {
      // if the offset matches the start, we're appending at the end of the previous section so move
      // both
      section->startOffset += num;
      section->endOffset += num;
    }
    else if(offs > section->startOffset && offs <= section->endOffset)
    {
      // if the offset is in the section (up to and including the end) then we're inserting in this
      // section, so move the end only
      section->endOffset += num;
    }
    else if(section->startOffset >= offs)
    {
      // otherwise move both or neither depending on which side the offset is.
      section->startOffset += num;
      section->endOffset += num;
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
