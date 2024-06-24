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

#define INITGUID

#include "core/core.h"
#include "driver/dx/official/D3D12Downlevel.h"
#include "driver/dx/official/d3d12.h"
#include "driver/dx/official/dxgi1_4.h"
#include "driver/shaders/dxbc/dxbc_compile.h"
#include "serialise/serialiser.h"

// we need to use the most-derived native interface all over the place. To make things easier when
// new versions come out we typedef it here when we don't need the specific interface
using ID3D12GraphicsCommandListX = ID3D12GraphicsCommandList9;

// replay only class for handling marker regions
struct D3D12MarkerRegion
{
  D3D12MarkerRegion(ID3D12GraphicsCommandList *list, const rdcstr &marker);
  D3D12MarkerRegion(ID3D12CommandQueue *queue, const rdcstr &marker);
  ~D3D12MarkerRegion();

  static void Set(ID3D12GraphicsCommandList *list, const rdcstr &marker);
  static void Set(ID3D12CommandQueue *queue, const rdcstr &marker);

  static void Begin(ID3D12GraphicsCommandList *list, const rdcstr &marker);
  static void End(ID3D12GraphicsCommandList *list);
  static void Begin(ID3D12CommandQueue *queue, const rdcstr &marker);
  static void End(ID3D12CommandQueue *queue);

  ID3D12GraphicsCommandList *list = NULL;
  ID3D12CommandQueue *queue = NULL;
};

struct D3D12DevConfiguration
{
  ID3D12SDKConfiguration1 *sdkconfig = NULL;
  ID3D12DeviceFactory *devfactory = NULL;
  ID3D12DeviceConfiguration *devconfig = NULL;
  ID3D12Debug *debug = NULL;
};

bool EnableD3D12DebugLayer(D3D12DevConfiguration *devConfig,
                           PFN_D3D12_GET_DEBUG_INTERFACE getDebugInterface);
HRESULT EnumAdapterByLuid(IDXGIFactory1 *factory, LUID luid, IDXGIAdapter **pAdapter);

D3D12DevConfiguration *D3D12_PrepareReplaySDKVersion(bool untrustedCapture, UINT SDKVersion,
                                                     bytebuf d3d12core, bytebuf d3d12sdklayers,
                                                     HMODULE d3d12lib);
void D3D12_CleanupReplaySDK();

inline void SetObjName(ID3D12Object *obj, const rdcstr &utf8name)
{
  if(obj)
    obj->SetName(StringFormat::UTF82Wide(utf8name).c_str());
}

#define PIX_EVENT_UNICODE_VERSION 0
#define PIX_EVENT_ANSI_VERSION 1
#define PIX_EVENT_PIX3BLOB_VERSION 2

rdcstr PIX3DecodeEventString(const UINT64 *pData, UINT64 &color);

inline rdcstr DecodeMarkerString(UINT Metadata, const void *pData, UINT Size, UINT64 &color)
{
  rdcstr MarkerText = "";

  // There may be a space appended to the marker string - see D3D12MarkerRegion::Begin
  // If we encounter this extra space (or a null terminator), remove it.
  if(Metadata == PIX_EVENT_UNICODE_VERSION)
  {
    const wchar_t *w = (const wchar_t *)pData;
    MarkerText = StringFormat::Wide2UTF8(rdcwstr(w, Size / sizeof(wchar_t)));
    if(!MarkerText.empty() && (MarkerText.back() == ' ' || MarkerText.back() == 0))
      MarkerText.pop_back();
    color = 0;
  }
  else if(Metadata == PIX_EVENT_ANSI_VERSION)
  {
    const char *c = (const char *)pData;
    MarkerText = rdcstr(c, Size);
    if(!MarkerText.empty() && (MarkerText.back() == ' ' || MarkerText.back() == 0))
      MarkerText.pop_back();
    color = 0;
  }
  else if(Metadata == PIX_EVENT_PIX3BLOB_VERSION)
  {
    MarkerText = PIX3DecodeEventString((UINT64 *)pData, color);
  }
  else
  {
    RDCERR("Unexpected/unsupported Metadata value %u in marker text", Metadata);
  }

  return MarkerText;
}

FloatVector DecodePIXColor(UINT64 Color);

TextureType MakeTextureDim(D3D12_SRV_DIMENSION dim);
TextureType MakeTextureDim(D3D12_RTV_DIMENSION dim);
TextureType MakeTextureDim(D3D12_DSV_DIMENSION dim);
TextureType MakeTextureDim(D3D12_UAV_DIMENSION dim);
AddressMode MakeAddressMode(D3D12_TEXTURE_ADDRESS_MODE addr);
CompareFunction MakeCompareFunc(D3D12_COMPARISON_FUNC func);
TextureFilter MakeFilter(D3D12_FILTER filter);
DescriptorFlags MakeDescriptorFlags(D3D12_BUFFER_SRV_FLAGS flags);
DescriptorFlags MakeDescriptorFlags(D3D12_BUFFER_UAV_FLAGS flags);
LogicOperation MakeLogicOp(D3D12_LOGIC_OP op);
BlendMultiplier MakeBlendMultiplier(D3D12_BLEND blend, bool alpha);
BlendOperation MakeBlendOp(D3D12_BLEND_OP op);
StencilOperation MakeStencilOp(D3D12_STENCIL_OP op);

uint32_t ArgumentTypeByteSize(const D3D12_INDIRECT_ARGUMENT_DESC &arg);

