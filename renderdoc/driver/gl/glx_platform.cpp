/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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
#include "driver/gl/glx_dispatch_table.h"

static bool X11ErrorSeen = false;

int NonFatalX11ErrorHandler(Display *display, XErrorEvent *error)
{
  X11ErrorSeen = true;
  return 0;
}

typedef int (*X11ErrorHandler)(Display *display, XErrorEvent *error);

void *GetGLHandle()
{
  void *handle = Process::LoadModule("libGL.so");

  if(!handle)
    handle = Process::LoadModule("libGL.so.1");

  return handle;
}

class GLXPlatform : public GLPlatform
{
  bool MakeContextCurrent(GLWindowingData data)
  {
    if(GLX.glXMakeCurrent)
      return GLX.glXMakeCurrent(data.dpy, data.wnd, data.ctx);

    return false;
  }

  GLWindowingData MakeContext(GLWindowingData share)
  {
    GLWindowingData ret = {};

    if(!GLX.glXCreateContextAttribsARB)
      return ret;

    const int attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB,
        3,
        GLX_CONTEXT_MINOR_VERSION_ARB,
        2,
        GLX_CONTEXT_FLAGS_ARB,
        0,
        GLX_CONTEXT_PROFILE_MASK_ARB,
        GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
        0,
    };
    bool is_direct = false;

    if(GLX.glXIsDirect)
      is_direct = GLX.glXIsDirect(share.dpy, share.ctx);

    if(GLX.glXChooseFBConfig && GLX.glXCreatePbuffer)
    {
      // don't need to care about the fb config as we won't be using the default framebuffer
      // (backbuffer)
      int visAttribs[] = {0};
      int numCfgs = 0;
      GLXFBConfig *fbcfg =
          GLX.glXChooseFBConfig(share.dpy, DefaultScreen(share.dpy), visAttribs, &numCfgs);

      // don't care about pbuffer properties as we won't render directly to this
      int pbAttribs[] = {GLX_PBUFFER_WIDTH, 32, GLX_PBUFFER_HEIGHT, 32, 0};

      if(fbcfg)
      {
        ret.wnd = GLX.glXCreatePbuffer(share.dpy, fbcfg[0], pbAttribs);
        ret.dpy = share.dpy;
        ret.ctx = GLX.glXCreateContextAttribsARB(share.dpy, fbcfg[0], share.ctx, is_direct, attribs);
      }
    }

