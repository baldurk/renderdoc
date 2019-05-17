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

#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"
#include "d3d11_resources.h"

#include "data/hlsl/hlsl_cbuffers.h"

void D3D11Replay::RenderMesh(uint32_t eventId, const std::vector<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
  if(cfg.position.vertexResourceId == ResourceId() || cfg.position.numIndices == 0)
    return;

  D3D11MarkerRegion renderMesh(
      StringFormat::Fmt("RenderMesh with %zu secondary draws", secondaryDraws.size()));

  MeshVertexCBuffer vertexData;
  MeshPixelCBuffer pixelData;

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, m_OutputWidth / m_OutputHeight);

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();
  Matrix4f guessProjInv;

  vertexData.ModelViewProj = projMat.Mul(camMat);
  vertexData.SpriteSize = Vec2f();

  Vec4f col(0.0f, 0.0f, 0.0f, 1.0f);
  ID3D11Buffer *psCBuf = GetDebugManager()->MakeCBuffer(&col, sizeof(col));

  m_pImmediateContext->PSSetConstantBuffers(0, 1, &psCBuf);
  m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);

  m_pImmediateContext->HSSetShader(NULL, NULL, 0);
  m_pImmediateContext->DSSetShader(NULL, NULL, 0);
  m_pImmediateContext->GSSetShader(NULL, NULL, 0);

  m_pImmediateContext->OMSetDepthStencilState(NULL, 0);
  m_pImmediateContext->OMSetBlendState(m_MeshRender.WireframeHelpersBS, NULL, 0xffffffff);

  // don't cull in wireframe mesh display
  m_pImmediateContext->RSSetState(m_MeshRender.WireframeRasterState);

  const ResourceFormat &resFmt = cfg.position.format;
  const ResourceFormat &resFmt2 = cfg.second.format;

  if(m_MeshRender.PrevPositionFormat != resFmt || m_MeshRender.PrevSecondaryFormat != resFmt2)
  {
    SAFE_RELEASE(m_MeshRender.MeshLayout);

    D3D11_INPUT_ELEMENT_DESC layoutdesc[2];

    layoutdesc[0].SemanticName = "pos";
    layoutdesc[0].SemanticIndex = 0;
    layoutdesc[0].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    if(cfg.position.vertexResourceId != ResourceId() && (resFmt.Special() || resFmt.compCount > 0))
      layoutdesc[0].Format = MakeDXGIFormat(resFmt);
    layoutdesc[0].AlignedByteOffset = 0;    // offset will be handled by vertex buffer offset
    layoutdesc[0].InputSlot = 0;
    layoutdesc[0].InputSlotClass =
        cfg.position.instanced ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
    layoutdesc[0].InstanceDataStepRate = 0;

    layoutdesc[1].SemanticName = "sec";
    layoutdesc[1].SemanticIndex = 0;
    layoutdesc[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    if(cfg.second.vertexResourceId != ResourceId() && (resFmt2.Special() || resFmt2.compCount > 0))
      layoutdesc[1].Format = MakeDXGIFormat(resFmt2);
    layoutdesc[1].AlignedByteOffset = 0;
    layoutdesc[1].InputSlot = 1;
    layoutdesc[1].InputSlotClass =
        cfg.second.instanced ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
    layoutdesc[1].InstanceDataStepRate = 0;

    HRESULT hr = m_pDevice->CreateInputLayout(layoutdesc, 2, m_MeshRender.MeshVSBytecode,
                                              m_MeshRender.MeshVSBytelen, &m_MeshRender.MeshLayout);

    if(FAILED(hr))
    {
      RDCERR("Failed to create m_MeshRender.m_MeshDisplayLayout HRESULT: %s", ToStr(hr).c_str());
      m_MeshRender.MeshLayout = NULL;
    }
  }

  m_MeshRender.PrevPositionFormat = resFmt;
  m_MeshRender.PrevSecondaryFormat = resFmt2;

  RDCASSERT(cfg.position.indexByteOffset < 0xffffffff);

  ID3D11Buffer *ibuf = NULL;
  DXGI_FORMAT ifmt = DXGI_FORMAT_R16_UINT;
  UINT ioffs = (UINT)cfg.position.indexByteOffset;

  D3D11_PRIMITIVE_TOPOLOGY topo = MakeD3DPrimitiveTopology(cfg.position.topology);

  ID3D11Buffer *vsCBuf = NULL;

  // render the mesh itself (solid, then wireframe)
  {
    if(cfg.position.unproject)
    {
      // the derivation of the projection matrix might not be right (hell, it could be an
      // orthographic projection). But it'll be close enough likely.
      Matrix4f guessProj =
          cfg.position.farPlane != FLT_MAX
              ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane,
                                      cfg.aspect)
              : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

      if(cfg.ortho)
      {
        guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
      }

      guessProjInv = guessProj.Inverse();

      vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
    }

    vsCBuf = GetDebugManager()->MakeCBuffer(&vertexData, sizeof(vertexData));

    m_pImmediateContext->VSSetConstantBuffers(0, 1, &vsCBuf);
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &psCBuf);

    m_pImmediateContext->VSSetShader(m_MeshRender.MeshVS, NULL, 0);
    m_pImmediateContext->PSSetShader(m_MeshRender.MeshPS, NULL, 0);

    // secondary draws - this is the "draw since last clear" feature. We don't have
    // full flexibility, it only draws wireframe, and only the final rasterized position.
    if(secondaryDraws.size() > 0)
    {
      m_pImmediateContext->IASetInputLayout(m_MeshRender.GenericLayout);

      pixelData.MeshDisplayFormat = MESHDISPLAY_SOLID;

      for(size_t i = 0; i < secondaryDraws.size(); i++)
      {
        const MeshFormat &fmt = secondaryDraws[i];

        if(fmt.vertexResourceId != ResourceId())
        {
          pixelData.MeshColour = Vec3f(fmt.meshColor.x, fmt.meshColor.y, fmt.meshColor.z);
          GetDebugManager()->FillCBuffer(psCBuf, &pixelData, sizeof(pixelData));
          m_pImmediateContext->PSSetConstantBuffers(0, 1, &psCBuf);

          m_pImmediateContext->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(fmt.topology));

          auto it = WrappedID3D11Buffer::m_BufferList.find(fmt.vertexResourceId);

          ID3D11Buffer *buf = it->second.m_Buffer;
          m_pImmediateContext->IASetVertexBuffers(0, 1, &buf, (UINT *)&fmt.vertexByteStride,
                                                  (UINT *)&fmt.vertexByteOffset);
          if(fmt.indexResourceId != ResourceId())
          {
            RDCASSERT(fmt.indexByteOffset < 0xffffffff);

            it = WrappedID3D11Buffer::m_BufferList.find(fmt.indexResourceId);
            buf = it->second.m_Buffer;
            m_pImmediateContext->IASetIndexBuffer(
                buf, fmt.indexByteStride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
                (UINT)fmt.indexByteOffset);

            m_pImmediateContext->DrawIndexed(fmt.numIndices, 0, fmt.baseVertex);
          }
          else
          {
            m_pImmediateContext->Draw(fmt.numIndices, 0);
          }
        }
      }
    }

    ID3D11InputLayout *layout = m_MeshRender.MeshLayout;

    if(layout == NULL)
    {
      RDCWARN("Couldn't get a mesh display layout");
      return;
    }

    m_pImmediateContext->IASetInputLayout(layout);

    RDCASSERT(cfg.position.vertexByteOffset < 0xffffffff && cfg.second.vertexByteOffset < 0xffffffff);

    ID3D11Buffer *vbs[2] = {NULL, NULL};
    UINT str[] = {cfg.position.vertexByteStride, cfg.second.vertexByteStride};
    UINT offs[] = {(UINT)cfg.position.vertexByteOffset, (UINT)cfg.second.vertexByteOffset};

    // we source all data from the first instanced value in the instanced case, so make sure we
    // offset correctly here.
    if(cfg.position.instanced && cfg.position.instStepRate)
      offs[0] += cfg.position.vertexByteStride * (cfg.curInstance / cfg.position.instStepRate);

    if(cfg.second.instanced && cfg.second.instStepRate)
      offs[1] += cfg.second.vertexByteStride * (cfg.curInstance / cfg.second.instStepRate);

    {
      auto it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.vertexResourceId);

      if(it != WrappedID3D11Buffer::m_BufferList.end())
        vbs[0] = it->second.m_Buffer;

      it = WrappedID3D11Buffer::m_BufferList.find(cfg.second.vertexResourceId);

      if(it != WrappedID3D11Buffer::m_BufferList.end())
        vbs[1] = it->second.m_Buffer;

      it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.indexResourceId);

      if(it != WrappedID3D11Buffer::m_BufferList.end())
        ibuf = it->second.m_Buffer;

      if(cfg.position.indexByteStride == 4)
        ifmt = DXGI_FORMAT_R32_UINT;
    }

    m_pImmediateContext->IASetVertexBuffers(0, 2, vbs, str, offs);
    if(cfg.position.indexByteStride)
      m_pImmediateContext->IASetIndexBuffer(ibuf, ifmt, ioffs);
    else
      m_pImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, NULL);

    // draw solid shaded mode
    if(cfg.solidShadeMode != SolidShade::NoSolid && cfg.position.topology < Topology::PatchList_1CPs)
    {
      m_pImmediateContext->RSSetState(m_General.RasterState);

      m_pImmediateContext->IASetPrimitiveTopology(topo);

      pixelData.MeshDisplayFormat = (int)cfg.solidShadeMode;
      if(cfg.solidShadeMode == SolidShade::Secondary && cfg.second.showAlpha)
        pixelData.MeshDisplayFormat = MESHDISPLAY_SECONDARY_ALPHA;

      pixelData.MeshColour = Vec3f(0.8f, 0.8f, 0.0f);
      GetDebugManager()->FillCBuffer(psCBuf, &pixelData, sizeof(pixelData));
      m_pImmediateContext->PSSetConstantBuffers(0, 1, &psCBuf);

      if(cfg.solidShadeMode == SolidShade::Lit)
      {
        MeshGeometryCBuffer geomData;

        geomData.InvProj = projMat.Inverse();

        ID3D11Buffer *gsBuf = GetDebugManager()->MakeCBuffer(&geomData, sizeof(MeshGeometryCBuffer));

        m_pImmediateContext->GSSetConstantBuffers(0, 1, &gsBuf);

        m_pImmediateContext->GSSetShader(m_MeshRender.MeshGS, NULL, 0);
      }

      if(cfg.position.indexByteStride)
        m_pImmediateContext->DrawIndexed(cfg.position.numIndices, 0, cfg.position.baseVertex);
      else
        m_pImmediateContext->Draw(cfg.position.numIndices, 0);

      if(cfg.solidShadeMode == SolidShade::Lit)
        m_pImmediateContext->GSSetShader(NULL, NULL, 0);
    }

    // draw wireframe mode
    if(cfg.solidShadeMode == SolidShade::NoSolid || cfg.wireframeDraw ||
       cfg.position.topology >= Topology::PatchList_1CPs)
    {
      m_pImmediateContext->RSSetState(m_MeshRender.WireframeRasterState);

      m_pImmediateContext->OMSetDepthStencilState(m_MeshRender.LessEqualDepthState, 0);

      pixelData.MeshDisplayFormat = MESHDISPLAY_SOLID;
      pixelData.MeshColour =
          Vec4f(cfg.position.meshColor.x, cfg.position.meshColor.y, cfg.position.meshColor.z);
      GetDebugManager()->FillCBuffer(psCBuf, &pixelData, sizeof(pixelData));
      m_pImmediateContext->PSSetConstantBuffers(0, 1, &psCBuf);

      if(cfg.position.topology >= Topology::PatchList_1CPs)
        m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
      else
        m_pImmediateContext->IASetPrimitiveTopology(topo);

      if(cfg.position.indexByteStride)
        m_pImmediateContext->DrawIndexed(cfg.position.numIndices, 0, cfg.position.baseVertex);
      else
        m_pImmediateContext->Draw(cfg.position.numIndices, 0);
    }
  }

  m_pImmediateContext->RSSetState(m_MeshRender.WireframeRasterState);

  // set up state for drawing helpers
  {
    vertexData.ModelViewProj = projMat.Mul(camMat);
    GetDebugManager()->FillCBuffer(vsCBuf, &vertexData, sizeof(vertexData));

    m_pImmediateContext->RSSetState(m_MeshRender.SolidRasterState);

    m_pImmediateContext->OMSetDepthStencilState(m_MeshRender.NoDepthState, 0);

    m_pImmediateContext->VSSetConstantBuffers(0, 1, &vsCBuf);
    m_pImmediateContext->VSSetShader(m_MeshRender.MeshVS, NULL, 0);
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &psCBuf);
    m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);
  }

  // axis markers
  if(!cfg.position.unproject)
  {
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &psCBuf);

    UINT strides[] = {sizeof(Vec4f)};
    UINT offsets[] = {0};

    m_pImmediateContext->IASetVertexBuffers(0, 1, &m_MeshRender.AxisHelper, strides, offsets);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_pImmediateContext->IASetInputLayout(m_MeshRender.GenericLayout);

    col = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
    GetDebugManager()->FillCBuffer(psCBuf, &col, sizeof(col));
    m_pImmediateContext->Draw(2, 0);

    col = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
    GetDebugManager()->FillCBuffer(psCBuf, &col, sizeof(col));
    m_pImmediateContext->Draw(2, 2);

    col = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
    GetDebugManager()->FillCBuffer(psCBuf, &col, sizeof(col));
    m_pImmediateContext->Draw(2, 4);
  }

  if(cfg.highlightVert != ~0U)
  {
    m_HighlightCache.CacheHighlightingData(eventId, cfg);

    D3D11_PRIMITIVE_TOPOLOGY meshtopo = topo;

    ///////////////////////////////////////////////////////////////
    // vectors to be set from buffers, depending on topology

    // this vert (blue dot, required)
    FloatVector activeVertex;

    // primitive this vert is a part of (red prim, optional)
    std::vector<FloatVector> activePrim;

    // for patch lists, to show other verts in patch (green dots, optional)
    // for non-patch lists, we use the activePrim and adjacentPrimVertices
    // to show what other verts are related
    std::vector<FloatVector> inactiveVertices;

    // adjacency (line or tri, strips or lists) (green prims, optional)
    // will be N*M long, N adjacent prims of M verts each. M = primSize below
    std::vector<FloatVector> adjacentPrimVertices;

    D3D11_PRIMITIVE_TOPOLOGY primTopo =
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;    // tri or line list
    uint32_t primSize = 3;                        // number of verts per primitive

    if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINELIST ||
       meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ ||
       meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP ||
       meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
    {
      primSize = 2;
      primTopo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
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
        vertexData.ModelViewProj = projMat.Mul(camMat);

      m_pImmediateContext->IASetInputLayout(m_MeshRender.GenericLayout);

      GetDebugManager()->FillCBuffer(vsCBuf, &vertexData, sizeof(vertexData));

      D3D11_MAPPED_SUBRESOURCE mapped;
      HRESULT hr = S_OK;
      UINT strides[] = {sizeof(Vec4f)};
      UINT offsets[] = {0};
      m_pImmediateContext->IASetVertexBuffers(0, 1, &m_MeshRender.TriHighlightHelper,
                                              (UINT *)&strides, (UINT *)&offsets);

      ////////////////////////////////////////////////////////////////
      // render primitives

      m_pImmediateContext->IASetPrimitiveTopology(primTopo);

      // Draw active primitive (red)
      col = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
      GetDebugManager()->FillCBuffer(psCBuf, &col, sizeof(col));

      if(activePrim.size() >= primSize)
      {
        hr = m_pImmediateContext->Map(m_MeshRender.TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD,
                                      0, &mapped);

        if(FAILED(hr))
        {
          RDCERR("Failde to map m_MeshRender.m_TriHighlightHelper HRESULT: %s", ToStr(hr).c_str());
          return;
        }

        memcpy(mapped.pData, &activePrim[0], sizeof(Vec4f) * primSize);
        m_pImmediateContext->Unmap(m_MeshRender.TriHighlightHelper, 0);

        m_pImmediateContext->Draw(primSize, 0);
      }

      // Draw adjacent primitives (green)
      col = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
      GetDebugManager()->FillCBuffer(psCBuf, &col, sizeof(col));

      if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
      {
        hr = m_pImmediateContext->Map(m_MeshRender.TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD,
                                      0, &mapped);

        if(FAILED(hr))
        {
          RDCERR("Failde to map m_MeshRender.m_TriHighlightHelper HRESULT: %s", ToStr(hr).c_str());
          return;
        }

        memcpy(mapped.pData, &adjacentPrimVertices[0], sizeof(Vec4f) * adjacentPrimVertices.size());
        m_pImmediateContext->Unmap(m_MeshRender.TriHighlightHelper, 0);

        m_pImmediateContext->Draw((UINT)adjacentPrimVertices.size(), 0);
      }

      ////////////////////////////////////////////////////////////////
      // prepare to render dots (set new VS params and topology)
      float scale = 800.0f / m_OutputHeight;
      float asp = m_OutputWidth / m_OutputHeight;

      vertexData.SpriteSize = Vec2f(scale / asp, scale);
      GetDebugManager()->FillCBuffer(vsCBuf, &vertexData, sizeof(vertexData));

      // Draw active vertex (blue)
      col = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
      GetDebugManager()->FillCBuffer(psCBuf, &col, sizeof(col));

      m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      FloatVector vertSprite[4] = {
          activeVertex, activeVertex, activeVertex, activeVertex,
      };

      hr = m_pImmediateContext->Map(m_MeshRender.TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0,
                                    &mapped);

      if(FAILED(hr))
      {
        RDCERR("Failde to map m_MeshRender.m_TriHighlightHelper HRESULT: %s", ToStr(hr).c_str());
        return;
      }

      memcpy(mapped.pData, vertSprite, sizeof(vertSprite));
      m_pImmediateContext->Unmap(m_MeshRender.TriHighlightHelper, 0);

      m_pImmediateContext->Draw(4, 0);

      // Draw inactive vertices (green)
      col = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
      GetDebugManager()->FillCBuffer(psCBuf, &col, sizeof(col));

      for(size_t i = 0; i < inactiveVertices.size(); i++)
      {
        vertSprite[0] = vertSprite[1] = vertSprite[2] = vertSprite[3] = inactiveVertices[i];

        hr = m_pImmediateContext->Map(m_MeshRender.TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD,
                                      0, &mapped);

        if(FAILED(hr))
        {
          RDCERR("Failde to map m_MeshRender.m_TriHighlightHelper HRESULT: %s", ToStr(hr).c_str());
          return;
        }

        memcpy(mapped.pData, vertSprite, sizeof(vertSprite));
        m_pImmediateContext->Unmap(m_MeshRender.TriHighlightHelper, 0);

        m_pImmediateContext->Draw(4, 0);
      }
    }

    if(cfg.position.unproject)
      m_pImmediateContext->VSSetShader(m_MeshRender.MeshVS, NULL, 0);
  }

  // bounding box
  if(cfg.showBBox)
  {
    UINT strides[] = {sizeof(Vec4f)};
    UINT offsets[] = {0};
    D3D11_MAPPED_SUBRESOURCE mapped;

    vertexData.SpriteSize = Vec2f();
    vertexData.ModelViewProj = projMat.Mul(camMat);
    GetDebugManager()->FillCBuffer(vsCBuf, &vertexData, sizeof(vertexData));

    HRESULT hr = m_pImmediateContext->Map(m_MeshRender.TriHighlightHelper, 0,
                                          D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    RDCASSERTEQUAL(hr, S_OK);

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

    memcpy(mapped.pData, bbox, sizeof(bbox));

    m_pImmediateContext->Unmap(m_MeshRender.TriHighlightHelper, 0);

    // we want this to clip
    m_pImmediateContext->OMSetDepthStencilState(m_MeshRender.LessEqualDepthState, 0);

    m_pImmediateContext->IASetVertexBuffers(0, 1, &m_MeshRender.TriHighlightHelper,
                                            (UINT *)&strides, (UINT *)&offsets);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_pImmediateContext->IASetInputLayout(m_MeshRender.GenericLayout);

    col = Vec4f(0.2f, 0.2f, 1.0f, 1.0f);
    GetDebugManager()->FillCBuffer(psCBuf, &col, sizeof(col));

    m_pImmediateContext->Draw(24, 0);

    m_pImmediateContext->OMSetDepthStencilState(m_MeshRender.NoDepthState, 0);
  }

  // 'fake' helper frustum
  if(cfg.position.unproject)
  {
    UINT strides[] = {sizeof(Vec4f)};
    UINT offsets[] = {0};

    vertexData.SpriteSize = Vec2f();
    vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
    GetDebugManager()->FillCBuffer(vsCBuf, &vertexData, sizeof(vertexData));

    m_pImmediateContext->IASetVertexBuffers(0, 1, &m_MeshRender.FrustumHelper, (UINT *)&strides,
                                            (UINT *)&offsets);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_pImmediateContext->IASetInputLayout(m_MeshRender.GenericLayout);

    col = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    GetDebugManager()->FillCBuffer(psCBuf, &col, sizeof(col));

    m_pImmediateContext->Draw(24, 0);
  }
}
