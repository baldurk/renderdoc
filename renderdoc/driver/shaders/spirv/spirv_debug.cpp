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
#include <time.h>
#include "common/formatting.h"
#include "maths/half_convert.h"
#include "os/os_specific.h"
#include "spirv_op_helpers.h"

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
      ret |= RDCISINF(val.value.fv[i]);
      ret |= RDCISNAN(val.value.fv[i]) != 0;
    }
  }
  else if(val.type == VarType::Double)
  {
    for(int i = 0; i < count; i++)
    {
      ret |= RDCISINF(val.value.dv[i]);
      ret |= RDCISNAN(val.value.dv[i]) != 0;
    }
  }

  return ret;
}

namespace rdcspv
{
const BindpointIndex DebugAPIWrapper::invalidBind = BindpointIndex(-12345, -12345, ~0U);

ThreadState::ThreadState(uint32_t workgroupIdx, Debugger &debug, const GlobalState &globalState)
    : debugger(debug), global(globalState)
{
  workgroupIndex = workgroupIdx;
  nextInstruction = 0;
  helperInvocation = false;
  killed = false;
}

ThreadState::~ThreadState()
{
  for(StackFrame *stack : callstack)
    delete stack;
  callstack.clear();
}

bool ThreadState::Finished() const
{
  return killed || callstack.empty();
}

void ThreadState::FillCallstack(ShaderDebugState &state)
{
  for(const StackFrame *frame : callstack)
    state.callstack.push_back(debugger.GetHumanName(frame->function));
}

void ThreadState::EnterFunction(const rdcarray<Id> &arguments)
{
  Iter it = debugger.GetIterForInstruction(nextInstruction);

  RDCASSERT(OpDecoder(it).op == Op::Function);

  OpFunction func(it);
  StackFrame *frame = new StackFrame();
  frame->function = func.result;

  // if there's a previous stack frame, save its live list
  if(!callstack.empty())
  {
    // process the outgoing scope
    ProcessScopeChange(live, {});
    callstack.back()->live = live;
    callstack.back()->sourceVars = sourceVars;
  }

  // start with just globals
  live = debugger.GetLiveGlobals();
  sourceVars = debugger.GetGlobalSourceVars();

  callstack.push_back(frame);

  it++;

  size_t arg = 0;
  while(OpDecoder(it).op == Op::FunctionParameter || OpDecoder(it).op == Op::Line ||
        OpDecoder(it).op == Op::NoLine)
  {
    if(OpDecoder(it).op == Op::Line || OpDecoder(it).op == Op::NoLine)
    {
      it++;
      continue;
    }

    OpFunctionParameter param(it);

    if(arg <= arguments.size())
    {
      // function parameters are copied into function calls. Thus a function parameter that is a
      // pointer does not have allocated storage for itself, it gets the pointer from the call site
      // copied in and points to whatever storage that is.
      // That means we don't have to allocate anything here, we just set up the ID and copy the
      // value from the argument
      SetDst(param.result, ids[arguments[arg]]);
    }
    else
    {
      RDCERR("Not enough function parameters!");
    }

    arg++;
    it++;
  }

  while(OpDecoder(it).op == Op::Line || OpDecoder(it).op == Op::NoLine)
    it++;

  // next should be the start of the first function block
  RDCASSERT(OpDecoder(it).op == Op::Label);
  frame->lastBlock = frame->curBlock = OpLabel(it).result;
  it++;

  size_t numVars = 0;
  Iter varCounter = it;
  while(OpDecoder(varCounter).op == Op::Variable || OpDecoder(varCounter).op == Op::Line ||
        OpDecoder(varCounter).op == Op::NoLine)
  {
    varCounter++;
    if(OpDecoder(varCounter).op == Op::Line || OpDecoder(varCounter).op == Op::NoLine)
      continue;

    numVars++;
  }

  frame->locals.resize(numVars);

  // don't add source vars for variables, we'll add it on the first store
  ShaderDebugState *state = m_State;
  m_State = NULL;

  size_t i = 0;
  // handle any variable declarations
  while(OpDecoder(it).op == Op::Variable || OpDecoder(it).op == Op::Line ||
        OpDecoder(it).op == Op::NoLine)
  {
    if(OpDecoder(it).op == Op::Line || OpDecoder(it).op == Op::NoLine)
    {
      it++;
      continue;
    }

    OpVariable decl(it);

    ShaderVariable &stackvar = frame->locals[i];
    stackvar.name = debugger.GetRawName(decl.result);

    rdcstr sourceName = debugger.GetHumanName(decl.result);

    debugger.AllocateVariable(decl.result, decl.resultType, stackvar);

    if(decl.HasInitializer())
      AssignValue(stackvar, ids[decl.initializer]);

    SetDst(decl.result, debugger.MakePointerVariable(decl.result, &stackvar));

    it++;
    i++;
  }

  m_State = state;

  // next instruction is the first actual instruction we'll execute
  nextInstruction = debugger.GetInstructionForIter(it);

  SkipIgnoredInstructions();
}

const ShaderVariable &ThreadState::GetSrc(Id id) const
{
  return ids[id];
}

void ThreadState::WritePointerValue(Id pointer, const ShaderVariable &val)
{
  RDCASSERT(ids[pointer].type == VarType::GPUPointer);

  // this is the only place we don't use SetDst because it's the only place that "violates" SSA
  // i.e. changes an existing value. That way SetDst can always unconditionally assign values,
  // and only here do we write through pointers

  if(!m_State)
  {
    debugger.WriteThroughPointer(ids[pointer], val);
  }
  else
  {
    ShaderVariable &var = ids[pointer];

    if(ContainsNaNInf(val))
      m_State->flags |= ShaderEvents::GeneratedNanOrInf;

    // if var is a pointer we update the underlying storage and generate at least one change,
    // plus any additional ones for other pointers.
    Id ptrid = debugger.GetPointerBaseId(var);

    ReferencePointer(ptrid);

    ShaderVariableChange basechange;

    if(debugger.IsOpaquePointer(ids[ptrid]))
    {
      // if this is a write to a SSBO pointer, don't record any alias changes, just record a no-op
      // change to this pointer
      basechange.after = basechange.before = debugger.GetPointerValue(ids[pointer]);
      m_State->changes.push_back(basechange);
      debugger.WriteThroughPointer(var, val);
      return;
    }

    rdcarray<ShaderVariableChange> changes;
    basechange.before = debugger.GetPointerValue(ids[ptrid]);

    rdcarray<Id> &pointers = pointersForId[ptrid];

    changes.resize(pointers.size());

    // for every other pointer, evaluate its value now before
    for(size_t i = 0; i < pointers.size(); i++)
      changes[i].before = debugger.GetPointerValue(ids[pointers[i]]);

    debugger.WriteThroughPointer(var, val);

    // now evaluate the value after
    for(size_t i = 0; i < pointers.size(); i++)
      changes[i].after = debugger.GetPointerValue(ids[pointers[i]]);

    // if the pointer we're writing is one of the aliased pointers, be sure we add it even if
    // it's a no-op change
    int ptrIdx = pointers.indexOf(pointer);

    if(ptrIdx >= 0)
    {
      m_State->changes.push_back(changes[ptrIdx]);
      changes.erase(ptrIdx);
    }

    // remove any no-op changes. Some pointers might point to the same ID but a child that
    // wasn't written to. Note that this might not actually mean nothing was changed (if e.g.
    // we're assigning the same value) but that false negative is not a concern.
    changes.removeIf([](const ShaderVariableChange &c) { return c.before == c.after; });

    m_State->changes.append(changes);

    // always add a change for the base storage variable written itself, even if that's a no-op.
    // This one is not included in any of the pointers lists above
    basechange.after = debugger.GetPointerValue(ids[ptrid]);
    m_State->changes.push_back(basechange);
  }
}

ShaderVariable ThreadState::ReadPointerValue(Id pointer)
{
  return debugger.ReadFromPointer(GetSrc(pointer));
}

void ThreadState::SetDst(Id id, const ShaderVariable &val)
{
  if(m_State && ContainsNaNInf(val))
    m_State->flags |= ShaderEvents::GeneratedNanOrInf;

  ids[id] = val;
  ids[id].name = debugger.GetRawName(id);

  auto it = std::lower_bound(live.begin(), live.end(), id);
  live.insert(it - live.begin(), id);

  if(m_State)
  {
    ShaderVariableChange change;
    change.after = debugger.GetPointerValue(ids[id]);
    m_State->changes.push_back(change);

    debugger.AddSourceVars(sourceVars, change.after, id);
  }
}

void ThreadState::ProcessScopeChange(const rdcarray<Id> &oldLive, const rdcarray<Id> &newLive)
{
  // nothing to do if we aren't tracking into a state
  if(!m_State)
    return;

  // all oldLive (except globals) are going out of scope. all newLive (except globals) are coming
  // into scope

  const rdcarray<Id> &liveGlobals = debugger.GetLiveGlobals();

  for(const Id id : oldLive)
  {
    if(liveGlobals.contains(id))
      continue;

    m_State->changes.push_back({debugger.GetPointerValue(ids[id])});
  }

  for(const Id id : newLive)
  {
    if(liveGlobals.contains(id))
      continue;

    m_State->changes.push_back({ShaderVariable(), debugger.GetPointerValue(ids[id])});
  }
}

ShaderVariable ThreadState::CalcDeriv(ThreadState::DerivDir dir, ThreadState::DerivType type,
                                      const rdcarray<ThreadState> &workgroup, Id val)
{
  const ThreadState *a = NULL, *b = NULL;

  const bool xdirection = (dir == DDX);
  if(type == Coarse)
  {
    // coarse derivatives are identical across the quad, based on the top-left.
    a = &workgroup[0];
    b = &workgroup[xdirection ? 1 : 2];
  }
  else
  {
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
          a = &workgroup[0];
          b = &workgroup[1];
        }
        else
        {
          a = &workgroup[0];
          b = &workgroup[2];
        }
      }
      else
      {
        // bottom-left
        if(xdirection)
        {
          a = &workgroup[2];
          b = &workgroup[3];
        }
        else
        {
          a = &workgroup[0];
          b = &workgroup[2];
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
          a = &workgroup[0];
          b = &workgroup[1];
        }
        else
        {
          a = &workgroup[1];
          b = &workgroup[3];
        }
      }
      else
      {
        // bottom-right
        if(xdirection)
        {
          a = &workgroup[2];
          b = &workgroup[3];
        }
        else
        {
          a = &workgroup[1];
          b = &workgroup[3];
        }
      }
    }
  }

  if(a->Finished() || b->Finished())
  {
    debugger.GetAPIWrapper()->AddDebugMessage(
        MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
        StringFormat::Fmt("Derivative calculation within non-uniform control flow on input %s",
                          debugger.GetHumanName(val).c_str()));
    return ShaderVariable("", 0.0f, 0.0f, 0.0f, 0.0f);
  }

  ShaderVariable aval = a->GetSrc(val);
  ShaderVariable bval = b->GetSrc(val);

  for(uint8_t c = 0; c < aval.columns; c++)
    aval.value.fv[c] = bval.value.fv[c] - aval.value.fv[c];

  return aval;
}

