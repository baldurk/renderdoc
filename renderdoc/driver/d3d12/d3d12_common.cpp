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

#include "d3d12_common.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "d3d12_manager.h"
#include "d3d12_resources.h"

D3D12MarkerRegion::D3D12MarkerRegion(ID3D12GraphicsCommandList *l, const rdcstr &marker)
{
  list = l;
  queue = NULL;

  D3D12MarkerRegion::Begin(list, marker);
}

D3D12MarkerRegion::D3D12MarkerRegion(ID3D12CommandQueue *q, const rdcstr &marker)
{
  list = NULL;
  queue = q;

  D3D12MarkerRegion::Begin(queue, marker);
}

D3D12MarkerRegion::~D3D12MarkerRegion()
{
  if(list)
    D3D12MarkerRegion::End(list);
  if(queue)
    D3D12MarkerRegion::End(queue);
}

void D3D12MarkerRegion::Begin(ID3D12GraphicsCommandList *list, const rdcstr &marker)
{
  if(list)
  {
    // Some debuggers (but not all) will assume the event string is null-terminated, and
    // display one less character than specified by the size. Append a space to pad the
    // output without visibly changing the event marker for other debuggers.
    rdcwstr text = StringFormat::UTF82Wide(marker + " ");
    UINT size = UINT(text.length() * sizeof(wchar_t));
    list->BeginEvent(0, text.c_str(), size);
  }
}

void D3D12MarkerRegion::Begin(ID3D12CommandQueue *queue, const rdcstr &marker)
{
  if(queue)
  {
    rdcwstr text = StringFormat::UTF82Wide(marker + " ");
    UINT size = UINT(text.length() * sizeof(wchar_t));
    queue->BeginEvent(0, text.c_str(), size);
  }
}

void D3D12MarkerRegion::Set(ID3D12GraphicsCommandList *list, const rdcstr &marker)
{
  if(list)
  {
    rdcwstr text = StringFormat::UTF82Wide(marker + " ");
    UINT size = UINT(text.length() * sizeof(wchar_t));
    list->SetMarker(0, text.c_str(), size);
  }
}

void D3D12MarkerRegion::Set(ID3D12CommandQueue *queue, const rdcstr &marker)
{
  if(queue)
  {
    rdcwstr text = StringFormat::UTF82Wide(marker + " ");
    UINT size = UINT(text.length() * sizeof(wchar_t));
    queue->SetMarker(0, text.c_str(), size);
  }
}

void D3D12MarkerRegion::End(ID3D12GraphicsCommandList *list)
{
  list->EndEvent();
}

void D3D12MarkerRegion::End(ID3D12CommandQueue *queue)
{
  queue->EndEvent();
}

void BarrierSet::Configure(ID3D12Resource *res, const SubresourceStateVector &states,
                           AccessType access)
{
  bool allowCommon = false;
  D3D12_RESOURCE_STATES resourceState;
  D3D12_BARRIER_LAYOUT resourceLayout;
  D3D12_BARRIER_ACCESS resourceAccess;
  D3D12_BARRIER_SYNC resourceSync;

  // we assume wrapped resources
  RDCASSERT(WrappedID3D12Resource::IsAlloc(res));

  const bool isBuffer = (res->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);

  switch(access)
  {
    case SRVAccess:
      resourceState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
      resourceLayout = D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
      resourceAccess = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
      resourceSync = D3D12_BARRIER_SYNC_ALL_SHADING;
      // common layouts allow shader resource access with no layout change
      allowCommon = true;
      break;
    case ResolveSourceAccess:
      resourceState = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
      resourceLayout = D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
      resourceAccess = D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
      resourceSync = D3D12_BARRIER_SYNC_RESOLVE;
      break;
    default:
    // should not happen but is the neatest solution to uninitialised variable warnings
    case CopySourceAccess:
      resourceState = D3D12_RESOURCE_STATE_COPY_SOURCE;
      resourceLayout = D3D12_BARRIER_LAYOUT_COPY_SOURCE;
      resourceAccess = D3D12_BARRIER_ACCESS_COPY_SOURCE;
      resourceSync = D3D12_BARRIER_SYNC_COPY;
      // common layouts allow shader resource access with no layout change
      allowCommon = true;
      break;
    case CopyDestAccess:
      resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
      resourceLayout = D3D12_BARRIER_LAYOUT_COPY_DEST;
      resourceAccess = D3D12_BARRIER_ACCESS_COPY_DEST;
      resourceSync = D3D12_BARRIER_SYNC_COPY;
      // common layouts allow shader resource access with no layout change
      allowCommon = true;
      break;
  }

  barriers.reserve(states.size());
  newBarriers.reserve(states.size());
  for(size_t i = 0; i < states.size(); i++)
  {
    if(states[i].IsStates())
    {
      D3D12_RESOURCE_BARRIER b;

      b.Transition.StateBefore = states[i].ToStates();

      // skip unneeded barriers
      if((resourceState != D3D12_RESOURCE_STATE_COMMON &&
          (b.Transition.StateBefore & resourceState) == resourceState) ||
         b.Transition.StateBefore == resourceState)
        continue;

      b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      b.Transition.pResource = res;
      b.Transition.Subresource = (UINT)i;
      b.Transition.StateAfter = resourceState;

      barriers.push_back(b);
    }
    // buffers don't need any transitions with the new layouts
    else if(!isBuffer)
    {
      D3D12_TEXTURE_BARRIER b = {};

      b.LayoutBefore = states[i].ToLayout();

      // as long as the layout matches we don't need any extra access/sync since we're in a
      // different command buffer.
      if(b.LayoutBefore == resourceLayout ||
         (allowCommon && (b.LayoutBefore == D3D12_BARRIER_LAYOUT_COMMON ||
                          b.LayoutBefore == D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON ||
                          b.LayoutBefore == D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COMMON)))
        continue;

      b.AccessBefore = D3D12_BARRIER_ACCESS_COMMON;
      b.SyncBefore = D3D12_BARRIER_SYNC_ALL;

      if(b.LayoutBefore == D3D12_BARRIER_LAYOUT_UNDEFINED)
        b.AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS;

      b.AccessAfter = resourceAccess;
      b.SyncAfter = resourceSync;
      b.LayoutAfter = resourceLayout;
      b.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;
      b.Subresources.IndexOrFirstMipLevel = (UINT)i;
      b.pResource = res;

      newBarriers.push_back(b);
    }
  }
}

void BarrierSet::Apply(ID3D12GraphicsCommandListX *list)
{
  D3D12_BARRIER_GROUP group;
  group.NumBarriers = (UINT)newBarriers.size();
  group.Type = D3D12_BARRIER_TYPE_TEXTURE;
  group.pTextureBarriers = newBarriers.data();

  if(!barriers.empty())
    list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);
  // we unconditionally call new barriers, because they can only appear if a new layout was
  // previously used (otherwise we stick to old states). This will only break if we're replaying
  // a capture that used new layouts but new barrier support isn't present.
  if(!newBarriers.empty())
    list->Barrier(1, &group);
  if(!newToOldBarriers.empty())
    list->ResourceBarrier((UINT)newToOldBarriers.size(), &newToOldBarriers[0]);
}

void BarrierSet::Unapply(ID3D12GraphicsCommandListX *list)
{
  D3D12_BARRIER_GROUP group;
  group.NumBarriers = (UINT)newBarriers.size();
  group.Type = D3D12_BARRIER_TYPE_TEXTURE;
  group.pTextureBarriers = newBarriers.data();

  // real resource back to itself
  for(size_t i = 0; i < barriers.size(); i++)
    std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);
  for(size_t i = 0; i < newBarriers.size(); i++)
  {
    std::swap(newBarriers[i].AccessBefore, newBarriers[i].AccessAfter);
    std::swap(newBarriers[i].SyncBefore, newBarriers[i].SyncAfter);
    std::swap(newBarriers[i].LayoutBefore, newBarriers[i].LayoutAfter);
  }

  if(!barriers.empty())
    list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);
  if(!newBarriers.empty())
    list->Barrier(1, &group);
  // if we had new-to-old barriers we should not ever be unapplying that barrier set as it's
  // one-way.
  RDCASSERT(newToOldBarriers.empty());
}

bool EnableD3D12DebugLayer(D3D12DevConfiguration *devConfig,
                           PFN_D3D12_GET_DEBUG_INTERFACE getDebugInterface)
{
  ID3D12Debug *debug = NULL;
  if(devConfig)
  {
    if(devConfig->debug)
    {
      debug = devConfig->debug;
      debug->AddRef();
    }
  }
  else
  {
    if(!getDebugInterface)
      getDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(
          GetModuleHandleA("d3d12.dll"), "D3D12GetDebugInterface");

    if(!getDebugInterface)
    {
      RDCERR("Couldn't find D3D12GetDebugInterface!");
      return false;
    }

    HRESULT hr = getDebugInterface(__uuidof(ID3D12Debug), (void **)&debug);

    if(FAILED(hr))
      SAFE_RELEASE(debug);

    if(hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
      RDCWARN("Debug layer not available: DXGI_ERROR_SDK_COMPONENT_MISSING");
    }
    else if(FAILED(hr))
    {
      RDCERR("Couldn't enable debug layer: %x", hr);
    }
  }

  if(debug)
  {
    debug->EnableDebugLayer();

    RDCDEBUG("Enabling debug layer");

// enable this to get GPU-based validation, where available, whenever we enable API validation
#if 0
    ID3D12Debug1 *debug1 = NULL;
    hr = debug->QueryInterface(__uuidof(ID3D12Debug1), (void **)&debug1);

    if(SUCCEEDED(hr) && debug1)
    {
      RDCDEBUG("Enabling GPU-based validation");
      debug1->SetEnableGPUBasedValidation(true);
      SAFE_RELEASE(debug1);
    }
    else
    {
      RDCDEBUG("GPU-based validation not available");
    }
#endif

    SAFE_RELEASE(debug);

    return true;
  }

  return false;
}

