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
#include "serialise/string_utils.h"
#include "d3d11_renderstate.h"
#include "d3d11_resources.h"

/////////////////////////////////
// implement ID3D11DeviceContext1

extern uint32_t NullCBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
extern uint32_t NullCBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

bool WrappedID3D11DeviceContext::Serialise_UpdateSubresource1(ID3D11Resource *pDstResource,
                                                              UINT DstSubresource,
                                                              const D3D11_BOX *pDstBox,
                                                              const void *pSrcData, UINT SrcRowPitch,
                                                              UINT SrcDepthPitch, UINT CopyFlags)
{
  SERIALISE_ELEMENT(ResourceId, idx, GetIDForResource(pDstResource));
  SERIALISE_ELEMENT(uint32_t, flags, CopyFlags);
  SERIALISE_ELEMENT(uint32_t, DestSubresource, DstSubresource);

  D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(idx);

  if(record && record->NumSubResources > (int)DestSubresource)
    record = (D3D11ResourceRecord *)record->SubResources[DestSubresource];

  SERIALISE_ELEMENT(uint8_t, isUpdate, record->DataInSerialiser || m_State == WRITING_CAPFRAME);

  ID3D11Resource *DestResource = pDstResource;
  if(m_State < WRITING)
  {
    if(m_pDevice->GetResourceManager()->HasLiveResource(idx))
      DestResource = (ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(idx);
  }

  if(isUpdate)
  {
    SERIALISE_ELEMENT(uint8_t, HasDestBox, pDstBox != NULL);
    SERIALISE_ELEMENT_OPT(D3D11_BOX, box, *pDstBox, HasDestBox);
    SERIALISE_ELEMENT(uint32_t, SourceRowPitch, SrcRowPitch);
    SERIALISE_ELEMENT(uint32_t, SourceDepthPitch, SrcDepthPitch);

    size_t srcLength = 0;

    if(m_State >= WRITING)
    {
      RDCASSERT(record);

      if(WrappedID3D11Buffer::IsAlloc(DestResource))
      {
        srcLength = (size_t)record->Length;

        if(HasDestBox)
          srcLength = RDCMIN((uint32_t)srcLength, pDstBox->right - pDstBox->left);
      }
      else
      {
        WrappedID3D11Texture1D *tex1 = WrappedID3D11Texture1D::IsAlloc(DestResource)
                                           ? (WrappedID3D11Texture1D *)DestResource
                                           : NULL;
        WrappedID3D11Texture2D1 *tex2 = WrappedID3D11Texture2D1::IsAlloc(DestResource)
                                            ? (WrappedID3D11Texture2D1 *)DestResource
                                            : NULL;
        WrappedID3D11Texture3D1 *tex3 = WrappedID3D11Texture3D1::IsAlloc(DestResource)
                                            ? (WrappedID3D11Texture3D1 *)DestResource
                                            : NULL;

        UINT mipLevel = GetMipForSubresource(DestResource, DestSubresource);

        if(tex1)
        {
          srcLength = (size_t)record->Length;

          if(HasDestBox)
            srcLength = RDCMIN((uint32_t)srcLength, pDstBox->right - pDstBox->left);
        }
        else if(tex2)
        {
          D3D11_TEXTURE2D_DESC desc = {0};
          tex2->GetDesc(&desc);
          size_t rows = RDCMAX(1U, desc.Height >> mipLevel);
          DXGI_FORMAT fmt = desc.Format;

          if(HasDestBox)
            rows = (pDstBox->bottom - pDstBox->top);

          if(IsBlockFormat(fmt))
            rows = RDCMAX((size_t)1, rows / 4);

          srcLength = SourceRowPitch * rows;
        }
        else if(tex3)
        {
          D3D11_TEXTURE3D_DESC desc = {0};
          tex3->GetDesc(&desc);
          size_t slices = RDCMAX(1U, desc.Depth >> mipLevel);

          srcLength = SourceDepthPitch * slices;

          if(HasDestBox)
            srcLength = SourceDepthPitch * (pDstBox->back - pDstBox->front);
        }
        else
        {
          RDCERR("UpdateSubResource on unexpected resource type");
        }
      }

      if(m_State == WRITING_CAPFRAME)
      {
        // partial update
        if(srcLength != (size_t)record->Length)
          MarkResourceReferenced(idx, eFrameRef_Read);
        MarkResourceReferenced(idx, eFrameRef_Write);
      }
    }

    SERIALISE_ELEMENT(uint32_t, SourceDataLength, (uint32_t)srcLength);

    SERIALISE_ELEMENT_BUF(byte *, SourceData, (byte *)pSrcData, SourceDataLength);

    if(m_State < WRITING && DestResource != NULL)
    {
      D3D11_BOX *pBox = NULL;
      if(HasDestBox)
        pBox = &box;

      if(m_State == READING)
        RecordUpdateStats(DestResource, SourceDataLength, true);

      if(flags == ~0U)
      {
        // don't need to apply update subresource workaround here because we never replay on
        // deferred contexts, so the bug doesn't arise (we don't record-in the workaround).

        m_pRealContext->UpdateSubresource(
            m_pDevice->GetResourceManager()->UnwrapResource(DestResource), DestSubresource, pBox,
            SourceData, SourceRowPitch, SourceDepthPitch);
      }
      else
      {
        if(m_pRealContext1)
        {
          m_pRealContext1->UpdateSubresource1(
              m_pDevice->GetResourceManager()->UnwrapResource(DestResource), DestSubresource, pBox,
              SourceData, SourceRowPitch, SourceDepthPitch, flags);
        }
        else if(flags == 0)
        {
          // if flags is 0 UpdateSubresource1 behaves identically to UpdateSubresource
          // according to the docs. The only case is the deferred context bug workaround
          // isn't needed, but this wasn't properly handled before, and now this ambiguity
          // is resolved by passing ~0U as the flags to indicate a 'real' call to UpdateSubresource
          m_pRealContext->UpdateSubresource(
              m_pDevice->GetResourceManager()->UnwrapResource(DestResource), DestSubresource, pBox,
              SourceData, SourceRowPitch, SourceDepthPitch);
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

      SAFE_DELETE_ARRAY(SourceData);
    }
    else if(m_State < WRITING)
    {
      SAFE_DELETE_ARRAY(SourceData);
    }
  }
  else
  {
    // fine to truncate the length, D3D11 resource sizes are uint32s
    SERIALISE_ELEMENT(uint32_t, ResourceBufLen, (uint32_t)record->Length);

    byte *padding = m_State >= WRITING ? new byte[ResourceBufLen] : NULL;

    // this is a bit of a hack, but to maintain backwards compatibility we have a
    // separate function here that aligns the next serialised buffer to a 32-byte
    // boundary in memory while writing (just skips the padding on read).
    if(m_State >= WRITING || m_pDevice->GetLogVersion() >= 0x000007)
      m_pSerialiser->AlignNextBuffer(32);

    SERIALISE_ELEMENT_BUF(byte *, bufData, padding, ResourceBufLen);

    if(record)
      record->SetDataOffset(m_pSerialiser->GetOffset() - ResourceBufLen);

    SAFE_DELETE_ARRAY(padding);

    if(m_State < WRITING && DestResource != NULL)
    {
      WrappedID3D11Texture1D *tex1 = WrappedID3D11Texture1D::IsAlloc(DestResource)
                                         ? (WrappedID3D11Texture1D *)DestResource
                                         : NULL;
      WrappedID3D11Texture2D1 *tex2 = WrappedID3D11Texture2D1::IsAlloc(DestResource)
                                          ? (WrappedID3D11Texture2D1 *)DestResource
                                          : NULL;
      WrappedID3D11Texture3D1 *tex3 = WrappedID3D11Texture3D1::IsAlloc(DestResource)
                                          ? (WrappedID3D11Texture3D1 *)DestResource
                                          : NULL;

      DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
      UINT subWidth = 1;
      UINT subHeight = 1;

      UINT mipLevel = GetMipForSubresource(DestResource, DestSubresource);

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

      if(m_State == READING)
        RecordUpdateStats(DestResource,
                          SourceRowPitch * subHeight + SourceDepthPitch * subWidth * subHeight, true);

      if(flags == ~0U)
      {
        m_pRealContext->UpdateSubresource(
            m_pDevice->GetResourceManager()->UnwrapResource(DestResource), DestSubresource, NULL,
            bufData, SourceRowPitch, SourceDepthPitch);
      }
      else
      {
        if(m_pRealContext1)
        {
          m_pRealContext1->UpdateSubresource1(
              m_pDevice->GetResourceManager()->UnwrapResource(DestResource), DestSubresource, NULL,
              bufData, SourceRowPitch, SourceDepthPitch, flags);
        }
        else if(flags == 0)
        {
          // if flags is 0 UpdateSubresource1 behaves identically to UpdateSubresource
          // according to the docs. The only case is the deferred context bug workaround
          // isn't needed, but this wasn't properly handled before, and now this ambiguity
          // is resolved by passing ~0U as the flags to indicate a 'real' call to UpdateSubresource
          m_pRealContext->UpdateSubresource(
              m_pDevice->GetResourceManager()->UnwrapResource(DestResource), DestSubresource, NULL,
              bufData, SourceRowPitch, SourceDepthPitch);
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

    if(m_State < WRITING)
      SAFE_DELETE_ARRAY(bufData);
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(UPDATE_SUBRESOURCE1);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
                                 SrcDepthPitch, CopyFlags);

    m_MissingTracks.insert(GetIDForResource(pDstResource));

    m_ContextRecord->AddChunk(scope.Get());
  }
  else
  {
    // just mark dirty
    MarkDirtyResource(GetIDForResource(pDstResource));
  }

  m_pRealContext1->UpdateSubresource1(m_pDevice->GetResourceManager()->UnwrapResource(pDstResource),
                                      DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch,
                                      CopyFlags);
}

bool WrappedID3D11DeviceContext::Serialise_CopySubresourceRegion1(
    ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ,
    ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox, UINT CopyFlags)
{
  SERIALISE_ELEMENT(ResourceId, Destination, GetIDForResource(pDstResource));
  SERIALISE_ELEMENT(uint32_t, DestSubresource, DstSubresource);
  SERIALISE_ELEMENT(uint32_t, DestX, DstX);
  SERIALISE_ELEMENT(uint32_t, DestY, DstY);
  SERIALISE_ELEMENT(uint32_t, DestZ, DstZ);
  SERIALISE_ELEMENT(ResourceId, Source, GetIDForResource(pSrcResource));
  SERIALISE_ELEMENT(uint32_t, SourceSubresource, SrcSubresource);
  SERIALISE_ELEMENT(uint8_t, HasSourceBox, pSrcBox != NULL);
  SERIALISE_ELEMENT_OPT(D3D11_BOX, SourceBox, *pSrcBox, HasSourceBox);
  SERIALISE_ELEMENT(UINT, flags, CopyFlags);

  if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(Destination) &&
     m_pDevice->GetResourceManager()->HasLiveResource(Source))
  {
    D3D11_BOX *box = &SourceBox;
    if(!HasSourceBox)
      box = NULL;

    if(m_pRealContext1)
    {
      m_pRealContext1->CopySubresourceRegion1(
          m_pDevice->GetResourceManager()->UnwrapResource(
              (ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(Destination)),
          DestSubresource, DestX, DestY, DestZ,
          m_pDevice->GetResourceManager()->UnwrapResource(
              (ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(Source)),
          SourceSubresource, box, flags);
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(COPY_SUBRESOURCE_REGION1);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource,
                                     SrcSubresource, pSrcBox, CopyFlags);

    m_MissingTracks.insert(GetIDForResource(pDstResource));

    m_ContextRecord->AddChunk(scope.Get());
  }
  else
  {
    // just mark dirty
    D3D11ResourceRecord *record =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
    RDCASSERT(record);

    MarkDirtyResource(GetIDForResource(pDstResource));
  }

  m_pRealContext1->CopySubresourceRegion1(
      m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, DstX, DstY,
      DstZ, m_pDevice->GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, pSrcBox,
      CopyFlags);
}

bool WrappedID3D11DeviceContext::Serialise_ClearView(ID3D11View *pView, const FLOAT ColorRGBA[4],
                                                     const D3D11_RECT *pRect, UINT NumRects)
{
  SERIALISE_ELEMENT(ResourceId, View, GetIDForResource(pView));

  float Color[4] = {0};

  if(m_State >= WRITING)
    memcpy(Color, ColorRGBA, sizeof(float) * 4);

  m_pSerialiser->SerialisePODArray<4>("ColorRGBA", Color);

  SERIALISE_ELEMENT(uint32_t, numRects, NumRects);
  SERIALISE_ELEMENT_ARR(D3D11_RECT, rects, pRect, numRects);

  if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(View))
  {
    ID3D11View *wrapped = (ID3D11View *)m_pDevice->GetResourceManager()->GetLiveResource(View);

    ID3D11View *real = NULL;

    if(WrappedID3D11RenderTargetView1::IsAlloc(wrapped))
      real = UNWRAP(WrappedID3D11RenderTargetView1, wrapped);
    else if(WrappedID3D11DepthStencilView::IsAlloc(wrapped))
      real = UNWRAP(WrappedID3D11DepthStencilView, wrapped);
    else if(WrappedID3D11ShaderResourceView1::IsAlloc(wrapped))
      real = UNWRAP(WrappedID3D11ShaderResourceView1, wrapped);
    else if(WrappedID3D11UnorderedAccessView1::IsAlloc(wrapped))
      real = UNWRAP(WrappedID3D11UnorderedAccessView1, wrapped);

    RDCASSERT(real);

    m_pRealContext1->ClearView(real, Color, rects, numRects);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "ClearView(" + ToStr::Get(Color[0]) + ", " + ToStr::Get(Color[1]) + ", " +
                  ToStr::Get(Color[2]) + ", " + ToStr::Get(Color[3]) + ", " + ToStr::Get(numRects) +
                  " rects"
                  ")";

    DrawcallDescription draw;

    draw.name = name;

    draw.flags |= DrawFlags::Clear;

    if(m_pDevice->GetResourceManager()->HasLiveResource(View))
    {
      ID3D11View *wrapped = (ID3D11View *)m_pDevice->GetResourceManager()->GetLiveResource(View);

      ResourceId resid;

      if(WrappedID3D11RenderTargetView1::IsAlloc(wrapped))
        resid = ((WrappedID3D11RenderTargetView1 *)wrapped)->GetResourceID();
      else if(WrappedID3D11DepthStencilView::IsAlloc(wrapped))
        resid = ((WrappedID3D11DepthStencilView *)wrapped)->GetResourceID();
      else if(WrappedID3D11ShaderResourceView1::IsAlloc(wrapped))
        resid = ((WrappedID3D11ShaderResourceView1 *)wrapped)->GetResourceID();
      else if(WrappedID3D11UnorderedAccessView1::IsAlloc(wrapped))
        resid = ((WrappedID3D11UnorderedAccessView1 *)wrapped)->GetResourceID();

      m_ResourceUses[resid].push_back(
          EventUsage(m_CurEventID, ResourceUsage::Clear, GetIDForResource(wrapped)));
      draw.copyDestination = m_pDevice->GetResourceManager()->GetOriginalID(resid);
    }

    AddDrawcall(draw, true);
  }

  SAFE_DELETE_ARRAY(rects);

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

    m_pRealContext1->ClearView(real, Color, pRect, NumRects);
  }

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEAR_VIEW);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_ClearView(pView, Color, pRect, NumRects);

    ID3D11Resource *viewRes = NULL;
    pView->GetResource(&viewRes);

    m_MissingTracks.insert(GetIDForResource(viewRes));
    MarkResourceReferenced(GetIDForResource(viewRes), eFrameRef_Write);

    SAFE_RELEASE(viewRes);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(m_State >= WRITING)
  {
    ID3D11Resource *viewRes = NULL;
    pView->GetResource(&viewRes);

    MarkDirtyResource(GetIDForResource(viewRes));

    SAFE_RELEASE(viewRes);
  }
}

bool WrappedID3D11DeviceContext::Serialise_VSSetConstantBuffers1(UINT StartSlot_, UINT NumBuffers_,
                                                                 ID3D11Buffer *const *ppConstantBuffers,
                                                                 const UINT *pFirstConstant,
                                                                 const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
  SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

  ID3D11Buffer *Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Offsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Counts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  SERIALISE_ELEMENT(bool, setCBs, ppConstantBuffers != NULL);
  SERIALISE_ELEMENT(bool, setOffs, pFirstConstant != NULL);
  SERIALISE_ELEMENT(bool, setCounts, pNumConstants != NULL);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    SERIALISE_ELEMENT(ResourceId, id,
                      ppConstantBuffers ? GetIDForResource(ppConstantBuffers[i]) : ResourceId());
    SERIALISE_ELEMENT(uint32_t, offs, pFirstConstant ? pFirstConstant[i] : 0);
    SERIALISE_ELEMENT(uint32_t, count, pNumConstants ? pNumConstants[i] : 4096);

    if(m_State <= EXECUTING)
    {
      Offsets[i] = offs;
      Counts[i] = count;
      if(m_pDevice->GetResourceManager()->HasLiveResource(id))
      {
        Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
      }
      else
      {
        Buffers[i] = NULL;
        Offsets[i] = NullCBOffsets[0];
        Counts[i] = NullCBCounts[0];
      }
    }
  }

  if(m_State <= EXECUTING)
  {
    if(setCBs)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.ConstantBuffers, Buffers,
                                            StartSlot, NumBuffers);
    if(setOffs)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBOffsets, Offsets, StartSlot,
                                     NumBuffers);
    if(setCounts)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBCounts, Counts, StartSlot,
                                     NumBuffers);

    for(UINT i = 0; i < NumBuffers; i++)
      Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->VSSetConstantBuffers1(StartSlot, NumBuffers, setCBs ? Buffers : NULL,
                                             setOffs ? Offsets : NULL, setCounts ? Counts : NULL);
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
      m_pRealContext->VSSetConstantBuffers(StartSlot, NumBuffers, setCBs ? Buffers : NULL);
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SET_VS_CBUFFERS1);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
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

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(m_State >= WRITING_CAPFRAME)
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  m_pRealContext1->VSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant, pNumConstants);
  VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_HSSetConstantBuffers1(UINT StartSlot_, UINT NumBuffers_,
                                                                 ID3D11Buffer *const *ppConstantBuffers,
                                                                 const UINT *pFirstConstant,
                                                                 const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
  SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

  ID3D11Buffer *Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Offsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Counts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  SERIALISE_ELEMENT(bool, setCBs, ppConstantBuffers != NULL);
  SERIALISE_ELEMENT(bool, setOffs, pFirstConstant != NULL);
  SERIALISE_ELEMENT(bool, setCounts, pNumConstants != NULL);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    SERIALISE_ELEMENT(ResourceId, id,
                      ppConstantBuffers ? GetIDForResource(ppConstantBuffers[i]) : ResourceId());
    SERIALISE_ELEMENT(uint32_t, offs, pFirstConstant ? pFirstConstant[i] : 0);
    SERIALISE_ELEMENT(uint32_t, count, pNumConstants ? pNumConstants[i] : 4096);

    if(m_State <= EXECUTING)
    {
      Offsets[i] = offs;
      Counts[i] = count;
      if(m_pDevice->GetResourceManager()->HasLiveResource(id))
      {
        Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
      }
      else
      {
        Buffers[i] = NULL;
        Offsets[i] = NullCBOffsets[0];
        Counts[i] = NullCBCounts[0];
      }
    }
  }

  if(m_State <= EXECUTING)
  {
    if(setCBs)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.ConstantBuffers, Buffers,
                                            StartSlot, NumBuffers);
    if(setOffs)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBOffsets, Offsets, StartSlot,
                                     NumBuffers);
    if(setCounts)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBCounts, Counts, StartSlot,
                                     NumBuffers);

    for(UINT i = 0; i < NumBuffers; i++)
      Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->HSSetConstantBuffers1(StartSlot, NumBuffers, setCBs ? Buffers : NULL,
                                             setOffs ? Offsets : NULL, setCounts ? Counts : NULL);
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
      m_pRealContext->HSSetConstantBuffers(StartSlot, NumBuffers, setCBs ? Buffers : NULL);
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SET_HS_CBUFFERS1);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
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

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(m_State >= WRITING_CAPFRAME)
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  m_pRealContext1->HSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant, pNumConstants);
  VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_DSSetConstantBuffers1(UINT StartSlot_, UINT NumBuffers_,
                                                                 ID3D11Buffer *const *ppConstantBuffers,
                                                                 const UINT *pFirstConstant,
                                                                 const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
  SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

  ID3D11Buffer *Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Offsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Counts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  SERIALISE_ELEMENT(bool, setCBs, ppConstantBuffers != NULL);
  SERIALISE_ELEMENT(bool, setOffs, pFirstConstant != NULL);
  SERIALISE_ELEMENT(bool, setCounts, pNumConstants != NULL);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    SERIALISE_ELEMENT(ResourceId, id,
                      ppConstantBuffers ? GetIDForResource(ppConstantBuffers[i]) : ResourceId());
    SERIALISE_ELEMENT(uint32_t, offs, pFirstConstant ? pFirstConstant[i] : 0);
    SERIALISE_ELEMENT(uint32_t, count, pNumConstants ? pNumConstants[i] : 4096);

    if(m_State <= EXECUTING)
    {
      Offsets[i] = offs;
      Counts[i] = count;
      if(m_pDevice->GetResourceManager()->HasLiveResource(id))
      {
        Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
      }
      else
      {
        Buffers[i] = NULL;
        Offsets[i] = NullCBOffsets[0];
        Counts[i] = NullCBCounts[0];
      }
    }
  }

  if(m_State <= EXECUTING)
  {
    if(setCBs)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.ConstantBuffers, Buffers,
                                            StartSlot, NumBuffers);
    if(setOffs)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBOffsets, Offsets, StartSlot,
                                     NumBuffers);
    if(setCounts)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBCounts, Counts, StartSlot,
                                     NumBuffers);

    for(UINT i = 0; i < NumBuffers; i++)
      Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->DSSetConstantBuffers1(StartSlot, NumBuffers, setCBs ? Buffers : NULL,
                                             setOffs ? Offsets : NULL, setCounts ? Counts : NULL);
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
      m_pRealContext->DSSetConstantBuffers(StartSlot, NumBuffers, setCBs ? Buffers : NULL);
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SET_DS_CBUFFERS1);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
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

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(m_State >= WRITING_CAPFRAME)
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  m_pRealContext1->DSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant, pNumConstants);
  VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_GSSetConstantBuffers1(UINT StartSlot_, UINT NumBuffers_,
                                                                 ID3D11Buffer *const *ppConstantBuffers,
                                                                 const UINT *pFirstConstant,
                                                                 const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
  SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

  ID3D11Buffer *Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Offsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Counts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  SERIALISE_ELEMENT(bool, setCBs, ppConstantBuffers != NULL);
  SERIALISE_ELEMENT(bool, setOffs, pFirstConstant != NULL);
  SERIALISE_ELEMENT(bool, setCounts, pNumConstants != NULL);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    SERIALISE_ELEMENT(ResourceId, id,
                      ppConstantBuffers ? GetIDForResource(ppConstantBuffers[i]) : ResourceId());
    SERIALISE_ELEMENT(uint32_t, offs, pFirstConstant ? pFirstConstant[i] : 0);
    SERIALISE_ELEMENT(uint32_t, count, pNumConstants ? pNumConstants[i] : 4096);

    if(m_State <= EXECUTING)
    {
      Offsets[i] = offs;
      Counts[i] = count;
      if(m_pDevice->GetResourceManager()->HasLiveResource(id))
      {
        Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
      }
      else
      {
        Buffers[i] = NULL;
        Offsets[i] = NullCBOffsets[0];
        Counts[i] = NullCBCounts[0];
      }
    }
  }

  if(m_State <= EXECUTING)
  {
    if(setCBs)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.ConstantBuffers, Buffers,
                                            StartSlot, NumBuffers);
    if(setOffs)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBOffsets, Offsets, StartSlot,
                                     NumBuffers);
    if(setCounts)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBCounts, Counts, StartSlot,
                                     NumBuffers);

    for(UINT i = 0; i < NumBuffers; i++)
      Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->GSSetConstantBuffers1(StartSlot, NumBuffers, setCBs ? Buffers : NULL,
                                             setOffs ? Offsets : NULL, setCounts ? Counts : NULL);
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
      m_pRealContext->GSSetConstantBuffers(StartSlot, NumBuffers, setCBs ? Buffers : NULL);
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SET_GS_CBUFFERS1);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
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

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(m_State >= WRITING_CAPFRAME)
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  m_pRealContext1->GSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant, pNumConstants);
  VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_PSSetConstantBuffers1(UINT StartSlot_, UINT NumBuffers_,
                                                                 ID3D11Buffer *const *ppConstantBuffers,
                                                                 const UINT *pFirstConstant,
                                                                 const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
  SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

  ID3D11Buffer *Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Offsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Counts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  SERIALISE_ELEMENT(bool, setCBs, ppConstantBuffers != NULL);
  SERIALISE_ELEMENT(bool, setOffs, pFirstConstant != NULL);
  SERIALISE_ELEMENT(bool, setCounts, pNumConstants != NULL);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    SERIALISE_ELEMENT(ResourceId, id,
                      ppConstantBuffers ? GetIDForResource(ppConstantBuffers[i]) : ResourceId());
    SERIALISE_ELEMENT(uint32_t, offs, pFirstConstant ? pFirstConstant[i] : 0);
    SERIALISE_ELEMENT(uint32_t, count, pNumConstants ? pNumConstants[i] : 4096);

    if(m_State <= EXECUTING)
    {
      Offsets[i] = offs;
      Counts[i] = count;
      if(m_pDevice->GetResourceManager()->HasLiveResource(id))
      {
        Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
      }
      else
      {
        Buffers[i] = NULL;
        Offsets[i] = NullCBOffsets[0];
        Counts[i] = NullCBCounts[0];
      }
    }
  }

  if(m_State <= EXECUTING)
  {
    if(setCBs)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.ConstantBuffers, Buffers,
                                            StartSlot, NumBuffers);
    if(setOffs)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBOffsets, Offsets, StartSlot,
                                     NumBuffers);
    if(setCounts)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBCounts, Counts, StartSlot,
                                     NumBuffers);

    for(UINT i = 0; i < NumBuffers; i++)
      Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

    if(m_pRealContext1 && m_SetCBuffer1)
    {
      m_pRealContext1->PSSetConstantBuffers1(StartSlot, NumBuffers, setCBs ? Buffers : NULL,
                                             setOffs ? Offsets : NULL, setCounts ? Counts : NULL);
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
      m_pRealContext->PSSetConstantBuffers(StartSlot, NumBuffers, setCBs ? Buffers : NULL);
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SET_PS_CBUFFERS1);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
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

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(m_State >= WRITING_CAPFRAME)
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  m_pRealContext1->PSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant, pNumConstants);
  VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_CSSetConstantBuffers1(UINT StartSlot_, UINT NumBuffers_,
                                                                 ID3D11Buffer *const *ppConstantBuffers,
                                                                 const UINT *pFirstConstant,
                                                                 const UINT *pNumConstants)
{
  SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
  SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

  ID3D11Buffer *Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Offsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  uint32_t Counts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  SERIALISE_ELEMENT(bool, setCBs, ppConstantBuffers != NULL);
  SERIALISE_ELEMENT(bool, setOffs, pFirstConstant != NULL);
  SERIALISE_ELEMENT(bool, setCounts, pNumConstants != NULL);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    SERIALISE_ELEMENT(ResourceId, id,
                      ppConstantBuffers ? GetIDForResource(ppConstantBuffers[i]) : ResourceId());
    SERIALISE_ELEMENT(uint32_t, offs, pFirstConstant ? pFirstConstant[i] : 0);
    SERIALISE_ELEMENT(uint32_t, count, pNumConstants ? pNumConstants[i] : 4096);

    if(m_State <= EXECUTING)
    {
      Offsets[i] = offs;
      Counts[i] = count;
      if(m_pDevice->GetResourceManager()->HasLiveResource(id))
      {
        Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
      }
      else
      {
        Buffers[i] = NULL;
        Offsets[i] = NullCBOffsets[0];
        Counts[i] = NullCBCounts[0];
      }
    }
  }

  if(m_State <= EXECUTING)
  {
    if(setCBs)
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.ConstantBuffers, Buffers,
                                            StartSlot, NumBuffers);
    if(setOffs)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBOffsets, Offsets, StartSlot,
                                     NumBuffers);
    if(setCounts)
      m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBCounts, Counts, StartSlot,
                                     NumBuffers);

    for(UINT i = 0; i < NumBuffers; i++)
      Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

    if(m_pRealContext1)
    {
      m_pRealContext1->CSSetConstantBuffers1(StartSlot, NumBuffers, setCBs ? Buffers : NULL,
                                             setOffs ? Offsets : NULL, setCounts ? Counts : NULL);
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
      m_pRealContext->CSSetConstantBuffers(StartSlot, NumBuffers, setCBs ? Buffers : NULL);
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SET_CS_CBUFFERS1);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant,
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

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers && ppConstantBuffers[i])
    {
      if(m_State >= WRITING_CAPFRAME)
        MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
    }
  }

  m_pRealContext1->CSSetConstantBuffers1(StartSlot, NumBuffers, bufs, pFirstConstant, pNumConstants);
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

