/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "d3d12_command_list.h"
#include "d3d12_debug.h"

#include "data/hlsl/hlsl_cbuffers.h"

static rdcstr ToHumanStr(const D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE &el)
{
  BEGIN_ENUM_STRINGISE(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE);
  {
    case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD: return "Discard";
    case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE: return "Preserve";
    case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR: return "Clear";
    case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS: return "None";
  }
  END_ENUM_STRINGISE();
}

static rdcstr ToHumanStr(const D3D12_RENDER_PASS_ENDING_ACCESS_TYPE &el)
{
  BEGIN_ENUM_STRINGISE(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE);
  {
    case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD: return "Discard";
    case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE: return "Preserve";
    case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE: return "Resolve";
    case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS: return "None";
  }
  END_ENUM_STRINGISE();
}

static rdcstr MakeRenderPassOpString(bool ending, UINT NumRenderTargets,
                                     const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets,
                                     const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil,
                                     D3D12_RENDER_PASS_FLAGS Flags)
{
  rdcstr opDesc = "";

  if(NumRenderTargets == 0 && pDepthStencil == NULL)
  {
    opDesc = "-";
  }
  else
  {
    bool colsame = true;

    // look through all other color attachments to see if they're identical
    for(UINT i = 1; i < NumRenderTargets; i++)
    {
      if(ending)
      {
        if(pRenderTargets[i].EndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS)
          continue;

        if(pRenderTargets[i].EndingAccess.Type != pRenderTargets[0].EndingAccess.Type)
          colsame = false;
      }
      else
      {
        if(pRenderTargets[i].BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS)
          continue;

        if(pRenderTargets[i].BeginningAccess.Type != pRenderTargets[0].BeginningAccess.Type)
          colsame = false;
      }
    }

    // handle depth only passes
    if(NumRenderTargets == 0)
    {
      opDesc = "";
    }
    else if(!colsame)
    {
      // if we have different storage for the colour, don't display
      // the full details

      opDesc = ending ? "Different end op" : "Different begin op";
    }
    else
    {
      // all colour ops are the same, print it
      opDesc = ending ? ToHumanStr(pRenderTargets[0].EndingAccess.Type)
                      : ToHumanStr(pRenderTargets[0].BeginningAccess.Type);
    }

    // do we have depth?
    if(pDepthStencil)
    {
      // could be empty if this is a depth-only pass
      if(!opDesc.empty())
        opDesc = "C=" + opDesc + ", ";

      // if there's no stencil, just print depth op
      if(pDepthStencil->StencilBeginningAccess.Type ==
             D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS &&
         pDepthStencil->StencilEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS)
      {
        opDesc += "D=" + (ending ? ToHumanStr(pDepthStencil->DepthEndingAccess.Type)
                                 : ToHumanStr(pDepthStencil->DepthBeginningAccess.Type));
      }
      else
      {
        if(ending)
        {
          // if depth and stencil have same op, print together, otherwise separately
          if(pDepthStencil->StencilEndingAccess.Type == pDepthStencil->DepthEndingAccess.Type)
            opDesc += "DS=" + ToHumanStr(pDepthStencil->DepthEndingAccess.Type);
          else
            opDesc += "D=" + ToHumanStr(pDepthStencil->DepthEndingAccess.Type) +
                      ", S=" + ToHumanStr(pDepthStencil->StencilEndingAccess.Type);
        }
        else
        {
          // if depth and stencil have same op, print together, otherwise separately
          if(pDepthStencil->StencilBeginningAccess.Type == pDepthStencil->DepthBeginningAccess.Type)
            opDesc += "DS=" + ToHumanStr(pDepthStencil->DepthBeginningAccess.Type);
          else
            opDesc += "D=" + ToHumanStr(pDepthStencil->DepthBeginningAccess.Type) +
                      ", S=" + ToHumanStr(pDepthStencil->StencilBeginningAccess.Type);
        }
      }
    }
  }

  if(ending && (Flags & D3D12_RENDER_PASS_FLAG_SUSPENDING_PASS))
    opDesc = "Suspend, " + opDesc;
  if(!ending && (Flags & D3D12_RENDER_PASS_FLAG_RESUMING_PASS))
    opDesc = "Resume, " + opDesc;

  return opDesc;
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_BeginRenderPass(
    SerialiserType &ser, UINT NumRenderTargets,
    const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets,
    const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags)
{
  ID3D12GraphicsCommandList4 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumRenderTargets).Important();
  SERIALISE_ELEMENT_ARRAY(pRenderTargets, NumRenderTargets);
  SERIALISE_ELEMENT_OPT(pDepthStencil);
  SERIALISE_ELEMENT(Flags);

  // since CPU handles are consumed in the call, we need to read out and serialise the contents
  // here.
  rdcarray<D3D12Descriptor> RTVs;
  D3D12Descriptor DSV;

  {
    if(ser.IsWriting())
    {
      for(UINT i = 0; i < NumRenderTargets; i++)
        RTVs.push_back(*GetWrapped(pRenderTargets[i].cpuDescriptor));
    }

    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately
    SERIALISE_ELEMENT(RTVs).Named("RenderTargetDescriptors"_lit);
  }

  {
    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately.
    const D3D12Descriptor *pDSV = NULL;

    if(ser.IsWriting())
      pDSV = pDepthStencil ? GetWrapped(pDepthStencil->cpuDescriptor) : NULL;

    SERIALISE_ELEMENT_OPT(pDSV).Named("DepthStencilDescriptor"_lit);

    if(pDSV)
      DSV = *pDSV;
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal4() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList4 which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    // patch the parameters so that we point into our local CPU descriptor handles that are up
    // to date
    {
      D3D12_RENDER_PASS_RENDER_TARGET_DESC *rts =
          (D3D12_RENDER_PASS_RENDER_TARGET_DESC *)pRenderTargets;
      D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *ds =
          (D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *)pDepthStencil;

      for(UINT i = 0; i < NumRenderTargets; i++)
        rts[i].cpuDescriptor = Unwrap(m_pDevice->GetDebugManager()->GetTempDescriptor(RTVs[i], i));

      if(ds)
        ds->cpuDescriptor = Unwrap(m_pDevice->GetDebugManager()->GetTempDescriptor(DSV));
    }

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        // perform any clears needed

        if((Flags & D3D12_RENDER_PASS_FLAG_RESUMING_PASS) == 0)
        {
          for(UINT i = 0; i < NumRenderTargets; i++)
          {
            if(pRenderTargets[i].BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
            {
              Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
                  ->ClearRenderTargetView(pRenderTargets[i].cpuDescriptor,
                                          pRenderTargets[i].BeginningAccess.Clear.ClearValue.Color,
                                          0, NULL);
            }
          }

          if(pDepthStencil)
          {
            D3D12_CLEAR_FLAGS flags = {};

            if(pDepthStencil->DepthBeginningAccess.Type ==
               D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
              flags |= D3D12_CLEAR_FLAG_DEPTH;
            if(pDepthStencil->StencilBeginningAccess.Type ==
               D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
              flags |= D3D12_CLEAR_FLAG_STENCIL;

            if(flags != 0)
            {
              // we can safely read from either depth/stencil clear values because if the access
              // type isn't clear the corresponding flag will be unset - so whatever garbage value
              // we have isn't used.
              Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
                  ->ClearDepthStencilView(
                      pDepthStencil->cpuDescriptor, flags,
                      pDepthStencil->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth,
                      pDepthStencil->StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil,
                      0, NULL);
            }
          }
        }

        {
          D3D12_CPU_DESCRIPTOR_HANDLE rtHandles[8];
          D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};

          if(pDepthStencil)
            dsvHandle = pDepthStencil->cpuDescriptor;

          for(UINT i = 0; i < NumRenderTargets; i++)
            rtHandles[i] = pRenderTargets[i].cpuDescriptor;

          // need to unwrap here, as FromPortableHandle unwraps too.
          Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
              ->OMSetRenderTargets(NumRenderTargets, rtHandles, FALSE,
                                   dsvHandle.ptr ? &dsvHandle : NULL);
        }

        // Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->BeginRenderPass(NumRenderTargets,
        // pRenderTargets, pDepthStencil, Flags);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          m_Cmd->m_Partial[D3D12CommandData::Primary].renderPassActive = true;
        }

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      for(UINT i = 0; i < NumRenderTargets; i++)
      {
        if(pRenderTargets[i].BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
        {
          Unwrap(pCommandList)
              ->ClearRenderTargetView(pRenderTargets[i].cpuDescriptor,
                                      pRenderTargets[i].BeginningAccess.Clear.ClearValue.Color, 0,
                                      NULL);
        }
      }

      if(pDepthStencil)
      {
        D3D12_CLEAR_FLAGS flags = {};

        if(pDepthStencil->DepthBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
          flags |= D3D12_CLEAR_FLAG_DEPTH;
        if(pDepthStencil->StencilBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
          flags |= D3D12_CLEAR_FLAG_STENCIL;

        if(flags != 0)
        {
          // we can safely read from either depth/stencil clear values because if the access
          // type isn't clear the corresponding flag will be unset - so whatever garbage value
          // we have isn't used.
          Unwrap(pCommandList)
              ->ClearDepthStencilView(
                  pDepthStencil->cpuDescriptor, flags,
                  pDepthStencil->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth,
                  pDepthStencil->StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil, 0,
                  NULL);
        }
      }

      D3D12_CPU_DESCRIPTOR_HANDLE rtHandles[8];
      D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};

      if(pDepthStencil)
        dsvHandle = pDepthStencil->cpuDescriptor;

      for(UINT i = 0; i < NumRenderTargets; i++)
        rtHandles[i] = pRenderTargets[i].cpuDescriptor;

      // need to unwrap here, as FromPortableHandle unwraps too.
      Unwrap(pCommandList)
          ->OMSetRenderTargets(NumRenderTargets, rtHandles, FALSE, dsvHandle.ptr ? &dsvHandle : NULL);

      // Unwrap4(pCommandList)->BeginRenderPass(NumRenderTargets, pRenderTargets, pDepthStencil,
      // Flags);

      m_Cmd->AddEvent();

      ActionDescription action;
      action.customName = StringFormat::Fmt(
          "BeginRenderPass(%s)",
          MakeRenderPassOpString(false, NumRenderTargets, pRenderTargets, pDepthStencil, Flags).c_str());
      action.flags |= ActionFlags::BeginPass | ActionFlags::PassBoundary;

      m_Cmd->AddAction(action);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.rts = RTVs;
      state.dsv = DSV;
      state.renderpass = true;

      state.rpResolves.clear();
      for(UINT r = 0; r < NumRenderTargets; r++)
      {
        if(pRenderTargets[r].EndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
        {
          state.rpResolves.append(pRenderTargets[r].EndingAccess.Resolve.pSubresourceParameters,
                                  pRenderTargets[r].EndingAccess.Resolve.SubresourceCount);
        }
      }

      if(pDepthStencil)
      {
        if(pDepthStencil->DepthEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
        {
          state.rpResolves.append(pDepthStencil->DepthEndingAccess.Resolve.pSubresourceParameters,
                                  pDepthStencil->DepthEndingAccess.Resolve.SubresourceCount);
        }

        if(pDepthStencil->StencilEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
        {
          state.rpResolves.append(pDepthStencil->StencilEndingAccess.Resolve.pSubresourceParameters,
                                  pDepthStencil->StencilEndingAccess.Resolve.SubresourceCount);
        }
      }

      D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS *resolves =
          state.rpResolves.data();

      state.rpRTs.resize(NumRenderTargets);
      for(UINT r = 0; r < NumRenderTargets; r++)
      {
        state.rpRTs[r] = pRenderTargets[r];

        if(pRenderTargets[r].EndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
        {
          state.rpRTs[r].EndingAccess.Resolve.pSubresourceParameters = resolves;
          resolves += pRenderTargets[r].EndingAccess.Resolve.SubresourceCount;
        }
      }

      state.rpDSV = {};

      if(pDepthStencil)
      {
        state.rpDSV = *pDepthStencil;

        if(pDepthStencil->DepthEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
        {
          state.rpDSV.DepthEndingAccess.Resolve.pSubresourceParameters = resolves;
          resolves += pDepthStencil->DepthEndingAccess.Resolve.SubresourceCount;
        }

        if(pDepthStencil->StencilEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
        {
          state.rpDSV.StencilEndingAccess.Resolve.pSubresourceParameters = resolves;
          resolves += pDepthStencil->StencilEndingAccess.Resolve.SubresourceCount;
        }
      }

      state.rpFlags = Flags;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::BeginRenderPass(
    UINT NumRenderTargets, const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets,
    const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags)
{
  D3D12_RENDER_PASS_RENDER_TARGET_DESC *unwrappedRTs =
      m_pDevice->GetTempArray<D3D12_RENDER_PASS_RENDER_TARGET_DESC>(NumRenderTargets);

  for(UINT i = 0; i < NumRenderTargets; i++)
  {
    unwrappedRTs[i] = pRenderTargets[i];
    unwrappedRTs[i].cpuDescriptor = Unwrap(unwrappedRTs[i].cpuDescriptor);
    if(unwrappedRTs[i].EndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
    {
      unwrappedRTs[i].EndingAccess.Resolve.pSrcResource =
          Unwrap(unwrappedRTs[i].EndingAccess.Resolve.pSrcResource);
      unwrappedRTs[i].EndingAccess.Resolve.pDstResource =
          Unwrap(unwrappedRTs[i].EndingAccess.Resolve.pDstResource);
    }
  }

  D3D12_RENDER_PASS_DEPTH_STENCIL_DESC unwrappedDSV;
  if(pDepthStencil)
  {
    unwrappedDSV = *pDepthStencil;
    unwrappedDSV.cpuDescriptor = Unwrap(unwrappedDSV.cpuDescriptor);
    if(unwrappedDSV.DepthEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
    {
      unwrappedDSV.DepthEndingAccess.Resolve.pSrcResource =
          Unwrap(unwrappedDSV.DepthEndingAccess.Resolve.pSrcResource);
      unwrappedDSV.DepthEndingAccess.Resolve.pDstResource =
          Unwrap(unwrappedDSV.DepthEndingAccess.Resolve.pDstResource);
    }
    if(unwrappedDSV.StencilEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
    {
      unwrappedDSV.StencilEndingAccess.Resolve.pSrcResource =
          Unwrap(unwrappedDSV.StencilEndingAccess.Resolve.pSrcResource);
      unwrappedDSV.StencilEndingAccess.Resolve.pDstResource =
          Unwrap(unwrappedDSV.StencilEndingAccess.Resolve.pDstResource);
    }
  }

  SERIALISE_TIME_CALL(m_pList4->BeginRenderPass(NumRenderTargets, unwrappedRTs,
                                                pDepthStencil ? &unwrappedDSV : NULL, Flags));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_BeginRenderPass);
    Serialise_BeginRenderPass(ser, NumRenderTargets, pRenderTargets, pDepthStencil, Flags);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    for(UINT i = 0; i < NumRenderTargets; i++)
    {
      D3D12Descriptor *desc = GetWrapped(pRenderTargets[i].cpuDescriptor);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);

      if(pRenderTargets[i].EndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
      {
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pRenderTargets[i].EndingAccess.Resolve.pSrcResource), eFrameRef_PartialWrite);
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pRenderTargets[i].EndingAccess.Resolve.pDstResource), eFrameRef_PartialWrite);
      }
    }

    if(pDepthStencil)
    {
      D3D12Descriptor *desc = GetWrapped(pDepthStencil->cpuDescriptor);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);

      if(pDepthStencil->DepthEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
      {
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pDepthStencil->DepthEndingAccess.Resolve.pSrcResource), eFrameRef_PartialWrite);
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pDepthStencil->DepthEndingAccess.Resolve.pDstResource), eFrameRef_PartialWrite);
      }

      if(pDepthStencil->StencilEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
      {
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pDepthStencil->StencilEndingAccess.Resolve.pSrcResource),
            eFrameRef_PartialWrite);
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pDepthStencil->StencilEndingAccess.Resolve.pDstResource),
            eFrameRef_PartialWrite);
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_EndRenderPass(SerialiserType &ser)
{
  ID3D12GraphicsCommandList4 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList).Unimportant();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal4() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList4 which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        const D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

        // perform any resolves requested. We assume the presence of List1 to do the subregion
        // resolve
        for(size_t i = 0; i < state.rpRTs.size(); i++)
        {
          if(state.rpRTs[i].EndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
          {
            const D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_PARAMETERS &r =
                state.rpRTs[i].EndingAccess.Resolve;

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = Unwrap(r.pSrcResource);
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

            Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->ResourceBarrier(1, &barrier);

            for(UINT s = 0; s < r.SubresourceCount; s++)
            {
              const D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS &sub =
                  r.pSubresourceParameters[s];
              Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
                  ->ResolveSubresourceRegion(Unwrap(r.pDstResource), sub.DstSubresource, sub.DstX,
                                             sub.DstY, Unwrap(r.pSrcResource), sub.SrcSubresource,
                                             (D3D12_RECT *)&sub.SrcRect, r.Format, r.ResolveMode);
            }

            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

            Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->ResourceBarrier(1, &barrier);
          }
        }

        if(state.rpDSV.cpuDescriptor.ptr != 0 &&
           (state.rpDSV.DepthEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE ||
            state.rpDSV.StencilEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE))
        {
          D3D12_RESOURCE_BARRIER barrier = {};
          barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
          barrier.Transition.pResource = Unwrap(state.rpDSV.DepthEndingAccess.Resolve.pSrcResource);
          barrier.Transition.StateBefore =
              D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE;
          barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

          Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->ResourceBarrier(1, &barrier);

          if(state.rpDSV.DepthEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
          {
            const D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_PARAMETERS &r =
                state.rpDSV.DepthEndingAccess.Resolve;

            for(UINT s = 0; s < r.SubresourceCount; s++)
            {
              const D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS &sub =
                  r.pSubresourceParameters[s];
              Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
                  ->ResolveSubresourceRegion(Unwrap(r.pDstResource), sub.DstSubresource, sub.DstX,
                                             sub.DstY, Unwrap(r.pSrcResource), sub.SrcSubresource,
                                             (D3D12_RECT *)&sub.SrcRect, r.Format, r.ResolveMode);
            }
          }

          if(state.rpDSV.StencilEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
          {
            const D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_PARAMETERS &r =
                state.rpDSV.StencilEndingAccess.Resolve;

            for(UINT s = 0; s < r.SubresourceCount; s++)
            {
              const D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS &sub =
                  r.pSubresourceParameters[s];
              Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
                  ->ResolveSubresourceRegion(Unwrap(r.pDstResource), sub.DstSubresource, sub.DstX,
                                             sub.DstY, Unwrap(r.pSrcResource), sub.SrcSubresource,
                                             (D3D12_RECT *)&sub.SrcRect, r.Format, r.ResolveMode);
            }
          }

          std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

          Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->ResourceBarrier(1, &barrier);
        }

        // Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->EndRenderPass();

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          m_Cmd->m_Partial[D3D12CommandData::Primary].renderPassActive = false;
        }

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      // Unwrap4(pCommandList)->EndRenderPass();

      m_Cmd->AddEvent();

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      ActionDescription action;
      action.customName = StringFormat::Fmt(
          "EndRenderPass(%s)",
          MakeRenderPassOpString(true, (UINT)state.rpRTs.size(), state.rpRTs.data(),
                                 state.rpDSV.cpuDescriptor.ptr ? &state.rpDSV : NULL, state.rpFlags)
              .c_str());
      action.flags |= ActionFlags::EndPass | ActionFlags::PassBoundary;

      m_Cmd->AddAction(action);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.rts.clear();
      state.dsv = D3D12Descriptor();
      state.renderpass = false;
      state.rpRTs.clear();
      state.rpDSV = {};
      state.rpFlags = D3D12_RENDER_PASS_FLAG_NONE;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::EndRenderPass()
{
  SERIALISE_TIME_CALL(m_pList4->EndRenderPass());

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_EndRenderPass);
    Serialise_EndRenderPass(ser);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

void WrappedID3D12GraphicsCommandList::InitializeMetaCommand(
    _In_ ID3D12MetaCommand *pMetaCommand,
    _In_reads_bytes_opt_(InitializationParametersDataSizeInBytes)
        const void *pInitializationParametersData,
    _In_ SIZE_T InitializationParametersDataSizeInBytes)
{
  RDCERR("InitializeMetaCommand called but no meta commands reported!");
}

void WrappedID3D12GraphicsCommandList::ExecuteMetaCommand(
    _In_ ID3D12MetaCommand *pMetaCommand,
    _In_reads_bytes_opt_(ExecutionParametersDataSizeInBytes) const void *pExecutionParametersData,
    _In_ SIZE_T ExecutionParametersDataSizeInBytes)
{
  RDCERR("ExecuteMetaCommand called but no meta commands reported!");
}

bool WrappedID3D12GraphicsCommandList::ProcessASBuildAfterSubmission(ResourceId destASBId,
                                                                     D3D12BufferOffset destASBOffset,
                                                                     UINT64 byteSize)
{
  bool success = false;
  D3D12ResourceManager *resManager = m_pDevice->GetResourceManager();

  D3D12AccelerationStructure *accStructAtDestOffset = NULL;

  WrappedID3D12Resource *dstASB = resManager->GetCurrentAs<WrappedID3D12Resource>(destASBId);

  // See if acc already exist at the given offset
  bool accStructExistAtDestOffset =
      dstASB->GetAccStructIfExist(destASBOffset, &accStructAtDestOffset);

  bool createAccStruct = false;

  if(accStructExistAtDestOffset)
  {
    if(accStructAtDestOffset && accStructAtDestOffset->Size() != byteSize)
    {
      dstASB->DeleteAccStructAtOffset(destASBOffset);
      createAccStruct = true;
    }
    else
    {
      // if the AS is being rebuilt in place, that's also successful
      success = true;
    }
  }
  else
  {
    createAccStruct = true;
  }

  if(createAccStruct)
  {
    // CreateAccStruct also deletes any previous overlapping ASs on the ASB
    if(dstASB->CreateAccStruct(destASBOffset, byteSize, &accStructAtDestOffset))
    {
      success = true;
      D3D12ResourceRecord *record =
          resManager->AddResourceRecord(accStructAtDestOffset->GetResourceID());
      record->type = Resource_AccelerationStructure;
      record->Length = 0;
      accStructAtDestOffset->SetResourceRecord(record);
      resManager->MarkDirtyResource(accStructAtDestOffset->GetResourceID());

      record->AddParent(
          resManager->GetResourceRecord(accStructAtDestOffset->GetBackingBufferResourceId()));

      // register this AS so its resource can be created during replay
      m_pDevice->CreateAS(dstASB, destASBOffset, byteSize, accStructAtDestOffset);

      m_pDevice->AddForcedReference(record);
    }
    else
    {
      RDCERR("Unable to create acceleration structure");
      success = false;
    }
  }

  return success;
}

bool WrappedID3D12GraphicsCommandList::PatchAccStructBlasAddress(
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *accStructInput,
    ID3D12GraphicsCommandList4 *list, BakedCmdListInfo::PatchRaytracing *patchRaytracing)
{
  if(accStructInput->Inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL &&
     accStructInput->Inputs.NumDescs > 0)
  {
    // Here, we are uploading the old BLAS addresses, and comparing the BLAS
    // addresses in the TLAS and patching it with the corresponding new address.

    D3D12RaytracingResourceAndUtilHandler *rtHandler =
        GetResourceManager()->GetRaytracingResourceAndUtilHandler();

    // Create a resource for patched instance desc; we don't
    // need a resource of same size but of same number of instances in the TLAS with uav
    uint64_t totalInstancesSize =
        accStructInput->Inputs.NumDescs * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

    totalInstancesSize =
        AlignUp<uint64_t>(totalInstancesSize, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);

    ResourceId instanceResourceId =
        WrappedID3D12Resource::GetResIDFromAddr(accStructInput->Inputs.InstanceDescs);

    ID3D12Resource *instanceResource =
        GetResourceManager()->GetCurrentAs<WrappedID3D12Resource>(instanceResourceId)->GetReal();
    D3D12_GPU_VIRTUAL_ADDRESS instanceGpuAddress = instanceResource->GetGPUVirtualAddress();
    uint64_t instanceResOffset = accStructInput->Inputs.InstanceDescs - instanceGpuAddress;

    D3D12_RESOURCE_STATES instanceResState =
        m_pDevice->GetSubresourceStates(instanceResourceId)[0].ToStates();

    bool needInitialTransition = false;
    if(!(instanceResState & D3D12_RESOURCE_STATE_COPY_SOURCE))
    {
      needInitialTransition = true;
    }

    {
      rdcarray<D3D12_RESOURCE_BARRIER> resBarriers;

      if(needInitialTransition)
      {
        D3D12_RESOURCE_BARRIER resBarrier;
        resBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        resBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        resBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        resBarrier.Transition.pResource = instanceResource;
        resBarrier.Transition.StateBefore = instanceResState;
        resBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        resBarriers.push_back(resBarrier);
      }

      {
        D3D12_RESOURCE_BARRIER resBarrier;
        resBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        resBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        resBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        resBarrier.Transition.pResource = patchRaytracing->m_patchedInstanceBuffer->Resource();
        resBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        resBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        resBarriers.push_back(resBarrier);
      }

      list->ResourceBarrier((UINT)resBarriers.size(), resBarriers.data());
    }

    list->CopyBufferRegion(patchRaytracing->m_patchedInstanceBuffer->Resource(),
                           patchRaytracing->m_patchedInstanceBuffer->Offset(), instanceResource,
                           instanceResOffset, totalInstancesSize);

    D3D12AccStructPatchInfo patchInfo = rtHandler->GetAccStructPatchInfo();

    {
      rdcarray<D3D12_RESOURCE_BARRIER> resBarriers;
      {
        D3D12_RESOURCE_BARRIER resBarrier;
        resBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        resBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        resBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        resBarrier.Transition.pResource = patchRaytracing->m_patchedInstanceBuffer->Resource();
        resBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        resBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        resBarriers.push_back(resBarrier);
      }

      if(needInitialTransition)
      {
        D3D12_RESOURCE_BARRIER resBarrier;
        resBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        resBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        resBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        resBarrier.Transition.pResource = instanceResource;
        resBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        resBarrier.Transition.StateAfter = instanceResState;
        resBarriers.push_back(resBarrier);
      }

      list->ResourceBarrier((UINT)resBarriers.size(), resBarriers.data());
    }

    RDCCOMPILE_ASSERT(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) == sizeof(InstanceDesc),
                      "Mismatch between the hlsl, and cpp size of instance desc");

    if(!patchInfo.m_pipeline || !patchInfo.m_rootSignature)
    {
      RDCERR("Pipeline or root signature for patching the TLAS not available");
      return false;
    }

    {
      D3D12_RESOURCE_BARRIER resBarrier;
      resBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      resBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      resBarrier.UAV.pResource = patchRaytracing->m_patchedInstanceBuffer->Resource();
      list->ResourceBarrier(1, &resBarrier);
    }

    ID3D12Resource *addressPairRes = m_pDevice->GetBLASAddressBufferResource();
    D3D12_GPU_VIRTUAL_ADDRESS addressPairResAddress = addressPairRes->GetGPUVirtualAddress();

    uint64_t addressCount = m_pDevice->GetBLASAddressCount();

    list->SetPipelineState(patchInfo.m_pipeline);
    list->SetComputeRootSignature(patchInfo.m_rootSignature);
    list->SetComputeRoot32BitConstant((UINT)D3D12PatchTLASBuildParam::RootConstantBuffer,
                                      (UINT)addressCount, 0);
    list->SetComputeRootShaderResourceView((UINT)D3D12PatchTLASBuildParam::RootAddressPairSrv,
                                           addressPairResAddress);
    list->SetComputeRootUnorderedAccessView((UINT)D3D12PatchTLASBuildParam::RootPatchedAddressUav,
                                            patchRaytracing->m_patchedInstanceBuffer->Address());
    list->Dispatch(accStructInput->Inputs.NumDescs, 1, 1);

    {
      D3D12_RESOURCE_BARRIER resBarrier;
      resBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      resBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      resBarrier.UAV.pResource = patchRaytracing->m_patchedInstanceBuffer->Resource();
      list->ResourceBarrier(1, &resBarrier);
    }

    {
      D3D12_RESOURCE_BARRIER resBarrier;
      resBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      resBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      resBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      resBarrier.Transition.pResource = patchRaytracing->m_patchedInstanceBuffer->Resource();
      resBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
      resBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      list->ResourceBarrier(1, &resBarrier);
    }

    patchRaytracing->m_patched = true;

    return true;
  }

  RDCDEBUG("Not a TLAS - Invalid call");
  return true;
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_BuildRaytracingAccelerationStructure(
    SerialiserType &ser, _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *pDesc,
    _In_ UINT NumPostbuildInfoDescs,
    _In_reads_opt_(NumPostbuildInfoDescs)
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pPostbuildInfoDescs)
{
  ID3D12GraphicsCommandList4 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT_LOCAL(AccStructDesc, *pDesc)
      .TypedAs("D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC"_lit)
      .Important();
  SERIALISE_ELEMENT(NumPostbuildInfoDescs);
  SERIALISE_ELEMENT_ARRAY(pPostbuildInfoDescs, NumPostbuildInfoDescs);

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));
    BakedCmdListInfo &bakedCmdInfo = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID];
    BakedCmdListInfo::PatchRaytracing &patchInfo =
        bakedCmdInfo.m_patchRaytracingInfo[bakedCmdInfo.curEventID];

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID));

        if(AccStructDesc.Inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL &&
           AccStructDesc.Inputs.NumDescs > 0)
        {
          patchInfo.m_patched = false;
          PatchAccStructBlasAddress(&AccStructDesc, list, &patchInfo);
          if(patchInfo.m_patched)
          {
            AccStructDesc.Inputs.InstanceDescs = patchInfo.m_patchedInstanceBuffer->Address();
          }
          else
          {
            RDCERR("TLAS Buffer isn't patched");
            return false;
          }

          // Switch back to previous state
          bakedCmdInfo.state.ApplyState(m_pDevice, (ID3D12GraphicsCommandListX *)pCommandList);
        }

        list->BuildRaytracingAccelerationStructure(&AccStructDesc, NumPostbuildInfoDescs,
                                                   pPostbuildInfoDescs);
      }
    }
    else
    {
      if(AccStructDesc.Inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL &&
         AccStructDesc.Inputs.NumDescs > 0)
      {
        uint64_t totalInstancesSize =
            (uint64_t)(AccStructDesc.Inputs.NumDescs * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

        totalInstancesSize =
            AlignUp<uint64_t>(totalInstancesSize, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);

        if(GetResourceManager()->GetGPUBufferAllocator().Alloc(
               D3D12GpuBufferHeapType::DefaultHeapWithUav, D3D12GpuBufferHeapMemoryFlag::Default,
               totalInstancesSize, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT,
               &patchInfo.m_patchedInstanceBuffer))
        {
          PatchAccStructBlasAddress(&AccStructDesc, Unwrap4(pCommandList), &patchInfo);

          if(patchInfo.m_patched)
          {
            AccStructDesc.Inputs.InstanceDescs = patchInfo.m_patchedInstanceBuffer->Address();
          }

          // Switch back to previous state
          bakedCmdInfo.state.ApplyState(m_pDevice, (ID3D12GraphicsCommandListX *)pCommandList);
        }
      }

      Unwrap4(pCommandList)
          ->BuildRaytracingAccelerationStructure(&AccStructDesc, NumPostbuildInfoDescs,
                                                 pPostbuildInfoDescs);

      m_Cmd->AddEvent();

      ActionDescription actionDesc;
      actionDesc.flags |= ActionFlags::BuildAccStruct;
      m_Cmd->AddAction(actionDesc);
    }
  }

  SERIALISE_CHECK_READ_ERRORS();
  return true;
}

