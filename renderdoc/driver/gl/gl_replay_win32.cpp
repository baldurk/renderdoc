/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "gl_replay.h"
#include "gl_driver.h"
#include "gl_resources.h"

PFNWGLCREATECONTEXTATTRIBSARBPROC createContextAttribs = NULL;
PFNWGLGETPIXELFORMATATTRIBIVARBPROC getPixelFormatAttrib = NULL;

typedef PROC(WINAPI *WGLGETPROCADDRESSPROC)(const char *);
typedef HGLRC(WINAPI *WGLCREATECONTEXTPROC)(HDC);
typedef BOOL(WINAPI *WGLMAKECURRENTPROC)(HDC, HGLRC);
typedef BOOL(WINAPI *WGLDELETECONTEXTPROC)(HGLRC);

WGLGETPROCADDRESSPROC wglGetProc = NULL;
WGLCREATECONTEXTPROC wglCreateRC = NULL;
WGLMAKECURRENTPROC wglMakeCurrentProc = NULL;
WGLDELETECONTEXTPROC wglDeleteRC = NULL;

ReplayStatus GL_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
  RDCDEBUG("Creating an OpenGL replay device");

  HMODULE lib = NULL;
  lib = LoadLibraryA("opengl32.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load opengl32.dll");
    return ReplayStatus::APIInitFailed;
  }

  GLInitParams initParams;
  RDCDriver driverType = RDC_OpenGL;
  string driverName = "OpenGL";
  uint64_t machineIdent = 0;
  if(logfile)
  {
    auto status = RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, machineIdent,
                                                   (RDCInitParams *)&initParams);
    if(status != ReplayStatus::Succeeded)
      return status;
  }

  PIXELFORMATDESCRIPTOR pfd = {0};

  if(wglGetProc == NULL)
  {
    wglGetProc = (WGLGETPROCADDRESSPROC)GetProcAddress(lib, "wglGetProcAddress");
    wglCreateRC = (WGLCREATECONTEXTPROC)GetProcAddress(lib, "wglCreateContext");
    wglMakeCurrentProc = (WGLMAKECURRENTPROC)GetProcAddress(lib, "wglMakeCurrent");
    wglDeleteRC = (WGLDELETECONTEXTPROC)GetProcAddress(lib, "wglDeleteContext");

    if(wglGetProc == NULL || wglCreateRC == NULL || wglMakeCurrentProc == NULL || wglDeleteRC == NULL)
    {
      RDCERR("Couldn't get wgl function addresses");
      return ReplayStatus::APIInitFailed;
    }

    WNDCLASSEX wc;
    RDCEraseEl(wc);
    wc.style = CS_OWNDC;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"renderdocGLclass";

    if(!RegisterClassEx(&wc))
    {
      RDCERR("Couldn't register GL window class");
      return ReplayStatus::APIInitFailed;
    }

    RDCEraseEl(pfd);
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 0;
  }

  HWND w = CreateWindowEx(WS_EX_CLIENTEDGE, L"renderdocGLclass", L"", WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL,
                          GetModuleHandle(NULL), NULL);

  HDC dc = GetDC(w);

  int pf = ChoosePixelFormat(dc, &pfd);
  if(pf == 0)
  {
    RDCERR("Couldn't choose pixel format");
    return ReplayStatus::APIInitFailed;
  }

  BOOL res = SetPixelFormat(dc, pf, &pfd);
  if(res == FALSE)
  {
    RDCERR("Couldn't set pixel format");
    return ReplayStatus::APIInitFailed;
  }

  HGLRC rc = wglCreateRC(dc);
  if(rc == NULL)
  {
    RDCERR("Couldn't create simple RC");
    return ReplayStatus::APIInitFailed;
  }

  res = wglMakeCurrentProc(dc, rc);
  if(res == FALSE)
  {
    RDCERR("Couldn't make simple RC current");
    return ReplayStatus::APIInitFailed;
  }

  createContextAttribs =
      (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProc("wglCreateContextAttribsARB");
  getPixelFormatAttrib =
      (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)wglGetProc("wglGetPixelFormatAttribivARB");

  if(createContextAttribs == NULL || getPixelFormatAttrib == NULL)
  {
    RDCERR("RenderDoc requires WGL_ARB_create_context and WGL_ARB_pixel_format");
    return ReplayStatus::APIHardwareUnsupported;
  }

  wglMakeCurrentProc(NULL, NULL);
  wglDeleteRC(rc);
  ReleaseDC(w, dc);
  DestroyWindow(w);

  GLReplay::PreContextInitCounters();

  // we don't use the default framebuffer (backbuffer) for anything, so we make it
  // tiny and with no depth/stencil bits
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 0;
  pfd.cStencilBits = 0;

  w = CreateWindowEx(WS_EX_CLIENTEDGE, L"renderdocGLclass", L"RenderDoc replay window",
                     WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 32, 32, NULL, NULL,
                     GetModuleHandle(NULL), NULL);

  dc = GetDC(w);

  pf = ChoosePixelFormat(dc, &pfd);
  if(pf == 0)
  {
    RDCERR("Couldn't choose pixel format");
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  res = SetPixelFormat(dc, pf, &pfd);
  if(res == FALSE)
  {
    RDCERR("Couldn't set pixel format");
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
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
  attribs[i++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

  // try to create all versions from 4.5 down to 3.2 in order to get the
  // highest versioned context we can
  struct
  {
    int major;
    int minor;
  } versions[] = {
      {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2},
  };

  rc = NULL;

  for(size_t v = 0; v < ARRAY_COUNT(versions); v++)
  {
    major = versions[v].major;
    minor = versions[v].minor;
    rc = createContextAttribs(dc, NULL, attribs);

    if(rc)
      break;
  }
  if(rc == NULL)
  {
    RDCERR("Couldn't create 3.2 RC - RenderDoc requires OpenGL 3.2 availability");
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIHardwareUnsupported;
  }

  GLCoreVersion = major * 10 + minor;

  res = wglMakeCurrentProc(dc, rc);
  if(res == FALSE)
  {
    RDCERR("Couldn't make 3.2 RC current");
    wglMakeCurrentProc(NULL, NULL);
    wglDeleteRC(rc);
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  PFNGLGETINTEGERVPROC getInt = (PFNGLGETINTEGERVPROC)GetProcAddress(lib, "glGetIntegerv");
  PFNGLGETSTRINGPROC getStr = (PFNGLGETSTRINGPROC)GetProcAddress(lib, "glGetString");
  PFNGLGETSTRINGIPROC getStri = (PFNGLGETSTRINGIPROC)wglGetProc("glGetStringi");

  if(getInt == NULL || getStr == NULL || getStri == NULL)
  {
    RDCERR("Couldn't get glGetIntegerv (%p), glGetString (%p) or glGetStringi (%p) entry points",
           getInt, getStr, getStri);
    wglMakeCurrentProc(NULL, NULL);
    wglDeleteRC(rc);
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  bool missingExt = CheckReplayContext(getStr, getInt, getStri);

  if(missingExt)
  {
    wglMakeCurrentProc(NULL, NULL);
    wglDeleteRC(rc);
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  const GLHookSet &real = GetRealGLFunctions();

  bool extensionsValidated = ValidateFunctionPointers(real);

  if(!extensionsValidated)
  {
    wglMakeCurrentProc(NULL, NULL);
    wglDeleteRC(rc);
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  WrappedOpenGL *gl = new WrappedOpenGL(logfile, real, GetGLPlatform());
  gl->Initialise(initParams);

  if(gl->GetSerialiser()->HasError())
  {
    delete gl;
    return ReplayStatus::FileIOFailed;
  }

  RDCLOG("Created device.");
  GLReplay *replay = gl->GetReplay();
  replay->SetProxy(logfile == NULL);
  GLWindowingData data;
  data.DC = dc;
  data.ctx = rc;
  data.wnd = w;
  replay->SetReplayData(data);

  *driver = (IReplayDriver *)replay;
  return ReplayStatus::Succeeded;
}
