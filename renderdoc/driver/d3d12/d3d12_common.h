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
  ~D3D12MarkerRegion();
  static void Set(ID3D12GraphicsCommandList *list, const std::string &marker);

  ID3D12GraphicsCommandList *list;
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

void MakeShaderReflection(DXBC::DXBCFile *dxbc, ShaderReflection *refl,
                          ShaderBindpointMapping *mapping);

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

#pragma region Chunks

#define D3D12_CHUNKS                                                                               \
  D3D12_CHUNK_MACRO(DEVICE_INIT = FIRST_CHUNK_ID, "ID3D12Device::Initialisation")                  \
  D3D12_CHUNK_MACRO(SET_RESOURCE_NAME, "ID3D12Object::SetName")                                    \
  D3D12_CHUNK_MACRO(RELEASE_RESOURCE, "IUnknown::Release")                                         \
  D3D12_CHUNK_MACRO(CREATE_SWAP_BUFFER, "IDXGISwapChain::GetBuffer")                               \
                                                                                                   \
  D3D12_CHUNK_MACRO(CAPTURE_SCOPE, "Capture")                                                      \
                                                                                                   \
  D3D12_CHUNK_MACRO(BEGIN_EVENT, "BeginEvent")                                                     \
  D3D12_CHUNK_MACRO(SET_MARKER, "SetMarker")                                                       \
  D3D12_CHUNK_MACRO(END_EVENT, "EndEvent")                                                         \
                                                                                                   \
  D3D12_CHUNK_MACRO(DEBUG_MESSAGES, "DebugMessageList")                                            \
                                                                                                   \
  D3D12_CHUNK_MACRO(CONTEXT_CAPTURE_HEADER, "ContextBegin")                                        \
  D3D12_CHUNK_MACRO(CONTEXT_CAPTURE_FOOTER, "ContextEnd")                                          \
                                                                                                   \
  D3D12_CHUNK_MACRO(SET_SHADER_DEBUG_PATH, "SetShaderDebugPath")                                   \
                                                                                                   \
  D3D12_CHUNK_MACRO(CREATE_COMMAND_QUEUE, "ID3D12Device::CreateCommandQueue")                      \
  D3D12_CHUNK_MACRO(CREATE_COMMAND_ALLOCATOR, "ID3D12Device::CreateCommandAllocator")              \
                                                                                                   \
  D3D12_CHUNK_MACRO(CREATE_GRAPHICS_PIPE, "ID3D12Device::CreateGraphicsPipeline")                  \
  D3D12_CHUNK_MACRO(CREATE_COMPUTE_PIPE, "ID3D12Device::CreateComputePipeline")                    \
  D3D12_CHUNK_MACRO(CREATE_DESCRIPTOR_HEAP, "ID3D12Device::CreateDescriptorHeap")                  \
  D3D12_CHUNK_MACRO(CREATE_ROOT_SIG, "ID3D12Device::CreateRootSignature")                          \
  D3D12_CHUNK_MACRO(CREATE_COMMAND_SIG, "ID3D12Device::CreateCommandSignature")                    \
                                                                                                   \
  D3D12_CHUNK_MACRO(CREATE_HEAP, "ID3D12Device::CreateHeap")                                       \
  D3D12_CHUNK_MACRO(CREATE_COMMITTED_RESOURCE, "ID3D12Device::CreateCommittedResource")            \
  D3D12_CHUNK_MACRO(CREATE_PLACED_RESOURCE, "ID3D12Device::CreatePlacedResource")                  \
                                                                                                   \
  D3D12_CHUNK_MACRO(CREATE_QUERY_HEAP, "ID3D12Device::CreateQueryHeap")                            \
  D3D12_CHUNK_MACRO(CREATE_FENCE, "ID3D12Device::CreateFence")                                     \
                                                                                                   \
  D3D12_CHUNK_MACRO(CLOSE_LIST, "ID3D12GraphicsCommandList::Close")                                \
  D3D12_CHUNK_MACRO(RESET_LIST, "ID3D12GraphicsCommandList::Reset")                                \
                                                                                                   \
  D3D12_CHUNK_MACRO(RESOURCE_BARRIER, "ID3D12GraphicsCommandList::ResourceBarrier")                \
                                                                                                   \
  D3D12_CHUNK_MACRO(MAP_DATA_WRITE, "ID3D12Resource::Unmap")                                       \
  D3D12_CHUNK_MACRO(WRITE_TO_SUB, "ID3D12Resource::WriteToSubresource")                            \
                                                                                                   \
  D3D12_CHUNK_MACRO(BEGIN_QUERY, "ID3D12GraphicsCommandList::BeginQuery")                          \
  D3D12_CHUNK_MACRO(END_QUERY, "ID3D12GraphicsCommandList::EndQuery")                              \
  D3D12_CHUNK_MACRO(RESOLVE_QUERY, "ID3D12GraphicsCommandList::ResolveQueryData")                  \
  D3D12_CHUNK_MACRO(SET_PREDICATION, "ID3D12GraphicsCommandList::SetPredication")                  \
                                                                                                   \
  D3D12_CHUNK_MACRO(DRAW_INDEXED_INST, "ID3D12GraphicsCommandList::DrawIndexedInstanced")          \
  D3D12_CHUNK_MACRO(DRAW_INST, "ID3D12GraphicsCommandList::DrawInstanced")                         \
  D3D12_CHUNK_MACRO(DISPATCH, "ID3D12GraphicsCommandList::Dispatch")                               \
  D3D12_CHUNK_MACRO(EXEC_INDIRECT, "ID3D12GraphicsCommandList::ExecuteIndirect")                   \
  D3D12_CHUNK_MACRO(EXEC_BUNDLE, "ID3D12GraphicsCommandList::ExecuteBundle")                       \
                                                                                                   \
  D3D12_CHUNK_MACRO(COPY_BUFFER, "ID3D12GraphicsCommandList::CopyBufferRegion")                    \
  D3D12_CHUNK_MACRO(COPY_TEXTURE, "ID3D12GraphicsCommandList::CopyTextureRegion")                  \
  D3D12_CHUNK_MACRO(COPY_RESOURCE, "ID3D12GraphicsCommandList::CopyResource")                      \
  D3D12_CHUNK_MACRO(RESOLVE_SUBRESOURCE, "ID3D12GraphicsCommandList::ResolveSubresource")          \
                                                                                                   \
  D3D12_CHUNK_MACRO(CLEAR_RTV, "ID3D12GraphicsCommandList::ClearRenderTargetView")                 \
  D3D12_CHUNK_MACRO(CLEAR_DSV, "ID3D12GraphicsCommandList::ClearDepthStencilView")                 \
  D3D12_CHUNK_MACRO(CLEAR_UAV_INT, "ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint")      \
  D3D12_CHUNK_MACRO(CLEAR_UAV_FLOAT, "ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat")   \
  D3D12_CHUNK_MACRO(DISCARD_RESOURCE, "ID3D12GraphicsCommandList::DiscardResource")                \
                                                                                                   \
  D3D12_CHUNK_MACRO(SET_TOPOLOGY, "ID3D12GraphicsCommandList::IASetPrimitiveTopology")             \
  D3D12_CHUNK_MACRO(SET_IBUFFER, "ID3D12GraphicsCommandList::IASetIndexBuffer")                    \
  D3D12_CHUNK_MACRO(SET_VBUFFERS, "ID3D12GraphicsCommandList::IASetVertexBuffers")                 \
  D3D12_CHUNK_MACRO(SET_SOTARGETS, "ID3D12GraphicsCommandList::SOSetTargets")                      \
  D3D12_CHUNK_MACRO(SET_VIEWPORTS, "ID3D12GraphicsCommandList::RSSetViewports")                    \
  D3D12_CHUNK_MACRO(SET_SCISSORS, "ID3D12GraphicsCommandList::RSSetScissors")                      \
  D3D12_CHUNK_MACRO(SET_PIPE, "ID3D12GraphicsCommandList::SetPipelineState")                       \
  D3D12_CHUNK_MACRO(SET_DESC_HEAPS, "ID3D12GraphicsCommandList::SetDescriptorHeaps")               \
  D3D12_CHUNK_MACRO(SET_RTVS, "ID3D12GraphicsCommandList::OMSetRenderTargets")                     \
  D3D12_CHUNK_MACRO(SET_STENCIL, "ID3D12GraphicsCommandList::OMSetStencilRef")                     \
  D3D12_CHUNK_MACRO(SET_BLENDFACTOR, "ID3D12GraphicsCommandList::OMSetBlendFactor")                \
                                                                                                   \
  D3D12_CHUNK_MACRO(SET_GFX_ROOT_TABLE,                                                            \
                    "ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable")                   \
  D3D12_CHUNK_MACRO(SET_GFX_ROOT_SIG, "ID3D12GraphicsCommandList::SetGraphicsRootSignature")       \
  D3D12_CHUNK_MACRO(SET_GFX_ROOT_CONST, "ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant") \
  D3D12_CHUNK_MACRO(SET_GFX_ROOT_CONSTS,                                                           \
                    "ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants")                    \
  D3D12_CHUNK_MACRO(SET_GFX_ROOT_CBV,                                                              \
                    "ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView")                \
  D3D12_CHUNK_MACRO(SET_GFX_ROOT_SRV,                                                              \
                    "ID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView")                \
  D3D12_CHUNK_MACRO(SET_GFX_ROOT_UAV,                                                              \
                    "ID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView")               \
                                                                                                   \
  D3D12_CHUNK_MACRO(SET_COMP_ROOT_TABLE,                                                           \
                    "ID3D12GraphicsCommandList::SetComputeRootDescriptorTable")                    \
  D3D12_CHUNK_MACRO(SET_COMP_ROOT_SIG, "ID3D12GraphicsCommandList::SetComputeRootSignature")       \
  D3D12_CHUNK_MACRO(SET_COMP_ROOT_CONST, "ID3D12GraphicsCommandList::SetComputeRoot32BitConstant") \
  D3D12_CHUNK_MACRO(SET_COMP_ROOT_CONSTS,                                                          \
                    "ID3D12GraphicsCommandList::SetComputeRoot32BitConstants")                     \
  D3D12_CHUNK_MACRO(SET_COMP_ROOT_CBV,                                                             \
                    "ID3D12GraphicsCommandList::SetComputeRootConstantBufferView")                 \
  D3D12_CHUNK_MACRO(SET_COMP_ROOT_SRV,                                                             \
                    "ID3D12GraphicsCommandList::SetComputeRootShaderResourceView")                 \
  D3D12_CHUNK_MACRO(SET_COMP_ROOT_UAV,                                                             \
                    "ID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView")                \
                                                                                                   \
  D3D12_CHUNK_MACRO(DYN_DESC_WRITE, "Dynamic descriptor write")                                    \
  D3D12_CHUNK_MACRO(DYN_DESC_COPIES, "Dynamic descriptor copies")                                  \
                                                                                                   \
  D3D12_CHUNK_MACRO(EXECUTE_CMD_LISTS, "ID3D12GraphicsCommandQueue::ExecuteCommandLists")          \
  D3D12_CHUNK_MACRO(SIGNAL, "ID3D12GraphicsCommandQueue::Signal")                                  \
  D3D12_CHUNK_MACRO(WAIT, "ID3D12GraphicsCommandQueue::Wait")                                      \
                                                                                                   \
  D3D12_CHUNK_MACRO(CREATE_RESERVED_RESOURCE, "ID3D12Device::CreateReservedResource")              \
  D3D12_CHUNK_MACRO(COPY_TILES, "ID3D12GraphicsCommandList::CopyTiles")                            \
  D3D12_CHUNK_MACRO(UPDATE_TILE_MAPPINGS, "ID3D12GraphicsCommandQueue::UpdateTileMappings")        \
  D3D12_CHUNK_MACRO(COPY_TILE_MAPPINGS, "ID3D12GraphicsCommandQueue::CopyTileMappings")            \
                                                                                                   \
  D3D12_CHUNK_MACRO(NUM_D3D12_CHUNKS, "")

enum D3D12ChunkType
{
#undef D3D12_CHUNK_MACRO
#define D3D12_CHUNK_MACRO(enum, string) enum,

  D3D12_CHUNKS
};

#pragma endregion Chunks
