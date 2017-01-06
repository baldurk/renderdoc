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

#pragma once

#include "common/common.h"

// typed enum so that templates will pick up specialisations
// header must be included before the official headers, so we/
// separate it out to avoid clang-format sorting them differently
#define GLenum RDCGLenum
#include "gl_enum.h"

// official headers
#include "official/glcorearb.h"
#include "official/glext.h"

#if ENABLED(RDOC_WIN32)
#include "official/wglext.h"

struct GLWindowingData
{
  GLWindowingData()
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

#elif ENABLED(RDOC_LINUX)
// cheeky way to prevent GL/gl.h from being included, as we want to use
// glcorearb.h from above
#define __gl_h_
#include <GL/glx.h>
#include "official/glxext.h"

struct GLWindowingData
{
  GLWindowingData()
  {
    dpy = NULL;
    ctx = NULL;
    wnd = 0;
  }

  void SetCtx(void *c) { ctx = (GLXContext)c; }
  Display *dpy;
  GLXContext ctx;
  GLXDrawable wnd;
};

#elif ENABLED(RDOC_APPLE)

struct GLWindowingData
{
  GLWindowingData()
  {
    ctx = NULL;
    wnd = 0;
  }

  void SetCtx(void *c) { ctx = (void *)c; }
  void *ctx;
  void *wnd;
};

#elif ENABLED(RDOC_ANDROID)

#include "EGL/egl.h"
#include "EGL/eglext.h"

struct GLWindowingData
{
  GLWindowingData()
  {
    ctx = NULL;
    wnd = 0;
  }

  void SetCtx(void *c) { ctx = (void *)c; }
  EGLContext ctx;
  ANativeWindow *wnd;
};

#else
#error "Unknown platform"
#endif

// define stubs so other platforms can define these functions, but empty
#if DISABLED(RDOC_WIN32)
typedef void *HANDLE;
typedef long BOOL;

typedef BOOL(APIENTRYP *PFNWGLDXSETRESOURCESHAREHANDLENVPROC)(void *dxObject, HANDLE shareHandle);
typedef HANDLE(APIENTRYP *PFNWGLDXOPENDEVICENVPROC)(void *dxDevice);
typedef BOOL(APIENTRYP *PFNWGLDXCLOSEDEVICENVPROC)(HANDLE hDevice);
typedef HANDLE(APIENTRYP *PFNWGLDXREGISTEROBJECTNVPROC)(HANDLE hDevice, void *dxObject, GLuint name,
                                                        GLenum type, GLenum access);
typedef BOOL(APIENTRYP *PFNWGLDXUNREGISTEROBJECTNVPROC)(HANDLE hDevice, HANDLE hObject);
typedef BOOL(APIENTRYP *PFNWGLDXOBJECTACCESSNVPROC)(HANDLE hObject, GLenum access);
typedef BOOL(APIENTRYP *PFNWGLDXLOCKOBJECTSNVPROC)(HANDLE hDevice, GLint count, HANDLE *hObjects);
typedef BOOL(APIENTRYP *PFNWGLDXUNLOCKOBJECTSNVPROC)(HANDLE hDevice, GLint count, HANDLE *hObjects);
#endif

#include "api/replay/renderdoc_replay.h"

// define this if you e.g. haven't compiled the D3D modules and want to disable
// interop capture support.
#define RENDERDOC_DX_GL_INTEROP OPTION_ON

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the
// debugbreak.
#define GLNOTIMP(...) RDCDEBUG("OpenGL not implemented - " __VA_ARGS__)

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) \
  ret func;                                      \
  bool CONCAT(Serialise_, func);

// no longer in glcorearb.h or glext.h
const GLenum eGL_LUMINANCE = (GLenum)0x1909;
const GLenum eGL_LUMINANCE_ALPHA = (GLenum)0x190A;
const GLenum eGL_INTENSITY = (GLenum)0x8049;
const GLenum eGL_LIGHTING = (GLenum)0x0B50;
const GLenum eGL_ALPHA_TEST = (GLenum)0x0BC0;
const GLenum eGL_CLAMP = (GLenum)0x2900;

