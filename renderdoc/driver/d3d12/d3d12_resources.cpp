/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

#include "d3d12_resources.h"
#include "3rdparty/lz4/lz4.h"
#include "driver/shaders/dxbc/dxbc_reflect.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"

GPUAddressRangeTracker WrappedID3D12Resource1::m_Addresses;
std::map<ResourceId, WrappedID3D12Resource1 *> *WrappedID3D12Resource1::m_List = NULL;
std::map<WrappedID3D12PipelineState::DXBCKey, WrappedID3D12Shader *> WrappedID3D12Shader::m_Shaders;
bool WrappedID3D12Shader::m_InternalResources = false;

const GUID RENDERDOC_ID3D12ShaderGUID_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

void WrappedID3D12Shader::TryReplaceOriginalByteCode()
{
  if(!DXBC::DXBCFile::CheckForDebugInfo((const void *)&m_Bytecode[0], m_Bytecode.size()))
  {
    std::string originalPath = m_DebugInfoPath;

    if(originalPath.empty())
      originalPath =
          DXBC::DXBCFile::GetDebugBinaryPath((const void *)&m_Bytecode[0], m_Bytecode.size());

    if(!originalPath.empty())
    {
      bool lz4 = false;

      if(!strncmp(originalPath.c_str(), "lz4#", 4))
      {
        originalPath = originalPath.substr(4);
        lz4 = true;
      }
      // could support more if we're willing to compile in the decompressor

      FILE *originalShaderFile = NULL;

      size_t numSearchPaths = m_DebugInfoSearchPaths ? m_DebugInfoSearchPaths->size() : 0;

      std::string foundPath;

      // while we haven't found a file, keep trying through the search paths. For i==0
      // check the path on its own, in case it's an absolute path.
      for(size_t i = 0; originalShaderFile == NULL && i <= numSearchPaths; i++)
      {
        if(i == 0)
        {
          originalShaderFile = FileIO::fopen(originalPath.c_str(), "rb");
          foundPath = originalPath;
          continue;
        }
        else
        {
          const std::string &searchPath = (*m_DebugInfoSearchPaths)[i - 1];
          foundPath = searchPath + "/" + originalPath;
          originalShaderFile = FileIO::fopen(foundPath.c_str(), "rb");
        }
      }

      if(originalShaderFile == NULL)
        return;

      FileIO::fseek64(originalShaderFile, 0L, SEEK_END);
      uint64_t originalShaderSize = FileIO::ftell64(originalShaderFile);
      FileIO::fseek64(originalShaderFile, 0, SEEK_SET);

      if(lz4 || originalShaderSize >= m_Bytecode.size())
      {
        std::vector<byte> originalBytecode;

        originalBytecode.resize((size_t)originalShaderSize);
        FileIO::fread(&originalBytecode[0], sizeof(byte), (size_t)originalShaderSize,
                      originalShaderFile);

        if(lz4)
        {
          std::vector<byte> decompressed;

          // first try decompressing to 1MB flat
          decompressed.resize(100 * 1024);

          int ret = LZ4_decompress_safe((const char *)&originalBytecode[0], (char *)&decompressed[0],
                                        (int)originalBytecode.size(), (int)decompressed.size());

          if(ret < 0)
          {
            // if it failed, either source is corrupt or we didn't allocate enough space.
            // Just allocate 255x compressed size since it can't need any more than that.
            decompressed.resize(255 * originalBytecode.size());

            ret = LZ4_decompress_safe((const char *)&originalBytecode[0], (char *)&decompressed[0],
                                      (int)originalBytecode.size(), (int)decompressed.size());

            if(ret < 0)
            {
              RDCERR("Failed to decompress LZ4 data from %s", foundPath.c_str());
              return;
            }
          }

          RDCASSERT(ret > 0, ret);

          // we resize and memcpy instead of just doing .swap() because that would
          // transfer over the over-large pessimistic capacity needed for decompression
          originalBytecode.resize(ret);
          memcpy(&originalBytecode[0], &decompressed[0], originalBytecode.size());
        }

        if(DXBC::DXBCFile::CheckForDebugInfo((const void *)&originalBytecode[0],
                                             originalBytecode.size()))
        {
          m_Bytecode.swap(originalBytecode);
        }
      }

      FileIO::fclose(originalShaderFile);
    }
  }
}

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface) WRAPPED_POOL_INST(CONCAT(Wrapped, iface));

ALL_D3D12_TYPES;

WRAPPED_POOL_INST(WrappedID3D12Shader);

D3D12ResourceType IdentifyTypeByPtr(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return Resource_Unknown;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)         \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return UnwrapHelper<iface>::GetTypeEnum();

  ALL_D3D12_TYPES;

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return Resource_GraphicsCommandList;
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return Resource_CommandQueue;

  RDCERR("Unknown type for ptr 0x%p", ptr);

  return Resource_Unknown;
}