void WrappedID3D12GraphicsCommandList::BuildRaytracingAccelerationStructure(
    _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *pDesc,
    _In_ UINT NumPostbuildInfoDescs,
    _In_reads_opt_(NumPostbuildInfoDescs)
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pPostbuildInfoDescs)
{
  SERIALISE_TIME_CALL(m_pList4->BuildRaytracingAccelerationStructure(pDesc, NumPostbuildInfoDescs,
                                                                     pPostbuildInfoDescs));

  if(IsCaptureMode(m_State))
  {
    // Acceleration structure (AS) are created on buffer created with Acceleration structure init
    // state which helps them differentiate between non-Acceleration structure buffers (non-ASB).

    // AS creation at recording can happen at any offset, given offset + its size is less than the
    // resource size. It can also be recorded for overwriting on same or another command list,
    // invalidating occupying previous acceleration structure(s) in order of command list execution.
    // It can also be updated but there are many update constraints around it.

    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_BuildRaytracingAccelerationStructure);
      Serialise_BuildRaytracingAccelerationStructure(ser, pDesc, NumPostbuildInfoDescs,
                                                     pPostbuildInfoDescs);

      m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    }

    ResourceId asbWrappedResourceId;
    D3D12BufferOffset asbWrappedResourceBufferOffset;

    WrappedID3D12Resource::GetResIDFromAddr(pDesc->DestAccelerationStructureData,
                                            asbWrappedResourceId, asbWrappedResourceBufferOffset);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO preBldInfo;
    m_pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&pDesc->Inputs, &preBldInfo);

    UINT64 byteSize = preBldInfo.ResultDataMaxSizeInBytes;

    AddSubmissionASBuildCallback(
        false, [this, asbWrappedResourceId, asbWrappedResourceBufferOffset, byteSize]() {
          return ProcessASBuildAfterSubmission(asbWrappedResourceId, asbWrappedResourceBufferOffset,
                                               byteSize);
        });
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_EmitRaytracingAccelerationStructurePostbuildInfo(
    SerialiserType &ser,
    _In_ const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pDesc,
    _In_ UINT NumSourceAccelerationStructures,
    _In_reads_(NumSourceAccelerationStructures)
        const D3D12_GPU_VIRTUAL_ADDRESS *pSourceAccelerationStructureData)
{
  ID3D12GraphicsCommandList4 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT_LOCAL(Desc, *pDesc).Named("pDesc");
  SERIALISE_ELEMENT(NumSourceAccelerationStructures).Important();
  SERIALISE_ELEMENT_ARRAY_TYPED(D3D12BufferLocation, pSourceAccelerationStructureData,
                                NumSourceAccelerationStructures);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal4() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList4 which isn't available");
      return false;
    }

    if(m_pDevice->GetOpts5().RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ray tracing support which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->EmitRaytracingAccelerationStructurePostbuildInfo(
                pDesc, NumSourceAccelerationStructures, pSourceAccelerationStructureData);
      }
    }
    else
    {
      Unwrap4(pCommandList)
          ->EmitRaytracingAccelerationStructurePostbuildInfo(pDesc, NumSourceAccelerationStructures,
                                                             pSourceAccelerationStructureData);

      m_Cmd->AddEvent();

      ActionDescription action;
      action.copyDestination = GetResourceManager()->GetOriginalID(
          WrappedID3D12Resource::GetResIDFromAddr(pDesc->DestBuffer));
      action.copyDestinationSubresource = Subresource();

      action.flags |= ActionFlags::Copy;

      m_Cmd->AddAction(action);

      D3D12ActionTreeNode &actionNode = m_Cmd->GetActionStack().back()->children.back();

      actionNode.resourceUsage.push_back(
          make_rdcpair(WrappedID3D12Resource::GetResIDFromAddr(pDesc->DestBuffer),
                       EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
      for(UINT i = 0; i < NumSourceAccelerationStructures; i++)
        actionNode.resourceUsage.push_back(make_rdcpair(
            WrappedID3D12Resource::GetResIDFromAddr(pSourceAccelerationStructureData[i]),
            EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::EmitRaytracingAccelerationStructurePostbuildInfo(
    _In_ const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pDesc,
    _In_ UINT NumSourceAccelerationStructures,
    _In_reads_(NumSourceAccelerationStructures)
        const D3D12_GPU_VIRTUAL_ADDRESS *pSourceAccelerationStructureData)
{
  SERIALISE_TIME_CALL(m_pList4->EmitRaytracingAccelerationStructurePostbuildInfo(
      pDesc, NumSourceAccelerationStructures, pSourceAccelerationStructureData));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_EmitRaytracingAccelerationStructurePostbuildInfo);
    Serialise_EmitRaytracingAccelerationStructurePostbuildInfo(
        ser, pDesc, NumSourceAccelerationStructures, pSourceAccelerationStructureData);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(
        WrappedID3D12Resource::GetResIDFromAddr(pDesc->DestBuffer), eFrameRef_PartialWrite);
    for(UINT i = 0; i < NumSourceAccelerationStructures; i++)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pSourceAccelerationStructureData[i]),
          eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_CopyRaytracingAccelerationStructure(
    SerialiserType &ser, _In_ D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData,
    _In_ D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData,
    _In_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode)
{
  ID3D12GraphicsCommandList4 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, DestAccelerationStructureData)
      .TypedAs("D3D12_GPU_VIRTUAL_ADDRESS"_lit)
      .Important();
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, SourceAccelerationStructureData)
      .TypedAs("D3D12_GPU_VIRTUAL_ADDRESS"_lit)
      .Important();
  SERIALISE_ELEMENT(Mode);

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID));

        list->CopyRaytracingAccelerationStructure(DestAccelerationStructureData,
                                                  SourceAccelerationStructureData, Mode);
      }
    }
    else
    {
      Unwrap4(pCommandList)
          ->CopyRaytracingAccelerationStructure(DestAccelerationStructureData,
                                                SourceAccelerationStructureData, Mode);

      m_Cmd->AddEvent();

      ActionDescription actionDesc;
      actionDesc.flags |= ActionFlags::BuildAccStruct;
      m_Cmd->AddAction(actionDesc);
    }
  }

  SERIALISE_CHECK_READ_ERRORS();
  return true;
}

