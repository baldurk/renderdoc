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

#define INITGUID

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "driver/dx/official/d3d12.h"
#include "driver/dx/official/dxgi1_4.h"
#include "driver/shaders/dxbc/dxbc_compile.h"
#include "serialise/serialiser.h"

// replay only class for handling marker regions
struct D3D12MarkerRegion
{
  D3D12MarkerRegion(ID3D12GraphicsCommandList *list, const std::string &marker);
  D3D12MarkerRegion(ID3D12CommandQueue *queue, const std::string &marker);
  ~D3D12MarkerRegion();

  static void Set(ID3D12GraphicsCommandList *list, const std::string &marker);
  static void Set(ID3D12CommandQueue *queue, const std::string &marker);

  static void Begin(ID3D12GraphicsCommandList *list, const std::string &marker);
  static void End(ID3D12GraphicsCommandList *list);
  static void Begin(ID3D12CommandQueue *queue, const std::string &marker);
  static void End(ID3D12CommandQueue *queue);

  ID3D12GraphicsCommandList *list = NULL;
  ID3D12CommandQueue *queue = NULL;
};

inline void SetObjName(ID3D12Object *obj, const std::string &utf8name)
{
  obj->SetName(StringFormat::UTF82Wide(utf8name).c_str());
}

TextureDim MakeTextureDim(D3D12_SRV_DIMENSION dim);
TextureDim MakeTextureDim(D3D12_RTV_DIMENSION dim);
TextureDim MakeTextureDim(D3D12_DSV_DIMENSION dim);
TextureDim MakeTextureDim(D3D12_UAV_DIMENSION dim);
AddressMode MakeAddressMode(D3D12_TEXTURE_ADDRESS_MODE addr);
CompareFunc MakeCompareFunc(D3D12_COMPARISON_FUNC func);
TextureFilter MakeFilter(D3D12_FILTER filter);
LogicOp MakeLogicOp(D3D12_LOGIC_OP op);
BlendMultiplier MakeBlendMultiplier(D3D12_BLEND blend, bool alpha);
BlendOp MakeBlendOp(D3D12_BLEND_OP op);
StencilOp MakeStencilOp(D3D12_STENCIL_OP op);

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the
// debugbreak.
#define D3D12NOTIMP(...)                                \
  {                                                     \
    static bool CONCAT(ignore, __LINE__) = false;       \
    if(!CONCAT(ignore, __LINE__))                       \
    {                                                   \
      RDCDEBUG("D3D12 not implemented - " __VA_ARGS__); \
      CONCAT(ignore, __LINE__) = true;                  \
    }                                                   \
  }

// uncomment this to cause every internal ExecuteCommandLists to immediately call
// FlushLists(), and to only submit one command list at once to narrow
// down the cause of device lost errors
#define SINGLE_FLUSH_VALIDATE OPTION_OFF

// uncomment this to get verbose debugging about when/where/why partial command
// buffer replay is happening
#define VERBOSE_PARTIAL_REPLAY OPTION_ON

ShaderStageMask ConvertVisibility(D3D12_SHADER_VISIBILITY ShaderVisibility);
UINT GetNumSubresources(ID3D12Device *dev, const D3D12_RESOURCE_DESC *desc);

class WrappedID3D12Device;

template <typename RealType>
class RefCounter12
{
private:
  unsigned int m_iRefcount;
  bool m_SelfDeleting;

protected:
  RealType *m_pReal;

  void SetSelfDeleting(bool selfDelete) { m_SelfDeleting = selfDelete; }
  // used for derived classes that need to soft ref but are handling their
  // own self-deletion

public:
  RefCounter12(RealType *real, bool selfDelete = true)
      : m_pReal(real), m_iRefcount(1), m_SelfDeleting(selfDelete)
  {
  }
  virtual ~RefCounter12() {}
  unsigned int GetRefCount() { return m_iRefcount; }
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(!m_pReal)
      return E_NOINTERFACE;

    return RefCountDXGIObject::WrapQueryInterface(m_pReal, riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef()
  {
    InterlockedIncrement(&m_iRefcount);
    return m_iRefcount;
  }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = InterlockedDecrement(&m_iRefcount);
    if(ret == 0 && m_SelfDeleting)
      delete this;
    return ret;
  }

  unsigned int SoftRef(WrappedID3D12Device *device)
  {
    unsigned int ret = AddRef();
    if(device)
      device->SoftRef();
    else
      RDCWARN("No device pointer, is a deleted resource being AddRef()d?");
    return ret;
  }

  unsigned int SoftRelease(WrappedID3D12Device *device)
  {
    unsigned int ret = Release();
    if(device)
      device->SoftRelease();
    else
      RDCWARN("No device pointer, is a deleted resource being Release()d?");
    return ret;
  }
};