bool WrappedID3D11DeviceContext::Serialise_DiscardResource(ID3D11Resource *pResource)
{
  SERIALISE_ELEMENT(ResourceId, res, GetIDForResource(pResource));

  if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(res))
  {
    // don't replay the discard, as it effectively does nothing meaningful but hint
    // to the driver that the contents can be discarded.
    // Instead we should overwrite the contents with something (during capture too)
    // to indicate that the discard has happened visually like a clear.
    // This also means we don't have to require/diverge if a 11.1 context is not
    // available on replay.
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);

    DrawcallDescription draw;

    draw.name = "DiscardResource()";
    draw.flags |= DrawFlags::Clear;
    draw.copyDestination = res;

    AddDrawcall(draw, true);

    if(m_pDevice->GetResourceManager()->HasLiveResource(res))
      m_ResourceUses[res].push_back(EventUsage(m_CurEventID, ResourceUsage::Clear));
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

    m_pRealContext1->DiscardResource(real);
  }

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DISCARD_RESOURCE);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_DiscardResource(pResource);

    m_MissingTracks.insert(GetIDForResource(pResource));
    MarkResourceReferenced(GetIDForResource(pResource), eFrameRef_Write);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(m_State >= WRITING)
  {
    MarkDirtyResource(GetIDForResource(pResource));
  }
}