TrackedResource12 *GetTracked(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return NULL;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)         \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return (TrackedResource12 *)GetWrapped((iface *)ptr);

  ALL_D3D12_TYPES;

  if(WrappedID3D12Shader::IsAlloc(ptr))
    return (TrackedResource12 *)(WrappedID3D12Shader *)ptr;

  return NULL;
}

template <>
ID3D12Object *Unwrap(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return NULL;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)         \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return (ID3D12Object *)GetWrapped((iface *)ptr)->GetReal();

  ALL_D3D12_TYPES;

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return (ID3D12Object *)(((WrappedID3D12GraphicsCommandList *)ptr)->GetReal());
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return (ID3D12Object *)(((WrappedID3D12CommandQueue *)ptr)->GetReal());

  RDCERR("Unknown type of ptr 0x%p", ptr);

  return NULL;
}

template <>
ResourceId GetResID(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return ResourceId();

  TrackedResource12 *res = GetTracked(ptr);

  if(res == NULL)
  {
    if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
      return ((WrappedID3D12GraphicsCommandList *)ptr)->GetResourceID();
    if(WrappedID3D12CommandQueue::IsAlloc(ptr))
      return ((WrappedID3D12CommandQueue *)ptr)->GetResourceID();

    RDCERR("Unknown type of ptr 0x%p", ptr);

    return ResourceId();
  }

  return res->GetResourceID();
}

template <>
D3D12ResourceRecord *GetRecord(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return NULL;

  TrackedResource12 *res = GetTracked(ptr);

  if(res == NULL)
  {
    if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
      return ((WrappedID3D12GraphicsCommandList *)ptr)->GetResourceRecord();
    if(WrappedID3D12CommandQueue::IsAlloc(ptr))
      return ((WrappedID3D12CommandQueue *)ptr)->GetResourceRecord();

    RDCERR("Unknown type of ptr 0x%p", ptr);

    return NULL;
  }

  return res->GetResourceRecord();
}

template <>
ResourceId GetResID(ID3D12DeviceChild *ptr)
{
  return GetResID((ID3D12Object *)ptr);
}

template <>
ResourceId GetResID(ID3D12Pageable *ptr)
{
  return GetResID((ID3D12Object *)ptr);
}

template <>
D3D12ResourceRecord *GetRecord(ID3D12DeviceChild *ptr)
{
  return GetRecord((ID3D12Object *)ptr);
}
template <>
ID3D12DeviceChild *Unwrap(ID3D12DeviceChild *ptr)
{
  return (ID3D12DeviceChild *)Unwrap((ID3D12Object *)ptr);
}

WrappedID3D12Resource1::~WrappedID3D12Resource1()
{
  SAFE_RELEASE(m_pReal1);

  // perform an implicit unmap on release
  if(GetResourceRecord())
  {
    D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;
    size_t mapcount = GetResourceRecord()->m_MapsCount;

    // may not have a map if e.g. no pointer was requested
    for(size_t i = 0; i < mapcount; i++)
    {
      if(map[i].refcount > 0)
      {
        m_pDevice->Unmap(this, (UINT)i, map[i].realPtr, NULL);

        FreeAlignedBuffer(map[i].shadowPtr);
        map[i].realPtr = NULL;
        map[i].shadowPtr = NULL;
      }
    }
  }

  if(m_List)
    (*m_List).erase(GetResourceID());

  // assuming only valid for buffers
  if(m_pReal->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    GPUAddressRange range;
    range.start = m_pReal->GetGPUVirtualAddress();
    range.end = m_pReal->GetGPUVirtualAddress() + m_pReal->GetDesc().Width;
    range.id = GetResourceID();

    m_Addresses.RemoveFrom(range);
  }

  Shutdown();

  m_ID = ResourceId();
}

byte *WrappedID3D12Resource1::GetMap(UINT Subresource)
{
  SCOPED_LOCK(GetResourceRecord()->m_MapLock);

  D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;
  size_t mapcount = GetResourceRecord()->m_MapsCount;

  if(Subresource < mapcount)
    return map[Subresource].realPtr;

  return NULL;
}

byte *WrappedID3D12Resource1::GetShadow(UINT Subresource)
{
  SCOPED_LOCK(GetResourceRecord()->m_MapLock);

  D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;

  return map[Subresource].shadowPtr;
}

void WrappedID3D12Resource1::AllocShadow(UINT Subresource, size_t size)
{
  SCOPED_LOCK(GetResourceRecord()->m_MapLock);

  D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;

  if(map[Subresource].shadowPtr == NULL)
    map[Subresource].shadowPtr = AllocAlignedBuffer(size);
}

