/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "d3d11_video.h"

/////////////////////////////////
// implement ID3D11DeviceContext1

extern uint32_t NullCBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
extern uint32_t NullCBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

static UINT UpdateDataSize(UINT width, UINT height, UINT depth, DXGI_FORMAT fmt, UINT SrcRowPitch,
                           UINT SrcDepthPitch, const D3D11_BOX *pDstBox)
{
  // if we have a box, apply its dimensions instead of the texture's
  if(pDstBox)
  {
    width = RDCMIN(width, pDstBox->right - pDstBox->left);
    height = pDstBox->bottom - pDstBox->top;
    depth = pDstBox->back - pDstBox->front;
  }

  // empty boxes in any dimension are always 0 bytes
  if(width == 0 || height == 0 || depth == 0)
    return 0;

  if(IsBlockFormat(fmt))
    height = RDCMAX(1U, AlignUp4(height) / 4);
  else if(IsYUVPlanarFormat(fmt))
    height = GetYUVNumRows(fmt, height);

  UINT SourceDataLength = 0;

  // if we're copying multiple slices, all but the last one consume SrcDepthPitch bytes
  if(depth > 1)
    SourceDataLength += SrcDepthPitch * (depth - 1);

  // similarly if we're copying multiple rows (possibly block-sized rows) in the final slice
  if(height > 1)
    SourceDataLength += SrcRowPitch * (height - 1);

  // lastly, the final row (or block row) consumes just a tightly packed amount of data
  SourceDataLength += GetRowPitch(width, fmt, 0);

  return SourceDataLength;
}

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

        const UINT mipLevel = GetMipForSubresource(pDstResource, DstSubresource);

        UINT width = 0, height = 0, depth = 0;
        DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

        if(tex1)
        {
          D3D11_TEXTURE1D_DESC desc = {0};
          tex1->GetDesc(&desc);

          fmt = desc.Format;
          width = RDCMAX(1U, desc.Width >> mipLevel);
          height = 1;
          depth = 1;
        }
        else if(tex2)
        {
          D3D11_TEXTURE2D_DESC desc = {0};
          tex2->GetDesc(&desc);

          fmt = desc.Format;
          width = RDCMAX(1U, desc.Width >> mipLevel);
          height = RDCMAX(1U, desc.Height >> mipLevel);
          depth = 1;
        }
        else if(tex3)
        {
          D3D11_TEXTURE3D_DESC desc = {0};
          tex3->GetDesc(&desc);

          fmt = desc.Format;
          width = RDCMAX(1U, desc.Width >> mipLevel);
          height = RDCMAX(1U, desc.Height >> mipLevel);
          depth = RDCMAX(1U, desc.Depth >> mipLevel);
        }
        else
        {
          RDCERR("UpdateSubResource on unexpected resource type");
        }

        SourceDataLength =
            UpdateDataSize(width, height, depth, fmt, SrcRowPitch, SrcDepthPitch, pDstBox);
      }

      if(IsActiveCapturing(m_State))
      {
        // partial update
        if(SourceDataLength != (uint32_t)record->Length)
          MarkResourceReferenced(record->GetResourceID(), eFrameRef_Read);
        MarkResourceReferenced(record->GetResourceID(), eFrameRef_PartialWrite);
      }
    }

    SERIALISE_ELEMENT_ARRAY(pSrcData, SourceDataLength);
    SERIALISE_ELEMENT(SourceDataLength);

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

    SERIALISE_ELEMENT(ContentsLength);

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

      UINT SourceRowPitch = GetRowPitch(subWidth, fmt, 0);
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

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext1->UpdateSubresource1(
      m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, pDstBox,
      pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::UpdateSubresource1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_UpdateSubresource1(ser, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
                                 SrcDepthPitch, CopyFlags);

    MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_PartialWrite);

    MarkDirtyResource(GetIDForResource(pDstResource));

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

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext1->CopySubresourceRegion1(
      GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, DstX, DstY, DstZ,
      GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, pSrcBox, CopyFlags));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CopySubresourceRegion1);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_CopySubresourceRegion1(ser, pDstResource, DstSubresource, DstX, DstY, DstZ,
                                     pSrcResource, SrcSubresource, pSrcBox, CopyFlags);

    MarkDirtyResource(GetIDForResource(pDstResource));

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
  SERIALISE_ELEMENT_ARRAY(ColorRGBA, 4);
  SERIALISE_ELEMENT_ARRAY(pRect, NumRects);
  SERIALISE_ELEMENT(NumRects);

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

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  if(pView == NULL)
    return;

  m_EmptyCommandList = false;

  bool isVideo = false;

  {
    ID3D11View *real = NULL;

    if(WrappedID3D11RenderTargetView1::IsAlloc(pView))
    {
      real = UNWRAP(WrappedID3D11RenderTargetView1, pView);
    }
    else if(WrappedID3D11DepthStencilView::IsAlloc(pView))
    {
      real = UNWRAP(WrappedID3D11DepthStencilView, pView);
    }
    else if(WrappedID3D11ShaderResourceView1::IsAlloc(pView))
    {
      real = UNWRAP(WrappedID3D11ShaderResourceView1, pView);
    }
    else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pView))
    {
      real = UNWRAP(WrappedID3D11UnorderedAccessView1, pView);
    }
    else if(WrappedID3D11VideoDecoderOutputView::IsAlloc(pView))
    {
      real = UNWRAP(WrappedID3D11VideoDecoderOutputView, pView);
      isVideo = true;
    }
    else if(WrappedID3D11VideoProcessorInputView::IsAlloc(pView))
    {
      real = UNWRAP(WrappedID3D11VideoProcessorInputView, pView);
      isVideo = true;
    }
    else if(WrappedID3D11VideoProcessorOutputView::IsAlloc(pView))
    {
      real = UNWRAP(WrappedID3D11VideoProcessorOutputView, pView);
      isVideo = true;
    }

    RDCASSERT(real);

    SERIALISE_TIME_CALL(m_pRealContext1->ClearView(real, Color, pRect, NumRects));
  }

  // video views don't take part in the capture at all
  if(!isVideo)
  {
    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      ser.SetDrawChunk();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::ClearView);
      SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
      Serialise_ClearView(ser, pView, Color, pRect, NumRects);

      ID3D11Resource *viewRes = NULL;
      pView->GetResource(&viewRes);

      MarkDirtyResource(GetIDForResource(viewRes));
      MarkResourceReferenced(GetIDForResource(viewRes), eFrameRef_PartialWrite);

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
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_VSSetConstantBuffers1(
    SerialiserType &ser, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
    const UINT *pFirstConstant, const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
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
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
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
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
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
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
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
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
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
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pFirstConstant, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pNumConstants, NumBuffers);

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

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
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DiscardResource(ser, pResource);

    MarkDirtyResource(GetIDForResource(pResource));
    MarkResourceReferenced(GetIDForResource(pResource), eFrameRef_PartialWrite);

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

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  if(pResourceView == NULL)
    return;

  m_EmptyCommandList = false;

  bool isVideo = false;

  {
    ID3D11View *real = NULL;

    if(WrappedID3D11RenderTargetView1::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11RenderTargetView1, pResourceView);
    }
    else if(WrappedID3D11DepthStencilView::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11DepthStencilView, pResourceView);
    }
    else if(WrappedID3D11ShaderResourceView1::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11ShaderResourceView1, pResourceView);
    }
    else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11UnorderedAccessView1, pResourceView);
    }
    else if(WrappedID3D11VideoDecoderOutputView::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11VideoDecoderOutputView, pResourceView);
      isVideo = true;
    }
    else if(WrappedID3D11VideoProcessorInputView::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11VideoProcessorInputView, pResourceView);
      isVideo = true;
    }
    else if(WrappedID3D11VideoProcessorOutputView::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11VideoProcessorOutputView, pResourceView);
      isVideo = true;
    }

    RDCASSERT(real);

    SERIALISE_TIME_CALL(m_pRealContext1->DiscardView(real));
  }

  // video views don't take part in the capture at all
  if(!isVideo)
  {
    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      ser.SetDrawChunk();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::DiscardView);
      SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
      Serialise_DiscardView(ser, pResourceView);

      ID3D11Resource *viewRes = NULL;
      pResourceView->GetResource(&viewRes);

      MarkDirtyResource(GetIDForResource(viewRes));
      MarkResourceReferenced(GetIDForResource(viewRes), eFrameRef_PartialWrite);

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
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DiscardView1(SerialiserType &ser,
                                                        ID3D11View *pResourceView,
                                                        const D3D11_RECT *pRect, UINT NumRects)
{
  SERIALISE_ELEMENT(pResourceView);
  SERIALISE_ELEMENT_ARRAY(pRect, NumRects);
  SERIALISE_ELEMENT(NumRects);

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

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  if(pResourceView == NULL)
    return;

  m_EmptyCommandList = false;

  bool isVideo = false;

  {
    ID3D11View *real = NULL;

    if(WrappedID3D11RenderTargetView1::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11RenderTargetView1, pResourceView);
    }
    else if(WrappedID3D11DepthStencilView::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11DepthStencilView, pResourceView);
    }
    else if(WrappedID3D11ShaderResourceView1::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11ShaderResourceView1, pResourceView);
    }
    else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11UnorderedAccessView1, pResourceView);
    }
    else if(WrappedID3D11VideoDecoderOutputView::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11VideoDecoderOutputView, pResourceView);
      isVideo = true;
    }
    else if(WrappedID3D11VideoProcessorInputView::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11VideoProcessorInputView, pResourceView);
      isVideo = true;
    }
    else if(WrappedID3D11VideoProcessorOutputView::IsAlloc(pResourceView))
    {
      real = UNWRAP(WrappedID3D11VideoProcessorOutputView, pResourceView);
      isVideo = true;
    }

    RDCASSERT(real);

    SERIALISE_TIME_CALL(m_pRealContext1->DiscardView1(real, pRects, NumRects));
  }

  // video views don't take part in the capture at all
  if(!isVideo)
  {
    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      ser.SetDrawChunk();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::DiscardView1);
      SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
      Serialise_DiscardView1(ser, pResourceView, pRects, NumRects);

      ID3D11Resource *viewRes = NULL;
      pResourceView->GetResource(&viewRes);

      MarkDirtyResource(GetIDForResource(viewRes));
      MarkResourceReferenced(GetIDForResource(viewRes), eFrameRef_PartialWrite);

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

  SERIALISE_ELEMENT(state).Named("pState"_lit);

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

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3DDeviceContextState *prev = NULL;

  SERIALISE_TIME_CALL(m_pRealContext1->SwapDeviceContextState(
      UNWRAP(WrappedID3DDeviceContextState, pState), &prev));

  {
    WrappedID3DDeviceContextState *wrapped = NULL;

    if(m_pDevice->GetResourceManager()->HasWrapper(prev))
    {
      wrapped = (WrappedID3DDeviceContextState *)m_pDevice->GetResourceManager()->GetWrapper(prev);

      wrapped->AddRef();
    }
    else if(prev)
    {
      wrapped = new WrappedID3DDeviceContextState(prev, m_pDevice);
    }

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
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
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

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

static D3D11_BOX *box(UINT x, UINT w, UINT y = 0, UINT h = 1, UINT z = 0, UINT d = 1)
{
  static D3D11_BOX ret = {};

  ret.left = x;
  ret.right = x + w;
  ret.top = y;
  ret.bottom = y + h;
  ret.front = z;
  ret.back = z + d;

  return &ret;
}

TEST_CASE("Check UpdateSubresource data length calculations", "[d3d]")
{
  // when we want to check that pitches aren't used, we set a high value.
  UINT p = 999999999U;

  SECTION("1D with no box")
  {
    // simple pitch
    CHECK(UpdateDataSize(45, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, p, p, NULL) == 45 * (4 * sizeof(byte)));

    // 1D textures can be block compressed, they still consume N bytes per 4x4 block, even if only
    // 4x1 is used. 9 blocks is 36 pixels (covering 35)
    CHECK(UpdateDataSize(35, 1, 1, DXGI_FORMAT_BC1_UNORM, p, p, NULL) == 9 * (8 * sizeof(byte)));
  }

  SECTION("1D with box")
  {
    CHECK(UpdateDataSize(45, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, p, p, box(10, 5)) ==
          5 * (4 * sizeof(byte)));

    // 1D textures can be block compressed, they still consume N bytes per 4x4 block, even if only
    // 4x1 is used. 4 blocks is 16 pixels (covering 15)
    CHECK(UpdateDataSize(35, 1, 1, DXGI_FORMAT_BC1_UNORM, p, p, box(8, 15)) == 4 * (8 * sizeof(byte)));

    // empty box
    CHECK(UpdateDataSize(115, 1, 1, DXGI_FORMAT_R32G32B32_FLOAT, p, p, box(99, 0)) == 0);
  }

  SECTION("2D with no box")
  {
    // tightly packed pitch
    CHECK(UpdateDataSize(45, 33, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 45 * (4 * sizeof(byte)), p, NULL) ==
          45 * 33 * (4 * sizeof(byte)));

    // degenerate 2D texture as 1D
    CHECK(UpdateDataSize(45, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, p, p, NULL) == 45 * (4 * sizeof(byte)));

    // pitch larger than a row
    CHECK(UpdateDataSize(45, 33, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 4096, p, NULL) ==
          4096 * 32 + 45 * (4 * sizeof(byte)));

    // pitch smaller than a row
    CHECK(UpdateDataSize(45, 33, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 12, p, NULL) ==
          12 * 32 + 45 * (4 * sizeof(byte)));

    // block compressed
    //
    // 48x48 = 12x12 blocks, tightly packed
    CHECK(UpdateDataSize(48, 48, 1, DXGI_FORMAT_BC1_UNORM, 12 * (8 * sizeof(byte)), p, NULL) ==
          12 * 12 * (8 * sizeof(byte)));
    // with a larger pitch
    CHECK(UpdateDataSize(48, 48, 1, DXGI_FORMAT_BC1_UNORM, 2500, p, NULL) ==
          2500 * 11 + 12 * (8 * sizeof(byte)));

    // 57x94 = 15x24 blocks, tightly packed
    CHECK(UpdateDataSize(57, 94, 1, DXGI_FORMAT_BC1_UNORM, 15 * (8 * sizeof(byte)), p, NULL) ==
          15 * 24 * (8 * sizeof(byte)));
    // with a larger pitch
    CHECK(UpdateDataSize(57, 94, 1, DXGI_FORMAT_BC1_UNORM, 797, p, NULL) ==
          797 * 23 + 15 * (8 * sizeof(byte)));
  }

  SECTION("2D with box")
  {
    // tightly packed source data pitch
    CHECK(UpdateDataSize(45, 33, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 7 * (4 * sizeof(byte)), p,
                         box(5, 7, 13, 8)) == 7 * 8 * (4 * sizeof(byte)));

    // degenerate 2D texture as 1D
    CHECK(UpdateDataSize(45, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, p, p, box(20, 7)) ==
          7 * (4 * sizeof(byte)));

    // pitch larger than a row
    CHECK(UpdateDataSize(45, 33, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 4096, p, box(19, 19, 20, 5)) ==
          4096 * 4 + 19 * (4 * sizeof(byte)));

    // pitch smaller than a row
    CHECK(UpdateDataSize(45, 33, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 12, p, box(2, 34, 5, 5)) ==
          12 * 4 + 34 * (4 * sizeof(byte)));

    // block compressed
    //
    // 16x24 = 4x6 blocks, tightly packed
    CHECK(UpdateDataSize(48, 48, 1, DXGI_FORMAT_BC1_UNORM, 4 * (8 * sizeof(byte)), p,
                         box(8, 16, 20, 24)) == 4 * 6 * (8 * sizeof(byte)));
    // with a larger pitch
    CHECK(UpdateDataSize(48, 48, 1, DXGI_FORMAT_BC1_UNORM, 2500, p, box(8, 16, 20, 24)) ==
          2500 * 5 + 4 * (8 * sizeof(byte)));

    // empty box
    CHECK(UpdateDataSize(812, 384, 1, DXGI_FORMAT_R32G32B32_FLOAT, p, p, box(19, 0, 58, 200)) == 0);
    CHECK(UpdateDataSize(812, 384, 1, DXGI_FORMAT_R32G32B32_FLOAT, p, p, box(19, 10, 58, 0)) == 0);
  }

  SECTION("3D with no box")
  {
    // simple pitch
    CHECK(UpdateDataSize(45, 34, 23, DXGI_FORMAT_R8G8B8A8_UNORM, 45 * (4 * sizeof(byte)),
                         45 * 34 * (4 * sizeof(byte)), NULL) == 45 * 34 * 23 * (4 * sizeof(byte)));

    // larger row pitch
    CHECK(UpdateDataSize(45, 34, 23, DXGI_FORMAT_R8G8B8A8_UNORM, 1200, 1200 * 34, NULL) ==
          1200 * 34 * 22 + 33 * 1200 + 45 * (4 * sizeof(byte)));

    // larger slice pitch
    CHECK(UpdateDataSize(45, 34, 23, DXGI_FORMAT_R8G8B8A8_UNORM, 1200, 800000, NULL) ==
          800000 * 22 + 33 * 1200 + 45 * (4 * sizeof(byte)));
  }

  SECTION("3D with box")
  {
    // simple pitch
    CHECK(UpdateDataSize(45, 34, 23, DXGI_FORMAT_R8G8B8A8_UNORM, 5 * (4 * sizeof(byte)),
                         5 * 10 * (4 * sizeof(byte)),
                         box(9, 5, 20, 10, 15, 8)) == 5 * 10 * 8 * (4 * sizeof(byte)));

    // larger row pitch
    CHECK(UpdateDataSize(45, 34, 23, DXGI_FORMAT_R8G8B8A8_UNORM, 1200, 1200 * 34,
                         box(9, 5, 20, 10, 15, 8)) ==
          1200 * 34 * 7 + 1200 * 9 + 5 * (4 * sizeof(byte)));

    // larger slice pitch
    CHECK(UpdateDataSize(45, 34, 23, DXGI_FORMAT_R8G8B8A8_UNORM, 1200, 800000,
                         box(9, 5, 20, 10, 15, 8)) == 800000 * 7 + 1200 * 9 + 5 * (4 * sizeof(byte)));
  }
}

#endif