/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"

std::vector<WrappedID3D12Resource::AddressRange> WrappedID3D12Resource::m_Addresses;
std::map<ResourceId, WrappedID3D12Resource *> WrappedID3D12Resource::m_List;
std::map<WrappedID3D12PipelineState::DXBCKey, WrappedID3D12PipelineState::ShaderEntry *>
    WrappedID3D12PipelineState::m_Shaders;

const GUID RENDERDOC_ID3D12ShaderGUID_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

void WrappedID3D12PipelineState::ShaderEntry::TryReplaceOriginalByteCode()
{
  if(!DXBC::DXBCFile::CheckForDebugInfo((const void *)&m_Bytecode[0], m_Bytecode.size()))
  {
    string originalPath = m_DebugInfoPath;

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

      string foundPath;

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
        vector<byte> originalBytecode;

        originalBytecode.resize((size_t)originalShaderSize);
        FileIO::fread(&originalBytecode[0], sizeof(byte), (size_t)originalShaderSize,
                      originalShaderFile);

        if(lz4)
        {
          vector<byte> decompressed;

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

WRAPPED_POOL_INST(WrappedID3D12PipelineState::ShaderEntry);

D3D12ResourceType IdentifyTypeByPtr(ID3D12DeviceChild *ptr)
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

TrackedResource *GetTracked(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return NULL;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)         \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return (TrackedResource *)GetWrapped((iface *)ptr);

  ALL_D3D12_TYPES;

  if(WrappedID3D12PipelineState::ShaderEntry::IsAlloc(ptr))
    return (TrackedResource *)(WrappedID3D12PipelineState::ShaderEntry *)ptr;

  return NULL;
}

template <>
ID3D12DeviceChild *Unwrap(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return NULL;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)         \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return (ID3D12DeviceChild *)GetWrapped((iface *)ptr)->GetReal();

  ALL_D3D12_TYPES;

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return (ID3D12DeviceChild *)(((WrappedID3D12GraphicsCommandList *)ptr)->GetReal());
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return (ID3D12DeviceChild *)(((WrappedID3D12CommandQueue *)ptr)->GetReal());

  RDCERR("Unknown type of ptr 0x%p", ptr);

  return NULL;
}

template <>
ResourceId GetResID(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return ResourceId();

  TrackedResource *res = GetTracked(ptr);

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
D3D12ResourceRecord *GetRecord(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return NULL;

  TrackedResource *res = GetTracked(ptr);

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

WrappedID3D12DescriptorHeap::WrappedID3D12DescriptorHeap(ID3D12DescriptorHeap *real,
                                                         WrappedID3D12Device *device,
                                                         const D3D12_DESCRIPTOR_HEAP_DESC &desc)
    : WrappedDeviceChild12(real, device)
{
  realCPUBase = real->GetCPUDescriptorHandleForHeapStart();
  realGPUBase = real->GetGPUDescriptorHandleForHeapStart();

  increment = device->GetUnwrappedDescriptorIncrement(desc.Type);
  numDescriptors = desc.NumDescriptors;

  descriptors = new D3D12Descriptor[numDescriptors];

  RDCEraseMem(descriptors, sizeof(D3D12Descriptor) * numDescriptors);
  for(UINT i = 0; i < numDescriptors; i++)
  {
    // only need to set this once, it's aliased between samp and nonsamp
    descriptors[i].samp.heap = this;
    descriptors[i].samp.idx = i;

    // initially descriptors are undefined. This way we just fill them with
    // some null SRV descriptor so it's safe to copy around etc but is no
    // less undefined for the application to use
    descriptors[i].nonsamp.type = D3D12Descriptor::TypeUndefined;
  }
}

WrappedID3D12DescriptorHeap::~WrappedID3D12DescriptorHeap()
{
  Shutdown();
  SAFE_DELETE_ARRAY(descriptors);
}
