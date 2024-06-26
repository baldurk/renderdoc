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
#include "core/settings.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/ihv/amd/official/DXExt/AmdExtD3D.h"
#include "driver/ihv/amd/official/DXExt/AmdExtD3DCommandListMarkerApi.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_replay.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

RDOC_DEBUG_CONFIG(bool, D3D12_Experimental_EnableRTSupport, false,
                  "Enable support for experimental DXR support");

static bool UsesExtensionUAV(const D3D12_SHADER_BYTECODE &sh, uint32_t reg, uint32_t space)
{
  return sh.BytecodeLength > 0 && sh.pShaderBytecode &&
         DXBC::DXBCContainer::UsesExtensionUAV(reg, space, sh.pShaderBytecode, sh.BytecodeLength);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommandQueue(SerialiserType &ser,
                                                       const D3D12_COMMAND_QUEUE_DESC *pDesc,
                                                       REFIID riid, void **ppCommandQueue)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pCommandQueue,
                          ((WrappedID3D12CommandQueue *)*ppCommandQueue)->GetResourceID())
      .TypedAs("ID3D12CommandQueue *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12CommandQueue *ret = NULL;
    HRESULT hr = m_pDevice->CreateCommandQueue(&Descriptor, guid, (void **)&ret);

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating command queue, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      SetObjName(ret, StringFormat::Fmt("Command Queue %s", ToStr(pCommandQueue).c_str()));

      ret = new WrappedID3D12CommandQueue(ret, this, m_State);

      GetResourceManager()->AddLiveResource(pCommandQueue, ret);

      AddResource(pCommandQueue, ResourceType::Queue, "Command Queue");

      WrappedID3D12CommandQueue *wrapped = (WrappedID3D12CommandQueue *)ret;

      if(Descriptor.Type == D3D12_COMMAND_LIST_TYPE_DIRECT && m_Queue == NULL)
      {
        m_Queue = wrapped;
        // we hold an extra ref on this during capture to keep it alive, for simplicity match that
        // behaviour here.
        m_Queue->AddRef();
        CreateInternalResources();
      }

      m_Queues.push_back(wrapped);

      // create a dummy (dummy) fence
      ID3D12Fence *fence = NULL;
      hr = this->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&fence);

      RDCASSERTEQUAL(hr, S_OK);

      m_QueueFences.push_back(fence);
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID riid,
                                                void **ppCommandQueue)
{
  if(ppCommandQueue == NULL)
    return m_pDevice->CreateCommandQueue(pDesc, riid, NULL);

  if(riid != __uuidof(ID3D12CommandQueue))
    return E_NOINTERFACE;

  ID3D12CommandQueue *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateCommandQueue(pDesc, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12CommandQueue *wrapped = new WrappedID3D12CommandQueue(real, this, m_State);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateCommandQueue);
      Serialise_CreateCommandQueue(ser, pDesc, riid, (void **)&wrapped);

      wrapped->GetCreationRecord()->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    if(pDesc->Type == D3D12_COMMAND_LIST_TYPE_DIRECT && m_Queue == NULL)
    {
      m_Queue = wrapped;
      // keep this queue alive even if the application frees it, for our own use
      m_Queue->AddRef();
      InternalRef();
      CreateInternalResources();
    }

    m_Queues.push_back(wrapped);

    bool capframe = false;
    {
      SCOPED_READLOCK(m_CapTransitionLock);
      capframe = IsActiveCapturing(m_State);

      // while capturing don't allow any queues to be freed, by adding another refcount, since we
      // gather any commands submitted to them at the end of the capture.
      if(capframe)
      {
        wrapped->AddRef();
        m_RefQueues.push_back(wrapped);
      }
    }

    if(capframe)
    {
      GetResourceManager()->MarkResourceFrameReferenced(
          wrapped->GetCreationRecord()->GetResourceID(), eFrameRef_Read);
    }

    *ppCommandQueue = (ID3D12CommandQueue *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommandAllocator(SerialiserType &ser,
                                                           D3D12_COMMAND_LIST_TYPE type,
                                                           REFIID riid, void **ppCommandAllocator)
{
  SERIALISE_ELEMENT(type).Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pCommandAllocator,
                          ((WrappedID3D12CommandAllocator *)*ppCommandAllocator)->GetResourceID())
      .TypedAs("ID3D12CommandAllocator *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12CommandAllocator *ret = NULL;
    HRESULT hr = m_pDevice->CreateCommandAllocator(type, guid, (void **)&ret);

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating command allocator, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12CommandAllocator(ret, this);

      m_CommandAllocators.push_back(ret);

      GetResourceManager()->AddLiveResource(pCommandAllocator, ret);

      AddResource(pCommandAllocator, ResourceType::Pool, "Command Allocator");
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid,
                                                    void **ppCommandAllocator)
{
  if(ppCommandAllocator == NULL)
    return m_pDevice->CreateCommandAllocator(type, riid, NULL);

  if(riid != __uuidof(ID3D12CommandAllocator))
    return E_NOINTERFACE;

  ID3D12CommandAllocator *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateCommandAllocator(type, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12CommandAllocator *wrapped = new WrappedID3D12CommandAllocator(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateCommandAllocator);
      Serialise_CreateCommandAllocator(ser, type, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_CommandAllocator;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->cmdInfo = new CmdListRecordingInfo;

      record->cmdInfo->allocPool = new ChunkPagePool(32 * 1024);
      record->cmdInfo->alloc = new ChunkAllocator(*record->cmdInfo->allocPool);

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppCommandAllocator = (ID3D12CommandAllocator *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommandList(SerialiserType &ser, UINT nodeMask,
                                                      D3D12_COMMAND_LIST_TYPE type,
                                                      ID3D12CommandAllocator *pCommandAllocator,
                                                      ID3D12PipelineState *pInitialState,
                                                      REFIID riid, void **ppCommandList)
{
  SERIALISE_ELEMENT(nodeMask);
  SERIALISE_ELEMENT(type).Important();
  SERIALISE_ELEMENT(pCommandAllocator);
  SERIALISE_ELEMENT(pInitialState);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pCommandList,
                          ((WrappedID3D12GraphicsCommandList *)*ppCommandList)->GetResourceID())
      .TypedAs("ID3D12GraphicsCommandList *"_lit);

  // this chunk is purely for user information and consistency, the command buffer we allocate is
  // a dummy and is not used for anything.

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    nodeMask = 0;

    ID3D12GraphicsCommandList *list = NULL;
    HRESULT hr = CreateCommandList(nodeMask, type, pCommandAllocator, pInitialState,
                                   __uuidof(ID3D12GraphicsCommandList), (void **)&list);

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating command list, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else if(list)
    {
      // close it immediately, we don't want to tie up the allocator
      list->Close();

      GetResourceManager()->AddLiveResource(pCommandList, list);
    }

    AddResource(pCommandList, ResourceType::CommandBuffer, "Command List");
    DerivedResource(pCommandAllocator, pCommandList);
    if(pInitialState)
      DerivedResource(pInitialState, pCommandList);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
                                               ID3D12CommandAllocator *pCommandAllocator,
                                               ID3D12PipelineState *pInitialState, REFIID riid,
                                               void **ppCommandList)
{
  if(ppCommandList == NULL)
    return m_pDevice->CreateCommandList(nodeMask, type, Unwrap(pCommandAllocator),
                                        Unwrap(pInitialState), riid, NULL);

  if(riid != __uuidof(ID3D12GraphicsCommandList) && riid != __uuidof(ID3D12CommandList) &&
     riid != __uuidof(ID3D12GraphicsCommandList1) && riid != __uuidof(ID3D12GraphicsCommandList2) &&
     riid != __uuidof(ID3D12GraphicsCommandList3) && riid != __uuidof(ID3D12GraphicsCommandList4) &&
     riid != __uuidof(ID3D12GraphicsCommandList5) && riid != __uuidof(ID3D12GraphicsCommandList6) &&
     riid != __uuidof(ID3D12GraphicsCommandList7) && riid != __uuidof(ID3D12GraphicsCommandList8) &&
     riid != __uuidof(ID3D12GraphicsCommandList9))
    return E_NOINTERFACE;

  void *realptr = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateCommandList(
                          nodeMask, type, Unwrap(pCommandAllocator), Unwrap(pInitialState),
                          __uuidof(ID3D12GraphicsCommandList), &realptr));

  ID3D12GraphicsCommandList *real = NULL;

  if(riid == __uuidof(ID3D12CommandList))
    real = (ID3D12GraphicsCommandList *)(ID3D12CommandList *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList))
    real = (ID3D12GraphicsCommandList *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList1))
    real = (ID3D12GraphicsCommandList1 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList2))
    real = (ID3D12GraphicsCommandList2 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList3))
    real = (ID3D12GraphicsCommandList3 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList4))
    real = (ID3D12GraphicsCommandList4 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList5))
    real = (ID3D12GraphicsCommandList5 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList6))
    real = (ID3D12GraphicsCommandList6 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList7))
    real = (ID3D12GraphicsCommandList7 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList8))
    real = (ID3D12GraphicsCommandList8 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList9))
    real = (ID3D12GraphicsCommandList9 *)realptr;

  if(SUCCEEDED(ret))
  {
    WrappedID3D12GraphicsCommandList *wrapped =
        new WrappedID3D12GraphicsCommandList(real, this, m_State);

    if(m_pAMDExtObject)
    {
      IAmdExtD3DCommandListMarker *markers = NULL;
      m_pAMDExtObject->CreateInterface(real, __uuidof(IAmdExtD3DCommandListMarker),
                                       (void **)&markers);
      wrapped->SetAMDMarkerInterface(markers);
    }

    if(IsCaptureMode(m_State))
    {
      wrapped->SetInitParams(riid, nodeMask, type);
      // we just serialise out command allocator creation as a reset, since it's equivalent.
      wrapped->ResetInternal(pCommandAllocator, pInitialState, true);

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateCommandList);
        Serialise_CreateCommandList(ser, nodeMask, type, pCommandAllocator, pInitialState, riid,
                                    (void **)&wrapped);

        wrapped->GetCreationRecord()->AddChunk(scope.Get());
      }

      // add parents so these are always in the capture. They won't necessarily be used for replay
      // but we want them to be available so the creation chunk is fully realised
      wrapped->GetCreationRecord()->AddParent(GetRecord(pCommandAllocator));
      if(pInitialState)
        wrapped->GetCreationRecord()->AddParent(GetRecord(pInitialState));
    }

    // during replay, the caller is responsible for calling AddLiveResource as this function
    // can be called from ID3D12GraphicsCommandList::Reset serialising

    if(riid == __uuidof(ID3D12GraphicsCommandList))
      *ppCommandList = (ID3D12GraphicsCommandList *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList1))
      *ppCommandList = (ID3D12GraphicsCommandList1 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList2))
      *ppCommandList = (ID3D12GraphicsCommandList2 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList3))
      *ppCommandList = (ID3D12GraphicsCommandList3 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList4))
      *ppCommandList = (ID3D12GraphicsCommandList4 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList5))
      *ppCommandList = (ID3D12GraphicsCommandList5 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList6))
      *ppCommandList = (ID3D12GraphicsCommandList6 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList7))
      *ppCommandList = (ID3D12GraphicsCommandList7 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList8))
      *ppCommandList = (ID3D12GraphicsCommandList8 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList9))
      *ppCommandList = (ID3D12GraphicsCommandList9 *)wrapped;
    else if(riid == __uuidof(ID3D12CommandList))
      *ppCommandList = (ID3D12CommandList *)wrapped;
    else
      RDCERR("Unexpected riid! %s", ToStr(riid).c_str());
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateGraphicsPipelineState(
    SerialiserType &ser, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid,
    void **ppPipelineState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pPipelineState,
                          ((WrappedID3D12PipelineState *)*ppPipelineState)->GetResourceID())
      .TypedAs("ID3D12PipelineState *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC unwrappedDesc = Descriptor;
    unwrappedDesc.pRootSignature = Unwrap(unwrappedDesc.pRootSignature);

    {
      D3D12_SHADER_BYTECODE *shaders[] = {
          &Descriptor.VS, &Descriptor.HS, &Descriptor.DS, &Descriptor.GS, &Descriptor.PS,
      };

      for(size_t i = 0; i < ARRAY_COUNT(shaders); i++)
      {
        if(shaders[i]->BytecodeLength == 0 || shaders[i]->pShaderBytecode == NULL)
          continue;

        // add any missing hashes ourselves. This probably comes from a capture with experimental
        // enabled so it can load unhashed, but we want to be more proactive
        if(!DXBC::DXBCContainer::IsHashedContainer(shaders[i]->pShaderBytecode,
                                                   shaders[i]->BytecodeLength))
          DXBC::DXBCContainer::HashContainer((void *)shaders[i]->pShaderBytecode,
                                             shaders[i]->BytecodeLength);
      }
    }

    ID3D12PipelineState *ret = NULL;
    HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&unwrappedDesc, guid, (void **)&ret);

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating graphics pipeline, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12PipelineState(ret, this);

      WrappedID3D12PipelineState *wrapped = (WrappedID3D12PipelineState *)ret;

      wrapped->graphics = new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(Descriptor);

      wrapped->FetchRootSig(GetShaderCache());

      D3D12_SHADER_BYTECODE *shaders[] = {
          &wrapped->graphics->VS, &wrapped->graphics->HS, &wrapped->graphics->DS,
          &wrapped->graphics->GS, &wrapped->graphics->PS,
      };

      AddResource(pPipelineState, ResourceType::PipelineState, "Graphics Pipeline State");
      if(Descriptor.pRootSignature)
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
        wrapped->graphics->StreamOutput.pSODeclaration = NULL;
        wrapped->graphics->StreamOutput.pBufferStrides = NULL;
      }

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

