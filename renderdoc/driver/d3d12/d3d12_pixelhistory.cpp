/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Baldur Karlsson
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

// TODO: Broadly speaking, support for pixel history with multiple render targets
// bound needs more test coverage and testing to ensure proper implementation.

#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_shader_cache.h"

struct D3D12CopyPixelParams
{
  // The image being copied from
  ID3D12Resource *srcImage;

  // The source image format and format to use when copying. In most cases these are
  // the same, but for some planar formats, the copy format will correspond to a single
  // plane's typeless format, not the multi-plane format.
  DXGI_FORMAT srcImageFormat;
  DXGI_FORMAT copyFormat;
  D3D12_RESOURCE_STATES srcImageState;

  // Data about the pixel we want to copy
  uint32_t x;
  uint32_t y;
  uint32_t mip;
  uint32_t sample;
  uint32_t planeSlice;
  uint32_t arraySlice;

  // Additional info to inform how to copy
  bool depthcopy;
  bool multisampled;
  bool scratchBuffer;
};

struct D3D12PixelHistoryResources
{
  ID3D12Resource *dstBuffer;

  // Used for offscreen color/depth/stencil rendering for draw call events.
  ID3D12Resource *colorImage;
  D3D12Descriptor *colorDescriptor;
  ID3D12Resource *dsImage;
  D3D12Descriptor *dsDescriptor;
};

struct D3D12PixelHistoryCallbackInfo
{
  // Original image for which pixel history is requested.
  WrappedID3D12Resource *targetImage;
  D3D12_RESOURCE_DESC targetDesc;

  // Information about the location of the pixel for which history was requested.
  Subresource targetSubresource;
  CompType compType;
  uint32_t x;
  uint32_t y;
  uint32_t sampleMask;

  // Image used to get per fragment data.
  ID3D12Resource *colorImage;
  D3D12Descriptor *colorDescriptor;

  // Image used to get stencil counts.
  ID3D12Resource *dsImage;
  D3D12Descriptor *dsDescriptor;

  // Buffer used to copy colour and depth information
  ID3D12Resource *dstBuffer;
};

struct D3D12PixelHistoryValue
{
  // Max size is 4 component with 8 byte component width
  uint8_t color[32];
  union
  {
    uint32_t udepth;
    float fdepth;
  } depth;
  int8_t stencil;
  uint8_t padding[3 + 8];
};

struct D3D12EventInfo
{
  D3D12PixelHistoryValue premod;
  D3D12PixelHistoryValue postmod;
  uint8_t dsWithoutShaderDiscard[8];
  uint8_t padding[8];
  uint8_t dsWithShaderDiscard[8];
  uint8_t padding1[8];
};

struct D3D12PipelineReplacements
{
  ID3D12PipelineState *fixedShaderStencil;
  ID3D12PipelineState *originalShaderStencil;
};

