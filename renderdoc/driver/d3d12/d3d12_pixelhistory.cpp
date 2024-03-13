/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2024 Baldur Karlsson
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

/*
 * The general algorithm for pixel history is this:
 *
 * We get passed a list of all events that could have touched the target texture
 * We replay all events (up to and including the last event that could have
 * touched the target texture) with a number of callbacks:
 *
 * - First callback: Occlusion callback (D3D12OcclusionCallback)
 * This callback performs an occlusion query around each draw event that was
 * passed in. Execute the draw with a modified pipeline that disables most tests,
 * and uses a fixed color fragment shader, so that we get a non 0 occlusion
 * result even if a test failed for the event.
 *
 * After this callback we collect all events where occlusion result > 0 and all
 * other non-draw events (copy, clear, resolve). We also filter out events where
 * the image view used did not overlap in the array layer.
 * The callbacks below will only deal with these events.
 *
 * - Second callback: Color and stencil callback (D3D12ColorAndStencilCallback)
 * This callback retrieves color/depth values before and after each event, and
 * uses a stencil increment to count the number of fragments for each event.
 * We then copy color information and associated depth value, and resume a
 * render pass, if there was one. Before each draw event we also execute the same
 * draw twice with a stencil increment state: 1) with a fixed color fragment shader
 * to count the number of fragments not accounting for shader discard, 2) with the
 * original fragment shader to count the number of fragments accounting for shader discard.
 *
 * - Third callback: Tests failed callback (D3D12TestsFailedCallback)
 * This callback is used to determine which tests (culling, depth, stencil, etc)
 * failed (if any) for each draw event. This replays each draw event a number of times
 * with an occlusion query for each test that might have failed (leaves the test
 * under question in the original state, and disables all tests that come after).
 *
 * At this point we retrieve the stencil results that represent the number of fragments,
 * and duplicate events that have multiple fragments.
 *
 * - Fourth callback: Per fragment callback (D3D12PixelHistoryPerFragmentCallback)
 * This callback is used to get per fragment data for each event and fragment
 * (primitive ID, shader output value, post event value for each fragment).
 * For each fragment the draw is replayed 3 times:
 * 1) with a fragment shader that outputs primitive ID only
 * 2) with blending OFF, to get shader output value
 * 3) with blending ON, to get post modification value
 * For each such replay we set the stencil reference to the fragment number and set the
 * stencil compare to equal, so it passes for that particular fragment only.
 *
 * - Fifth callback: Discarded fragments callback
 * (D3D12PixelHistoryDiscardedFragmentsCallback)
 * This callback is used to determine which individual fragments were discarded in a fragment
 * shader. Only runs for the events where the number of fragments accounting for shader
 * discard is less that the number of fragments not accounting for shader discard.
 * This replays the particular fragment with an occlusion query.
 *
 * We slot the per-fragment data correctly accounting for the fragments that were discarded.
 */

#include "core/settings.h"
#include "driver/dxgi/dxgi_common.h"
#include "maths/formatpacking.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_replay.h"
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
  uint8_t padding1[24];
};

struct D3D12PerFragmentInfo
{
  // primitive ID is copied from a R32G32B32A32 texture.
  int32_t primitiveID;
  uint32_t padding[3];
  D3D12PixelHistoryValue shaderOut;
  D3D12PixelHistoryValue postMod;
};

struct D3D12PipelineReplacements
{
  ID3D12PipelineState *fixedShaderStencil;
  ID3D12PipelineState *originalShaderStencil;
};

enum D3D12PixelHistoryTests : uint32_t
{
  TestEnabled_DepthClipping = 1 << 0,
  TestEnabled_Culling = 1 << 1,
  TestEnabled_Scissor = 1 << 2,    // Scissor test always enabled with D3D12
  TestEnabled_SampleMask = 1 << 3,
  TestEnabled_DepthBounds = 1 << 4,
  TestEnabled_StencilTesting = 1 << 5,
  TestEnabled_DepthTesting = 1 << 6,
  TestEnabled_FragmentDiscard = 1 << 7,

  Blending_Enabled = 1 << 8,
  UnboundFragmentShader = 1 << 9,
  TestMustFail_Scissor = 1 << 11,
  TestMustPass_Scissor = 1 << 12,
  TestMustFail_DepthTesting = 1 << 13,
  TestMustFail_StencilTesting = 1 << 14,
  TestMustFail_SampleMask = 1 << 15,

  DepthTest_Shift = 29,
  DepthTest_Always = 0U << DepthTest_Shift,
  DepthTest_Never = 1U << DepthTest_Shift,
  DepthTest_Equal = 2U << DepthTest_Shift,
  DepthTest_NotEqual = 3U << DepthTest_Shift,
  DepthTest_Less = 4U << DepthTest_Shift,
  DepthTest_LessEqual = 5U << DepthTest_Shift,
  DepthTest_Greater = 6U << DepthTest_Shift,
  DepthTest_GreaterEqual = 7U << DepthTest_Shift,
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

DXGI_FORMAT GetDepthCopyFormat(DXGI_FORMAT format)
{
  if(format == DXGI_FORMAT_D16_UNORM)
    return DXGI_FORMAT_R16_TYPELESS;
  return DXGI_FORMAT_R32_TYPELESS;
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
                               ID3DBlob *PersistentPrimIDPSDxil,
                               ID3DBlob *FixedColorPS[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT],
                               ID3DBlob *FixedColorPSDxil[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT])
      : m_pDevice(device), m_PrimIDPS(PersistentPrimIDPS), m_PrimIDPSDxil(PersistentPrimIDPSDxil)
  {
    for(int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
      m_FixedColorPS[i] = FixedColorPS[i];
      m_FixedColorPSDxil[i] = FixedColorPSDxil[i];
    }
  }

  ~D3D12PixelHistoryShaderCache() {}

  // Returns a fragment shader that outputs a fixed color
  ID3DBlob *GetFixedColorShader(bool dxil, int outputIndex)
  {
    return dxil ? m_FixedColorPSDxil[outputIndex] : m_FixedColorPS[outputIndex];
  }

  // Returns a fragment shader that outputs primitive ID
  ID3DBlob *GetPrimitiveIdShader(bool dxil) { return dxil ? m_PrimIDPSDxil : m_PrimIDPS; }

  // TODO: This class should also manage any shader replacements needed during pixel history

private:
  WrappedID3D12Device *m_pDevice;

