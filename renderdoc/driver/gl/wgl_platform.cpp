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

#include "driver/gl/gl_common.h"
#include "driver/gl/wgl_dispatch_table.h"

#define WINDOW_CLASS_NAME L"renderdocGLclass"

class WGLPlatform : public GLPlatform
{
  bool MakeContextCurrent(GLWindowingData data)
  {
    if(WGL.wglMakeCurrent)
      return WGL.wglMakeCurrent(data.DC, data.ctx) == TRUE;

    return false;
  }

  GLWindowingData CloneTemporaryContext(GLWindowingData share)
  {
    GLWindowingData ret = share;
    ret.ctx = NULL;

    if(!WGL.wglCreateContextAttribsARB)
      return ret;

    const int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB,
        3,
        WGL_CONTEXT_MINOR_VERSION_ARB,
        2,
        WGL_CONTEXT_FLAGS_ARB,
        0,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
        0,
    };

    ret.ctx = WGL.wglCreateContextAttribsARB(share.DC, share.ctx, attribs);

    return ret;
  }

  void DeleteClonedContext(GLWindowingData context)
  {
    if(context.ctx && WGL.wglDeleteContext)
      WGL.wglDeleteContext(context.ctx);
  }

  void DeleteReplayContext(GLWindowingData context)
  {
    if(WGL.wglMakeCurrent && WGL.wglDeleteContext)
    {
      WGL.wglMakeCurrent(NULL, NULL);
      WGL.wglDeleteContext(context.ctx);
      ::ReleaseDC(context.wnd, context.DC);
      ::DestroyWindow(context.wnd);
    }
  }

  void SwapBuffers(GLWindowingData context)
  {
    if(WGL.SwapBuffers)
      WGL.SwapBuffers(context.DC);
  }

  void WindowResized(GLWindowingData context) {}
  void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h)
  {
    RECT rect = {0};
    ::GetClientRect(context.wnd, &rect);
    w = rect.right - rect.left;
    h = rect.bottom - rect.top;
  }

  bool IsOutputWindowVisible(GLWindowingData context)
  {
    return (IsWindowVisible(context.wnd) == TRUE);
  }

  GLWindowingData MakeOutputWindow(WindowingData window, bool depth, GLWindowingData share_context)
  {
    GLWindowingData ret = {};

    if(!WGL.wglGetPixelFormatAttribivARB || !WGL.wglCreateContextAttribsARB)
      return ret;

    RDCASSERT(window.system == WindowingSystem::Win32 || window.system == WindowingSystem::Unknown ||
                  window.system == WindowingSystem::Headless,
              window.system);

    HWND w = NULL;

    if(window.system == WindowingSystem::Win32)
      w = window.win32.window;
    else
      w = CreateWindowEx(WS_EX_CLIENTEDGE, WINDOW_CLASS_NAME, L"", WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL,
                         GetModuleHandle(NULL), NULL);

    HDC DC = GetDC(w);

    PIXELFORMATDESCRIPTOR pfd = {0};

    int attrib = eWGL_NUMBER_PIXEL_FORMATS_ARB;
    int value = 1;

    WGL.wglGetPixelFormatAttribivARB(DC, 1, 0, 1, &attrib, &value);

    int pf = 0;

    int numpfs = value;
    for(int i = 1; i <= numpfs; i++)
    {
      // verify that we have the properties we want
      attrib = eWGL_DRAW_TO_WINDOW_ARB;
      WGL.wglGetPixelFormatAttribivARB(DC, i, 0, 1, &attrib, &value);
      if(value == 0)
        continue;

      attrib = eWGL_ACCELERATION_ARB;
      WGL.wglGetPixelFormatAttribivARB(DC, i, 0, 1, &attrib, &value);
      if(value == eWGL_NO_ACCELERATION_ARB)
        continue;

      attrib = eWGL_SUPPORT_OPENGL_ARB;
      WGL.wglGetPixelFormatAttribivARB(DC, i, 0, 1, &attrib, &value);
      if(value == 0)
        continue;

      attrib = eWGL_DOUBLE_BUFFER_ARB;
      WGL.wglGetPixelFormatAttribivARB(DC, i, 0, 1, &attrib, &value);
      if(value == 0)
        continue;

      attrib = eWGL_PIXEL_TYPE_ARB;
      WGL.wglGetPixelFormatAttribivARB(DC, i, 0, 1, &attrib, &value);
      if(value != eWGL_TYPE_RGBA_ARB)
        continue;

      // we have an opengl-capable accelerated RGBA context.
      // we use internal framebuffers to do almost all rendering, so we just need
      // RGB (color bits > 24) and SRGB buffer.

      attrib = eWGL_COLOR_BITS_ARB;
      WGL.wglGetPixelFormatAttribivARB(DC, i, 0, 1, &attrib, &value);
      if(value < 24)
        continue;

      attrib = WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
      WGL.wglGetPixelFormatAttribivARB(DC, i, 0, 1, &attrib, &value);
      if(value == 0)
        continue;

      // this one suits our needs, choose it
      pf = i;
      break;
    }

    if(pf == 0)
    {
      ReleaseDC(w, DC);
      RDCERR("Couldn't choose pixel format");
      return ret;
    }

    BOOL res = DescribePixelFormat(DC, pf, sizeof(pfd), &pfd);
    if(res == FALSE)
    {
      ReleaseDC(w, DC);
      RDCERR("Couldn't describe pixel format");
      return ret;
    }

    res = SetPixelFormat(DC, pf, &pfd);
    if(res == FALSE)
    {
      ReleaseDC(w, DC);
      RDCERR("Couldn't set pixel format");
      return ret;
    }

    int attribs[64] = {0};
    int i = 0;

    attribs[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
    attribs[i++] = GLCoreVersion / 10;
    attribs[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
    attribs[i++] = GLCoreVersion % 10;
    attribs[i++] = WGL_CONTEXT_FLAGS_ARB;
#if ENABLED(RDOC_DEVEL)
    attribs[i++] = WGL_CONTEXT_DEBUG_BIT_ARB;
#else
    attribs[i++] = 0;
#endif
    attribs[i++] = WGL_CONTEXT_PROFILE_MASK_ARB;
    attribs[i++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

    HGLRC rc = WGL.wglCreateContextAttribsARB(DC, share_context.ctx, attribs);
    if(rc == NULL)
    {
      ReleaseDC(w, DC);
      RDCERR("Couldn't create %d.%d context - something changed since creation", GLCoreVersion / 10,
             GLCoreVersion % 10);
      return ret;
    }

    ret.DC = DC;
    ret.ctx = rc;
    ret.wnd = w;

    return ret;
  }

  void *GetReplayFunction(const char *funcname)
  {
    // prefer wglGetProcAddress
    void *ret = WGL.wglGetProcAddress(funcname);
    if(ret)
      return ret;

    // but if it returns NULL, try looking up the dll directly
    return Process::GetFunctionAddress(Process::LoadModule("opengl32.dll"), funcname);
  }

  bool CanCreateGLESContext()
  {
    bool success = WGL.PopulateForReplay();

    // if we can't populate our functions we bail now.
    if(!success)
      return false;

    // we need to check for the presence of EXT_create_context_es2_profile.
    // Unfortunately on windows this means creating a trampoline context
    success = RegisterClass();

    if(!success)
      return false;

    HWND w = NULL;
    HDC dc = NULL;
    HGLRC rc = NULL;

    success = CreateTrampolineContext(w, dc, rc);

    if(!success)
      return false;

    const char *exts = NULL;

    if(WGL.wglGetExtensionsStringARB)
      exts = WGL.wglGetExtensionsStringARB(dc);

    if(!exts && WGL.wglGetExtensionsStringEXT)
      exts = WGL.wglGetExtensionsStringEXT();

    if(!exts)
      RDCERR("Couldn't get WGL extension string");

    bool ret = (exts && strstr(exts, "EXT_create_context_es2_profile") != NULL);

    WGL.wglMakeCurrent(NULL, NULL);
    WGL.wglDeleteContext(rc);
    ReleaseDC(w, dc);
    DestroyWindow(w);

    return ret;
  }

  bool PopulateForReplay() { return WGL.PopulateForReplay(); }
  ReplayStatus InitialiseAPI(GLWindowingData &replayContext, RDCDriver api)
  {
    RDCASSERT(api == RDCDriver::OpenGL || api == RDCDriver::OpenGLES);

    bool success = RegisterClass();

    if(!success)
      return ReplayStatus::APIInitFailed;

    HWND w = NULL;
    HDC dc = NULL;
    HGLRC rc = NULL;

    success = CreateTrampolineContext(w, dc, rc);

    if(!success)
      return ReplayStatus::APIInitFailed;

    if(!WGL.wglCreateContextAttribsARB || !WGL.wglGetPixelFormatAttribivARB)
    {
      WGL.wglMakeCurrent(NULL, NULL);
      WGL.wglDeleteContext(rc);
      ReleaseDC(w, dc);
      DestroyWindow(w);
      RDCERR("RenderDoc requires WGL_ARB_create_context and WGL_ARB_pixel_format");
      return ReplayStatus::APIHardwareUnsupported;
    }

    WGL.wglMakeCurrent(NULL, NULL);
    WGL.wglDeleteContext(rc);
    ReleaseDC(w, dc);
    DestroyWindow(w);

    // we don't use the default framebuffer (backbuffer) for anything, so we make it
    // tiny and with no depth/stencil bits
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;

    w = CreateWindowEx(WS_EX_CLIENTEDGE, WINDOW_CLASS_NAME, L"RenderDoc replay window",
                       WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 32, 32, NULL, NULL,
                       GetModuleHandle(NULL), NULL);

    dc = GetDC(w);

    int pf = ChoosePixelFormat(dc, &pfd);
    if(pf == 0)
    {
      RDCERR("Couldn't choose pixel format");
      ReleaseDC(w, dc);
      DestroyWindow(w);
      return ReplayStatus::APIInitFailed;
    }

    BOOL res = SetPixelFormat(dc, pf, &pfd);
    if(res == FALSE)
    {
      RDCERR("Couldn't set pixel format");
      ReleaseDC(w, dc);
      DestroyWindow(w);
      return ReplayStatus::APIInitFailed;
    }

    int attribs[64] = {0};
    int i = 0;

    attribs[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
    int &major = attribs[i];
    attribs[i++] = 0;
    attribs[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
    int &minor = attribs[i];
    attribs[i++] = 0;
    attribs[i++] = WGL_CONTEXT_FLAGS_ARB;
#if ENABLED(RDOC_DEVEL)
    attribs[i++] = WGL_CONTEXT_DEBUG_BIT_ARB;
#else
    attribs[i++] = 0;
#endif
    attribs[i++] = WGL_CONTEXT_PROFILE_MASK_ARB;
    attribs[i++] = api == RDCDriver::OpenGLES ? WGL_CONTEXT_ES2_PROFILE_BIT_EXT
                                              : WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

    rc = NULL;

    std::vector<GLVersion> versions = GetReplayVersions(api);

    for(GLVersion v : versions)
    {
      major = v.major;
      minor = v.minor;
      rc = WGL.wglCreateContextAttribsARB(dc, NULL, attribs);

      if(rc)
        break;
    }

    if(rc == NULL)
    {
      RDCERR("Couldn't create at least 3.2 context - RenderDoc requires OpenGL 3.2 availability");
      ReleaseDC(w, dc);
      DestroyWindow(w);
      return ReplayStatus::APIHardwareUnsupported;
    }

    GLCoreVersion = major * 10 + minor;

    res = WGL.wglMakeCurrent(dc, rc);
    if(res == FALSE)
    {
      RDCERR("Couldn't make 3.2 RC current");
      WGL.wglMakeCurrent(NULL, NULL);
      WGL.wglDeleteContext(rc);
      ReleaseDC(w, dc);
      DestroyWindow(w);
      return ReplayStatus::APIInitFailed;
    }

    replayContext.DC = dc;
    replayContext.ctx = rc;
    replayContext.wnd = w;

    return ReplayStatus::Succeeded;
  }

  bool CreateTrampolineContext(HWND &w, HDC &dc, HGLRC &rc)
  {
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;

    w = CreateWindowEx(WS_EX_CLIENTEDGE, WINDOW_CLASS_NAME, L"", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL,
                       GetModuleHandle(NULL), NULL);

    dc = GetDC(w);

    int pf = ::ChoosePixelFormat(dc, &pfd);
    if(pf == 0)
    {
      ReleaseDC(w, dc);
      DestroyWindow(w);
      RDCERR("Couldn't choose pixel format");
      return false;
    }

    BOOL res = ::SetPixelFormat(dc, pf, &pfd);
    if(res == FALSE)
    {
      ReleaseDC(w, dc);
      DestroyWindow(w);
      RDCERR("Couldn't set pixel format");
      return false;
    }

    rc = WGL.wglCreateContext(dc);
    if(rc == NULL)
    {
      ReleaseDC(w, dc);
      DestroyWindow(w);
      RDCERR("Couldn't create trampoline context");
      return false;
    }

    res = WGL.wglMakeCurrent(dc, rc);
    if(res == FALSE)
    {
      WGL.wglMakeCurrent(NULL, NULL);
      WGL.wglDeleteContext(rc);
      ReleaseDC(w, dc);
      DestroyWindow(w);
      RDCERR("Couldn't make trampoline context current");
      return false;
    }

    // now we can fetch the extension functions we need and fill out WGL
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

    return true;
  }

  bool RegisterClass()
  {
    WNDCLASSEX wc = {};
    wc.style = CS_OWNDC;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    WNDCLASSEX dummyCheck = {};

    // if the class isn't already registered, then register it.
    if(!GetClassInfoExW(wc.hInstance, wc.lpszClassName, &dummyCheck))
    {
      if(!RegisterClassEx(&wc))
      {
        RDCERR("Couldn't register GL window class");
        return false;
      }
    }

    return true;
  }

  void DrawQuads(float width, float height, const std::vector<Vec4f> &vertices)
  {
    ::DrawQuads(WGL, width, height, vertices);
  }
} wglPlatform;

GLPlatform &GetGLPlatform()
{
  return wglPlatform;
}

WGLDispatchTable WGL = {};

bool WGLDispatchTable::PopulateForReplay()
{
  RDCASSERT(RenderDoc::Inst().IsReplayApp());

  RDCDEBUG("Initialising WGL function pointers");

  bool symbols_ok = true;

// library will be NULL for extension functions that we can't fetch yet.
#define LOAD_FUNC(library, func)                                                                 \
  if(CheckConstParam(sizeof(library) > 2))                                                       \
  {                                                                                              \
    if(!this->func)                                                                              \
      this->func = (CONCAT(PFN_, func))Process::GetFunctionAddress(Process::LoadModule(library), \
                                                                   STRINGIZE(func));             \
                                                                                                 \
    if(!this->func)                                                                              \
    {                                                                                            \
      symbols_ok = false;                                                                        \
      RDCWARN("Unable to load '%s'", STRINGIZE(func));                                           \
    }                                                                                            \
  }

  WGL_HOOKED_SYMBOLS(LOAD_FUNC);
  WGL_NONHOOKED_SYMBOLS(LOAD_FUNC);

#undef LOAD_FUNC
  return symbols_ok;
}
