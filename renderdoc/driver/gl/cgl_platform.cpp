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

#define GL_GLEXT_PROTOTYPES

#include "cgl_dispatch_table.h"
#include "gl_common.h"

#include "apple_gl_hook_defs.h"

// helpers defined in cgl_platform.mm
extern "C" int NSGL_getLayerWidth(void *layer);
extern "C" int NSGL_getLayerHeight(void *layer);
extern "C" void *NSGL_createContext(void *view, void *shareNSCtx);
extern "C" void NSGL_makeCurrentContext(void *nsctx);
extern "C" void NSGL_update(void *nsctx);
extern "C" void NSGL_flushBuffer(void *nsctx);
extern "C" void NSGL_destroyContext(void *nsctx);

// gl functions (used for quad rendering on legacy contexts)
extern "C" void glPushMatrix();
extern "C" void glLoadIdentity();
extern "C" void glMatrixMode(GLenum);
extern "C" void glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
extern "C" void glPopMatrix();
extern "C" void glBegin(GLenum);
extern "C" void glVertex2f(float, float);
extern "C" void glTexCoord2f(float, float);
extern "C" void glEnd();

#define QUAD_GL_FUNCS(FUNC) \
  FUNC(glGetIntegerv);      \
  FUNC(glPushMatrix);       \
  FUNC(glLoadIdentity);     \
  FUNC(glMatrixMode);       \
  FUNC(glOrtho);            \
  FUNC(glPopMatrix);        \
  FUNC(glBegin);            \
  FUNC(glVertex2f);         \
  FUNC(glTexCoord2f);       \
  FUNC(glEnd);

#define DECL_PTR(a) decltype(&a) a;
struct QuadGL
{
  QUAD_GL_FUNCS(DECL_PTR);
};

template <>
rdcstr DoStringise(const CGLError &el)
{
  BEGIN_ENUM_STRINGISE(CGLError);
  {
    STRINGISE_ENUM_NAMED(kCGLNoError, "no error");
    STRINGISE_ENUM_NAMED(kCGLBadAttribute, "invalid pixel format attribute");
    STRINGISE_ENUM_NAMED(kCGLBadProperty, "invalid renderer property");
    STRINGISE_ENUM_NAMED(kCGLBadPixelFormat, "invalid pixel format");
    STRINGISE_ENUM_NAMED(kCGLBadRendererInfo, "invalid renderer info");
    STRINGISE_ENUM_NAMED(kCGLBadContext, "invalid context");
    STRINGISE_ENUM_NAMED(kCGLBadDrawable, "invalid drawable");
    STRINGISE_ENUM_NAMED(kCGLBadDisplay, "invalid graphics device");
    STRINGISE_ENUM_NAMED(kCGLBadState, "invalid context state");
    STRINGISE_ENUM_NAMED(kCGLBadValue, "invalid numerical value");
    STRINGISE_ENUM_NAMED(kCGLBadMatch, "invalid share context");
    STRINGISE_ENUM_NAMED(kCGLBadEnumeration, "invalid enumerant");
    STRINGISE_ENUM_NAMED(kCGLBadOffScreen, "invalid offscreen drawable");
    STRINGISE_ENUM_NAMED(kCGLBadFullScreen, "invalid fullscreen drawable");
    STRINGISE_ENUM_NAMED(kCGLBadWindow, "invalid window");
    STRINGISE_ENUM_NAMED(kCGLBadAddress, "invalid pointer");
    STRINGISE_ENUM_NAMED(kCGLBadCodeModule, "invalid code module");
    STRINGISE_ENUM_NAMED(kCGLBadAlloc, "invalid memory allocation");
    STRINGISE_ENUM_NAMED(kCGLBadConnection, "invalid CoreGraphics connection");
  }
  END_ENUM_STRINGISE();
}