// wrapper around D3D12_RESOURCE_STATES and D3D12_BARRIER_LAYOUT to handle resources that could be
// in either and varying support
struct D3D12ResourceLayout
{
  D3D12ResourceLayout() : value(0)
  {
    RDCCOMPILE_ASSERT(sizeof(*this) == sizeof(D3D12_BARRIER_LAYOUT),
                      "Layout/state wrapper is wrongly sized");
    RDCCOMPILE_ASSERT(sizeof(*this) == sizeof(D3D12_RESOURCE_STATES),
                      "Layout/state wrapper is wrongly sized");
  }
  static D3D12ResourceLayout FromStates(D3D12_RESOURCE_STATES s)
  {
    return D3D12ResourceLayout((uint32_t)s);
  }
  static D3D12ResourceLayout FromLayout(D3D12_BARRIER_LAYOUT s)
  {
    return D3D12ResourceLayout(uint32_t(s) | LayoutBit);
  }
  bool IsLayout() const { return (value & LayoutBit) != 0; }
  bool IsStates() const { return (value & LayoutBit) == 0; }
  D3D12_RESOURCE_STATES ToStates() const { return D3D12_RESOURCE_STATES(value & ~LayoutBit); }
  D3D12_BARRIER_LAYOUT ToLayout() const
  {
    if(value == D3D12_BARRIER_LAYOUT_UNDEFINED)
      return D3D12_BARRIER_LAYOUT_UNDEFINED;
    return D3D12_BARRIER_LAYOUT(value & ~LayoutBit);
  }
  bool operator==(const D3D12ResourceLayout &o) const { return value == o.value; }
  bool operator!=(const D3D12ResourceLayout &o) const { return !(*this == o); }
private:
  explicit D3D12ResourceLayout(uint32_t v) : value(v) {}
  // layouts are an enum so this bit should hopefully never be used. Note that LAYOUT_UNDEFINED is
  // ~0U and annoyingly it has to be specified for buffers (instead of D3D12_BARRIER_LAYOUT_COMMON)
  // so we need to special-case it.
  // states are a bitmask but they only use just over 6 hex digits so far, and we assume they won't
  // be extended (or not by much).
  // We set the bit for layouts and not for states so that we can serialise this
  // backwards-compatibly and replace any serialised D3D12_RESOURCE_STATES and have them appear as
  // the correct enum.
  static constexpr uint32_t LayoutBit = 0x80000000U;
  uint32_t value;
};

typedef rdcarray<D3D12ResourceLayout> SubresourceStateVector;

struct BarrierSet
{
  enum AccessType
  {
    SRVAccess,
    CopySourceAccess,
    CopyDestAccess,
    ResolveSourceAccess,
  };

  // set up the barriers to barrier this resource a given type of access
  void Configure(ID3D12Resource *res, const SubresourceStateVector &states, AccessType access);

  // apply the barrier set onto this list
  void Apply(ID3D12GraphicsCommandListX *list);

  // unapply the barrier set - used for when we are barrier'ing a resource, doing some work, then
  // barrier'ing it back to the original state
  void Unapply(ID3D12GraphicsCommandListX *list);

  void clear()
  {
    barriers.clear();
    newBarriers.clear();
    newToOldBarriers.clear();
  }
  bool empty() const { return barriers.empty() && newBarriers.empty(); }
  void swap(BarrierSet &other)
  {
    barriers.swap(other.barriers);
    newBarriers.swap(other.newBarriers);
  }

  rdcarray<D3D12_RESOURCE_BARRIER> barriers;
  rdcarray<D3D12_TEXTURE_BARRIER> newBarriers;
  // these are used specifically when we need to record a transition from a new layout to an old
  // state. Because we execute barriers before newBarriers we can do the other way without this
  rdcarray<D3D12_RESOURCE_BARRIER> newToOldBarriers;
};

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

// uncomment this to get verbose debugging about when/where/why partial command
// buffer replay is happening
#define VERBOSE_PARTIAL_REPLAY OPTION_OFF

D3D12_DEPTH_STENCIL_DESC2 Upconvert(const D3D12_DEPTH_STENCIL_DESC1 &desc);
D3D12_RASTERIZER_DESC2 Upconvert(const D3D12_RASTERIZER_DESC &desc);
D3D12_SAMPLER_DESC2 ConvertStaticSampler(const D3D12_STATIC_SAMPLER_DESC1 &samp);

ShaderStageMask ConvertVisibility(D3D12_SHADER_VISIBILITY ShaderVisibility);
UINT GetNumSubresources(ID3D12Device *dev, const D3D12_RESOURCE_DESC *desc);
inline UINT GetNumSubresources(ID3D12Device *dev, const D3D12_RESOURCE_DESC1 *desc)
{
  // D3D12_RESOURCE_DESC is the same as the start of D3D12_RESOURCE_DESC1
  D3D12_RESOURCE_DESC desc0;
  memcpy(&desc0, desc, sizeof(desc0));
  return GetNumSubresources(dev, &desc0);
}

UINT D3D12CalcSubresource(UINT MipSlice, UINT ArraySlice, UINT PlaneSlice, UINT MipLevels,
                          UINT ArraySize);

class WrappedID3D12Device;

template <typename RealType>
class RefCounter12
{
private:
  unsigned int m_iRefcount;
  unsigned int m_InternalRefcount : 31;
  unsigned int m_SelfDeleting : 1;

protected:
  RealType *m_pReal;

