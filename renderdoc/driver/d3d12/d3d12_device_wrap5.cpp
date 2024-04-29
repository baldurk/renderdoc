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

    // TODO: here we would need to process the state descriptor into any internal replay
    // representation to know what's inside. We can also apply m_GlobalEXTUAV, m_GlobalEXTUAVSpace
    // for processing extensions in the DXBC files

    // unwrap the subobjects that need unwrapping

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
        // both structs are the same
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
                       "Capture requires ID3D12Device2 which isn't available");
      return false;
    }

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating pipeline state, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12StateObject(ret, this);

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
      GetResourceManager()->AddLiveResource(pStateObject, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateStateObject(const D3D12_STATE_OBJECT_DESC *pDesc, REFIID riid,
                                               _COM_Outptr_ void **ppStateObject)
{
  if(pDesc == NULL)
    return m_pDevice5->CreateStateObject(pDesc, riid, ppStateObject);

  D3D12_STATE_OBJECT_DESC unwrappedDesc = *pDesc;
  rdcarray<D3D12_STATE_SUBOBJECT> subobjects;
  subobjects.resize(unwrappedDesc.NumSubobjects);

  rdcarray<ID3D12RootSignature *> rootsigs;
  rdcarray<ID3D12StateObject *> collections;
  for(size_t i = 0; i < subobjects.size(); i++)
  {
    subobjects[i] = pDesc->pSubobjects[i];
    if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE ||
       subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
    {
      // both structs are the same
      D3D12_GLOBAL_ROOT_SIGNATURE *rootsig = (D3D12_GLOBAL_ROOT_SIGNATURE *)subobjects[i].pDesc;
      rootsigs.push_back(rootsig->pGlobalRootSignature);
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
    {
      D3D12_EXISTING_COLLECTION_DESC *coll = (D3D12_EXISTING_COLLECTION_DESC *)subobjects[i].pDesc;
      collections.push_back(coll->pExistingCollection);
    }
  }

  rdcarray<D3D12_GLOBAL_ROOT_SIGNATURE> rootsigObjs;
  rdcarray<D3D12_EXISTING_COLLECTION_DESC> collObjs;
  rootsigObjs.resize(rootsigs.size());
  collObjs.resize(collections.size());

  for(size_t i = 0, r = 0, c = 0; i < subobjects.size(); i++)
  {
    if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE ||
       subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
    {
      D3D12_GLOBAL_ROOT_SIGNATURE *rootsig = (D3D12_GLOBAL_ROOT_SIGNATURE *)subobjects[i].pDesc;
      rootsigObjs[r].pGlobalRootSignature = Unwrap(rootsig->pGlobalRootSignature);
      subobjects[i].pDesc = &rootsigObjs[r++];
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
    {
      D3D12_EXISTING_COLLECTION_DESC *coll = (D3D12_EXISTING_COLLECTION_DESC *)subobjects[i].pDesc;
      collObjs[c] = *coll;
      collObjs[c].pExistingCollection = Unwrap(collObjs[c].pExistingCollection);
    }
  }

  unwrappedDesc.pSubobjects = subobjects.data();

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

      D3D12ShaderExportDatabase *exports = wrapped->exports;

      // store the default local root signature - if we only find one in the whole state object then it becomes default
      ID3D12RootSignature *defaultRoot = NULL;
      bool unassocDefaultValid = false;
      bool explicitDefault = false;
      bool unassocDXILDefaultValid = false;
      uint32_t dxilDefaultRoot = ~0U;

      rdcarray<rdcpair<rdcstr, uint32_t>> explicitRootSigAssocs;
      rdcarray<rdcstr> explicitDefaultDxilAssocs;
      rdcarray<rdcpair<rdcstr, rdcstr>> explicitDxilAssocs;
      rdcflatmap<rdcstr, uint32_t> dxilLocalRootSigs;

      rdcarray<rdcpair<rdcstr, uint32_t>> inheritedRootSigAssocs;
      rdcarray<rdcpair<rdcstr, rdcstr>> inheritedDXILRootSigAssocs;
      rdcflatmap<rdcstr, uint32_t> inheritedDXILLocalRootSigs;

      // fill shader exports list as well as local root signature lookups.
      // shader exports that can be queried come from two sources:
      // - hit groups
      // - exports from a DXIL library
      // - exports from a collection
      for(size_t i = 0; i < subobjects.size(); i++)
      {
        if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP)
        {
          D3D12_HIT_GROUP_DESC *desc = (D3D12_HIT_GROUP_DESC *)subobjects[i].pDesc;
          exports->AddExport(StringFormat::Wide2UTF8(desc->HitGroupExport));

          rdcarray<rdcstr> shaders;
          if(desc->IntersectionShaderImport)
            shaders.push_back(StringFormat::Wide2UTF8(desc->IntersectionShaderImport));
          if(desc->AnyHitShaderImport)
            shaders.push_back(StringFormat::Wide2UTF8(desc->AnyHitShaderImport));
          if(desc->ClosestHitShaderImport)
            shaders.push_back(StringFormat::Wide2UTF8(desc->ClosestHitShaderImport));

          // register the hit group so that if we get associations with the individual shaders we
          // can apply that up to the hit group
          exports->AddLastHitGroupShaders(std::move(shaders));
        }
        else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY)
        {
          D3D12_DXIL_LIBRARY_DESC *dxil = (D3D12_DXIL_LIBRARY_DESC *)subobjects[i].pDesc;

          if(dxil->NumExports > 0)
          {
            for(UINT e = 0; e < dxil->NumExports; e++)
            {
              // Name is always the name used for exports - if renaming then the renamed-from name
              // is only used to lookup in the dxil library and not for any associations-by-name
              exports->AddExport(StringFormat::Wide2UTF8(dxil->pExports[e].Name));
            }
          }
          else
          {
            // hard part, we need to parse the DXIL to get the entry points
            DXBC::DXBCContainer container(
                bytebuf((byte *)dxil->DXILLibrary.pShaderBytecode, dxil->DXILLibrary.BytecodeLength),
                rdcstr(), GraphicsAPI::D3D12, ~0U, ~0U);

            rdcarray<ShaderEntryPoint> entries = container.GetEntryPoints();

            for(const ShaderEntryPoint &e : entries)
              exports->AddExport(e.name);
          }

          // TODO: register local root signature subobjects into dxilLocalRootSigs. Override
          // anything in there, unlike the import from a collection below.
        }
        else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
        {
          D3D12_EXISTING_COLLECTION_DESC *coll =
              (D3D12_EXISTING_COLLECTION_DESC *)subobjects[i].pDesc;

          WrappedID3D12StateObject *stateObj = (WrappedID3D12StateObject *)coll->pExistingCollection;

          if(coll->NumExports > 0)
          {
            for(UINT e = 0; e < coll->NumExports; e++)
              exports->InheritCollectionExport(
                  stateObj->exports, StringFormat::Wide2UTF8(coll->pExports[e].Name),
                  StringFormat::Wide2UTF8(coll->pExports[e].ExportToRename
                                              ? coll->pExports[e].ExportToRename
                                              : coll->pExports[e].Name));
          }
          else
          {
            exports->InheritAllCollectionExports(stateObj->exports);
          }

          // inherit explicit associations from the collection as lowest priority
          inheritedRootSigAssocs.append(stateObj->exports->danglingRootSigAssocs);
          inheritedDXILRootSigAssocs.append(stateObj->exports->danglingDXILRootSigAssocs);

          for(auto it = stateObj->exports->danglingDXILLocalRootSigs.begin();
              it != stateObj->exports->danglingDXILLocalRootSigs.end(); ++it)
          {
            // don't override any local root signatures with the same name we already have. Not sure
            // how this conflict should be resolved properly?
            if(dxilLocalRootSigs.find(it->first) == dxilLocalRootSigs.end())
              dxilLocalRootSigs[it->first] = it->second;
          }
        }
        else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
        {
          // ignore these if an explicit default association has been made
          if(!explicitDefault)
          {
            // if multiple root signatures are defined, then there can't be an unspecified default
            unassocDefaultValid = defaultRoot != NULL;
            defaultRoot = ((D3D12_LOCAL_ROOT_SIGNATURE *)subobjects[i].pDesc)->pLocalRootSignature;
          }
        }
        else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
        {
          D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION *assoc =
              (D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION *)subobjects[i].pDesc;

          const D3D12_STATE_SUBOBJECT *other = assoc->pSubobjectToAssociate;

          // only care about associating local root signatures
          if(other->Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
          {
            ID3D12RootSignature *root =
                ((D3D12_LOCAL_ROOT_SIGNATURE *)other->pDesc)->pLocalRootSignature;

            WrappedID3D12RootSignature *wrappedRoot = (WrappedID3D12RootSignature *)root;

            // if there are no exports this is an explicit default association. We assume this
            // matches and doesn't conflict
            if(assoc->NumExports == NULL)
            {
              explicitDefault = true;
              defaultRoot = root;
            }
            else
            {
              // otherwise record the explicit associations - these may refer to exports that
              // haven't been seen yet so we record them locally
              for(UINT e = 0; e < assoc->NumExports; e++)
                explicitRootSigAssocs.push_back(
                    {StringFormat::Wide2UTF8(assoc->pExports[e]), wrappedRoot->localRootSigIdx});
            }
          }
        }
        else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
        {
          D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION *assoc =
              (D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION *)subobjects[i].pDesc;

          rdcstr other = StringFormat::Wide2UTF8(assoc->SubobjectToAssociate);

          // we can't tell yet if this is a local root signature or not so we have to store it regardless
          {
            // if there are no exports this is an explicit default association, but we don't know if
            // it's for a local root signature...
            if(assoc->NumExports == NULL)
            {
              explicitDefaultDxilAssocs.push_back(other);
            }
            else
            {
              // otherwise record the explicit associations - these may refer to exports that
              // haven't been seen yet so we record them locally
              for(UINT e = 0; e < assoc->NumExports; e++)
                explicitDxilAssocs.push_back({StringFormat::Wide2UTF8(assoc->pExports[e]), other});
            }
          }
        }
      }

      // now that we have all exports registered, apply all associations we have in order of
      // priority to get the right

      for(size_t i = 0; i < explicitRootSigAssocs.size(); i++)
      {
        exports->ApplyRoot(SubObjectPriority::CodeExplicitAssociation,
                           explicitRootSigAssocs[i].first, explicitRootSigAssocs[i].second);
      }

      if(explicitDefault)
      {
        WrappedID3D12RootSignature *wrappedRoot = (WrappedID3D12RootSignature *)defaultRoot;

        exports->ApplyDefaultRoot(SubObjectPriority::CodeExplicitDefault,
                                  wrappedRoot->localRootSigIdx);
      }
      // shouldn't be possible to have both explicit and implicit defaults?
      else if(unassocDefaultValid)
      {
        WrappedID3D12RootSignature *wrappedRoot = (WrappedID3D12RootSignature *)defaultRoot;

        exports->ApplyDefaultRoot(SubObjectPriority::CodeImplicitDefault,
                                  wrappedRoot->localRootSigIdx);
      }

      for(size_t i = 0; i < explicitDxilAssocs.size(); i++)
      {
        auto it = dxilLocalRootSigs.find(explicitDxilAssocs[i].second);

        if(it == dxilLocalRootSigs.end())
          continue;

        uint32_t localRootSigIdx = it->second;

        exports->ApplyRoot(SubObjectPriority::DXILExplicitAssociation, explicitDxilAssocs[i].first,
                           localRootSigIdx);
      }

      for(size_t i = 0; i < explicitDefaultDxilAssocs.size(); i++)
      {
        auto it = dxilLocalRootSigs.find(explicitDefaultDxilAssocs[i]);

        if(it == dxilLocalRootSigs.end())
          continue;

        uint32_t localRootSigIdx = it->second;

        exports->ApplyDefaultRoot(SubObjectPriority::DXILExplicitDefault, localRootSigIdx);

        // only expect one local root signature - the array is because we can't tell the type of the
        // default subobject when we encounter it
        break;
      }

      if(unassocDXILDefaultValid)
      {
        exports->ApplyDefaultRoot(SubObjectPriority::DXILImplicitDefault, dxilDefaultRoot);
      }

      // we assume it's not possible to inherit two different explicit associations for a single export

      for(size_t i = 0; i < inheritedRootSigAssocs.size(); i++)
      {
        exports->ApplyRoot(SubObjectPriority::CollectionExplicitAssociation,
                           inheritedRootSigAssocs[i].first, inheritedRootSigAssocs[i].second);
      }
      for(size_t i = 0; i < inheritedDXILRootSigAssocs.size(); i++)
      {
        auto it = dxilLocalRootSigs.find(inheritedDXILRootSigAssocs[i].second);

        if(it == dxilLocalRootSigs.end())
          continue;

        uint32_t localRootSigIdx = it->second;

        exports->ApplyRoot(SubObjectPriority::CollectionExplicitAssociation,
                           inheritedDXILRootSigAssocs[i].first, localRootSigIdx);
      }

      exports->danglingRootSigAssocs.swap(inheritedRootSigAssocs);
      exports->danglingDXILRootSigAssocs.swap(inheritedDXILRootSigAssocs);
      exports->danglingDXILLocalRootSigs.swap(dxilLocalRootSigs);

      exports->UpdateHitGroupAssociations();

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_PipelineState;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      for(ID3D12RootSignature *sig : rootsigs)
        record->AddParent(GetRecord(sig));
      for(ID3D12StateObject *coll : collections)
        record->AddParent(GetRecord(coll));

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