HRESULT EnumAdapterByLuid(IDXGIFactory1 *factory, LUID luid, IDXGIAdapter **pAdapter)
{
  HRESULT hr = S_OK;

  *pAdapter = NULL;

  for(UINT i = 0; i < 10; i++)
  {
    IDXGIAdapter *adapter = NULL;
    hr = factory->EnumAdapters(i, &adapter);
    if(hr == S_OK && adapter)
    {
      DXGI_ADAPTER_DESC desc;
      adapter->GetDesc(&desc);

      if(desc.AdapterLuid.LowPart == luid.LowPart && desc.AdapterLuid.HighPart == luid.HighPart)
      {
        *pAdapter = adapter;
        return S_OK;
      }

      adapter->Release();
    }
    else
    {
      break;
    }
  }

  return E_FAIL;
}

bool D3D12InitParams::IsSupportedVersion(uint64_t ver)
{
  if(ver == CurrentVersion)
    return true;

  // 0x4 -> 0x5 - CPU_DESCRIPTOR_HANDLE serialised inline as D3D12Descriptor in appropriate
  //              list-recording functions
  if(ver == 0x4)
    return true;

  // 0x5 -> 0x6 - Multiply by number of planes in format when serialising initial states -
  //              i.e. stencil is saved with depth in initial states.
  if(ver == 0x5)
    return true;

  // 0x6 -> 0x7 - Fixed serialisation of D3D12_WRITEBUFFERIMMEDIATE_PARAMETER to properly replay the
  //              GPU address
  if(ver == 0x6)
    return true;

  // 0x7 -> 0x8 - Added serialisation of adapter descriptor in D3D12InitParams
  if(ver == 0x7)
    return true;

  // 0x8 -> 0x9 - Added serialisation of usedDXIL in D3D12InitParams
  if(ver == 0x8)
    return true;

  // 0x9 -> 0xA - Added serialisation of vendor extension use in D3D12InitParams
  if(ver == 0x9)
    return true;

  // 0xA -> 0xB - Added support for sparse/reserved/tiled resources
  if(ver == 0xA)
    return true;

  // 0xB -> 0xC - Serialised D3D12 SDK version
  if(ver == 0xB)
    return true;

  // 0xC -> 0xD - Serialised encoded PIX marker color
  if(ver == 0xC)
    return true;

  // 0xD -> 0xE - Initial contents of sparse resources only serialise subresources with mapped pages
  if(ver == 0xD)
    return true;

  // 0xE -> 0xF - Sampler descriptors are now serialised as D3D12_SAMPLER_DESC2 in a backwards
  //              compatible manner
  if(ver == 0xE)
    return true;

  // 0xF -> 0x10 - Expanded PSO desc is serialised with new rasterizer/depth-stencil descs
  if(ver == 0xF)
    return true;

  // 0x10 -> 0x11 - Expanded PSO desc is serialised with amplification and mesh shader descs
  if(ver == 0x10)
    return true;

  // 0x11 -> 0x12 - Descriptor heaps serialise the original pointer to their descriptor array for GPU unwrapping
  if(ver == 0x11)
    return true;

  return false;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12InitParams &el)
{
  SERIALISE_MEMBER(MinimumFeatureLevel);

  if(ser.VersionAtLeast(0x8))
  {
    SERIALISE_MEMBER(AdapterDesc);
  }
  else
  {
    RDCEraseEl(el.AdapterDesc);
  }

  if(ser.VersionAtLeast(0x9))
  {
    SERIALISE_MEMBER(usedDXIL);
  }

  if(ser.VersionAtLeast(0xA))
  {
    SERIALISE_MEMBER(VendorExtensions);
    SERIALISE_MEMBER(VendorUAV);
    SERIALISE_MEMBER(VendorUAVSpace);
  }
  else
  {
    el.VendorExtensions = GPUVendor::Unknown;
    el.VendorUAV = ~0U;
    el.VendorUAVSpace = ~0U;
  }

  if(ser.VersionAtLeast(0xC))
  {
    SERIALISE_MEMBER(SDKVersion);
  }
  else
  {
    el.SDKVersion = 0;
  }
}

INSTANTIATE_SERIALISE_TYPE(D3D12InitParams);

FloatVector DecodePIXColor(UINT64 Color)
{
  if((Color & 0xff000000) != 0xff000000)
  {
    // indexed thing, look up our fixed array
    static const uint32_t fixedColors[] = {
        0xffff0000, 0xff0000ff, 0xff00ff00, 0xff00ffff, 0xffff00ff, 0xff008000, 0xff800080,
        0xff00008b, 0xfff08080, 0xff3cb371, 0xffb8860b, 0xffbdb76b, 0xff32cd32, 0xffb03060,
        0xffff8c00, 0xff9400d3, 0xff00fa9a, 0xffdc143c, 0xff00bfff, 0xffadff2f, 0xffda70d6,
        0xffd8bfd8, 0xff1e90ff, 0xffffff54, 0xffff1493, 0xff7b68ee, 0xfffafad2, 0xff2f4f4f,
        0xff556b2f, 0xff8b4513, 0xff483d8b, 0xff5f9ea0,
    };

    Color = fixedColors[Color % ARRAY_COUNT(fixedColors)];
  }

  FloatVector ret;
  ret.x = float(((Color >> 16) & 0xff)) / 255.0f;
  ret.y = float(((Color >> 8) & 0xff)) / 255.0f;
  ret.z = float(((Color >> 0) & 0xff)) / 255.0f;
  ret.w = 1.0f;

  return ret;
}

TextureType MakeTextureDim(D3D12_SRV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_SRV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D12_SRV_DIMENSION_BUFFER: return TextureType::Buffer;
    case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE: return TextureType::Buffer;
    case D3D12_SRV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D12_SRV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D12_SRV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
    case D3D12_SRV_DIMENSION_TEXTURE3D: return TextureType::Texture3D;
    case D3D12_SRV_DIMENSION_TEXTURECUBE: return TextureType::TextureCube;
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: return TextureType::TextureCubeArray;
    default: break;
  }

  return TextureType::Unknown;
}

TextureType MakeTextureDim(D3D12_RTV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_RTV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D12_RTV_DIMENSION_BUFFER: return TextureType::Buffer;
    case D3D12_RTV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D12_RTV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D12_RTV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
    case D3D12_RTV_DIMENSION_TEXTURE3D: return TextureType::Texture3D;
    default: break;
  }

  return TextureType::Unknown;
}

TextureType MakeTextureDim(D3D12_DSV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_DSV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D12_DSV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D12_DSV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D12_DSV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
    default: break;
  }

  return TextureType::Unknown;
}

TextureType MakeTextureDim(D3D12_UAV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_UAV_DIMENSION_UNKNOWN: return TextureType::Unknown;
    case D3D12_UAV_DIMENSION_BUFFER: return TextureType::Buffer;
    case D3D12_UAV_DIMENSION_TEXTURE1D: return TextureType::Texture1D;
    case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: return TextureType::Texture1DArray;
    case D3D12_UAV_DIMENSION_TEXTURE2D: return TextureType::Texture2D;
    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: return TextureType::Texture2DArray;
    case D3D12_UAV_DIMENSION_TEXTURE2DMS: return TextureType::Texture2DMS;
    case D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY: return TextureType::Texture2DMSArray;
    case D3D12_UAV_DIMENSION_TEXTURE3D: return TextureType::Texture3D;
    default: break;
  }

  return TextureType::Unknown;
}

AddressMode MakeAddressMode(D3D12_TEXTURE_ADDRESS_MODE addr)
{
  switch(addr)
  {
    case D3D12_TEXTURE_ADDRESS_MODE_WRAP: return AddressMode::Wrap;
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR: return AddressMode::Mirror;
    case D3D12_TEXTURE_ADDRESS_MODE_CLAMP: return AddressMode::ClampEdge;
    case D3D12_TEXTURE_ADDRESS_MODE_BORDER: return AddressMode::ClampBorder;
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE: return AddressMode::MirrorOnce;
    default: break;
  }

  return AddressMode::Wrap;
}