  ID3DBlob *m_PrimIDPS;
  ID3DBlob *m_PrimIDPSDxil;
  ID3DBlob *m_FixedColorPS[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
  ID3DBlob *m_FixedColorPSDxil[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
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

  bool IsPSOUsingDXIL(D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &pipeDesc)
  {
    // Check early stages first, for both VS and MS, but use PS as a fallback. If any shader stage
    // uses DXIL, they all need to use DXIL.
    return DXBC::DXBCContainer::CheckForDXIL(pipeDesc.VS.pShaderBytecode, pipeDesc.VS.BytecodeLength) ||
           DXBC::DXBCContainer::CheckForDXIL(pipeDesc.MS.pShaderBytecode, pipeDesc.MS.BytecodeLength) ||
           DXBC::DXBCContainer::CheckForDXIL(pipeDesc.PS.pShaderBytecode, pipeDesc.PS.BytecodeLength);
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
    bool copy3d = m_CallbackInfo.targetDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    uint32_t baseSlice = m_CallbackInfo.targetSubresource.slice;
    uint32_t arraySize = m_CallbackInfo.targetDesc.DepthOrArraySize;
    uint32_t depthIndex = 0;

    // The images that are created specifically for evaluating pixel history are
    // already based on the target mip/slice
    if(p.srcImage == m_CallbackInfo.colorImage || p.srcImage == m_CallbackInfo.dsImage)
    {
      // TODO: Is this always true when we call CopyImagePixel? Also need to test this case with MSAA
      baseMip = 0;
      baseSlice = 0;
      copy3d = false;
    }
    else if(copy3d)
    {
      baseSlice = 0;
      arraySize = 1;
      depthIndex = m_CallbackInfo.targetSubresource.slice;
    }

    // For pipeline barriers.
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = p.srcImage;
    barrier.Transition.StateBefore = p.srcImageState;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.Subresource = D3D12CalcSubresource(
        baseMip, baseSlice, p.planeSlice, m_CallbackInfo.targetDesc.MipLevels, arraySize);

    // Multi-sampled images can't call CopyTextureRegion for a single sample, so instead
    // copy using a compute shader into a staging image first
    if(p.multisampled)
    {
      // For pipeline barriers.
      D3D12_RESOURCE_BARRIER barriers[2] = {};
      barriers[0] = barrier;
      barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      // Validation will complain if we don't transition all subresources for an SRV.
      barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

      barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barriers[1].Transition.pResource = m_CallbackInfo.dstBuffer;
      barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
      barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
      barriers[1].Transition.Subresource = 0;

      cmd->ResourceBarrier(2, barriers);

      m_pDevice->GetDebugManager()->PixelHistoryCopyPixel(cmd, m_CallbackInfo.dstBuffer, p, offset);

      std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
      std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
      cmd->ResourceBarrier(2, barriers);
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
      // Use Offset to get to the nearest 16KB
      UINT64 footprintOffset = (offset >> 14) << 14;
      UINT64 offsetRemainder = offset - footprintOffset;
      UINT dstX = (UINT)(offsetRemainder / elementSize);

      dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      dst.pResource = m_CallbackInfo.dstBuffer;
      dst.PlacedFootprint.Offset = footprintOffset;
      dst.PlacedFootprint.Footprint.Width = dstX + 1;
      dst.PlacedFootprint.Footprint.Height = 1;
      dst.PlacedFootprint.Footprint.Depth = 1;
      dst.PlacedFootprint.Footprint.Format = p.copyFormat;
      dst.PlacedFootprint.Footprint.RowPitch =
          AlignUp(dst.PlacedFootprint.Footprint.Width * elementSize,
                  (uint32_t)D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

      D3D12_BOX srcBox = {};
      srcBox.left = p.x;
      srcBox.top = p.y;
      srcBox.right = srcBox.left + 1;
      srcBox.bottom = srcBox.top + 1;
      srcBox.front = depthIndex;
      srcBox.back = srcBox.front + 1;

      if(offsetRemainder % elementSize == 0)
        cmd->CopyTextureRegion(&dst, dstX, 0, 0, &src, &srcBox);
      else
        RDCERR("OffsetRemainder %zu is not a multiple of elementSize %u", offsetRemainder,
               elementSize);

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
    ID3D12PipelineState *pso = GetPixelOcclusionPipeline(eid, pipeState);

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
  ID3D12PipelineState *GetPixelOcclusionPipeline(uint32_t eid, D3D12RenderState &state)
  {
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

    bool dxil = IsPSOUsingDXIL(pipeDesc);

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
                               const rdcarray<uint32_t> &events,
                               std::map<uint32_t, D3D12_RESOURCE_STATES> resourceStates)
      : D3D12PixelHistoryCallback(device, shaderCache, callbackInfo, NULL),
        m_Events(events),
        m_ResourceStates(resourceStates)
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
    m_SavedState = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();
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
    targetCopyParams.multisampled = (m_CallbackInfo.targetDesc.SampleDesc.Count != 1);
    D3D12_RESOURCE_STATES nonRtFallback = m_ResourceStates[eid];
    bool rtOutput = (nonRtFallback == D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_RESOURCE_STATES fallback = D3D12_RESOURCE_STATE_RENDER_TARGET;

    if(IsDepthFormat(m_CallbackInfo.targetDesc, m_CallbackInfo.compType))
    {
      fallback = m_SavedState.dsv.GetDSV().Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH
                     ? D3D12_RESOURCE_STATE_DEPTH_READ
                     : D3D12_RESOURCE_STATE_DEPTH_WRITE;
      targetCopyParams.srcImageFormat = GetDepthSRVFormat(m_CallbackInfo.targetDesc.Format, 0);
      targetCopyParams.depthcopy = true;
      targetCopyParams.copyFormat = GetDepthCopyFormat(m_CallbackInfo.targetDesc.Format);
      offset += offsetof(struct D3D12PixelHistoryValue, depth);
    }
    targetCopyParams.srcImageState = rtOutput ? fallback : nonRtFallback;

    CopyImagePixel(cmd, targetCopyParams, offset);

    if(IsDepthAndStencilFormat(m_CallbackInfo.targetDesc.Format))
    {
      D3D12CopyPixelParams stencilCopyParams = targetCopyParams;
      stencilCopyParams.srcImageFormat =
          GetDepthSRVFormat(m_CallbackInfo.targetImage->GetDesc().Format, 1);
      stencilCopyParams.copyFormat = DXGI_FORMAT_R8_TYPELESS;
      stencilCopyParams.planeSlice = 1;
      fallback = m_SavedState.dsv.GetDSV().Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL
                     ? D3D12_RESOURCE_STATE_DEPTH_READ
                     : D3D12_RESOURCE_STATE_DEPTH_WRITE;
      stencilCopyParams.srcImageState = rtOutput ? fallback : nonRtFallback;
      CopyImagePixel(cmd, stencilCopyParams, offset + sizeof(float));
    }

    // If the target image is a depth/stencil view, we already copied the value above.
    if(IsDepthFormat(m_CallbackInfo.targetDesc.Format))
      return;

    // Get the bound depth format for this event
    ResourceId resId = m_SavedState.dsv.GetResResourceId();
    if(resId != ResourceId())
    {
      WrappedID3D12Resource *depthImage =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Resource>(resId);

      DXGI_FORMAT depthFormat = m_SavedState.dsv.GetDSV().Format;
      // Descriptors with unknown type are valid and indicate to use the resource's format
      if(depthFormat == DXGI_FORMAT_UNKNOWN)
        depthFormat = depthImage->GetDesc().Format;

      D3D12CopyPixelParams depthCopyParams = targetCopyParams;
      depthCopyParams.srcImage = depthImage;
      depthCopyParams.srcImageFormat = GetDepthSRVFormat(depthFormat, 0);
      depthCopyParams.copyFormat = GetDepthCopyFormat(depthFormat);
      depthCopyParams.depthcopy = true;
      fallback = m_SavedState.dsv.GetDSV().Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH
                     ? D3D12_RESOURCE_STATE_DEPTH_READ
                     : D3D12_RESOURCE_STATE_DEPTH_WRITE;
      depthCopyParams.srcImageState = rtOutput ? fallback : nonRtFallback;
      CopyImagePixel(cmd, depthCopyParams, offset + offsetof(struct D3D12PixelHistoryValue, depth));

      if(IsDepthAndStencilFormat(depthFormat))
      {
        depthCopyParams.srcImageFormat = GetDepthSRVFormat(depthFormat, 1);
        depthCopyParams.copyFormat = DXGI_FORMAT_R8_TYPELESS;
        depthCopyParams.planeSlice = 1;
        fallback = m_SavedState.dsv.GetDSV().Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL
                       ? D3D12_RESOURCE_STATE_DEPTH_READ
                       : D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthCopyParams.srcImageState = rtOutput ? fallback : nonRtFallback;
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
    m_pDevice->ReplayDraw(cmd, *action);
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

    bool dxil = IsPSOUsingDXIL(desc);
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
  std::map<uint32_t, D3D12_RESOURCE_STATES> m_ResourceStates;
  // Key is event ID, and value is an index of where the event data is stored.
  std::map<uint32_t, size_t> m_EventIndices;
  std::map<uint32_t, DXGI_FORMAT> m_DepthFormats;
};

// TestsFailedCallback replays draws to figure out which tests failed, such as
// depth test, stencil test, etc.
struct D3D12TestsFailedCallback : public D3D12PixelHistoryCallback
{
  D3D12TestsFailedCallback(WrappedID3D12Device *device, D3D12PixelHistoryShaderCache *shaderCache,
                           const D3D12PixelHistoryCallbackInfo &callbackInfo,
                           ID3D12QueryHeap *occlusionQueryHeap, rdcarray<uint32_t> events)
      : D3D12PixelHistoryCallback(device, shaderCache, callbackInfo, occlusionQueryHeap),
        m_Events(events)
  {
  }

  ~D3D12TestsFailedCallback()
  {
    for(auto it = m_PipeCache.begin(); it != m_PipeCache.end(); ++it)
    {
      SAFE_RELEASE(it->second);
    }
  }

  void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(!m_Events.contains(eid))
      return;

    D3D12RenderState pipeState = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();

    uint32_t eventFlags = CalculateEventFlags(pipeState);
    m_EventFlags[eid] = eventFlags;

    WrappedID3D12PipelineState *origPSO =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(pipeState.pipe);
    if(origPSO == NULL)
      RDCERR("Failed to retrieve original PSO for pixel history.");

    D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
    origPSO->Fill(pipeDesc);

    if(pipeDesc.DepthStencilState.DepthBoundsTestEnable)
      m_EventDepthBounds[eid] = {pipeState.depthBoundsMin, pipeState.depthBoundsMax};
    else
      m_EventDepthBounds[eid] = {};

    // TODO: figure out if the shader has early fragments tests turned on,
    // based on the currently bound fragment shader.
    bool earlyFragmentTests = false;
    m_HasEarlyFragments[eid] = earlyFragmentTests;

    ReplayDrawWithTests(cmd, eid, eventFlags, pipeState, GetPixelHistoryRenderTargetIndex(pipeState));
    pipeState.ApplyState(m_pDevice, cmd);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) { return false; }
  void AliasEvent(uint32_t primary, uint32_t alias) {}

  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}

  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}
  void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}

  void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) {}

  bool HasEventFlags(uint32_t eventId) { return m_EventFlags.find(eventId) != m_EventFlags.end(); }
  uint32_t GetEventFlags(uint32_t eventId)
  {
    auto it = m_EventFlags.find(eventId);
    if(it == m_EventFlags.end())
      RDCERR("Can't find event flags for event %u", eventId);
    return it->second;
  }
  rdcpair<float, float> GetEventDepthBounds(uint32_t eventId)
  {
    auto it = m_EventDepthBounds.find(eventId);
    if(it == m_EventDepthBounds.end())
      RDCERR("Can't find event flags for event %u", eventId);
    return it->second;
  }

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

  uint64_t GetOcclusionResult(uint32_t eventId, uint32_t test) const
  {
    auto it = m_OcclusionQueries.find(rdcpair<uint32_t, uint32_t>(eventId, test));
    if(it == m_OcclusionQueries.end())
      RDCERR("Can't locate occlusion query for event id %u and test flags %u", eventId, test);
    if(it->second >= m_OcclusionResults.size())
      RDCERR(
          "Event %u, occlusion index is %u, and the total # of occlusion "
          "query data %zu",
          eventId, it->second, m_OcclusionResults.size());
    return m_OcclusionResults[it->second];
  }

  bool HasEarlyFragments(uint32_t eventId) const
  {
    auto it = m_HasEarlyFragments.find(eventId);
    RDCASSERT(it != m_HasEarlyFragments.end());
    return it->second;
  }

private:
  uint32_t CalculateEventFlags(const D3D12RenderState &pipeState)
  {
    uint32_t flags = 0;

    WrappedID3D12PipelineState *origPSO =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(pipeState.pipe);
    if(origPSO == NULL)
    {
      RDCERR("Failed to retrieve original PSO for pixel history.");
      return flags;
    }

    D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
    origPSO->Fill(pipeDesc);

    // Culling
    {
      if(pipeDesc.RasterizerState.DepthClipEnable)
        flags |= TestEnabled_DepthClipping;

      if(pipeDesc.RasterizerState.CullMode != D3D12_CULL_MODE_NONE)
        flags |= TestEnabled_Culling;
    }

    // Depth and Stencil tests
    if(pipeState.dsv.GetResResourceId() != ResourceId())
    {
      if(pipeDesc.DepthStencilState.DepthBoundsTestEnable)
        flags |= TestEnabled_DepthBounds;

      if(pipeDesc.DepthStencilState.DepthEnable)
      {
        if(pipeDesc.DepthStencilState.DepthFunc != D3D12_COMPARISON_FUNC_ALWAYS)
          flags |= TestEnabled_DepthTesting;
        if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_NEVER)
          flags |= TestMustFail_DepthTesting;

        if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_NEVER)
          flags |= DepthTest_Never;
        if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_LESS)
          flags |= DepthTest_Less;
        if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_EQUAL)
          flags |= DepthTest_Equal;
        if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_LESS_EQUAL)
          flags |= DepthTest_LessEqual;
        if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_GREATER)
          flags |= DepthTest_Greater;
        if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_NOT_EQUAL)
          flags |= DepthTest_NotEqual;
        if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_GREATER_EQUAL)
          flags |= DepthTest_GreaterEqual;
        if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_ALWAYS)
          flags |= DepthTest_Always;
      }
      else
      {
        flags |= DepthTest_Always;
      }

      if(pipeDesc.DepthStencilState.StencilEnable)
      {
        if(pipeDesc.DepthStencilState.FrontFace.StencilFunc != D3D12_COMPARISON_FUNC_ALWAYS ||
           pipeDesc.DepthStencilState.BackFace.StencilFunc != D3D12_COMPARISON_FUNC_ALWAYS)
          flags |= TestEnabled_StencilTesting;

        if(pipeDesc.DepthStencilState.FrontFace.StencilFunc == D3D12_COMPARISON_FUNC_NEVER &&
           pipeDesc.DepthStencilState.BackFace.StencilFunc == D3D12_COMPARISON_FUNC_NEVER)
          flags |= TestMustFail_StencilTesting;
        else if(pipeDesc.DepthStencilState.FrontFace.StencilFunc == D3D12_COMPARISON_FUNC_NEVER &&
                pipeDesc.RasterizerState.CullMode == D3D12_CULL_MODE_BACK)
          flags |= TestMustFail_StencilTesting;
        else if(pipeDesc.RasterizerState.CullMode == D3D12_CULL_MODE_FRONT &&
                pipeDesc.DepthStencilState.BackFace.StencilFunc == D3D12_COMPARISON_FUNC_NEVER)
          flags |= TestMustFail_StencilTesting;
      }
    }

    // Scissor
    {
      // Scissor is always enabled in D3D12
      flags |= TestEnabled_Scissor;

      bool inRegion = false;
      bool inAllRegions = true;
      // Do we even need to know viewport here?
      const D3D12_RECT *pScissors = pipeState.scissors.data();
      size_t scissorCount = pipeState.scissors.size();

      for(size_t i = 0; i < scissorCount; ++i)
      {
        if((int32_t)m_CallbackInfo.x >= pScissors[i].left &&
           (int32_t)m_CallbackInfo.y >= pScissors[i].top &&
           (int32_t)m_CallbackInfo.x < pScissors[i].right &&
           (int32_t)m_CallbackInfo.y < pScissors[i].bottom)
          inRegion = true;
        else
          inAllRegions = false;
      }

      if(!inRegion)
        flags |= TestMustFail_Scissor;
      if(inAllRegions)
        flags |= TestMustPass_Scissor;
    }

    // Blending
    {
      if(pipeDesc.BlendState.IndependentBlendEnable)
      {
        for(size_t i = 0; i < pipeState.rts.size(); ++i)
        {
          if(pipeDesc.BlendState.RenderTarget[i].BlendEnable)
          {
            flags |= Blending_Enabled;
            break;
          }
        }
      }
      else
      {
        // Might not have render targets if rasterization is disabled
        if(pipeState.rts.size() > 0 && pipeDesc.BlendState.RenderTarget[0].BlendEnable)
          flags |= Blending_Enabled;
      }
    }

    // TODO: Is there a better test for this?
    if(pipeDesc.PS.pShaderBytecode == NULL)
      flags |= UnboundFragmentShader;

    // Samples
    {
      // TODO: figure out if we always need to check this.
      flags |= TestEnabled_SampleMask;

      if((pipeDesc.SampleMask & m_CallbackInfo.sampleMask) == 0)
        flags |= TestMustFail_SampleMask;
    }

    // TODO: is shader discard always possible when PS is bound?
    if(pipeDesc.PS.BytecodeLength > 0 && pipeDesc.PS.pShaderBytecode != NULL)
      flags |= TestEnabled_FragmentDiscard;

    return flags;
  }

  // Flags to create a pipeline for tests, can be combined to control how
  // a pipeline is created.
  enum
  {
    PipelineCreationFlags_DisableCulling = 1 << 0,
    PipelineCreationFlags_DisableDepthTest = 1 << 1,
    PipelineCreationFlags_DisableStencilTest = 1 << 2,
    PipelineCreationFlags_DisableDepthBoundsTest = 1 << 3,
    PipelineCreationFlags_DisableDepthClipping = 1 << 4,
    PipelineCreationFlags_FixedColorShader = 1 << 5,
    PipelineCreationFlags_IntersectOriginalScissor = 1 << 6,
  };

  void ReplayDrawWithTests(ID3D12GraphicsCommandListX *cmd, uint32_t eid, uint32_t eventFlags,
                           D3D12RenderState pipeState, uint32_t outputIndex)
  {
    // TODO: Handle shader side effects?

    rdcarray<D3D12_RECT> prevScissors = pipeState.scissors;
    for(uint32_t i = 0; i < pipeState.views.size(); i++)
      ScissorToPixel(pipeState.views[i], pipeState.scissors[i]);
    pipeState.ApplyState(m_pDevice, cmd);

    if(eventFlags & TestEnabled_Culling)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_DisableDepthClipping |
          PipelineCreationFlags_DisableDepthBoundsTest | PipelineCreationFlags_DisableStencilTest |
          PipelineCreationFlags_FixedColorShader;
      ID3D12PipelineState *pso = CreatePipeline(pipeState, pipeFlags, outputIndex);
      D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Test culling on %u", eid));
      ReplayDraw(cmd, pipeState, pso, eid, TestEnabled_Culling);
    }

    if(eventFlags & TestEnabled_DepthClipping)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_DisableDepthBoundsTest |
          PipelineCreationFlags_DisableStencilTest | PipelineCreationFlags_FixedColorShader;
      ID3D12PipelineState *pso = CreatePipeline(pipeState, pipeFlags, outputIndex);
      D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Test depth clipping on %u", eid));
      ReplayDraw(cmd, pipeState, pso, eid, TestEnabled_DepthClipping);
    }

    // Scissor is always enabled on D3D12 but we still check some ensured pass/fail cases

    // If scissor must fail, we're done
    if(eventFlags & TestMustFail_Scissor)
      return;

    // If scissor must pass, we can skip this test
    if((eventFlags & TestMustPass_Scissor) == 0)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_IntersectOriginalScissor | PipelineCreationFlags_DisableDepthTest |
          PipelineCreationFlags_DisableDepthBoundsTest | PipelineCreationFlags_DisableStencilTest |
          PipelineCreationFlags_FixedColorShader;
      ID3D12PipelineState *pso = CreatePipeline(pipeState, pipeFlags, outputIndex);
      // This will change the scissor for the later tests, but since those
      // tests happen later in the pipeline, it does not matter.
      for(uint32_t i = 0; i < pipeState.views.size(); i++)
        IntersectScissors(prevScissors[i], pipeState.scissors[i]);
      D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Test scissor on %u", eid));
      ReplayDraw(cmd, pipeState, pso, eid, TestEnabled_Scissor);
    }

    // Sample mask
    if(eventFlags & TestMustFail_SampleMask)
      return;

    if(eventFlags & TestEnabled_SampleMask)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthBoundsTest | PipelineCreationFlags_DisableStencilTest |
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_FixedColorShader;
      ID3D12PipelineState *pso = CreatePipeline(pipeState, pipeFlags, outputIndex);
      D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Test sample mask on %u", eid));
      ReplayDraw(cmd, pipeState, pso, eid, TestEnabled_SampleMask);
    }

    // Depth bounds
    if(eventFlags & TestEnabled_DepthBounds)
    {
      uint32_t pipeFlags = PipelineCreationFlags_DisableStencilTest |
                           PipelineCreationFlags_DisableDepthTest |
                           PipelineCreationFlags_FixedColorShader;
      ID3D12PipelineState *pso = CreatePipeline(pipeState, pipeFlags, outputIndex);
      D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Test depth bounds on %u", eid));
      ReplayDraw(cmd, pipeState, pso, eid, TestEnabled_DepthBounds);
    }

    // Stencil test
    if(eventFlags & TestMustFail_StencilTesting)
      return;

    if(eventFlags & TestEnabled_StencilTesting)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_FixedColorShader;
      ID3D12PipelineState *pso = CreatePipeline(pipeState, pipeFlags, outputIndex);
      D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Test stencil on %u", eid));
      ReplayDraw(cmd, pipeState, pso, eid, TestEnabled_StencilTesting);
    }

    // Depth test
    if(eventFlags & TestMustFail_DepthTesting)
      return;

    if(eventFlags & TestEnabled_DepthTesting)
    {
      // Previous test might have modified the stencil state, which could cause this event to fail.
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableStencilTest | PipelineCreationFlags_FixedColorShader;

      ID3D12PipelineState *pso = CreatePipeline(pipeState, pipeFlags, outputIndex);
      D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Test depth on %u", eid));
      ReplayDraw(cmd, pipeState, pso, eid, TestEnabled_DepthTesting);
    }

    // Shader discard
    if(eventFlags & TestEnabled_FragmentDiscard)
    {
      // With early fragment tests, sample counting (occlusion query) will be
      // done before the shader executes.
      // TODO: remove early fragment tests if it is ON.
      uint32_t pipeFlags = PipelineCreationFlags_DisableDepthBoundsTest |
                           PipelineCreationFlags_DisableStencilTest |
                           PipelineCreationFlags_DisableDepthTest;
      ID3D12PipelineState *pso = CreatePipeline(pipeState, pipeFlags, outputIndex);
      D3D12MarkerRegion::Set(cmd, StringFormat::Fmt("Test shader discard on %u", eid));
      ReplayDraw(cmd, pipeState, pso, eid, TestEnabled_FragmentDiscard);
    }
  }

  // Creates a pipeline that is based on the given pipeline and the given
  // pipeline flags. Modifies the base pipeline according to the flags, and
  // leaves the original pipeline behavior if a flag is not set.
  ID3D12PipelineState *CreatePipeline(D3D12RenderState baseState, uint32_t pipeCreateFlags,
                                      uint32_t outputIndex)
  {
    rdcpair<ResourceId, uint32_t> pipeKey(baseState.pipe, pipeCreateFlags);
    auto it = m_PipeCache.find(pipeKey);
    // Check if we processed this pipeline before.
    if(it != m_PipeCache.end())
      return it->second;

    WrappedID3D12PipelineState *origPSO =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(baseState.pipe);
    if(origPSO == NULL)
    {
      RDCERR("Failed to retrieve original PSO for pixel history.");
      return NULL;
    }

    D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
    origPSO->Fill(pipeDesc);

    // Only interested in a single sample.
    pipeDesc.SampleMask = m_CallbackInfo.sampleMask;

    // We are going to replay a draw multiple times, don't want to modify the
    // depth value, not to influence later tests.
    pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    if(pipeCreateFlags & PipelineCreationFlags_DisableCulling)
      pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    if(pipeCreateFlags & PipelineCreationFlags_DisableDepthTest)
      pipeDesc.DepthStencilState.DepthEnable = FALSE;
    if(pipeCreateFlags & PipelineCreationFlags_DisableStencilTest)
      pipeDesc.DepthStencilState.StencilEnable = FALSE;
    if(pipeCreateFlags & PipelineCreationFlags_DisableDepthBoundsTest)
      pipeDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;
    if(pipeCreateFlags & PipelineCreationFlags_DisableDepthClipping)
      pipeDesc.RasterizerState.DepthClipEnable = FALSE;

    if(pipeCreateFlags & PipelineCreationFlags_FixedColorShader)
    {
      bool dxil = IsPSOUsingDXIL(pipeDesc);

      ID3DBlob *FixedColorPS = m_ShaderCache->GetFixedColorShader(dxil, outputIndex);
      pipeDesc.PS.pShaderBytecode = FixedColorPS->GetBufferPointer();
      pipeDesc.PS.BytecodeLength = FixedColorPS->GetBufferSize();
    }

    ID3D12PipelineState *pso = NULL;
    HRESULT hr = m_pDevice->CreatePipeState(pipeDesc, &pso);
    if(FAILED(hr))
    {
      RDCERR("Failed to create PSO for pixel history.");
      return NULL;
    }

    m_PipeCache.insert(std::make_pair(pipeKey, pso));
    return pso;
  }

  void ReplayDraw(ID3D12GraphicsCommandListX *cmd, D3D12RenderState pipeState,
                  ID3D12PipelineState *pso, int eventId, uint32_t test)
  {
    pipeState.pipe = GetResID(pso);
    pipeState.ApplyState(m_pDevice, cmd);

    uint32_t index = (uint32_t)m_OcclusionQueries.size();
    if(m_OcclusionQueries.find(rdcpair<uint32_t, uint32_t>(eventId, test)) != m_OcclusionQueries.end())
      RDCERR("A query already exist for event id %u and test %u", eventId, test);
    m_OcclusionQueries.insert(std::make_pair(rdcpair<uint32_t, uint32_t>(eventId, test), index));

    cmd->BeginQuery(m_OcclusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, index);
    m_pDevice->ReplayDraw(cmd, *m_pDevice->GetAction(eventId));

    cmd->EndQuery(m_OcclusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, index);
  }

  rdcarray<uint32_t> m_Events;
  // Key is event ID, value is the flags for that event.
  std::map<uint32_t, uint32_t> m_EventFlags;
  std::map<uint32_t, rdcpair<float, float>> m_EventDepthBounds;
  // Key is a pair <Base pipeline, pipeline flags>
  std::map<rdcpair<ResourceId, uint32_t>, ID3D12PipelineState *> m_PipeCache;
  // Key: pair <event ID, test>
  // value: the index where occlusion query is in m_OcclusionResults
  std::map<rdcpair<uint32_t, uint32_t>, uint32_t> m_OcclusionQueries;
  std::map<uint32_t, bool> m_HasEarlyFragments;
  rdcarray<uint64_t> m_OcclusionResults;
};