  void SetSelfDeleting(bool selfDelete) { m_SelfDeleting = selfDelete ? 1 : 0; }
  // used for derived classes that need to soft ref but are handling their
  // own self-deletion

public:
  RefCounter12(RealType *real, bool selfDelete = true)
      : m_pReal(real), m_iRefcount(1), m_InternalRefcount(0), m_SelfDeleting(selfDelete ? 1 : 0)
  {
  }
  virtual ~RefCounter12() {}
  unsigned int GetRefCount() { return m_iRefcount; }
  // some applications wrongly check refcount return values and expect them to
  // match D3D's values. When we have some internal refs we need to hide, we
  // add them here and they're subtracted from return values
  void AddInternalRef() { m_InternalRefcount++; }
  void ReleaseInternalRef() { m_InternalRefcount--; }
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    return QueryInterface("ID3D12Object", riid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(const char *ifaceName, REFIID riid, void **ppvObject)
  {
    if(!m_pReal)
      return E_NOINTERFACE;

    return RefCountDXGIObject::WrapQueryInterface(m_pReal, ifaceName, riid, ppvObject);
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

    if(ret >= m_InternalRefcount)
      ret -= m_InternalRefcount;

    return ret;
  }

  unsigned int SoftRelease(WrappedID3D12Device *device)
  {
    unsigned int ret = Release();
    if(device)
      device->SoftRelease();
    else
      RDCWARN("No device pointer, is a deleted resource being Release()d?");

    if(ret == 0)
      return ret;

    if(ret >= m_InternalRefcount)
      ret -= m_InternalRefcount;

    return ret;
  }
};

class WrappedID3D12DeviceConfiguration : public ID3D12DeviceConfiguration
{
private:
  ID3D12DeviceConfiguration *m_pReal = NULL;
  IUnknown *wrappedParent = NULL;
public:
  WrappedID3D12DeviceConfiguration(IUnknown *real, IUnknown *wrapped)
  {
    wrappedParent = wrapped;

    if(real)
    {
      HRESULT hr = real->QueryInterface(__uuidof(ID3D12DeviceConfiguration), (void **)&m_pReal);
      if(FAILED(hr))
        SAFE_RELEASE(m_pReal);
    }
  }

  ~WrappedID3D12DeviceConfiguration() { SAFE_RELEASE(m_pReal); }

  bool IsValid() { return m_pReal != NULL; }

  //////////////////////////////
  // Implement IUnknown
  ULONG STDMETHODCALLTYPE AddRef() { return wrappedParent->AddRef(); }
  ULONG STDMETHODCALLTYPE Release() { return wrappedParent->Release(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(IUnknown))
    {
      *ppvObject = (IUnknown *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D12DeviceConfiguration) && m_pReal)
    {
      *ppvObject = (ID3D12DeviceConfiguration *)this;
      AddRef();
      return S_OK;
    }

    return wrappedParent->QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // Implement ID3D12DeviceConfiguration
#if defined(_MSC_VER) || !defined(_WIN32)
  virtual D3D12_DEVICE_CONFIGURATION_DESC STDMETHODCALLTYPE GetDesc(void)
  {
    return m_pReal->GetDesc();
  }
#else
  virtual D3D12_DEVICE_CONFIGURATION_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_DEVICE_CONFIGURATION_DESC *RetVal)
  {
    return m_pReal->GetDesc(RetVal);
  }
#endif

  virtual HRESULT STDMETHODCALLTYPE GetEnabledExperimentalFeatures(_Out_writes_(NumGuids)
                                                                       GUID *pGuids,
                                                                   UINT NumGuids)
  {
    return m_pReal->GetEnabledExperimentalFeatures(pGuids, NumGuids);
  }

  virtual HRESULT STDMETHODCALLTYPE SerializeVersionedRootSignature(
      _In_ const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pDesc, _COM_Outptr_ ID3DBlob **ppResult,
      _Always_(_Outptr_opt_result_maybenull_) ID3DBlob **ppError)
  {
    return m_pReal->SerializeVersionedRootSignature(pDesc, ppResult, ppError);
  }

  virtual HRESULT STDMETHODCALLTYPE
  CreateVersionedRootSignatureDeserializer(_In_reads_bytes_(Size) const void *pBlob, SIZE_T Size,
                                           REFIID riid, _COM_Outptr_ void **ppvDeserializer)
  {
    return m_pReal->CreateVersionedRootSignatureDeserializer(pBlob, Size, riid, ppvDeserializer);
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

  void MakeFrom(const D3D12_ROOT_PARAMETER1 &param, UINT &maxSpaceIndex)
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

        maxSpaceIndex = RDCMAX(maxSpaceIndex, ranges[i].RegisterSpace + 1);
      }

      DescriptorTable.NumDescriptorRanges = (UINT)ranges.size();
      DescriptorTable.pDescriptorRanges = &ranges[0];
    }
    else if(ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
      maxSpaceIndex = RDCMAX(maxSpaceIndex, Constants.RegisterSpace + 1);
    }
    else
    {
      maxSpaceIndex = RDCMAX(maxSpaceIndex, Descriptor.RegisterSpace + 1);
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

  rdcarray<D3D12_DESCRIPTOR_RANGE1> ranges;
};

DECLARE_REFLECTION_STRUCT(D3D12RootSignatureParameter);

struct D3D12RootSignature
{
  uint32_t maxSpaceIndex = 0;
  uint32_t dwordLength = 0;

