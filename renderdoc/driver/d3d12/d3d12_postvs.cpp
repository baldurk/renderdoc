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

#include "driver/dxgi/dxgi_common.h"
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_shader_cache.h"

void D3D12Replay::CreateSOBuffers()
{
  HRESULT hr = S_OK;

  SAFE_RELEASE(m_SOBuffer);
  SAFE_RELEASE(m_SOStagingBuffer);
  SAFE_RELEASE(m_SOPatchedIndexBuffer);
  SAFE_RELEASE(m_SOQueryHeap);

  D3D12_RESOURCE_DESC soBufDesc;
  soBufDesc.Alignment = 0;
  soBufDesc.DepthOrArraySize = 1;
  soBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  // need to allow UAV access to reset the counter each time
  soBufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  soBufDesc.Format = DXGI_FORMAT_UNKNOWN;
  soBufDesc.Height = 1;
  soBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  soBufDesc.MipLevels = 1;
  soBufDesc.SampleDesc.Count = 1;
  soBufDesc.SampleDesc.Quality = 0;
  // add 64 bytes for the counter at the start
  soBufDesc.Width = m_SOBufferSize + 64;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc,
                                          D3D12_RESOURCE_STATE_STREAM_OUT, NULL,
                                          __uuidof(ID3D12Resource), (void **)&m_SOBuffer);

  m_SOBuffer->SetName(L"m_SOBuffer");

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO output buffer, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  soBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  heapProps.Type = D3D12_HEAP_TYPE_READBACK;

  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc,
                                          D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                          __uuidof(ID3D12Resource), (void **)&m_SOStagingBuffer);

  m_SOStagingBuffer->SetName(L"m_SOStagingBuffer");

  if(FAILED(hr))
  {
    RDCERR("Failed to create readback buffer, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  // this is a buffer of unique indices, so it allows for
  // the worst case - float4 per vertex, all unique indices.
  soBufDesc.Width = m_SOBufferSize / sizeof(Vec4f);
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  hr = m_pDevice->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
      __uuidof(ID3D12Resource), (void **)&m_SOPatchedIndexBuffer);

  m_SOPatchedIndexBuffer->SetName(L"m_SOPatchedIndexBuffer");

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO index buffer, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  D3D12_QUERY_HEAP_DESC queryDesc;
  queryDesc.Count = 16;
  queryDesc.NodeMask = 1;
  queryDesc.Type = D3D12_QUERY_HEAP_TYPE_SO_STATISTICS;
  hr = m_pDevice->CreateQueryHeap(&queryDesc, __uuidof(m_SOQueryHeap), (void **)&m_SOQueryHeap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO query heap, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  D3D12_UNORDERED_ACCESS_VIEW_DESC counterDesc = {};
  counterDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  counterDesc.Format = DXGI_FORMAT_R32_UINT;
  counterDesc.Buffer.FirstElement = 0;
  counterDesc.Buffer.NumElements = UINT(m_SOBufferSize / sizeof(UINT));

  m_pDevice->CreateUnorderedAccessView(m_SOBuffer, NULL, &counterDesc,
                                       GetDebugManager()->GetCPUHandle(STREAM_OUT_UAV));

  m_pDevice->CreateUnorderedAccessView(m_SOBuffer, NULL, &counterDesc,
                                       GetDebugManager()->GetUAVClearHandle(STREAM_OUT_UAV));
}

void D3D12Replay::ClearPostVSCache()
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

void D3D12Replay::InitPostVSBuffers(uint32_t eventId)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventId) != m_PostVSAlias.end())
    eventId = m_PostVSAlias[eventId];

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    return;

  D3D12MarkerRegion postvs(m_pDevice->GetQueue(), StringFormat::Fmt("PostVS for %u", eventId));

  D3D12CommandData *cmd = m_pDevice->GetQueue()->GetCommandData();
  const D3D12RenderState &rs = cmd->m_RenderState;

  if(rs.pipe == ResourceId())
    return;

  WrappedID3D12PipelineState *origPSO =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(!origPSO->IsGraphics())
    return;

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
  origPSO->Fill(psoDesc);

  if(psoDesc.VS.BytecodeLength == 0)
    return;

  WrappedID3D12Shader *vs = origPSO->VS();

  D3D_PRIMITIVE_TOPOLOGY topo = rs.topo;

  const DrawcallDescription *drawcall = m_pDevice->GetDrawcall(eventId);

  if(drawcall->numIndices == 0 || drawcall->numInstances == 0)
    return;

  DXBC::DXBCFile *dxbcVS = vs->GetDXBC();

  RDCASSERT(dxbcVS);

  DXBC::DXBCFile *dxbcGS = NULL;

  WrappedID3D12Shader *gs = origPSO->GS();

  if(gs)
  {
    dxbcGS = gs->GetDXBC();

    RDCASSERT(dxbcGS);
  }

  DXBC::DXBCFile *dxbcDS = NULL;

  WrappedID3D12Shader *ds = origPSO->DS();

  if(ds)
  {
    dxbcDS = ds->GetDXBC();

    RDCASSERT(dxbcDS);
  }

  ID3D12RootSignature *soSig = NULL;

  HRESULT hr = S_OK;

  {
    WrappedID3D12RootSignature *sig =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rs.graphics.rootsig);

    D3D12RootSignature rootsig = sig->sig;

    // create a root signature that allows stream out, if necessary
    if((rootsig.Flags & D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT) == 0)
    {
      rootsig.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;

      ID3DBlob *blob = m_pDevice->GetShaderCache()->MakeRootSig(rootsig);

      hr = m_pDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                          __uuidof(ID3D12RootSignature), (void **)&soSig);
      if(FAILED(hr))
      {
        RDCERR("Couldn't enable stream-out in root signature: HRESULT: %s", ToStr(hr).c_str());
        return;
      }

      SAFE_RELEASE(blob);
    }
  }

  std::vector<D3D12_SO_DECLARATION_ENTRY> sodecls;

  UINT stride = 0;
  int posidx = -1;
  int numPosComponents = 0;

  if(!dxbcVS->m_OutputSig.empty())
  {
    for(const SigParameter &sign : dxbcVS->m_OutputSig)
    {
      D3D12_SO_DECLARATION_ENTRY decl;

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

    if(stride == 0)
    {
      RDCERR("Didn't get valid stride! Setting to 4 bytes");
      stride = 4;
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      D3D12_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(sodecls.begin() + posidx);
      sodecls.insert(sodecls.begin(), pos);
    }

    // set up stream output entries and buffers
    psoDesc.StreamOutput.NumEntries = (UINT)sodecls.size();
    psoDesc.StreamOutput.pSODeclaration = &sodecls[0];
    psoDesc.StreamOutput.NumStrides = 1;
    psoDesc.StreamOutput.pBufferStrides = &stride;
    psoDesc.StreamOutput.RasterizedStream = D3D12_SO_NO_RASTERIZED_STREAM;

    // disable all other shader stages
    psoDesc.HS.BytecodeLength = 0;
    psoDesc.HS.pShaderBytecode = NULL;
    psoDesc.DS.BytecodeLength = 0;
    psoDesc.DS.pShaderBytecode = NULL;
    psoDesc.GS.BytecodeLength = 0;
    psoDesc.GS.pShaderBytecode = NULL;
    psoDesc.PS.BytecodeLength = 0;
    psoDesc.PS.pShaderBytecode = NULL;

    // disable any rasterization/use of output targets
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    if(soSig)
      psoDesc.pRootSignature = soSig;

    // render as points
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

    // disable MSAA
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    // disable outputs
    RDCEraseEl(psoDesc.RTVFormats);
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    ID3D12PipelineState *pipe = NULL;
    hr = m_pDevice->CreatePipeState(psoDesc, &pipe);
    if(FAILED(hr))
    {
      RDCERR("Couldn't create patched graphics pipeline: HRESULT: %s", ToStr(hr).c_str());
      SAFE_RELEASE(soSig);
      return;
    }

    ID3D12Resource *idxBuf = NULL;

    bool recreate = false;
    // we add 64 to account for the stream-out data counter
    uint64_t outputSize = uint64_t(drawcall->numIndices) * drawcall->numInstances * stride + 64;

    if(m_SOBufferSize < outputSize)
    {
      uint64_t oldSize = m_SOBufferSize;
      m_SOBufferSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
      RDCWARN("Resizing stream-out buffer from %llu to %llu for output data", oldSize,
              m_SOBufferSize);
      recreate = true;
    }

    ID3D12GraphicsCommandList4 *list = NULL;

    if(!(drawcall->flags & DrawFlags::Indexed))
    {
      if(recreate)
      {
        m_pDevice->GPUSync();

        CreateSOBuffers();
      }

      list = GetDebugManager()->ResetDebugList();

      rs.ApplyState(m_pDevice, list);

      list->SetPipelineState(pipe);

      if(soSig)
      {
        list->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(list);
      }

      D3D12_STREAM_OUTPUT_BUFFER_VIEW view;
      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize - 64;
      list->SOSetTargets(0, 1, &view);

      list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
      list->DrawInstanced(drawcall->numIndices, drawcall->numInstances, drawcall->vertexOffset,
                          drawcall->instanceOffset);
    }
    else    // drawcall is indexed
    {
      bytebuf idxdata;
      if(rs.ibuffer.buf != ResourceId())
        GetBufferData(rs.ibuffer.buf, rs.ibuffer.offs + drawcall->indexOffset * rs.ibuffer.bytewidth,
                      RDCMIN(drawcall->numIndices * rs.ibuffer.bytewidth, rs.ibuffer.size), idxdata);

      std::vector<uint32_t> indices;

      uint16_t *idx16 = (uint16_t *)&idxdata[0];
      uint32_t *idx32 = (uint32_t *)&idxdata[0];

      // only read as many indices as were available in the buffer
      uint32_t numIndices =
          RDCMIN(uint32_t(idxdata.size() / RDCMAX(1, rs.ibuffer.bytewidth)), drawcall->numIndices);

      uint32_t idxclamp = 0;
      if(drawcall->baseVertex < 0)
        idxclamp = uint32_t(-drawcall->baseVertex);

      // grab all unique vertex indices referenced
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = rs.ibuffer.bytewidth == 2 ? uint32_t(idx16[i]) : idx32[i];

        // apply baseVertex but clamp to 0 (don't allow index to become negative)
        if(i32 < idxclamp)
          i32 = 0;
        else if(drawcall->baseVertex < 0)
          i32 -= idxclamp;
        else if(drawcall->baseVertex > 0)
          i32 += drawcall->baseVertex;

        auto it = std::lower_bound(indices.begin(), indices.end(), i32);

        if(it != indices.end() && *it == i32)
          continue;

        indices.insert(it, i32);
      }

      // if we read out of bounds, we'll also have a 0 index being referenced
      // (as 0 is read). Don't insert 0 if we already have 0 though
      if(numIndices < drawcall->numIndices && (indices.empty() || indices[0] != 0))
        indices.insert(indices.begin(), 0);

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

      outputSize = uint64_t(indices.size() * sizeof(uint32_t) * sizeof(Vec4f));

      if(m_SOBufferSize < outputSize)
      {
        uint64_t oldSize = m_SOBufferSize;
        m_SOBufferSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
        RDCWARN("Resizing stream-out buffer from %llu to %llu for indices", oldSize, m_SOBufferSize);
        recreate = true;
      }

      if(recreate)
      {
        m_pDevice->GPUSync();

        CreateSOBuffers();
      }

      GetDebugManager()->FillBuffer(m_SOPatchedIndexBuffer, 0, &indices[0],
                                    indices.size() * sizeof(uint32_t));

      D3D12_INDEX_BUFFER_VIEW patchedIB;

      patchedIB.BufferLocation = m_SOPatchedIndexBuffer->GetGPUVirtualAddress();
      patchedIB.Format = DXGI_FORMAT_R32_UINT;
      patchedIB.SizeInBytes = UINT(indices.size() * sizeof(uint32_t));

      list = GetDebugManager()->ResetDebugList();

      rs.ApplyState(m_pDevice, list);

      list->SetPipelineState(pipe);

      list->IASetIndexBuffer(&patchedIB);

      if(soSig)
      {
        list->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(list);
      }

      D3D12_STREAM_OUTPUT_BUFFER_VIEW view;
      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize - 64;
      list->SOSetTargets(0, 1, &view);

      list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

      list->DrawIndexedInstanced((UINT)indices.size(), drawcall->numInstances, 0, 0,
                                 drawcall->instanceOffset);

      uint32_t stripCutValue = 0;
      if(psoDesc.IBStripCutValue == D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF)
        stripCutValue = 0xffff;
      else if(psoDesc.IBStripCutValue == D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF)
        stripCutValue = 0xffffffff;

      // rebase existing index buffer to point to the right elements in our stream-out'd
      // vertex buffer
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = rs.ibuffer.bytewidth == 2 ? uint32_t(idx16[i]) : idx32[i];

        // preserve primitive restart indices
        if(stripCutValue && i32 == stripCutValue)
          continue;

        // apply baseVertex but clamp to 0 (don't allow index to become negative)
        if(i32 < idxclamp)
          i32 = 0;
        else if(drawcall->baseVertex < 0)
          i32 -= idxclamp;
        else if(drawcall->baseVertex > 0)
          i32 += drawcall->baseVertex;

        if(rs.ibuffer.bytewidth == 2)
          idx16[i] = uint16_t(indexRemap[i32]);
        else
          idx32[i] = uint32_t(indexRemap[i32]);
      }

      idxBuf = NULL;

      if(!idxdata.empty())
      {
        D3D12_RESOURCE_DESC idxBufDesc;
        idxBufDesc.Alignment = 0;
        idxBufDesc.DepthOrArraySize = 1;
        idxBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        idxBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        idxBufDesc.Format = DXGI_FORMAT_UNKNOWN;
        idxBufDesc.Height = 1;
        idxBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        idxBufDesc.MipLevels = 1;
        idxBufDesc.SampleDesc.Count = 1;
        idxBufDesc.SampleDesc.Quality = 0;
        idxBufDesc.Width = idxdata.size();

        D3D12_HEAP_PROPERTIES heapProps;
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &idxBufDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                __uuidof(ID3D12Resource), (void **)&idxBuf);
        RDCASSERTEQUAL(hr, S_OK);

        SetObjName(idxBuf, StringFormat::Fmt("PostVS idxBuf for %u", eventId));

        GetDebugManager()->FillBuffer(idxBuf, 0, &idxdata[0], idxdata.size());
      }
    }

    D3D12_RESOURCE_BARRIER sobarr = {};
    sobarr.Transition.pResource = m_SOBuffer;
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_STREAM_OUT;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    list->ResourceBarrier(1, &sobarr);

    list->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    // we're done with this after the copy, so we can discard it and reset
    // the counter for the next stream-out
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    list->DiscardResource(m_SOBuffer, NULL);
    list->ResourceBarrier(1, &sobarr);

    UINT zeroes[4] = {0, 0, 0, 0};
    list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(STREAM_OUT_UAV),
                                       GetDebugManager()->GetUAVClearHandle(STREAM_OUT_UAV),
                                       m_SOBuffer, zeroes, 0, NULL);

    list->Close();

    ID3D12CommandList *l = list;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();

    GetDebugManager()->ResetDebugAlloc();

    SAFE_RELEASE(pipe);

    byte *byteData = NULL;
    D3D12_RANGE range = {0, (SIZE_T)m_SOBufferSize};
    hr = m_SOStagingBuffer->Map(0, &range, (void **)&byteData);
    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer HRESULT: %s", ToStr(hr).c_str());
      SAFE_RELEASE(idxBuf);
      SAFE_RELEASE(soSig);
      return;
    }

    range.End = 0;

    uint64_t numBytesWritten = *(uint64_t *)byteData;

    if(numBytesWritten == 0)
    {
      m_PostVSData[eventId] = D3D12PostVSData();
      SAFE_RELEASE(idxBuf);
      SAFE_RELEASE(soSig);
      return;
    }

    // skip past the counter
    byteData += 64;

    uint64_t numPrims = numBytesWritten / stride;

    ID3D12Resource *vsoutBuffer = NULL;

    {
      D3D12_RESOURCE_DESC vertBufDesc;
      vertBufDesc.Alignment = 0;
      vertBufDesc.DepthOrArraySize = 1;
      vertBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      vertBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
      vertBufDesc.Format = DXGI_FORMAT_UNKNOWN;
      vertBufDesc.Height = 1;
      vertBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      vertBufDesc.MipLevels = 1;
      vertBufDesc.SampleDesc.Count = 1;
      vertBufDesc.SampleDesc.Quality = 0;
      vertBufDesc.Width = numBytesWritten;

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertBufDesc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                              __uuidof(ID3D12Resource), (void **)&vsoutBuffer);
      RDCASSERTEQUAL(hr, S_OK);

      if(vsoutBuffer)
      {
        SetObjName(vsoutBuffer, StringFormat::Fmt("PostVS vsoutBuffer for %u", eventId));
        GetDebugManager()->FillBuffer(vsoutBuffer, 0, byteData, (size_t)numBytesWritten);
      }
    }

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(uint64_t i = 1; numPosComponents == 4 && i < numPrims; i++)
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

        if(m == 1.0f)
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

    m_SOStagingBuffer->Unmap(0, &range);

    m_PostVSData[eventId].vsin.topo = topo;
    m_PostVSData[eventId].vsout.buf = vsoutBuffer;
    m_PostVSData[eventId].vsout.vertStride = stride;
    m_PostVSData[eventId].vsout.nearPlane = nearp;
    m_PostVSData[eventId].vsout.farPlane = farp;

    m_PostVSData[eventId].vsout.useIndices = bool(drawcall->flags & DrawFlags::Indexed);
    m_PostVSData[eventId].vsout.numVerts = drawcall->numIndices;

    m_PostVSData[eventId].vsout.instStride = 0;
    if(drawcall->flags & DrawFlags::Instanced)
      m_PostVSData[eventId].vsout.instStride =
          uint32_t(numBytesWritten / RDCMAX(1U, drawcall->numInstances));

    m_PostVSData[eventId].vsout.idxBuf = NULL;
    if(m_PostVSData[eventId].vsout.useIndices && idxBuf)
    {
      m_PostVSData[eventId].vsout.idxBuf = idxBuf;
      m_PostVSData[eventId].vsout.idxFmt =
          rs.ibuffer.bytewidth == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    }

    m_PostVSData[eventId].vsout.hasPosOut = posidx >= 0;

    m_PostVSData[eventId].vsout.topo = topo;
  }
  else
  {
    // empty vertex output signature
    m_PostVSData[eventId].vsin.topo = topo;
    m_PostVSData[eventId].vsout.buf = NULL;
    m_PostVSData[eventId].vsout.instStride = 0;
    m_PostVSData[eventId].vsout.vertStride = 0;
    m_PostVSData[eventId].vsout.nearPlane = 0.0f;
    m_PostVSData[eventId].vsout.farPlane = 0.0f;
    m_PostVSData[eventId].vsout.useIndices = false;
    m_PostVSData[eventId].vsout.hasPosOut = false;
    m_PostVSData[eventId].vsout.idxBuf = NULL;

    m_PostVSData[eventId].vsout.topo = topo;
  }

  if(dxbcGS || dxbcDS)
  {
    stride = 0;
    posidx = -1;
    numPosComponents = 0;

    DXBC::DXBCFile *lastShader = dxbcGS;
    if(dxbcDS)
      lastShader = dxbcDS;

    sodecls.clear();
    for(const SigParameter &sign : lastShader->m_OutputSig)
    {
      D3D12_SO_DECLARATION_ENTRY decl;

      // for now, skip streams that aren't stream 0
      if(sign.stream != 0)
        continue;

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
      D3D12_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(sodecls.begin() + posidx);
      sodecls.insert(sodecls.begin(), pos);
    }

    // enable the other shader stages again
    if(origPSO->DS())
      psoDesc.DS = origPSO->DS()->GetDesc();
    if(origPSO->HS())
      psoDesc.HS = origPSO->HS()->GetDesc();
    if(origPSO->GS())
      psoDesc.GS = origPSO->GS()->GetDesc();

    // configure new SO declarations
    psoDesc.StreamOutput.NumEntries = (UINT)sodecls.size();
    psoDesc.StreamOutput.pSODeclaration = &sodecls[0];
    psoDesc.StreamOutput.NumStrides = 1;
    psoDesc.StreamOutput.pBufferStrides = &stride;

    // we're using the same topology this time
    psoDesc.PrimitiveTopologyType = origPSO->graphics->PrimitiveTopologyType;

    ID3D12PipelineState *pipe = NULL;
    hr = m_pDevice->CreatePipeState(psoDesc, &pipe);
    if(FAILED(hr))
    {
      RDCERR("Couldn't create patched graphics pipeline: HRESULT: %s", ToStr(hr).c_str());
      SAFE_RELEASE(soSig);
      return;
    }

    D3D12_STREAM_OUTPUT_BUFFER_VIEW view;

    ID3D12GraphicsCommandList4 *list = NULL;

    view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
    view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
    view.SizeInBytes = m_SOBufferSize - 64;
    // draws with multiple instances must be replayed one at a time so we can record the number of
    // primitives from each drawcall, as due to expansion this can vary per-instance.
    if(drawcall->numInstances > 1)
    {
      list = GetDebugManager()->ResetDebugList();

      rs.ApplyState(m_pDevice, list);

      list->SetPipelineState(pipe);

      if(soSig)
      {
        list->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(list);
      }

      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize - 64;

      // do a dummy draw to make sure we have enough space in the output buffer
      list->SOSetTargets(0, 1, &view);

      list->BeginQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

      // because the result is expanded we don't have to remap index buffers or anything
      if(drawcall->flags & DrawFlags::Indexed)
      {
        list->DrawIndexedInstanced(drawcall->numIndices, drawcall->numInstances,
                                   drawcall->indexOffset, drawcall->baseVertex,
                                   drawcall->instanceOffset);
      }
      else
      {
        list->DrawInstanced(drawcall->numIndices, drawcall->numInstances, drawcall->vertexOffset,
                            drawcall->instanceOffset);
      }

      list->EndQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

      list->ResolveQueryData(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0, 1,
                             m_SOStagingBuffer, 0);

      list->Close();

      ID3D12CommandList *l = list;
      m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
      m_pDevice->GPUSync();

      // check that things are OK, and resize up if needed
      D3D12_RANGE range;
      range.Begin = 0;
      range.End = (SIZE_T)sizeof(D3D12_QUERY_DATA_SO_STATISTICS);

      D3D12_QUERY_DATA_SO_STATISTICS *data;
      hr = m_SOStagingBuffer->Map(0, &range, (void **)&data);

      D3D12_QUERY_DATA_SO_STATISTICS result = *data;

      range.End = 0;
      m_SOStagingBuffer->Unmap(0, &range);

      uint64_t outputSize = data->PrimitivesStorageNeeded * 3 * stride;

      if(m_SOBufferSize < outputSize)
      {
        uint64_t oldSize = m_SOBufferSize;
        m_SOBufferSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
        RDCWARN("Resizing stream-out buffer from %llu to %llu for output", oldSize, m_SOBufferSize);
        CreateSOBuffers();
      }

      GetDebugManager()->ResetDebugAlloc();

      // now do the actual stream out
      list = GetDebugManager()->ResetDebugList();

      // first need to reset the counter byte values which may have either been written to above, or
      // are newly created
      {
        D3D12_RESOURCE_BARRIER sobarr = {};
        sobarr.Transition.pResource = m_SOBuffer;
        sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_STREAM_OUT;
        sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        list->ResourceBarrier(1, &sobarr);

        UINT zeroes[4] = {0, 0, 0, 0};
        list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(STREAM_OUT_UAV),
                                           GetDebugManager()->GetUAVClearHandle(STREAM_OUT_UAV),
                                           m_SOBuffer, zeroes, 0, NULL);

        std::swap(sobarr.Transition.StateBefore, sobarr.Transition.StateAfter);
        list->ResourceBarrier(1, &sobarr);
      }

      rs.ApplyState(m_pDevice, list);

      list->SetPipelineState(pipe);

      if(soSig)
      {
        list->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(list);
      }

      // reserve space for enough 'buffer filled size' locations
      UINT64 SizeCounterBytes = AlignUp(uint64_t(drawcall->numInstances * sizeof(UINT64)), 64ULL);
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + SizeCounterBytes;
      view.SizeInBytes = m_SOBufferSize - SizeCounterBytes;

      // do incremental draws to get the output size. We have to do this O(N^2) style because
      // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N instances
      // and count the total number of verts each time, then we can see from the difference how much
      // each instance wrote.
      for(uint32_t inst = 1; inst <= drawcall->numInstances; inst++)
      {
        if(drawcall->flags & DrawFlags::Indexed)
        {
          view.BufferFilledSizeLocation =
              m_SOBuffer->GetGPUVirtualAddress() + (inst - 1) * sizeof(UINT64);
          list->SOSetTargets(0, 1, &view);
          list->DrawIndexedInstanced(drawcall->numIndices, inst, drawcall->indexOffset,
                                     drawcall->baseVertex, drawcall->instanceOffset);
        }
        else
        {
          view.BufferFilledSizeLocation =
              m_SOBuffer->GetGPUVirtualAddress() + (inst - 1) * sizeof(UINT64);
          list->SOSetTargets(0, 1, &view);
          list->DrawInstanced(drawcall->numIndices, inst, drawcall->vertexOffset,
                              drawcall->instanceOffset);
        }
      }

      list->Close();

      l = list;
      m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
      m_pDevice->GPUSync();

      GetDebugManager()->ResetDebugAlloc();

      // the last draw will have written the actual data we want into the buffer
    }
    else
    {
      // this only loops if we find from a query that we need to resize up
      while(true)
      {
        list = GetDebugManager()->ResetDebugList();

        rs.ApplyState(m_pDevice, list);

        list->SetPipelineState(pipe);

        if(soSig)
        {
          list->SetGraphicsRootSignature(soSig);
          rs.ApplyGraphicsRootElements(list);
        }

        view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
        view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
        view.SizeInBytes = m_SOBufferSize - 64;

        list->SOSetTargets(0, 1, &view);

        list->BeginQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

        // because the result is expanded we don't have to remap index buffers or anything
        if(drawcall->flags & DrawFlags::Indexed)
        {
          list->DrawIndexedInstanced(drawcall->numIndices, drawcall->numInstances,
                                     drawcall->indexOffset, drawcall->baseVertex,
                                     drawcall->instanceOffset);
        }
        else
        {
          list->DrawInstanced(drawcall->numIndices, drawcall->numInstances, drawcall->vertexOffset,
                              drawcall->instanceOffset);
        }

        list->EndQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

        list->ResolveQueryData(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0, 1,
                               m_SOStagingBuffer, 0);

        list->Close();

        ID3D12CommandList *l = list;
        m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
        m_pDevice->GPUSync();

        // check that things are OK, and resize up if needed
        D3D12_RANGE range;
        range.Begin = 0;
        range.End = (SIZE_T)sizeof(D3D12_QUERY_DATA_SO_STATISTICS);

        D3D12_QUERY_DATA_SO_STATISTICS *data;
        hr = m_SOStagingBuffer->Map(0, &range, (void **)&data);

        uint64_t outputSize = data->PrimitivesStorageNeeded * 3 * stride;

        if(m_SOBufferSize < outputSize)
        {
          uint64_t oldSize = m_SOBufferSize;
          m_SOBufferSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
          RDCWARN("Resizing stream-out buffer from %llu to %llu for output", oldSize, m_SOBufferSize);
          CreateSOBuffers();

          continue;
        }

        range.End = 0;
        m_SOStagingBuffer->Unmap(0, &range);

        GetDebugManager()->ResetDebugAlloc();

        break;
      }
    }

    list = GetDebugManager()->ResetDebugList();

    D3D12_RESOURCE_BARRIER sobarr = {};
    sobarr.Transition.pResource = m_SOBuffer;
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_STREAM_OUT;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    list->ResourceBarrier(1, &sobarr);

    list->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    // we're done with this after the copy, so we can discard it and reset
    // the counter for the next stream-out
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    list->DiscardResource(m_SOBuffer, NULL);
    list->ResourceBarrier(1, &sobarr);

    UINT zeroes[4] = {0, 0, 0, 0};
    list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(STREAM_OUT_UAV),
                                       GetDebugManager()->GetUAVClearHandle(STREAM_OUT_UAV),
                                       m_SOBuffer, zeroes, 0, NULL);

    list->Close();

    ID3D12CommandList *l = list;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();

    GetDebugManager()->ResetDebugAlloc();

    SAFE_RELEASE(pipe);

    byte *byteData = NULL;
    D3D12_RANGE range = {0, (SIZE_T)m_SOBufferSize};
    hr = m_SOStagingBuffer->Map(0, &range, (void **)&byteData);
    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer HRESULT: %s", ToStr(hr).c_str());
      SAFE_RELEASE(soSig);
      return;
    }

    range.End = 0;

    uint64_t *counters = (uint64_t *)byteData;

    uint64_t numBytesWritten = 0;
    std::vector<D3D12PostVSData::InstData> instData;
    if(drawcall->numInstances > 1)
    {
      uint64_t prevByteCount = 0;

      for(uint32_t inst = 0; inst < drawcall->numInstances; inst++)
      {
        uint64_t byteCount = counters[inst];

        D3D12PostVSData::InstData d;
        d.numVerts = uint32_t((byteCount - prevByteCount) / stride);
        d.bufOffset = prevByteCount;
        prevByteCount = byteCount;

        instData.push_back(d);
      }

      numBytesWritten = prevByteCount;
    }
    else
    {
      numBytesWritten = counters[0];
    }

    if(numBytesWritten == 0)
    {
      SAFE_RELEASE(soSig);
      return;
    }

    // skip past the counter(s)
    byteData += (view.BufferLocation - m_SOBuffer->GetGPUVirtualAddress());

    uint64_t numVerts = numBytesWritten / stride;

    ID3D12Resource *gsoutBuffer = NULL;

    {
      D3D12_RESOURCE_DESC vertBufDesc;
      vertBufDesc.Alignment = 0;
      vertBufDesc.DepthOrArraySize = 1;
      vertBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      vertBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
      vertBufDesc.Format = DXGI_FORMAT_UNKNOWN;
      vertBufDesc.Height = 1;
      vertBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      vertBufDesc.MipLevels = 1;
      vertBufDesc.SampleDesc.Count = 1;
      vertBufDesc.SampleDesc.Quality = 0;
      vertBufDesc.Width = numBytesWritten;

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertBufDesc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                              __uuidof(ID3D12Resource), (void **)&gsoutBuffer);
      RDCASSERTEQUAL(hr, S_OK);

      if(gsoutBuffer)
      {
        SetObjName(gsoutBuffer, StringFormat::Fmt("PostVS gsoutBuffer for %u", eventId));
        GetDebugManager()->FillBuffer(gsoutBuffer, 0, byteData, (size_t)numBytesWritten);
      }
    }

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(UINT64 i = 1; numPosComponents == 4 && i < numVerts; i++)
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

        if(m == 1.0f)
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

    m_SOStagingBuffer->Unmap(0, &range);

    m_PostVSData[eventId].gsout.buf = gsoutBuffer;
    m_PostVSData[eventId].gsout.instStride = 0;
    if(drawcall->flags & DrawFlags::Instanced)
      m_PostVSData[eventId].gsout.instStride =
          uint32_t(numBytesWritten / RDCMAX(1U, drawcall->numInstances));
    m_PostVSData[eventId].gsout.vertStride = stride;
    m_PostVSData[eventId].gsout.nearPlane = nearp;
    m_PostVSData[eventId].gsout.farPlane = farp;
    m_PostVSData[eventId].gsout.useIndices = false;
    m_PostVSData[eventId].gsout.hasPosOut = posidx >= 0;
    m_PostVSData[eventId].gsout.idxBuf = NULL;

    topo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    if(lastShader == dxbcGS)
    {
      for(size_t i = 0; i < dxbcGS->GetNumDeclarations(); i++)
      {
        const DXBC::ASMDecl &decl = dxbcGS->GetDeclaration(i);

        if(decl.declaration == DXBC::OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
        {
          topo = decl.outTopology;
          break;
        }
      }
    }
    else if(lastShader == dxbcDS)
    {
      for(size_t i = 0; i < dxbcDS->GetNumDeclarations(); i++)
      {
        const DXBC::ASMDecl &decl = dxbcDS->GetDeclaration(i);

        if(decl.declaration == DXBC::OPCODE_DCL_TESS_DOMAIN)
        {
          if(decl.domain == DXBC::DOMAIN_ISOLINE)
            topo = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
          else
            topo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
          break;
        }
      }
    }

    m_PostVSData[eventId].gsout.topo = topo;

    // streamout expands strips unfortunately
    if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
      m_PostVSData[eventId].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP)
      m_PostVSData[eventId].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
      m_PostVSData[eventId].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
      m_PostVSData[eventId].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;

    m_PostVSData[eventId].gsout.numVerts = (uint32_t)numVerts;

    if(drawcall->flags & DrawFlags::Instanced)
      m_PostVSData[eventId].gsout.numVerts /= RDCMAX(1U, drawcall->numInstances);

    m_PostVSData[eventId].gsout.instData = instData;
  }

  SAFE_RELEASE(soSig);
}