void D3D12UpdateTestsFailed(const D3D12TestsFailedCallback *tfCb, uint32_t eventId,
                            uint32_t eventFlags, PixelModification &mod)
{
  bool earlyFragmentTests = tfCb->HasEarlyFragments(eventId);

  if(eventFlags & TestEnabled_Culling)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_Culling);
    mod.backfaceCulled = (occlData == 0);
  }
  if(mod.backfaceCulled)
    return;

  if(eventFlags & TestEnabled_DepthClipping)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_DepthClipping);
    mod.depthClipped = (occlData == 0);
  }
  if(mod.depthClipped)
    return;

  if((eventFlags & (TestEnabled_Scissor | TestMustPass_Scissor | TestMustFail_Scissor)) ==
     TestEnabled_Scissor)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_Scissor);
    mod.scissorClipped = (occlData == 0);
  }
  if(mod.scissorClipped)
    return;

  // TODO: Exclusive Scissor Test if NV extension is turned on?

  if((eventFlags & (TestEnabled_SampleMask | TestMustFail_SampleMask)) == TestEnabled_SampleMask)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_SampleMask);
    mod.sampleMasked = (occlData == 0);
  }
  if(mod.sampleMasked)
    return;

  // Shader discard with default fragment tests order.
  if((eventFlags & TestEnabled_FragmentDiscard) && !earlyFragmentTests)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_FragmentDiscard);
    mod.shaderDiscarded = (occlData == 0);
    if(mod.shaderDiscarded)
      return;
  }

  if(eventFlags & TestEnabled_DepthBounds)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_DepthBounds);
    mod.depthBoundsFailed = (occlData == 0);
  }
  if(mod.depthBoundsFailed)
    return;

  if((eventFlags & (TestEnabled_StencilTesting | TestMustFail_StencilTesting)) ==
     TestEnabled_StencilTesting)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_StencilTesting);
    mod.stencilTestFailed = (occlData == 0);
  }
  if(mod.stencilTestFailed)
    return;

  if((eventFlags & (TestEnabled_DepthTesting | TestMustFail_DepthTesting)) == TestEnabled_DepthTesting)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_DepthTesting);
    mod.depthTestFailed = (occlData == 0);
  }
  if(mod.depthTestFailed)
    return;

  // Shader discard with early fragment tests order.
  if((eventFlags & TestEnabled_FragmentDiscard) && earlyFragmentTests)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_FragmentDiscard);
    mod.shaderDiscarded = (occlData == 0);
  }
}

