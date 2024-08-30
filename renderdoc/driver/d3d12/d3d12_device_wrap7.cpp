/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_AddToStateObject(SerialiserType &ser,
                                                     const D3D12_STATE_OBJECT_DESC *pAddition,
                                                     ID3D12StateObject *pStateObjectToGrowFrom,
                                                     REFIID riid,
                                                     _COM_Outptr_ void **ppNewStateObject)
{
  SERIALISE_ELEMENT_LOCAL(Addition, *pAddition);
  SERIALISE_ELEMENT(pStateObjectToGrowFrom).Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pNewStateObject,
                          ((WrappedID3D12StateObject *)*ppNewStateObject)->GetResourceID())
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

    const D3D12_STATE_SUBOBJECT *subs = Addition.pSubobjects;
    for(UINT i = 0; i < Addition.NumSubobjects; i++)
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

    if(m_pDevice7)
    {
      hr = m_pDevice7->AddToStateObject(&Addition, Unwrap(pStateObjectToGrowFrom), guid,
                                        (void **)&ret);
    }
    else
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12Device7 which isn't available");
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
      for(UINT i = 0, r = 0, c = 0; i < Addition.NumSubobjects; i++)
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
          pNewStateObject, GetResourceManager()->GetRaytracingResourceAndUtilHandler(),
          wrapped->GetProperties());

      wrapped->exports->GrowFrom(((WrappedID3D12StateObject *)pStateObjectToGrowFrom)->exports);
      wrapped->exports->PopulateDatabase(Addition.NumSubobjects, subs);

      AddResource(pNewStateObject, ResourceType::PipelineState, "State Object");
      for(ID3D12RootSignature *rootSig : rootSigs)
        DerivedResource(rootSig, pNewStateObject);
      for(ID3D12StateObject *coll : collections)
        DerivedResource(coll, pNewStateObject);

      // if this shader was initialised with nvidia's dynamic UAV, pull in that chunk as one of ours
      // and unset it (there will be one for each create that actually used vendor extensions)
      if(m_VendorEXT == GPUVendor::nVidia && m_GlobalEXTUAV != ~0U)
      {
        GetResourceDesc(pNewStateObject)
            .initialisationChunks.push_back((uint32_t)m_StructuredFile->chunks.size() - 2);
        m_GlobalEXTUAV = ~0U;
      }
      GetResourceManager()->AddLiveResource(pNewStateObject, wrapped);
    }
  }

  return true;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12Device::AddToStateObject(
    const D3D12_STATE_OBJECT_DESC *pAddition, ID3D12StateObject *pStateObjectToGrowFrom,
    REFIID riid, _COM_Outptr_ void **ppNewStateObject)
{
  if(pAddition == NULL)
    return m_pDevice7->AddToStateObject(pAddition, Unwrap(pStateObjectToGrowFrom), riid,
                                        ppNewStateObject);

  D3D12_UNWRAPPED_STATE_OBJECT_DESC unwrappedDesc(*pAddition);

  if(ppNewStateObject == NULL)
    return m_pDevice7->AddToStateObject(&unwrappedDesc, Unwrap(pStateObjectToGrowFrom), riid,
                                        ppNewStateObject);

  if(riid != __uuidof(ID3D12StateObject))
    return E_NOINTERFACE;

  ID3D12StateObject *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice7->AddToStateObject(
                          &unwrappedDesc, Unwrap(pStateObjectToGrowFrom), riid, (void **)&real));

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

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_AddToStateObject);
      Serialise_AddToStateObject(ser, pAddition, pStateObjectToGrowFrom, riid, (void **)&wrapped);

      wrapped->exports = new D3D12ShaderExportDatabase(
          wrapped->GetResourceID(), GetResourceManager()->GetRaytracingResourceAndUtilHandler(),
          wrapped->GetProperties());

      wrapped->exports->GrowFrom(((WrappedID3D12StateObject *)pStateObjectToGrowFrom)->exports);
      wrapped->exports->PopulateDatabase(pAddition->NumSubobjects, pAddition->pSubobjects);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_PipelineState;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      for(UINT i = 0; i < pAddition->NumSubobjects; i++)
      {
        if(pAddition->pSubobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE ||
           pAddition->pSubobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
        {
          // both structs are the same
          D3D12_GLOBAL_ROOT_SIGNATURE *rootsig =
              (D3D12_GLOBAL_ROOT_SIGNATURE *)pAddition->pSubobjects[i].pDesc;
          record->AddParent(GetRecord(rootsig->pGlobalRootSignature));
        }
        else if(pAddition->pSubobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
        {
          D3D12_EXISTING_COLLECTION_DESC *coll =
              (D3D12_EXISTING_COLLECTION_DESC *)pAddition->pSubobjects[i].pDesc;
          record->AddParent(GetRecord(coll->pExistingCollection));
        }
      }

      record->AddParent(GetRecord(pStateObjectToGrowFrom));

      if(vendorChunk)
        record->AddChunk(vendorChunk);
      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppNewStateObject = (ID3D12StateObject *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

HRESULT WrappedID3D12Device::CreateProtectedResourceSession1(
    _In_ const D3D12_PROTECTED_RESOURCE_SESSION_DESC1 *pDesc, _In_ REFIID riid,
    _COM_Outptr_ void **ppSession)
{
  if(ppSession == NULL)
    return m_pDevice7->CreateProtectedResourceSession1(pDesc, riid, NULL);

  if(riid != __uuidof(ID3D12ProtectedResourceSession) &&
     riid != __uuidof(ID3D12ProtectedResourceSession1) && riid != __uuidof(ID3D12ProtectedSession))
    return E_NOINTERFACE;

  ID3D12ProtectedResourceSession *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice7->CreateProtectedResourceSession1(
                          pDesc, __uuidof(ID3D12ProtectedResourceSession), (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12ProtectedResourceSession *wrapped =
        new WrappedID3D12ProtectedResourceSession(real, this);

    if(riid == __uuidof(ID3D12ProtectedResourceSession))
      *ppSession = (ID3D12ProtectedResourceSession *)wrapped;
    else if(riid == __uuidof(ID3D12ProtectedResourceSession1))
      *ppSession = (ID3D12ProtectedResourceSession1 *)wrapped;
    else if(riid == __uuidof(ID3D12ProtectedSession))
      *ppSession = (ID3D12ProtectedSession *)wrapped;
  }

  return ret;
}

INSTANTIATE_FUNCTION_SERIALISED(HRESULT, WrappedID3D12Device, AddToStateObject,
                                const D3D12_STATE_OBJECT_DESC *pAddition,
                                ID3D12StateObject *pStateObjectToGrowFrom, REFIID riid,
                                _COM_Outptr_ void **ppNewStateObject)
