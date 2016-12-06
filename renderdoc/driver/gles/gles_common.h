/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 * Copyright (c) 2016 University of Szeged
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#ifndef APIENTRY
#define APIENTRY
#endif

// typed enum so that templates will pick up specialisations
// header must be included before the official headers, so we/
// separate it out to avoid clang-format sorting them differently
#define GLenum RDCGLenum
#include "gles_enum.h"

// official headers
#include "official/gl32.h"
#include "official/gl2ext.h"

#if defined(RENDERDOC_PLATFORM_WIN32)
#include "official/wglext.h"

struct GLWindowingData
{
  GLESWindowingData()
  {
    DC = NULL;
    ctx = NULL;
    wnd = NULL;
  }

  void SetCtx(void *c) { ctx = (HGLRC)c; }
  HDC DC;
  HGLRC ctx;
  HWND wnd;
};

#elif defined(RENDERDOC_PLATFORM_LINUX)
#include "official/egl.h"

struct GLESWindowingData
{
  GLESWindowingData()
    : eglDisplay    (NULL)
    , ctx           (NULL)
    , surface       (0)
  {
  }

  void SetCtx(void *ctx) { this->ctx = (EGLContext)ctx; }

  EGLDisplay eglDisplay;
  EGLContext ctx;
  EGLSurface surface;
};

#elif defined(RENDERDOC_PLATFORM_APPLE)

struct GLESWindowingData
{
  GLESWindowingData()
  {
    ctx = NULL;
    wnd = 0;
  }

  void SetCtx(void *c) { ctx = (void *)c; }
  void *ctx;
  void *wnd;
};

#elif defined(RENDERDOC_PLATFORM_ANDROID)

#include "EGL/egl.h"
#include "EGL/eglext.h"

struct GLESWindowingData
{
  GLESWindowingData()
  {
    ctx = NULL;
    wnd = 0;
  }

  // TODO(elecro): make this more android compatible
  void SetCtx(void *c) { ctx = (void *)c; }
  EGLDisplay eglDisplay;
  EGLContext ctx;
  EGLSurface surface;

  ANativeWindow *wnd;
};

#else
#error "Unknown platform"
#endif

#include "api/replay/renderdoc_replay.h"

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the
// debugbreak.
#define GLNOTIMP(...) RDCDEBUG("OpenGL ES not implemented - " __VA_ARGS__)

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) \
  ret func;                                      \
  bool CONCAT(Serialise_, func);

// TODO pantos copied from gl_common.h
const GLenum eGL_CLAMP = (GLenum)0x2900;

// convenience, the script to pick these out doesn't find them since they are #define'd
// to just straight integers not hex codes
const GLenum eGL_ZERO = (GLenum)0;
const GLenum eGL_ONE = (GLenum)1;

class WrappedGLES;
struct GLHookSet;

size_t BufferIdx(GLenum buf);
GLenum BufferEnum(size_t idx);

size_t QueryIdx(GLenum query);
GLenum QueryEnum(size_t idx);

size_t ShaderIdx(GLenum buf);
std::string ShaderName(GLenum buf);
GLenum ShaderBit(size_t idx);
GLenum ShaderEnum(size_t idx);

ResourceFormat MakeResourceFormat(WrappedGLES &gl, GLenum target, GLenum fmt);
GLenum MakeGLFormat(WrappedGLES &gl, ResourceFormat fmt);
PrimitiveTopology MakePrimitiveTopology(const GLHookSet &gl, GLenum Topo);
GLenum MakeGLPrimitiveTopology(PrimitiveTopology Topo);
const char *BlendString(GLenum blendenum);
const char *SamplerString(GLenum smpenum);

void ClearGLErrors(const GLHookSet &gl);

GLuint GetBoundVertexBuffer(const GLHookSet &gl, GLuint idx);

void GetBindpointMapping(const GLHookSet &gl, GLuint curProg, int shadIdx, ShaderReflection *refl,
                         ShaderBindpointMapping &mapping);

extern int GLCoreVersion;
extern bool GLIsCore;

// extensions we know we want to check for are precached, indexd by this enum
enum ExtensionCheckEnum
{
  ExtensionSupported_EXT_polygon_offset_clamp = 0,
  ExtensionSupported_KHR_blend_equation_advanced_coherent,
  ExtensionSupported_EXT_raster_multisample,
  ExtensionSupported_EXT_clip_cull_distance,
  ExtensionSupported_NV_polygon_mode,
  ExtensionSupported_NV_viewport_array,
  ExtensionSupported_OES_viewport_array,
  ExtensionSupported_EXT_buffer_storage,
  ExtensionSupported_EXT_texture_storage,
  ExtensionSupported_EXT_map_buffer_range,
  ExtensionSupported_EXT_base_instance,
  ExtensionSupported_EXT_debug_label,
  ExtensionSupported_EXT_multisample_compatibility, // TODO pantos in gl2ext.h its name is GL_EXT_multisampled_compatibility
  ExtensionSupported_EXT_multisampled_render_to_texture,
  ExtensionSupported_IMG_multisampled_render_to_texture,
  ExtensionSupported_OES_texture_view,
  ExtensionSupported_EXT_texture_filter_anisotropic,
  ExtensionSupported_Count,
};
extern bool ExtensionSupported[ExtensionSupported_Count];

// for some things we need to know how a specific implementation behaves to work around it
// or adjust things. We centralise that here (similar to extensions)
enum VendorCheckEnum
{
  VendorCheck_AMD_vertex_buffer_query,
  VendorCheck_EXT_compressed_cube_size,
  VendorCheck_NV_avoid_D32S8_copy,
  VendorCheck_EXT_fbo_shared,
  VendorCheck_EXT_vao_shared,
  VendorCheck_AMD_polygon_mode_query,
  VendorCheck_AMD_pipeline_compute_query,
  VendorCheck_NV_ClearNamedFramebufferfiBugs,
  VendorCheck_Count,
};
extern bool VendorCheck[VendorCheck_Count];

// fills out the extension supported array above
void DoExtensionChecks(const GLHookSet &gl);

// fills out the version-specific checks above
void DoVendorChecks(const GLHookSet &gl, GLESWindowingData context);

#include "core/core.h"
#include "serialise/serialiser.h"

struct ShaderReflection;

void CopyProgramUniforms(const GLHookSet &gl, GLuint progSrc, GLuint progDst);
void SerialiseProgramUniforms(const GLHookSet &gl, Serialiser *ser, GLuint prog,
                              map<GLint, GLint> *locTranslate, bool writing);
void CopyProgramAttribBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst,
                               ShaderReflection *refl);
void CopyProgramFragDataBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst,
                                 ShaderReflection *refl);

struct DrawElementsIndirectCommand
{
  uint32_t count;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t baseVertex;
  uint32_t baseInstance;
};

struct DrawArraysIndirectCommand
{
  uint32_t count;
  uint32_t instanceCount;
  uint32_t first;
  uint32_t baseInstance;
};

#include "gles_chunks.h"

enum GLChunkType
{
  #define CHUNK_FUNC(ID, TEXT) ID,
  CHUNKS(CHUNK_FUNC)
  #undef CHUNK_FUNC

  NUM_OPENGL_CHUNKS,
};

void dumpShaderCompileStatus(const GLHookSet& gl, GLuint shader, GLsizei numSources, const char** sources);
void dumpProgramBinary(const GLHookSet& gl, GLuint program);
void dumpFBOState(const GLHookSet& gl);
