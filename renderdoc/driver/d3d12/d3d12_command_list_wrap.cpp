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

HRESULT WrappedID3D12GraphicsCommandList::Close()
{
  return m_pReal->Close();
}

HRESULT WrappedID3D12GraphicsCommandList::Reset(ID3D12CommandAllocator *pAllocator,
                                                ID3D12PipelineState *pInitialState)
{
  return m_pReal->Reset(pAllocator, pInitialState);
}

void WrappedID3D12GraphicsCommandList::ClearState(ID3D12PipelineState *pPipelineState)
{
  return m_pReal->ClearState(pPipelineState);
}

void WrappedID3D12GraphicsCommandList::DrawInstanced(UINT VertexCountPerInstance,
                                                     UINT InstanceCount, UINT StartVertexLocation,
                                                     UINT StartInstanceLocation)
{
  return m_pReal->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                                StartInstanceLocation);
}

void WrappedID3D12GraphicsCommandList::DrawIndexedInstanced(UINT IndexCountPerInstance,
                                                            UINT InstanceCount,
                                                            UINT StartIndexLocation,
                                                            INT BaseVertexLocation,
                                                            UINT StartInstanceLocation)
{
  return m_pReal->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                       BaseVertexLocation, StartInstanceLocation);
}

void WrappedID3D12GraphicsCommandList::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                                                UINT ThreadGroupCountZ)
{
  return m_pReal->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void WrappedID3D12GraphicsCommandList::CopyBufferRegion(ID3D12Resource *pDstBuffer,
                                                        UINT64 DstOffset, ID3D12Resource *pSrcBuffer,
                                                        UINT64 SrcOffset, UINT64 NumBytes)
{
  return m_pReal->CopyBufferRegion(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);
}

void WrappedID3D12GraphicsCommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst,
                                                         UINT DstX, UINT DstY, UINT DstZ,
                                                         const D3D12_TEXTURE_COPY_LOCATION *pSrc,
                                                         const D3D12_BOX *pSrcBox)
{
  return m_pReal->CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
}

void WrappedID3D12GraphicsCommandList::CopyResource(ID3D12Resource *pDstResource,
                                                    ID3D12Resource *pSrcResource)
{
  return m_pReal->CopyResource(pDstResource, pSrcResource);
}

void WrappedID3D12GraphicsCommandList::CopyTiles(
    ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer,
    UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags)
{
  return m_pReal->CopyTiles(pTiledResource, pTileRegionStartCoordinate, pTileRegionSize, pBuffer,
                            BufferStartOffsetInBytes, Flags);
}

void WrappedID3D12GraphicsCommandList::ResolveSubresource(ID3D12Resource *pDstResource,
                                                          UINT DstSubresource,
                                                          ID3D12Resource *pSrcResource,
                                                          UINT SrcSubresource, DXGI_FORMAT Format)
{
  return m_pReal->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource,
                                     Format);
}

void WrappedID3D12GraphicsCommandList::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
  return m_pReal->IASetPrimitiveTopology(PrimitiveTopology);
}

void WrappedID3D12GraphicsCommandList::RSSetViewports(UINT NumViewports,
                                                      const D3D12_VIEWPORT *pViewports)
{
  return m_pReal->RSSetViewports(NumViewports, pViewports);
}

void WrappedID3D12GraphicsCommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects)
{
  return m_pReal->RSSetScissorRects(NumRects, pRects);
}

void WrappedID3D12GraphicsCommandList::OMSetBlendFactor(const FLOAT BlendFactor[4])
{
  return m_pReal->OMSetBlendFactor(BlendFactor);
}

void WrappedID3D12GraphicsCommandList::OMSetStencilRef(UINT StencilRef)
{
  return m_pReal->OMSetStencilRef(StencilRef);
}

void WrappedID3D12GraphicsCommandList::SetPipelineState(ID3D12PipelineState *pPipelineState)
{
  return m_pReal->SetPipelineState(pPipelineState);
}

void WrappedID3D12GraphicsCommandList::ResourceBarrier(UINT NumBarriers,
                                                       const D3D12_RESOURCE_BARRIER *pBarriers)
{
  return m_pReal->ResourceBarrier(NumBarriers, pBarriers);
}

void WrappedID3D12GraphicsCommandList::ExecuteBundle(ID3D12GraphicsCommandList *pCommandList)
{
  return m_pReal->ExecuteBundle(pCommandList);
}

void WrappedID3D12GraphicsCommandList::SetDescriptorHeaps(UINT NumDescriptorHeaps,
                                                          ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
  return m_pReal->SetDescriptorHeaps(NumDescriptorHeaps, ppDescriptorHeaps);
}

void WrappedID3D12GraphicsCommandList::SetComputeRootSignature(ID3D12RootSignature *pRootSignature)
{
  return m_pReal->SetComputeRootSignature(pRootSignature);
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature)
{
  return m_pReal->SetGraphicsRootSignature(pRootSignature);
}

void WrappedID3D12GraphicsCommandList::SetComputeRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  return m_pReal->SetComputeRootDescriptorTable(RootParameterIndex, BaseDescriptor);
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  return m_pReal->SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);
}

