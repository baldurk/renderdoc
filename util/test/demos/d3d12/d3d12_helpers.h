/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2019-2021 Baldur Karlsson
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

#include <comdef.h>
#include "dx/d3d_helpers.h"
#include "dx/official/D3D12Downlevel.h"
#include "dx/official/d3d12.h"

COM_SMARTPTR(ID3DBlob);

COM_SMARTPTR(ID3D12Debug);
COM_SMARTPTR(ID3D12Debug1);

COM_SMARTPTR(ID3D12Device);
COM_SMARTPTR(ID3D12Device1);
COM_SMARTPTR(ID3D12Device2);
COM_SMARTPTR(ID3D12Device3);
COM_SMARTPTR(ID3D12Device4);
COM_SMARTPTR(ID3D12Device5);
COM_SMARTPTR(ID3D12Device6);
COM_SMARTPTR(ID3D12Device7);
COM_SMARTPTR(ID3D12Device8);

COM_SMARTPTR(ID3D12Fence);

COM_SMARTPTR(ID3D12CommandQueue);
COM_SMARTPTR(ID3D12CommandAllocator);
COM_SMARTPTR(ID3D12CommandList);
COM_SMARTPTR(ID3D12GraphicsCommandList);
COM_SMARTPTR(ID3D12GraphicsCommandList1);
COM_SMARTPTR(ID3D12GraphicsCommandList2);
COM_SMARTPTR(ID3D12GraphicsCommandList3);
COM_SMARTPTR(ID3D12GraphicsCommandList4);
COM_SMARTPTR(ID3D12GraphicsCommandList5);
COM_SMARTPTR(ID3D12GraphicsCommandList6);

COM_SMARTPTR(ID3D12CommandSignature);

COM_SMARTPTR(ID3D12RootSignature);
COM_SMARTPTR(ID3D12PipelineState);

COM_SMARTPTR(ID3D12DebugCommandList);

COM_SMARTPTR(ID3D12Resource);

COM_SMARTPTR(ID3D12Heap);
COM_SMARTPTR(ID3D12Heap1);

COM_SMARTPTR(ID3D12DescriptorHeap);

COM_SMARTPTR(ID3D12InfoQueue);

COM_SMARTPTR(ID3D12CommandQueueDownlevel);

struct D3D12GraphicsTest;

class D3D12PSOCreator
{
public:
  D3D12PSOCreator(ID3D12DevicePtr dev);

  D3D12PSOCreator &VS(ID3DBlobPtr blob);
  D3D12PSOCreator &HS(ID3DBlobPtr blob);
  D3D12PSOCreator &DS(ID3DBlobPtr blob);
  D3D12PSOCreator &GS(ID3DBlobPtr blob);
  D3D12PSOCreator &PS(ID3DBlobPtr blob);
  D3D12PSOCreator &CS(ID3DBlobPtr blob);

  D3D12PSOCreator &InputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC> &elements);
  D3D12PSOCreator &InputLayout();

  D3D12PSOCreator &StripRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE stripCut);

  D3D12PSOCreator &Topology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topo);

  D3D12PSOCreator &RootSig(ID3D12RootSignaturePtr rootSig);

  D3D12PSOCreator &RTVs(const std::vector<DXGI_FORMAT> &fmts);
  D3D12PSOCreator &DSV(DXGI_FORMAT fmt);

  D3D12PSOCreator &SampleCount(UINT Samples);

  operator ID3D12PipelineStatePtr() const;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsDesc = {};
  D3D12_COMPUTE_PIPELINE_STATE_DESC ComputeDesc = {};

private:
  ID3D12DevicePtr m_Dev;
};

class D3D12BufferCreator
{
public:
  D3D12BufferCreator(ID3D12DevicePtr dev, D3D12GraphicsTest *test);

  D3D12BufferCreator &UAV();

  D3D12BufferCreator &Upload();
  D3D12BufferCreator &Readback();

  D3D12BufferCreator &Data(const void *data);
  D3D12BufferCreator &Size(UINT size);

  template <typename T, size_t N>
  D3D12BufferCreator &Data(const T (&data)[N])
  {
    return Data(&data[0]).Size(UINT(N * sizeof(T)));
  }

  template <typename T>
  D3D12BufferCreator &Data(const std::vector<T> &data)
  {
    return Data(data.data()).Size(UINT(data.size() * sizeof(T)));
  }

  operator ID3D12ResourcePtr() const;

private:
  ID3D12DevicePtr m_Dev;
  D3D12GraphicsTest *m_Test;

  D3D12_RESOURCE_DESC m_BufDesc;
  D3D12_HEAP_PROPERTIES m_HeapDesc;
  const void *m_Initdata = NULL;
};

class D3D12TextureCreator
{
public:
  D3D12TextureCreator(ID3D12DevicePtr dev, DXGI_FORMAT format, UINT width, UINT height, UINT depth);

  D3D12TextureCreator &Mips(UINT mips);
  D3D12TextureCreator &Array(UINT size);
  D3D12TextureCreator &Multisampled(UINT count, UINT quality = 0);

  D3D12TextureCreator &UAV();
  D3D12TextureCreator &RTV();
  D3D12TextureCreator &DSV();

  D3D12TextureCreator &Upload();
  D3D12TextureCreator &Readback();
  D3D12TextureCreator &CustomHeap(D3D12_HEAP_PROPERTIES heap);

  D3D12TextureCreator &Shared();

  D3D12TextureCreator &InitialState(D3D12_RESOURCE_STATES state);