namespace
{

bool IsDepthFormat(D3D12_RESOURCE_DESC desc, CompType typeCast)
{
  // TODO: This function might need to handle where the resource is typeless but is actually depth

  if(IsDepthFormat(desc.Format))
    return true;

  if(typeCast == CompType::Depth && (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
    return true;

  return false;
}

void ReplayDraw(ID3D12GraphicsCommandListX *cmd, const ActionDescription &action)
{
  // TODO: Once this is fully supported for indirect draws, this should be moved to WrappedID3D12Device
  if(action.drawIndex == 0)
  {
    if(action.flags & ActionFlags::Indexed)
      cmd->DrawIndexedInstanced(action.numIndices, action.numInstances, action.indexOffset,
                                action.baseVertex, action.instanceOffset);
    else
      cmd->DrawInstanced(action.numIndices, action.numInstances, action.vertexOffset,
                         action.instanceOffset);
  }
  else
  {
    // TODO: Support replay of single indirect draws
    RDCERR("Indirect draws are NYI with ReplayDraw");
  }
}

}

// Helper function to copy a single pixel out of a source texture, which will handle any texture
// type and binding type, doing any copying as needed. Writes the result to a given buffer UAV.
void D3D12DebugManager::PixelHistoryCopyPixel(ID3D12GraphicsCommandListX *cmd,
                                              ID3D12Resource *dstBuffer, D3D12CopyPixelParams &p,
                                              size_t offset)
{
  D3D12RenderState &state = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();
  D3D12RenderState prevState = state;

  state.pipe = GetResID(m_PixelHistoryCopyPso);
  state.compute.rootsig = GetResID(m_PixelHistoryCopySig);

  bool floatTex = false, uintTex = false, intTex = false;
  DXGI_FORMAT srvFormat = p.srcImageFormat;
  bool uintStencil = p.depthcopy && p.planeSlice == 1 &&
                     (p.srcImageFormat == DXGI_FORMAT_X24_TYPELESS_G8_UINT ||
                      p.srcImageFormat == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT);
  if(IsUIntFormat(p.srcImageFormat) || uintStencil)
  {
    uintTex = true;
    srvFormat = GetUIntTypedFormat(srvFormat);
  }
  else if(IsIntFormat(p.srcImageFormat))
  {
    intTex = true;
    srvFormat = GetSIntTypedFormat(srvFormat);
  }
  else
  {
    floatTex = true;
    srvFormat = GetFloatTypedFormat(srvFormat);
  }

  UINT srvIndex = 0;
  UINT uavIndex = 0;

  // SRV indices by type:
  //  0 - depth
  //  1 - stencil
  //  2 - depth MSAA
  //  3 - stencil MSAA
  //  4 - float
  //  5 - float MSAA
  //  6 - uint
  //  7 - uint MSAA
  //  8 - int
  //  9 - int MSAA

  // UAV indices by type:
  //  0 - depth
  //  1 - stencil
  //  2 - float
  //  3 - uint
  //  4 - int

  // Determine which SRV we will read from in the shader
  if(p.depthcopy)
  {
    srvIndex = p.planeSlice;
    if(p.multisampled)
      srvIndex += 2;

    // This should already be the SRV format for depth/stencil copies
    srvFormat = p.srcImageFormat;
  }
  else
  {
    if(floatTex)
      srvIndex = 4;
    else if(uintTex)
      srvIndex = 6;
    else if(intTex)
      srvIndex = 8;

    if(p.multisampled)
      srvIndex++;
  }

  // Determine which UAV we will write to in the shader
  if(p.depthcopy && p.planeSlice == 0)
    uavIndex = 0;
  else if(p.depthcopy && p.planeSlice == 1)
    uavIndex = 1;
  else if(floatTex)
    uavIndex = 2;
  else if(uintTex)
    uavIndex = 3;
  else
    uavIndex = 4;

  struct CopyPixelShaderInput
  {
    Vec4u src_coord;    // x, y, mip/sample, slice

    uint32_t dst_slot;
    uint32_t copy_depth;
    uint32_t copy_stencil;

    uint32_t multisampled;
    uint32_t is_float;
    uint32_t is_uint;
    uint32_t is_int;
  } inputData;

  inputData.src_coord = {p.x, p.y, p.multisampled ? p.sample : p.mip, p.arraySlice};
  inputData.multisampled = p.multisampled;
  inputData.is_float = floatTex;
  inputData.is_uint = uintTex;
  inputData.is_int = intTex;

  inputData.dst_slot = (uint32_t)(offset / sizeof(float));
  inputData.copy_depth = p.depthcopy && p.planeSlice == 0;
  inputData.copy_stencil = p.depthcopy && p.planeSlice == 1;

  // When copying a scratch buffer, we need to use a different SRV range from the heap
  CBVUAVSRVSlot srvStartSlot =
      p.scratchBuffer ? FIRST_PIXELHISTORY_SCRATCH_SRV : FIRST_PIXELHISTORY_SRV;
  D3D12_CPU_DESCRIPTOR_HANDLE srv = m_pDevice->GetDebugManager()->GetCPUHandle(srvStartSlot);
  D3D12_CPU_DESCRIPTOR_HANDLE uav =
      m_pDevice->GetDebugManager()->GetCPUHandle(FIRST_PIXELHISTORY_UAV);

  m_pDevice->GetDebugManager()->SetDescriptorHeaps(state.heaps, true, false);
  state.compute.sigelems = {
      D3D12RenderState::SignatureElement(
          eRootCBV, m_pDevice->GetDebugManager()->UploadConstants(&inputData, sizeof(inputData))),
      D3D12RenderState::SignatureElement(eRootTable, uav),
      D3D12RenderState::SignatureElement(eRootTable, srv),
  };

  srv.ptr += srvIndex * sizeof(D3D12Descriptor);

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.ViewDimension =
      p.multisampled ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Format = p.srcImageFormat;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  if(!p.multisampled)
  {
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.PlaneSlice = p.planeSlice;
  }
  m_pDevice->CreateShaderResourceView(p.srcImage, &srvDesc, srv);

  uav.ptr += uavIndex * sizeof(D3D12Descriptor);

  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.Buffer.NumElements = (UINT)(dstBuffer->GetDesc().Width / sizeof(float));
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.StructureByteStride = sizeof(float);
  m_pDevice->CreateUnorderedAccessView(dstBuffer, NULL, &uavDesc, uav);

  state.ApplyState(m_pDevice, cmd);
  cmd->Dispatch(1, 1, 1);

  state = prevState;
  state.ApplyState(m_pDevice, cmd);
}

// D3D12PixelHistoryShaderCache manages temporary shaders created for pixel history.
struct D3D12PixelHistoryShaderCache
{
  D3D12PixelHistoryShaderCache(WrappedID3D12Device *device, ID3DBlob *PersistentPrimIDPS,
                               ID3DBlob *PersistentPrimIDPSDxil, ID3DBlob *FixedColorPS,
                               ID3DBlob *FixedColorPSDxil)
      : m_pDevice(device),
        m_PrimIDPS(PersistentPrimIDPS),
        m_PrimIDPSDxil(PersistentPrimIDPSDxil),
        m_FixedColorPS(FixedColorPS),
        m_FixedColorPSDxil(FixedColorPSDxil)
  {
  }

  ~D3D12PixelHistoryShaderCache() {}

  // Returns a fragment shader that outputs a fixed color
  ID3DBlob *GetFixedColorShader(bool dxil) { return dxil ? m_FixedColorPSDxil : m_FixedColorPS; }

  // Returns a fragment shader that outputs primitive ID
  ID3DBlob *GetPrimitiveIdShader(bool dxil) { return dxil ? m_PrimIDPSDxil : m_PrimIDPS; }

  // TODO: This class should also manage any shader replacements needed during pixel history

private:
  WrappedID3D12Device *m_pDevice;

  ID3DBlob *m_PrimIDPS;
  ID3DBlob *m_PrimIDPSDxil;
  ID3DBlob *m_FixedColorPS;
  ID3DBlob *m_FixedColorPSDxil;
};

// D3D12PixelHistoryCallback is a generic D3D12ActionCallback that can be used
// for pixel history replays.
struct D3D12PixelHistoryCallback : public D3D12ActionCallback
{
  D3D12PixelHistoryCallback(WrappedID3D12Device *device, D3D12PixelHistoryShaderCache *shaderCache,
                            const D3D12PixelHistoryCallbackInfo &callbackInfo,
                            ID3D12QueryHeap *occlusionQueryHeap)
      : m_pDevice(device),
        m_ShaderCache(shaderCache),
        m_CallbackInfo(callbackInfo),
        m_OcclusionQueryHeap(occlusionQueryHeap)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = this;
  }

  virtual ~D3D12PixelHistoryCallback()
  {
    m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = NULL;
  }

protected:
  // Update the given scissor to just the pixel for which pixel history was requested.
  void ScissorToPixel(const D3D12_VIEWPORT &view, D3D12_RECT &scissor)
  {
    float fx = (float)m_CallbackInfo.x;
    float fy = (float)m_CallbackInfo.y;
    float y_start = view.TopLeftY;
    float y_end = view.TopLeftY + view.Height;
    if(view.Height < 0)
    {
      // Handle negative viewport which was added in Agility SDK 1.602.0
      y_start = view.TopLeftY + view.Height;
      y_end = view.TopLeftY;
    }

    if(fx < view.TopLeftX || fy < y_start || fx >= view.TopLeftX + view.Width || fy >= y_end)
    {
      scissor.left = scissor.top = scissor.right = scissor.bottom = 0;
    }
    else
    {
      scissor.left = m_CallbackInfo.x;
      scissor.top = m_CallbackInfo.y;
      scissor.right = scissor.left + 1;
      scissor.bottom = scissor.top + 1;
    }
  }

  // Intersects the originalScissor and newScissor and writes intersection to the newScissor.
  // newScissor always covers a single pixel, so if originalScissor does not touch that pixel
  // returns an empty scissor.
  void IntersectScissors(const D3D12_RECT &originalScissor, D3D12_RECT &newScissor)
  {
    RDCASSERT(newScissor.right == newScissor.left + 1);
    RDCASSERT(newScissor.bottom == newScissor.top + 1);
    if(originalScissor.left > newScissor.left || originalScissor.right < newScissor.right ||
       originalScissor.top > newScissor.top || originalScissor.bottom < newScissor.bottom)
    {
      // Scissor does not touch our target pixel, make it empty
      newScissor.left = newScissor.top = newScissor.right = newScissor.bottom = 0;
    }
  }

  // ModifyPSOForStencilIncrement modifies the provided pipeDesc, by disabling depth test
  // and write, stencil is set to always pass and increment, scissor is set to scissor around
  // the target pixel, and all color modifications are disabled.
  // Optionally disables other tests like culling, depth bounds.
  void ModifyPSOForStencilIncrement(uint32_t eid, D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &pipeDesc,
                                    bool disableTests)
  {
    pipeDesc.DepthStencilState.DepthEnable = FALSE;
    pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    if(disableTests)
    {
      pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      pipeDesc.RasterizerState.DepthClipEnable = FALSE;
      pipeDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;
    }

    // TODO: Get from callbackinfo/pixelhistoryresources?
    pipeDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

    // TODO: If the original depth buffer doesn't have stencil, this will not work as expected.
    // We will need to detect that and switch to a DSV with a stencil for some pixel history passes.

    // Set up the stencil state.
    {
      pipeDesc.DepthStencilState.StencilEnable = TRUE;
      pipeDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      pipeDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_INCR_SAT;
      pipeDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR_SAT;
      pipeDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_INCR_SAT;
      pipeDesc.DepthStencilState.FrontFace.StencilReadMask = 0xff;
      pipeDesc.DepthStencilState.FrontFace.StencilWriteMask = 0xff;
      pipeDesc.DepthStencilState.BackFace = pipeDesc.DepthStencilState.FrontFace;
      // Stencil ref is set separately from the PSO
    }

    // Narrow on the specific pixel and sample.
    {
      pipeDesc.SampleMask = m_CallbackInfo.sampleMask;
    }

    // Turn off all color modifications.
    {
      for(uint32_t i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        pipeDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = 0;
    }
  }

  void CopyImagePixel(ID3D12GraphicsCommandListX *cmd, D3D12CopyPixelParams &p, size_t offset)
  {
    uint32_t baseMip = m_CallbackInfo.targetSubresource.mip;
    uint32_t baseSlice = m_CallbackInfo.targetSubresource.slice;

    // The images that are created specifically for evaluating pixel history are
    // already based on the target mip/slice
    if(p.srcImage == m_CallbackInfo.colorImage || p.srcImage == m_CallbackInfo.dsImage)
    {
      // TODO: Is this always true when we call CopyImagePixel? Also need to test this case with MSAA
      baseMip = 0;
      baseSlice = 0;
    }

    // For pipeline barriers.
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = p.srcImage;
    barrier.Transition.StateBefore = p.srcImageState;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.Subresource =
        D3D12CalcSubresource(baseMip, baseSlice, p.planeSlice, m_CallbackInfo.targetDesc.MipLevels,
                             m_CallbackInfo.targetDesc.DepthOrArraySize);

    // Multi-sampled images can't call CopyTextureRegion for a single sample, so instead
    // copy using a compute shader into a staging image first
    if(p.multisampled)
    {
      // TODO: Is a resource transition needed here?
      m_pDevice->GetDebugManager()->PixelHistoryCopyPixel(cmd, m_CallbackInfo.dstBuffer, p, offset);
    }
    else
    {
      cmd->ResourceBarrier(1, &barrier);

      D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};

      src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      src.pResource = p.srcImage;
      src.SubresourceIndex = barrier.Transition.Subresource;

      // Copy into a buffer, but treat the footprint as the same format as the target image
      uint32_t elementSize = GetByteSize(0, 0, 0, p.copyFormat, 0);

      dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      dst.pResource = m_CallbackInfo.dstBuffer;
      dst.PlacedFootprint.Offset = 0;
      dst.PlacedFootprint.Footprint.Width =
          (UINT)m_CallbackInfo.dstBuffer->GetDesc().Width / elementSize;
      dst.PlacedFootprint.Footprint.Height = 1;
      dst.PlacedFootprint.Footprint.Depth = 1;
      dst.PlacedFootprint.Footprint.Format = p.copyFormat;
      dst.PlacedFootprint.Footprint.RowPitch = (UINT)m_CallbackInfo.dstBuffer->GetDesc().Width;

      D3D12_BOX srcBox = {};
      srcBox.left = p.x;
      srcBox.top = p.y;
      srcBox.right = srcBox.left + 1;
      srcBox.bottom = srcBox.top + 1;
      srcBox.front = 0;
      srcBox.back = 1;

      // We need to apply the offset here (measured in number of elements) rather than using
      // PlacedFootprint.Offset (measured in bytes) because the latter must be a multiple of 512
      RDCASSERT((offset % elementSize) == 0);
      cmd->CopyTextureRegion(&dst, (UINT)(offset / elementSize), 0, 0, &src, &srcBox);

      std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
      cmd->ResourceBarrier(1, &barrier);
    }
  }

  // Returns the render target index that corresponds to the target image for pixel history.
  uint32_t GetPixelHistoryRenderTargetIndex(const D3D12RenderState &renderstate)
  {
    ResourceId targetId = GetResID(m_CallbackInfo.targetImage);
    if(renderstate.dsv.GetResResourceId() == targetId)
      return 0;

    uint32_t targetIndex = 0;
    for(uint32_t i = 0; i < renderstate.rts.size(); i++)
    {
      ResourceId id = renderstate.rts[i].GetResResourceId();
      if(id == targetId)
      {
        targetIndex = i;
        break;
      }
    }

    return targetIndex;
  }

  WrappedID3D12Device *m_pDevice;
  D3D12PixelHistoryShaderCache *m_ShaderCache;
  D3D12PixelHistoryCallbackInfo m_CallbackInfo;
  ID3D12QueryHeap *m_OcclusionQueryHeap;
};

// D3D12OcclusionCallback callback is used to determine which draw events might
// have modified the pixel by doing an occlusion query.
struct D3D12OcclusionCallback : public D3D12PixelHistoryCallback
{
  D3D12OcclusionCallback(WrappedID3D12Device *device, D3D12PixelHistoryShaderCache *shaderCache,
                         const D3D12PixelHistoryCallbackInfo &callbackInfo,
                         ID3D12QueryHeap *occlusionQueryHeap, const rdcarray<EventUsage> &allEvents)
      : D3D12PixelHistoryCallback(device, shaderCache, callbackInfo, occlusionQueryHeap)
  {
    for(size_t i = 0; i < allEvents.size(); i++)
      m_Events.push_back(allEvents[i].eventId);
  }