// Callback used to get information for each fragment that touched a pixel
struct D3D12PixelHistoryPerFragmentCallback : D3D12PixelHistoryCallback
{
  D3D12PixelHistoryPerFragmentCallback(WrappedID3D12Device *device,
                                       D3D12PixelHistoryShaderCache *shaderCache,
                                       const D3D12PixelHistoryCallbackInfo &callbackInfo,
                                       const std::map<uint32_t, uint32_t> &eventFragments,
                                       const std::map<uint32_t, ModificationValue> &eventPremods)
      : D3D12PixelHistoryCallback(device, shaderCache, callbackInfo, NULL),
        m_EventFragments(eventFragments),
        m_EventPremods(eventPremods)
  {
  }

  ~D3D12PixelHistoryPerFragmentCallback()
  {
    for(ID3D12PipelineState *pso : m_PSOsToDestroy)
      SAFE_RELEASE(pso);
  }

  struct PerFragmentPipelines
  {
    // Disable all tests, use the new render pass to render into a separate
    // attachment, and use fragment shader that outputs primitive ID.
    ID3D12PipelineState *primitiveIdPipe;
    // Turn off blending.
    ID3D12PipelineState *shaderOutPipe;
    // Enable blending to get post event values.
    ID3D12PipelineState *postModPipe;
  };

  void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(m_EventFragments.find(eid) == m_EventFragments.end())
      return;

    D3D12RenderState prevState = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();
    D3D12RenderState &state = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();

    uint32_t numFragmentsInEvent = m_EventFragments[eid];

    // TODO: Do we need to test for stencil format here too, or does this check catch planar depth/stencil?
    uint32_t renderTargetIndex = 0;
    if(IsDepthFormat(m_CallbackInfo.targetDesc.Format))
    {
      // Color target not needed
      renderTargetIndex = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
    }
    else
    {
      for(uint32_t i = 0; i < state.rts.size(); ++i)
      {
        ResourceId img = state.rts[i].GetResResourceId();
        if(img == GetResID(m_CallbackInfo.targetImage))
        {
          renderTargetIndex = i;
          break;
        }
      }
    }

    WrappedID3D12PipelineState *origPSO =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(state.pipe);
    if(origPSO == NULL)
    {
      RDCERR("Failed to retrieve original PSO for pixel history.");
      return;
    }

    D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC origPipeDesc;
    origPSO->Fill(origPipeDesc);

    PerFragmentPipelines pipes = CreatePerFragmentPipelines(state, eid, 0, renderTargetIndex);

    for(uint32_t i = 0; i < state.views.size(); i++)
    {
      ScissorToPixel(state.views[i], state.scissors[i]);

      // Set scissor to the whole pixel quad
      state.scissors[i].left &= ~0x1;
      state.scissors[i].top &= ~0x1;
      state.scissors[i].right = state.scissors[i].left + 2;
      state.scissors[i].bottom = state.scissors[i].top + 2;
    }

    ID3D12PipelineState *psosIter[2];
    psosIter[0] = pipes.primitiveIdPipe;
    psosIter[1] = pipes.shaderOutPipe;

    D3D12CopyPixelParams colorCopyParams = {};
    colorCopyParams.srcImage = m_CallbackInfo.colorImage;
    colorCopyParams.srcImageFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
    colorCopyParams.copyFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
    colorCopyParams.srcImageState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    colorCopyParams.multisampled = m_CallbackInfo.targetDesc.SampleDesc.Count > 1;
    colorCopyParams.x = m_CallbackInfo.x;
    colorCopyParams.y = m_CallbackInfo.y;
    colorCopyParams.sample = m_CallbackInfo.targetSubresource.sample;
    colorCopyParams.mip = m_CallbackInfo.targetSubresource.mip;
    colorCopyParams.arraySlice = m_CallbackInfo.targetSubresource.slice;
    colorCopyParams.scratchBuffer = true;

    bool depthEnabled = origPipeDesc.DepthStencilState.DepthEnable != FALSE;

    D3D12MarkerRegion::Set(
        cmd, StringFormat::Fmt("Event %u has %u fragments", eid, numFragmentsInEvent));

    rdcarray<D3D12Descriptor> origRts = state.rts;

    // Get primitive ID and shader output value for each fragment.
    for(uint32_t f = 0; f < numFragmentsInEvent; f++)
    {
      for(uint32_t i = 0; i < 2; i++)
      {
        uint32_t storeOffset = (fragsProcessed + f) * sizeof(D3D12PerFragmentInfo);

        bool isPrimPass = (i == 0);

        D3D12MarkerRegion region(
            cmd, StringFormat::Fmt("Getting %s for %u",
                                   isPrimPass ? "primitive ID" : "shader output", eid));

        if(psosIter[i] == NULL)
        {
          // Without one of the pipelines (e.g. if there was a geometry shader in use and we can't
          // read primitive ID in the fragment shader) we can't continue.
          // Technically we can if the geometry shader outs a primitive ID, but that is unlikely.
          D3D12MarkerRegion::Set(cmd, "Can't get primitive ID with geometry shader in use");

          D3D12_WRITEBUFFERIMMEDIATE_PARAMETER param = {};
          param.Dest = m_CallbackInfo.dstBuffer->GetGPUVirtualAddress() + storeOffset;
          param.Value = ~0U;
          cmd->WriteBufferImmediate(1, &param, NULL);
          continue;
        }

        // TODO: Are there any barriers needed here before/after clearing DSV?

        cmd->ClearDepthStencilView(m_pDevice->GetDebugManager()->GetCPUHandle(PIXEL_HISTORY_DSV),
                                   D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 1,
                                   &state.scissors[0]);

        if(isPrimPass)
        {
          state.rts.resize(1);
          state.rts[0] = *m_CallbackInfo.colorDescriptor;
        }
        else if(renderTargetIndex != D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
        {
          state.rts[renderTargetIndex] = *m_CallbackInfo.colorDescriptor;
        }
        state.dsv = *m_CallbackInfo.dsDescriptor;
        state.pipe = GetResID(psosIter[i]);

        // Update stencil reference to the current fragment index, so that we
        // get values for a single fragment only.
        state.stencilRefFront = f;
        state.stencilRefBack = f;
        state.ApplyState(m_pDevice, cmd);

        const ActionDescription *action = m_pDevice->GetAction(eid);
        m_pDevice->ReplayDraw(cmd, *action);

        if(!isPrimPass)
        {
          storeOffset += offsetof(struct D3D12PerFragmentInfo, shaderOut);
          if(depthEnabled)
          {
            DXGI_FORMAT depthFormat = m_CallbackInfo.dsImage->GetDesc().Format;

            D3D12CopyPixelParams depthCopyParams = colorCopyParams;
            depthCopyParams.srcImage = m_CallbackInfo.dsImage;
            depthCopyParams.srcImageFormat = GetDepthSRVFormat(depthFormat, 0);
            depthCopyParams.copyFormat = GetDepthCopyFormat(depthFormat);
            depthCopyParams.srcImageState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            depthCopyParams.planeSlice = 0;
            depthCopyParams.depthcopy = true;
            CopyImagePixel(cmd, depthCopyParams,
                           storeOffset + offsetof(struct D3D12PixelHistoryValue, depth));
          }
        }
        CopyImagePixel(cmd, colorCopyParams, storeOffset);

        // restore the original render targets as subsequent steps will use them
        state.rts = origRts;
      }
    }

    if(renderTargetIndex != D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
    {
      state.rts[renderTargetIndex] = *m_CallbackInfo.colorDescriptor;
    }

    // Get post-modification value, use the original framebuffer attachment.
    state.pipe = GetResID(pipes.postModPipe);

    const ModificationValue &premod = m_EventPremods[eid];
    // For every fragment except the last one, retrieve post-modification value.
    for(uint32_t f = 0; f < numFragmentsInEvent - 1; ++f)
    {
      D3D12MarkerRegion region(cmd,
                               StringFormat::Fmt("Getting postmod for fragment %u in %u", f, eid));

      // Have to reset stencil
      D3D12_CLEAR_FLAGS clearFlags =
          (f == 0 ? D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL : D3D12_CLEAR_FLAG_STENCIL);
      cmd->ClearDepthStencilView(m_pDevice->GetDebugManager()->GetCPUHandle(PIXEL_HISTORY_DSV),
                                 clearFlags, premod.depth, 0, 1, &state.scissors[0]);

      if(f == 0)
      {
        // Before starting the draw, initialize the pixel to the premodification value
        // for this event, for both color and depth. Depth was handled above already.
        cmd->ClearRenderTargetView(m_pDevice->GetDebugManager()->GetCPUHandle(PIXEL_HISTORY_RTV),
                                   premod.col.floatValue.data(), 1, &state.scissors[0]);

        // TODO: Does anything different need to happen here if the target resource is depth/stencil?
      }

      state.stencilRefFront = f;
      state.stencilRefBack = f;
      state.ApplyState(m_pDevice, cmd);

      const ActionDescription *action = m_pDevice->GetAction(eid);
      m_pDevice->ReplayDraw(cmd, *action);

      CopyImagePixel(cmd, colorCopyParams,
                     (fragsProcessed + f) * sizeof(D3D12PerFragmentInfo) +
                         offsetof(struct D3D12PerFragmentInfo, postMod));

      if(prevState.dsv.GetResResourceId() != ResourceId())
      {
        DXGI_FORMAT depthFormat = m_CallbackInfo.dsImage->GetDesc().Format;

        D3D12CopyPixelParams depthCopyParams = colorCopyParams;
        depthCopyParams.srcImage = m_CallbackInfo.dsImage;
        depthCopyParams.srcImageFormat = GetDepthSRVFormat(depthFormat, 0);
        depthCopyParams.copyFormat = GetDepthCopyFormat(depthFormat);
        depthCopyParams.srcImageState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthCopyParams.depthcopy = true;
        CopyImagePixel(cmd, depthCopyParams,
                       (fragsProcessed + f) * sizeof(D3D12PerFragmentInfo) +
                           offsetof(struct D3D12PerFragmentInfo, postMod) +
                           offsetof(struct D3D12PixelHistoryValue, depth));
      }
    }

    m_EventIndices[eid] = fragsProcessed;
    fragsProcessed += numFragmentsInEvent;

    state = prevState;
    state.ApplyState(m_pDevice, cmd);
  }
  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}

