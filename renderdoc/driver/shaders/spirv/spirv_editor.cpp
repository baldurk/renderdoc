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

SPIRVEditor::SPIRVEditor(const std::vector<uint32_t> &spirvWords) : spirv(spirvWords)
{
  if(spirv.size() < 5 || spirv[0] != spv::MagicNumber)
  {
    RDCERR("Empty or invalid SPIR-V module");
    return;
  }

  moduleVersion.major = uint8_t((spirv[1] & 0x00ff0000) >> 16);
  moduleVersion.minor = uint8_t((spirv[1] & 0x0000ff00) >> 8);
  generator = spirv[2];
  idBound = SPIRVIterator(spirv, 3);
  ids.resize(*idBound);

  // [4] is reserved
  RDCASSERT(spirv[4] == 0);

  bool decorations = false;

  bool headerSections = false;

  for(SPIRVIterator it(spirv, 5); it; it++)
  {
    spv::Op opcode = it.opcode();

    // identify entry points
    if(opcode == spv::OpEntryPoint)
    {
      SPIRVEntry entry;
      entry.entryPoint = it;
      entry.id = it.word(2);
      entry.name = (const char *)&it.word(3);

      entries.push_back(entry);

      headerSections = true;
    }

    // identify functions
    if(opcode == spv::OpFunction)
    {
      // if we don't have the types/variables iter yet, this is the point to set it
      if(!typeVarSection.iter)
        typeVarSection.iter = it;

      uint32_t id = it.word(2);
      ids[id] = it;

      // if this is an entry point function, point the iter
      for(SPIRVEntry &entry : entries)
      {
        if(entry.id == id)
        {
          entry.function = it;
          break;
        }
      }

      SPIRVFunction func;
      func.id = id;
      func.iter = it;

      functions.push_back(func);
    }

    if(opcode == spv::OpDecorate || opcode == spv::OpMemberDecorate ||
       opcode == spv::OpGroupDecorate || opcode == spv::OpGroupMemberDecorate ||
       opcode == spv::OpDecorationGroup)
    {
      // when we hit the first decoration, this is the insert point for new debug instructions
      if(!decorations)
      {
        debugSection.iter = it;
      }

      decorations = true;
    }
    else
    {
      // when we stop seeing decoration instructions, this is the insert point for them
      if(decorations)
      {
        decorationSection.iter = it;
        decorations = false;
      }
    }

    // identify declared scalar/vector/matrix types
    if(opcode == spv::OpTypeBool || opcode == spv::OpTypeInt || opcode == spv::OpTypeFloat)
    {
      uint32_t id = it.word(1);
      ids[id] = it;

      SPIRVScalar scalar(it);
      scalarTypes[scalar] = id;
    }

    if(opcode == spv::OpTypeVector)
    {
      uint32_t id = it.word(1);
      ids[id] = it;

      SPIRVIterator scalarIt = ids[it.word(2)];

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
      ids[id] = it;

      SPIRVIterator vectorIt = ids[it.word(2)];

      if(!vectorIt)
      {
        RDCERR("Matrix type declared with unknown vector component type %u", it.word(2));
        continue;
      }

      SPIRVIterator scalarIt = ids[vectorIt.word(2)];
      uint32_t vectorDim = vectorIt.word(3);

      matrixTypes[SPIRVMatrix(SPIRVVector(scalarIt, vectorDim), it.word(3))] = id;
    }

    if(opcode == spv::OpTypePointer)
    {
      uint32_t id = it.word(1);
      ids[id] = it;

      pointerTypes[SPIRVPointer(it.word(3), (spv::StorageClass)it.word(2))] = id;
    }
  }
}

