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

#include <algorithm>
#include "data/resource.h"
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_manager.h"
#include "d3d11_renderstate.h"
#include "d3d11_replay.h"
#include "d3d11_resources.h"

struct ScopedOOMHandle11
{
  ScopedOOMHandle11(WrappedID3D11Device *dev)
  {
    m_pDevice = dev;
    m_pDevice->HandleOOM(true);
  }

  ~ScopedOOMHandle11() { m_pDevice->HandleOOM(false); }
  WrappedID3D11Device *m_pDevice;
};

void D3D11Replay::InitStreamOut()
{
  CreateSOBuffers();

  HRESULT hr = S_OK;

  D3D11_QUERY_DESC qdesc;
  qdesc.MiscFlags = 0;
  qdesc.Query = D3D11_QUERY_SO_STATISTICS;

  m_SOStatsQueries.push_back(NULL);
  hr = m_pDevice->CreateQuery(&qdesc, &m_SOStatsQueries[0]);
  if(FAILED(hr))
    RDCERR("Failed to create m_SOStatsQuery HRESULT: %s", ToStr(hr).c_str());
}

void D3D11Replay::ShutdownStreamOut()
{
  SAFE_RELEASE(m_SOBuffer);
  for(ID3D11Query *q : m_SOStatsQueries)
    SAFE_RELEASE(q);
  SAFE_RELEASE(m_SOStagingBuffer);
}

void D3D11Replay::CreateSOBuffers()
{
  HRESULT hr = S_OK;

  SAFE_RELEASE(m_SOBuffer);
  SAFE_RELEASE(m_SOStagingBuffer);

  if(m_SOBufferSize > 0xFFFF0000ULL ||
     // workaround nv driver bug, it crashes copying with an offset over 2GB (which we need for
     // readback). Treat this as an OOM scenario
     (m_DriverInfo.vendor == GPUVendor::nVidia && m_SOBufferSize > 0x80000000ULL))
  {
    RDCERR("Can't resize stream-out buffer to larger than 4GB, needed %llu bytes.", m_SOBufferSize);
    SAFE_RELEASE(m_SOBuffer);
    SAFE_RELEASE(m_SOStagingBuffer);
    m_SOBufferSize = 0;
    return;
  }

  D3D11_BUFFER_DESC bufferDesc = {
      (uint32_t)m_SOBufferSize, D3D11_USAGE_DEFAULT, D3D11_BIND_STREAM_OUTPUT, 0, 0, 0};

  hr = m_pDevice->CreateBuffer(&bufferDesc, NULL, &m_SOBuffer);

  if(FAILED(hr))
    RDCERR("Failed to create m_SOBuffer HRESULT: %s", ToStr(hr).c_str());

  bufferDesc.Usage = D3D11_USAGE_STAGING;
  bufferDesc.BindFlags = 0;
  bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  hr = m_pDevice->CreateBuffer(&bufferDesc, NULL, &m_SOStagingBuffer);
  if(FAILED(hr))
    RDCERR("Failed to create m_SOStagingBuffer HRESULT: %s", ToStr(hr).c_str());

  if(!m_SOBuffer || !m_SOStagingBuffer)
  {
    SAFE_RELEASE(m_SOBuffer);
    SAFE_RELEASE(m_SOStagingBuffer);
    m_SOBufferSize = 0;
  }
}

void D3D11Replay::ClearPostVSCache()
{
  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    SAFE_RELEASE(it->second.vsout.buf);
    SAFE_RELEASE(it->second.vsout.idxBuf);
    SAFE_RELEASE(it->second.gsout.buf);
    SAFE_RELEASE(it->second.gsout.idxBuf);
  }

  m_PostVSData.clear();
}

