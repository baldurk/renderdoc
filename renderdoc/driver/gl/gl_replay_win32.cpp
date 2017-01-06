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

void GLReplay::MakeCurrentReplayContext(GLWindowingData *ctx)
{
  static GLWindowingData *prev = NULL;

  if(wglMakeCurrentProc && ctx && ctx != prev)
  {
    prev = ctx;
    wglMakeCurrentProc(ctx->DC, ctx->ctx);
    m_pDriver->ActivateContext(*ctx);
  }
}

void GLReplay::SwapBuffers(GLWindowingData *ctx)
{
  ::SwapBuffers(ctx->DC);
}

void GLReplay::CloseReplayContext()
{
  if(wglDeleteRC)
  {
    wglMakeCurrentProc(NULL, NULL);
    wglDeleteRC(m_ReplayCtx.ctx);
    ReleaseDC(m_ReplayCtx.wnd, m_ReplayCtx.DC);
    ::DestroyWindow(m_ReplayCtx.wnd);
  }
}

uint64_t GLReplay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  RDCASSERT(system == eWindowingSystem_Win32 || system == eWindowingSystem_Unknown, system);

  HWND w = (HWND)data;

  if(w == NULL)
    w = CreateWindowEx(WS_EX_CLIENTEDGE, L"renderdocGLclass", L"", WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL,
                       GetModuleHandle(NULL), NULL);

  HDC DC = GetDC(w);

  PIXELFORMATDESCRIPTOR pfd = {0};

  int attrib = eWGL_NUMBER_PIXEL_FORMATS_ARB;
  int value = 1;

  getPixelFormatAttrib(DC, 1, 0, 1, &attrib, &value);

  int pf = 0;

  int numpfs = value;
  for(int i = 1; i <= numpfs; i++)
  {
    // verify that we have the properties we want
    attrib = eWGL_DRAW_TO_WINDOW_ARB;
    getPixelFormatAttrib(DC, i, 0, 1, &attrib, &value);
    if(value == 0)
      continue;

    attrib = eWGL_ACCELERATION_ARB;
    getPixelFormatAttrib(DC, i, 0, 1, &attrib, &value);
    if(value == eWGL_NO_ACCELERATION_ARB)
      continue;

    attrib = eWGL_SUPPORT_OPENGL_ARB;
    getPixelFormatAttrib(DC, i, 0, 1, &attrib, &value);
    if(value == 0)
      continue;

    attrib = eWGL_DOUBLE_BUFFER_ARB;
    getPixelFormatAttrib(DC, i, 0, 1, &attrib, &value);
    if(value == 0)
      continue;

    attrib = eWGL_PIXEL_TYPE_ARB;
    getPixelFormatAttrib(DC, i, 0, 1, &attrib, &value);
    if(value != eWGL_TYPE_RGBA_ARB)
      continue;

    // we have an opengl-capable accelerated RGBA context.
    // we use internal framebuffers to do almost all rendering, so we just need
    // RGB (color bits > 24) and SRGB buffer.

    attrib = eWGL_COLOR_BITS_ARB;
    getPixelFormatAttrib(DC, i, 0, 1, &attrib, &value);
    if(value < 24)
      continue;

    attrib = WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
    getPixelFormatAttrib(DC, i, 0, 1, &attrib, &value);
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
    return NULL;
  }

  BOOL res = DescribePixelFormat(DC, pf, sizeof(pfd), &pfd);
  if(res == FALSE)
  {
    ReleaseDC(w, DC);
    RDCERR("Couldn't describe pixel format");
    return NULL;
  }

  res = SetPixelFormat(DC, pf, &pfd);
  if(res == FALSE)
  {
    ReleaseDC(w, DC);
    RDCERR("Couldn't set pixel format");
    return NULL;
  }

  int attribs[64] = {0};
  int i = 0;

  attribs[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
  attribs[i++] = 4;
  attribs[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
  attribs[i++] = 3;
  attribs[i++] = WGL_CONTEXT_FLAGS_ARB;
#if ENABLED(RDOC_DEVEL)
  attribs[i++] = WGL_CONTEXT_DEBUG_BIT_ARB;
#else
  attribs[i++] = 0;
#endif
  attribs[i++] = WGL_CONTEXT_PROFILE_MASK_ARB;
  attribs[i++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

  HGLRC rc = createContextAttribs(DC, m_ReplayCtx.ctx, attribs);
  if(rc == NULL)
  {
    ReleaseDC(w, DC);
    RDCERR("Couldn't create 4.3 RC - RenderDoc requires OpenGL 4.3 availability");
    return 0;
  }

  OutputWindow win;
  win.DC = DC;
  win.ctx = rc;
  win.wnd = w;

  RECT rect = {0};
  GetClientRect(w, &rect);
  win.width = rect.right - rect.left;
  win.height = rect.bottom - rect.top;

  m_pDriver->RegisterContext(win, m_ReplayCtx.ctx, true, true);

  InitOutputWindow(win);
  CreateOutputWindowBackbuffer(win, depth);

  uint64_t ret = m_OutputWindowID++;

  m_OutputWindows[ret] = win;

  return ret;
}

void GLReplay::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(id == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  MakeCurrentReplayContext(&outw);

  WrappedOpenGL &gl = *m_pDriver;
  gl.glDeleteFramebuffers(1, &outw.BlitData.readFBO);

  wglMakeCurrentProc(NULL, NULL);
  wglDeleteRC(outw.ctx);
  ReleaseDC(outw.wnd, outw.DC);

  m_OutputWindows.erase(it);
}

void GLReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  RECT rect = {0};
  GetClientRect(outw.wnd, &rect);
  w = rect.right - rect.left;
  h = rect.bottom - rect.top;
}

bool GLReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  return (IsWindowVisible(m_OutputWindows[id].wnd) == TRUE);
}