struct D3D12RootSignatureParameter : D3D12_ROOT_PARAMETER1
{
  D3D12RootSignatureParameter()
  {
    ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // copy the POD ones first
    Constants.Num32BitValues = 0;
    Constants.RegisterSpace = 0;
    Constants.ShaderRegister = 0;
  }

  D3D12RootSignatureParameter(const D3D12RootSignatureParameter &other) { *this = other; }
  D3D12RootSignatureParameter &operator=(const D3D12RootSignatureParameter &other)
  {
    // copy first
    ParameterType = other.ParameterType;
    ShaderVisibility = other.ShaderVisibility;

    // copy the POD ones first
    Descriptor = other.Descriptor;
    Constants = other.Constants;

    ranges = other.ranges;

    // repoint ranges
    if(ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
    {
      DescriptorTable.NumDescriptorRanges = (UINT)ranges.size();
      DescriptorTable.pDescriptorRanges = &ranges[0];
    }
    return *this;
  }

  void MakeFrom(const D3D12_ROOT_PARAMETER1 &param, UINT &numSpaces)
  {
    ParameterType = param.ParameterType;
    ShaderVisibility = param.ShaderVisibility;

    // copy the POD ones first
    Descriptor = param.Descriptor;
    Constants = param.Constants;

    if(ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
    {
      ranges.resize(param.DescriptorTable.NumDescriptorRanges);
      for(size_t i = 0; i < ranges.size(); i++)
      {
        ranges[i] = param.DescriptorTable.pDescriptorRanges[i];

        numSpaces = RDCMAX(numSpaces, ranges[i].RegisterSpace + 1);
      }

      DescriptorTable.NumDescriptorRanges = (UINT)ranges.size();
      DescriptorTable.pDescriptorRanges = &ranges[0];
    }
    else if(ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
      numSpaces = RDCMAX(numSpaces, Constants.RegisterSpace + 1);
    }
    else
    {
      numSpaces = RDCMAX(numSpaces, Descriptor.RegisterSpace + 1);
    }
  }

  void MakeFrom(const D3D12_ROOT_PARAMETER &param, UINT &numSpaces)
  {
    ParameterType = param.ParameterType;
    ShaderVisibility = param.ShaderVisibility;

    // copy the POD ones first
    Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
    Descriptor.RegisterSpace = param.Descriptor.RegisterSpace;
    Descriptor.ShaderRegister = param.Descriptor.ShaderRegister;
    Constants = param.Constants;

    if(ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
    {
      ranges.resize(param.DescriptorTable.NumDescriptorRanges);
      for(size_t i = 0; i < ranges.size(); i++)
      {
        ranges[i].RangeType = param.DescriptorTable.pDescriptorRanges[i].RangeType;
        ranges[i].NumDescriptors = param.DescriptorTable.pDescriptorRanges[i].NumDescriptors;
        ranges[i].BaseShaderRegister = param.DescriptorTable.pDescriptorRanges[i].BaseShaderRegister;
        ranges[i].RegisterSpace = param.DescriptorTable.pDescriptorRanges[i].RegisterSpace;
        ranges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                          D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        ranges[i].OffsetInDescriptorsFromTableStart =
            param.DescriptorTable.pDescriptorRanges[i].OffsetInDescriptorsFromTableStart;

        numSpaces = RDCMAX(numSpaces, ranges[i].RegisterSpace + 1);
      }

      DescriptorTable.NumDescriptorRanges = (UINT)ranges.size();
      DescriptorTable.pDescriptorRanges = &ranges[0];
    }
    else if(ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
      numSpaces = RDCMAX(numSpaces, Constants.RegisterSpace + 1);
    }
    else
    {
      numSpaces = RDCMAX(numSpaces, Descriptor.RegisterSpace + 1);
    }
  }

  vector<D3D12_DESCRIPTOR_RANGE1> ranges;
};

struct D3D12RootSignature
{
  D3D12RootSignature() : numSpaces(0) {}
  uint32_t numSpaces;
  uint32_t dwordLength;

  D3D12_ROOT_SIGNATURE_FLAGS Flags;
  vector<D3D12RootSignatureParameter> params;
  vector<D3D12_STATIC_SAMPLER_DESC> samplers;
};

struct D3D12CommandSignature
{
  bool graphics;
  UINT numDraws;
  UINT ByteStride;
  vector<D3D12_INDIRECT_ARGUMENT_DESC> arguments;
};

#define IMPLEMENT_IUNKNOWN_WITH_REFCOUNTER_CUSTOMQUERY                \
  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter12::AddRef(); } \
  ULONG STDMETHODCALLTYPE Release() { return RefCounter12::Release(); }
#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) \
  ret func;                                      \
  bool CONCAT(Serialise_, func);

template <>
void Serialiser::Serialise(const char *name, D3D12_RESOURCE_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_COMMAND_QUEUE_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_SHADER_BYTECODE &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_GRAPHICS_PIPELINE_STATE_DESC &el);
template <>
void Serialiser::Deserialise(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *const el) const;
template <>
void Serialiser::Serialise(const char *name, D3D12_COMPUTE_PIPELINE_STATE_DESC &el);
template <>
void Serialiser::Deserialise(const D3D12_COMPUTE_PIPELINE_STATE_DESC *const el) const;
template <>
void Serialiser::Serialise(const char *name, D3D12_INDEX_BUFFER_VIEW &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_VERTEX_BUFFER_VIEW &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_STREAM_OUTPUT_BUFFER_VIEW &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_RESOURCE_BARRIER &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_HEAP_PROPERTIES &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_HEAP_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_DESCRIPTOR_HEAP_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_INDIRECT_ARGUMENT_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_COMMAND_SIGNATURE_DESC &el);
template <>
void Serialiser::Deserialise(const D3D12_COMMAND_SIGNATURE_DESC *const el) const;
template <>
void Serialiser::Serialise(const char *name, D3D12_QUERY_HEAP_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_SAMPLER_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_CONSTANT_BUFFER_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_SHADER_RESOURCE_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_RENDER_TARGET_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_DEPTH_STENCIL_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_UNORDERED_ACCESS_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_CLEAR_VALUE &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_TEXTURE_COPY_LOCATION &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_TILED_RESOURCE_COORDINATE &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_TILE_REGION_SIZE &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_DISCARD_REGION &el);
template <>
void Serialiser::Deserialise(const D3D12_DISCARD_REGION *const el) const;

struct D3D12Descriptor;
template <>
void Serialiser::Serialise(const char *name, D3D12Descriptor &el);

enum class D3D12Chunk : uint32_t
{
  CaptureBegin = (uint32_t)SystemChunk::FirstDriverChunk,
  CaptureEnd,
  CaptureScope,
  SetName,
  PushMarker,
  SetMarker,
  PopMarker,
  SetShaderDebugPath,
  CreateSwapBuffer,
  Device_CreateCommandQueue,
  Device_CreateCommandAllocator,
  Device_CreateGraphicsPipeline,
  Device_CreateComputePipeline,
  Device_CreateDescriptorHeap,
  Device_CreateRootSignature,
  Device_CreateCommandSignature,
  Device_CreateHeap,
  Device_CreateCommittedResource,
  Device_CreatePlacedResource,
  Device_CreateQueryHeap,
  Device_CreateFence,
  Device_CreateReservedResource,
  Device_CreateConstantBufferView,
  Device_CreateShaderResourceView,
  Device_CreateUnorderedAccessView,
  Device_CreateRenderTargetView,
  Device_CreateDepthStencilView,
  Device_CreateSampler,
  Device_CopyDescriptors,
  Device_CopyDescriptorsSimple,
  Queue_ExecuteCommandLists,
  Queue_Signal,
  Queue_Wait,
  Queue_UpdateTileMappings,
  Queue_CopyTileMappings,
  List_Close,
  List_Reset,
  List_ResourceBarrier,
  List_BeginQuery,
  List_EndQuery,
  List_ResolveQueryData,
  List_SetPredication,
  List_DrawIndexedInstanced,
  List_DrawInstanced,
  List_Dispatch,
  List_ExecuteIndirect,
  List_ExecuteBundle,
  List_CopyBufferRegion,
  List_CopyTextureRegion,
  List_CopyResource,
  List_ResolveSubresource,
  List_ClearRenderTargetView,
  List_ClearDepthStencilView,
  List_ClearUnorderedAccessViewUint,
  List_ClearUnorderedAccessViewFloat,
  List_DiscardResource,
  List_IASetPrimitiveTopology,
  List_IASetIndexBuffer,
  List_IASetVertexBuffers,
  List_SOSetTargets,
  List_RSSetViewports,
  List_RSSetScissorRects,
  List_SetPipelineState,
  List_SetDescriptorHeaps,
  List_OMSetRenderTargets,
  List_OMSetStencilRef,
  List_OMSetBlendFactor,
  List_SetGraphicsRootDescriptorTable,
  List_SetGraphicsRootSignature,
  List_SetGraphicsRoot32BitConstant,
  List_SetGraphicsRoot32BitConstants,
  List_SetGraphicsRootConstantBufferView,
  List_SetGraphicsRootShaderResourceView,
  List_SetGraphicsRootUnorderedAccessView,
  List_SetComputeRootDescriptorTable,
  List_SetComputeRootSignature,
  List_SetComputeRoot32BitConstant,
  List_SetComputeRoot32BitConstants,
  List_SetComputeRootConstantBufferView,
  List_SetComputeRootShaderResourceView,
  List_SetComputeRootUnorderedAccessView,
  List_CopyTiles,
  Resource_Unmap,
  Resource_WriteToSubresource,
  Max,
};
