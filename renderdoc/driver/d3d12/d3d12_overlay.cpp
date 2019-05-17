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

#include "common/shader_cache.h"
#include "data/resource.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "stb/stb_truetype.h"
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

struct D3D12QuadOverdrawCallback : public D3D12DrawcallCallback
{
  D3D12QuadOverdrawCallback(WrappedID3D12Device *dev, D3D12_SHADER_BYTECODE quadWrite,
                            const std::vector<uint32_t> &events, PortableHandle uav)
      : m_pDevice(dev), m_QuadWritePS(quadWrite), m_Events(events), m_UAV(uav)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = this;
  }
  ~D3D12QuadOverdrawCallback()
  {
    m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = NULL;
  }
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandList4 *cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) == m_Events.end())
      return;

    // we customise the pipeline to disable framebuffer writes, but perform normal testing
    // and substitute our quad calculation fragment shader that writes to a storage image
    // that is bound in a new root signature element.

    D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
    m_PrevState = rs;

    // check cache first
    CachedPipeline cache = m_PipelineCache[rs.pipe];

    // if we don't get a hit, create a modified pipeline
    if(cache.pipe == NULL)
    {
      HRESULT hr = S_OK;

      WrappedID3D12RootSignature *sig =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
              rs.graphics.rootsig);

      // need to be able to add a descriptor table with our UAV without hitting the 64 DWORD limit
      RDCASSERT(sig->sig.dwordLength < 64);

      D3D12RootSignature modsig = sig->sig;

      // make sure no other UAV tables overlap. We can't remove elements entirely because then the
      // root signature indices wouldn't match up as expected.
      // Instead move them into an unused space.
      for(size_t i = 0; i < modsig.params.size(); i++)
      {
        if(modsig.params[i].ShaderVisibility == D3D12_SHADER_VISIBILITY_PIXEL ||
           modsig.params[i].ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL)
        {
          // use different register spaces for each just in case
          UINT regSpace = modsig.maxSpaceIndex + modsig.params[i].ShaderVisibility;

          if(modsig.params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV)
          {
            modsig.params[i].Descriptor.RegisterSpace = regSpace;
          }
          else if(modsig.params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
          {
            for(size_t r = 0; r < modsig.params[i].ranges.size(); r++)
            {
              modsig.params[i].ranges[r].RegisterSpace = regSpace;
            }
          }
        }
      }

      D3D12_DESCRIPTOR_RANGE1 range;
      range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      range.NumDescriptors = 1;
      range.BaseShaderRegister = 0;
      range.RegisterSpace = 0;
      range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
      range.OffsetInDescriptorsFromTableStart = 0;

      modsig.params.push_back(D3D12RootSignatureParameter());
      D3D12RootSignatureParameter &param = modsig.params.back();
      param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
      param.DescriptorTable.NumDescriptorRanges = 1;
      param.DescriptorTable.pDescriptorRanges = &range;

      cache.sigElem = uint32_t(modsig.params.size() - 1);

      std::vector<D3D12_ROOT_PARAMETER1> params;
      params.resize(modsig.params.size());
      for(size_t i = 0; i < params.size(); i++)
        params[i] = modsig.params[i];

      ID3DBlob *root = m_pDevice->GetShaderCache()->MakeRootSig(modsig);

      hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                          __uuidof(ID3D12RootSignature), (void **)&cache.sig);
      RDCASSERTEQUAL(hr, S_OK);

      SAFE_RELEASE(root);

      WrappedID3D12PipelineState *origPSO =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

      RDCASSERT(origPSO->IsGraphics());

      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
      origPSO->Fill(pipeDesc);

      for(size_t i = 0; i < ARRAY_COUNT(pipeDesc.BlendState.RenderTarget); i++)
        pipeDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = 0;

      // disable depth/stencil writes
      pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      pipeDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.StencilWriteMask = 0;

      pipeDesc.PS = m_QuadWritePS;

      pipeDesc.pRootSignature = cache.sig;

      hr = m_pDevice->CreatePipeState(pipeDesc, &cache.pipe);
      RDCASSERTEQUAL(hr, S_OK);

      m_PipelineCache[rs.pipe] = cache;
    }

    // modify state for first draw call
    rs.pipe = GetResID(cache.pipe);
    rs.graphics.rootsig = GetResID(cache.sig);

    if(rs.graphics.sigelems.size() <= cache.sigElem)
      rs.graphics.sigelems.resize(cache.sigElem + 1);

    PortableHandle uav = m_UAV;

    // if a CBV_SRV_UAV heap is already set, we need to copy our descriptor in
    // if we haven't already. Otherwise we can set our own heap.
    for(size_t i = 0; i < rs.heaps.size(); i++)
    {
      WrappedID3D12DescriptorHeap *h =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(rs.heaps[i]);
      if(h->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
      {
        // use the last descriptor
        D3D12_CPU_DESCRIPTOR_HANDLE dst = h->GetCPUDescriptorHandleForHeapStart();
        dst.ptr += (h->GetDesc().NumDescriptors - 1) * sizeof(D3D12Descriptor);

        if(m_CopiedHeaps.find(rs.heaps[i]) == m_CopiedHeaps.end())
        {
          WrappedID3D12DescriptorHeap *h2 =
              m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(m_UAV.heap);
          D3D12_CPU_DESCRIPTOR_HANDLE src = h2->GetCPUDescriptorHandleForHeapStart();
          src.ptr += m_UAV.index * sizeof(D3D12Descriptor);

          // can't do a copy because the src heap is CPU write-only (shader visible). So instead,
          // create directly
          D3D12Descriptor *srcDesc = (D3D12Descriptor *)src.ptr;
          srcDesc->Create(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_pDevice, dst);

          m_CopiedHeaps.insert(rs.heaps[i]);
        }

        uav = ToPortableHandle(dst);

        break;
      }
    }

    if(uav.heap == m_UAV.heap)
      rs.heaps.push_back(m_UAV.heap);

    rs.graphics.sigelems[cache.sigElem] =
        D3D12RenderState::SignatureElement(eRootTable, uav.heap, uav.index);

    // as we're changing the root signature, we need to reapply all elements,
    // so just apply all state
    if(cmd)
      rs.ApplyState(m_pDevice, cmd);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandList4 *cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) == m_Events.end())
      return false;

    // restore the render state and go ahead with the real draw
    m_pDevice->GetQueue()->GetCommandData()->m_RenderState = m_PrevState;

    RDCASSERT(cmd);
    m_pDevice->GetQueue()->GetCommandData()->m_RenderState.ApplyState(m_pDevice, cmd);

    return true;
  }

  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandList4 *cmd)
  {
    // nothing to do
  }

  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandList4 *cmd) {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandList4 *cmd) { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandList4 *cmd) {}
  void PreCloseCommandList(ID3D12GraphicsCommandList4 *cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // don't care
  }

  WrappedID3D12Device *m_pDevice;
  D3D12_SHADER_BYTECODE m_QuadWritePS;
  const std::vector<uint32_t> &m_Events;
  PortableHandle m_UAV;

  // cache modified pipelines
  struct CachedPipeline
  {
    ID3D12RootSignature *sig;
    uint32_t sigElem;
    ID3D12PipelineState *pipe;
  };
  std::map<ResourceId, CachedPipeline> m_PipelineCache;
  std::set<ResourceId> m_CopiedHeaps;
  D3D12RenderState m_PrevState;
};

