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

#pragma once

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "driver/dx/official/d3d9.h"

class WrappedD3DDevice9;

class RefCounter9
{
private:
  IUnknown *m_pReal;
  unsigned int m_iRefcount;
  bool m_SelfDeleting;

protected:
  void SetSelfDeleting(bool selfDelete) { m_SelfDeleting = selfDelete; }
  // used for derived classes that need to soft ref but are handling their
  // own self-deletion
  static void AddDeviceSoftref(WrappedD3DDevice9 *device);
  static void ReleaseDeviceSoftref(WrappedD3DDevice9 *device);

public:
  RefCounter9(IUnknown *real, bool selfDelete = true)
      : m_pReal(real), m_iRefcount(1), m_SelfDeleting(selfDelete)
  {
  }
  virtual ~RefCounter9() {}
  unsigned int GetRefCount() { return m_iRefcount; }
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(
      /* [in] */ REFIID riid,
      /* [annotation][iid_is][out] */
      __RPC__deref_out void **ppvObject)
  {
    return E_NOINTERFACE;
  }

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

  unsigned int SoftRef(WrappedD3DDevice9 *device);
  unsigned int SoftRelease(WrappedD3DDevice9 *device);
};
