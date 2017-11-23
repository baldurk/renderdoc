/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "d3d11_context.h"
#include "strings/string_utils.h"
#include "d3d11_renderstate.h"
#include "d3d11_resources.h"

/////////////////////////////////
// implement ID3D11DeviceContext1

extern uint32_t NullCBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
extern uint32_t NullCBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_UpdateSubresource1(
    SerialiserType &ser, ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox,
    const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags)
{
  SERIALISE_ELEMENT(pDstResource);
  SERIALISE_ELEMENT(DstSubresource);
  SERIALISE_ELEMENT_OPT(pDstBox);
  SERIALISE_ELEMENT(SrcRowPitch);
  SERIALISE_ELEMENT(SrcDepthPitch);
  SERIALISE_ELEMENT_TYPED(D3D11_COPY_FLAGS, CopyFlags);

  if(CopyFlags == ~0U)
    ser.Hidden();

  D3D11ResourceRecord *record = NULL;

  if(ser.IsWriting())
  {
    record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));

    if(record && record->NumSubResources > (int)DstSubresource)
      record = (D3D11ResourceRecord *)record->SubResources[DstSubresource];
  }

  SERIALISE_ELEMENT_LOCAL(IsUpdate,
                          bool((record && record->DataInSerialiser) || IsActiveCapturing(m_State)))
      .Hidden();

  // do we already have data allocated for this resource, or are we capturing a frame? if so, we
  // just record the updated data.
  // If not we take an alternate path to record the full subresource length into a chunk for future
  // idle updates.
  if(IsUpdate)
  {
    uint32_t SourceDataLength = 0;

    if(ser.IsWriting())
    {
      RDCASSERT(record);

      if(WrappedID3D11Buffer::IsAlloc(pDstResource))
      {
        SourceDataLength = (uint32_t)record->Length;

        if(pDstBox)
          SourceDataLength = RDCMIN(SourceDataLength, pDstBox->right - pDstBox->left);
      }
      else
      {
        WrappedID3D11Texture1D *tex1 = WrappedID3D11Texture1D::IsAlloc(pDstResource)
                                           ? (WrappedID3D11Texture1D *)pDstResource
                                           : NULL;
        WrappedID3D11Texture2D1 *tex2 = WrappedID3D11Texture2D1::IsAlloc(pDstResource)
                                            ? (WrappedID3D11Texture2D1 *)pDstResource
                                            : NULL;
        WrappedID3D11Texture3D1 *tex3 = WrappedID3D11Texture3D1::IsAlloc(pDstResource)
                                            ? (WrappedID3D11Texture3D1 *)pDstResource
                                            : NULL;

        UINT mipLevel = GetMipForSubresource(pDstResource, DstSubresource);

        if(tex1)
        {
          SourceDataLength = (uint32_t)record->Length;

          if(pDstBox)
            SourceDataLength = RDCMIN(SourceDataLength, pDstBox->right - pDstBox->left);
        }
        else if(tex2)
        {
          D3D11_TEXTURE2D_DESC desc = {0};
          tex2->GetDesc(&desc);
          UINT rows = RDCMAX(1U, desc.Height >> mipLevel);
          DXGI_FORMAT fmt = desc.Format;

          if(pDstBox)
            rows = (pDstBox->bottom - pDstBox->top);

          if(IsBlockFormat(fmt))
            rows = RDCMAX(1U, rows / 4);

          SourceDataLength = SrcRowPitch * rows;
        }
        else if(tex3)
        {
          D3D11_TEXTURE3D_DESC desc = {0};
          tex3->GetDesc(&desc);
          UINT slices = RDCMAX(1U, desc.Depth >> mipLevel);

          SourceDataLength = SrcDepthPitch * slices;

          if(pDstBox)
            SourceDataLength = SrcDepthPitch * (pDstBox->back - pDstBox->front);
        }
        else
        {
          RDCERR("UpdateSubResource on unexpected resource type");
        }
      }

      if(IsActiveCapturing(m_State))
      {
        // partial update
        if(SourceDataLength != (uint32_t)record->Length)
          MarkResourceReferenced(record->GetResourceID(), eFrameRef_Read);
        MarkResourceReferenced(record->GetResourceID(), eFrameRef_Write);
      }
    }

    SERIALISE_ELEMENT_ARRAY(pSrcData, SourceDataLength);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading() && pDstResource)
    {
      RecordUpdateStats(pDstResource, SourceDataLength, true);

      if(CopyFlags == ~0U)
      {
        // don't need to apply update subresource workaround here because we never replay on
        // deferred contexts, so the bug doesn't arise (we don't record-in the workaround).

        m_pRealContext->UpdateSubresource(
            m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, pDstBox,
            pSrcData, SrcRowPitch, SrcDepthPitch);
      }
      else
      {
        if(m_pRealContext1)
        {
          m_pRealContext1->UpdateSubresource1(
              m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource,
              pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
        }
        else if(CopyFlags == 0)
        {
          // if flags is 0 UpdateSubresource1 behaves identically to UpdateSubresource
          // according to the docs. The only case is the deferred context bug workaround
          // isn't needed, but this wasn't properly handled before, and now this ambiguity
          // is resolved by passing ~0U as the flags to indicate a 'real' call to UpdateSubresource
          m_pRealContext->UpdateSubresource(
              m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource,
              pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
        }
        else
        {
          RDCERR("Replaying a D3D11.1 context without D3D11.1 available");
          m_pDevice->AddDebugMessage(
              MessageCategory::Portability, MessageSeverity::High,
              MessageSource::UnsupportedConfiguration,
              "Replaying a call to UpdateSubresource1() without D3D11.1 available");
        }
      }
    }
  }
  else
  {
    uint64_t ContentsLength = record ? record->Length : 0;

    // need to allocate data to be able to serialise it and create the chunk
    byte *Contents = NULL;
    if(ser.IsWriting())
      Contents = new byte[(size_t)ContentsLength];

    SERIALISE_ELEMENT_ARRAY(Contents, ContentsLength);

    // the automatic deserialisation only happens when reading, when writing we need to
    // free this data
    if(ser.IsWriting())
    {
      SAFE_DELETE_ARRAY(Contents);

      if(record)
        record->SetDataOffset(ser.GetWriter()->GetOffset() - ContentsLength);
    }

    // nothing more to do here during write - the parent function will handle setting the record's
    // data pointer and either marking the resource as dirty or updating the backing store with
    // pSrcData passed in

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading() && pDstResource)
    {
      WrappedID3D11Texture1D *tex1 = WrappedID3D11Texture1D::IsAlloc(pDstResource)
                                         ? (WrappedID3D11Texture1D *)pDstResource
                                         : NULL;
      WrappedID3D11Texture2D1 *tex2 = WrappedID3D11Texture2D1::IsAlloc(pDstResource)
                                          ? (WrappedID3D11Texture2D1 *)pDstResource
                                          : NULL;
      WrappedID3D11Texture3D1 *tex3 = WrappedID3D11Texture3D1::IsAlloc(pDstResource)
                                          ? (WrappedID3D11Texture3D1 *)pDstResource
                                          : NULL;

      DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
      UINT subWidth = 1;
      UINT subHeight = 1;

      UINT mipLevel = GetMipForSubresource(pDstResource, DstSubresource);

      if(tex1)
      {
        D3D11_TEXTURE1D_DESC desc = {0};
        tex1->GetDesc(&desc);
        fmt = desc.Format;
        subWidth = RDCMAX(1U, desc.Width >> mipLevel);
      }
      else if(tex2)
      {
        D3D11_TEXTURE2D_DESC desc = {0};
        tex2->GetDesc(&desc);
        fmt = desc.Format;
        subWidth = RDCMAX(1U, desc.Width >> mipLevel);
        subHeight = RDCMAX(1U, desc.Height >> mipLevel);
      }
      else if(tex3)
      {
        D3D11_TEXTURE3D_DESC desc = {0};
        tex3->GetDesc(&desc);
        fmt = desc.Format;
        subWidth = RDCMAX(1U, desc.Width >> mipLevel);
        subHeight = RDCMAX(1U, desc.Height >> mipLevel);
      }

      UINT SourceRowPitch = GetByteSize(subWidth, 1, 1, fmt, 0);
      UINT SourceDepthPitch = GetByteSize(subWidth, subHeight, 1, fmt, 0);

      if(IsReplayingAndReading() && m_CurEventID > 0)
        RecordUpdateStats(pDstResource,
                          SourceRowPitch * subHeight + SourceDepthPitch * subWidth * subHeight, true);

      // flags set specially to indicate we're serialising UpdateSubresource not UpdateSubresource1.
      // We specify no box, since the contents contain the entire updated resource (as updated
      // idly to the record's data pointer).
      if(CopyFlags == ~0U)
      {
        m_pRealContext->UpdateSubresource(
            m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, NULL,
            Contents, SourceRowPitch, SourceDepthPitch);
      }
      else
      {
        if(m_pRealContext1)
        {
          m_pRealContext1->UpdateSubresource1(
              m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, NULL,
              Contents, SourceRowPitch, SourceDepthPitch, CopyFlags);
        }
        else if(CopyFlags == 0)
        {
          // if flags is 0 UpdateSubresource1 behaves identically to UpdateSubresource
          // according to the docs. The only case is the deferred context bug workaround
          // isn't needed, but this wasn't properly handled before, and now this ambiguity
          // is resolved by passing ~0U as the flags to indicate a 'real' call to UpdateSubresource
          m_pRealContext->UpdateSubresource(
              m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, NULL,
              Contents, SourceRowPitch, SourceDepthPitch);
        }
        else
        {
          RDCERR("Replaying a D3D11.1 context without D3D11.1 available");
          m_pDevice->AddDebugMessage(
              MessageCategory::Portability, MessageSeverity::High,
              MessageSource::UnsupportedConfiguration,
              "Replaying a call to UpdateSubresource1() without D3D11.1 available");
        }
      }
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::UpdateSubresource1(ID3D11Resource *pDstResource,
                                                    UINT DstSubresource, const D3D11_BOX *pDstBox,
                                                    const void *pSrcData, UINT SrcRowPitch,
                                                    UINT SrcDepthPitch, UINT CopyFlags)
{
  if(m_pRealContext1 == NULL)
    return;

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext1->UpdateSubresource1(
      m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, pDstBox,
      pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::UpdateSubresource1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_UpdateSubresource1(ser, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
                                 SrcDepthPitch, CopyFlags);

    m_MissingTracks.insert(GetIDForResource(pDstResource));

    m_ContextRecord->AddChunk(scope.Get());
  }
  else
  {
    // just mark dirty
    MarkDirtyResource(GetIDForResource(pDstResource));
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CopySubresourceRegion1(
    SerialiserType &ser, ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY,
    UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox,
    UINT CopyFlags)
{
  SERIALISE_ELEMENT(pDstResource);
  SERIALISE_ELEMENT(DstSubresource);
  SERIALISE_ELEMENT(DstX);
  SERIALISE_ELEMENT(DstY);
  SERIALISE_ELEMENT(DstZ);
  SERIALISE_ELEMENT(pSrcResource);
  SERIALISE_ELEMENT(SrcSubresource);
  SERIALISE_ELEMENT_OPT(pSrcBox);
  SERIALISE_ELEMENT_TYPED(D3D11_COPY_FLAGS, CopyFlags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pDstResource && pSrcResource)
  {
    if(m_pRealContext1)
    {
      m_pRealContext1->CopySubresourceRegion1(
          GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, DstX, DstY, DstZ,
          GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, pSrcBox, CopyFlags);
    }
    else if(CopyFlags == 0)
    {
      // CopyFlags == 0 just degrades to the old CopySubresourceRegion
      m_pRealContext->CopySubresourceRegion(
          GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, DstX, DstY, DstZ,
          GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, pSrcBox);
    }
    else
    {
      RDCERR("Replaying a D3D11.1 context without D3D11.1 available");
      m_pDevice->AddDebugMessage(
          MessageCategory::Portability, MessageSeverity::High,
          MessageSource::UnsupportedConfiguration,
          "Replaying a call to CopySubresourceRegion1() without D3D11.1 available");
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::CopySubresourceRegion1(ID3D11Resource *pDstResource,
                                                        UINT DstSubresource, UINT DstX, UINT DstY,
                                                        UINT DstZ, ID3D11Resource *pSrcResource,
                                                        UINT SrcSubresource,
                                                        const D3D11_BOX *pSrcBox, UINT CopyFlags)
{
  if(m_pRealContext1 == NULL)
    return;

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext1->CopySubresourceRegion1(
      GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, DstX, DstY, DstZ,
      GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, pSrcBox, CopyFlags));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CopySubresourceRegion1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_CopySubresourceRegion1(ser, pDstResource, DstSubresource, DstX, DstY, DstZ,
                                     pSrcResource, SrcSubresource, pSrcBox, CopyFlags);

    m_MissingTracks.insert(GetIDForResource(pDstResource));

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsBackgroundCapturing(m_State))
  {
    // just mark dirty
    D3D11ResourceRecord *record =
        GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
    RDCASSERT(record);

    MarkDirtyResource(GetIDForResource(pDstResource));
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_ClearView(SerialiserType &ser, ID3D11View *pView,
                                                     const FLOAT ColorRGBA[4],
                                                     const D3D11_RECT *pRect, UINT NumRects)
{
  SERIALISE_ELEMENT(pView);
  SERIALISE_ELEMENT_ARRAY(ColorRGBA, FIXED_COUNT(4));
  SERIALISE_ELEMENT_ARRAY(pRect, NumRects);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(ser.IsReading())
  {
    ResourceId resid;

    if(IsReplayMode(m_State) && pView)
    {
      ID3D11View *real = NULL;

      if(WrappedID3D11RenderTargetView1::IsAlloc(pView))
      {
        real = UNWRAP(WrappedID3D11RenderTargetView1, pView);
        resid = ((WrappedID3D11RenderTargetView1 *)pView)->GetResourceID();
      }
      else if(WrappedID3D11DepthStencilView::IsAlloc(pView))
      {
        real = UNWRAP(WrappedID3D11DepthStencilView, pView);
        resid = ((WrappedID3D11DepthStencilView *)pView)->GetResourceID();
      }
      else if(WrappedID3D11ShaderResourceView1::IsAlloc(pView))
      {
        real = UNWRAP(WrappedID3D11ShaderResourceView1, pView);
        resid = ((WrappedID3D11ShaderResourceView1 *)pView)->GetResourceID();
      }
      else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pView))
      {
        real = UNWRAP(WrappedID3D11UnorderedAccessView1, pView);
        resid = ((WrappedID3D11UnorderedAccessView1 *)pView)->GetResourceID();
      }

      RDCASSERT(real);

      m_pRealContext1->ClearView(real, ColorRGBA, pRect, NumRects);
    }

    if(IsLoading(m_State))
    {
      // add this event
      AddEvent();

      DrawcallDescription draw;

      draw.name = StringFormat::Fmt("ClearView(%f, %f, %f, %f, %u rects)", ColorRGBA[0],
                                    ColorRGBA[1], ColorRGBA[2], ColorRGBA[3], NumRects);
      draw.flags |= DrawFlags::Clear;

      if(resid != ResourceId())
      {
        m_ResourceUses[resid].push_back(EventUsage(m_CurEventID, ResourceUsage::Clear, resid));
        draw.copyDestination = GetResourceManager()->GetOriginalID(resid);
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::ClearView(ID3D11View *pView, const FLOAT Color[4],
                                           const D3D11_RECT *pRect, UINT NumRects)
{
  if(m_pRealContext1 == NULL)
    return;

  DrainAnnotationQueue();

  if(pView == NULL)
    return;

  m_EmptyCommandList = false;

  {
    ID3D11View *real = NULL;

    if(WrappedID3D11RenderTargetView1::IsAlloc(pView))
      real = UNWRAP(WrappedID3D11RenderTargetView1, pView);
    else if(WrappedID3D11DepthStencilView::IsAlloc(pView))
      real = UNWRAP(WrappedID3D11DepthStencilView, pView);
    else if(WrappedID3D11ShaderResourceView1::IsAlloc(pView))
      real = UNWRAP(WrappedID3D11ShaderResourceView1, pView);
    else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pView))
      real = UNWRAP(WrappedID3D11UnorderedAccessView1, pView);

    RDCASSERT(real);

    SERIALISE_TIME_CALL(m_pRealContext1->ClearView(real, Color, pRect, NumRects));
  }

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::ClearView);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_ClearView(ser, pView, Color, pRect, NumRects);

    ID3D11Resource *viewRes = NULL;
    pView->GetResource(&viewRes);

    m_MissingTracks.insert(GetIDForResource(viewRes));
    MarkResourceReferenced(GetIDForResource(viewRes), eFrameRef_Write);

    SAFE_RELEASE(viewRes);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsBackgroundCapturing(m_State))
  {
    ID3D11Resource *viewRes = NULL;
    pView->GetResource(&viewRes);

    MarkDirtyResource(GetIDForResource(viewRes));

    SAFE_RELEASE(viewRes);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_VSSetConstantBuffers1(
    SerialiserType &ser, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
    const UINT *pFirstConstant, const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);
  SERIALISE_ELEMENT(NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Vertex, NumBuffers, ppConstantBuffers);

    if(ppConstantBuffers)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.ConstantBuffers,
                                            ppConstantBuffers, StartSlot, NumBuffers);
    if(pFirstConstant)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBOffsets, pFirstConstant,
                                     StartSlot, NumBuffers);
    if(pNumConstants)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBCounts, pNumConstants, StartSlot,
                                     NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    for(UINT i = 0; ppConstantBuffers && i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->VSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant,
                                             pNumConstants);
    }
    else
    {
      RDCERR("Replaying a D3D11.1 context without D3D11.1 available");
      m_pDevice->AddDebugMessage(
          MessageCategory::Portability, MessageSeverity::High,
          MessageSource::UnsupportedConfiguration,
          "Replaying a call to VSSetConstantBuffers1() without D3D11.1 available");

      // if there's a missing offset there's nothing we can do, everything will be nonsense
      // after this point, but try to use the non-offset version in case the offset is 0
      // and we can safely emulate it. It's a best-effort that doesn't make things worse.
      m_pRealContext->VSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    }
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer *const *ppConstantBuffers,
                                                       const UINT *pFirstConstant,
                                                       const UINT *pNumConstants)
{
  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    return;
  }

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(IsActiveCapturing(m_State))
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  SERIALISE_TIME_CALL(m_pRealContext1->VSSetConstantBuffers1(StartSlot, NumBuffers, bufs,
                                                             pFirstConstant, pNumConstants));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::VSSetConstantBuffers1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_VSSetConstantBuffers1(ser, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
                                    pNumConstants);

    m_ContextRecord->AddChunk(scope.Get());
  }

  UINT offs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  UINT cnts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};

  if(ppConstantBuffers)
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);

  if(pFirstConstant)
  {
    memcpy(offs, pFirstConstant, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        offs[i] = NullCBOffsets[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBOffsets, offs, StartSlot, NumBuffers);
  }

  if(pNumConstants)
  {
    memcpy(cnts, pNumConstants, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        cnts[i] = NullCBCounts[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBCounts, cnts, StartSlot, NumBuffers);
  }

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_HSSetConstantBuffers1(
    SerialiserType &ser, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
    const UINT *pFirstConstant, const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);
  SERIALISE_ELEMENT(NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Hull, NumBuffers, ppConstantBuffers);

    if(ppConstantBuffers)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.ConstantBuffers,
                                            ppConstantBuffers, StartSlot, NumBuffers);
    if(pFirstConstant)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBOffsets, pFirstConstant,
                                     StartSlot, NumBuffers);
    if(pNumConstants)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBCounts, pNumConstants, StartSlot,
                                     NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    for(UINT i = 0; ppConstantBuffers && i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->HSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant,
                                             pNumConstants);
    }
    else
    {
      RDCERR("Replaying a D3D11.1 context without D3D11.1 available");
      m_pDevice->AddDebugMessage(
          MessageCategory::Portability, MessageSeverity::High,
          MessageSource::UnsupportedConfiguration,
          "Replaying a call to HSSetConstantBuffers1() without D3D11.1 available");

      // if there's a missing offset there's nothing we can do, everything will be nonsense
      // after this point, but try to use the non-offset version in case the offset is 0
      // and we can safely emulate it. It's a best-effort that doesn't make things worse.
      m_pRealContext->HSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    }
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer *const *ppConstantBuffers,
                                                       const UINT *pFirstConstant,
                                                       const UINT *pNumConstants)
{
  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    return;
  }

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(IsActiveCapturing(m_State))
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  SERIALISE_TIME_CALL(m_pRealContext1->HSSetConstantBuffers1(StartSlot, NumBuffers, bufs,
                                                             pFirstConstant, pNumConstants));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::HSSetConstantBuffers1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_HSSetConstantBuffers1(ser, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
                                    pNumConstants);

    m_ContextRecord->AddChunk(scope.Get());
  }

  UINT offs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  UINT cnts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};

  if(ppConstantBuffers)
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);

  if(pFirstConstant)
  {
    memcpy(offs, pFirstConstant, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        offs[i] = NullCBOffsets[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBOffsets, offs, StartSlot, NumBuffers);
  }

  if(pNumConstants)
  {
    memcpy(cnts, pNumConstants, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        cnts[i] = NullCBCounts[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBCounts, cnts, StartSlot, NumBuffers);
  }

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DSSetConstantBuffers1(
    SerialiserType &ser, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
    const UINT *pFirstConstant, const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);
  SERIALISE_ELEMENT(NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Domain, NumBuffers, ppConstantBuffers);

    if(ppConstantBuffers)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.ConstantBuffers,
                                            ppConstantBuffers, StartSlot, NumBuffers);
    if(pFirstConstant)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBOffsets, pFirstConstant,
                                     StartSlot, NumBuffers);
    if(pNumConstants)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBCounts, pNumConstants, StartSlot,
                                     NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    for(UINT i = 0; ppConstantBuffers && i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->DSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant,
                                             pNumConstants);
    }
    else
    {
      RDCERR("Replaying a D3D11.1 context without D3D11.1 available");
      m_pDevice->AddDebugMessage(
          MessageCategory::Portability, MessageSeverity::High,
          MessageSource::UnsupportedConfiguration,
          "Replaying a call to DSSetConstantBuffers1() without D3D11.1 available");

      // if there's a missing offset there's nothing we can do, everything will be nonsense
      // after this point, but try to use the non-offset version in case the offset is 0
      // and we can safely emulate it. It's a best-effort that doesn't make things worse.
      m_pRealContext->DSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    }
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer *const *ppConstantBuffers,
                                                       const UINT *pFirstConstant,
                                                       const UINT *pNumConstants)
{
  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    return;
  }

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(IsActiveCapturing(m_State))
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  SERIALISE_TIME_CALL(m_pRealContext1->DSSetConstantBuffers1(StartSlot, NumBuffers, bufs,
                                                             pFirstConstant, pNumConstants));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DSSetConstantBuffers1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_DSSetConstantBuffers1(ser, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
                                    pNumConstants);

    m_ContextRecord->AddChunk(scope.Get());
  }

  UINT offs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  UINT cnts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};

  if(ppConstantBuffers)
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);

  if(pFirstConstant)
  {
    memcpy(offs, pFirstConstant, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        offs[i] = NullCBOffsets[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBOffsets, offs, StartSlot, NumBuffers);
  }

  if(pNumConstants)
  {
    memcpy(cnts, pNumConstants, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        cnts[i] = NullCBCounts[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBCounts, cnts, StartSlot, NumBuffers);
  }

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_GSSetConstantBuffers1(
    SerialiserType &ser, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
    const UINT *pFirstConstant, const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);
  SERIALISE_ELEMENT(NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Geometry, NumBuffers, ppConstantBuffers);

    if(ppConstantBuffers)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.ConstantBuffers,
                                            ppConstantBuffers, StartSlot, NumBuffers);
    if(pFirstConstant)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBOffsets, pFirstConstant,
                                     StartSlot, NumBuffers);
    if(pNumConstants)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBCounts, pNumConstants, StartSlot,
                                     NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    for(UINT i = 0; ppConstantBuffers && i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->GSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant,
                                             pNumConstants);
    }
    else
    {
      RDCERR("Replaying a D3D11.1 context without D3D11.1 available");
      m_pDevice->AddDebugMessage(
          MessageCategory::Portability, MessageSeverity::High,
          MessageSource::UnsupportedConfiguration,
          "Replaying a call to GSSetConstantBuffers1() without D3D11.1 available");

      // if there's a missing offset there's nothing we can do, everything will be nonsense
      // after this point, but try to use the non-offset version in case the offset is 0
      // and we can safely emulate it. It's a best-effort that doesn't make things worse.
      m_pRealContext->GSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    }
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer *const *ppConstantBuffers,
                                                       const UINT *pFirstConstant,
                                                       const UINT *pNumConstants)
{
  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    return;
  }

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(IsActiveCapturing(m_State))
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  SERIALISE_TIME_CALL(m_pRealContext1->GSSetConstantBuffers1(StartSlot, NumBuffers, bufs,
                                                             pFirstConstant, pNumConstants));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::GSSetConstantBuffers1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_GSSetConstantBuffers1(ser, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
                                    pNumConstants);

    m_ContextRecord->AddChunk(scope.Get());
  }

  UINT offs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  UINT cnts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};

  if(ppConstantBuffers)
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);

  if(pFirstConstant)
  {
    memcpy(offs, pFirstConstant, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        offs[i] = NullCBOffsets[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBOffsets, offs, StartSlot, NumBuffers);
  }

  if(pNumConstants)
  {
    memcpy(cnts, pNumConstants, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        cnts[i] = NullCBCounts[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBCounts, cnts, StartSlot, NumBuffers);
  }

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_PSSetConstantBuffers1(
    SerialiserType &ser, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
    const UINT *pFirstConstant, const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);
  SERIALISE_ELEMENT(NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Pixel, NumBuffers, ppConstantBuffers);

    if(ppConstantBuffers)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.ConstantBuffers,
                                            ppConstantBuffers, StartSlot, NumBuffers);
    if(pFirstConstant)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBOffsets, pFirstConstant,
                                     StartSlot, NumBuffers);
    if(pNumConstants)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBCounts, pNumConstants, StartSlot,
                                     NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    for(UINT i = 0; ppConstantBuffers && i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->PSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant,
                                             pNumConstants);
    }
    else
    {
      RDCERR("Replaying a D3D11.1 context without D3D11.1 available");
      m_pDevice->AddDebugMessage(
          MessageCategory::Portability, MessageSeverity::High,
          MessageSource::UnsupportedConfiguration,
          "Replaying a call to PSSetConstantBuffers1() without D3D11.1 available");

      // if there's a missing offset there's nothing we can do, everything will be nonsense
      // after this point, but try to use the non-offset version in case the offset is 0
      // and we can safely emulate it. It's a best-effort that doesn't make things worse.
      m_pRealContext->PSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    }
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer *const *ppConstantBuffers,
                                                       const UINT *pFirstConstant,
                                                       const UINT *pNumConstants)
{
  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    return;
  }

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(IsActiveCapturing(m_State))
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  SERIALISE_TIME_CALL(m_pRealContext1->PSSetConstantBuffers1(StartSlot, NumBuffers, bufs,
                                                             pFirstConstant, pNumConstants));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::PSSetConstantBuffers1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_PSSetConstantBuffers1(ser, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
                                    pNumConstants);

    m_ContextRecord->AddChunk(scope.Get());
  }

  UINT offs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  UINT cnts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};

  if(ppConstantBuffers)
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);

  if(pFirstConstant)
  {
    memcpy(offs, pFirstConstant, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        offs[i] = NullCBOffsets[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBOffsets, offs, StartSlot, NumBuffers);
  }

  if(pNumConstants)
  {
    memcpy(cnts, pNumConstants, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        cnts[i] = NullCBCounts[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBCounts, cnts, StartSlot, NumBuffers);
  }

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CSSetConstantBuffers1(
    SerialiserType &ser, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
    const UINT *pFirstConstant, const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);
  SERIALISE_ELEMENT(NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Compute, NumBuffers, ppConstantBuffers);

    if(ppConstantBuffers)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.ConstantBuffers,
                                            ppConstantBuffers, StartSlot, NumBuffers);
    if(pFirstConstant)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBOffsets, pFirstConstant,
                                     StartSlot, NumBuffers);
    if(pNumConstants)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBCounts, pNumConstants, StartSlot,
                                     NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    for(UINT i = 0; ppConstantBuffers && i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->CSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant,
                                             pNumConstants);
    }
    else
    {
      RDCERR("Replaying a D3D11.1 context without D3D11.1 available");
      m_pDevice->AddDebugMessage(
          MessageCategory::Portability, MessageSeverity::High,
          MessageSource::UnsupportedConfiguration,
          "Replaying a call to CSSetConstantBuffers1() without D3D11.1 available");

      // if there's a missing offset there's nothing we can do, everything will be nonsense
      // after this point, but try to use the non-offset version in case the offset is 0
      // and we can safely emulate it. It's a best-effort that doesn't make things worse.
      m_pRealContext->CSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    }
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer *const *ppConstantBuffers,
                                                       const UINT *pFirstConstant,
                                                       const UINT *pNumConstants)
{
  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
    return;
  }

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(IsActiveCapturing(m_State))
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  SERIALISE_TIME_CALL(m_pRealContext1->CSSetConstantBuffers1(StartSlot, NumBuffers, bufs,
                                                             pFirstConstant, pNumConstants));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CSSetConstantBuffers1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_CSSetConstantBuffers1(ser, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
                                    pNumConstants);

    m_ContextRecord->AddChunk(scope.Get());
  }

  UINT offs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  UINT cnts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};

  if(ppConstantBuffers)
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);

  if(pFirstConstant)
  {
    memcpy(offs, pFirstConstant, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        offs[i] = NullCBOffsets[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBOffsets, offs, StartSlot, NumBuffers);
  }

  if(pNumConstants)
  {
    memcpy(cnts, pNumConstants, sizeof(UINT) * NumBuffers);
    for(UINT i = 0; i < NumBuffers; i++)
    {
      if(ppConstantBuffers && ppConstantBuffers[i] == NULL)
        cnts[i] = NullCBCounts[i];
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBCounts, cnts, StartSlot, NumBuffers);
  }

  VerifyState();
}