bool WrappedID3D11DeviceContext::Serialise_DiscardView(ID3D11View *pResourceView)
{
  SERIALISE_ELEMENT(ResourceId, View, GetIDForResource(pResourceView));

  if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(View))
  {
    // don't replay the discard, as it effectively does nothing meaningful but hint
    // to the driver that the contents can be discarded.
    // Instead we should overwrite the contents with something (during capture too)
    // to indicate that the discard has happened visually like a clear.
    // This also means we don't have to require/diverge if a 11.1 context is not
    // available on replay.
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);

    DrawcallDescription draw;

    draw.name = "DiscardView()";

    draw.flags |= DrawFlags::Clear;

    if(m_pDevice->GetResourceManager()->HasLiveResource(View))
    {
      ID3D11DeviceChild *pLiveView = m_pDevice->GetResourceManager()->GetLiveResource(View);

      if(WrappedID3D11RenderTargetView1::IsAlloc(pLiveView))
      {
        WrappedID3D11RenderTargetView1 *view = (WrappedID3D11RenderTargetView1 *)pLiveView;
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }
      else if(WrappedID3D11DepthStencilView::IsAlloc(pLiveView))
      {
        WrappedID3D11DepthStencilView *view = (WrappedID3D11DepthStencilView *)pLiveView;
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }
      else if(WrappedID3D11ShaderResourceView1::IsAlloc(pLiveView))
      {
        WrappedID3D11ShaderResourceView1 *view = (WrappedID3D11ShaderResourceView1 *)pLiveView;
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }
      else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pLiveView))
      {
        WrappedID3D11UnorderedAccessView1 *view = (WrappedID3D11UnorderedAccessView1 *)pLiveView;
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }
    }

    AddDrawcall(draw, true);
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

    // no need to serialise
    m_pRealContext1->DiscardView(real);
  }

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DISCARD_VIEW);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_DiscardView(pResourceView);

    ID3D11Resource *viewRes = NULL;
    pResourceView->GetResource(&viewRes);

    m_MissingTracks.insert(GetIDForResource(viewRes));
    MarkResourceReferenced(GetIDForResource(viewRes), eFrameRef_Write);

    SAFE_RELEASE(viewRes);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(m_State >= WRITING)
  {
    ID3D11Resource *viewRes = NULL;
    pResourceView->GetResource(&viewRes);

    MarkDirtyResource(GetIDForResource(viewRes));

    SAFE_RELEASE(viewRes);
  }
}