// convenience, the script to pick these out doesn't find them since they are #define'd
// to just straight integers not hex codes
const GLenum eGL_ZERO = (GLenum)0;
const GLenum eGL_ONE = (GLenum)1;

class WrappedOpenGL;
struct GLHookSet;

size_t BufferIdx(GLenum buf);
GLenum BufferEnum(size_t idx);

size_t QueryIdx(GLenum query);
GLenum QueryEnum(size_t idx);

size_t ShaderIdx(GLenum buf);
GLenum ShaderBit(size_t idx);
GLenum ShaderEnum(size_t idx);

ResourceFormat MakeResourceFormat(WrappedOpenGL &gl, GLenum target, GLenum fmt);
GLenum MakeGLFormat(WrappedOpenGL &gl, ResourceFormat fmt);
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
  ExtensionSupported_ARB_enhanced_layouts = 0,
  ExtensionSupported_ARB_clip_control,
  ExtensionSupported_EXT_polygon_offset_clamp,
  ExtensionSupported_KHR_blend_equation_advanced_coherent,
  ExtensionSupported_EXT_raster_multisample,
  ExtensionSupported_ARB_indirect_parameters,
  ExtensionSupported_EXT_depth_bounds_test,
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
  VendorCheck_AMD_copy_compressed_tinymips,
  VendorCheck_AMD_pipeline_compute_query,
  VendorCheck_NV_ClearNamedFramebufferfiBugs,
  VendorCheck_AMD_copy_compressed_cubemaps,
  VendorCheck_AMD_vertex_array_elem_buffer_query,
  VendorCheck_Count,
};
extern bool VendorCheck[VendorCheck_Count];

// fills out the extension supported array and the version-specific checks above
void DoVendorChecks(const GLHookSet &gl, GLWindowingData context);

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

enum GLChunkType
{
  DEVICE_INIT = FIRST_CHUNK_ID,

  GEN_TEXTURE,
  CREATE_TEXTURE,
  BIND_TEXTURE,
  BIND_TEXTURES,
  BIND_MULTI_TEX,
  BIND_TEXTURE_UNIT,
  BIND_IMAGE_TEXTURE,
  BIND_IMAGE_TEXTURES,
  ACTIVE_TEXTURE,
  TEXSTORAGE1D,
  TEXSTORAGE2D,
  TEXSTORAGE3D,
  TEXSTORAGE2DMS,
  TEXSTORAGE3DMS,
  TEXIMAGE1D,
  TEXIMAGE2D,
  TEXIMAGE3D,
  TEXSUBIMAGE1D,
  TEXSUBIMAGE2D,
  TEXSUBIMAGE3D,
  TEXIMAGE1D_COMPRESSED,
  TEXIMAGE2D_COMPRESSED,
  TEXIMAGE3D_COMPRESSED,
  TEXSUBIMAGE1D_COMPRESSED,
  TEXSUBIMAGE2D_COMPRESSED,
  TEXSUBIMAGE3D_COMPRESSED,
  TEXBUFFER,
  TEXBUFFER_RANGE,
  PIXELSTORE,
  TEXPARAMETERF,
  TEXPARAMETERFV,
  TEXPARAMETERI,
  TEXPARAMETERIV,
  TEXPARAMETERIIV,
  TEXPARAMETERIUIV,
  GENERATE_MIPMAP,
  COPY_SUBIMAGE,
  COPY_IMAGE1D,
  COPY_IMAGE2D,
  COPY_SUBIMAGE1D,
  COPY_SUBIMAGE2D,
  COPY_SUBIMAGE3D,
  TEXTURE_VIEW,

  CREATE_SHADER,
  CREATE_PROGRAM,
  CREATE_SHADERPROGRAM,
  COMPILESHADER,
  SHADERSOURCE,
  ATTACHSHADER,
  DETACHSHADER,
  USEPROGRAM,
  PROGRAMPARAMETER,
  FEEDBACK_VARYINGS,
  BINDATTRIB_LOCATION,
  BINDFRAGDATA_LOCATION,
  BINDFRAGDATA_LOCATION_INDEXED,
  UNIFORM_BLOCKBIND,
  STORAGE_BLOCKBIND,
  UNIFORM_SUBROUTINE,
  PROGRAMUNIFORM_VECTOR,
  PROGRAMUNIFORM_MATRIX,
  LINKPROGRAM,