  ~D3D12OcclusionCallback()
  {
    for(auto it = m_PipeCache.begin(); it != m_PipeCache.end(); ++it)
      SAFE_RELEASE(it->second);
  }

  void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(!m_Events.contains(eid))
      return;

    D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Replaying event %u", eid));

    m_SavedState = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();
    D3D12RenderState pipeState = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();

    pipeState.rts.clear();
    pipeState.dsv = *m_CallbackInfo.dsDescriptor;
    ID3D12PipelineState *pso =
        GetPixelOcclusionPipeline(eid, pipeState, GetPixelHistoryRenderTargetIndex(pipeState));

    pipeState.pipe = GetResID(pso);
    // set the scissor
    for(uint32_t i = 0; i < pipeState.views.size(); i++)
      ScissorToPixel(pipeState.views[i], pipeState.scissors[i]);
    pipeState.stencilRefFront = 0;
    pipeState.stencilRefBack = 0;

    pipeState.ApplyState(m_pDevice, cmd);

    uint32_t occlIndex = (uint32_t)m_OcclusionQueries.size();
    cmd->BeginQuery(m_OcclusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, occlIndex);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(!m_Events.contains(eid))
      return false;

    uint32_t occlIndex = (uint32_t)m_OcclusionQueries.size();
    cmd->EndQuery(m_OcclusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, occlIndex);
    m_OcclusionQueries.insert(std::make_pair(eid, occlIndex));

