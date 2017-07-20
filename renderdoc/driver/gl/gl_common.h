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
#include "maths/vec.h"

// typed enum so that templates will pick up specialisations
// header must be included before the official headers, so we/
// separate it out to avoid clang-format sorting them differently
#define GLenum RDCGLenum
#include "gl_enum.h"

// official headers
#include "official/glcorearb.h"
#include "official/glext.h"

#include "official/gl32.h"
// TODO there are some extensions which are in both headers but with different content
// however it does not seem to be a problem at this time
#include "official/glesext.h"

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

#ifdef RENDERDOC_SUPPORT_GL
// cheeky way to prevent GL/gl.h from being included, as we want to use
// glcorearb.h from above
#define __gl_h_
#include <GL/glx.h>
#include "official/glxext.h"
#endif
#if RENDERDOC_SUPPORT_GLES

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
    wnd = 0;
  }

  void SetCtx(void *c) { ctx = (GLContextPtr)c; }

#if defined(RENDERDOC_SUPPORT_GL)
  typedef Display *GLDisplayPtr;
  typedef GLXContext GLContextPtr;
  typedef GLXDrawable GLWindowPtr;
#else
  typedef void *GLDisplayPtr;
  typedef void *GLContextPtr;
  typedef void *GLWindowPtr;
#endif

#if defined(RENDERDOC_SUPPORT_GLES)
  typedef EGLDisplay GLESDisplayPtr;
  typedef EGLContext GLESContextPtr;
  typedef EGLSurface GLESWindowPtr;
#else
  typedef void *GLESDisplayPtr;
  typedef void *GLESContextPtr;
  typedef void *GLESWindowPtr;
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
    GLWindowPtr wnd;
    GLESWindowPtr egl_wnd;
  };
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
    egl_ctx = 0;
    egl_dpy = 0;
    egl_wnd = 0;
    wnd = 0;
  }

  void SetCtx(void *c) { egl_ctx = (void *)c; }
  union
  {
    // currently required to allow compatiblity with the driver parts
    void *ctx;
    EGLContext egl_ctx;
  };
  EGLDisplay egl_dpy;
  EGLSurface egl_wnd;
  ANativeWindow *wnd;
};

#else
#error "Unknown platform"
#endif

#include "api/replay/renderdoc_replay.h"

struct GLPlatform
{
  // simple wrapper for OS functions to make/delete a context
  virtual GLWindowingData MakeContext(GLWindowingData share) = 0;
  virtual void DeleteContext(GLWindowingData context) = 0;
  virtual void DeleteReplayContext(GLWindowingData context) = 0;
  virtual void MakeContextCurrent(GLWindowingData data) = 0;
  virtual void SwapBuffers(GLWindowingData context) = 0;
  virtual void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h) = 0;
  virtual bool IsOutputWindowVisible(GLWindowingData context) = 0;
  virtual GLWindowingData MakeOutputWindow(WindowingSystem system, void *data, bool depth,
                                           GLWindowingData share_context) = 0;

  // for 'backwards compatible' overlay rendering
  virtual bool DrawQuads(float width, float height, const std::vector<Vec4f> &vertices) = 0;
};

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

// replay only class for handling marker regions
struct GLMarkerRegion
{
  GLMarkerRegion(const std::string &marker);
  ~GLMarkerRegion();
  static void Set(const std::string &marker);

  static const GLHookSet *gl;
};

size_t GLTypeSize(GLenum type);

size_t BufferIdx(GLenum buf);
GLenum BufferEnum(size_t idx);

size_t QueryIdx(GLenum query);
GLenum QueryEnum(size_t idx);

size_t ShaderIdx(GLenum buf);
GLenum ShaderBit(size_t idx);
GLenum ShaderEnum(size_t idx);

ResourceFormat MakeResourceFormat(const GLHookSet &gl, GLenum target, GLenum fmt);
GLenum MakeGLFormat(WrappedOpenGL &gl, ResourceFormat fmt);
Topology MakePrimitiveTopology(const GLHookSet &gl, GLenum Topo);
GLenum MakeGLPrimitiveTopology(Topology Topo);
BufferCategory MakeBufferCategory(GLenum bufferTarget);
AddressMode MakeAddressMode(GLenum addr);
TextureFilter MakeFilter(GLenum minf, GLenum magf, bool shadowSampler, float maxAniso);
CompareFunc MakeCompareFunc(GLenum func);
StencilOp MakeStencilOp(GLenum op);
LogicOp MakeLogicOp(GLenum op);
BlendMultiplier MakeBlendMultiplier(GLenum blend);
BlendOp MakeBlendOp(GLenum op);
const char *BlendString(GLenum blendenum);
const char *SamplerString(GLenum smpenum);

void ClearGLErrors(const GLHookSet &gl);

GLuint GetBoundVertexBuffer(const GLHookSet &gl, GLuint idx);

void GetBindpointMapping(const GLHookSet &gl, GLuint curProg, int shadIdx, ShaderReflection *refl,
                         ShaderBindpointMapping &mapping);

extern int GLCoreVersion;
extern bool GLIsCore;
extern bool IsGLES;