void WrappedID3D12Device::ProcessCreatedGraphicsPSO(ID3D12PipelineState *real,
                                                    uint32_t vendorExtReg, uint32_t vendorExtSpace,
                                                    const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc,
                                                    REFIID riid, void **ppPipelineState)
{
  for(const D3D12_SHADER_BYTECODE &sh : {pDesc->VS, pDesc->HS, pDesc->DS, pDesc->GS, pDesc->PS})
  {
    if(sh.BytecodeLength > 0 && sh.pShaderBytecode &&
       DXBC::DXBCContainer::CheckForDXIL(sh.pShaderBytecode, sh.BytecodeLength))
      m_UsedDXIL = true;
  }

  WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(real, this);

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();

    Chunk *vendorChunk = NULL;
    if(m_VendorEXT != GPUVendor::Unknown)
    {
      if(UsesExtensionUAV(pDesc->VS, vendorExtReg, vendorExtSpace) ||
         UsesExtensionUAV(pDesc->HS, vendorExtReg, vendorExtSpace) ||
         UsesExtensionUAV(pDesc->DS, vendorExtReg, vendorExtSpace) ||
         UsesExtensionUAV(pDesc->GS, vendorExtReg, vendorExtSpace) ||
         UsesExtensionUAV(pDesc->PS, vendorExtReg, vendorExtSpace))
      {
        // don't set initparams until we've seen at least one shader actually created using the
        // extensions.
        m_InitParams.VendorExtensions = m_VendorEXT;

        // if this shader uses the UAV slot registered for vendor extensions, serialise that out
        // too
        SCOPED_SERIALISE_CHUNK(D3D12Chunk::SetShaderExtUAV);
        Serialise_SetShaderExtUAV(ser, m_VendorEXT, vendorExtReg, vendorExtSpace, true);
        vendorChunk = scope.Get();
      }
    }

    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateGraphicsPipeline);
    Serialise_CreateGraphicsPipelineState(ser, pDesc, riid, (void **)&wrapped);

    D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
    record->type = Resource_PipelineState;
    record->Length = 0;
    wrapped->SetResourceRecord(record);

    if(pDesc->pRootSignature)
      record->AddParent(GetRecord(pDesc->pRootSignature));

    if(vendorChunk)
      record->AddChunk(vendorChunk);
    record->AddChunk(scope.Get());
  }
  else
  {
    GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);

    wrapped->graphics = new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(*pDesc);

    wrapped->FetchRootSig(GetShaderCache());

    D3D12_SHADER_BYTECODE *shaders[] = {
        &wrapped->graphics->VS, &wrapped->graphics->HS, &wrapped->graphics->DS,
        &wrapped->graphics->GS, &wrapped->graphics->PS, &wrapped->graphics->AS,
        &wrapped->graphics->MS,
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
        if(m_GlobalEXTUAV != ~0U)
          sh->SetShaderExtSlot(m_GlobalEXTUAV, m_GlobalEXTUAVSpace);
        shaders[i]->pShaderBytecode = sh;
      }
    }

    if(wrapped->graphics->InputLayout.NumElements)
    {
      wrapped->graphics->InputLayout.pInputElementDescs =
          new D3D12_INPUT_ELEMENT_DESC[wrapped->graphics->InputLayout.NumElements];
      memcpy((void *)wrapped->graphics->InputLayout.pInputElementDescs,
             pDesc->InputLayout.pInputElementDescs,
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
             pDesc->StreamOutput.pSODeclaration,
             sizeof(D3D12_SO_DECLARATION_ENTRY) * wrapped->graphics->StreamOutput.NumEntries);
    }
    else
    {
      wrapped->graphics->StreamOutput.pSODeclaration = NULL;
    }

    if(wrapped->graphics->StreamOutput.NumStrides)
    {
      wrapped->graphics->StreamOutput.pBufferStrides =
          new UINT[wrapped->graphics->StreamOutput.NumStrides];
      memcpy((void *)wrapped->graphics->StreamOutput.pBufferStrides,
             pDesc->StreamOutput.pBufferStrides,
             sizeof(UINT) * wrapped->graphics->StreamOutput.NumStrides);
    }
    else
    {
      wrapped->graphics->StreamOutput.pBufferStrides = NULL;
    }
  }

  *ppPipelineState = (ID3D12PipelineState *)wrapped;
}