    m_SavedState.ApplyState(m_pDevice, cmd);
    return false;
  }

  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}
  void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}
  void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias) {}

  void FetchOcclusionResults()
  {
    if(m_OcclusionQueries.size() == 0)
      return;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufDesc;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Alignment = 0;
    bufDesc.Width = sizeof(uint64_t) * m_OcclusionQueries.size();
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource *readbackBuf = NULL;
    HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                    D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                    __uuidof(ID3D12Resource), (void **)&readbackBuf);
    m_pDevice->CheckHRESULT(hr);
    if(FAILED(hr))
    {
      RDCERR("Failed to create query readback buffer HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    ID3D12GraphicsCommandListX *list = m_pDevice->GetNewList();
    if(!list)
      return;

    list->ResolveQueryData(m_OcclusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, 0,
                           (UINT)m_OcclusionQueries.size(), readbackBuf, 0);

    list->Close();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();

    D3D12_RANGE range;
    range.Begin = 0;
    range.End = (SIZE_T)bufDesc.Width;

    uint64_t *data;
    hr = readbackBuf->Map(0, &range, (void **)&data);
    m_pDevice->CheckHRESULT(hr);
    if(FAILED(hr))
    {
      RDCERR("Failed to map query heap data HRESULT: %s", ToStr(hr).c_str());
      SAFE_RELEASE(readbackBuf);
      return;
    }

    m_OcclusionResults.resize(m_OcclusionQueries.size());
    for(size_t i = 0; i < m_OcclusionResults.size(); ++i)
      m_OcclusionResults[i] = data[i];

    readbackBuf->Unmap(0, NULL);
    SAFE_RELEASE(readbackBuf);
  }

  uint64_t GetOcclusionResult(uint32_t eventId)
  {
    auto it = m_OcclusionQueries.find(eventId);
    if(it == m_OcclusionQueries.end())
      return 0;
    RDCASSERT(it->second < m_OcclusionResults.size());
    return m_OcclusionResults[it->second];
  }

private:
  ID3D12PipelineState *GetPixelOcclusionPipeline(uint32_t eid, D3D12RenderState &state,
                                                 uint32_t outputIndex)
  {
    // TODO: outputIndex is unused. Either we need to select a fixed color shader that writes to the
    // preferred RT, or use RenderTargetWriteMask in the blend desc to mask out unrelated RTs
    auto it = m_PipeCache.find(state.pipe);
    if(it != m_PipeCache.end())
      return it->second;

    WrappedID3D12PipelineState *origPSO =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(state.pipe);
    if(origPSO == NULL)
    {
      RDCERR("Failed to retrieve original PSO for pixel history.");
      return NULL;
    }

    D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
    origPSO->Fill(pipeDesc);

    ModifyPSOForStencilIncrement(eid, pipeDesc, true);

    bool dxil =
        DXBC::DXBCContainer::CheckForDXIL(pipeDesc.VS.pShaderBytecode, pipeDesc.VS.BytecodeLength);

    ID3DBlob *psBlob =
        m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::HIGHLIGHT, dxil);
    if(psBlob == NULL)
    {
      RDCERR("Failed to create fixed color shader for pixel history.");
      return NULL;
    }

    pipeDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
    pipeDesc.PS.BytecodeLength = psBlob->GetBufferSize();

    ID3D12PipelineState *pso = NULL;
    HRESULT hr = m_pDevice->CreatePipeState(pipeDesc, &pso);
    if(FAILED(hr))
    {
      RDCERR("Failed to create PSO for pixel history.");
      SAFE_RELEASE(psBlob);
      return NULL;
    }

    m_PipeCache.insert(std::make_pair(state.pipe, pso));
    SAFE_RELEASE(psBlob);
    return pso;
  }

