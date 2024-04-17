/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "core/settings.h"
#include "driver/dxgi/dxgi_common.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_replay.h"

#include "data/hlsl/hlsl_cbuffers.h"

RDOC_EXTERN_CONFIG(bool, D3D12_Debug_SingleSubmitFlushing);

static uint32_t VisModeToMeshDisplayFormat(const MeshDisplay &cfg)
{
  switch(cfg.visualisationMode)
  {
    default: return (uint32_t)cfg.visualisationMode;
    case Visualisation::Secondary:
      return cfg.second.showAlpha ? MESHDISPLAY_SECONDARY_ALPHA : MESHDISPLAY_SECONDARY;
  }
}

MeshDisplayPipelines D3D12DebugManager::CacheMeshDisplayPipelines(const MeshFormat &primary,
                                                                  const MeshFormat &secondary)
{
  // generate a key to look up the map
  uint64_t key = 0;

  uint64_t bit = 0;

  if(primary.indexByteStride == 4)
    key |= 1ULL << bit;
  bit++;

  RDCASSERT((uint32_t)primary.topology < 64);
  key |= uint64_t((uint32_t)primary.topology & 0x3f) << bit;
  bit += 6;

  DXGI_FORMAT primaryFmt = MakeDXGIFormat(primary.format);
  DXGI_FORMAT secondaryFmt = secondary.vertexResourceId == ResourceId()
                                 ? DXGI_FORMAT_UNKNOWN
                                 : MakeDXGIFormat(secondary.format);

  key |= uint64_t((uint32_t)primaryFmt & 0xff) << bit;
  bit += 8;

  key |= uint64_t((uint32_t)secondaryFmt & 0xff) << bit;
  bit += 8;

  RDCASSERT(primary.vertexByteStride <= 0xffff);
  key |= uint64_t((uint32_t)primary.vertexByteStride & 0xffff) << bit;
  bit += 16;

  if(secondary.vertexResourceId != ResourceId())
  {
    RDCASSERT(secondary.vertexByteStride <= 0xffff);
    key |= uint64_t((uint32_t)secondary.vertexByteStride & 0xffff) << bit;
  }
  bit += 16;

  if(primary.instanced)
    key |= 1ULL << bit;
  bit++;

  if(secondary.instanced)
    key |= 1ULL << bit;
  bit++;

  if(primary.allowRestart)
    key |= 1ULL << bit;
  bit++;

  // only 64 bits, make sure they all fit
  RDCASSERT(bit < 64);

  MeshDisplayPipelines &cache = m_CachedMeshPipelines[key];

  if(cache.pipes[(uint32_t)Visualisation::NoSolid] != NULL)
    return cache;

  cache.rootsig = m_MeshRootSig;

  // should we try and evict old pipelines from the cache here?
  // or just keep them forever

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc;
  RDCEraseEl(pipeDesc);
  pipeDesc.pRootSignature = m_MeshRootSig;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.RasterizerState.AntialiasedLineEnable = TRUE;
  pipeDesc.RasterizerState.MultisampleEnable = TRUE;
  pipeDesc.SampleMask = 0xFFFFFFFF;
  pipeDesc.SampleDesc.Count = D3D12_MSAA_SAMPLECOUNT;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

  if(primary.allowRestart)
  {
    if(primary.indexByteStride == 2)
      pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
    else
      pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF;
  }

  D3D_PRIMITIVE_TOPOLOGY topo = MakeD3DPrimitiveTopology(primary.topology);

  if(topo == D3D_PRIMITIVE_TOPOLOGY_POINTLIST ||
     topo >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
  else if(topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP || topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST ||
          topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ || topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ)
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
  else
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  pipeDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  D3D12_INPUT_ELEMENT_DESC ia[2] = {};
  ia[0].SemanticName = "pos";
  ia[0].Format = primaryFmt;
  ia[0].InputSlotClass = primary.instanced ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                                           : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  ia[1].SemanticName = "sec";
  ia[1].InputSlot = 1;
  ia[1].Format = secondaryFmt == DXGI_FORMAT_UNKNOWN ? primaryFmt : secondaryFmt;
  ia[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;

  pipeDesc.InputLayout.NumElements = 2;
  pipeDesc.InputLayout.pInputElementDescs = ia;

  RDCASSERT(primaryFmt != DXGI_FORMAT_UNKNOWN);

  // wireframe pipeline
  pipeDesc.VS.BytecodeLength = m_MeshVS->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = m_MeshVS->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = m_MeshPS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = m_MeshPS->GetBufferPointer();

  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
  pipeDesc.DepthStencilState.DepthEnable = FALSE;
  pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

  HRESULT hr = S_OK;

  hr = m_pDevice->CreateGraphicsPipelineState(
      &pipeDesc, __uuidof(ID3D12PipelineState),
      (void **)&cache.pipes[MeshDisplayPipelines::ePipe_Wire]);
  RDCASSERTEQUAL(hr, S_OK);

  pipeDesc.DepthStencilState.DepthEnable = TRUE;
  pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

  hr = m_pDevice->CreateGraphicsPipelineState(
      &pipeDesc, __uuidof(ID3D12PipelineState),
      (void **)&cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);
  RDCASSERTEQUAL(hr, S_OK);

  // solid shading pipeline
  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.DepthStencilState.DepthEnable = FALSE;
  pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

  hr = m_pDevice->CreateGraphicsPipelineState(
      &pipeDesc, __uuidof(ID3D12PipelineState),
      (void **)&cache.pipes[MeshDisplayPipelines::ePipe_Solid]);
  RDCASSERTEQUAL(hr, S_OK);

  pipeDesc.DepthStencilState.DepthEnable = TRUE;
  pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

  hr = m_pDevice->CreateGraphicsPipelineState(
      &pipeDesc, __uuidof(ID3D12PipelineState),
      (void **)&cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth]);
  RDCASSERTEQUAL(hr, S_OK);

  if(secondary.vertexResourceId != ResourceId())
  {
    // pull secondary information from second vertex buffer
    ia[1].InputSlotClass = secondary.instanced ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                                               : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    RDCASSERT(secondaryFmt != DXGI_FORMAT_UNKNOWN);

    hr = m_pDevice->CreateGraphicsPipelineState(
        &pipeDesc, __uuidof(ID3D12PipelineState),
        (void **)&cache.pipes[MeshDisplayPipelines::ePipe_Secondary]);
    RDCASSERTEQUAL(hr, S_OK);
  }

  if(pipeDesc.PrimitiveTopologyType == D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
  {
    ia[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;

    // flat lit pipeline, needs geometry shader to calculate face normals
    pipeDesc.GS.BytecodeLength = m_MeshGS->GetBufferSize();
    pipeDesc.GS.pShaderBytecode = m_MeshGS->GetBufferPointer();

    hr = m_pDevice->CreateGraphicsPipelineState(
        &pipeDesc, __uuidof(ID3D12PipelineState),
        (void **)&cache.pipes[MeshDisplayPipelines::ePipe_Lit]);
    RDCASSERTEQUAL(hr, S_OK);
  }

  return cache;
}

void D3D12Replay::RenderMesh(uint32_t eventId, const rdcarray<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
  if(cfg.position.vertexResourceId == ResourceId() || cfg.position.numIndices == 0)
    return;

  auto it = m_OutputWindows.find(m_CurrentOutputWindow);
  if(m_CurrentOutputWindow == 0 || it == m_OutputWindows.end())
    return;

  D3D12MarkerRegion renderMesh(
      m_pDevice->GetQueue(),
      StringFormat::Fmt("RenderMesh with %zu secondary draws", secondaryDraws.size()));

  OutputWindow &outw = it->second;

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
  if(!list)
    return;

  D3D12MarkerRegion::Begin(list, StringFormat::Fmt("RenderMesh(%u)", eventId));

  list->OMSetRenderTargets(1, &outw.rtv, TRUE, &outw.dsv);

  D3D12_VIEWPORT viewport = {0, 0, (float)outw.width, (float)outw.height, 0.0f, 1.0f};
  list->RSSetViewports(1, &viewport);

  D3D12_RECT scissor = {0, 0, outw.width, outw.height};
  list->RSSetScissorRects(1, &scissor);

  MeshVertexCBuffer vertexData;

  Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, viewport.Width / viewport.Height);
  Matrix4f InvProj = projMat.Inverse();

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f axisMapMat = Matrix4f(cfg.axisMapping);

  Matrix4f guessProjInv;

  vertexData.ModelViewProj = projMat.Mul(camMat.Mul(axisMapMat));
  vertexData.SpriteSize = Vec2f();
  vertexData.homogenousInput = cfg.position.unproject;
  vertexData.vtxExploderSNorm = cfg.vtxExploderSliderSNorm;
  vertexData.exploderCentre =
      Vec3f((cfg.minBounds.x + cfg.maxBounds.x) * 0.5f, (cfg.minBounds.y + cfg.maxBounds.y) * 0.5f,
            (cfg.minBounds.z + cfg.maxBounds.z) * 0.5f);
  vertexData.exploderScale =
      (cfg.visualisationMode == Visualisation::Explode) ? cfg.exploderScale : 0.0f;
  vertexData.vertMeshDisplayFormat = MESHDISPLAY_SOLID;

  MeshPixelCBuffer pixelData;

  pixelData.MeshColour = Vec3f(0.0f, 0.0f, 0.0f);
  pixelData.MeshDisplayFormat = MESHDISPLAY_SOLID;

  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    Matrix4f guessProj =
        cfg.position.farPlane != FLT_MAX
            ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
            : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

    if(cfg.ortho)
    {
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
    }

    if(cfg.position.flipY)
    {
      guessProj[5] *= -1.0f;
    }

    guessProjInv = guessProj.Inverse();

    vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
  }

  memcpy(&vertexData.meshletColours[0].x, uniqueColors, sizeof(uniqueColors));
  RDCCOMPILE_ASSERT(sizeof(vertexData.meshletColours) == sizeof(uniqueColors),
                    "Unique colors array is wrongly sized");

  D3D12_GPU_VIRTUAL_ADDRESS vsCB =
      GetDebugManager()->UploadConstants(&vertexData, sizeof(vertexData));

  D3D12_GPU_VIRTUAL_ADDRESS meshletBuf = GetDebugManager()->UploadMeshletSizes(
      cfg.position.meshletIndexOffset, cfg.position.meshletSizes);

  if(!secondaryDraws.empty())
  {
    D3D12MarkerRegion region(list, "Secondary draws");

    ID3D12RootSignature *rootSig = NULL;

    for(size_t i = 0; i < secondaryDraws.size(); i++)
    {
      const MeshFormat &fmt = secondaryDraws[i];

      if(fmt.vertexResourceId != ResourceId())
      {
        MeshDisplayPipelines secondaryCache =
            GetDebugManager()->CacheMeshDisplayPipelines(secondaryDraws[i], secondaryDraws[i]);

        if(secondaryCache.rootsig != rootSig)
        {
          rootSig = secondaryCache.rootsig;
          list->SetGraphicsRootSignature(rootSig);
          list->SetGraphicsRootConstantBufferView(0, vsCB);
          list->SetGraphicsRootConstantBufferView(1, vsCB);    // geometry - dummy fill
          list->SetGraphicsRootShaderResourceView(3, meshletBuf);
        }

        pixelData.MeshColour.x = fmt.meshColor.x;
        pixelData.MeshColour.y = fmt.meshColor.y;
        pixelData.MeshColour.z = fmt.meshColor.z;
        list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);

        list->SetPipelineState(secondaryCache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);

        ID3D12Resource *vb =
            m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.vertexResourceId);

        UINT64 offs = fmt.vertexByteOffset;
        D3D12_VERTEX_BUFFER_VIEW view;
        view.BufferLocation = vb->GetGPUVirtualAddress() + offs;
        view.StrideInBytes = fmt.vertexByteStride;
        view.SizeInBytes = (UINT)(fmt.vertexByteSize - offs);
        list->IASetVertexBuffers(0, 1, &view);

        // set it to the secondary buffer too just as dummy info
        list->IASetVertexBuffers(1, 1, &view);

        list->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(fmt.topology));

        if(PatchList_Count(fmt.topology) > 0)
          list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

        if(fmt.indexByteStride)
        {
          ID3D12Resource *ib =
              m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.indexResourceId);

          if(ib)
          {
            D3D12_INDEX_BUFFER_VIEW iview;
            iview.BufferLocation = ib->GetGPUVirtualAddress() + fmt.indexByteOffset;
            iview.SizeInBytes = (UINT)(fmt.indexByteSize - fmt.indexByteOffset);
            iview.Format = fmt.indexByteStride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
            list->IASetIndexBuffer(&iview);
          }
          else
          {
            list->IASetIndexBuffer(NULL);
          }

          list->DrawIndexedInstanced(fmt.numIndices, 1, 0, fmt.baseVertex, 0);
        }
        else
        {
          list->DrawInstanced(fmt.numIndices, 1, 0, 0);
        }
      }
    }
  }

  MeshDisplayPipelines cache = GetDebugManager()->CacheMeshDisplayPipelines(cfg.position, cfg.second);

  if(cfg.position.vertexResourceId != ResourceId())
  {
    D3D12MarkerRegion::Set(list, "Primary");

    ID3D12Resource *vb =
        m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.vertexResourceId);

    UINT64 offs = cfg.position.vertexByteOffset;

    // we source all data from the first instanced value in the instanced case, so make sure we
    // offset correctly here.
    if(cfg.position.instanced && cfg.position.instStepRate)
      offs += cfg.position.vertexByteStride * (cfg.curInstance / cfg.position.instStepRate);

    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = vb->GetGPUVirtualAddress() + offs;
    view.StrideInBytes = cfg.position.vertexByteStride;
    view.SizeInBytes = (UINT)(cfg.position.vertexByteSize - offs);
    list->IASetVertexBuffers(0, 1, &view);

    // set it to the secondary buffer too just as dummy info
    list->IASetVertexBuffers(1, 1, &view);

    list->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(cfg.position.topology));

    if(PatchList_Count(cfg.position.topology) > 0)
      list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
  }

  // can't support secondary shading without a buffer - no pipeline will have been created
  const Visualisation finalVisualisation = (cfg.visualisationMode == Visualisation::Secondary &&
                                            cfg.second.vertexResourceId == ResourceId())
                                               ? Visualisation::NoSolid
                                               : cfg.visualisationMode;

  if(finalVisualisation == Visualisation::Secondary)
  {
    D3D12MarkerRegion::Set(list, "Secondary");

    ID3D12Resource *vb =
        m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.vertexResourceId);

    UINT64 offs = cfg.second.vertexByteOffset;

    // we source all data from the first instanced value in the instanced case, so make sure we
    // offset correctly here.
    if(cfg.second.instanced && cfg.second.instStepRate)
      offs += cfg.second.vertexByteStride * (cfg.curInstance / cfg.second.instStepRate);

    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = vb->GetGPUVirtualAddress() + offs;
    view.StrideInBytes = cfg.second.vertexByteStride;
    view.SizeInBytes = (UINT)(cfg.second.vertexByteSize - offs);

    list->IASetVertexBuffers(1, 1, &view);
  }

  // solid render
  if(finalVisualisation != Visualisation::NoSolid && cfg.position.topology < Topology::PatchList)
  {
    D3D12MarkerRegion region(list, "Solid render");

    ID3D12PipelineState *pipe = NULL;
    switch(finalVisualisation)
    {
      default:
      case Visualisation::Solid: pipe = cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth]; break;
      case Visualisation::Lit:
      case Visualisation::Explode:
        pipe = cache.pipes[MeshDisplayPipelines::ePipe_Lit];
        // point list topologies don't have lighting obvious, just render them as solid
        if(!pipe)
          pipe = cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth];
        break;
      case Visualisation::Secondary:
        pipe = cache.pipes[MeshDisplayPipelines::ePipe_Secondary];
        break;
      case Visualisation::Meshlet:
        pipe = cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth];
        break;
    }

    pixelData.MeshDisplayFormat = VisModeToMeshDisplayFormat(cfg);

    pixelData.MeshColour = Vec3f(0.8f, 0.8f, 0.0f);

    list->SetPipelineState(pipe);
    list->SetGraphicsRootSignature(cache.rootsig);

    size_t numMeshlets = RDCMIN(cfg.position.meshletSizes.size(), (size_t)MAX_NUM_MESHLETS);

    if(finalVisualisation == Visualisation::Meshlet)
    {
      vertexData.meshletCount = (uint32_t)numMeshlets;
      vertexData.meshletOffset = (uint32_t)cfg.position.meshletOffset;

      vertexData.vertMeshDisplayFormat = MESHDISPLAY_MESHLET;
    }

    D3D12_GPU_VIRTUAL_ADDRESS vsCBSolid =
        GetDebugManager()->UploadConstants(&vertexData, sizeof(vertexData));

    list->SetGraphicsRootConstantBufferView(0, vsCBSolid);

    if(finalVisualisation == Visualisation::Lit || finalVisualisation == Visualisation::Explode)
    {
      MeshGeometryCBuffer geomData;
      geomData.InvProj = projMat.Inverse();

      list->SetGraphicsRootConstantBufferView(
          1, GetDebugManager()->UploadConstants(&geomData, sizeof(geomData)));
    }
    else
    {
      list->SetGraphicsRootConstantBufferView(1, vsCB);    // dummy fill for geometry
    }
    list->SetGraphicsRootShaderResourceView(3, meshletBuf);

    Vec4f colour(0.8f, 0.8f, 0.0f, 1.0f);
    list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);

    if(cfg.position.indexByteStride)
    {
      ID3D12Resource *ib =
          m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.indexResourceId);

      if(ib)
      {
        D3D12_INDEX_BUFFER_VIEW view;
        view.BufferLocation = ib->GetGPUVirtualAddress() + cfg.position.indexByteOffset;
        view.SizeInBytes = (UINT)(cfg.position.indexByteSize - cfg.position.indexByteOffset);
        view.Format = cfg.position.indexByteStride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        list->IASetIndexBuffer(&view);
      }
      else
      {
        list->IASetIndexBuffer(NULL);
      }

      list->DrawIndexedInstanced(cfg.position.numIndices, 1, 0, cfg.position.baseVertex, 0);
    }
    else
    {
      list->DrawInstanced(cfg.position.numIndices, 1, 0, 0);
    }
  }

  // wireframe render
  if(finalVisualisation == Visualisation::NoSolid || cfg.wireframeDraw ||
     cfg.position.topology >= Topology::PatchList)
  {
    D3D12MarkerRegion region(list, "Wireframe render");

    Vec4f wireCol =
        Vec4f(cfg.position.meshColor.x, cfg.position.meshColor.y, cfg.position.meshColor.z, 1.0f);

    pixelData.MeshDisplayFormat = MESHDISPLAY_SOLID;

    list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);
    list->SetGraphicsRootSignature(cache.rootsig);

    list->SetGraphicsRootConstantBufferView(0, vsCB);
    list->SetGraphicsRootConstantBufferView(1, vsCB);
    list->SetGraphicsRootShaderResourceView(3, meshletBuf);

    pixelData.MeshColour.x = cfg.position.meshColor.x;
    pixelData.MeshColour.y = cfg.position.meshColor.y;
    pixelData.MeshColour.z = cfg.position.meshColor.z;

    list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);

    if(cfg.position.indexByteStride)
    {
      ID3D12Resource *ib =
          m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.indexResourceId);

      if(ib)
      {
        D3D12_INDEX_BUFFER_VIEW view;
        view.BufferLocation = ib->GetGPUVirtualAddress() + cfg.position.indexByteOffset;
        view.SizeInBytes = (UINT)(cfg.position.indexByteSize - cfg.position.indexByteOffset);
        view.Format = cfg.position.indexByteStride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        list->IASetIndexBuffer(&view);
      }
      else
      {
        list->IASetIndexBuffer(NULL);
      }

      list->DrawIndexedInstanced(cfg.position.numIndices, 1, 0, cfg.position.baseVertex, 0);
    }
    else
    {
      list->DrawInstanced(cfg.position.numIndices, 1, 0, 0);
    }
  }

  MeshFormat helper;
  helper.indexByteStride = 2;
  helper.topology = Topology::LineList;

  helper.format.type = ResourceFormatType::Regular;
  helper.format.compByteWidth = 4;
  helper.format.compCount = 4;
  helper.format.compType = CompType::Float;

  helper.vertexByteStride = sizeof(Vec4f);

  pixelData.MeshDisplayFormat = MESHDISPLAY_SOLID;

  vertexData.homogenousInput = 0U;
  vertexData.vtxExploderSNorm = 0.0f;
  vertexData.exploderScale = 0.0f;

  vsCB = GetDebugManager()->UploadConstants(&vertexData, sizeof(vertexData));

  // cache pipelines for use in drawing wireframe helpers
  cache = GetDebugManager()->CacheMeshDisplayPipelines(helper, helper);

  if(cfg.showBBox)
  {
    D3D12MarkerRegion region(list, "Bounding box");

    Vec4f a = Vec4f(cfg.minBounds.x, cfg.minBounds.y, cfg.minBounds.z, cfg.minBounds.w);
    Vec4f b = Vec4f(cfg.maxBounds.x, cfg.maxBounds.y, cfg.maxBounds.z, cfg.maxBounds.w);

    Vec4f TLN = Vec4f(a.x, b.y, a.z, 1.0f);    // TopLeftNear, etc...
    Vec4f TRN = Vec4f(b.x, b.y, a.z, 1.0f);
    Vec4f BLN = Vec4f(a.x, a.y, a.z, 1.0f);
    Vec4f BRN = Vec4f(b.x, a.y, a.z, 1.0f);

    Vec4f TLF = Vec4f(a.x, b.y, b.z, 1.0f);
    Vec4f TRF = Vec4f(b.x, b.y, b.z, 1.0f);
    Vec4f BLF = Vec4f(a.x, a.y, b.z, 1.0f);
    Vec4f BRF = Vec4f(b.x, a.y, b.z, 1.0f);

    // 12 frustum lines => 24 verts
    Vec4f bbox[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = GetDebugManager()->UploadConstants(bbox, sizeof(bbox));
    view.SizeInBytes = sizeof(bbox);
    view.StrideInBytes = sizeof(Vec4f);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    list->IASetVertexBuffers(0, 1, &view);

    pixelData.MeshColour = Vec3f(0.2f, 0.2f, 1.0f);
    list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);

    list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);

    list->SetGraphicsRootConstantBufferView(0, vsCB);
    list->SetGraphicsRootConstantBufferView(1, vsCB);
    list->SetGraphicsRootShaderResourceView(3, meshletBuf);

    list->DrawInstanced(24, 1, 0, 0);
  }

  // draw axis helpers
  if(!cfg.position.unproject)
  {
    D3D12MarkerRegion region(list, "Axis helpers");

    Vec4f axismarker[6] = {
        Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
        Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f),
    };

    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = GetDebugManager()->UploadConstants(axismarker, sizeof(axismarker));
    view.SizeInBytes = sizeof(axismarker);
    view.StrideInBytes = sizeof(Vec4f);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    list->IASetVertexBuffers(0, 1, &view);

    list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_Wire]);

    list->SetGraphicsRootConstantBufferView(0, vsCB);
    list->SetGraphicsRootConstantBufferView(1, vsCB);
    list->SetGraphicsRootShaderResourceView(3, meshletBuf);

    pixelData.MeshColour = Vec3f(1.0f, 0.0f, 0.0f);
    list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);
    list->DrawInstanced(2, 1, 0, 0);

    pixelData.MeshColour = Vec3f(0.0f, 1.0f, 0.0f);
    list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);
    list->DrawInstanced(2, 1, 2, 0);

    pixelData.MeshColour = Vec3f(0.0f, 0.0f, 1.0f);
    list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);
    list->DrawInstanced(2, 1, 4, 0);
  }

  // 'fake' helper frustum
  if(cfg.position.unproject)
  {
    D3D12MarkerRegion region(list, "Frustum");

    Vec4f TLN = Vec4f(-1.0f, 1.0f, 0.0f, 1.0f);    // TopLeftNear, etc...
    Vec4f TRN = Vec4f(1.0f, 1.0f, 0.0f, 1.0f);
    Vec4f BLN = Vec4f(-1.0f, -1.0f, 0.0f, 1.0f);
    Vec4f BRN = Vec4f(1.0f, -1.0f, 0.0f, 1.0f);

    Vec4f TLF = Vec4f(-1.0f, 1.0f, 1.0f, 1.0f);
    Vec4f TRF = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    Vec4f BLF = Vec4f(-1.0f, -1.0f, 1.0f, 1.0f);
    Vec4f BRF = Vec4f(1.0f, -1.0f, 1.0f, 1.0f);

    // 12 frustum lines => 24 verts
    Vec4f bbox[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = GetDebugManager()->UploadConstants(bbox, sizeof(bbox));
    view.SizeInBytes = sizeof(bbox);
    view.StrideInBytes = sizeof(Vec4f);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    list->IASetVertexBuffers(0, 1, &view);

    pixelData.MeshColour = Vec3f(1.0f, 1.0f, 1.0f);
    list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);

    list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_Wire]);

    list->SetGraphicsRootConstantBufferView(0, vsCB);
    list->SetGraphicsRootConstantBufferView(1, vsCB);
    list->SetGraphicsRootShaderResourceView(3, meshletBuf);

    list->DrawInstanced(24, 1, 0, 0);
  }

  // show highlighted vertex
  if(cfg.highlightVert != ~0U)
  {
    vertexData.homogenousInput = cfg.position.unproject;

    D3D12MarkerRegion region(list, "Highlighted Vertex");

    m_HighlightCache.CacheHighlightingData(eventId, cfg);

    Topology meshtopo = cfg.position.topology;

    ///////////////////////////////////////////////////////////////
    // vectors to be set from buffers, depending on topology

    // this vert (blue dot, required)
    FloatVector activeVertex;

    // primitive this vert is a part of (red prim, optional)
    rdcarray<FloatVector> activePrim;

    // for patch lists, to show other verts in patch (green dots, optional)
    // for non-patch lists, we use the activePrim and adjacentPrimVertices
    // to show what other verts are related
    rdcarray<FloatVector> inactiveVertices;

    // adjacency (line or tri, strips or lists) (green prims, optional)
    // will be N*M long, N adjacent prims of M verts each. M = primSize below
    rdcarray<FloatVector> adjacentPrimVertices;

    helper.topology = Topology::TriangleList;
    uint32_t primSize = 3;    // number of verts per primitive

    if(meshtopo == Topology::LineList || meshtopo == Topology::LineStrip ||
       meshtopo == Topology::LineList_Adj || meshtopo == Topology::LineStrip_Adj)
    {
      primSize = 2;
      helper.topology = Topology::LineList;
    }
    else
    {
      // update the cache, as it's currently linelist
      helper.topology = Topology::TriangleList;
      cache = GetDebugManager()->CacheMeshDisplayPipelines(helper, helper);
    }

    bool valid = m_HighlightCache.FetchHighlightPositions(cfg, activeVertex, activePrim,
                                                          adjacentPrimVertices, inactiveVertices);

    if(valid)
    {
      ////////////////////////////////////////////////////////////////
      // prepare rendering (for both vertices & primitives)

      // if data is from post transform, it will be in clipspace
      if(cfg.position.unproject)
        vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
      else
        vertexData.ModelViewProj = projMat.Mul(camMat.Mul(axisMapMat));

      list->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(helper.topology));

      if(PatchList_Count(helper.topology) > 0)
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

      list->SetGraphicsRootConstantBufferView(
          0, GetDebugManager()->UploadConstants(&vertexData, sizeof(vertexData)));
      list->SetGraphicsRootShaderResourceView(3, meshletBuf);

      list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_Solid]);

      ////////////////////////////////////////////////////////////////
      // render primitives

      // Draw active primitive (red)
      pixelData.MeshColour = Vec3f(1.0f, 0.0f, 0.0f);
      list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);

      D3D12_VERTEX_BUFFER_VIEW view = {};
      view.StrideInBytes = sizeof(Vec4f);

      if(activePrim.size() >= primSize)
      {
        view.BufferLocation =
            GetDebugManager()->UploadConstants(&activePrim[0], sizeof(Vec4f) * primSize);
        view.SizeInBytes = sizeof(Vec4f) * primSize;

        list->IASetVertexBuffers(0, 1, &view);

        list->DrawInstanced(primSize, 1, 0, 0);
      }

      // Draw adjacent primitives (green)
      pixelData.MeshColour = Vec3f(0.0f, 1.0f, 0.0f);
      list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);

      if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
      {
        view.BufferLocation = GetDebugManager()->UploadConstants(
            &activePrim[0], sizeof(Vec4f) * adjacentPrimVertices.size());
        view.SizeInBytes = UINT(sizeof(Vec4f) * adjacentPrimVertices.size());

        list->IASetVertexBuffers(0, 1, &view);

        list->DrawInstanced((UINT)adjacentPrimVertices.size(), 1, 0, 0);
      }

      ////////////////////////////////////////////////////////////////
      // prepare to render dots

      float scale = 800.0f / viewport.Height;
      float asp = viewport.Width / viewport.Height;

      vertexData.SpriteSize = Vec2f(scale / asp, scale);

      list->SetGraphicsRootConstantBufferView(
          0, GetDebugManager()->UploadConstants(&vertexData, sizeof(vertexData)));

      // Draw active vertex (blue)
      pixelData.MeshColour = Vec3f(0.0f, 0.0f, 1.0f);
      list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);

      // vertices are drawn with tri strips
      helper.topology = Topology::TriangleStrip;
      cache = GetDebugManager()->CacheMeshDisplayPipelines(helper, helper);

      FloatVector vertSprite[4] = {
          activeVertex,
          activeVertex,
          activeVertex,
          activeVertex,
      };

      list->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(helper.topology));

      if(PatchList_Count(helper.topology) > 0)
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

      list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_Solid]);

      {
        view.BufferLocation = GetDebugManager()->UploadConstants(&vertSprite[0], sizeof(vertSprite));
        view.SizeInBytes = sizeof(vertSprite);

        list->IASetVertexBuffers(0, 1, &view);

        list->DrawInstanced(4, 1, 0, 0);
      }

      // Draw inactive vertices (green)
      pixelData.MeshColour = Vec3f(0.0f, 1.0f, 0.0f);
      list->SetGraphicsRoot32BitConstants(2, 4, &pixelData, 0);

      if(!inactiveVertices.empty())
      {
        rdcarray<FloatVector> inactiveVB;
        inactiveVB.reserve(inactiveVertices.size() * 4);

        for(size_t i = 0; i < inactiveVertices.size(); i++)
        {
          inactiveVB.push_back(inactiveVertices[i]);
          inactiveVB.push_back(inactiveVertices[i]);
          inactiveVB.push_back(inactiveVertices[i]);
          inactiveVB.push_back(inactiveVertices[i]);
        }

        view.BufferLocation = GetDebugManager()->UploadConstants(
            &inactiveVB[0], sizeof(vertSprite) * inactiveVertices.size());
        view.SizeInBytes = UINT(sizeof(vertSprite) * inactiveVertices.size());

        for(size_t i = 0; i < inactiveVertices.size(); i++)
        {
          list->IASetVertexBuffers(0, 1, &view);

          list->DrawInstanced(4, 1, 0, 0);

          view.BufferLocation += sizeof(FloatVector) * 4;
        }
      }
    }
  }

  D3D12MarkerRegion::End(list);

  list->Close();

  if(D3D12_Debug_SingleSubmitFlushing())
  {
    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();
  }
}
