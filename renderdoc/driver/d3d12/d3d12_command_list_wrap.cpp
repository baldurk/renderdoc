/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

ID3D12GraphicsCommandList *WrappedID3D12GraphicsCommandList::GetList(ResourceId id)
{
  return GetResourceManager()->GetLiveAs<WrappedID3D12GraphicsCommandList>(id)->GetReal();
}

bool WrappedID3D12GraphicsCommandList::Serialise_Close()
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());

  if(m_State <= READING)
    GetList(CommandList)->Close();

  return true;
}

HRESULT WrappedID3D12GraphicsCommandList::Close()
{
  if(m_State >= WRITING)
  {
    {
      SCOPED_SERIALISE_CONTEXT(CLOSE_LIST);
      Serialise_Close();

      m_ListRecord->AddChunk(scope.Get());
    }

    m_ListRecord->Bake();
  }

  // bake m_ListRecord to somewhere else

  return m_pReal->Close();
}

bool WrappedID3D12GraphicsCommandList::Serialise_Reset(ID3D12CommandAllocator *pAllocator,
                                                       ID3D12PipelineState *pInitialState)
{
  // parameters to create the list with if needed
  SERIALISE_ELEMENT(IID, riid, m_Init.riid);
  SERIALISE_ELEMENT(UINT, nodeMask, m_Init.nodeMask);
  SERIALISE_ELEMENT(D3D12_COMMAND_LIST_TYPE, type, m_Init.type);

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, Allocator, GetResID(pAllocator));
  SERIALISE_ELEMENT(ResourceId, State, GetResID(pInitialState));

  if(m_State <= READING)
  {
    pAllocator = GetResourceManager()->GetLiveAs<ID3D12CommandAllocator>(Allocator);
    pInitialState =
        State == ResourceId() ? NULL : GetResourceManager()->GetLiveAs<ID3D12PipelineState>(State);

    if(m_State == READING && !GetResourceManager()->HasLiveResource(CommandList))
    {
      ID3D12GraphicsCommandList *list = NULL;
      m_pDevice->CreateCommandList(nodeMask, type, pAllocator, pInitialState, riid, (void **)&list);
    }

    GetList(CommandList)->Reset(Unwrap(pAllocator), Unwrap(pInitialState));
  }

  return true;
}

HRESULT WrappedID3D12GraphicsCommandList::Reset(ID3D12CommandAllocator *pAllocator,
                                                ID3D12PipelineState *pInitialState)
{
  if(m_State >= WRITING)
  {
    // reset for new recording
    m_ListRecord->DeleteChunks();

    // free parents
    m_ListRecord->FreeParents(GetResourceManager());

    // free any baked commands
    if(m_ListRecord->bakedCommands)
      m_ListRecord->bakedCommands->Delete(GetResourceManager());

    m_ListRecord->bakedCommands =
        GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_ListRecord->bakedCommands->type = Resource_GraphicsCommandList;
    m_ListRecord->bakedCommands->SpecialResource = true;
    m_ListRecord->bakedCommands->cmdInfo = new CmdListRecordingInfo();

    {
      SCOPED_SERIALISE_CONTEXT(RESET_LIST);
      Serialise_Reset(pAllocator, pInitialState);

      m_ListRecord->AddChunk(scope.Get());
    }

    // add allocator and initial state (if there is one) as parents
    m_ListRecord->AddParent(GetRecord(pAllocator));
    if(pInitialState)
      m_ListRecord->AddParent(GetRecord(pInitialState));
  }

  return m_pReal->Reset(Unwrap(pAllocator), Unwrap(pInitialState));
}

void WrappedID3D12GraphicsCommandList::ClearState(ID3D12PipelineState *pPipelineState)
{
  m_pReal->ClearState(Unwrap(pPipelineState));
}

void WrappedID3D12GraphicsCommandList::DrawInstanced(UINT VertexCountPerInstance,
                                                     UINT InstanceCount, UINT StartVertexLocation,
                                                     UINT StartInstanceLocation)
{
  m_pReal->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                         StartInstanceLocation);
}

