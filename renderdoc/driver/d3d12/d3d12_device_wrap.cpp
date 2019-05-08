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

#include "d3d12_device.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/ihv/amd/official/DXExt/AmdExtD3D.h"
#include "driver/ihv/amd/official/DXExt/AmdExtD3DCommandListMarkerApi.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommandQueue(SerialiserType &ser,
                                                       const D3D12_COMMAND_QUEUE_DESC *pDesc,
                                                       REFIID riid, void **ppCommandQueue)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit);
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
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      SetObjName(ret, StringFormat::Fmt("Command Queue ID %llu", pCommandQueue));

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

      m_DeviceRecord->AddChunk(scope.Get());
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

    *ppCommandQueue = (ID3D12CommandQueue *)wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommandAllocator(SerialiserType &ser,
                                                           D3D12_COMMAND_LIST_TYPE type,
                                                           REFIID riid, void **ppCommandAllocator)
{
  SERIALISE_ELEMENT(type);
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
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
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

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppCommandAllocator = (ID3D12CommandAllocator *)wrapped;
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
  SERIALISE_ELEMENT(type);
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
    ID3D12GraphicsCommandList *list = NULL;
    HRESULT hr = CreateCommandList(nodeMask, type, pCommandAllocator, pInitialState,
                                   __uuidof(ID3D12GraphicsCommandList), (void **)&list);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
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
     riid != __uuidof(ID3D12GraphicsCommandList3) && riid != __uuidof(ID3D12GraphicsCommandList4))
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
      // we just serialise out command allocator creation as a reset, since it's equivalent.
      wrapped->SetInitParams(riid, nodeMask, type);
      wrapped->Reset(pCommandAllocator, pInitialState);

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
    else if(riid == __uuidof(ID3D12CommandList))
      *ppCommandList = (ID3D12CommandList *)wrapped;
    else
      RDCERR("Unexpected riid! %s", ToStr(riid).c_str());
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateGraphicsPipelineState(
    SerialiserType &ser, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid,
    void **ppPipelineState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pPipelineState,
                          ((WrappedID3D12PipelineState *)*ppPipelineState)->GetResourceID())
      .TypedAs("ID3D12PipelineState *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC unwrappedDesc = Descriptor;
    unwrappedDesc.pRootSignature = Unwrap(unwrappedDesc.pRootSignature);

    ID3D12PipelineState *ret = NULL;
    HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&unwrappedDesc, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12PipelineState(ret, this);

      WrappedID3D12PipelineState *wrapped = (WrappedID3D12PipelineState *)ret;

      wrapped->graphics = new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(Descriptor);

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
          WrappedID3D12Shader *entry = WrappedID3D12Shader::AddShader(*shaders[i], this, wrapped);

          shaders[i]->pShaderBytecode = entry;

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
               Descriptor.StreamOutput.pBufferStrides,
               sizeof(UINT) * wrapped->graphics->StreamOutput.NumStrides);
      }
      else
      {
        wrapped->graphics->StreamOutput.pBufferStrides = NULL;
      }

      GetResourceManager()->AddLiveResource(pPipelineState, ret);
    }
  }

  return true;
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
    WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateGraphicsPipeline);
      Serialise_CreateGraphicsPipelineState(ser, pDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_PipelineState;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      if(pDesc->pRootSignature)
        record->AddParent(GetRecord(pDesc->pRootSignature));

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);

      wrapped->graphics = new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(*pDesc);

      D3D12_SHADER_BYTECODE *shaders[] = {
          &wrapped->graphics->VS, &wrapped->graphics->HS, &wrapped->graphics->DS,
          &wrapped->graphics->GS, &wrapped->graphics->PS,
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
          shaders[i]->pShaderBytecode = WrappedID3D12Shader::AddShader(*shaders[i], this, NULL);
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

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateComputePipelineState(
    SerialiserType &ser, const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid,
    void **ppPipelineState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pPipelineState,
                          ((WrappedID3D12PipelineState *)*ppPipelineState)->GetResourceID())
      .TypedAs("ID3D12PipelineState *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_COMPUTE_PIPELINE_STATE_DESC unwrappedDesc = Descriptor;
    unwrappedDesc.pRootSignature = Unwrap(unwrappedDesc.pRootSignature);

    ID3D12PipelineState *ret = NULL;
    HRESULT hr = m_pDevice->CreateComputePipelineState(&unwrappedDesc, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(ret, this);
      ret = wrapped;

      wrapped->compute = new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(Descriptor);

      WrappedID3D12Shader *entry =
          WrappedID3D12Shader::AddShader(wrapped->compute->CS, this, wrapped);

      AddResourceCurChunk(entry->GetResourceID());

      AddResource(pPipelineState, ResourceType::PipelineState, "Compute Pipeline State");
      if(Descriptor.pRootSignature)
        DerivedResource(Descriptor.pRootSignature, pPipelineState);
      DerivedResource(entry->GetResourceID(), pPipelineState);

      wrapped->compute->CS.pShaderBytecode = entry;

      GetResourceManager()->AddLiveResource(pPipelineState, ret);
    }
  }

  return true;
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
    WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateComputePipeline);
      Serialise_CreateComputePipelineState(ser, pDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_PipelineState;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      if(pDesc->pRootSignature)
        record->AddParent(GetRecord(pDesc->pRootSignature));

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);

      wrapped->compute = new D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC(*pDesc);

      wrapped->compute->CS.pShaderBytecode =
          WrappedID3D12Shader::AddShader(wrapped->compute->CS, this, NULL);
    }

    *ppPipelineState = (ID3D12PipelineState *)wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateDescriptorHeap(
    SerialiserType &ser, const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc, REFIID riid,
    void **ppvHeap)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDescriptorHeapDesc).Named("pDescriptorHeapDesc"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pHeap, ((WrappedID3D12DescriptorHeap *)*ppvHeap)->GetResourceID())
      .TypedAs("ID3D12DescriptorHeap *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // inflate the heap so we can insert our own descriptors at the end
    // while patching, because DX12 has a stupid limitation to not be able
    // to set multiple descriptor heaps at once of the same type
    if(Descriptor.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
      if(m_D3D12Opts.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_3 ||
         Descriptor.NumDescriptors + 16 <= 1000000)
        Descriptor.NumDescriptors += 16;
      else
        RDCERR(
            "RenderDoc needs extra descriptors for patching during analysis,"
            "but heap is already at binding tier limit");
    }

    ID3D12DescriptorHeap *ret = NULL;
    HRESULT hr = m_pDevice->CreateDescriptorHeap(&Descriptor, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12DescriptorHeap(ret, this, Descriptor);

      GetResourceManager()->AddLiveResource(pHeap, ret);

      AddResource(pHeap, ResourceType::ShaderBinding, "Descriptor Heap");
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
    WrappedID3D12DescriptorHeap *wrapped =
        new WrappedID3D12DescriptorHeap(real, this, *pDescriptorHeapDesc);

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

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateRootSignature(SerialiserType &ser, UINT nodeMask,
                                                        const void *pBlobWithRootSignature,
                                                        SIZE_T blobLengthInBytes_, REFIID riid,
                                                        void **ppvRootSignature)
{
  SERIALISE_ELEMENT(nodeMask);
  SERIALISE_ELEMENT_ARRAY(pBlobWithRootSignature, blobLengthInBytes_);
  SERIALISE_ELEMENT_LOCAL(blobLengthInBytes, uint64_t(blobLengthInBytes_));
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pRootSignature,
                          ((WrappedID3D12RootSignature *)*ppvRootSignature)->GetResourceID())
      .TypedAs("ID3D12RootSignature *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12RootSignature *ret = NULL;
    HRESULT hr = m_pDevice->CreateRootSignature(nodeMask, pBlobWithRootSignature,
                                                (SIZE_T)blobLengthInBytes, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      if(GetResourceManager()->HasWrapper(ret))
      {
        ret = (ID3D12RootSignature *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(pRootSignature, ret);
      }
      else
      {
        ret = new WrappedID3D12RootSignature(ret, this);

        WrappedID3D12RootSignature *wrapped = (WrappedID3D12RootSignature *)ret;

        wrapped->sig =
            GetShaderCache()->GetRootSig(pBlobWithRootSignature, (size_t)blobLengthInBytes);

        GetResourceManager()->AddLiveResource(pRootSignature, ret);
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
    // duplicate signatures can be returned, if Create is called with a previous equivalent blob
    if(GetResourceManager()->HasWrapper(real))
    {
      real->Release();
      ID3D12RootSignature *existing = (ID3D12RootSignature *)GetResourceManager()->GetWrapper(real);
      existing->AddRef();
      *ppvRootSignature = existing;
      return ret;
    }

    WrappedID3D12RootSignature *wrapped = new WrappedID3D12RootSignature(real, this);

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

      record->AddChunk(scope.Get());
    }
    else
    {
      wrapped->sig = GetShaderCache()->GetRootSig(pBlobWithRootSignature, blobLengthInBytes);

      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppvRootSignature = (ID3D12RootSignature *)wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_DynamicDescriptorWrite(SerialiserType &ser,
                                                           const DynamicDescriptorWrite *write)
{
  SERIALISE_ELEMENT_LOCAL(desc, write->desc);
  SERIALISE_ELEMENT_LOCAL(dst, ToPortableHandle(write->dest));

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
      m_DynamicDescriptorWrites.push_back(write);
      write.dest->GetHeap()->AddRef();
      m_DynamicDescriptorRefs.push_back(write.desc);
    }

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateConstantBufferView);
      Serialise_DynamicDescriptorWrite(ser, &write);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }

    GetResourceManager()->MarkResourceFrameReferenced(
        WrappedID3D12Resource1::GetResIDFromAddr(pDesc->BufferLocation), eFrameRef_Read);
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
    {
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorWrites.push_back(write);
      write.dest->GetHeap()->AddRef();
      if(pResource && pResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
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
    {
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorWrites.push_back(write);
      write.dest->GetHeap()->AddRef();
      if(pResource && pResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
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
    {
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorWrites.push_back(write);
      write.dest->GetHeap()->AddRef();
      if(pResource && pResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
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
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorWrites.push_back(write);
      write.dest->GetHeap()->AddRef();
    }

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
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorWrites.push_back(write);
      write.dest->GetHeap()->AddRef();
    }

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
bool WrappedID3D12Device::Serialise_CreateCommittedResource(
    SerialiserType &ser, const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource, void **ppvResource)
{
  SERIALISE_ELEMENT_LOCAL(props, *pHeapProperties).Named("pHeapProperties"_lit);
  SERIALISE_ELEMENT(HeapFlags);
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit);
  SERIALISE_ELEMENT(InitialResourceState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  SERIALISE_ELEMENT_LOCAL(guid, riidResource).Named("riidResource"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource1 *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource1 *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(props.Type == D3D12_HEAP_TYPE_UPLOAD && desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      // place large resources in local memory so that initial contents and maps can
      // be cached and copied on the GPU instead of memcpy'd from the CPU every time.
      // smaller resources it's better to just leave them as upload and map into them
      if(desc.Width >= 1024 * 1024)
      {
        RDCLOG("Remapping committed resource %llu from upload to default for efficient replay",
               pResource);
        props.Type = D3D12_HEAP_TYPE_DEFAULT;
        m_UploadResourceIds.insert(pResource);
      }
    }

    APIProps.YUVTextures |= IsYUVFormat(desc.Format);

    // always allow SRVs on replay so we can inspect resources
    desc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

    if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      GPUAddressRange range;
      range.start = gpuAddress;
      range.end = gpuAddress + desc.Width;
      range.id = pResource;

      m_GPUAddresses.AddTo(range);
    }

    ID3D12Resource *ret = NULL;
    HRESULT hr = m_pDevice->CreateCommittedResource(&props, HeapFlags, &desc, InitialResourceState,
                                                    pOptimizedClearValue, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      SetObjName(ret, StringFormat::Fmt("Committed Resource %s ID %llu",
                                        ToStr(desc.Dimension).c_str(), pResource));

      ret = new WrappedID3D12Resource1(ret, this);

      GetResourceManager()->AddLiveResource(pResource, ret);

      SubresourceStateVector &states = m_ResourceStates[GetResID(ret)];
      states.resize(GetNumSubresources(m_pDevice, &desc), InitialResourceState);

      ResourceType type = ResourceType::Texture;
      const char *prefix = "Texture";

      if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
      {
        type = ResourceType::Buffer;
        prefix = "Buffer";
      }
      else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
      {
        prefix = desc.DepthOrArraySize > 1 ? "1D TextureArray" : "1D Texture";

        if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
          prefix = "1D Render Target";
        else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
          prefix = "1D Depth Target";
      }
      else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
      {
        prefix = desc.DepthOrArraySize > 1 ? "2D TextureArray" : "2D Texture";

        if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
          prefix = "2D Render Target";
        else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
          prefix = "2D Depth Target";
      }
      else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
      {
        prefix = "3D Texture";

        if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
          prefix = "3D Render Target";
        else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
          prefix = "3D Depth Target";
      }

      AddResource(pResource, type, prefix);
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                                     D3D12_HEAP_FLAGS HeapFlags,
                                                     const D3D12_RESOURCE_DESC *pDesc,
                                                     D3D12_RESOURCE_STATES InitialResourceState,
                                                     const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                     REFIID riidResource, void **ppvResource)
{
  if(ppvResource == NULL)
    return m_pDevice->CreateCommittedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState,
                                              pOptimizedClearValue, riidResource, NULL);

  if(riidResource != __uuidof(ID3D12Resource) && riidResource != __uuidof(ID3D12Resource1))
    return E_NOINTERFACE;

  void *realptr = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateCommittedResource(
                          pHeapProperties, HeapFlags, pDesc, InitialResourceState,
                          pOptimizedClearValue, riidResource, &realptr));

  ID3D12Resource *real = NULL;
  if(riidResource == __uuidof(ID3D12Resource))
    real = (ID3D12Resource *)realptr;
  else if(riidResource == __uuidof(ID3D12Resource1))
    real = (ID3D12Resource1 *)realptr;

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Resource1 *wrapped = new WrappedID3D12Resource1(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateCommittedResource);
      Serialise_CreateCommittedResource(ser, pHeapProperties, HeapFlags, pDesc, InitialResourceState,
                                        pOptimizedClearValue, riidResource, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Resource;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->m_MapsCount = GetNumSubresources(this, pDesc);
      record->m_Maps = new D3D12ResourceRecord::MapData[record->m_MapsCount];

      record->AddChunk(scope.Get());

      GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    {
      SCOPED_LOCK(m_ResourceStatesLock);
      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      states.resize(GetNumSubresources(m_pDevice, pDesc), InitialResourceState);
    }

    *ppvResource = (ID3D12Resource *)wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateHeap(SerialiserType &ser, const D3D12_HEAP_DESC *pDesc,
                                               REFIID riid, void **ppvHeap)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pHeap, ((WrappedID3D12Heap1 *)*ppvHeap)->GetResourceID())
      .TypedAs("ID3D12Heap *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    void *realptr = NULL;
    HRESULT hr = m_pDevice->CreateHeap(&Descriptor, guid, &realptr);

    ID3D12Heap *ret = NULL;
    if(guid == __uuidof(ID3D12Heap))
      ret = (ID3D12Heap *)realptr;
    else if(guid == __uuidof(ID3D12Heap1))
      ret = (ID3D12Heap1 *)realptr;

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12Heap1(ret, this);

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
    WrappedID3D12Heap1 *wrapped = new WrappedID3D12Heap1(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateHeap);
      Serialise_CreateHeap(ser, pDesc, riid, (void **)&wrapped);

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

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreatePlacedResource(
    SerialiserType &ser, ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *pDesc,
    D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
    void **ppvResource)
{
  SERIALISE_ELEMENT(pHeap);
  SERIALISE_ELEMENT(HeapOffset);
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit);
  SERIALISE_ELEMENT(InitialState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource1 *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource1 *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      GPUAddressRange range;
      range.start = gpuAddress;
      range.end = gpuAddress + Descriptor.Width;
      range.id = pResource;

      m_GPUAddresses.AddTo(range);
    }

    APIProps.YUVTextures |= IsYUVFormat(Descriptor.Format);

    // always allow SRVs on replay so we can inspect resources
    Descriptor.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

    D3D12_HEAP_DESC heapDesc = pHeap->GetDesc();

    // if the heap was from OpenExistingHeap* then we will have removed the shared flags from it as
    // it's CPU-visible and impossible to share.
    // That means any resources placed to it would have had this flag that we then need to remove as
    // well.
    if((heapDesc.Flags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER) == 0)
      Descriptor.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;

    ID3D12Resource *ret = NULL;
    HRESULT hr = m_pDevice->CreatePlacedResource(Unwrap(pHeap), HeapOffset, &Descriptor, InitialState,
                                                 pOptimizedClearValue, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      SetObjName(ret, StringFormat::Fmt("Placed Resource %s ID %llu",
                                        ToStr(Descriptor.Dimension).c_str(), pResource));

      ret = new WrappedID3D12Resource1(ret, this);

      GetResourceManager()->AddLiveResource(pResource, ret);

      SubresourceStateVector &states = m_ResourceStates[GetResID(ret)];
      states.resize(GetNumSubresources(m_pDevice, &Descriptor), InitialState);
    }

    ResourceType type = ResourceType::Texture;
    const char *prefix = "Texture";

    if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      type = ResourceType::Buffer;
      prefix = "Buffer";
    }
    else if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
    {
      prefix = Descriptor.DepthOrArraySize > 1 ? "1D TextureArray" : "1D Texture";

      if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        prefix = "1D Render Target";
      else if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        prefix = "1D Depth Target";
    }
    else if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
      prefix = Descriptor.DepthOrArraySize > 1 ? "2D TextureArray" : "2D Texture";

      if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        prefix = "2D Render Target";
      else if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        prefix = "2D Depth Target";
    }
    else if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    {
      prefix = "3D Texture";

      if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        prefix = "3D Render Target";
      else if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        prefix = "3D Depth Target";
    }

    AddResource(pResource, type, prefix);
    DerivedResource(pHeap, pResource);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset,
                                                  const D3D12_RESOURCE_DESC *pDesc,
                                                  D3D12_RESOURCE_STATES InitialState,
                                                  const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                  REFIID riid, void **ppvResource)
{
  if(ppvResource == NULL)
    return m_pDevice->CreatePlacedResource(Unwrap(pHeap), HeapOffset, pDesc, InitialState,
                                           pOptimizedClearValue, riid, NULL);

  if(riid != __uuidof(ID3D12Resource))
    return E_NOINTERFACE;

  ID3D12Resource *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreatePlacedResource(Unwrap(pHeap), HeapOffset, pDesc,
                                                            InitialState, pOptimizedClearValue,
                                                            riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Resource1 *wrapped = new WrappedID3D12Resource1(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreatePlacedResource);
      Serialise_CreatePlacedResource(ser, pHeap, HeapOffset, pDesc, InitialState,
                                     pOptimizedClearValue, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Resource;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->m_MapsCount = GetNumSubresources(this, pDesc);
      record->m_Maps = new D3D12ResourceRecord::MapData[record->m_MapsCount];

      RDCASSERT(pHeap);

      record->AddParent(GetRecord(pHeap));
      record->AddChunk(scope.Get());

      GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    {
      SCOPED_LOCK(m_ResourceStatesLock);
      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      states.resize(GetNumSubresources(m_pDevice, pDesc), InitialState);
    }

    *ppvResource = (ID3D12Resource *)wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateReservedResource(
    SerialiserType &ser, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
  D3D12NOTIMP("Tiled Resources");
  APIProps.SparseResources = true;
  return true;
}

HRESULT WrappedID3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc,
                                                    D3D12_RESOURCE_STATES InitialState,
                                                    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                    REFIID riid, void **ppvResource)
{
  RDCERR("Tiled Resources are not currently implemented on D3D12");
  return E_NOINTERFACE;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateFence(SerialiserType &ser, UINT64 InitialValue,
                                                D3D12_FENCE_FLAGS Flags, REFIID riid, void **ppFence)
{
  SERIALISE_ELEMENT(InitialValue);
  SERIALISE_ELEMENT(Flags);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pFence, ((WrappedID3D12Fence1 *)*ppFence)->GetResourceID())
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
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12Fence1(ret, this);

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
    WrappedID3D12Fence1 *wrapped = new WrappedID3D12Fence1(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateFence);
      Serialise_CreateFence(ser, InitialValue, Flags, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Resource;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppFence = (ID3D12Fence *)wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateQueryHeap(SerialiserType &ser,
                                                    const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid,
                                                    void **ppvHeap)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit);
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
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
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

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommandSignature(SerialiserType &ser,
                                                           const D3D12_COMMAND_SIGNATURE_DESC *pDesc,
                                                           ID3D12RootSignature *pRootSignature,
                                                           REFIID riid, void **ppvCommandSignature)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit);
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
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      WrappedID3D12CommandSignature *wrapped = new WrappedID3D12CommandSignature(ret, this);

      wrapped->sig.ByteStride = Descriptor.ByteStride;
      wrapped->sig.arguments.insert(wrapped->sig.arguments.begin(), Descriptor.pArgumentDescs,
                                    Descriptor.pArgumentDescs + Descriptor.NumArgumentDescs);

      wrapped->sig.graphics = true;
      wrapped->sig.numDraws = 0;

      // From MSDN, command signatures are either graphics or compute so just search for dispatches:
      // "A given command signature is either a draw or a compute command signature. If a command
      // signature contains a drawing operation, then it is a graphics command signature. Otherwise,
      // the command signature must contain a dispatch operation, and it is a compute command
      // signature."
      for(uint32_t i = 0; i < Descriptor.NumArgumentDescs; i++)
      {
        if(Descriptor.pArgumentDescs[i].Type == D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH)
          wrapped->sig.graphics = false;

        if(Descriptor.pArgumentDescs[i].Type == D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH ||
           Descriptor.pArgumentDescs[i].Type == D3D12_INDIRECT_ARGUMENT_TYPE_DRAW ||
           Descriptor.pArgumentDescs[i].Type == D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED)
          wrapped->sig.numDraws++;
      }

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
    if(GetResourceManager()->HasWrapper(real))
    {
      real->Release();
      ID3D12CommandSignature *existing =
          (ID3D12CommandSignature *)GetResourceManager()->GetWrapper(real);
      existing->AddRef();
      *ppvCommandSignature = existing;
      return ret;
    }

    WrappedID3D12CommandSignature *wrapped = new WrappedID3D12CommandSignature(real, this);

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

    *ppvCommandSignature = (ID3D12CommandSignature *)wrapped;
  }

  return ret;
}

HRESULT WrappedID3D12Device::CreateSharedHandle(ID3D12DeviceChild *pObject,
                                                const SECURITY_ATTRIBUTES *pAttributes,
                                                DWORD Access, LPCWSTR Name, HANDLE *pHandle)
{
  D3D12NOTIMP("Shared Handles / API interop");
  return m_pDevice->CreateSharedHandle(Unwrap(pObject), pAttributes, Access, Name, pHandle);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_DynamicDescriptorCopies(
    SerialiserType &ser, const std::vector<DynamicDescriptorCopy> &DescriptorCopies)
{
  SERIALISE_ELEMENT(DescriptorCopies);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // not optimal, but simple for now. Do a wrapped copy so that internal tracking is also updated
    for(const DynamicDescriptorCopy &copy : DescriptorCopies)
      CopyDescriptorsSimple(1, *copy.dst, *copy.src, copy.type);
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

  std::vector<DynamicDescriptorCopy> copies;

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
      else
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
      m_DynamicDescriptorCopies.insert(m_DynamicDescriptorCopies.end(), copies.begin(), copies.end());
      for(size_t i = 0; i < copies.size(); i++)
      {
        copies[i].src->GetHeap()->AddRef();
        copies[i].dst->GetHeap()->AddRef();
        m_DynamicDescriptorRefs.push_back(*copies[i].src);
      }
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

    std::vector<DynamicDescriptorCopy> copies;
    copies.reserve(NumDescriptors);
    for(UINT i = 0; i < NumDescriptors; i++)
      copies.push_back(DynamicDescriptorCopy(&dst[i], &src[i], DescriptorHeapsType));

    {
      SCOPED_LOCK(m_DynDescLock);
      m_DynamicDescriptorCopies.insert(m_DynamicDescriptorCopies.end(), copies.begin(), copies.end());
      for(size_t i = 0; i < copies.size(); i++)
      {
        copies[i].src->GetHeap()->AddRef();
        copies[i].dst->GetHeap()->AddRef();
        m_DynamicDescriptorRefs.push_back(*copies[i].src);
      }
    }

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CopyDescriptorsSimple);
      Serialise_DynamicDescriptorCopies(ser, copies);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }
  }
  else
  {
    for(UINT i = 0; i < NumDescriptors; i++)
      dst[i].CopyFrom(src[i]);
  }
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_OpenSharedHandle(SerialiserType &ser, HANDLE NTHandle,
                                                     REFIID riid, void **ppvObj)
{
  SERIALISE_ELEMENT_LOCAL(ResourceRIID, riid);

  SERIALISE_CHECK_READ_ERRORS();

  bool isRes = (ResourceRIID == __uuidof(ID3D12Resource) ? true : false);
  if(isRes)
  {
    D3D12_RESOURCE_DESC desc;
    D3D12_HEAP_PROPERTIES heapProperties;
    D3D12_HEAP_FLAGS heapFlags;

    ID3D12Resource *res{};
    if(ser.IsWriting())
    {
      res = (ID3D12Resource *)*ppvObj;
      desc = res->GetDesc();
      res->GetHeapProperties(&heapProperties, &heapFlags);
    }

    SERIALISE_ELEMENT_LOCAL(resourceId, GetResID(res));
    SERIALISE_ELEMENT(desc);
    SERIALISE_ELEMENT(heapProperties);
    SERIALISE_ELEMENT(heapFlags);

    if(IsReplayingAndReading())
    {
      D3D12_RESOURCE_STATES InitialResourceState = D3D12_RESOURCE_STATE_COMMON;
      const D3D12_CLEAR_VALUE *pOptimizedClearValue = NULL;

      // always allow SRVs on replay so we can inspect resources
      desc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

      HRESULT hr;
      ID3D12Resource *ret;
      hr = m_pDevice->CreateCommittedResource(&heapProperties, heapFlags, &desc, InitialResourceState,
                                              pOptimizedClearValue, ResourceRIID, (void **)&ret);
      if(FAILED(hr))
      {
        RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
        return false;
      }
      else
      {
        SetObjName(ret, StringFormat::Fmt("Shared Resource %s ID %llu",
                                          ToStr(desc.Dimension).c_str(), resourceId));

        ret = new WrappedID3D12Resource1(ret, this);

        GetResourceManager()->AddLiveResource(resourceId, ret);

        SubresourceStateVector &states = m_ResourceStates[GetResID(ret)];
        states.resize(GetNumSubresources(m_pDevice, &desc), InitialResourceState);

        ResourceType type = ResourceType::Texture;
        const char *prefix = "Texture";

        if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
          type = ResourceType::Buffer;
          prefix = "Buffer";
        }
        else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
        {
          prefix = desc.DepthOrArraySize > 1 ? "1D TextureArray" : "1D Texture";

          if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
            prefix = "1D Render Target";
          else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            prefix = "1D Depth Target";
        }
        else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        {
          prefix = desc.DepthOrArraySize > 1 ? "2D TextureArray" : "2D Texture";

          if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
            prefix = "2D Render Target";
          else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            prefix = "2D Depth Target";
        }
        else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
        {
          prefix = "3D Texture";

          if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
            prefix = "3D Render Target";
          else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            prefix = "3D Depth Target";
        }

        AddResource(resourceId, type, prefix);
      }
    }
  }
  else
  {
    RDCERR("Unknown type of resource being shared");
  }

  return true;
}

HRESULT WrappedID3D12Device::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **ppvObj)
{
  if(IsReplayMode(m_State))
  {
    RDCERR("Don't support opening shared handle during replay.");
    return E_NOTIMPL;
  }

  if(ppvObj == NULL)
    return E_INVALIDARG;

  bool isRes = (riid == __uuidof(ID3D12Resource) ? true : false);
  if(isRes)
  {
    void *ret = NULL;
    HRESULT hr;
    SERIALISE_TIME_CALL(hr = m_pDevice->OpenSharedHandle(NTHandle, riid, &ret));

    if(FAILED(hr))
    {
      IUnknown *unk = (IUnknown *)ret;
      SAFE_RELEASE(unk);
      return hr;
    }
    else
    {
      ID3D12Resource *real = (ID3D12Resource *)ret;

      WrappedID3D12Resource1 *wrapped = new WrappedID3D12Resource1(real, this);
      *ppvObj = (ID3D12Resource *)wrapped;

      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_OpenSharedHandle);
      Serialise_OpenSharedHandle(ser, NTHandle, riid, ppvObj);

      D3D12_RESOURCE_DESC desc = wrapped->GetDesc();
      D3D12_RESOURCE_STATES InitialResourceState = D3D12_RESOURCE_STATE_COMMON;

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Resource;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->m_MapsCount = GetNumSubresources(this, &desc);
      record->m_Maps = new D3D12ResourceRecord::MapData[record->m_MapsCount];

      record->AddChunk(scope.Get());

      GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());

      {
        SCOPED_LOCK(m_ResourceStatesLock);
        SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

        states.resize(GetNumSubresources(m_pDevice, &desc), InitialResourceState);
      }

      return S_OK;
    }
  }

  return E_NOINTERFACE;
}

HRESULT WrappedID3D12Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle)
{
  D3D12NOTIMP("Shared Handles / API interop");
  return m_pDevice->OpenSharedHandleByName(Name, Access, pNTHandle);
}

