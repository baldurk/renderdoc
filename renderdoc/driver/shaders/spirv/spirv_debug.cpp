/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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
#include "core/settings.h"
#include "maths/half_convert.h"
#include "os/os_specific.h"
#include "spirv_op_helpers.h"
#include "var_dispatch_helpers.h"

static bool ContainsNaNInf(const ShaderVariable &var)
{
  bool ret = false;

  for(const ShaderVariable &member : var.members)
    ret |= ContainsNaNInf(member);

  int count = int(var.rows) * int(var.columns);

  for(int c = 0; c < count; c++)
  {
#undef _IMPL
#define _IMPL(T) ret |= RDCISINF(comp<T>(var, c)) || RDCISNAN(comp<T>(var, c))

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return ret;
}

namespace Bits
{
// add simple overloads that upcast for small types
inline uint16_t CountOnes(uint16_t value)
{
  return Bits::CountOnes((uint32_t)value) & 0xffffu;
}
inline uint8_t CountOnes(uint8_t value)
{
  return Bits::CountOnes((uint32_t)value) & 0xffu;
}

// on non-64-bit platforms we implement a two-half manual bitcount for 64-bit integers
#if DISABLED(RDOC_X64)
inline uint64_t CountOnes(uint64_t value)
{
  uint32_t words[2];
  memcpy(words, &value, sizeof(value));
  return Bits::CountOnes(words[0]) + Bits::CountOnes(words[1]);
}
#endif
}

namespace rdcspv
{
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

void ThreadState::FillCallstack(rdcarray<Id> &funcs)
{
  for(const StackFrame *frame : callstack)
    funcs.push_back(frame->function);
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
  }

  // start with just globals
  live = debugger.GetLiveGlobals();

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

  ShaderDebugState *state = m_State;

  // don't add variables if we don't have debug info, we'll add it on the first store to reduce
  // noise on unoptimised shaders with lots of variables and no scope information. However if we
  // have debug info we'll add the variable immediately because the source variable will only be
  // added at the correct scope and we want to display that before it's stored to.
  if(!debugger.HasDebugInfo())
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
    stackvar.name = GetRawName(decl.result);

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
    if(ptrid == Id())
      ptrid = pointer;

    // only track local writes when we don't have debug info, so we can track variables first
    // becoming alive
    bool firstLocalWrite = false;
    if(!debugger.HasDebugInfo())
      firstLocalWrite = ReferencePointer(ptrid);

    ShaderVariableChange basechange;

    if(debugger.IsOpaquePointer(ids[ptrid]) || debugger.IsPhysicalPointer(ids[ptrid]))
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

    // if this is the first local write, mark this variable as becoming alive here, instead of at
    // its declaration
    if(firstLocalWrite)
      basechange.before.name = "";

    m_State->changes.push_back(basechange);

    if(ptrIdx == -1)
      pointers.push_back(pointer);
    if(!pointers.contains(ptrid) && ptrid != Id())
      pointers.push_back(ptrid);

    for(size_t i = 0; i < pointers.size(); i++)
      lastWrite[pointers[i]] = m_State ? m_State->stepIndex : nextInstruction;
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

  ShaderVariable prev = ids[id];

  // if this id didn't exist before it's not a global so it's a local variable, function parameter,
  // or plain id. Track it in the current frame so it's emptied upon return
  if(prev.name.empty() && prev.type == VarType::Unknown)
    callstack.back()->idsCreated.push_back(id);

  ids[id] = val;
  ids[id].name = GetRawName(id);

  lastWrite[id] = m_State ? m_State->stepIndex : nextInstruction;

  auto it = std::lower_bound(live.begin(), live.end(), id);
  live.insert(it - live.begin(), id);

  if(val.type == VarType::GPUPointer && !debugger.IsPhysicalPointer(val))
  {
    Id ptrId = debugger.GetPointerBaseId(val);
    if(ptrId != Id() && ptrId != id)
    {
      if(!pointersForId[ptrId].contains(id))
        pointersForId[ptrId].push_back(id);
    }
  }

  if(m_State)
  {
    ShaderVariableChange change;
    change.before = debugger.GetPointerValue(prev);
    change.after = debugger.GetPointerValue(ids[id]);
    m_State->changes.push_back(change);
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

  for(const Id &id : oldLive)
  {
    if(liveGlobals.contains(id))
      continue;

    m_State->changes.push_back({debugger.GetPointerValue(ids[id])});

    if(ids[id].type == VarType::GPUPointer && !debugger.IsOpaquePointer(ids[id]) &&
       !debugger.IsPhysicalPointer(ids[id]))
    {
      Id ptrId = debugger.GetPointerBaseId(ids[id]);
      pointersForId[ptrId].removeOne(id);
      pointersForId.erase(id);
    }
  }

  for(const Id &id : newLive)
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
  ShaderVariable var = aval;

  for(uint8_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = comp<T>(bval, c) - comp<T>(aval, c)

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }

  return var;
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

bool ThreadState::ReferencePointer(Id id)
{
  bool firstLocalWrite = false;

  if(m_State)
  {
    StackFrame *frame = callstack.back();

    rdcstr name = GetRawName(id);

    // see if this is a local variable which is newly referenced, if so add source vars for it
    for(size_t i = 0; i < frame->locals.size(); i++)
    {
      if(name == frame->locals[i].name)
      {
        if(!frame->localsUsed.contains(id))
        {
          frame->localsUsed.push_back(id);
          firstLocalWrite = true;
        }

        break;
      }
    }
  }

  return firstLocalWrite;
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
    if(op == Op::Line || op == Op::NoLine || op == Op::Undef)
    {
      nextInstruction++;
      continue;
    }

    if(op == Op::ExtInst || op == Op::ExtInstWithForwardRefsKHR)
    {
      if(debugger.IsDebugExtInstSet(Id::fromWord(it.word(3))))
      {
        if(ShaderDbg(it.word(4)) != ShaderDbg::Value || !debugger.InDebugScope(nextInstruction))
        {
          nextInstruction++;
          continue;
        }
      }
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
        indices.push_back(uintComp(GetSrc(id), 0));

      SetDst(chain.result, debugger.MakeCompositePointer(
                               ids[chain.base], debugger.GetPointerBaseId(ids[chain.base]), indices));
      break;
    }
    case Op::PtrAccessChain:
    case Op::InBoundsPtrAccessChain:
    {
      OpPtrAccessChain chain(it);

      rdcarray<uint32_t> indices;
      // evaluate the indices
      indices.reserve(chain.indexes.size());
      for(Id id : chain.indexes)
        indices.push_back(uintComp(GetSrc(id), 0));

      ShaderVariable base = ids[chain.base];
      PointerVal val = base.GetPointer();
      int32_t element = intComp(GetSrc(chain.element), 0);
      // adjust the address by the element. We should have the array stride since the base pointer
      // must point into an array and we can't go outside it.
      base.SetTypedPointer(val.pointer + element * debugger.GetPointerArrayStride(base), val.shader,
                           val.pointerTypeID);
      SetDst(chain.result,
             debugger.MakeCompositePointer(base, debugger.GetPointerBaseId(base), indices));
      break;
    }
    case Op::ArrayLength:
    {
      OpArrayLength len(it);

      ShaderVariable structPointer = GetSrc(len.structure);

      // "Structure must be a logical pointer..." which is opaqaue in RD terminolgoy
      RDCASSERT(debugger.IsOpaquePointer(structPointer));

      // get the pointer base offset (should be zero for any binding but could be non-zero for a
      // buffer_device_address pointer)
      uint64_t offset = debugger.GetPointerByteOffset(structPointer);

      // add the offset of the member
      const DataType &pointerType = debugger.GetTypeForId(len.structure);
      const DataType &structType = debugger.GetType(pointerType.InnerType());

      offset += structType.children[len.arraymember].decorations.offset;

      ShaderVariable result;
      result.rows = result.columns = 1;

      ShaderBindIndex bind = debugger.GetPointerValue(structPointer).GetBindIndex();

      uint64_t byteLen = debugger.GetAPIWrapper()->GetBufferLength(bind) - offset;

      const Decorations &dec = debugger.GetDecorations(structType.children[len.arraymember].type);

      RDCASSERT(dec.flags & Decorations::HasArrayStride);
      byteLen /= dec.arrayStride;

      // Result Type must be an OpTypeInt with 32-bit Width and 0 Signedness
      result.type = VarType::UInt;
      setUintComp(result, 0, uint32_t(byteLen));

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
        setUintComp(var, 0, isEqual ? 1 : 0);
      else
        setUintComp(var, 0, isEqual ? 0 : 1);

      SetDst(equal.result, var);
      break;
    }
    // physical storage pointers
    case Op::ConvertPtrToU:
    {
      OpConvertPtrToU convert(it);
      ShaderVariable ptr = GetSrc(convert.pointer);
      const DataType &resultType = debugger.GetType(convert.resultType);
      ptr.type = resultType.scalar().Type();
      SetDst(convert.result, ptr);
      break;
    }
    case Op::ConvertUToPtr:
    {
      OpConvertUToPtr convert(it);
      ShaderVariable ptr = GetSrc(convert.integerValue);
      const DataType &type = debugger.GetType(convert.resultType);
      SetDst(convert.result, debugger.MakeTypedPointer(ptr.value.u64v[0], type));
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
      {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = fabs(comp<T>(var, c)) + fabs(comp<T>(ddy, c))

        IMPL_FOR_FLOAT_TYPES(_IMPL);
      }

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
            copyComp(*mod, row * mod->columns + column, obj, row);
        }
        else
        {
          // if it's a vector, replace one scalar
          copyComp(*mod, idx, obj, 0);
        }
      }
      else if(i + 2 == insert.indexes.size())
      {
        // two more indices, selecting column then scalar in a matrix
        uint32_t column = insert.indexes[i];
        uint32_t row = insert.indexes[i + 1];

        copyComp(*mod, row * mod->columns + column, obj, 0);
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
          mem = GetSrc(construct.constituents[i]);
          if(!type.children[i].name.empty())
            mem.name = type.children[i].name;
          else
            mem.name = StringFormat::Fmt("_child%zu", i);
        }
      }
      else if(type.type == DataType::VectorType)
      {
        RDCASSERT(construct.constituents.size() <= 4);

        var.type = type.scalar().Type();
        var.rows = 1U;
        var.columns = RDCMAX(1U, type.vector().count) & 0xff;

        // it is possible to construct larger vectors from a collection of scalars and smaller
        // vectors.
        uint32_t dst = 0;
        for(size_t i = 0; i < construct.constituents.size(); i++)
        {
          ShaderVariable src = GetSrc(construct.constituents[i]);

          RDCASSERTEQUAL(src.rows, 1);

          for(uint32_t j = 0; j < src.columns; j++)
            copyComp(var, dst++, src, j);
        }
      }
      else if(type.type == DataType::MatrixType)
      {
        // matrices are constructed from a list of columns
        var.type = type.scalar().Type();
        var.columns = RDCMAX(1U, type.matrix().count) & 0xff;
        var.rows = RDCMAX(1U, type.vector().count) & 0xff;

        RDCASSERTEQUAL(var.columns, construct.constituents.size());

        rdcarray<ShaderVariable> columns;
        columns.resize(construct.constituents.size());
        for(size_t i = 0; i < construct.constituents.size(); i++)
          columns[i] = GetSrc(construct.constituents[i]);

        for(uint32_t r = 0; r < var.rows; r++)
          for(uint32_t c = 0; c < var.columns; c++)
            copyComp(var, r * var.columns + c, columns[c], r);
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
      var.columns = RDCMAX(1U, (uint32_t)shuffle.components.size()) & 0xff;

      ShaderVariable src1 = GetSrc(shuffle.vector1);
      ShaderVariable src2 = GetSrc(shuffle.vector2);

      uint32_t vec1Cols = src1.columns;

      for(uint32_t i = 0; i < shuffle.components.size(); i++)
      {
        uint32_t c = shuffle.components[i];

        // "A Component literal may also be FFFFFFFF, which means the corresponding result component
        // has no source and is undefined."
        // If it has no defined source, we can use 0 safely and know that it's at least going to
        // index validly
        if(c == ~0U)
          c = 0;

        if(c < vec1Cols)
          copyComp(var, i, src1, c);
        else
          copyComp(var, i, src2, c - vec1Cols);
      }

      SetDst(shuffle.result, var);

      break;
    }
    case Op::VectorExtractDynamic:
    {
      OpVectorExtractDynamic extract(it);

      ShaderVariable var = GetSrc(extract.vector);
      ShaderVariable idx = GetSrc(extract.index);

      uint32_t comp = uintComp(idx, 0);

      if(comp != 0)
        copyComp(var, 0, var, comp);

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

      uint32_t comp = uintComp(idx, 0);

      copyComp(var, comp, scalar, 0);

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
        if(uintComp(cond, 0) == 0)
          var = b;
      }
      else
      {
        for(uint8_t c = 0; c < cond.columns; c++)
        {
          if(uintComp(cond, c) == 0)
            copyComp(var, c, b, c);
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
      OpConvertFToS convert(it);

      const ShaderVariable &var = GetSrc(convert.floatValue);
      const DataType &resultType = debugger.GetType(convert.resultType);

      ShaderVariable conv = var;
      conv.type = resultType.scalar().Type();

      if(opdata.op == Op::ConvertFToS)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          double x = 0.0;

#undef _IMPL
#define _IMPL(T) x = comp<T>(var, c);
          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, var.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<S>(conv, c) = (S)x;
          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, conv.type);
        }
      }
      else if(opdata.op == Op::ConvertFToU)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          double x = 0.0;

#undef _IMPL
#define _IMPL(T) x = comp<T>(var, c);
          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, var.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<U>(conv, c) = (U)x;
          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, conv.type);
        }
      }
      else if(opdata.op == Op::ConvertSToF)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          int64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<S>(var, c);
          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, var.type);

          if(conv.type == VarType::Float)
            comp<float>(conv, c) = (float)x;
          else if(conv.type == VarType::Half)
            comp<half_float::half>(conv, c) = (float)x;
          else if(conv.type == VarType::Double)
            comp<double>(conv, c) = (double)x;
        }
      }
      else if(opdata.op == Op::ConvertUToF)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
          uint64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<U>(var, c);
          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, var.type);

          if(conv.type == VarType::Float)
            comp<float>(conv, c) = (float)x;
          else if(conv.type == VarType::Half)
            comp<half_float::half>(conv, c) = (float)x;
          else if(conv.type == VarType::Double)
            comp<double>(conv, c) = (double)x;
        }
      }

      SetDst(convert.result, conv);
      break;
    }
    case Op::QuantizeToF16:
    {
      OpQuantizeToF16 quant(it);

      ShaderVariable var = GetSrc(quant.value);
      ShaderVariable conv = var;

      // Result Type must be a scalar or vector of floating-point type. The component width must be
      // 32 bits.
      conv.type = VarType::Float;

      for(uint8_t c = 0; c < var.columns; c++)
        setFloatComp(conv, c, ConvertFromHalf(ConvertToHalf(floatComp(var, c))));

      SetDst(quant.result, conv);
      break;
    }
    case Op::UConvert:
    {
      OpUConvert cast(it);

      const ShaderVariable &var = GetSrc(cast.unsignedValue);
      const DataType &resultType = debugger.GetType(cast.resultType);

      ShaderVariable conv = var;
      conv.type = resultType.scalar().Type();

      RDCEraseEl(conv.value);

      // this is a zero-extend or truncate. Column-wise we read the variable out into a u64 then
      // cast
      for(uint8_t c = 0; c < var.columns; c++)
      {
        uint64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<U>(var, c);
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, var.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<U>(conv, c) = (U)x;
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, conv.type);
      }

      SetDst(cast.result, conv);
      break;
    }
    case Op::SConvert:
    {
      OpSConvert cast(it);

      const ShaderVariable &var = GetSrc(cast.signedValue);
      const DataType &resultType = debugger.GetType(cast.resultType);

      ShaderVariable conv = var;
      conv.type = resultType.scalar().Type();

      RDCEraseEl(conv.value);

      // this is a sign-extend or truncate. Column-wise we read the variable out into a u64 then
      // cast
      for(uint8_t c = 0; c < var.columns; c++)
      {
        int64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<S>(var, c);
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, var.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<S>(conv, c) = (S)x;
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, conv.type);
      }

      SetDst(cast.result, var);
      break;
    }
    case Op::FConvert:
    {
      OpFConvert cast(it);

      const ShaderVariable &var = GetSrc(cast.floatValue);
      const DataType &resultType = debugger.GetType(cast.resultType);

      ShaderVariable conv = var;
      conv.type = resultType.scalar().Type();

      // we can safely upconvert to double as an intermediary because the IEEE format is the same.
      // All we're doing effectively is sign extending the exponent and zero extending the mantissa.
      for(uint8_t c = 0; c < var.columns; c++)
      {
        double x = 0.0;

#undef _IMPL
#define _IMPL(T) x = comp<T>(var, c);
        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, var.type);

#undef _IMPL
#define _IMPL(T) comp<T>(conv, c) = (T)x;
        // IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, conv.type);

        if(conv.type == VarType::Float)
          comp<float>(conv, c) = (float)x;
        else if(conv.type == VarType::Half)
          comp<half_float::half>(conv, c) = (float)x;
        else if(conv.type == VarType::Double)
          comp<double>(conv, c) = (double)x;
      }

      SetDst(cast.result, conv);
      break;
    }
    case Op::Bitcast:
    {
      OpBitcast cast(it);

      const DataType &type = debugger.GetType(cast.resultType);
      ShaderVariable var = GetSrc(cast.operand);

      if(type.type == DataType::PointerType)
      {
        var = debugger.MakeTypedPointer(var.value.u64v[0], type);
      }
      else if((type.type == DataType::ScalarType && var.columns == 1) ||
              type.vector().count == var.columns)
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
        uint32_t dstColumns = (type.type == DataType::ScalarType) ? 1 : type.vector().count;

        // must be identical bit count
        RDCASSERT(dstByteCount * dstColumns == srcByteCount * var.columns);

        // because this is a bitcast, we leave var.value entirely alone. There is the same number of
        // bytes so the union handles it. E.g. uv[0], uv[1] being bitcast to a single 64-bit
        // corresponds exactly to the LSB and MSB of u64v[0]

        var.type = type.scalar().Type();
        var.columns = dstColumns & 0xff;
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
    case Op::ExtInstWithForwardRefsKHR:
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
      OpFMul compare(it);

      ShaderVariable a = GetSrc(compare.operand1);
      ShaderVariable b = GetSrc(compare.operand2);
      ShaderVariable var = a;

      if(opdata.op == Op::IEqual || opdata.op == Op::LogicalEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<I>(a, c) == comp<I>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::INotEqual || opdata.op == Op::LogicalNotEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<I>(a, c) != comp<I>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::LogicalAnd)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<I>(a, c) & comp<I>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::LogicalOr)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<I>(a, c) | comp<I>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::UGreaterThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(a, c) > comp<U>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::UGreaterThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(a, c) >= comp<U>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::ULessThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(a, c) < comp<U>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::ULessThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(a, c) <= comp<U>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::SGreaterThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<S>(a, c) > comp<S>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::SGreaterThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<S>(a, c) >= comp<S>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::SLessThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<S>(a, c) < comp<S>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::SLessThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<S>(a, c) <= comp<S>(b, c) ? 1 : 0

          IMPL_FOR_INT_TYPES(_IMPL);
        }
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
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) == comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FOrdNotEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) != comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FOrdGreaterThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) > comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FOrdGreaterThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) >= comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FOrdLessThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) < comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FOrdLessThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) <= comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }

      if(opdata.op == Op::FUnordEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) != comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FUnordNotEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) == comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FUnordGreaterThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) <= comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FUnordGreaterThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) < comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FUnordLessThan)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) >= comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FUnordLessThanEqual)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(var, c) = (comp<T>(a, c) <= comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }

      var.type = VarType::Bool;

      SetDst(compare.result, var);
      break;
    }
    case Op::LogicalNot:
    {
      OpLogicalNot negate(it);

      ShaderVariable var = GetSrc(negate.operand);

      for(uint8_t c = 0; c < var.columns; c++)
        setUintComp(var, c, 1U - uintComp(var, c));

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
          setUintComp(var, 0, uintComp(var, 0) | uintComp(var, c));
        else
          setUintComp(var, 0, uintComp(var, 0) & uintComp(var, c));
      }

      var.columns = 1;

      SetDst(any.result, var);
      break;
    }
    case Op::IsNan:
    {
      OpIsNan is(it);

      ShaderVariable x = GetSrc(is.x);
      ShaderVariable var = x;

      for(uint8_t c = 0; c < var.columns; c++)
      {
#undef _IMPL
#define _IMPL(T) setUintComp(var, c, RDCISNAN(comp<T>(x, c)) ? 1 : 0)

        IMPL_FOR_FLOAT_TYPES(_IMPL);
      }

      var.type = VarType::Bool;

      SetDst(is.result, var);
      break;
    }
    case Op::IsInf:
    {
      OpIsNan is(it);

      ShaderVariable x = GetSrc(is.x);
      ShaderVariable var = x;

      for(uint8_t c = 0; c < var.columns; c++)
      {
#undef _IMPL
#define _IMPL(T) setUintComp(var, c, RDCISINF(comp<T>(x, c)) ? 1 : 0);

        IMPL_FOR_FLOAT_TYPES(_IMPL);
      }

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

      const DataType &type = debugger.GetType(bitwise.resultType);
      ShaderVariable var = GetSrc(bitwise.base);
      ShaderVariable ret = var;
      ret.type = type.scalar().Type();

      for(uint8_t c = 0; c < var.columns; c++)
      {
#undef _IMPL
#define _IMPL(I, S, U) setUintComp(ret, c, (uint32_t)Bits::CountOnes(comp<U>(var, c)));

        IMPL_FOR_INT_TYPES(_IMPL);
      }

      SetDst(bitwise.result, ret);
      break;
    }
    case Op::BitReverse:
    {
      OpBitReverse bitwise(it);

      ShaderVariable var = GetSrc(bitwise.base);

      for(uint8_t c = 0; c < var.columns; c++)
      {
#undef _IMPL
#define _IMPL(I, S, U)                  \
  U v = comp<U>(var, c);                \
  comp<U>(var, c) = 0;                  \
  for(uint8_t b = 0; b < 32; b++)       \
  {                                     \
    uint32_t bit = (v >> b) & 0x1;      \
    comp<U>(var, c) |= bit << (31 - b); \
  }

        IMPL_FOR_INT_TYPES(_IMPL);
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
#undef _IMPL
#define _IMPL(I, S, U)                               \
  const U mask = (U(1) << comp<U>(count, c)) - U(1); \
                                                     \
  comp<U>(var, c) >>= comp<U>(offset, c);            \
  comp<U>(var, c) &= mask;                           \
                                                     \
  if(opdata.op == Op::BitFieldSExtract)              \
  {                                                  \
    U topbit = (mask + U(1)) >> U(1);                \
    if(comp<U>(var, c) & topbit)                     \
      comp<U>(var, c) |= (~0ULL ^ mask);             \
  }

        IMPL_FOR_INT_TYPES(_IMPL);
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
#undef _IMPL
#define _IMPL(I, S, U)                               \
  const U mask = (U(1) << comp<U>(count, c)) - U(1); \
                                                     \
  comp<U>(var, c) &= ~(mask << comp<U>(offset, c));  \
  comp<U>(var, c) |= (comp<U>(insert, c) & mask) << comp<U>(offset, c);

        IMPL_FOR_INT_TYPES(_IMPL);
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
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(var, c) | comp<U>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::BitwiseAnd)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(var, c) & comp<U>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::BitwiseXor)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(var, c) ^ comp<U>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::ShiftLeftLogical)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(var, c) << comp<U>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::ShiftRightArithmetic)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(var, c) = comp<S>(var, c) >> comp<S>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::ShiftRightLogical)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(var, c) >> comp<U>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }

      SetDst(bitwise.result, var);
      break;
    }
    case Op::GroupNonUniformBitwiseOr:
    {
      OpGroupNonUniformBitwiseOr group(it);

      ShaderVariable var;

      for(size_t i = 0; i < workgroup.size(); i++)
      {
        if(i == 0)
        {
          var = workgroup[i].GetSrc(group.value);
        }
        else
        {
          ShaderVariable b = workgroup[i].GetSrc(group.value);

          for(uint8_t c = 0; c < var.columns; c++)
          {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = comp<U>(var, c) | comp<U>(b, c)

            IMPL_FOR_INT_TYPES(_IMPL);
          }
        }
      }

      SetDst(group.result, var);

      break;
    }
    case Op::Not:
    {
      OpNot bitwise(it);

      ShaderVariable var = GetSrc(bitwise.operand);

      for(uint8_t c = 0; c < var.columns; c++)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = ~comp<U>(var, c)

        IMPL_FOR_INT_TYPES(_IMPL);
      }

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
        {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) *= comp<T>(b, c)

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FDiv)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) /= comp<T>(b, c)

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FMod)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T)                                \
  T af = comp<T>(var, c), bf = comp<T>(b, c);   \
  comp<T>(var, c) = fmod(af, bf);               \
  if(comp<T>(var, c) < 0.0f && bf >= 0.0f)      \
    comp<T>(var, c) += fabs(bf);                \
  else if(comp<T>(var, c) >= 0.0f && bf < 0.0f) \
    comp<T>(var, c) -= fabs(bf);

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FRem)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T)                                \
  T af = comp<T>(var, c), bf = comp<T>(b, c);   \
  comp<T>(var, c) = fmod(af, bf);               \
  if(comp<T>(var, c) < 0.0f && af >= 0.0f)      \
    comp<T>(var, c) += fabs(bf);                \
  else if(comp<T>(var, c) >= 0.0f && af < 0.0f) \
    comp<T>(var, c) -= fabs(bf);

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FAdd)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) += comp<T>(b, c)

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::FSub)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) -= comp<T>(b, c)

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::IMul)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(var, c) *= comp<I>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::SDiv)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U)                                   \
  if(comp<S>(b, c) != 0)                                 \
  {                                                      \
    comp<S>(var, c) /= comp<S>(b, c);                    \
  }                                                      \
  else                                                   \
  {                                                      \
    comp<U>(var, c) = 0;                                 \
    if(m_State)                                          \
      m_State->flags |= ShaderEvents::GeneratedNanOrInf; \
  }

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::UDiv)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U)                                   \
  if(comp<U>(b, c) != 0)                                 \
  {                                                      \
    comp<U>(var, c) /= comp<U>(b, c);                    \
  }                                                      \
  else                                                   \
  {                                                      \
    comp<U>(var, c) = 0;                                 \
    if(m_State)                                          \
      m_State->flags |= ShaderEvents::GeneratedNanOrInf; \
  }

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::UMod)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U)                                   \
  if(comp<U>(b, c) != 0)                                 \
  {                                                      \
    comp<U>(var, c) %= comp<U>(b, c);                    \
  }                                                      \
  else                                                   \
  {                                                      \
    comp<U>(var, c) = 0;                                 \
    if(m_State)                                          \
      m_State->flags |= ShaderEvents::GeneratedNanOrInf; \
  }

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::SRem || opdata.op == Op::SMod)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U)                                   \
  if(comp<S>(b, c) != 0)                                 \
  {                                                      \
    comp<S>(var, c) %= comp<S>(b, c);                    \
  }                                                      \
  else                                                   \
  {                                                      \
    comp<S>(var, c) = 0;                                 \
    if(m_State)                                          \
      m_State->flags |= ShaderEvents::GeneratedNanOrInf; \
  }

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::IAdd)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(var, c) += comp<I>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::ISub)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(var, c) -= comp<I>(b, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
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

      uint32_t elemSize = VarTypeByteSize(a.type);
      uint32_t elemBits = elemSize * 8;

      if(opdata.op == Op::UMulExtended)
      {
        // if this is less than 64-bit precision inputs, we can just upcast, do the mul, and then
        // mask off the bits we care about
        if(elemSize < 8)
        {
          uint32_t mask = 0xFFFFFFFFu >> (32 - elemBits);
          for(uint8_t c = 0; c < a.columns; c++)
          {
            const uint64_t x = uintComp(a, c);
            const uint64_t y = uintComp(b, c);
            const uint64_t res = x * y;

            setUintComp(lsb, c, uint32_t(res & mask));
            setUintComp(msb, c, uint32_t(res >> elemBits));
          }
        }
        else
        {
          RDCERR("Unsupported UMulExtended on 64-bit operands");
        }
      }
      else if(opdata.op == Op::SMulExtended)
      {
        if(elemSize < 8)
        {
          uint32_t mask = 0xFFFFFFFFu >> (32 - elemBits);
          for(uint8_t c = 0; c < a.columns; c++)
          {
            const int64_t x = intComp(a, c);
            const int64_t y = intComp(b, c);
            const int64_t res = x * y;

            setIntComp(lsb, c, int32_t(res & mask));
            setIntComp(msb, c, int32_t(res >> elemBits));
          }
        }
        else
        {
          RDCERR("Unsupported SMulExtended on 64-bit operands");
        }
      }
      else if(opdata.op == Op::IAddCarry)
      {
        for(uint8_t c = 0; c < a.columns; c++)
        {
// unsigned overflow is well-defined to wrap around, giving us the lsb we want.
// if the result is less than one of the operands, we overflowed so set msb
#undef _IMPL
#define _IMPL(I, S, U)                             \
  comp<U>(lsb, c) = comp<U>(a, c) + comp<U>(b, c); \
  comp<U>(msb, c) = (comp<U>(lsb, c) < comp<U>(b, c)) ? 1 : 0;

          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
      }
      else if(opdata.op == Op::ISubBorrow)
      {
        for(uint8_t c = 0; c < a.columns; c++)
        {
          // if b <= a we don't need to borrow, otherwise set the borrow bit

#undef _IMPL
#define _IMPL(I, S, U)                                              \
  if(comp<U>(b, c) <= comp<U>(a, c))                                \
  {                                                                 \
    comp<U>(msb, c) = 0;                                            \
    comp<U>(lsb, c) = comp<U>(a, c) - comp<U>(b, c);                \
  }                                                                 \
  else                                                              \
  {                                                                 \
    comp<U>(msb, c) = 1;                                            \
    comp<U>(lsb, c) = ~0ULL - (comp<U>(b, c) - comp<U>(a, c) - 1U); \
  }

          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
      }

      ShaderVariable result;
      result.rows = 1;
      result.columns = 1;
      result.type = VarType::Struct;
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
        {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = -comp<T>(var, c)

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }
      else if(opdata.op == Op::SNegate)
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(var, c) = -comp<S>(var, c)

          IMPL_FOR_INT_TYPES(_IMPL);
        }
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

#undef _IMPL
#define _IMPL(T)                            \
  T ret(0.0);                               \
  for(uint8_t c = 0; c < var.columns; c++)  \
    ret += comp<T>(var, c) * comp<T>(b, c); \
  comp<T>(var, 0) = ret;

      IMPL_FOR_FLOAT_TYPES(_IMPL);

      var.columns = 1;

      SetDst(dot.result, var);
      break;
    }
    case Op::VectorTimesScalar:
    {
      OpVectorTimesScalar mul(it);

      ShaderVariable var = GetSrc(mul.vector);
      ShaderVariable scalar = GetSrc(mul.scalar);

      for(uint8_t c = 0; c < var.columns; c++)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) *= comp<T>(scalar, 0)

        IMPL_FOR_FLOAT_TYPES(_IMPL);
      }

      SetDst(mul.result, var);
      break;
    }
    case Op::MatrixTimesScalar:
    {
      OpMatrixTimesScalar mul(it);

      ShaderVariable var = GetSrc(mul.matrix);
      ShaderVariable scalar = GetSrc(mul.scalar);

      for(uint8_t c = 0; c < var.rows * var.columns; c++)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) *= comp<T>(scalar, 0)

        IMPL_FOR_FLOAT_TYPES(_IMPL);
      }

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

      const DataType &type = debugger.GetType(mul.resultType);
      RDCASSERTEQUAL(type.vector().count, var.columns);
      RDCASSERTEQUAL(matrix.rows, vector.columns);

      for(uint8_t c = 0; c < matrix.columns; c++)
      {
#undef _IMPL
#define _IMPL(T)                           \
  comp<T>(var, c) = 0.0;                   \
  for(uint8_t r = 0; r < matrix.rows; r++) \
    comp<T>(var, c) += comp<T>(matrix, r * matrix.columns + c) * comp<T>(vector, r);

        IMPL_FOR_FLOAT_TYPES(_IMPL);
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
      {
        for(uint8_t c = 0; c < var.columns; c++)
        {
#undef _IMPL
#define _IMPL(T) comp<T>(var, r * var.columns + c) = comp<T>(matrix, c * matrix.columns + r)

          IMPL_FOR_FLOAT_TYPES(_IMPL);
        }
      }

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

      const DataType &type = debugger.GetType(mul.resultType);
      RDCASSERTEQUAL(type.vector().count, var.columns);
      RDCASSERTEQUAL(matrix.columns, vector.columns);

      for(uint8_t r = 0; r < matrix.rows; r++)
      {
#undef _IMPL
#define _IMPL(T)                              \
  comp<T>(var, r) = 0.0;                      \
  for(uint8_t c = 0; c < matrix.columns; c++) \
    comp<T>(var, r) += comp<T>(matrix, r * matrix.columns + c) * comp<T>(vector, c);

        IMPL_FOR_FLOAT_TYPES(_IMPL);
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

      RDCASSERTEQUAL(left.columns, right.rows);

      for(uint8_t dstr = 0; dstr < var.rows; dstr++)
      {
        for(uint8_t dstc = 0; dstc < var.columns; dstc++)
        {
#undef _IMPL
#define _IMPL(T)                                       \
  T &dstval = comp<T>(var, dstr * var.columns + dstc); \
  dstval = 0.0;                                        \
                                                       \
  for(uint8_t src = 0; src < right.rows; src++)        \
    dstval += comp<T>(left, dstr * left.columns + src) * comp<T>(right, src * right.columns + dstc);

          IMPL_FOR_FLOAT_TYPES(_IMPL);
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
#undef _IMPL
#define _IMPL(T) comp<T>(var, r * var.columns + c) = comp<T>(left, r) * comp<T>(right, c);
          IMPL_FOR_FLOAT_TYPES(_IMPL);
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
      result.type = VarType::Struct;
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
        gather = GatherChannel(uintComp(GetSrc(image.component), 0));
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
      DebugAPIWrapper::TextureType texType = debugger.GetTextureType(img);

      // should not be sampling or fetching from subpass textures
      RDCASSERT((texType & DebugAPIWrapper::Subpass_Texture) == 0);

      ShaderVariable result;

      result.type = resultType.scalar().Type();

      ShaderBindIndex samplerIndex;
      if(sampler.type == VarType::Sampler || sampler.type == VarType::ReadOnlyResource)
        samplerIndex = sampler.GetBindIndex();

      if(!debugger.GetAPIWrapper()->CalculateSampleGather(
             *this, opdata.op, texType, img.GetBindIndex(), samplerIndex, uv, ddxCalc, ddyCalc,
             compare, gather, operands, result))
      {
        // sample failed. Pretend we got 0 columns back
        set0001(result);
      }

      result.rows = 1;
      result.columns = RDCMAX(1U, resultType.vector().count) & 0xff;

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
      result.rows = 1;
      result.columns = RDCMAX(1U, resultType.vector().count) & 0xff;

      DebugAPIWrapper::TextureType texType = debugger.GetTextureType(img);

      if(texType & DebugAPIWrapper::Subpass_Texture)
      {
        // get current position
        ShaderVariable curCoord(rdcstr(), 0.0f, 0.0f, 0.0f, 0.0f);
        debugger.GetAPIWrapper()->FillInputValue(curCoord, ShaderBuiltin::Position, 0, 0);

        // co-ords are relative to the current position
        setUintComp(coord, 0, uintComp(coord, 0) + (uint32_t)floatComp(curCoord, 0));
        setUintComp(coord, 1, uintComp(coord, 1) + (uint32_t)floatComp(curCoord, 1));

        // do it with samplegather as ImageFetch rather than a Read which caches the whole texture
        // on the CPU for no reason (since we can't write to it)

        if(!debugger.GetAPIWrapper()->CalculateSampleGather(
               *this, Op::ImageFetch, texType, img.GetBindIndex(), ShaderBindIndex(), coord,
               ShaderVariable(), ShaderVariable(), ShaderVariable(), GatherChannel::Red,
               ImageOperandsAndParamDatas(), result))
        {
          // sample failed. Pretend we got 0 columns back
          set0001(result);
        }
      }
      else
      {
        if(!debugger.GetAPIWrapper()->ReadTexel(img.GetBindIndex(), coord,
                                                read.imageOperands.flags & ImageOperands::Sample
                                                    ? uintComp(GetSrc(read.imageOperands.sample), 0)
                                                    : 0,
                                                result))
        {
          // sample failed. Pretend we got 0 columns back
          set0001(result);
        }
      }

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

      debugger.GetAPIWrapper()->WriteTexel(img.GetBindIndex(), coord,
                                           write.imageOperands.flags & ImageOperands::Sample
                                               ? uintComp(GetSrc(write.imageOperands.sample), 0)
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
      OpSwitch32 switch32(it);
      // selector and default are common beteen 32-bit and 64-bit versions of OpSwitch
      Id selectorId = switch32.selector;
      Id targetLabel = switch32.def;

      ShaderVariable selector = GetSrc(selectorId);
      bool longLiterals = ((selector.type == VarType::SLong) || (selector.type == VarType::ULong));
      if(!longLiterals)
      {
        const uint32_t selectorVal = uintComp(selector, 0);
        for(size_t i = 0; i < switch32.targets.size(); ++i)
        {
          SwitchPairU32LiteralId target = switch32.targets[i];
          if(selectorVal == target.literal)
          {
            targetLabel = target.target;
            break;
          }
        }
      }
      else
      {
        OpSwitch64 switch64(it);
        const uint64_t selectorVal = selector.value.u64v[0];
        for(size_t i = 0; i < switch64.targets.size(); ++i)
        {
          SwitchPairU64LiteralId target = switch64.targets[i];
          if(selectorVal == target.literal)
          {
            targetLabel = target.target;
            break;
          }
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
      if(uintComp(GetSrc(branch.condition), 0))
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
      result.columns = RDCMAX(1U, resultType.vector().count) & 0xff;

      // whatever the type is, we just write the full 64-bit value. If it's a 64-bit integer it gets
      // it natively, or if it's a 2-vector of uint32_t then it gets the lsb/msb automatically from
      // the union.
      result.value.u64v[0] = global.clock;

      SetDst(opdata.result, result);
      break;
    }
    case Op::IsHelperInvocationEXT:
    {
      ShaderVariable result;

      result.type = VarType::Bool;
      result.rows = 1;
      result.columns = 1;

      setUintComp(result, 0, helperInvocation ? 1 : 0);

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

    case Op::Unreachable:
      RDCERR("Op::Unreachable reached, terminating debugging!");
      DELIBERATE_FALLTHROUGH();
    case Op::TerminateInvocation:
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
      }

      for(Id id : exitingFrame->idsCreated)
        ids[id] = ShaderVariable();

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
      result.type = VarType::Struct;
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

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                                uintComp(ptr.members[2], 0), result))
        {
          // sample failed. Pretend we got 0 columns back
          RDCEraseEl(result.value);
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
        debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                             uintComp(ptr.members[2], 0), value);
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

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                                uintComp(ptr.members[2], 0), result))
        {
          // sample failed. Pretend we got 0 columns back
          RDCEraseEl(result.value);
        }

        debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                             uintComp(ptr.members[2], 0), value);
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
      const ShaderVariable &comparator = GetSrc(cmpexcg.comparator);

      if(ptr.members.empty())
      {
        result = ReadPointerValue(cmpexcg.pointer);
      }
      else
      {
        const DataType &resultType = debugger.GetType(opdata.resultType);

        result.rows = result.columns = 1;
        result.type = resultType.scalar().Type();

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                                uintComp(ptr.members[2], 0), result))
        {
          // sample failed. Pretend we got 0 columns back
          RDCEraseEl(result.value);
        }
      }

      SetDst(cmpexcg.result, result);

      uint64_t resultVal = 0, compareVal = 0;