  // CreatePerFragmentPipelines for getting per fragment information.
  PerFragmentPipelines CreatePerFragmentPipelines(const D3D12RenderState &state, uint32_t eid,
                                                  uint32_t fragmentIndex, uint32_t colorOutputIndex)
  {
    PerFragmentPipelines pipes = {};

    WrappedID3D12PipelineState *origPSO =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(state.pipe);
    if(origPSO == NULL)
    {
      RDCERR("Failed to retrieve original PSO for pixel history.");
      return pipes;
    }

    D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
    origPSO->Fill(pipeDesc);

    if(colorOutputIndex != D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
    {
      // TODO: We provide our own render target here, but some of the PSOs use the original shader,
      // which may write to a different number of channels (such as a R11G11B10 render target).
      // This results in a D3D12 debug layer warning, but shouldn't be hazardous in practice since
      // we restrict what we read from based on the source RT target. But maybe there's a way to
      // gather the right data and avoid  the debug layer warning.
      DXGI_FORMAT colorFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
      // For this pass, we will be binding the color target for capturing the shader output to the
      //  slot in question. The others can be left intact.
      RDCASSERT(colorOutputIndex < pipeDesc.RTVFormats.NumRenderTargets);
      pipeDesc.RTVFormats.RTFormats[colorOutputIndex] = colorFormat;

      // In order to have different write masks per render target, we need to switch to independent
      //  blend if not already in use.
      if(!pipeDesc.BlendState.IndependentBlendEnable)
      {
        pipeDesc.BlendState.IndependentBlendEnable = TRUE;
        for(uint32_t i = 1; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
          pipeDesc.BlendState.RenderTarget[i] = pipeDesc.BlendState.RenderTarget[0];
      }

      // Mask out writes to targets which aren't the pixel history color target
      for(uint32_t i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
      {
        pipeDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
        if(i != colorOutputIndex)
          pipeDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = 0;
      }
    }

    pipeDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    // TODO: Do we want to get the widest DSV format, or get it from callbackinfo/pixelhistoryresources?

    // Modify the stencil state, so that only one fragment passes.
    {
      pipeDesc.DepthStencilState.StencilEnable = TRUE;
      pipeDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
      pipeDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_INCR_SAT;
      pipeDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR_SAT;
      pipeDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_INCR_SAT;
      pipeDesc.DepthStencilState.FrontFace.StencilReadMask = 0xff;
      pipeDesc.DepthStencilState.FrontFace.StencilWriteMask = 0xff;
      pipeDesc.DepthStencilState.BackFace = pipeDesc.DepthStencilState.FrontFace;
    }

    // TODO: The original pixel shader may have side effects such as UAV writes. The Vulkan impl
    // removes side effects with shader patching but D3D12 does not support that yet.

    HRESULT hr = m_pDevice->CreatePipeState(pipeDesc, &pipes.postModPipe);
    if(FAILED(hr))
    {
      RDCERR("Failed to create PSO for pixel history.");
      return pipes;
    }

    m_PSOsToDestroy.push_back(pipes.postModPipe);

    // Other targets were already disabled for the post mod pass, now force enable the
    //  pixel history color target
    if(colorOutputIndex != D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
      pipeDesc.BlendState.RenderTarget[colorOutputIndex].RenderTargetWriteMask =
          D3D12_COLOR_WRITE_ENABLE_ALL;

    for(uint32_t i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
      pipeDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
    }

    {
      pipeDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;
      pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
      pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    }

    hr = m_pDevice->CreatePipeState(pipeDesc, &pipes.shaderOutPipe);
    if(FAILED(hr))
    {
      RDCERR("Failed to create PSO for pixel history.");
      return pipes;
    }

    m_PSOsToDestroy.push_back(pipes.shaderOutPipe);

    {
      pipeDesc.DepthStencilState.DepthEnable = FALSE;
      pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    }

    if(pipeDesc.MS.pShaderBytecode != NULL || pipeDesc.MS.BytecodeLength > 0)
    {
      RDCWARN("Can't get primitive ID at event %u due to mesh shader usage", eid);
    }
    else if(pipeDesc.GS.pShaderBytecode != NULL || pipeDesc.GS.BytecodeLength > 0)
    {
      RDCWARN("Can't get primitive ID at event %u due to geometry shader usage", eid);
    }
    else
    {
      // Regardless of which RT is the one in question for history, we write the
      //  primitive ID to RT0.
      pipeDesc.RTVFormats.NumRenderTargets = 1;
      pipeDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
      for(int i = 1; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        pipeDesc.RTVFormats.RTFormats[i] = DXGI_FORMAT_UNKNOWN;
      pipeDesc.BlendState.IndependentBlendEnable = FALSE;
      pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

      bool dxil = IsPSOUsingDXIL(pipeDesc);

      ID3DBlob *PrimIDPS = m_ShaderCache->GetPrimitiveIdShader(dxil);
      pipeDesc.PS.pShaderBytecode = PrimIDPS->GetBufferPointer();
      pipeDesc.PS.BytecodeLength = PrimIDPS->GetBufferSize();

      hr = m_pDevice->CreatePipeState(pipeDesc, &pipes.primitiveIdPipe);
      if(FAILED(hr))
      {
        RDCERR("Failed to create PSO for pixel history.");
        return pipes;
      }

      m_PSOsToDestroy.push_back(pipes.primitiveIdPipe);
    }

    return pipes;
  }

  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}

  void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}

  void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias) {}

  uint32_t GetEventOffset(uint32_t eid)
  {
    auto it = m_EventIndices.find(eid);
    RDCASSERT(it != m_EventIndices.end());
    return it->second;
  }

private:
  // For each event, specifies where the occlusion query results start
  std::map<uint32_t, uint32_t> m_EventIndices;
  // Number of fragments for each event
  std::map<uint32_t, uint32_t> m_EventFragments;
  // Pre-modification values for events to initialize attachments to,
  // so that we can get blended post-modification values.
  std::map<uint32_t, ModificationValue> m_EventPremods;
  // Number of fragments processed so far
  uint32_t fragsProcessed = 0;

  rdcarray<ID3D12PipelineState *> m_PSOsToDestroy;
};

// Callback used to determine the shader discard status for each fragment, where
// an event has multiple fragments with some being discarded in a fragment shader.
struct D3D12PixelHistoryDiscardedFragmentsCallback : D3D12PixelHistoryCallback
{
  // Key is event ID and value is a list of primitive IDs
  std::map<uint32_t, rdcarray<int32_t>> m_Events;
  D3D12PixelHistoryDiscardedFragmentsCallback(WrappedID3D12Device *device,
                                              D3D12PixelHistoryShaderCache *shaderCache,
                                              const D3D12PixelHistoryCallbackInfo &callbackInfo,
                                              std::map<uint32_t, rdcarray<int32_t>> events,
                                              ID3D12QueryHeap *occlusionQueryHeap)
      : D3D12PixelHistoryCallback(device, shaderCache, callbackInfo, occlusionQueryHeap),
        m_Events(events)
  {
  }

  ~D3D12PixelHistoryDiscardedFragmentsCallback()
  {
    for(ID3D12PipelineState *pso : m_PSOsToDestroy)
      SAFE_RELEASE(pso);
  }

  void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return;

    const rdcarray<int32_t> &primIds = m_Events[eid];

    D3D12RenderState &state = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();
    D3D12RenderState prevState = state;

    // Create a pipeline with a scissor and colorWriteMask = 0, and disable all tests.
    ID3D12PipelineState *newPso = CreateDiscardedFragmentPipeline(state, eid);

    for(uint32_t i = 0; i < state.views.size(); i++)
      ScissorToPixel(state.views[i], state.scissors[i]);

    Topology topo = MakePrimitiveTopology(state.topo);
    state.pipe = GetResID(newPso);
    state.ApplyState(m_pDevice, cmd);

    for(uint32_t i = 0; i < primIds.size(); i++)
    {
      uint32_t queryId = (uint32_t)m_OcclusionQueries.size();
      cmd->BeginQuery(m_OcclusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, queryId);

      uint32_t primId = primIds[i];
      ActionDescription action = *m_pDevice->GetAction(eid);
      action.numIndices = RENDERDOC_NumVerticesPerPrimitive(topo);
      action.indexOffset += RENDERDOC_VertexOffset(topo, primId);
      action.vertexOffset += RENDERDOC_VertexOffset(topo, primId);

      // TODO once pixel history distinguishes between instances, draw only the instance
      // for this fragment.
      // TODO replay with a dummy index buffer so that all primitives other than the target
      // one are degenerate - that way the vertex index etc is still the same as it should be.
      m_pDevice->ReplayDraw(cmd, action);
      cmd->EndQuery(m_OcclusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, queryId);

      m_OcclusionQueries[make_rdcpair<uint32_t, uint32_t>(eid, primId)] = queryId;
    }

    state = prevState;
    state.ApplyState(m_pDevice, cmd);
  }

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

  uint64_t GetOcclusionResult(uint32_t eventId, uint32_t test) const
  {
    auto it = m_OcclusionQueries.find(rdcpair<uint32_t, uint32_t>(eventId, test));
    if(it == m_OcclusionQueries.end())
      RDCERR("Can't locate occlusion query for event id %u and test flags %u", eventId, test);
    if(it->second >= m_OcclusionResults.size())
      RDCERR(
          "Event %u, occlusion index is %u, and the total # of occlusion "
          "query data %zu",
          eventId, it->second, m_OcclusionResults.size());
    return m_OcclusionResults[it->second];
  }

  bool PrimitiveDiscarded(uint32_t eid, uint32_t primId)
  {
    auto it = m_OcclusionQueries.find(make_rdcpair<uint32_t, uint32_t>(eid, primId));
    if(it == m_OcclusionQueries.end())
      return false;
    return m_OcclusionResults[it->second] == 0;
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}

  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}

  void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}

  void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias) {}

private:
  ID3D12PipelineState *CreateDiscardedFragmentPipeline(const D3D12RenderState &state, uint32_t eid)
  {
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
    pipeDesc.DepthStencilState.StencilEnable = FALSE;

    ID3D12PipelineState *pso = NULL;
    HRESULT hr = m_pDevice->CreatePipeState(pipeDesc, &pso);
    if(FAILED(hr))
    {
      RDCERR("Failed to create PSO for pixel history.");
      return NULL;
    }

    m_PSOsToDestroy.push_back(pso);
    return pso;
  }

  std::map<rdcpair<uint32_t, uint32_t>, uint32_t> m_OcclusionQueries;
  rdcarray<uint64_t> m_OcclusionResults;

  rdcarray<ID3D12PipelineState *> m_PSOsToDestroy;
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
  imageDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

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