void WrappedID3D11DeviceContext::VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer **ppConstantBuffers,
                                                       UINT *pFirstConstant, UINT *pNumConstants)
{
  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

    for(UINT i = 0; i < NumBuffers && (pFirstConstant || pNumConstants); i++)
    {
      if(pFirstConstant)
        pFirstConstant[i] = 0;
      if(pNumConstants)
        pNumConstants[i] = 4096;
    }

    return;
  }

  ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  m_pRealContext1->VSGetConstantBuffers1(StartSlot, NumBuffers, real, pFirstConstant, pNumConstants);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->VS.ConstantBuffers[i + StartSlot]);
    }

    if(pFirstConstant)
      RDCASSERT(pFirstConstant[i] == m_CurrentPipelineState->VS.CBOffsets[i + StartSlot]);

    if(pNumConstants)
      RDCASSERT(pNumConstants[i] == m_CurrentPipelineState->VS.CBCounts[i + StartSlot]);
  }
}

void WrappedID3D11DeviceContext::HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer **ppConstantBuffers,
                                                       UINT *pFirstConstant, UINT *pNumConstants)
{
  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

    for(UINT i = 0; i < NumBuffers && (pFirstConstant || pNumConstants); i++)
    {
      if(pFirstConstant)
        pFirstConstant[i] = 0;
      if(pNumConstants)
        pNumConstants[i] = 4096;
    }

    return;
  }

  ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  m_pRealContext1->HSGetConstantBuffers1(StartSlot, NumBuffers, real, pFirstConstant, pNumConstants);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->HS.ConstantBuffers[i + StartSlot]);
    }

    if(pFirstConstant)
      RDCASSERT(pFirstConstant[i] == m_CurrentPipelineState->HS.CBOffsets[i + StartSlot]);

    if(pNumConstants)
      RDCASSERT(pNumConstants[i] == m_CurrentPipelineState->HS.CBCounts[i + StartSlot]);
  }
}

