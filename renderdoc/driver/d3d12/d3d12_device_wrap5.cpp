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

#include "d3d12_device.h"
#include "driver/dxgi/dxgi_common.h"
#include "d3d12_resources.h"

HRESULT WrappedID3D12Device::CreateLifetimeTracker(_In_ ID3D12LifetimeOwner *pOwner, REFIID riid,
                                                   _COM_Outptr_ void **ppvTracker)
{
  // without a spec it's really unclear how this is used.
  return E_NOINTERFACE;
}

void WrappedID3D12Device::RemoveDevice()
{
  return m_pDevice5->RemoveDevice();
}

HRESULT WrappedID3D12Device::EnumerateMetaCommands(_Inout_ UINT *pNumMetaCommands,
                                                   _Out_writes_opt_(*pNumMetaCommands)
                                                       D3D12_META_COMMAND_DESC *pDescs)
{
  // we pretend there are no meta commands, as we do not support capturing/replaying them
  UINT numCommands = 0;
  m_pDevice5->EnumerateMetaCommands(&numCommands, NULL);

  RDCLOG("Suppressing the report of %u meta commands", numCommands);

  if(pDescs)
    memset(pDescs, 0, sizeof(D3D12_META_COMMAND_DESC) * (*pNumMetaCommands));
  if(pNumMetaCommands)
    pNumMetaCommands = 0;

  return S_OK;
}

HRESULT WrappedID3D12Device::EnumerateMetaCommandParameters(
    _In_ REFGUID CommandId, _In_ D3D12_META_COMMAND_PARAMETER_STAGE Stage,
    _Out_opt_ UINT *pTotalStructureSizeInBytes, _Inout_ UINT *pParameterCount,
    _Out_writes_opt_(*pParameterCount) D3D12_META_COMMAND_PARAMETER_DESC *pParameterDescs)
{
  RDCERR("EnumerateMetaCommandParameters called but no meta commands reported!");
  return E_INVALIDARG;
}