private:
  D3D12RenderState m_SavedState;
  std::map<ResourceId, ID3D12PipelineState *> m_PipeCache;
  rdcarray<uint32_t> m_Events;

  // Key is event ID, and value is an index of where the occlusion result.
  std::map<uint32_t, uint32_t> m_OcclusionQueries;
  rdcarray<uint64_t> m_OcclusionResults;
};

struct D3D12ColorAndStencilCallback : public D3D12PixelHistoryCallback
{
  D3D12ColorAndStencilCallback(WrappedID3D12Device *device, D3D12PixelHistoryShaderCache *shaderCache,
                               const D3D12PixelHistoryCallbackInfo &callbackInfo,
                               const rdcarray<uint32_t> &events)
      : D3D12PixelHistoryCallback(device, shaderCache, callbackInfo, NULL), m_Events(events)
  {
  }

  ~D3D12ColorAndStencilCallback()
  {
    for(auto it = m_PipeCache.begin(); it != m_PipeCache.end(); ++it)
    {
      SAFE_RELEASE(it->second.fixedShaderStencil);
      SAFE_RELEASE(it->second.originalShaderStencil);
    }
  }

  void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(!m_Events.contains(eid))
      return;

    D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Replaying event %u", eid));

    m_SavedState = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();
    D3D12RenderState &pipeState = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();

    // Get pre-modification values
    size_t storeOffset = m_EventIndices.size() * sizeof(D3D12EventInfo);

    CopyPixel(eid, cmd, storeOffset + offsetof(struct D3D12EventInfo, premod));