  D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
  rdcarray<D3D12RootSignatureParameter> Parameters;
  rdcarray<D3D12_STATIC_SAMPLER_DESC1> StaticSamplers;
};

DECLARE_REFLECTION_STRUCT(D3D12RootSignature);

struct D3D12CommandSignature
{
  bool graphics = true;
  bool raytraced = false;
  UINT ByteStride = 0;
  UINT PackedByteSize = 0;
  rdcarray<D3D12_INDIRECT_ARGUMENT_DESC> arguments;
};

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                              \
  template <typename SerialiserType>                  \
  bool CONCAT(Serialise_, func(SerialiserType &ser, __VA_ARGS__));

#define INSTANTIATE_FUNCTION_SERIALISED(ret, parent, func, ...)                     \
  template bool parent::CONCAT(Serialise_, func(ReadSerialiser &ser, __VA_ARGS__)); \
  template bool parent::CONCAT(Serialise_, func(WriteSerialiser &ser, __VA_ARGS__));

#define CACHE_THREAD_SERIALISER() WriteSerialiser &ser = GetThreadSerialiser();

#define SERIALISE_TIME_CALL(...)                                                                \
  {                                                                                             \
    WriteSerialiser &ser = GetThreadSerialiser();                                               \
    ser.ChunkMetadata().timestampMicro = Timing::GetTick();                                     \
    __VA_ARGS__;                                                                                \
    ser.ChunkMetadata().durationMicro = Timing::GetTick() - ser.ChunkMetadata().timestampMicro; \
  }

// A handy macros to say "is the serialiser reading and we're doing replay-mode stuff?"
// The reason we check both is that checking the first allows the compiler to eliminate the other
// path at compile-time, and the second because we might be just struct-serialising in which case we
// should be doing no work to restore states.
// Writing is unambiguously during capture mode, so we don't have to check both in that case.
#define IsReplayingAndReading() (ser.IsReading() && IsReplayMode(m_State))

// this is special - these serialise overloads will fetch the ID during capture, serialise the ID
// directly as-if it were the original type, then on replay load up the resource if available.
// Really this is only one type of serialisation, but we declare a couple of overloads to account
// for resources being accessed through different interfaces in different functions
#define SERIALISE_D3D_INTERFACES()                 \
  SERIALISE_INTERFACE(ID3D12Object);               \
  SERIALISE_INTERFACE(ID3D12DeviceChild);          \
  SERIALISE_INTERFACE(ID3D12Pageable);             \
  SERIALISE_INTERFACE(ID3D12CommandList);          \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList);  \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList1); \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList2); \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList3); \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList4); \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList5); \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList6); \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList7); \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList8); \
  SERIALISE_INTERFACE(ID3D12GraphicsCommandList9); \
  SERIALISE_INTERFACE(ID3D12RootSignature);        \
  SERIALISE_INTERFACE(ID3D12Resource);             \
  SERIALISE_INTERFACE(ID3D12QueryHeap);            \
  SERIALISE_INTERFACE(ID3D12PipelineState);        \
  SERIALISE_INTERFACE(ID3D12Heap);                 \
  SERIALISE_INTERFACE(ID3D12Fence);                \
  SERIALISE_INTERFACE(ID3D12DescriptorHeap);       \
  SERIALISE_INTERFACE(ID3D12CommandSignature);     \
  SERIALISE_INTERFACE(ID3D12CommandQueue);         \
  SERIALISE_INTERFACE(ID3D12CommandAllocator);     \
  SERIALISE_INTERFACE(ID3D12StateObject);

#define SERIALISE_INTERFACE(iface) DECLARE_REFLECTION_STRUCT(iface *)

SERIALISE_D3D_INTERFACES();

// a thin utility wrapper around a UINT64, that serialises a BufferLocation as an Id + Offset.
struct D3D12BufferLocation
{
  D3D12BufferLocation() : Location(0) {}
  D3D12BufferLocation(UINT64 l) : Location(l) {}
  operator UINT64() const { return Location; }
  UINT64 Location;
};

DECLARE_REFLECTION_STRUCT(D3D12BufferLocation);

DECLARE_REFLECTION_STRUCT(D3D12_CPU_DESCRIPTOR_HANDLE);
DECLARE_REFLECTION_STRUCT(D3D12_GPU_DESCRIPTOR_HANDLE);

inline bool operator==(const D3D12_CPU_DESCRIPTOR_HANDLE &l, const D3D12_CPU_DESCRIPTOR_HANDLE &r)
{
  return l.ptr == r.ptr;
}

struct D3D12_UNWRAPPED_STATE_OBJECT_DESC : public D3D12_STATE_OBJECT_DESC
{
  D3D12_UNWRAPPED_STATE_OBJECT_DESC(const D3D12_STATE_OBJECT_DESC &wrappedDesc);

private:
  rdcarray<D3D12_STATE_SUBOBJECT> subobjects;
  rdcarray<D3D12_GLOBAL_ROOT_SIGNATURE> unwrappedRootsigObjs;
  rdcarray<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> rebasedAssocs;
  rdcarray<D3D12_EXISTING_COLLECTION_DESC> unwrappedCollObjs;
};

// expanded version of D3D12_GRAPHICS_PIPELINE_STATE_DESC / D3D12_COMPUTE_PIPELINE_STATE_DESC with
// all subobjects. No enums suitable to make this a stream though.
struct D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC
{
  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC() = default;

  // construct from the normal graphics descriptor
  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &graphics);

  // construct from the normal compute descriptor
  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(const D3D12_COMPUTE_PIPELINE_STATE_DESC &compute);

  // construct from the stream descriptor
  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(const D3D12_PIPELINE_STATE_STREAM_DESC &stream);

  bool errored = false;

  // graphics properties
  ID3D12RootSignature *pRootSignature = NULL;
  D3D12_SHADER_BYTECODE VS = {};
  D3D12_SHADER_BYTECODE PS = {};
  D3D12_SHADER_BYTECODE DS = {};
  D3D12_SHADER_BYTECODE HS = {};
  D3D12_SHADER_BYTECODE GS = {};
  D3D12_SHADER_BYTECODE AS = {};
  D3D12_SHADER_BYTECODE MS = {};
  D3D12_STREAM_OUTPUT_DESC StreamOutput = {};
  D3D12_BLEND_DESC BlendState = {};
  UINT SampleMask = 0;
  D3D12_RASTERIZER_DESC2 RasterizerState = {};
  D3D12_DEPTH_STENCIL_DESC2 DepthStencilState = {};
  D3D12_INPUT_LAYOUT_DESC InputLayout = {};
  D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
  D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
  D3D12_RT_FORMAT_ARRAY RTVFormats = {};
  DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;
  DXGI_SAMPLE_DESC SampleDesc = {};
  UINT NodeMask = 0;
  D3D12_CACHED_PIPELINE_STATE CachedPSO = {};
  D3D12_PIPELINE_STATE_FLAGS Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  D3D12_VIEW_INSTANCING_DESC ViewInstancing = {};

  // unique compute properties (many are duplicated above)
  D3D12_SHADER_BYTECODE CS = {};
};