MeshFormat D3D11Replay::GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                                         MeshDataStage stage)
{
  D3D11PostVSData postvs;
  RDCEraseEl(postvs);

  // no multiview support
  (void)viewID;

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    postvs = m_PostVSData[eventId];

  const D3D11PostVSData::StageData &s = postvs.GetStage(stage);

  MeshFormat ret;

  ret.indexByteOffset = 0;
  ret.baseVertex = 0;

  if(s.useIndices && s.idxBuf)
  {
    ret.indexResourceId = ((WrappedID3D11Buffer *)s.idxBuf)->GetResourceID();
    ret.indexByteStride = s.idxFmt == DXGI_FORMAT_R16_UINT ? 2 : 4;
    ret.indexByteSize = ~0ULL;
  }
  else
  {
    ret.indexResourceId = ResourceId();
    ret.indexByteStride = 0;
  }

  if(s.buf)
  {
    ret.vertexResourceId = ((WrappedID3D11Buffer *)s.buf)->GetResourceID();
    ret.vertexByteSize = ~0ULL;
  }
  else
  {
    ret.vertexResourceId = ResourceId();
  }

  ret.vertexByteOffset = s.instStride * instID;
  ret.vertexByteStride = s.vertStride;

  ret.format.compCount = 4;
  ret.format.compByteWidth = 4;
  ret.format.compType = CompType::Float;
  ret.format.type = ResourceFormatType::Regular;

  ret.showAlpha = false;

  ret.topology = MakePrimitiveTopology(s.topo);
  ret.numIndices = s.numVerts;

  ret.unproject = s.hasPosOut;
  ret.nearPlane = s.nearPlane;
  ret.farPlane = s.farPlane;

  if(instID < s.instData.size())
  {
    D3D11PostVSData::InstData inst = s.instData[instID];

    ret.vertexByteOffset = inst.bufOffset;
    ret.numIndices = inst.numVerts;
  }

  ret.status = s.status;

  return ret;
}

