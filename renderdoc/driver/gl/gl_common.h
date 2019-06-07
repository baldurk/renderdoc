/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#pragma once

#include "common/common.h"
#include "core/core.h"
#include "maths/vec.h"

// typed enum so that templates will pick up specialisations
// header must be included before the official headers, so we/
// separate it out to avoid clang-format sorting them differently
#define GLenum RDCGLenum
#include "gl_enum.h"

DECLARE_REFLECTION_ENUM(RDCGLenum);

#if ENABLED(RDOC_WIN32)

// Windows always supports both GL and GLES at compile time - because the driver may support
// WGL_EXT_create_context_es2_profile.
//
// We try to replay using EGL first on all platforms, on windows this means having a libEGL.dll
// available from an OpenGL ES emulator. This is treated as a plugin but it doesn't ship with
// renderdoc, so if you want to replay GLES captures using such an emulator you need to drop the
// appropriate libEGL.dll into plugins/gles/ in your RenderDoc folder.
#define RENDERDOC_SUPPORT_GL
#define RENDERDOC_SUPPORT_GLES

// checks a runtime opt-out option to disallow hooking EGL on windows. This means that the
// underlying GL or D3D calls will be captured instead.
bool ShouldHookEGL();

#else

// other cases are defined by build-time configuration

// for consistency, we always enable this on other platforms - if GLES support is disabled at
// compile time then it does nothing.
#define RENDERDOC_HOOK_EGL OPTION_ON

#endif

#define GL_APICALL

// official headers
#include "official/glcorearb.h"
#include "official/glext.h"

#include "official/gl32.h"
// TODO there are some extensions which are in both headers but with different content
// however it does not seem to be a problem at this time
#include "official/glesext.h"

#if ENABLED(RDOC_WIN32)
#include "official/wglext.h"

#define EGLAPI

// force include the elgplatform.h, as we want to use
// our own because the system one could be a bit older and
// propably not suitable for the given egl.h
#include "official/eglplatform.h"

#include "official/egl.h"
#include "official/eglext.h"

struct GLWindowingData
{
  GLWindowingData()
  {
    DC = NULL;
    ctx = NULL;
    wnd = NULL;
    egl_wnd = NULL;
    egl_cfg = NULL;
  }

  union
  {
    HDC DC;
    EGLDisplay egl_dpy;
  };
  union
  {
    HGLRC ctx;
    EGLContext egl_ctx;
  };
  HWND wnd;
  EGLSurface egl_wnd;

  EGLConfig egl_cfg;
};

#elif ENABLED(RDOC_LINUX)

#if defined(RENDERDOC_SUPPORT_GL)
// cheeky way to prevent GL/gl.h from being included, as we want to use
// glcorearb.h from above
#define __gl_h_

// this prevents glx.h from including its own glxext.h
#define GLX_GLXEXT_LEGACY

#include <GL/glx.h>
#include "official/glxext.h"

#endif

#if defined(RENDERDOC_SUPPORT_GLES)

// force include the elgplatform.h, as we want to use
// our own because the system one could be a bit older and
// propably not suitable for the given egl.h
#include "official/eglplatform.h"

#include "official/egl.h"
#include "official/eglext.h"
#endif

struct GLWindowingData
{
  GLWindowingData()
  {
    dpy = NULL;
    ctx = NULL;
    wnd = (GLWindowPtr)NULL;
    egl_wnd = (GLESWindowPtr)NULL;
    cfg = NULL;
  }

#if defined(RENDERDOC_SUPPORT_GL)
  typedef Display *GLDisplayPtr;
  typedef GLXContext GLContextPtr;
  typedef GLXDrawable GLWindowPtr;
  typedef XVisualInfo *GLConfigPtr;
#else
  typedef void *GLDisplayPtr;
  typedef void *GLContextPtr;
  typedef void *GLWindowPtr;
  typedef void *GLConfigPtr;
#endif

#if defined(RENDERDOC_SUPPORT_GLES)
  typedef EGLDisplay GLESDisplayPtr;
  typedef EGLContext GLESContextPtr;
  typedef EGLSurface GLESWindowPtr;
  typedef EGLConfig GLESConfigPtr;
#else
  typedef void *GLESDisplayPtr;
  typedef void *GLESContextPtr;
  typedef void *GLESWindowPtr;
  typedef void *GLESConfigPtr;
#endif

  union
  {
    GLDisplayPtr dpy;
    GLESDisplayPtr egl_dpy;
  };
  union
  {
    GLContextPtr ctx;
    GLESContextPtr egl_ctx;
  };
  union
  {
    GLConfigPtr cfg;
    GLESConfigPtr egl_cfg;
  };
  GLWindowPtr wnd;
  GLESWindowPtr egl_wnd;
};

#elif ENABLED(RDOC_APPLE)

#include "official/cgl.h"

struct GLWindowingData
{
  GLWindowingData()
  {
    ctx = NULL;
    wnd = NULL;
    pix = NULL;

    layer = NULL;
  }

  union
  {
    CGLContextObj ctx;
    void *nsctx;    // during replay only, this is the NSOpenGLContext
  };

  void *wnd;    // during capture, this is the CGL window ID. During replay, it's the NSView
  CGLPixelFormatObj pix;

  void *layer;    // during replay only, this is the CALayer
};

#define DECL_HOOK_EXPORT(function)                                                                    \
  __attribute__((used)) static struct                                                                 \
  {                                                                                                   \
    const void *replacment;                                                                           \
    const void *replacee;                                                                             \
  } _interpose_def_##function __attribute__((section("__DATA,__interpose"))) = {                      \
      (const void *)(unsigned long)&GL_EXPORT_NAME(function), (const void *)(unsigned long)&function, \
  };

#elif ENABLED(RDOC_ANDROID)

// force include the eglplatform.h, as we want to use
// our own because the system one could be a bit older and
// propably not suitable for the given egl.h
#include "official/eglplatform.h"

#include "official/egl.h"
#include "official/eglext.h"

struct GLWindowingData
{
  GLWindowingData()
  {
    egl_ctx = NULL;
    egl_dpy = NULL;
    wnd = NULL;
    egl_wnd = NULL;
    egl_cfg = NULL;
  }

  union
  {
    // currently required to allow compatiblity with the driver parts
    void *ctx;
    EGLContext egl_ctx;
  };
  EGLSurface egl_wnd;
  void *wnd;
  EGLDisplay egl_dpy;
  EGLConfig egl_cfg;
};

#else
#error "Unknown platform"
#endif

#include "api/replay/renderdoc_replay.h"

struct GLPlatform
{
  // simple wrapper for OS functions to make/delete a context
  virtual GLWindowingData CloneTemporaryContext(GLWindowingData share) = 0;
  virtual void DeleteClonedContext(GLWindowingData context) = 0;
  virtual void DeleteReplayContext(GLWindowingData context) = 0;
  virtual bool MakeContextCurrent(GLWindowingData data) = 0;
  virtual void SwapBuffers(GLWindowingData context) = 0;
  virtual void WindowResized(GLWindowingData context) = 0;
  virtual void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h) = 0;
  virtual bool IsOutputWindowVisible(GLWindowingData context) = 0;
  virtual GLWindowingData MakeOutputWindow(WindowingData window, bool depth,
                                           GLWindowingData share_context) = 0;

  // for 'backwards compatible' overlay rendering
  virtual void DrawQuads(float width, float height, const std::vector<Vec4f> &vertices) = 0;

  // for initialisation at replay time
  virtual bool CanCreateGLESContext() = 0;
  virtual bool PopulateForReplay() = 0;
  virtual ReplayStatus InitialiseAPI(GLWindowingData &replayContext, RDCDriver api) = 0;
  virtual void *GetReplayFunction(const char *funcname) = 0;
};

class GLDummyPlatform : public GLPlatform
{
  virtual GLWindowingData CloneTemporaryContext(GLWindowingData share) { return GLWindowingData(); }
  virtual void DeleteClonedContext(GLWindowingData context) {}
  virtual void DeleteReplayContext(GLWindowingData context) {}
  virtual bool MakeContextCurrent(GLWindowingData data) { return true; }
  virtual void SwapBuffers(GLWindowingData context) {}
  virtual void WindowResized(GLWindowingData context) {}
  virtual void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h) {}
  virtual bool IsOutputWindowVisible(GLWindowingData context) { return false; }
  virtual GLWindowingData MakeOutputWindow(WindowingData window, bool depth,
                                           GLWindowingData share_context)
  {
    return GLWindowingData();
  }
  virtual void DrawQuads(float width, float height, const std::vector<Vec4f> &vertices) {}
  virtual void *GetReplayFunction(const char *funcname) { return NULL; }
  // for initialisation at replay time
  virtual bool CanCreateGLESContext() { return true; }
  virtual bool PopulateForReplay() { return true; }
  virtual ReplayStatus InitialiseAPI(GLWindowingData &replayContext, RDCDriver api)
  {
    return ReplayStatus::Succeeded;
  }
};

struct GLVersion
{
  int major;
  int minor;
};

std::vector<GLVersion> GetReplayVersions(RDCDriver api);

#if defined(RENDERDOC_SUPPORT_GL)

// platform specific (WGL, GLX, etc)
GLPlatform &GetGLPlatform();

#endif

#if defined(RENDERDOC_SUPPORT_GLES)

// using EGL. Different name since it both platform GL and EGL libraries can be available at once
GLPlatform &GetEGLPlatform();

#endif

// Win32 doesn't need to export GL functions, but we still want them as they are our hook
// implementations.
#if ENABLED(RDOC_WIN32)

#define HOOK_EXPORT extern "C"
#define HOOK_CC WINAPI

#else

#define HOOK_EXPORT extern "C" RENDERDOC_EXPORT_API
#define HOOK_CC

#endif

// on macOS we used compile time interposing to hook
#if ENABLED(RDOC_APPLE)

// never declare the actual raw function name as an export, declare the functions with a suffix that
// will be connected in the struct below
#define GL_EXPORT_NAME(function) CONCAT(interposed_, function)

#else

// on all other platforms we just export functions with the bare name
#define GL_EXPORT_NAME(function) function

#endif

class RDCFile;
class IReplayDriver;

// define stubs so other platforms can define these functions, but empty
#if DISABLED(RDOC_WIN32)
typedef void *HANDLE;
typedef long BOOL;