// List of extensions and the versions when they became core (first column for GL, second column for
// GLES). In case of GLES compatible extensions and new features of the different versions are also
// taken into account.
// 99 means the extension never became core, so you can easily just do a check of CoreVersion >= NN
// and they will always fail.
#define EXTENSION_CHECKS()                                       \
  EXT_TO_CHECK(31, 99, ARB_texture_buffer_object)                \
  EXT_TO_CHECK(33, 30, ARB_explicit_attrib_location)             \
  EXT_TO_CHECK(33, 30, ARB_sampler_objects)                      \
  EXT_TO_CHECK(33, 30, ARB_texture_swizzle)                      \
  EXT_TO_CHECK(40, 32, ARB_draw_buffers_blend)                   \
  EXT_TO_CHECK(40, 31, ARB_draw_indirect)                        \
  EXT_TO_CHECK(40, 32, ARB_gpu_shader5)                          \
  EXT_TO_CHECK(40, 32, ARB_sample_shading)                       \
  EXT_TO_CHECK(40, 99, ARB_shader_subroutine)                    \
  EXT_TO_CHECK(40, 32, ARB_tessellation_shader)                  \
  EXT_TO_CHECK(40, 32, ARB_texture_cube_map_array)               \
  EXT_TO_CHECK(40, 30, ARB_transform_feedback2)                  \
  EXT_TO_CHECK(41, 99, ARB_geometry_shader4)                     \
  EXT_TO_CHECK(41, 31, ARB_separate_shader_objects)              \
  EXT_TO_CHECK(41, 99, ARB_viewport_array)                       \
  EXT_TO_CHECK(42, 99, ARB_base_instance)                        \
  EXT_TO_CHECK(42, 31, ARB_shader_atomic_counters)               \
  EXT_TO_CHECK(42, 31, ARB_shader_image_load_store)              \
  EXT_TO_CHECK(42, 31, ARB_shading_language_420pack)             \
  EXT_TO_CHECK(42, 30, ARB_texture_storage)                      \
  EXT_TO_CHECK(43, 99, ARB_clear_buffer_object)                  \
  EXT_TO_CHECK(43, 31, ARB_compute_shader)                       \
  EXT_TO_CHECK(43, 32, ARB_copy_image)                           \
  EXT_TO_CHECK(43, 30, ARB_ES3_compatibility)                    \
  EXT_TO_CHECK(43, 99, ARB_internalformat_query2)                \
  EXT_TO_CHECK(43, 31, ARB_program_interface_query)              \
  EXT_TO_CHECK(43, 31, ARB_shader_storage_buffer_object)         \
  EXT_TO_CHECK(43, 31, ARB_stencil_texturing)                    \
  EXT_TO_CHECK(43, 32, ARB_texture_storage_multisample)          \
  EXT_TO_CHECK(43, 99, ARB_texture_view)                         \
  EXT_TO_CHECK(43, 31, ARB_vertex_attrib_binding)                \
  EXT_TO_CHECK(43, 32, KHR_debug)                                \
  EXT_TO_CHECK(44, 99, ARB_enhanced_layouts)                     \
  EXT_TO_CHECK(44, 99, ARB_query_buffer_object)                  \
  EXT_TO_CHECK(45, 99, ARB_clip_control)                         \
  EXT_TO_CHECK(99, 99, ARB_indirect_parameters)                  \
  EXT_TO_CHECK(99, 99, ARB_seamless_cubemap_per_texture)         \
  EXT_TO_CHECK(99, 99, EXT_depth_bounds_test)                    \
  EXT_TO_CHECK(99, 99, EXT_direct_state_access)                  \
  EXT_TO_CHECK(99, 99, EXT_polygon_offset_clamp)                 \
  EXT_TO_CHECK(99, 99, EXT_raster_multisample)                   \
  EXT_TO_CHECK(99, 99, EXT_texture_filter_anisotropic)           \
  EXT_TO_CHECK(99, 30, EXT_texture_swizzle)                      \
  EXT_TO_CHECK(99, 99, KHR_blend_equation_advanced_coherent)     \
  /* OpenGL ES extensions */                                     \
  EXT_TO_CHECK(99, 32, EXT_color_buffer_float)                   \
  EXT_TO_CHECK(99, 32, EXT_primitive_bounding_box)               \
  EXT_TO_CHECK(99, 32, OES_primitive_bounding_box)               \
  EXT_TO_CHECK(99, 32, OES_texture_storage_multisample_2d_array) \
  EXT_TO_CHECK(99, 99, EXT_clip_cull_distance)                   \
  EXT_TO_CHECK(99, 99, EXT_multisample_compatibility)            \
  EXT_TO_CHECK(99, 99, NV_polygon_mode)                          \
  EXT_TO_CHECK(99, 99, NV_read_depth)                            \
  EXT_TO_CHECK(99, 99, NV_read_stencil)                          \
  EXT_TO_CHECK(99, 99, NV_read_depth_stencil)                    \
  EXT_TO_CHECK(99, 99, EXT_disjoint_timer_query)

// GL extensions and their roughly equivalent GLES alternatives
#define EXTENSION_COMPATIBILITY_CHECKS()                                                    \
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
  EXT_COMP_CHECK(ARB_texture_buffer_object, OES_texture_buffer)

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
  VendorCheck_Count,
};
extern bool VendorCheck[VendorCheck_Count];

// fills out the extension supported array and the version-specific checks above
void DoVendorChecks(const GLHookSet &gl, GLPlatform &platform, GLWindowingData context);
void CheckExtensions(const GLHookSet &gl);

// verify that we got a replay context that we can work with
bool CheckReplayContext(PFNGLGETSTRINGPROC getStr, PFNGLGETINTEGERVPROC getInt,
                        PFNGLGETSTRINGIPROC getStri);
bool ValidateFunctionPointers(const GLHookSet &real);

namespace glEmulate
{
void EmulateUnsupportedFunctions(GLHookSet *hooks);
void EmulateRequiredExtensions(GLHookSet *hooks);
};

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
void SerialiseProgramBindings(const GLHookSet &gl, Serialiser *ser, GLuint prog, bool writing);

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

  PRIMITIVE_BOUNDING_BOX,

  FRAMEBUFFER_TEX2DMS,

  NUM_OPENGL_CHUNKS,
};