HRESULT WrappedID3D12Device::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc,
                                                         REFIID riid, void **ppPipelineState)
{
  D3D12_GRAPHICS_PIPELINE_STATE_DESC unwrappedDesc = *pDesc;
  unwrappedDesc.pRootSignature = Unwrap(unwrappedDesc.pRootSignature);

  if(ppPipelineState == NULL)
    return m_pDevice->CreateGraphicsPipelineState(&unwrappedDesc, riid, NULL);

  if(riid != __uuidof(ID3D12PipelineState))
    return E_NOINTERFACE;

  ID3D12PipelineState *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice->CreateGraphicsPipelineState(&unwrappedDesc, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    // use implicit register/space
    uint32_t reg = ~0U, space = ~0U;
    if(m_VendorEXT != GPUVendor::Unknown)
      GetShaderExtUAV(reg, space);

    ProcessCreatedGraphicsPSO(real, reg, space, pDesc, riid, ppPipelineState);
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateComputePipelineState(
    SerialiserType &ser, const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid,
    void **ppPipelineState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pPipelineState,
                          ((WrappedID3D12PipelineState *)*ppPipelineState)->GetResourceID())
      .TypedAs("ID3D12PipelineState *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_COMPUTE_PIPELINE_STATE_DESC unwrappedDesc = Descriptor;
    unwrappedDesc.pRootSignature = Unwrap(unwrappedDesc.pRootSignature);

    // add any missing hashes ourselves. This probably comes from a capture with experimental
    // enabled so it can load unhashed, but we want to be more proactive
    if(!DXBC::DXBCContainer::IsHashedContainer(Descriptor.CS.pShaderBytecode,
                                               Descriptor.CS.BytecodeLength))
      DXBC::DXBCContainer::HashContainer((void *)Descriptor.CS.pShaderBytecode,
                                         Descriptor.CS.BytecodeLength);

    ID3D12PipelineState *ret = NULL;
    HRESULT hr = m_pDevice->CreateComputePipelineState(&unwrappedDesc, guid, (void **)&ret);

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating compute pipeline, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(ret, this);
      ret = wrapped;

      wrapped->compute = new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(Descriptor);

      wrapped->FetchRootSig(GetShaderCache());

      WrappedID3D12Shader *entry = WrappedID3D12Shader::AddShader(wrapped->compute->CS, this);
      entry->AddRef();

      if(m_GlobalEXTUAV != ~0U)
        entry->SetShaderExtSlot(m_GlobalEXTUAV, m_GlobalEXTUAVSpace);

      AddResourceCurChunk(entry->GetResourceID());

      AddResource(pPipelineState, ResourceType::PipelineState, "Compute Pipeline State");
      if(Descriptor.pRootSignature)
        DerivedResource(Descriptor.pRootSignature, pPipelineState);
      DerivedResource(entry->GetResourceID(), pPipelineState);

      wrapped->compute->CS.pShaderBytecode = entry;

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

void WrappedID3D12Device::ProcessCreatedComputePSO(ID3D12PipelineState *real, uint32_t vendorExtReg,
                                                   uint32_t vendorExtSpace,
                                                   const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc,
                                                   REFIID riid, void **ppPipelineState)
{
  if(DXBC::DXBCContainer::CheckForDXIL(pDesc->CS.pShaderBytecode, pDesc->CS.BytecodeLength))
    m_UsedDXIL = true;

  WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(real, this);

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();

    Chunk *vendorChunk = NULL;
    if(m_VendorEXT != GPUVendor::Unknown)
    {
      if(UsesExtensionUAV(pDesc->CS, vendorExtReg, vendorExtSpace))
      {
        // don't set initparams until we've seen at least one shader actually created using the
        // extensions.
        m_InitParams.VendorExtensions = m_VendorEXT;

        // if this shader uses the UAV slot registered for vendor extensions, serialise that out
        // too
        SCOPED_SERIALISE_CHUNK(D3D12Chunk::SetShaderExtUAV);
        Serialise_SetShaderExtUAV(ser, m_VendorEXT, vendorExtReg, vendorExtSpace, true);
        vendorChunk = scope.Get();
      }
    }

    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateComputePipeline);
    Serialise_CreateComputePipelineState(ser, pDesc, riid, (void **)&wrapped);

    D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
    record->type = Resource_PipelineState;
    record->Length = 0;
    wrapped->SetResourceRecord(record);

    if(pDesc->pRootSignature)
      record->AddParent(GetRecord(pDesc->pRootSignature));

    if(vendorChunk)
      record->AddChunk(vendorChunk);
    record->AddChunk(scope.Get());
  }
  else
  {
    GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);

    wrapped->compute = new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(*pDesc);

    wrapped->FetchRootSig(GetShaderCache());

    WrappedID3D12Shader *sh = WrappedID3D12Shader::AddShader(wrapped->compute->CS, this);
    sh->AddRef();
    wrapped->compute->CS.pShaderBytecode = sh;
  }

  *ppPipelineState = (ID3D12PipelineState *)wrapped;
}