    {
      D3D12PipelineReplacements replacements = GetPipelineReplacements(eid, pipeState);

      // Set scissor to only the pixel we're getting history for
      for(uint32_t i = 0; i < pipeState.views.size(); i++)
        ScissorToPixel(pipeState.views[i], pipeState.scissors[i]);

      // Replay the draw with the original shader, but with state changed to
      // count fragments with stencil
      pipeState.rts.clear();
      pipeState.dsv = *m_CallbackInfo.dsDescriptor;
      pipeState.stencilRefFront = 0;
      pipeState.stencilRefBack = 0;
      pipeState.pipe = GetResID(replacements.originalShaderStencil);
      pipeState.ApplyState(m_pDevice, cmd);

      ReplayDraw(cmd, eid, true);

      D3D12CopyPixelParams params = {};
      params.scratchBuffer = true;
      params.srcImage = m_CallbackInfo.dsImage;
      params.srcImageState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
      params.srcImageFormat = GetDepthSRVFormat(m_CallbackInfo.dsImage->GetDesc().Format, 1);
      params.copyFormat = DXGI_FORMAT_R8_TYPELESS;
      params.x = m_CallbackInfo.x;
      params.y = m_CallbackInfo.y;
      params.sample = m_CallbackInfo.targetSubresource.sample;
      params.mip = m_CallbackInfo.targetSubresource.mip;
      params.arraySlice = m_CallbackInfo.targetSubresource.slice;
      params.depthcopy = true;
      params.planeSlice = 1;
      params.multisampled = m_CallbackInfo.targetDesc.SampleDesc.Count > 1;
      CopyImagePixel(cmd, params,
                     storeOffset + offsetof(struct D3D12EventInfo, dsWithoutShaderDiscard));

      // TODO: In between draws, do we need to reset the depth/stencil value?

      // Replay the draw with a fixed color shader that never discards, and stencil
      // increment to count number of fragments. We will get the number of fragments
      // not accounting for shader discard.
      pipeState.pipe = GetResID(replacements.fixedShaderStencil);
      pipeState.ApplyState(m_pDevice, cmd);

      ReplayDraw(cmd, eid, true);

      CopyImagePixel(cmd, params, storeOffset + offsetof(struct D3D12EventInfo, dsWithShaderDiscard));
    }

    // Restore the state.
    pipeState = m_SavedState;
    pipeState.ApplyState(m_pDevice, cmd);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(!m_Events.contains(eid))
      return false;

    // Get post-modification values
    size_t storeOffset =
        m_EventIndices.size() * sizeof(D3D12EventInfo) + offsetof(struct D3D12EventInfo, postmod);
    CopyPixel(eid, cmd, storeOffset);

    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));

    D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Finished replaying event %u", eid));

    return false;
  }
  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}

  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(!m_Events.contains(eid))
      return;

    D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Replaying event %u", eid));

    size_t storeOffset =
        m_EventIndices.size() * sizeof(D3D12EventInfo) + offsetof(struct D3D12EventInfo, premod);
    CopyPixel(eid, cmd, storeOffset);
  }
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(!m_Events.contains(eid))
      return false;
    size_t storeOffset =
        m_EventIndices.size() * sizeof(D3D12EventInfo) + offsetof(struct D3D12EventInfo, postmod);
    CopyPixel(eid, cmd, storeOffset);
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));

    D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Finished replaying event %u", eid));

    return false;
  }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}

  void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd)
  {
    PreDispatch(eid, cmd);
  }
  bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd)
  {
    return PostDispatch(eid, cmd);
  }
  void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}

  void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    RDCWARN(
        "Alised events are not supported, results might be inaccurate. "
        "Primary event id: %u, "
        "alias: %u.",
        primary, alias);
  }

  int32_t GetEventIndex(uint32_t eventId)
  {
    auto it = m_EventIndices.find(eventId);
    if(it == m_EventIndices.end())
      // Most likely a secondary command buffer event for which there is no information.
      return -1;
    RDCASSERT(it != m_EventIndices.end());
    return (int32_t)it->second;
  }

  DXGI_FORMAT GetDepthFormat(uint32_t eventId)
  {
    if(IsDepthFormat(m_CallbackInfo.targetDesc.Format))
      return m_CallbackInfo.targetDesc.Format;
    auto it = m_DepthFormats.find(eventId);
    if(it == m_DepthFormats.end())
      return DXGI_FORMAT_UNKNOWN;
    return it->second;
  }