void ThreadState::JumpToLabel(Id target)
{
  StackFrame *frame = callstack.back();

  frame->lastBlock = frame->curBlock;
  frame->curBlock = target;

  nextInstruction = debugger.GetInstructionForLabel(target) + 1;

  // if jumping to an empty unconditional loop header, continue to the loop block
  Iter it = debugger.GetIterForInstruction(nextInstruction);
  if(it.opcode() == Op::LoopMerge)
  {
    OpLoopMerge merge(it);

    mergeBlock = merge.mergeBlock;

    it++;
    if(it.opcode() == Op::Branch)
    {
      JumpToLabel(OpBranch(it).targetLabel);
    }
  }

  SkipIgnoredInstructions();
}

void ThreadState::ReferencePointer(Id id)
{
  if(m_State)
  {
    StackFrame *frame = callstack.back();

    rdcstr name = debugger.GetRawName(id);

    // see if this is a local variable which is newly referenced, if so add source vars for it
    for(size_t i = 0; i < frame->locals.size(); i++)
    {
      if(name == frame->locals[i].name)
      {
        if(!frame->localsUsed.contains(id))
        {
          debugger.AddSourceVars(sourceVars, frame->locals[i], id);
          frame->localsUsed.push_back(id);
        }

        break;
      }
    }

    // otherwise if we have sourcevars referencing this ID, shuffle them to the back as they are
    // newly touched.
    rdcarray<SourceVariableMapping> refs;
    for(size_t i = 0; i < sourceVars.size();)
    {
      if(!sourceVars[i].variables.empty() && sourceVars[i].variables[0].name == name)
      {
        refs.push_back(sourceVars[i]);
        sourceVars.erase(i);
        continue;
      }

      i++;
    }

    sourceVars.append(refs);
  }
}

void ThreadState::SkipIgnoredInstructions()
{
  // skip OpLine/OpNoLine now, so that nextInstruction points to the next real instruction
  // Also for structured control flow we just save the merge block in case we need it for converging
  // in pixel shaders, but otherwise skip them.
  while(true)
  {
    Iter it = debugger.GetIterForInstruction(nextInstruction);
    rdcspv::Op op = it.opcode();
    if(op == Op::Line || op == Op::NoLine)
    {
      nextInstruction++;
      continue;
    }

    if(op == Op::SelectionMerge)
    {
      OpSelectionMerge merge(it);

      mergeBlock = merge.mergeBlock;

      nextInstruction++;
      continue;
    }

    if(op == Op::LoopMerge)
    {
      OpLoopMerge merge(it);

      mergeBlock = merge.mergeBlock;

      nextInstruction++;
      continue;
    }

    break;
  }
}

void ThreadState::EnterEntryPoint(ShaderDebugState *state)
{
  m_State = state;

  EnterFunction({});

  m_State = NULL;
}