std::vector<uint32_t> SPIRVEditor::GetWords()
{
  std::vector<uint32_t> ret(spirv);

  // add sections in reverse order so that each one doesn't perturb the offset for the next
  for(const LogicalSection &section : {typeVarSection, decorationSection, debugSection})
    ret.insert(ret.begin() + section.iter.offset, section.additions.begin(), section.additions.end());

  // remove any Nops

  for(size_t i = 5; i < ret.size();)
  {
    while(ret[i] == SPV_NOP)
      ret.erase(ret.begin() + i);

    i += ret[i] >> spv::WordCountShift;
  }

  return ret;
}

uint32_t SPIRVEditor::MakeId()
{
  if(!idBound)
    return 0;

  uint32_t ret = *idBound;
  (*idBound)++;
  ids.push_back(SPIRVIterator());
  return ret;
}

void SPIRVEditor::SetName(uint32_t id, const char *name)
{
  size_t sz = strlen(name);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], name, sz);

  uintName.insert(uintName.begin(), id);

  SPIRVOperation op(spv::OpName, uintName);

  debugSection.additions.insert(debugSection.additions.end(), op.begin(), op.end());
}

void SPIRVEditor::AddDecoration(const SPIRVOperation &op)
{
  decorationSection.additions.insert(decorationSection.additions.end(), op.begin(), op.end());
}

void SPIRVEditor::AddType(const SPIRVOperation &op)
{
  ids[op[1]] = SPIRVIterator(typeVarSection.additions, typeVarSection.additions.size());
  typeVarSection.additions.insert(typeVarSection.additions.end(), op.begin(), op.end());
}

void SPIRVEditor::AddVariable(const SPIRVOperation &op)
{
  ids[op[2]] = SPIRVIterator(typeVarSection.additions, typeVarSection.additions.size());
  typeVarSection.additions.insert(typeVarSection.additions.end(), op.begin(), op.end());
}

void SPIRVEditor::AddFunction(const SPIRVOperation *ops, size_t count)
{
  // because this is added to the end, we can add this immediately
  ids[ops[0][2]] = SPIRVIterator(spirv, spirv.size());

  auto insertIter = spirv.end();
  for(size_t i = 0; i < count; i++)
  {
    spirv.insert(insertIter, ops[i].begin(), ops[i].end());
    insertIter += ops[i].size();
  }
}

uint32_t SPIRVEditor::DeclareType(const SPIRVScalar &scalar)
{
  auto it = scalarTypes.lower_bound(scalar);
  if(it != scalarTypes.end() && it->first == scalar)
    return it->second;

  SPIRVOperation decl = scalar.decl();
  uint32_t id = decl[1] = MakeId();
  AddType(decl);

  scalarTypes.insert(it, std::make_pair(scalar, id));

  return id;
}

uint32_t SPIRVEditor::DeclareType(const SPIRVVector &vector)
{
  auto it = vectorTypes.lower_bound(vector);
  if(it != vectorTypes.end() && it->first == vector)
    return it->second;

  uint32_t id = MakeId();
  SPIRVOperation decl(spv::OpTypeVector, {id, DeclareType(vector.scalar), vector.count});
  AddType(decl);

  vectorTypes.insert(it, std::make_pair(vector, id));

  return id;
}

uint32_t SPIRVEditor::DeclareType(const SPIRVMatrix &matrix)
{
  auto it = matrixTypes.lower_bound(matrix);
  if(it != matrixTypes.end() && it->first == matrix)
    return it->second;

  uint32_t id = MakeId();
  SPIRVOperation decl(spv::OpTypeVector, {id, DeclareType(matrix.vector), matrix.count});
  AddType(decl);

  matrixTypes.insert(it, std::make_pair(matrix, id));

  return id;
}

uint32_t SPIRVEditor::DeclareType(const SPIRVPointer &pointer)
{
  auto it = pointerTypes.lower_bound(pointer);
  if(it != pointerTypes.end() && it->first == pointer)
    return it->second;

  uint32_t id = MakeId();
  SPIRVOperation decl(spv::OpTypePointer, {id, (uint32_t)pointer.storage, pointer.baseId});
  AddType(decl);

  pointerTypes.insert(it, std::make_pair(pointer, id));

  return id;
}