void WrappedID3D12GraphicsCommandList::SetComputeRoot32BitConstant(UINT RootParameterIndex,
                                                                   UINT SrcData,
                                                                   UINT DestOffsetIn32BitValues)
{
  return m_pReal->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(UINT RootParameterIndex,
                                                                    UINT SrcData,
                                                                    UINT DestOffsetIn32BitValues)
{
  return m_pReal->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void WrappedID3D12GraphicsCommandList::SetComputeRoot32BitConstants(UINT RootParameterIndex,
                                                                    UINT Num32BitValuesToSet,
                                                                    const void *pSrcData,
                                                                    UINT DestOffsetIn32BitValues)
{
  return m_pReal->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                               DestOffsetIn32BitValues);
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(UINT RootParameterIndex,
                                                                     UINT Num32BitValuesToSet,
                                                                     const void *pSrcData,
                                                                     UINT DestOffsetIn32BitValues)
{
  return m_pReal->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                                DestOffsetIn32BitValues);
}

void WrappedID3D12GraphicsCommandList::SetComputeRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  return m_pReal->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  return m_pReal->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
}

void WrappedID3D12GraphicsCommandList::SetComputeRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  return m_pReal->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  return m_pReal->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);
}

void WrappedID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  return m_pReal->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  return m_pReal->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

void WrappedID3D12GraphicsCommandList::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView)
{
  return m_pReal->IASetIndexBuffer(pView);
}

void WrappedID3D12GraphicsCommandList::IASetVertexBuffers(UINT StartSlot, UINT NumViews,
                                                          const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
  return m_pReal->IASetVertexBuffers(StartSlot, NumViews, pViews);
}

void WrappedID3D12GraphicsCommandList::SOSetTargets(UINT StartSlot, UINT NumViews,
                                                    const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
  return m_pReal->SOSetTargets(StartSlot, NumViews, pViews);
}

void WrappedID3D12GraphicsCommandList::OMSetRenderTargets(
    UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor)
{
  return m_pReal->OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors,
                                     RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
}

void WrappedID3D12GraphicsCommandList::ClearDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
    UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
  return m_pReal->ClearDepthStencilView(DepthStencilView, ClearFlags, Depth, Stencil, NumRects,
                                        pRects);
}

void WrappedID3D12GraphicsCommandList::ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects,
    const D3D12_RECT *pRects)
{
  return m_pReal->ClearRenderTargetView(RenderTargetView, ColorRGBA, NumRects, pRects);
}

void WrappedID3D12GraphicsCommandList::ClearUnorderedAccessViewUint(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  return m_pReal->ClearUnorderedAccessViewUint(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource,
                                               Values, NumRects, pRects);
}

void WrappedID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  return m_pReal->ClearUnorderedAccessViewFloat(ViewGPUHandleInCurrentHeap, ViewCPUHandle,
                                                pResource, Values, NumRects, pRects);
}

void WrappedID3D12GraphicsCommandList::DiscardResource(ID3D12Resource *pResource,
                                                       const D3D12_DISCARD_REGION *pRegion)
{
  return m_pReal->DiscardResource(pResource, pRegion);
}

void WrappedID3D12GraphicsCommandList::BeginQuery(ID3D12QueryHeap *pQueryHeap,
                                                  D3D12_QUERY_TYPE Type, UINT Index)
{
  return m_pReal->BeginQuery(pQueryHeap, Type, Index);
}

void WrappedID3D12GraphicsCommandList::EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type,
                                                UINT Index)
{
  return m_pReal->EndQuery(pQueryHeap, Type, Index);
}

void WrappedID3D12GraphicsCommandList::ResolveQueryData(ID3D12QueryHeap *pQueryHeap,
                                                        D3D12_QUERY_TYPE Type, UINT StartIndex,
                                                        UINT NumQueries,
                                                        ID3D12Resource *pDestinationBuffer,
                                                        UINT64 AlignedDestinationBufferOffset)
{
  return m_pReal->ResolveQueryData(pQueryHeap, Type, StartIndex, NumQueries, pDestinationBuffer,
                                   AlignedDestinationBufferOffset);
}

void WrappedID3D12GraphicsCommandList::SetPredication(ID3D12Resource *pBuffer,
                                                      UINT64 AlignedBufferOffset,
                                                      D3D12_PREDICATION_OP Operation)
{
  return m_pReal->SetPredication(pBuffer, AlignedBufferOffset, Operation);
}

void WrappedID3D12GraphicsCommandList::SetMarker(UINT Metadata, const void *pData, UINT Size)
{
  return m_pReal->SetMarker(Metadata, pData, Size);
}

void WrappedID3D12GraphicsCommandList::BeginEvent(UINT Metadata, const void *pData, UINT Size)
{
  return m_pReal->BeginEvent(Metadata, pData, Size);
}

void WrappedID3D12GraphicsCommandList::EndEvent()
{
  return m_pReal->EndEvent();
}

void WrappedID3D12GraphicsCommandList::ExecuteIndirect(ID3D12CommandSignature *pCommandSignature,
                                                       UINT MaxCommandCount,
                                                       ID3D12Resource *pArgumentBuffer,
                                                       UINT64 ArgumentBufferOffset,
                                                       ID3D12Resource *pCountBuffer,
                                                       UINT64 CountBufferOffset)
{
  return m_pReal->ExecuteIndirect(pCommandSignature, MaxCommandCount, pArgumentBuffer,
                                  ArgumentBufferOffset, pCountBuffer, CountBufferOffset);
}