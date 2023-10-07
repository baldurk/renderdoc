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

#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"

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