HRESULT WrappedID3D12Device::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc,
                                                        REFIID riid, void **ppPipelineState)
{
  D3D12_COMPUTE_PIPELINE_STATE_DESC unwrappedDesc = *pDesc;
  unwrappedDesc.pRootSignature = Unwrap(unwrappedDesc.pRootSignature);

  if(ppPipelineState == NULL)
    return m_pDevice->CreateComputePipelineState(&unwrappedDesc, riid, NULL);

  if(riid != __uuidof(ID3D12PipelineState))
    return E_NOINTERFACE;

  ID3D12PipelineState *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice->CreateComputePipelineState(&unwrappedDesc, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    // use implicit register/space
    uint32_t reg = ~0U, space = ~0U;
    if(m_VendorEXT != GPUVendor::Unknown)
      GetShaderExtUAV(reg, space);

    ProcessCreatedComputePSO(real, reg, space, pDesc, riid, ppPipelineState);
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateDescriptorHeap(
    SerialiserType &ser, const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc, REFIID riid,
    void **ppvHeap)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDescriptorHeapDesc).Named("pDescriptorHeapDesc"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pHeap, ((WrappedID3D12DescriptorHeap *)*ppvHeap)->GetResourceID())
      .TypedAs("ID3D12DescriptorHeap *"_lit);

  uint64_t originalGPUBase = 0;
  if(ser.IsWriting())
    originalGPUBase = ((WrappedID3D12DescriptorHeap *)*ppvHeap)->GetOriginalGPUBase();
  if(ser.VersionAtLeast(0x12))
  {
    SERIALISE_ELEMENT(originalGPUBase).Hidden();
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_DESCRIPTOR_HEAP_DESC PatchedDesc = Descriptor;
    bool patched = false;

    // inflate the heap so we can insert our own descriptors at the end
    // while patching, because DX12 has a stupid limitation to not be able
    // to set multiple descriptor heaps at once of the same type
    if(PatchedDesc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
      if(m_D3D12Opts.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_3 ||
         PatchedDesc.NumDescriptors + 16 <= 1000000)
      {
        PatchedDesc.NumDescriptors += 16;
        patched = true;
      }
      else
      {
        RDCERR(
            "RenderDoc needs extra descriptors for patching during analysis,"
            "but heap is already at binding tier limit");
      }
    }

    ID3D12DescriptorHeap *ret = NULL;
    HRESULT hr = m_pDevice->CreateDescriptorHeap(&PatchedDesc, guid, (void **)&ret);

    if(patched && FAILED(hr))
    {
      RDCWARN(
          "RenderDoc needs extra descriptors for patching during analysis,"
          "but heap failed to expand any further even at tier 3");
      PatchedDesc.NumDescriptors = Descriptor.NumDescriptors;

      hr = m_pDevice->CreateDescriptorHeap(&PatchedDesc, guid, (void **)&ret);
    }

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating descriptor heap, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      WrappedID3D12DescriptorHeap *wrapped =
          new WrappedID3D12DescriptorHeap(ret, this, PatchedDesc, Descriptor.NumDescriptors);

      wrapped->SetOriginalGPUBase(originalGPUBase);

      ret = wrapped;

      GetResourceManager()->AddLiveResource(pHeap, ret);

      AddResource(pHeap, ResourceType::DescriptorStore, "Descriptor Heap");

      DescriptorStoreDescription desc;
      desc.resourceId = pHeap;
      desc.descriptorByteSize = 1;
      desc.firstDescriptorOffset = 0;
      desc.descriptorCount = Descriptor.NumDescriptors;
      GetReplay()->RegisterDescriptorStore(desc);
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc,
                                                  REFIID riid, void **ppvHeap)
{
  if(ppvHeap == NULL)
    return m_pDevice->CreateDescriptorHeap(pDescriptorHeapDesc, riid, NULL);

  if(riid != __uuidof(ID3D12DescriptorHeap))
    return E_NOINTERFACE;

  ID3D12DescriptorHeap *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice->CreateDescriptorHeap(pDescriptorHeapDesc, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12DescriptorHeap *wrapped = new WrappedID3D12DescriptorHeap(
        real, this, *pDescriptorHeapDesc, pDescriptorHeapDesc->NumDescriptors);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateDescriptorHeap);
      Serialise_CreateDescriptorHeap(ser, pDescriptorHeapDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_DescriptorHeap;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());

      GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppvHeap = (ID3D12DescriptorHeap *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateRootSignature(SerialiserType &ser, UINT nodeMask,
                                                        const void *pBlobWithRootSignature,
                                                        SIZE_T blobLengthInBytes_, REFIID riid,
                                                        void **ppvRootSignature)
{
  SERIALISE_ELEMENT(nodeMask);
  SERIALISE_ELEMENT_ARRAY(pBlobWithRootSignature, blobLengthInBytes_).Important();
  SERIALISE_ELEMENT_LOCAL(blobLengthInBytes, uint64_t(blobLengthInBytes_));
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pRootSignature,
                          ((WrappedID3D12RootSignature *)*ppvRootSignature)->GetResourceID())
      .TypedAs("ID3D12RootSignature *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    nodeMask = 0;

    ID3D12RootSignature *ret = NULL;
    HRESULT hr = m_pDevice->CreateRootSignature(nodeMask, pBlobWithRootSignature,
                                                (SIZE_T)blobLengthInBytes, guid, (void **)&ret);

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

      wrapped->sig = GetShaderCache()->GetRootSig(pBlobWithRootSignature, (size_t)blobLengthInBytes);

      if(wrapped->sig.Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE)
        wrapped->localRootSigIdx =
            GetResourceManager()->GetRaytracingResourceAndUtilHandler()->RegisterLocalRootSig(
                wrapped->sig);

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

HRESULT WrappedID3D12Device::CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature,
                                                 SIZE_T blobLengthInBytes, REFIID riid,
                                                 void **ppvRootSignature)
{
  if(ppvRootSignature == NULL)
    return m_pDevice->CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid,
                                          NULL);

  if(riid != __uuidof(ID3D12RootSignature))
    return E_NOINTERFACE;

  ID3D12RootSignature *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateRootSignature(nodeMask, pBlobWithRootSignature,
                                                           blobLengthInBytes, riid, (void **)&real));

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

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateRootSignature);
      Serialise_CreateRootSignature(ser, nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid,
                                    (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_RootSignature;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      wrapped->sig = GetShaderCache()->GetRootSig(pBlobWithRootSignature, blobLengthInBytes);

      if(wrapped->sig.Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE)
        wrapped->localRootSigIdx =
            GetResourceManager()->GetRaytracingResourceAndUtilHandler()->RegisterLocalRootSig(
                wrapped->sig);

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
    else
    {
      wrapped->sig = GetShaderCache()->GetRootSig(pBlobWithRootSignature, blobLengthInBytes);
    }

    *ppvRootSignature = (ID3D12RootSignature *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_DynamicDescriptorWrite(SerialiserType &ser,
                                                           const DynamicDescriptorWrite *write)
{
  SERIALISE_ELEMENT_LOCAL(desc, write->desc).Important();
  SERIALISE_ELEMENT_LOCAL(dst, ToPortableHandle(write->dest)).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12Descriptor *handle = DescriptorFromPortableHandle(GetResourceManager(), dst);

    if(handle)
    {
      // safe to pass an invalid heap type to Create() as these descriptors will by definition not
      // be undefined
      RDCASSERT(desc.GetType() != D3D12DescriptorType::Undefined);
      desc.Create(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES, this, *handle);
      handle->GetHeap()->MarkMutableIndex(handle->GetHeapIndex());
    }
  }

  return true;
}

void WrappedID3D12Device::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc,
                                                   D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  SERIALISE_TIME_CALL(m_pDevice->CreateConstantBufferView(pDesc, Unwrap(DestDescriptor)));

  // assume descriptors are volatile
  if(capframe)
  {
    DynamicDescriptorWrite write;
    write.desc.Init(pDesc);
    write.dest = GetWrapped(DestDescriptor);
    {
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorRefs.push_back(write.desc);
    }

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateConstantBufferView);
      Serialise_DynamicDescriptorWrite(ser, &write);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }

    if(pDesc)
      GetResourceManager()->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pDesc->BufferLocation), eFrameRef_Read);
  }

  GetWrapped(DestDescriptor)->Init(pDesc);
}

