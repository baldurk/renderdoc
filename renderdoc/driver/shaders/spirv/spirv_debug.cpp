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
ThreadState::ThreadState(int workgroupIdx, Debugger &debug, const GlobalState &globalState)
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

    // TODO we need to handle re-entry into functions (ID lifetime)
    live.removeOne(decl.result);

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
  // if we don't have a state to track, we can take a much faster pa We just update the value and
  // return. We don't have to propagate ID changes between aliased pointers because *internally* we
  // always look up pointers and don't care about aliasing. That's only needed for *external* facing
  // things because we have to update the changes for all copied pointer values.
  if(!state)
  {
    if(ids[id].name.empty())
    {
      // for uninitialised values, init by copying
      ids[id] = val;
      ids[id].name = debugger.GetRawName(id);
      live.push_back(id);

      debugger.AddSourceVars(id);
    }
    else
    {
      RDCASSERT(ids[id].isPointer);
      // otherwise just update the pointed-to value (only pointers should be existing before being
      // assigned)
      debugger.WriteThroughPointer(ids[id], val);
    }

    return;
  }

  // otherwise when we're tracking changes, take a slower pa

  if(ContainsNaNInf(val))
    state->flags |= ShaderEvents::GeneratedNanOrInf;

  ShaderVariable &var = ids[id];

  if(!var.name.empty())
  {
    // if this ID was already initialised, we must have a change of a pointer. Otherwise we're SSA
    // form so no other ID should change.

    RDCASSERT(var.isPointer);

    // if var is a pointer we update the underlying storage and generate at least one change, plus
    // any additional ones for other pointers.
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

    // if the pointer we're writing is one of the aliased pointers, be sure we add it even if it's a
    // no-op change
    int ptrIdx = pointers.indexOf(id);

    if(ptrIdx >= 0)
    {
      state->changes.push_back(changes[ptrIdx]);
      changes.erase(ptrIdx);
    }

    // remove any no-op changes. Some pointers might point to the same ID but a child that wasn't
    // written to. Note that this might not actually mean nothing was changed (if e.g. we're
    // assigning the same value) but that false negative is not a concern.
    changes.removeIf([](const ShaderVariableChange &c) { return c.before == c.after; });

    state->changes.append(changes);

    // always add a change for the base storage variable written itself, even if that's a no-op.
    // This one is not included in any of the pointers lists above
    basechange.after = debugger.EvaluatePointerVariable(ids[ptrid]);
    state->changes.push_back(basechange);
  }
  else
  {
    // otherwise it's a new SSA variable, record a change-from-nothing
    ids[id] = val;
    ids[id].name = debugger.GetRawName(id);
    live.push_back(id);

    ShaderVariableChange change;
    change.after = debugger.EvaluatePointerVariable(ids[id]);
    state->changes.push_back(change);

    debugger.AddSourceVars(id);
  }
}

void ThreadState::StepNext(ShaderDebugState *state,
                           const rdcarray<rdcarray<ShaderVariable>> &prevWorkgroup)
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

  switch(opdata.op)
  {
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

      RDCASSERT(ids[store.pointer].isPointer);

      SetDst(state, store.pointer, GetSrc(store.object));

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
        if(opdata.op == Op::ReturnValue)
        {
          OpReturnValue ret(it);

          returnValue = GetSrc(ret.value);
          returnValue.name = "<return value>";
        }

        nextInstruction = exitingFrame->funcCallInstruction;
      }

      delete exitingFrame;

      break;
    }

    default: RDCWARN("Unhandled SPIR-V operation %s", ToStr(opdata.op).c_str()); break;
  }

  // set the state's next instruction (if we have one) to ours, bounded by how many
  // instructions there are
  if(state)
    state->nextInstruction = RDCMIN(nextInstruction, debugger.GetNumInstructions() - 1);
}
};    // namespace rdcspv