void D3D11Replay::InitPostVSBuffers(uint32_t eventId)
{
  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    return;

  D3D11PostVSData &ret = m_PostVSData[eventId];

  // we handle out-of-memory errors while processing postvs, don't treat it as a fatal error
  ScopedOOMHandle11 oom(m_pDevice);

  D3D11MarkerRegion postvs(StringFormat::Fmt("PostVS for %u", eventId));

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11VertexShader *vs = NULL;
  m_pImmediateContext->VSGetShader(&vs, NULL, NULL);

  ID3D11GeometryShader *gs = NULL;
  m_pImmediateContext->GSGetShader(&gs, NULL, NULL);

  ID3D11HullShader *hs = NULL;
  m_pImmediateContext->HSGetShader(&hs, NULL, NULL);

  ID3D11DomainShader *ds = NULL;
  m_pImmediateContext->DSGetShader(&ds, NULL, NULL);

  if(vs)
    vs->Release();
  if(gs)
    gs->Release();
  if(hs)
    hs->Release();
  if(ds)
    ds->Release();

  if(!vs)
  {
    ret.gsout.status = ret.vsout.status = "No vertex shader bound";
    return;
  }

  D3D11_PRIMITIVE_TOPOLOGY topo;
  m_pImmediateContext->IAGetPrimitiveTopology(&topo);

  WrappedID3D11Shader<ID3D11VertexShader> *wrappedVS = (WrappedID3D11Shader<ID3D11VertexShader> *)vs;

  const ActionDescription *action = m_pDevice->GetAction(eventId);

  if(action->numIndices == 0)
  {
    ret.gsout.status = ret.vsout.status = "Empty drawcall (0 indices/vertices)";
    return;
  }

  if((action->flags & ActionFlags::Instanced) && action->numInstances == 0)
  {
    ret.gsout.status = ret.vsout.status = "Empty drawcall (0 instances)";
    return;
  }

  DXBC::DXBCContainer *dxbcVS = wrappedVS->GetDXBC();

  RDCASSERT(dxbcVS);

  DXBC::DXBCContainer *dxbcGS = NULL;

  if(gs)
  {
    WrappedID3D11Shader<ID3D11GeometryShader> *wrappedGS =
        (WrappedID3D11Shader<ID3D11GeometryShader> *)gs;

    dxbcGS = wrappedGS->GetDXBC();

    RDCASSERT(dxbcGS);
  }

  DXBC::DXBCContainer *dxbcDS = NULL;

  if(ds)
  {
    WrappedID3D11Shader<ID3D11DomainShader> *wrappedDS =
        (WrappedID3D11Shader<ID3D11DomainShader> *)ds;

    dxbcDS = wrappedDS->GetDXBC();

    RDCASSERT(dxbcDS);
  }

  ResourceId lastShaderId = GetIDForDeviceChild(ds);
  DXBC::DXBCContainer *lastShader = dxbcDS;
  if(dxbcGS)
  {
    lastShaderId = GetIDForDeviceChild(gs);
    lastShader = dxbcGS;
  }

  if(lastShader)
  {
    // put a general error in here in case anything goes wrong fetching VS outputs
    ret.gsout.status =
        "No geometry/tessellation output fetched due to error processing vertex stage.";
  }
  else
  {
    ret.gsout.status = "No geometry and no tessellation shader bound.";
  }

  rdcarray<D3D11_SO_DECLARATION_ENTRY> sodecls;

  UINT stride = 0;
  int posidx = -1;
  int numPosComponents = 0;

  ID3D11GeometryShader *streamoutGS = NULL;

  if(!dxbcVS->GetReflection()->OutputSig.empty())
  {
    for(size_t i = 0; i < dxbcVS->GetReflection()->OutputSig.size(); i++)
    {
      const SigParameter &sign = dxbcVS->GetReflection()->OutputSig[i];

      D3D11_SO_DECLARATION_ENTRY decl;

      decl.Stream = 0;
      decl.OutputSlot = 0;

      decl.SemanticName = sign.semanticName.c_str();
      decl.SemanticIndex = sign.semanticIndex;
      decl.StartComponent = 0;
      decl.ComponentCount = sign.compCount & 0xff;

      if(sign.systemValue == ShaderBuiltin::Position)
      {
        posidx = (int)sodecls.size();
        numPosComponents = decl.ComponentCount = 4;
      }

      stride += decl.ComponentCount * sizeof(float);
      sodecls.push_back(decl);
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      D3D11_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(posidx);
      sodecls.insert(0, pos);
    }

    HRESULT hr = m_pDevice->CreateGeometryShaderWithStreamOutput(
        (void *)dxbcVS->GetShaderBlob().data(), dxbcVS->GetShaderBlob().size(), &sodecls[0],
        (UINT)sodecls.size(), &stride, 1, D3D11_SO_NO_RASTERIZED_STREAM, NULL, &streamoutGS);

    if(FAILED(hr))
    {
      ret.vsout.status =
          StringFormat::Fmt("Failed to fetch output via streamout, HRESULT: %s", ToStr(hr).c_str());
      RDCERR("%s", ret.vsout.status.c_str());
      return;
    }

    m_pImmediateContext->GSSetShader(streamoutGS, NULL, 0);
    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);

    SAFE_RELEASE(streamoutGS);

    UINT offset = 0;
    ID3D11Buffer *idxBuf = NULL;
    DXGI_FORMAT idxFmt = DXGI_FORMAT_UNKNOWN;
    UINT idxOffs = 0;

    m_pImmediateContext->IAGetIndexBuffer(&idxBuf, &idxFmt, &idxOffs);

    ID3D11Buffer *origBuf = idxBuf;

    if(!(action->flags & ActionFlags::Indexed))
    {
      m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

      SAFE_RELEASE(idxBuf);

      uint64_t outputSize = stride * uint64_t(action->numIndices);
      if(action->flags & ActionFlags::Instanced)
        outputSize *= action->numInstances;

      if(m_SOBufferSize < outputSize)
      {
        const uint64_t oldSize = m_SOBufferSize;
        const uint64_t newSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
        m_SOBufferSize = newSize;
        RDCWARN("Resizing stream-out buffer from %llu to %llu", oldSize, newSize);
        CreateSOBuffers();

        if(!m_SOStagingBuffer)
        {
          ret.vsout.status = StringFormat::Fmt(
              "Vertex output generated %llu bytes of data which ran out of memory", newSize);
          return;
        }
      }

      m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);

      m_pImmediateContext->Begin(m_SOStatsQueries[0]);

      if(action->flags & ActionFlags::Instanced)
        m_pImmediateContext->DrawInstanced(action->numIndices, action->numInstances,
                                           action->vertexOffset, action->instanceOffset);
      else
        m_pImmediateContext->Draw(action->numIndices, action->vertexOffset);

      m_pImmediateContext->End(m_SOStatsQueries[0]);
    }
    else    // drawcall is indexed
    {
      bool index16 = (idxFmt == DXGI_FORMAT_R16_UINT);
      UINT bytesize = index16 ? 2 : 4;

      bytebuf idxdata;
      GetDebugManager()->GetBufferData(idxBuf, idxOffs + action->indexOffset * bytesize,
                                       action->numIndices * bytesize, idxdata);

      SAFE_RELEASE(idxBuf);

      rdcarray<uint32_t> indices;

      uint16_t *idx16 = (uint16_t *)&idxdata[0];
      uint32_t *idx32 = (uint32_t *)&idxdata[0];

      // only read as many indices as were available in the buffer
      uint32_t numIndices =
          RDCMIN(uint32_t(index16 ? idxdata.size() / 2 : idxdata.size() / 4), action->numIndices);

      // grab all unique vertex indices referenced
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

        auto it = std::lower_bound(indices.begin(), indices.end(), i32);

        if(it != indices.end() && *it == i32)
          continue;

        indices.insert(it - indices.begin(), i32);
      }

      // if we read out of bounds, we'll also have a 0 index being referenced
      // (as 0 is read). Don't insert 0 if we already have 0 though
      if(numIndices < action->numIndices && (indices.empty() || indices[0] != 0))
        indices.insert(0, 0);

      // An index buffer could be something like: 500, 501, 502, 501, 503, 502
      // in which case we can't use the existing index buffer without filling 499 slots of vertex
      // data with padding. Instead we rebase the indices based on the smallest vertex so it becomes
      // 0, 1, 2, 1, 3, 2 and then that matches our stream-out'd buffer.
      //
      // Note that there could also be gaps, like: 500, 501, 502, 510, 511, 512
      // which would become 0, 1, 2, 3, 4, 5 and so the old index buffer would no longer be valid.
      // We just stream-out a tightly packed list of unique indices, and then remap the index buffer
      // so that what did point to 500 points to 0 (accounting for rebasing), and what did point
      // to 510 now points to 3 (accounting for the unique sort).

      // we use a map here since the indices may be sparse. Especially considering if an index
      // is 'invalid' like 0xcccccccc then we don't want an array of 3.4 billion entries.
      std::map<uint32_t, size_t> indexRemap;
      for(size_t i = 0; i < indices.size(); i++)
      {
        // by definition, this index will only appear once in indices[]
        indexRemap[indices[i]] = i;
      }

      D3D11_BUFFER_DESC desc = {UINT(sizeof(uint32_t) * indices.size()),
                                D3D11_USAGE_IMMUTABLE,
                                D3D11_BIND_INDEX_BUFFER,
                                0,
                                0,
                                0};
      D3D11_SUBRESOURCE_DATA initData = {&indices[0], desc.ByteWidth, desc.ByteWidth};

      if(!indices.empty())
        m_pDevice->CreateBuffer(&desc, &initData, &idxBuf);
      else
        idxBuf = NULL;

      m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
      m_pImmediateContext->IASetIndexBuffer(idxBuf, DXGI_FORMAT_R32_UINT, 0);
      SAFE_RELEASE(idxBuf);

      uint64_t outputSize = stride * uint64_t(indices.size());
      if(action->flags & ActionFlags::Instanced)
        outputSize *= action->numInstances;

      if(m_SOBufferSize < outputSize)
      {
        const uint64_t oldSize = m_SOBufferSize;
        const uint64_t newSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
        m_SOBufferSize = newSize;
        RDCWARN("Resizing stream-out buffer from %llu to %llu", oldSize, newSize);
        CreateSOBuffers();

        if(!m_SOStagingBuffer)
        {
          ret.vsout.status = StringFormat::Fmt(
              "Vertex output generated %llu bytes of data which ran out of memory", newSize);
          return;
        }
      }

      m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);

      m_pImmediateContext->Begin(m_SOStatsQueries[0]);

      if(action->flags & ActionFlags::Instanced)
        m_pImmediateContext->DrawIndexedInstanced((UINT)indices.size(), action->numInstances, 0,
                                                  action->baseVertex, action->instanceOffset);
      else
        m_pImmediateContext->DrawIndexed((UINT)indices.size(), 0, action->baseVertex);

      m_pImmediateContext->End(m_SOStatsQueries[0]);

      // rebase existing index buffer to point to the right elements in our stream-out'd
      // vertex buffer
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

        // preserve primitive restart indices
        if(i32 == (index16 ? 0xffff : 0xffffffff))
          continue;

        if(index16)
          idx16[i] = uint16_t(indexRemap[i32]);
        else
          idx32[i] = uint32_t(indexRemap[i32]);
      }

      desc.ByteWidth = (UINT)idxdata.size();
      initData.pSysMem = &idxdata[0];
      initData.SysMemPitch = initData.SysMemSlicePitch = desc.ByteWidth;

      if(desc.ByteWidth > 0)
        m_pDevice->CreateBuffer(&desc, &initData, &idxBuf);
      else
        idxBuf = NULL;
    }

    m_pImmediateContext->IASetPrimitiveTopology(topo);
    m_pImmediateContext->IASetIndexBuffer(origBuf, idxFmt, idxOffs);

    m_pImmediateContext->GSSetShader(NULL, NULL, 0);
    m_pImmediateContext->SOSetTargets(0, NULL, NULL);

    D3D11_QUERY_DATA_SO_STATISTICS numPrims;

    m_pImmediateContext->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    do
    {
      hr = m_pImmediateContext->GetData(m_SOStatsQueries[0], &numPrims,
                                        sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
    } while(hr == S_FALSE);

    if(numPrims.NumPrimitivesWritten == 0)
    {
      ret.vsout.status = "Failed to generate vertex output data on GPU";
      SAFE_RELEASE(idxBuf);
      return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_pImmediateContext->Map(m_SOStagingBuffer, 0, D3D11_MAP_READ, 0, &mapped);

    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer HRESULT: %s", ToStr(hr).c_str());
      ret.vsout.status = "Couldn't read back vertex output data from GPU";
      SAFE_RELEASE(idxBuf);
      return;
    }

    D3D11_BUFFER_DESC bufferDesc = {stride * (uint32_t)numPrims.NumPrimitivesWritten,
                                    D3D11_USAGE_IMMUTABLE,
                                    D3D11_BIND_VERTEX_BUFFER,
                                    0,
                                    0,
                                    0};

    ID3D11Buffer *vsoutBuffer = NULL;

    // we need to map this data into memory for read anyway, might as well make this VB
    // immutable while we're at it.
    D3D11_SUBRESOURCE_DATA initialData;
    initialData.pSysMem = mapped.pData;
    initialData.SysMemPitch = bufferDesc.ByteWidth;
    initialData.SysMemSlicePitch = bufferDesc.ByteWidth;

    hr = m_pDevice->CreateBuffer(&bufferDesc, &initialData, &vsoutBuffer);

    if(FAILED(hr))
    {
      RDCERR("Failed to create postvs pos buffer HRESULT: %s", ToStr(hr).c_str());
      ret.vsout.status = "Failed to create vertex output cache on GPU";

      m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
      SAFE_RELEASE(idxBuf);
      return;
    }

    byte *byteData = (byte *)mapped.pData;

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(UINT64 i = 1; numPosComponents == 4 && i < numPrims.NumPrimitivesWritten; i++)
    {
      //////////////////////////////////////////////////////////////////////////////////
      // derive near/far, assuming a standard perspective matrix
      //
      // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
      // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
      // and we know Wpost = Zpre from the perspective matrix.
      // we can then see from the perspective matrix that
      // m = F/(F-N)
      // c = -(F*N)/(F-N)
      //
      // with re-arranging and substitution, we then get:
      // N = -c/m
      // F = c/(1-m)
      //
      // so if we can derive m and c then we can determine N and F. We can do this with
      // two points, and we pick them reasonably distinct on z to reduce floating-point
      // error

      Vec4f *pos = (Vec4f *)(byteData + i * stride);

      if(fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
      {
        Vec2f A(pos0->w, pos0->z);
        Vec2f B(pos->w, pos->z);

        float m = (B.y - A.y) / (B.x - A.x);
        float c = B.y - B.x * m;

        if(m == 1.0f || c == 0.0f)
          continue;

        if(-c / m <= 0.000001f)
          continue;

        nearp = -c / m;
        farp = c / (1 - m);

        found = true;

        break;
      }
    }

    // if we didn't find anything, all z's and w's were identical.
    // If the z is positive and w greater for the first element then
    // we detect this projection as reversed z with infinite far plane
    if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
    {
      nearp = pos0->z;
      farp = FLT_MAX;
    }

    m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);

    ret.vsin.topo = topo;
    ret.vsout.buf = vsoutBuffer;
    ret.vsout.vertStride = stride;
    ret.vsout.nearPlane = nearp;
    ret.vsout.farPlane = farp;

    ret.vsout.useIndices = bool(action->flags & ActionFlags::Indexed);
    ret.vsout.numVerts = action->numIndices;

    ret.vsout.instStride = 0;
    if(action->flags & ActionFlags::Instanced)
      ret.vsout.instStride = bufferDesc.ByteWidth / RDCMAX(1U, action->numInstances);

    ret.vsout.idxBuf = NULL;
    if(ret.vsout.useIndices && idxBuf)
    {
      ret.vsout.idxBuf = idxBuf;
      ret.vsout.idxFmt = idxFmt;
    }

    ret.vsout.hasPosOut = posidx >= 0;

    ret.vsout.topo = topo;
  }
  else
  {
    // empty vertex output signature
    ret.vsin.topo = topo;
    ret.vsout.buf = NULL;
    ret.vsout.instStride = 0;
    ret.vsout.vertStride = 0;
    ret.vsout.nearPlane = 0.0f;
    ret.vsout.farPlane = 0.0f;
    ret.vsout.useIndices = false;
    ret.vsout.hasPosOut = false;
    ret.vsout.idxBuf = NULL;

    ret.vsout.topo = topo;
  }

  if(lastShader)
  {
    ret.gsout.status.clear();

    const SOShaderData &soshader = m_pDevice->GetSOShaderData(lastShaderId);

    stride = 0;
    posidx = -1;
    numPosComponents = 0;

    sodecls.clear();
    for(size_t i = 0; i < lastShader->GetReflection()->OutputSig.size(); i++)
    {
      const SigParameter &sign = lastShader->GetReflection()->OutputSig[i];

      D3D11_SO_DECLARATION_ENTRY decl;

      // skip streams that aren't rasterized, or if none are rasterized skip non-zero
      if(soshader.rastStream == ~0U)
      {
        if(sign.stream != 0)
          continue;
      }
      else
      {
        if(sign.stream != soshader.rastStream)
          continue;
      }

      decl.Stream = 0;
      decl.OutputSlot = 0;

      decl.SemanticName = sign.semanticName.c_str();
      decl.SemanticIndex = sign.semanticIndex;
      decl.StartComponent = 0;
      decl.ComponentCount = sign.compCount & 0xff;

      if(sign.systemValue == ShaderBuiltin::Position)
      {
        posidx = (int)sodecls.size();
        numPosComponents = decl.ComponentCount = 4;
      }

      stride += decl.ComponentCount * sizeof(float);
      sodecls.push_back(decl);
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      D3D11_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(posidx);
      sodecls.insert(0, pos);
    }

    streamoutGS = NULL;

    HRESULT hr = m_pDevice->CreateGeometryShaderWithStreamOutput(
        (void *)lastShader->GetShaderBlob().data(), lastShader->GetShaderBlob().size(), &sodecls[0],
        (UINT)sodecls.size(), &stride, 1, D3D11_SO_NO_RASTERIZED_STREAM, NULL, &streamoutGS);

    if(FAILED(hr))
    {
      ret.gsout.status =
          StringFormat::Fmt("Failed to fetch output via streamout, HRESULT: %s", ToStr(hr).c_str());
      RDCERR("%s", ret.gsout.status.c_str());
      return;
    }

    m_pImmediateContext->GSSetShader(streamoutGS, NULL, 0);
    m_pImmediateContext->HSSetShader(hs, NULL, 0);
    m_pImmediateContext->DSSetShader(ds, NULL, 0);

    SAFE_RELEASE(streamoutGS);

    UINT offset = 0;

    D3D11_QUERY_DATA_SO_STATISTICS numPrims = {0};

    // do the whole draw, and if our output buffer isn't large enough then loop around.
    while(true)
    {
      m_pImmediateContext->Begin(m_SOStatsQueries[0]);

      m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);

      if(action->flags & ActionFlags::Instanced)
      {
        if(action->flags & ActionFlags::Indexed)
        {
          m_pImmediateContext->DrawIndexedInstanced(action->numIndices, action->numInstances,
                                                    action->indexOffset, action->baseVertex,
                                                    action->instanceOffset);
        }
        else
        {
          m_pImmediateContext->DrawInstanced(action->numIndices, action->numInstances,
                                             action->vertexOffset, action->instanceOffset);
        }
      }
      else
      {
        // trying to stream out a stream-out-auto based drawcall would be bad!
        // instead just draw the number of verts we pre-calculated
        if(action->flags & ActionFlags::Auto)
        {
          m_pImmediateContext->Draw(action->numIndices, 0);
        }
        else
        {
          if(action->flags & ActionFlags::Indexed)
          {
            m_pImmediateContext->DrawIndexed(action->numIndices, action->indexOffset,
                                             action->baseVertex);
          }
          else
          {
            m_pImmediateContext->Draw(action->numIndices, action->vertexOffset);
          }
        }
      }

      m_pImmediateContext->End(m_SOStatsQueries[0]);

      do
      {
        hr = m_pImmediateContext->GetData(m_SOStatsQueries[0], &numPrims,
                                          sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
      } while(hr == S_FALSE);

      uint64_t outputSize = stride * numPrims.PrimitivesStorageNeeded * 3;

      if(m_SOBufferSize < outputSize)
      {
        const uint64_t oldSize = m_SOBufferSize;
        const uint64_t newSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
        m_SOBufferSize = newSize;
        RDCWARN("Resizing stream-out buffer from %llu to %llu", oldSize, newSize);
        CreateSOBuffers();

        if(!m_SOStagingBuffer)
        {
          ret.gsout.status = StringFormat::Fmt(
              "Geometry/tessellation output generated %llu bytes of data which ran out of memory",
              newSize);
          return;
        }

        continue;
      }

      break;
    }

    // instanced draws must be replayed one at a time so we can record the number of primitives from
    // each action, as due to expansion this can vary per-instance.
    if(action->flags & ActionFlags::Instanced && action->numInstances > 1)
    {
      // ensure we have enough queries
      while(m_SOStatsQueries.size() < action->numInstances)
      {
        D3D11_QUERY_DESC qdesc;
        qdesc.MiscFlags = 0;
        qdesc.Query = D3D11_QUERY_SO_STATISTICS;

        ID3D11Query *q = NULL;
        hr = m_pDevice->CreateQuery(&qdesc, &q);
        if(FAILED(hr))
          RDCERR("Failed to create m_SOStatsQuery HRESULT: %s", ToStr(hr).c_str());

        m_SOStatsQueries.push_back(q);
      }

      // do incremental draws to get the output size. We have to do this O(N^2) style because
      // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N
      // instances and count the total number of verts each time, then we can see from the
      // difference how much each instance wrote.
      for(uint32_t inst = 1; inst <= action->numInstances; inst++)
      {
        if(action->flags & ActionFlags::Indexed)
        {
          m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);
          m_pImmediateContext->Begin(m_SOStatsQueries[inst - 1]);
          m_pImmediateContext->DrawIndexedInstanced(action->numIndices, inst, action->indexOffset,
                                                    action->baseVertex, action->instanceOffset);
          m_pImmediateContext->End(m_SOStatsQueries[inst - 1]);
        }
        else
        {
          m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);
          m_pImmediateContext->Begin(m_SOStatsQueries[inst - 1]);
          m_pImmediateContext->DrawInstanced(action->numIndices, inst, action->vertexOffset,
                                             action->instanceOffset);
          m_pImmediateContext->End(m_SOStatsQueries[inst - 1]);
        }

        if((inst % 2000) == 0)
          SerializeImmediateContext();
      }
    }

    m_pImmediateContext->GSSetShader(NULL, NULL, 0);
    m_pImmediateContext->SOSetTargets(0, NULL, NULL);

    m_pImmediateContext->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    rdcarray<D3D11PostVSData::InstData> instData;

    if((action->flags & ActionFlags::Instanced) && action->numInstances > 1)
    {
      uint64_t prevVertCount = 0;

      for(uint32_t inst = 0; inst < action->numInstances; inst++)
      {
        do
        {
          hr = m_pImmediateContext->GetData(m_SOStatsQueries[inst], &numPrims,
                                            sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
        } while(hr == S_FALSE);

        uint64_t vertCount = 3 * numPrims.NumPrimitivesWritten;

        D3D11PostVSData::InstData d;
        d.numVerts = uint32_t(vertCount - prevVertCount);
        d.bufOffset = uint32_t(stride * prevVertCount);
        prevVertCount = vertCount;

        instData.push_back(d);
      }
    }
    else
    {
      do
      {
        hr = m_pImmediateContext->GetData(m_SOStatsQueries[0], &numPrims,
                                          sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
      } while(hr == S_FALSE);
    }

    if(numPrims.NumPrimitivesWritten == 0)
    {
      ret.gsout.status = "No detectable output generated by geometry/tessellation shaders";
      return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_pImmediateContext->Map(m_SOStagingBuffer, 0, D3D11_MAP_READ, 0, &mapped);

    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer HRESULT: %s", ToStr(hr).c_str());
      ret.gsout.status = "Couldn't read back geometry/tessellation output data from GPU";
      return;
    }

    uint64_t bytesWritten = stride * numPrims.NumPrimitivesWritten * 3;

    if(bytesWritten > 0xFFFFFFFFULL)
    {
      RDCERR("More than 4GB of data generated, cannot create output buffer large enough.");
      ret.gsout.status =
          "More than 4GB of data generated by geometry/tessellation shaders, which caused an out "
          "of memory error.";
      return;
    }

    D3D11_BUFFER_DESC bufferDesc = {
        (uint32_t)bytesWritten, D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0,
    };

    if(bytesWritten > m_SOBufferSize)
    {
      RDCERR("Generated output data too large: %08x %08x", bufferDesc.ByteWidth, m_SOBufferSize);

      ret.gsout.status =
          "More data generated during readback than initial sizing, output is potentially "
          "non-deterministic";

      m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
      return;
    }

    ID3D11Buffer *gsoutBuffer = NULL;

    // we need to map this data into memory for read anyway, might as well make this VB
    // immutable while we're at it.
    D3D11_SUBRESOURCE_DATA initialData;
    initialData.pSysMem = mapped.pData;
    initialData.SysMemPitch = bufferDesc.ByteWidth;
    initialData.SysMemSlicePitch = bufferDesc.ByteWidth;

    hr = m_pDevice->CreateBuffer(&bufferDesc, &initialData, &gsoutBuffer);

    if(FAILED(hr))
    {
      RDCERR("Failed to create postvs pos buffer HRESULT: %s", ToStr(hr).c_str());
      ret.gsout.status = "Failed to create geometry/tessellation output cache on GPU";

      m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
      return;
    }

    byte *byteData = (byte *)mapped.pData;

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(UINT64 i = 1; numPosComponents == 4 && i < numPrims.NumPrimitivesWritten; i++)
    {
      //////////////////////////////////////////////////////////////////////////////////
      // derive near/far, assuming a standard perspective matrix
      //
      // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
      // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
      // and we know Wpost = Zpre from the perspective matrix.
      // we can then see from the perspective matrix that
      // m = F/(F-N)
      // c = -(F*N)/(F-N)
      //
      // with re-arranging and substitution, we then get:
      // N = -c/m
      // F = c/(1-m)
      //
      // so if we can derive m and c then we can determine N and F. We can do this with
      // two points, and we pick them reasonably distinct on z to reduce floating-point
      // error

      Vec4f *pos = (Vec4f *)(byteData + i * stride);

      if(fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
      {
        Vec2f A(pos0->w, pos0->z);
        Vec2f B(pos->w, pos->z);

        float m = (B.y - A.y) / (B.x - A.x);
        float c = B.y - B.x * m;

        if(m == 1.0f || c == 0.0f)
          continue;

        if(-c / m <= 0.000001f)
          continue;

        nearp = -c / m;
        farp = c / (1 - m);

        found = true;

        break;
      }
    }

    // if we didn't find anything, all z's and w's were identical.
    // If the z is positive and w greater for the first element then
    // we detect this projection as reversed z with infinite far plane
    if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
    {
      nearp = pos0->z;
      farp = FLT_MAX;
    }

    m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);

    ret.gsout.buf = gsoutBuffer;
    ret.gsout.instStride = 0;
    if(action->flags & ActionFlags::Instanced)
      ret.gsout.instStride = bufferDesc.ByteWidth / RDCMAX(1U, action->numInstances);
    ret.gsout.vertStride = stride;
    ret.gsout.nearPlane = nearp;
    ret.gsout.farPlane = farp;
    ret.gsout.useIndices = false;
    ret.gsout.hasPosOut = posidx >= 0;
    ret.gsout.idxBuf = NULL;

    topo = lastShader->GetOutputTopology();

    ret.gsout.topo = topo;

    // streamout expands strips unfortunately
    if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
      ret.gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP)
      ret.gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
      ret.gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
      ret.gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;

    switch(ret.gsout.topo)
    {
      case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
        ret.gsout.numVerts = (uint32_t)numPrims.NumPrimitivesWritten;
        break;
      case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
      case D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
        ret.gsout.numVerts = (uint32_t)numPrims.NumPrimitivesWritten * 2;
        break;
      default:
      case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
      case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
        ret.gsout.numVerts = (uint32_t)numPrims.NumPrimitivesWritten * 3;
        break;
    }

    if(action->flags & ActionFlags::Instanced)
      ret.gsout.numVerts /= RDCMAX(1U, action->numInstances);

    ret.gsout.instData = instData;
  }
}

void D3D11Replay::InitPostVSBuffers(const rdcarray<uint32_t> &passEvents)
{
  uint32_t prev = 0;

  // since we can always replay between drawcalls, just loop through all the events
  // doing partial replays and calling InitPostVSBuffers for each
  for(size_t i = 0; i < passEvents.size(); i++)
  {
    if(prev != passEvents[i])
    {
      m_pDevice->ReplayLog(prev, passEvents[i], eReplay_WithoutDraw);

      prev = passEvents[i];
    }

    const ActionDescription *d = m_pDevice->GetAction(passEvents[i]);

    if(d)
      InitPostVSBuffers(passEvents[i]);
  }
}
