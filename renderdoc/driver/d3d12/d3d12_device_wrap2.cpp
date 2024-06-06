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

static bool UsesExtensionUAV(const D3D12_SHADER_BYTECODE &sh, uint32_t reg, uint32_t space)
{
  return sh.BytecodeLength > 0 && sh.pShaderBytecode &&
         DXBC::DXBCContainer::UsesExtensionUAV(reg, space, sh.pShaderBytecode, sh.BytecodeLength);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreatePipelineState(SerialiserType &ser,
                                                        const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc,
                                                        REFIID riid, void **ppPipelineState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(*pDesc))
      .Named("pDesc"_lit)
      .Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pPipelineState,
                          ((WrappedID3D12PipelineState *)*ppPipelineState)->GetResourceID())
      .TypedAs("ID3D12PipelineState *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_PACKED_PIPELINE_STATE_STREAM_DESC unwrappedDesc(Descriptor);
    unwrappedDesc.Unwrap();

    ID3D12PipelineState *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    for(size_t i = 0; i < unwrappedDesc.GetStageCount(); i++)
    {
      D3D12_SHADER_BYTECODE &sh = unwrappedDesc.GetStage(i);

      if(sh.BytecodeLength == 0 || sh.pShaderBytecode == NULL)
        continue;

      if(!DXBC::DXBCContainer::IsHashedContainer(sh.pShaderBytecode, sh.BytecodeLength))
        DXBC::DXBCContainer::HashContainer((void *)sh.pShaderBytecode, sh.BytecodeLength);
      if(DXBC::DXBCContainer::CheckForDXIL(sh.pShaderBytecode, sh.BytecodeLength))
        m_UsedDXIL = true;
    }

    if(m_pDevice2)
    {
      hr = m_pDevice2->CreatePipelineState(unwrappedDesc.AsDescStream(), guid, (void **)&ret);
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
      ret = new WrappedID3D12PipelineState(ret, this);

      WrappedID3D12PipelineState *wrapped = (WrappedID3D12PipelineState *)ret;

      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC *storedDesc =
          new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(Descriptor);

      D3D12_SHADER_BYTECODE *shaders[] = {
          &storedDesc->VS, &storedDesc->HS, &storedDesc->DS, &storedDesc->GS,
          &storedDesc->PS, &storedDesc->CS, &storedDesc->AS, &storedDesc->MS,
      };

      AddResource(pPipelineState, ResourceType::PipelineState, "Pipeline State");
      DerivedResource(Descriptor.pRootSignature, pPipelineState);

      for(size_t i = 0; i < ARRAY_COUNT(shaders); i++)
      {
        if(shaders[i]->BytecodeLength == 0 || shaders[i]->pShaderBytecode == NULL)
        {
          shaders[i]->pShaderBytecode = NULL;
          shaders[i]->BytecodeLength = 0;
        }
        else
        {
          WrappedID3D12Shader *entry = WrappedID3D12Shader::AddShader(*shaders[i], this);
          entry->AddRef();

          shaders[i]->pShaderBytecode = entry;

          if(m_GlobalEXTUAV != ~0U)
            entry->SetShaderExtSlot(m_GlobalEXTUAV, m_GlobalEXTUAVSpace);

          AddResourceCurChunk(entry->GetResourceID());

          DerivedResource(entry->GetResourceID(), pPipelineState);
        }
      }

      if(storedDesc->CS.BytecodeLength > 0)
      {
        wrapped->compute = storedDesc;
      }
      else
      {
        wrapped->graphics = storedDesc;

        if(wrapped->graphics->InputLayout.NumElements)
        {
          wrapped->graphics->InputLayout.pInputElementDescs =
              new D3D12_INPUT_ELEMENT_DESC[wrapped->graphics->InputLayout.NumElements];
          memcpy((void *)wrapped->graphics->InputLayout.pInputElementDescs,
                 Descriptor.InputLayout.pInputElementDescs,
                 sizeof(D3D12_INPUT_ELEMENT_DESC) * wrapped->graphics->InputLayout.NumElements);
        }
        else
        {
          wrapped->graphics->InputLayout.pInputElementDescs = NULL;
        }

        if(wrapped->graphics->StreamOutput.NumEntries)
        {
          wrapped->graphics->StreamOutput.pSODeclaration =
              new D3D12_SO_DECLARATION_ENTRY[wrapped->graphics->StreamOutput.NumEntries];
          memcpy((void *)wrapped->graphics->StreamOutput.pSODeclaration,
                 Descriptor.StreamOutput.pSODeclaration,
                 sizeof(D3D12_SO_DECLARATION_ENTRY) * wrapped->graphics->StreamOutput.NumEntries);

          if(wrapped->graphics->StreamOutput.NumStrides)
          {
            wrapped->graphics->StreamOutput.pBufferStrides =
                new UINT[wrapped->graphics->StreamOutput.NumStrides];
            memcpy((void *)wrapped->graphics->StreamOutput.pBufferStrides,
                   Descriptor.StreamOutput.pBufferStrides,
                   sizeof(UINT) * wrapped->graphics->StreamOutput.NumStrides);
          }
          else
          {
            wrapped->graphics->StreamOutput.pBufferStrides = NULL;
          }
        }
        else
        {
          wrapped->graphics->StreamOutput.NumEntries = 0;
          wrapped->graphics->StreamOutput.NumStrides = 0;
          wrapped->graphics->StreamOutput.pSODeclaration = NULL;
          wrapped->graphics->StreamOutput.pBufferStrides = NULL;
        }

        if(wrapped->graphics->ViewInstancing.ViewInstanceCount)
        {
          wrapped->graphics->ViewInstancing.pViewInstanceLocations =
              new D3D12_VIEW_INSTANCE_LOCATION[wrapped->graphics->ViewInstancing.ViewInstanceCount];
          memcpy((void *)wrapped->graphics->ViewInstancing.pViewInstanceLocations,
                 Descriptor.ViewInstancing.pViewInstanceLocations,
                 sizeof(D3D12_VIEW_INSTANCE_LOCATION) *
                     wrapped->graphics->ViewInstancing.ViewInstanceCount);
        }
        else
        {
          wrapped->graphics->ViewInstancing.pViewInstanceLocations = NULL;
        }
      }

      wrapped->FetchRootSig(GetShaderCache());

      // if this shader was initialised with nvidia's dynamic UAV, pull in that chunk as one of ours
      // and unset it (there will be one for each create that actually used vendor extensions)
      if(m_VendorEXT == GPUVendor::nVidia && m_GlobalEXTUAV != ~0U)
      {
        GetResourceDesc(pPipelineState)
            .initialisationChunks.push_back((uint32_t)m_StructuredFile->chunks.size() - 2);
        m_GlobalEXTUAV = ~0U;
      }
      GetResourceManager()->AddLiveResource(pPipelineState, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc,
                                                 REFIID riid, void **ppPipelineState)
{
  if(pDesc == NULL)
    return m_pDevice3->CreatePipelineState(pDesc, riid, ppPipelineState);

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC expandedDesc = *pDesc;
  D3D12_PACKED_PIPELINE_STATE_STREAM_DESC unwrappedDesc = expandedDesc;
  unwrappedDesc.Unwrap();

  if(expandedDesc.errored)
    return E_INVALIDARG;

  if(ppPipelineState == NULL)
    return m_pDevice3->CreatePipelineState(unwrappedDesc.AsDescStream(), riid, ppPipelineState);

  if(riid != __uuidof(ID3D12PipelineState))
    return E_NOINTERFACE;

  ID3D12PipelineState *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice3->CreatePipelineState(unwrappedDesc.AsDescStream(), riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      Chunk *vendorChunk = NULL;
      if(m_VendorEXT != GPUVendor::Unknown)
      {
        uint32_t reg = ~0U, space = ~0U;
        GetShaderExtUAV(reg, space);

        if(UsesExtensionUAV(expandedDesc.VS, reg, space) ||
           UsesExtensionUAV(expandedDesc.HS, reg, space) ||
           UsesExtensionUAV(expandedDesc.DS, reg, space) ||
           UsesExtensionUAV(expandedDesc.GS, reg, space) ||
           UsesExtensionUAV(expandedDesc.PS, reg, space) ||
           UsesExtensionUAV(expandedDesc.CS, reg, space) ||
           UsesExtensionUAV(expandedDesc.AS, reg, space) ||
           UsesExtensionUAV(expandedDesc.MS, reg, space))
        {
          // don't set initparams until we've seen at least one shader actually created using the
          // extensions.
          m_InitParams.VendorExtensions = m_VendorEXT;

          // if this shader uses the UAV slot registered for vendor extensions, serialise that out
          // too
          SCOPED_SERIALISE_CHUNK(D3D12Chunk::SetShaderExtUAV);
          Serialise_SetShaderExtUAV(ser, m_VendorEXT, reg, space, true);
          vendorChunk = scope.Get();
        }
      }

      for(const D3D12_SHADER_BYTECODE &sh :
          {expandedDesc.VS, expandedDesc.HS, expandedDesc.DS, expandedDesc.GS, expandedDesc.PS,
           expandedDesc.CS, expandedDesc.AS, expandedDesc.MS})
      {
        if(sh.BytecodeLength == 0 || sh.pShaderBytecode == NULL)
          continue;

        if(DXBC::DXBCContainer::CheckForDXIL(sh.pShaderBytecode, sh.BytecodeLength))
          m_UsedDXIL = true;
      }

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreatePipelineState);
      Serialise_CreatePipelineState(ser, pDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_PipelineState;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      if(expandedDesc.pRootSignature)
        record->AddParent(GetRecord(expandedDesc.pRootSignature));

      if(vendorChunk)
        record->AddChunk(vendorChunk);
      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);

      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC *storedDesc =
          new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(expandedDesc);

      D3D12_SHADER_BYTECODE *shaders[] = {
          &storedDesc->VS, &storedDesc->HS, &storedDesc->DS, &storedDesc->GS,
          &storedDesc->PS, &storedDesc->CS, &storedDesc->AS, &storedDesc->MS,
      };

      for(size_t i = 0; i < ARRAY_COUNT(shaders); i++)
      {
        if(shaders[i]->BytecodeLength == 0 || shaders[i]->pShaderBytecode == NULL)
        {
          shaders[i]->pShaderBytecode = NULL;
          shaders[i]->BytecodeLength = 0;
        }
        else
        {
          WrappedID3D12Shader *sh = WrappedID3D12Shader::AddShader(*shaders[i], this);
          sh->AddRef();
          shaders[i]->pShaderBytecode = sh;
        }
      }

      if(storedDesc->CS.BytecodeLength > 0)
      {
        wrapped->compute = storedDesc;
      }
      else
      {
        wrapped->graphics = storedDesc;

        if(wrapped->graphics->InputLayout.NumElements)
        {
          wrapped->graphics->InputLayout.pInputElementDescs =
              new D3D12_INPUT_ELEMENT_DESC[wrapped->graphics->InputLayout.NumElements];
          memcpy((void *)wrapped->graphics->InputLayout.pInputElementDescs,
                 expandedDesc.InputLayout.pInputElementDescs,
                 sizeof(D3D12_INPUT_ELEMENT_DESC) * wrapped->graphics->InputLayout.NumElements);
        }
        else
        {
          wrapped->graphics->InputLayout.pInputElementDescs = NULL;
        }

        if(wrapped->graphics->StreamOutput.NumEntries)
        {
          wrapped->graphics->StreamOutput.pSODeclaration =
              new D3D12_SO_DECLARATION_ENTRY[wrapped->graphics->StreamOutput.NumEntries];
          memcpy((void *)wrapped->graphics->StreamOutput.pSODeclaration,
                 expandedDesc.StreamOutput.pSODeclaration,
                 sizeof(D3D12_SO_DECLARATION_ENTRY) * wrapped->graphics->StreamOutput.NumEntries);

          if(wrapped->graphics->StreamOutput.NumStrides)
          {
            wrapped->graphics->StreamOutput.pBufferStrides =
                new UINT[wrapped->graphics->StreamOutput.NumStrides];
            memcpy((void *)wrapped->graphics->StreamOutput.pBufferStrides,
                   expandedDesc.StreamOutput.pBufferStrides,
                   sizeof(UINT) * wrapped->graphics->StreamOutput.NumStrides);
          }
          else
          {
            wrapped->graphics->StreamOutput.NumStrides = 0;
            wrapped->graphics->StreamOutput.pBufferStrides = NULL;
          }
        }
        else
        {
          wrapped->graphics->StreamOutput.NumStrides = 0;
          wrapped->graphics->StreamOutput.NumEntries = 0;
          wrapped->graphics->StreamOutput.pSODeclaration = NULL;
          wrapped->graphics->StreamOutput.pBufferStrides = NULL;
        }

        if(wrapped->graphics->ViewInstancing.ViewInstanceCount)
        {
          wrapped->graphics->ViewInstancing.pViewInstanceLocations =
              new D3D12_VIEW_INSTANCE_LOCATION[wrapped->graphics->ViewInstancing.ViewInstanceCount];
          memcpy((void *)wrapped->graphics->ViewInstancing.pViewInstanceLocations,
                 expandedDesc.ViewInstancing.pViewInstanceLocations,
                 sizeof(D3D12_VIEW_INSTANCE_LOCATION) *
                     wrapped->graphics->ViewInstancing.ViewInstanceCount);
        }
        else
        {
          wrapped->graphics->ViewInstancing.pViewInstanceLocations = NULL;
        }
      }

      wrapped->FetchRootSig(GetShaderCache());
    }

    *ppPipelineState = (ID3D12PipelineState *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

INSTANTIATE_FUNCTION_SERIALISED(HRESULT, WrappedID3D12Device, CreatePipelineState,
                                const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid,
                                void **ppPipelineState);
