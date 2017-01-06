/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include <vector>
#include "d3d12_common.h"
#include "d3d12_manager.h"

class D3D12ResourceManager;

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
  D3D12RenderState();
  D3D12RenderState &operator=(const D3D12RenderState &o);

  void ApplyState(ID3D12GraphicsCommandList *list) const;
  void ApplyComputeRootElements(ID3D12GraphicsCommandList *cmd) const;
  void ApplyGraphicsRootElements(ID3D12GraphicsCommandList *cmd) const;

  vector<D3D12_VIEWPORT> views;
  vector<D3D12_RECT> scissors;

  vector<PortableHandle> rts;
  bool rtSingle;
  PortableHandle dsv;

  vector<ResourceId> GetRTVIDs() const;
  ResourceId GetDSVID() const;

  struct SignatureElement
  {
    SignatureElement() : type(eRootUnknown), offset(0) {}
    SignatureElement(SignatureElementType t, ResourceId i, UINT64 o) : type(t), id(i), offset(o) {}
    void SetConstant(UINT offs, UINT val) { SetConstants(1, &val, offs); }
    void SetConstants(UINT numVals, const void *vals, UINT offs)
    {
      type = eRootConst;

      if(constants.size() < offs + numVals)
        constants.resize(offs + numVals);

      memcpy(&constants[offs], vals, numVals * sizeof(UINT));
    }

    void SetToGraphics(D3D12ResourceManager *rm, ID3D12GraphicsCommandList *cmd, UINT slot) const
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
        cmd->SetGraphicsRootDescriptorTable(slot, handle);
      }
      else if(type == eRootCBV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetGraphicsRootConstantBufferView(slot, res->GetGPUVirtualAddress() + offset);
      }
      else if(type == eRootSRV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetGraphicsRootShaderResourceView(slot, res->GetGPUVirtualAddress() + offset);
      }
      else if(type == eRootUAV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetGraphicsRootUnorderedAccessView(slot, res->GetGPUVirtualAddress() + offset);
      }
    }

    void SetToCompute(D3D12ResourceManager *rm, ID3D12GraphicsCommandList *cmd, UINT slot) const
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
        cmd->SetComputeRootDescriptorTable(slot, handle);
      }
      else if(type == eRootCBV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetComputeRootConstantBufferView(slot, res->GetGPUVirtualAddress() + offset);
      }
      else if(type == eRootSRV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetComputeRootShaderResourceView(slot, res->GetGPUVirtualAddress() + offset);
      }
      else if(type == eRootUAV)
      {
        ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(id);
        cmd->SetComputeRootUnorderedAccessView(slot, res->GetGPUVirtualAddress() + offset);
      }
    }

    SignatureElementType type;

    ResourceId id;
    UINT64 offset;
    vector<UINT> constants;
  };

  vector<ResourceId> heaps;

  struct StreamOut
  {
    ResourceId buf;
    UINT64 offs;
    UINT64 size;

    ResourceId countbuf;
    UINT64 countoffs;
  };
  vector<StreamOut> streamouts;

  struct RootSignature
  {
    ResourceId rootsig;

    vector<SignatureElement> sigelems;
  } compute, graphics;

  ResourceId pipe;

  D3D12_PRIMITIVE_TOPOLOGY topo;
  UINT stencilRef;
  float blendFactor[4];

  struct IdxBuffer
  {
    ResourceId buf;
    UINT64 offs;
    int bytewidth;
    UINT size;
  } ibuffer;

  struct VertBuffer
  {
    ResourceId buf;
    UINT64 offs;
    UINT stride;
    UINT size;
  };
  vector<VertBuffer> vbuffers;

  D3D12ResourceManager *GetResourceManager() const { return m_ResourceManager; }
  D3D12ResourceManager *m_ResourceManager;
};
