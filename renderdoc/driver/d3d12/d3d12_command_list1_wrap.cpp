/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
#include "driver/dxgi/dxgi_common.h"

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_AtomicCopyBufferUINT(
    SerialiserType &ser, ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer,
    UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources,
    const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges)
{
  ID3D12GraphicsCommandList1 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pDstBuffer).Important();
  SERIALISE_ELEMENT(DstOffset).OffsetOrSize();
  SERIALISE_ELEMENT(pSrcBuffer).Important();
  SERIALISE_ELEMENT(SrcOffset).OffsetOrSize();
  SERIALISE_ELEMENT(Dependencies);
  SERIALISE_ELEMENT_ARRAY(ppDependentResources, Dependencies);
  SERIALISE_ELEMENT_ARRAY(pDependentSubresourceRanges, Dependencies);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList1 which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        rdcarray<ID3D12Resource *> deps;
        deps.resize(Dependencies);
        for(size_t i = 0; i < deps.size(); i++)
          deps[i] = Unwrap(ppDependentResources[i]);

        ID3D12GraphicsCommandList1 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);
        Unwrap1(list)->AtomicCopyBufferUINT(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer),
                                            SrcOffset, Dependencies, deps.data(),
                                            pDependentSubresourceRanges);
      }
    }
    else
    {
      rdcarray<ID3D12Resource *> deps;
      deps.resize(Dependencies);
      for(size_t i = 0; i < deps.size(); i++)
        deps[i] = Unwrap(ppDependentResources[i]);

      Unwrap1(pCommandList)
          ->AtomicCopyBufferUINT(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset,
                                 Dependencies, deps.data(), pDependentSubresourceRanges);

      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.copySource = GetResourceManager()->GetOriginalID(GetResID(pSrcBuffer));
        action.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pDstBuffer));

        action.flags |= ActionFlags::Copy;

        m_Cmd->AddAction(action);

        D3D12ActionTreeNode &actionNode = m_Cmd->GetActionStack().back()->children.back();

        if(pSrcBuffer == pDstBuffer)
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::Copy)));
        }
        else
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pDstBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::AtomicCopyBufferUINT(
    ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset,
    UINT Dependencies, ID3D12Resource *const *ppDependentResources,
    const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges)
{
  SERIALISE_TIME_CALL(m_pList1->AtomicCopyBufferUINT(
      Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset, Dependencies,
      ppDependentResources, pDependentSubresourceRanges));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_AtomicCopyBufferUINT);
    Serialise_AtomicCopyBufferUINT(ser, pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies,
                                   ppDependentResources, pDependentSubresourceRanges);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstBuffer), eFrameRef_PartialWrite);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcBuffer), eFrameRef_Read);

    for(UINT i = 0; i < Dependencies; i++)
      m_ListRecord->MarkResourceFrameReferenced(GetResID(ppDependentResources[i]), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_AtomicCopyBufferUINT64(
    SerialiserType &ser, ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer,
    UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources,
    const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges)
{
  ID3D12GraphicsCommandList1 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pDstBuffer).Important();
  SERIALISE_ELEMENT(DstOffset).OffsetOrSize();
  SERIALISE_ELEMENT(pSrcBuffer).Important();
  SERIALISE_ELEMENT(SrcOffset).OffsetOrSize();
  SERIALISE_ELEMENT(Dependencies);
  SERIALISE_ELEMENT_ARRAY(ppDependentResources, Dependencies);
  SERIALISE_ELEMENT_ARRAY(pDependentSubresourceRanges, Dependencies);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList1 which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        rdcarray<ID3D12Resource *> deps;
        deps.resize(Dependencies);
        for(size_t i = 0; i < deps.size(); i++)
          deps[i] = Unwrap(ppDependentResources[i]);

        ID3D12GraphicsCommandList1 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);
        Unwrap1(list)->AtomicCopyBufferUINT64(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer),
                                              SrcOffset, Dependencies, deps.data(),
                                              pDependentSubresourceRanges);
      }
    }
    else
    {
      rdcarray<ID3D12Resource *> deps;
      deps.resize(Dependencies);
      for(size_t i = 0; i < deps.size(); i++)
        deps[i] = Unwrap(ppDependentResources[i]);

      Unwrap1(pCommandList)
          ->AtomicCopyBufferUINT64(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset,
                                   Dependencies, deps.data(), pDependentSubresourceRanges);

      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.copySource = GetResourceManager()->GetOriginalID(GetResID(pSrcBuffer));
        action.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pDstBuffer));

        action.flags |= ActionFlags::Copy;

        m_Cmd->AddAction(action);

        D3D12ActionTreeNode &actionNode = m_Cmd->GetActionStack().back()->children.back();

        if(pSrcBuffer == pDstBuffer)
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::Copy)));
        }
        else
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pDstBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::AtomicCopyBufferUINT64(
    ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset,
    UINT Dependencies, ID3D12Resource *const *ppDependentResources,
    const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges)
{
  SERIALISE_TIME_CALL(m_pList1->AtomicCopyBufferUINT64(
      Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset, Dependencies,
      ppDependentResources, pDependentSubresourceRanges));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_AtomicCopyBufferUINT64);
    Serialise_AtomicCopyBufferUINT64(ser, pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies,
                                     ppDependentResources, pDependentSubresourceRanges);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstBuffer), eFrameRef_PartialWrite);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcBuffer), eFrameRef_Read);

    for(UINT i = 0; i < Dependencies; i++)
      m_ListRecord->MarkResourceFrameReferenced(GetResID(ppDependentResources[i]), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_OMSetDepthBounds(SerialiserType &ser, FLOAT Min,
                                                                  FLOAT Max)
{
  ID3D12GraphicsCommandList1 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(Min).Important();
  SERIALISE_ELEMENT(Max).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList1 which isn't available");
      return false;
    }

    if(m_pDevice->GetOpts2().DepthBoundsTestSupported == 0)
    {
      if(Min <= 0.0f && Max >= 1.0f)
      {
        RDCWARN(
            "Depth bounds is not supported, but skipping no-op "
            "OMSetDepthBounds(Min=%F, Max=%f)",
            Min, Max);
        return true;
      }

      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires depth bounds support which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap1(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->OMSetDepthBounds(Min, Max);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap1(pCommandList)->OMSetDepthBounds(Min, Max);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.depthBoundsMin = Min;
      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.depthBoundsMax = Max;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::OMSetDepthBounds(FLOAT Min, FLOAT Max)
{
  SERIALISE_TIME_CALL(m_pList1->OMSetDepthBounds(Min, Max));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_OMSetDepthBounds);
    Serialise_OMSetDepthBounds(ser, Min, Max);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetSamplePositions(
    SerialiserType &ser, UINT NumSamplesPerPixel, UINT NumPixels,
    D3D12_SAMPLE_POSITION *pSamplePositions)
{
  ID3D12GraphicsCommandList1 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumSamplesPerPixel).Important();
  SERIALISE_ELEMENT(NumPixels);
  SERIALISE_ELEMENT_ARRAY(pSamplePositions, NumSamplesPerPixel * NumPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList1 which isn't available");
      return false;
    }

    if(m_pDevice->GetOpts2().ProgrammableSamplePositionsTier ==
       D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED)
    {
      if(NumSamplesPerPixel == 0 || NumPixels == 0)
      {
        RDCWARN(
            "View instancing is not supported, but skipping no-op "
            "SetSamplePositions(NumSamplesPerPixel=%u, NumPixels=%u)",
            NumSamplesPerPixel, NumPixels);
        return true;
      }

      SET_ERROR_RESULT(
          m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
          "Capture requires programmable sample position support which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap1(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetSamplePositions(NumSamplesPerPixel, NumPixels, pSamplePositions);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap1(pCommandList)->SetSamplePositions(NumSamplesPerPixel, NumPixels, pSamplePositions);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.samplePos.NumSamplesPerPixel = NumSamplesPerPixel;
      state.samplePos.NumPixels = NumPixels;
      state.samplePos.Positions.assign(pSamplePositions, NumSamplesPerPixel * NumPixels);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetSamplePositions(UINT NumSamplesPerPixel, UINT NumPixels,
                                                          D3D12_SAMPLE_POSITION *pSamplePositions)
{
  SERIALISE_TIME_CALL(m_pList1->SetSamplePositions(NumSamplesPerPixel, NumPixels, pSamplePositions));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetSamplePositions);
    Serialise_SetSamplePositions(ser, NumSamplesPerPixel, NumPixels, pSamplePositions);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ResolveSubresourceRegion(
    SerialiserType &ser, ID3D12Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY,
    ID3D12Resource *pSrcResource, UINT SrcSubresource, D3D12_RECT *pSrcRect, DXGI_FORMAT Format,
    D3D12_RESOLVE_MODE ResolveMode)
{
  ID3D12GraphicsCommandList1 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pDstResource).Important();
  SERIALISE_ELEMENT(DstSubresource);
  SERIALISE_ELEMENT(DstX);
  SERIALISE_ELEMENT(DstY);
  SERIALISE_ELEMENT(pSrcResource).Important();
  SERIALISE_ELEMENT(SrcSubresource);
  SERIALISE_ELEMENT_OPT(pSrcRect);
  SERIALISE_ELEMENT(Format);
  SERIALISE_ELEMENT(ResolveMode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList1 which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList1 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);
        Unwrap1(list)->ResolveSubresourceRegion(Unwrap(pDstResource), DstSubresource, DstX, DstY,
                                                Unwrap(pSrcResource), SrcSubresource, pSrcRect,
                                                Format, ResolveMode);
      }
    }
    else
    {
      Unwrap1(pCommandList)
          ->ResolveSubresourceRegion(Unwrap(pDstResource), DstSubresource, DstX, DstY,
                                     Unwrap(pSrcResource), SrcSubresource, pSrcRect, Format,
                                     ResolveMode);

      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.copySource = GetResourceManager()->GetOriginalID(GetResID(pSrcResource));
        action.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pDstResource));

        action.flags |= ActionFlags::Resolve;

        m_Cmd->AddAction(action);

        D3D12ActionTreeNode &actionNode = m_Cmd->GetActionStack().back()->children.back();

        if(pSrcResource == pDstResource)
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcResource), EventUsage(actionNode.action.eventId, ResourceUsage::Resolve)));
        }
        else
        {
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pSrcResource),
                           EventUsage(actionNode.action.eventId, ResourceUsage::ResolveSrc)));
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pDstResource),
                           EventUsage(actionNode.action.eventId, ResourceUsage::ResolveDst)));
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ResolveSubresourceRegion(
    ID3D12Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY,
    ID3D12Resource *pSrcResource, UINT SrcSubresource, D3D12_RECT *pSrcRect, DXGI_FORMAT Format,
    D3D12_RESOLVE_MODE ResolveMode)
{
  SERIALISE_TIME_CALL(m_pList1->ResolveSubresourceRegion(Unwrap(pDstResource), DstSubresource, DstX,
                                                         DstY, Unwrap(pSrcResource), SrcSubresource,
                                                         pSrcRect, Format, ResolveMode));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ResolveSubresourceRegion);
    Serialise_ResolveSubresourceRegion(ser, pDstResource, DstSubresource, DstX, DstY, pSrcResource,
                                       SrcSubresource, pSrcRect, Format, ResolveMode);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstResource), eFrameRef_PartialWrite);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcResource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetViewInstanceMask(SerialiserType &ser, UINT Mask)
{
  ID3D12GraphicsCommandList1 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(Mask).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList1 which isn't available");
      return false;
    }

    if(m_pDevice->GetOpts3().ViewInstancingTier == D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED)
    {
      if(Mask == 0 || Mask == 1)
      {
        RDCWARN("View instancing is not supported, but skipping no-op SetViewInstanceMask(%u)", Mask);
        return true;
      }

      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires view instancing support which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap1(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->SetViewInstanceMask(Mask);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap1(pCommandList)->SetViewInstanceMask(Mask);

      stateUpdate = true;
    }

    if(stateUpdate)
      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.viewInstMask = Mask;
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetViewInstanceMask(UINT Mask)
{
  SERIALISE_TIME_CALL(m_pList1->SetViewInstanceMask(Mask));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetViewInstanceMask);
    Serialise_SetViewInstanceMask(ser, Mask);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, AtomicCopyBufferUINT,
                                ID3D12Resource *pDstBuffer, UINT64 DstOffset,
                                ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies,
                                ID3D12Resource *const *ppDependentResources,
                                const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, AtomicCopyBufferUINT64,
                                ID3D12Resource *pDstBuffer, UINT64 DstOffset,
                                ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies,
                                ID3D12Resource *const *ppDependentResources,
                                const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, OMSetDepthBounds, FLOAT Min,
                                FLOAT Max);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetSamplePositions,
                                UINT NumSamplesPerPixel, UINT NumPixels,
                                D3D12_SAMPLE_POSITION *pSamplePositions);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ResolveSubresourceRegion,
                                ID3D12Resource *pDstResource, UINT DstSubresource, UINT DstX,
                                UINT DstY, ID3D12Resource *pSrcResource, UINT SrcSubresource,
                                D3D12_RECT *pSrcRect, DXGI_FORMAT Format,
                                D3D12_RESOLVE_MODE ResolveMode);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetViewInstanceMask,
                                UINT Mask);