namespace
{

bool CreateOcclusionPool(WrappedID3D12Device *device, uint32_t poolSize, ID3D12QueryHeap **ppQueryHeap)
{
  D3D12MarkerRegion region(device->GetQueue()->GetReal(),
                           StringFormat::Fmt("CreateQueryHeap %u", poolSize));

  D3D12_QUERY_HEAP_DESC queryDesc = {};
  queryDesc.Count = poolSize;
  queryDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
  HRESULT hr = device->CreateQueryHeap(&queryDesc, __uuidof(ID3D12QueryHeap), (void **)ppQueryHeap);
  if(FAILED(hr))
  {
    RDCERR("Failed to create query heap for pixel history HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  return true;
}

bool IsUavWrite(ResourceUsage usage)
{
  return (usage >= ResourceUsage::VS_RWResource && usage <= ResourceUsage::CS_RWResource);
}

bool IsResolveWrite(ResourceUsage usage)
{
  return (usage == ResourceUsage::Resolve || usage == ResourceUsage::ResolveDst);
}

bool IsCopyWrite(ResourceUsage usage)
{
  return (usage == ResourceUsage::CopyDst || usage == ResourceUsage::Copy ||
          usage == ResourceUsage::GenMips);
}

bool IsDirectWrite(ResourceUsage usage)
{
  return IsUavWrite(usage) || IsResolveWrite(usage) || IsCopyWrite(usage);
}

// NOTE: This function is quite similar to formatpacking.cpp::DecodeFormattedComponents,
//  but it doesn't convert everything to a float since the pixel history viewer will expect
//  them to be in the target's format.
void PixelHistoryDecode(const ResourceFormat &fmt, const byte *data, PixelValue &out)
{
  if(fmt.compType == CompType::UInt || fmt.compType == CompType::SInt || fmt.compCount == 4)
    out.floatValue[3] = 0.0f;

  if(fmt.type == ResourceFormatType::R10G10B10A2)
  {
    if(fmt.compType == CompType::SNorm)
    {
      Vec4f v = ConvertFromR10G10B10A2SNorm(*(const uint32_t *)data);
      out.floatValue[0] = v.x;
      out.floatValue[1] = v.y;
      out.floatValue[2] = v.z;
      out.floatValue[3] = v.w;
    }
    else if(fmt.compType == CompType::UNorm)
    {
      Vec4f v = ConvertFromR10G10B10A2(*(const uint32_t *)data);
      out.floatValue[0] = v.x;
      out.floatValue[1] = v.y;
      out.floatValue[2] = v.z;
      out.floatValue[3] = v.w;
    }
    else if(fmt.compType == CompType::UInt)
    {
      Vec4u v = ConvertFromR10G10B10A2UInt(*(const uint32_t *)data);
      out.uintValue[0] = v.x;
      out.uintValue[1] = v.y;
      out.uintValue[2] = v.z;
      out.uintValue[3] = v.w;
    }

    // the different types are a union so we can ignore format and just treat it as a data swap
    if(fmt.BGRAOrder())
      std::swap(out.uintValue[0], out.uintValue[2]);
  }
  else if(fmt.type == ResourceFormatType::R11G11B10)
  {
    Vec3f v = ConvertFromR11G11B10(*(const uint32_t *)data);
    out.floatValue[0] = v.x;
    out.floatValue[1] = v.y;
    out.floatValue[2] = v.z;
  }
  else if(fmt.type == ResourceFormatType::R5G5B5A1)
  {
    Vec4f v = ConvertFromB5G5R5A1(*(const uint16_t *)data);
    out.floatValue[0] = v.x;
    out.floatValue[1] = v.y;
    out.floatValue[2] = v.z;
    out.floatValue[3] = v.w;

    // conversely we *expect* BGRA order for this format and the above conversion implicitly flips
    // when bit-unpacking. So if the format wasn't BGRA order, flip it back
    if(!fmt.BGRAOrder())
      std::swap(out.floatValue[0], out.floatValue[2]);
  }
  else if(fmt.type == ResourceFormatType::R5G6B5)
  {
    Vec3f v = ConvertFromB5G6R5(*(const uint16_t *)data);
    out.floatValue[0] = v.x;
    out.floatValue[1] = v.y;
    out.floatValue[2] = v.z;

    // conversely we *expect* BGRA order for this format and the above conversion implicitly flips
    // when bit-unpacking. So if the format wasn't BGRA order, flip it back
    if(!fmt.BGRAOrder())
      std::swap(out.floatValue[0], out.floatValue[2]);
  }
  else if(fmt.type == ResourceFormatType::R4G4B4A4)
  {
    Vec4f v = ConvertFromB4G4R4A4(*(const uint16_t *)data);
    out.floatValue[0] = v.x;
    out.floatValue[1] = v.y;
    out.floatValue[2] = v.z;
    out.floatValue[3] = v.w;

    // conversely we *expect* BGRA order for this format and the above conversion implicitly flips
    // when bit-unpacking. So if the format wasn't BGRA order, flip it back
    if(!fmt.BGRAOrder())
      std::swap(out.floatValue[0], out.floatValue[2]);
  }
  else if(fmt.type == ResourceFormatType::R4G4)
  {
    Vec4f v = ConvertFromR4G4(*(const uint8_t *)data);
    out.floatValue[0] = v.x;
    out.floatValue[1] = v.y;
  }
  else if(fmt.type == ResourceFormatType::R9G9B9E5)
  {
    Vec3f v = ConvertFromR9G9B9E5(*(const uint32_t *)data);
    out.floatValue[0] = v.x;
    out.floatValue[1] = v.y;
    out.floatValue[2] = v.z;
  }
  else if(fmt.type == ResourceFormatType::D16S8)
  {
    uint32_t val = *(const uint32_t *)data;
    out.floatValue[0] = float(val & 0x00ffff) / 65535.0f;
    out.floatValue[1] = float((val & 0xff0000) >> 16) / 255.0f;
    out.floatValue[2] = 0.0f;
  }
  else if(fmt.type == ResourceFormatType::D24S8)
  {
    uint32_t val = *(const uint32_t *)data;
    out.floatValue[0] = float(val & 0x00ffffff) / 16777215.0f;
    out.floatValue[1] = float((val & 0xff000000) >> 24) / 255.0f;
    out.floatValue[2] = 0.0f;
  }
  else if(fmt.type == ResourceFormatType::D32S8)
  {
    struct ds
    {
      float f;
      uint32_t s;
    } val;
    val = *(const ds *)data;
    out.floatValue[0] = val.f;
    out.floatValue[1] = float(val.s) / 255.0f;
    out.floatValue[2] = 0.0f;
  }
  else if(fmt.type == ResourceFormatType::Regular || fmt.type == ResourceFormatType::A8 ||
          fmt.type == ResourceFormatType::S8)
  {
    CompType compType = fmt.compType;
    for(size_t c = 0; c < fmt.compCount; c++)
    {
      // alpha is never interpreted as sRGB
      if(compType == CompType::UNormSRGB && c == 3)
        compType = CompType::UNorm;

      if(fmt.compByteWidth == 8)
      {
        // we just downcast
        const uint64_t *u64 = (const uint64_t *)data;
        const int64_t *i64 = (const int64_t *)data;

        if(compType == CompType::Float)
        {
          out.floatValue[c] = float(*(const double *)u64);
        }
        else if(compType == CompType::UInt)
        {
          out.uintValue[c] = uint32_t(*u64);
        }
        else if(compType == CompType::UScaled)
        {
          out.floatValue[c] = float(*u64);
        }
        else if(compType == CompType::SInt)
        {
          out.intValue[c] = int32_t(*i64);
        }
        else if(compType == CompType::SScaled)
        {
          out.floatValue[c] = float(*i64);
        }
      }
      else if(fmt.compByteWidth == 4)
      {
        const uint32_t *u32 = (const uint32_t *)data;
        const int32_t *i32 = (const int32_t *)data;

        if(compType == CompType::Float || compType == CompType::Depth)
        {
          out.floatValue[c] = *(const float *)u32;
        }
        else if(compType == CompType::UInt)
        {
          out.uintValue[c] = uint32_t(*u32);
        }
        else if(compType == CompType::UScaled)
        {
          out.floatValue[c] = float(*u32);
        }
        else if(compType == CompType::SInt)
        {
          out.intValue[c] = int32_t(*i32);
        }
        else if(compType == CompType::SScaled)
        {
          out.floatValue[c] = float(*i32);
        }
      }
      else if(fmt.compByteWidth == 3 && compType == CompType::Depth)
      {
        // 24-bit depth is a weird edge case we need to assemble it by hand
        const uint8_t *u8 = (const uint8_t *)data;

        uint32_t depth = 0;
        depth |= uint32_t(u8[0]);
        depth |= uint32_t(u8[1]) << 8;
        depth |= uint32_t(u8[2]) << 16;

        out.floatValue[c] = float(depth) / float(16777215.0f);
      }
      else if(fmt.compByteWidth == 2)
      {
        const uint16_t *u16 = (const uint16_t *)data;
        const int16_t *i16 = (const int16_t *)data;

        if(compType == CompType::Float)
        {
          out.floatValue[c] = ConvertFromHalf(*u16);
        }
        else if(compType == CompType::UInt)
        {
          out.uintValue[c] = uint32_t(*u16);
        }
        else if(compType == CompType::UScaled)
        {
          out.floatValue[c] = float(*u16);
        }
        else if(compType == CompType::SInt)
        {
          out.intValue[c] = int32_t(*i16);
        }
        else if(compType == CompType::SScaled)
        {
          out.floatValue[c] = float(*i16);
        }
        // 16-bit depth is UNORM
        else if(compType == CompType::UNorm || compType == CompType::Depth)
        {
          out.floatValue[c] = float(*u16) / 65535.0f;
        }
        else if(compType == CompType::SNorm)
        {
          float f = -1.0f;

          if(*i16 == -32768)
            f = -1.0f;
          else
            f = ((float)*i16) / 32767.0f;

          out.floatValue[c] = f;
        }
      }
      else if(fmt.compByteWidth == 1)
      {
        const uint8_t *u8 = (const uint8_t *)data;
        const int8_t *i8 = (const int8_t *)data;

        if(compType == CompType::UInt)
        {
          out.uintValue[c] = uint32_t(*u8);
        }
        else if(compType == CompType::UScaled)
        {
          out.floatValue[c] = float(*u8);
        }
        else if(compType == CompType::SInt)
        {
          out.intValue[c] = int32_t(*i8);
        }
        else if(compType == CompType::SScaled)
        {
          out.floatValue[c] = float(*i8);
        }
        else if(compType == CompType::UNormSRGB)
        {
          out.floatValue[c] = ConvertFromSRGB8(*u8);
        }
        else if(compType == CompType::UNorm)
        {
          out.floatValue[c] = float(*u8) / 255.0f;
        }
        else if(compType == CompType::SNorm)
        {
          float f = -1.0f;

          if(*i8 == -128)
            f = -1.0f;
          else
            f = ((float)*i8) / 127.0f;

          out.floatValue[c] = f;
        }
      }
      else
      {
        RDCERR("Unexpected format to convert from %u %u", fmt.compByteWidth, compType);
      }

      data += fmt.compByteWidth;
    }

    if(fmt.type == ResourceFormatType::A8)
    {
      out.floatValue[2] = out.floatValue[0];
      out.floatValue[0] = 0.0f;
    }
    else if(fmt.type == ResourceFormatType::S8)
    {
      out.uintValue[1] = out.uintValue[0];
      out.uintValue[0] = 0;
    }

    // the different types are a union so we can ignore format and just treat it as a data swap
    if(fmt.BGRAOrder())
      std::swap(out.uintValue[0], out.uintValue[2]);
  }
}

void FillInColor(ResourceFormat fmt, const D3D12PixelHistoryValue &value, ModificationValue &mod)
{
  PixelHistoryDecode(fmt, value.color, mod.col);
}

void ConvertAndFillInColor(ResourceFormat srcFmt, ResourceFormat outFmt,
                           const D3D12PixelHistoryValue &value, ModificationValue &mod)
{
  if((outFmt.compType == CompType::UInt) || (outFmt.compType == CompType::SInt))
  {
    PixelHistoryDecode(srcFmt, value.color, mod.col);
    // Clamp values based on format
    if(outFmt.compType == CompType::UInt)
    {
      uint32_t limits[4] = {
          255,
          UINT16_MAX,
          0,
          UINT32_MAX,
      };
      int limit_idx = outFmt.compByteWidth - 1;
      for(size_t c = 0; c < outFmt.compCount; c++)
        mod.col.uintValue[c] = RDCMIN(limits[limit_idx], mod.col.uintValue[c]);
    }
    else
    {
      int32_t limits[8] = {
          INT8_MIN, INT8_MAX, INT16_MIN, INT16_MAX, 0, 0, INT32_MIN, INT32_MAX,
      };
      int limit_idx = 2 * (outFmt.compByteWidth - 1);
      for(size_t c = 0; c < outFmt.compCount; c++)
        mod.col.intValue[c] = RDCCLAMP(mod.col.intValue[c], limits[limit_idx], limits[limit_idx + 1]);
    }
  }
  else
  {
    FloatVector v4 = DecodeFormattedComponents(srcFmt, value.color);
    // To properly handle some cases of component bounds, roundtrip through encoding again
    uint8_t tempColor[32];
    EncodeFormattedComponents(outFmt, v4, (byte *)tempColor);
    v4 = DecodeFormattedComponents(outFmt, tempColor);
    memcpy(mod.col.floatValue.data(), &v4, sizeof(v4));
  }
}

float GetDepthValue(DXGI_FORMAT depthFormat, const D3D12PixelHistoryValue &value)
{
  FloatVector v4 = DecodeFormattedComponents(MakeResourceFormat(depthFormat), (byte *)&value.depth);
  return v4.x;
}

float GetDepthValue(DXGI_FORMAT depthFormat, const byte *pValue)
{
  FloatVector v4 = DecodeFormattedComponents(MakeResourceFormat(depthFormat), pValue);
  return v4.x;
}

}

rdcarray<PixelModification> D3D12Replay::PixelHistory(rdcarray<EventUsage> events,
                                                      ResourceId target, uint32_t x, uint32_t y,
                                                      const Subresource &sub, CompType typeCast)
{
  rdcarray<PixelModification> history;

  RDCCOMPILE_ASSERT(sizeof(D3D12EventInfo) % 16 == 0, "D3D12EventInfo not multiple of 16-bytes");
  RDCCOMPILE_ASSERT(sizeof(D3D12EventInfo) % 12 == 0, "D3D12EventInfo not multiple of 12-bytes");

  RDCCOMPILE_ASSERT(offsetof(D3D12EventInfo, premod) % 16 == 0,
                    "D3D12EventInfo::premod not aligned to 16-bytes");
  RDCCOMPILE_ASSERT(offsetof(D3D12EventInfo, premod) % 12 == 0,
                    "D3D12EventInfo::premod not aligned to 12-bytes");
  RDCCOMPILE_ASSERT(offsetof(D3D12EventInfo, postmod) % 16 == 0,
                    "D3D12EventInfo::postmod not aligned to 16-bytes");
  RDCCOMPILE_ASSERT(offsetof(D3D12EventInfo, postmod) % 12 == 0,
                    "D3D12EventInfo::postmod not aligned to 12-bytes");
  RDCCOMPILE_ASSERT(offsetof(D3D12EventInfo, dsWithoutShaderDiscard) % 16 == 0,
                    "D3D12EventInfo::dsWithoutShaderDiscard not aligned to 16-bytes");
  RDCCOMPILE_ASSERT(offsetof(D3D12EventInfo, dsWithShaderDiscard) % 16 == 0,
                    "D3D12EventInfo::dsWithShaderDiscard not aligned to 16-bytes");

  if(events.empty())
    return history;

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();
  WrappedID3D12Resource *pResource = rm->GetCurrentAs<WrappedID3D12Resource>(target);
  D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
  if(resDesc.Format == DXGI_FORMAT_UNKNOWN)
    return history;

  rdcstr regionName = StringFormat::Fmt(
      "PixelHistory: pixel: (%u, %u) on %s subresource (%u, %u, %u) cast to %s with %zu events", x, y,
      ToStr(target).c_str(), sub.mip, sub.slice, sub.sample, ToStr(typeCast).c_str(), events.size());

  RDCDEBUG("%s", regionName.c_str());

  D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), regionName);

