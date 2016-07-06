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

#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"

WRAPPED_POOL_INST(WrappedID3D12CommandQueue);
WRAPPED_POOL_INST(WrappedID3D12GraphicsCommandList);

template <>
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

template <>
ID3D12CommandList *Unwrap(ID3D12CommandList *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

template <>
ID3D12CommandQueue *Unwrap(ID3D12CommandQueue *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12CommandQueue *)obj)->GetReal();
}

ULONG STDMETHODCALLTYPE DummyID3D12DebugCommandQueue::AddRef()
{
  m_pQueue->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D12DebugCommandQueue::Release()
{
  m_pQueue->Release();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D12DebugCommandList::AddRef()
{
  m_pList->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D12DebugCommandList::Release()
{
  m_pList->Release();
  return 1;
}

WrappedID3D12CommandQueue::WrappedID3D12CommandQueue(ID3D12CommandQueue *real,
                                                     WrappedID3D12Device *device,
                                                     Serialiser *serialiser)
    : RefCounter12(real), m_pDevice(device), m_pQueue(real)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this,
                                                              sizeof(WrappedID3D12CommandQueue));

  m_pQueue->QueryInterface(__uuidof(ID3D12DebugCommandQueue), (void **)&m_DummyDebug.m_pReal);

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = READING;
    m_pSerialiser = serialiser;
  }
  else
  {
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, true);
    m_State = WRITING_IDLE;

    m_pSerialiser->SetDebugText(true);
  }

  m_pSerialiser->SetUserData(m_pDevice->GetResourceManager());

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_QueueRecord = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_QueueRecord = m_pDevice->GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_QueueRecord->DataInSerialiser = false;
    m_QueueRecord->SpecialResource = true;
    m_QueueRecord->Length = 0;
    m_QueueRecord->NumSubResources = 0;
    m_QueueRecord->SubResources = NULL;
    m_QueueRecord->ignoreSerialise = true;
  }

  m_pDevice->SoftRef();
}

WrappedID3D12CommandQueue::~WrappedID3D12CommandQueue()
{
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D12CommandQueue *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12CommandQueue))
  {
    *ppvObject = (ID3D12CommandQueue *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12Pageable))
  {
    *ppvObject = (ID3D12Pageable *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12DeviceChild))
  {
    *ppvObject = (ID3D12DeviceChild *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12Object))
  {
    *ppvObject = (ID3D12DeviceChild *)this;
    AddRef();
    return S_OK;
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying ID3D12CommandQueue for interface: %s", guid.c_str());
  }

  return RefCounter12::QueryInterface(riid, ppvObject);
}

WrappedID3D12GraphicsCommandList::WrappedID3D12GraphicsCommandList(ID3D12GraphicsCommandList *real,
                                                                   WrappedID3D12Device *device,
                                                                   Serialiser *serialiser)
    : RefCounter12(real), m_pDevice(device), m_pList(real)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(
        this, sizeof(WrappedID3D12GraphicsCommandList));

  m_pList->QueryInterface(__uuidof(ID3D12DebugCommandList), (void **)&m_DummyDebug.m_pReal);

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = READING;
    m_pSerialiser = serialiser;
  }
  else
  {
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, true);
    m_State = WRITING_IDLE;

    m_pSerialiser->SetDebugText(true);
  }

  m_pSerialiser->SetUserData(m_pDevice->GetResourceManager());

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_ListRecord = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_ListRecord = m_pDevice->GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_ListRecord->DataInSerialiser = false;
    m_ListRecord->SpecialResource = true;
    m_ListRecord->Length = 0;
    m_ListRecord->NumSubResources = 0;
    m_ListRecord->SubResources = NULL;
    m_ListRecord->ignoreSerialise = true;
  }

  m_pDevice->SoftRef();
}

WrappedID3D12GraphicsCommandList::~WrappedID3D12GraphicsCommandList()
{
}

HRESULT STDMETHODCALLTYPE WrappedID3D12GraphicsCommandList::QueryInterface(REFIID riid,
                                                                           void **ppvObject)
{
  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D12GraphicsCommandList *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList))
  {
    *ppvObject = (ID3D12GraphicsCommandList *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12CommandList))
  {
    *ppvObject = (ID3D12CommandList *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12Pageable))
  {
    *ppvObject = (ID3D12Pageable *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12DeviceChild))
  {
    *ppvObject = (ID3D12DeviceChild *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12Object))
  {
    *ppvObject = (ID3D12DeviceChild *)this;
    AddRef();
    return S_OK;
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying ID3D12GraphicsCommandList for interface: %s", guid.c_str());
  }

  return RefCounter12::QueryInterface(riid, ppvObject);
}