typedef BOOL(APIENTRY *PFNWGLDXSETRESOURCESHAREHANDLENVPROC)(void *dxObject, HANDLE shareHandle);
typedef HANDLE(APIENTRY *PFNWGLDXOPENDEVICENVPROC)(void *dxDevice);
typedef BOOL(APIENTRY *PFNWGLDXCLOSEDEVICENVPROC)(HANDLE hDevice);
typedef HANDLE(APIENTRY *PFNWGLDXREGISTEROBJECTNVPROC)(HANDLE hDevice, void *dxObject, GLuint name,
                                                       GLenum type, GLenum access);
typedef BOOL(APIENTRY *PFNWGLDXUNREGISTEROBJECTNVPROC)(HANDLE hDevice, HANDLE hObject);
typedef BOOL(APIENTRY *PFNWGLDXOBJECTACCESSNVPROC)(HANDLE hObject, GLenum access);
typedef BOOL(APIENTRY *PFNWGLDXLOCKOBJECTSNVPROC)(HANDLE hDevice, GLint count, HANDLE *hObjects);
typedef BOOL(APIENTRY *PFNWGLDXUNLOCKOBJECTSNVPROC)(HANDLE hDevice, GLint count, HANDLE *hObjects);
#endif

#include "api/replay/renderdoc_replay.h"

// bit of a hack, to work around C4127: conditional expression is constant
// on template parameters
template <typename T>
T CheckConstParam(T t);