class CGLPlatform : public GLPlatform
{
  bool MakeContextCurrent(GLWindowingData data)
  {
    if(RenderDoc::Inst().IsReplayApp())
    {
      NSGL_makeCurrentContext(data.nsctx);
      return true;
    }
    else
    {
      if(CGL.CGLSetCurrentContext)
      {
        CGLError err = CGL.CGLSetCurrentContext(data.ctx);
        if(err == kCGLNoError)
          return true;
        RDCERR("MakeContextCurrent: %s", ToStr(err).c_str());
      }
    }
    return false;
  }
  GLWindowingData CloneTemporaryContext(GLWindowingData share)
  {
    GLWindowingData ret = share;

    ret.ctx = NULL;

    if(RenderDoc::Inst().IsReplayApp())
    {
      RDCASSERT(share.nsctx);
      ret.nsctx = NSGL_createContext(NULL, share.nsctx);
    }
    else
    {
      if(share.ctx && CGL.CGLCreateContext)
      {
        CGLError err = CGL.CGLCreateContext(share.pix, share.ctx, &ret.ctx);
        RDCASSERTMSG("Error creating temporary context", err == kCGLNoError, err);
      }
    }

    return ret;
  }

  void DeleteClonedContext(GLWindowingData context)
  {
    if(RenderDoc::Inst().IsReplayApp())
    {
      NSGL_destroyContext(context.nsctx);
    }
    else
    {
      if(context.ctx && CGL.CGLDestroyContext)
        CGL.CGLDestroyContext(context.ctx);
    }
  }
  void DeleteReplayContext(GLWindowingData context)
  {
    RDCASSERT(context.nsctx);
    NSGL_destroyContext(context.nsctx);
  }
  void SwapBuffers(GLWindowingData context) { NSGL_flushBuffer(context.nsctx); }
  void WindowResized(GLWindowingData context) { NSGL_update(context.nsctx); }
  void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h)
  {
    if(context.layer)
    {
      w = NSGL_getLayerWidth(context.layer);
      h = NSGL_getLayerHeight(context.layer);
    }
    else
    {
      w = h = 0;
    }
  }
  bool IsOutputWindowVisible(GLWindowingData context) { return true; }
  void *GetReplayFunction(const char *funcname)
  {
#undef APPLE_FUNC
#define APPLE_FUNC(function)                 \
  if(!strcmp(funcname, STRINGIZE(function))) \
    return (void *)&::function;

    ForEachAppleSupported();

    return NULL;
  }
  bool CanCreateGLESContext() { return false; }
  bool PopulateForReplay() { return CGL.PopulateForReplay(); }
  GLWindowingData MakeOutputWindow(WindowingData window, bool depth, GLWindowingData share_context)
  {
    GLWindowingData ret = {};

    if(window.system == WindowingSystem::MacOS)
    {
      RDCASSERT(window.macOS.layer && window.macOS.view);

      ret.nsctx = NSGL_createContext(window.macOS.view, share_context.nsctx);
      ret.wnd = window.macOS.view;
      ret.layer = window.macOS.layer;

      return ret;
    }
    else if(window.system == WindowingSystem::Unknown || window.system == WindowingSystem::Headless)
    {
      ret.nsctx = NSGL_createContext(NULL, share_context.nsctx);

      return ret;
    }
    else
    {
      RDCERR("Unexpected window system %u", system);
    }

    return ret;
  }

  ReplayStatus InitialiseAPI(GLWindowingData &replayContext, RDCDriver api)
  {
    RDCASSERT(api == RDCDriver::OpenGL);

    replayContext.nsctx = NSGL_createContext(NULL, NULL);

    return ReplayStatus::Succeeded;
  }

  void DrawQuads(float width, float height, const std::vector<Vec4f> &vertices)
  {
    static QuadGL quadGL = {};

    if(!quadGL.glBegin)
    {
#define FETCH_PTR(a) quadGL.a = &::a;
      QUAD_GL_FUNCS(FETCH_PTR)
    }
    ::DrawQuads(quadGL, width, height, vertices);
  }
} cglPlatform;

CGLDispatchTable CGL = {};

GLPlatform &GetGLPlatform()
{
  return cglPlatform;
}

bool CGLDispatchTable::PopulateForReplay()
{
  RDCASSERT(RenderDoc::Inst().IsReplayApp());

  RDCDEBUG("Initialising GL function pointers");

  bool symbols_ok = true;

#define LOAD_FUNC(func)                              \
  if(!this->func)                                    \
    this->func = &::func;                            \
                                                     \
  if(!this->func)                                    \
  {                                                  \
    symbols_ok = false;                              \
    RDCWARN("Unable to load '%s'", STRINGIZE(func)); \
  }

  CGL_HOOKED_SYMBOLS(LOAD_FUNC)
  CGL_NONHOOKED_SYMBOLS(LOAD_FUNC)

#undef LOAD_FUNC
  return symbols_ok;
}