bool WrappedID3D12GraphicsCommandList::Serialise_DrawIndexedInstanced(UINT IndexCountPerInstance,
                                                                      UINT InstanceCount,
                                                                      UINT StartIndexLocation,
                                                                      INT BaseVertexLocation,
                                                                      UINT StartInstanceLocation)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idxCount, IndexCountPerInstance);
  SERIALISE_ELEMENT(UINT, instCount, InstanceCount);
  SERIALISE_ELEMENT(UINT, startIdx, StartIndexLocation);
  SERIALISE_ELEMENT(INT, startVtx, BaseVertexLocation);
  SERIALISE_ELEMENT(UINT, startInst, StartInstanceLocation);

  if(m_State <= READING)
  {
    GetList(CommandList)->DrawIndexedInstanced(idxCount, instCount, startIdx, startVtx, startInst);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  // TODO - Serialise_DebugMessages();

  if(m_State == READING)
  {
    // TODO - AddEvent(DRAW_INDEXED_INST, desc);
    string name =
        "DrawIndexedInstanced(" + ToStr::Get(idxCount) + ", " + ToStr::Get(instCount) + ")";

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = idxCount;
    draw.numInstances = instCount;
    draw.indexOffset = startIdx;
    draw.baseVertex = startVtx;
    draw.instanceOffset = startInst;

    draw.flags |= eDraw_Drawcall | eDraw_Instanced | eDraw_UseIBuffer;

    // TODO - AddDrawcall(draw, true);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::DrawIndexedInstanced(UINT IndexCountPerInstance,
                                                            UINT InstanceCount,
                                                            UINT StartIndexLocation,
                                                            INT BaseVertexLocation,
                                                            UINT StartInstanceLocation)
{
  m_pReal->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                BaseVertexLocation, StartInstanceLocation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED_INST);
    Serialise_DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                   BaseVertexLocation, StartInstanceLocation);

    m_ListRecord->AddChunk(scope.Get());
  }
}

void WrappedID3D12GraphicsCommandList::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                                                UINT ThreadGroupCountZ)
{
  m_pReal->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

bool WrappedID3D12GraphicsCommandList::Serialise_CopyBufferRegion(ID3D12Resource *pDstBuffer,
                                                                  UINT64 DstOffset,
                                                                  ID3D12Resource *pSrcBuffer,
                                                                  UINT64 SrcOffset, UINT64 NumBytes)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, dst, GetResID(pDstBuffer));
  SERIALISE_ELEMENT(UINT64, dstoffs, DstOffset);
  SERIALISE_ELEMENT(ResourceId, src, GetResID(pSrcBuffer));
  SERIALISE_ELEMENT(UINT64, srcoffs, SrcOffset);
  SERIALISE_ELEMENT(UINT64, num, NumBytes);

  if(m_State <= READING && GetResourceManager()->HasLiveResource(dst) &&
     GetResourceManager()->HasLiveResource(src))
  {
    pDstBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(dst);
    pSrcBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(src);

    GetList(CommandList)->CopyBufferRegion(Unwrap(pDstBuffer), dstoffs, Unwrap(pSrcBuffer), srcoffs, num);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::CopyBufferRegion(ID3D12Resource *pDstBuffer,
                                                        UINT64 DstOffset, ID3D12Resource *pSrcBuffer,
                                                        UINT64 SrcOffset, UINT64 NumBytes)
{
  m_pReal->CopyBufferRegion(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset, NumBytes);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(COPY_BUFFER);
    Serialise_CopyBufferRegion(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstBuffer), eFrameRef_Write);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcBuffer), eFrameRef_Read);
  }
}

void WrappedID3D12GraphicsCommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst,
                                                         UINT DstX, UINT DstY, UINT DstZ,
                                                         const D3D12_TEXTURE_COPY_LOCATION *pSrc,
                                                         const D3D12_BOX *pSrcBox)
{
  D3D12_TEXTURE_COPY_LOCATION dst = *pDst;
  dst.pResource = Unwrap(dst.pResource);

  D3D12_TEXTURE_COPY_LOCATION src = *pSrc;
  src.pResource = Unwrap(src.pResource);

  m_pReal->CopyTextureRegion(&dst, DstX, DstY, DstZ, &src, pSrcBox);
}