void ThreadState::StepNext(ShaderDebugState *state, const rdcarray<ThreadState> &workgroup)
{
  m_State = state;

  Iter it = debugger.GetIterForInstruction(nextInstruction);
  nextInstruction++;

  OpDecoder opdata(it);

  // don't skip any instructions here. These should be skipped *after* processing, so that
  // nextInstruction always points to the next real instruction.

  switch(opdata.op)
  {
    //////////////////////////////////////////////////////////////////////////////
    //
    // Pointer manipulation opcodes
    //
    //////////////////////////////////////////////////////////////////////////////
    case Op::Load:
    {
      OpLoad load(it);

      // ignore
      (void)load.memoryAccess;

      // get the pointer value, evaluate it (i.e. dereference) and store the result
      SetDst(load.result, ReadPointerValue(load.pointer));

      break;
    }
    case Op::Store:
    {
      OpStore store(it);

      // ignore
      (void)store.memoryAccess;

      WritePointerValue(store.pointer, GetSrc(store.object));

      break;
    }
    case Op::CopyMemory:
    {
      OpCopyMemory copy(it);

      // ignore
      (void)copy.memoryAccess0;
      (void)copy.memoryAccess1;

      WritePointerValue(copy.target, ReadPointerValue(copy.source));

      break;
    }
    case Op::AccessChain:
    case Op::InBoundsAccessChain:
    {
      OpAccessChain chain(it);

      rdcarray<uint32_t> indices;

      // evaluate the indices
      indices.reserve(chain.indexes.size());
      for(Id id : chain.indexes)
        indices.push_back(GetSrc(id).value.uv[0]);

      SetDst(chain.result, debugger.MakeCompositePointer(ids[chain.base], chain.base, indices));

      break;
    }
    case Op::ArrayLength:
    {
      OpArrayLength len(it);

      ShaderVariable structPointer = GetSrc(len.structure);

      // get the pointer base offset (should be zero for any binding but could be non-zero for a
      // buffer_device_address pointer)
      uint64_t offset = structPointer.value.u64v[BufferPointerByteOffsetVariableSlot];

      // add the offset of the member
      const DataType &pointerType = debugger.GetTypeForId(len.structure);
      const DataType &structType = debugger.GetType(pointerType.InnerType());

      offset += structType.children[len.arraymember].decorations.offset;

      ShaderVariable result;
      result.rows = result.columns = 1;
      result.type = VarType::UInt;

      BindpointIndex bind = debugger.GetPointerValue(structPointer).GetBinding();

      uint64_t byteLen = debugger.GetAPIWrapper()->GetBufferLength(bind) - offset;

      const Decorations &dec = debugger.GetDecorations(structType.children[len.arraymember].type);

      RDCASSERT(dec.flags & Decorations::HasArrayStride);
      byteLen /= dec.arrayStride;

      result.value.uv[0] = uint32_t(byteLen);

      SetDst(len.result, result);

      break;
    }
    case Op::PtrEqual:
    case Op::PtrNotEqual:
    {
      OpPtrEqual equal(it);

      ShaderVariable a = GetSrc(equal.operand1);
      ShaderVariable b = GetSrc(equal.operand2);

      bool isEqual = debugger.ArePointersAndEqual(a, b);

      ShaderVariable var;
      var.rows = var.columns = 1;
      var.type = VarType::Bool;

      if(opdata.op == Op::PtrEqual)
        var.value.uv[0] = isEqual ? 1 : 0;
      else
        var.value.uv[0] = isEqual ? 0 : 1;

      SetDst(equal.result, var);
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
    case Op::DPdy:
    case Op::DPdxCoarse:
    case Op::DPdyCoarse:
    case Op::DPdxFine:
    case Op::DPdyFine:
    {
      // these all share a format
      OpDPdx deriv(it);

      DerivDir dir = DDX;
      if(opdata.op == Op::DPdy || opdata.op == Op::DPdyCoarse || opdata.op == Op::DPdyFine)
        dir = DDY;

      DerivType type = Coarse;
      if(opdata.op == Op::DPdxFine || opdata.op == Op::DPdyFine)
        type = Fine;

      SetDst(deriv.result, CalcDeriv(dir, type, workgroup, deriv.p));

      break;
    }
    case Op::Fwidth:
    case Op::FwidthCoarse:
    case Op::FwidthFine:
    {
      // these all share a format
      OpFwidth deriv(it);

      DerivType type = Coarse;
      if(opdata.op == Op::FwidthFine)
        type = Fine;

      ShaderVariable var = CalcDeriv(DDX, type, workgroup, deriv.p);
      ShaderVariable ddy = CalcDeriv(DDY, type, workgroup, deriv.p);

      for(uint32_t c = 0; c < var.columns; c++)
        var.value.fv[c] = fabsf(var.value.fv[c]) + fabsf(ddy.value.fv[c]);

      SetDst(deriv.result, var);

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
      SetDst(extract.result, debugger.ReadFromPointer(ptr));

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
      SetDst(insert.result, var);

      break;
    }
    case Op::CompositeConstruct:
    {
      OpCompositeConstruct construct(it);

      ShaderVariable var;

      const DataType &type = debugger.GetType(construct.resultType);

      RDCASSERT(!construct.constituents.empty());

      if(type.type == DataType::ArrayType)
      {
        var.members.resize(construct.constituents.size());
        for(size_t i = 0; i < construct.constituents.size(); i++)
        {
          var.members[i] = GetSrc(construct.constituents[i]);
          var.members[i].name = StringFormat::Fmt("[%zu]", i);
        }
      }
      else if(type.type == DataType::StructType)
      {
        RDCASSERTEQUAL(type.children.size(), construct.constituents.size());
        var.members.resize(construct.constituents.size());
        for(size_t i = 0; i < construct.constituents.size(); i++)
        {
          ShaderVariable &mem = var.members[i];
          var.members[i] = GetSrc(construct.constituents[i]);
          if(!type.children[i].name.empty())
            var.members[i].name = type.children[i].name;
          else
            var.members[i].name = StringFormat::Fmt("_child%zu", i);
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

      SetDst(construct.result, var);

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

      uint32_t vec1Cols = src1.columns;

      for(size_t i = 0; i < shuffle.components.size(); i++)
      {
        uint32_t c = shuffle.components[i];
        if(c < vec1Cols)
          var.value.uv[i] = src1.value.uv[c];
        else
          var.value.uv[i] = src2.value.uv[c - vec1Cols];
      }

      SetDst(shuffle.result, var);

      break;
    }
    case Op::VectorExtractDynamic:
    {
      OpVectorExtractDynamic extract(it);

      ShaderVariable var = GetSrc(extract.vector);
      ShaderVariable idx = GetSrc(extract.index);

      uint32_t comp = idx.value.uv[0];

      if(VarTypeByteSize(var.type) == 8)
        var.value.u64v[0] = var.value.u64v[comp];
      else
        var.value.uv[0] = var.value.uv[comp];

      // result is now scalar
      var.columns = 1;

      SetDst(extract.result, var);
      break;
    }
    case Op::VectorInsertDynamic:
    {
      OpVectorInsertDynamic insert(it);

      ShaderVariable var = GetSrc(insert.vector);
      ShaderVariable scalar = GetSrc(insert.component);
      ShaderVariable idx = GetSrc(insert.index);

      uint32_t comp = idx.value.uv[0];

      if(VarTypeByteSize(var.type) == 8)
        var.value.u64v[comp] = scalar.value.u64v[0];
      else
        var.value.uv[comp] = scalar.value.uv[0];

      SetDst(insert.result, var);
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
        if(cond.value.uv[0] == 0)
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

      SetDst(select.result, var);

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

      SetDst(conv.result, var);
      break;
    }
    case Op::QuantizeToF16:
    {
      OpQuantizeToF16 quant(it);

      ShaderVariable var = GetSrc(quant.value);

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.fv[c] = ConvertFromHalf(ConvertToHalf(var.value.fv[c]));

      SetDst(quant.result, var);
      break;
    }
    case Op::UConvert:
    {
      OpUConvert cast(it);

      ShaderVariable var = GetSrc(cast.unsignedValue);

      // TODO - conversion between bit widths once we support it

      SetDst(cast.result, var);
      break;
    }
    case Op::SConvert:
    {
      OpSConvert cast(it);

      ShaderVariable var = GetSrc(cast.signedValue);

      // TODO - conversion between bit widths once we support it

      SetDst(cast.result, var);
      break;
    }
    case Op::FConvert:
    {
      OpFConvert cast(it);

      const DataType &type = debugger.GetType(cast.resultType);

      ShaderVariable var = GetSrc(cast.floatValue);

      ShaderVariable result = var;

      uint32_t srcWidth = VarTypeByteSize(var.type);
      uint32_t dstWidth = type.scalar().width / 8;

      for(uint8_t c = 0; c < var.columns; c++)
      {
        if(srcWidth == 8)
        {
          if(dstWidth == 8)
          {
            // nop
          }
          else
          {
            result.value.fv[c] = (float)var.value.dv[c];
          }
        }
        else if(srcWidth == 4)
        {
          if(dstWidth == 8)
          {
            result.value.dv[c] = (double)var.value.dv[c];
          }
          else
          {
            // nop
          }
        }
      }

      SetDst(cast.result, result);
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

      SetDst(cast.result, var);
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

      SetDst(result, dispatch.functions[instruction](*this, instruction, params));
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Comparison opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::LogicalEqual:
    case Op::LogicalNotEqual:
    case Op::LogicalOr:
    case Op::LogicalAnd:
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

      if(opdata.op == Op::IEqual || opdata.op == Op::LogicalEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.uv[c] == b.value.uv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::INotEqual || opdata.op == Op::LogicalNotEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = (var.value.uv[c] != b.value.uv[c]) ? 1 : 0;
      }
      else if(opdata.op == Op::LogicalAnd)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = var.value.uv[c] & b.value.uv[c];
      }
      else if(opdata.op == Op::LogicalOr)
      {
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.uv[c] = var.value.uv[c] | b.value.uv[c];
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

      var.type = VarType::Bool;

      SetDst(comp.result, var);
      break;
    }
    case Op::LogicalNot:
    {
      OpLogicalNot negate(it);

      ShaderVariable var = GetSrc(negate.operand);

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.uv[c] = 1U - var.value.uv[c];

      var.type = VarType::Bool;

      SetDst(negate.result, var);
      break;
    }
    case Op::Any:
    case Op::All:
    {
      OpAny any(it);

      ShaderVariable var = GetSrc(any.vector);

      for(uint8_t c = 1; c < var.columns; c++)
      {
        if(opdata.op == Op::Any)
          var.value.uv[0] |= var.value.uv[c];
        else
          var.value.uv[0] &= var.value.uv[c];
      }

      var.columns = 1;

      SetDst(any.result, var);
      break;
    }
    case Op::IsNan:
    {
      OpIsNan is(it);

      ShaderVariable var = GetSrc(is.x);

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.uv[c] = RDCISNAN(var.value.fv[c]) ? 1 : 0;

      var.type = VarType::Bool;

      SetDst(is.result, var);
      break;
    }
    case Op::IsInf:
    {
      OpIsNan is(it);

      ShaderVariable var = GetSrc(is.x);

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.uv[c] = RDCISINF(var.value.fv[c]) ? 1 : 0;

      var.type = VarType::Bool;

      SetDst(is.result, var);
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Bitwise/logical opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::BitCount:
    {
      OpBitCount bitwise(it);

      ShaderVariable var = GetSrc(bitwise.base);

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.uv[c] = Bits::CountOnes(var.value.uv[c]);

      SetDst(bitwise.result, var);
      break;
    }
    case Op::BitReverse:
    {
      OpBitReverse bitwise(it);

      ShaderVariable var = GetSrc(bitwise.base);

      for(uint8_t c = 0; c < var.columns; c++)
      {
        uint32_t u = var.value.uv[c];
        var.value.uv[c] = 0;
        for(uint8_t b = 0; b < 32; b++)
        {
          uint32_t bit = u & (1u << b);
          var.value.uv[c] |= bit << (31 - b);
        }
      }

      SetDst(bitwise.result, var);
      break;
    }
    case Op::BitFieldUExtract:
    case Op::BitFieldSExtract:
    {
      OpBitFieldUExtract bitwise(it);

      ShaderVariable var = GetSrc(bitwise.base);
      ShaderVariable offset = GetSrc(bitwise.offset);
      ShaderVariable count = GetSrc(bitwise.count);

      for(uint8_t c = 0; c < var.columns; c++)
      {
        const uint32_t mask = (1u << count.value.uv[c]) - 1;

        var.value.uv[c] >>= offset.value.uv[c];
        var.value.uv[c] &= (1u << count.value.uv[c]) - 1;

        if(opdata.op == Op::BitFieldSExtract)
        {
          uint32_t topbit = (mask + 1u) >> 1u;
          if(var.value.uv[c] & topbit)
            var.value.uv[c] |= (0xffffffffu ^ mask);
        }
      }

      SetDst(bitwise.result, var);
      break;
    }
    case Op::BitFieldInsert:
    {
      OpBitFieldInsert bitwise(it);

      ShaderVariable var = GetSrc(bitwise.base);
      ShaderVariable insert = GetSrc(bitwise.insert);
      ShaderVariable offset = GetSrc(bitwise.offset);
      ShaderVariable count = GetSrc(bitwise.count);

      for(uint8_t c = 0; c < var.columns; c++)
      {
        const uint32_t mask = (1u << count.value.uv[c]) - 1;

        var.value.uv[c] &= ~(mask << offset.value.uv[c]);
        var.value.uv[c] |= (insert.value.uv[c] & mask) << offset.value.uv[c];
      }

      SetDst(bitwise.result, var);
      break;
    }
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

      SetDst(bitwise.result, var);
      break;
    }
    case Op::Not:
    {
      OpNot bitwise(it);

      ShaderVariable var = GetSrc(bitwise.operand);

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.uv[c] = ~var.value.uv[c];

      SetDst(bitwise.result, var);
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
            if(m_State)
              m_State->flags |= ShaderEvents::GeneratedNanOrInf;
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
            if(m_State)
              m_State->flags |= ShaderEvents::GeneratedNanOrInf;
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
            if(m_State)
              m_State->flags |= ShaderEvents::GeneratedNanOrInf;
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
            if(m_State)
              m_State->flags |= ShaderEvents::GeneratedNanOrInf;
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

      SetDst(math.result, var);
      break;
    }
    // extended math ops
    case Op::UMulExtended:
    case Op::SMulExtended:
    case Op::IAddCarry:
    case Op::ISubBorrow:
    {
      OpUMulExtended math(it);

      ShaderVariable a = GetSrc(math.operand1);
      ShaderVariable b = GetSrc(math.operand2);

      ShaderVariable lsb = a;
      ShaderVariable msb = a;

      if(opdata.op == Op::UMulExtended)
      {
        // if this is less than 64-bit precision inputs, we can just upcast, do the mul, and then
        // mask off the bits we care about
        if(VarTypeByteSize(a.type) < 8)
        {
          for(uint8_t c = 0; c < a.columns; c++)
          {
            const uint64_t x = a.value.uv[c];
            const uint64_t y = b.value.uv[c];
            const uint64_t res = x * y;

            lsb.value.uv[c] = uint32_t(res & 0xFFFFFFFFu);
            msb.value.uv[c] = uint32_t(res >> 32);
          }
        }
      }
      else if(opdata.op == Op::SMulExtended)
      {
        if(VarTypeByteSize(a.type) < 8)
        {
          for(uint8_t c = 0; c < a.columns; c++)
          {
            const int64_t x = a.value.iv[c];
            const int64_t y = b.value.iv[c];
            const int64_t res = x * y;

            lsb.value.iv[c] = int32_t(res & 0xFFFFFFFFu);
            msb.value.iv[c] = int32_t(res >> 32);
          }
        }
      }
      else if(opdata.op == Op::IAddCarry)
      {
        for(uint8_t c = 0; c < a.columns; c++)
        {
          // unsigned overflow is well-defined to wrap around, giving us the lsb we want.
          lsb.value.uv[c] = a.value.uv[c] + b.value.uv[c];
          // if the result is less than one of the operands, we overflowed so set msb
          msb.value.uv[c] = (lsb.value.uv[c] < b.value.uv[c]) ? 1 : 0;
        }
      }
      else if(opdata.op == Op::ISubBorrow)
      {
        for(uint8_t c = 0; c < a.columns; c++)
        {
          // if b <= a we don't need to borrow
          if(b.value.uv[c] <= a.value.uv[c])
          {
            msb.value.uv[c] = 0;
            lsb.value.uv[c] = a.value.uv[c] - b.value.uv[c];
          }
          else
          {
            // otherwise set borrow bit
            msb.value.uv[c] = 1;
            lsb.value.uv[c] = 0xFFFFFFFFu - (b.value.uv[c] - a.value.uv[c] - 1);
          }
        }
      }

      ShaderVariable result;
      result.rows = 1;
      result.columns = 1;
      result.isStruct = true;
      result.members = {lsb, msb};
      result.members[0].name = "lsb";
      result.members[1].name = "msb";

      SetDst(math.result, result);
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

      SetDst(math.result, var);
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
      var.value.fv[0] = ret;

      SetDst(dot.result, var);
      break;
    }
    case Op::VectorTimesScalar:
    {
      OpVectorTimesScalar mul(it);

      ShaderVariable var = GetSrc(mul.vector);
      ShaderVariable scalar = GetSrc(mul.scalar);

      for(uint8_t c = 0; c < var.columns; c++)
        var.value.fv[c] *= scalar.value.fv[0];

      SetDst(mul.result, var);
      break;
    }
    case Op::MatrixTimesScalar:
    {
      OpMatrixTimesScalar mul(it);

      ShaderVariable var = GetSrc(mul.matrix);
      ShaderVariable scalar = GetSrc(mul.scalar);

      for(uint8_t c = 0; c < var.rows * var.columns; c++)
        var.value.fv[c] *= scalar.value.fv[0];

      SetDst(mul.result, var);
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

      SetDst(mul.result, var);
      break;
    }
    case Op::Transpose:
    {
      OpTranspose transpose(it);

      ShaderVariable matrix = GetSrc(transpose.matrix);
      ShaderVariable var = matrix;
      std::swap(var.rows, var.columns);

      for(uint8_t r = 0; r < var.rows; r++)
        for(uint8_t c = 0; c < var.columns; c++)
          var.value.fv[r * var.columns + c] = matrix.value.fv[c * matrix.columns + r];

      SetDst(transpose.result, var);
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

      SetDst(mul.result, var);
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

      SetDst(mul.result, var);
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

      SetDst(mul.result, var);
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
      result.members[0].name = "image";
      result.members[1].name = "sampler";

      SetDst(opdata.result, result);
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

      SetDst(image.result, var);
      break;
    }
    case Op::ImageQueryLevels:
    case Op::ImageQuerySamples:
    case Op::ImageQuerySize:
    case Op::ImageQuerySizeLod:
    case Op::ImageFetch:
    case Op::ImageGather:
    case Op::ImageDrefGather:
    case Op::ImageQueryLod:
    case Op::ImageSampleExplicitLod:
    case Op::ImageSampleImplicitLod:
    case Op::ImageSampleDrefExplicitLod:
    case Op::ImageSampleDrefImplicitLod:
    case Op::ImageSampleProjExplicitLod:
    case Op::ImageSampleProjImplicitLod:
    case Op::ImageSampleProjDrefExplicitLod:
    case Op::ImageSampleProjDrefImplicitLod:
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
      else if(opdata.op == Op::ImageGather)
      {
        OpImageGather image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        gather = GatherChannel(GetSrc(image.component).value.uv[0]);
        operands = image.imageOperands;
      }
      else if(opdata.op == Op::ImageDrefGather)
      {
        OpImageDrefGather image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;
        gather = GatherChannel::Red;
        compare = GetSrc(image.dref);
      }
      else if(opdata.op == Op::ImageQueryLod)
      {
        OpImageQueryLod image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);

        derivId = image.coordinate;
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
      else if(opdata.op == Op::ImageSampleDrefExplicitLod)
      {
        OpImageSampleDrefExplicitLod image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;
        compare = GetSrc(image.dref);
      }
      else if(opdata.op == Op::ImageSampleDrefImplicitLod)
      {
        OpImageSampleDrefImplicitLod image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;
        compare = GetSrc(image.dref);

        derivId = image.coordinate;
      }
      else if(opdata.op == Op::ImageSampleProjExplicitLod)
      {
        OpImageSampleProjExplicitLod image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;
      }
      else if(opdata.op == Op::ImageSampleProjImplicitLod)
      {
        OpImageSampleProjImplicitLod image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;

        derivId = image.coordinate;
      }
      else if(opdata.op == Op::ImageSampleProjDrefExplicitLod)
      {
        OpImageSampleProjDrefExplicitLod image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;
        compare = GetSrc(image.dref);
      }
      else if(opdata.op == Op::ImageSampleProjDrefImplicitLod)
      {
        OpImageSampleProjDrefImplicitLod image(it);

        sampler = img = GetSrc(image.sampledImage);
        uv = GetSrc(image.coordinate);
        operands = image.imageOperands;
        compare = GetSrc(image.dref);

        derivId = image.coordinate;
      }
      else if(opdata.op == Op::ImageQueryLevels || opdata.op == Op::ImageQuerySamples ||
              opdata.op == Op::ImageQuerySize)
      {
        // these opcodes are all identical, they just query a property of the image
        OpImageQueryLevels query(it);

        img = GetSrc(query.image);
      }
      else if(opdata.op == Op::ImageQuerySizeLod)
      {
        OpImageQuerySizeLod query(it);

        img = GetSrc(query.image);
        operands.setLod(query.levelofDetail);
      }

      if(derivId != Id())
      {
        // calculate DDX/DDY in coarse fashion
        ddxCalc = CalcDeriv(DDX, Coarse, workgroup, derivId);
        ddyCalc = CalcDeriv(DDY, Coarse, workgroup, derivId);
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
          (DebugAPIWrapper::TextureType)img.value.u64v[TextureTypeVariableSlot];

      // should not be sampling or fetching from subpass textures
      RDCASSERT((texType & DebugAPIWrapper::Subpass_Texture) == 0);

      ShaderVariable result;

      result.type = resultType.scalar().Type();

      BindpointIndex samplerIndex = DebugAPIWrapper::invalidBind;
      if(sampler.type == VarType::Sampler || sampler.type == VarType::ReadOnlyResource)
        samplerIndex = sampler.GetBinding();

      if(!debugger.GetAPIWrapper()->CalculateSampleGather(
             *this, opdata.op, texType, img.GetBinding(), samplerIndex, uv, ddxCalc, ddyCalc,
             compare, gather, operands, result))
      {
        // sample failed. Pretend we got 0 columns back

        result.value.uv[0] = 0;
        result.value.uv[1] = 0;
        result.value.uv[2] = 0;

        if(result.type == VarType::Float || result.type == VarType::Half)
          result.value.fv[3] = 1.0f;
        else if(result.type == VarType::Double)
          result.value.dv[3] = 1.0;
        else
          result.value.uv[3] = 1;
      }

      result.rows = 1;
      result.columns = RDCMAX(1U, resultType.vector().count);

      SetDst(opdata.result, result);
      break;
    }
    case Op::ImageRead:
    {
      OpImageRead read(it);

      ShaderVariable img = GetSrc(read.image);
      ShaderVariable coord = GetSrc(read.coordinate);

      const DataType &resultType = debugger.GetType(opdata.resultType);

      // only the sample operand should be here
      RDCASSERT((read.imageOperands.flags & ImageOperands::Sample) == read.imageOperands.flags);

      ShaderVariable result;
      result.type = resultType.scalar().Type();

      DebugAPIWrapper::TextureType texType =
          (DebugAPIWrapper::TextureType)img.value.u64v[TextureTypeVariableSlot];

      if(texType & DebugAPIWrapper::Subpass_Texture)
      {
        // get current position
        ShaderVariable curCoord;
        debugger.GetAPIWrapper()->FillInputValue(curCoord, ShaderBuiltin::Position, 0, 0);

        // co-ords are relative to the current position
        coord.value.uv[0] += curCoord.value.uv[0];
        coord.value.uv[1] += curCoord.value.uv[1];

        // do it with samplegather as ImageFetch rather than a Read which caches the whole texture
        // on the CPU for no reason (since we can't write to it)

        if(!debugger.GetAPIWrapper()->CalculateSampleGather(
               *this, Op::ImageFetch, texType, img.GetBinding(), DebugAPIWrapper::invalidBind,
               coord, ShaderVariable(), ShaderVariable(), ShaderVariable(), GatherChannel::Red,
               ImageOperandsAndParamDatas(), result))
        {
          // sample failed. Pretend we got 0 columns back
          result.value.uv[0] = 0;
          result.value.uv[1] = 0;
          result.value.uv[2] = 0;

          if(result.type == VarType::Float || result.type == VarType::Half)
            result.value.fv[3] = 1.0f;
          else if(result.type == VarType::Double)
            result.value.dv[3] = 1.0;
          else
            result.value.uv[3] = 1;
        }
      }
      else
      {
        if(!debugger.GetAPIWrapper()->ReadTexel(img.GetBinding(), coord,
                                                read.imageOperands.flags & ImageOperands::Sample
                                                    ? GetSrc(read.imageOperands.sample).value.uv[0]
                                                    : 0,
                                                result))
        {
          // sample failed. Pretend we got 0 columns back
          result.value.uv[0] = 0;
          result.value.uv[1] = 0;
          result.value.uv[2] = 0;

          if(result.type == VarType::Float || result.type == VarType::Half)
            result.value.fv[3] = 1.0f;
          else if(result.type == VarType::Double)
            result.value.dv[3] = 1.0;
          else
            result.value.uv[3] = 1;
        }
      }

      result.rows = 1;
      result.columns = RDCMAX(1U, resultType.vector().count);

      SetDst(read.result, result);
      break;
    }
    case Op::ImageWrite:
    {
      OpImageWrite write(it);

      ShaderVariable img = GetSrc(write.image);
      ShaderVariable coord = GetSrc(write.coordinate);
      ShaderVariable texel = GetSrc(write.texel);

      // only the sample operand should be here
      RDCASSERT((write.imageOperands.flags & ImageOperands::Sample) == write.imageOperands.flags);

      debugger.GetAPIWrapper()->WriteTexel(img.GetBinding(), coord,
                                           write.imageOperands.flags & ImageOperands::Sample
                                               ? GetSrc(write.imageOperands.sample).value.uv[0]
                                               : 0,
                                           texel);

      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Block flow control opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::MemoryBarrier:
    case Op::ControlBarrier:
    {
      // do nothing for now
      break;
    }
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
        if(selector.value.uv[0] == case_.first)
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
      if(GetSrc(branch.condition).value.uv[0])
        target = branch.trueLabel;

      JumpToLabel(target);

      break;
    }
    case Op::Phi:
    {
      OpPhi phi(it);

      ShaderVariable var;

      StackFrame *frame = callstack.back();

      for(const PairIdRefIdRef &parent : phi.parents)
      {
        if(parent.second == frame->lastBlock)
        {
          var = GetSrc(parent.first);
          break;
        }
      }

      // we should have had a matching for the OpPhi of the block we came from
      RDCASSERT(!var.name.empty());

      SetDst(phi.result, var);
      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Misc opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::CopyObject:
    case Op::CopyLogical:
    {
      // for our purposes differences in offset/decoration between types doesn't matter, so we can
      // implement these two the same.
      OpCopyObject copy(it);

      SetDst(copy.result, GetSrc(copy.operand));
      break;
    }
    case Op::ReadClockKHR:
    {
      const DataType &resultType = debugger.GetType(opdata.resultType);

      ShaderVariable result;

      result.type = resultType.scalar().Type();
      result.rows = 1;
      result.columns = RDCMAX(1U, resultType.vector().count);

      result.value.u64v[0] = global.clock;

      SetDst(opdata.result, result);
      break;
    }
    case Op::IsHelperInvocationEXT:
    {
      ShaderVariable result;

      result.type = VarType::UInt;
      result.rows = 1;
      result.columns = 1;

      result.value.uv[0] = helperInvocation;

      SetDst(opdata.result, result);
      break;
    }
    case Op::DemoteToHelperInvocationEXT:
    {
      helperInvocation = true;
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

        EnterFunction(call.arguments);

        RDCASSERT(callstack.back()->function == call.function);
        callstack.back()->funcCallInstruction = returnInstruction;
      }
      else
      {
        SetDst(call.result, returnValue);
        returnValue.name.clear();
      }
      break;
    }

    case Op::Kill:
    {
      killed = true;

      // destroy all stack frames
      for(StackFrame *exitingFrame : callstack)
        delete exitingFrame;

      callstack.clear();

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
        ProcessScopeChange(live, callstack.back()->live);

        // restore the live list from the calling frame
        live = callstack.back()->live;
        sourceVars = callstack.back()->sourceVars;
      }

      delete exitingFrame;

      break;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Atomic opcodes
    //
    //////////////////////////////////////////////////////////////////////////////

    case Op::ImageTexelPointer:
    {
      // we don't actually process this right now, we just store the parameters for future
      // read/write texel use.
      OpImageTexelPointer ptr(it);

      ShaderVariable result;
      result.rows = 1;
      result.columns = 1;
      result.isStruct = true;
      result.members = {ReadPointerValue(ptr.image), GetSrc(ptr.coordinate), GetSrc(ptr.sample)};
      result.members[0].name = "image";
      result.members[1].name = "coord";
      result.members[2].name = "sample";

      SetDst(opdata.result, result);
      break;
    }
    case Op::AtomicLoad:
    {
      OpAtomicLoad load(it);

      // ignore for now
      (void)load.memory;
      (void)load.semantics;

      const ShaderVariable &ptr = GetSrc(load.pointer);
      ShaderVariable result;

      if(ptr.members.empty())
      {
        result = ReadPointerValue(load.pointer);
      }
      else
      {
        const DataType &resultType = debugger.GetType(opdata.resultType);

        result.rows = result.columns = 1;
        result.type = resultType.scalar().Type();

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                                ptr.members[2].value.uv[0], result))
        {
          // sample failed. Pretend we got 0 columns back
          result.value.uv[0] = 0;
        }
      }

      SetDst(load.result, result);
      break;
    }
    case Op::AtomicStore:
    {
      OpAtomicStore store(it);

      // ignore for now
      (void)store.memory;
      (void)store.semantics;

      const ShaderVariable &ptr = GetSrc(store.pointer);
      const ShaderVariable &value = GetSrc(store.value);

      if(ptr.members.empty())
      {
        WritePointerValue(store.pointer, value);
      }
      else
      {
        debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                             ptr.members[2].value.uv[0], value);
      }

      break;
    }
    case Op::AtomicExchange:
    {
      OpAtomicExchange excg(it);

      // ignore for now
      (void)excg.memory;
      (void)excg.semantics;

      ShaderVariable result;
      const ShaderVariable &ptr = GetSrc(excg.pointer);
      const ShaderVariable &value = GetSrc(excg.value);

      if(ptr.members.empty())
      {
        result = ReadPointerValue(excg.pointer);
        WritePointerValue(excg.pointer, value);
      }
      else
      {
        const DataType &resultType = debugger.GetType(opdata.resultType);

        result.rows = result.columns = 1;
        result.type = resultType.scalar().Type();

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                                ptr.members[2].value.uv[0], result))
        {
          // sample failed. Pretend we got 0 columns back
          result.value.uv[0] = 0;
        }

        debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                             ptr.members[2].value.uv[0], value);
      }

      SetDst(excg.result, result);

      break;
    }
    case Op::AtomicCompareExchange:
    {
      OpAtomicCompareExchange cmpexcg(it);

      // ignore for now
      (void)cmpexcg.memory;
      (void)cmpexcg.equal;
      (void)cmpexcg.unequal;

      ShaderVariable result;
      const ShaderVariable &ptr = GetSrc(cmpexcg.pointer);
      const ShaderVariable &value = GetSrc(cmpexcg.value);

      if(ptr.members.empty())
      {
        result = ReadPointerValue(cmpexcg.pointer);
      }
      else
      {
        const DataType &resultType = debugger.GetType(opdata.resultType);

        result.rows = result.columns = 1;
        result.type = resultType.scalar().Type();

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                                ptr.members[2].value.uv[0], result))
        {
          // sample failed. Pretend we got 0 columns back
          result.value.uv[0] = 0;
        }
      }

      SetDst(cmpexcg.result, result);

      // write the new value, only if the value is the same as expected
      if(result.value.u64v[0] == GetSrc(cmpexcg.comparator).value.u64v[0])
      {
        if(ptr.members.empty())
        {
          WritePointerValue(cmpexcg.pointer, value);
        }
        else
        {
          debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                               ptr.members[2].value.uv[0], value);
        }
      }
      break;
    }
    case Op::AtomicIIncrement:
    case Op::AtomicIDecrement:
    {
      OpAtomicIIncrement atomic(it);

      // ignore for now
      (void)atomic.memory;
      (void)atomic.semantics;

      ShaderVariable result;
      const ShaderVariable &ptr = GetSrc(atomic.pointer);

      if(ptr.members.empty())
      {
        result = ReadPointerValue(atomic.pointer);
      }
      else
      {
        const DataType &resultType = debugger.GetType(opdata.resultType);

        result.rows = result.columns = 1;
        result.type = resultType.scalar().Type();

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                                ptr.members[2].value.uv[0], result))
        {
          // sample failed. Pretend we got 0 columns back
          result.value.uv[0] = 0;
        }
      }

      SetDst(atomic.result, result);

      if(opdata.op == Op::AtomicIIncrement)
        result.value.uv[0]++;
      else
        result.value.uv[0]--;

      // write the new value
      if(ptr.members.empty())
      {
        WritePointerValue(atomic.pointer, result);
      }
      else
      {
        debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                             ptr.members[2].value.uv[0], result);
      }
      break;
    }
    case Op::AtomicIAdd:
    case Op::AtomicISub:
    case Op::AtomicSMin:
    case Op::AtomicUMin:
    case Op::AtomicSMax:
    case Op::AtomicUMax:
    case Op::AtomicAnd:
    case Op::AtomicOr:
    case Op::AtomicXor:
    {
      OpAtomicIAdd atomic(it);

      // ignore for now
      (void)atomic.memory;
      (void)atomic.semantics;

      ShaderVariable result;
      const ShaderVariable &ptr = GetSrc(atomic.pointer);
      const ShaderVariable &value = GetSrc(atomic.value);

      if(ptr.members.empty())
      {
        result = ReadPointerValue(atomic.pointer);
      }
      else
      {
        const DataType &resultType = debugger.GetType(opdata.resultType);

        result.rows = result.columns = 1;
        result.type = resultType.scalar().Type();

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                                ptr.members[2].value.uv[0], result))
        {
          // sample failed. Pretend we got 0 columns back
          result.value.uv[0] = 0;
        }
      }

      SetDst(atomic.result, result);

      if(opdata.op == Op::AtomicIAdd)
        result.value.uv[0] += value.value.uv[0];
      else if(opdata.op == Op::AtomicISub)
        result.value.uv[0] -= value.value.uv[0];
      else if(opdata.op == Op::AtomicSMin)
        result.value.iv[0] = RDCMIN(result.value.iv[0], value.value.iv[0]);
      else if(opdata.op == Op::AtomicUMin)
        result.value.uv[0] = RDCMIN(result.value.uv[0], value.value.uv[0]);
      else if(opdata.op == Op::AtomicSMax)
        result.value.iv[0] = RDCMAX(result.value.iv[0], value.value.iv[0]);
      else if(opdata.op == Op::AtomicUMax)
        result.value.uv[0] = RDCMAX(result.value.uv[0], value.value.uv[0]);
      else if(opdata.op == Op::AtomicAnd)
        result.value.uv[0] &= value.value.uv[0];
      else if(opdata.op == Op::AtomicOr)
        result.value.uv[0] |= value.value.uv[0];
      else if(opdata.op == Op::AtomicXor)
        result.value.uv[0] ^= value.value.uv[0];

      // write the new value
      if(ptr.members.empty())
      {
        WritePointerValue(atomic.pointer, result);
      }
      else
      {
        debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBinding(), ptr.members[1],
                                             ptr.members[2].value.uv[0], result);
      }
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

      SetDst(undef.result, GetSrc(undef.result));

      break;
    }
    case Op::Nop:
    {
      // nothing to do
      break;
    }

    // TODO sparse sampling
    case Op::ImageSparseSampleImplicitLod:
    case Op::ImageSparseSampleExplicitLod:
    case Op::ImageSparseSampleDrefImplicitLod:
    case Op::ImageSparseSampleDrefExplicitLod:
    case Op::ImageSparseSampleProjImplicitLod:
    case Op::ImageSparseSampleProjExplicitLod:
    case Op::ImageSparseSampleProjDrefImplicitLod:
    case Op::ImageSparseSampleProjDrefExplicitLod:
    case Op::ImageSparseFetch:
    case Op::ImageSparseGather:
    case Op::ImageSparseDrefGather:
    case Op::ImageSparseTexelsResident:
    case Op::ImageSparseRead:
    {
      RDCERR("Sparse opcodes not supported. SPIR-V should have been rejected by capability!");

      ShaderVariable var("", 0U, 0U, 0U, 0U);
      var.columns = 1;

      SetDst(opdata.result, var);

      break;
    }

    // TODO group ops
    case Op::GroupAll:
    case Op::GroupAny:
    case Op::GroupBroadcast:
    case Op::GroupIAdd:
    case Op::GroupFAdd:
    case Op::GroupFMin:
    case Op::GroupUMin:
    case Op::GroupSMin:
    case Op::GroupFMax:
    case Op::GroupUMax:
    case Op::GroupSMax:
    case Op::GroupNonUniformElect:
    case Op::GroupNonUniformAll:
    case Op::GroupNonUniformAny:
    case Op::GroupNonUniformAllEqual:
    case Op::GroupNonUniformBroadcast:
    case Op::GroupNonUniformBroadcastFirst:
    case Op::GroupNonUniformBallot:
    case Op::GroupNonUniformInverseBallot:
    case Op::GroupNonUniformBallotBitExtract:
    case Op::GroupNonUniformBallotBitCount:
    case Op::GroupNonUniformBallotFindLSB:
    case Op::GroupNonUniformBallotFindMSB:
    case Op::GroupNonUniformShuffle:
    case Op::GroupNonUniformShuffleXor:
    case Op::GroupNonUniformShuffleUp:
    case Op::GroupNonUniformShuffleDown:
    case Op::GroupNonUniformIAdd:
    case Op::GroupNonUniformFAdd:
    case Op::GroupNonUniformIMul:
    case Op::GroupNonUniformFMul:
    case Op::GroupNonUniformSMin:
    case Op::GroupNonUniformUMin:
    case Op::GroupNonUniformFMin:
    case Op::GroupNonUniformSMax:
    case Op::GroupNonUniformUMax:
    case Op::GroupNonUniformFMax:
    case Op::GroupNonUniformBitwiseAnd:
    case Op::GroupNonUniformBitwiseOr:
    case Op::GroupNonUniformBitwiseXor:
    case Op::GroupNonUniformLogicalAnd:
    case Op::GroupNonUniformLogicalOr:
    case Op::GroupNonUniformLogicalXor:
    case Op::GroupNonUniformQuadBroadcast:
    case Op::GroupNonUniformQuadSwap:

    case Op::SubgroupBallotKHR:
    case Op::SubgroupFirstInvocationKHR:
    case Op::SubgroupAllKHR:
    case Op::SubgroupAnyKHR:
    case Op::SubgroupAllEqualKHR:
    case Op::SubgroupReadInvocationKHR:
    {
      RDCERR("Group opcodes not supported. SPIR-V should have been rejected by capability!");

      ShaderVariable var("", 0U, 0U, 0U, 0U);
      var.columns = 1;

      SetDst(opdata.result, var);

      break;
    }

    // TODO physical storage pointers
    case Op::ConvertPtrToU:
    case Op::ConvertUToPtr:
    case Op::PtrAccessChain:
    case Op::InBoundsPtrAccessChain:
    case Op::PtrDiff:
    {
      RDCERR(
          "Physical storage pointers not supported. SPIR-V should have been rejected by "
          "capability!");

      ShaderVariable var("", 0U, 0U, 0U, 0U);
      var.columns = 1;

      SetDst(opdata.result, var);

      break;
    }

    case Op::EmitVertex:
    case Op::EndPrimitive:
    case Op::EmitStreamVertex:
    case Op::EndStreamPrimitive:
    {
      // nothing to do for these, even if debugging geometry shaders?
      break;
    }

    case Op::GroupIAddNonUniformAMD:
    case Op::GroupFAddNonUniformAMD:
    case Op::GroupFMinNonUniformAMD:
    case Op::GroupUMinNonUniformAMD:
    case Op::GroupSMinNonUniformAMD:
    case Op::GroupFMaxNonUniformAMD:
    case Op::GroupUMaxNonUniformAMD:
    case Op::GroupSMaxNonUniformAMD:
    case Op::FragmentMaskFetchAMD:
    case Op::FragmentFetchAMD:
    case Op::ImageSampleFootprintNV:
    case Op::GroupNonUniformPartitionNV:
    case Op::WritePackedPrimitiveIndices4x8NV:
    case Op::ReportIntersectionNV:
    case Op::IgnoreIntersectionNV:
    case Op::TerminateRayNV:
    case Op::TraceNV:
    case Op::TypeAccelerationStructureNV:
    case Op::ExecuteCallableNV:
    case Op::TypeCooperativeMatrixNV:
    case Op::CooperativeMatrixLoadNV:
    case Op::CooperativeMatrixStoreNV:
    case Op::CooperativeMatrixMulAddNV:
    case Op::CooperativeMatrixLengthNV:
    case Op::BeginInvocationInterlockEXT:
    case Op::EndInvocationInterlockEXT:
    case Op::SubgroupShuffleINTEL:
    case Op::SubgroupShuffleDownINTEL:
    case Op::SubgroupShuffleUpINTEL:
    case Op::SubgroupShuffleXorINTEL:
    case Op::SubgroupBlockReadINTEL:
    case Op::SubgroupBlockWriteINTEL:
    case Op::SubgroupImageBlockReadINTEL:
    case Op::SubgroupImageBlockWriteINTEL:
    case Op::SubgroupImageMediaBlockReadINTEL:
    case Op::SubgroupImageMediaBlockWriteINTEL:
    case Op::UCountLeadingZerosINTEL:
    case Op::UCountTrailingZerosINTEL:
    case Op::AbsISubINTEL:
    case Op::AbsUSubINTEL:
    case Op::IAddSatINTEL:
    case Op::UAddSatINTEL:
    case Op::IAverageINTEL:
    case Op::UAverageINTEL:
    case Op::IAverageRoundedINTEL:
    case Op::UAverageRoundedINTEL:
    case Op::ISubSatINTEL:
    case Op::USubSatINTEL:
    case Op::IMul32x16INTEL:
    case Op::UMul32x16INTEL:
    case Op::VmeImageINTEL:
    case Op::TypeVmeImageINTEL:
    case Op::TypeAvcImePayloadINTEL:
    case Op::TypeAvcRefPayloadINTEL:
    case Op::TypeAvcSicPayloadINTEL:
    case Op::TypeAvcMcePayloadINTEL:
    case Op::TypeAvcMceResultINTEL:
    case Op::TypeAvcImeResultINTEL:
    case Op::TypeAvcImeResultSingleReferenceStreamoutINTEL:
    case Op::TypeAvcImeResultDualReferenceStreamoutINTEL:
    case Op::TypeAvcImeSingleReferenceStreaminINTEL:
    case Op::TypeAvcImeDualReferenceStreaminINTEL:
    case Op::TypeAvcRefResultINTEL:
    case Op::TypeAvcSicResultINTEL:
    case Op::SubgroupAvcMceGetDefaultInterBaseMultiReferencePenaltyINTEL:
    case Op::SubgroupAvcMceSetInterBaseMultiReferencePenaltyINTEL:
    case Op::SubgroupAvcMceGetDefaultInterShapePenaltyINTEL:
    case Op::SubgroupAvcMceSetInterShapePenaltyINTEL:
    case Op::SubgroupAvcMceGetDefaultInterDirectionPenaltyINTEL:
    case Op::SubgroupAvcMceSetInterDirectionPenaltyINTEL:
    case Op::SubgroupAvcMceGetDefaultIntraLumaShapePenaltyINTEL:
    case Op::SubgroupAvcMceGetDefaultInterMotionVectorCostTableINTEL:
    case Op::SubgroupAvcMceGetDefaultHighPenaltyCostTableINTEL:
    case Op::SubgroupAvcMceGetDefaultMediumPenaltyCostTableINTEL:
    case Op::SubgroupAvcMceGetDefaultLowPenaltyCostTableINTEL:
    case Op::SubgroupAvcMceSetMotionVectorCostFunctionINTEL:
    case Op::SubgroupAvcMceGetDefaultIntraLumaModePenaltyINTEL:
    case Op::SubgroupAvcMceGetDefaultNonDcLumaIntraPenaltyINTEL:
    case Op::SubgroupAvcMceGetDefaultIntraChromaModeBasePenaltyINTEL:
    case Op::SubgroupAvcMceSetAcOnlyHaarINTEL:
    case Op::SubgroupAvcMceSetSourceInterlacedFieldPolarityINTEL:
    case Op::SubgroupAvcMceSetSingleReferenceInterlacedFieldPolarityINTEL:
    case Op::SubgroupAvcMceSetDualReferenceInterlacedFieldPolaritiesINTEL:
    case Op::SubgroupAvcMceConvertToImePayloadINTEL:
    case Op::SubgroupAvcMceConvertToImeResultINTEL:
    case Op::SubgroupAvcMceConvertToRefPayloadINTEL:
    case Op::SubgroupAvcMceConvertToRefResultINTEL:
    case Op::SubgroupAvcMceConvertToSicPayloadINTEL:
    case Op::SubgroupAvcMceConvertToSicResultINTEL:
    case Op::SubgroupAvcMceGetMotionVectorsINTEL:
    case Op::SubgroupAvcMceGetInterDistortionsINTEL:
    case Op::SubgroupAvcMceGetBestInterDistortionsINTEL:
    case Op::SubgroupAvcMceGetInterMajorShapeINTEL:
    case Op::SubgroupAvcMceGetInterMinorShapeINTEL:
    case Op::SubgroupAvcMceGetInterDirectionsINTEL:
    case Op::SubgroupAvcMceGetInterMotionVectorCountINTEL:
    case Op::SubgroupAvcMceGetInterReferenceIdsINTEL:
    case Op::SubgroupAvcMceGetInterReferenceInterlacedFieldPolaritiesINTEL:
    case Op::SubgroupAvcImeInitializeINTEL:
    case Op::SubgroupAvcImeSetSingleReferenceINTEL:
    case Op::SubgroupAvcImeSetDualReferenceINTEL:
    case Op::SubgroupAvcImeRefWindowSizeINTEL:
    case Op::SubgroupAvcImeAdjustRefOffsetINTEL:
    case Op::SubgroupAvcImeConvertToMcePayloadINTEL:
    case Op::SubgroupAvcImeSetMaxMotionVectorCountINTEL:
    case Op::SubgroupAvcImeSetUnidirectionalMixDisableINTEL:
    case Op::SubgroupAvcImeSetEarlySearchTerminationThresholdINTEL:
    case Op::SubgroupAvcImeSetWeightedSadINTEL:
    case Op::SubgroupAvcImeEvaluateWithSingleReferenceINTEL:
    case Op::SubgroupAvcImeEvaluateWithDualReferenceINTEL:
    case Op::SubgroupAvcImeEvaluateWithSingleReferenceStreaminINTEL:
    case Op::SubgroupAvcImeEvaluateWithDualReferenceStreaminINTEL:
    case Op::SubgroupAvcImeEvaluateWithSingleReferenceStreamoutINTEL:
    case Op::SubgroupAvcImeEvaluateWithDualReferenceStreamoutINTEL:
    case Op::SubgroupAvcImeEvaluateWithSingleReferenceStreaminoutINTEL:
    case Op::SubgroupAvcImeEvaluateWithDualReferenceStreaminoutINTEL:
    case Op::SubgroupAvcImeConvertToMceResultINTEL:
    case Op::SubgroupAvcImeGetSingleReferenceStreaminINTEL:
    case Op::SubgroupAvcImeGetDualReferenceStreaminINTEL:
    case Op::SubgroupAvcImeStripSingleReferenceStreamoutINTEL:
    case Op::SubgroupAvcImeStripDualReferenceStreamoutINTEL:
    case Op::SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeMotionVectorsINTEL:
    case Op::SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeDistortionsINTEL:
    case Op::SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeReferenceIdsINTEL:
    case Op::SubgroupAvcImeGetStreamoutDualReferenceMajorShapeMotionVectorsINTEL:
    case Op::SubgroupAvcImeGetStreamoutDualReferenceMajorShapeDistortionsINTEL:
    case Op::SubgroupAvcImeGetStreamoutDualReferenceMajorShapeReferenceIdsINTEL:
    case Op::SubgroupAvcImeGetBorderReachedINTEL:
    case Op::SubgroupAvcImeGetTruncatedSearchIndicationINTEL:
    case Op::SubgroupAvcImeGetUnidirectionalEarlySearchTerminationINTEL:
    case Op::SubgroupAvcImeGetWeightingPatternMinimumMotionVectorINTEL:
    case Op::SubgroupAvcImeGetWeightingPatternMinimumDistortionINTEL:
    case Op::SubgroupAvcFmeInitializeINTEL:
    case Op::SubgroupAvcBmeInitializeINTEL:
    case Op::SubgroupAvcRefConvertToMcePayloadINTEL:
    case Op::SubgroupAvcRefSetBidirectionalMixDisableINTEL:
    case Op::SubgroupAvcRefSetBilinearFilterEnableINTEL:
    case Op::SubgroupAvcRefEvaluateWithSingleReferenceINTEL:
    case Op::SubgroupAvcRefEvaluateWithDualReferenceINTEL:
    case Op::SubgroupAvcRefEvaluateWithMultiReferenceINTEL:
    case Op::SubgroupAvcRefEvaluateWithMultiReferenceInterlacedINTEL:
    case Op::SubgroupAvcRefConvertToMceResultINTEL:
    case Op::SubgroupAvcSicInitializeINTEL:
    case Op::SubgroupAvcSicConfigureSkcINTEL:
    case Op::SubgroupAvcSicConfigureIpeLumaINTEL:
    case Op::SubgroupAvcSicConfigureIpeLumaChromaINTEL:
    case Op::SubgroupAvcSicGetMotionVectorMaskINTEL:
    case Op::SubgroupAvcSicConvertToMcePayloadINTEL:
    case Op::SubgroupAvcSicSetIntraLumaShapePenaltyINTEL:
    case Op::SubgroupAvcSicSetIntraLumaModeCostFunctionINTEL:
    case Op::SubgroupAvcSicSetIntraChromaModeCostFunctionINTEL:
    case Op::SubgroupAvcSicSetBilinearFilterEnableINTEL:
    case Op::SubgroupAvcSicSetSkcForwardTransformEnableINTEL:
    case Op::SubgroupAvcSicSetBlockBasedRawSkipSadINTEL:
    case Op::SubgroupAvcSicEvaluateIpeINTEL:
    case Op::SubgroupAvcSicEvaluateWithSingleReferenceINTEL:
    case Op::SubgroupAvcSicEvaluateWithDualReferenceINTEL:
    case Op::SubgroupAvcSicEvaluateWithMultiReferenceINTEL:
    case Op::SubgroupAvcSicEvaluateWithMultiReferenceInterlacedINTEL:
    case Op::SubgroupAvcSicConvertToMceResultINTEL:
    case Op::SubgroupAvcSicGetIpeLumaShapeINTEL:
    case Op::SubgroupAvcSicGetBestIpeLumaDistortionINTEL:
    case Op::SubgroupAvcSicGetBestIpeChromaDistortionINTEL:
    case Op::SubgroupAvcSicGetPackedIpeLumaModesINTEL:
    case Op::SubgroupAvcSicGetIpeChromaModeINTEL:
    case Op::SubgroupAvcSicGetPackedSkcLumaCountThresholdINTEL:
    case Op::SubgroupAvcSicGetPackedSkcLumaSumThresholdINTEL:
    case Op::SubgroupAvcSicGetInterRawSadsINTEL:
    {
      RDCERR("Unsupported extension opcode used %s", ToStr(opdata.op).c_str());

      ShaderVariable var("", 0U, 0U, 0U, 0U);
      var.columns = 1;

      SetDst(opdata.result, var);

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
    case Op::SizeOf:
    case Op::CopyMemorySized:
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

    case Op::Max: RDCWARN("Unhandled SPIR-V operation %s", ToStr(opdata.op).c_str()); break;
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

  SkipIgnoredInstructions();

  // set the state's next instruction (if we have one) to ours, bounded by how many
  // instructions there are
  if(m_State)
    m_State->nextInstruction = RDCMIN(nextInstruction, debugger.GetNumInstructions() - 1);

  m_State = NULL;
}

};    // namespace rdcspv