  operator ID3D12ResourcePtr() const;

protected:
  ID3D12DevicePtr m_Dev;

  D3D12_RESOURCE_STATES m_InitialState;
  D3D12_RESOURCE_DESC m_TexDesc;
  D3D12_HEAP_PROPERTIES m_HeapDesc;
  D3D12_HEAP_FLAGS m_HeapFlags = D3D12_HEAP_FLAG_NONE;
};

class D3D12ViewCreator
{
public:
  D3D12ViewCreator(ID3D12DevicePtr dev, ID3D12DescriptorHeap *heap, ID3D12DescriptorHeap *clearHeap,
                   ViewType viewType, ID3D12Resource *res);

  // common params
  D3D12ViewCreator &Format(DXGI_FORMAT format);

  // cbv params
  D3D12ViewCreator &Offset(UINT offset);
  D3D12ViewCreator &SizeBytes(UINT size);

  // buffer params
  D3D12ViewCreator &FirstElement(UINT el);
  D3D12ViewCreator &NumElements(UINT num);
  D3D12ViewCreator &StructureStride(UINT stride);
  D3D12ViewCreator &ByteAddressed();

  // TODO counter buffer offset

  // texture params
  D3D12ViewCreator &FirstMip(UINT mip);
  D3D12ViewCreator &NumMips(UINT num);
  D3D12ViewCreator &FirstSlice(UINT mip);
  D3D12ViewCreator &NumSlices(UINT num);

  // TODO ResourceMinLODClamp
  D3D12ViewCreator &Swizzle(UINT swizzle);
  D3D12ViewCreator &PlaneSlice(UINT plane);

  // depth stencil only
  D3D12ViewCreator &ReadOnlyDepth();
  D3D12ViewCreator &ReadOnlyStencil();

  D3D12_CPU_DESCRIPTOR_HANDLE CreateCPU(ID3D12DescriptorHeap *heap, uint32_t descriptor);
  D3D12_GPU_DESCRIPTOR_HANDLE CreateGPU(ID3D12DescriptorHeap *heap, uint32_t descriptor);

  D3D12_CPU_DESCRIPTOR_HANDLE CreateCPU(uint32_t descriptor)
  {
    return CreateCPU(m_Heap, descriptor);
  }
  D3D12_GPU_DESCRIPTOR_HANDLE CreateGPU(uint32_t descriptor)
  {
    return CreateGPU(m_Heap, descriptor);
  }
  D3D12_CPU_DESCRIPTOR_HANDLE CreateClearCPU(uint32_t descriptor)
  {
    return CreateCPU(m_ClearHeap, descriptor);
  }

private:
  void SetupDescriptors(ViewType viewType, ResourceType resType);

  ID3D12DevicePtr m_Dev;
  ID3D12Resource *m_Res;
  ID3D12DescriptorHeap *m_Heap;
  ID3D12DescriptorHeap *m_ClearHeap;
  ViewType m_Type;

  // instead of a huge mess trying to auto populate the actual descriptors from saved values, as
  // they aren't very nicely compatible (e.g. RTVs have mipslice selection on 3D textures, SRVs
  // don't, SRVs support 1D texturse but DSVs don't, UAVs don't support MSAA textures, etc etc).
  // Instead we just set save pointers that might be NULL in the constructor based on view and
  // resource type
  union
  {
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv;
    D3D12_RENDER_TARGET_VIEW_DESC rtv;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav;
  } desc;

  UINT Shader4ComponentMapping;

  UINT64 *firstElement = NULL;
  UINT *numElements = NULL;
  UINT *planeSlice = NULL;
  UINT *firstMip = NULL, *numMips = NULL;
  UINT *firstSlice = NULL, *numSlices = NULL;
};

D3D12_ROOT_PARAMETER1 cbvParam(D3D12_SHADER_VISIBILITY vis, UINT space, UINT reg);

D3D12_ROOT_PARAMETER1 constParam(D3D12_SHADER_VISIBILITY vis, UINT space, UINT reg, UINT num);

D3D12_ROOT_PARAMETER1 tableParam(
    D3D12_SHADER_VISIBILITY vis, D3D12_DESCRIPTOR_RANGE_TYPE type, UINT space, UINT basereg,
    UINT numreg, UINT descOffset = 0,
    D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                                         D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

D3D12_INDIRECT_ARGUMENT_DESC vbArg(UINT slot);
D3D12_INDIRECT_ARGUMENT_DESC ibArg();
D3D12_INDIRECT_ARGUMENT_DESC uavArg(UINT root);
D3D12_INDIRECT_ARGUMENT_DESC srvArg(UINT root);
D3D12_INDIRECT_ARGUMENT_DESC cbvArg(UINT root);
D3D12_INDIRECT_ARGUMENT_DESC constArg(UINT root, UINT wordOffset, UINT wordCount);
D3D12_INDIRECT_ARGUMENT_DESC drawArg();
D3D12_INDIRECT_ARGUMENT_DESC drawIndexedArg();
D3D12_INDIRECT_ARGUMENT_DESC dispatchArg();

#define CHECK_HR(expr)                                                                    \
  {                                                                                       \
    HRESULT hr = (expr);                                                                  \
    if(FAILED(hr))                                                                        \
    {                                                                                     \
      TEST_ERROR("Failed HRESULT at %s:%d (%x): %s", __FILE__, (int)__LINE__, hr, #expr); \
      DEBUG_BREAK();                                                                      \
      exit(1);                                                                            \
    }                                                                                     \
  }