HRESULT WrappedID3D12Device::MakeResident(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
  ID3D12Pageable **unwrapped = GetTempArray<ID3D12Pageable *>(NumObjects);

  for(UINT i = 0; i < NumObjects; i++)
  {
    if(WrappedID3D12DescriptorHeap::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12DescriptorHeap *heap = (WrappedID3D12DescriptorHeap *)ppObjects[i];
      heap->SetResident(true);
      unwrapped[i] = heap->GetReal();
    }
    else if(WrappedID3D12Resource1::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12Resource1 *res = (WrappedID3D12Resource1 *)ppObjects[i];
      res->SetResident(true);
      unwrapped[i] = res->GetReal();
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
  ID3D12Pageable **unwrapped = GetTempArray<ID3D12Pageable *>(NumObjects);

  for(UINT i = 0; i < NumObjects; i++)
  {
    if(WrappedID3D12DescriptorHeap::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12DescriptorHeap *heap = (WrappedID3D12DescriptorHeap *)ppObjects[i];
      heap->SetResident(false);
      unwrapped[i] = heap->GetReal();
    }
    else if(WrappedID3D12Resource1::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12Resource1 *res = (WrappedID3D12Resource1 *)ppObjects[i];
      res->SetResident(false);
      unwrapped[i] = res->GetReal();
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
  if(Feature == D3D12_FEATURE_SHADER_MODEL)
  {
    D3D12_FEATURE_DATA_SHADER_MODEL *model = (D3D12_FEATURE_DATA_SHADER_MODEL *)pFeatureSupportData;
    if(FeatureSupportDataSize != sizeof(D3D12_FEATURE_DATA_SHADER_MODEL))
      return E_INVALIDARG;

    // don't support sm6.0 and over
    model->HighestShaderModel = RDCMIN(model->HighestShaderModel, D3D_SHADER_MODEL_5_1);

    return S_OK;
  }
  if(Feature == D3D12_FEATURE_PROTECTED_RESOURCE_SESSION_SUPPORT)
  {
    D3D12_FEATURE_DATA_PROTECTED_RESOURCE_SESSION_SUPPORT *opts =
        (D3D12_FEATURE_DATA_PROTECTED_RESOURCE_SESSION_SUPPORT *)pFeatureSupportData;

    // declare no support for protected resource sessions. We still wrap it just in case but we
    // can't capture or replay this properly.
    opts->Support = D3D12_PROTECTED_RESOURCE_SESSION_SUPPORT_FLAG_NONE;

    return S_OK;
  }
  if(Feature == D3D12_FEATURE_D3D12_OPTIONS5)
  {
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 *opts =
        (D3D12_FEATURE_DATA_D3D12_OPTIONS5 *)pFeatureSupportData;
    if(FeatureSupportDataSize != sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5))
      return E_INVALIDARG;

    // don't support raytracing
    opts->RaytracingTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;

    return S_OK;
  }
  return m_pDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
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
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommittedResource,
                                const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc,
                                D3D12_RESOURCE_STATES InitialResourceState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource,
                                void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateHeap, const D3D12_HEAP_DESC *pDesc,
                                REFIID riid, void **ppvHeap);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreatePlacedResource, ID3D12Heap *pHeap,
                                UINT64 HeapOffset, const D3D12_RESOURCE_DESC *pDesc,
                                D3D12_RESOURCE_STATES InitialState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
                                void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateReservedResource,
                                const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
                                void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateFence, UINT64 InitialValue,
                                D3D12_FENCE_FLAGS Flags, REFIID riid, void **ppFence);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateQueryHeap,
                                const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommandSignature,
                                const D3D12_COMMAND_SIGNATURE_DESC *pDesc,
                                ID3D12RootSignature *pRootSignature, REFIID riid,
                                void **ppvCommandSignature);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, DynamicDescriptorCopies,
                                const std::vector<DynamicDescriptorCopy> &DescriptorCopies);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, OpenSharedHandle, HANDLE NTHandle,
                                REFIID riid, void **ppvObj);
