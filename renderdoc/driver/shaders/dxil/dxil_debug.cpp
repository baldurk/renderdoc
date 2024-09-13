/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#pragma once

#include "dxil_debug.h"

using namespace DXIL;
using namespace DXDebug;

namespace DXILDebug
{

static void ApplyDerivatives(GlobalState &global, rdcarray<ThreadState> &quad, int input,
                             int numWords, float *data, float signmul, int32_t quadIdxA,
                             int32_t quadIdxB)
{
  for(int w = 0; w < numWords; w++)
  {
    quad[quadIdxA].m_Input.members[input].value.f32v[w] += signmul * data[w];
    if(quadIdxB >= 0)
      quad[quadIdxB].m_Input.members[input].value.f32v[w] += signmul * data[w];
  }

  // TODO: SAMPLE EVALUATE
#if 0
  // quick check to see if this register was evaluated
  if(global.sampleEvalRegisterMask & (1ULL << reg))
  {
    // apply derivative to any cached sample evaluations on these quad indices
    for(auto it = global.sampleEvalCache.begin(); it != global.sampleEvalCache.end(); ++it)
    {
      if((it->first.quadIndex == quadIdxA || it->first.quadIndex == quadIdxB) &&
         reg == it->first.inputRegisterIndex)
      {
        for(int w = 0; w < numWords; w++)
          it->second.value.f32v[w] += data[w];
      }
    }
  }
#endif
}

void ApplyAllDerivatives(GlobalState &global, rdcarray<ThreadState> &quad, int destIdx,
                         const rdcarray<PSInputData> &psInputs, float *data)
{
  // We make the assumption that the coarse derivatives are generated from (0,0) in the quad, and
  // fine derivatives are generated from the destination index and its neighbours in X and Y.
  // This isn't spec'd but we must assume something and this will hopefully get us closest to
  // reproducing actual results.
  //
  // For debugging, we need members of the quad to be able to generate coarse and fine
  // derivatives.
  //
  // For (0,0) we only need the coarse derivatives to get our neighbours (1,0) and (0,1) which
  // will give us coarse and fine derivatives being identical.
  //
  // For the others we will need to use a combination of coarse and fine derivatives to get the
  // diagonal element in the quad. In the examples below, remember that the quad indices are:
  //
  // +---+---+
  // | 0 | 1 |
  // +---+---+
  // | 2 | 3 |
  // +---+---+
  //
  // And that we have definitions of the derivatives:
  //
  // ddx_coarse = (1,0) - (0,0)
  // ddy_coarse = (0,1) - (0,0)
  //
  // i.e. the same for all members of the quad
  //
  // ddx_fine   = (x,y) - (1-x,y)
  // ddy_fine   = (x,y) - (x,1-y)
  //
  // i.e. the difference to the neighbour of our desired invocation (the one we have the actual
  // inputs for, from gathering above).
  //
  // So e.g. if our thread is at (1,1) destIdx = 3
  //
  // (1,0) = (1,1) - ddx_fine
  // (0,1) = (1,1) - ddy_fine
  // (0,0) = (1,1) - ddy_fine - ddx_coarse
  //
  // and ddy_coarse is unused. For (1,0) destIdx = 1:
  //
  // (1,1) = (1,0) + ddy_fine
  // (0,1) = (1,0) - ddx_coarse + ddy_coarse
  // (0,0) = (1,0) - ddx_coarse
  //
  // and ddx_fine is unused (it's identical to ddx_coarse anyway)

  // this is the value of input[1] - input[0]
  float *ddx_coarse = (float *)data;

  for(const PSInputData &psInput : psInputs)
  {
    const int input = psInput.input;
    const int numWords = psInput.numwords;
    if(destIdx == 0)
      ApplyDerivatives(global, quad, input, numWords, ddx_coarse, 1.0f, 1, 3);
    else if(destIdx == 1)
      ApplyDerivatives(global, quad, input, numWords, ddx_coarse, -1.0f, 0, 2);
    else if(destIdx == 2)
      ApplyDerivatives(global, quad, input, numWords, ddx_coarse, 1.0f, 1, -1);
    else if(destIdx == 3)
      ApplyDerivatives(global, quad, input, numWords, ddx_coarse, -1.0f, 0, -1);

    ddx_coarse += numWords;
  }

  // this is the value of input[2] - input[0]
  float *ddy_coarse = ddx_coarse;

  for(const PSInputData &psInput : psInputs)
  {
    const int input = psInput.input;
    const int numWords = psInput.numwords;
    if(destIdx == 0)
      ApplyDerivatives(global, quad, input, numWords, ddy_coarse, 1.0f, 2, 3);
    else if(destIdx == 1)
      ApplyDerivatives(global, quad, input, numWords, ddy_coarse, -1.0f, 2, -1);
    else if(destIdx == 2)
      ApplyDerivatives(global, quad, input, numWords, ddy_coarse, 1.0f, 0, 1);

    ddy_coarse += numWords;
  }

  float *ddxfine = ddy_coarse;

  for(const PSInputData &psInput : psInputs)
  {
    const int input = psInput.input;
    const int numWords = psInput.numwords;

    if(destIdx == 2)
      ApplyDerivatives(global, quad, input, numWords, ddxfine, 1.0f, 3, -1);
    else if(destIdx == 3)
      ApplyDerivatives(global, quad, input, numWords, ddxfine, -1.0f, 2, -1);

    ddxfine += numWords;
  }

  float *ddyfine = ddxfine;

  for(const PSInputData &psInput : psInputs)
  {
    const int input = psInput.input;
    const int numWords = psInput.numwords;

    if(destIdx == 1)
      ApplyDerivatives(global, quad, input, numWords, ddyfine, 1.0f, 3, -1);
    else if(destIdx == 3)
      ApplyDerivatives(global, quad, input, numWords, ddyfine, -1.0f, 0, 1);

    ddyfine += numWords;
  }
}

void ThreadState::InitialiseHelper(const ThreadState &activeState)
{
  m_Input = activeState.m_Input;
  m_Semantics = activeState.m_Semantics;
}

// static helper function
rdcstr Debugger::GetResourceReferenceName(const DXIL::Program *program,
                                          DXIL::ResourceClass resClass, const BindingSlot &slot)
{
  RDCASSERT(program);
  for(const ResourceReference &resRef : program->m_ResourceReferences)
  {
    if(resRef.resourceBase.resClass != resClass)
      continue;
    if(resRef.resourceBase.space != slot.registerSpace)
      continue;
    if(resRef.resourceBase.regBase > slot.shaderRegister)
      continue;
    if(resRef.resourceBase.regBase + resRef.resourceBase.regCount < slot.shaderRegister)
      continue;

    return program->GetHandleAlias(resRef.handleID);
  }
  RDCERR("Failed to find DXIL %s Resource Space %d Register %d", ToStr(resClass).c_str(),
         slot.registerSpace, slot.shaderRegister);
  return "UNKNOWN_RESOURCE_HANDLE";
}

// member functions

ShaderDebugTrace *Debugger::BeginDebug(uint32_t eventId, const DXBC::DXBCContainer *dxbcContainer,
                                       const ShaderReflection &reflection, uint32_t activeLaneIndex)
{
  ShaderStage shaderStage = reflection.stage;

  m_DXBC = dxbcContainer;
  m_EventId = eventId;
  m_ActiveLaneIndex = activeLaneIndex;

  ShaderDebugTrace *ret = new ShaderDebugTrace;
  ret->stage = shaderStage;

  return ret;
}

rdcarray<ShaderDebugState> Debugger::ContinueDebug(DebugAPIWrapper *apiWrapper)
{
  rdcarray<ShaderDebugState> ret;

  return ret;
}

};    // namespace DXILDebug