HRESULT WrappedID3D12Device::CreateMetaCommand(_In_ REFGUID CommandId, _In_ UINT NodeMask,
                                               _In_reads_bytes_opt_(CreationParametersDataSizeInBytes)
                                                   const void *pCreationParametersData,
                                               _In_ SIZE_T CreationParametersDataSizeInBytes,
                                               REFIID riid, _COM_Outptr_ void **ppMetaCommand)
{
  RDCERR("CreateMetaCommand called but no meta commands reported!");
  return E_INVALIDARG;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateStateObject(SerialiserType &ser,
                                                      const D3D12_STATE_OBJECT_DESC *pDesc,
                                                      REFIID riid, _COM_Outptr_ void **ppStateObject)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pStateObject, ((WrappedID3D12StateObject *)*ppStateObject)->GetResourceID())
      .TypedAs("ID3D12StateObject *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12StateObject *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    // unwrap the subobjects that need unwrapping in-place. We'll undo these after creating the
    // object - this is probably better than unwrapping to a separate object since that requires
    // rebasing all the associations etc

    rdcarray<ID3D12RootSignature *> rootSigs;
    rdcarray<ID3D12StateObject *> collections;

    const D3D12_STATE_SUBOBJECT *subs = Descriptor.pSubobjects;
    for(UINT i = 0; i < Descriptor.NumSubobjects; i++)
    {
      if(subs[i].Type == D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE ||
         subs[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
      {
        // both structs are the same
        D3D12_GLOBAL_ROOT_SIGNATURE *global = (D3D12_GLOBAL_ROOT_SIGNATURE *)subs[i].pDesc;
        rootSigs.push_back(global->pGlobalRootSignature);
        global->pGlobalRootSignature = Unwrap(global->pGlobalRootSignature);
      }
      else if(subs[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
      {
        D3D12_EXISTING_COLLECTION_DESC *coll = (D3D12_EXISTING_COLLECTION_DESC *)subs[i].pDesc;
        collections.push_back(coll->pExistingCollection);
        coll->pExistingCollection = Unwrap(coll->pExistingCollection);
      }
    }

    m_UsedDXIL = true;

    if(m_pDevice5)
    {
      hr = m_pDevice5->CreateStateObject(&Descriptor, guid, (void **)&ret);
    }
    else
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12Device5 which isn't available");
      return false;
    }

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating state object, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      for(UINT i = 0, r = 0, c = 0; i < Descriptor.NumSubobjects; i++)
      {
        if(subs[i].Type == D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE ||
           subs[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
        {
          D3D12_GLOBAL_ROOT_SIGNATURE *global = (D3D12_GLOBAL_ROOT_SIGNATURE *)subs[i].pDesc;
          // the same order as above, we can consume the rootSigs in order
          global->pGlobalRootSignature = rootSigs[r++];
        }
        else if(subs[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
        {
          D3D12_EXISTING_COLLECTION_DESC *coll = (D3D12_EXISTING_COLLECTION_DESC *)subs[i].pDesc;
          coll->pExistingCollection = collections[c++];
        }
      }

      WrappedID3D12StateObject *wrapped = new WrappedID3D12StateObject(ret, this);

      // TODO: Apply m_GlobalEXTUAV, m_GlobalEXTUAVSpace for processing extensions in the DXBC files?

      wrapped->exports = new D3D12ShaderExportDatabase(
          pStateObject, GetResourceManager()->GetRaytracingResourceAndUtilHandler(),
          wrapped->GetProperties());

      wrapped->exports->PopulateDatabase(Descriptor.NumSubobjects, subs);

      AddResource(pStateObject, ResourceType::PipelineState, "State Object");
      for(ID3D12RootSignature *rootSig : rootSigs)
        DerivedResource(rootSig, pStateObject);
      for(ID3D12StateObject *coll : collections)
        DerivedResource(coll, pStateObject);

      // if this shader was initialised with nvidia's dynamic UAV, pull in that chunk as one of ours
      // and unset it (there will be one for each create that actually used vendor extensions)
      if(m_VendorEXT == GPUVendor::nVidia && m_GlobalEXTUAV != ~0U)
      {
        GetResourceDesc(pStateObject)
            .initialisationChunks.push_back((uint32_t)m_StructuredFile->chunks.size() - 2);
        m_GlobalEXTUAV = ~0U;
      }
      GetResourceManager()->AddLiveResource(pStateObject, wrapped);
    }
  }

  return true;
}

HRESULT
WrappedID3D12Device::CreateStateObject(const D3D12_STATE_OBJECT_DESC *pDesc, REFIID riid,
                                       _COM_Outptr_ void **ppStateObject)
{
  if(pDesc == NULL)
    return m_pDevice5->CreateStateObject(pDesc, riid, ppStateObject);

  D3D12_UNWRAPPED_STATE_OBJECT_DESC unwrappedDesc(*pDesc);

  if(ppStateObject == NULL)
    return m_pDevice5->CreateStateObject(&unwrappedDesc, riid, ppStateObject);

  if(riid != __uuidof(ID3D12StateObject))
    return E_NOINTERFACE;

  ID3D12StateObject *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice5->CreateStateObject(&unwrappedDesc, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12StateObject *wrapped = new WrappedID3D12StateObject(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      Chunk *vendorChunk = NULL;
      if(m_VendorEXT != GPUVendor::Unknown)
      {
        uint32_t reg = ~0U, space = ~0U;
        GetShaderExtUAV(reg, space);

        // TODO: detect use of shader extensions and serialise vendor chunk here
      }

      m_UsedDXIL = true;

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateStateObject);
      Serialise_CreateStateObject(ser, pDesc, riid, (void **)&wrapped);

      wrapped->exports = new D3D12ShaderExportDatabase(
          wrapped->GetResourceID(), GetResourceManager()->GetRaytracingResourceAndUtilHandler(),
          wrapped->GetProperties());

      wrapped->exports->PopulateDatabase(pDesc->NumSubobjects, pDesc->pSubobjects);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_PipelineState;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      for(UINT i = 0; i < pDesc->NumSubobjects; i++)
      {
        if(pDesc->pSubobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE ||
           pDesc->pSubobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
        {
          // both structs are the same
          D3D12_GLOBAL_ROOT_SIGNATURE *rootsig =
              (D3D12_GLOBAL_ROOT_SIGNATURE *)pDesc->pSubobjects[i].pDesc;
          record->AddParent(GetRecord(rootsig->pGlobalRootSignature));
        }
        else if(pDesc->pSubobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
        {
          D3D12_EXISTING_COLLECTION_DESC *coll =
              (D3D12_EXISTING_COLLECTION_DESC *)pDesc->pSubobjects[i].pDesc;
          record->AddParent(GetRecord(coll->pExistingCollection));
        }
      }

      if(vendorChunk)
        record->AddChunk(vendorChunk);
      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppStateObject = (ID3D12StateObject *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

void WrappedID3D12Device::GetRaytracingAccelerationStructurePrebuildInfo(
    _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS *pDesc,
    _Out_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO *pInfo)
{
  return m_pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(pDesc, pInfo);
}

D3D12_DRIVER_MATCHING_IDENTIFIER_STATUS WrappedID3D12Device::CheckDriverMatchingIdentifier(
    _In_ D3D12_SERIALIZED_DATA_TYPE SerializedDataType,
    _In_ const D3D12_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER *pIdentifierToCheck)
{
  // never allow the application to use serialised data
  return D3D12_DRIVER_MATCHING_IDENTIFIER_INCOMPATIBLE_VERSION;
}

INSTANTIATE_FUNCTION_SERIALISED(HRESULT, WrappedID3D12Device, CreateStateObject,
                                const D3D12_STATE_OBJECT_DESC *pDesc, REFIID riid,
                                _COM_Outptr_ void **ppStateObject)