    return ret;
  }

  void DeleteContext(GLWindowingData context)
  {
    if(context.wnd && GLX.glXDestroyPbuffer)
      GLX.glXDestroyPbuffer(context.dpy, context.wnd);

    if(context.ctx && GLX.glXDestroyContext)
      GLX.glXDestroyContext(context.dpy, context.ctx);
  }

  void DeleteReplayContext(GLWindowingData context)
  {
    if(GLX.glXDestroyContext)
    {
      GLX.glXMakeContextCurrent(context.dpy, 0L, 0L, NULL);
      GLX.glXDestroyContext(context.dpy, context.ctx);
    }
  }

  void SwapBuffers(GLWindowingData context) { GLX.glXSwapBuffers(context.dpy, context.wnd); }
  void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h)
  {
    GLX.glXQueryDrawable(context.dpy, context.wnd, GLX_WIDTH, (unsigned int *)&w);
    GLX.glXQueryDrawable(context.dpy, context.wnd, GLX_HEIGHT, (unsigned int *)&h);
  }

  bool IsOutputWindowVisible(GLWindowingData context)
  {
    GLNOTIMP("Optimisation missing - output window always returning true");

    return true;
  }

  GLWindowingData MakeOutputWindow(WindowingData window, bool depth, GLWindowingData share_context)
  {
    GLWindowingData ret;

    Display *dpy = NULL;
    Drawable draw = 0;

    if(window.system == WindowingSystem::Xlib)
    {
#if ENABLED(RDOC_XLIB)
      dpy = window.xlib.display;
      draw = window.xlib.window;
#else
      RDCERR(
          "Xlib windowing system data passed in, but support is not compiled in. GL must have xlib "
          "support compiled in");
#endif
    }
    else if(window.system == WindowingSystem::Unknown)
    {
      // allow WindowingSystem::Unknown so that internally we can create a window-less context
      dpy = RenderDoc::Inst().GetGlobalEnvironment().xlibDisplay;

      if(dpy == NULL)
        return ret;
    }
    else
    {
      RDCERR("Unexpected window system %u", system);
    }

    // GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB MUST be the last attrib so that we can remove it to retry
    // if we find no srgb fbconfigs
    static int visAttribs[] = {GLX_X_RENDERABLE,
                               True,
                               GLX_DRAWABLE_TYPE,
                               GLX_WINDOW_BIT,
                               GLX_RENDER_TYPE,
                               GLX_RGBA_BIT,
                               GLX_X_VISUAL_TYPE,
                               GLX_TRUE_COLOR,
                               GLX_RED_SIZE,
                               8,
                               GLX_GREEN_SIZE,
                               8,
                               GLX_BLUE_SIZE,
                               8,
                               GLX_DOUBLEBUFFER,
                               True,
                               GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB,
                               True,
                               0};

    int numCfgs = 0;
    GLXFBConfig *fbcfg = GLX.glXChooseFBConfig(dpy, DefaultScreen(dpy), visAttribs, &numCfgs);

    if(fbcfg == NULL)
    {
      const size_t len = ARRAY_COUNT(visAttribs);
      if(visAttribs[len - 3] != GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB)
      {
        RDCERR(
            "GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB isn't the last attribute, and no SRGB fbconfigs were "
            "found!");
      }
      else
      {
        visAttribs[len - 3] = 0;
        fbcfg = GLX.glXChooseFBConfig(dpy, DefaultScreen(dpy), visAttribs, &numCfgs);
      }
    }

    if(fbcfg == NULL)
    {
      RDCERR("Couldn't choose default framebuffer config");
      return ret;
    }

    if(draw != 0)
    {
      // Choose FB config with a GLX_VISUAL_ID that matches the X screen.
      VisualID visualid_correct = DefaultVisual(dpy, DefaultScreen(dpy))->visualid;
      for(int i = 0; i < numCfgs; i++)
      {
        int visualid;
        GLX.glXGetFBConfigAttrib(dpy, fbcfg[i], GLX_VISUAL_ID, &visualid);
        if((VisualID)visualid == visualid_correct)
        {
          fbcfg[0] = fbcfg[i];
          break;
        }
      }
    }

    int attribs[64] = {0};
    int i = 0;

    attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
    attribs[i++] = GLCoreVersion / 10;
    attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
    attribs[i++] = GLCoreVersion % 10;
    attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
#if ENABLED(RDOC_DEVEL)
    attribs[i++] = GLX_CONTEXT_DEBUG_BIT_ARB;
#else
    attribs[i++] = 0;
#endif
    attribs[i++] = GLX_CONTEXT_PROFILE_MASK_ARB;
    attribs[i++] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;

    GLXContext ctx = GLX.glXCreateContextAttribsARB(dpy, fbcfg[0], share_context.ctx, true, attribs);

    if(ctx == NULL)
    {
      RDCERR("Couldn't create %d.%d context - something changed since creation", GLCoreVersion / 10,
             GLCoreVersion % 10);
      return ret;
    }

    GLXDrawable wnd = 0;

    if(draw == 0)
    {
      // don't care about pbuffer properties as we won't render directly to this
      int pbAttribs[] = {GLX_PBUFFER_WIDTH, 32, GLX_PBUFFER_HEIGHT, 32, 0};

      wnd = GLX.glXCreatePbuffer(dpy, fbcfg[0], pbAttribs);
    }
    else
    {
      // on NV and AMD creating this window causes problems rendering to any widgets in Qt, with the
      // width/height queries failing to return any values and the framebuffer blitting not working.
      // For the moment, we use the passed-in drawable directly as this works in testing on
      // renderdoccmd and qrenderdoc
      wnd = draw;
      // glXCreateWindow(dpy, fbcfg[0], draw, 0);
    }

    XFree(fbcfg);

    ret.dpy = dpy;
    ret.ctx = ctx;
    ret.wnd = wnd;

    return ret;
  }

  void *GetReplayFunction(const char *funcname)
  {
    void *ret = NULL;
    if(GLX.glXGetProcAddressARB)
      ret = (void *)GLX.glXGetProcAddressARB((const GLubyte *)funcname);

    if(!ret && GLX.glXGetProcAddress)
      ret = (void *)GLX.glXGetProcAddress((const GLubyte *)funcname);

    if(!ret)
      ret = Process::GetFunctionAddress(GetGLHandle(), funcname);

    return ret;
  }

  bool CanCreateGLESContext()
  {
    bool success = GLX.PopulateForReplay();

    // if we can't populate our functions we bail now.
    if(!success)
      return false;

    // we need to check for the presence of EXT_create_context_es2_profile
    Display *dpy = RenderDoc::Inst().GetGlobalEnvironment().xlibDisplay;

    const char *exts = GLX.glXQueryExtensionsString(dpy, DefaultScreen(dpy));

    bool ret = (strstr(exts, "EXT_create_context_es2_profile") != NULL);

    RDCDEBUG("%s find EXT_create_context_es2_profile to create GLES context",
             ret ? "Could" : "Couldn't");

    return ret;
  }

  bool PopulateForReplay() { return GLX.PopulateForReplay(); }
  ReplayStatus InitialiseAPI(GLWindowingData &replayContext, RDCDriver api)
  {
    RDCASSERT(api == RDCDriver::OpenGL || api == RDCDriver::OpenGLES);

    int attribs[64] = {0};
    int i = 0;

    attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
    int &major = attribs[i];
    attribs[i++] = 0;
    attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
    int &minor = attribs[i];
    attribs[i++] = 0;
    attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
#if ENABLED(RDOC_DEVEL)
    attribs[i++] = GLX_CONTEXT_DEBUG_BIT_ARB;
#else
    attribs[i++] = 0;
#endif
    attribs[i++] = GLX_CONTEXT_PROFILE_MASK_ARB;
    attribs[i++] = api == RDCDriver::OpenGLES ? GLX_CONTEXT_ES2_PROFILE_BIT_EXT
                                              : GLX_CONTEXT_CORE_PROFILE_BIT_ARB;

    Display *dpy = RenderDoc::Inst().GetGlobalEnvironment().xlibDisplay;

    if(dpy == NULL)
    {
      RDCERR("Couldn't open default X display");
      return ReplayStatus::APIInitFailed;
    }

    // don't need to care about the fb config as we won't be using the default framebuffer
    // (backbuffer)
    static int visAttribs[] = {0};
    int numCfgs = 0;
    GLXFBConfig *fbcfg = GLX.glXChooseFBConfig(dpy, DefaultScreen(dpy), visAttribs, &numCfgs);

    if(fbcfg == NULL)
    {
      RDCERR("Couldn't choose default framebuffer config");
      return ReplayStatus::APIInitFailed;
    }

    GLXContext ctx = NULL;

    {
      X11ErrorHandler prev = XSetErrorHandler(&NonFatalX11ErrorHandler);

      std::vector<GLVersion> versions = GetReplayVersions(api);

      for(GLVersion v : versions)
      {
        X11ErrorSeen = false;

        major = v.major;
        minor = v.minor;
        ctx = GLX.glXCreateContextAttribsARB(dpy, fbcfg[0], 0, true, attribs);

        if(ctx && !X11ErrorSeen)
          break;
      }

      XSetErrorHandler(prev);
    }

    if(ctx == NULL || X11ErrorSeen)
    {
      XFree(fbcfg);
      RDCERR("Couldn't create 3.2 context - RenderDoc requires OpenGL 3.2 availability");
      return ReplayStatus::APIHardwareUnsupported;
    }

    GLCoreVersion = major * 10 + minor;

    // don't care about pbuffer properties for same reason as backbuffer
    int pbAttribs[] = {GLX_PBUFFER_WIDTH, 32, GLX_PBUFFER_HEIGHT, 32, 0};

    GLXPbuffer pbuffer = GLX.glXCreatePbuffer(dpy, fbcfg[0], pbAttribs);

    XFree(fbcfg);

    Bool res = GLX.glXMakeContextCurrent(dpy, pbuffer, pbuffer, ctx);

    if(!res)
    {
      GLX.glXDestroyPbuffer(dpy, pbuffer);
      GLX.glXDestroyContext(dpy, ctx);
      RDCERR("Couldn't make pbuffer & context current");
      return ReplayStatus::APIInitFailed;
    }

    PFNGLGETSTRINGPROC GetString = (PFNGLGETSTRINGPROC)GetReplayFunction("glGetString");

    if(GetString)
    {
      const char *vendor = (const char *)GetString(eGL_VENDOR);
      const char *version = (const char *)GetString(eGL_VERSION);

      if(strstr(vendor, "NVIDIA") && strstr(version, "378."))
      {
        RDCLOG("There is a known crash issue on NVIDIA 378.x series drivers.");
        RDCLOG(
            "If you hit a crash after this message, try setting __GL_THREADED_OPTIMIZATIONS=0 or "
            "upgrade to 381.x or newer.");
        RDCLOG("See https://github.com/baldurk/renderdoc/issues/609 for more information.");
      }
    }

    replayContext.dpy = dpy;
    replayContext.ctx = ctx;
    replayContext.wnd = pbuffer;

    return ReplayStatus::Succeeded;
  }

  void DrawQuads(float width, float height, const std::vector<Vec4f> &vertices)
  {
    ::DrawQuads(GLX, width, height, vertices);
  }
} glXPlatform;