  uint32_t sampleIdx = sub.sample;

  SCOPED_TIMER("D3D12DebugManager::PixelHistory");

  if(sampleIdx > (uint32_t)resDesc.SampleDesc.Count)
    sampleIdx = 0;

  uint32_t sampleMask = ~0U;
  if(sampleIdx < 32)
    sampleMask = 1U << sampleIdx;

  bool multisampled = (resDesc.SampleDesc.Count > 1);

  if(sampleIdx == ~0U || !multisampled)
    sampleIdx = 0;

  ID3D12QueryHeap *pOcclusionQueryHeap = NULL;
  if(!CreateOcclusionPool(m_pDevice, (uint32_t)events.size(), &pOcclusionQueryHeap))
  {
    return history;
  }

  // If the resource desc format is typeless, replace it with a typed format
  if(IsTypelessFormat(resDesc.Format))
    resDesc.Format = GetTypedFormat(resDesc.Format, typeCast);

  // TODO: perhaps should allocate most resources after D3D12OcclusionCallback, since we will
  // get a smaller subset of events that passed the occlusion query.
  D3D12PixelHistoryResources resources = {};
  if(!GetDebugManager()->PixelHistorySetupResources(resources, pResource, resDesc,
                                                    (uint32_t)events.size()))
  {
    SAFE_RELEASE(pOcclusionQueryHeap);
    return history;
  }

  D3D12PixelHistoryShaderCache *shaderCache = new D3D12PixelHistoryShaderCache(
      m_pDevice, m_PixelHistory.PrimitiveIDPS, m_PixelHistory.PrimitiveIDPSDxil,
      m_PixelHistory.FixedColorPS, m_PixelHistory.FixedColorPSDxil);

  D3D12PixelHistoryCallbackInfo callbackInfo = {};
  callbackInfo.targetImage = pResource;
  callbackInfo.targetDesc = resDesc;
  callbackInfo.targetSubresource = sub;
  callbackInfo.compType = typeCast;
  callbackInfo.x = x;
  callbackInfo.y = y;
  callbackInfo.sampleMask = sampleMask;
  callbackInfo.colorImage = resources.colorImage;
  callbackInfo.colorDescriptor = resources.colorDescriptor;
  callbackInfo.dsImage = resources.dsImage;
  callbackInfo.dsDescriptor = resources.dsDescriptor;

  callbackInfo.dstBuffer = resources.dstBuffer;

  D3D12OcclusionCallback occlCb(m_pDevice, shaderCache, callbackInfo, pOcclusionQueryHeap, events);
  {
    D3D12MarkerRegion occlRegion(m_pDevice->GetQueue()->GetReal(), "D3D12OcclusionCallback");
    m_pDevice->ReplayLog(0, events.back().eventId, eReplay_Full);
    m_pDevice->FlushLists(true);
    occlCb.FetchOcclusionResults();
  }

  // Gather all draw events that could have written to pixel for another replay pass,
  // to determine if these draws failed for some reason (for ex., depth test).
  rdcarray<uint32_t> modEvents;
  rdcarray<uint32_t> drawEvents;
  std::map<uint32_t, D3D12_RESOURCE_STATES> resourceStates;
  for(size_t ev = 0; ev < events.size(); ev++)
  {
    ResourceUsage usage = events[ev].usage;
    bool clear = (usage == ResourceUsage::Clear);
    bool directWrite = IsDirectWrite(events[ev].usage);

    D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    if(IsUavWrite(usage))
      resourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    else if(IsResolveWrite(usage))
      resourceState = D3D12_RESOURCE_STATE_RESOLVE_DEST;
    else if(IsCopyWrite(usage))
      resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
    resourceStates[events[ev].eventId] = resourceState;

    if(directWrite || clear)
    {
      modEvents.push_back(events[ev].eventId);
    }
    else
    {
      uint64_t occlData = occlCb.GetOcclusionResult((uint32_t)events[ev].eventId);
      if(occlData > 0)
      {
        D3D12MarkerRegion::Set(m_pDevice->GetQueue()->GetReal(),
                               StringFormat::Fmt("%u has occl %llu", events[ev].eventId, occlData));
        drawEvents.push_back(events[ev].eventId);
        modEvents.push_back(events[ev].eventId);
      }
    }
  }

  D3D12ColorAndStencilCallback cb(m_pDevice, shaderCache, callbackInfo, modEvents, resourceStates);
  {
    D3D12MarkerRegion colorStencilRegion(m_pDevice->GetQueue()->GetReal(),
                                         "D3D12ColorAndStencilCallback");
    m_pDevice->ReplayLog(0, events.back().eventId, eReplay_Full);
    m_pDevice->FlushLists(true);
  }

  // If there are any draw events, do another replay pass, in order to figure
  // out which tests failed for each draw event.
  D3D12TestsFailedCallback *tfCb = NULL;
  if(drawEvents.size() > 0)
  {
    D3D12MarkerRegion testsRegion(m_pDevice->GetQueue()->GetReal(), "D3D12TestsFailedCallback");

    ID3D12QueryHeap *pTfOcclusionQueryHeap = NULL;
    if(!CreateOcclusionPool(m_pDevice, (uint32_t)drawEvents.size() * 6, &pTfOcclusionQueryHeap))
    {
      GetDebugManager()->PixelHistoryDestroyResources(resources);
      SAFE_RELEASE(pOcclusionQueryHeap);
      SAFE_DELETE(shaderCache);
      return history;
    }

    tfCb = new D3D12TestsFailedCallback(m_pDevice, shaderCache, callbackInfo, pTfOcclusionQueryHeap,
                                        drawEvents);
    m_pDevice->ReplayLog(0, events.back().eventId, eReplay_Full);
    m_pDevice->FlushLists(true);
    tfCb->FetchOcclusionResults();
    SAFE_RELEASE(pTfOcclusionQueryHeap);
  }

  for(size_t ev = 0; ev < events.size(); ev++)
  {
    uint32_t eventId = events[ev].eventId;
    bool clear = (events[ev].usage == ResourceUsage::Clear);
    bool directWrite = IsDirectWrite(events[ev].usage);

    if(drawEvents.contains(events[ev].eventId) || clear || directWrite)
    {
      PixelModification mod;
      RDCEraseEl(mod);

      mod.eventId = eventId;
      mod.directShaderWrite = directWrite;
      mod.unboundPS = false;

      if(!clear && !directWrite)
      {
        RDCASSERT(tfCb != NULL);
        uint32_t flags = tfCb->GetEventFlags(eventId);
        D3D12MarkerRegion::Set(m_pDevice->GetQueue()->GetReal(),
                               StringFormat::Fmt("%u has flags %x", eventId, flags));

        if(flags & TestMustFail_DepthTesting)
          mod.depthTestFailed = true;
        if(flags & TestMustFail_Scissor)
          mod.scissorClipped = true;
        if(flags & TestMustFail_SampleMask)
          mod.sampleMasked = true;
        if(flags & UnboundFragmentShader)
          mod.unboundPS = true;

        D3D12UpdateTestsFailed(tfCb, eventId, flags, mod);
      }
      history.push_back(mod);
    }
  }