private:
  void CopyPixel(uint32_t eid, ID3D12GraphicsCommandListX *cmd, size_t offset)
  {
    D3D12CopyPixelParams targetCopyParams = {};
    targetCopyParams.scratchBuffer = false;
    targetCopyParams.srcImage = m_CallbackInfo.targetImage;
    targetCopyParams.srcImageFormat = m_CallbackInfo.targetDesc.Format;
    targetCopyParams.copyFormat = m_CallbackInfo.targetDesc.Format;
    targetCopyParams.x = m_CallbackInfo.x;
    targetCopyParams.y = m_CallbackInfo.y;
    targetCopyParams.sample = m_CallbackInfo.targetSubresource.sample;
    targetCopyParams.mip = m_CallbackInfo.targetSubresource.mip;
    targetCopyParams.arraySlice = m_CallbackInfo.targetSubresource.slice;
    targetCopyParams.srcImageState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    targetCopyParams.multisampled = (m_CallbackInfo.targetDesc.SampleDesc.Count != 1);
    if(IsDepthFormat(m_CallbackInfo.targetDesc, m_CallbackInfo.compType))
    {
      targetCopyParams.srcImageState = m_SavedState.dsv.GetDSV().Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH
                                           ? D3D12_RESOURCE_STATE_DEPTH_READ
                                           : D3D12_RESOURCE_STATE_DEPTH_WRITE;
      targetCopyParams.srcImageFormat = GetDepthSRVFormat(m_CallbackInfo.targetDesc.Format, 0);
      targetCopyParams.copyFormat = DXGI_FORMAT_R32_TYPELESS;
      offset += offsetof(struct D3D12PixelHistoryValue, depth);
    }

    CopyImagePixel(cmd, targetCopyParams, offset);

    // If the target image is a depth/stencil view, we already copied the value above.
    if(IsDepthFormat(m_CallbackInfo.targetDesc.Format))
      return;

    // Get the bound depth format for this event
    ResourceId resId = m_SavedState.dsv.GetResResourceId();
    if(resId != ResourceId())
    {
      WrappedID3D12Resource *depthImage =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Resource>(resId);

      // TODO: What about D16? Still copy as 32 bit?
      DXGI_FORMAT depthFormat = m_SavedState.dsv.GetDSV().Format;

      D3D12CopyPixelParams depthCopyParams = targetCopyParams;
      depthCopyParams.srcImage = depthImage;
      depthCopyParams.srcImageFormat = GetDepthSRVFormat(depthImage->GetDesc().Format, 0);
      depthCopyParams.copyFormat = DXGI_FORMAT_R32_TYPELESS;
      depthCopyParams.depthcopy = true;
      depthCopyParams.srcImageState = m_SavedState.dsv.GetDSV().Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH
                                          ? D3D12_RESOURCE_STATE_DEPTH_READ
                                          : D3D12_RESOURCE_STATE_DEPTH_WRITE;
      CopyImagePixel(cmd, depthCopyParams, offset + offsetof(struct D3D12PixelHistoryValue, depth));

      if(IsDepthAndStencilFormat(depthFormat))
      {
        depthCopyParams.srcImageFormat = GetDepthSRVFormat(depthImage->GetDesc().Format, 1);
        depthCopyParams.copyFormat = DXGI_FORMAT_R8_TYPELESS;
        depthCopyParams.planeSlice = 1;
        depthCopyParams.srcImageState =
            m_SavedState.dsv.GetDSV().Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL
                ? D3D12_RESOURCE_STATE_DEPTH_READ
                : D3D12_RESOURCE_STATE_DEPTH_WRITE;
        CopyImagePixel(cmd, depthCopyParams,
                       offset + offsetof(struct D3D12PixelHistoryValue, stencil));
      }

      m_DepthFormats.insert(std::make_pair(eid, depthFormat));
    }
  }

  // Executes a single draw defined by the eventId
  void ReplayDraw(ID3D12GraphicsCommandListX *cmd, uint32_t eventId, bool clear = false)
  {
    if(clear)
    {
      D3D12_RECT clearRect;
      clearRect.left = m_CallbackInfo.x;
      clearRect.top = m_CallbackInfo.y;
      clearRect.right = clearRect.left + 1;
      clearRect.bottom = clearRect.top + 1;
      cmd->ClearDepthStencilView(m_pDevice->GetDebugManager()->GetCPUHandle(PIXEL_HISTORY_DSV),
                                 D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 1, &clearRect);
    }

    const ActionDescription *action = m_pDevice->GetAction(eventId);
    ::ReplayDraw(cmd, *action);
  }

  // GetPipelineReplacements creates pipeline replacements that disable all tests,
  // and use either fixed or original fragment shader, and shaders that don't have side effects.
  D3D12PipelineReplacements GetPipelineReplacements(uint32_t eid, const D3D12RenderState &state)
  {
    // The map does not keep track of the event ID, event ID is only used to figure out
    // which shaders need to be modified. Those flags are based on the shaders bound,
    // so in theory all events should share those flags if they are using the same pipeline.
    auto pipeIt = m_PipeCache.find(state.pipe);
    if(pipeIt != m_PipeCache.end())
      return pipeIt->second;

    D3D12PipelineReplacements replacements = {};

    WrappedID3D12PipelineState *origPSO =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(state.pipe);

    D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC desc;
    origPSO->Fill(desc);

    bool dxil = DXBC::DXBCContainer::CheckForDXIL(desc.VS.pShaderBytecode, desc.VS.BytecodeLength);
    ID3DBlob *psBlob =
        m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::HIGHLIGHT, dxil);
    if(psBlob == NULL)
    {
      RDCERR("Failed to create fixed color shader for pixel history.");
      return replacements;
    }

    // Modify state so that we increment stencil and skip depth testing
    desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR_SAT;
    desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    desc.DepthStencilState.FrontFace.StencilReadMask = 0xff;
    desc.DepthStencilState.FrontFace.StencilWriteMask = 0xff;
    desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;
    desc.DepthStencilState.StencilEnable = TRUE;
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthBoundsTestEnable = FALSE;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    // TODO: Get DSVFormat from callbackinfo/pixelhistoryresources?
    // TODO: Any other state need updated?

    // Create the PSO for testing with the original shader, to account for shader discards
    HRESULT hr = m_pDevice->CreatePipeState(desc, &replacements.originalShaderStencil);
    if(FAILED(hr))
    {
      RDCERR("Failed to create PSO for pixel history.");
      SAFE_RELEASE(psBlob);
      return replacements;
    }

    // Create the PSO for testing with a fixed color shader, to count fragments
    // without discards
    desc.PS.pShaderBytecode = psBlob->GetBufferPointer();
    desc.PS.BytecodeLength = psBlob->GetBufferSize();
    hr = m_pDevice->CreatePipeState(desc, &replacements.fixedShaderStencil);
    if(FAILED(hr))
    {
      RDCERR("Failed to create PSO for pixel history.");
      SAFE_RELEASE(psBlob);
      SAFE_RELEASE(replacements.originalShaderStencil);
      return replacements;
    }

    m_PipeCache.insert(std::make_pair(state.pipe, replacements));
    SAFE_RELEASE(psBlob);
    return replacements;
  }

  D3D12RenderState m_SavedState;
  std::map<ResourceId, D3D12PipelineReplacements> m_PipeCache;
  rdcarray<uint32_t> m_Events;
  // Key is event ID, and value is an index of where the event data is stored.
  std::map<uint32_t, size_t> m_EventIndices;
  std::map<uint32_t, DXGI_FORMAT> m_DepthFormats;
};

