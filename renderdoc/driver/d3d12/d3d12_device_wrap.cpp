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

#include "d3d12_device.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_resources.h"

bool WrappedID3D12Device::Serialise_CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *pDesc,
                                                       REFIID riid, void **ppCommandQueue)
{
  SERIALISE_ELEMENT_PTR(D3D12_COMMAND_QUEUE_DESC, Descriptor, pDesc);
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, Queue,
                    ((WrappedID3D12CommandQueue *)*ppCommandQueue)->GetResourceID());

  if(m_State == READING)
  {
    ID3D12CommandQueue *ret = NULL;
    HRESULT hr = m_pDevice->CreateCommandQueue(&Descriptor, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12CommandQueue(ret, this, m_pSerialiser, m_State);

      GetResourceManager()->AddLiveResource(Queue, ret);

      if(Descriptor.Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
      {
        if(m_Queue != NULL)
          RDCERR("Don't support multiple direct queues yet!");

        m_Queue = (WrappedID3D12CommandQueue *)ret;

        CreateInternalResources();
      }
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
  HRESULT ret = m_pDevice->CreateCommandQueue(pDesc, riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12CommandQueue *wrapped =
        new WrappedID3D12CommandQueue(real, this, m_pSerialiser, m_State);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_COMMAND_QUEUE);
      Serialise_CreateCommandQueue(pDesc, riid, (void **)&wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    if(pDesc->Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
      if(m_Queue != NULL)
        RDCERR("Don't support multiple queues yet!");

      m_Queue = wrapped;

      CreateInternalResources();
    }

    *ppCommandQueue = (ID3D12CommandQueue *)wrapped;
  }

  return ret;
}

bool WrappedID3D12Device::Serialise_CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type,
                                                           REFIID riid, void **ppCommandAllocator)
{
  SERIALISE_ELEMENT(D3D12_COMMAND_LIST_TYPE, ListType, type);
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, Alloc,
                    ((WrappedID3D12CommandAllocator *)*ppCommandAllocator)->GetResourceID());

  if(m_State == READING)
  {
    ID3D12CommandAllocator *ret = NULL;
    HRESULT hr = m_pDevice->CreateCommandAllocator(ListType, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12CommandAllocator(ret, this);

      GetResourceManager()->AddLiveResource(Alloc, ret);
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
  HRESULT ret = m_pDevice->CreateCommandAllocator(type, riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12CommandAllocator *wrapped = new WrappedID3D12CommandAllocator(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_COMMAND_ALLOCATOR);
      Serialise_CreateCommandAllocator(type, riid, (void **)&wrapped);

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

HRESULT WrappedID3D12Device::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
                                               ID3D12CommandAllocator *pCommandAllocator,
                                               ID3D12PipelineState *pInitialState, REFIID riid,
                                               void **ppCommandList)
{
  if(ppCommandList == NULL)
    return m_pDevice->CreateCommandList(nodeMask, type, Unwrap(pCommandAllocator),
                                        Unwrap(pInitialState), riid, NULL);

  if(riid != __uuidof(ID3D12GraphicsCommandList))
    return E_NOINTERFACE;

  ID3D12GraphicsCommandList *real = NULL;
  HRESULT ret = m_pDevice->CreateCommandList(nodeMask, type, Unwrap(pCommandAllocator),
                                             Unwrap(pInitialState), riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    WrappedID3D12GraphicsCommandList *wrapped =
        new WrappedID3D12GraphicsCommandList(real, this, m_pSerialiser, m_State);

    if(m_State >= WRITING)
    {
      // we just serialise out command allocator creation as a reset, since it's equivalent.
      wrapped->SetInitParams(riid, nodeMask, type);
      wrapped->Reset(pCommandAllocator, pInitialState);
    }

    // during replay, the caller is responsible for calling AddLiveResource as this function
    // can be called from ID3D12GraphicsCommandList::Reset serialising

    *ppCommandList = (ID3D12GraphicsCommandList *)wrapped;
  }

  return ret;
}

bool WrappedID3D12Device::Serialise_CreateGraphicsPipelineState(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
  SERIALISE_ELEMENT_PTR(D3D12_GRAPHICS_PIPELINE_STATE_DESC, Descriptor, pDesc);
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, Pipe,
                    ((WrappedID3D12PipelineState *)*ppPipelineState)->GetResourceID());

  if(m_State == READING)
  {
    ID3D12PipelineState *ret = NULL;
    HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&Descriptor, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12PipelineState(ret, this);

      WrappedID3D12PipelineState *wrapped = (WrappedID3D12PipelineState *)ret;

      wrapped->graphics = new D3D12_GRAPHICS_PIPELINE_STATE_DESC(Descriptor);

      D3D12_SHADER_BYTECODE *shaders[] = {
          &wrapped->graphics->VS, &wrapped->graphics->HS, &wrapped->graphics->DS,
          &wrapped->graphics->GS, &wrapped->graphics->PS,
      };

      for(size_t i = 0; i < ARRAY_COUNT(shaders); i++)
      {
        if(shaders[i]->BytecodeLength == 0)
          shaders[i]->pShaderBytecode = NULL;
        else
          shaders[i]->pShaderBytecode = WrappedID3D12PipelineState::AddShader(*shaders[i], this);
      }

      GetResourceManager()->AddLiveResource(Pipe, ret);
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
  HRESULT ret = m_pDevice->CreateGraphicsPipelineState(&unwrappedDesc, riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_GRAPHICS_PIPE);
      Serialise_CreateGraphicsPipelineState(pDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_PipelineState;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddParent(GetRecord(pDesc->pRootSignature));

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppPipelineState = (ID3D12PipelineState *)wrapped;
  }

  return ret;
}

bool WrappedID3D12Device::Serialise_CreateComputePipelineState(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
  SERIALISE_ELEMENT_PTR(D3D12_COMPUTE_PIPELINE_STATE_DESC, Descriptor, pDesc);
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, Pipe,
                    ((WrappedID3D12PipelineState *)*ppPipelineState)->GetResourceID());

  if(m_State == READING)
  {
    ID3D12PipelineState *ret = NULL;
    HRESULT hr = m_pDevice->CreateComputePipelineState(&Descriptor, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(ret, this);
      ret = wrapped;

      wrapped->compute = new D3D12_COMPUTE_PIPELINE_STATE_DESC(Descriptor);

      wrapped->compute->CS.pShaderBytecode =
          WrappedID3D12PipelineState::AddShader(wrapped->compute->CS, this);

      GetResourceManager()->AddLiveResource(Pipe, ret);
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
  HRESULT ret = m_pDevice->CreateComputePipelineState(&unwrappedDesc, riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12PipelineState *wrapped = new WrappedID3D12PipelineState(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_COMPUTE_PIPE);
      Serialise_CreateComputePipelineState(pDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_PipelineState;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddParent(GetRecord(pDesc->pRootSignature));

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppPipelineState = (ID3D12PipelineState *)wrapped;
  }

  return ret;
}

bool WrappedID3D12Device::Serialise_CreateDescriptorHeap(
    const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc, REFIID riid, void **ppvHeap)
{
  SERIALISE_ELEMENT_PTR(D3D12_DESCRIPTOR_HEAP_DESC, Descriptor, pDescriptorHeapDesc);
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, Heap, ((WrappedID3D12DescriptorHeap *)*ppvHeap)->GetResourceID());

  if(m_State == READING)
  {
    ID3D12DescriptorHeap *ret = NULL;
    HRESULT hr = m_pDevice->CreateDescriptorHeap(&Descriptor, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12DescriptorHeap(ret, this, Descriptor);

      GetResourceManager()->AddLiveResource(Heap, ret);
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
  HRESULT ret = m_pDevice->CreateDescriptorHeap(pDescriptorHeapDesc, riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12DescriptorHeap *wrapped =
        new WrappedID3D12DescriptorHeap(real, this, *pDescriptorHeapDesc);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_HEAP);
      Serialise_CreateDescriptorHeap(pDescriptorHeapDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_DescriptorHeap;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());

      {
        SCOPED_LOCK(m_CapTransitionLock);
        if(m_State != WRITING_CAPFRAME)
          GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());
        else
          GetResourceManager()->MarkPendingDirty(wrapped->GetResourceID());
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppvHeap = (ID3D12DescriptorHeap *)wrapped;
  }

  return ret;
}

bool WrappedID3D12Device::Serialise_CreateRootSignature(UINT nodeMask,
                                                        const void *pBlobWithRootSignature,
                                                        SIZE_T blobLengthInBytes, REFIID riid,
                                                        void **ppvRootSignature)
{
  SERIALISE_ELEMENT(UINT, mask, nodeMask);
  SERIALISE_ELEMENT(uint32_t, blobLen, (uint32_t)blobLengthInBytes);
  SERIALISE_ELEMENT_BUF(byte *, blobBytes, pBlobWithRootSignature, blobLen);
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, Sig,
                    ((WrappedID3D12RootSignature *)*ppvRootSignature)->GetResourceID());

  if(m_State == READING)
  {
    ID3D12RootSignature *ret = NULL;
    HRESULT hr = m_pDevice->CreateRootSignature(mask, blobBytes, blobLen, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12RootSignature(ret, this);

      WrappedID3D12RootSignature *wrapped = (WrappedID3D12RootSignature *)ret;

      wrapped->sig = D3D12DebugManager::GetRootSig(blobBytes, blobLen);

      GetResourceManager()->AddLiveResource(Sig, ret);
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
  HRESULT ret = m_pDevice->CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes,
                                               riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

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

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_ROOT_SIG);
      Serialise_CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid,
                                    (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_RootSignature;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      wrapped->sig = D3D12DebugManager::GetRootSig(pBlobWithRootSignature, blobLengthInBytes);

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppvRootSignature = (ID3D12RootSignature *)wrapped;
  }

  return ret;
}

void WrappedID3D12Device::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc,
                                                   D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  GetWrapped(DestDescriptor)->Init(pDesc);
  return m_pDevice->CreateConstantBufferView(pDesc, Unwrap(DestDescriptor));
}

void WrappedID3D12Device::CreateShaderResourceView(ID3D12Resource *pResource,
                                                   const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                                   D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  GetWrapped(DestDescriptor)->Init(pResource, pDesc);
  return m_pDevice->CreateShaderResourceView(Unwrap(pResource), pDesc, Unwrap(DestDescriptor));
}

void WrappedID3D12Device::CreateUnorderedAccessView(ID3D12Resource *pResource,
                                                    ID3D12Resource *pCounterResource,
                                                    const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                                                    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  GetWrapped(DestDescriptor)->Init(pResource, pCounterResource, pDesc);
  return m_pDevice->CreateUnorderedAccessView(Unwrap(pResource), Unwrap(pCounterResource), pDesc,
                                              Unwrap(DestDescriptor));
}

void WrappedID3D12Device::CreateRenderTargetView(ID3D12Resource *pResource,
                                                 const D3D12_RENDER_TARGET_VIEW_DESC *pDesc,
                                                 D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  GetWrapped(DestDescriptor)->Init(pResource, pDesc);
  return m_pDevice->CreateRenderTargetView(Unwrap(pResource), pDesc, Unwrap(DestDescriptor));
}

void WrappedID3D12Device::CreateDepthStencilView(ID3D12Resource *pResource,
                                                 const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                                 D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  GetWrapped(DestDescriptor)->Init(pResource, pDesc);
  return m_pDevice->CreateDepthStencilView(Unwrap(pResource), pDesc, Unwrap(DestDescriptor));
}

void WrappedID3D12Device::CreateSampler(const D3D12_SAMPLER_DESC *pDesc,
                                        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  GetWrapped(DestDescriptor)->Init(pDesc);
  return m_pDevice->CreateSampler(pDesc, Unwrap(DestDescriptor));
}

bool WrappedID3D12Device::Serialise_CreateCommittedResource(
    const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource, void **ppvResource)
{
  SERIALISE_ELEMENT(D3D12_HEAP_PROPERTIES, props, *pHeapProperties);
  SERIALISE_ELEMENT(D3D12_HEAP_FLAGS, flags, HeapFlags);
  SERIALISE_ELEMENT(D3D12_RESOURCE_DESC, desc, *pDesc);
  SERIALISE_ELEMENT(D3D12_RESOURCE_STATES, state, InitialResourceState);

  SERIALISE_ELEMENT(bool, HasClearValue, pOptimizedClearValue != NULL);
  SERIALISE_ELEMENT_OPT(D3D12_CLEAR_VALUE, clearVal, *pOptimizedClearValue, HasClearValue);

  SERIALISE_ELEMENT(IID, guid, riidResource);
  SERIALISE_ELEMENT(ResourceId, Res, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID());

  if(m_State == READING)
  {
    pOptimizedClearValue = HasClearValue ? &clearVal : NULL;

    ID3D12Resource *ret = NULL;
    HRESULT hr = m_pDevice->CreateCommittedResource(&props, flags, &desc, state,
                                                    pOptimizedClearValue, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12Resource(ret, this);

      GetResourceManager()->AddLiveResource(Res, ret);

      SubresourceStateVector &states = m_ResourceStates[GetResID(ret)];
      states.resize(GetNumSubresources(&desc), state);
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

  if(riidResource != __uuidof(ID3D12Resource))
    return E_NOINTERFACE;

  ID3D12Resource *real = NULL;
  HRESULT ret =
      m_pDevice->CreateCommittedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState,
                                         pOptimizedClearValue, riidResource, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12Resource *wrapped = new WrappedID3D12Resource(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_COMMITTED_RESOURCE);
      Serialise_CreateCommittedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState,
                                        pOptimizedClearValue, riidResource, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Resource;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());

      {
        SCOPED_LOCK(m_CapTransitionLock);
        if(m_State != WRITING_CAPFRAME)
          GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());
        else
          GetResourceManager()->MarkPendingDirty(wrapped->GetResourceID());
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    {
      SCOPED_LOCK(m_ResourceStatesLock);
      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      states.resize(GetNumSubresources(pDesc), InitialResourceState);
    }

    *ppvResource = (ID3D12Resource *)wrapped;
  }

  return ret;
}

bool WrappedID3D12Device::Serialise_CreateHeap(const D3D12_HEAP_DESC *pDesc, REFIID riid,
                                               void **ppvHeap)
{
  SERIALISE_ELEMENT(D3D12_HEAP_DESC, desc, *pDesc);
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, Res, ((WrappedID3D12Heap *)*ppvHeap)->GetResourceID());

  if(m_State == READING)
  {
    ID3D12Heap *ret = NULL;
    HRESULT hr = m_pDevice->CreateHeap(&desc, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12Heap(ret, this);

      GetResourceManager()->AddLiveResource(Res, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateHeap(const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap)
{
  if(ppvHeap == NULL)
    return m_pDevice->CreateHeap(pDesc, riid, ppvHeap);

  if(riid != __uuidof(ID3D12Heap))
    return E_NOINTERFACE;

  ID3D12Heap *real = NULL;
  HRESULT ret = m_pDevice->CreateHeap(pDesc, riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12Heap *wrapped = new WrappedID3D12Heap(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_HEAP);
      Serialise_CreateHeap(pDesc, riid, (void **)&wrapped);

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

bool WrappedID3D12Device::Serialise_CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset,
                                                         const D3D12_RESOURCE_DESC *pDesc,
                                                         D3D12_RESOURCE_STATES InitialState,
                                                         const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                         REFIID riid, void **ppvResource)
{
  SERIALISE_ELEMENT(ResourceId, Heap, GetResID(pHeap));
  SERIALISE_ELEMENT(UINT64, Offset, HeapOffset);
  SERIALISE_ELEMENT(D3D12_RESOURCE_DESC, desc, *pDesc);
  SERIALISE_ELEMENT(D3D12_RESOURCE_STATES, state, InitialState);

  SERIALISE_ELEMENT(bool, HasClearValue, pOptimizedClearValue != NULL);
  SERIALISE_ELEMENT_OPT(D3D12_CLEAR_VALUE, clearVal, *pOptimizedClearValue, HasClearValue);

  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, Res, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID());

  if(m_State == READING)
  {
    pHeap = GetResourceManager()->GetLiveAs<ID3D12Heap>(Heap);
    pOptimizedClearValue = HasClearValue ? &clearVal : NULL;

    ID3D12Resource *ret = NULL;
    HRESULT hr = m_pDevice->CreatePlacedResource(Unwrap(pHeap), Offset, &desc, state,
                                                 pOptimizedClearValue, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12Resource(ret, this);

      GetResourceManager()->AddLiveResource(Res, ret);

      SubresourceStateVector &states = m_ResourceStates[GetResID(ret)];
      states.resize(GetNumSubresources(&desc), state);
    }
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
  HRESULT ret = m_pDevice->CreatePlacedResource(Unwrap(pHeap), HeapOffset, pDesc, InitialState,
                                                pOptimizedClearValue, riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12Resource *wrapped = new WrappedID3D12Resource(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_PLACED_RESOURCE);
      Serialise_CreatePlacedResource(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue,
                                     riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Resource;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      RDCASSERT(pHeap);

      record->AddParent(GetRecord(pHeap));
      record->AddChunk(scope.Get());

      {
        SCOPED_LOCK(m_CapTransitionLock);
        if(m_State != WRITING_CAPFRAME)
          GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());
        else
          GetResourceManager()->MarkPendingDirty(wrapped->GetResourceID());
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    {
      SCOPED_LOCK(m_ResourceStatesLock);
      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      states.resize(GetNumSubresources(pDesc), InitialState);
    }

    *ppvResource = (ID3D12Resource *)wrapped;
  }

  return ret;
}

HRESULT WrappedID3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc,
                                                    D3D12_RESOURCE_STATES InitialState,
                                                    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                    REFIID riid, void **ppvResource)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  return m_pDevice->CreateReservedResource(pDesc, InitialState, pOptimizedClearValue, riid,
                                           ppvResource);
}

bool WrappedID3D12Device::Serialise_CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags,
                                                REFIID riid, void **ppFence)
{
  SERIALISE_ELEMENT(UINT64, val, InitialValue);
  SERIALISE_ELEMENT(D3D12_FENCE_FLAGS, flags, Flags);
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, Fence, ((WrappedID3D12Fence *)*ppFence)->GetResourceID());

  if(m_State == READING)
  {
    ID3D12Fence *ret = NULL;
    HRESULT hr = m_pDevice->CreateFence(val, flags, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12Fence(ret, this);

      GetResourceManager()->AddLiveResource(Fence, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid,
                                         void **ppFence)
{
  if(ppFence == NULL)
    return m_pDevice->CreateFence(InitialValue, Flags, riid, NULL);

  if(riid != __uuidof(ID3D12Fence))
    return E_NOINTERFACE;

  ID3D12Fence *real = NULL;
  HRESULT ret = m_pDevice->CreateFence(InitialValue, Flags, riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12Fence *wrapped = new WrappedID3D12Fence(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_FENCE);
      Serialise_CreateFence(InitialValue, Flags, riid, (void **)&wrapped);

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

bool WrappedID3D12Device::Serialise_CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid,
                                                    void **ppvHeap)
{
  SERIALISE_ELEMENT(D3D12_QUERY_HEAP_DESC, desc, *pDesc);
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, QueryHeap, ((WrappedID3D12QueryHeap *)*ppvHeap)->GetResourceID());

  if(m_State == READING)
  {
    ID3D12QueryHeap *ret = NULL;
    HRESULT hr = m_pDevice->CreateQueryHeap(&desc, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12QueryHeap(ret, this);

      GetResourceManager()->AddLiveResource(QueryHeap, ret);
    }
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
  HRESULT ret = m_pDevice->CreateQueryHeap(pDesc, riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D12QueryHeap *wrapped = new WrappedID3D12QueryHeap(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_QUERY_HEAP);
      Serialise_CreateQueryHeap(pDesc, riid, (void **)&wrapped);

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

bool WrappedID3D12Device::Serialise_CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *pDesc,
                                                           ID3D12RootSignature *pRootSignature,
                                                           REFIID riid, void **ppvCommandSignature)
{
  SERIALISE_ELEMENT(D3D12_COMMAND_SIGNATURE_DESC, desc, *pDesc);
  SERIALISE_ELEMENT(ResourceId, RootSig, GetResID(pRootSignature));
  SERIALISE_ELEMENT(IID, guid, riid);
  SERIALISE_ELEMENT(ResourceId, CommandSig,
                    ((WrappedID3D12CommandSignature *)*ppvCommandSignature)->GetResourceID());

  if(m_State == READING)
  {
    pRootSignature = NULL;
    if(RootSig != ResourceId())
      pRootSignature = GetResourceManager()->GetLiveAs<ID3D12RootSignature>(RootSig);

    ID3D12CommandSignature *ret = NULL;
    HRESULT hr = m_pDevice->CreateCommandSignature(&desc, pRootSignature, guid, (void **)&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D12CommandSignature(ret, this);

      GetResourceManager()->AddLiveResource(CommandSig, ret);
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
  HRESULT ret =
      m_pDevice->CreateCommandSignature(pDesc, Unwrap(pRootSignature), riid, (void **)&real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

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

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_COMMAND_SIG);
      Serialise_CreateCommandSignature(pDesc, pRootSignature, riid, (void **)&wrapped);

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
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  return m_pDevice->CreateSharedHandle(Unwrap(pObject), pAttributes, Access, Name, pHandle);
}

void WrappedID3D12Device::CopyDescriptors(
    UINT NumDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pDestDescriptorRangeStarts,
    const UINT *pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcDescriptorRangeStarts,
    const UINT *pSrcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
  D3D12_CPU_DESCRIPTOR_HANDLE *dstStarts = new D3D12_CPU_DESCRIPTOR_HANDLE[NumDestDescriptorRanges];
  D3D12_CPU_DESCRIPTOR_HANDLE *srcStarts = new D3D12_CPU_DESCRIPTOR_HANDLE[NumSrcDescriptorRanges];

  for(UINT i = 0; i < NumDestDescriptorRanges; i++)
    dstStarts[i] = Unwrap(pDestDescriptorRangeStarts[i]);

  for(UINT i = 0; i < NumSrcDescriptorRanges; i++)
    srcStarts[i] = Unwrap(pSrcDescriptorRangeStarts[i]);

  m_pDevice->CopyDescriptors(NumDestDescriptorRanges, dstStarts, pDestDescriptorRangeSizes,
                             NumSrcDescriptorRanges, srcStarts, pSrcDescriptorRangeSizes,
                             DescriptorHeapsType);

  UINT srcRange = 0, dstRange = 0;
  UINT srcIdx = 0, dstIdx = 0;

  D3D12Descriptor *src = GetWrapped(pSrcDescriptorRangeStarts[0]);
  D3D12Descriptor *dst = GetWrapped(pDestDescriptorRangeStarts[0]);

  for(; srcRange < NumSrcDescriptorRanges && dstRange < NumDestDescriptorRanges;)
  {
    dst[dstIdx].CopyFrom(src[srcIdx]);

    srcIdx++;
    dstIdx++;

    // move source onto the next range
    if(srcIdx >= pSrcDescriptorRangeSizes[srcRange])
    {
      srcRange++;
      srcIdx = 0;

      // check srcRange is valid - we might be about to exit the loop from reading off the end
      if(srcRange < NumSrcDescriptorRanges)
        src = GetWrapped(pSrcDescriptorRangeStarts[srcRange]);
    }

    if(dstIdx >= pDestDescriptorRangeSizes[dstRange])
    {
      dstRange++;
      dstIdx = 0;

      if(dstRange < NumDestDescriptorRanges)
        dst = GetWrapped(pDestDescriptorRangeStarts[dstRange]);
    }
  }

  SAFE_DELETE_ARRAY(dstStarts);
  SAFE_DELETE_ARRAY(srcStarts);
}

void WrappedID3D12Device::CopyDescriptorsSimple(UINT NumDescriptors,
                                                D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
                                                D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
                                                D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
  m_pDevice->CopyDescriptorsSimple(NumDescriptors, Unwrap(DestDescriptorRangeStart),
                                   Unwrap(SrcDescriptorRangeStart), DescriptorHeapsType);

  D3D12Descriptor *src = GetWrapped(SrcDescriptorRangeStart);
  D3D12Descriptor *dst = GetWrapped(DestDescriptorRangeStart);

  for(UINT i = 0; i < NumDescriptors; i++)
    dst[i].CopyFrom(src[i]);
}

HRESULT WrappedID3D12Device::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **ppvObj)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  return m_pDevice->OpenSharedHandle(NTHandle, riid, ppvObj);
}

HRESULT WrappedID3D12Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  return m_pDevice->OpenSharedHandleByName(Name, Access, pNTHandle);
}

HRESULT WrappedID3D12Device::MakeResident(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
  RDCUNIMPLEMENTED("MakeResident");    // need to unwrap objects
  return m_pDevice->MakeResident(NumObjects, ppObjects);
}

HRESULT WrappedID3D12Device::Evict(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
  RDCUNIMPLEMENTED("Evict");    // need to unwrap objects
  return m_pDevice->Evict(NumObjects, ppObjects);
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