  NAMEDSTRING,
  DELETENAMEDSTRING,
  COMPILESHADERINCLUDE,

  GEN_FEEDBACK,
  CREATE_FEEDBACK,
  BIND_FEEDBACK,
  BEGIN_FEEDBACK,
  END_FEEDBACK,
  PAUSE_FEEDBACK,
  RESUME_FEEDBACK,

  GEN_PROGRAMPIPE,
  CREATE_PROGRAMPIPE,
  USE_PROGRAMSTAGES,
  BIND_PROGRAMPIPE,

  FENCE_SYNC,
  CLIENTWAIT_SYNC,
  WAIT_SYNC,

  GEN_QUERIES,
  CREATE_QUERIES,
  BEGIN_QUERY,
  BEGIN_QUERY_INDEXED,
  END_QUERY,
  END_QUERY_INDEXED,
  BEGIN_CONDITIONAL,
  END_CONDITIONAL,
  QUERY_COUNTER,

  CLEAR_COLOR,
  CLEAR_DEPTH,
  CLEAR_STENCIL,
  CLEAR,
  CLEARBUFFERF,
  CLEARBUFFERI,
  CLEARBUFFERUI,
  CLEARBUFFERFI,
  CLEARBUFFERDATA,
  CLEARBUFFERSUBDATA,
  CLEARTEXIMAGE,
  CLEARTEXSUBIMAGE,
  POLYGON_MODE,
  POLYGON_OFFSET,
  POLYGON_OFFSET_CLAMP,
  CULL_FACE,
  HINT,
  ENABLE,
  DISABLE,
  ENABLEI,
  DISABLEI,
  FRONT_FACE,
  BLEND_FUNC,
  BLEND_FUNCI,
  BLEND_COLOR,
  BLEND_FUNC_SEP,
  BLEND_FUNC_SEPI,
  BLEND_EQ,
  BLEND_EQI,
  BLEND_EQ_SEP,
  BLEND_EQ_SEPI,
  BLEND_BARRIER,
  LOGIC_OP,
  STENCIL_OP,
  STENCIL_OP_SEP,
  STENCIL_FUNC,
  STENCIL_FUNC_SEP,
  STENCIL_MASK,
  STENCIL_MASK_SEP,
  COLOR_MASK,
  COLOR_MASKI,
  SAMPLE_MASK,
  SAMPLE_COVERAGE,
  MIN_SAMPLE_SHADING,
  RASTER_SAMPLES,
  DEPTH_FUNC,
  DEPTH_MASK,
  DEPTH_RANGE,
  DEPTH_RANGEF,
  DEPTH_RANGE_IDX,
  DEPTH_RANGEARRAY,
  DEPTH_BOUNDS,
  CLIP_CONTROL,
  PROVOKING_VERTEX,
  PRIMITIVE_RESTART,
  PATCH_PARAMI,
  PATCH_PARAMFV,
  LINE_WIDTH,
  POINT_SIZE,
  POINT_PARAMF,
  POINT_PARAMFV,
  POINT_PARAMI,
  POINT_PARAMIV,
  VIEWPORT,
  VIEWPORT_ARRAY,
  SCISSOR,
  SCISSOR_ARRAY,
  BIND_VERTEXBUFFER,
  BIND_VERTEXBUFFERS,
  VERTEXBINDINGDIVISOR,
  DISPATCH_COMPUTE,
  DISPATCH_COMPUTE_GROUP_SIZE,
  DISPATCH_COMPUTE_INDIRECT,
  MEMORY_BARRIER,
  MEMORY_BARRIER_BY_REGION,
  TEXTURE_BARRIER,
  DRAWARRAYS,
  DRAWARRAYS_INDIRECT,
  DRAWARRAYS_INSTANCED,
  DRAWARRAYS_INSTANCEDBASEINSTANCE,
  DRAWELEMENTS,
  DRAWELEMENTS_INDIRECT,
  DRAWRANGEELEMENTS,
  DRAWRANGEELEMENTSBASEVERTEX,
  DRAWELEMENTS_INSTANCED,
  DRAWELEMENTS_INSTANCEDBASEINSTANCE,
  DRAWELEMENTS_BASEVERTEX,
  DRAWELEMENTS_INSTANCEDBASEVERTEX,
  DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE,
  DRAW_FEEDBACK,
  DRAW_FEEDBACK_INSTANCED,
  DRAW_FEEDBACK_STREAM,
  DRAW_FEEDBACK_STREAM_INSTANCED,
  MULTI_DRAWARRAYS,
  MULTI_DRAWELEMENTS,
  MULTI_DRAWELEMENTSBASEVERTEX,
  MULTI_DRAWARRAYS_INDIRECT,
  MULTI_DRAWELEMENTS_INDIRECT,
  MULTI_DRAWARRAYS_INDIRECT_COUNT,
  MULTI_DRAWELEMENTS_INDIRECT_COUNT,

