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
#include <math.h>
#include "common/formatting.h"
#include "spirv_op_helpers.h"

#if defined(_MSC_VER)
#define finite _finite
#endif

static bool ContainsNaNInf(const ShaderVariable &val)
{
  bool ret = false;

  for(const ShaderVariable &member : val.members)
    ret |= ContainsNaNInf(member);

  int count = int(val.rows) * int(val.columns);

  if(val.type == VarType::Float || val.type == VarType::Half)
  {
    for(int i = 0; i < count; i++)
    {
      ret |= isinf(val.value.fv[i]);
      ret |= isnan(val.value.fv[i]) != 0;
    }
  }
  else if(val.type == VarType::Double)
  {
    for(int i = 0; i < count; i++)
    {
      ret |= isinf(val.value.dv[i]);
      ret |= isnan(val.value.dv[i]) != 0;
    }
  }

  return ret;
}

namespace rdcspv
{
ThreadState::ThreadState(uint32_t workgroupIdx, Debugger &debug, const GlobalState &globalState)
    : debugger(debug), global(globalState)
{
  workgroupIndex = workgroupIdx;
  nextInstruction = 0;
  done = false;
}

ThreadState::~ThreadState()
{
  for(StackFrame *stack : callstack)
    delete stack;
  callstack.clear();
}

bool ThreadState::Finished() const
{
  return done || callstack.empty();
}

void ThreadState::FillCallstack(ShaderDebugState &state)
{
  for(const StackFrame *frame : callstack)
    state.callstack.push_back(debugger.GetHumanName(frame->function));
}

void ThreadState::EnterFunction(ShaderDebugState *state, const rdcarray<Id> &arguments)
{
  Iter it = debugger.GetIterForInstruction(nextInstruction);

  RDCASSERT(OpDecoder(it).op == Op::Function);

  OpFunction func(it);
  StackFrame *frame = new StackFrame();
  frame->function = func.result;

  // if there's a previous stack frame, save its live list
  if(!callstack.empty())
  {
    callstack.back()->live = live;
    callstack.back()->sourceVars = sourceVars;
  }

  // start with just globals
  live = debugger.GetLiveGlobals();
  sourceVars = debugger.GetGlobalSourceVars();
  // process the outgoing scope
  if(state)
    ProcessScopeChange(*state, frame->live, live);

  callstack.push_back(frame);

  it++;

  size_t arg = 0;
  while(OpDecoder(it).op == Op::FunctionParameter)
  {
    OpFunctionParameter param(it);

    if(arg <= arguments.size())
    {
      // function parameters are copied into function calls. Thus a function parameter that is a
      // pointer does not have allocated storage for itself, it gets the pointer from the call site
      // copied in and points to whatever storage that is.
      // That means we don't have to allocate anything here, we just set up the ID and copy the
      // value from the argument
      SetDst(state, param.result, ids[arguments[arg]]);
    }
    else
    {
      RDCERR("Not enough function parameters!");
    }

    arg++;
    it++;
  }

  // next should be the start of the first function block
  RDCASSERT(OpDecoder(it).op == Op::Label);
  lastBlock = curBlock = OpLabel(it).result;
  it++;

  size_t numVars = 0;
  Iter varCounter = it;
  while(OpDecoder(varCounter).op == Op::Variable)
  {
    varCounter++;
    numVars++;
  }

  frame->locals.resize(numVars);

  size_t i = 0;
  // handle any variable declarations
  while(OpDecoder(it).op == Op::Variable)
  {
    OpVariable decl(it);

    ShaderVariable &stackvar = frame->locals[i];
    stackvar.name = debugger.GetRawName(decl.result);

    rdcstr sourceName = debugger.GetHumanName(decl.result);

    // don't add source vars - SetDst below will do that
    debugger.AllocateVariable(decl.result, decl.resultType, DebugVariableType::Undefined,
                              sourceName, stackvar);

    if(decl.HasInitializer())
      AssignValue(stackvar, ids[decl.initializer]);

    SetDst(state, decl.result, debugger.MakePointerVariable(decl.result, &stackvar));

    it++;
    i++;
  }

  // next instruction is the first actual instruction we'll execute
  nextInstruction = debugger.GetInstructionForIter(it);
}

const ShaderVariable &ThreadState::GetSrc(Id id)
{
  return ids[id];
}

void ThreadState::SetDst(ShaderDebugState *state, Id id, const ShaderVariable &val)
{
  if(state && ContainsNaNInf(val))
    state->flags |= ShaderEvents::GeneratedNanOrInf;

  ids[id] = val;
  ids[id].name = debugger.GetRawName(id);

  auto it = std::lower_bound(live.begin(), live.end(), id);
  live.insert(it - live.begin(), id);

  if(state)
  {
    ShaderVariableChange change;
    change.after = debugger.EvaluatePointerVariable(ids[id]);
    state->changes.push_back(change);

    debugger.AddSourceVars(sourceVars, id);
  }
}

void ThreadState::ProcessScopeChange(ShaderDebugState &state, const rdcarray<Id> &oldLive,
                                     const rdcarray<Id> &newLive)
{
  // all oldLive (except globals) are going out of scope. all newLive (except globals) are coming
  // into scope

  const rdcarray<Id> &liveGlobals = debugger.GetLiveGlobals();

  for(const Id id : oldLive)
  {
    if(liveGlobals.contains(id))
      continue;

    state.changes.push_back({debugger.EvaluatePointerVariable(ids[id])});
  }

  for(const Id id : newLive)
  {
    if(liveGlobals.contains(id))
      continue;

    state.changes.push_back({ShaderVariable(), debugger.EvaluatePointerVariable(ids[id])});
  }
}

void ThreadState::JumpToLabel(Id target)
{
  lastBlock = curBlock;
  curBlock = target;

  nextInstruction = debugger.GetInstructionForLabel(target) + 1;

  // if jumping to an empty unconditional loop header, continue to the loop block
  Iter it = debugger.GetIterForInstruction(nextInstruction);
  if(it.opcode() == Op::LoopMerge)
  {
    it++;
    if(it.opcode() == Op::Branch)
    {
      JumpToLabel(OpBranch(it).targetLabel);
    }
  }
}

void ThreadState::StepNext(ShaderDebugState *state,
                           const rdcarray<DenseIdMap<ShaderVariable>> &prevWorkgroup)
{
  Iter it = debugger.GetIterForInstruction(nextInstruction);
  nextInstruction++;

  OpDecoder opdata(it);

  // skip OpLine/OpNoLine
  while(opdata.op == Op::Line || opdata.op == Op::NoLine)
  {
    it++;
    nextInstruction++;
    opdata = OpDecoder(it);
  }

  // for now we don't care about structured control flow so skip past merge statements so we process
  // the branch. OpLine can't be in between so we can safely advance
  if(opdata.op == Op::SelectionMerge || opdata.op == Op::LoopMerge)
  {
    it++;
    nextInstruction++;
    opdata = OpDecoder(it);
  }

  switch(opdata.op)
  {
    //////////////////////////////////////////////////////////////////////////////
    //
    // Pointer manipulation opcodes
    //
    //////////////////////////////////////////////////////////////////////////////
    case Op::Load:
    {
      // we currently handle pointers as fixed storage, so a load becomes a copy
      OpLoad load(it);

      // ignore
      (void)load.memoryAccess;

      // get the pointer value, evaluate it (i.e. dereference) and store the result
      SetDst(state, load.result, debugger.EvaluatePointerVariable(GetSrc(load.pointer)));

      break;
    }
    case Op::Store:
    {
      OpStore store(it);

      // ignore
      (void)store.memoryAccess;

      RDCASSERT(ids[store.pointer].type == VarType::GPUPointer);

      // this is the only place we don't use SetDst because it's the only place that "violates" SSA
      // i.e. changes an existing value. That way SetDst can always unconditionally assign values,
      // and only here do we write through pointers

      ShaderVariable val = GetSrc(store.object);

      if(!state)
      {
        debugger.WriteThroughPointer(ids[store.pointer], val);
      }
      else
      {
        ShaderVariable &var = ids[store.pointer];

        if(ContainsNaNInf(val))
          state->flags |= ShaderEvents::GeneratedNanOrInf;

        // if var is a pointer we update the underlying storage and generate at least one change,
        // plus any additional ones for other pointers.
        Id ptrid = debugger.GetPointerBaseId(var);

        rdcarray<ShaderVariableChange> changes;
        ShaderVariableChange basechange;
        basechange.before = debugger.EvaluatePointerVariable(ids[ptrid]);

        rdcarray<Id> &pointers = pointersForId[ptrid];

        changes.resize(pointers.size());

        // for every other pointer, evaluate its value now before
        for(size_t i = 0; i < pointers.size(); i++)
          changes[i].before = debugger.EvaluatePointerVariable(ids[pointers[i]]);

        debugger.WriteThroughPointer(var, val);

        // now evaluate the value after
        for(size_t i = 0; i < pointers.size(); i++)
          changes[i].after = debugger.EvaluatePointerVariable(ids[pointers[i]]);

        // if the pointer we're writing is one of the aliased pointers, be sure we add it even if
        // it's a no-op change
        int ptrIdx = pointers.indexOf(store.pointer);

        if(ptrIdx >= 0)
        {
          state->changes.push_back(changes[ptrIdx]);
          changes.erase(ptrIdx);
        }

        // remove any no-op changes. Some pointers might point to the same ID but a child that
        // wasn't written to. Note that this might not actually mean nothing was changed (if e.g.
        // we're assigning the same value) but that false negative is not a concern.
        changes.removeIf([](const ShaderVariableChange &c) { return c.before == c.after; });

        state->changes.append(changes);

        // always add a change for the base storage variable written itself, even if that's a no-op.
        // This one is not included in any of the pointers lists above
        basechange.after = debugger.EvaluatePointerVariable(ids[ptrid]);
        state->changes.push_back(basechange);
      }

      break;
    }
    case Op::AccessChain:
    {
      OpAccessChain chain(it);

      rdcarray<uint32_t> indices;

      // evaluate the indices
      indices.reserve(chain.indexes.size());
      for(Id id : chain.indexes)
        indices.push_back(GetSrc(id).value.u.x);

      SetDst(state, chain.result,
             debugger.MakeCompositePointer(ids[chain.base], chain.base, indices));

      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Derivative opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    // spec allows the implementation to choose what DPdx means (coarse or fine), so we choose
    // coarse which seems a reasonable default. In future we could driver-detect the selection in
    // use (assuming it's not dynamic base on circumstances)
    case Op::DPdx:
    case Op::DPdxCoarse:
    {
      // these all share a format
      OpDPdx deriv(it);

      // coarse derivatives are identical across the quad, based on the top-left.
      ShaderVariable var = prevWorkgroup[0][deriv.p];
      ShaderVariable other = prevWorkgroup[1][deriv.p];

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.fv[c] = other.value.fv[c] - var.value.fv[c];

      SetDst(state, deriv.result, var);

      break;
    }
    case Op::DPdy:
    case Op::DPdyCoarse:
    {
      // these all share a format
      OpDPdx deriv(it);

      // coarse derivatives are identical across the quad, based on the top-left.
      ShaderVariable var = prevWorkgroup[0][deriv.p];
      ShaderVariable other = prevWorkgroup[2][deriv.p];

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.fv[c] = other.value.fv[c] - var.value.fv[c];

      SetDst(state, deriv.result, var);

      break;
    }
    case Op::DPdxFine:
    case Op::DPdyFine:
    {
      // these all share a format
      OpDPdxFine deriv(it);

      const bool xdirection = (opdata.op == Op::DPdxFine);

      ShaderVariable a, b;

      // we need to figure out the exact pair to use
      int x = workgroupIndex & 1;
      int y = workgroupIndex / 2;

      if(x == 0)
      {
        if(y == 0)
        {
          // top-left
          if(xdirection)
          {
            a = prevWorkgroup[0][deriv.p];
            b = prevWorkgroup[1][deriv.p];
          }
          else
          {
            a = prevWorkgroup[0][deriv.p];
            b = prevWorkgroup[2][deriv.p];
          }
        }
        else
        {
          // bottom-left
          if(xdirection)
          {
            a = prevWorkgroup[2][deriv.p];
            b = prevWorkgroup[3][deriv.p];
          }
          else
          {
            a = prevWorkgroup[0][deriv.p];
            b = prevWorkgroup[2][deriv.p];
          }
        }
      }
      else
      {
        if(y == 0)
        {
          // top-right
          if(xdirection)
          {
            a = prevWorkgroup[0][deriv.p];
            b = prevWorkgroup[1][deriv.p];
          }
          else
          {
            a = prevWorkgroup[1][deriv.p];
            b = prevWorkgroup[3][deriv.p];
          }
        }
        else
        {
          // bottom-right
          if(xdirection)
          {
            a = prevWorkgroup[2][deriv.p];
            b = prevWorkgroup[3][deriv.p];
          }
          else
          {
            a = prevWorkgroup[1][deriv.p];
            b = prevWorkgroup[3][deriv.p];
          }
        }
      }

      // do the subtract
      for(uint8_t c = 0; c < a.columns; c++)
        a.value.fv[c] = b.value.fv[c] - a.value.fv[c];

      SetDst(state, deriv.result, a);

      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Composite/vector opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::CompositeExtract:
    {
      OpCompositeExtract extract(it);

      // to re-use composite/access chain logic, temporarily make a pointer to the composite
      // (illegal in SPIR-V)
      ShaderVariable ptr =
          debugger.MakeCompositePointer(ids[extract.composite], extract.composite, extract.indexes);

      // then evaluate it, to get the extracted value
      SetDst(state, extract.result, debugger.EvaluatePointerVariable(ptr));

      break;
    }
    case Op::CompositeInsert:
    {
      OpCompositeInsert insert(it);

      ShaderVariable var = GetSrc(insert.composite);
      ShaderVariable obj = GetSrc(insert.object);

      // walk any struct member indices
      ShaderVariable *mod = &var;
      size_t i = 0;
      while(i < insert.indexes.size() && !mod->members.empty())
      {
        mod = &mod->members[insert.indexes[i]];
        i++;
      }

      if(i == insert.indexes.size())
      {
        // if there are no more indices, replace the object here
        mod->value = obj.value;
      }
      else if(i + 1 == insert.indexes.size())
      {
        // one more index
        uint32_t idx = insert.indexes[i];

        // if it's a matrix, replace a whole (column) vector
        if(mod->rows > 1)
        {
          uint32_t column = idx;

          RDCASSERTEQUAL(mod->rows, obj.columns);

          for(uint32_t row = 0; row < mod->rows; row++)
          {
            if(VarTypeByteSize(mod->type) == 8)
              mod->value.u64v[row * mod->columns + column] = obj.value.u64v[row];
            else
              mod->value.uv[row * mod->columns + column] = obj.value.uv[row];
          }
        }
        else
        {
          // if it's a vector, replace one scalar
          if(VarTypeByteSize(mod->type) == 8)
            mod->value.u64v[idx] = obj.value.u64v[0];
          else
            mod->value.uv[idx] = obj.value.uv[0];
        }
      }
      else if(i + 2 == insert.indexes.size())
      {
        // two more indices, selecting column then scalar in a matrix
        uint32_t column = insert.indexes[i];
        uint32_t row = insert.indexes[i + 1];

        if(VarTypeByteSize(mod->type) == 8)
          mod->value.u64v[row * mod->columns + column] = obj.value.u64v[0];
        else
          mod->value.uv[row * mod->columns + column] = obj.value.uv[0];
      }

      // then evaluate it, to get the extracted value
      SetDst(state, insert.result, var);

      break;
    }
    case Op::CompositeConstruct:
    {
      OpCompositeConstruct construct(it);

      ShaderVariable var;

      const DataType &type = debugger.GetType(construct.resultType);

      RDCASSERT(!construct.constituents.empty());

      if(type.type == DataType::ArrayType || type.type == DataType::StructType)
      {
        var.members.resize(construct.constituents.size());
        for(size_t i = 0; i < construct.constituents.size(); i++)
        {
          ShaderVariable &mem = var.members[i];
          mem = GetSrc(construct.constituents[i]);

          if(type.type == DataType::ArrayType)
            mem.name = StringFormat::Fmt("[%zu]", i);
          else
            mem.name = StringFormat::Fmt("_child%zu", i);
        }
      }
      else if(type.type == DataType::VectorType)
      {
        RDCASSERT(construct.constituents.size() <= 4);

        var.type = type.scalar().Type();
        var.rows = 1;
        var.columns = RDCMAX(1U, type.vector().count);

        // it is possible to construct larger vectors from a collection of scalars and smaller
        // vectors.
        size_t dst = 0;
        for(size_t i = 0; i < construct.constituents.size(); i++)
        {
          ShaderVariable src = GetSrc(construct.constituents[i]);

          RDCASSERTEQUAL(src.rows, 1);

          for(size_t j = 0; j < src.columns; j++)
          {
            if(VarTypeByteSize(var.type) == 8)
              var.value.u64v[dst++] = src.value.u64v[j];
            else
              var.value.uv[dst++] = src.value.uv[j];
          }
        }
      }
      else if(type.type == DataType::MatrixType)
      {
        // matrices are constructed from a list of columns
        var.type = type.scalar().Type();
        var.columns = RDCMAX(1U, type.matrix().count);
        var.rows = RDCMAX(1U, type.vector().count);

        RDCASSERTEQUAL(var.columns, construct.constituents.size());

        rdcarray<ShaderVariable> columns;
        columns.resize(construct.constituents.size());
        for(size_t i = 0; i < construct.constituents.size(); i++)
          columns[i] = GetSrc(construct.constituents[i]);

        for(size_t r = 0; r < var.rows; r++)
        {
          for(size_t c = 0; c < var.columns; c++)
          {
            if(VarTypeByteSize(var.type) == 8)
              var.value.u64v[r * var.columns + c] = columns[c].value.u64v[r];
            else
              var.value.uv[r * var.columns + c] = columns[c].value.uv[r];
          }
        }
      }

      SetDst(state, construct.result, var);

      break;
    }
    case Op::VectorShuffle:
    {
      OpVectorShuffle shuffle(it);

      ShaderVariable var;

      const DataType &type = debugger.GetType(shuffle.resultType);

      var.type = type.scalar().Type();
      var.rows = 1;
      var.columns = RDCMAX(1U, (uint32_t)shuffle.components.size());

      ShaderVariable src1 = GetSrc(shuffle.vector1);
      ShaderVariable src2 = GetSrc(shuffle.vector2);

      for(size_t i = 0; i < shuffle.components.size(); i++)
      {
        uint32_t c = shuffle.components[i];
        if(c <= 3)
          var.value.uv[i] = src1.value.uv[c];
        else
          var.value.uv[i] = src2.value.uv[c - 4];
      }

      SetDst(state, shuffle.result, var);

      break;
    }
    case Op::Select:
    {
      OpSelect select(it);

      // we treat this as a composite instruction for the case where the condition is a vector

      ShaderVariable cond = GetSrc(select.condition);

      ShaderVariable var = GetSrc(select.object1);
      ShaderVariable b = GetSrc(select.object2);
      if(cond.columns == 1)
      {
        if(cond.value.u.x == 0)
          var = b;
      }
      else
      {
        for(uint8_t c = 0; c < cond.columns; c++)
        {
          if(cond.value.uv[c] == 0)
          {
            if(VarTypeByteSize(var.type) == 8)
              var.value.u64v[c] = b.value.u64v[c];
            else
              var.value.uv[c] = b.value.uv[c];
          }
        }
      }

      SetDst(state, select.result, var);

      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Conversion opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::ConvertFToS:
    case Op::ConvertFToU:
    case Op::ConvertSToF:
    case Op::ConvertUToF:
    {
      OpConvertFToS conv(it);

      ShaderVariable var = GetSrc(conv.floatValue);

      if(opdata.op == Op::ConvertFToS)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.iv[c] = (int)var.value.fv[c];
        var.type = VarType::SInt;
      }
      else if(opdata.op == Op::ConvertFToU)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = var.value.fv[c] > 0.0f ? (uint32_t)var.value.fv[c] : 0U;
        var.type = VarType::UInt;
      }
      else if(opdata.op == Op::ConvertSToF)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.fv[c] = (float)var.value.iv[c];
        var.type = VarType::Float;
      }
      else if(opdata.op == Op::ConvertUToF)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.fv[c] = (float)var.value.uv[c];
        var.type = VarType::Float;
      }

      SetDst(state, conv.result, var);
      break;
    }
    case Op::Bitcast:
    {
      OpBitcast cast(it);

      const DataType &type = debugger.GetType(cast.resultType);
      ShaderVariable var = GetSrc(cast.operand);

      if((type.type == DataType::ScalarType && var.columns == 1) || type.vector().count == var.columns)
      {
        // if the column count is unchanged, just change the underlying type
        var.type = type.scalar().Type();
      }
      else
      {
        uint32_t srcByteCount = 4;
        if(var.type == VarType::Double || var.type == VarType::ULong || var.type == VarType::SLong)
          srcByteCount = 8;
        else if(var.type == VarType::Half || var.type == VarType::UShort ||
                var.type == VarType::SShort)
          srcByteCount = 2;
        else if(var.type == VarType::UByte || var.type == VarType::SByte)
          srcByteCount = 1;

        uint32_t dstByteCount = type.scalar().width / 8;

        // must be identical bit count
        RDCASSERT(dstByteCount * type.vector().count == srcByteCount * var.columns);

        uint32_t byteSize = VarTypeByteSize(var.type);

        bytebuf bytes;
        for(uint32_t c = 0; c < var.columns; c++)
        {
          if(byteSize == 8)
            bytes.append((const byte *)&var.value.u64v[c], byteSize);
          else
            bytes.append((const byte *)&var.value.uv[c], byteSize);
        }

        var.type = type.scalar().Type();
        var.columns = type.vector().count;
        var.value = ShaderValue();

        byte *b = bytes.data();
        for(uint32_t c = 0; c < var.columns; c++)
        {
          if(byteSize == 8)
            memcpy(&var.value.u64v[c], b, byteSize);
          else
            memcpy(&var.value.uv[c], b, byteSize);
          b += byteSize;
        }
      }

      SetDst(state, cast.result, var);
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Extended instruction set handling
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::ExtInst:
    {
      Id result = Id::fromWord(it.word(2));
      Id extinst = Id::fromWord(it.word(3));

      if(global.extInsts.find(extinst) == global.extInsts.end())
      {
        RDCERR("Unknown extended instruction set %u", extinst.value());
        break;
      }

      const ExtInstDispatcher &dispatch = global.extInsts[extinst];

      // ignore nonsemantic instructions
      if(dispatch.nonsemantic)
        break;

      uint32_t instruction = it.word(4);

      if(instruction >= dispatch.functions.size())
      {
        RDCERR("Unsupported instruction %u in set %s (only %zu instructions defined)", instruction,
               dispatch.name.c_str(), dispatch.functions.size());
        break;
      }

      if(dispatch.functions[instruction] == NULL)
      {
        RDCWARN("Unimplemented extended instruction %s::%s", dispatch.name.c_str(),
                dispatch.names[instruction].c_str());
        break;
      }

      rdcarray<Id> params;
      for(size_t i = 5; i < it.size(); i++)
        params.push_back(Id::fromWord(it.word(i)));

      SetDst(state, result, dispatch.functions[instruction](*this, params));
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Comparison opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::IEqual:
    case Op::INotEqual:
    case Op::UGreaterThan:
    case Op::UGreaterThanEqual:
    case Op::ULessThan:
    case Op::ULessThanEqual:
    case Op::SGreaterThan:
    case Op::SGreaterThanEqual:
    case Op::SLessThan:
    case Op::SLessThanEqual:
    case Op::FOrdEqual:
    case Op::FOrdNotEqual:
    case Op::FOrdGreaterThan:
    case Op::FOrdGreaterThanEqual:
    case Op::FOrdLessThan:
    case Op::FOrdLessThanEqual:
    case Op::FUnordEqual:
    case Op::FUnordNotEqual:
    case Op::FUnordGreaterThan:
    case Op::FUnordGreaterThanEqual:
    case Op::FUnordLessThan:
    case Op::FUnordLessThanEqual:
    {
      OpFMul comp(it);

      ShaderVariable var = GetSrc(comp.operand1);
      ShaderVariable b = GetSrc(comp.operand2);

      if(opdata.op == Op::IEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.uv[c] == b.value.uv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::INotEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.uv[c] != b.value.uv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::UGreaterThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.uv[c] > b.value.uv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::UGreaterThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.uv[c] >= b.value.uv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::ULessThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.uv[c] < b.value.uv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::ULessThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.uv[c] <= b.value.uv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::SGreaterThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.iv[c] > b.value.iv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::SGreaterThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.iv[c] >= b.value.iv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::SLessThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.iv[c] < b.value.iv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::SLessThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.iv[c] <= b.value.iv[c]) ? 1 : 0;
      }

      // FOrd are all "Floating-point comparison if operands are ordered and Operand 1 is ... than
      // Operand 2.".
      // Since NaN is the only unordered value, and NaN comparisons are always false, we can take
      // advantage of that by FOrd just being straight comparisons. If the operands are unordered
      // (i.e. one is NaN) then the FOrd variatns return false as expected.
      //
      // FUnord are all "Floating-point comparison if operands are unordered or Operand 1 is ...
      // than Operand 2."
      // Again as above, any comparison with unordered comparisons will return false. Since we want
      // 'or are unordered' then we want to negate the comparison so that unordered comparisons will
      // always return true. So we negate and invert the actual comparison so that the comparison
      // will be unchanged effectively.

      if(opdata.op == Op::FOrdEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] == b.value.fv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::FOrdNotEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] != b.value.fv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::FOrdGreaterThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] > b.value.fv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::FOrdGreaterThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] >= b.value.fv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::FOrdLessThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] < b.value.fv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::FOrdLessThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] <= b.value.fv[c]) ? 1 : 0;
      }

      if(opdata.op == Op::FUnordEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] != b.value.fv[c]) ? 0 : 1;
      }
      else if(opdata.op == Op::FUnordNotEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] == b.value.fv[c]) ? 0 : 1;
      }
      else if(opdata.op == Op::FUnordGreaterThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] <= b.value.fv[c]) ? 0 : 1;
      }
      else if(opdata.op == Op::FUnordGreaterThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] < b.value.fv[c]) ? 0 : 1;
      }
      else if(opdata.op == Op::FUnordLessThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] >= b.value.fv[c]) ? 0 : 1;
      }
      else if(opdata.op == Op::FUnordLessThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.fv[c] > b.value.fv[c]) ? 0 : 1;
      }

      // TODO we should add a bool type
      var.type = VarType::UInt;

      SetDst(state, comp.result, var);
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Bitwise/logical opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::BitwiseOr:
    case Op::BitwiseAnd:
    case Op::BitwiseXor:
    case Op::ShiftLeftLogical:
    case Op::ShiftRightArithmetic:
    case Op::ShiftRightLogical:
    {
      OpBitwiseOr bitwise(it);

      ShaderVariable var = GetSrc(bitwise.operand1);
      ShaderVariable b = GetSrc(bitwise.operand2);

      if(opdata.op == Op::BitwiseOr)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = var.value.uv[c] | b.value.uv[c];
      }
      else if(opdata.op == Op::BitwiseAnd)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = var.value.uv[c] & b.value.uv[c];
      }
      else if(opdata.op == Op::BitwiseXor)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = var.value.uv[c] ^ b.value.uv[c];
      }
      else if(opdata.op == Op::ShiftLeftLogical)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = var.value.uv[c] << b.value.uv[c];
      }
      else if(opdata.op == Op::ShiftRightArithmetic)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.iv[c] = var.value.iv[c] >> b.value.uv[c];
      }
      else if(opdata.op == Op::ShiftRightLogical)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = var.value.uv[c] >> b.value.uv[c];
      }

      SetDst(state, bitwise.result, var);
      break;
    }
    case Op::Not:
    {
      OpNot bitwise(it);

      ShaderVariable var = GetSrc(bitwise.operand);

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.uv[c] = ~var.value.uv[c];

      SetDst(state, bitwise.result, var);
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Mathematical opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::FMul:
    case Op::FDiv:
    case Op::FMod:
    case Op::FRem:
    case Op::FAdd:
    case Op::FSub:
    case Op::IMul:
    case Op::SDiv:
    case Op::UDiv:
    case Op::UMod:
    case Op::SMod:
    case Op::SRem:
    case Op::IAdd:
    case Op::ISub:
    {
      OpFMul math(it);

      ShaderVariable var = GetSrc(math.operand1);
      ShaderVariable b = GetSrc(math.operand2);

      if(opdata.op == Op::FMul)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.fv[c] *= b.value.fv[c];
      }
      else if(opdata.op == Op::FDiv)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.fv[c] /= b.value.fv[c];
      }
      else if(opdata.op == Op::FMod)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          float af = var.value.fv[c], bf = b.value.fv[c];
          var.value.fv[c] = fmodf(af, bf);
          if(var.value.fv[c] < 0.0f && bf >= 0.0f)
            var.value.fv[c] += fabsf(bf);
          else if(var.value.fv[c] >= 0.0f && bf < 0.0f)
            var.value.fv[c] -= fabsf(bf);
        }
      }
      else if(opdata.op == Op::FRem)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          float af = var.value.fv[c], bf = b.value.fv[c];
          var.value.fv[c] = fmodf(af, bf);
          if(var.value.fv[c] < 0.0f && af >= 0.0f)
            var.value.fv[c] += fabsf(bf);
          else if(var.value.fv[c] >= 0.0f && af < 0.0f)
            var.value.fv[c] -= fabsf(bf);
        }
      }
      else if(opdata.op == Op::FAdd)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.fv[c] += b.value.fv[c];
      }
      else if(opdata.op == Op::FSub)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.fv[c] -= b.value.fv[c];
      }
      else if(opdata.op == Op::IMul)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] *= b.value.uv[c];
      }
      else if(opdata.op == Op::SDiv)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          if(b.value.iv[c] != 0)
          {
            var.value.iv[c] /= b.value.iv[c];
          }
          else
          {
            var.value.uv[c] = ~0U;
            if(state)
              state->flags |= ShaderEvents::GeneratedNanOrInf;
          }
        }
      }
      else if(opdata.op == Op::UDiv)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          if(b.value.uv[c] != 0)
          {
            var.value.uv[c] /= b.value.uv[c];
          }
          else
          {
            var.value.uv[c] = ~0U;
            if(state)
              state->flags |= ShaderEvents::GeneratedNanOrInf;
          }
        }
      }
      else if(opdata.op == Op::UMod)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          if(b.value.uv[c] != 0)
          {
            var.value.uv[c] %= b.value.uv[c];
          }
          else
          {
            var.value.uv[c] = ~0U;
            if(state)
              state->flags |= ShaderEvents::GeneratedNanOrInf;
          }
        }
      }
      else if(opdata.op == Op::SRem || opdata.op == Op::SMod)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          if(b.value.iv[c] != 0)
          {
            var.value.iv[c] %= b.value.iv[c];
          }
          else
          {
            var.value.uv[c] = ~0U;
            if(state)
              state->flags |= ShaderEvents::GeneratedNanOrInf;
          }
        }
      }
      else if(opdata.op == Op::IAdd)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] += b.value.uv[c];
      }
      else if(opdata.op == Op::ISub)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] -= b.value.uv[c];
      }

      SetDst(state, math.result, var);
      break;
    }
    case Op::FNegate:
    case Op::SNegate:
    {
      OpFNegate math(it);

      ShaderVariable var = GetSrc(math.operand);

      if(opdata.op == Op::FNegate)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.fv[c] = -var.value.fv[c];
      }
      else if(opdata.op == Op::SNegate)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.iv[c] = -var.value.iv[c];
      }

      SetDst(state, math.result, var);
      break;
    }
    case Op::Dot:
    {
      OpDot dot(it);

      ShaderVariable var = GetSrc(dot.vector1);
      ShaderVariable b = GetSrc(dot.vector2);

      RDCASSERTEQUAL(var.columns, b.columns);

      float ret = 0;
      for(uint8_t c = 0; c < var.columns; c++)
        ret += var.value.fv[c] * b.value.fv[c];

      var.columns = 1;
      var.value.f.x = ret;

      SetDst(state, dot.result, var);
      break;
    }
    case Op::VectorTimesScalar:
    {
      OpVectorTimesScalar mul(it);

      ShaderVariable var = GetSrc(mul.vector);
      ShaderVariable scalar = GetSrc(mul.scalar);

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.fv[c] *= scalar.value.f.x;

      SetDst(state, mul.result, var);
      break;
    }
    case Op::MatrixTimesScalar:
    {
      OpMatrixTimesScalar mul(it);

      ShaderVariable var = GetSrc(mul.matrix);
      ShaderVariable scalar = GetSrc(mul.scalar);

      for(uint8_t c = 0; c < var.rows * var.columns; c++)
        var.value.fv[c] *= scalar.value.f.x;

      SetDst(state, mul.result, var);
      break;
    }
    case Op::VectorTimesMatrix:
    {
      OpVectorTimesMatrix mul(it);

      ShaderVariable matrix = GetSrc(mul.matrix);
      ShaderVariable vector = GetSrc(mul.vector);

      ShaderVariable var = vector;
      var.columns = matrix.columns;

      float *m = matrix.value.fv;
      float *v = vector.value.fv;

      const DataType &type = debugger.GetType(mul.resultType);
      RDCASSERTEQUAL(type.vector().count, var.columns);
      RDCASSERTEQUAL(matrix.rows, vector.columns);

      for(uint8_t c = 0; c < matrix.columns; c++)
      {
        var.value.fv[c] = 0.0f;
        for(uint8_t r = 0; r < matrix.rows; r++)
        {
          var.value.fv[c] += m[r * matrix.columns + c] * v[r];
        }
      }

      SetDst(state, mul.result, var);
      break;
    }
    case Op::MatrixTimesVector:
    {
      OpMatrixTimesVector mul(it);

      ShaderVariable matrix = GetSrc(mul.matrix);
      ShaderVariable vector = GetSrc(mul.vector);

      ShaderVariable var = vector;
      var.columns = matrix.rows;

      float *m = matrix.value.fv;
      float *v = vector.value.fv;

      const DataType &type = debugger.GetType(mul.resultType);
      RDCASSERTEQUAL(type.vector().count, var.columns);
      RDCASSERTEQUAL(matrix.columns, vector.columns);

      for(uint8_t r = 0; r < matrix.rows; r++)
      {
        var.value.fv[r] = 0.0f;
        for(uint8_t c = 0; c < matrix.columns; c++)
        {
          var.value.fv[r] += m[r * matrix.columns + c] * v[c];
        }
      }

      SetDst(state, mul.result, var);
      break;
    }
    case Op::MatrixTimesMatrix:
    {
      OpMatrixTimesMatrix mul(it);

      ShaderVariable left = GetSrc(mul.leftMatrix);
      ShaderVariable right = GetSrc(mul.rightMatrix);

      ShaderVariable var = left;
      var.rows = left.rows;
      var.columns = right.columns;

      float *l = left.value.fv;
      float *r = right.value.fv;

      RDCASSERTEQUAL(left.columns, right.rows);

      for(uint8_t dstr = 0; dstr < var.rows; dstr++)
      {
        for(uint8_t dstc = 0; dstc < var.columns; dstc++)
        {
          float &dstval = var.value.fv[dstr * var.columns + dstc];
          dstval = 0.0f;

          for(uint8_t src = 0; src < right.rows; src++)
          {
            dstval += l[dstr * left.columns + src] * r[src * right.columns + dstc];
          }
        }
      }

      SetDst(state, mul.result, var);
      break;
    }
    case Op::OuterProduct:
    {
      OpOuterProduct mul(it);

      ShaderVariable left = GetSrc(mul.vector1);
      ShaderVariable right = GetSrc(mul.vector2);

      ShaderVariable var = left;
      var.rows = left.columns;
      var.columns = right.columns;

      for(uint8_t r = 0; r < var.rows; r++)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          var.value.fv[r * var.columns + c] = left.value.fv[r] * right.value.fv[c];
        }
      }

      SetDst(state, mul.result, var);
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Image opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::SampledImage:
    {
      OpSampledImage sampled(it);

      // we make a little struct out of the combination

      ShaderVariable result;
      result.rows = 1;
      result.columns = 1;
      result.isStruct = true;
      result.members = {GetSrc(sampled.image), GetSrc(sampled.sampler)};

      SetDst(state, opdata.result, result);
      break;
    }
    case Op::Image:
    {
      OpImage image(it);

      ShaderVariable var = GetSrc(image.sampledImage);

      // if this is a struct, pull out the image. Otherwise leave it alone because it's just a
      // reference to a binding which we use as-is.
      if(!var.members.empty())
        var = var.members[0];

      SetDst(state, image.result, var);
      break;
    }
    case Op::ImageFetch:
    case Op::ImageSampleExplicitLod:
    case Op::ImageSampleImplicitLod:
    {
      ShaderVariable img;
      ShaderVariable sampler;
      ShaderVariable uv;
      ShaderVariable ddxCalc;
      ShaderVariable ddyCalc;
      ShaderVariable compare;
      ImageOperandsAndParamDatas operands;
      GatherChannel gather = GatherChannel::Red;

      Id derivId;

      if(opdata.op == Op::ImageFetch)
      {
        OpImageFetch image(it);

        img = GetSrc(image.image);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;
      }
      else if(opdata.op == Op::ImageSampleExplicitLod)
      {
        OpImageSampleExplicitLod image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;
      }
      else if(opdata.op == Op::ImageSampleImplicitLod)
      {
        OpImageSampleImplicitLod image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;

        derivId = image.coordinate;
      }

      if(derivId != Id())
      {
        // calculate DDX/DDY in coarse fashion
        ShaderVariable topleft = prevWorkgroup[0][derivId];
        ShaderVariable topright = prevWorkgroup[1][derivId];
        ShaderVariable bottomleft = prevWorkgroup[2][derivId];

        ddxCalc = ddyCalc = topleft;

        for(uint8_t c = 0; c < ddxCalc.columns; c++)
        {
          ddxCalc.value.fv[c] = topright.value.fv[c] - topleft.value.fv[c];
          ddyCalc.value.fv[c] = bottomleft.value.fv[c] - topleft.value.fv[c];
        }
      }

      // if we have a dynamically combined image sampler, split it up here
      if(!img.members.empty() && !sampler.members.empty())
      {
        img = img.members[0];
        sampler = sampler.members[1];
      }

      const DataType &resultType = debugger.GetType(opdata.resultType);

      RDCASSERT(img.type == VarType::ReadOnlyResource || img.type == VarType::ReadWriteResource);
      RDCASSERT(sampler.type == VarType::Unknown || sampler.type == VarType::ReadOnlyResource ||
                sampler.type == VarType::Sampler);

      // at setup time we stored the texture type for easy access here
      DebugAPIWrapper::TextureType texType =
          (DebugAPIWrapper::TextureType)img.value.uv[TextureTypeVariableSlot];

      ShaderVariable result;

      result.type = resultType.scalar().Type();

      BindpointIndex samplerIndex = BindpointIndex(-1, -1, ~0U);
      if(sampler.type == VarType::Sampler || sampler.type == VarType::ReadOnlyResource)
        samplerIndex = sampler.GetBinding();

      if(!debugger.GetAPIWrapper()->CalculateSampleGather(
             *this, opdata.op, texType, img.GetBinding(), samplerIndex, uv, ddxCalc, ddyCalc,
             compare, gather, operands, result))
      {
        // sample failed. Pretend we got 0 columns back

        result.value.u.x = 0;
        result.value.u.y = 0;
        result.value.u.z = 0;

        if(result.type == VarType::Float || result.type == VarType::Half)
          result.value.f.w = 1.0f;
        else if(result.type == VarType::Double)
          result.value.d.w = 1.0;
        else
          result.value.u.w = 1;
      }

      result.columns = RDCMAX(1U, resultType.vector().count);

      SetDst(state, opdata.result, result);
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Block flow control opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::Label:
    case Op::SelectionMerge:
    case Op::LoopMerge:
    {
      // we shouldn't process these, we should always jump past them
      RDCERR("Unexpected %s", ToStr(opdata.op).c_str());
      break;
    }
    case Op::Switch:
    {
      OpSwitch switch_(it);

      ShaderVariable selector = GetSrc(switch_.selector);

      Id targetLabel = switch_.def;

      for(const PairLiteralIntegerIdRef &case_ : switch_.target)
      {
        if(selector.value.u.x == case_.first)
        {
          targetLabel = case_.second;
          break;
        }
      }

      JumpToLabel(targetLabel);
      break;
    }
    case Op::Branch:
    {
      OpBranch branch(it);
      JumpToLabel(branch.targetLabel);
      break;
    }
    case Op::BranchConditional:
    {
      OpBranchConditional branch(it);

      Id target = branch.falseLabel;
      if(GetSrc(branch.condition).value.u.x)
        target = branch.trueLabel;

      JumpToLabel(target);

      break;
    }
    case Op::Phi:
    {
      OpPhi phi(it);

      ShaderVariable var;

      for(const PairIdRefIdRef &parent : phi.parents)
      {
        if(parent.second == lastBlock)
        {
          var = GetSrc(parent.first);
          break;
        }
      }

      // we should have had a matching for the OpPhi of the block we came from
      RDCASSERT(!var.name.empty());

      SetDst(state, phi.result, var);
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Function flow control opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::FunctionCall:
    {
      OpFunctionCall call(it);

      // we hit this twice. The first time we don't have a return value so we jump into the
      // function. The second time we do have a return value so we process it and continue
      if(returnValue.name.empty())
      {
        uint32_t returnInstruction = nextInstruction - 1;
        nextInstruction = debugger.GetInstructionForFunction(call.function);

        EnterFunction(state, call.arguments);

        RDCASSERT(callstack.back()->function == call.function);
        callstack.back()->funcCallInstruction = returnInstruction;
      }
      else
      {
        SetDst(state, call.result, returnValue);
        returnValue.name.clear();
      }
      break;
    }

    case Op::Return:
    case Op::ReturnValue:
    {
      StackFrame *exitingFrame = callstack.back();
      callstack.pop_back();

      if(callstack.empty())
      {
        // if there's no callstack there's no return address, jump to the function end

        it++;    // see what the next instruction is
        // keep going until it's the end of the function

        while(OpDecoder(it).op != Op::FunctionEnd)
        {
          nextInstruction++;
          it++;
        }
      }
      else
      {
        returnValue.name = "<return value>";
        if(opdata.op == Op::ReturnValue)
        {
          OpReturnValue ret(it);

          returnValue = GetSrc(ret.value);
        }

        nextInstruction = exitingFrame->funcCallInstruction;

        // process the outgoing and incoming scopes
        if(state)
          ProcessScopeChange(*state, live, callstack.back()->live);

        // restore the live list from the calling frame
        live = callstack.back()->live;
        sourceVars = callstack.back()->sourceVars;
      }

      delete exitingFrame;

      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Misc. opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::Undef:
    {
      // this was processed as a constant, since it can appear in the constants section as well as
      // in blocks. Just assign the value to itself so that it shows up as a change
      OpUndef undef(it);

      SetDst(state, undef.result, GetSrc(undef.result));

      break;
    }
    case Op::Nop:
    {
      // nothing to do
      break;
    }

    case Op::SourceContinued:
    case Op::Source:
    case Op::SourceExtension:
    case Op::Name:
    case Op::MemberName:
    case Op::String:
    case Op::Extension:
    case Op::ExtInstImport:
    case Op::MemoryModel:
    case Op::EntryPoint:
    case Op::ExecutionMode:
    case Op::Capability:
    case Op::TypeVoid:
    case Op::TypeBool:
    case Op::TypeInt:
    case Op::TypeFloat:
    case Op::TypeVector:
    case Op::TypeMatrix:
    case Op::TypeImage:
    case Op::TypeSampler:
    case Op::TypeSampledImage:
    case Op::TypeArray:
    case Op::TypeRuntimeArray:
    case Op::TypeStruct:
    case Op::TypeOpaque:
    case Op::TypePointer:
    case Op::TypeFunction:
    case Op::TypeEvent:
    case Op::TypeDeviceEvent:
    case Op::TypeReserveId:
    case Op::TypeQueue:
    case Op::TypePipe:
    case Op::TypeForwardPointer:
    case Op::ConstantTrue:
    case Op::ConstantFalse:
    case Op::Constant:
    case Op::ConstantComposite:
    case Op::ConstantSampler:
    case Op::ConstantNull:
    case Op::SpecConstantTrue:
    case Op::SpecConstantFalse:
    case Op::SpecConstant:
    case Op::SpecConstantComposite:
    case Op::SpecConstantOp:
    case Op::Decorate:
    case Op::MemberDecorate:
    case Op::DecorationGroup:
    case Op::GroupDecorate:
    case Op::GroupMemberDecorate:
    case Op::Unreachable:
    case Op::DecorateString:
    case Op::MemberDecorateString:
    case Op::DecorateId:
    case Op::ModuleProcessed:
    case Op::ExecutionModeId:
    {
      RDCERR("Encountered unexpected global SPIR-V operation %s", ToStr(opdata.op).c_str());
      break;
    }

    case Op::GenericPtrMemSemantics:
    case Op::ImageQueryFormat:
    case Op::ImageQueryOrder:
    case Op::SatConvertSToU:
    case Op::SatConvertUToS:
    case Op::PtrCastToGeneric:
    case Op::GenericCastToPtr:
    case Op::GenericCastToPtrExplicit:
    case Op::IsFinite:
    case Op::IsNormal:
    case Op::SignBitSet:
    case Op::LessOrGreater:
    case Op::Ordered:
    case Op::Unordered:
    case Op::LifetimeStart:
    case Op::LifetimeStop:
    case Op::AtomicCompareExchangeWeak:
    case Op::AtomicFlagTestAndSet:
    case Op::AtomicFlagClear:
    case Op::GroupAsyncCopy:
    case Op::GroupWaitEvents:
    case Op::GetKernelLocalSizeForSubgroupCount:
    case Op::GetKernelMaxNumSubgroups:
    case Op::EnqueueMarker:
    case Op::EnqueueKernel:
    case Op::GetKernelNDrangeSubGroupCount:
    case Op::GetKernelNDrangeMaxSubGroupSize:
    case Op::GetKernelWorkGroupSize:
    case Op::GetKernelPreferredWorkGroupSizeMultiple:
    case Op::RetainEvent:
    case Op::ReleaseEvent:
    case Op::CreateUserEvent:
    case Op::IsValidEvent:
    case Op::SetUserEventStatus:
    case Op::CaptureEventProfilingInfo:
    case Op::GetDefaultQueue:
    case Op::BuildNDRange:
    case Op::TypeNamedBarrier:
    case Op::NamedBarrierInitialize:
    case Op::MemoryNamedBarrier:
    case Op::ReadPipe:
    case Op::WritePipe:
    case Op::ReservedReadPipe:
    case Op::ReservedWritePipe:
    case Op::ReserveReadPipePackets:
    case Op::ReserveWritePipePackets:
    case Op::CommitReadPipe:
    case Op::CommitWritePipe:
    case Op::IsValidReserveId:
    case Op::GetNumPipePackets:
    case Op::GetMaxPipePackets:
    case Op::GroupReserveReadPipePackets:
    case Op::GroupReserveWritePipePackets:
    case Op::GroupCommitReadPipe:
    case Op::GroupCommitWritePipe:
    case Op::TypePipeStorage:
    case Op::ConstantPipeStorage:
    case Op::CreatePipeFromPipeStorage:
    {
      // these are kernel only
      RDCERR("Encountered unexpected kernel SPIR-V operation %s", ToStr(opdata.op).c_str());
      break;
    }

    case Op::Line:
    case Op::NoLine:
    case Op::Function:
    case Op::FunctionParameter:
    case Op::FunctionEnd:
    case Op::Variable:
    {
      // these should be handled elsewhere specially
      RDCERR("Encountered SPIR-V operation %s in general dispatch loop", ToStr(opdata.op).c_str());
      break;
    }

    case Op::Max:
    default: RDCWARN("Unhandled SPIR-V operation %s", ToStr(opdata.op).c_str()); break;
  }

  // skip over any degenerate branches
  while(true)
  {
    it = debugger.GetIterForInstruction(nextInstruction);
    if(it.opcode() == Op::Branch)
    {
      Id target = OpBranch(it).targetLabel;

      it++;

      while(it.opcode() == Op::Line || it.opcode() == Op::NoLine)
        it++;

      if(target == OpLabel(it).result)
      {
        JumpToLabel(target);
        continue;
      }
    }

    break;
  }

  // set the state's next instruction (if we have one) to ours, bounded by how many
  // instructions there are
  if(state)
    state->nextInstruction = RDCMIN(nextInstruction, debugger.GetNumInstructions() - 1);
}
};    // namespace rdcspv
