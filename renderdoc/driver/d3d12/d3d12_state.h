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

#pragma once

#include "d3d12_common.h"
#include "d3d12_manager.h"

class D3D12ResourceManager;
class D3D12DebugManager;

enum SignatureElementType
{
  eRootUnknown,
  eRootConst,
  eRootTable,
  eRootCBV,
  eRootSRV,
  eRootUAV,
};

struct D3D12RenderState
{
  D3D12RenderState() = default;
  D3D12RenderState(const D3D12RenderState &o) = default;
  D3D12RenderState &operator=(const D3D12RenderState &o) = default;

  void ApplyState(WrappedID3D12Device *dev, ID3D12GraphicsCommandListX *list) const;
  void ApplyDescriptorHeaps(ID3D12GraphicsCommandList *list) const;
  void ApplyComputeRootElements(ID3D12GraphicsCommandList *cmd) const;
  void ApplyGraphicsRootElements(ID3D12GraphicsCommandList *cmd) const;
  void ApplyComputeRootElementsUnwrapped(ID3D12GraphicsCommandList *cmd) const;
  void ApplyGraphicsRootElementsUnwrapped(ID3D12GraphicsCommandList *cmd) const;

  rdcarray<D3D12_VIEWPORT> views;
  rdcarray<D3D12_RECT> scissors;

  // these are D3D12Descriptor copies since the values of the descriptors are read during
  // OMSetRenderTargets and may not exist anywhere after that if they are immediately overwritten.
  rdcarray<D3D12Descriptor> rts;
  D3D12Descriptor dsv;

  bool renderpass = false;
  rdcarray<D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS> rpResolves;
  rdcarray<D3D12_RENDER_PASS_RENDER_TARGET_DESC> rpRTs;
  D3D12_RENDER_PASS_DEPTH_STENCIL_DESC rpDSV;
  D3D12_RENDER_PASS_FLAGS rpFlags;

  rdcarray<ResourceId> GetRTVIDs() const;
  ResourceId GetDSVID() const;

  float depthBias = 0.0f, depthBiasClamp = 0.0f, slopeScaledDepthBias = 0.0f;
  D3D12_INDEX_BUFFER_STRIP_CUT_VALUE cutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

  ResourceId shadingRateImage;
  D3D12_SHADING_RATE shadingRate;
  D3D12_SHADING_RATE_COMBINER shadingRateCombiners[2];

  struct SignatureElement
  {
    SignatureElement() : type(eRootUnknown), offset(0) {}
    SignatureElement(SignatureElementType t, ResourceId i, UINT64 o) : type(t), id(i), offset(o) {}
    SignatureElement(SignatureElementType t, D3D12_GPU_VIRTUAL_ADDRESS addr);
    SignatureElement(SignatureElementType t, D3D12_CPU_DESCRIPTOR_HANDLE handle);

    void SetConstant(UINT offs, UINT val) { SetConstants(1, &val, offs); }
    void SetConstants(UINT numVals, const void *vals, UINT offs)
    {
      type = eRootConst;

      if(constants.size() < offs + numVals)
        constants.resize(offs + numVals);

      memcpy(&constants[offs], vals, numVals * sizeof(UINT));
    }

    void SetToGraphics(D3D12ResourceManager *rm, ID3D12GraphicsCommandList *cmd, UINT slot,
                       bool unwrapped) const
    {
      if(type == eRootConst)
      {
        cmd->SetGraphicsRoot32BitConstants(slot, (UINT)constants.size(), &constants[0], 0);
      }
      else if(type == eRootTable)
      {
        D3D12_GPU_DESCRIPTOR_HANDLE handle =
            rm->GetCurrentAs<ID3D12DescriptorHeap>(id)->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += sizeof(D3D12Descriptor) * offset;
        if(unwrapped)
          cmd->SetGraphicsRootDescriptorTable(slot, Unwrap(handle));
        else
          cmd->SetGraphicsRootDescriptorTable(slot, handle);
      }
      else if(type == eRootCBV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetGraphicsRootConstantBufferView(slot, res ? res->GetGPUVirtualAddress() + offset : 0);
      }
      else if(type == eRootSRV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetGraphicsRootShaderResourceView(slot, res ? res->GetGPUVirtualAddress() + offset : 0);
      }
      else if(type == eRootUAV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetGraphicsRootUnorderedAccessView(slot, res ? res->GetGPUVirtualAddress() + offset : 0);
      }
    }

    void SetToCompute(D3D12ResourceManager *rm, ID3D12GraphicsCommandList *cmd, UINT slot,
                      bool unwrapped) const
    {
      if(type == eRootConst)
      {
        cmd->SetComputeRoot32BitConstants(slot, (UINT)constants.size(), &constants[0], 0);
      }
      else if(type == eRootTable)
      {
        D3D12_GPU_DESCRIPTOR_HANDLE handle =
            rm->GetCurrentAs<ID3D12DescriptorHeap>(id)->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += sizeof(D3D12Descriptor) * offset;
        if(unwrapped)
          cmd->SetComputeRootDescriptorTable(slot, Unwrap(handle));
        else
          cmd->SetComputeRootDescriptorTable(slot, handle);
      }
      else if(type == eRootCBV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetComputeRootConstantBufferView(slot, res ? res->GetGPUVirtualAddress() + offset : 0);
      }
      else if(type == eRootSRV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetComputeRootShaderResourceView(slot, res ? res->GetGPUVirtualAddress() + offset : 0);
      }
      else if(type == eRootUAV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetComputeRootUnorderedAccessView(slot, res ? res->GetGPUVirtualAddress() + offset : 0);
      }
    }

    SignatureElementType type;

    ResourceId id;
    UINT64 offset;
    rdcarray<UINT> constants;
  };

  rdcarray<ResourceId> heaps;

  struct StreamOut
  {
    ResourceId buf;
    UINT64 offs;
    UINT64 size;

    ResourceId countbuf;
    UINT64 countoffs;
  };
  rdcarray<StreamOut> streamouts;

  struct RootSignature
  {
    ResourceId rootsig;

    rdcarray<SignatureElement> sigelems;
  } compute, graphics;

  ResourceId pipe;
  ResourceId stateobj;

  UINT viewInstMask = 0;

  struct SamplePositions
  {
    UINT NumSamplesPerPixel, NumPixels;
    rdcarray<D3D12_SAMPLE_POSITION> Positions;
  } samplePos;

  D3D12_PRIMITIVE_TOPOLOGY topo = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
  UINT stencilRefFront = 0, stencilRefBack = 0;
  float blendFactor[4] = {};

  float depthBoundsMin = 0.0f, depthBoundsMax = 1.0f;

  struct IdxBuffer
  {
    ResourceId buf;
    UINT64 offs = 0;
    int bytewidth = 0;
    UINT size = 0;
  } ibuffer;

  struct VertBuffer
  {
    ResourceId buf;
    UINT64 offs;
    UINT stride;
    UINT size;
  };
  rdcarray<VertBuffer> vbuffers;

  D3D12ResourceManager *GetResourceManager() const { return m_ResourceManager; }
  D3D12ResourceManager *m_ResourceManager = NULL;

  D3D12DebugManager *GetDebugManager() const { return m_DebugManager; }
  D3D12DebugManager *m_DebugManager = NULL;

  struct IndirectPendingState
  {
    ID3D12Resource *argsBuf = NULL;
    uint64_t argsOffs = 0;
    ID3D12CommandSignature *comSig = NULL;
    uint32_t argsToProcess = 0;
  } indirectState;
  void ResolvePendingIndirectState(WrappedID3D12Device *device);
};