void WrappedID3D12Device::CreateShaderResourceView(ID3D12Resource *pResource,
                                                   const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                                   D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  SERIALISE_TIME_CALL(
      m_pDevice->CreateShaderResourceView(Unwrap(pResource), pDesc, Unwrap(DestDescriptor)));

  // assume descriptors are volatile
  if(capframe)
  {
    DynamicDescriptorWrite write;
    write.desc.Init(pResource, pDesc);
    write.dest = GetWrapped(DestDescriptor);
    if(pResource && pResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorRefs.push_back(write.desc);
    }

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateShaderResourceView);
      Serialise_DynamicDescriptorWrite(ser, &write);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }

    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_Read);
  }

  GetWrapped(DestDescriptor)->Init(pResource, pDesc);

  if(IsReplayMode(m_State) && pDesc)
  {
    if(pDesc->ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE ||
       pDesc->ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY)
      m_Cubemaps.insert(GetResID(pResource));
  }
}

void WrappedID3D12Device::CreateUnorderedAccessView(ID3D12Resource *pResource,
                                                    ID3D12Resource *pCounterResource,
                                                    const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                                                    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  SERIALISE_TIME_CALL(m_pDevice->CreateUnorderedAccessView(
      Unwrap(pResource), Unwrap(pCounterResource), pDesc, Unwrap(DestDescriptor)));

  // assume descriptors are volatile
  if(capframe)
  {
    DynamicDescriptorWrite write;
    write.desc.Init(pResource, pCounterResource, pDesc);
    write.dest = GetWrapped(DestDescriptor);
    if(pResource && pResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorRefs.push_back(write.desc);
    }

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateUnorderedAccessView);
      Serialise_DynamicDescriptorWrite(ser, &write);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }

    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_PartialWrite);
    if(pCounterResource)
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(pCounterResource),
                                                        eFrameRef_PartialWrite);
  }

  GetWrapped(DestDescriptor)->Init(pResource, pCounterResource, pDesc);
}

void WrappedID3D12Device::CreateRenderTargetView(ID3D12Resource *pResource,
                                                 const D3D12_RENDER_TARGET_VIEW_DESC *pDesc,
                                                 D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  SERIALISE_TIME_CALL(
      m_pDevice->CreateRenderTargetView(Unwrap(pResource), pDesc, Unwrap(DestDescriptor)));

  // assume descriptors are volatile
  if(capframe)
  {
    DynamicDescriptorWrite write;
    write.desc.Init(pResource, pDesc);
    write.dest = GetWrapped(DestDescriptor);
    if(pResource && pResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorRefs.push_back(write.desc);
    }

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateRenderTargetView);
      Serialise_DynamicDescriptorWrite(ser, &write);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }

    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_PartialWrite);
  }

  GetWrapped(DestDescriptor)->Init(pResource, pDesc);
}

void WrappedID3D12Device::CreateDepthStencilView(ID3D12Resource *pResource,
                                                 const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                                 D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  SERIALISE_TIME_CALL(
      m_pDevice->CreateDepthStencilView(Unwrap(pResource), pDesc, Unwrap(DestDescriptor)));

  // assume descriptors are volatile
  if(capframe)
  {
    DynamicDescriptorWrite write;
    write.desc.Init(pResource, pDesc);
    write.dest = GetWrapped(DestDescriptor);

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateDepthStencilView);
      Serialise_DynamicDescriptorWrite(ser, &write);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }

    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_PartialWrite);
  }

  GetWrapped(DestDescriptor)->Init(pResource, pDesc);
}

