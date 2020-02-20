/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Baldur Karlsson
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

#include "spirv_debug.h"
#include "common/formatting.h"
#include "spirv_op_helpers.h"

namespace rdcspv
{
void AssignValue(ShaderVariable &dst, const ShaderVariable &src)
{
  dst.value = src.value;

  RDCASSERTEQUAL(dst.members.size(), src.members.size());

  for(size_t i = 0; i < src.members.size(); i++)
    AssignValue(dst.members[i], src.members[i]);
}

ThreadState::ThreadState(int workgroupIdx, GlobalState &globalState) : global(globalState)
{
  workgroupIndex = workgroupIdx;
  nextInstruction = 0;
  done = false;
}

Debugger::Debugger()
{
}

Debugger::~Debugger()
{
  SAFE_DELETE(apiWrapper);
}

void Debugger::Parse(const rdcarray<uint32_t> &spirvWords)
{
  Processor::Parse(spirvWords);
}

ShaderDebugTrace *Debugger::BeginDebug(DebugAPIWrapper *apiWrapper, const ShaderStage stage,
                                       const rdcstr &entryPoint,
                                       const rdcarray<SpecConstant> &specInfo,
                                       const std::map<size_t, uint32_t> &instructionLines,
                                       uint32_t activeIndex)
{
  ShaderDebugTrace *ret = new ShaderDebugTrace;
  ret->debugger = this;
  this->activeLaneIndex = activeIndex;
  this->stage = stage;
  this->apiWrapper = apiWrapper;

  int workgroupSize = stage == ShaderStage::Pixel ? 4 : 1;
  for(int i = 0; i < workgroupSize; i++)
    workgroup.push_back(ThreadState(i, global));

  ThreadState &active = GetActiveLane();

  active.ids.resize(idOffsets.size());

  // evaluate all constants
  for(auto it = constants.begin(); it != constants.end(); it++)
    active.ids[it->first] = EvaluateConstant(it->first, specInfo);

  ret->lineInfo.resize(instructionOffsets.size());
  for(size_t i = 0; i < instructionOffsets.size(); i++)
  {
    auto it = instructionLines.find(instructionOffsets[i]);
    if(it != instructionLines.end())
      ret->lineInfo[i].disassemblyLine = it->second;
    else
      ret->lineInfo[i].disassemblyLine = 0;
  }

  ret->constantBlocks = global.constantBlocks;
  ret->inputs = active.inputs;

  return ret;
}

rdcarray<ShaderDebugState> Debugger::ContinueDebug()
{
  ThreadState &active = GetActiveLane();

  rdcarray<ShaderDebugState> ret;

  // if we've finished, return an empty set to signify that
  if(active.Finished())
    return ret;

  // initialise a blank set of shader variable changes in the first ShaderDebugState
  if(steps == 0)
  {
    ShaderDebugState initial;

    for(const Id &v : active.live)
      initial.changes.push_back({ShaderVariable(), EvaluatePointerVariable(active.ids[v])});

    initial.sourceVars = sourceVars;

    ret.push_back(initial);

    steps++;
  }

  return ret;
}

ShaderVariable Debugger::MakePointerVariable(Id id, const ShaderVariable *v, uint32_t scalar0,
                                             uint32_t scalar1) const
{
  ShaderVariable var;
  var.rows = var.columns = 1;
  var.type = VarType::ULong;
  var.name = GetRawName(id);
  var.isPointer = true;
  // encode the pointer into the first u64v
  var.value.u64v[0] = (uint64_t)(uintptr_t)v;

  // uv[1] overlaps with u64v[0], so start from [2] storing scalar indices
  var.value.uv[2] = scalar0;
  var.value.uv[3] = scalar1;
  // store the base ID of the allocated storage in [4]
  var.value.uv[4] = id.value();
  return var;
}

ShaderVariable Debugger::MakeCompositePointer(const ShaderVariable &base, Id id,
                                              rdcarray<uint32_t> &indices)
{
  const ShaderVariable *leaf = &base;

  // if the base is a plain value, we just start walking down the chain. If the base is a pointer
  // though, we want to step down the chain in the underlying storage, so dereference first.
  if(base.isPointer)
    leaf = (const ShaderVariable *)(uintptr_t)base.value.u64v[0];

  // first walk any struct member/array indices
  size_t i = 0;
  while(!leaf->members.empty())
  {
    RDCASSERT(i < indices.size(), i, indices.size());
    leaf = &leaf->members[indices[i++]];
  }

  // apply any remaining scalar selectors
  uint32_t scalar0 = ~0U, scalar1 = ~0U;

  size_t remaining = indices.size() - i;
  if(remaining == 2)
  {
    scalar0 = indices[i];
    scalar1 = indices[i + 1];
  }
  else if(remaining == 1)
  {
    scalar0 = indices[i];
  }

  return MakePointerVariable(id, leaf, scalar0, scalar1);
}

ShaderVariable Debugger::EvaluatePointerVariable(const ShaderVariable &ptr) const
{
  if(!ptr.isPointer)
    return ptr;

  ShaderVariable ret;
  ret = *(const ShaderVariable *)(uintptr_t)ptr.value.u64v[0];
  ret.name = ptr.name;

  // we don't support pointers to scalars since our 'unit' of pointer is a ShaderVariable, so check
  // if we have scalar indices to apply:
  uint32_t scalar0 = ptr.value.uv[2];
  uint32_t scalar1 = ptr.value.uv[3];

  ShaderValue val;

  if(ret.rows > 1)
  {
    // matrix case

    if(scalar0 != ~0U && scalar1 != ~0U)
    {
      // two indices - selecting a scalar. scalar0 is the first index in the chain so it chooses
      // column
      if(VarTypeByteSize(ret.type) == 8)
        val.u64v[0] = ret.value.u64v[scalar1 * ret.columns + scalar0];
      else
        val.uv[0] = ret.value.uv[scalar1 * ret.columns + scalar0];

      // it's a scalar now, even if it was a matrix before
      ret.rows = ret.columns = 1;
      ret.value = val;
    }
    else if(scalar0 != ~0U)
    {
      // one index, selecting a column
      for(uint32_t row = 0; row < ret.rows; row++)
      {
        if(VarTypeByteSize(ret.type) == 8)
          val.u64v[0] = ret.value.u64v[row * ret.columns + scalar0];
        else
          val.uv[0] = ret.value.uv[row * ret.columns + scalar0];
      }

      // it's a vector now, even if it was a matrix before
      ret.rows = 1;
      ret.value = val;
    }
  }
  else
  {
    // vector case, selecting a scalar (if anything)
    if(scalar0 != ~0U)
    {
      if(VarTypeByteSize(ret.type) == 8)
        val.u64v[0] = ret.value.u64v[scalar0];
      else
        val.uv[0] = ret.value.uv[scalar0];

      // it's a scalar now, even if it was a matrix before
      ret.columns = 1;
      ret.value = val;
    }
  }

  return ret;
}

Id Debugger::GetPointerBaseId(const ShaderVariable &ptr) const
{
  RDCASSERT(ptr.isPointer);

  // we stored the base ID in [4] so that it's always available regardless of access chains
  return Id::fromWord(ptr.value.uv[4]);
}

void Debugger::WriteThroughPointer(const ShaderVariable &ptr, const ShaderVariable &val)
{
  ShaderVariable *storage = (ShaderVariable *)(uintptr_t)ptr.value.u64v[0];

  // we don't support pointers to scalars since our 'unit' of pointer is a ShaderVariable, so check
  // if we have scalar indices to apply:
  uint32_t scalar0 = ptr.value.uv[2];
  uint32_t scalar1 = ptr.value.uv[3];

  // in the common case we don't have scalar selectors. In this case just assign the value
  if(scalar0 == ~0U && scalar1 == ~0U)
  {
    AssignValue(*storage, val);
  }
  else
  {
    // otherwise we need to store only the selected part of this pointer. We assume by SPIR-V
    // validity rules that the incoming value matches the pointed value
    if(storage->rows > 1)
    {
      // matrix case

      if(scalar0 != ~0U && scalar1 != ~0U)
      {
        // two indices - selecting a scalar. scalar0 is the first index in the chain so it chooses
        // column
        if(VarTypeByteSize(storage->type) == 8)
          storage->value.u64v[scalar1 * storage->columns + scalar0] = val.value.u64v[0];
        else
          storage->value.uv[scalar1 * storage->columns + scalar0] = val.value.uv[0];
      }
      else if(scalar0 != ~0U)
      {
        // one index, selecting a column
        for(uint32_t row = 0; row < storage->rows; row++)
        {
          if(VarTypeByteSize(storage->type) == 8)
            storage->value.u64v[row * storage->columns + scalar0] = val.value.u64v[0];
          else
            storage->value.uv[row * storage->columns + scalar0] = val.value.uv[0];
        }
      }
    }
    else
    {
      // vector case, selecting a scalar
      if(VarTypeByteSize(storage->type) == 8)
        storage->value.u64v[scalar0] = val.value.u64v[0];
      else
        storage->value.uv[scalar0] = val.value.uv[0];
    }
  }
}

rdcstr Debugger::GetRawName(Id id) const
{
  return StringFormat::Fmt("_%u", id.value());
}

rdcstr Debugger::GetHumanName(Id id)
{
  // see if we have a dynamic name assigned (to disambiguate), if so use that
  auto it = dynamicNames.find(id);
  if(it != dynamicNames.end())
    return it->second;

  // otherwise try the string first
  rdcstr name = strings[id];

  // if we don't have a string name, we can be sure the id is unambiguous
  if(name.empty())
    return GetRawName(id);

  rdcstr basename = name;

  // otherwise check to see if it's been used before. If so give it a new name
  int alias = 2;
  while(usedNames.find(name) != usedNames.end())
  {
    name = basename + "@" + ToStr(alias);
    alias++;
  }

  usedNames.insert(name);
  dynamicNames[id] = name;

  return name;
}

void Debugger::PreParse(uint32_t maxId)
{
  Processor::PreParse(maxId);

  strings.resize(idTypes.size());
}

void Debugger::PostParse()
{
  Processor::PostParse();

  for(const MemberName &mem : memberNames)
    dataTypes[mem.id].children[mem.member].name = mem.name;

  memberNames.clear();
}

void Debugger::RegisterOp(Iter it)
{
  Processor::RegisterOp(it);

  OpDecoder opdata(it);

  if(opdata.op == Op::String)
  {
    OpString string(it);

    strings[string.result] = string.string;
  }
  else if(opdata.op == Op::Name)
  {
    OpName name(it);

    // technically you could name a string - in that case we ignore the name
    if(strings[name.target].empty())
      strings[name.target] = name.name;
  }
  else if(opdata.op == Op::MemberName)
  {
    OpMemberName memberName(it);

    memberNames.push_back({memberName.type, memberName.member, memberName.name});
  }
}

};    // namespace rdcspv
