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

template <>
std::string DoStringise(const SPIRVId &el)
{
  return StringFormat::Fmt("%u", el.id);
}

SPIRVScalar::SPIRVScalar(SPIRVIterator it)
{
  type = it.opcode();

  if(type != spv::OpTypeBool)
    width = it.word(2);
  else
    width = 0;

  if(type == spv::OpTypeInt)
    signedness = it.word(3) == 1;
  else
    signedness = false;
}

SPIRVEditor::SPIRVEditor(std::vector<uint32_t> &spirvWords) : spirv(spirvWords)
{
  if(spirv.size() < 5 || spirv[0] != spv::MagicNumber)
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

  for(SPIRVIterator it(spirv, 5); it; it++)
  {
    spv::Op opcode = it.opcode();

    // identify entry points
    if(opcode == spv::OpEntryPoint)
    {
      SPIRVEntry entry;
      entry.id = it.word(2);
      entry.name = (const char *)&it.word(3);

      entries.push_back(entry);

      // first entry point marks the start of this section
      if(entryPointSection.startOffset == 0)
        entryPointSection.startOffset = it.offset;
    }
    else
    {
      // once we found the start of this section, the first non-entrypoint instruction marks the end
      if(entryPointSection.startOffset && entryPointSection.endOffset == 0)
        entryPointSection.endOffset = it.offset;
    }

    // apart from OpExecutionMode, the first instruction after the entry points is the debug section
    if(opcode != spv::OpExecutionMode && entryPointSection.endOffset && debugSection.startOffset == 0)
      debugSection.startOffset = it.offset;

    if(opcode == spv::OpDecorate || opcode == spv::OpMemberDecorate ||
       opcode == spv::OpGroupDecorate || opcode == spv::OpGroupMemberDecorate ||
       opcode == spv::OpDecorationGroup)
    {
      // when we hit the first decoration, this is the end of the debug section and the start of the
      // annotation section
      if(decorationSection.startOffset == 0)
      {
        debugSection.endOffset = it.offset;
        decorationSection.startOffset = it.offset;
      }
    }
    else
    {
      // when we stop seeing decoration instructions, this is the end of that section and the start
      // of the types section
      if(decorationSection.startOffset && decorationSection.endOffset == 0)
      {
        decorationSection.endOffset = it.offset;
        typeVarSection.startOffset = it.offset;
      }
    }

    // identify functions
    if(opcode == spv::OpFunction)
    {
      // if we don't have the end of the types/variables section, this is it
      if(typeVarSection.endOffset == 0)
      {
        typeVarSection.endOffset = it.offset;

        // we must have started the debug section, but if there were no annotations at all, we won't
        // have ended it. In that case, end the debug section (which might be empty) and mark the
        // annotation section as empty.
        if(debugSection.endOffset == 0)
        {
          debugSection.endOffset = it.offset;
          decorationSection.startOffset = decorationSection.endOffset = it.offset;
        }
      }

      SPIRVId id = it.word(2);
      idOffsets[id] = it.offset;

      functions.push_back(id);
    }

    // identify declared scalar/vector/matrix types
    if(opcode == spv::OpTypeBool || opcode == spv::OpTypeInt || opcode == spv::OpTypeFloat)
    {
      uint32_t id = it.word(1);
      idOffsets[id] = it.offset;

      SPIRVScalar scalar(it);
      scalarTypes[scalar] = id;
    }

    if(opcode == spv::OpTypeVector)
    {
      uint32_t id = it.word(1);
      idOffsets[id] = it.offset;

      SPIRVIterator scalarIt = GetID(it.word(2));

      if(!scalarIt)
      {
        RDCERR("Vector type declared with unknown scalar component type %u", it.word(2));
        continue;
      }

      vectorTypes[SPIRVVector(scalarIt, it.word(3))] = id;
    }

    if(opcode == spv::OpTypeMatrix)
    {
      uint32_t id = it.word(1);
      idOffsets[id] = it.offset;

      SPIRVIterator vectorIt = GetID(it.word(2));

      if(!vectorIt)
      {
        RDCERR("Matrix type declared with unknown vector component type %u", it.word(2));
        continue;
      }

      SPIRVIterator scalarIt = GetID(vectorIt.word(2));
      uint32_t vectorDim = vectorIt.word(3);

      matrixTypes[SPIRVMatrix(SPIRVVector(scalarIt, vectorDim), it.word(3))] = id;
    }

    if(opcode == spv::OpTypePointer)
    {
      uint32_t id = it.word(1);
      idOffsets[id] = it.offset;

      pointerTypes[SPIRVPointer(it.word(3), (spv::StorageClass)it.word(2))] = id;
    }

    if(opcode == spv::OpTypeFunction)
    {
      uint32_t id = it.word(1);
      idOffsets[id] = it.offset;

      std::vector<SPIRVId> args;

      for(size_t i = 3; i < it.size(); i++)
        args.push_back(it.word(i));

      functionTypes[SPIRVFunction(it.word(2), args)] = id;
    }

    if(opcode == spv::OpTypeVoid)
    {
      voidType = it.word(1);
      idOffsets[voidType] = it.offset;
    }
  }
}

