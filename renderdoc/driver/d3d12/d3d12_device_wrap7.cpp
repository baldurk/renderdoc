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
#include "d3d12_resources.h"

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_AddToStateObject(SerialiserType &ser,
                                                     const D3D12_STATE_OBJECT_DESC *pAddition,
                                                     ID3D12StateObject *pStateObjectToGrowFrom,
                                                     REFIID riid,
                                                     _COM_Outptr_ void **ppNewStateObject)
{
  // AMD TODO - //Serialize Members

  if(IsReplayingAndReading())
  {
    // AMD TODO
    // Handle reading, and replaying
  }

  return false;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12Device::AddToStateObject(
    const D3D12_STATE_OBJECT_DESC *pAddition, ID3D12StateObject *pStateObjectToGrowFrom,
    REFIID riid, _COM_Outptr_ void **ppNewStateObject)
{
  // TODO AMD
  RDCERR("AddToStateObject called but raytracing is not supported!");
  return E_INVALIDARG;
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
