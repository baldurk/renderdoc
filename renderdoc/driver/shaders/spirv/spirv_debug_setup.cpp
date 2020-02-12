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
}

void Debugger::Parse(const rdcarray<uint32_t> &spirvWords)
{
  Processor::Parse(spirvWords);
}

ShaderDebugTrace *Debugger::BeginDebug(const ShaderStage stage, const rdcstr &entryPoint,
                                       const rdcarray<SpecConstant> &specInfo,
                                       const std::map<size_t, uint32_t> &instructionLines,
                                       uint32_t activeIndex)
{
  ShaderDebugTrace *ret = new ShaderDebugTrace;
  ret->debugger = this;
  this->activeLaneIndex = activeIndex;

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
      initial.changes.push_back({ShaderVariable(), active.ids[v]});

    initial.sourceVars = sourceVars;

    ret.push_back(initial);

    steps++;
  }

  return ret;
}

rdcstr Debugger::GetRawName(Id id)
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