bool WrappedID3D11DeviceContext::Serialise_DiscardView1(ID3D11View *pResourceView,
                                                        const D3D11_RECT *pRect, UINT NumRects)
{
  SERIALISE_ELEMENT(ResourceId, View, GetIDForResource(pResourceView));
  SERIALISE_ELEMENT(uint32_t, numRects, NumRects);
  SERIALISE_ELEMENT_ARR(D3D11_RECT, rects, pRect, NumRects);

  if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(View))
  {
    // don't replay the discard, as it effectively does nothing meaningful but hint
    // to the driver that the contents can be discarded.
    // Instead we should overwrite the contents with something (during capture too)
    // to indicate that the discard has happened visually like a clear.
    // This also means we don't have to require/diverge if a 11.1 context is not
    // available on replay.
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "DiscardView1(" + ToStr::Get(numRects) + " rects)";

    DrawcallDescription draw;

    draw.name = name;

    draw.flags |= DrawFlags::Clear;

    if(m_pDevice->GetResourceManager()->HasLiveResource(View))
    {
      ID3D11DeviceChild *pLiveView = m_pDevice->GetResourceManager()->GetLiveResource(View);

      if(WrappedID3D11RenderTargetView1::IsAlloc(pLiveView))
      {
        WrappedID3D11RenderTargetView1 *view = (WrappedID3D11RenderTargetView1 *)pLiveView;
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }
      else if(WrappedID3D11DepthStencilView::IsAlloc(pLiveView))
      {
        WrappedID3D11DepthStencilView *view = (WrappedID3D11DepthStencilView *)pLiveView;
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }
      else if(WrappedID3D11ShaderResourceView1::IsAlloc(pLiveView))
      {
        WrappedID3D11ShaderResourceView1 *view = (WrappedID3D11ShaderResourceView1 *)pLiveView;
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }
      else if(WrappedID3D11UnorderedAccessView1::IsAlloc(pLiveView))
      {
        WrappedID3D11UnorderedAccessView1 *view = (WrappedID3D11UnorderedAccessView1 *)pLiveView;
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }
    }

    AddDrawcall(draw, true);
  }

  SAFE_DELETE_ARRAY(rects);

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

    // no need to serialise
    m_pRealContext1->DiscardView1(real, pRects, NumRects);
  }

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DISCARD_VIEW1);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_DiscardView1(pResourceView, pRects, NumRects);

    ID3D11Resource *viewRes = NULL;
    pResourceView->GetResource(&viewRes);

    m_MissingTracks.insert(GetIDForResource(viewRes));
    MarkResourceReferenced(GetIDForResource(viewRes), eFrameRef_Write);

    SAFE_RELEASE(viewRes);

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(m_State >= WRITING)
  {
    ID3D11Resource *viewRes = NULL;
    pResourceView->GetResource(&viewRes);

    MarkDirtyResource(GetIDForResource(viewRes));

    SAFE_RELEASE(viewRes);
  }
}

bool WrappedID3D11DeviceContext::Serialise_SwapDeviceContextState(
    ID3DDeviceContextState *pState, ID3DDeviceContextState **ppPreviousState)
{
  D3D11RenderState state(m_pSerialiser);

  if(m_State >= WRITING)
  {
    WrappedID3DDeviceContextState *wrapped = (WrappedID3DDeviceContextState *)pState;
    state.CopyState(*wrapped->state);

    state.SetSerialiser(m_pSerialiser);

    state.MarkReferenced(this, true);
  }

  state.Serialise(m_State, m_pDevice);

  if(m_State <= EXECUTING)
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

  m_pRealContext1->SwapDeviceContextState(UNWRAP(WrappedID3DDeviceContextState, pState), &prev);

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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SWAP_DEVICE_STATE);
    m_pSerialiser->Serialise("context", m_ResourceID);
    Serialise_SwapDeviceContextState(pState, NULL);

    m_ContextRecord->AddChunk(scope.Get());
  }
}