void WrappedID3D12GraphicsCommandList::CopyResource(ID3D12Resource *pDstResource,
                                                    ID3D12Resource *pSrcResource)
{
  m_pReal->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));
}

void WrappedID3D12GraphicsCommandList::CopyTiles(
    ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer,
    UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags)
{
  m_pReal->CopyTiles(Unwrap(pTiledResource), pTileRegionStartCoordinate, pTileRegionSize,
                     Unwrap(pBuffer), BufferStartOffsetInBytes, Flags);
}

void WrappedID3D12GraphicsCommandList::ResolveSubresource(ID3D12Resource *pDstResource,
                                                          UINT DstSubresource,
                                                          ID3D12Resource *pSrcResource,
                                                          UINT SrcSubresource, DXGI_FORMAT Format)
{
  m_pReal->ResolveSubresource(Unwrap(pDstResource), DstSubresource, Unwrap(pSrcResource),
                              SrcSubresource, Format);
}

bool WrappedID3D12GraphicsCommandList::Serialise_IASetPrimitiveTopology(
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(D3D12_PRIMITIVE_TOPOLOGY, topo, PrimitiveTopology);

  if(m_State <= READING)
  {
    GetList(CommandList)->IASetPrimitiveTopology(topo);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
  m_pReal->IASetPrimitiveTopology(PrimitiveTopology);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_TOPOLOGY);
    Serialise_IASetPrimitiveTopology(PrimitiveTopology);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_RSSetViewports(UINT NumViewports,
                                                                const D3D12_VIEWPORT *pViewports)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, num, NumViewports);
  SERIALISE_ELEMENT_ARR(D3D12_VIEWPORT, views, pViewports, num);

  if(m_State <= READING)
  {
    GetList(CommandList)->RSSetViewports(num, views);
  }

  SAFE_DELETE_ARRAY(views);

  return true;
}

void WrappedID3D12GraphicsCommandList::RSSetViewports(UINT NumViewports,
                                                      const D3D12_VIEWPORT *pViewports)
{
  m_pReal->RSSetViewports(NumViewports, pViewports);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_VIEWPORTS);
    Serialise_RSSetViewports(NumViewports, pViewports);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_RSSetScissorRects(UINT NumRects,
                                                                   const D3D12_RECT *pRects)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, num, NumRects);
  SERIALISE_ELEMENT_ARR(D3D12_RECT, rects, pRects, num);

  if(m_State <= READING)
  {
    GetList(CommandList)->RSSetScissorRects(num, rects);
  }

  SAFE_DELETE_ARRAY(rects);

  return true;
}

void WrappedID3D12GraphicsCommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects)
{
  m_pReal->RSSetScissorRects(NumRects, pRects);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_SCISSORS);
    Serialise_RSSetScissorRects(NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());
  }
}

void WrappedID3D12GraphicsCommandList::OMSetBlendFactor(const FLOAT BlendFactor[4])
{
  m_pReal->OMSetBlendFactor(BlendFactor);
}