void WrappedID3D12Device::CreateSampler(const D3D12_SAMPLER_DESC *pDesc,
                                        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  SERIALISE_TIME_CALL(m_pDevice->CreateSampler(pDesc, Unwrap(DestDescriptor)));

  // assume descriptors are volatile
  if(capframe)
  {
    DynamicDescriptorWrite write;
    write.desc.Init(pDesc);
    write.dest = GetWrapped(DestDescriptor);

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateSampler);
      Serialise_DynamicDescriptorWrite(ser, &write);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }
  }

  GetWrapped(DestDescriptor)->Init(pDesc);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateHeap(SerialiserType &ser, const D3D12_HEAP_DESC *pDesc,
                                               REFIID riid, void **ppvHeap)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pHeap, ((WrappedID3D12Heap *)*ppvHeap)->GetResourceID())
      .TypedAs("ID3D12Heap *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    void *realptr = NULL;

    // don't create resources non-resident
    Descriptor.Flags &= ~D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;

    // don't create displayable heaps (?!)
    Descriptor.Flags &= ~D3D12_HEAP_FLAG_ALLOW_DISPLAY;

    if(m_DriverInfo.vendor == GPUVendor::Software &&
       Descriptor.Properties.Type == D3D12_HEAP_TYPE_CUSTOM)
      Descriptor.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    HRESULT hr = m_pDevice->CreateHeap(&Descriptor, guid, &realptr);

    ID3D12Heap *ret = NULL;
    if(guid == __uuidof(ID3D12Heap))
      ret = (ID3D12Heap *)realptr;
    else if(guid == __uuidof(ID3D12Heap1))
      ret = (ID3D12Heap1 *)realptr;

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating heap, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12Heap(ret, this);

      GetResourceManager()->AddLiveResource(pHeap, ret);
    }

    AddResource(pHeap, ResourceType::Memory, "Heap");
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateHeap(const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap)
{
  if(ppvHeap == NULL)
    return m_pDevice->CreateHeap(pDesc, riid, ppvHeap);

  if(riid != __uuidof(ID3D12Heap) && riid != __uuidof(ID3D12Heap1))
    return E_NOINTERFACE;

  void *realptr = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateHeap(pDesc, riid, (void **)&realptr));

  ID3D12Heap *real = NULL;

  if(riid == __uuidof(ID3D12Heap))
    real = (ID3D12Heap *)realptr;
  else if(riid == __uuidof(ID3D12Heap1))
    real = (ID3D12Heap1 *)realptr;

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Heap *wrapped = new WrappedID3D12Heap(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateHeap);
      Serialise_CreateHeap(ser, pDesc, riid, (void **)&wrapped);

      if(pDesc->Flags & D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT)
        wrapped->Evict();

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Heap;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppvHeap = (ID3D12Heap *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateFence(SerialiserType &ser, UINT64 InitialValue,
                                                D3D12_FENCE_FLAGS Flags, REFIID riid, void **ppFence)
{
  SERIALISE_ELEMENT(InitialValue).Important();
  SERIALISE_ELEMENT(Flags);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pFence, ((WrappedID3D12Fence *)*ppFence)->GetResourceID())
      .TypedAs("ID3D12Fence *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    void *realptr = NULL;
    HRESULT hr = m_pDevice->CreateFence(InitialValue, Flags, guid, &realptr);

    ID3D12Fence *ret = NULL;
    if(guid == __uuidof(ID3D12Fence))
      ret = (ID3D12Fence *)realptr;
    else if(guid == __uuidof(ID3D12Fence1))
      ret = (ID3D12Fence1 *)realptr;

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating fence, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12Fence(ret, this);

      GetResourceManager()->AddLiveResource(pFence, ret);
    }

    AddResource(pFence, ResourceType::Sync, "Fence");
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid,
                                         void **ppFence)
{
  if(ppFence == NULL)
    return m_pDevice->CreateFence(InitialValue, Flags, riid, NULL);

  if(riid != __uuidof(ID3D12Fence) && riid != __uuidof(ID3D12Fence1))
    return E_NOINTERFACE;

  void *realptr = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateFence(InitialValue, Flags, riid, &realptr));

  ID3D12Fence *real = NULL;

  if(riid == __uuidof(ID3D12Fence))
    real = (ID3D12Fence *)realptr;
  else if(riid == __uuidof(ID3D12Fence1))
    real = (ID3D12Fence1 *)realptr;

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Fence *wrapped = new WrappedID3D12Fence(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateFence);
      Serialise_CreateFence(ser, InitialValue, Flags, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Fence;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    if(riid == __uuidof(ID3D12Fence))
      *ppFence = (ID3D12Fence *)wrapped;
    else if(riid == __uuidof(ID3D12Fence1))
      *ppFence = (ID3D12Fence1 *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateQueryHeap(SerialiserType &ser,
                                                    const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid,
                                                    void **ppvHeap)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pQueryHeap, ((WrappedID3D12QueryHeap *)*ppvHeap)->GetResourceID())
      .TypedAs("ID3D12QueryHeap *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12QueryHeap *ret = NULL;
    HRESULT hr = m_pDevice->CreateQueryHeap(&Descriptor, guid, (void **)&ret);

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating query heap, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12QueryHeap(ret, this);

      GetResourceManager()->AddLiveResource(pQueryHeap, ret);
    }

    AddResource(pQueryHeap, ResourceType::Query, "Query Heap");
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid,
                                             void **ppvHeap)
{
  if(ppvHeap == NULL)
    return m_pDevice->CreateQueryHeap(pDesc, riid, NULL);

  if(riid != __uuidof(ID3D12QueryHeap))
    return E_NOINTERFACE;

  ID3D12QueryHeap *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateQueryHeap(pDesc, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12QueryHeap *wrapped = new WrappedID3D12QueryHeap(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateQueryHeap);
      Serialise_CreateQueryHeap(ser, pDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_QueryHeap;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppvHeap = (ID3D12QueryHeap *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommandSignature(SerialiserType &ser,
                                                           const D3D12_COMMAND_SIGNATURE_DESC *pDesc,
                                                           ID3D12RootSignature *pRootSignature,
                                                           REFIID riid, void **ppvCommandSignature)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(pRootSignature);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pCommandSignature,
                          ((WrappedID3D12CommandSignature *)*ppvCommandSignature)->GetResourceID())
      .TypedAs("ID3D12CommandSignature *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12CommandSignature *ret = NULL;
    HRESULT hr =
        m_pDevice->CreateCommandSignature(&Descriptor, Unwrap(pRootSignature), guid, (void **)&ret);

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating command signature, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      WrappedID3D12CommandSignature *wrapped =
          new WrappedID3D12CommandSignature(ret, this, Descriptor);

      ret = wrapped;

      GetResourceManager()->AddLiveResource(pCommandSignature, ret);

      AddResource(pCommandSignature, ResourceType::ShaderBinding, "Command Signature");
      if(pRootSignature)
        DerivedResource(pRootSignature, pCommandSignature);
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *pDesc,
                                                    ID3D12RootSignature *pRootSignature,
                                                    REFIID riid, void **ppvCommandSignature)
{
  if(ppvCommandSignature == NULL)
    return m_pDevice->CreateCommandSignature(pDesc, Unwrap(pRootSignature), riid, NULL);

  if(riid != __uuidof(ID3D12CommandSignature))
    return E_NOINTERFACE;

  ID3D12CommandSignature *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice->CreateCommandSignature(pDesc, Unwrap(pRootSignature), riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12CommandSignature *wrapped = NULL;

    {
      SCOPED_LOCK(m_WrapDeduplicateLock);

      if(GetResourceManager()->HasWrapper(real))
      {
        real->Release();
        ID3D12CommandSignature *existing =
            (ID3D12CommandSignature *)GetResourceManager()->GetWrapper(real);
        existing->AddRef();
        *ppvCommandSignature = existing;
        return ret;
      }

      wrapped = new WrappedID3D12CommandSignature(real, this, *pDesc);
    }

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateCommandSignature);
      Serialise_CreateCommandSignature(ser, pDesc, pRootSignature, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_CommandSignature;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      if(pRootSignature)
        record->AddParent(GetRecord(pRootSignature));
      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    if(pDesc->pArgumentDescs[pDesc->NumArgumentDescs - 1].Type ==
       D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS)
      wrapped->sig.raytraced = true;

    *ppvCommandSignature = (ID3D12CommandSignature *)wrapped;
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

HRESULT WrappedID3D12Device::CreateSharedHandle(ID3D12DeviceChild *pObject,
                                                const SECURITY_ATTRIBUTES *pAttributes,
                                                DWORD Access, LPCWSTR Name, HANDLE *pHandle)
{
  return m_pDevice->CreateSharedHandle(Unwrap(pObject), pAttributes, Access, Name, pHandle);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_DynamicDescriptorCopies(
    SerialiserType &ser, const rdcarray<DynamicDescriptorCopy> &DescriptorCopies)
{
  SERIALISE_ELEMENT(DescriptorCopies);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // not optimal, but simple for now. Do a wrapped copy so that internal tracking is also updated
    for(const DynamicDescriptorCopy &copy : DescriptorCopies)
    {
      CopyDescriptorsSimple(1, *copy.dst, *copy.src, copy.type);
      copy.dst->GetHeap()->MarkMutableIndex(copy.dst->GetHeapIndex());
    }
  }

  return true;
}

void WrappedID3D12Device::CopyDescriptors(
    UINT NumDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pDestDescriptorRangeStarts,
    const UINT *pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcDescriptorRangeStarts,
    const UINT *pSrcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
  D3D12_CPU_DESCRIPTOR_HANDLE *unwrappedArray =
      GetTempArray<D3D12_CPU_DESCRIPTOR_HANDLE>(NumDestDescriptorRanges + NumSrcDescriptorRanges);

  D3D12_CPU_DESCRIPTOR_HANDLE *dstStarts = unwrappedArray;
  D3D12_CPU_DESCRIPTOR_HANDLE *srcStarts = unwrappedArray + NumDestDescriptorRanges;

  for(UINT i = 0; i < NumDestDescriptorRanges; i++)
    dstStarts[i] = Unwrap(pDestDescriptorRangeStarts[i]);

  for(UINT i = 0; i < NumSrcDescriptorRanges; i++)
    srcStarts[i] = Unwrap(pSrcDescriptorRangeStarts[i]);

  SERIALISE_TIME_CALL(m_pDevice->CopyDescriptors(
      NumDestDescriptorRanges, dstStarts, pDestDescriptorRangeSizes, NumSrcDescriptorRanges,
      srcStarts, pSrcDescriptorRangeSizes, DescriptorHeapsType));

  UINT srcRange = 0, dstRange = 0;
  UINT srcIdx = 0, dstIdx = 0;

  D3D12Descriptor *src = GetWrapped(pSrcDescriptorRangeStarts[0]);
  D3D12Descriptor *dst = GetWrapped(pDestDescriptorRangeStarts[0]);

  rdcarray<DynamicDescriptorCopy> copies;

  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  for(; srcRange < NumSrcDescriptorRanges && dstRange < NumDestDescriptorRanges;)
  {
    const UINT srcSize = pSrcDescriptorRangeSizes ? pSrcDescriptorRangeSizes[srcRange] : 1;
    const UINT dstSize = pDestDescriptorRangeSizes ? pDestDescriptorRangeSizes[dstRange] : 1;

    // just in case a size is specified as 0, check here
    if(srcIdx < srcSize && dstIdx < dstSize)
    {
      // assume descriptors are volatile
      if(capframe)
        copies.push_back(DynamicDescriptorCopy(&dst[dstIdx], &src[srcIdx], DescriptorHeapsType));

      dst[dstIdx].CopyFrom(src[srcIdx]);
    }

    srcIdx++;
    dstIdx++;

    // move source onto the next range
    if(srcIdx >= srcSize)
    {
      srcRange++;
      srcIdx = 0;

      // check srcRange is valid - we might be about to exit the loop from reading off the end
      if(srcRange < NumSrcDescriptorRanges)
        src = GetWrapped(pSrcDescriptorRangeStarts[srcRange]);
    }

    if(dstIdx >= dstSize)
    {
      dstRange++;
      dstIdx = 0;

      if(dstRange < NumDestDescriptorRanges)
        dst = GetWrapped(pDestDescriptorRangeStarts[dstRange]);
    }
  }

  if(!copies.empty())
  {
    // reference all the individual heaps
    for(UINT i = 0; i < NumSrcDescriptorRanges; i++)
    {
      D3D12Descriptor *desc = GetWrapped(pSrcDescriptorRangeStarts[i]);
      GetResourceManager()->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);

      // make sure we reference the resources in the source descriptors too
      const UINT srcSize = pSrcDescriptorRangeSizes ? pSrcDescriptorRangeSizes[i] : 1;
      for(UINT d = 0; d < srcSize; d++)
      {
        ResourceId id, id2;
        FrameRefType ref = eFrameRef_Read;

        desc[d].GetRefIDs(id, id2, ref);

        if(id != ResourceId())
          GetResourceManager()->MarkResourceFrameReferenced(id, ref);

        if(id2 != ResourceId())
          GetResourceManager()->MarkResourceFrameReferenced(id2, ref);
      }
    }

    for(UINT i = 0; i < NumDestDescriptorRanges; i++)
    {
      D3D12Descriptor *desc = GetWrapped(pDestDescriptorRangeStarts[i]);
      GetResourceManager()->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
    }

    {
      SCOPED_LOCK(m_DynDescLock);
      for(size_t i = 0; i < copies.size(); i++)
        m_DynamicDescriptorRefs.push_back(*copies[i].src);
    }

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CopyDescriptors);
      Serialise_DynamicDescriptorCopies(ser, copies);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }
  }
}

void WrappedID3D12Device::CopyDescriptorsSimple(UINT NumDescriptors,
                                                D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
                                                D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
                                                D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
  SERIALISE_TIME_CALL(
      m_pDevice->CopyDescriptorsSimple(NumDescriptors, Unwrap(DestDescriptorRangeStart),
                                       Unwrap(SrcDescriptorRangeStart), DescriptorHeapsType));

  D3D12Descriptor *src = GetWrapped(SrcDescriptorRangeStart);
  D3D12Descriptor *dst = GetWrapped(DestDescriptorRangeStart);

  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  if(capframe)
  {
    // reference the heaps
    {
      D3D12Descriptor *desc = GetWrapped(SrcDescriptorRangeStart);
      GetResourceManager()->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);

      // make sure we reference the resources in the source descriptors too
      for(UINT d = 0; d < NumDescriptors; d++)
      {
        ResourceId id, id2;
        FrameRefType ref = eFrameRef_Read;

        desc[d].GetRefIDs(id, id2, ref);

        if(id != ResourceId())
          GetResourceManager()->MarkResourceFrameReferenced(id, ref);

        if(id2 != ResourceId())
          GetResourceManager()->MarkResourceFrameReferenced(id2, ref);
      }
    }

    {
      D3D12Descriptor *desc = GetWrapped(DestDescriptorRangeStart);
      GetResourceManager()->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
    }

    rdcarray<DynamicDescriptorCopy> copies;
    copies.reserve(NumDescriptors);
    for(UINT i = 0; i < NumDescriptors; i++)
      copies.push_back(DynamicDescriptorCopy(&dst[i], &src[i], DescriptorHeapsType));

    {
      SCOPED_LOCK(m_DynDescLock);
      for(size_t i = 0; i < copies.size(); i++)
        m_DynamicDescriptorRefs.push_back(*copies[i].src);
    }

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CopyDescriptorsSimple);
      Serialise_DynamicDescriptorCopies(ser, copies);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }
  }

  for(UINT i = 0; i < NumDescriptors; i++)
    dst[i].CopyFrom(src[i]);
}

