/******************************************************************************
 * The MIT License (MIT)
 *
 * * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "driver/d3d11/d3d11_device.h"
#include "hooks/hooks.h"

#include "driver/dx/official/d3d9.h"
#include "d3d9_device.h"

#define DLL_NAME "d3d9.dll"

typedef int(WINAPI *PFN_BEGIN_EVENT)(DWORD, WCHAR *);
typedef int(WINAPI *PFN_END_EVENT)();
typedef int(WINAPI *PFN_SET_MARKER_EVENT)(DWORD, WCHAR *);
typedef void(WINAPI *PFN_SET_OPTIONS)(DWORD);
typedef DWORD(WINAPI *PFN_GET_OPTIONS)();

typedef IDirect3D9 *(WINAPI *PFN_D3D9_CREATE)(UINT);

class D3D9Hook : LibraryHook
{
public:
  D3D9Hook()
  {
    LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);
    m_HasHooks = false;
    m_EnabledHooks = true;
  }

  bool CreateHooks(const char *libName)
  {
    bool success = true;

    success &= PERF_BeginEvent.Initialize("D3DPERF_BeginEvent", DLL_NAME, PERF_BeginEvent_hook);
    success &= PERF_EndEvent.Initialize("D3DPERF_EndEvent", DLL_NAME, PERF_EndEvent_hook);
    success &= PERF_SetMarker.Initialize("D3DPERF_SetMarker", DLL_NAME, PERF_SetMarker_hook);
    success &= PERF_SetOptions.Initialize("D3DPERF_SetOptions", DLL_NAME, PERF_SetOptions_hook);
    success &= PERF_GetStatus.Initialize("D3DPERF_GetStatus", DLL_NAME, PERF_GetStatus_hook);

    success &= Create9.Initialize("Direct3DCreate9", DLL_NAME, Create9_hook);

    if(!success)
      return false;

    m_HasHooks = true;
    m_EnabledHooks = true;

    return true;
  }

  void EnableHooks(const char *libName, bool enable) { m_EnabledHooks = enable; }
  void OptionsUpdated(const char *libName) {}
private:
  static D3D9Hook d3d9hooks;

  bool m_HasHooks;
  bool m_EnabledHooks;

  // D3DPERF api
  Hook<PFN_BEGIN_EVENT> PERF_BeginEvent;
  Hook<PFN_END_EVENT> PERF_EndEvent;
  Hook<PFN_SET_MARKER_EVENT> PERF_SetMarker;
  Hook<PFN_SET_OPTIONS> PERF_SetOptions;
  Hook<PFN_GET_OPTIONS> PERF_GetStatus;
  Hook<PFN_D3D9_CREATE> Create9;

  static int WINAPI PERF_BeginEvent_hook(DWORD col, WCHAR *wszName)
  {
    int ret = WrappedID3D11Device::BeginEvent((uint32_t)col, wszName);

    d3d9hooks.PERF_BeginEvent()(col, wszName);

    return ret;
  }

  static int WINAPI PERF_EndEvent_hook()
  {
    int ret = WrappedID3D11Device::EndEvent();

    d3d9hooks.PERF_EndEvent()();

    return ret;
  }

  static void WINAPI PERF_SetMarker_hook(DWORD col, WCHAR *wszName)
  {
    WrappedID3D11Device::SetMarker((uint32_t)col, wszName);

    d3d9hooks.PERF_SetMarker()(col, wszName);
  }

  static void WINAPI PERF_SetOptions_hook(DWORD dwOptions)
  {
    if(dwOptions & 1)
    {
      RDCDEBUG("Application requested not to be hooked.");
      LibraryHooks::GetInstance().EnableHooks(false);
    }
    else
    {
      LibraryHooks::GetInstance().EnableHooks(true);
    }
  }

  static DWORD WINAPI PERF_GetStatus_hook()
  {
    if(d3d9hooks.m_HasHooks && d3d9hooks.m_EnabledHooks)
    {
      return 1;
    }

    return 0;
  }

  static IDirect3D9 *WINAPI Create9_hook(UINT SDKVersion)
  {
    RDCLOG("App creating d3d9 %x", SDKVersion);

    IDirect3D9 *realD3D = d3d9hooks.Create9()(SDKVersion);

    return new WrappedD3D9(realD3D);
  }
};

D3D9Hook D3D9Hook::d3d9hooks;