DECLARE_REFLECTION_STRUCT(D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC);

// subobject headers have to be aligned to pointer boundaries
#define SUBOBJECT_HEADER(subobj)                                               \
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE alignas(void *) CONCAT(header, subobj) = \
      CONCAT(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_, subobj);

// similar to D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC but a packed version that can be passed
// through a D3D12_PIPELINE_STATE_STREAM_DESC. Keeps members private to ensure it's only used for
// that.
struct D3D12_PACKED_PIPELINE_STATE_STREAM_DESC
{
public:
  D3D12_PACKED_PIPELINE_STATE_STREAM_DESC() = default;
  D3D12_PACKED_PIPELINE_STATE_STREAM_DESC(const D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &expanded)
  {
    *this = expanded;
  }

  // unwrap in place
  void Unwrap();

  // initialise from an expanded descriptor
  D3D12_PACKED_PIPELINE_STATE_STREAM_DESC &operator=(
      const D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &expanded);

  const D3D12_PIPELINE_STATE_STREAM_DESC *AsDescStream()
  {
    if(m_ComputeStreamData.CS.BytecodeLength > 0)
    {
      m_StreamDesc.pPipelineStateSubobjectStream = &m_ComputeStreamData;
      m_StreamDesc.SizeInBytes = sizeof(m_ComputeStreamData);
      return &m_StreamDesc;
    }
    else
    {
      m_StreamDesc.pPipelineStateSubobjectStream = &m_GraphicsStreamData;
      m_StreamDesc.SizeInBytes =
          offsetof(GraphicsStreamData, VariableVersionedData) + m_VariableVersionedDataLength;
      return &m_StreamDesc;
    }
  }

  size_t GetStageCount() { return 8; }
  D3D12_SHADER_BYTECODE &GetStage(size_t i)
  {
    D3D12_SHADER_BYTECODE *stages[] = {
        &m_GraphicsStreamData.VS,
        &m_GraphicsStreamData.HS,
        &m_GraphicsStreamData.DS,
        &m_GraphicsStreamData.GS,
        &m_GraphicsStreamData.PS,
        &m_ComputeStreamData.CS,
        &AS,
        &MS,
    };
    return *stages[i];
  }

private:
  struct GraphicsStreamData
  {
    // graphics properties
    SUBOBJECT_HEADER(ROOT_SIGNATURE);
    ID3D12RootSignature *pRootSignature = NULL;
    SUBOBJECT_HEADER(INPUT_LAYOUT);
    D3D12_INPUT_LAYOUT_DESC InputLayout = {};
    SUBOBJECT_HEADER(VS);
    D3D12_SHADER_BYTECODE VS = {};
    SUBOBJECT_HEADER(PS);
    D3D12_SHADER_BYTECODE PS = {};
    SUBOBJECT_HEADER(DS);
    D3D12_SHADER_BYTECODE DS = {};
    SUBOBJECT_HEADER(HS);
    D3D12_SHADER_BYTECODE HS = {};
    SUBOBJECT_HEADER(GS);
    D3D12_SHADER_BYTECODE GS = {};
    SUBOBJECT_HEADER(STREAM_OUTPUT);
    D3D12_STREAM_OUTPUT_DESC StreamOutput = {};
    SUBOBJECT_HEADER(CACHED_PSO);
    D3D12_CACHED_PIPELINE_STATE CachedPSO = {};
    SUBOBJECT_HEADER(VIEW_INSTANCING);
    D3D12_VIEW_INSTANCING_DESC ViewInstancing = {};
    SUBOBJECT_HEADER(RENDER_TARGET_FORMATS);
    D3D12_RT_FORMAT_ARRAY RTVFormats = {};
    SUBOBJECT_HEADER(DEPTH_STENCIL_FORMAT);
    DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;
    SUBOBJECT_HEADER(PRIMITIVE_TOPOLOGY);
    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    SUBOBJECT_HEADER(IB_STRIP_CUT_VALUE);
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    SUBOBJECT_HEADER(NODE_MASK);
    UINT NodeMask = 0;
    SUBOBJECT_HEADER(SAMPLE_MASK);
    UINT SampleMask = 0;
    SUBOBJECT_HEADER(FLAGS);
    D3D12_PIPELINE_STATE_FLAGS Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    SUBOBJECT_HEADER(BLEND);
    D3D12_BLEND_DESC BlendState = {};
#if ENABLED(RDOC_X64)
    UINT pad0;
#endif
    SUBOBJECT_HEADER(SAMPLE_DESC);
    DXGI_SAMPLE_DESC SampleDesc = {};
#if ENABLED(RDOC_X64)
    UINT pad1;
#endif
    // since data must be tightly packed, the final structs that are versioned and may not be
    // supported are pushed in here
    alignas(void *) byte VariableVersionedData[
        // depth-stencil is versioned for separate stencil masks
        sizeof(D3D12_DEPTH_STENCIL_DESC2) + sizeof(void *) +
        // rasterization is versioned for line raster mode
        sizeof(D3D12_RASTERIZER_DESC2) + sizeof(void *) +
        // AS ...
        sizeof(D3D12_SHADER_BYTECODE) + sizeof(void *) +
        // ... and MS are optional
        sizeof(D3D12_SHADER_BYTECODE) + sizeof(void *)];
  } m_GraphicsStreamData;

  D3D12_SHADER_BYTECODE AS = {};
  D3D12_SHADER_BYTECODE MS = {};

  size_t m_VariableVersionedDataLength;

  struct
  {
    // compute properties
    SUBOBJECT_HEADER(ROOT_SIGNATURE);
    ID3D12RootSignature *pRootSignature = NULL;
    SUBOBJECT_HEADER(CS);
    D3D12_SHADER_BYTECODE CS = {};
    SUBOBJECT_HEADER(NODE_MASK);
    UINT NodeMask = 0;
    SUBOBJECT_HEADER(CACHED_PSO);
    D3D12_CACHED_PIPELINE_STATE CachedPSO = {};
    SUBOBJECT_HEADER(FLAGS);
    D3D12_PIPELINE_STATE_FLAGS Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  } m_ComputeStreamData;