  GEN_FRAMEBUFFERS,
  CREATE_FRAMEBUFFERS,
  FRAMEBUFFER_TEX,
  FRAMEBUFFER_TEX1D,
  FRAMEBUFFER_TEX2D,
  FRAMEBUFFER_TEX3D,
  FRAMEBUFFER_RENDBUF,
  FRAMEBUFFER_TEXLAYER,
  FRAMEBUFFER_PARAM,
  READ_BUFFER,
  BIND_FRAMEBUFFER,
  DRAW_BUFFER,
  DRAW_BUFFERS,
  BLIT_FRAMEBUFFER,

  GEN_RENDERBUFFERS,
  CREATE_RENDERBUFFERS,
  RENDERBUFFER_STORAGE,
  RENDERBUFFER_STORAGEMS,

  GEN_SAMPLERS,
  CREATE_SAMPLERS,
  SAMPLER_PARAMETERI,
  SAMPLER_PARAMETERF,
  SAMPLER_PARAMETERIV,
  SAMPLER_PARAMETERFV,
  SAMPLER_PARAMETERIIV,
  SAMPLER_PARAMETERIUIV,
  BIND_SAMPLER,
  BIND_SAMPLERS,

  GEN_BUFFER,
  CREATE_BUFFER,
  BIND_BUFFER,
  BIND_BUFFER_BASE,
  BIND_BUFFER_RANGE,
  BIND_BUFFERS_BASE,
  BIND_BUFFERS_RANGE,
  BUFFERSTORAGE,
  BUFFERDATA,
  BUFFERSUBDATA,
  COPYBUFFERSUBDATA,
  UNMAP,
  FLUSHMAP,
  GEN_VERTEXARRAY,
  CREATE_VERTEXARRAY,
  BIND_VERTEXARRAY,
  VERTEXATTRIB_GENERIC,
  VERTEXATTRIBPOINTER,
  VERTEXATTRIBIPOINTER,
  VERTEXATTRIBLPOINTER,
  ENABLEVERTEXATTRIBARRAY,
  DISABLEVERTEXATTRIBARRAY,
  VERTEXATTRIBFORMAT,
  VERTEXATTRIBIFORMAT,
  VERTEXATTRIBLFORMAT,
  VERTEXATTRIBDIVISOR,
  VERTEXATTRIBBINDING,

  VAO_ELEMENT_BUFFER,
  FEEDBACK_BUFFER_BASE,
  FEEDBACK_BUFFER_RANGE,

  OBJECT_LABEL,
  BEGIN_EVENT,
  SET_MARKER,
  END_EVENT,

  DEBUG_MESSAGES,

  CAPTURE_SCOPE,
  CONTEXT_CAPTURE_HEADER,
  CONTEXT_CAPTURE_FOOTER,

  INTEROP_INIT,
  INTEROP_DATA,

  NUM_OPENGL_CHUNKS,
};