bool D3D12DebugManager::PixelHistorySetupResources(D3D12PixelHistoryResources &resources,
                                                   WrappedID3D12Resource *targetImage,
                                                   const D3D12_RESOURCE_DESC &desc,
                                                   uint32_t numEvents)
{
  D3D12MarkerRegion region(
      m_pDevice->GetQueue()->GetReal(),
      StringFormat::Fmt("PixelHistorySetupResources %ux%ux%u %s %ux MSAA", desc.Width, desc.Height,
                        desc.DepthOrArraySize, ToStr(desc.Format).c_str(), desc.SampleDesc.Count));

  ResourceFormat targetFmt = MakeResourceFormat(desc.Format);

  ID3D12Resource *colorImage;
  ID3D12Resource *dsImage;
  ID3D12Resource *dstBuffer;

  HRESULT hr = S_OK;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC imageDesc = desc;
  imageDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  imageDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  imageDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &imageDesc,
                                          D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                          __uuidof(ID3D12Resource), (void **)&colorImage);
  m_pDevice->CheckHRESULT(hr);
  if(FAILED(hr))
  {
    RDCERR("Failed to create scratch render target for pixel history: %s", ToStr(hr).c_str());
    return false;
  }
  colorImage->SetName(L"Pixel History Color Image");

  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
  rtvDesc.Format = imageDesc.Format;
  rtvDesc.ViewDimension = imageDesc.SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS
                                                         : D3D12_RTV_DIMENSION_TEXTURE2D;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_pDevice->GetDebugManager()->GetCPUHandle(PIXEL_HISTORY_RTV);
  m_pDevice->CreateRenderTargetView(colorImage, &rtvDesc, rtv);

  imageDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
  imageDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &imageDesc,
                                          D3D12_RESOURCE_STATE_DEPTH_WRITE, NULL,
                                          __uuidof(ID3D12Resource), (void **)&dsImage);
  m_pDevice->CheckHRESULT(hr);
  if(FAILED(hr))
  {
    RDCERR("Failed to create scratch depth stencil for pixel history: %s", ToStr(hr).c_str());
    SAFE_RELEASE(colorImage);

    return false;
  }
  dsImage->SetName(L"Pixel History Depth Stencil");

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
  dsvDesc.Format = imageDesc.Format;
  dsvDesc.ViewDimension = imageDesc.SampleDesc.Count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS
                                                         : D3D12_DSV_DIMENSION_TEXTURE2D;
  D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_pDevice->GetDebugManager()->GetCPUHandle(PIXEL_HISTORY_DSV);
  m_pDevice->CreateDepthStencilView(dsImage, &dsvDesc, dsv);

  // With a readback heap, buffers cannot be created with the UAV flag. As a workaround, a custom heap
  // can be created with the same properties as a readback heap, and then the UAV flag is permitted.
  D3D12_HEAP_PROPERTIES readbackHeapProps =
      m_pDevice->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_READBACK);

  D3D12_RESOURCE_DESC bufDesc;
  bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufDesc.Alignment = 0;
  bufDesc.Height = 1;
  bufDesc.DepthOrArraySize = 1;
  bufDesc.MipLevels = 1;
  bufDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufDesc.SampleDesc.Count = 1;
  bufDesc.SampleDesc.Quality = 0;
  bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  bufDesc.Width = AlignUp((uint32_t)(numEvents * sizeof(D3D12EventInfo)), 4096U);

  hr = m_pDevice->CreateCommittedResource(&readbackHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                          D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                          __uuidof(ID3D12Resource), (void **)&dstBuffer);
  m_pDevice->CheckHRESULT(hr);
  if(FAILED(hr))
  {
    RDCERR("Failed to create readback buffer for pixel history: %s", ToStr(hr).c_str());
    SAFE_RELEASE(colorImage);
    SAFE_RELEASE(dsImage);
    return false;
  }
  dstBuffer->SetName(L"Pixel History DstBuffer");

  resources.colorImage = colorImage;
  resources.colorDescriptor = GetWrapped(rtv);

  resources.dsImage = dsImage;
  resources.dsDescriptor = GetWrapped(dsv);

  resources.dstBuffer = dstBuffer;

  return true;
}

bool D3D12DebugManager::PixelHistoryDestroyResources(D3D12PixelHistoryResources &r)
{
  SAFE_RELEASE(r.colorImage);
  r.colorDescriptor = NULL;
  SAFE_RELEASE(r.dsImage);
  r.dsDescriptor = NULL;
  SAFE_RELEASE(r.dstBuffer);

  return true;
}