CompareFunction MakeCompareFunc(D3D12_COMPARISON_FUNC func)
{
  switch(func)
  {
    case D3D12_COMPARISON_FUNC_NONE:
    case D3D12_COMPARISON_FUNC_NEVER: return CompareFunction::Never;
    case D3D12_COMPARISON_FUNC_LESS: return CompareFunction::Less;
    case D3D12_COMPARISON_FUNC_EQUAL: return CompareFunction::Equal;
    case D3D12_COMPARISON_FUNC_LESS_EQUAL: return CompareFunction::LessEqual;
    case D3D12_COMPARISON_FUNC_GREATER: return CompareFunction::Greater;
    case D3D12_COMPARISON_FUNC_NOT_EQUAL: return CompareFunction::NotEqual;
    case D3D12_COMPARISON_FUNC_GREATER_EQUAL: return CompareFunction::GreaterEqual;
    case D3D12_COMPARISON_FUNC_ALWAYS: return CompareFunction::AlwaysTrue;
    default: break;
  }

  return CompareFunction::AlwaysTrue;
}

TextureFilter MakeFilter(D3D12_FILTER filter)
{
  TextureFilter ret;

  ret.filter = FilterFunction::Normal;

  if(filter >= D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT &&
     filter <= D3D12_FILTER_COMPARISON_ANISOTROPIC)
  {
    ret.filter = FilterFunction::Comparison;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D12_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT &&
          filter <= D3D12_FILTER_MINIMUM_ANISOTROPIC)
  {
    ret.filter = FilterFunction::Minimum;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D12_FILTER(filter & 0x7f);
  }
  else if(filter >= D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT &&
          filter <= D3D12_FILTER_MAXIMUM_ANISOTROPIC)
  {
    ret.filter = FilterFunction::Maximum;
    // the first 0x7f is the min/mag/mip filtering
    filter = D3D12_FILTER(filter & 0x7f);
  }

  switch(filter)
  {
    case D3D12_FILTER_ANISOTROPIC:
      ret.minify = ret.magnify = ret.mip = FilterMode::Anisotropic;
      break;
    case D3D12_FILTER_MIN_MAG_ANISOTROPIC_MIP_POINT:
      ret.minify = ret.magnify = FilterMode::Anisotropic;
      ret.mip = FilterMode::Point;
      break;
    case D3D12_FILTER_MIN_MAG_MIP_POINT:
      ret.minify = ret.magnify = ret.mip = FilterMode::Point;
      break;
    case D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR:
      ret.minify = ret.magnify = FilterMode::Point;
      ret.mip = FilterMode::Linear;
      break;
    case D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:
      ret.minify = FilterMode::Point;
      ret.magnify = FilterMode::Linear;
      ret.mip = FilterMode::Point;
      break;
    case D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR:
      ret.minify = FilterMode::Point;
      ret.magnify = ret.mip = FilterMode::Linear;
      break;
    case D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT:
      ret.minify = FilterMode::Linear;
      ret.magnify = ret.mip = FilterMode::Point;
      break;
    case D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
      ret.minify = FilterMode::Linear;
      ret.magnify = FilterMode::Point;
      ret.mip = FilterMode::Linear;
      break;
    case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT:
      ret.minify = ret.magnify = FilterMode::Linear;
      ret.mip = FilterMode::Point;
      break;
    case D3D12_FILTER_MIN_MAG_MIP_LINEAR:
      ret.minify = ret.magnify = ret.mip = FilterMode::Linear;
      break;
    default: break;
  }

  return ret;
}

DescriptorFlags MakeDescriptorFlags(D3D12_BUFFER_SRV_FLAGS flags)
{
  DescriptorFlags ret = DescriptorFlags::NoFlags;

  if(flags & D3D12_BUFFER_SRV_FLAG_RAW)
    ret |= DescriptorFlags::RawBuffer;

  return ret;
}

DescriptorFlags MakeDescriptorFlags(D3D12_BUFFER_UAV_FLAGS flags)
{
  DescriptorFlags ret = DescriptorFlags::NoFlags;

  if(flags & D3D12_BUFFER_UAV_FLAG_RAW)
    ret |= DescriptorFlags::RawBuffer;

  return ret;
}

LogicOperation MakeLogicOp(D3D12_LOGIC_OP op)
{
  switch(op)
  {
    case D3D12_LOGIC_OP_CLEAR: return LogicOperation::Clear;
    case D3D12_LOGIC_OP_AND: return LogicOperation::And;
    case D3D12_LOGIC_OP_AND_REVERSE: return LogicOperation::AndReverse;
    case D3D12_LOGIC_OP_COPY: return LogicOperation::Copy;
    case D3D12_LOGIC_OP_AND_INVERTED: return LogicOperation::AndInverted;
    case D3D12_LOGIC_OP_NOOP: return LogicOperation::NoOp;
    case D3D12_LOGIC_OP_XOR: return LogicOperation::Xor;
    case D3D12_LOGIC_OP_OR: return LogicOperation::Or;
    case D3D12_LOGIC_OP_NOR: return LogicOperation::Nor;
    case D3D12_LOGIC_OP_EQUIV: return LogicOperation::Equivalent;
    case D3D12_LOGIC_OP_INVERT: return LogicOperation::Invert;
    case D3D12_LOGIC_OP_OR_REVERSE: return LogicOperation::OrReverse;
    case D3D12_LOGIC_OP_COPY_INVERTED: return LogicOperation::CopyInverted;
    case D3D12_LOGIC_OP_OR_INVERTED: return LogicOperation::OrInverted;
    case D3D12_LOGIC_OP_NAND: return LogicOperation::Nand;
    case D3D12_LOGIC_OP_SET: return LogicOperation::Set;
    default: break;
  }

  return LogicOperation::NoOp;
}

BlendMultiplier MakeBlendMultiplier(D3D12_BLEND blend, bool alpha)
{
  switch(blend)
  {
    case D3D12_BLEND_ZERO: return BlendMultiplier::Zero;
    case D3D12_BLEND_ONE: return BlendMultiplier::One;
    case D3D12_BLEND_SRC_COLOR: return BlendMultiplier::SrcCol;
    case D3D12_BLEND_INV_SRC_COLOR: return BlendMultiplier::InvSrcCol;
    case D3D12_BLEND_DEST_COLOR: return BlendMultiplier::DstCol;
    case D3D12_BLEND_INV_DEST_COLOR: return BlendMultiplier::InvDstCol;
    case D3D12_BLEND_SRC_ALPHA: return BlendMultiplier::SrcAlpha;
    case D3D12_BLEND_INV_SRC_ALPHA: return BlendMultiplier::InvSrcAlpha;
    case D3D12_BLEND_DEST_ALPHA: return BlendMultiplier::DstAlpha;
    case D3D12_BLEND_INV_DEST_ALPHA: return BlendMultiplier::InvDstAlpha;
    case D3D12_BLEND_BLEND_FACTOR:
      return alpha ? BlendMultiplier::FactorAlpha : BlendMultiplier::FactorRGB;
    case D3D12_BLEND_INV_BLEND_FACTOR:
      return alpha ? BlendMultiplier::InvFactorAlpha : BlendMultiplier::InvFactorRGB;
    case D3D12_BLEND_SRC_ALPHA_SAT: return BlendMultiplier::SrcAlphaSat;
    case D3D12_BLEND_SRC1_COLOR: return BlendMultiplier::Src1Col;
    case D3D12_BLEND_INV_SRC1_COLOR: return BlendMultiplier::InvSrc1Col;
    case D3D12_BLEND_SRC1_ALPHA: return BlendMultiplier::Src1Alpha;
    case D3D12_BLEND_INV_SRC1_ALPHA: return BlendMultiplier::InvSrc1Alpha;
    case D3D12_BLEND_ALPHA_FACTOR: return BlendMultiplier::FactorAlpha;
    case D3D12_BLEND_INV_ALPHA_FACTOR: return BlendMultiplier::InvFactorAlpha;
    default: break;
  }

  return BlendMultiplier::One;
}

BlendOperation MakeBlendOp(D3D12_BLEND_OP op)
{
  switch(op)
  {
    case D3D12_BLEND_OP_ADD: return BlendOperation::Add;
    case D3D12_BLEND_OP_SUBTRACT: return BlendOperation::Subtract;
    case D3D12_BLEND_OP_REV_SUBTRACT: return BlendOperation::ReversedSubtract;
    case D3D12_BLEND_OP_MIN: return BlendOperation::Minimum;
    case D3D12_BLEND_OP_MAX: return BlendOperation::Maximum;
    default: break;
  }

  return BlendOperation::Add;
}

StencilOperation MakeStencilOp(D3D12_STENCIL_OP op)
{
  switch(op)
  {
    case D3D12_STENCIL_OP_KEEP: return StencilOperation::Keep;
    case D3D12_STENCIL_OP_ZERO: return StencilOperation::Zero;
    case D3D12_STENCIL_OP_REPLACE: return StencilOperation::Replace;
    case D3D12_STENCIL_OP_INCR_SAT: return StencilOperation::IncSat;
    case D3D12_STENCIL_OP_DECR_SAT: return StencilOperation::DecSat;
    case D3D12_STENCIL_OP_INVERT: return StencilOperation::Invert;
    case D3D12_STENCIL_OP_INCR: return StencilOperation::IncWrap;
    case D3D12_STENCIL_OP_DECR: return StencilOperation::DecWrap;
    default: break;
  }

  return StencilOperation::Keep;
}