GLXDispatchTable GLX = {};

GLPlatform &GetGLPlatform()
{
  return glXPlatform;
}

bool GLXDispatchTable::PopulateForReplay()
{
  RDCASSERT(RenderDoc::Inst().IsReplayApp());
  GetGLHandle();

  void *handle = GetGLHandle();

  if(!handle)
  {
    RDCERR("Can't load libGL.so or libGL.so.1");
    return false;
  }

  RDCDEBUG("Initialising GL function pointers");

  bool symbols_ok = true;

#define LOAD_FUNC(func)                                                                             \
  if(!this->func)                                                                                   \
    this->func = (CONCAT(PFN_, func))Process::GetFunctionAddress(handle, STRINGIZE(func));          \
                                                                                                    \
  if(!func && this->glXGetProcAddressARB)                                                           \
    this->func = (CONCAT(PFN_, func)) this->glXGetProcAddressARB((const GLubyte *)STRINGIZE(func)); \
                                                                                                    \
  if(!func && this->glXGetProcAddress)                                                              \
    this->func = (CONCAT(PFN_, func)) this->glXGetProcAddress((const GLubyte *)STRINGIZE(func));    \
                                                                                                    \
  if(!func)                                                                                         \
  {                                                                                                 \
    symbols_ok = false;                                                                             \
    RDCWARN("Unable to load '%s'", STRINGIZE(func));                                                \
  }

  GLX_HOOKED_SYMBOLS(LOAD_FUNC)
  GLX_NONHOOKED_SYMBOLS(LOAD_FUNC)

#undef LOAD_FUNC
  return symbols_ok;
}
