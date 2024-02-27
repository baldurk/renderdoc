/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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
#include "d3d12_command_queue.h"
#include "d3d12_resources.h"

HRESULT WrappedID3D12Device::CreateShaderCacheSession(_In_ const D3D12_SHADER_CACHE_SESSION_DESC *pDesc,
                                                      REFIID riid, _COM_Outptr_opt_ void **ppvSession)
{
  if(ppvSession == NULL)
    return m_pDevice9->CreateShaderCacheSession(pDesc, riid, NULL);

  if(riid != __uuidof(ID3D12ShaderCacheSession))
    return E_NOINTERFACE;

  ID3D12ShaderCacheSession *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice9->CreateShaderCacheSession(
                          pDesc, __uuidof(ID3D12ShaderCacheSession), (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12ShaderCacheSession *wrapped = new WrappedID3D12ShaderCacheSession(real, this);

    if(riid == __uuidof(ID3D12ShaderCacheSession))
      *ppvSession = (ID3D12ShaderCacheSession *)wrapped;
  }

  return ret;
}

HRESULT WrappedID3D12Device::ShaderCacheControl(D3D12_SHADER_CACHE_KIND_FLAGS Kinds,
                                                D3D12_SHADER_CACHE_CONTROL_FLAGS Control)
{
  return m_pDevice9->ShaderCacheControl(Kinds, Control);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommandQueue1(SerialiserType &ser,
                                                        const D3D12_COMMAND_QUEUE_DESC *pDesc,
                                                        REFIID CreatorID, REFIID riid,
                                                        void **ppCommandQueue)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(creator, CreatorID).Named("CreatorID"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pCommandQueue,
                          ((WrappedID3D12CommandQueue *)*ppCommandQueue)->GetResourceID())
      .TypedAs("ID3D12CommandQueue *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12CommandQueue *ret = NULL;
    HRESULT hr = E_NOINTERFACE;
    if(m_pDevice9)
    {
      hr = m_pDevice9->CreateCommandQueue1(&Descriptor, creator, guid, (void **)&ret);
    }
    else
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12Device9 which isn't available");
      return false;
    }

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

HRESULT WrappedID3D12Device::CreateCommandQueue1(const D3D12_COMMAND_QUEUE_DESC *pDesc,
                                                 REFIID CreatorID, REFIID riid, void **ppCommandQueue)
{
  if(ppCommandQueue == NULL)
    return m_pDevice9->CreateCommandQueue1(pDesc, CreatorID, riid, NULL);

  if(riid != __uuidof(ID3D12CommandQueue))
    return E_NOINTERFACE;

  ID3D12CommandQueue *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice9->CreateCommandQueue1(pDesc, CreatorID, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12CommandQueue *wrapped = new WrappedID3D12CommandQueue(real, this, m_State);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateCommandQueue1);
      Serialise_CreateCommandQueue1(ser, pDesc, CreatorID, riid, (void **)&wrapped);

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

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommandQueue1,
                                const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID CreatorID,
                                REFIID riid, void **ppCommandQueue);
