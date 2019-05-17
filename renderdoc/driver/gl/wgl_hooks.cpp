/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "driver/gl/gl_driver.h"
#include "driver/gl/wgl_dispatch_table.h"
#include "hooks/hooks.h"

class WGLHook : LibraryHook
{
public:
  WGLHook() : driver(GetGLPlatform()) {}
  void RegisterHooks();

  WrappedOpenGL driver;

  // prevent recursion in nested calls, e.g. if SwapBuffers() calls wglSwapBuffers or
  // wglCreateLayerContext calls wglCreateContext
  bool swapRecurse = false;
  bool createRecurse = false;

  // when we have loaded EGL try to completely disable all WGL hooks, to avoid clashing with EGL
  // when fetching dispatch tables or hooking.
  bool eglDisabled = false;

  // we use this to check if we've seen a context be created. If we HAVEN'T then RenderDoc was
  // probably injected after the start of the application so we should not call our hooked functions
  // - things will go wrong like missing context data, references to resources we don't know about
  // and hooked functions via wglGetProcAddress being NULL and never being called by the app.
  bool haveContextCreation = false;

  std::set<HGLRC> contexts;

  void RefreshWindowParameters(const GLWindowingData &data);
  void ProcessSwapBuffers(HDC dc);
  void PopulateFromContext(HDC dc, HGLRC rc);
  GLInitParams GetInitParamsForDC(HDC dc);
} wglhook;

void DisableWGLHooksForEGL()
{
  RDCLOG("Disabling WGL hooks for EGL");
  wglhook.eglDisabled = true;
}

void WGLHook::PopulateFromContext(HDC dc, HGLRC rc)
{
  SetDriverForHooks(&driver);
  EnableGLHooks();

  // called from wglCreate*Context*, to populate GL functions as soon as possible by making a new
  // context current and fetching our function pointers
  {
    // get the current DC/context
    HDC prevDC = WGL.wglGetCurrentDC();
    HGLRC prevContext = WGL.wglGetCurrentContext();

    // activate the given context
    WGL.wglMakeCurrent(dc, rc);

    // fill out all functions that we need to get from wglGetProcAddress
    if(!WGL.wglCreateContextAttribsARB)
      WGL.wglCreateContextAttribsARB =
          (PFN_wglCreateContextAttribsARB)WGL.wglGetProcAddress("wglCreateContextAttribsARB");

    if(!WGL.wglGetPixelFormatAttribivARB)
      WGL.wglGetPixelFormatAttribivARB =
          (PFN_wglGetPixelFormatAttribivARB)WGL.wglGetProcAddress("wglGetPixelFormatAttribivARB");

    if(!WGL.wglGetExtensionsStringEXT)
      WGL.wglGetExtensionsStringEXT =
          (PFN_wglGetExtensionsStringEXT)WGL.wglGetProcAddress("wglGetExtensionsStringEXT");

    if(!WGL.wglGetExtensionsStringARB)
      WGL.wglGetExtensionsStringARB =
          (PFN_wglGetExtensionsStringARB)WGL.wglGetProcAddress("wglGetExtensionsStringARB");

    GL.PopulateWithCallback([](const char *funcName) {
      ScopedSuppressHooking suppress;
      return (void *)WGL.wglGetProcAddress(funcName);
    });

    // restore DC/context
    WGL.wglMakeCurrent(prevDC, prevContext);
  }
}