HRESULT WrappedID3D12Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle)
{
  return m_pDevice->OpenSharedHandleByName(Name, Access, pNTHandle);
}

HRESULT WrappedID3D12Device::MakeResident(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
  SCOPED_READLOCK(m_CapTransitionLock);

  ID3D12Pageable **unwrapped = GetTempArray<ID3D12Pageable *>(NumObjects);

  for(UINT i = 0; i < NumObjects; i++)
  {
    if(WrappedID3D12DescriptorHeap::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12DescriptorHeap *heap = (WrappedID3D12DescriptorHeap *)ppObjects[i];
      heap->MakeResident();
      unwrapped[i] = heap->GetReal();
    }
    else if(WrappedID3D12Resource::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12Resource *res = (WrappedID3D12Resource *)ppObjects[i];
      res->MakeResident();
      unwrapped[i] = res->GetReal();
    }
    else if(WrappedID3D12Heap::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12Heap *heap = (WrappedID3D12Heap *)ppObjects[i];
      heap->MakeResident();
      unwrapped[i] = heap->GetReal();
    }
    else
    {
      unwrapped[i] = (ID3D12Pageable *)Unwrap((ID3D12DeviceChild *)ppObjects[i]);
    }
  }

  return m_pDevice->MakeResident(NumObjects, unwrapped);
}

HRESULT WrappedID3D12Device::Evict(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
  SCOPED_READLOCK(m_CapTransitionLock);

  ID3D12Pageable **unwrapped = GetTempArray<ID3D12Pageable *>(NumObjects);

  for(UINT i = 0; i < NumObjects; i++)
  {
    if(WrappedID3D12DescriptorHeap::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12DescriptorHeap *heap = (WrappedID3D12DescriptorHeap *)ppObjects[i];
      heap->Evict();
      unwrapped[i] = heap->GetReal();
    }
    else if(WrappedID3D12Resource::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12Resource *res = (WrappedID3D12Resource *)ppObjects[i];
      res->Evict();
      unwrapped[i] = res->GetReal();
    }
    else if(WrappedID3D12Heap::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12Heap *heap = (WrappedID3D12Heap *)ppObjects[i];
      heap->Evict();
      unwrapped[i] = heap->GetReal();
    }
    else
    {
      unwrapped[i] = (ID3D12Pageable *)Unwrap((ID3D12DeviceChild *)ppObjects[i]);
    }
  }

  return m_pDevice->Evict(NumObjects, unwrapped);
}

//////////////////////////////////////////////////////////////////////
// we don't need to wrap any of these functions below

HRESULT WrappedID3D12Device::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
  return m_pDevice->GetPrivateData(guid, pDataSize, pData);
}

HRESULT WrappedID3D12Device::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
  return m_pDevice->SetPrivateData(guid, DataSize, pData);
}

HRESULT WrappedID3D12Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
  return m_pDevice->SetPrivateDataInterface(guid, pData);
}

HRESULT WrappedID3D12Device::SetName(LPCWSTR Name)
{
  return m_pDevice->SetName(Name);
}

UINT WrappedID3D12Device::GetNodeCount()
{
  return m_pDevice->GetNodeCount();
}

LUID WrappedID3D12Device::GetAdapterLuid()
{
  return m_pDevice->GetAdapterLuid();
}

void WrappedID3D12Device::GetResourceTiling(
    ID3D12Resource *pTiledResource, UINT *pNumTilesForEntireResource,
    D3D12_PACKED_MIP_INFO *pPackedMipDesc, D3D12_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
    UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
    D3D12_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips)
{
  return m_pDevice->GetResourceTiling(Unwrap(pTiledResource), pNumTilesForEntireResource,
                                      pPackedMipDesc, pStandardTileShapeForNonPackedMips,
                                      pNumSubresourceTilings, FirstSubresourceTilingToGet,
                                      pSubresourceTilingsForNonPackedMips);
}

