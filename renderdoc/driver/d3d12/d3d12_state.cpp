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

void D3D12RenderState::ResolvePendingIndirectState(WrappedID3D12Device *device)
{
  if(indirectState.argsBuf == NULL)
    return;

  device->GPUSync();

  D3D12_RANGE range = {0, D3D12CommandData::m_IndirectSize};
  byte *mapPtr = NULL;
  device->CheckHRESULT(indirectState.argsBuf->Map(0, &range, (void **)&mapPtr));

  if(device->HasFatalError())
    return;

  WrappedID3D12CommandSignature *comSig = (WrappedID3D12CommandSignature *)indirectState.comSig;

  {
    byte *data = mapPtr + indirectState.argsOffs;

    for(uint32_t argIdx = 0; argIdx < indirectState.argsToProcess; argIdx++)
    {
      size_t execIdx = argIdx / comSig->sig.arguments.size();
      uint32_t argWithinExecIdx = argIdx % comSig->sig.arguments.size();
      const D3D12_INDIRECT_ARGUMENT_DESC &arg = comSig->sig.arguments[argWithinExecIdx];

      switch(arg.Type)
      {
        case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
        case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
        case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH:
        case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS:
        {
          // we know this is the final argument in the signature. Set the data pointer to the start
          // of the next execute with the proper stride. This may be unused if we are only
          // processing one execute's worth of arguments
          data = mapPtr + indirectState.argsOffs + comSig->sig.ByteStride * (execIdx + 1);
          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
        {
          size_t argSize = sizeof(uint32_t) * arg.Constant.Num32BitValuesToSet;
          const uint32_t *data32 = (uint32_t *)data;
          data += argSize;

          if(comSig->sig.graphics)
          {
            graphics.sigelems.resize_for_index(arg.ConstantBufferView.RootParameterIndex);
            graphics.sigelems[arg.Constant.RootParameterIndex].constants.assign(
                data32, arg.Constant.Num32BitValuesToSet);
          }
          else
          {
            compute.sigelems.resize_for_index(arg.ConstantBufferView.RootParameterIndex);
            compute.sigelems[arg.Constant.RootParameterIndex].constants.assign(
                data32, arg.Constant.Num32BitValuesToSet);
          }

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
        {
          const D3D12_VERTEX_BUFFER_VIEW *vb = (D3D12_VERTEX_BUFFER_VIEW *)data;
          data += sizeof(D3D12_VERTEX_BUFFER_VIEW);

          ResourceId id;
          uint64_t offs = 0;
          D3D12_GPU_VIRTUAL_ADDRESS va = vb->BufferLocation;
          device->GetResIDFromOrigAddr(va, id, offs);

          ID3D12Resource *res = GetResourceManager()->GetLiveAs<ID3D12Resource>(id);

          if(arg.VertexBuffer.Slot >= vbuffers.size())
            vbuffers.resize(arg.VertexBuffer.Slot + 1);

          vbuffers[arg.VertexBuffer.Slot].buf = GetResID(res);
          vbuffers[arg.VertexBuffer.Slot].offs = offs;
          vbuffers[arg.VertexBuffer.Slot].size = vb->SizeInBytes;
          vbuffers[arg.VertexBuffer.Slot].stride = vb->StrideInBytes;

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
        {
          const D3D12_INDEX_BUFFER_VIEW *ib = (D3D12_INDEX_BUFFER_VIEW *)data;
          data += sizeof(D3D12_INDEX_BUFFER_VIEW);

          ResourceId id;
          uint64_t offs = 0;
          device->GetResIDFromOrigAddr(ib->BufferLocation, id, offs);

          ID3D12Resource *res = GetResourceManager()->GetLiveAs<ID3D12Resource>(id);

          ibuffer.buf = GetResID(res);
          ibuffer.offs = offs;
          ibuffer.size = ib->SizeInBytes;
          ibuffer.bytewidth = ib->Format == DXGI_FORMAT_R32_UINT ? 4 : 2;

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
        {
          const D3D12_GPU_VIRTUAL_ADDRESS *addr = (D3D12_GPU_VIRTUAL_ADDRESS *)data;
          data += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);

          ResourceId id;
          uint64_t offs = 0;
          device->GetResIDFromOrigAddr(*addr, id, offs);

          ID3D12Resource *res = GetResourceManager()->GetLiveAs<ID3D12Resource>(id);

          SignatureElementType t = eRootCBV;
          if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW)
            t = eRootSRV;
          if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW)
            t = eRootUAV;

          // ConstantBufferView, ShaderResourceView and UnorderedAccessView all have one member -
          // RootParameterIndex
          if(comSig->sig.graphics)
          {
            graphics.sigelems.resize_for_index(arg.ConstantBufferView.RootParameterIndex);
            graphics.sigelems[arg.ConstantBufferView.RootParameterIndex] =
                D3D12RenderState::SignatureElement(t, GetResID(res), offs);
          }
          else
          {
            compute.sigelems.resize_for_index(arg.ConstantBufferView.RootParameterIndex);
            compute.sigelems[arg.ConstantBufferView.RootParameterIndex] =
                D3D12RenderState::SignatureElement(t, GetResID(res), offs);
          }

          break;
        }
        default: RDCERR("Unexpected argument type! %d", arg.Type); break;
      }
    }
  }

  indirectState.argsBuf->Unmap(0, &range);
  indirectState.argsBuf = NULL;
  indirectState.argsOffs = 0;
  indirectState.comSig = NULL;
  indirectState.argsToProcess = 0;
}

void D3D12RenderState::ApplyState(WrappedID3D12Device *dev, ID3D12GraphicsCommandListX *cmd) const
{
  D3D12_COMMAND_LIST_TYPE type = cmd->GetType();

  if(pipe != ResourceId())
    cmd->SetPipelineState(GetResourceManager()->GetCurrentAs<ID3D12PipelineState>(pipe));

  if(stateobj != ResourceId())
    cmd->SetPipelineState1(GetResourceManager()->GetCurrentAs<ID3D12StateObject>(stateobj));

  if(type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_BUNDLE)
  {
    if(!views.empty())
      cmd->RSSetViewports((UINT)views.size(), &views[0]);

    if(!scissors.empty())
      cmd->RSSetScissorRects((UINT)scissors.size(), &scissors[0]);

    if(topo != D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
      cmd->IASetPrimitiveTopology(topo);

    if(stencilRefFront != stencilRefBack && GetWrapped(cmd)->GetReal8() &&
       dev->GetOpts14().IndependentFrontAndBackStencilRefMaskSupported)
      cmd->OMSetFrontAndBackStencilRef(stencilRefFront, stencilRefBack);
    else
      cmd->OMSetStencilRef(stencilRefFront);
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
      if(dev->GetOpts3().ViewInstancingTier != D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED &&
         viewInstMask != 0)
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

    if(GetWrapped(cmd)->GetReal9())
    {
      if(dev->GetOpts15().DynamicIndexBufferStripCutSupported)
      {
        cmd->IASetIndexBufferStripCutValue(cutValue);
      }

      if(dev->GetOpts16().DynamicDepthBiasSupported)
      {
        cmd->RSSetDepthBias(depthBias, depthBiasClamp, slopeScaledDepthBias);
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

void D3D12RenderState::ApplyDescriptorHeaps(ID3D12GraphicsCommandList *cmd) const
{
  rdcarray<ID3D12DescriptorHeap *> descHeaps;
  descHeaps.resize(heaps.size());

  for(size_t i = 0; i < heaps.size(); i++)
    descHeaps[i] = GetResourceManager()->GetCurrentAs<ID3D12DescriptorHeap>(heaps[i]);

  if(!descHeaps.empty())
    cmd->SetDescriptorHeaps((UINT)descHeaps.size(), &descHeaps[0]);
}

void D3D12RenderState::ApplyComputeRootElements(ID3D12GraphicsCommandList *cmd) const
{
  for(size_t i = 0; i < compute.sigelems.size(); i++)
  {
    // just don't set tables that aren't in the descriptor heaps, since it's invalid and can crash
    // and is probably just from stale bindings that aren't going to be used
    if(compute.sigelems[i].type != eRootTable || heaps.contains(compute.sigelems[i].id))
    {
      compute.sigelems[i].SetToCompute(GetResourceManager(), cmd, (UINT)i, false);
    }
    else
    {
      RDCDEBUG("Skipping setting possibly stale compute root table referring to heap %s",
               ToStr(compute.sigelems[i].id).c_str());
    }
  }
}

void D3D12RenderState::ApplyGraphicsRootElements(ID3D12GraphicsCommandList *cmd) const
{
  for(size_t i = 0; i < graphics.sigelems.size(); i++)
  {
    // just don't set tables that aren't in the descriptor heaps, since it's invalid and can crash
    // and is probably just from stale bindings that aren't going to be used
    if(graphics.sigelems[i].type != eRootTable || heaps.contains(graphics.sigelems[i].id))
    {
      graphics.sigelems[i].SetToGraphics(GetResourceManager(), cmd, (UINT)i, false);
    }
    else
    {
      RDCDEBUG("Skipping setting possibly stale graphics root table referring to heap %s",
               ToStr(graphics.sigelems[i].id).c_str());
    }
  }
}

void D3D12RenderState::ApplyComputeRootElementsUnwrapped(ID3D12GraphicsCommandList *cmd) const
{
  for(size_t i = 0; i < compute.sigelems.size(); i++)
  {
    // just don't set tables that aren't in the descriptor heaps, since it's invalid and can crash
    // and is probably just from stale bindings that aren't going to be used
    if(compute.sigelems[i].type != eRootTable || heaps.contains(compute.sigelems[i].id))
    {
      compute.sigelems[i].SetToCompute(GetResourceManager(), cmd, (UINT)i, true);
    }
    else
    {
      RDCDEBUG("Skipping setting possibly stale compute root table referring to heap %s",
               ToStr(compute.sigelems[i].id).c_str());
    }
  }
}

void D3D12RenderState::ApplyGraphicsRootElementsUnwrapped(ID3D12GraphicsCommandList *cmd) const
{
  for(size_t i = 0; i < graphics.sigelems.size(); i++)
  {
    // just don't set tables that aren't in the descriptor heaps, since it's invalid and can crash
    // and is probably just from stale bindings that aren't going to be used
    if(graphics.sigelems[i].type != eRootTable || heaps.contains(graphics.sigelems[i].id))
    {
      graphics.sigelems[i].SetToGraphics(GetResourceManager(), cmd, (UINT)i, true);
    }
    else
    {
      RDCDEBUG("Skipping setting possibly stale graphics root table referring to heap %s",
               ToStr(graphics.sigelems[i].id).c_str());
    }
  }
}