ResourceId D3D12Replay::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                      uint32_t eventId, const std::vector<uint32_t> &passEvents)
{
  ID3D12Resource *resource = WrappedID3D12Resource1::GetList()[texid];

  if(resource == NULL)
    return ResourceId();

  D3D12MarkerRegion renderoverlay(m_pDevice->GetQueue(),
                                  StringFormat::Fmt("RenderOverlay %d", overlay));

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  std::vector<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  GetDebugManager()->PrepareTextureSampling(resource, typeHint, resType, barriers);

  D3D12_RESOURCE_DESC overlayTexDesc;
  overlayTexDesc.Alignment = 0;
  overlayTexDesc.DepthOrArraySize = 1;
  overlayTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  overlayTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  overlayTexDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  overlayTexDesc.Height = resourceDesc.Height;
  overlayTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  overlayTexDesc.MipLevels = 1;
  overlayTexDesc.SampleDesc = resourceDesc.SampleDesc;
  overlayTexDesc.Width = resourceDesc.Width;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC currentOverlayDesc;
  RDCEraseEl(currentOverlayDesc);
  if(m_Overlay.Texture)
    currentOverlayDesc = m_Overlay.Texture->GetDesc();

  WrappedID3D12Resource1 *wrappedCustomRenderTex = (WrappedID3D12Resource1 *)m_Overlay.Texture;

  // need to recreate backing custom render tex
  if(overlayTexDesc.Width != currentOverlayDesc.Width ||
     overlayTexDesc.Height != currentOverlayDesc.Height ||
     overlayTexDesc.Format != currentOverlayDesc.Format ||
     overlayTexDesc.SampleDesc.Count != currentOverlayDesc.SampleDesc.Count ||
     overlayTexDesc.SampleDesc.Quality != currentOverlayDesc.SampleDesc.Quality)
  {
    SAFE_RELEASE(m_Overlay.Texture);
    m_Overlay.resourceId = ResourceId();

    ID3D12Resource *customRenderTex = NULL;
    HRESULT hr = m_pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &overlayTexDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
        __uuidof(ID3D12Resource), (void **)&customRenderTex);
    if(FAILED(hr))
    {
      RDCERR("Failed to create custom render tex HRESULT: %s", ToStr(hr).c_str());
      return ResourceId();
    }
    wrappedCustomRenderTex = (WrappedID3D12Resource1 *)customRenderTex;

    customRenderTex->SetName(L"customRenderTex");

    m_Overlay.Texture = wrappedCustomRenderTex;
    m_Overlay.resourceId = wrappedCustomRenderTex->GetResourceID();
  }

  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  ID3D12Resource *renderDepth = NULL;

  D3D12Descriptor dsView = rs.dsv;

  D3D12_RESOURCE_DESC depthTexDesc = {};
  D3D12_DEPTH_STENCIL_VIEW_DESC dsViewDesc = {};
  if(dsView.GetResResourceId() != ResourceId())
  {
    ID3D12Resource *realDepth =
        m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(dsView.GetResResourceId());

    dsViewDesc = dsView.GetDSV();

    depthTexDesc = realDepth->GetDesc();
    depthTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    depthTexDesc.Alignment = 0;

    HRESULT hr = S_OK;

    hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthTexDesc,
                                            D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                            __uuidof(ID3D12Resource), (void **)&renderDepth);
    if(FAILED(hr))
    {
      RDCERR("Failed to create renderDepth HRESULT: %s", ToStr(hr).c_str());
      return m_Overlay.resourceId;
    }

    renderDepth->SetName(L"Overlay renderDepth");

    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

    const std::vector<D3D12_RESOURCE_STATES> &states =
        m_pDevice->GetSubresourceStates(GetResID(realDepth));

    std::vector<D3D12_RESOURCE_BARRIER> depthBarriers;
    depthBarriers.reserve(states.size());
    for(size_t i = 0; i < states.size(); i++)
    {
      D3D12_RESOURCE_BARRIER b;

      // skip unneeded barriers
      if(states[i] & D3D12_RESOURCE_STATE_COPY_SOURCE)
        continue;

      b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      b.Transition.pResource = realDepth;
      b.Transition.Subresource = (UINT)i;
      b.Transition.StateBefore = states[i];
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

      depthBarriers.push_back(b);
    }

    if(!depthBarriers.empty())
      list->ResourceBarrier((UINT)depthBarriers.size(), &depthBarriers[0]);

    list->CopyResource(renderDepth, realDepth);

    for(size_t i = 0; i < depthBarriers.size(); i++)
      std::swap(depthBarriers[i].Transition.StateBefore, depthBarriers[i].Transition.StateAfter);

    if(!depthBarriers.empty())
      list->ResourceBarrier((UINT)depthBarriers.size(), &depthBarriers[0]);

    D3D12_RESOURCE_BARRIER b = {};

    b.Transition.pResource = renderDepth;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    // prepare tex resource for copying
    list->ResourceBarrier(1, &b);

    list->Close();
  }

  D3D12_RENDER_TARGET_VIEW_DESC rtDesc = {};
  rtDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  rtDesc.Texture2D.MipSlice = 0;
  rtDesc.Texture2D.PlaneSlice = 0;

  if(overlayTexDesc.SampleDesc.Count > 1 || overlayTexDesc.SampleDesc.Quality > 0)
    rtDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetDebugManager()->GetCPUHandle(OVERLAY_RTV);

  m_pDevice->CreateRenderTargetView(wrappedCustomRenderTex, &rtDesc, rtv);

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

  FLOAT black[] = {0.0f, 0.0f, 0.0f, 0.0f};
  list->ClearRenderTargetView(rtv, black, 0, NULL);

  D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};

  if(renderDepth)
  {
    dsv = GetDebugManager()->GetCPUHandle(OVERLAY_DSV);
    m_pDevice->CreateDepthStencilView(
        renderDepth, dsViewDesc.Format == DXGI_FORMAT_UNKNOWN ? NULL : &dsViewDesc, dsv);
  }

  D3D12_DEPTH_STENCIL_DESC dsDesc;

  dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
      dsDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
  dsDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
      dsDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
  dsDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  dsDesc.DepthEnable = TRUE;
  dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  dsDesc.StencilEnable = FALSE;
  dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

  WrappedID3D12PipelineState *pipe = NULL;

  if(rs.pipe != ResourceId())
    pipe = m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    // just need the basic texture
  }
  else if(overlay == DebugOverlay::Drawcall)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
      pipe->Fill(psoDesc);

      float overlayConsts[4] = {0.8f, 0.1f, 0.8f, 1.0f};
      ID3DBlob *ps = m_pDevice->GetShaderCache()->MakeFixedColShader(overlayConsts);

      psoDesc.PS.pShaderBytecode = ps->GetBufferPointer();
      psoDesc.PS.BytecodeLength = ps->GetBufferSize();

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;
      psoDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      RDCEraseEl(psoDesc.RTVFormats.RTFormats);
      psoDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      psoDesc.RTVFormats.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.MultisampleEnable = FALSE;
      psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;

      float clearColour[] = {0.0f, 0.0f, 0.0f, 0.5f};
      list->ClearRenderTargetView(rtv, clearColour, 0, NULL);

      list->Close();
      list = NULL;

      ID3D12PipelineState *pso = NULL;
      HRESULT hr = m_pDevice->CreatePipeState(psoDesc, &pso);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(ps);
        return m_Overlay.resourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(pso);
      rs.rts.resize(1);
      rs.rts[0] = *GetWrapped(rtv);
      RDCEraseEl(rs.dsv);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs = prev;

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      SAFE_RELEASE(pso);
      SAFE_RELEASE(ps);
    }
  }
  else if(overlay == DebugOverlay::BackfaceCull)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
      pipe->Fill(psoDesc);

      D3D12_CULL_MODE origCull = psoDesc.RasterizerState.CullMode;
      BOOL origFrontCCW = psoDesc.RasterizerState.FrontCounterClockwise;

      float redCol[4] = {1.0f, 0.0f, 0.0f, 1.0f};
      ID3DBlob *red = m_pDevice->GetShaderCache()->MakeFixedColShader(redCol);

      float greenCol[4] = {0.0f, 1.0f, 0.0f, 1.0f};
      ID3DBlob *green = m_pDevice->GetShaderCache()->MakeFixedColShader(greenCol);

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      RDCEraseEl(psoDesc.RTVFormats.RTFormats);
      psoDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      psoDesc.RTVFormats.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.MultisampleEnable = FALSE;
      psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;

      psoDesc.PS.pShaderBytecode = red->GetBufferPointer();
      psoDesc.PS.BytecodeLength = red->GetBufferSize();

      list->Close();
      list = NULL;

      ID3D12PipelineState *redPSO = NULL;
      HRESULT hr = m_pDevice->CreatePipeState(psoDesc, &redPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        return m_Overlay.resourceId;
      }

      psoDesc.RasterizerState.CullMode = origCull;
      psoDesc.RasterizerState.FrontCounterClockwise = origFrontCCW;
      psoDesc.PS.pShaderBytecode = green->GetBufferPointer();
      psoDesc.PS.BytecodeLength = green->GetBufferSize();

      ID3D12PipelineState *greenPSO = NULL;
      hr = m_pDevice->CreatePipeState(psoDesc, &greenPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(red);
        SAFE_RELEASE(redPSO);
        SAFE_RELEASE(green);
        return m_Overlay.resourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(redPSO);
      rs.rts.resize(1);
      rs.rts[0] = *GetWrapped(rtv);
      RDCEraseEl(rs.dsv);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs.pipe = GetResID(greenPSO);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs = prev;

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      SAFE_RELEASE(red);
      SAFE_RELEASE(green);
      SAFE_RELEASE(redPSO);
      SAFE_RELEASE(greenPSO);
    }
  }
  else if(overlay == DebugOverlay::Wireframe)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
      pipe->Fill(psoDesc);

      float overlayConsts[] = {200.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 1.0f};
      ID3DBlob *ps = m_pDevice->GetShaderCache()->MakeFixedColShader(overlayConsts);

      psoDesc.PS.pShaderBytecode = ps->GetBufferPointer();
      psoDesc.PS.BytecodeLength = ps->GetBufferSize();

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      RDCEraseEl(psoDesc.RTVFormats.RTFormats);
      psoDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      psoDesc.RTVFormats.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.MultisampleEnable = FALSE;
      psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;

      overlayConsts[3] = 0.0f;
      list->ClearRenderTargetView(rtv, overlayConsts, 0, NULL);

      list->Close();
      list = NULL;

      ID3D12PipelineState *pso = NULL;
      HRESULT hr = m_pDevice->CreatePipeState(psoDesc, &pso);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(ps);
        return m_Overlay.resourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(pso);
      rs.rts.resize(1);
      rs.rts[0] = *GetWrapped(rtv);
      RDCEraseEl(rs.dsv);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs = prev;

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      SAFE_RELEASE(pso);
      SAFE_RELEASE(ps);
    }
  }
  else if(overlay == DebugOverlay::ClearBeforePass || overlay == DebugOverlay::ClearBeforeDraw)
  {
    std::vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty())
    {
      list->Close();
      list = NULL;

      std::vector<D3D12Descriptor> rts = rs.rts;

      if(overlay == DebugOverlay::ClearBeforePass)
        m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

      list = m_pDevice->GetNewList();

      for(size_t i = 0; i < rts.size(); i++)
      {
        const D3D12Descriptor &desc = rts[i];

        if(desc.GetResResourceId() != ResourceId())
          Unwrap(list)->ClearRenderTargetView(Unwrap(GetDebugManager()->GetTempDescriptor(desc)),
                                              black, 0, NULL);
      }

      list->Close();
      list = NULL;

      for(size_t i = 0; i < events.size(); i++)
      {
        m_pDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass && i + 1 < events.size())
          m_pDevice->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
      }
    }
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    if(pipe && pipe->IsGraphics() && !rs.views.empty())
    {
      list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

      D3D12_VIEWPORT viewport = rs.views[0];
      list->RSSetViewports(1, &viewport);

      D3D12_RECT scissor = {0, 0, 16384, 16384};
      list->RSSetScissorRects(1, &scissor);

      list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      list->SetPipelineState(m_General.CheckerboardF16Pipe[overlayTexDesc.SampleDesc.Count - 1]);

      list->SetGraphicsRootSignature(m_General.CheckerboardRootSig);

      CheckerboardCBuffer pixelData = {0};

      pixelData.BorderWidth = 3;
      pixelData.CheckerSquareDimension = 16.0f;

      // set primary/secondary to the same to 'disable' checkerboard
      pixelData.PrimaryColor = pixelData.SecondaryColor = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
      pixelData.InnerColor = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);

      // set viewport rect
      pixelData.RectPosition = Vec2f(viewport.TopLeftX, viewport.TopLeftY);
      pixelData.RectSize = Vec2f(viewport.Width, viewport.Height);

      D3D12_GPU_VIRTUAL_ADDRESS viewCB =
          GetDebugManager()->UploadConstants(&pixelData, sizeof(pixelData));

      list->SetGraphicsRootConstantBufferView(0, viewCB);

      float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
      list->OMSetBlendFactor(factor);

      list->DrawInstanced(3, 1, 0, 0);

      viewport.TopLeftX = (float)rs.scissors[0].left;
      viewport.TopLeftY = (float)rs.scissors[0].top;
      viewport.Width = (float)(rs.scissors[0].right - rs.scissors[0].left);
      viewport.Height = (float)(rs.scissors[0].bottom - rs.scissors[0].top);
      list->RSSetViewports(1, &viewport);

      // black/white checkered border
      pixelData.PrimaryColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
      pixelData.SecondaryColor = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

      // nothing at all inside
      pixelData.InnerColor = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);

      // set scissor rect
      pixelData.RectPosition = Vec2f(viewport.TopLeftX, viewport.TopLeftY);
      pixelData.RectSize = Vec2f(viewport.Width, viewport.Height);

      D3D12_GPU_VIRTUAL_ADDRESS scissorCB =
          GetDebugManager()->UploadConstants(&pixelData, sizeof(pixelData));

      list->SetGraphicsRootConstantBufferView(0, scissorCB);

      list->DrawInstanced(3, 1, 0, 0);
    }
  }
  else if(overlay == DebugOverlay::TriangleSizeDraw || overlay == DebugOverlay::TriangleSizePass)
  {
    if(pipe && pipe->IsGraphics())
    {
      SCOPED_TIMER("Triangle size");

      std::vector<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::TriangleSizeDraw)
        events.clear();

      while(!events.empty())
      {
        const DrawcallDescription *draw = m_pDevice->GetDrawcall(events[0]);

        // remove any non-drawcalls, like the pass boundary.
        if(!(draw->flags & DrawFlags::Drawcall))
          events.erase(events.begin());
        else
          break;
      }

      events.push_back(eventId);

      if(overlay == DebugOverlay::TriangleSizePass)
      {
        list->Close();
        list = NULL;

        m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

        list = m_pDevice->GetNewList();
      }

      pipe = m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
      pipe->Fill(pipeDesc);
      pipeDesc.pRootSignature = GetDebugManager()->GetMeshRootSig();
      pipeDesc.SampleMask = 0xFFFFFFFF;
      pipeDesc.SampleDesc.Count = 1;
      pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

      RDCEraseEl(pipeDesc.RTVFormats.RTFormats);
      pipeDesc.RTVFormats.NumRenderTargets = 1;
      pipeDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
      pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
      pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

      D3D12_INPUT_ELEMENT_DESC ia[2] = {};
      ia[0].SemanticName = "pos";
      ia[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      ia[1].SemanticName = "sec";
      ia[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      ia[1].InputSlot = 1;
      ia[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;

      pipeDesc.InputLayout.NumElements = 2;
      pipeDesc.InputLayout.pInputElementDescs = ia;

      pipeDesc.VS.BytecodeLength = m_Overlay.MeshVS->GetBufferSize();
      pipeDesc.VS.pShaderBytecode = m_Overlay.MeshVS->GetBufferPointer();
      RDCEraseEl(pipeDesc.HS);
      RDCEraseEl(pipeDesc.DS);
      pipeDesc.GS.BytecodeLength = m_Overlay.TriangleSizeGS->GetBufferSize();
      pipeDesc.GS.pShaderBytecode = m_Overlay.TriangleSizeGS->GetBufferPointer();
      pipeDesc.PS.BytecodeLength = m_Overlay.TriangleSizePS->GetBufferSize();
      pipeDesc.PS.pShaderBytecode = m_Overlay.TriangleSizePS->GetBufferPointer();

      pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

      if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_GREATER)
        pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
      if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_LESS)
        pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

      // enough for all primitive topology types
      ID3D12PipelineState *pipes[D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH + 1] = {};

      MeshVertexCBuffer vertexData = {};
      vertexData.ModelViewProj = Matrix4f::Identity();
      vertexData.SpriteSize = Vec2f();

      Vec4f viewport(rs.views[0].Width, rs.views[0].Height);

      if(rs.dsv.GetResResourceId() != ResourceId())
      {
        D3D12_CPU_DESCRIPTOR_HANDLE tmpdsv = GetDebugManager()->GetTempDescriptor(rs.dsv);
        list->OMSetRenderTargets(1, &rtv, TRUE, &tmpdsv);
      }

      list->RSSetViewports(1, &rs.views[0]);

      D3D12_RECT scissor = {0, 0, 16384, 16384};
      list->RSSetScissorRects(1, &scissor);

      list->OMSetStencilRef(rs.stencilRef);
      list->OMSetBlendFactor(rs.blendFactor);

      list->SetGraphicsRootSignature(GetDebugManager()->GetMeshRootSig());

      list->SetGraphicsRootConstantBufferView(
          0, GetDebugManager()->UploadConstants(&vertexData, sizeof(vertexData)));
      list->SetGraphicsRootConstantBufferView(
          1, GetDebugManager()->UploadConstants(&viewport, sizeof(viewport)));
      list->SetGraphicsRoot32BitConstants(2, 4, &viewport.x, 0);

      for(size_t i = 0; i < events.size(); i++)
      {
        const DrawcallDescription *draw = m_pDevice->GetDrawcall(events[i]);

        for(uint32_t inst = 0; draw && inst < RDCMAX(1U, draw->numInstances); inst++)
        {
          MeshFormat fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::GSOut);
          if(fmt.vertexResourceId == ResourceId())
            fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::VSOut);

          if(fmt.vertexResourceId != ResourceId())
          {
            D3D_PRIMITIVE_TOPOLOGY topo = MakeD3DPrimitiveTopology(fmt.topology);

            if(topo == D3D_PRIMITIVE_TOPOLOGY_POINTLIST ||
               topo >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
              pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            else if(topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP ||
                    topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST ||
                    topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ ||
                    topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ)
              pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            else
              pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

            list->IASetPrimitiveTopology(topo);

            if(pipes[pipeDesc.PrimitiveTopologyType] == NULL)
            {
              HRESULT hr =
                  m_pDevice->CreatePipeState(pipeDesc, &pipes[pipeDesc.PrimitiveTopologyType]);
              RDCASSERTEQUAL(hr, S_OK);
            }

            ID3D12Resource *vb =
                m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.vertexResourceId);

            D3D12_VERTEX_BUFFER_VIEW vbView = {};
            vbView.BufferLocation = vb->GetGPUVirtualAddress() + fmt.vertexByteOffset;
            vbView.StrideInBytes = fmt.vertexByteStride;
            vbView.SizeInBytes = UINT(vb->GetDesc().Width - fmt.vertexByteOffset);

            // second bind is just a dummy, so we don't have to make a shader
            // that doesn't accept the secondary stream
            list->IASetVertexBuffers(0, 1, &vbView);
            list->IASetVertexBuffers(1, 1, &vbView);

            list->SetPipelineState(pipes[pipeDesc.PrimitiveTopologyType]);

            if(fmt.indexByteStride && fmt.indexResourceId != ResourceId())
            {
              ID3D12Resource *ib =
                  m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.indexResourceId);

              D3D12_INDEX_BUFFER_VIEW view;
              view.BufferLocation = ib->GetGPUVirtualAddress() + fmt.indexByteOffset;
              view.SizeInBytes = UINT(ib->GetDesc().Width - fmt.indexByteOffset);
              view.Format = fmt.indexByteStride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
              list->IASetIndexBuffer(&view);

              list->DrawIndexedInstanced(fmt.numIndices, 1, 0, fmt.baseVertex, 0);
            }
            else
            {
              list->DrawInstanced(fmt.numIndices, 1, 0, 0);
            }
          }
        }
      }

      list->Close();
      list = NULL;

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      for(size_t i = 0; i < ARRAY_COUNT(pipes); i++)
        SAFE_RELEASE(pipes[i]);
    }

    // restore back to normal
    m_pDevice->ReplayLog(0, eventId, eReplay_WithoutDraw);
  }
  else if(overlay == DebugOverlay::QuadOverdrawPass || overlay == DebugOverlay::QuadOverdrawDraw)
  {
    SCOPED_TIMER("Quad Overdraw");

    std::vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::QuadOverdrawDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::QuadOverdrawPass)
      {
        list->Close();
        m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);
        list = m_pDevice->GetNewList();
      }

      uint32_t width = uint32_t(resourceDesc.Width >> 1);
      uint32_t height = resourceDesc.Height >> 1;

      width = RDCMAX(1U, width);
      height = RDCMAX(1U, height);

      D3D12_RESOURCE_DESC uavTexDesc = {};
      uavTexDesc.Alignment = 0;
      uavTexDesc.DepthOrArraySize = 4;
      uavTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      uavTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
      uavTexDesc.Format = DXGI_FORMAT_R32_UINT;
      uavTexDesc.Height = height;
      uavTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      uavTexDesc.MipLevels = 1;
      uavTexDesc.SampleDesc.Count = 1;
      uavTexDesc.SampleDesc.Quality = 0;
      uavTexDesc.Width = width;

      ID3D12Resource *overdrawTex = NULL;
      HRESULT hr = m_pDevice->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &uavTexDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
          NULL, __uuidof(ID3D12Resource), (void **)&overdrawTex);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overdrawTex HRESULT: %s", ToStr(hr).c_str());
        list->Close();
        list = NULL;
        return m_Overlay.resourceId;
      }

      m_pDevice->CreateShaderResourceView(overdrawTex, NULL,
                                          GetDebugManager()->GetCPUHandle(OVERDRAW_SRV));
      m_pDevice->CreateUnorderedAccessView(overdrawTex, NULL, NULL,
                                           GetDebugManager()->GetCPUHandle(OVERDRAW_UAV));
      m_pDevice->CreateUnorderedAccessView(overdrawTex, NULL, NULL,
                                           GetDebugManager()->GetUAVClearHandle(OVERDRAW_UAV));

      UINT zeroes[4] = {0, 0, 0, 0};
      list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(OVERDRAW_UAV),
                                         GetDebugManager()->GetUAVClearHandle(OVERDRAW_UAV),
                                         overdrawTex, zeroes, 0, NULL);
      list->Close();
      list = NULL;

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();
#endif

      m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

      D3D12_SHADER_BYTECODE quadWrite;
      quadWrite.BytecodeLength = m_Overlay.QuadOverdrawWritePS->GetBufferSize();
      quadWrite.pShaderBytecode = m_Overlay.QuadOverdrawWritePS->GetBufferPointer();

      // declare callback struct here
      D3D12QuadOverdrawCallback cb(m_pDevice, quadWrite, events,
                                   ToPortableHandle(GetDebugManager()->GetCPUHandle(OVERDRAW_UAV)));

      m_pDevice->ReplayLog(events.front(), events.back(), eReplay_Full);

      // resolve pass
      {
        list = m_pDevice->GetNewList();

        D3D12_RESOURCE_BARRIER overdrawBarriers[2] = {};

        // make sure UAV work is done then prepare for reading in PS
        overdrawBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        overdrawBarriers[0].UAV.pResource = overdrawTex;
        overdrawBarriers[1].Transition.pResource = overdrawTex;
        overdrawBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        overdrawBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        overdrawBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        // prepare tex resource for copying
        list->ResourceBarrier(2, overdrawBarriers);

        list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

        list->RSSetViewports(1, &rs.views[0]);

        D3D12_RECT scissor = {0, 0, 16384, 16384};
        list->RSSetScissorRects(1, &scissor);

        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        list->SetPipelineState(m_Overlay.QuadResolvePipe);

        list->SetGraphicsRootSignature(m_Overlay.QuadResolveRootSig);

        GetDebugManager()->SetDescriptorHeaps(list, true, false);

        list->SetGraphicsRootDescriptorTable(0, GetDebugManager()->GetGPUHandle(OVERDRAW_SRV));

        list->DrawInstanced(3, 1, 0, 0);

        list->Close();
        list = NULL;
      }

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      for(auto it = cb.m_PipelineCache.begin(); it != cb.m_PipelineCache.end(); ++it)
      {
        SAFE_RELEASE(it->second.pipe);
        SAFE_RELEASE(it->second.sig);
      }

      SAFE_RELEASE(overdrawTex);
    }

    if(overlay == DebugOverlay::QuadOverdrawPass)
      m_pDevice->ReplayLog(0, eventId, eReplay_WithoutDraw);
  }
  else if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
      pipe->Fill(psoDesc);

      float redCol[4] = {1.0f, 0.0f, 0.0f, 1.0f};
      ID3DBlob *red = m_pDevice->GetShaderCache()->MakeFixedColShader(redCol);

      float greenCol[4] = {0.0f, 1.0f, 0.0f, 1.0f};
      ID3DBlob *green = m_pDevice->GetShaderCache()->MakeFixedColShader(greenCol);

      // make sure that if a test is disabled, it shows all
      // pixels passing
      if(!psoDesc.DepthStencilState.DepthEnable)
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      if(!psoDesc.DepthStencilState.StencilEnable)
      {
        psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      }

      if(overlay == DebugOverlay::Depth)
      {
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      }
      else
      {
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;
      }

      RDCEraseEl(psoDesc.RTVFormats.RTFormats);
      psoDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      psoDesc.RTVFormats.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.MultisampleEnable = FALSE;
      psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;

      psoDesc.PS.pShaderBytecode = green->GetBufferPointer();
      psoDesc.PS.BytecodeLength = green->GetBufferSize();

      list->Close();
      list = NULL;

      ID3D12PipelineState *greenPSO = NULL;
      HRESULT hr = m_pDevice->CreatePipeState(psoDesc, &greenPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        return m_Overlay.resourceId;
      }

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;
      psoDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;

      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;

      psoDesc.PS.pShaderBytecode = red->GetBufferPointer();
      psoDesc.PS.BytecodeLength = red->GetBufferSize();

      ID3D12PipelineState *redPSO = NULL;
      hr = m_pDevice->CreatePipeState(psoDesc, &redPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(redPSO);
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        return m_Overlay.resourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(redPSO);
      rs.rts.resize(1);
      rs.rts[0] = *GetWrapped(rtv);
      if(dsv.ptr)
        rs.dsv = *GetWrapped(dsv);
      else
        RDCEraseEl(rs.dsv);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs.pipe = GetResID(greenPSO);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs = prev;

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      SAFE_RELEASE(red);
      SAFE_RELEASE(green);
      SAFE_RELEASE(redPSO);
      SAFE_RELEASE(greenPSO);
    }
  }
  else
  {
    RDCERR("Unhandled overlay case!");
  }

  if(list)
    list->Close();

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists();

  SAFE_RELEASE(renderDepth);

  return m_Overlay.resourceId;
}