void WrappedID3D12GraphicsCommandList::OMSetStencilRef(UINT StencilRef)
{
  m_pReal->OMSetStencilRef(StencilRef);
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetPipelineState(ID3D12PipelineState *pPipelineState)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, pipe, GetResID(pPipelineState));

  if(m_State <= READING)
  {
    pPipelineState = GetResourceManager()->GetLiveAs<ID3D12PipelineState>(pipe);
    GetList(CommandList)->SetPipelineState(Unwrap(pPipelineState));
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetPipelineState(ID3D12PipelineState *pPipelineState)
{
  m_pReal->SetPipelineState(Unwrap(pPipelineState));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_PIPE);
    Serialise_SetPipelineState(pPipelineState);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pPipelineState), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_ResourceBarrier(UINT NumBarriers,
                                                                 const D3D12_RESOURCE_BARRIER *pBarriers)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, num, NumBarriers);
  SERIALISE_ELEMENT_ARR(D3D12_RESOURCE_BARRIER, barriers, pBarriers, num);

  if(m_State <= READING)
  {
    GetList(CommandList)->ResourceBarrier(num, barriers);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ResourceBarrier(UINT NumBarriers,
                                                       const D3D12_RESOURCE_BARRIER *pBarriers)
{
  D3D12_RESOURCE_BARRIER *barriers = new D3D12_RESOURCE_BARRIER[NumBarriers];

  for(UINT i = 0; i < NumBarriers; i++)
  {
    barriers[i] = pBarriers[i];
    barriers[i].Transition.pResource = Unwrap(barriers[i].Transition.pResource);

    // hack while not all resources are wrapped
    if(barriers[i].Transition.pResource == NULL)
      barriers[i].Transition.pResource = pBarriers[i].Transition.pResource;
  }

  m_pReal->ResourceBarrier(NumBarriers, barriers);

  SAFE_DELETE_ARRAY(barriers);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(RESOURCE_BARRIER);
    Serialise_ResourceBarrier(NumBarriers, pBarriers);

    m_ListRecord->AddChunk(scope.Get());

    m_ListRecord->cmdInfo->barriers.insert(m_ListRecord->cmdInfo->barriers.end(), pBarriers,
                                           pBarriers + NumBarriers);
  }
}

void WrappedID3D12GraphicsCommandList::ExecuteBundle(ID3D12GraphicsCommandList *pCommandList)
{
  m_pReal->ExecuteBundle(Unwrap(pCommandList));
}

void WrappedID3D12GraphicsCommandList::SetDescriptorHeaps(UINT NumDescriptorHeaps,
                                                          ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
  RDCUNIMPLEMENTED("SetDescriptorHeaps");    // need to unwrap heaps
  m_pReal->SetDescriptorHeaps(NumDescriptorHeaps, ppDescriptorHeaps);
}

void WrappedID3D12GraphicsCommandList::SetComputeRootSignature(ID3D12RootSignature *pRootSignature)
{
  m_pReal->SetComputeRootSignature(Unwrap(pRootSignature));
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootSignature(
    ID3D12RootSignature *pRootSignature)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, sig, GetResID(pRootSignature));

  if(m_State <= READING)
  {
    pRootSignature = GetResourceManager()->GetLiveAs<ID3D12RootSignature>(sig);
    GetList(CommandList)->SetGraphicsRootSignature(Unwrap(pRootSignature));
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature)
{
  m_pReal->SetGraphicsRootSignature(Unwrap(pRootSignature));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_ROOT_SIG);
    Serialise_SetGraphicsRootSignature(pRootSignature);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pRootSignature), eFrameRef_Read);
  }
}

void WrappedID3D12GraphicsCommandList::SetComputeRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  m_pReal->SetComputeRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  m_pReal->SetGraphicsRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));
}

void WrappedID3D12GraphicsCommandList::SetComputeRoot32BitConstant(UINT RootParameterIndex,
                                                                   UINT SrcData,
                                                                   UINT DestOffsetIn32BitValues)
{
  m_pReal->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(UINT RootParameterIndex,
                                                                    UINT SrcData,
                                                                    UINT DestOffsetIn32BitValues)
{
  m_pReal->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void WrappedID3D12GraphicsCommandList::SetComputeRoot32BitConstants(UINT RootParameterIndex,
                                                                    UINT Num32BitValuesToSet,
                                                                    const void *pSrcData,
                                                                    UINT DestOffsetIn32BitValues)
{
  m_pReal->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                        DestOffsetIn32BitValues);
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(UINT RootParameterIndex,
                                                                     UINT Num32BitValuesToSet,
                                                                     const void *pSrcData,
                                                                     UINT DestOffsetIn32BitValues)
{
  m_pReal->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                         DestOffsetIn32BitValues);
}

void WrappedID3D12GraphicsCommandList::SetComputeRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetComputeRootConstantBufferView(RootParameterIndex, Unwrap(BufferLocation));
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(ResourceId, buffer, GetResID(BufferLocation));

  if(m_State <= READING)
  {
    WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

    GetList(CommandList)->SetGraphicsRootConstantBufferView(idx, pRes->GetGPU());
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetGraphicsRootConstantBufferView(RootParameterIndex, Unwrap(BufferLocation));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_GFX_ROOT_CBV);
    Serialise_SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(BufferLocation), eFrameRef_Read);
  }
}

