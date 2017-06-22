/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "driver/dx/official/d3d8.h"
#include "hooks/hooks.h"
#include "d3d8_device.h"

#define DLL_NAME "d3d8.dll"

typedef IDirect3D8 *(WINAPI *PFN_D3D8_CREATE)(UINT);

class D3D8Hook : LibraryHook
{
public:
  D3D8Hook()
  {
    LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);
    m_HasHooks = false;
    m_EnabledHooks = true;
  }

  bool CreateHooks(const char *libName)
  {
    bool success = true;

    success &= Create8.Initialize("Direct3DCreate8", DLL_NAME, Create8_hook);

    if(!success)
      return false;

    m_HasHooks = true;
    m_EnabledHooks = true;

    return true;
  }

  void EnableHooks(const char *libName, bool enable) { m_EnabledHooks = enable; }
  void OptionsUpdated(const char *libName) {}
private:
  static D3D8Hook d3d8hooks;

  bool m_HasHooks;
  bool m_EnabledHooks;

  Hook<PFN_D3D8_CREATE> Create8;

  static IDirect3D8 *WINAPI Create8_hook(UINT SDKVersion)
  {
    RDCLOG("App creating d3d8 %x", SDKVersion);

    IDirect3D8 *realD3D = d3d8hooks.Create8()(SDKVersion);

    return new WrappedD3D8(realD3D);
  }
};

D3D8Hook D3D8Hook::d3d8hooks;