  D3D12_PIPELINE_STATE_STREAM_DESC m_StreamDesc;
};

#undef SUBOBJECT_HEADER

DECLARE_REFLECTION_ENUM(D3D12_COMMAND_LIST_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_HEAP_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_CPU_PAGE_PROPERTY);
DECLARE_REFLECTION_ENUM(D3D12_MEMORY_POOL);
DECLARE_REFLECTION_ENUM(D3D12_QUERY_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_QUERY_HEAP_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_DESCRIPTOR_HEAP_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_DESCRIPTOR_HEAP_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_PREDICATION_OP);
DECLARE_REFLECTION_ENUM(D3D12_CLEAR_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_DSV_DIMENSION);
DECLARE_REFLECTION_ENUM(D3D12_UAV_DIMENSION);
DECLARE_REFLECTION_ENUM(D3D12_SRV_DIMENSION);
DECLARE_REFLECTION_ENUM(D3D12_RTV_DIMENSION);
DECLARE_REFLECTION_ENUM(D3D12_DSV_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_BUFFER_SRV_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_BUFFER_UAV_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_RESOURCE_STATES);
DECLARE_REFLECTION_ENUM(D3D12_HEAP_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_FENCE_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_PRIMITIVE_TOPOLOGY_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_PRIMITIVE_TOPOLOGY);
DECLARE_REFLECTION_ENUM(D3D12_INDIRECT_ARGUMENT_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_PIPELINE_STATE_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_RESOURCE_BARRIER_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_RESOURCE_BARRIER_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_INPUT_CLASSIFICATION);
DECLARE_REFLECTION_ENUM(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE);
DECLARE_REFLECTION_ENUM(D3D12_RESOURCE_DIMENSION);
DECLARE_REFLECTION_ENUM(D3D12_TEXTURE_LAYOUT);
DECLARE_REFLECTION_ENUM(D3D12_RESOURCE_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_COMMAND_QUEUE_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_TEXTURE_COPY_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_FILL_MODE);
DECLARE_REFLECTION_ENUM(D3D12_CULL_MODE);
DECLARE_REFLECTION_ENUM(D3D12_FILTER);
DECLARE_REFLECTION_ENUM(D3D12_COMPARISON_FUNC);
DECLARE_REFLECTION_ENUM(D3D12_TEXTURE_ADDRESS_MODE);
DECLARE_REFLECTION_ENUM(D3D12_CONSERVATIVE_RASTERIZATION_MODE);
DECLARE_REFLECTION_ENUM(D3D12_LOGIC_OP);
DECLARE_REFLECTION_ENUM(D3D12_BLEND);
DECLARE_REFLECTION_ENUM(D3D12_BLEND_OP);
DECLARE_REFLECTION_ENUM(D3D12_STENCIL_OP);
DECLARE_REFLECTION_ENUM(D3D12_COLOR_WRITE_ENABLE);
DECLARE_REFLECTION_ENUM(D3D12_DEPTH_WRITE_MASK);
DECLARE_REFLECTION_ENUM(D3D12_VIEW_INSTANCING_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_RESOLVE_MODE);
DECLARE_REFLECTION_ENUM(D3D12_LINE_RASTERIZATION_MODE);
DECLARE_REFLECTION_ENUM(D3D12_WRITEBUFFERIMMEDIATE_MODE);
DECLARE_REFLECTION_ENUM(D3D12_COMMAND_LIST_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_RENDER_PASS_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_SHADING_RATE);
DECLARE_REFLECTION_ENUM(D3D12_SHADING_RATE_COMBINER);
DECLARE_REFLECTION_ENUM(D3D12_ROOT_SIGNATURE_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_ROOT_PARAMETER_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_ROOT_DESCRIPTOR_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_SAMPLER_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_SHADER_VISIBILITY);
DECLARE_REFLECTION_ENUM(D3D12_STATIC_BORDER_COLOR);
DECLARE_REFLECTION_ENUM(D3D12_DESCRIPTOR_RANGE_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_DESCRIPTOR_RANGE_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_TILE_COPY_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_TILE_RANGE_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_TILE_MAPPING_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_BARRIER_ACCESS);
DECLARE_REFLECTION_ENUM(D3D12_BARRIER_SYNC);
DECLARE_REFLECTION_ENUM(D3D12_BARRIER_LAYOUT);
DECLARE_REFLECTION_ENUM(D3D12_BARRIER_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_TEXTURE_BARRIER_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_RAYTRACING_GEOMETRY_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_RAYTRACING_GEOMETRY_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_ELEMENTS_LAYOUT);
DECLARE_REFLECTION_ENUM(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE);
DECLARE_REFLECTION_ENUM(D3D12_STATE_OBJECT_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_STATE_SUBOBJECT_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_STATE_OBJECT_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_EXPORT_FLAGS);
DECLARE_REFLECTION_ENUM(D3D12_HIT_GROUP_TYPE);
DECLARE_REFLECTION_ENUM(D3D12_RAYTRACING_PIPELINE_FLAGS);