void WrappedID3D12GraphicsCommandList::SetComputeRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetComputeRootShaderResourceView(RootParameterIndex, Unwrap(BufferLocation));
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetGraphicsRootShaderResourceView(RootParameterIndex, Unwrap(BufferLocation));
}

void WrappedID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetComputeRootUnorderedAccessView(RootParameterIndex, Unwrap(BufferLocation));
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetGraphicsRootUnorderedAccessView(RootParameterIndex, Unwrap(BufferLocation));
}

bool WrappedID3D12GraphicsCommandList::Serialise_IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(bool, HasView, pView != NULL);
  SERIALISE_ELEMENT_OPT(D3D12_INDEX_BUFFER_VIEW, view, *pView, HasView);

  if(m_State <= READING)
  {
    if(HasView)
      GetList(CommandList)->IASetIndexBuffer(&view);
    else
      GetList(CommandList)->IASetIndexBuffer(NULL);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView)
{
  if(pView)
  {
    D3D12_INDEX_BUFFER_VIEW view = *pView;
    view.BufferLocation = Unwrap(view.BufferLocation);

    m_pReal->IASetIndexBuffer(&view);
  }
  else
  {
    m_pReal->IASetIndexBuffer(pView);
  }

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_IBUFFER);
    Serialise_IASetIndexBuffer(pView);

    m_ListRecord->AddChunk(scope.Get());
    if(pView)
      m_ListRecord->MarkResourceFrameReferenced(GetResID(pView->BufferLocation), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_IASetVertexBuffers(
    UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, start, StartSlot);
  SERIALISE_ELEMENT(UINT, num, NumViews);
  SERIALISE_ELEMENT_ARR(D3D12_VERTEX_BUFFER_VIEW, views, pViews, num);

  if(m_State <= READING)
  {
    GetList(CommandList)->IASetVertexBuffers(start, num, views);
  }

  SAFE_DELETE_ARRAY(views);

  return true;
}

void WrappedID3D12GraphicsCommandList::IASetVertexBuffers(UINT StartSlot, UINT NumViews,
                                                          const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
  D3D12_VERTEX_BUFFER_VIEW *unwrapped = new D3D12_VERTEX_BUFFER_VIEW[NumViews];

  for(UINT i = 0; i < NumViews; i++)
  {
    unwrapped[i] = pViews[i];
    unwrapped[i].BufferLocation = Unwrap(unwrapped[i].BufferLocation);
  }

  m_pReal->IASetVertexBuffers(StartSlot, NumViews, unwrapped);

  SAFE_DELETE_ARRAY(unwrapped);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_VBUFFERS);
    Serialise_IASetVertexBuffers(StartSlot, NumViews, pViews);

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < NumViews; i++)
      m_ListRecord->MarkResourceFrameReferenced(GetResID(pViews[i].BufferLocation), eFrameRef_Read);
  }
}

void WrappedID3D12GraphicsCommandList::SOSetTargets(UINT StartSlot, UINT NumViews,
                                                    const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
  D3D12_STREAM_OUTPUT_BUFFER_VIEW *unwrapped = new D3D12_STREAM_OUTPUT_BUFFER_VIEW[NumViews];

  for(UINT i = 0; i < NumViews; i++)
  {
    unwrapped[i] = pViews[i];
    unwrapped[i].BufferLocation = Unwrap(unwrapped[i].BufferLocation);
    unwrapped[i].BufferFilledSizeLocation = Unwrap(unwrapped[i].BufferFilledSizeLocation);
  }

  m_pReal->SOSetTargets(StartSlot, NumViews, unwrapped);

  SAFE_DELETE_ARRAY(unwrapped);
}