  // Try to read memory back for stencil results
  bytebuf eventData;
  GetDebugManager()->GetBufferData(resources.dstBuffer, 0, 0, eventData);
  const D3D12EventInfo *eventsInfo = (const D3D12EventInfo *)eventData.data();

  std::map<uint32_t, uint32_t> eventsWithFrags;
  std::map<uint32_t, ModificationValue> eventPremods;
  ResourceFormat fmt = MakeResourceFormat(callbackInfo.targetDesc.Format);
  if(typeCast != CompType::Typeless)
    fmt.compType = typeCast;

  for(size_t h = 0; h < history.size();)
  {
    PixelModification &mod = history[h];

    int32_t eventIndex = cb.GetEventIndex(mod.eventId);
    if(eventIndex == -1)
    {
      // There is no information, skip the event.
      mod.preMod.SetInvalid();
      mod.postMod.SetInvalid();
      mod.shaderOut.SetInvalid();
      h++;
      continue;
    }

    const D3D12EventInfo &ei = eventsInfo[eventIndex];

    if(multisampled)
    {
      // If the resource uses MSAA, the copy pixel already expands it to floats
      // TODO: Need to verify this works as expected with uint/int MSAA targets
      memcpy(mod.preMod.col.floatValue.data(), &ei.premod.color[0],
             mod.preMod.col.floatValue.byteSize());
      memcpy(mod.postMod.col.floatValue.data(), &ei.postmod.color[0],
             mod.postMod.col.floatValue.byteSize());
    }
    else
    {
      FillInColor(fmt, ei.premod, mod.preMod);
      FillInColor(fmt, ei.postmod, mod.postMod);
    }

    DXGI_FORMAT depthFormat = cb.GetDepthFormat(mod.eventId);
    if(depthFormat != DXGI_FORMAT_UNKNOWN)
    {
      mod.preMod.stencil = ei.premod.stencil;
      mod.postMod.stencil = ei.postmod.stencil;
      if(multisampled)
      {
        mod.preMod.depth = ei.premod.depth.fdepth;
        mod.postMod.depth = ei.postmod.depth.fdepth;
      }
      else
      {
        mod.preMod.depth = GetDepthValue(depthFormat, ei.premod);
        mod.postMod.depth = GetDepthValue(depthFormat, ei.postmod);
      }
    }

    int32_t frags = int32_t(ei.dsWithoutShaderDiscard[0]);
    int32_t fragsClipped = int32_t(ei.dsWithShaderDiscard[0]);
    mod.shaderOut.col.intValue[0] = frags;
    mod.shaderOut.col.intValue[1] = fragsClipped;
    bool someFragsClipped = (fragsClipped < frags);
    mod.primitiveID = someFragsClipped;

    if(frags > 0)
    {
      eventsWithFrags[mod.eventId] = frags;
      eventPremods[mod.eventId] = mod.preMod;
    }

    for(int32_t f = 1; f < frags; f++)
    {
      history.insert(h + 1, mod);
    }
    for(int32_t f = 0; f < frags; f++)
      history[h + f].fragIndex = f;
    h += RDCMAX(1, frags);
    RDCDEBUG(
        "PixelHistory event id: %u, fixed shader stencilValue = %u, "
        "original shader stencilValue = "
        "%u",
        mod.eventId, ei.dsWithoutShaderDiscard[0], ei.dsWithShaderDiscard[0]);
  }

  if(eventsWithFrags.size() > 0)
  {
    // Replay to get shader output value, post modification value and primitive ID for every fragment
    D3D12PixelHistoryPerFragmentCallback perFragmentCB(m_pDevice, shaderCache, callbackInfo,
                                                       eventsWithFrags, eventPremods);
    {
      D3D12MarkerRegion perFragRegion(m_pDevice->GetQueue()->GetReal(),
                                      "D3D12PixelHistoryPerFragmentCallback");
      m_pDevice->ReplayLog(0, eventsWithFrags.rbegin()->first, eReplay_Full);
      m_pDevice->FlushLists(true);
    }

    bytebuf fragData;
    GetDebugManager()->GetBufferData(resources.dstBuffer, 0, 0, fragData);
    const D3D12PerFragmentInfo *fragInfo = (const D3D12PerFragmentInfo *)fragData.data();

    // Retrieve primitive ID values where fragment shader discarded some fragments. For these
    // primitives we are going to perform an occlusion query to see if a primitive was discarded.
    std::map<uint32_t, rdcarray<int32_t>> discardedPrimsEvents;
    uint32_t primitivesToCheck = 0;
    for(size_t h = 0; h < history.size(); h++)
    {
      uint32_t eid = history[h].eventId;
      if(eventsWithFrags.find(eid) == eventsWithFrags.end())
        continue;
      uint32_t f = history[h].fragIndex;
      bool someFragsClipped = (history[h].primitiveID == 1);
      int32_t primId = fragInfo[perFragmentCB.GetEventOffset(eid) + f].primitiveID;
      history[h].primitiveID = primId;
      if(someFragsClipped)
      {
        discardedPrimsEvents[eid].push_back(primId);
        primitivesToCheck++;
      }
    }

    if(primitivesToCheck > 0)
    {
      D3D12MarkerRegion discardedRegion(m_pDevice->GetQueue()->GetReal(),
                                        "D3D12PixelHistoryDiscardedFragmentsCallback");

      ID3D12QueryHeap *pDiscardedFragsOcclusionQueryHeap = NULL;
      if(!CreateOcclusionPool(m_pDevice, primitivesToCheck, &pDiscardedFragsOcclusionQueryHeap))
      {
        GetDebugManager()->PixelHistoryDestroyResources(resources);
        SAFE_RELEASE(pOcclusionQueryHeap);
        SAFE_DELETE(shaderCache);
        return history;
      }

      D3D12PixelHistoryDiscardedFragmentsCallback discardedCb(m_pDevice, shaderCache, callbackInfo,
                                                              discardedPrimsEvents,
                                                              pDiscardedFragsOcclusionQueryHeap);

      m_pDevice->ReplayLog(0, events.back().eventId, eReplay_Full);
      m_pDevice->FlushLists(true);
      discardedCb.FetchOcclusionResults();
      SAFE_RELEASE(pDiscardedFragsOcclusionQueryHeap);

      for(size_t h = 0; h < history.size(); h++)
      {
        history[h].shaderDiscarded =
            discardedCb.PrimitiveDiscarded(history[h].eventId, history[h].primitiveID);
      }
    }

    uint32_t discardOffset = 0;
    ResourceFormat shaderOutFormat = MakeResourceFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
    for(size_t h = 0; h < history.size(); h++)
    {
      uint32_t eid = history[h].eventId;
      uint32_t f = history[h].fragIndex;
      // Reset discard offset if this is a new event.
      if(h > 0 && (eid != history[h - 1].eventId))
        discardOffset = 0;
      if(eventsWithFrags.find(eid) != eventsWithFrags.end())
      {
        if(history[h].shaderDiscarded)
        {
          discardOffset++;
          // Copy previous post-mod value if its not the first event
          if(h > 0)
            history[h].postMod = history[h - 1].postMod;
          continue;
        }
        uint32_t offset = perFragmentCB.GetEventOffset(eid) + f - discardOffset;
        if(multisampled)
          memcpy(history[h].shaderOut.col.floatValue.data(), &fragInfo[offset].shaderOut.color[0],
                 history[h].shaderOut.col.floatValue.byteSize());
        else
          FillInColor(shaderOutFormat, fragInfo[offset].shaderOut, history[h].shaderOut);

        if(multisampled)
          history[h].shaderOut.depth = fragInfo[offset].shaderOut.depth.fdepth;
        else
          history[h].shaderOut.depth =
              GetDepthValue(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, fragInfo[offset].shaderOut);

        if((h < history.size() - 1) && (history[h].eventId == history[h + 1].eventId))
        {
          // Get post-modification value if this is not the last fragment for the event.
          ConvertAndFillInColor(shaderOutFormat, fmt, fragInfo[offset].postMod, history[h].postMod);

          // MSAA depth is expanded out to floats in the compute shader
          if(multisampled)
            history[h].postMod.depth = fragInfo[offset].postMod.depth.fdepth;
          else
            history[h].postMod.depth =
                GetDepthValue(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, fragInfo[offset].postMod);
          history[h].postMod.stencil = -2;
        }
        // If it is not the first fragment for the event, set the preMod to the
        // postMod of the previous fragment.
        if(h > 0 && (history[h].eventId == history[h - 1].eventId))
        {
          history[h].preMod = history[h - 1].postMod;
        }
      }

      // Check the depth value between premod/shaderout against the known test if we have valid
      // depth values, as we don't have per-fragment depth test information.
      if(history[h].preMod.depth >= 0.0f && history[h].shaderOut.depth >= 0.0f && tfCb &&
         tfCb->HasEventFlags(history[h].eventId))
      {
        uint32_t flags = tfCb->GetEventFlags(history[h].eventId);

        flags &= 0x7 << DepthTest_Shift;

        DXGI_FORMAT dfmt = cb.GetDepthFormat(eid);
        float shadDepth = history[h].shaderOut.depth;

        // Quantize depth to match before comparing
        if(dfmt == DXGI_FORMAT_D24_UNORM_S8_UINT || dfmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT ||
           dfmt == DXGI_FORMAT_R24_UNORM_X8_TYPELESS || dfmt == DXGI_FORMAT_R24G8_TYPELESS)
        {
          shadDepth = float(uint32_t(float(shadDepth * 0xffffff))) / float(0xffffff);
        }
        else if(dfmt == DXGI_FORMAT_D16_UNORM || dfmt == DXGI_FORMAT_R16_TYPELESS ||
                dfmt == DXGI_FORMAT_R16_UNORM)
        {
          shadDepth = float(uint32_t(float(shadDepth * 0xffff))) / float(0xffff);
        }

        bool passed = true;
        if(flags == DepthTest_Equal)
          passed = (shadDepth == history[h].preMod.depth);
        else if(flags == DepthTest_NotEqual)
          passed = (shadDepth != history[h].preMod.depth);
        else if(flags == DepthTest_Less)
          passed = (shadDepth < history[h].preMod.depth);
        else if(flags == DepthTest_LessEqual)
          passed = (shadDepth <= history[h].preMod.depth);
        else if(flags == DepthTest_Greater)
          passed = (shadDepth > history[h].preMod.depth);
        else if(flags == DepthTest_GreaterEqual)
          passed = (shadDepth >= history[h].preMod.depth);

        if(!passed)
          history[h].depthTestFailed = true;

        rdcpair<float, float> depthBounds = tfCb->GetEventDepthBounds(history[h].eventId);

        if((history[h].preMod.depth < depthBounds.first ||
            history[h].preMod.depth > depthBounds.second) &&
           depthBounds.second > depthBounds.first)
          history[h].depthBoundsFailed = true;
      }
    }
  }

  SAFE_DELETE(tfCb);

  SAFE_RELEASE(pOcclusionQueryHeap);

  GetDebugManager()->PixelHistoryDestroyResources(resources);
  SAFE_DELETE(shaderCache);

  return history;
}