void SPIRVEditor::StripNops()
{
  for(size_t i = 5; i < spirv.size();)
  {
    while(spirv[i] == SPV_NOP)
    {
      spirv.erase(spirv.begin() + i);
      addWords(i, -1);
    }

    i += spirv[i] >> spv::WordCountShift;
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
  addWords(debugSection.endOffset, op.size());
}

void SPIRVEditor::AddDecoration(const SPIRVOperation &op)
{
  spirv.insert(spirv.begin() + decorationSection.endOffset, op.begin(), op.end());
  addWords(decorationSection.endOffset, op.size());
}

void SPIRVEditor::AddType(const SPIRVOperation &op)
{
  idOffsets[op[1]] = typeVarSection.endOffset;
  spirv.insert(spirv.begin() + typeVarSection.endOffset, op.begin(), op.end());
  addWords(typeVarSection.endOffset, op.size());
}

void SPIRVEditor::AddVariable(const SPIRVOperation &op)
{
  idOffsets[op[2]] = typeVarSection.endOffset;
  spirv.insert(spirv.begin() + typeVarSection.endOffset, op.begin(), op.end());
  addWords(typeVarSection.endOffset, op.size());
}

void SPIRVEditor::AddConstant(const SPIRVOperation &op)
{
  idOffsets[op[2]] = typeVarSection.endOffset;
  spirv.insert(spirv.begin() + typeVarSection.endOffset, op.begin(), op.end());
  addWords(typeVarSection.endOffset, op.size());
}

void SPIRVEditor::AddFunction(const SPIRVOperation *ops, size_t count)
{
  idOffsets[ops[0][2]] = spirv.size();

  auto insertIter = spirv.end();
  for(size_t i = 0; i < count; i++)
  {
    spirv.insert(insertIter, ops[i].begin(), ops[i].end());
    insertIter += ops[i].size();
  }
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

  while(it)
  {
    if(it.word(2) == id)
      return it;
    it++;
  }

  return SPIRVIterator();
}

SPIRVIterator SPIRVEditor::GetDebugInstructions()
{
  return SPIRVIterator(spirv, debugSection.startOffset);
}

SPIRVIterator SPIRVEditor::GetDecorationInstructions()
{
  return SPIRVIterator(spirv, decorationSection.startOffset);
}

SPIRVIterator SPIRVEditor::GetTypeInstructions()
{
  return SPIRVIterator(spirv, typeVarSection.startOffset);
}

SPIRVId SPIRVEditor::DeclareType(const SPIRVVoid &)
{
  if(voidType)
    return voidType;

  voidType = MakeId();
  AddType(SPIRVOperation(spv::OpTypeVoid, {voidType}));

  return voidType;
}

SPIRVId SPIRVEditor::DeclareType(const SPIRVScalar &scalar)
{
  auto it = scalarTypes.lower_bound(scalar);
  if(it != scalarTypes.end() && it->first == scalar)
    return it->second;

  SPIRVOperation decl = scalar.decl();
  SPIRVId id = decl[1] = MakeId();
  AddType(decl);

  scalarTypes.insert(it, std::make_pair(scalar, id));

  return id;
}

SPIRVId SPIRVEditor::DeclareType(const SPIRVVector &vector)
{
  auto it = vectorTypes.lower_bound(vector);
  if(it != vectorTypes.end() && it->first == vector)
    return it->second;

  SPIRVId id = MakeId();
  AddType(SPIRVOperation(spv::OpTypeVector, {id, DeclareType(vector.scalar), vector.count}));

  vectorTypes.insert(it, std::make_pair(vector, id));

  return id;
}

SPIRVId SPIRVEditor::DeclareType(const SPIRVMatrix &matrix)
{
  auto it = matrixTypes.lower_bound(matrix);
  if(it != matrixTypes.end() && it->first == matrix)
    return it->second;

  SPIRVId id = MakeId();
  AddType(SPIRVOperation(spv::OpTypeVector, {id, DeclareType(matrix.vector), matrix.count}));

  matrixTypes.insert(it, std::make_pair(matrix, id));

  return id;
}

SPIRVId SPIRVEditor::DeclareType(const SPIRVPointer &pointer)
{
  auto it = pointerTypes.lower_bound(pointer);
  if(it != pointerTypes.end() && it->first == pointer)
    return it->second;

  SPIRVId id = MakeId();
  AddType(SPIRVOperation(spv::OpTypePointer, {id, (uint32_t)pointer.storage, pointer.baseId}));

  pointerTypes.insert(it, std::make_pair(pointer, id));

  return id;
}

SPIRVId SPIRVEditor::DeclareType(const SPIRVFunction &func)
{
  auto it = functionTypes.lower_bound(func);
  if(it != functionTypes.end() && it->first == func)
    return it->second;

  SPIRVId id = MakeId();

  std::vector<uint32_t> words;

  words.push_back(id);
  words.push_back(func.returnId);
  for(SPIRVId id : func.argumentIds)
    words.push_back(id);

  AddType(SPIRVOperation(spv::OpTypeFunction, words));

  functionTypes.insert(it, std::make_pair(func, id));

  return id;
}

SPIRVId SPIRVEditor::DeclareStructType(std::vector<uint32_t> members)
{
  SPIRVId typeId = MakeId();
  members.insert(members.begin(), typeId);
  AddType(SPIRVOperation(spv::OpTypeStruct, members));
  return typeId;
}
void SPIRVEditor::addWords(size_t offs, int32_t num)
{
  // look through every section, any that are >= this point, adjust the offsets
  // note that if we're removing words then any offsets pointing directly to the removed words
  // will go backwards - but they no longer have anywhere valid to point.
  for(LogicalSection *section :
      {&entryPointSection, &debugSection, &decorationSection, &typeVarSection})
  {
    if(section->startOffset >= offs)
      section->startOffset += num;
    if(section->endOffset >= offs)
      section->endOffset += num;
  }

  // look through every id, and do the same
  for(size_t &o : idOffsets)
    if(o >= offs)
      o += num;
}