const GLHookSet &GetRealGLFunctions();

ReplayCreateStatus GL_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
  RDCDEBUG("Creating an OpenGL replay device");

  HMODULE lib = NULL;
  lib = LoadLibraryA("opengl32.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load opengl32.dll");
    return eReplayCreate_APIInitFailed;
  }

  GLInitParams initParams;
  RDCDriver driverType = RDC_OpenGL;
  string driverName = "OpenGL";
  uint64_t machineIdent = 0;
  if(logfile)
  {
    auto status = RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, machineIdent,
                                                   (RDCInitParams *)&initParams);
    if(status != eReplayCreate_Success)
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
      return eReplayCreate_APIInitFailed;
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
      return eReplayCreate_APIInitFailed;
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
    return eReplayCreate_APIInitFailed;
  }

  BOOL res = SetPixelFormat(dc, pf, &pfd);
  if(res == FALSE)
  {
    RDCERR("Couldn't set pixel format");
    return eReplayCreate_APIInitFailed;
  }

  HGLRC rc = wglCreateRC(dc);
  if(rc == NULL)
  {
    RDCERR("Couldn't create simple RC");
    return eReplayCreate_APIInitFailed;
  }

  res = wglMakeCurrentProc(dc, rc);
  if(res == FALSE)
  {
    RDCERR("Couldn't make simple RC current");
    return eReplayCreate_APIInitFailed;
  }

  createContextAttribs =
      (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProc("wglCreateContextAttribsARB");
  getPixelFormatAttrib =
      (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)wglGetProc("wglGetPixelFormatAttribivARB");

  if(createContextAttribs == NULL || getPixelFormatAttrib == NULL)
  {
    RDCERR("RenderDoc requires WGL_ARB_create_context and WGL_ARB_pixel_format");
    return eReplayCreate_APIHardwareUnsupported;
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
    return eReplayCreate_APIInitFailed;
  }

  res = SetPixelFormat(dc, pf, &pfd);
  if(res == FALSE)
  {
    RDCERR("Couldn't set pixel format");
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
    return eReplayCreate_APIInitFailed;
  }

  int attribs[64] = {0};
  int i = 0;

  attribs[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
  attribs[i++] = 4;
  attribs[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
  attribs[i++] = 3;
  attribs[i++] = WGL_CONTEXT_FLAGS_ARB;
#if ENABLED(RDOC_DEVEL)
  attribs[i++] = WGL_CONTEXT_DEBUG_BIT_ARB;
#else
  attribs[i++] = 0;
#endif
  attribs[i++] = WGL_CONTEXT_PROFILE_MASK_ARB;
  attribs[i++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

  rc = createContextAttribs(dc, NULL, attribs);
  if(rc == NULL)
  {
    RDCERR("Couldn't create 4.3 RC - RenderDoc requires OpenGL 4.3 availability");
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
    return eReplayCreate_APIHardwareUnsupported;
  }

  res = wglMakeCurrentProc(dc, rc);
  if(res == FALSE)
  {
    RDCERR("Couldn't make 4.3 RC current");
    wglMakeCurrentProc(NULL, NULL);
    wglDeleteRC(rc);
    ReleaseDC(w, dc);
    GLReplay::PostContextShutdownCounters();
    return eReplayCreate_APIInitFailed;
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
    return eReplayCreate_APIInitFailed;
  }
  else
  {
    // eventually we want to emulate EXT_dsa on replay if it isn't present, but for
    // now we just require it.
    bool dsa = false;
    bool bufstorage = false;

    if(getStr)
      RDCLOG("Running GL replay on: %s / %s / %s", getStr(eGL_VENDOR), getStr(eGL_RENDERER),
             getStr(eGL_VERSION));

    GLint numExts = 0;
    getInt(eGL_NUM_EXTENSIONS, &numExts);
    for(GLint e = 0; e < numExts; e++)
    {
      const char *ext = (const char *)getStri(eGL_EXTENSIONS, (GLuint)e);

      RDCLOG("Extension % 3d: %s", e, ext);

      if(!strcmp(ext, "GL_EXT_direct_state_access"))
        dsa = true;
      if(!strcmp(ext, "GL_ARB_buffer_storage"))
        bufstorage = true;
    }

    if(!dsa)
      RDCERR(
          "RenderDoc requires EXT_direct_state_access availability, and it is not reported. Try "
          "updating your drivers.");

    if(!bufstorage)
      RDCERR(
          "RenderDoc requires ARB_buffer_storage availability, and it is not reported. Try "
          "updating your drivers.");

    if(!dsa || !bufstorage)
    {
      wglMakeCurrentProc(NULL, NULL);
      wglDeleteRC(rc);
      ReleaseDC(w, dc);
      GLReplay::PostContextShutdownCounters();
      return eReplayCreate_APIHardwareUnsupported;
    }
  }

  const GLHookSet &real = GetRealGLFunctions();

  PFNGLGETSTRINGPROC *ptrs = (PFNGLGETSTRINGPROC *)&real;
  size_t num = sizeof(real) / sizeof(PFNGLGETSTRINGPROC);

  RDCLOG("Function pointers available:");
  for(size_t ptr = 0; ptr < num;)
  {
    uint64_t ptrmask = 0;

    for(size_t j = 0; j < 64; j++)
      if(ptr + j < num && ptrs[ptr + j])
        ptrmask |= 1ULL << (63 - j);

    ptr += 64;

    RDCLOG("%064llb", ptrmask);
  }

// check for the presence of GL functions we will call unconditionally as part of the replay
// process.
// Other functions that are only called to deserialise are checked for presence separately

#define CHECK_PRESENT(func)                                                            \
  if(!real.func)                                                                       \
  {                                                                                    \
    RDCERR(                                                                            \
        "Missing function %s, required for replay. RenderDoc requires a 4.3 context, " \
        "EXT_direct_state_access and ARB_buffer_storage",                              \
        #func);                                                                        \
    wglMakeCurrentProc(NULL, NULL);                                                    \
    wglDeleteRC(rc);                                                                   \
    ReleaseDC(w, dc);                                                                  \
    GLReplay::PostContextShutdownCounters();                                           \
    return eReplayCreate_APIHardwareUnsupported;                                       \
  }

  // these functions should all be present as part of a 4.3 context, but let's just be extra-careful
  CHECK_PRESENT(glActiveTexture)
  CHECK_PRESENT(glAttachShader)
  CHECK_PRESENT(glBeginQuery)
  CHECK_PRESENT(glBeginTransformFeedback)
  CHECK_PRESENT(glBindAttribLocation)
  CHECK_PRESENT(glBindBuffer)
  CHECK_PRESENT(glBindBufferBase)
  CHECK_PRESENT(glBindBufferRange)
  CHECK_PRESENT(glBindFragDataLocation)
  CHECK_PRESENT(glBindFramebuffer)
  CHECK_PRESENT(glBindImageTexture)
  CHECK_PRESENT(glBindProgramPipeline)
  CHECK_PRESENT(glBindSampler)
  CHECK_PRESENT(glBindTexture)
  CHECK_PRESENT(glBindTransformFeedback)
  CHECK_PRESENT(glBindVertexArray)
  CHECK_PRESENT(glBindVertexBuffer)
  CHECK_PRESENT(glBlendColor)
  CHECK_PRESENT(glBlendEquationSeparate)
  CHECK_PRESENT(glBlendEquationSeparatei)
  CHECK_PRESENT(glBlendFunc)
  CHECK_PRESENT(glBlendFuncSeparate)
  CHECK_PRESENT(glBlendFuncSeparatei)
  CHECK_PRESENT(glBlitFramebuffer)
  CHECK_PRESENT(glBufferData)
  CHECK_PRESENT(glBufferSubData)
  CHECK_PRESENT(glClearBufferData)
  CHECK_PRESENT(glClearBufferfi)
  CHECK_PRESENT(glClearBufferfv)
  CHECK_PRESENT(glClearBufferiv)
  CHECK_PRESENT(glClearBufferuiv)
  CHECK_PRESENT(glClearColor)
  CHECK_PRESENT(glClearDepth)
  CHECK_PRESENT(glColorMaski)
  CHECK_PRESENT(glCompileShader)
  CHECK_PRESENT(glCopyImageSubData)
  CHECK_PRESENT(glCreateProgram)
  CHECK_PRESENT(glCreateShader)
  CHECK_PRESENT(glCreateShaderProgramv)
  CHECK_PRESENT(glCullFace)
  CHECK_PRESENT(glDebugMessageCallback)
  CHECK_PRESENT(glDeleteBuffers)
  CHECK_PRESENT(glDeleteFramebuffers)
  CHECK_PRESENT(glDeleteProgram)
  CHECK_PRESENT(glDeleteProgramPipelines)
  CHECK_PRESENT(glDeleteQueries)
  CHECK_PRESENT(glDeleteSamplers)
  CHECK_PRESENT(glDeleteShader)
  CHECK_PRESENT(glDeleteTextures)
  CHECK_PRESENT(glDeleteTransformFeedbacks)
  CHECK_PRESENT(glDeleteVertexArrays)
  CHECK_PRESENT(glDepthFunc)
  CHECK_PRESENT(glDepthMask)
  CHECK_PRESENT(glDepthRangeArrayv)
  CHECK_PRESENT(glDetachShader)
  CHECK_PRESENT(glDisable)
  CHECK_PRESENT(glDisablei)
  CHECK_PRESENT(glDisableVertexAttribArray)
  CHECK_PRESENT(glDispatchCompute)
  CHECK_PRESENT(glDrawArrays)
  CHECK_PRESENT(glDrawArraysInstanced)
  CHECK_PRESENT(glDrawArraysInstancedBaseInstance)
  CHECK_PRESENT(glDrawBuffers)
  CHECK_PRESENT(glDrawElements)
  CHECK_PRESENT(glDrawElementsBaseVertex)
  CHECK_PRESENT(glDrawElementsInstancedBaseVertexBaseInstance)
  CHECK_PRESENT(glEnable)
  CHECK_PRESENT(glEnablei)
  CHECK_PRESENT(glEnableVertexAttribArray)
  CHECK_PRESENT(glEndConditionalRender)
  CHECK_PRESENT(glEndQuery)
  CHECK_PRESENT(glEndQueryIndexed)
  CHECK_PRESENT(glEndTransformFeedback)
  CHECK_PRESENT(glFramebufferTexture)
  CHECK_PRESENT(glFramebufferTexture2D)
  CHECK_PRESENT(glFramebufferTexture3D)
  CHECK_PRESENT(glFramebufferTextureLayer)
  CHECK_PRESENT(glFrontFace)
  CHECK_PRESENT(glGenBuffers)
  CHECK_PRESENT(glGenFramebuffers)
  CHECK_PRESENT(glGenProgramPipelines)
  CHECK_PRESENT(glGenQueries)
  CHECK_PRESENT(glGenSamplers)
  CHECK_PRESENT(glGenTextures)
  CHECK_PRESENT(glGenTransformFeedbacks)
  CHECK_PRESENT(glGenVertexArrays)
  CHECK_PRESENT(glGetActiveAtomicCounterBufferiv)
  CHECK_PRESENT(glGetActiveUniformBlockiv)
  CHECK_PRESENT(glGetAttribLocation)
  CHECK_PRESENT(glGetBooleani_v)
  CHECK_PRESENT(glGetBooleanv)
  CHECK_PRESENT(glGetBufferParameteriv)
  CHECK_PRESENT(glGetBufferSubData)
  CHECK_PRESENT(glGetCompressedTexImage)
  CHECK_PRESENT(glGetDoublei_v)
  CHECK_PRESENT(glGetDoublev)
  CHECK_PRESENT(glGetError)
  CHECK_PRESENT(glGetFloati_v)
  CHECK_PRESENT(glGetFloatv)
  CHECK_PRESENT(glGetFragDataLocation)
  CHECK_PRESENT(glGetFramebufferAttachmentParameteriv)
  CHECK_PRESENT(glGetInteger64i_v)
  CHECK_PRESENT(glGetIntegeri_v)
  CHECK_PRESENT(glGetIntegerv)
  CHECK_PRESENT(glGetInternalformativ)
  CHECK_PRESENT(glGetObjectLabel)
  CHECK_PRESENT(glGetProgramInfoLog)
  CHECK_PRESENT(glGetProgramInterfaceiv)
  CHECK_PRESENT(glGetProgramiv)
  CHECK_PRESENT(glGetProgramPipelineiv)
  CHECK_PRESENT(glGetProgramResourceIndex)
  CHECK_PRESENT(glGetProgramResourceiv)
  CHECK_PRESENT(glGetProgramResourceName)
  CHECK_PRESENT(glGetProgramStageiv)
  CHECK_PRESENT(glGetQueryObjectuiv)
  CHECK_PRESENT(glGetSamplerParameterfv)
  CHECK_PRESENT(glGetSamplerParameteriv)
  CHECK_PRESENT(glGetShaderInfoLog)
  CHECK_PRESENT(glGetShaderiv)
  CHECK_PRESENT(glGetString)
  CHECK_PRESENT(glGetStringi)
  CHECK_PRESENT(glGetTexImage)
  CHECK_PRESENT(glGetTexLevelParameteriv)
  CHECK_PRESENT(glGetTexParameterfv)
  CHECK_PRESENT(glGetTexParameteriv)
  CHECK_PRESENT(glGetUniformBlockIndex)
  CHECK_PRESENT(glGetUniformdv)
  CHECK_PRESENT(glGetUniformfv)
  CHECK_PRESENT(glGetUniformiv)
  CHECK_PRESENT(glGetUniformLocation)
  CHECK_PRESENT(glGetUniformSubroutineuiv)
  CHECK_PRESENT(glGetUniformuiv)
  CHECK_PRESENT(glGetVertexAttribfv)
  CHECK_PRESENT(glGetVertexAttribiv)
  CHECK_PRESENT(glHint)
  CHECK_PRESENT(glIsEnabled)
  CHECK_PRESENT(glIsEnabledi)
  CHECK_PRESENT(glLineWidth)
  CHECK_PRESENT(glLinkProgram)
  CHECK_PRESENT(glLogicOp)
  CHECK_PRESENT(glMapBufferRange)
  CHECK_PRESENT(glMinSampleShading)
  CHECK_PRESENT(glObjectLabel)
  CHECK_PRESENT(glPatchParameterfv)
  CHECK_PRESENT(glPatchParameteri)
  CHECK_PRESENT(glPixelStorei)
  CHECK_PRESENT(glPointParameterf)
  CHECK_PRESENT(glPointParameteri)
  CHECK_PRESENT(glPointSize)
  CHECK_PRESENT(glPolygonMode)
  CHECK_PRESENT(glPolygonOffset)
  CHECK_PRESENT(glPrimitiveRestartIndex)
  CHECK_PRESENT(glProgramParameteri)
  CHECK_PRESENT(glProgramUniform1dv)
  CHECK_PRESENT(glProgramUniform1fv)
  CHECK_PRESENT(glProgramUniform1iv)
  CHECK_PRESENT(glProgramUniform1ui)
  CHECK_PRESENT(glProgramUniform1uiv)
  CHECK_PRESENT(glProgramUniform2dv)
  CHECK_PRESENT(glProgramUniform2fv)
  CHECK_PRESENT(glProgramUniform2iv)
  CHECK_PRESENT(glProgramUniform2uiv)
  CHECK_PRESENT(glProgramUniform3dv)
  CHECK_PRESENT(glProgramUniform3fv)
  CHECK_PRESENT(glProgramUniform3iv)
  CHECK_PRESENT(glProgramUniform3uiv)
  CHECK_PRESENT(glProgramUniform4dv)
  CHECK_PRESENT(glProgramUniform4fv)
  CHECK_PRESENT(glProgramUniform4iv)
  CHECK_PRESENT(glProgramUniform4ui)
  CHECK_PRESENT(glProgramUniform4uiv)
  CHECK_PRESENT(glProgramUniformMatrix2dv)
  CHECK_PRESENT(glProgramUniformMatrix2fv)
  CHECK_PRESENT(glProgramUniformMatrix2x3dv)
  CHECK_PRESENT(glProgramUniformMatrix2x3fv)
  CHECK_PRESENT(glProgramUniformMatrix2x4dv)
  CHECK_PRESENT(glProgramUniformMatrix2x4fv)
  CHECK_PRESENT(glProgramUniformMatrix3dv)
  CHECK_PRESENT(glProgramUniformMatrix3fv)
  CHECK_PRESENT(glProgramUniformMatrix3x2dv)
  CHECK_PRESENT(glProgramUniformMatrix3x2fv)
  CHECK_PRESENT(glProgramUniformMatrix3x4dv)
  CHECK_PRESENT(glProgramUniformMatrix3x4fv)
  CHECK_PRESENT(glProgramUniformMatrix4dv)
  CHECK_PRESENT(glProgramUniformMatrix4fv)
  CHECK_PRESENT(glProgramUniformMatrix4x2dv)
  CHECK_PRESENT(glProgramUniformMatrix4x2fv)
  CHECK_PRESENT(glProgramUniformMatrix4x3dv)
  CHECK_PRESENT(glProgramUniformMatrix4x3fv)
  CHECK_PRESENT(glProvokingVertex)
  CHECK_PRESENT(glReadBuffer)
  CHECK_PRESENT(glReadPixels)
  CHECK_PRESENT(glSampleCoverage)
  CHECK_PRESENT(glSampleMaski)
  CHECK_PRESENT(glSamplerParameteri)
  CHECK_PRESENT(glScissorIndexedv)
  CHECK_PRESENT(glShaderSource)
  CHECK_PRESENT(glShaderStorageBlockBinding)
  CHECK_PRESENT(glStencilFuncSeparate)
  CHECK_PRESENT(glStencilMask)
  CHECK_PRESENT(glStencilMaskSeparate)
  CHECK_PRESENT(glStencilOpSeparate)
  CHECK_PRESENT(glTexImage2D)
  CHECK_PRESENT(glTexParameteri)
  CHECK_PRESENT(glTexStorage2D)
  CHECK_PRESENT(glTextureView)
  CHECK_PRESENT(glTransformFeedbackVaryings)
  CHECK_PRESENT(glUniform1i)
  CHECK_PRESENT(glUniform1ui)
  CHECK_PRESENT(glUniform2f)
  CHECK_PRESENT(glUniform2fv)
  CHECK_PRESENT(glUniform4fv)
  CHECK_PRESENT(glUniformBlockBinding)
  CHECK_PRESENT(glUniformMatrix4fv)
  CHECK_PRESENT(glUniformSubroutinesuiv)
  CHECK_PRESENT(glUnmapBuffer)
  CHECK_PRESENT(glUseProgram)
  CHECK_PRESENT(glUseProgramStages)
  CHECK_PRESENT(glVertexAttrib4fv)
  CHECK_PRESENT(glVertexAttribBinding)
  CHECK_PRESENT(glVertexAttribFormat)
  CHECK_PRESENT(glVertexAttribIFormat)
  CHECK_PRESENT(glVertexAttribLFormat)
  CHECK_PRESENT(glVertexAttribPointer)
  CHECK_PRESENT(glVertexBindingDivisor)
  CHECK_PRESENT(glViewport)
  CHECK_PRESENT(glViewportArrayv)
  CHECK_PRESENT(glViewportIndexedf)

  // these functions should be present as part of EXT_direct_state_access,
  // and ARB_buffer_storage. Let's verify
  CHECK_PRESENT(glCompressedTextureImage1DEXT)
  CHECK_PRESENT(glCompressedTextureImage2DEXT)
  CHECK_PRESENT(glCompressedTextureImage3DEXT)
  CHECK_PRESENT(glCompressedTextureSubImage1DEXT)
  CHECK_PRESENT(glCompressedTextureSubImage2DEXT)
  CHECK_PRESENT(glCompressedTextureSubImage3DEXT)
  CHECK_PRESENT(glGetCompressedTextureImageEXT)
  CHECK_PRESENT(glGetNamedBufferParameterivEXT)
  CHECK_PRESENT(glGetNamedBufferSubDataEXT)
  CHECK_PRESENT(glGetNamedFramebufferAttachmentParameterivEXT)
  CHECK_PRESENT(glGetTextureLevelParameterivEXT)
  CHECK_PRESENT(glGetTextureParameterfvEXT)
  CHECK_PRESENT(glGetTextureParameterivEXT)
  CHECK_PRESENT(glMapNamedBufferEXT)
  CHECK_PRESENT(glNamedBufferDataEXT)
  CHECK_PRESENT(glNamedBufferStorageEXT)    // needs ARB_buffer_storage as well
  CHECK_PRESENT(glNamedBufferSubDataEXT)
  CHECK_PRESENT(glNamedFramebufferRenderbufferEXT)
  CHECK_PRESENT(glNamedFramebufferTextureEXT)
  CHECK_PRESENT(glNamedFramebufferTextureLayerEXT)
  CHECK_PRESENT(glTextureImage1DEXT)
  CHECK_PRESENT(glTextureImage2DEXT)
  CHECK_PRESENT(glTextureImage3DEXT)
  CHECK_PRESENT(glTextureParameterfvEXT)
  CHECK_PRESENT(glTextureParameterivEXT)
  CHECK_PRESENT(glTextureStorage1DEXT)
  CHECK_PRESENT(glTextureStorage2DEXT)
  CHECK_PRESENT(glTextureStorage2DMultisampleEXT)
  CHECK_PRESENT(glTextureStorage3DEXT)
  CHECK_PRESENT(glTextureStorage3DMultisampleEXT)
  CHECK_PRESENT(glTextureSubImage1DEXT)
  CHECK_PRESENT(glTextureSubImage2DEXT)
  CHECK_PRESENT(glTextureSubImage3DEXT)
  CHECK_PRESENT(glUnmapNamedBufferEXT)

  // other functions are either checked for presence explicitly (like
  // depth bounds or polygon offset clamp EXT functions), or they are
  // only called when such a call is serialised from the logfile, and
  // so they are checked for validity separately.

  WrappedOpenGL *gl = new WrappedOpenGL(logfile, real);
  gl->Initialise(initParams);

  if(gl->GetSerialiser()->HasError())
  {
    delete gl;
    return eReplayCreate_FileIOFailed;
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
  return eReplayCreate_Success;
}