uint32_t ArgumentTypeByteSize(const D3D12_INDIRECT_ARGUMENT_DESC &arg)
{
  switch(arg.Type)
  {
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW: return sizeof(D3D12_DRAW_ARGUMENTS);
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED: return sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH: return sizeof(D3D12_DISPATCH_ARGUMENTS);
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH: return sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS: return sizeof(D3D12_DISPATCH_RAYS_DESC);
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
      return sizeof(uint32_t) * arg.Constant.Num32BitValuesToSet;
    case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW: return sizeof(D3D12_VERTEX_BUFFER_VIEW);
    case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW: return sizeof(D3D12_INDEX_BUFFER_VIEW);
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
    case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
    case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
      return sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
    default: RDCERR("Unexpected argument type! %d", arg.Type); break;
  }

  return 0;
}

UINT GetResourceNumMipLevels(const D3D12_RESOURCE_DESC *desc)
{
  switch(desc->Dimension)
  {
    default:
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
      RDCERR("Unexpected resource dimension! %d", desc->Dimension);
      break;
    case D3D12_RESOURCE_DIMENSION_BUFFER: return 1;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
    {
      if(desc->MipLevels)
        return desc->MipLevels;
      UINT w = RDCMAX(1U, UINT(desc->Width));
      UINT count = 1;
      while(w > 1)
      {
        ++count;
        w = RDCMAX(1U, w >> 1U);
      }
      return count;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
    {
      if(desc->MipLevels)
        return desc->MipLevels;
      UINT w = RDCMAX(1U, UINT(desc->Width));
      UINT h = RDCMAX(1U, desc->Height);
      UINT count = 1;
      while(w > 1 || h > 1)
      {
        ++count;
        w = RDCMAX(1U, w >> 1U);
        h = RDCMAX(1U, h >> 1U);
      }
      return count;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
    {
      if(desc->MipLevels)
        return desc->MipLevels;
      UINT w = RDCMAX(1U, UINT(desc->Width));
      UINT h = RDCMAX(1U, desc->Height);
      UINT d = RDCMAX(1U, UINT(desc->DepthOrArraySize));
      UINT count = 1;
      while(w > 1 || h > 1 || d > 1)
      {
        ++count;
        w = RDCMAX(1U, w >> 1U);
        h = RDCMAX(1U, h >> 1U);
        d = RDCMAX(1U, d >> 1U);
      }
      return count;
    }
  }

  return 1;
}

UINT GetNumSubresources(ID3D12Device *dev, const D3D12_RESOURCE_DESC *desc)
{
  D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
  formatInfo.Format = desc->Format;
  dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

  UINT planes = RDCMAX((UINT8)1, formatInfo.PlaneCount);

  switch(desc->Dimension)
  {
    default:
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
      RDCERR("Unexpected resource dimension! %d", desc->Dimension);
      break;
    case D3D12_RESOURCE_DIMENSION_BUFFER: return planes;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
      return RDCMAX((UINT16)1, desc->DepthOrArraySize) * GetResourceNumMipLevels(desc) * planes;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D: return GetResourceNumMipLevels(desc) * planes;
  }

  return 1;
}

UINT D3D12CalcSubresource(UINT MipSlice, UINT ArraySlice, UINT PlaneSlice, UINT MipLevels,
                          UINT ArraySize)
{
  return MipSlice + (ArraySlice * MipLevels) + (PlaneSlice * MipLevels * ArraySize);
}

ShaderStageMask ConvertVisibility(D3D12_SHADER_VISIBILITY ShaderVisibility)
{
  switch(ShaderVisibility)
  {
    case D3D12_SHADER_VISIBILITY_ALL: return ShaderStageMask::All;
    case D3D12_SHADER_VISIBILITY_VERTEX: return ShaderStageMask::Vertex;
    case D3D12_SHADER_VISIBILITY_HULL: return ShaderStageMask::Hull;
    case D3D12_SHADER_VISIBILITY_DOMAIN: return ShaderStageMask::Domain;
    case D3D12_SHADER_VISIBILITY_GEOMETRY: return ShaderStageMask::Geometry;
    case D3D12_SHADER_VISIBILITY_PIXEL: return ShaderStageMask::Pixel;
    case D3D12_SHADER_VISIBILITY_AMPLIFICATION: return ShaderStageMask::Amplification;
    case D3D12_SHADER_VISIBILITY_MESH: return ShaderStageMask::Mesh;
    default: RDCERR("Unexpected visibility %u", ShaderVisibility); break;
  }

  return ShaderStageMask::Unknown;
}

// from PIXEventsCommon.h of winpixeventruntime
enum PIXEventType
{
  ePIXEvent_EndEvent = 0x000,
  ePIXEvent_BeginEvent_VarArgs = 0x001,
  ePIXEvent_BeginEvent_NoArgs = 0x002,
  ePIXEvent_SetMarker_VarArgs = 0x007,
  ePIXEvent_SetMarker_NoArgs = 0x008,

  ePIXEvent_EndEvent_OnContext = 0x010,
  ePIXEvent_BeginEvent_OnContext_VarArgs = 0x011,
  ePIXEvent_BeginEvent_OnContext_NoArgs = 0x012,
  ePIXEvent_SetMarker_OnContext_VarArgs = 0x017,
  ePIXEvent_SetMarker_OnContext_NoArgs = 0x018,
};

inline void PIX3DecodeEventInfo(const UINT64 BlobData, UINT64 &Timestamp, PIXEventType &EventType)
{
  static const UINT64 PIXEventsBlockEndMarker = 0x00000000000FFF80;

  static const UINT64 PIXEventsTypeReadMask = 0x00000000000FFC00;
  static const UINT64 PIXEventsTypeWriteMask = 0x00000000000003FF;
  static const UINT64 PIXEventsTypeBitShift = 10;

  static const UINT64 PIXEventsTimestampReadMask = 0xFFFFFFFFFFF00000;
  static const UINT64 PIXEventsTimestampWriteMask = 0x00000FFFFFFFFFFF;
  static const UINT64 PIXEventsTimestampBitShift = 20;

  Timestamp = (BlobData >> PIXEventsTimestampBitShift) & PIXEventsTimestampWriteMask;
  EventType = PIXEventType((BlobData >> PIXEventsTypeBitShift) & PIXEventsTypeWriteMask);
}

inline void PIX3DecodeStringInfo(const UINT64 BlobData, UINT64 &Alignment, UINT64 &CopyChunkSize,
                                 bool &IsANSI, bool &IsShortcut)
{
  static const UINT64 PIXEventsStringAlignmentWriteMask = 0x000000000000000F;
  static const UINT64 PIXEventsStringAlignmentReadMask = 0xF000000000000000;
  static const UINT64 PIXEventsStringAlignmentBitShift = 60;

  static const UINT64 PIXEventsStringCopyChunkSizeWriteMask = 0x000000000000001F;
  static const UINT64 PIXEventsStringCopyChunkSizeReadMask = 0x0F80000000000000;
  static const UINT64 PIXEventsStringCopyChunkSizeBitShift = 55;

  static const UINT64 PIXEventsStringIsANSIWriteMask = 0x0000000000000001;
  static const UINT64 PIXEventsStringIsANSIReadMask = 0x0040000000000000;
  static const UINT64 PIXEventsStringIsANSIBitShift = 54;

  static const UINT64 PIXEventsStringIsShortcutWriteMask = 0x0000000000000001;
  static const UINT64 PIXEventsStringIsShortcutReadMask = 0x0020000000000000;
  static const UINT64 PIXEventsStringIsShortcutBitShift = 53;

  Alignment = (BlobData >> PIXEventsStringAlignmentBitShift) & PIXEventsStringAlignmentWriteMask;
  CopyChunkSize =
      (BlobData >> PIXEventsStringCopyChunkSizeBitShift) & PIXEventsStringCopyChunkSizeWriteMask;
  IsANSI = (BlobData >> PIXEventsStringIsANSIBitShift) & PIXEventsStringIsANSIWriteMask;
  IsShortcut = (BlobData >> PIXEventsStringIsShortcutBitShift) & PIXEventsStringIsShortcutWriteMask;
}

const UINT64 *PIX3DecodeStringParam(const UINT64 *pData, rdcstr &DecodedString)
{
  UINT64 alignment;
  UINT64 copyChunkSize;
  bool isANSI;
  bool isShortcut;
  PIX3DecodeStringInfo(*pData, alignment, copyChunkSize, isANSI, isShortcut);
  ++pData;

  UINT totalStringBytes = 0;
  if(isANSI)
  {
    const char *c = (const char *)pData;
    UINT formatStringCharCount = UINT(strlen((const char *)pData));
    DecodedString = rdcstr(c, formatStringCharCount);
    totalStringBytes = formatStringCharCount + 1;
  }
  else
  {
    const wchar_t *w = (const wchar_t *)pData;
    UINT formatStringCharCount = UINT(wcslen((const wchar_t *)pData));
    DecodedString = StringFormat::Wide2UTF8(rdcwstr(w, formatStringCharCount));
    totalStringBytes = (formatStringCharCount + 1) * sizeof(wchar_t);
  }

  UINT64 byteChunks = ((totalStringBytes + copyChunkSize - 1) / copyChunkSize) * copyChunkSize;
  UINT64 stringQWordCount = (byteChunks + 7) / 8;
  pData += stringQWordCount;

  return pData;
}

rdcstr PIX3SprintfParams(const rdcstr &Format, const UINT64 *pData)
{
  rdcstr finalString;
  rdcstr formatPart;
  int32_t lastFind = 0;

  for(int32_t found = Format.indexOf('%'); found >= 0;)
  {
    finalString += Format.substr(lastFind, found - lastFind);

    int32_t endOfFormat = Format.find_first_of("%diufFeEgGxXoscpaAn", found + 1);
    if(endOfFormat < 0)
    {
      finalString += "<FORMAT_ERROR>";
      break;
    }

    formatPart = Format.substr(found, (endOfFormat - found) + 1);

    // strings
    if(formatPart.back() == 's')
    {
      rdcstr stringParam;
      pData = PIX3DecodeStringParam(pData, stringParam);
      finalString += stringParam;
    }
    // numerical values
    else
    {
      finalString += StringFormat::Fmt(formatPart.c_str(), *pData);
      ++pData;
    }

    lastFind = endOfFormat + 1;
    found = Format.indexOf('%', lastFind);
  }

  finalString += Format.substr(lastFind);

  return finalString;
}

rdcstr PIX3DecodeEventString(const UINT64 *pData, UINT64 &color)
{
  // event header
  UINT64 timestamp;
  PIXEventType eventType;
  PIX3DecodeEventInfo(*pData, timestamp, eventType);
  ++pData;

  // convert setmarker event types to beginevent event types because they're identical and it makes
  // for easier processing.
  if(eventType == ePIXEvent_SetMarker_NoArgs)
    eventType = ePIXEvent_BeginEvent_NoArgs;

  if(eventType == ePIXEvent_SetMarker_VarArgs)
    eventType = ePIXEvent_BeginEvent_VarArgs;

  if(eventType != ePIXEvent_BeginEvent_NoArgs && eventType != ePIXEvent_BeginEvent_VarArgs)
  {
    RDCERR("Unexpected/unsupported PIX3Event %u type in PIXDecodeMarkerEventString", eventType);
    return "";
  }

  // color
  color = *pData;
  ++pData;

  // format string
  rdcstr formatString;
  pData = PIX3DecodeStringParam(pData, formatString);

  if(eventType == ePIXEvent_BeginEvent_NoArgs)
    return formatString;

  // sprintf remaining args
  formatString = PIX3SprintfParams(formatString, pData);
  return formatString;
}

D3D12_SAMPLER_DESC2 ConvertStaticSampler(const D3D12_STATIC_SAMPLER_DESC1 &samp)
{
  D3D12_SAMPLER_DESC2 desc;
  desc.Filter = samp.Filter;
  desc.AddressU = samp.AddressU;
  desc.AddressV = samp.AddressV;
  desc.AddressW = samp.AddressW;
  desc.MipLODBias = samp.MipLODBias;
  desc.MaxAnisotropy = samp.MaxAnisotropy;
  desc.ComparisonFunc = samp.ComparisonFunc;
  switch(samp.BorderColor)
  {
    default:
    case D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK:
      desc.FloatBorderColor[0] = desc.FloatBorderColor[1] = desc.FloatBorderColor[2] =
          desc.FloatBorderColor[3] = 0.0f;
      break;
    case D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK:
      desc.FloatBorderColor[0] = desc.FloatBorderColor[1] = desc.FloatBorderColor[2] = 0.0f;
      desc.FloatBorderColor[3] = 1.0f;
      break;
    case D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE:
      desc.FloatBorderColor[0] = desc.FloatBorderColor[1] = desc.FloatBorderColor[2] =
          desc.FloatBorderColor[3] = 1.0f;
      break;
    case D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT:
      desc.UintBorderColor[0] = desc.UintBorderColor[1] = desc.UintBorderColor[2] = 0;
      desc.UintBorderColor[3] = 1;
      // this flag is optional in D3D, add it here to ensure we can check it elsewhere
      desc.Flags |= D3D12_SAMPLER_FLAG_UINT_BORDER_COLOR;
      break;
    case D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT:
      desc.UintBorderColor[0] = desc.UintBorderColor[1] = desc.UintBorderColor[2] =
          desc.UintBorderColor[3] = 1;
      // this flag is optional in D3D, add it here to ensure we can check it elsewhere
      desc.Flags |= D3D12_SAMPLER_FLAG_UINT_BORDER_COLOR;
      break;
  }
  desc.MinLOD = samp.MinLOD;
  desc.MaxLOD = samp.MaxLOD;
  desc.Flags = samp.Flags;
  return desc;
}

D3D12_DEPTH_STENCILOP_DESC1 Upconvert(const D3D12_DEPTH_STENCILOP_DESC &face)
{
  D3D12_DEPTH_STENCILOP_DESC1 ret = {};

  ret.StencilFunc = face.StencilFunc;
  ret.StencilPassOp = face.StencilPassOp;
  ret.StencilFailOp = face.StencilFailOp;
  ret.StencilDepthFailOp = face.StencilDepthFailOp;

  return ret;
}

D3D12_DEPTH_STENCILOP_DESC Downconvert(const D3D12_DEPTH_STENCILOP_DESC1 &face)
{
  D3D12_DEPTH_STENCILOP_DESC ret;

  ret.StencilFunc = face.StencilFunc;
  ret.StencilPassOp = face.StencilPassOp;
  ret.StencilFailOp = face.StencilFailOp;
  ret.StencilDepthFailOp = face.StencilDepthFailOp;

  return ret;
}

D3D12_DEPTH_STENCIL_DESC2 Upconvert(const D3D12_DEPTH_STENCIL_DESC1 &desc)
{
  D3D12_DEPTH_STENCIL_DESC2 DepthStencilState;

  DepthStencilState.DepthBoundsTestEnable = desc.DepthBoundsTestEnable;
  DepthStencilState.DepthEnable = desc.DepthEnable;
  DepthStencilState.DepthFunc = desc.DepthFunc;
  DepthStencilState.DepthWriteMask = desc.DepthWriteMask;
  DepthStencilState.StencilEnable = desc.StencilEnable;
  DepthStencilState.FrontFace = Upconvert(desc.FrontFace);
  DepthStencilState.BackFace = Upconvert(desc.BackFace);

  // duplicate this across both faces when it's not independent
  DepthStencilState.FrontFace.StencilReadMask = desc.StencilReadMask;
  DepthStencilState.FrontFace.StencilWriteMask = desc.StencilWriteMask;
  DepthStencilState.BackFace.StencilReadMask = desc.StencilReadMask;
  DepthStencilState.BackFace.StencilWriteMask = desc.StencilWriteMask;

  return DepthStencilState;
}

D3D12_RASTERIZER_DESC2 Upconvert(const D3D12_RASTERIZER_DESC &desc)
{
  D3D12_RASTERIZER_DESC2
  RasterizerState;

  RasterizerState.FillMode = desc.FillMode;
  RasterizerState.CullMode = desc.CullMode;
  RasterizerState.FrontCounterClockwise = desc.FrontCounterClockwise;
  RasterizerState.DepthBias = FLOAT(desc.DepthBias);
  RasterizerState.DepthBiasClamp = desc.DepthBiasClamp;
  RasterizerState.SlopeScaledDepthBias = desc.SlopeScaledDepthBias;
  RasterizerState.DepthClipEnable = desc.DepthClipEnable;
  RasterizerState.ForcedSampleCount = desc.ForcedSampleCount;
  RasterizerState.ConservativeRaster = desc.ConservativeRaster;

  if(desc.MultisampleEnable)
    RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_WIDE;
  else if(desc.AntialiasedLineEnable)
    RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALPHA_ANTIALIASED;
  else
    RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED;

  return RasterizerState;
}

D3D12_UNWRAPPED_STATE_OBJECT_DESC::D3D12_UNWRAPPED_STATE_OBJECT_DESC(
    const D3D12_STATE_OBJECT_DESC &wrappedDesc)
{
  Type = wrappedDesc.Type;
  NumSubobjects = wrappedDesc.NumSubobjects;

  size_t numRoots = 0, numColls = 0, numAssocs = 0;

  subobjects.resize(NumSubobjects);
  for(size_t i = 0; i < subobjects.size(); i++)
  {
    subobjects[i] = wrappedDesc.pSubobjects[i];
    if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE ||
       subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
    {
      numRoots++;
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
    {
      numColls++;
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
    {
      numAssocs++;
    }
  }

  unwrappedRootsigObjs.resize(numRoots);
  unwrappedCollObjs.resize(numColls);
  rebasedAssocs.reserve(numAssocs);

  for(size_t i = 0, r = 0, c = 0; i < subobjects.size(); i++)
  {
    if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE ||
       subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
    {
      D3D12_GLOBAL_ROOT_SIGNATURE *rootsig = (D3D12_GLOBAL_ROOT_SIGNATURE *)subobjects[i].pDesc;
      unwrappedRootsigObjs[r].pGlobalRootSignature = Unwrap(rootsig->pGlobalRootSignature);
      subobjects[i].pDesc = &unwrappedRootsigObjs[r++];
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
    {
      D3D12_EXISTING_COLLECTION_DESC *coll = (D3D12_EXISTING_COLLECTION_DESC *)subobjects[i].pDesc;
      unwrappedCollObjs[c] = *coll;
      unwrappedCollObjs[c].pExistingCollection = Unwrap(unwrappedCollObjs[c].pExistingCollection);
      subobjects[i].pDesc = &unwrappedCollObjs[c++];
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
    {
      D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION assoc =
          *(D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION *)subobjects[i].pDesc;

      size_t idx = assoc.pSubobjectToAssociate - wrappedDesc.pSubobjects;
      assoc.pSubobjectToAssociate = subobjects.data() + idx;
      rebasedAssocs.push_back(assoc);
      subobjects[i].pDesc = &rebasedAssocs.back();
    }
  }

  pSubobjects = subobjects.data();
}

D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC::D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC &graphics)
{
  pRootSignature = graphics.pRootSignature;
  VS = graphics.VS;
  PS = graphics.PS;
  DS = graphics.DS;
  HS = graphics.HS;
  GS = graphics.GS;
  StreamOutput = graphics.StreamOutput;
  BlendState = graphics.BlendState;
  SampleMask = graphics.SampleMask;

  {
    RasterizerState.FillMode = graphics.RasterizerState.FillMode;
    RasterizerState.CullMode = graphics.RasterizerState.CullMode;
    RasterizerState.FrontCounterClockwise = graphics.RasterizerState.FrontCounterClockwise;
    RasterizerState.DepthBias = FLOAT(graphics.RasterizerState.DepthBias);
    RasterizerState.DepthBiasClamp = graphics.RasterizerState.DepthBiasClamp;
    RasterizerState.SlopeScaledDepthBias = graphics.RasterizerState.SlopeScaledDepthBias;
    RasterizerState.DepthClipEnable = graphics.RasterizerState.DepthClipEnable;
    RasterizerState.ForcedSampleCount = graphics.RasterizerState.ForcedSampleCount;
    RasterizerState.ConservativeRaster = graphics.RasterizerState.ConservativeRaster;

    if(graphics.RasterizerState.MultisampleEnable)
      RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_WIDE;
    else if(graphics.RasterizerState.AntialiasedLineEnable)
      RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALPHA_ANTIALIASED;
    else
      RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED;
  }

  {
    DepthStencilState.DepthEnable = graphics.DepthStencilState.DepthEnable;
    DepthStencilState.DepthWriteMask = graphics.DepthStencilState.DepthWriteMask;
    DepthStencilState.DepthFunc = graphics.DepthStencilState.DepthFunc;
    DepthStencilState.StencilEnable = graphics.DepthStencilState.StencilEnable;

    DepthStencilState.FrontFace = Upconvert(graphics.DepthStencilState.FrontFace);
    DepthStencilState.BackFace = Upconvert(graphics.DepthStencilState.BackFace);

    // this is not separate, so duplicate it
    DepthStencilState.FrontFace.StencilReadMask = graphics.DepthStencilState.StencilReadMask;
    DepthStencilState.FrontFace.StencilWriteMask = graphics.DepthStencilState.StencilWriteMask;
    DepthStencilState.BackFace.StencilReadMask = graphics.DepthStencilState.StencilReadMask;
    DepthStencilState.BackFace.StencilWriteMask = graphics.DepthStencilState.StencilWriteMask;

    // DepthBounds defaults to disabled
    DepthStencilState.DepthBoundsTestEnable = FALSE;
  }
  InputLayout = graphics.InputLayout;
  IBStripCutValue = graphics.IBStripCutValue;
  PrimitiveTopologyType = graphics.PrimitiveTopologyType;
  RTVFormats.NumRenderTargets = graphics.NumRenderTargets;
  memcpy(RTVFormats.RTFormats, graphics.RTVFormats, 8 * sizeof(DXGI_FORMAT));
  DSVFormat = graphics.DSVFormat;
  SampleDesc = graphics.SampleDesc;
  NodeMask = graphics.NodeMask;
  CachedPSO = graphics.CachedPSO;
  Flags = graphics.Flags;

  // default state
  ViewInstancing.Flags = D3D12_VIEW_INSTANCING_FLAG_NONE;
  ViewInstancing.pViewInstanceLocations = NULL;
  ViewInstancing.ViewInstanceCount = 0;
}

D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC::D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC &compute)
{
  pRootSignature = compute.pRootSignature;
  CS = compute.CS;
  NodeMask = compute.NodeMask;
  CachedPSO = compute.CachedPSO;
  Flags = compute.Flags;
}

// this awkward construction is to account for UINT and pointer aligned data on both 32-bit and
// 64-bit.
struct D3D12_PSO_SUBOBJECT
{
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type;
};

struct D3D12_U32_PSO_SUBOBJECT
{
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type;

  union U32Data
  {
    UINT SampleMask;
    DXGI_FORMAT DSVFormat;
    UINT NodeMask;
    D3D12_BLEND_DESC BlendState;
    D3D12_RASTERIZER_DESC RasterizerState;
    D3D12_RASTERIZER_DESC1 RasterizerState1;
    D3D12_RASTERIZER_DESC2 RasterizerState2;
    D3D12_DEPTH_STENCIL_DESC DepthStencilState;
    D3D12_DEPTH_STENCIL_DESC1 DepthStencilState1;
    D3D12_DEPTH_STENCIL_DESC2 DepthStencilState2;
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
    D3D12_RT_FORMAT_ARRAY RTVFormats;
    DXGI_SAMPLE_DESC SampleDesc;
    D3D12_PIPELINE_STATE_FLAGS Flags;
  } data;
};

struct D3D12_PTR_PSO_SUBOBJECT
{
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type;

#if ENABLED(RDOC_X64)
  UINT padding;
#endif

  union PTRData
  {
    ID3D12RootSignature *pRootSignature;
    D3D12_SHADER_BYTECODE shader;
    D3D12_STREAM_OUTPUT_DESC StreamOutput;
    D3D12_INPUT_LAYOUT_DESC InputLayout;
    D3D12_CACHED_PIPELINE_STATE CachedPSO;
    D3D12_VIEW_INSTANCING_DESC ViewInstancing;
  } data;
};

D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC::D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(
    const D3D12_PIPELINE_STATE_STREAM_DESC &stream)
{
  // ensure data is naturally aligned.
  RDCCOMPILE_ASSERT(offsetof(D3D12_U32_PSO_SUBOBJECT, data.SampleMask) == 4,
                    "D3D12_U32_PSO_SUBOBJECT UINT data is misaligned");
  RDCCOMPILE_ASSERT(offsetof(D3D12_PTR_PSO_SUBOBJECT, data.pRootSignature) == sizeof(void *),
                    "D3D12_PTR_PSO_SUBOBJECT Pointer data is misaligned");
  RDCCOMPILE_ASSERT(offsetof(D3D12_PSO_SUBOBJECT, type) == 0,
                    "D3D12_PSO_SUBOBJECT type member is misaligned");
  RDCCOMPILE_ASSERT(offsetof(D3D12_U32_PSO_SUBOBJECT, type) == 0,
                    "D3D12_U32_PSO_SUBOBJECT type member is misaligned");
  RDCCOMPILE_ASSERT(offsetof(D3D12_PTR_PSO_SUBOBJECT, type) == 0,
                    "D3D12_PTR_PSO_SUBOBJECT type member is misaligned");

  // first set default state
  pRootSignature = NULL;
  RDCEraseEl(VS);
  RDCEraseEl(HS);
  RDCEraseEl(DS);
  RDCEraseEl(GS);
  RDCEraseEl(PS);
  RDCEraseEl(CS);
  RDCEraseEl(AS);
  RDCEraseEl(MS);
  NodeMask = 0;
  RDCEraseEl(CachedPSO);
  Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  SampleMask = ~0U;
  RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
  RasterizerState.FrontCounterClockwise = FALSE;
  RasterizerState.DepthBias = 0;
  RasterizerState.DepthBiasClamp = 0.0f;
  RasterizerState.SlopeScaledDepthBias = 0.0f;
  RasterizerState.DepthClipEnable = TRUE;
  RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED;
  RasterizerState.ForcedSampleCount = 0;
  RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  RDCEraseEl(StreamOutput);

  BlendState.AlphaToCoverageEnable = FALSE;
  BlendState.IndependentBlendEnable = FALSE;

  for(int i = 0; i < 8; i++)
  {
    BlendState.RenderTarget[i].BlendEnable = FALSE;
    BlendState.RenderTarget[i].LogicOpEnable = FALSE;
    BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
    BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
    BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
    BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
    BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
    BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
    BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  }

  {
    // Per D3D12 headers, depth is disabled if no DSV format is specified. We track this below
    // and enable depth if DSVFormat is specified without dpeth stencil state.
    DepthStencilState.DepthEnable = FALSE;
    DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    DepthStencilState.StencilEnable = FALSE;
    DepthStencilState.FrontFace.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    DepthStencilState.FrontFace.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;

    DepthStencilState.BackFace = DepthStencilState.FrontFace;

    // DepthBounds defaults to disabled
    DepthStencilState.DepthBoundsTestEnable = FALSE;
  }

  RDCEraseEl(InputLayout);
  IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  // Per D3D12 headers, if primitive topology is absent from the PSO stream, it defaults to triangle
  PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  RDCEraseEl(RTVFormats);
  DSVFormat = DXGI_FORMAT_UNKNOWN;
  SampleDesc.Count = 1;
  SampleDesc.Quality = 0;

  ViewInstancing.Flags = D3D12_VIEW_INSTANCING_FLAG_NONE;
  ViewInstancing.pViewInstanceLocations = NULL;
  ViewInstancing.ViewInstanceCount = 0;

  bool SeenDSS = false;

#define ITER_ADV(objtype)                    \
  iter = iter + sizeof(obj->type);           \
  iter = AlignUpPtr(iter, alignof(objtype)); \
  iter += sizeof(objtype);

  byte *iter = (byte *)stream.pPipelineStateSubobjectStream;
  byte *end = iter + stream.SizeInBytes;
  while(iter < end)
  {
    D3D12_PSO_SUBOBJECT *obj = (D3D12_PSO_SUBOBJECT *)iter;
    D3D12_U32_PSO_SUBOBJECT *u32 = (D3D12_U32_PSO_SUBOBJECT *)obj;
    D3D12_PTR_PSO_SUBOBJECT *ptr = (D3D12_PTR_PSO_SUBOBJECT *)obj;
    switch(obj->type)
    {
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:
      {
        pRootSignature = ptr->data.pRootSignature;
        ITER_ADV(ID3D12RootSignature *);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
      {
        VS = ptr->data.shader;
        ITER_ADV(D3D12_SHADER_BYTECODE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
      {
        PS = ptr->data.shader;
        ITER_ADV(D3D12_SHADER_BYTECODE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
      {
        HS = ptr->data.shader;
        ITER_ADV(D3D12_SHADER_BYTECODE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
      {
        DS = ptr->data.shader;
        ITER_ADV(D3D12_SHADER_BYTECODE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
      {
        GS = ptr->data.shader;
        ITER_ADV(D3D12_SHADER_BYTECODE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:
      {
        CS = ptr->data.shader;
        ITER_ADV(D3D12_SHADER_BYTECODE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:
      {
        AS = ptr->data.shader;
        ITER_ADV(D3D12_SHADER_BYTECODE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:
      {
        MS = ptr->data.shader;
        ITER_ADV(D3D12_SHADER_BYTECODE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
      {
        StreamOutput = ptr->data.StreamOutput;
        ITER_ADV(D3D12_STREAM_OUTPUT_DESC);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:
      {
        BlendState = u32->data.BlendState;
        ITER_ADV(D3D12_BLEND_DESC);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:
      {
        SampleMask = u32->data.SampleMask;
        ITER_ADV(UINT);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:
      {
        RasterizerState = Upconvert(u32->data.RasterizerState);

        ITER_ADV(D3D12_RASTERIZER_DESC);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER1:
      {
        RasterizerState.FillMode = u32->data.RasterizerState1.FillMode;
        RasterizerState.CullMode = u32->data.RasterizerState1.CullMode;
        RasterizerState.FrontCounterClockwise = u32->data.RasterizerState1.FrontCounterClockwise;
        RasterizerState.DepthBias = FLOAT(u32->data.RasterizerState1.DepthBias);
        RasterizerState.DepthBiasClamp = u32->data.RasterizerState1.DepthBiasClamp;
        RasterizerState.SlopeScaledDepthBias = u32->data.RasterizerState1.SlopeScaledDepthBias;
        RasterizerState.DepthClipEnable = u32->data.RasterizerState1.DepthClipEnable;
        RasterizerState.ForcedSampleCount = u32->data.RasterizerState1.ForcedSampleCount;
        RasterizerState.ConservativeRaster = u32->data.RasterizerState1.ConservativeRaster;

        if(u32->data.RasterizerState1.MultisampleEnable)
          RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_WIDE;
        else if(u32->data.RasterizerState1.AntialiasedLineEnable)
          RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALPHA_ANTIALIASED;
        else
          RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED;

        ITER_ADV(D3D12_RASTERIZER_DESC1);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER2:
      {
        RasterizerState = u32->data.RasterizerState2;

        ITER_ADV(D3D12_RASTERIZER_DESC2);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:
      {
        const D3D12_DEPTH_STENCIL_DESC &dsdesc = u32->data.DepthStencilState;
        DepthStencilState.DepthEnable = dsdesc.DepthEnable;
        DepthStencilState.DepthWriteMask = dsdesc.DepthWriteMask;
        DepthStencilState.DepthFunc = dsdesc.DepthFunc;
        DepthStencilState.StencilEnable = dsdesc.StencilEnable;
        DepthStencilState.FrontFace = Upconvert(dsdesc.FrontFace);
        DepthStencilState.BackFace = Upconvert(dsdesc.BackFace);

        // duplicate this across both faces when it's not independent
        DepthStencilState.FrontFace.StencilReadMask = dsdesc.StencilReadMask;
        DepthStencilState.FrontFace.StencilWriteMask = dsdesc.StencilWriteMask;
        DepthStencilState.BackFace.StencilReadMask = dsdesc.StencilReadMask;
        DepthStencilState.BackFace.StencilWriteMask = dsdesc.StencilWriteMask;
        SeenDSS = true;
        ITER_ADV(D3D12_DEPTH_STENCIL_DESC);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
      {
        InputLayout = ptr->data.InputLayout;
        ITER_ADV(D3D12_INPUT_LAYOUT_DESC);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:
      {
        IBStripCutValue = u32->data.IBStripCutValue;
        ITER_ADV(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:
      {
        PrimitiveTopologyType = u32->data.PrimitiveTopologyType;
        ITER_ADV(D3D12_PRIMITIVE_TOPOLOGY_TYPE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:
      {
        RTVFormats = u32->data.RTVFormats;
        ITER_ADV(D3D12_RT_FORMAT_ARRAY);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:
      {
        DSVFormat = u32->data.DSVFormat;
        if(!SeenDSS && DSVFormat != DXGI_FORMAT_UNKNOWN)
          DepthStencilState.DepthEnable = TRUE;

        ITER_ADV(DXGI_FORMAT);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:
      {
        SampleDesc = u32->data.SampleDesc;
        ITER_ADV(DXGI_SAMPLE_DESC);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:
      {
        NodeMask = u32->data.NodeMask;
        ITER_ADV(UINT);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:
      {
        CachedPSO = ptr->data.CachedPSO;
        ITER_ADV(D3D12_CACHED_PIPELINE_STATE);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:
      {
        Flags = u32->data.Flags;
        ITER_ADV(D3D12_PIPELINE_STATE_FLAGS);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:
      {
        DepthStencilState = Upconvert(u32->data.DepthStencilState1);

        SeenDSS = true;
        ITER_ADV(D3D12_DEPTH_STENCIL_DESC1);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2:
      {
        DepthStencilState = u32->data.DepthStencilState2;
        SeenDSS = true;
        ITER_ADV(D3D12_DEPTH_STENCIL_DESC2);
        break;
      }
      case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
      {
        ViewInstancing = ptr->data.ViewInstancing;
        ITER_ADV(D3D12_VIEW_INSTANCING_DESC);
        break;
      }
      default:
      {
        RDCERR("Unknown subobject type %d", obj->type);
        break;
      }
    }
    iter = AlignUpPtr(iter, sizeof(void *));
  }
}

void D3D12_PACKED_PIPELINE_STATE_STREAM_DESC::Unwrap()
{
  m_GraphicsStreamData.pRootSignature = ::Unwrap(m_GraphicsStreamData.pRootSignature);
  m_ComputeStreamData.pRootSignature = ::Unwrap(m_ComputeStreamData.pRootSignature);
}

D3D12_PACKED_PIPELINE_STATE_STREAM_DESC &D3D12_PACKED_PIPELINE_STATE_STREAM_DESC::operator=(
    const D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &expanded)
{
  if(expanded.CS.BytecodeLength > 0)
  {
    m_ComputeStreamData.pRootSignature = expanded.pRootSignature;
    m_ComputeStreamData.CS = expanded.CS;
    m_ComputeStreamData.NodeMask = expanded.NodeMask;
    m_ComputeStreamData.CachedPSO = expanded.CachedPSO;
    m_ComputeStreamData.Flags = expanded.Flags;
  }
  else
  {
    m_GraphicsStreamData.pRootSignature = expanded.pRootSignature;
    m_GraphicsStreamData.VS = expanded.VS;
    m_GraphicsStreamData.PS = expanded.PS;
    m_GraphicsStreamData.DS = expanded.DS;
    m_GraphicsStreamData.HS = expanded.HS;
    m_GraphicsStreamData.GS = expanded.GS;
    m_GraphicsStreamData.StreamOutput = expanded.StreamOutput;
    m_GraphicsStreamData.BlendState = expanded.BlendState;
    m_GraphicsStreamData.SampleMask = expanded.SampleMask;
    m_GraphicsStreamData.InputLayout = expanded.InputLayout;
    m_GraphicsStreamData.IBStripCutValue = expanded.IBStripCutValue;
    m_GraphicsStreamData.PrimitiveTopologyType = expanded.PrimitiveTopologyType;
    m_GraphicsStreamData.RTVFormats = expanded.RTVFormats;
    m_GraphicsStreamData.DSVFormat = expanded.DSVFormat;
    m_GraphicsStreamData.SampleDesc = expanded.SampleDesc;
    m_GraphicsStreamData.NodeMask = expanded.NodeMask;
    m_GraphicsStreamData.CachedPSO = expanded.CachedPSO;
    m_GraphicsStreamData.Flags = expanded.Flags;
    m_GraphicsStreamData.ViewInstancing = expanded.ViewInstancing;
    AS = expanded.AS;
    MS = expanded.MS;

    byte *ptr = m_GraphicsStreamData.VariableVersionedData;
    const byte *start = ptr;
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type;

#define WRITE_VERSIONED_SUBOJBECT(subobjType, subobj) \
  type = subobjType;                                  \
  memcpy(ptr, &type, sizeof(type));                   \
  ptr += sizeof(type);                                \
  ptr = AlignUpPtr(ptr, alignof(decltype(subobj)));   \
  memcpy(ptr, &subobj, sizeof(subobj));               \
  ptr += sizeof(subobj);                              \
  ptr = AlignUpPtr(ptr, sizeof(void *));

    // is the line rasterization mode narrow quadrilateral? if so we need version 2.
    if(expanded.RasterizerState.LineRasterizationMode ==
       D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_NARROW)
    {
      WRITE_VERSIONED_SUBOJBECT(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER2,
                                expanded.RasterizerState);
    }
    // otherwise is the depth bias not an int? then we need version 1
    else if(FLOAT(INT(expanded.RasterizerState.DepthBias)) != expanded.RasterizerState.DepthBias)
    {
      D3D12_RASTERIZER_DESC1 desc1;

      desc1.FillMode = expanded.RasterizerState.FillMode;
      desc1.CullMode = expanded.RasterizerState.CullMode;
      desc1.FrontCounterClockwise = expanded.RasterizerState.FrontCounterClockwise;
      desc1.DepthBias = expanded.RasterizerState.DepthBias;
      desc1.DepthBiasClamp = expanded.RasterizerState.DepthBiasClamp;
      desc1.SlopeScaledDepthBias = expanded.RasterizerState.SlopeScaledDepthBias;
      desc1.DepthClipEnable = expanded.RasterizerState.DepthClipEnable;
      desc1.ForcedSampleCount = expanded.RasterizerState.ForcedSampleCount;
      desc1.ConservativeRaster = expanded.RasterizerState.ConservativeRaster;

      switch(expanded.RasterizerState.LineRasterizationMode)
      {
        case D3D12_LINE_RASTERIZATION_MODE_ALIASED:
          desc1.MultisampleEnable = FALSE;
          desc1.AntialiasedLineEnable = FALSE;
          break;
        case D3D12_LINE_RASTERIZATION_MODE_ALPHA_ANTIALIASED:
          desc1.MultisampleEnable = FALSE;
          desc1.AntialiasedLineEnable = TRUE;
          break;
        case D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_WIDE:
        case D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_NARROW:
          desc1.MultisampleEnable = TRUE;
          desc1.AntialiasedLineEnable = FALSE;
          break;
        default:
          desc1.MultisampleEnable = FALSE;
          desc1.AntialiasedLineEnable = FALSE;
          break;
      }

      WRITE_VERSIONED_SUBOJBECT(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER1, desc1);
    }
    // if neither of those, we can use the old version
    else
    {
      D3D12_RASTERIZER_DESC desc;

      desc.FillMode = expanded.RasterizerState.FillMode;
      desc.CullMode = expanded.RasterizerState.CullMode;
      desc.FrontCounterClockwise = expanded.RasterizerState.FrontCounterClockwise;
      desc.DepthBias = INT(expanded.RasterizerState.DepthBias);
      desc.DepthBiasClamp = expanded.RasterizerState.DepthBiasClamp;
      desc.SlopeScaledDepthBias = expanded.RasterizerState.SlopeScaledDepthBias;
      desc.DepthClipEnable = expanded.RasterizerState.DepthClipEnable;
      desc.ForcedSampleCount = expanded.RasterizerState.ForcedSampleCount;
      desc.ConservativeRaster = expanded.RasterizerState.ConservativeRaster;

      switch(expanded.RasterizerState.LineRasterizationMode)
      {
        case D3D12_LINE_RASTERIZATION_MODE_ALIASED:
          desc.MultisampleEnable = FALSE;
          desc.AntialiasedLineEnable = FALSE;
          break;
        case D3D12_LINE_RASTERIZATION_MODE_ALPHA_ANTIALIASED:
          desc.MultisampleEnable = FALSE;
          desc.AntialiasedLineEnable = TRUE;
          break;
        case D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_WIDE:
        case D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_NARROW:
          desc.MultisampleEnable = TRUE;
          desc.AntialiasedLineEnable = FALSE;
          break;
        default:
          desc.MultisampleEnable = FALSE;
          desc.AntialiasedLineEnable = FALSE;
          break;
      }

      WRITE_VERSIONED_SUBOJBECT(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, desc);
    }

    // do we have separate stencil masks? if so use the new type of D/S desc. Otherwise use the old
    // one to ensure we don't fail when the new one isn't supported
    if(expanded.DepthStencilState.StencilEnable &&
       (expanded.DepthStencilState.FrontFace.StencilReadMask !=
            expanded.DepthStencilState.BackFace.StencilReadMask ||
        expanded.DepthStencilState.FrontFace.StencilWriteMask !=
            expanded.DepthStencilState.BackFace.StencilWriteMask))
    {
      WRITE_VERSIONED_SUBOJBECT(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2,
                                expanded.DepthStencilState);
    }
    else
    {
      D3D12_DEPTH_STENCIL_DESC1 desc1;

      desc1.DepthEnable = expanded.DepthStencilState.DepthEnable;
      desc1.DepthFunc = expanded.DepthStencilState.DepthFunc;
      desc1.DepthBoundsTestEnable = expanded.DepthStencilState.DepthBoundsTestEnable;
      desc1.DepthWriteMask = expanded.DepthStencilState.DepthWriteMask;
      desc1.StencilEnable = expanded.DepthStencilState.StencilEnable;
      desc1.FrontFace = Downconvert(expanded.DepthStencilState.FrontFace);
      desc1.BackFace = Downconvert(expanded.DepthStencilState.BackFace);
      desc1.StencilReadMask = expanded.DepthStencilState.FrontFace.StencilReadMask;
      desc1.StencilWriteMask = expanded.DepthStencilState.FrontFace.StencilWriteMask;

      WRITE_VERSIONED_SUBOJBECT(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1, desc1);
    }

    if(expanded.AS.BytecodeLength > 0)
    {
      WRITE_VERSIONED_SUBOJBECT(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, expanded.AS);
    }

    if(expanded.MS.BytecodeLength > 0)
    {
      WRITE_VERSIONED_SUBOJBECT(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, expanded.MS);
    }

    m_VariableVersionedDataLength = ptr - start;
  }

  return *this;
}

#if ENABLED(ENABLE_UNIT_TESTS)
#include "catch/catch.hpp"

#define INCLUDE_GPUADDRESS_HELPERS

#include "data/hlsl/hlsl_cbuffers.h"

GPUAddress toaddr(uint64_t addr)
{
  GPUAddress ret;
  RDCCOMPILE_ASSERT(sizeof(ret) == sizeof(addr), "GPU address isn't 64-bit");
  memcpy(&ret, &addr, sizeof(ret));
  return ret;
}

uint64_t fromaddr(GPUAddress addr)
{
  uint64_t ret;
  RDCCOMPILE_ASSERT(sizeof(ret) == sizeof(addr), "GPU address isn't 64-bit");
  memcpy(&ret, &addr, sizeof(ret));
  return ret;
}

TEST_CASE("HLSL uint64 helpers", "[d3d]")
{
  rdcarray<uint64_t> testValues = {
      0,
      1,
      2,
      3,
      4,
      5,
      6,
      7,
      8,
      9,
      10,
      11,
      100,
      128,
      1000,

      0xfffffffa,
      0xfffffffb,
      0xfffffffc,
      0xfffffffd,
      0xfffffffe,
      0xffffffff,

      0x100000000ULL,
      0x100000001ULL,
      0x100000002ULL,
      0x100000003ULL,
      0x100000004ULL,
      0x100000005ULL,
      0x100000006ULL,

      0x1000000000001000ULL,
      0x100000000fffffffULL,
      0x1000000010000000ULL,
      0x1000000010000001ULL,
      0x1000000010000002ULL,
      0x1000000010000002ULL,

      0x4000000000001000ULL,
      0x400000000fffffffULL,
      0x4000000010000000ULL,
      0x4000000010000001ULL,
      0x4000000010000002ULL,
      0x4000000010000002ULL,
      // don't test anything that could overflow if summed together for simplicity
  };

  for(uint64_t first : testValues)
  {
    for(uint64_t second : testValues)
    {
      GPUAddress a, b;
      a = toaddr(first);
      b = toaddr(second);

      // sanity check
      CHECK(fromaddr(a) == first);
      CHECK(fromaddr(b) == second);

      CHECK(lessThan(a, b) == (first < second));
      CHECK(lessEqual(a, b) == (first <= second));

      CHECK(lessThan(b, a) == (second < first));
      CHECK(lessEqual(b, a) == (second <= first));

      CHECK(fromaddr(add(a, b)) == (first + second));
      CHECK(fromaddr(add(b, a)) == (first + second));

      if(first >= second)
        CHECK(fromaddr(sub(a, b)) == (first - second));
      else
        CHECK(fromaddr(sub(b, a)) == (second - first));
    }
  }
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
