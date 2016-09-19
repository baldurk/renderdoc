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

#include "d3d12_state.h"
#include "d3d12_command_list.h"
#include "d3d12_manager.h"
#include "d3d12_resources.h"

D3D12RenderState::D3D12RenderState()
{
  views.clear();
  scissors.clear();

  rts.clear();
  rtSingle = false;
  dsv = PortableHandle();

  m_ResourceManager = NULL;

  heaps.clear();

  pipe = graphics.rootsig = compute.rootsig = ResourceId();
  graphics.sigelems.clear();
  compute.sigelems.clear();

  topo = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

  stencilRef = 0;
  RDCEraseEl(blendFactor);

  RDCEraseEl(ibuffer);
  vbuffers.clear();
}

D3D12RenderState &D3D12RenderState::operator=(const D3D12RenderState &o)
{
  views = o.views;
  scissors = o.scissors;

  rts = o.rts;
  rtSingle = o.rtSingle;
  dsv = o.dsv;

  pipe = o.pipe;

  heaps = o.heaps;

  graphics.rootsig = o.graphics.rootsig;
  graphics.sigelems = o.graphics.sigelems;
  compute.rootsig = o.compute.rootsig;
  compute.sigelems = o.compute.sigelems;

  topo = o.topo;
  stencilRef = o.stencilRef;
  memcpy(blendFactor, o.blendFactor, sizeof(blendFactor));

  ibuffer = o.ibuffer;
  vbuffers = o.vbuffers;

  return *this;
}

void D3D12RenderState::ApplyState(ID3D12GraphicsCommandList *cmd)
{
  if(pipe != ResourceId())
    cmd->SetPipelineState(GetResourceManager()->GetCurrentAs<ID3D12PipelineState>(pipe));

  if(!views.empty())
    cmd->RSSetViewports((UINT)views.size(), &views[0]);

  if(!scissors.empty())
    cmd->RSSetScissorRects((UINT)scissors.size(), &scissors[0]);

  if(topo != D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
    cmd->IASetPrimitiveTopology(topo);

  cmd->OMSetStencilRef(stencilRef);
  cmd->OMSetBlendFactor(blendFactor);

  if(ibuffer.buf != ResourceId())
  {
    D3D12_INDEX_BUFFER_VIEW ib;

    ID3D12Resource *res = GetResourceManager()->GetCurrentAs<ID3D12Resource>(ibuffer.buf);
    if(res)
      ib.BufferLocation = res->GetGPUVirtualAddress() + ibuffer.offs;
    else
      ib.BufferLocation = 0;

    ib.Format = (ibuffer.bytewidth == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
    ib.SizeInBytes = ibuffer.size;

    cmd->IASetIndexBuffer(&ib);
  }

  for(size_t i = 0; i < vbuffers.size(); i++)
  {
    D3D12_VERTEX_BUFFER_VIEW vb;

    ID3D12Resource *res = GetResourceManager()->GetCurrentAs<ID3D12Resource>(vbuffers[i].buf);
    if(res)
      vb.BufferLocation = res->GetGPUVirtualAddress() + vbuffers[i].offs;
    else
      vb.BufferLocation = 0;

    vb.StrideInBytes = vbuffers[i].stride;
    vb.SizeInBytes = vbuffers[i].size;

    cmd->IASetVertexBuffers((UINT)i, 1, &vb);
  }

  std::vector<ID3D12DescriptorHeap *> descHeaps;
  descHeaps.resize(heaps.size());

  for(size_t i = 0; i < heaps.size(); i++)
    descHeaps[i] = GetResourceManager()->GetCurrentAs<ID3D12DescriptorHeap>(heaps[i]);

  if(!descHeaps.empty())
    cmd->SetDescriptorHeaps((UINT)descHeaps.size(), &descHeaps[0]);

  if(!rts.empty() || dsv.heap != ResourceId())
  {
    D3D12_CPU_DESCRIPTOR_HANDLE rtHandles[8];
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = CPUHandleFromPortableHandle(GetResourceManager(), dsv);

    UINT numActualHandles = rtSingle ? 1 : (UINT)rts.size();

    for(UINT i = 0; i < numActualHandles; i++)
      rtHandles[i] = CPUHandleFromPortableHandle(GetResourceManager(), rts[i]);

    // need to unwrap here, as FromPortableHandle unwraps too.
    Unwrap(cmd)->OMSetRenderTargets((UINT)rts.size(), rtHandles, rtSingle ? TRUE : FALSE,
                                    dsv.heap != ResourceId() ? &dsvHandle : NULL);
  }

  if(graphics.rootsig != ResourceId())
  {
    cmd->SetGraphicsRootSignature(
        GetResourceManager()->GetCurrentAs<ID3D12RootSignature>(graphics.rootsig));

    for(size_t i = 0; i < graphics.sigelems.size(); i++)
      graphics.sigelems[i].SetToGraphics(GetResourceManager(), cmd, (UINT)i);
  }

  if(compute.rootsig != ResourceId())
  {
    cmd->SetComputeRootSignature(
        GetResourceManager()->GetCurrentAs<ID3D12RootSignature>(compute.rootsig));

    for(size_t i = 0; i < compute.sigelems.size(); i++)
      compute.sigelems[i].SetToCompute(GetResourceManager(), cmd, (UINT)i);
  }
}