bool WrappedID3D12GraphicsCommandList::Serialise_OMSetRenderTargets(
    UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, num, NumRenderTargetDescriptors);
  SERIALISE_ELEMENT(bool, singlehandle, RTsSingleHandleToDescriptorRange != FALSE);

  UINT numHandles = singlehandle ? 1U : num;

  std::vector<PortableHandle> rts;

  if(m_State >= WRITING)
  {
    rts.resize(numHandles);
    // indexing pRenderTargetDescriptors with [i] is fine since if single handle is true,
    // i will only ever be 0 (so we do equivalent of *pRenderTargetDescriptors)
    for(UINT i = 0; i < numHandles; i++)
      rts[i] = ToPortableHandle(pRenderTargetDescriptors[i]);
  }

  m_pSerialiser->Serialise("pRenderTargetDescriptors", rts);

  SERIALISE_ELEMENT(PortableHandle, dsv, pDepthStencilDescriptor
                                             ? ToPortableHandle(*pDepthStencilDescriptor)
                                             : PortableHandle(0));

  if(m_State <= READING)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = FromPortableHandle(GetResourceManager(), dsv);

    D3D12_CPU_DESCRIPTOR_HANDLE *rtHandles = new D3D12_CPU_DESCRIPTOR_HANDLE[numHandles];

    for(UINT i = 0; i < numHandles; i++)
      rtHandles[i] = FromPortableHandle(GetResourceManager(), rts[i]);

    GetList(CommandList)
        ->OMSetRenderTargets(num, rtHandles, singlehandle ? TRUE : FALSE,
                             dsv.heap != ResourceId() ? &dsvHandle : NULL);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::OMSetRenderTargets(
    UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor)
{
  UINT numHandles = RTsSingleHandleToDescriptorRange ? 1U : NumRenderTargetDescriptors;
  D3D12_CPU_DESCRIPTOR_HANDLE *unwrapped = new D3D12_CPU_DESCRIPTOR_HANDLE[numHandles];
  for(UINT i = 0; i < numHandles; i++)
    unwrapped[i] = Unwrap(pRenderTargetDescriptors[i]);

  D3D12_CPU_DESCRIPTOR_HANDLE dsv =
      pDepthStencilDescriptor ? Unwrap(*pDepthStencilDescriptor) : D3D12_CPU_DESCRIPTOR_HANDLE();

  m_pReal->OMSetRenderTargets(NumRenderTargetDescriptors, unwrapped, RTsSingleHandleToDescriptorRange,
                              pDepthStencilDescriptor ? &dsv : NULL);

  SAFE_DELETE_ARRAY(unwrapped);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_RTVS);
    Serialise_OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors,
                                 RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < numHandles; i++)
    {
      D3D12Descriptor *desc = GetWrapped(pRenderTargetDescriptors[i]);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Read);
    }

    if(pDepthStencilDescriptor)
    {
      D3D12Descriptor *desc = GetWrapped(*pDepthStencilDescriptor);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Read);
    }
  }
}

void WrappedID3D12GraphicsCommandList::ClearDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
    UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
  m_pReal->ClearDepthStencilView(Unwrap(DepthStencilView), ClearFlags, Depth, Stencil, NumRects,
                                 pRects);
}