HRESULT WrappedID3D12Device::SetStablePowerState(BOOL Enable)
{
  return m_pDevice->SetStablePowerState(Enable);
}

HRESULT WrappedID3D12Device::CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData,
                                                 UINT FeatureSupportDataSize)
{
  static uint64_t logged = 0;
  bool dolog = true;
  if(uint32_t(Feature) < 64)
  {
    const uint64_t bit = 1ULL << uint32_t(Feature);
    dolog = (logged & bit) == 0;
    logged |= bit;
  }

  if(dolog)
    RDCLOG("Checking feature support for %d", Feature);
  HRESULT hr = m_pDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);

  if(FAILED(hr))
  {
    if(dolog)
      RDCLOG("CheckFeatureSupport returned %s", ToStr(hr).c_str());
    return hr;
  }

  if(Feature == D3D12_FEATURE_SHADER_MODEL)
  {
    D3D12_FEATURE_DATA_SHADER_MODEL *model = (D3D12_FEATURE_DATA_SHADER_MODEL *)pFeatureSupportData;
    if(FeatureSupportDataSize != sizeof(D3D12_FEATURE_DATA_SHADER_MODEL))
      return E_INVALIDARG;

    if(dolog)
      RDCLOG("Clamping shader model from 0x%x to 6.7", model->HighestShaderModel);

    // clamp SM to what we support
    model->HighestShaderModel = RDCMIN(model->HighestShaderModel, D3D_SHADER_MODEL_6_7);

    return S_OK;
  }
  else if(Feature == D3D12_FEATURE_PROTECTED_RESOURCE_SESSION_SUPPORT)
  {
    D3D12_FEATURE_DATA_PROTECTED_RESOURCE_SESSION_SUPPORT *opts =
        (D3D12_FEATURE_DATA_PROTECTED_RESOURCE_SESSION_SUPPORT *)pFeatureSupportData;

    // declare no support for protected resource sessions. We still wrap it just in case but we
    // can't capture or replay this properly.
    opts->Support = D3D12_PROTECTED_RESOURCE_SESSION_SUPPORT_FLAG_NONE;

    if(dolog)
      RDCLOG("Forcing no protected session tier support");

    return S_OK;
  }
  else if(Feature == D3D12_FEATURE_D3D12_OPTIONS5)
  {
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 *opts =
        (D3D12_FEATURE_DATA_D3D12_OPTIONS5 *)pFeatureSupportData;
    if(FeatureSupportDataSize != sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5))
      return E_INVALIDARG;

    // don't support raytracing
    if(!D3D12_Experimental_EnableRTSupport())
    {
      opts->RaytracingTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;

      if(dolog)
        RDCLOG("Forcing no raytracing tier support");
    }

    return S_OK;
  }
  else if(Feature == D3D12_FEATURE_D3D12_OPTIONS7)
  {
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 *opts =
        (D3D12_FEATURE_DATA_D3D12_OPTIONS7 *)pFeatureSupportData;
    if(FeatureSupportDataSize != sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS7))
      return E_INVALIDARG;

    // don't support sampler feedback
    opts->SamplerFeedbackTier = D3D12_SAMPLER_FEEDBACK_TIER_NOT_SUPPORTED;

    if(dolog)
      RDCLOG("Forcing no sampler feedback tier support");

    return S_OK;
  }
  else if(Feature == D3D12_FEATURE_D3D12_OPTIONS18)
  {
    D3D12_FEATURE_DATA_D3D12_OPTIONS18 *opts =
        (D3D12_FEATURE_DATA_D3D12_OPTIONS18 *)pFeatureSupportData;
    if(FeatureSupportDataSize != sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS18))
      return E_INVALIDARG;

    opts->RenderPassesValid = FALSE;

    if(dolog)
      RDCLOG("Forcing no changed renderpass support");

    return S_OK;
  }

  return hr;
}

UINT WrappedID3D12Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType)
{
  // we essentially intercept this, so it's a fixed size.
  return sizeof(D3D12Descriptor);
}

D3D12_RESOURCE_ALLOCATION_INFO WrappedID3D12Device::GetResourceAllocationInfo(
    UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC *pResourceDescs)
{
  return m_pDevice->GetResourceAllocationInfo(visibleMask, numResourceDescs, pResourceDescs);
}

D3D12_HEAP_PROPERTIES WrappedID3D12Device::GetCustomHeapProperties(UINT nodeMask,
                                                                   D3D12_HEAP_TYPE heapType)
{
  return m_pDevice->GetCustomHeapProperties(nodeMask, heapType);
}

HRESULT WrappedID3D12Device::GetDeviceRemovedReason()
{
  return m_pDevice->GetDeviceRemovedReason();
}

void WrappedID3D12Device::GetCopyableFootprints(const D3D12_RESOURCE_DESC *pResourceDesc,
                                                UINT FirstSubresource, UINT NumSubresources,
                                                UINT64 BaseOffset,
                                                D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts,
                                                UINT *pNumRows, UINT64 *pRowSizeInBytes,
                                                UINT64 *pTotalBytes)
{
  return m_pDevice->GetCopyableFootprints(pResourceDesc, FirstSubresource, NumSubresources,
                                          BaseOffset, pLayouts, pNumRows, pRowSizeInBytes,
                                          pTotalBytes);
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommandQueue,
                                const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID riid,
                                void **ppCommandQueue);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommandAllocator,
                                D3D12_COMMAND_LIST_TYPE type, REFIID riid, void **ppCommandAllocator);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommandList, UINT nodeMask,
                                D3D12_COMMAND_LIST_TYPE type,
                                ID3D12CommandAllocator *pCommandAllocator,
                                ID3D12PipelineState *pInitialState, REFIID riid,
                                void **ppCommandList);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateGraphicsPipelineState,
                                const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid,
                                void **ppPipelineState);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateComputePipelineState,
                                const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid,
                                void **ppPipelineState);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateDescriptorHeap,
                                const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc, REFIID riid,
                                void **ppvHeap);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateRootSignature, UINT nodeMask,
                                const void *pBlobWithRootSignature, SIZE_T blobLengthInBytes,
                                REFIID riid, void **ppvRootSignature);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, DynamicDescriptorWrite,
                                const DynamicDescriptorWrite *write);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateHeap, const D3D12_HEAP_DESC *pDesc,
                                REFIID riid, void **ppvHeap);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateFence, UINT64 InitialValue,
                                D3D12_FENCE_FLAGS Flags, REFIID riid, void **ppFence);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateQueryHeap,
                                const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommandSignature,
                                const D3D12_COMMAND_SIGNATURE_DESC *pDesc,
                                ID3D12RootSignature *pRootSignature, REFIID riid,
                                void **ppvCommandSignature);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, DynamicDescriptorCopies,
                                const rdcarray<DynamicDescriptorCopy> &DescriptorCopies);

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

// ensure that we don't have to pay attention to the GUID requested and can return our
// WrappedID3D12Resource anywhere and know it satisfies all possibilities
TEST_CASE("Check ID3D12Resource inheritance chain", "[d3d]")
{
  // this should really be checkable at compile time
  IUnknown *u = (IUnknown *)0x1000000;
  ID3D12Resource *a = (ID3D12Resource *)u;
  ID3D12Resource1 *b = (ID3D12Resource1 *)a;
  ID3D12Resource2 *c = (ID3D12Resource2 *)b;

  CHECK((byte *)a == (byte *)u);
  CHECK((byte *)b == (byte *)a);
  CHECK((byte *)c == (byte *)b);
}

#endif