#undef _IMPL
#define _IMPL(I, S, U) resultVal = comp<U>(result, 0);

      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);

#undef _IMPL
#define _IMPL(I, S, U) compareVal = comp<U>(comparator, 0);

      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, comparator.type);

      // write the new value, only if the value is the same as expected.
      if(resultVal == compareVal)
      {
        if(ptr.members.empty())
        {
          WritePointerValue(cmpexcg.pointer, value);
        }
        else
        {
          debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                               uintComp(ptr.members[2], 0), value);
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

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                                uintComp(ptr.members[2], 0), result))
        {
          // sample failed. Pretend we got 0 columns back
          RDCEraseEl(result.value);
        }
      }

      SetDst(atomic.result, result);

      {
#undef _IMPL
#define _IMPL(I, S, U)                  \
  if(opdata.op == Op::AtomicIIncrement) \
    comp<I>(result, 0)++;               \
  else                                  \
    comp<I>(result, 0)--;

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }

      // write the new value
      if(ptr.members.empty())
      {
        WritePointerValue(atomic.pointer, result);
      }
      else
      {
        debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                             uintComp(ptr.members[2], 0), result);
      }
      break;
    }
    case Op::AtomicFAddEXT:
    case Op::AtomicFMinEXT:
    case Op::AtomicFMaxEXT:
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

        if(!debugger.GetAPIWrapper()->ReadTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                                uintComp(ptr.members[2], 0), result))
        {
          // sample failed. Pretend we got 0 columns back
          RDCEraseEl(result.value);
        }
      }

      SetDst(atomic.result, result);

      if(opdata.op == Op::AtomicIAdd)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, 0) += comp<I>(value, 0)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicISub)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, 0) -= comp<I>(value, 0)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicSMin)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, 0) = RDCMIN(comp<S>(result, 0), comp<S>(value, 0))

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicUMin)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, 0) = RDCMIN(comp<U>(result, 0), comp<U>(value, 0))

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicSMax)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, 0) = RDCMAX(comp<S>(result, 0), comp<S>(value, 0))

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicUMax)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, 0) = RDCMAX(comp<U>(result, 0), comp<U>(value, 0))

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicAnd)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, 0) &= comp<U>(value, 0)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicOr)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, 0) |= comp<U>(value, 0)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicXor)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, 0) ^= comp<U>(value, 0)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicFAddEXT)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(result, 0) += comp<T>(value, 0)
        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicFMaxEXT)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(result, 0) += RDCMAX(comp<T>(result, 0), comp<T>(value, 0))
        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, value.type);
      }
      else if(opdata.op == Op::AtomicFMinEXT)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(result, 0) += RDCMIN(comp<T>(result, 0), comp<T>(value, 0))
        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, value.type);
      }

      // write the new value
      if(ptr.members.empty())
      {
        WritePointerValue(atomic.pointer, result);
      }
      else
      {
        debugger.GetAPIWrapper()->WriteTexel(ptr.members[0].GetBindIndex(), ptr.members[1],
                                             uintComp(ptr.members[2], 0), result);
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
    case Op::SDotKHR:
    case Op::UDotKHR:
    case Op::SUDotKHR:
    case Op::SDotAccSatKHR:
    case Op::UDotAccSatKHR:
    case Op::SUDotAccSatKHR:

    case Op::GroupIMulKHR:
    case Op::GroupFMulKHR:
    case Op::GroupBitwiseAndKHR:
    case Op::GroupBitwiseOrKHR:
    case Op::GroupBitwiseXorKHR:
    case Op::GroupLogicalAndKHR:
    case Op::GroupLogicalOrKHR:
    case Op::GroupLogicalXorKHR:

    case Op::GroupNonUniformRotateKHR:
    {
      RDCERR("Group opcodes not supported. SPIR-V should have been rejected by capability!");

      ShaderVariable var("", 0U, 0U, 0U, 0U);
      var.columns = 1;

      SetDst(opdata.result, var);

      break;
    }

    case Op::PtrDiff:
    {
      RDCERR(
          "Variable pointers are not supported, PtrDiff must only be used with variable pointers, "
          "not physical pointers");

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

    case Op::AssumeTrueKHR:
    case Op::ExpectKHR:
    {
      // we can ignore these, they are optimisation hints
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
    case Op::ReportIntersectionKHR:
    case Op::IgnoreIntersectionNV:
    case Op::TerminateRayNV:
    case Op::TraceNV:
    case Op::TypeAccelerationStructureKHR:
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
    case Op::LoopControlINTEL:
    case Op::RayQueryGetRayTMinKHR:
    case Op::RayQueryGetRayFlagsKHR:
    case Op::RayQueryGetIntersectionTKHR:
    case Op::RayQueryGetIntersectionInstanceCustomIndexKHR:
    case Op::RayQueryGetIntersectionInstanceIdKHR:
    case Op::RayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetKHR:
    case Op::RayQueryGetIntersectionGeometryIndexKHR:
    case Op::RayQueryGetIntersectionPrimitiveIndexKHR:
    case Op::RayQueryGetIntersectionBarycentricsKHR:
    case Op::RayQueryGetIntersectionFrontFaceKHR:
    case Op::RayQueryGetIntersectionCandidateAABBOpaqueKHR:
    case Op::RayQueryGetIntersectionObjectRayDirectionKHR:
    case Op::RayQueryGetIntersectionObjectRayOriginKHR:
    case Op::RayQueryGetWorldRayDirectionKHR:
    case Op::RayQueryGetWorldRayOriginKHR:
    case Op::RayQueryGetIntersectionObjectToWorldKHR:
    case Op::RayQueryGetIntersectionWorldToObjectKHR:
    case Op::TypeRayQueryKHR:
    case Op::RayQueryInitializeKHR:
    case Op::RayQueryTerminateKHR:
    case Op::RayQueryGenerateIntersectionKHR:
    case Op::RayQueryConfirmIntersectionKHR:
    case Op::RayQueryProceedKHR:
    case Op::RayQueryGetIntersectionTypeKHR:
    case Op::TraceRayKHR:
    case Op::ExecuteCallableKHR:
    case Op::ConvertUToAccelerationStructureKHR:
    case Op::IgnoreIntersectionKHR:
    case Op::TerminateRayKHR:
    case Op::TraceMotionNV:
    case Op::TraceRayMotionNV:
    case Op::TypeBufferSurfaceINTEL:
    case Op::TypeStructContinuedINTEL:
    case Op::ConstantCompositeContinuedINTEL:
    case Op::SpecConstantCompositeContinuedINTEL:
    case Op::ConvertUToImageNV:
    case Op::ConvertUToSamplerNV:
    case Op::ConvertUToSampledImageNV:
    case Op::ConvertImageToUNV:
    case Op::ConvertSamplerToUNV:
    case Op::ConvertSampledImageToUNV:
    case Op::SamplerImageAddressingModeNV:
    case Op::EmitMeshTasksEXT:
    case Op::SetMeshOutputsEXT:
    case Op::HitObjectRecordHitMotionNV:
    case Op::HitObjectRecordHitWithIndexMotionNV:
    case Op::HitObjectRecordMissMotionNV:
    case Op::HitObjectGetWorldToObjectNV:
    case Op::HitObjectGetObjectToWorldNV:
    case Op::HitObjectGetObjectRayDirectionNV:
    case Op::HitObjectGetObjectRayOriginNV:
    case Op::HitObjectTraceRayMotionNV:
    case Op::HitObjectGetShaderRecordBufferHandleNV:
    case Op::HitObjectGetShaderBindingTableRecordIndexNV:
    case Op::HitObjectRecordEmptyNV:
    case Op::HitObjectTraceRayNV:
    case Op::HitObjectRecordHitNV:
    case Op::HitObjectRecordHitWithIndexNV:
    case Op::HitObjectRecordMissNV:
    case Op::HitObjectExecuteShaderNV:
    case Op::HitObjectGetCurrentTimeNV:
    case Op::HitObjectGetAttributesNV:
    case Op::HitObjectGetHitKindNV:
    case Op::HitObjectGetPrimitiveIndexNV:
    case Op::HitObjectGetGeometryIndexNV:
    case Op::HitObjectGetInstanceIdNV:
    case Op::HitObjectGetInstanceCustomIndexNV:
    case Op::HitObjectGetWorldRayDirectionNV:
    case Op::HitObjectGetWorldRayOriginNV:
    case Op::HitObjectGetRayTMaxNV:
    case Op::HitObjectGetRayTMinNV:
    case Op::HitObjectIsEmptyNV:
    case Op::HitObjectIsHitNV:
    case Op::HitObjectIsMissNV:
    case Op::ReorderThreadWithHitObjectNV:
    case Op::ReorderThreadWithHintNV:
    case Op::TypeHitObjectNV:
    case Op::ColorAttachmentReadEXT:
    case Op::DepthAttachmentReadEXT:
    case Op::StencilAttachmentReadEXT:
    case Op::ImageSampleWeightedQCOM:
    case Op::ImageBoxFilterQCOM:
    case Op::ImageBlockMatchSADQCOM:
    case Op::ImageBlockMatchSSDQCOM:
    case Op::RayQueryGetIntersectionTriangleVertexPositionsKHR:
    case Op::ConvertBF16ToFINTEL:
    case Op::ConvertFToBF16INTEL:
    case Op::TypeCooperativeMatrixKHR:
    case Op::CooperativeMatrixLoadKHR:
    case Op::CooperativeMatrixStoreKHR:
    case Op::CooperativeMatrixMulAddKHR:
    case Op::CooperativeMatrixLengthKHR:
    case Op::ImageBlockMatchWindowSSDQCOM:
    case Op::ImageBlockMatchWindowSADQCOM:
    case Op::ImageBlockMatchGatherSSDQCOM:
    case Op::ImageBlockMatchGatherSADQCOM:
    case Op::FinalizeNodePayloadsAMDX:
    case Op::FinishWritingNodePayloadAMDX:
    case Op::InitializeNodePayloadsAMDX:
    case Op::GroupNonUniformQuadAllKHR:
    case Op::GroupNonUniformQuadAnyKHR:
    case Op::FetchMicroTriangleVertexBarycentricNV:
    case Op::FetchMicroTriangleVertexPositionNV:
    case Op::CompositeConstructContinuedINTEL:
    case Op::MaskedGatherINTEL:
    case Op::MaskedScatterINTEL:
    case Op::CompositeConstructReplicateEXT:
    case Op::ConstantCompositeReplicateEXT:
    case Op::SpecConstantCompositeReplicateEXT:
    case Op::RawAccessChainNV:
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
    case Op::FPGARegINTEL:
    case Op::ReadPipeBlockingINTEL:
    case Op::WritePipeBlockingINTEL:
    case Op::ControlBarrierArriveINTEL:
    case Op::ControlBarrierWaitINTEL:
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
  while(!debugger.HasDebugInfo())
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
