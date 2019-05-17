/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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
  SERIALISE_ELEMENT(pDstBuffer);
  SERIALISE_ELEMENT(DstOffset);
  SERIALISE_ELEMENT(pSrcBuffer);
  SERIALISE_ELEMENT(SrcOffset);
  SERIALISE_ELEMENT(Dependencies);
  SERIALISE_ELEMENT_ARRAY(ppDependentResources, Dependencies);
  SERIALISE_ELEMENT_ARRAY(pDependentSubresourceRanges, Dependencies);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      RDCERR("Can't replay ID3D12GraphicsCommandList1 command");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        std::vector<ID3D12Resource *> deps;
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
      std::vector<ID3D12Resource *> deps;
      deps.resize(Dependencies);
      for(size_t i = 0; i < deps.size(); i++)
        deps[i] = Unwrap(ppDependentResources[i]);

      Unwrap1(pCommandList)
          ->AtomicCopyBufferUINT(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset,
                                 Dependencies, deps.data(), pDependentSubresourceRanges);
      GetCrackedList1()->AtomicCopyBufferUINT(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer),
                                              SrcOffset, Dependencies, deps.data(),
                                              pDependentSubresourceRanges);

      {
        m_Cmd->AddEvent();

        DrawcallDescription draw;
        draw.copySource = GetResourceManager()->GetOriginalID(GetResID(pSrcBuffer));
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pDstBuffer));

        draw.name =
            StringFormat::Fmt("AtomicCopyBufferUINT(%s, %s)", ToStr(draw.copyDestination).c_str(),
                              ToStr(draw.copySource).c_str());
        draw.flags |= DrawFlags::Copy;

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        if(pSrcBuffer == pDstBuffer)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Copy)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopySrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pDstBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopyDst)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_AtomicCopyBufferUINT);
    Serialise_AtomicCopyBufferUINT(ser, pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies,
                                   ppDependentResources, pDependentSubresourceRanges);

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(pDstBuffer);
  SERIALISE_ELEMENT(DstOffset);
  SERIALISE_ELEMENT(pSrcBuffer);
  SERIALISE_ELEMENT(SrcOffset);
  SERIALISE_ELEMENT(Dependencies);
  SERIALISE_ELEMENT_ARRAY(ppDependentResources, Dependencies);
  SERIALISE_ELEMENT_ARRAY(pDependentSubresourceRanges, Dependencies);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      RDCERR("Can't replay ID3D12GraphicsCommandList1 command");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        std::vector<ID3D12Resource *> deps;
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
      std::vector<ID3D12Resource *> deps;
      deps.resize(Dependencies);
      for(size_t i = 0; i < deps.size(); i++)
        deps[i] = Unwrap(ppDependentResources[i]);

      Unwrap1(pCommandList)
          ->AtomicCopyBufferUINT64(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset,
                                   Dependencies, deps.data(), pDependentSubresourceRanges);
      GetCrackedList1()->AtomicCopyBufferUINT64(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer),
                                                SrcOffset, Dependencies, deps.data(),
                                                pDependentSubresourceRanges);

      {
        m_Cmd->AddEvent();

        DrawcallDescription draw;
        draw.copySource = GetResourceManager()->GetOriginalID(GetResID(pSrcBuffer));
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pDstBuffer));

        draw.name =
            StringFormat::Fmt("AtomicCopyBufferUINT64(%s, %s)", ToStr(draw.copyDestination).c_str(),
                              ToStr(draw.copySource).c_str());
        draw.flags |= DrawFlags::Copy;

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        if(pSrcBuffer == pDstBuffer)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Copy)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopySrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pDstBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopyDst)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_AtomicCopyBufferUINT64);
    Serialise_AtomicCopyBufferUINT64(ser, pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies,
                                     ppDependentResources, pDependentSubresourceRanges);

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(Min);
  SERIALISE_ELEMENT(Max);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      RDCERR("Can't replay ID3D12GraphicsCommandList1 command");
      return false;
    }

    if(m_pDevice->GetOpts2().DepthBoundsTestSupported == 0)
    {
      RDCERR("Can't replay OMSetDepthBounds without device support");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap1(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->OMSetDepthBounds(Min, Max);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          m_Cmd->m_RenderState.depthBoundsMin = Min;
          m_Cmd->m_RenderState.depthBoundsMax = Max;
        }
      }
    }
    else
    {
      Unwrap1(pCommandList)->OMSetDepthBounds(Min, Max);
      GetCrackedList1()->OMSetDepthBounds(Min, Max);

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

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetSamplePositions(
    SerialiserType &ser, UINT NumSamplesPerPixel, UINT NumPixels,
    D3D12_SAMPLE_POSITION *pSamplePositions)
{
  ID3D12GraphicsCommandList1 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumSamplesPerPixel);
  SERIALISE_ELEMENT(NumPixels);
  SERIALISE_ELEMENT_ARRAY(pSamplePositions, NumSamplesPerPixel * NumPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      RDCERR("Can't replay ID3D12GraphicsCommandList1 command");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap1(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetSamplePositions(NumSamplesPerPixel, NumPixels, pSamplePositions);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          D3D12RenderState &state = m_Cmd->m_RenderState;

          std::vector<D3D12_SAMPLE_POSITION> pos(
              pSamplePositions, pSamplePositions + (NumSamplesPerPixel * NumPixels));

          state.samplePos.NumSamplesPerPixel = NumSamplesPerPixel;
          state.samplePos.NumPixels = NumPixels;
          state.samplePos.Positions.swap(pos);
        }
      }
    }
    else
    {
      Unwrap1(pCommandList)->SetSamplePositions(NumSamplesPerPixel, NumPixels, pSamplePositions);
      GetCrackedList1()->SetSamplePositions(NumSamplesPerPixel, NumPixels, pSamplePositions);

      {
        D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

        std::vector<D3D12_SAMPLE_POSITION> pos(pSamplePositions,
                                               pSamplePositions + (NumSamplesPerPixel * NumPixels));

        state.samplePos.NumSamplesPerPixel = NumSamplesPerPixel;
        state.samplePos.NumPixels = NumPixels;
        state.samplePos.Positions.swap(pos);
      }
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

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(pDstResource);
  SERIALISE_ELEMENT(DstSubresource);
  SERIALISE_ELEMENT(DstX);
  SERIALISE_ELEMENT(DstY);
  SERIALISE_ELEMENT(pSrcResource);
  SERIALISE_ELEMENT(SrcSubresource);
  SERIALISE_ELEMENT_OPT(pSrcRect);
  SERIALISE_ELEMENT(Format);
  SERIALISE_ELEMENT(ResolveMode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      RDCERR("Can't replay ID3D12GraphicsCommandList1 command");
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
      GetCrackedList1()->ResolveSubresourceRegion(Unwrap(pDstResource), DstSubresource, DstX, DstY,
                                                  Unwrap(pSrcResource), SrcSubresource, pSrcRect,
                                                  Format, ResolveMode);

      {
        m_Cmd->AddEvent();

        DrawcallDescription draw;
        draw.copySource = GetResourceManager()->GetOriginalID(GetResID(pSrcResource));
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pDstResource));

        draw.name =
            StringFormat::Fmt("ResolveSubresourceRegion(%s, %s)",
                              ToStr(draw.copyDestination).c_str(), ToStr(draw.copySource).c_str());
        draw.flags |= DrawFlags::Resolve;

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        if(pSrcResource == pDstResource)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcResource), EventUsage(drawNode.draw.eventId, ResourceUsage::Resolve)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcResource), EventUsage(drawNode.draw.eventId, ResourceUsage::ResolveSrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pDstResource), EventUsage(drawNode.draw.eventId, ResourceUsage::ResolveDst)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ResolveSubresourceRegion);
    Serialise_ResolveSubresourceRegion(ser, pDstResource, DstSubresource, DstX, DstY, pSrcResource,
                                       SrcSubresource, pSrcRect, Format, ResolveMode);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstResource), eFrameRef_PartialWrite);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcResource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetViewInstanceMask(SerialiserType &ser, UINT Mask)
{
  ID3D12GraphicsCommandList1 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(Mask);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal1() == NULL)
    {
      RDCERR("Can't replay ID3D12GraphicsCommandList1 command");
      return false;
    }

    if(m_pDevice->GetOpts3().ViewInstancingTier == D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED)
    {
      RDCERR("Can't replay SetViewInstanceMask without device support");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap1(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->SetViewInstanceMask(Mask);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
          m_Cmd->m_RenderState.viewInstMask = Mask;
      }
    }
    else
    {
      Unwrap1(pCommandList)->SetViewInstanceMask(Mask);
      GetCrackedList1()->SetViewInstanceMask(Mask);

      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.viewInstMask = Mask;
    }
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

    m_ListRecord->AddChunk(scope.Get());
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
