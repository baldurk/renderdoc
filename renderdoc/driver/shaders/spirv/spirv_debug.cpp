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
      ids[param.result] = ids[arguments[arg]];
      ids[param.result].name = debugger.GetRawName(param.result);
      live.push_back(param.result);
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

    debugger.AllocateVariable(decl.result, decl.resultType, DebugVariableType::Variable, sourceName,
                              stackvar);

    if(decl.HasInitializer())
      AssignValue(stackvar, ids[decl.initializer]);

    live.removeOne(decl.result);

    ids[decl.result] = debugger.MakePointerVariable(decl.result, &stackvar);
    live.push_back(decl.result);

    it++;
    i++;
  }

  // next instruction is the first actual instruction we'll execute
  nextInstruction = debugger.GetInstructionForIter(it);
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
        // process ret;
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

          // TODO: returnValue = MakeValue(ret.value);
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