void WrappedID3D11DeviceContext::DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer **ppConstantBuffers,
                                                       UINT *pFirstConstant, UINT *pNumConstants)
{
  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

    for(UINT i = 0; i < NumBuffers && (pFirstConstant || pNumConstants); i++)
    {
      if(pFirstConstant)
        pFirstConstant[i] = 0;
      if(pNumConstants)
        pNumConstants[i] = 4096;
    }

    return;
  }

  ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  m_pRealContext1->DSGetConstantBuffers1(StartSlot, NumBuffers, real, pFirstConstant, pNumConstants);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->DS.ConstantBuffers[i + StartSlot]);
    }

    if(pFirstConstant)
      RDCASSERT(pFirstConstant[i] == m_CurrentPipelineState->DS.CBOffsets[i + StartSlot]);

    if(pNumConstants)
      RDCASSERT(pNumConstants[i] == m_CurrentPipelineState->DS.CBCounts[i + StartSlot]);
  }
}

void WrappedID3D11DeviceContext::GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer **ppConstantBuffers,
                                                       UINT *pFirstConstant, UINT *pNumConstants)
{
  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

    for(UINT i = 0; i < NumBuffers && (pFirstConstant || pNumConstants); i++)
    {
      if(pFirstConstant)
        pFirstConstant[i] = 0;
      if(pNumConstants)
        pNumConstants[i] = 4096;
    }

    return;
  }

  ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  m_pRealContext1->GSGetConstantBuffers1(StartSlot, NumBuffers, real, pFirstConstant, pNumConstants);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->GS.ConstantBuffers[i + StartSlot]);
    }

    if(pFirstConstant)
      RDCASSERT(pFirstConstant[i] == m_CurrentPipelineState->GS.CBOffsets[i + StartSlot]);

    if(pNumConstants)
      RDCASSERT(pNumConstants[i] == m_CurrentPipelineState->GS.CBCounts[i + StartSlot]);
  }
}

