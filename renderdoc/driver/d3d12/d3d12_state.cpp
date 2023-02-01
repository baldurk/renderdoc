/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
#include "d3d12_debug.h"
#include "d3d12_manager.h"
#include "d3d12_resources.h"

D3D12RenderState::SignatureElement::SignatureElement(SignatureElementType t,
                                                     D3D12_GPU_VIRTUAL_ADDRESS addr)
{
  type = t;
  WrappedID3D12Resource::GetResIDFromAddr(addr, id, offset);
}

D3D12RenderState::SignatureElement::SignatureElement(SignatureElementType t,
                                                     D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  type = t;
  D3D12Descriptor *desc = (D3D12Descriptor *)handle.ptr;
  id = desc->GetHeap()->GetResourceID();
  offset = desc->GetHeapIndex();
}

rdcarray<ResourceId> D3D12RenderState::GetRTVIDs() const
{
  rdcarray<ResourceId> ret;

  for(UINT i = 0; i < rts.size(); i++)
  {
    RDCASSERT(rts[i].GetType() == D3D12DescriptorType::RTV);
    ret.push_back(rts[i].GetResResourceId());
  }

  return ret;
}

ResourceId D3D12RenderState::GetDSVID() const
{
  return dsv.GetResResourceId();
}

void D3D12RenderState::ApplyState(WrappedID3D12Device *dev, ID3D12GraphicsCommandListX *cmd) const
{
  D3D12_COMMAND_LIST_TYPE type = cmd->GetType();

  if(pipe != ResourceId())
    cmd->SetPipelineState(GetResourceManager()->GetCurrentAs<ID3D12PipelineState>(pipe));

  if(type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_BUNDLE)
  {
    if(!views.empty())
      cmd->RSSetViewports((UINT)views.size(), &views[0]);

    if(!scissors.empty())
      cmd->RSSetScissorRects((UINT)scissors.size(), &scissors[0]);

    if(topo != D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
      cmd->IASetPrimitiveTopology(topo);

    cmd->OMSetStencilRef(stencilRef);
    cmd->OMSetBlendFactor(blendFactor);

    if(GetWrapped(cmd)->GetReal1())
    {
      if(dev->GetOpts2().DepthBoundsTestSupported)
        cmd->OMSetDepthBounds(depthBoundsMin, depthBoundsMax);

      if(dev->GetOpts2().ProgrammableSamplePositionsTier !=
         D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED)
      {
        if(samplePos.NumPixels > 0 && samplePos.NumSamplesPerPixel > 0)
        {
          cmd->SetSamplePositions(samplePos.NumSamplesPerPixel, samplePos.NumPixels,
                                  (D3D12_SAMPLE_POSITION *)samplePos.Positions.data());
        }
      }

      // safe to set this - if the pipeline has view instancing disabled, it will do nothing
      if(dev->GetOpts3().ViewInstancingTier != D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED)
        cmd->SetViewInstanceMask(viewInstMask);
    }

    if(GetWrapped(cmd)->GetReal5())
    {
      if(dev->GetOpts6().VariableShadingRateTier != D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED)
      {
        cmd->RSSetShadingRate(shadingRate, shadingRateCombiners);
        if(shadingRateImage != ResourceId())
          cmd->RSSetShadingRateImage(
              GetResourceManager()->GetCurrentAs<ID3D12Resource>(shadingRateImage));
      }
    }

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
      vb.BufferLocation = 0;

      if(vbuffers[i].buf != ResourceId())
      {
        ID3D12Resource *res = GetResourceManager()->GetCurrentAs<ID3D12Resource>(vbuffers[i].buf);
        if(res)
          vb.BufferLocation = res->GetGPUVirtualAddress() + vbuffers[i].offs;
        else
          vb.BufferLocation = 0;

        vb.StrideInBytes = vbuffers[i].stride;
        vb.SizeInBytes = vbuffers[i].size;

        cmd->IASetVertexBuffers((UINT)i, 1, &vb);
      }
    }

    if(!rts.empty() || dsv.GetResResourceId() != ResourceId())
    {
      D3D12_CPU_DESCRIPTOR_HANDLE rtHandles[8];
      D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};

      if(dsv.GetResResourceId() != ResourceId())
        dsvHandle = Unwrap(GetDebugManager()->GetTempDescriptor(dsv));

      for(size_t i = 0; i < rts.size(); i++)
        rtHandles[i] = Unwrap(GetDebugManager()->GetTempDescriptor(rts[i], i));

      // need to unwrap here, as FromPortableHandle unwraps too.
      Unwrap(cmd)->OMSetRenderTargets((UINT)rts.size(), rtHandles, FALSE,
                                      dsvHandle.ptr ? &dsvHandle : NULL);
    }
  }

  ApplyDescriptorHeaps(cmd);

  if(graphics.rootsig != ResourceId())
  {
    cmd->SetGraphicsRootSignature(
        GetResourceManager()->GetCurrentAs<ID3D12RootSignature>(graphics.rootsig));

    ApplyGraphicsRootElements(cmd);
  }

  if(compute.rootsig != ResourceId())
  {
    cmd->SetComputeRootSignature(
        GetResourceManager()->GetCurrentAs<ID3D12RootSignature>(compute.rootsig));

    ApplyComputeRootElements(cmd);
  }
}

void D3D12RenderState::ApplyDescriptorHeaps(ID3D12GraphicsCommandListX *cmd) const
{
  rdcarray<ID3D12DescriptorHeap *> descHeaps;
  descHeaps.resize(heaps.size());

  for(size_t i = 0; i < heaps.size(); i++)
    descHeaps[i] = GetResourceManager()->GetCurrentAs<ID3D12DescriptorHeap>(heaps[i]);

  if(!descHeaps.empty())
    cmd->SetDescriptorHeaps((UINT)descHeaps.size(), &descHeaps[0]);
}

void D3D12RenderState::ApplyComputeRootElements(ID3D12GraphicsCommandListX *cmd) const
{
  for(size_t i = 0; i < compute.sigelems.size(); i++)
  {
    // just don't set tables that aren't in the descriptor heaps, since it's invalid and can crash
    // and is probably just from stale bindings that aren't going to be used
    if(compute.sigelems[i].type != eRootTable || heaps.contains(compute.sigelems[i].id))
    {
      compute.sigelems[i].SetToCompute(GetResourceManager(), cmd, (UINT)i);
    }
    else
    {
      RDCDEBUG("Skipping setting possibly stale compute root table referring to heap %s",
               ToStr(compute.sigelems[i].id).c_str());
    }
  }
}

void D3D12RenderState::ApplyGraphicsRootElements(ID3D12GraphicsCommandListX *cmd) const
{
  for(size_t i = 0; i < graphics.sigelems.size(); i++)
  {
    // just don't set tables that aren't in the descriptor heaps, since it's invalid and can crash
    // and is probably just from stale bindings that aren't going to be used
    if(graphics.sigelems[i].type != eRootTable || heaps.contains(graphics.sigelems[i].id))
    {
      graphics.sigelems[i].SetToGraphics(GetResourceManager(), cmd, (UINT)i);
    }
    else
    {
      RDCDEBUG("Skipping setting possibly stale graphics root table referring to heap %s",
               ToStr(graphics.sigelems[i].id).c_str());
    }
  }
}