void WrappedID3D12GraphicsCommandList::CopyRaytracingAccelerationStructure(
    _In_ D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData,
    _In_ D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData,
    _In_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode)
{
  SERIALISE_TIME_CALL(m_pList4->CopyRaytracingAccelerationStructure(
      DestAccelerationStructureData, SourceAccelerationStructureData, Mode));

  if(IsCaptureMode(m_State))
  {
    // Acceleration structure (AS) are created on buffer created with Acceleration structure init
    // state which helps them differentiate between non-Acceleration structure buffers (non-ASB).

    // AS creation at recording can happen at any offset, given offset + its size is less than the
    // resource size. It can also be recorded for overwriting on same or another command list,
    // invalidating occupying previous acceleration structure(s) in order of command list execution.
    // It can also be updated but there are many update constraints around it.

    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_CopyRaytracingAccelerationStructure);
      Serialise_CopyRaytracingAccelerationStructure(ser, DestAccelerationStructureData,
                                                    SourceAccelerationStructureData, Mode);

      m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    }

    ResourceId destASBId;
    D3D12BufferOffset destASBOffset;

    WrappedID3D12Resource::GetResIDFromAddr(DestAccelerationStructureData, destASBId, destASBOffset);

    if(Mode == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_SERIALIZE ||
       Mode == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_VISUALIZATION_DECODE_FOR_TOOLS)
    {
      // these outputs are not ASs themselves, so we don't need to do any further tracking
    }
    else if(Mode == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_DESERIALIZE ||
            Mode == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT)
    {
      // this needs special handling because the size of the destination AS is not known in advance
      // on the CPU.
      // For serialisation the size is given by the serialisation data - it's stored in
      // the header of the serialisation data.
      // For compaction the size is determined by an EmitPostbuildInfo. We could do this earlier,
      // when the AS is first built but that would still require an asynchronous read and it could
      // be built in the same command buffer that it's compacted.
      //
      // In either case we want to late-read this size. We could grab the size directly from the
      // serialised data but instead we just do a postbuild info of the current size of the dest
      // after the copy operation for simplicity.

      D3D12GpuBuffer *sizeBuffer = NULL;
      GetResourceManager()->GetGPUBufferAllocator().Alloc(
          D3D12GpuBufferHeapType::CustomHeapWithUavCpuAccess, D3D12GpuBufferHeapMemoryFlag::Default,
          8, 8, &sizeBuffer);

      D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC desc = {};
      desc.DestBuffer = sizeBuffer->Address();
      desc.InfoType = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_CURRENT_SIZE;

      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

      // wait for build to finish
      m_pList4->ResourceBarrier(1, &barrier);

      // query current size
      m_pList4->EmitRaytracingAccelerationStructurePostbuildInfo(&desc, 1,
                                                                 &DestAccelerationStructureData);

      auto PostBldExecute = [this, destASBId, destASBOffset, sizeBuffer]() -> bool {
        UINT64 *size = (UINT64 *)sizeBuffer->Map();
        UINT64 destSize = *size;
        sizeBuffer->Unmap();
        sizeBuffer->Release();

        return ProcessASBuildAfterSubmission(destASBId, destASBOffset, destSize);
      };

      AddSubmissionASBuildCallback(true, PostBldExecute);
    }
    else
    {
      ResourceId srcASBId;
      D3D12BufferOffset srcASBOffset;

      WrappedID3D12Resource::GetResIDFromAddr(SourceAccelerationStructureData, srcASBId,
                                              srcASBOffset);

      // simple clone - easy case where size is known from the source
      // this does _not_ strictly need to be marked as pending - we could process it immediately in
      // almost every case. However if this copy's source comes from a copy itself that is not
      // directly processed then we need to defer this to wait until the source acceleration
      // structure is up to date. Deferring this should not cause a problem as we will still have it
      // up to date before any subsequent work that depends on it like beginning a capture.
      AddSubmissionASBuildCallback(true, [this, destASBId, destASBOffset, srcASBId, srcASBOffset]() {
        D3D12ResourceManager *resManager = m_pDevice->GetResourceManager();

        D3D12AccelerationStructure *accStructAtSrcOffset = NULL;

        WrappedID3D12Resource *srcASB = resManager->GetCurrentAs<WrappedID3D12Resource>(srcASBId);

        // get the source AS, we should have this and can't proceed without it to give us the size
        if(!srcASB->GetAccStructIfExist(srcASBOffset, &accStructAtSrcOffset))
        {
          RDCERR("Couldn't find source acceleration structure in AS copy");
          return false;
        }

        return ProcessASBuildAfterSubmission(destASBId, destASBOffset, accStructAtSrcOffset->Size());
      });
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetPipelineState1(SerialiserType &ser,
                                                                   _In_ ID3D12StateObject *pStateObject)
{
  ID3D12GraphicsCommandList4 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pStateObject).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal4() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList4 which isn't available");
      return false;
    }

    if(m_pDevice->GetOpts5().RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ray tracing support which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->SetPipelineState1(Unwrap(pStateObject));

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap4(pCommandList)->SetPipelineState1(Unwrap(pStateObject));

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;
      state.pipe = ResourceId();
      state.stateobj = GetResID(pStateObject);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetPipelineState1(_In_ ID3D12StateObject *pStateObject)
{
  SERIALISE_TIME_CALL(m_pList4->SetPipelineState1(Unwrap(pStateObject)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetPipelineState1);
    Serialise_SetPipelineState1(ser, pStateObject);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pStateObject), eFrameRef_Read);

    m_CaptureComputeState.stateobj = GetResID(pStateObject);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_DispatchRays(SerialiserType &ser,
                                                              _In_ const D3D12_DISPATCH_RAYS_DESC *pDesc)
{
  ID3D12GraphicsCommandList4 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT_LOCAL(Desc, *pDesc).Named("pDesc").Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal4() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList4 which isn't available");
      return false;
    }

    if(m_pDevice->GetOpts5().RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ray tracing support which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    const D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        // this call will copy the specified buffers containing shader records and patch them. We get
        // a reference to the lookup buffer used as well as a reference to the scratch buffer
        // containing the patched shader records.
        PatchedRayDispatch patchedDispatch =
            GetResourceManager()->GetRaytracingResourceAndUtilHandler()->PatchRayDispatch(
                Unwrap4(list), state.heaps, Desc);

        // restore state that would have been mutated by the patching process
        Unwrap4(list)->SetComputeRootSignature(
            Unwrap(GetResourceManager()->GetCurrentAs<ID3D12RootSignature>(state.compute.rootsig)));
        Unwrap4(list)->SetPipelineState1(
            Unwrap(GetResourceManager()->GetCurrentAs<ID3D12StateObject>(state.stateobj)));
        state.ApplyComputeRootElementsUnwrapped(Unwrap4(list));

        m_Cmd->m_RayDispatches.push_back(patchedDispatch.resources);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::DispatchRay);
        Unwrap4(list)->DispatchRays(&patchedDispatch.desc);
        if(eventId && m_Cmd->m_ActionCallback->PostDispatch(eventId, list))
        {
          Unwrap4(list)->DispatchRays(&patchedDispatch.desc);
          m_Cmd->m_ActionCallback->PostRedispatch(eventId, list);
        }
      }
    }
    else
    {
      // this call will copy the specified buffers containing shader records and patch them. We get
      // a reference to the lookup buffer used as well as a reference to the scratch buffer
      // containing the patched shader records.
      PatchedRayDispatch patchedDispatch =
          GetResourceManager()->GetRaytracingResourceAndUtilHandler()->PatchRayDispatch(
              Unwrap4(pCommandList), state.heaps, Desc);

      // restore state that would have been mutated by the patching process
      Unwrap4(pCommandList)
          ->SetComputeRootSignature(Unwrap(
              GetResourceManager()->GetCurrentAs<ID3D12RootSignature>(state.compute.rootsig)));
      Unwrap4(pCommandList)
          ->SetPipelineState1(
              Unwrap(GetResourceManager()->GetCurrentAs<ID3D12StateObject>(state.stateobj)));
      state.ApplyComputeRootElementsUnwrapped(Unwrap4(pCommandList));

      m_Cmd->m_RayDispatches.push_back(patchedDispatch.resources);

      Unwrap4(pCommandList)->DispatchRays(&patchedDispatch.desc);

      m_Cmd->AddEvent();

      ActionDescription action;
      action.dispatchDimension[0] = Desc.Width;
      action.dispatchDimension[1] = Desc.Height;
      action.dispatchDimension[2] = Desc.Depth;

      action.flags |= ActionFlags::DispatchRay;

      m_Cmd->AddAction(action);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::DispatchRays(_In_ const D3D12_DISPATCH_RAYS_DESC *pDesc)
{
  // this call will copy the specified buffers containing shader records and patch them. We get a
  // reference to the lookup buffer used as well as a reference to the scratch buffer containing the
  // patched shader records.
  PatchedRayDispatch patchedDispatch =
      GetResourceManager()->GetRaytracingResourceAndUtilHandler()->PatchRayDispatch(
          m_pList4, m_CaptureComputeState.heaps, *pDesc);

  // restore state that would have been mutated by the patching process
  m_pList4->SetComputeRootSignature(Unwrap(GetResourceManager()->GetCurrentAs<ID3D12RootSignature>(
      m_CaptureComputeState.compute.rootsig)));
  m_pList4->SetPipelineState1(
      Unwrap(GetResourceManager()->GetCurrentAs<ID3D12StateObject>(m_CaptureComputeState.stateobj)));
  m_CaptureComputeState.ApplyComputeRootElementsUnwrapped(m_pList);

  SERIALISE_TIME_CALL(m_pList4->DispatchRays(&patchedDispatch.desc));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_DispatchRays);
    Serialise_DispatchRays(ser, pDesc);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    if(pDesc->CallableShaderTable.SizeInBytes > 0)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pDesc->CallableShaderTable.StartAddress),
          eFrameRef_Read);
    if(pDesc->RayGenerationShaderRecord.SizeInBytes > 0)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pDesc->RayGenerationShaderRecord.StartAddress),
          eFrameRef_Read);
    if(pDesc->MissShaderTable.SizeInBytes > 0)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pDesc->MissShaderTable.StartAddress),
          eFrameRef_Read);
    if(pDesc->HitGroupTable.SizeInBytes > 0)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pDesc->HitGroupTable.StartAddress), eFrameRef_Read);

    // during capture track the ray dispatches so the memory can be freed dynamically. On replay we
    // free all the memory at the end of each replay
    m_RayDispatches.push_back(patchedDispatch.resources);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, BeginRenderPass,
                                UINT NumRenderTargets,
                                const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets,
                                const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil,
                                D3D12_RENDER_PASS_FLAGS Flags);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, EndRenderPass);

INSTANTIATE_FUNCTION_SERIALISED(
    void, WrappedID3D12GraphicsCommandList, BuildRaytracingAccelerationStructure,
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *pDesc, _In_ UINT NumPostbuildInfoDescs,
    _In_reads_opt_(NumPostbuildInfoDescs)
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pPostbuildInfoDescs);

INSTANTIATE_FUNCTION_SERIALISED(
    void, WrappedID3D12GraphicsCommandList, EmitRaytracingAccelerationStructurePostbuildInfo,
    _In_ const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pDesc,
    _In_ UINT NumSourceAccelerationStructures,
    _In_reads_(NumSourceAccelerationStructures)
        const D3D12_GPU_VIRTUAL_ADDRESS *pSourceAccelerationStructureData);

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList,
                                CopyRaytracingAccelerationStructure,
                                _In_ D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData,
                                _In_ D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData,
                                _In_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode);

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetPipelineState1,
                                _In_ ID3D12StateObject *pStateObject);

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, DispatchRays,
                                _In_ const D3D12_DISPATCH_RAYS_DESC *pDesc);
