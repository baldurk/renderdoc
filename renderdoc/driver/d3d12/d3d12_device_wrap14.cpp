/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#include "d3d12_device.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/shaders/dxil/dxil_metadata.h"
#include "d3d12_command_queue.h"
#include "d3d12_resources.h"

static D3D12RootSignature DecodeRootSig(const void *data, size_t dataSize, rdcstr subobjectName)
{
  size_t chunkSize = 0;
  const byte *chunkData =
      DXBC::DXBCContainer::FindChunk((const byte *)data, dataSize, DXBC::FOURCC_RDAT, chunkSize);

  if(chunkSize > 0 && chunkData)
  {
    DXIL::RDATData rdat;
    DXBC::DXBCContainer::GetRuntimeData(chunkData, chunkSize, rdat);

    for(const DXIL::RDATData::SubobjectInfo &sub : rdat.subobjectsInfo)
    {
      if(sub.name == subobjectName)
      {
        if(sub.type == DXIL::RDATData::SubobjectInfo::SubobjectType::LocalRS ||
           sub.type == DXIL::RDATData::SubobjectInfo::SubobjectType::GlobalRS)
        {
          return DecodeRootSig(sub.rs.data.data(), sub.rs.data.size(), false);
        }
        else
        {
          RDCWARN("Subobject '%s' is not a root signature", subobjectName.c_str());
          return {};
        }
      }
    }

    RDCWARN("Subobject '%s' not found in library", subobjectName.c_str());
  }
  else
  {
    RDCWARN("Library blob does not contain RDAT");
  }
  return {};
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateRootSignatureFromSubobjectInLibrary(
    SerialiserType &ser, UINT nodeMask, const void *pLibraryBlob, SIZE_T blobLengthInBytes_,
    LPCWSTR subobjectName_, REFIID riid, void **ppvRootSignature)
{
  SERIALISE_ELEMENT(nodeMask);
  SERIALISE_ELEMENT_ARRAY(pLibraryBlob, blobLengthInBytes_).Important();
  SERIALISE_ELEMENT_LOCAL(blobLengthInBytes, uint64_t(blobLengthInBytes_));
  SERIALISE_ELEMENT_LOCAL(subobjectName, StringFormat::Wide2UTF8(subobjectName_));
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pRootSignature,
                          ((WrappedID3D12RootSignature *)*ppvRootSignature)->GetResourceID())
      .TypedAs("ID3D12RootSignature *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    nodeMask = 0;

    HRESULT hr = S_OK;
    ID3D12RootSignature *ret = NULL;
    if(m_pDevice14)
    {
      hr = m_pDevice14->CreateRootSignatureFromSubobjectInLibrary(
          nodeMask, pLibraryBlob, (SIZE_T)blobLengthInBytes,
          StringFormat::UTF82Wide(subobjectName).c_str(), guid, (void **)&ret);
    }
    else
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12Device14 which isn't available");
      return false;
    }

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating root signature, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      if(GetResourceManager()->HasWrapper(ret))
      {
        ret->Release();
        ret = (ID3D12RootSignature *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(pRootSignature, ret);
      }
      else
      {
        ret = new WrappedID3D12RootSignature(ret, this);

        GetResourceManager()->AddLiveResource(pRootSignature, ret);
      }

      WrappedID3D12RootSignature *wrapped = (WrappedID3D12RootSignature *)ret;

      wrapped->sig = DecodeRootSig(pLibraryBlob, (size_t)blobLengthInBytes, subobjectName);

      if(wrapped->sig.Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE)
        wrapped->localRootSigIdx =
            GetResourceManager()->GetRTManager()->RegisterLocalRootSig(wrapped->sig);

      {
        StructuredSerialiser structuriser(ser.GetStructuredFile().chunks.back(), &GetChunkName);
        structuriser.SetUserData(GetResourceManager());

        structuriser.Serialise("UnpackedSignature"_lit, wrapped->sig);
      }

      AddResource(pRootSignature, ResourceType::ShaderBinding, "Root Signature");
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateRootSignatureFromSubobjectInLibrary(
    UINT nodeMask, const void *pLibraryBlob, SIZE_T blobLengthInBytes, LPCWSTR subobjectName,
    REFIID riid, void **ppvRootSignature)
{
  if(ppvRootSignature == NULL)
    return m_pDevice14->CreateRootSignatureFromSubobjectInLibrary(
        nodeMask, pLibraryBlob, blobLengthInBytes, subobjectName, riid, NULL);

  if(riid != __uuidof(ID3D12RootSignature))
    return E_NOINTERFACE;

  ID3D12RootSignature *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice14->CreateRootSignatureFromSubobjectInLibrary(
          nodeMask, pLibraryBlob, blobLengthInBytes, subobjectName, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12RootSignature *wrapped = NULL;

    {
      SCOPED_LOCK(m_WrapDeduplicateLock);

      // duplicate signatures can be returned, if Create is called with a previous equivalent blob
      if(GetResourceManager()->HasWrapper(real))
      {
        real->Release();
        ID3D12RootSignature *existing = (ID3D12RootSignature *)GetResourceManager()->GetWrapper(real);
        existing->AddRef();
        *ppvRootSignature = existing;
        return ret;
      }

      wrapped = new WrappedID3D12RootSignature(real, this);
    }

    wrapped->sig =
        DecodeRootSig(pLibraryBlob, blobLengthInBytes, StringFormat::Wide2UTF8(subobjectName));

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateRootSignatureFromSubobjectInLibrary);
      Serialise_CreateRootSignatureFromSubobjectInLibrary(
          ser, nodeMask, pLibraryBlob, blobLengthInBytes, subobjectName, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_RootSignature;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      if(wrapped->sig.Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE)
        wrapped->localRootSigIdx =
            GetResourceManager()->GetRTManager()->RegisterLocalRootSig(wrapped->sig);

      if(!m_BindlessResourceUseActive)
      {
        // force ref-all-resources if the heap is directly indexed because we can't track resource
        // access
        if(wrapped->sig.Flags & (D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                                 D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED))
        {
          m_BindlessResourceUseActive = true;
          RDCDEBUG("Forcing Ref All Resources due to heap-indexing root signature flags");
        }
        else
        {
          for(const D3D12RootSignatureParameter &param : wrapped->sig.Parameters)
          {
            if(param.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
              continue;

            for(UINT r = 0; r < param.DescriptorTable.NumDescriptorRanges; r++)
            {
              const D3D12_DESCRIPTOR_RANGE1 &range = param.DescriptorTable.pDescriptorRanges[r];
              if(range.NumDescriptors > 100000)
              {
                m_BindlessResourceUseActive = true;
                RDCDEBUG(
                    "Forcing Ref All Resources due to large root signature range of %u descriptors "
                    "(space=%u, reg=%u, visibility=%s)",
                    range.NumDescriptors, range.RegisterSpace, range.BaseShaderRegister,
                    ToStr(param.ShaderVisibility).c_str());
                break;
              }
            }

            if(m_BindlessResourceUseActive)
              break;
          }
        }
      }

      record->AddChunk(scope.Get());
    }

    *ppvRootSignature = (ID3D12RootSignature *)wrapped;
  }
  else
  {
    CHECK_HR(this, ret);
  }

  return ret;
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateRootSignatureFromSubobjectInLibrary,
                                UINT nodeMask, const void *pLibraryBlob, SIZE_T blobLengthInBytes,
                                LPCWSTR subobjectName, REFIID riid, void **ppvRootSignature);