struct D3D12InitPostVSCallback : public D3D12DrawcallCallback
{
  D3D12InitPostVSCallback(WrappedID3D12Device *dev, D3D12Replay *replay,
                          const std::vector<uint32_t> &events)
      : m_pDevice(dev), m_Replay(replay), m_Events(events)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = this;
  }
  ~D3D12InitPostVSCallback() { m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = NULL; }
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandList4 *cmd) override
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) != m_Events.end())
      m_Replay->InitPostVSBuffers(eid);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandList4 *cmd) override { return false; }
  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandList4 *cmd) override {}
  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandList4 *cmd) override {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandList4 *cmd) override { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandList4 *cmd) override {}
  void PreCloseCommandList(ID3D12GraphicsCommandList4 *cmd) override {}
  void AliasEvent(uint32_t primary, uint32_t alias) override
  {
    if(std::find(m_Events.begin(), m_Events.end(), primary) != m_Events.end())
      m_Replay->AliasPostVSBuffers(primary, alias);
  }

  WrappedID3D12Device *m_pDevice;
  D3D12Replay *m_Replay;
  const std::vector<uint32_t> &m_Events;
};

void D3D12Replay::InitPostVSBuffers(const std::vector<uint32_t> &events)
{
  // first we must replay up to the first event without replaying it. This ensures any
  // non-command buffer calls like memory unmaps etc all happen correctly before this
  // command buffer
  m_pDevice->ReplayLog(0, events.front(), eReplay_WithoutDraw);

  D3D12InitPostVSCallback cb(m_pDevice, this, events);

  // now we replay the events, which are guaranteed (because we generated them in
  // GetPassEvents above) to come from the same command buffer, so the event IDs are
  // still locally continuous, even if we jump into replaying.
  m_pDevice->ReplayLog(events.front(), events.back(), eReplay_Full);
}

MeshFormat D3D12Replay::GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                                         MeshDataStage stage)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventId) != m_PostVSAlias.end())
    eventId = m_PostVSAlias[eventId];

  D3D12PostVSData postvs;
  RDCEraseEl(postvs);

  // no multiview support
  (void)viewID;

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    postvs = m_PostVSData[eventId];

  const D3D12PostVSData::StageData &s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf != NULL)
  {
    ret.indexResourceId = GetResID(s.idxBuf);
    ret.indexByteStride = s.idxFmt == DXGI_FORMAT_R16_UINT ? 2 : 4;
  }
  else
  {
    ret.indexResourceId = ResourceId();
    ret.indexByteStride = 0;
  }
  ret.indexByteOffset = 0;
  ret.baseVertex = 0;

  if(s.buf != NULL)
    ret.vertexResourceId = GetResID(s.buf);
  else
    ret.vertexResourceId = ResourceId();

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
    D3D12PostVSData::InstData inst = s.instData[instID];

    ret.vertexByteOffset = inst.bufOffset;
    ret.numIndices = inst.numVerts;
  }

  return ret;
}