bool WrappedID3D12GraphicsCommandList::Serialise_ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects,
    const D3D12_RECT *pRects)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(PortableHandle, rtv, ToPortableHandle(RenderTargetView));

  float Color[4] = {0};

  if(m_State >= WRITING)
    memcpy(Color, ColorRGBA, sizeof(float) * 4);

  m_pSerialiser->SerialisePODArray<4>("ColorRGBA", Color);

  SERIALISE_ELEMENT(UINT, num, NumRects);
  SERIALISE_ELEMENT_ARR(D3D12_RECT, rects, pRects, num);

  if(m_State <= READING)
  {
    RenderTargetView = FromPortableHandle(GetResourceManager(), rtv);

    GetList(CommandList)->ClearRenderTargetView(RenderTargetView, Color, num, rects);
  }

  SAFE_DELETE_ARRAY(rects);

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects,
    const D3D12_RECT *pRects)
{
  m_pReal->ClearRenderTargetView(Unwrap(RenderTargetView), ColorRGBA, NumRects, pRects);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(CLEAR_RTV);
    Serialise_ClearRenderTargetView(RenderTargetView, ColorRGBA, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());

    {
      D3D12Descriptor *desc = GetWrapped(RenderTargetView);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Read);
    }
  }
}

void WrappedID3D12GraphicsCommandList::ClearUnorderedAccessViewUint(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  m_pReal->ClearUnorderedAccessViewUint(Unwrap(ViewGPUHandleInCurrentHeap), ViewCPUHandle,
                                        Unwrap(pResource), Values, NumRects, pRects);
}

void WrappedID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  m_pReal->ClearUnorderedAccessViewFloat(Unwrap(ViewGPUHandleInCurrentHeap), ViewCPUHandle,
                                         Unwrap(pResource), Values, NumRects, pRects);
}

void WrappedID3D12GraphicsCommandList::DiscardResource(ID3D12Resource *pResource,
                                                       const D3D12_DISCARD_REGION *pRegion)
{
  m_pReal->DiscardResource(Unwrap(pResource), pRegion);
}

void WrappedID3D12GraphicsCommandList::BeginQuery(ID3D12QueryHeap *pQueryHeap,
                                                  D3D12_QUERY_TYPE Type, UINT Index)
{
  m_pReal->BeginQuery(Unwrap(pQueryHeap), Type, Index);
}

void WrappedID3D12GraphicsCommandList::EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type,
                                                UINT Index)
{
  m_pReal->EndQuery(Unwrap(pQueryHeap), Type, Index);
}

void WrappedID3D12GraphicsCommandList::ResolveQueryData(ID3D12QueryHeap *pQueryHeap,
                                                        D3D12_QUERY_TYPE Type, UINT StartIndex,
                                                        UINT NumQueries,
                                                        ID3D12Resource *pDestinationBuffer,
                                                        UINT64 AlignedDestinationBufferOffset)
{
  m_pReal->ResolveQueryData(Unwrap(pQueryHeap), Type, StartIndex, NumQueries,
                            Unwrap(pDestinationBuffer), AlignedDestinationBufferOffset);
}

void WrappedID3D12GraphicsCommandList::SetPredication(ID3D12Resource *pBuffer,
                                                      UINT64 AlignedBufferOffset,
                                                      D3D12_PREDICATION_OP Operation)
{
  m_pReal->SetPredication(Unwrap(pBuffer), AlignedBufferOffset, Operation);
}

void WrappedID3D12GraphicsCommandList::SetMarker(UINT Metadata, const void *pData, UINT Size)
{
  m_pReal->SetMarker(Metadata, pData, Size);
}

void WrappedID3D12GraphicsCommandList::BeginEvent(UINT Metadata, const void *pData, UINT Size)
{
  m_pReal->BeginEvent(Metadata, pData, Size);
}

void WrappedID3D12GraphicsCommandList::EndEvent()
{
  m_pReal->EndEvent();
}

void WrappedID3D12GraphicsCommandList::ExecuteIndirect(ID3D12CommandSignature *pCommandSignature,
                                                       UINT MaxCommandCount,
                                                       ID3D12Resource *pArgumentBuffer,
                                                       UINT64 ArgumentBufferOffset,
                                                       ID3D12Resource *pCountBuffer,
                                                       UINT64 CountBufferOffset)
{
  m_pReal->ExecuteIndirect(Unwrap(pCommandSignature), MaxCommandCount, Unwrap(pArgumentBuffer),
                           ArgumentBufferOffset, Unwrap(pCountBuffer), CountBufferOffset);
}