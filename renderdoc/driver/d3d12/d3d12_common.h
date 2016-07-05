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

#pragma once

#define INITGUID

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "driver/dx/official/d3d12.h"
#include "driver/dx/official/dxgi1_4.h"
#include "driver/shaders/dxbc/dxbc_compile.h"
#include "serialise/serialiser.h"

class WrappedID3D12Device;

class RefCounter12
{
private:
  IUnknown *m_pReal;
  unsigned int m_iRefcount;
  bool m_SelfDeleting;

protected:
  void SetSelfDeleting(bool selfDelete) { m_SelfDeleting = selfDelete; }
  // used for derived classes that need to soft ref but are handling their
  // own self-deletion
  static void AddDeviceSoftref(WrappedID3D12Device *device);
  static void ReleaseDeviceSoftref(WrappedID3D12Device *device);

public:
  RefCounter12(IUnknown *real, bool selfDelete = true)
      : m_pReal(real), m_iRefcount(1), m_SelfDeleting(selfDelete)
  {
  }
  virtual ~RefCounter12() {}
  unsigned int GetRefCount() { return m_iRefcount; }
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  ULONG STDMETHODCALLTYPE AddRef()
  {
    InterlockedIncrement(&m_iRefcount);
    return m_iRefcount;
  }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = InterlockedDecrement(&m_iRefcount);
    if(ret == 0 && m_SelfDeleting)
      delete this;
    return ret;
  }

  unsigned int SoftRef(WrappedID3D12Device *device);
  unsigned int SoftRelease(WrappedID3D12Device *device);
};

#define IMPLEMENT_IUNKNOWN_WITH_REFCOUNTER_CUSTOMQUERY                \
  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter12::AddRef(); } \
  ULONG STDMETHODCALLTYPE Release() { return RefCounter12::Release(); }
#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) \
  ret func;                                      \
  bool CONCAT(Serialise_, func);

#pragma region Chunks

enum D3D12ChunkType
{
  DEVICE_INIT = FIRST_CHUNK_ID,
  SET_RESOURCE_NAME,
  RELEASE_RESOURCE,
  CREATE_SWAP_BUFFER,

  CAPTURE_SCOPE,

  PUSH_EVENT,
  SET_MARKER,
  POP_EVENT,

  DEBUG_MESSAGES,

  CONTEXT_CAPTURE_HEADER,    // chunk at beginning of context's chunk stream
  CONTEXT_CAPTURE_FOOTER,    // chunk at end of context's chunk stream

  SET_SHADER_DEBUG_PATH,

  NUM_D3D12_CHUNKS,
};

#pragma endregion Chunks