void WrappedID3D11DeviceContext::PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer **ppConstantBuffers,
                                                       UINT *pFirstConstant, UINT *pNumConstants)
{
  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

    for(UINT i = 0; i < NumBuffers && (pFirstConstant || pNumConstants); i++)
    {
      if(pFirstConstant)
        pFirstConstant[i] = 0;
      if(pNumConstants)
        pNumConstants[i] = 4096;
    }

    return;
  }

  ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  m_pRealContext1->PSGetConstantBuffers1(StartSlot, NumBuffers, real, pFirstConstant, pNumConstants);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->PS.ConstantBuffers[i + StartSlot]);
    }

    if(pFirstConstant)
      RDCASSERT(pFirstConstant[i] == m_CurrentPipelineState->PS.CBOffsets[i + StartSlot]);

    if(pNumConstants)
      RDCASSERT(pNumConstants[i] == m_CurrentPipelineState->PS.CBCounts[i + StartSlot]);
  }
}

void WrappedID3D11DeviceContext::CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                       ID3D11Buffer **ppConstantBuffers,
                                                       UINT *pFirstConstant, UINT *pNumConstants)
{
  if(m_pRealContext1 == NULL || !m_SetCBuffer1)
  {
    CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

    for(UINT i = 0; i < NumBuffers && (pFirstConstant || pNumConstants); i++)
    {
      if(pFirstConstant)
        pFirstConstant[i] = 0;
      if(pNumConstants)
        pNumConstants[i] = 4096;
    }

    return;
  }

  ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  m_pRealContext1->CSGetConstantBuffers1(StartSlot, NumBuffers, real, pFirstConstant, pNumConstants);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->CS.ConstantBuffers[i + StartSlot]);
    }

    if(pFirstConstant)
      RDCASSERT(pFirstConstant[i] == m_CurrentPipelineState->CS.CBOffsets[i + StartSlot]);

    if(pNumConstants)
      RDCASSERT(pNumConstants[i] == m_CurrentPipelineState->CS.CBCounts[i + StartSlot]);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DiscardResource(SerialiserType &ser,
                                                           ID3D11Resource *pResource)
{
  SERIALISE_ELEMENT(pResource);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pResource)
    {
      // don't replay the discard, as it effectively does nothing meaningful but hint
      // to the driver that the contents can be discarded.
      // Instead we should overwrite the contents with something (during capture too)
      // to indicate that the discard has happened visually like a clear.
      // This also means we don't have to require/diverge if a 11.1 context is not
      // available on replay.
    }

    if(IsLoading(m_State))
    {
      ResourceId dstLiveID = GetIDForResource(pResource);
      ResourceId dstOrigID = GetResourceManager()->GetOriginalID(dstLiveID);

      AddEvent();

      DrawcallDescription draw;

      draw.name = "DiscardResource()";
      draw.flags |= DrawFlags::Clear;
      draw.copyDestination = dstOrigID;

      AddDrawcall(draw, true);

      if(pResource)
        m_ResourceUses[dstLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::Clear));
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DiscardResource(ID3D11Resource *pResource)
{
  if(m_pRealContext1 == NULL)
    return;

  DrainAnnotationQueue();

  if(pResource == NULL)
    return;

  m_EmptyCommandList = false;

  {
    ID3D11Resource *real = NULL;

    if(WrappedID3D11Buffer::IsAlloc(pResource))
      real = UNWRAP(WrappedID3D11Buffer, pResource);
    else if(WrappedID3D11Texture1D::IsAlloc(pResource))
      real = UNWRAP(WrappedID3D11Texture1D, pResource);
    else if(WrappedID3D11Texture2D1::IsAlloc(pResource))
      real = UNWRAP(WrappedID3D11Texture2D1, pResource);
    else if(WrappedID3D11Texture3D1::IsAlloc(pResource))
      real = UNWRAP(WrappedID3D11Texture3D1, pResource);

    RDCASSERT(real);

    SERIALISE_TIME_CALL(m_pRealContext1->DiscardResource(real));
  }

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DiscardResource);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_DiscardResource(ser, pResource);

    m_MissingTracks.insert(GetIDForResource(pResource));
    MarkResourceReferenced(GetIDForResource(pResource), eFrameRef_Write);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsCaptureMode(m_State))
  {
    MarkDirtyResource(GetIDForResource(pResource));
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DiscardView(SerialiserType &ser, ID3D11View *pResourceView)
{
  SERIALISE_ELEMENT(pResourceView);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pResourceView)
    {
      // don't replay the discard, as it effectively does nothing meaningful but hint
      // to the driver that the contents can be discarded.
      // Instead we should overwrite the contents with something (during capture too)
      // to indicate that the discard has happened visually like a clear.
      // This also means we don't have to require/diverge if a 11.1 context is not
      // available on replay.
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;

      draw.name = "DiscardView()";

      draw.flags |= DrawFlags::Clear;

      if(pResourceView)
      {
        if(WrappedID3D11RenderTargetView1::IsAlloc(pResourceView))
        {
          WrappedID3D11RenderTargetView1 *view = (WrappedID3D11RenderTargetView1 *)pResourceView;
          m_ResourceUses[view->GetResourceResID()].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
          draw.copyDestination =
              m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
        }
        else if(WrappedID3D11DepthStencilView::IsAlloc(pResourceView))
        {
          WrappedID3D11DepthStencilView *view = (WrappedID3D11DepthStencilView *)pResourceView;
          m_ResourceUses[view->GetResourceResID()].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
          draw.copyDestination =
              m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
        }
        else if(WrappedID3D11ShaderResourceView1::IsAlloc(pResourceView))
        {
          WrappedID3D11ShaderResourceView1 *view = (WrappedID3D11ShaderResourceView1 *)pResourceView;
          m_ResourceUses[view->GetResourceResID()].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
          draw.copyDestination =
              m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
        }
        else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pResourceView))
        {
          WrappedID3D11UnorderedAccessView1 *view =
              (WrappedID3D11UnorderedAccessView1 *)pResourceView;
          m_ResourceUses[view->GetResourceResID()].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
          draw.copyDestination =
              m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
        }
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DiscardView(ID3D11View *pResourceView)
{
  if(m_pRealContext1 == NULL)
    return;

  DrainAnnotationQueue();

  if(pResourceView == NULL)
    return;

  m_EmptyCommandList = false;

  {
    ID3D11View *real = NULL;

    if(WrappedID3D11RenderTargetView1::IsAlloc(pResourceView))
      real = UNWRAP(WrappedID3D11RenderTargetView1, pResourceView);
    else if(WrappedID3D11DepthStencilView::IsAlloc(pResourceView))
      real = UNWRAP(WrappedID3D11DepthStencilView, pResourceView);
    else if(WrappedID3D11ShaderResourceView1::IsAlloc(pResourceView))
      real = UNWRAP(WrappedID3D11ShaderResourceView1, pResourceView);
    else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pResourceView))
      real = UNWRAP(WrappedID3D11UnorderedAccessView1, pResourceView);

    RDCASSERT(real);

    SERIALISE_TIME_CALL(m_pRealContext1->DiscardView(real));
  }

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DiscardView);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_DiscardView(ser, pResourceView);

    ID3D11Resource *viewRes = NULL;
    pResourceView->GetResource(&viewRes);

    m_MissingTracks.insert(GetIDForResource(viewRes));
    MarkResourceReferenced(GetIDForResource(viewRes), eFrameRef_Write);

    SAFE_RELEASE(viewRes);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsCaptureMode(m_State))
  {
    ID3D11Resource *viewRes = NULL;
    pResourceView->GetResource(&viewRes);

    MarkDirtyResource(GetIDForResource(viewRes));

    SAFE_RELEASE(viewRes);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DiscardView1(SerialiserType &ser,
                                                        ID3D11View *pResourceView,
                                                        const D3D11_RECT *pRect, UINT NumRects)
{
  SERIALISE_ELEMENT(pResourceView);
  SERIALISE_ELEMENT_ARRAY(pRect, NumRects);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pResourceView)
    {
      // don't replay the discard, as it effectively does nothing meaningful but hint
      // to the driver that the contents can be discarded.
      // Instead we should overwrite the contents with something (during capture too)
      // to indicate that the discard has happened visually like a clear.
      // This also means we don't have to require/diverge if a 11.1 context is not
      // available on replay.
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;

      draw.name = StringFormat::Fmt("DiscardView1(%u)", NumRects);
      draw.flags |= DrawFlags::Clear;

      if(pResourceView)
      {
        if(WrappedID3D11RenderTargetView1::IsAlloc(pResourceView))
        {
          WrappedID3D11RenderTargetView1 *view = (WrappedID3D11RenderTargetView1 *)pResourceView;
          m_ResourceUses[view->GetResourceResID()].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
          draw.copyDestination =
              m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
        }
        else if(WrappedID3D11DepthStencilView::IsAlloc(pResourceView))
        {
          WrappedID3D11DepthStencilView *view = (WrappedID3D11DepthStencilView *)pResourceView;
          m_ResourceUses[view->GetResourceResID()].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
          draw.copyDestination =
              m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
        }
        else if(WrappedID3D11ShaderResourceView1::IsAlloc(pResourceView))
        {
          WrappedID3D11ShaderResourceView1 *view = (WrappedID3D11ShaderResourceView1 *)pResourceView;
          m_ResourceUses[view->GetResourceResID()].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
          draw.copyDestination =
              m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
        }
        else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pResourceView))
        {
          WrappedID3D11UnorderedAccessView1 *view =
              (WrappedID3D11UnorderedAccessView1 *)pResourceView;
          m_ResourceUses[view->GetResourceResID()].push_back(
              EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
          draw.copyDestination =
              m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
        }
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DiscardView1(ID3D11View *pResourceView, const D3D11_RECT *pRects,
                                              UINT NumRects)
{
  if(m_pRealContext1 == NULL)
    return;

  DrainAnnotationQueue();

  if(pResourceView == NULL)
    return;

  m_EmptyCommandList = false;

  {
    ID3D11View *real = NULL;

    if(WrappedID3D11RenderTargetView1::IsAlloc(pResourceView))
      real = UNWRAP(WrappedID3D11RenderTargetView1, pResourceView);
    else if(WrappedID3D11DepthStencilView::IsAlloc(pResourceView))
      real = UNWRAP(WrappedID3D11DepthStencilView, pResourceView);
    else if(WrappedID3D11ShaderResourceView1::IsAlloc(pResourceView))
      real = UNWRAP(WrappedID3D11ShaderResourceView1, pResourceView);
    else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pResourceView))
      real = UNWRAP(WrappedID3D11UnorderedAccessView1, pResourceView);

    RDCASSERT(real);

    SERIALISE_TIME_CALL(m_pRealContext1->DiscardView1(real, pRects, NumRects));
  }

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DiscardView1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_DiscardView1(ser, pResourceView, pRects, NumRects);

    ID3D11Resource *viewRes = NULL;
    pResourceView->GetResource(&viewRes);

    m_MissingTracks.insert(GetIDForResource(viewRes));
    MarkResourceReferenced(GetIDForResource(viewRes), eFrameRef_Write);

    SAFE_RELEASE(viewRes);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsCaptureMode(m_State))
  {
    ID3D11Resource *viewRes = NULL;
    pResourceView->GetResource(&viewRes);

    MarkDirtyResource(GetIDForResource(viewRes));

    SAFE_RELEASE(viewRes);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_SwapDeviceContextState(
    SerialiserType &ser, ID3DDeviceContextState *pState, ID3DDeviceContextState **ppPreviousState)
{
  D3D11RenderState state(D3D11RenderState::Empty);

  if(ser.IsWriting())
  {
    WrappedID3DDeviceContextState *wrapped = (WrappedID3DDeviceContextState *)pState;
    state.CopyState(*wrapped->state);

    state.MarkReferenced(this, true);
  }

  SERIALISE_ELEMENT(state).Named("pState");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_DoStateVerify = false;
    {
      m_CurrentPipelineState->CopyState(state);
      m_CurrentPipelineState->SetDevice(m_pDevice);
      state.ApplyState(this);
    }
    m_DoStateVerify = true;
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::SwapDeviceContextState(ID3DDeviceContextState *pState,
                                                        ID3DDeviceContextState **ppPreviousState)
{
  if(m_pRealContext1 == NULL)
    return;

  ID3DDeviceContextState *prev = NULL;

  SERIALISE_TIME_CALL(m_pRealContext1->SwapDeviceContextState(
      UNWRAP(WrappedID3DDeviceContextState, pState), &prev));

  {
    WrappedID3DDeviceContextState *wrapped = NULL;

    if(m_pDevice->GetResourceManager()->HasWrapper(prev))
      wrapped = (WrappedID3DDeviceContextState *)m_pDevice->GetResourceManager()->GetWrapper(prev);
    else if(prev)
      wrapped = new WrappedID3DDeviceContextState(prev, m_pDevice);

    if(wrapped)
      wrapped->state->CopyState(*m_CurrentPipelineState);

    if(ppPreviousState)
      *ppPreviousState = wrapped;
  }

  {
    WrappedID3DDeviceContextState *wrapped = (WrappedID3DDeviceContextState *)pState;

    m_CurrentPipelineState->CopyState(*wrapped->state);
  }

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::SwapDeviceContextState);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
    Serialise_SwapDeviceContextState(ser, pState, NULL);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

#undef IMPLEMENT_FUNCTION_SERIALISED
#define IMPLEMENT_FUNCTION_SERIALISED(ret, func, ...)                                       \
  template bool WrappedID3D11DeviceContext::CONCAT(Serialise_,                              \
                                                   func(ReadSerialiser &ser, __VA_ARGS__)); \
  template bool WrappedID3D11DeviceContext::CONCAT(Serialise_,                              \
                                                   func(WriteSerialiser &ser, __VA_ARGS__));

SERIALISED_ID3D11CONTEXT1_FUNCTIONS();