GLInitParams WGLHook::GetInitParamsForDC(HDC dc)
{
  GLInitParams ret;

  int pf = GetPixelFormat(dc);

  PIXELFORMATDESCRIPTOR pfd;
  DescribePixelFormat(dc, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

  HWND w = WindowFromDC(dc);

  RECT r;
  GetClientRect(w, &r);

  ret.colorBits = pfd.cColorBits;
  ret.depthBits = pfd.cDepthBits;
  ret.stencilBits = pfd.cStencilBits;
  ret.width = (r.right - r.left);
  ret.height = (r.bottom - r.top);

  ret.isSRGB = true;

  if(WGL.wglGetPixelFormatAttribivARB)
  {
    int attrname = eWGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
    int srgb = 1;
    WGL.wglGetPixelFormatAttribivARB(dc, pf, 0, 1, &attrname, &srgb);
    ret.isSRGB = srgb;

    attrname = eWGL_SAMPLES_ARB;
    int ms = 1;
    WGL.wglGetPixelFormatAttribivARB(dc, pf, 0, 1, &attrname, &ms);
    ret.multiSamples = RDCMAX(1, ms);
  }

  if(pfd.iPixelType != PFD_TYPE_RGBA)
  {
    RDCERR("Unsupported OpenGL pixel type");
  }

  return ret;
}

void WGLHook::RefreshWindowParameters(const GLWindowingData &data)
{
  if(haveContextCreation && data.ctx && data.wnd)
  {
    RECT r;
    GetClientRect(data.wnd, &r);

    GLInitParams &params = driver.GetInitParams(data);
    params.width = r.right - r.left;
    params.height = r.bottom - r.top;
  }
}

void WGLHook::ProcessSwapBuffers(HDC dc)
{
  if(eglDisabled)
    return;

  HWND w = WindowFromDC(dc);

  SetDriverForHooks(&wglhook.driver);

  if(w != NULL && haveContextCreation && !swapRecurse)
  {
    GLWindowingData data;
    data.DC = dc;
    data.wnd = w;
    data.ctx = WGL.wglGetCurrentContext();

    RefreshWindowParameters(data);

    {
      SCOPED_LOCK(glLock);
      driver.SwapBuffers(w);
    }

    SetLastError(0);
  }
}

static HGLRC WINAPI wglCreateContext_hooked(HDC dc)
{
  if(wglhook.createRecurse || wglhook.eglDisabled)
    return WGL.wglCreateContext(dc);

  wglhook.createRecurse = true;

  HGLRC ret = WGL.wglCreateContext(dc);

  if(ret)
  {
    DWORD err = GetLastError();

    wglhook.PopulateFromContext(dc, ret);

    GLWindowingData data;
    data.DC = dc;
    data.wnd = WindowFromDC(dc);
    data.ctx = ret;

    wglhook.driver.CreateContext(data, NULL, wglhook.GetInitParamsForDC(dc), false, false);

    wglhook.haveContextCreation = true;

    SetLastError(err);
  }

  wglhook.createRecurse = false;

  return ret;
}

static BOOL WINAPI wglDeleteContext_hooked(HGLRC rc)
{
  if(wglhook.haveContextCreation && !wglhook.eglDisabled)
  {
    SCOPED_LOCK(glLock);
    wglhook.driver.DeleteContext(rc);
    wglhook.contexts.erase(rc);
  }

  SetLastError(0);

  return WGL.wglDeleteContext(rc);
}

static HGLRC WINAPI wglCreateLayerContext_hooked(HDC dc, int iLayerPlane)
{
  if(wglhook.createRecurse || wglhook.eglDisabled)
    return WGL.wglCreateLayerContext(dc, iLayerPlane);

  wglhook.createRecurse = true;

  HGLRC ret = WGL.wglCreateLayerContext(dc, iLayerPlane);

  if(ret)
  {
    DWORD err = GetLastError();

    wglhook.PopulateFromContext(dc, ret);

    wglhook.createRecurse = true;

    GLWindowingData data;
    data.DC = dc;
    data.wnd = WindowFromDC(dc);
    data.ctx = ret;

    wglhook.driver.CreateContext(data, NULL, wglhook.GetInitParamsForDC(dc), false, false);

    wglhook.haveContextCreation = true;

    SetLastError(err);
  }

  wglhook.createRecurse = false;

  return ret;
}

static HGLRC WINAPI wglCreateContextAttribsARB_hooked(HDC dc, HGLRC hShareContext,
                                                      const int *attribList)
{
  // don't recurse
  if(wglhook.createRecurse || wglhook.eglDisabled)
    return WGL.wglCreateContextAttribsARB(dc, hShareContext, attribList);

  wglhook.createRecurse = true;

  int defaultAttribList[] = {0};

  const int *attribs = attribList ? attribList : defaultAttribList;
  std::vector<int> attribVec;

  // modify attribs to our liking
  {
    bool flagsFound = false;
    const int *a = attribs;
    while(*a)
    {
      int name = *a++;
      int val = *a++;

      if(name == WGL_CONTEXT_FLAGS_ARB)
      {
        if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
          val |= WGL_CONTEXT_DEBUG_BIT_ARB;
        else
          val &= ~WGL_CONTEXT_DEBUG_BIT_ARB;

        // remove NO_ERROR bit
        val &= ~GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR;

        flagsFound = true;
      }

      attribVec.push_back(name);
      attribVec.push_back(val);
    }

    if(!flagsFound && RenderDoc::Inst().GetCaptureOptions().apiValidation)
    {
      attribVec.push_back(WGL_CONTEXT_FLAGS_ARB);
      attribVec.push_back(WGL_CONTEXT_DEBUG_BIT_ARB);
    }

    attribVec.push_back(0);

    attribs = &attribVec[0];
  }

  RDCDEBUG("wglCreateContextAttribsARB:");

  bool core = false, es = false;

  int *a = (int *)attribs;
  while(*a)
  {
    RDCDEBUG("%x: %d", a[0], a[1]);

    if(a[0] == WGL_CONTEXT_PROFILE_MASK_ARB)
    {
      core = (a[1] & WGL_CONTEXT_CORE_PROFILE_BIT_ARB) != 0;
      es = (a[1] & (WGL_CONTEXT_ES_PROFILE_BIT_EXT | WGL_CONTEXT_ES2_PROFILE_BIT_EXT)) != 0;
    }

    a += 2;
  }

  if(es)
  {
    wglhook.driver.SetDriverType(RDCDriver::OpenGLES);
    core = true;
  }

  SetLastError(0);

  HGLRC ret = WGL.wglCreateContextAttribsARB(dc, hShareContext, attribs);

  DWORD err = GetLastError();

  // don't continue if creation failed
  if(ret == NULL)
  {
    wglhook.createRecurse = false;
    return ret;
  }

  wglhook.PopulateFromContext(dc, ret);

  GLWindowingData data;
  data.DC = dc;
  data.wnd = WindowFromDC(dc);
  data.ctx = ret;

  wglhook.driver.CreateContext(data, hShareContext, wglhook.GetInitParamsForDC(dc), core, true);

  wglhook.haveContextCreation = true;

  SetLastError(err);

  wglhook.createRecurse = false;

  return ret;
}

static BOOL WINAPI wglShareLists_hooked(HGLRC oldContext, HGLRC newContext)
{
  bool ret = WGL.wglShareLists(oldContext, newContext) == TRUE;

  DWORD err = GetLastError();

  if(ret && !wglhook.eglDisabled)
  {
    SCOPED_LOCK(glLock);

    ret &= wglhook.driver.ForceSharedObjects(oldContext, newContext);
  }

  SetLastError(err);

  return ret ? TRUE : FALSE;
}

static BOOL WINAPI wglMakeCurrent_hooked(HDC dc, HGLRC rc)
{
  BOOL ret = WGL.wglMakeCurrent(dc, rc);

  DWORD err = GetLastError();

  if(ret && !wglhook.eglDisabled)
  {
    SCOPED_LOCK(glLock);

    SetDriverForHooks(&wglhook.driver);

    if(rc && wglhook.contexts.find(rc) == wglhook.contexts.end())
    {
      wglhook.contexts.insert(rc);

      FetchEnabledExtensions();

      // see gl_emulated.cpp
      GL.EmulateUnsupportedFunctions();
      GL.EmulateRequiredExtensions();
      GL.DriverForEmulation(&wglhook.driver);
    }

    GLWindowingData data;
    data.DC = dc;
    data.wnd = WindowFromDC(dc);
    data.ctx = rc;

    wglhook.RefreshWindowParameters(data);

    if(wglhook.haveContextCreation)
      wglhook.driver.ActivateContext(data);
  }

  SetLastError(err);

  return ret;
}

static BOOL WINAPI SwapBuffers_hooked(HDC dc)
{
  SCOPED_LOCK(glLock);

  wglhook.ProcessSwapBuffers(dc);

  wglhook.swapRecurse = true;
  BOOL ret = WGL.SwapBuffers(dc);
  wglhook.swapRecurse = false;

  return ret;
}

static BOOL WINAPI wglSwapBuffers_hooked(HDC dc)
{
  SCOPED_LOCK(glLock);

  wglhook.ProcessSwapBuffers(dc);

  wglhook.swapRecurse = true;
  BOOL ret = WGL.wglSwapBuffers(dc);
  wglhook.swapRecurse = false;

  return ret;
}

static BOOL WINAPI wglSwapLayerBuffers_hooked(HDC dc, UINT planes)
{
  SCOPED_LOCK(glLock);

  wglhook.ProcessSwapBuffers(dc);

  wglhook.swapRecurse = true;
  BOOL ret = WGL.wglSwapLayerBuffers(dc, planes);
  wglhook.swapRecurse = false;

  return ret;
}

static BOOL WINAPI wglSwapMultipleBuffers_hooked(UINT numSwaps, CONST WGLSWAP *pSwaps)
{
  for(UINT i = 0; pSwaps && i < numSwaps; i++)
    wglhook.ProcessSwapBuffers(pSwaps[i].hdc);

  wglhook.swapRecurse = true;
  BOOL ret = WGL.wglSwapMultipleBuffers(numSwaps, pSwaps);
  wglhook.swapRecurse = false;

  return ret;
}

static LONG WINAPI ChangeDisplaySettingsA_hooked(DEVMODEA *mode, DWORD flags)
{
  if((flags & CDS_FULLSCREEN) == 0 || wglhook.eglDisabled ||
     RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
    return WGL.ChangeDisplaySettingsA(mode, flags);

  return DISP_CHANGE_SUCCESSFUL;
}

static LONG WINAPI ChangeDisplaySettingsW_hooked(DEVMODEW *mode, DWORD flags)
{
  if((flags & CDS_FULLSCREEN) == 0 || wglhook.eglDisabled ||
     RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
    return WGL.ChangeDisplaySettingsW(mode, flags);

  return DISP_CHANGE_SUCCESSFUL;
}

static LONG WINAPI ChangeDisplaySettingsExA_hooked(LPCSTR devname, DEVMODEA *mode, HWND wnd,
                                                   DWORD flags, LPVOID param)
{
  if((flags & CDS_FULLSCREEN) == 0 || wglhook.eglDisabled ||
     RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
    return WGL.ChangeDisplaySettingsExA(devname, mode, wnd, flags, param);

  return DISP_CHANGE_SUCCESSFUL;
}

static LONG WINAPI ChangeDisplaySettingsExW_hooked(LPCWSTR devname, DEVMODEW *mode, HWND wnd,
                                                   DWORD flags, LPVOID param)
{
  if((flags & CDS_FULLSCREEN) == 0 || wglhook.eglDisabled ||
     RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
    return WGL.ChangeDisplaySettingsExW(devname, mode, wnd, flags, param);

  return DISP_CHANGE_SUCCESSFUL;
}

static PROC WINAPI wglGetProcAddress_hooked(const char *func)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!WGL.wglGetProcAddress)
      WGL.PopulateForReplay();

    return WGL.wglGetProcAddress(func);
  }

  PROC realFunc = NULL;
  {
    ScopedSuppressHooking suppress;
    realFunc = WGL.wglGetProcAddress(func);
  }

  if(wglhook.eglDisabled)
    return realFunc;

  // if the real context doesn't support this function, and we don't provide an implementation fully
  // ourselves, return NULL
  if(realFunc == NULL && !FullyImplementedFunction(func))
    return realFunc;

  if(!strcmp(func, "wglCreateContext"))
    return (PROC)&wglCreateContext_hooked;
  if(!strcmp(func, "wglDeleteContext"))
    return (PROC)&wglDeleteContext_hooked;
  if(!strcmp(func, "wglCreateLayerContext"))
    return (PROC)&wglCreateLayerContext_hooked;
  if(!strcmp(func, "wglCreateContextAttribsARB"))
    return (PROC)&wglCreateContextAttribsARB_hooked;
  if(!strcmp(func, "wglMakeCurrent"))
    return (PROC)&wglMakeCurrent_hooked;
  if(!strcmp(func, "wglSwapBuffers"))
    return (PROC)&wglSwapBuffers_hooked;
  if(!strcmp(func, "wglSwapLayerBuffers"))
    return (PROC)&wglSwapLayerBuffers_hooked;
  if(!strcmp(func, "wglSwapMultipleBuffers"))
    return (PROC)&wglSwapMultipleBuffers_hooked;
  if(!strcmp(func, "wglGetProcAddress"))
    return (PROC)&wglGetProcAddress_hooked;

  // assume wgl functions are safe to just pass straight through, but don't pass through the wgl DX
  // interop functions
  if(!strncmp(func, "wgl", 3) && strncmp(func, "wglDX", 5))
    return realFunc;

  // otherwise, consult our database of hooks
  return (PROC)HookedGetProcAddress(func, (void *)realFunc);
}

static void WGLHooked(void *handle)
{
  RDCDEBUG("WGL library hooked");

  // as a hook callback this is only called while capturing
  RDCASSERT(!RenderDoc::Inst().IsReplayApp());

// fetch non-hooked functions into our dispatch table
#define WGL_FETCH(library, func) \
  WGL.func = (CONCAT(PFN_, func))Process::GetFunctionAddress(handle, STRINGIZE(func));
  WGL_NONHOOKED_SYMBOLS(WGL_FETCH)
#undef WGL_FETCH

  // maybe in future we could create a dummy context here and populate the GL hooks already?
}

void WGLHook::RegisterHooks()
{
  RDCLOG("Registering WGL hooks");

  // we load GL here to ensure that it is loaded by the time that we end hook registration and apply
  // any callbacks. That ensures that it doesn't get loaded later e.g. while we're in the middle of
  // loading libEGL, and break due to recursive calls.
  LoadLibraryA("opengl32.dll");

  LibraryHooks::RegisterLibraryHook("opengl32.dll", &WGLHooked);
  LibraryHooks::RegisterLibraryHook("gdi32.dll", NULL);
  LibraryHooks::RegisterLibraryHook("user32.dll", NULL);

// register EGL hooks
#define WGL_REGISTER(library, func)                                                               \
  if(CheckConstParam(sizeof(library) > 2))                                                        \
  {                                                                                               \
    LibraryHooks::RegisterFunctionHook(library, FunctionHook(STRINGIZE(func), (void **)&WGL.func, \
                                                             (void *)&CONCAT(func, _hooked)));    \
  }
  WGL_HOOKED_SYMBOLS(WGL_REGISTER)
#undef WGL_REGISTER
}