DECLARE_REFLECTION_STRUCT(D3D12_RESOURCE_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_COMMAND_QUEUE_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_SHADER_BYTECODE);
DECLARE_REFLECTION_STRUCT(D3D12_SO_DECLARATION_ENTRY);
DECLARE_REFLECTION_STRUCT(D3D12_STREAM_OUTPUT_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_RASTERIZER_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_RASTERIZER_DESC1);
DECLARE_REFLECTION_STRUCT(D3D12_RASTERIZER_DESC2);
DECLARE_REFLECTION_STRUCT(D3D12_DEPTH_STENCILOP_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_DEPTH_STENCILOP_DESC1);
DECLARE_REFLECTION_STRUCT(D3D12_DEPTH_STENCIL_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_DEPTH_STENCIL_DESC1);
DECLARE_REFLECTION_STRUCT(D3D12_DEPTH_STENCIL_DESC2);
DECLARE_REFLECTION_STRUCT(D3D12_INPUT_ELEMENT_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_INPUT_LAYOUT_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_RENDER_TARGET_BLEND_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_BLEND_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_CACHED_PIPELINE_STATE);
DECLARE_REFLECTION_STRUCT(D3D12_GRAPHICS_PIPELINE_STATE_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_COMPUTE_PIPELINE_STATE_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_INDEX_BUFFER_VIEW);
DECLARE_REFLECTION_STRUCT(D3D12_VERTEX_BUFFER_VIEW);
DECLARE_REFLECTION_STRUCT(D3D12_STREAM_OUTPUT_BUFFER_VIEW);
DECLARE_REFLECTION_STRUCT(D3D12_RESOURCE_TRANSITION_BARRIER);
DECLARE_REFLECTION_STRUCT(D3D12_RESOURCE_ALIASING_BARRIER);
DECLARE_REFLECTION_STRUCT(D3D12_RESOURCE_UAV_BARRIER);
DECLARE_REFLECTION_STRUCT(D3D12_RESOURCE_BARRIER);
DECLARE_REFLECTION_STRUCT(D3D12_HEAP_PROPERTIES);
DECLARE_REFLECTION_STRUCT(D3D12_HEAP_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_DESCRIPTOR_HEAP_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_INDIRECT_ARGUMENT_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_COMMAND_SIGNATURE_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_QUERY_HEAP_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_SAMPLER_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_SAMPLER_DESC2);
DECLARE_REFLECTION_STRUCT(D3D12_CONSTANT_BUFFER_VIEW_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_BUFFER_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX1D_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX1D_ARRAY_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2D_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2D_ARRAY_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2DMS_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2DMS_ARRAY_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_TEXCUBE_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_TEXCUBE_ARRAY_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX3D_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_SHADER_RESOURCE_VIEW_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_BUFFER_RTV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX1D_RTV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX1D_ARRAY_RTV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2D_RTV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2D_ARRAY_RTV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2DMS_RTV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2DMS_ARRAY_RTV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX3D_RTV);
DECLARE_REFLECTION_STRUCT(D3D12_RENDER_TARGET_VIEW_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_TEX1D_DSV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX1D_ARRAY_DSV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2D_DSV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2D_ARRAY_DSV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2DMS_DSV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2DMS_ARRAY_DSV);
DECLARE_REFLECTION_STRUCT(D3D12_DEPTH_STENCIL_VIEW_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_BUFFER_UAV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX1D_UAV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX1D_ARRAY_UAV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2D_UAV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2D_ARRAY_UAV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2DMS_UAV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX2DMS_ARRAY_UAV);
DECLARE_REFLECTION_STRUCT(D3D12_TEX3D_UAV);
DECLARE_REFLECTION_STRUCT(D3D12_UNORDERED_ACCESS_VIEW_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_DEPTH_STENCIL_VALUE);
DECLARE_REFLECTION_STRUCT(D3D12_CLEAR_VALUE);
DECLARE_REFLECTION_STRUCT(D3D12_PLACED_SUBRESOURCE_FOOTPRINT);
DECLARE_REFLECTION_STRUCT(D3D12_SUBRESOURCE_FOOTPRINT);
DECLARE_REFLECTION_STRUCT(D3D12_TEXTURE_COPY_LOCATION);
DECLARE_REFLECTION_STRUCT(D3D12_TILED_RESOURCE_COORDINATE);
DECLARE_REFLECTION_STRUCT(D3D12_TILE_REGION_SIZE);
DECLARE_REFLECTION_STRUCT(D3D12_DISCARD_REGION);
DECLARE_REFLECTION_STRUCT(D3D12_RANGE);
DECLARE_REFLECTION_STRUCT(D3D12_RECT);
DECLARE_REFLECTION_STRUCT(D3D12_BOX);
DECLARE_REFLECTION_STRUCT(D3D12_VIEWPORT);
DECLARE_REFLECTION_STRUCT(D3D12_RT_FORMAT_ARRAY);
DECLARE_REFLECTION_STRUCT(D3D12_VIEW_INSTANCE_LOCATION);
DECLARE_REFLECTION_STRUCT(D3D12_VIEW_INSTANCING_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_SAMPLE_POSITION);
DECLARE_REFLECTION_STRUCT(D3D12_RANGE_UINT64);
DECLARE_REFLECTION_STRUCT(D3D12_SUBRESOURCE_RANGE_UINT64);
DECLARE_REFLECTION_STRUCT(D3D12_WRITEBUFFERIMMEDIATE_PARAMETER);
DECLARE_REFLECTION_STRUCT(D3D12_DRAW_ARGUMENTS);
DECLARE_REFLECTION_STRUCT(D3D12_DRAW_INDEXED_ARGUMENTS);
DECLARE_REFLECTION_STRUCT(D3D12_DISPATCH_ARGUMENTS);
DECLARE_REFLECTION_STRUCT(D3D12_DISPATCH_MESH_ARGUMENTS);
DECLARE_REFLECTION_STRUCT(D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS);
DECLARE_REFLECTION_STRUCT(D3D12_RENDER_PASS_BEGINNING_ACCESS);
DECLARE_REFLECTION_STRUCT(D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS);
DECLARE_REFLECTION_STRUCT(D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_PARAMETERS);
DECLARE_REFLECTION_STRUCT(D3D12_RENDER_PASS_ENDING_ACCESS);
DECLARE_REFLECTION_STRUCT(D3D12_RENDER_PASS_RENDER_TARGET_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_RENDER_PASS_DEPTH_STENCIL_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_STATIC_SAMPLER_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_STATIC_SAMPLER_DESC1);
DECLARE_REFLECTION_STRUCT(D3D12_DESCRIPTOR_RANGE1);
DECLARE_REFLECTION_STRUCT(D3D12_ROOT_DESCRIPTOR_TABLE1);
DECLARE_REFLECTION_STRUCT(D3D12_ROOT_CONSTANTS);
DECLARE_REFLECTION_STRUCT(D3D12_ROOT_DESCRIPTOR1);
DECLARE_REFLECTION_STRUCT(D3D12_RESOURCE_DESC1);
DECLARE_REFLECTION_STRUCT(D3D12_MIP_REGION);
DECLARE_REFLECTION_STRUCT(D3D12_BARRIER_SUBRESOURCE_RANGE);
DECLARE_REFLECTION_STRUCT(D3D12_BUFFER_BARRIER);
DECLARE_REFLECTION_STRUCT(D3D12_TEXTURE_BARRIER);
DECLARE_REFLECTION_STRUCT(D3D12_GLOBAL_BARRIER);
DECLARE_REFLECTION_STRUCT(D3D12_BARRIER_GROUP);
DECLARE_REFLECTION_STRUCT(D3D12ResourceLayout);
DECLARE_REFLECTION_STRUCT(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_GEOMETRY_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_GEOMETRY_AABBS_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_SERIALIZATION_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV);
DECLARE_REFLECTION_STRUCT(D3D12_STATE_OBJECT_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_STATE_SUBOBJECT);
DECLARE_REFLECTION_STRUCT(D3D12_STATE_OBJECT_CONFIG);
DECLARE_REFLECTION_STRUCT(D3D12_GLOBAL_ROOT_SIGNATURE);
DECLARE_REFLECTION_STRUCT(D3D12_LOCAL_ROOT_SIGNATURE);
DECLARE_REFLECTION_STRUCT(D3D12_NODE_MASK);
DECLARE_REFLECTION_STRUCT(D3D12_DXIL_LIBRARY_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_EXISTING_COLLECTION_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
DECLARE_REFLECTION_STRUCT(D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_SHADER_CONFIG);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_PIPELINE_CONFIG);
DECLARE_REFLECTION_STRUCT(D3D12_HIT_GROUP_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_RAYTRACING_PIPELINE_CONFIG1);
DECLARE_REFLECTION_STRUCT(D3D12_EXPORT_DESC);
DECLARE_REFLECTION_STRUCT(D3D12_GPU_VIRTUAL_ADDRESS_RANGE);
DECLARE_REFLECTION_STRUCT(D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE);
DECLARE_REFLECTION_STRUCT(D3D12_DISPATCH_RAYS_DESC);