// define this if you e.g. haven't compiled the D3D modules and want to disable
// interop capture support.
#define RENDERDOC_DX_GL_INTEROP OPTION_ON

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the
// debugbreak.
#define GLNOTIMP(...) RDCDEBUG("OpenGL not implemented - " __VA_ARGS__)

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                              \
  template <typename SerialiserType>                  \
  bool CONCAT(Serialise_, func)(SerialiserType & ser, ##__VA_ARGS__);

#define INSTANTIATE_FUNCTION_SERIALISED(ret, func, ...)                                      \
  template bool WrappedOpenGL::CONCAT(Serialise_, func(ReadSerialiser &ser, ##__VA_ARGS__)); \
  template bool WrappedOpenGL::CONCAT(Serialise_, func(WriteSerialiser &ser, ##__VA_ARGS__));

#define USE_SCRATCH_SERIALISER() WriteSerialiser &ser = m_ScratchSerialiser;

#define SERIALISE_TIME_CALL(...)                                                                    \
  m_ScratchSerialiser.ChunkMetadata().timestampMicro = RenderDoc::Inst().GetMicrosecondTimestamp(); \
  __VA_ARGS__;                                                                                      \
  m_ScratchSerialiser.ChunkMetadata().durationMicro =                                               \
      RenderDoc::Inst().GetMicrosecondTimestamp() -                                                 \
      m_ScratchSerialiser.ChunkMetadata().timestampMicro;

// A handy macros to say "is the serialiser reading and we're doing replay-mode stuff?"
// The reason we check both is that checking the first allows the compiler to eliminate the other
// path at compile-time, and the second because we might be just struct-serialising in which case we
// should be doing no work to restore states.
// Writing is unambiguously during capture mode, so we don't have to check both in that case.
#define IsReplayingAndReading() (ser.IsReading() && IsReplayMode(m_State))

// on GL we don't have an easy way of checking which functions/extensions were used or which
// functions/extensions on replay could suffice. So we check at the last minute on replay and bail
// out if it's not present
#define CheckReplayFunctionPresent(func)                         \
  if(func == NULL)                                               \
  {                                                              \
    m_FailedReplayStatus = ReplayStatus::APIHardwareUnsupported; \
    return false;                                                \
  }

// no longer in glcorearb.h or glext.h
const GLenum eGL_LIGHTING = (GLenum)0x0B50;
const GLenum eGL_ALPHA_TEST = (GLenum)0x0BC0;
const GLenum eGL_CLAMP = (GLenum)0x2900;

// convenience, the script to pick these out doesn't find them since they are #define'd
// to just straight integers not hex codes
const GLenum eGL_ZERO = (GLenum)0;
const GLenum eGL_ONE = (GLenum)1;

extern Threading::CriticalSection glLock;

// replay only class for handling marker regions
struct GLMarkerRegion
{
  GLMarkerRegion(const std::string &marker, GLenum source = eGL_DEBUG_SOURCE_APPLICATION,
                 GLuint id = 0);
  ~GLMarkerRegion();

  static void Begin(const std::string &marker, GLenum source = eGL_DEBUG_SOURCE_APPLICATION,
                    GLuint id = 0);
  static void Set(const std::string &marker, GLenum source = eGL_DEBUG_SOURCE_APPLICATION,
                  GLuint id = 0, GLenum severity = eGL_DEBUG_SEVERITY_NOTIFICATION);
  static void End();
};

// TODO ideally we'd support the full state vector, and only fetch&restore state we actually change
// - i.e. change state through here, and track dirty bits.
struct GLPushPopState
{
  bool enableBits[8];
  GLenum ClipOrigin, ClipDepth;
  GLenum EquationRGB, EquationAlpha;
  GLenum SourceRGB, SourceAlpha;
  GLenum DestinationRGB, DestinationAlpha;
  GLenum PolygonMode;
  GLfloat Viewportf[4];
  GLint Viewport[4];
  GLenum ActiveTexture;
  GLuint tex0;
  GLuint arraybuf;
  struct IndexedBuffer
  {
    GLuint buf;
    GLuint64 offs;
    GLuint64 size;
  } idxubo[3];
  GLuint ubo;
  GLuint prog;
  GLuint pipe;
  GLuint VAO;
  GLuint drawFBO;

  GLboolean ColorMask[4];

  // if the current context wasn't created with CreateContextAttribs we do an immediate mode render,
  // so fewer states are pushed/popped.
  // Note we don't assume a 1.0 context since that would be painful to handle. Instead we just skip
  // bits of state we're not going to mess with. In some cases this might cause problems e.g. we
  // don't use indexed enable states for blend and scissor test because we're assuming there's no
  // separate blending.
  //
  // In the end, this is just a best-effort to keep going without crashing. Old GL versions aren't
  // supported.
  void Push(bool modern);
  void Pop(bool modern);
};

template <class DispatchTable>
void DrawQuads(DispatchTable &GL, float width, float height, const std::vector<Vec4f> &vertices)
{
  const GLenum GL_MATRIX_MODE = (GLenum)0x0BA0;
  const GLenum GL_MODELVIEW = (GLenum)0x1700;
  const GLenum GL_PROJECTION = (GLenum)0x1701;

  GLenum prevMatMode = eGL_NONE;
  GL.glGetIntegerv(GL_MATRIX_MODE, (GLint *)&prevMatMode);

  GL.glMatrixMode(GL_PROJECTION);
  GL.glPushMatrix();
  GL.glLoadIdentity();
  GL.glOrtho(0.0, width, height, 0.0, -1.0, 1.0);

  GL.glMatrixMode(GL_MODELVIEW);
  GL.glPushMatrix();
  GL.glLoadIdentity();

  GL.glBegin(eGL_QUADS);

  for(size_t i = 0; i < vertices.size(); i++)
  {
    GL.glTexCoord2f(vertices[i].z, vertices[i].w);
    GL.glVertex2f(vertices[i].x, vertices[i].y);
  }

  GL.glEnd();

  GL.glMatrixMode(GL_PROJECTION);
  GL.glPopMatrix();
  GL.glMatrixMode(GL_MODELVIEW);
  GL.glPopMatrix();

  GL.glMatrixMode(prevMatMode);
}

size_t GLTypeSize(GLenum type);

size_t BufferIdx(GLenum buf);
GLenum BufferEnum(size_t idx);

size_t TextureIdx(GLenum buf);
GLenum TextureEnum(size_t idx);

size_t QueryIdx(GLenum query);
GLenum QueryEnum(size_t idx);

size_t ShaderIdx(GLenum buf);
GLenum ShaderBit(size_t idx);
GLenum ShaderEnum(size_t idx);

ResourceFormat MakeResourceFormat(GLenum target, GLenum fmt);
GLenum MakeGLFormat(ResourceFormat fmt);
Topology MakePrimitiveTopology(GLenum Topo);
GLenum MakeGLPrimitiveTopology(Topology Topo);
BufferCategory MakeBufferCategory(GLenum bufferTarget);
AddressMode MakeAddressMode(GLenum addr);
TextureFilter MakeFilter(GLenum minf, GLenum magf, bool shadowSampler, float maxAniso);
ShaderStage MakeShaderStage(GLenum type);
CompareFunction MakeCompareFunc(GLenum func);
StencilOperation MakeStencilOp(GLenum op);
LogicOperation MakeLogicOp(GLenum op);
BlendMultiplier MakeBlendMultiplier(GLenum blend);
BlendOperation MakeBlendOp(GLenum op);

void ClearGLErrors();

GLuint GetBoundVertexBuffer(GLuint idx);
GLint GetNumVertexBuffers();

void EvaluateSPIRVBindpointMapping(GLuint curProg, int shadIdx, const ShaderReflection *refl,
                                   ShaderBindpointMapping &mapping);

void GetBindpointMapping(GLuint curProg, int shadIdx, const ShaderReflection *refl,
                         ShaderBindpointMapping &mapping);

void ResortBindings(ShaderReflection *refl, ShaderBindpointMapping *mapping);

// calls glBlitFramebuffer but ensures no state can interfere like scissor or color mask
// pops state for only a single drawbuffer!
void SafeBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
                         GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

enum UniformType
{
  UNIFORM_UNKNOWN,

  VEC1fv,
  VEC1iv,
  VEC1uiv,
  VEC1dv,

  VEC2fv,
  VEC2iv,
  VEC2uiv,
  VEC2dv,

  VEC3fv,
  VEC3iv,
  VEC3uiv,
  VEC3dv,

  VEC4fv,
  VEC4iv,
  VEC4uiv,
  VEC4dv,

  MAT2fv,
  MAT2x3fv,
  MAT2x4fv,
  MAT3fv,
  MAT3x2fv,
  MAT3x4fv,
  MAT4fv,
  MAT4x2fv,
  MAT4x3fv,

  MAT2dv,
  MAT2x3dv,
  MAT2x4dv,
  MAT3dv,
  MAT3x2dv,
  MAT3x4dv,
  MAT4dv,
  MAT4x2dv,
  MAT4x3dv,
};

DECLARE_REFLECTION_ENUM(UniformType);

enum AttribType
{
  Attrib_GLdouble = 0x01,
  Attrib_GLfloat = 0x02,
  Attrib_GLshort = 0x03,
  Attrib_GLushort = 0x04,
  Attrib_GLbyte = 0x05,
  Attrib_GLubyte = 0x06,
  Attrib_GLint = 0x07,
  Attrib_GLuint = 0x08,
  Attrib_packed = 0x09,
  Attrib_typemask = 0x0f,

  Attrib_L = 0x10,
  Attrib_I = 0x20,
  Attrib_N = 0x40,
};

DECLARE_REFLECTION_ENUM(AttribType);

enum GLframebufferbitfield
{
};

DECLARE_REFLECTION_ENUM(GLframebufferbitfield);

extern int GLCoreVersion;
extern bool GLIsCore;
extern bool IsGLES;

// List of extensions and the versions when they became core (first column for GL, second column for
// GLES). In case of GLES compatible extensions and new features of the different versions are also
// taken into account.
// 99 means the extension never became core, so you can easily just do a check of CoreVersion >= NN
// and they will always fail.
#define EXTENSION_CHECKS()                                       \
  EXT_TO_CHECK(13, 99, ARB_texture_border_clamp)                 \
  EXT_TO_CHECK(30, 30, EXT_transform_feedback)                   \
  EXT_TO_CHECK(30, 32, EXT_draw_buffers2)                        \
  EXT_TO_CHECK(31, 99, EXT_framebuffer_sRGB)                     \
  EXT_TO_CHECK(31, 32, ARB_texture_buffer_object)                \
  /* This is a hack, the extension doesn't exist but is      */  \
  /* equivalent to GLES 3.1's addition of MSAA textures but  */  \
  /* NOT array MSAA textures. We'll treat the real ext as a  */  \
  /* super-set.                                              */  \
  EXT_TO_CHECK(32, 31, ARB_texture_multisample_no_array)         \
  EXT_TO_CHECK(32, 32, ARB_texture_multisample)                  \
  EXT_TO_CHECK(33, 30, ARB_explicit_attrib_location)             \
  EXT_TO_CHECK(33, 30, ARB_sampler_objects)                      \
  EXT_TO_CHECK(33, 30, ARB_texture_swizzle)                      \
  EXT_TO_CHECK(33, 30, ARB_occlusion_query2)                     \
  EXT_TO_CHECK(40, 32, ARB_draw_buffers_blend)                   \
  EXT_TO_CHECK(40, 31, ARB_draw_indirect)                        \
  EXT_TO_CHECK(40, 32, ARB_gpu_shader5)                          \
  EXT_TO_CHECK(40, 32, ARB_sample_shading)                       \
  EXT_TO_CHECK(40, 99, ARB_shader_subroutine)                    \
  EXT_TO_CHECK(40, 99, ARB_gpu_shader_fp64)                      \
  EXT_TO_CHECK(40, 32, ARB_tessellation_shader)                  \
  EXT_TO_CHECK(40, 32, ARB_texture_cube_map_array)               \
  EXT_TO_CHECK(40, 30, ARB_transform_feedback2)                  \
  EXT_TO_CHECK(32, 32, ARB_geometry_shader4)                     \
  EXT_TO_CHECK(41, 31, ARB_separate_shader_objects)              \
  EXT_TO_CHECK(41, 99, ARB_viewport_array)                       \
  EXT_TO_CHECK(41, 99, ARB_ES2_compatibility)                    \
  EXT_TO_CHECK(41, 99, ARB_vertex_attrib_64bit)                  \
  EXT_TO_CHECK(42, 99, ARB_base_instance)                        \
  EXT_TO_CHECK(42, 31, ARB_shader_atomic_counters)               \
  EXT_TO_CHECK(42, 31, ARB_shader_image_load_store)              \
  EXT_TO_CHECK(42, 31, ARB_shading_language_420pack)             \
  EXT_TO_CHECK(42, 30, ARB_texture_storage)                      \
  EXT_TO_CHECK(43, 99, ARB_clear_buffer_object)                  \
  EXT_TO_CHECK(43, 32, ARB_texture_buffer_range)                 \
  EXT_TO_CHECK(43, 31, ARB_compute_shader)                       \
  EXT_TO_CHECK(43, 32, ARB_copy_image)                           \
  EXT_TO_CHECK(43, 30, ARB_ES3_compatibility)                    \
  EXT_TO_CHECK(43, 99, ARB_internalformat_query2)                \
  EXT_TO_CHECK(43, 31, ARB_program_interface_query)              \
  EXT_TO_CHECK(43, 31, ARB_shader_storage_buffer_object)         \
  EXT_TO_CHECK(43, 31, ARB_stencil_texturing)                    \
  /* See above with ARB_texture_multisample_no_array          */ \
  EXT_TO_CHECK(43, 32, ARB_texture_storage_multisample_no_array) \
  EXT_TO_CHECK(43, 99, ARB_texture_storage_multisample)          \
  EXT_TO_CHECK(43, 99, ARB_texture_view)                         \
  EXT_TO_CHECK(43, 31, ARB_vertex_attrib_binding)                \
  EXT_TO_CHECK(43, 32, KHR_debug)                                \
  EXT_TO_CHECK(44, 99, ARB_enhanced_layouts)                     \
  EXT_TO_CHECK(44, 99, ARB_query_buffer_object)                  \
  EXT_TO_CHECK(45, 99, ARB_clip_control)                         \
  EXT_TO_CHECK(45, 99, ARB_direct_state_access)                  \
  EXT_TO_CHECK(45, 99, ARB_derivative_control)                   \
  EXT_TO_CHECK(46, 99, ARB_polygon_offset_clamp)                 \
  EXT_TO_CHECK(46, 99, ARB_texture_filter_anisotropic)           \
  EXT_TO_CHECK(46, 99, ARB_pipeline_statistics_query)            \
  EXT_TO_CHECK(46, 99, ARB_gl_spirv)                             \
  EXT_TO_CHECK(99, 99, ARB_indirect_parameters)                  \
  EXT_TO_CHECK(99, 99, ARB_seamless_cubemap_per_texture)         \
  EXT_TO_CHECK(99, 99, EXT_depth_bounds_test)                    \
  EXT_TO_CHECK(99, 99, EXT_direct_state_access)                  \
  EXT_TO_CHECK(99, 99, EXT_raster_multisample)                   \
  EXT_TO_CHECK(99, 30, EXT_texture_swizzle)                      \
  EXT_TO_CHECK(99, 99, KHR_blend_equation_advanced_coherent)     \
  EXT_TO_CHECK(99, 99, EXT_texture_sRGB_decode)                  \
  EXT_TO_CHECK(99, 99, INTEL_performance_query)                  \
  EXT_TO_CHECK(99, 99, EXT_texture_buffer)                       \
  /* OpenGL ES extensions */                                     \
  EXT_TO_CHECK(99, 32, EXT_color_buffer_float)                   \
  EXT_TO_CHECK(99, 32, EXT_primitive_bounding_box)               \
  EXT_TO_CHECK(99, 32, OES_primitive_bounding_box)               \
  EXT_TO_CHECK(99, 32, OES_texture_border_color)                 \
  EXT_TO_CHECK(99, 99, EXT_texture_cube_map_array)               \
  EXT_TO_CHECK(99, 99, OES_texture_cube_map_array)               \
  EXT_TO_CHECK(99, 32, OES_texture_storage_multisample_2d_array) \
  EXT_TO_CHECK(99, 99, EXT_clip_cull_distance)                   \
  EXT_TO_CHECK(99, 99, EXT_multisample_compatibility)            \
  EXT_TO_CHECK(99, 99, EXT_read_format_bgra)                     \
  EXT_TO_CHECK(99, 99, EXT_texture_format_BGRA8888)              \
  EXT_TO_CHECK(99, 99, NV_polygon_mode)                          \
  EXT_TO_CHECK(99, 99, NV_read_depth)                            \
  EXT_TO_CHECK(99, 99, NV_read_stencil)                          \
  EXT_TO_CHECK(99, 99, NV_read_depth_stencil)                    \
  EXT_TO_CHECK(99, 99, EXT_disjoint_timer_query)                 \
  EXT_TO_CHECK(99, 99, EXT_multisampled_render_to_texture)       \
  EXT_TO_CHECK(99, 99, OVR_multiview)

// GL extensions equivalents
// Either promoted extensions from EXT to ARB, or
// desktop extensions and their roughly equivalent GLES alternatives
#define EXTENSION_COMPATIBILITY_CHECKS()                                                    \
  EXT_COMP_CHECK(ARB_texture_border_clamp, OES_texture_border_color)                        \
  EXT_COMP_CHECK(ARB_polygon_offset_clamp, EXT_polygon_offset_clamp)                        \
  EXT_COMP_CHECK(ARB_texture_filter_anisotropic, EXT_texture_filter_anisotropic)            \
  EXT_COMP_CHECK(ARB_base_instance, EXT_base_instance)                                      \
  EXT_COMP_CHECK(ARB_copy_image, EXT_copy_image)                                            \
  EXT_COMP_CHECK(ARB_copy_image, OES_copy_image)                                            \
  EXT_COMP_CHECK(ARB_draw_buffers_blend, EXT_draw_buffers_indexed)                          \
  EXT_COMP_CHECK(ARB_draw_buffers_blend, OES_draw_buffers_indexed)                          \
  EXT_COMP_CHECK(ARB_geometry_shader4, EXT_geometry_shader)                                 \
  EXT_COMP_CHECK(ARB_geometry_shader4, OES_geometry_shader)                                 \
  EXT_COMP_CHECK(ARB_gpu_shader5, EXT_gpu_shader5)                                          \
  EXT_COMP_CHECK(ARB_gpu_shader5, OES_gpu_shader5)                                          \
  EXT_COMP_CHECK(ARB_sample_shading, OES_sample_shading)                                    \
  EXT_COMP_CHECK(ARB_separate_shader_objects, EXT_separate_shader_objects)                  \
  EXT_COMP_CHECK(ARB_tessellation_shader, EXT_tessellation_shader)                          \
  EXT_COMP_CHECK(ARB_tessellation_shader, OES_tessellation_shader)                          \
  EXT_COMP_CHECK(ARB_texture_cube_map_array, EXT_texture_cube_map_array)                    \
  EXT_COMP_CHECK(ARB_texture_cube_map_array, OES_texture_cube_map_array)                    \
  EXT_COMP_CHECK(ARB_texture_storage, EXT_texture_storage)                                  \
  EXT_COMP_CHECK(ARB_texture_storage_multisample, OES_texture_storage_multisample_2d_array) \
  EXT_COMP_CHECK(ARB_texture_view, EXT_texture_view)                                        \
  EXT_COMP_CHECK(ARB_texture_view, OES_texture_view)                                        \
  EXT_COMP_CHECK(ARB_viewport_array, NV_viewport_array)                                     \
  EXT_COMP_CHECK(ARB_viewport_array, OES_viewport_array)                                    \
  EXT_COMP_CHECK(ARB_texture_buffer_object, EXT_texture_buffer)                             \
  EXT_COMP_CHECK(ARB_texture_buffer_object, OES_texture_buffer)                             \
  EXT_COMP_CHECK(ARB_texture_buffer_range, EXT_texture_buffer)                              \
  EXT_COMP_CHECK(ARB_texture_buffer_range, OES_texture_buffer)                              \
  EXT_COMP_CHECK(EXT_framebuffer_sRGB, EXT_sRGB_write_control)

// extensions we know we want to check for are precached, indexd by this enum
enum ExtensionCheckEnum
{
#undef EXT_TO_CHECK
#define EXT_TO_CHECK(ver, glesver, ext) ext,
  EXTENSION_CHECKS()

      GLExtension_Count,
};
extern bool HasExt[GLExtension_Count];

// for some things we need to know how a specific implementation behaves to work around it
// or adjust things. We centralise that here (similar to extensions)
enum VendorCheckEnum
{
  VendorCheck_AMD_vertex_buffer_query,
  VendorCheck_NV_avoid_D32S8_copy,
  VendorCheck_EXT_fbo_shared,
  VendorCheck_EXT_vao_shared,
  VendorCheck_AMD_polygon_mode_query,
  VendorCheck_AMD_copy_compressed_tinymips,
  VendorCheck_AMD_pipeline_compute_query,
  VendorCheck_NV_ClearNamedFramebufferfiBugs,
  VendorCheck_AMD_copy_compressed_cubemaps,
  VendorCheck_AMD_vertex_array_elem_buffer_query,
  VendorCheck_Qualcomm_avoid_glCopyImageSubData,
  VendorCheck_Count,
};
extern bool VendorCheck[VendorCheck_Count];

// fills out the extension supported array and the version-specific checks above
void DoVendorChecks(GLPlatform &platform, GLWindowingData context);
void GetContextVersion(bool &ctxGLES, int &ctxVersion);
void FetchEnabledExtensions();

// verify that we got a replay context that we can work with
bool CheckReplayContext();
bool ValidateFunctionPointers();

#include "core/core.h"
#include "serialise/serialiser.h"

struct ShaderReflection;

struct PerStageReflections
{
  const ShaderReflection *refls[6] = {};
  const ShaderBindpointMapping *mappings[6] = {};
};

void CopyProgramUniforms(const PerStageReflections &srcStages, GLuint progSrc,
                         const PerStageReflections &dstStages, GLuint progDst);
template <typename SerialiserType>
void SerialiseProgramUniforms(SerialiserType &ser, CaptureState state,
                              const PerStageReflections &stages, GLuint prog,
                              std::map<GLint, GLint> *locTranslate);
bool CopyProgramAttribBindings(GLuint progsrc, GLuint progdst, ShaderReflection *refl);
bool CopyProgramFragDataBindings(GLuint progsrc, GLuint progdst, ShaderReflection *refl);
template <typename SerialiserType>
bool SerialiseProgramBindings(SerialiserType &ser, CaptureState state,
                              const PerStageReflections &stages, GLuint prog);

struct DrawElementsIndirectCommand
{
  uint32_t count;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t baseVertex;
  uint32_t baseInstance;
};

DECLARE_REFLECTION_STRUCT(DrawElementsIndirectCommand);

struct DrawArraysIndirectCommand
{
  uint32_t count;
  uint32_t instanceCount;
  uint32_t first;
  uint32_t baseInstance;
};

DECLARE_REFLECTION_STRUCT(DrawArraysIndirectCommand);

enum class GLChunk : uint32_t
{
  DeviceInitialisation = (uint32_t)SystemChunk::FirstDriverChunk,

  MakeContextCurrent,

  vrapi_CreateTextureSwapChain,
  vrapi_CreateTextureSwapChain2,

  // we can't use the hookset macro to define the chunk list, because then it would be impossible to
  // add an alias in the middle of the list of functions. Instead we expand it here by hand, and so
  // any new functions or aliases can be added to the end of the list.
  glBindTexture,
  glBlendFunc,
  glClear,
  glClearColor,
  glClearDepth,
  glClearStencil,
  glColorMask,
  glCullFace,
  glDepthFunc,
  glDepthMask,
  glDepthRange,
  glStencilFunc,
  glStencilMask,
  glStencilOp,
  glDisable,
  glDrawBuffer,
  glDrawElements,
  glDrawArrays,
  glEnable,
  glFlush,
  glFinish,
  glFrontFace,
  glGenTextures,
  glDeleteTextures,
  glIsEnabled,
  glIsTexture,
  glGetError,
  glGetTexLevelParameteriv,
  glGetTexLevelParameterfv,
  glGetTexParameterfv,
  glGetTexParameteriv,
  glGetTexImage,
  glGetBooleanv,
  glGetFloatv,
  glGetDoublev,
  glGetIntegerv,
  glGetPointerv,
  glGetPointervKHR,
  glGetString,
  glHint,
  glLogicOp,
  glPixelStorei,
  glPixelStoref,
  glPolygonMode,
  glPolygonOffset,
  glPointSize,
  glLineWidth,
  glReadPixels,
  glReadBuffer,
  glScissor,
  glTexImage1D,
  glTexImage2D,
  glTexSubImage1D,
  glTexSubImage2D,
  glCopyTexImage1D,
  glCopyTexImage2D,
  glCopyTexSubImage1D,
  glCopyTexSubImage2D,
  glTexParameterf,
  glTexParameterfv,
  glTexParameteri,
  glTexParameteriv,
  glViewport,
  glActiveTexture,
  glActiveTextureARB,
  glTexStorage1D,
  glTexStorage1DEXT,
  glTexStorage2D,
  glTexStorage2DEXT,
  glTexStorage3D,
  glTexStorage3DEXT,
  glTexStorage2DMultisample,
  glTexStorage3DMultisample,
  glTexStorage3DMultisampleOES,
  glTexImage3D,
  glTexImage3DEXT,
  glTexImage3DOES,
  glTexSubImage3D,
  glTexSubImage3DOES,
  glTexBuffer,
  glTexBufferARB,
  glTexBufferEXT,
  glTexBufferOES,
  glTexImage2DMultisample,
  glTexImage3DMultisample,
  glCompressedTexImage1D,
  glCompressedTexImage1DARB,
  glCompressedTexImage2D,
  glCompressedTexImage2DARB,
  glCompressedTexImage3D,
  glCompressedTexImage3DARB,
  glCompressedTexImage3DOES,
  glCompressedTexSubImage1D,
  glCompressedTexSubImage1DARB,
  glCompressedTexSubImage2D,
  glCompressedTexSubImage2DARB,
  glCompressedTexSubImage3D,
  glCompressedTexSubImage3DARB,
  glCompressedTexSubImage3DOES,
  glTexBufferRange,
  glTexBufferRangeEXT,
  glTexBufferRangeOES,
  glTextureView,
  glTextureViewEXT,
  glTextureViewOES,
  glTexParameterIiv,
  glTexParameterIivEXT,
  glTexParameterIivOES,
  glTexParameterIuiv,
  glTexParameterIuivEXT,
  glTexParameterIuivOES,
  glGenerateMipmap,
  glGenerateMipmapEXT,
  glCopyImageSubData,
  glCopyImageSubDataEXT,
  glCopyImageSubDataOES,
  glCopyTexSubImage3D,
  glCopyTexSubImage3DOES,
  glGetInternalformativ,
  glGetInternalformati64v,
  glGetBufferParameteriv,
  glGetBufferParameterivARB,
  glGetBufferParameteri64v,
  glGetBufferPointerv,
  glGetBufferPointervARB,
  glGetBufferPointervOES,
  glGetFragDataIndex,
  glGetFragDataLocation,
  glGetFragDataLocationEXT,
  glGetStringi,
  glGetBooleani_v,
  glGetIntegeri_v,
  glGetFloati_v,
  glGetFloati_vEXT,
  glGetFloati_vOES,
  glGetFloati_vNV,
  glGetDoublei_v,
  glGetDoublei_vEXT,
  glGetInteger64i_v,
  glGetInteger64v,
  glGetShaderiv,
  glGetShaderInfoLog,
  glGetShaderPrecisionFormat,
  glGetShaderSource,
  glGetAttachedShaders,
  glGetProgramiv,
  glGetProgramInfoLog,
  glGetProgramInterfaceiv,
  glGetProgramResourceIndex,
  glGetProgramResourceiv,
  glGetProgramResourceName,
  glGetProgramPipelineiv,
  glGetProgramPipelineivEXT,
  glGetProgramPipelineInfoLog,
  glGetProgramPipelineInfoLogEXT,
  glGetProgramBinary,
  glGetProgramResourceLocation,
  glGetProgramResourceLocationIndex,
  glGetProgramStageiv,
  glGetGraphicsResetStatus,
  glGetGraphicsResetStatusARB,
  glGetGraphicsResetStatusEXT,
  glGetObjectLabel,
  glGetObjectLabelKHR,
  glGetObjectLabelEXT,
  glGetObjectPtrLabel,
  glGetObjectPtrLabelKHR,
  glGetDebugMessageLog,
  glGetDebugMessageLogARB,
  glGetDebugMessageLogKHR,
  glGetFramebufferAttachmentParameteriv,
  glGetFramebufferAttachmentParameterivEXT,
  glGetFramebufferParameteriv,
  glGetRenderbufferParameteriv,
  glGetRenderbufferParameterivEXT,
  glGetMultisamplefv,
  glGetQueryIndexediv,
  glGetQueryObjectui64v,
  glGetQueryObjectui64vEXT,
  glGetQueryObjectuiv,
  glGetQueryObjectuivARB,
  glGetQueryObjectuivEXT,
  glGetQueryObjecti64v,
  glGetQueryObjecti64vEXT,
  glGetQueryObjectiv,
  glGetQueryObjectivARB,
  glGetQueryObjectivEXT,
  glGetQueryiv,
  glGetQueryivARB,
  glGetQueryivEXT,
  glGetSynciv,
  glGetBufferSubData,
  glGetBufferSubDataARB,
  glGetVertexAttribiv,
  glGetVertexAttribPointerv,
  glGetCompressedTexImage,
  glGetCompressedTexImageARB,
  glGetnCompressedTexImage,
  glGetnCompressedTexImageARB,
  glGetnTexImage,
  glGetnTexImageARB,
  glGetTexParameterIiv,
  glGetTexParameterIivEXT,
  glGetTexParameterIivOES,
  glGetTexParameterIuiv,
  glGetTexParameterIuivEXT,
  glGetTexParameterIuivOES,
  glClampColor,
  glClampColorARB,
  glReadnPixels,
  glReadnPixelsARB,
  glReadnPixelsEXT,
  glGetSamplerParameterIiv,
  glGetSamplerParameterIivEXT,
  glGetSamplerParameterIivOES,
  glGetSamplerParameterIuiv,
  glGetSamplerParameterIuivEXT,
  glGetSamplerParameterIuivOES,
  glGetSamplerParameterfv,
  glGetSamplerParameteriv,
  glGetTransformFeedbackVarying,
  glGetTransformFeedbackVaryingEXT,
  glGetSubroutineIndex,
  glGetSubroutineUniformLocation,
  glGetActiveAtomicCounterBufferiv,
  glGetActiveSubroutineName,
  glGetActiveSubroutineUniformName,
  glGetActiveSubroutineUniformiv,
  glGetUniformLocation,
  glGetUniformIndices,
  glGetUniformSubroutineuiv,
  glGetUniformBlockIndex,
  glGetAttribLocation,
  glGetActiveUniform,
  glGetActiveUniformName,
  glGetActiveUniformBlockName,
  glGetActiveUniformBlockiv,
  glGetActiveUniformsiv,
  glGetActiveAttrib,
  glGetUniformfv,
  glGetUniformiv,
  glGetUniformuiv,
  glGetUniformuivEXT,
  glGetUniformdv,
  glGetnUniformdv,
  glGetnUniformdvARB,
  glGetnUniformfv,
  glGetnUniformfvARB,
  glGetnUniformfvEXT,
  glGetnUniformiv,
  glGetnUniformivARB,
  glGetnUniformivEXT,
  glGetnUniformuiv,
  glGetnUniformuivARB,
  glGetVertexAttribIiv,
  glGetVertexAttribIivEXT,
  glGetVertexAttribIuiv,
  glGetVertexAttribIuivEXT,
  glGetVertexAttribLdv,
  glGetVertexAttribLdvEXT,
  glGetVertexAttribdv,
  glGetVertexAttribfv,
  glCheckFramebufferStatus,
  glCheckFramebufferStatusEXT,
  glBlendColor,
  glBlendColorEXT,
  glBlendFunci,
  glBlendFunciARB,
  glBlendFunciEXT,
  glBlendFunciOES,
  glBlendFuncSeparate,
  glBlendFuncSeparateARB,
  glBlendFuncSeparatei,
  glBlendFuncSeparateiARB,
  glBlendFuncSeparateiEXT,
  glBlendFuncSeparateiOES,
  glBlendEquation,
  glBlendEquationEXT,
  glBlendEquationi,
  glBlendEquationiARB,
  glBlendEquationiEXT,
  glBlendEquationiOES,
  glBlendEquationSeparate,
  glBlendEquationSeparateARB,
  glBlendEquationSeparateEXT,
  glBlendEquationSeparatei,
  glBlendEquationSeparateiARB,
  glBlendEquationSeparateiEXT,
  glBlendEquationSeparateiOES,
  glBlendBarrierKHR,
  glStencilFuncSeparate,
  glStencilMaskSeparate,
  glStencilOpSeparate,
  glColorMaski,
  glColorMaskiEXT,
  glColorMaskIndexedEXT,
  glColorMaskiOES,
  glSampleMaski,
  glSampleCoverage,
  glSampleCoverageARB,
  glMinSampleShading,
  glMinSampleShadingARB,
  glMinSampleShadingOES,
  glDepthRangef,
  glDepthRangeIndexed,
  glDepthRangeArrayv,
  glClipControl,
  glProvokingVertex,
  glProvokingVertexEXT,
  glPrimitiveRestartIndex,
  glCreateShader,
  glDeleteShader,
  glShaderSource,
  glCompileShader,
  glCreateShaderProgramv,
  glCreateShaderProgramvEXT,
  glCreateProgram,
  glDeleteProgram,
  glAttachShader,
  glDetachShader,
  glReleaseShaderCompiler,
  glLinkProgram,
  glProgramParameteri,
  glProgramParameteriARB,
  glProgramParameteriEXT,
  glUseProgram,
  glShaderBinary,
  glProgramBinary,
  glUseProgramStages,
  glUseProgramStagesEXT,
  glValidateProgram,
  glGenProgramPipelines,
  glGenProgramPipelinesEXT,
  glBindProgramPipeline,
  glBindProgramPipelineEXT,
  glActiveShaderProgram,
  glActiveShaderProgramEXT,
  glDeleteProgramPipelines,
  glDeleteProgramPipelinesEXT,
  glValidateProgramPipeline,
  glValidateProgramPipelineEXT,
  glDebugMessageCallback,
  glDebugMessageCallbackARB,
  glDebugMessageCallbackKHR,
  glDebugMessageControl,
  glDebugMessageControlARB,
  glDebugMessageControlKHR,
  glDebugMessageInsert,
  glDebugMessageInsertARB,
  glDebugMessageInsertKHR,
  glPushDebugGroup,
  glPushDebugGroupKHR,
  glPopDebugGroup,
  glPopDebugGroupKHR,
  glObjectLabel,
  glObjectLabelKHR,
  glLabelObjectEXT,
  glObjectPtrLabel,
  glObjectPtrLabelKHR,
  glEnablei,
  glEnableiEXT,
  glEnableIndexedEXT,
  glEnableiOES,
  glEnableiNV,
  glDisablei,
  glDisableiEXT,
  glDisableIndexedEXT,
  glDisableiOES,
  glDisableiNV,
  glIsEnabledi,
  glIsEnablediEXT,
  glIsEnabledIndexedEXT,
  glIsEnablediOES,
  glIsEnablediNV,
  glIsBuffer,
  glIsBufferARB,
  glIsFramebuffer,
  glIsFramebufferEXT,
  glIsProgram,
  glIsProgramPipeline,
  glIsProgramPipelineEXT,
  glIsQuery,
  glIsQueryARB,
  glIsQueryEXT,
  glIsRenderbuffer,
  glIsRenderbufferEXT,
  glIsSampler,
  glIsShader,
  glIsSync,
  glIsTransformFeedback,
  glIsVertexArray,
  glIsVertexArrayOES,
  glGenBuffers,
  glGenBuffersARB,
  glBindBuffer,
  glBindBufferARB,
  glDrawBuffers,
  glDrawBuffersARB,
  glDrawBuffersEXT,
  glGenFramebuffers,
  glGenFramebuffersEXT,
  glBindFramebuffer,
  glBindFramebufferEXT,
  glFramebufferTexture,
  glFramebufferTextureARB,
  glFramebufferTextureOES,
  glFramebufferTextureEXT,
  glFramebufferTexture1D,
  glFramebufferTexture1DEXT,
  glFramebufferTexture2D,
  glFramebufferTexture2DEXT,
  glFramebufferTexture3D,
  glFramebufferTexture3DEXT,
  glFramebufferTexture3DOES,
  glFramebufferRenderbuffer,
  glFramebufferRenderbufferEXT,
  glFramebufferTextureLayer,
  glFramebufferTextureLayerARB,
  glFramebufferTextureLayerEXT,
  glFramebufferParameteri,
  glDeleteFramebuffers,
  glDeleteFramebuffersEXT,
  glGenRenderbuffers,
  glGenRenderbuffersEXT,
  glRenderbufferStorage,
  glRenderbufferStorageEXT,
  glRenderbufferStorageMultisample,
  glRenderbufferStorageMultisampleEXT,
  glDeleteRenderbuffers,
  glDeleteRenderbuffersEXT,
  glBindRenderbuffer,
  glBindRenderbufferEXT,
  glFenceSync,
  glClientWaitSync,
  glWaitSync,
  glDeleteSync,
  glGenQueries,
  glGenQueriesARB,
  glGenQueriesEXT,
  glBeginQuery,
  glBeginQueryARB,
  glBeginQueryEXT,
  glBeginQueryIndexed,
  glEndQuery,
  glEndQueryARB,
  glEndQueryEXT,
  glEndQueryIndexed,
  glBeginConditionalRender,
  glEndConditionalRender,
  glQueryCounter,
  glQueryCounterEXT,
  glDeleteQueries,
  glDeleteQueriesARB,
  glDeleteQueriesEXT,
  glBufferData,
  glBufferDataARB,
  glBufferStorage,
  glBufferSubData,
  glBufferSubDataARB,
  glCopyBufferSubData,
  glBindBufferBase,
  glBindBufferBaseEXT,
  glBindBufferRange,
  glBindBufferRangeEXT,
  glBindBuffersBase,
  glBindBuffersRange,
  glMapBuffer,
  glMapBufferARB,
  glMapBufferOES,
  glMapBufferRange,
  glFlushMappedBufferRange,
  glUnmapBuffer,
  glUnmapBufferARB,
  glUnmapBufferOES,
  glTransformFeedbackVaryings,
  glTransformFeedbackVaryingsEXT,
  glGenTransformFeedbacks,
  glDeleteTransformFeedbacks,
  glBindTransformFeedback,
  glBeginTransformFeedback,
  glBeginTransformFeedbackEXT,
  glPauseTransformFeedback,
  glResumeTransformFeedback,
  glEndTransformFeedback,
  glEndTransformFeedbackEXT,
  glDrawTransformFeedback,
  glDrawTransformFeedbackInstanced,
  glDrawTransformFeedbackStream,
  glDrawTransformFeedbackStreamInstanced,
  glDeleteBuffers,
  glDeleteBuffersARB,
  glGenVertexArrays,
  glGenVertexArraysOES,
  glBindVertexArray,
  glBindVertexArrayOES,
  glDeleteVertexArrays,
  glDeleteVertexArraysOES,
  glVertexAttrib1d,
  glVertexAttrib1dARB,
  glVertexAttrib1dv,
  glVertexAttrib1dvARB,
  glVertexAttrib1f,
  glVertexAttrib1fARB,
  glVertexAttrib1fv,
  glVertexAttrib1fvARB,
  glVertexAttrib1s,
  glVertexAttrib1sARB,
  glVertexAttrib1sv,
  glVertexAttrib1svARB,
  glVertexAttrib2d,
  glVertexAttrib2dARB,
  glVertexAttrib2dv,
  glVertexAttrib2dvARB,
  glVertexAttrib2f,
  glVertexAttrib2fARB,
  glVertexAttrib2fv,
  glVertexAttrib2fvARB,
  glVertexAttrib2s,
  glVertexAttrib2sARB,
  glVertexAttrib2sv,
  glVertexAttrib2svARB,
  glVertexAttrib3d,
  glVertexAttrib3dARB,
  glVertexAttrib3dv,
  glVertexAttrib3dvARB,
  glVertexAttrib3f,
  glVertexAttrib3fARB,
  glVertexAttrib3fv,
  glVertexAttrib3fvARB,
  glVertexAttrib3s,
  glVertexAttrib3sARB,
  glVertexAttrib3sv,
  glVertexAttrib3svARB,
  glVertexAttrib4Nbv,
  glVertexAttrib4NbvARB,
  glVertexAttrib4Niv,
  glVertexAttrib4NivARB,
  glVertexAttrib4Nsv,
  glVertexAttrib4NsvARB,
  glVertexAttrib4Nub,
  glVertexAttrib4Nubv,
  glVertexAttrib4NubvARB,
  glVertexAttrib4Nuiv,
  glVertexAttrib4NuivARB,
  glVertexAttrib4Nusv,
  glVertexAttrib4NusvARB,
  glVertexAttrib4bv,
  glVertexAttrib4bvARB,
  glVertexAttrib4d,
  glVertexAttrib4dARB,
  glVertexAttrib4dv,
  glVertexAttrib4dvARB,
  glVertexAttrib4f,
  glVertexAttrib4fARB,
  glVertexAttrib4fv,
  glVertexAttrib4fvARB,
  glVertexAttrib4iv,
  glVertexAttrib4ivARB,
  glVertexAttrib4s,
  glVertexAttrib4sARB,
  glVertexAttrib4sv,
  glVertexAttrib4svARB,
  glVertexAttrib4ubv,
  glVertexAttrib4ubvARB,
  glVertexAttrib4uiv,
  glVertexAttrib4uivARB,
  glVertexAttrib4usv,
  glVertexAttrib4usvARB,
  glVertexAttribI1i,
  glVertexAttribI1iEXT,
  glVertexAttribI1iv,
  glVertexAttribI1ivEXT,
  glVertexAttribI1ui,
  glVertexAttribI1uiEXT,
  glVertexAttribI1uiv,
  glVertexAttribI1uivEXT,
  glVertexAttribI2i,
  glVertexAttribI2iEXT,
  glVertexAttribI2iv,
  glVertexAttribI2ivEXT,
  glVertexAttribI2ui,
  glVertexAttribI2uiEXT,
  glVertexAttribI2uiv,
  glVertexAttribI2uivEXT,
  glVertexAttribI3i,
  glVertexAttribI3iEXT,
  glVertexAttribI3iv,
  glVertexAttribI3ivEXT,
  glVertexAttribI3ui,
  glVertexAttribI3uiEXT,
  glVertexAttribI3uiv,
  glVertexAttribI3uivEXT,
  glVertexAttribI4bv,
  glVertexAttribI4bvEXT,
  glVertexAttribI4i,
  glVertexAttribI4iEXT,
  glVertexAttribI4iv,
  glVertexAttribI4ivEXT,
  glVertexAttribI4sv,
  glVertexAttribI4svEXT,
  glVertexAttribI4ubv,
  glVertexAttribI4ubvEXT,
  glVertexAttribI4ui,
  glVertexAttribI4uiEXT,
  glVertexAttribI4uiv,
  glVertexAttribI4uivEXT,
  glVertexAttribI4usv,
  glVertexAttribI4usvEXT,
  glVertexAttribL1d,
  glVertexAttribL1dEXT,
  glVertexAttribL1dv,
  glVertexAttribL1dvEXT,
  glVertexAttribL2d,
  glVertexAttribL2dEXT,
  glVertexAttribL2dv,
  glVertexAttribL2dvEXT,
  glVertexAttribL3d,
  glVertexAttribL3dEXT,
  glVertexAttribL3dv,
  glVertexAttribL3dvEXT,
  glVertexAttribL4d,
  glVertexAttribL4dEXT,
  glVertexAttribL4dv,
  glVertexAttribL4dvEXT,
  glVertexAttribP1ui,
  glVertexAttribP1uiv,
  glVertexAttribP2ui,
  glVertexAttribP2uiv,
  glVertexAttribP3ui,
  glVertexAttribP3uiv,
  glVertexAttribP4ui,
  glVertexAttribP4uiv,
  glVertexAttribPointer,
  glVertexAttribPointerARB,
  glVertexAttribIPointer,
  glVertexAttribIPointerEXT,
  glVertexAttribLPointer,
  glVertexAttribLPointerEXT,
  glVertexAttribBinding,
  glVertexAttribFormat,
  glVertexAttribIFormat,
  glVertexAttribLFormat,
  glVertexAttribDivisor,
  glVertexAttribDivisorARB,
  glBindAttribLocation,
  glBindFragDataLocation,
  glBindFragDataLocationEXT,
  glBindFragDataLocationIndexed,
  glEnableVertexAttribArray,
  glEnableVertexAttribArrayARB,
  glDisableVertexAttribArray,
  glDisableVertexAttribArrayARB,
  glBindVertexBuffer,
  glBindVertexBuffers,
  glVertexBindingDivisor,
  glBindImageTexture,
  glBindImageTextureEXT,
  glBindImageTextures,
  glGenSamplers,
  glBindSampler,
  glBindSamplers,
  glBindTextures,
  glDeleteSamplers,
  glSamplerParameteri,
  glSamplerParameterf,
  glSamplerParameteriv,
  glSamplerParameterfv,
  glSamplerParameterIiv,
  glSamplerParameterIivEXT,
  glSamplerParameterIivOES,
  glSamplerParameterIuiv,
  glSamplerParameterIuivEXT,
  glSamplerParameterIuivOES,
  glPatchParameteri,
  glPatchParameteriEXT,
  glPatchParameteriOES,
  glPatchParameterfv,
  glPointParameterf,
  glPointParameterfARB,
  glPointParameterfEXT,
  glPointParameterfv,
  glPointParameterfvARB,
  glPointParameterfvEXT,
  glPointParameteri,
  glPointParameteriv,
  glDispatchCompute,
  glDispatchComputeIndirect,
  glMemoryBarrier,
  glMemoryBarrierEXT,
  glMemoryBarrierByRegion,
  glTextureBarrier,
  glClearDepthf,
  glClearBufferfv,
  glClearBufferiv,
  glClearBufferuiv,
  glClearBufferfi,
  glClearBufferData,
  glClearBufferSubData,
  glClearTexImage,
  glClearTexSubImage,
  glInvalidateBufferData,
  glInvalidateBufferSubData,
  glInvalidateFramebuffer,
  glInvalidateSubFramebuffer,
  glInvalidateTexImage,
  glInvalidateTexSubImage,
  glScissorArrayv,
  glScissorArrayvOES,
  glScissorArrayvNV,
  glScissorIndexed,
  glScissorIndexedOES,
  glScissorIndexedNV,
  glScissorIndexedv,
  glScissorIndexedvOES,
  glScissorIndexedvNV,
  glViewportIndexedf,
  glViewportIndexedfOES,
  glViewportIndexedfNV,
  glViewportIndexedfv,
  glViewportIndexedfvOES,
  glViewportIndexedfvNV,
  glViewportArrayv,
  glViewportArrayvOES,
  glViewportArrayvNV,
  glUniformBlockBinding,
  glShaderStorageBlockBinding,
  glUniformSubroutinesuiv,
  glUniform1f,
  glUniform1i,
  glUniform1ui,
  glUniform1uiEXT,
  glUniform1d,
  glUniform2f,
  glUniform2i,
  glUniform2ui,
  glUniform2uiEXT,
  glUniform2d,
  glUniform3f,
  glUniform3i,
  glUniform3ui,
  glUniform3uiEXT,
  glUniform3d,
  glUniform4f,
  glUniform4i,
  glUniform4ui,
  glUniform4uiEXT,
  glUniform4d,
  glUniform1fv,
  glUniform1iv,
  glUniform1uiv,
  glUniform1uivEXT,
  glUniform1dv,
  glUniform2fv,
  glUniform2iv,
  glUniform2uiv,
  glUniform2uivEXT,
  glUniform2dv,
  glUniform3fv,
  glUniform3iv,
  glUniform3uiv,
  glUniform3uivEXT,
  glUniform3dv,
  glUniform4fv,
  glUniform4iv,
  glUniform4uiv,
  glUniform4uivEXT,
  glUniform4dv,
  glUniformMatrix2fv,
  glUniformMatrix2x3fv,
  glUniformMatrix2x4fv,
  glUniformMatrix3fv,
  glUniformMatrix3x2fv,
  glUniformMatrix3x4fv,
  glUniformMatrix4fv,
  glUniformMatrix4x2fv,
  glUniformMatrix4x3fv,
  glUniformMatrix2dv,
  glUniformMatrix2x3dv,
  glUniformMatrix2x4dv,
  glUniformMatrix3dv,
  glUniformMatrix3x2dv,
  glUniformMatrix3x4dv,
  glUniformMatrix4dv,
  glUniformMatrix4x2dv,
  glUniformMatrix4x3dv,
  glProgramUniform1f,
  glProgramUniform1fEXT,
  glProgramUniform1i,
  glProgramUniform1iEXT,
  glProgramUniform1ui,
  glProgramUniform1uiEXT,
  glProgramUniform1d,
  glProgramUniform1dEXT,
  glProgramUniform2f,
  glProgramUniform2fEXT,
  glProgramUniform2i,
  glProgramUniform2iEXT,
  glProgramUniform2ui,
  glProgramUniform2uiEXT,
  glProgramUniform2d,
  glProgramUniform2dEXT,
  glProgramUniform3f,
  glProgramUniform3fEXT,
  glProgramUniform3i,
  glProgramUniform3iEXT,
  glProgramUniform3ui,
  glProgramUniform3uiEXT,
  glProgramUniform3d,
  glProgramUniform3dEXT,
  glProgramUniform4f,
  glProgramUniform4fEXT,
  glProgramUniform4i,
  glProgramUniform4iEXT,
  glProgramUniform4ui,
  glProgramUniform4uiEXT,
  glProgramUniform4d,
  glProgramUniform4dEXT,
  glProgramUniform1fv,
  glProgramUniform1fvEXT,
  glProgramUniform1iv,
  glProgramUniform1ivEXT,
  glProgramUniform1uiv,
  glProgramUniform1uivEXT,
  glProgramUniform1dv,
  glProgramUniform1dvEXT,
  glProgramUniform2fv,
  glProgramUniform2fvEXT,
  glProgramUniform2iv,
  glProgramUniform2ivEXT,
  glProgramUniform2uiv,
  glProgramUniform2uivEXT,
  glProgramUniform2dv,
  glProgramUniform2dvEXT,
  glProgramUniform3fv,
  glProgramUniform3fvEXT,
  glProgramUniform3iv,
  glProgramUniform3ivEXT,
  glProgramUniform3uiv,
  glProgramUniform3uivEXT,
  glProgramUniform3dv,
  glProgramUniform3dvEXT,
  glProgramUniform4fv,
  glProgramUniform4fvEXT,
  glProgramUniform4iv,
  glProgramUniform4ivEXT,
  glProgramUniform4uiv,
  glProgramUniform4uivEXT,
  glProgramUniform4dv,
  glProgramUniform4dvEXT,
  glProgramUniformMatrix2fv,
  glProgramUniformMatrix2fvEXT,
  glProgramUniformMatrix2x3fv,
  glProgramUniformMatrix2x3fvEXT,
  glProgramUniformMatrix2x4fv,
  glProgramUniformMatrix2x4fvEXT,
  glProgramUniformMatrix3fv,
  glProgramUniformMatrix3fvEXT,
  glProgramUniformMatrix3x2fv,
  glProgramUniformMatrix3x2fvEXT,
  glProgramUniformMatrix3x4fv,
  glProgramUniformMatrix3x4fvEXT,
  glProgramUniformMatrix4fv,
  glProgramUniformMatrix4fvEXT,
  glProgramUniformMatrix4x2fv,
  glProgramUniformMatrix4x2fvEXT,
  glProgramUniformMatrix4x3fv,
  glProgramUniformMatrix4x3fvEXT,
  glProgramUniformMatrix2dv,
  glProgramUniformMatrix2dvEXT,
  glProgramUniformMatrix2x3dv,
  glProgramUniformMatrix2x3dvEXT,
  glProgramUniformMatrix2x4dv,
  glProgramUniformMatrix2x4dvEXT,
  glProgramUniformMatrix3dv,
  glProgramUniformMatrix3dvEXT,
  glProgramUniformMatrix3x2dv,
  glProgramUniformMatrix3x2dvEXT,
  glProgramUniformMatrix3x4dv,
  glProgramUniformMatrix3x4dvEXT,
  glProgramUniformMatrix4dv,
  glProgramUniformMatrix4dvEXT,
  glProgramUniformMatrix4x2dv,
  glProgramUniformMatrix4x2dvEXT,
  glProgramUniformMatrix4x3dv,
  glProgramUniformMatrix4x3dvEXT,
  glDrawRangeElements,
  glDrawRangeElementsEXT,
  glDrawRangeElementsBaseVertex,
  glDrawRangeElementsBaseVertexEXT,
  glDrawRangeElementsBaseVertexOES,
  glDrawArraysInstancedBaseInstance,
  glDrawArraysInstancedBaseInstanceEXT,
  glDrawArraysInstanced,
  glDrawArraysInstancedARB,
  glDrawArraysInstancedEXT,
  glDrawElementsInstanced,
  glDrawElementsInstancedARB,
  glDrawElementsInstancedEXT,
  glDrawElementsInstancedBaseInstance,
  glDrawElementsInstancedBaseInstanceEXT,
  glDrawElementsBaseVertex,
  glDrawElementsBaseVertexEXT,
  glDrawElementsBaseVertexOES,
  glDrawElementsInstancedBaseVertex,
  glDrawElementsInstancedBaseVertexEXT,
  glDrawElementsInstancedBaseVertexOES,
  glDrawElementsInstancedBaseVertexBaseInstance,
  glDrawElementsInstancedBaseVertexBaseInstanceEXT,
  glMultiDrawArrays,
  glMultiDrawArraysEXT,
  glMultiDrawElements,
  glMultiDrawElementsBaseVertex,
  glMultiDrawElementsBaseVertexEXT,
  glMultiDrawElementsBaseVertexOES,
  glMultiDrawArraysIndirect,
  glMultiDrawElementsIndirect,
  glDrawArraysIndirect,
  glDrawElementsIndirect,
  glBlitFramebuffer,
  glBlitFramebufferEXT,
  glPrimitiveBoundingBox,
  glPrimitiveBoundingBoxEXT,
  glPrimitiveBoundingBoxOES,
  glBlendBarrier,
  glFramebufferTexture2DMultisampleEXT,
  glDiscardFramebufferEXT,
  glDepthRangeArrayfvOES,
  glDepthRangeArrayfvNV,
  glDepthRangeIndexedfOES,
  glDepthRangeIndexedfNV,
  glNamedStringARB,
  glDeleteNamedStringARB,
  glCompileShaderIncludeARB,
  glIsNamedStringARB,
  glGetNamedStringARB,
  glGetNamedStringivARB,
  glDispatchComputeGroupSizeARB,
  glMultiDrawArraysIndirectCountARB,
  glMultiDrawElementsIndirectCountARB,
  glRasterSamplesEXT,
  glDepthBoundsEXT,
  glPolygonOffsetClampEXT,
  glInsertEventMarkerEXT,
  glPushGroupMarkerEXT,
  glPopGroupMarkerEXT,
  glFrameTerminatorGREMEDY,
  glStringMarkerGREMEDY,
  glFramebufferTextureMultiviewOVR,
  glFramebufferTextureMultisampleMultiviewOVR,
  glCompressedTextureImage1DEXT,
  glCompressedTextureImage2DEXT,
  glCompressedTextureImage3DEXT,
  glCompressedTextureSubImage1DEXT,
  glCompressedTextureSubImage2DEXT,
  glCompressedTextureSubImage3DEXT,
  glGenerateTextureMipmapEXT,
  glGetPointeri_vEXT,
  glGetDoubleIndexedvEXT,
  glGetPointerIndexedvEXT,
  glGetIntegerIndexedvEXT,
  glGetBooleanIndexedvEXT,
  glGetFloatIndexedvEXT,
  glGetMultiTexImageEXT,
  glGetMultiTexParameterfvEXT,
  glGetMultiTexParameterivEXT,
  glGetMultiTexParameterIivEXT,
  glGetMultiTexParameterIuivEXT,
  glGetMultiTexLevelParameterfvEXT,
  glGetMultiTexLevelParameterivEXT,
  glGetCompressedMultiTexImageEXT,
  glGetNamedBufferPointervEXT,
  glGetNamedBufferPointerv,
  glGetNamedProgramivEXT,
  glGetNamedFramebufferAttachmentParameterivEXT,
  glGetNamedFramebufferAttachmentParameteriv,
  glGetNamedBufferParameterivEXT,
  glGetNamedBufferParameteriv,
  glCheckNamedFramebufferStatusEXT,
  glCheckNamedFramebufferStatus,
  glGetNamedBufferSubDataEXT,
  glGetNamedFramebufferParameterivEXT,
  glGetFramebufferParameterivEXT,
  glGetNamedFramebufferParameteriv,
  glGetNamedRenderbufferParameterivEXT,
  glGetNamedRenderbufferParameteriv,
  glGetVertexArrayIntegervEXT,
  glGetVertexArrayPointervEXT,
  glGetVertexArrayIntegeri_vEXT,
  glGetVertexArrayPointeri_vEXT,
  glGetCompressedTextureImageEXT,
  glGetTextureImageEXT,
  glGetTextureParameterivEXT,
  glGetTextureParameterfvEXT,
  glGetTextureParameterIivEXT,
  glGetTextureParameterIuivEXT,
  glGetTextureLevelParameterivEXT,
  glGetTextureLevelParameterfvEXT,
  glBindMultiTextureEXT,
  glMapNamedBufferEXT,
  glMapNamedBuffer,
  glMapNamedBufferRangeEXT,
  glFlushMappedNamedBufferRangeEXT,
  glUnmapNamedBufferEXT,
  glUnmapNamedBuffer,
  glClearNamedBufferDataEXT,
  glClearNamedBufferData,
  glClearNamedBufferSubDataEXT,
  glNamedBufferDataEXT,
  glNamedBufferStorageEXT,
  glNamedBufferSubDataEXT,
  glNamedCopyBufferSubDataEXT,
  glNamedFramebufferTextureEXT,
  glNamedFramebufferTexture,
  glNamedFramebufferTexture1DEXT,
  glNamedFramebufferTexture2DEXT,
  glNamedFramebufferTexture3DEXT,
  glNamedFramebufferRenderbufferEXT,
  glNamedFramebufferRenderbuffer,
  glNamedFramebufferTextureLayerEXT,
  glNamedFramebufferTextureLayer,
  glNamedFramebufferParameteriEXT,
  glNamedFramebufferParameteri,
  glNamedRenderbufferStorageEXT,
  glNamedRenderbufferStorage,
  glNamedRenderbufferStorageMultisampleEXT,
  glNamedRenderbufferStorageMultisample,
  glFramebufferDrawBufferEXT,
  glNamedFramebufferDrawBuffer,
  glFramebufferDrawBuffersEXT,
  glNamedFramebufferDrawBuffers,
  glFramebufferReadBufferEXT,
  glNamedFramebufferReadBuffer,
  glTextureBufferEXT,
  glTextureBufferRangeEXT,
  glTextureImage1DEXT,
  glTextureImage2DEXT,
  glTextureImage3DEXT,
  glTextureParameterfEXT,
  glTextureParameterfvEXT,
  glTextureParameteriEXT,
  glTextureParameterivEXT,
  glTextureParameterIivEXT,
  glTextureParameterIuivEXT,
  glTextureStorage1DEXT,
  glTextureStorage2DEXT,
  glTextureStorage3DEXT,
  glTextureStorage2DMultisampleEXT,
  glTextureStorage3DMultisampleEXT,
  glTextureSubImage1DEXT,
  glTextureSubImage2DEXT,
  glTextureSubImage3DEXT,
  glCopyTextureImage1DEXT,
  glCopyTextureImage2DEXT,
  glCopyTextureSubImage1DEXT,
  glCopyTextureSubImage2DEXT,
  glCopyTextureSubImage3DEXT,
  glMultiTexParameteriEXT,
  glMultiTexParameterivEXT,
  glMultiTexParameterfEXT,
  glMultiTexParameterfvEXT,
  glMultiTexImage1DEXT,
  glMultiTexImage2DEXT,
  glMultiTexSubImage1DEXT,
  glMultiTexSubImage2DEXT,
  glCopyMultiTexImage1DEXT,
  glCopyMultiTexImage2DEXT,
  glCopyMultiTexSubImage1DEXT,
  glCopyMultiTexSubImage2DEXT,
  glMultiTexImage3DEXT,
  glMultiTexSubImage3DEXT,
  glCopyMultiTexSubImage3DEXT,
  glCompressedMultiTexImage3DEXT,
  glCompressedMultiTexImage2DEXT,
  glCompressedMultiTexImage1DEXT,
  glCompressedMultiTexSubImage3DEXT,
  glCompressedMultiTexSubImage2DEXT,
  glCompressedMultiTexSubImage1DEXT,
  glMultiTexBufferEXT,
  glMultiTexParameterIivEXT,
  glMultiTexParameterIuivEXT,
  glGenerateMultiTexMipmapEXT,
  glVertexArrayVertexAttribOffsetEXT,
  glVertexArrayVertexAttribIOffsetEXT,
  glEnableVertexArrayAttribEXT,
  glEnableVertexArrayAttrib,
  glDisableVertexArrayAttribEXT,
  glDisableVertexArrayAttrib,
  glVertexArrayBindVertexBufferEXT,
  glVertexArrayVertexBuffer,
  glVertexArrayVertexAttribFormatEXT,
  glVertexArrayAttribFormat,
  glVertexArrayVertexAttribIFormatEXT,
  glVertexArrayAttribIFormat,
  glVertexArrayVertexAttribLFormatEXT,
  glVertexArrayAttribLFormat,
  glVertexArrayVertexAttribBindingEXT,
  glVertexArrayAttribBinding,
  glVertexArrayVertexBindingDivisorEXT,
  glVertexArrayBindingDivisor,
  glVertexArrayVertexAttribLOffsetEXT,
  glVertexArrayVertexAttribDivisorEXT,
  glCreateTransformFeedbacks,
  glTransformFeedbackBufferBase,
  glTransformFeedbackBufferRange,
  glGetTransformFeedbacki64_v,
  glGetTransformFeedbacki_v,
  glGetTransformFeedbackiv,
  glCreateBuffers,
  glGetNamedBufferSubData,
  glNamedBufferStorage,
  glNamedBufferData,
  glNamedBufferSubData,
  glCopyNamedBufferSubData,
  glClearNamedBufferSubData,
  glMapNamedBufferRange,
  glFlushMappedNamedBufferRange,
  glGetNamedBufferParameteri64v,
  glCreateFramebuffers,
  glInvalidateNamedFramebufferData,
  glInvalidateNamedFramebufferSubData,
  glClearNamedFramebufferiv,
  glClearNamedFramebufferuiv,
  glClearNamedFramebufferfv,
  glClearNamedFramebufferfi,
  glBlitNamedFramebuffer,
  glCreateRenderbuffers,
  glCreateTextures,
  glTextureBuffer,
  glTextureBufferRange,
  glTextureStorage1D,
  glTextureStorage2D,
  glTextureStorage3D,
  glTextureStorage2DMultisample,
  glTextureStorage3DMultisample,
  glTextureSubImage1D,
  glTextureSubImage2D,
  glTextureSubImage3D,
  glCompressedTextureSubImage1D,
  glCompressedTextureSubImage2D,
  glCompressedTextureSubImage3D,
  glCopyTextureSubImage1D,
  glCopyTextureSubImage2D,
  glCopyTextureSubImage3D,
  glTextureParameterf,
  glTextureParameterfv,
  glTextureParameteri,
  glTextureParameterIiv,
  glTextureParameterIuiv,
  glTextureParameteriv,
  glGenerateTextureMipmap,
  glBindTextureUnit,
  glGetTextureImage,
  glGetTextureSubImage,
  glGetCompressedTextureImage,
  glGetCompressedTextureSubImage,
  glGetTextureLevelParameterfv,
  glGetTextureLevelParameteriv,
  glGetTextureParameterIiv,
  glGetTextureParameterIuiv,
  glGetTextureParameterfv,
  glGetTextureParameteriv,
  glCreateVertexArrays,
  glCreateSamplers,
  glCreateProgramPipelines,
  glCreateQueries,
  glVertexArrayElementBuffer,
  glVertexArrayVertexBuffers,
  glGetVertexArrayiv,
  glGetVertexArrayIndexed64iv,
  glGetVertexArrayIndexediv,
  glGetQueryBufferObjecti64v,
  glGetQueryBufferObjectiv,
  glGetQueryBufferObjectui64v,
  glGetQueryBufferObjectuiv,
  wglDXSetResourceShareHandleNV,
  wglDXOpenDeviceNV,
  wglDXCloseDeviceNV,
  wglDXRegisterObjectNV,
  wglDXUnregisterObjectNV,
  wglDXObjectAccessNV,
  wglDXLockObjectsNV,
  wglDXUnlockObjectsNV,

  glIndirectSubCommand,

  glContextInit,

  glMultiDrawArraysIndirectCount,
  glMultiDrawElementsIndirectCount,
  glPolygonOffsetClamp,
  glMaxShaderCompilerThreadsARB,
  glMaxShaderCompilerThreadsKHR,

  glSpecializeShader,
  glSpecializeShaderARB,

  glUniform1fARB,
  glUniform1iARB,
  glUniform2fARB,
  glUniform2iARB,
  glUniform3fARB,
  glUniform3iARB,
  glUniform4fARB,
  glUniform4iARB,
  glUniform1fvARB,
  glUniform1ivARB,
  glUniform2fvARB,
  glUniform2ivARB,
  glUniform3fvARB,
  glUniform3ivARB,
  glUniform4fvARB,
  glUniform4ivARB,
  glUniformMatrix2fvARB,
  glUniformMatrix3fvARB,
  glUniformMatrix4fvARB,

  glGetUnsignedBytevEXT,
  glGetUnsignedBytei_vEXT,
  glDeleteMemoryObjectsEXT,
  glIsMemoryObjectEXT,
  glCreateMemoryObjectsEXT,
  glMemoryObjectParameterivEXT,
  glGetMemoryObjectParameterivEXT,
  glTexStorageMem2DEXT,
  glTexStorageMem2DMultisampleEXT,
  glTexStorageMem3DEXT,
  glTexStorageMem3DMultisampleEXT,
  glBufferStorageMemEXT,
  glTextureStorageMem2DEXT,
  glTextureStorageMem2DMultisampleEXT,
  glTextureStorageMem3DEXT,
  glTextureStorageMem3DMultisampleEXT,
  glNamedBufferStorageMemEXT,
  glTexStorageMem1DEXT,
  glTextureStorageMem1DEXT,
  glGenSemaphoresEXT,
  glDeleteSemaphoresEXT,
  glIsSemaphoreEXT,
  glSemaphoreParameterui64vEXT,
  glGetSemaphoreParameterui64vEXT,
  glWaitSemaphoreEXT,
  glSignalSemaphoreEXT,
  glImportMemoryFdEXT,
  glImportSemaphoreFdEXT,
  glImportMemoryWin32HandleEXT,
  glImportMemoryWin32NameEXT,
  glImportSemaphoreWin32HandleEXT,
  glImportSemaphoreWin32NameEXT,
  glAcquireKeyedMutexWin32EXT,
  glReleaseKeyedMutexWin32EXT,

  ContextConfiguration,

  glTextureFoveationParametersQCOM,

  glBufferStorageEXT,

  CoherentMapWrite,

  glBeginPerfQueryINTEL,
  glCreatePerfQueryINTEL,
  glDeletePerfQueryINTEL,
  glEndPerfQueryINTEL,
  glGetFirstPerfQueryIdINTEL,
  glGetNextPerfQueryIdINTEL,
  glGetPerfCounterInfoINTEL,
  glGetPerfQueryDataINTEL,
  glGetPerfQueryIdByNameINTEL,
  glGetPerfQueryInfoINTEL,

  glBlendEquationARB,
  glPrimitiveBoundingBoxARB,

  Max,
};

class GLChunkPreserver
{
public:
  GLChunkPreserver(GLChunk &original) : m_original(original), m_value(original) {}
  ~GLChunkPreserver() { m_original = m_value; }
private:
  GLChunk &m_original;
  GLChunk m_value;
};

// set at the point of each hooked entry point, so we know precisely which function was called
extern GLChunk gl_CurChunk;

#define PUSH_CURRENT_CHUNK GLChunkPreserver _chunk_restore(gl_CurChunk)

DECLARE_REFLECTION_ENUM(GLChunk);
