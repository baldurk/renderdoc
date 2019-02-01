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

#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_resources.h"

/////////////////////////////////
// implement ID3D11DeviceContext3

void WrappedID3D11DeviceContext::Flush1(D3D11_CONTEXT_TYPE ContextType, HANDLE hEvent)
{
  if(m_pRealContext3 == NULL)
    return;

  m_pRealContext3->Flush1(ContextType, hEvent);
}

void WrappedID3D11DeviceContext::SetHardwareProtectionState(BOOL HwProtectionEnable)
{
  if(m_pRealContext3 == NULL)
    return;

  m_pRealContext3->SetHardwareProtectionState(HwProtectionEnable);
}

void WrappedID3D11DeviceContext::GetHardwareProtectionState(BOOL *pHwProtectionEnable)
{
  if(m_pRealContext3 == NULL)
    return;

  m_pRealContext3->GetHardwareProtectionState(pHwProtectionEnable);
}

/////////////////////////////////
// implement ID3D11DeviceContext4

HRESULT WrappedID3D11DeviceContext::Signal(ID3D11Fence *pFence, UINT64 Value)
{
  if(m_pRealContext4 == NULL)
    return E_NOINTERFACE;

  return m_pRealContext4->Signal(UNWRAP(WrappedID3D11Fence, pFence), Value);
}

HRESULT WrappedID3D11DeviceContext::Wait(ID3D11Fence *pFence, UINT64 Value)
{
  if(m_pRealContext4 == NULL)
    return E_NOINTERFACE;

  return m_pRealContext4->Wait(UNWRAP(WrappedID3D11Fence, pFence), Value);
}