DECLARE_DESERIALISE_TYPE(D3D12_DISCARD_REGION);
DECLARE_DESERIALISE_TYPE(D3D12_GRAPHICS_PIPELINE_STATE_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_COMPUTE_PIPELINE_STATE_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_COMMAND_SIGNATURE_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_RENDER_PASS_RENDER_TARGET_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_RENDER_PASS_DEPTH_STENCIL_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_BARRIER_GROUP);
DECLARE_DESERIALISE_TYPE(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_STATE_OBJECT_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_DXIL_LIBRARY_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_EXISTING_COLLECTION_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
DECLARE_DESERIALISE_TYPE(D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
DECLARE_DESERIALISE_TYPE(D3D12_HIT_GROUP_DESC);
DECLARE_DESERIALISE_TYPE(D3D12_EXPORT_DESC);

enum class D3D12Chunk : uint32_t
{
  SetName = (uint32_t)SystemChunk::FirstDriverChunk,
  PushMarker,
  SetMarker,
  PopMarker,
  SetShaderDebugPath,
  CreateSwapBuffer,
  Device_CreateCommandQueue,
  Device_CreateCommandAllocator,
  Device_CreateCommandList,
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
  List_IndirectSubCommand,
  Queue_BeginEvent,
  Queue_SetMarker,
  Queue_EndEvent,
  Device_CreatePipelineState,
  Device_CreateHeapFromAddress,
  Device_CreateHeapFromFileMapping,
  List_AtomicCopyBufferUINT,
  List_AtomicCopyBufferUINT64,
  List_OMSetDepthBounds,
  List_ResolveSubresourceRegion,
  List_SetSamplePositions,
  List_SetViewInstanceMask,
  List_WriteBufferImmediate,
  Device_OpenSharedHandle,
  Device_CreateCommandList1,
  Device_CreateCommittedResource1,
  Device_CreateHeap1,
  List_BeginRenderPass,
  List_EndRenderPass,
  List_RSSetShadingRate,
  List_RSSetShadingRateImage,
  Device_ExternalDXGIResource,
  Swapchain_Present,
  List_ClearState,
  CompatDevice_CreateSharedResource,
  CompatDevice_CreateSharedHeap,
  SetShaderExtUAV,
  Device_CreateCommittedResource2,
  Device_CreatePlacedResource1,
  Device_CreateCommandQueue1,
  CoherentMapWrite,
  Device_CreateSampler2,
  Device_CreateCommittedResource3,
  Device_CreatePlacedResource2,
  Device_CreateReservedResource1,
  Device_CreateReservedResource2,
  List_OMSetFrontAndBackStencilRef,
  List_RSSetDepthBias,
  List_IASetIndexBufferStripCutValue,
  List_Barrier,
  List_DispatchMesh,
  Device_CreateStateObject,
  Device_AddToStateObject,
  List_BuildRaytracingAccelerationStructure,
  List_CopyRaytracingAccelerationStructure,
  List_EmitRaytracingAccelerationStructurePostbuildInfo,
  List_DispatchRays,
  List_SetPipelineState1,
  CreateAS,
  StateObject_SetPipelineStackSize,
  Max,
};