void WrappedID3D12Resource1::FreeShadow()
{
  SCOPED_LOCK(GetResourceRecord()->m_MapLock);

  D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;
  size_t mapcount = GetResourceRecord()->m_MapsCount;

  for(size_t i = 0; i < mapcount; i++)
  {
    FreeAlignedBuffer(map[i].shadowPtr);
    map[i].shadowPtr = NULL;
  }
}

WriteSerialiser &WrappedID3D12Resource1::GetThreadSerialiser()
{
  return m_pDevice->GetThreadSerialiser();
}

HRESULT STDMETHODCALLTYPE WrappedID3D12Resource1::Map(UINT Subresource,
                                                      const D3D12_RANGE *pReadRange, void **ppData)
{
  // don't care about maps without returned pointers - we'll just intercept the WriteToSubresource
  // calls
  if(ppData == NULL)
    return m_pReal->Map(Subresource, pReadRange, ppData);

  void *mapPtr = NULL;

  // pass a NULL range as we might want to read from the whole range
  HRESULT hr = m_pReal->Map(Subresource, NULL, &mapPtr);

  *ppData = mapPtr;

  if(SUCCEEDED(hr) && GetResourceRecord())
  {
    SCOPED_LOCK(GetResourceRecord()->m_MapLock);

    D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;

    map[Subresource].realPtr = (byte *)mapPtr;
    map[Subresource].refcount++;

    // on the first map, register this so we can flush any updates in case it's left persistant
    if(map[Subresource].refcount == 1)
      m_pDevice->Map(this, Subresource);
  }

  return hr;
}

void STDMETHODCALLTYPE WrappedID3D12Resource1::Unmap(UINT Subresource,
                                                     const D3D12_RANGE *pWrittenRange)
{
  if(GetResourceRecord())
  {
    D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;

    {
      SCOPED_LOCK(GetResourceRecord()->m_MapLock);

      // may not have a ref at all if e.g. no pointer was requested
      if(map[Subresource].refcount >= 1)
      {
        map[Subresource].refcount--;

        if(map[Subresource].refcount == 0)
        {
          m_pDevice->Unmap(this, Subresource, map[Subresource].realPtr, pWrittenRange);

          FreeAlignedBuffer(map[Subresource].shadowPtr);
          map[Subresource].realPtr = NULL;
          map[Subresource].shadowPtr = NULL;
        }
      }
    }
  }

  return m_pReal->Unmap(Subresource, pWrittenRange);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12Resource1::WriteToSubresource(UINT DstSubresource,
                                                                     const D3D12_BOX *pDstBox,
                                                                     const void *pSrcData,
                                                                     UINT SrcRowPitch,
                                                                     UINT SrcDepthPitch)
{
  HRESULT ret;

  SERIALISE_TIME_CALL(ret = m_pReal->WriteToSubresource(DstSubresource, pDstBox, pSrcData,
                                                        SrcRowPitch, SrcDepthPitch));

  if(GetResourceRecord())
  {
    m_pDevice->WriteToSubresource(this, DstSubresource, pDstBox, pSrcData, SrcDepthPitch,
                                  SrcDepthPitch);
  }

  return ret;
}

void WrappedID3D12Resource1::RefBuffers(D3D12ResourceManager *rm)
{
  // only buffers go into m_Addresses
  SCOPED_READLOCK(m_Addresses.addressLock);
  for(size_t i = 0; i < m_Addresses.addresses.size(); i++)
    rm->MarkResourceFrameReferenced(m_Addresses.addresses[i].id, eFrameRef_Read);
}

WrappedID3D12DescriptorHeap::WrappedID3D12DescriptorHeap(ID3D12DescriptorHeap *real,
                                                         WrappedID3D12Device *device,
                                                         const D3D12_DESCRIPTOR_HEAP_DESC &desc)
    : WrappedDeviceChild12(real, device)
{
  realCPUBase = real->GetCPUDescriptorHandleForHeapStart();
  realGPUBase = real->GetGPUDescriptorHandleForHeapStart();

  SetResident(true);

  increment = device->GetUnwrappedDescriptorIncrement(desc.Type);
  numDescriptors = desc.NumDescriptors;

  descriptors = new D3D12Descriptor[numDescriptors];

  RDCEraseMem(descriptors, sizeof(D3D12Descriptor) * numDescriptors);
  for(UINT i = 0; i < numDescriptors; i++)
    descriptors[i].Setup(this, i);
}

WrappedID3D12DescriptorHeap::~WrappedID3D12DescriptorHeap()
{
  Shutdown();
  SAFE_DELETE_ARRAY(descriptors);
}

void WrappedID3D12PipelineState::ShaderEntry::BuildReflection()
{
  RDCCOMPILE_ASSERT(
      D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT == D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
      "Mismatched vertex input count");

  MakeShaderReflection(m_DXBCFile, &m_Details, &m_Mapping);
  m_Details.resourceId = GetResourceID();
}
