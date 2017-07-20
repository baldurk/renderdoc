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

#include "gl_driver.h"
#include <algorithm>
#include "common/common.h"
#include "data/glsl_shaders.h"
#include "driver/shaders/spirv/spirv_common.h"
#include "jpeg-compressor/jpge.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "replay/type_helpers.h"
#include "serialise/string_utils.h"
#include "stb/stb_truetype.h"

#define OPENGL 1
#include "data/glsl/debuguniforms.h"

const int firstChar = int(' ') + 1;
const int lastChar = 127;
const int numChars = lastChar - firstChar;
const float charPixelHeight = 20.0f;

stbtt_bakedchar chardata[numChars];

const char *GLChunkNames[] = {
    "WrappedOpenGL::Initialisation",

    "glGenTextures",
    "glCreateTextures",
    "glBindTexture",
    "glBindTextures",
    "glBindMultiTexture",
    "glBindTextureUnit",
    "glBindImageTexture",
    "glBindImageTextures",
    "glActiveTexture",
    "glTexStorage1D",
    "glTexStorage2D",
    "glTexStorage3D",
    "glTexStorage2DMultisample",
    "glTexStorage3DMultisample",
    "glTexImage1D",
    "glTexImage2D",
    "glTexImage3D",
    "glTexSubImage1D",
    "glTexSubImage2D",
    "glTexSubImage3D",
    "glCompressedTexImage1D",
    "glCompressedTexImage2D",
    "glCompressedTexImage3D",
    "glCompressedTexSubImage1D",
    "glCompressedTexSubImage2D",
    "glCompressedTexSubImage3D",
    "glTexBuffer",
    "glTexBufferRange",
    "glPixelStore",
    "glTexParameterf",
    "glTexParameterfv",
    "glTexParameteri",
    "glTexParameteriv",
    "glTexParameterIiv",
    "glTexParameterIuiv",
    "glGenerateMipmap",
    "glCopyImageSubData",
    "glCopyTexImage1D",
    "glCopyTexImage2D",
    "glCopyTexSubImage1D",
    "glCopyTexSubImage2D",
    "glCopyTexSubImage3D",
    "glTextureView",

    "glCreateShader",
    "glCreateProgram",
    "glCreateShaderProgramv",
    "glCompileShader",
    "glShaderSource",
    "glAttachShader",
    "glDetachShader",
    "glUseProgram",
    "glProgramParameter",
    "glTransformFeedbackVaryings",
    "glBindAttribLocation",
    "glBindFragDataLocation",
    "glBindFragDataLocationIndexed",
    "glUniformBlockBinding",
    "glShaderStorageBlockBinding",
    "glUniformSubroutinesuiv",
    "glProgramUniformVector*",
    "glProgramUniformMatrix*",
    "glLinkProgram",

    "glNamedStringARB",
    "glDeleteNamedStringARB",
    "glCompileShaderIncludeARB",

    "glGenTransformFeedbacks",
    "glCreateTransformFeedbacks",
    "glBindTransformFeedback",
    "glBeginTransformFeedback",
    "glEndTransformFeedback",
    "glPauseTransformFeedback",
    "glResumeTransformFeedback",

    "glGenProgramPipelines",
    "glCreateProgramPipelines",
    "glUseProgramStages",
    "glBindProgramPipeline",

    "glFenceSync",
    "glClientWaitSync",
    "glWaitSync",

    "glGenQueries",
    "glCreateQueries",
    "glBeginQuery",
    "glBeginQueryIndexed",
    "glEndQuery",
    "glEndQueryIndexed",
    "glBeginConditional",
    "glEndConditional",
    "glQueryCounter",

    "glClearColor",
    "glClearDepth",
    "glClearStencil",
    "glClear",
    "glClearBufferfv",
    "glClearBufferiv",
    "glClearBufferuiv",
    "glClearBufferfi",
    "glClearBufferData",
    "glClearBufferSubData",
    "glClearTexImage",
    "glClearTexSubImage",
    "glPolygonMode",
    "glPolygonOffset",
    "glPolygonOffsetClampEXT",
    "glCullFace",
    "glHint",
    "glEnable",
    "glDisable",
    "glEnablei",
    "glDisablei",
    "glFrontFace",
    "glBlendFunc",
    "glBlendFunci",
    "glBlendColor",
    "glBlendFuncSeparate",
    "glBlendFuncSeparatei",
    "glBlendEquation",
    "glBlendEquationi",
    "glBlendEquationSeparate",
    "glBlendEquationSeparatei",
    "glBlendBarrierKHR",
    "glLogicOp",
    "glStencilOp",
    "glStencilOpSeparate",
    "glStencilFunc",
    "glStencilFuncSeparate",
    "glStencilMask",
    "glStencilMaskSeparate",
    "glColorMask",
    "glColorMaski",
    "glSampleMaski",
    "glSampleCoverage",
    "glMinSampleShading",
    "glRasterSamplesEXT",
    "glDepthFunc",
    "glDepthMask",
    "glDepthRange",
    "glDepthRangef",
    "glDepthRangeIndexed",
    "glDepthRangeArrayv",
    "glDepthBounds",
    "glClipControl",
    "glProvokingVertex",
    "glPrimitiveRestartIndex",
    "glPatchParameteri",
    "glPatchParameterfv",
    "glLineWidth",
    "glPointSize",
    "glPointParameterf",
    "glPointParameterfv",
    "glPointParameteri",
    "glPointParameteriv",
    "glViewport",
    "glViewportArrayv",
    "glScissor",
    "glScissorArrayv",
    "glBindVertexBuffer",
    "glBindVertexBuffers",
    "glVertexBindingDivisor",
    "glDispatchCompute",
    "glDispatchComputeGroupSizeARB",
    "glDispatchComputeIndirect",
    "glMemoryBarrier",
    "glMemoryBarrierByRegion",
    "glTextureBarrier",
    "glDrawArrays",
    "glDrawArraysIndirect",
    "glDrawArraysInstanced",
    "glDrawArraysInstancedBaseInstance",
    "glDrawElements",
    "glDrawElementsIndirect",
    "glDrawRangeElements",
    "glDrawRangeElementsBaseVertex",
    "glDrawElementsInstanced",
    "glDrawElementsInstancedBaseInstance",
    "glDrawElementsBaseVertex",
    "glDrawElementsInstancedBaseVertex",
    "glDrawElementsInstancedBaseVertexBaseInstance",
    "glDrawTransformFeedback",
    "glDrawTransformFeedbackInstanced",
    "glDrawTransformFeedbackStream",
    "glDrawTransformFeedbackStreamInstanced",
    "glMultiDrawArrays",
    "glMultiDrawElements",
    "glMultiDrawElementsBaseVertex",
    "glMultiDrawArraysIndirect",
    "glMultiDrawElementsIndirect",
    "glMultiDrawArraysIndirectCountARB",
    "glMultiDrawElementsIndirectCountARB",

    "glGenFramebuffers",
    "glCreateFramebuffers",
    "glFramebufferTexture",
    "glFramebufferTexture1D",
    "glFramebufferTexture2D",
    "glFramebufferTexture3D",
    "glFramebufferRenderbuffer",
    "glFramebufferTextureLayer",
    "glFramebufferParameteri",
    "glReadBuffer",
    "glBindFramebuffer",
    "glDrawBuffer",
    "glDrawBuffers",
    "glBlitFramebuffer",

    "glGenRenderbuffers",
    "glCreateRenderbuffers",
    "glRenderbufferStorage",
    "glRenderbufferStorageMultisample",

    "glGenSamplers",
    "glCreateSamplers",
    "glSamplerParameteri",
    "glSamplerParameterf",
    "glSamplerParameteriv",
    "glSamplerParameterfv",
    "glSamplerParameterIiv",
    "glSamplerParameterIuiv",
    "glBindSampler",
    "glBindSamplers",

    "glGenBuffers",
    "glCreateBuffers",
    "glBindBuffer",
    "glBindBufferBase",
    "glBindBufferRange",
    "glBindBuffersBase",
    "glBindBuffersRange",
    "glBufferStorage",
    "glBufferData",
    "glBufferSubData",
    "glCopyBufferSubData",
    "glUnmapBuffer",
    "glFlushMappedBufferRange",
    "glGenVertexArrays",
    "glCreateVertexArrays",
    "glBindVertexArray",
    "glVertexAttrib*",
    "glVertexAttribPointer",
    "glVertexAttribIPointer",
    "glVertexAttribLPointer",
    "glEnableVertexAttribArray",
    "glDisableVertexAttribArray",
    "glVertexAttribFormat",
    "glVertexAttribIFormat",
    "glVertexAttribLFormat",
    "glVertexAttribDivisor",
    "glVertexAttribBinding",

    "glVertexArrayElementBuffer",
    "glTransformFeedbackBufferBase",
    "glTransformFeedbackBufferRange",

    "glObjectLabel",
    "glPushDebugGroup",
    "glDebugMessageInsert",
    "glPopDebugGroup",

    "DebugMessageList",

    "Capture",
    "BeginCapture",
    "EndCapture",

    "wglDXRegisterObjectNV",
    "wglDXLockObjectsNV",

    "glPrimitiveBoundingBox",

    "glFramebufferTexture2DMultisample",
};

GLInitParams::GLInitParams()
{
  SerialiseVersion = GL_SERIALISE_VERSION;
  colorBits = 32;
  depthBits = 32;
  stencilBits = 8;
  isSRGB = 1;
  multiSamples = 1;
  width = 32;
  height = 32;
}

// handling for these versions is scattered throughout the code (as relevant to enable/disable bits
// of serialisation and set some defaults if necessary).
// Here we list which non-current versions we support, and what changed
const uint32_t GLInitParams::GL_OLD_VERSIONS[GLInitParams::GL_NUM_SUPPORTED_OLD_VERSIONS] = {
    0x000010,    // from 0x10 to 0x11, we added a dummy marker value used to identify serialised
                 // data in glUseProgramStages (hack :( )
    0x000011,    // We added initial contents for buffers in this version, we don't have to do
                 // anything special to support older logs, just make sure we don't open new logs
                 // in an older version.
    0x000012,    // Added support for GL-DX interop
    0x000013,    // Serialised vertex attribute and fragdata bindings for programs as initial
                 // contents data
    0x000014,    // Added support for primitive bounding boxes on GLES
    0x000015,    // Changed serialisation of client-side index buffers which removed a bool even
                 // when they aren't used.
};

ReplayStatus GLInitParams::Serialise()
{
  SERIALISE_ELEMENT(uint32_t, ver, GL_SERIALISE_VERSION);
  SerialiseVersion = ver;

  if(ver != GL_SERIALISE_VERSION)
  {
    bool oldsupported = false;
    for(uint32_t i = 0; i < GL_NUM_SUPPORTED_OLD_VERSIONS; i++)
    {
      if(ver == GL_OLD_VERSIONS[i])
      {
        oldsupported = true;
        RDCWARN(
            "Old OpenGL serialise version %d, latest is %d. Loading with possibly degraded "
            "features/support.",
            ver, GL_SERIALISE_VERSION);
      }
    }

    if(!oldsupported)
    {
      RDCERR("Incompatible OpenGL serialise version, expected %d got %d", GL_SERIALISE_VERSION, ver);
      return ReplayStatus::APIIncompatibleVersion;
    }
  }

  m_pSerialiser->Serialise("Color bits", colorBits);
  m_pSerialiser->Serialise("Depth bits", depthBits);
  m_pSerialiser->Serialise("Stencil bits", stencilBits);
  m_pSerialiser->Serialise("Is SRGB", isSRGB);
  m_pSerialiser->Serialise("MSAA samples", multiSamples);
  m_pSerialiser->Serialise("Width", width);
  m_pSerialiser->Serialise("Height", height);

  return ReplayStatus::Succeeded;
}

void WrappedOpenGL::BuildGLExtensions()
{
  m_GLExtensions.push_back("GL_ARB_arrays_of_arrays");
  m_GLExtensions.push_back("GL_ARB_base_instance");
  m_GLExtensions.push_back("GL_ARB_blend_func_extended");
  m_GLExtensions.push_back("GL_ARB_buffer_storage");
  m_GLExtensions.push_back("GL_ARB_clear_buffer_object");
  m_GLExtensions.push_back("GL_ARB_clear_texture");
  m_GLExtensions.push_back("GL_ARB_clip_control");
  m_GLExtensions.push_back("GL_ARB_color_buffer_float");
  m_GLExtensions.push_back("GL_ARB_compressed_texture_pixel_storage");
  m_GLExtensions.push_back("GL_ARB_compute_shader");
  m_GLExtensions.push_back("GL_ARB_compute_variable_group_size");
  m_GLExtensions.push_back("GL_ARB_conditional_render_inverted");
  m_GLExtensions.push_back("GL_ARB_conservative_depth");
  m_GLExtensions.push_back("GL_ARB_copy_buffer");
  m_GLExtensions.push_back("GL_ARB_copy_image");
  m_GLExtensions.push_back("GL_ARB_cull_distance");
  m_GLExtensions.push_back("GL_ARB_debug_output");
  m_GLExtensions.push_back("GL_ARB_depth_buffer_float");
  m_GLExtensions.push_back("GL_ARB_depth_clamp");
  m_GLExtensions.push_back("GL_ARB_depth_texture");
  m_GLExtensions.push_back("GL_ARB_derivative_control");
  m_GLExtensions.push_back("GL_ARB_direct_state_access");
  m_GLExtensions.push_back("GL_ARB_draw_buffers");
  m_GLExtensions.push_back("GL_ARB_draw_buffers_blend");
  m_GLExtensions.push_back("GL_ARB_draw_elements_base_vertex");
  m_GLExtensions.push_back("GL_ARB_draw_indirect");
  m_GLExtensions.push_back("GL_ARB_draw_instanced");
  m_GLExtensions.push_back("GL_ARB_enhanced_layouts");
  m_GLExtensions.push_back("GL_ARB_ES2_compatibility");
  m_GLExtensions.push_back("GL_ARB_ES3_1_compatibility");
  m_GLExtensions.push_back("GL_ARB_ES3_compatibility");
  m_GLExtensions.push_back("GL_ARB_explicit_attrib_location");
  m_GLExtensions.push_back("GL_ARB_explicit_uniform_location");
  m_GLExtensions.push_back("GL_ARB_fragment_coord_conventions");
  m_GLExtensions.push_back("GL_ARB_fragment_layer_viewport");
  m_GLExtensions.push_back("GL_ARB_fragment_shader_interlock");
  m_GLExtensions.push_back("GL_ARB_framebuffer_no_attachments");
  m_GLExtensions.push_back("GL_ARB_framebuffer_object");
  m_GLExtensions.push_back("GL_ARB_framebuffer_sRGB");
  m_GLExtensions.push_back("GL_ARB_geometry_shader4");
  m_GLExtensions.push_back("GL_ARB_get_program_binary");
  m_GLExtensions.push_back("GL_ARB_get_texture_sub_image");
  m_GLExtensions.push_back("GL_ARB_gpu_shader_fp64");
  m_GLExtensions.push_back("GL_ARB_gpu_shader5");
  m_GLExtensions.push_back("GL_ARB_half_float_pixel");
  m_GLExtensions.push_back("GL_ARB_half_float_vertex");
  m_GLExtensions.push_back("GL_ARB_indirect_parameters");
  m_GLExtensions.push_back("GL_ARB_instanced_arrays");
  m_GLExtensions.push_back("GL_ARB_internalformat_query");
  m_GLExtensions.push_back("GL_ARB_internalformat_query2");
  m_GLExtensions.push_back("GL_ARB_invalidate_subdata");
  m_GLExtensions.push_back("GL_ARB_map_buffer_alignment");
  m_GLExtensions.push_back("GL_ARB_map_buffer_range");
  m_GLExtensions.push_back("GL_ARB_multi_bind");
  m_GLExtensions.push_back("GL_ARB_multi_draw_indirect");
  m_GLExtensions.push_back("GL_ARB_multisample");
  m_GLExtensions.push_back("GL_ARB_multitexture");
  m_GLExtensions.push_back("GL_ARB_occlusion_query");
  m_GLExtensions.push_back("GL_ARB_occlusion_query2");
  m_GLExtensions.push_back("GL_ARB_pixel_buffer_object");
  m_GLExtensions.push_back("GL_ARB_pipeline_statistics_query");
  m_GLExtensions.push_back("GL_ARB_point_parameters");
  m_GLExtensions.push_back("GL_ARB_point_sprite");
  m_GLExtensions.push_back("GL_ARB_post_depth_coverage");
  m_GLExtensions.push_back("GL_ARB_program_interface_query");
  m_GLExtensions.push_back("GL_ARB_provoking_vertex");
  m_GLExtensions.push_back("GL_ARB_query_buffer_object");
  m_GLExtensions.push_back("GL_ARB_robust_buffer_access_behavior");
  m_GLExtensions.push_back("GL_ARB_robustness");
  m_GLExtensions.push_back("GL_ARB_robustness_application_isolation");
  m_GLExtensions.push_back("GL_ARB_robustness_share_group_isolation");
  m_GLExtensions.push_back("GL_ARB_sample_shading");
  m_GLExtensions.push_back("GL_ARB_sampler_objects");
  m_GLExtensions.push_back("GL_ARB_seamless_cube_map");
  m_GLExtensions.push_back("GL_ARB_seamless_cubemap_per_texture");
  m_GLExtensions.push_back("GL_ARB_separate_shader_objects");
  m_GLExtensions.push_back("GL_ARB_shader_atomic_counters");
  m_GLExtensions.push_back("GL_ARB_shader_atomic_counter_ops");
  m_GLExtensions.push_back("GL_ARB_shader_ballot");
  m_GLExtensions.push_back("GL_ARB_shader_bit_encoding");
  m_GLExtensions.push_back("GL_ARB_shader_clock");
  m_GLExtensions.push_back("GL_ARB_shader_draw_parameters");
  m_GLExtensions.push_back("GL_ARB_shader_group_vote");
  m_GLExtensions.push_back("GL_ARB_shader_image_load_store");
  m_GLExtensions.push_back("GL_ARB_shader_image_size");
  m_GLExtensions.push_back("GL_ARB_shader_precision");
  m_GLExtensions.push_back("GL_ARB_shader_stencil_export");
  m_GLExtensions.push_back("GL_ARB_shader_storage_buffer_object");
  m_GLExtensions.push_back("GL_ARB_shader_subroutine");
  m_GLExtensions.push_back("GL_ARB_shader_texture_image_samples");
  m_GLExtensions.push_back("GL_ARB_shader_texture_lod");
  m_GLExtensions.push_back("GL_ARB_shader_viewport_layer_array");
  m_GLExtensions.push_back("GL_ARB_shading_language_100");
  m_GLExtensions.push_back("GL_ARB_shading_language_420pack");
  m_GLExtensions.push_back("GL_ARB_shading_language_include");
  m_GLExtensions.push_back("GL_ARB_shading_language_packing");
  m_GLExtensions.push_back("GL_ARB_shadow");
  m_GLExtensions.push_back("GL_ARB_shadow_ambient");
  m_GLExtensions.push_back("GL_ARB_stencil_texturing");
  m_GLExtensions.push_back("GL_ARB_sync");
  m_GLExtensions.push_back("GL_ARB_tessellation_shader");
  m_GLExtensions.push_back("GL_ARB_texture_barrier");
  m_GLExtensions.push_back("GL_ARB_texture_border_clamp");
  m_GLExtensions.push_back("GL_ARB_texture_buffer_object");
  m_GLExtensions.push_back("GL_ARB_texture_buffer_object_rgb32");
  m_GLExtensions.push_back("GL_ARB_texture_buffer_range");
  m_GLExtensions.push_back("GL_ARB_texture_compression");
  m_GLExtensions.push_back("GL_ARB_texture_compression_bptc");
  m_GLExtensions.push_back("GL_ARB_texture_compression_rgtc");
  m_GLExtensions.push_back("GL_ARB_texture_cube_map");
  m_GLExtensions.push_back("GL_ARB_texture_cube_map_array");
  m_GLExtensions.push_back("GL_ARB_texture_float");
  m_GLExtensions.push_back("GL_ARB_texture_gather");
  m_GLExtensions.push_back("GL_ARB_texture_mirror_clamp_to_edge");
  m_GLExtensions.push_back("GL_ARB_texture_mirrored_repeat");
  m_GLExtensions.push_back("GL_ARB_texture_multisample");
  m_GLExtensions.push_back("GL_ARB_texture_non_power_of_two");
  m_GLExtensions.push_back("GL_ARB_texture_query_levels");
  m_GLExtensions.push_back("GL_ARB_texture_query_lod");
  m_GLExtensions.push_back("GL_ARB_texture_rectangle");
  m_GLExtensions.push_back("GL_ARB_texture_rg");
  m_GLExtensions.push_back("GL_ARB_texture_rgb10_a2ui");
  m_GLExtensions.push_back("GL_ARB_texture_stencil8");
  m_GLExtensions.push_back("GL_ARB_texture_storage");
  m_GLExtensions.push_back("GL_ARB_texture_storage_multisample");
  m_GLExtensions.push_back("GL_ARB_texture_swizzle");
  m_GLExtensions.push_back("GL_ARB_texture_view");
  m_GLExtensions.push_back("GL_ARB_timer_query");
  m_GLExtensions.push_back("GL_ARB_transform_feedback_instanced");
  m_GLExtensions.push_back("GL_ARB_transform_feedback_overflow_query");
  m_GLExtensions.push_back("GL_ARB_transform_feedback2");
  m_GLExtensions.push_back("GL_ARB_transform_feedback3");
  m_GLExtensions.push_back("GL_ARB_uniform_buffer_object");
  m_GLExtensions.push_back("GL_ARB_vertex_array_bgra");
  m_GLExtensions.push_back("GL_ARB_vertex_array_object");
  m_GLExtensions.push_back("GL_ARB_vertex_attrib_64bit");
  m_GLExtensions.push_back("GL_ARB_vertex_attrib_binding");
  m_GLExtensions.push_back("GL_ARB_vertex_buffer_object");
  m_GLExtensions.push_back("GL_ARB_vertex_program");
  m_GLExtensions.push_back("GL_ARB_vertex_type_10f_11f_11f_rev");
  m_GLExtensions.push_back("GL_ARB_vertex_type_2_10_10_10_rev");
  m_GLExtensions.push_back("GL_ARB_viewport_array");
  m_GLExtensions.push_back("GL_EXT_bgra");
  m_GLExtensions.push_back("GL_EXT_blend_color");
  m_GLExtensions.push_back("GL_EXT_blend_equation_separate");
  m_GLExtensions.push_back("GL_EXT_blend_func_separate");
  m_GLExtensions.push_back("GL_EXT_blend_minmax");
  m_GLExtensions.push_back("GL_EXT_blend_subtract");
  m_GLExtensions.push_back("GL_EXT_debug_label");
  m_GLExtensions.push_back("GL_EXT_debug_marker");
  m_GLExtensions.push_back("GL_EXT_depth_bounds_test");
  m_GLExtensions.push_back("GL_EXT_direct_state_access");
  m_GLExtensions.push_back("GL_EXT_draw_buffers2");
  m_GLExtensions.push_back("GL_EXT_draw_instanced");
  m_GLExtensions.push_back("GL_EXT_draw_range_elements");
  m_GLExtensions.push_back("GL_EXT_framebuffer_blit");
  m_GLExtensions.push_back("GL_EXT_framebuffer_multisample");
  m_GLExtensions.push_back("GL_EXT_framebuffer_multisample_blit_scaled");
  m_GLExtensions.push_back("GL_EXT_framebuffer_object");
  m_GLExtensions.push_back("GL_EXT_framebuffer_sRGB");
  m_GLExtensions.push_back("GL_EXT_gpu_shader4");
  m_GLExtensions.push_back("GL_EXT_multisample");
  m_GLExtensions.push_back("GL_EXT_multi_draw_arrays");
  m_GLExtensions.push_back("GL_EXT_packed_depth_stencil");
  m_GLExtensions.push_back("GL_EXT_packed_float");
  m_GLExtensions.push_back("GL_EXT_pixel_buffer_object");
  m_GLExtensions.push_back("GL_EXT_pixel_buffer_object");
  m_GLExtensions.push_back("GL_EXT_point_parameters");
  m_GLExtensions.push_back("GL_EXT_polygon_offset_clamp");
  m_GLExtensions.push_back("GL_EXT_post_depth_coverage");
  m_GLExtensions.push_back("GL_EXT_provoking_vertex");
  m_GLExtensions.push_back("GL_EXT_raster_multisample");
  m_GLExtensions.push_back("GL_EXT_shader_image_load_store");
  m_GLExtensions.push_back("GL_EXT_shader_image_load_formatted");
  m_GLExtensions.push_back("GL_EXT_shader_integer_mix");
  m_GLExtensions.push_back("GL_EXT_shadow_funcs");
  m_GLExtensions.push_back("GL_EXT_stencil_wrap");
  m_GLExtensions.push_back("GL_EXT_texture_array");
  m_GLExtensions.push_back("GL_EXT_texture_buffer_object");
  m_GLExtensions.push_back("GL_EXT_texture_compression_dxt1");
  m_GLExtensions.push_back("GL_EXT_texture_compression_rgtc");
  m_GLExtensions.push_back("GL_EXT_texture_compression_s3tc");
  m_GLExtensions.push_back("GL_EXT_texture_cube_map");
  m_GLExtensions.push_back("GL_EXT_texture_edge_clamp");
  m_GLExtensions.push_back("GL_EXT_texture_filter_anisotropic");
  m_GLExtensions.push_back("GL_EXT_texture_filter_minmax");
  m_GLExtensions.push_back("GL_EXT_texture_integer");
  m_GLExtensions.push_back("GL_EXT_texture_lod_bias");
  m_GLExtensions.push_back("GL_EXT_texture_mirror_clamp");
  m_GLExtensions.push_back("GL_EXT_texture_shared_exponent");
  m_GLExtensions.push_back("GL_EXT_texture_snorm");
  m_GLExtensions.push_back("GL_EXT_texture_sRGB");
  m_GLExtensions.push_back("GL_EXT_texture_sRGB_decode");
  m_GLExtensions.push_back("GL_EXT_texture_swizzle");
  m_GLExtensions.push_back("GL_EXT_texture3D");
  m_GLExtensions.push_back("GL_EXT_timer_query");
  m_GLExtensions.push_back("GL_EXT_transform_feedback");
  m_GLExtensions.push_back("GL_EXT_vertex_attrib_64bit");
  m_GLExtensions.push_back("GL_GREMEDY_frame_terminator");
  m_GLExtensions.push_back("GL_GREMEDY_string_marker");
  m_GLExtensions.push_back("GL_KHR_blend_equation_advanced");
  m_GLExtensions.push_back("GL_KHR_blend_equation_advanced_coherent");
  m_GLExtensions.push_back("GL_KHR_context_flush_control");
  m_GLExtensions.push_back("GL_KHR_debug");
  m_GLExtensions.push_back("GL_KHR_no_error");
  m_GLExtensions.push_back("GL_KHR_robustness");
  m_GLExtensions.push_back("GL_KHR_robust_buffer_access_behavior");

  // this WGL extension is advertised in the gl ext string instead of via the wgl ext string,
  // return it just in case anyone is checking for it via this place. On non-windows platforms
  // it won't be reported as we do the intersection of renderdoc supported extensions and
  // implementation supported extensions.
  m_GLExtensions.push_back("WGL_EXT_swap_control");

  /************************************************************************

  Extensions I plan to support, but haven't implemented yet for one reason or another.
  Usually complexity/time considerations.

  Vendor specific extensions aren't listed here, or below in the 'will never support' list.
  Only very important/commonly used vendor extensions will be supported, generally I'll
  stick to ARB, EXT and KHR.

  * GL_ARB_bindless_texture
  * GL_ARB_cl_event
  * GL_ARB_sparse_buffer
  * GL_ARB_sparse_texture
  * GL_EXT_sparse_texture2
  * GL_ARB_sparse_texture2
  * GL_ARB_sparse_texture_clamp <- this one is free, but no point exposing until other spares exts
  * GL_EXT_x11_sync_object
  * GL_KHR_texture_compression_astc_hdr <- without support for astc textures on PC hardware this
  * GL_KHR_texture_compression_astc_ldr <- could be difficult. Maybe falls into the category of
  'only
                                           support if it's supported on replaying driver'?
  * GL_ARB_ES3_2_compatibility
  * GL_ARB_gpu_shader_int64
  * GL_ARB_parallel_shader_compile
  * GL_ARB_sample_locations
  * GL_ARB_texture_filter_minmax

  ************************************************************************/

  /************************************************************************

  Extensions I never plan to support due to only referring to old/outdated functionality listed
  below.

  I'm not sure what to do about GL_ARB_imaging, it seems like it's somewhat used in modern GL? For
  now
  I'm hoping I can get away with not reporting it but implementing the functionality it still
  describes.

  * GL_ARB_compatibility
  * GL_ARB_fragment_program
  * GL_ARB_fragment_program_shadow
  * GL_ARB_fragment_shader
  * GL_ARB_matrix_palette
  * GL_ARB_shader_objects
  * GL_ARB_texture_env_add
  * GL_ARB_texture_env_combine
  * GL_ARB_texture_env_crossbar
  * GL_ARB_texture_env_dot3
  * GL_ARB_transpose_matrix
  * GL_ARB_vertex_blend
  * GL_ARB_vertex_program
  * GL_ARB_vertex_shader
  * GL_ARB_window_pos
  * GL_ATI_draw_buffers
  * GL_ATI_texture_float
  * GL_ATI_texture_mirror_once
  * GL_EXT_422_pixels
  * GL_EXT_abgr
  * GL_EXT_bindable_uniform
  * GL_EXT_blend_logic_op
  * GL_EXT_Cg_shader
  * GL_EXT_clip_volume_hint
  * GL_EXT_cmyka
  * GL_EXT_color_subtable
  * GL_EXT_compiled_vertex_array
  * GL_EXT_convolution
  * GL_EXT_coordinate_frame
  * GL_EXT_copy_texture
  * GL_EXT_cull_vertex
  * GL_EXT_fog_coord
  * GL_EXT_fragment_lighting
  * GL_EXT_geometry_shader4
  * GL_EXT_gpu_program_parameters
  * GL_EXT_histogram
  * GL_EXT_import_sync_object
  * GL_EXT_index_array_formats
  * GL_EXT_index_func
  * GL_EXT_index_material
  * GL_EXT_index_texture
  * GL_EXT_light_texture
  * GL_EXT_misc_attribute
  * GL_EXT_packed_pixels
  * GL_EXT_paletted_texture
  * GL_EXT_pixel_transform
  * GL_EXT_pixel_transform_color_table
  * GL_EXT_rescale_normal
  * GL_EXT_scene_marker
  * GL_EXT_secondary_color
  * GL_EXT_separate_shader_objects
  * GL_EXT_separate_specular_color
  * GL_EXT_shared_texture_palette
  * GL_EXT_stencil_clear_tag
  * GL_EXT_stencil_two_side
  * GL_EXT_subtexture
  * GL_EXT_texture_compression_latc
  * GL_EXT_texture_env_add
  * GL_EXT_texture_env_combine
  * GL_EXT_texture_env_dot3
  * GL_EXT_texture_lod
  * GL_EXT_texture_object
  * GL_EXT_texture_perturb_normal
  * GL_EXT_texture_storage
  * GL_EXT_vertex_array
  * GL_EXT_vertex_array_bgra
  * GL_EXT_vertex_shader
  * GL_EXT_vertex_weighting
  * GL_S3_s3tc

  ************************************************************************/

  // we'll be sorting the implementation extension array, so make sure the
  // sorts are identical so we can do the intersection easily
  std::sort(m_GLExtensions.begin(), m_GLExtensions.end());
}

void WrappedOpenGL::BuildGLESExtensions()
{
  m_GLESExtensions.push_back("GL_ARM_rgba8");
  m_GLESExtensions.push_back("GL_EXT_base_instance");
  m_GLESExtensions.push_back("GL_EXT_blend_minmax");
  m_GLESExtensions.push_back("GL_EXT_clip_cull_distance");
  m_GLESExtensions.push_back("GL_EXT_color_buffer_float");
  m_GLESExtensions.push_back("GL_EXT_color_buffer_half_float");
  m_GLESExtensions.push_back("GL_EXT_copy_image");
  m_GLESExtensions.push_back("GL_EXT_debug_label");
  m_GLESExtensions.push_back("GL_EXT_debug_marker");
  m_GLESExtensions.push_back("GL_EXT_discard_framebuffer");
  m_GLESExtensions.push_back("GL_EXT_disjoint_timer_query");
  m_GLESExtensions.push_back("GL_EXT_draw_buffers");
  m_GLESExtensions.push_back("GL_EXT_draw_buffers_indexed");
  m_GLESExtensions.push_back("GL_EXT_draw_elements_base_vertex");
  m_GLESExtensions.push_back("GL_EXT_geometry_point_size");
  m_GLESExtensions.push_back("GL_EXT_geometry_shader");
  m_GLESExtensions.push_back("GL_EXT_gpu_shader5");
  m_GLESExtensions.push_back("GL_EXT_multisampled_render_to_texture");
  m_GLESExtensions.push_back("GL_EXT_primitive_bounding_box");
  m_GLESExtensions.push_back("GL_EXT_pvrtc_sRGB");
  m_GLESExtensions.push_back("GL_EXT_robustness");
  m_GLESExtensions.push_back("GL_EXT_separate_shader_objects");
  m_GLESExtensions.push_back("GL_EXT_shader_framebuffer_fetch");
  m_GLESExtensions.push_back("GL_EXT_shader_group_vote");
  m_GLESExtensions.push_back("GL_EXT_shader_implicit_conversions");
  m_GLESExtensions.push_back("GL_EXT_shader_integer_mix");
  m_GLESExtensions.push_back("GL_EXT_shader_io_blocks");
  m_GLESExtensions.push_back("GL_EXT_shader_non_constant_global_initializers");
  m_GLESExtensions.push_back("GL_EXT_shader_texture_lod");
  m_GLESExtensions.push_back("GL_EXT_shadow_samplers");
  m_GLESExtensions.push_back("GL_EXT_sRGB");
  m_GLESExtensions.push_back("GL_EXT_sRGB_write_control");
  m_GLESExtensions.push_back("GL_EXT_tessellation_shader");
  m_GLESExtensions.push_back("GL_EXT_texture_border_clamp");
  m_GLESExtensions.push_back("GL_EXT_texture_buffer");
  m_GLESExtensions.push_back("GL_EXT_texture_compression_astc_decode_mode");
  m_GLESExtensions.push_back("GL_EXT_texture_compression_dxt1");
  m_GLESExtensions.push_back("GL_EXT_texture_compression_s3tc");
  m_GLESExtensions.push_back("GL_EXT_texture_cube_map_array");
  m_GLESExtensions.push_back("GL_EXT_texture_filter_anisotropic");
  m_GLESExtensions.push_back("GL_EXT_texture_filter_minmax");
  m_GLESExtensions.push_back("GL_EXT_texture_format_BGRA8888");
  m_GLESExtensions.push_back("GL_EXT_texture_norm16");
  m_GLESExtensions.push_back("GL_EXT_texture_rg");
  m_GLESExtensions.push_back("GL_EXT_texture_sRGB_decode");
  m_GLESExtensions.push_back("GL_EXT_texture_sRGB_R8");
  m_GLESExtensions.push_back("GL_EXT_texture_sRGB_RG8");
  m_GLESExtensions.push_back("GL_EXT_texture_storage");
  m_GLESExtensions.push_back("GL_EXT_texture_type_2_10_10_10_REV");
  m_GLESExtensions.push_back("GL_KHR_blend_equation_advanced");
  m_GLESExtensions.push_back("GL_KHR_blend_equation_advanced_coherent");
  m_GLESExtensions.push_back("GL_KHR_context_flush_control");
  m_GLESExtensions.push_back("GL_KHR_debug");
  m_GLESExtensions.push_back("GL_KHR_no_error");
  m_GLESExtensions.push_back("GL_KHR_robust_buffer_access_behavior");
  m_GLESExtensions.push_back("GL_KHR_texture_compression_astc_hdr");
  m_GLESExtensions.push_back("GL_KHR_texture_compression_astc_ldr");
  m_GLESExtensions.push_back("GL_KHR_texture_compression_astc_sliced_3d");
  m_GLESExtensions.push_back("GL_OES_compressed_ETC1_RGB8_texture");
  m_GLESExtensions.push_back("GL_OES_copy_image");
  m_GLESExtensions.push_back("GL_OES_depth24");
  m_GLESExtensions.push_back("GL_OES_depth32");
  m_GLESExtensions.push_back("GL_OES_depth_texture");
  m_GLESExtensions.push_back("GL_OES_depth_texture_cube_map");
  m_GLESExtensions.push_back("GL_OES_draw_elements_base_vertex");
  m_GLESExtensions.push_back("GL_OES_fbo_render_mipmap");
  m_GLESExtensions.push_back("GL_OES_geometry_shader");
  m_GLESExtensions.push_back("GL_OES_gpu_shader5");
  m_GLESExtensions.push_back("GL_OES_mapbuffer");
  m_GLESExtensions.push_back("GL_OES_packed_depth_stencil");
  m_GLESExtensions.push_back("GL_OES_primitive_bounding_box");
  m_GLESExtensions.push_back("GL_OES_rgb8_rgba8");
  m_GLESExtensions.push_back("GL_OES_sample_shading");
  m_GLESExtensions.push_back("GL_OES_standard_derivatives");
  m_GLESExtensions.push_back("GL_OES_tessellation_shader");
  m_GLESExtensions.push_back("GL_OES_texture_3D");
  m_GLESExtensions.push_back("GL_OES_texture_border_clamp");
  m_GLESExtensions.push_back("GL_OES_texture_buffer");
  m_GLESExtensions.push_back("GL_OES_texture_compression_astc");
  m_GLESExtensions.push_back("GL_OES_texture_float");
  m_GLESExtensions.push_back("GL_OES_texture_float_linear");
  m_GLESExtensions.push_back("GL_OES_texture_half_float");
  m_GLESExtensions.push_back("GL_OES_texture_half_float_linear");
  m_GLESExtensions.push_back("GL_OES_texture_npot");
  m_GLESExtensions.push_back("GL_OES_texture_stencil8");
  m_GLESExtensions.push_back("GL_OES_texture_storage_multisample_2d_array");
  m_GLESExtensions.push_back("GL_OES_vertex_array_object");
  m_GLESExtensions.push_back("GL_OES_vertex_half_float");

  // advertise EGL extensions in the gl ext string, just in case anyone is checking it for
  // this way.
  m_GLESExtensions.push_back("EGL_KHR_create_context");
  m_GLESExtensions.push_back("EGL_KHR_surfaceless_context");

  // we'll be sorting the implementation extension array, so make sure the
  // sorts are identical so we can do the intersection easily
  std::sort(m_GLESExtensions.begin(), m_GLESExtensions.end());
}

WrappedOpenGL::WrappedOpenGL(const char *logfile, const GLHookSet &funcs, GLPlatform &platform)
    : m_Real(funcs), m_Platform(platform)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedOpenGL));

  BuildGLExtensions();
  BuildGLESExtensions();
  // by default we assume OpenGL driver
  SetDriverType(RDC_OpenGL);

  m_Replay.SetDriver(this);

  m_FrameCounter = 0;
  m_NoCtxFrames = 0;
  m_FailedFrame = 0;
  m_FailedReason = CaptureSucceeded;
  m_Failures = 0;
  m_SuccessfulCapture = true;
  m_FailureReason = CaptureSucceeded;

  m_AppControlledCapture = false;

  m_RealDebugFunc = NULL;
  m_RealDebugFuncParam = NULL;
  m_SuppressDebugMessages = false;

  m_DrawcallStack.push_back(&m_ParentDrawcall);

  m_CurEventID = 0;
  m_CurDrawcallID = 0;
  m_FirstEventID = 0;
  m_LastEventID = ~0U;

  m_FetchCounters = false;

  RDCEraseEl(m_ActiveQueries);
  m_ActiveConditional = false;
  m_ActiveFeedback = false;

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = READING;
    if(logfile)
    {
      m_pSerialiser = new Serialiser(logfile, Serialiser::READING, false);
    }
    else
    {
      byte dummy[4];
      m_pSerialiser = new Serialiser(4, dummy, false);
    }

    GLMarkerRegion::gl = &m_Real;

    // once GL driver is more tested, this can be disabled
    if(HasExt[KHR_debug] && m_Real.glDebugMessageCallback)
    {
      m_Real.glDebugMessageCallback(&DebugSnoopStatic, this);
#if ENABLED(RDOC_DEVEL)
      m_Real.glEnable(eGL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
    }
  }
  else
  {
    m_State = WRITING_IDLE;
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, false);
  }

  m_DeviceRecord = NULL;

  m_ResourceManager = new GLResourceManager(m_State, m_pSerialiser, this);

  m_DeviceResourceID =
      GetResourceManager()->RegisterResource(GLResource(NULL, eResSpecial, eSpecialResDevice));
  m_ContextResourceID =
      GetResourceManager()->RegisterResource(GLResource(NULL, eResSpecial, eSpecialResContext));

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_DeviceRecord = GetResourceManager()->AddResourceRecord(m_DeviceResourceID);
    m_DeviceRecord->DataInSerialiser = false;
    m_DeviceRecord->Length = 0;
    m_DeviceRecord->SpecialResource = true;

    m_ContextRecord = GetResourceManager()->AddResourceRecord(m_ContextResourceID);
    m_ContextRecord->DataInSerialiser = false;
    m_ContextRecord->Length = 0;
    m_ContextRecord->SpecialResource = true;

    // register VAO 0 as a special VAO, so that it can be tracked if the app uses it
    // we immediately mark it dirty since the vertex array tracking functions expect a proper VAO
    m_FakeVAOID = GetResourceManager()->RegisterResource(VertexArrayRes(NULL, 0));
    GetResourceManager()->AddResourceRecord(m_FakeVAOID);
    GetResourceManager()->MarkDirtyResource(m_FakeVAOID);
  }
  else
  {
    m_DeviceRecord = m_ContextRecord = NULL;

    ResourceIDGen::SetReplayResourceIDs();

    InitSPIRVCompiler();
    RenderDoc::Inst().RegisterShutdownFunction(&ShutdownSPIRVCompiler);
  }

  m_FakeBB_FBO = 0;
  m_FakeBB_Color = 0;
  m_FakeBB_DepthStencil = 0;
  m_FakeVAO = 0;
  m_FakeIdxSize = 0;

  m_CurChunkOffset = 0;
  m_AddedDrawcall = false;

  RDCDEBUG("Debug Text enabled - for development! remove before release!");
  m_pSerialiser->SetDebugText(true);

  m_pSerialiser->SetChunkNameLookup(&GetChunkName);

  //////////////////////////////////////////////////////////////////////////
  // Compile time asserts

  RDCCOMPILE_ASSERT(ARRAY_COUNT(GLChunkNames) == NUM_OPENGL_CHUNKS - FIRST_CHUNK_ID,
                    "Not right number of chunk names");
}

void WrappedOpenGL::Initialise(GLInitParams &params)
{
  // deliberately want to go through our own wrappers to set up e.g. m_Textures members
  WrappedOpenGL &gl = *this;

  m_InitParams = params;

  // as a concession to compatibility, generate a 'fake' VBO to act as VBO 0.
  // consider making it an error/warning for programs to use this?
  gl.glGenVertexArrays(1, &m_FakeVAO);
  gl.glBindVertexArray(m_FakeVAO);
  gl.glBindVertexArray(0);

  gl.glGenFramebuffers(1, &m_FakeBB_FBO);
  gl.glBindFramebuffer(eGL_FRAMEBUFFER, m_FakeBB_FBO);

  GLenum colfmt = eGL_RGBA8;

  if(params.colorBits == 32)
    colfmt = params.isSRGB ? eGL_SRGB8_ALPHA8 : eGL_RGBA8;
  else if(params.colorBits == 24)
    colfmt = params.isSRGB ? eGL_SRGB8 : eGL_RGB8;
  else
    RDCERR("Unexpected # colour bits: %d", params.colorBits);

  GLenum target = eGL_TEXTURE_2D;
  if(params.multiSamples > 1)
    target = eGL_TEXTURE_2D_MULTISAMPLE;

  gl.glGenTextures(1, &m_FakeBB_Color);
  gl.glBindTexture(target, m_FakeBB_Color);

  GetResourceManager()->SetName(GetResourceManager()->GetID(TextureRes(GetCtx(), m_FakeBB_Color)),
                                "Backbuffer Color");

  if(params.multiSamples > 1)
  {
    gl.glTextureStorage2DMultisampleEXT(m_FakeBB_Color, target, params.multiSamples, colfmt,
                                        params.width, params.height, true);
  }
  else
  {
    gl.glTextureImage2DEXT(m_FakeBB_Color, target, 0, colfmt, params.width, params.height, 0,
                           GetBaseFormat(colfmt), GetDataType(colfmt), NULL);
    gl.glTexParameteri(target, eGL_TEXTURE_MAX_LEVEL, 0);
    gl.glTexParameteri(target, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    gl.glTexParameteri(target, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    gl.glTexParameteri(target, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    gl.glTexParameteri(target, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  }
  gl.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, target, m_FakeBB_Color, 0);

  gl.glViewport(0, 0, params.width, params.height);

  m_FakeBB_DepthStencil = 0;
  if(params.depthBits > 0 || params.stencilBits > 0)
  {
    gl.glGenTextures(1, &m_FakeBB_DepthStencil);
    gl.glBindTexture(target, m_FakeBB_DepthStencil);

    GLenum depthfmt = eGL_DEPTH32F_STENCIL8;
    bool stencil = false;

    if(params.stencilBits == 8)
    {
      stencil = true;

      if(params.depthBits == 32)
        depthfmt = eGL_DEPTH32F_STENCIL8;
      else if(params.depthBits == 24)
        depthfmt = eGL_DEPTH24_STENCIL8;
      else
        RDCERR("Unexpected combination of depth & stencil bits: %d & %d", params.depthBits,
               params.stencilBits);
    }
    else if(params.stencilBits == 0)
    {
      if(params.depthBits == 32)
        depthfmt = eGL_DEPTH_COMPONENT32F;
      else if(params.depthBits == 24)
        depthfmt = eGL_DEPTH_COMPONENT24;
      else if(params.depthBits == 16)
        depthfmt = eGL_DEPTH_COMPONENT16;
      else
        RDCERR("Unexpected # depth bits: %d", params.depthBits);
    }
    else
      RDCERR("Unexpected # stencil bits: %d", params.stencilBits);

    GetResourceManager()->SetName(
        GetResourceManager()->GetID(TextureRes(GetCtx(), m_FakeBB_DepthStencil)),
        stencil ? "Backbuffer Depth-stencil" : "Backbuffer Depth");

    if(params.multiSamples > 1)
    {
      gl.glTextureStorage2DMultisampleEXT(m_FakeBB_DepthStencil, target, params.multiSamples,
                                          depthfmt, params.width, params.height, true);
    }
    else
    {
      gl.glTexParameteri(target, eGL_TEXTURE_MAX_LEVEL, 0);
      gl.glTextureImage2DEXT(m_FakeBB_DepthStencil, target, 0, depthfmt, params.width,
                             params.height, 0, GetBaseFormat(depthfmt), GetDataType(depthfmt), NULL);
    }

    if(stencil)
      gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, m_FakeBB_DepthStencil,
                              0);
    else
      gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, m_FakeBB_DepthStencil, 0);
  }

  // give the backbuffer a default clear color
  gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl.glClear(GL_COLOR_BUFFER_BIT);

  if(params.depthBits > 0)
  {
    gl.glClearDepthf(1.0f);
    gl.glClear(GL_DEPTH_BUFFER_BIT);
  }

  if(params.stencilBits > 0)
  {
    gl.glClearStencil(0);
    gl.glClear(GL_STENCIL_BUFFER_BIT);
  }
}

const char *WrappedOpenGL::GetChunkName(uint32_t idx)
{
  if(idx == CREATE_PARAMS)
    return "Create Params";
  if(idx == THUMBNAIL_DATA)
    return "Thumbnail Data";
  if(idx == DRIVER_INIT_PARAMS)
    return "Driver Init Params";
  if(idx == INITIAL_CONTENTS)
    return "Initial Contents";
  if(idx < FIRST_CHUNK_ID || idx >= NUM_OPENGL_CHUNKS)
    return "<unknown>";
  return GLChunkNames[idx - FIRST_CHUNK_ID];
}

template <>
string ToStrHelper<false, GLChunkType>::Get(const GLChunkType &el)
{
  return WrappedOpenGL::GetChunkName(el);
}

WrappedOpenGL::~WrappedOpenGL()
{
  if(m_FakeVAO)
    m_Real.glDeleteVertexArrays(1, &m_FakeVAO);
  if(m_FakeBB_FBO)
    m_Real.glDeleteFramebuffers(1, &m_FakeBB_FBO);
  if(m_FakeBB_Color)
    m_Real.glDeleteTextures(1, &m_FakeBB_Color);
  if(m_FakeBB_DepthStencil)
    m_Real.glDeleteTextures(1, &m_FakeBB_DepthStencil);

  SAFE_DELETE(m_pSerialiser);

  GetResourceManager()->ReleaseCurrentResource(m_DeviceResourceID);
  GetResourceManager()->ReleaseCurrentResource(m_ContextResourceID);

  if(m_ContextRecord)
  {
    RDCASSERT(m_ContextRecord->GetRefCount() == 1);
    m_ContextRecord->Delete(GetResourceManager());
  }

  if(m_DeviceRecord)
  {
    RDCASSERT(m_DeviceRecord->GetRefCount() == 1);
    m_DeviceRecord->Delete(GetResourceManager());
  }

  m_ResourceManager->Shutdown();

  SAFE_DELETE(m_ResourceManager);

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

void *WrappedOpenGL::GetCtx()
{
  return (void *)m_ActiveContexts[Threading::GetCurrentID()].ctx;
}

WrappedOpenGL::ContextData &WrappedOpenGL::GetCtxData()
{
  return m_ContextData[GetCtx()];
}

// defined in gl_<platform>_hooks.cpp
Threading::CriticalSection &GetGLLock();

////////////////////////////////////////////////////////////////
// Windowing/setup/etc
////////////////////////////////////////////////////////////////

void WrappedOpenGL::DeleteContext(void *contextHandle)
{
  ContextData &ctxdata = m_ContextData[contextHandle];

  RenderDoc::Inst().RemoveDeviceFrameCapturer(ctxdata.ctx);

  if(ctxdata.built && ctxdata.ready)
  {
    if(ctxdata.Program)
      m_Real.glDeleteProgram(ctxdata.Program);
    if(ctxdata.GeneralUBO)
      m_Real.glDeleteBuffers(1, &ctxdata.GeneralUBO);
    if(ctxdata.GlyphUBO)
      m_Real.glDeleteBuffers(1, &ctxdata.GlyphUBO);
    if(ctxdata.StringUBO)
      m_Real.glDeleteBuffers(1, &ctxdata.StringUBO);
    if(ctxdata.GlyphTexture)
      m_Real.glDeleteTextures(1, &ctxdata.GlyphTexture);
  }

  if(ctxdata.m_ClientMemoryVBOs[0])
    glDeleteBuffers(ARRAY_COUNT(ctxdata.m_ClientMemoryVBOs), ctxdata.m_ClientMemoryVBOs);
  if(ctxdata.m_ClientMemoryIBO)
    glDeleteBuffers(1, &ctxdata.m_ClientMemoryIBO);

  for(auto it = m_LastContexts.begin(); it != m_LastContexts.end(); ++it)
  {
    if(it->ctx == contextHandle)
    {
      m_LastContexts.erase(it);
      break;
    }
  }

  m_ContextData.erase(contextHandle);
}

void WrappedOpenGL::ContextData::UnassociateWindow(void *wndHandle)
{
  auto it = windows.find(wndHandle);
  if(it != windows.end())
  {
    windows.erase(wndHandle);
    RenderDoc::Inst().RemoveFrameCapturer(ctx, wndHandle);
  }
}

void WrappedOpenGL::ContextData::AssociateWindow(WrappedOpenGL *gl, void *wndHandle)
{
  auto it = windows.find(wndHandle);
  if(it == windows.end())
    RenderDoc::Inst().AddFrameCapturer(ctx, wndHandle, gl);

  windows[wndHandle] = Timing::GetUnixTimestamp();
}

void WrappedOpenGL::ContextData::CreateDebugData(const GLHookSet &gl)
{
  // to let us display the overlay on old GL contexts, use as simple a subset of functionality as
  // possible
  // to upload the texture. VAO and shaders are used optionally on modern contexts, otherwise we
  // fall back
  // to immediate mode rendering by hand
  if(gl.glGetIntegerv && gl.glGenTextures && gl.glBindTexture && gl.glTexImage2D && gl.glTexParameteri)
  {
    string ttfstring = GetEmbeddedResource(sourcecodepro_ttf);
    byte *ttfdata = (byte *)ttfstring.c_str();

    byte *buf = new byte[FONT_TEX_WIDTH * FONT_TEX_HEIGHT];

    stbtt_BakeFontBitmap(ttfdata, 0, charPixelHeight, buf, FONT_TEX_WIDTH, FONT_TEX_HEIGHT,
                         firstChar, numChars, chardata);

    CharSize = charPixelHeight;
    CharAspect = chardata->xadvance / charPixelHeight;

    stbtt_fontinfo f = {0};
    stbtt_InitFont(&f, ttfdata, 0);

    int ascent = 0;
    stbtt_GetFontVMetrics(&f, &ascent, NULL, NULL);

    float maxheight = float(ascent) * stbtt_ScaleForPixelHeight(&f, charPixelHeight);

    {
      PixelUnpackState unpack;

      unpack.Fetch(&gl, false);

      ResetPixelUnpackState(gl, false, 1);

      GLuint curtex = 0;
      gl.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint *)&curtex);

      GLenum texFmt = eGL_R8;
      if(Legacy())
        texFmt = eGL_LUMINANCE;

      gl.glGenTextures(1, &GlyphTexture);
      gl.glBindTexture(eGL_TEXTURE_2D, GlyphTexture);
      gl.glTexImage2D(eGL_TEXTURE_2D, 0, texFmt, FONT_TEX_WIDTH, FONT_TEX_HEIGHT, 0, eGL_RED,
                      eGL_UNSIGNED_BYTE, buf);
      gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
      gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_LINEAR);
      gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_LINEAR);

      gl.glBindTexture(eGL_TEXTURE_2D, curtex);

      unpack.Apply(&gl, false);
    }

    delete[] buf;

    Vec4f glyphData[2 * (numChars + 1)];

    for(int i = 0; i < numChars; i++)
    {
      stbtt_bakedchar *b = chardata + i;

      float x = b->xoff;
      float y = b->yoff + maxheight;

      glyphData[(i + 1) * 2 + 0] =
          Vec4f(x / b->xadvance, y / charPixelHeight, b->xadvance / float(b->x1 - b->x0),
                charPixelHeight / float(b->y1 - b->y0));
      glyphData[(i + 1) * 2 + 1] = Vec4f(b->x0, b->y0, b->x1, b->y1);
    }

    if(Modern() && gl.glGenVertexArrays && gl.glBindVertexArray)
    {
      GLuint curvao = 0;
      gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&curvao);

      gl.glGenVertexArrays(1, &DummyVAO);
      gl.glBindVertexArray(DummyVAO);

      gl.glBindVertexArray(curvao);
    }

    if(Modern() && gl.glGenBuffers && gl.glBufferData && gl.glBindBuffer)
    {
      GLuint curubo = 0;
      gl.glGetIntegerv(eGL_UNIFORM_BUFFER_BINDING, (GLint *)&curubo);

      gl.glGenBuffers(1, &GlyphUBO);
      gl.glBindBuffer(eGL_UNIFORM_BUFFER, GlyphUBO);
      gl.glBufferData(eGL_UNIFORM_BUFFER, sizeof(glyphData), glyphData, eGL_STATIC_DRAW);

      gl.glGenBuffers(1, &GeneralUBO);
      gl.glBindBuffer(eGL_UNIFORM_BUFFER, GeneralUBO);
      gl.glBufferData(eGL_UNIFORM_BUFFER, sizeof(FontUBOData), NULL, eGL_DYNAMIC_DRAW);

      gl.glGenBuffers(1, &StringUBO);
      gl.glBindBuffer(eGL_UNIFORM_BUFFER, StringUBO);
      gl.glBufferData(eGL_UNIFORM_BUFFER, sizeof(uint32_t) * 4 * FONT_MAX_CHARS, NULL,
                      eGL_DYNAMIC_DRAW);

      gl.glBindBuffer(eGL_UNIFORM_BUFFER, curubo);
    }

    if(Modern() && gl.glCreateShader && gl.glShaderSource && gl.glCompileShader &&
       gl.glGetShaderiv && gl.glGetShaderInfoLog && gl.glDeleteShader && gl.glCreateProgram &&
       gl.glAttachShader && gl.glLinkProgram && gl.glGetProgramiv && gl.glGetProgramInfoLog)
    {
      vector<string> vs;
      vector<string> fs;

      ShaderType shaderType;
      int glslVersion;
      string fragDefines;

      if(IsGLES)
      {
        shaderType = eShaderGLSLES;
        glslVersion = 310;
        fragDefines = "";
      }
      else
      {
        shaderType = eShaderGLSL;
        glslVersion = 150;
        fragDefines =
            "#extension GL_ARB_shading_language_420pack : require\n"
            "#extension GL_ARB_separate_shader_objects : require\n"
            "#extension GL_ARB_explicit_attrib_location : require\n";
      }

      GenerateGLSLShader(vs, shaderType, "", GetEmbeddedResource(glsl_text_vert), glslVersion);
      GenerateGLSLShader(fs, shaderType, fragDefines, GetEmbeddedResource(glsl_text_frag),
                         glslVersion);

      vector<const char *> vsc;
      vsc.reserve(vs.size());
      vector<const char *> fsc;
      fsc.reserve(fs.size());

      for(size_t i = 0; i < vs.size(); i++)
        vsc.push_back(vs[i].c_str());

      for(size_t i = 0; i < fs.size(); i++)
        fsc.push_back(fs[i].c_str());

      GLuint vert = gl.glCreateShader(eGL_VERTEX_SHADER);
      GLuint frag = gl.glCreateShader(eGL_FRAGMENT_SHADER);

      gl.glShaderSource(vert, (GLsizei)vs.size(), &vsc[0], NULL);
      gl.glShaderSource(frag, (GLsizei)fs.size(), &fsc[0], NULL);

      gl.glCompileShader(vert);
      gl.glCompileShader(frag);

      char buffer[1024] = {0};
      GLint status = 0;

      gl.glGetShaderiv(vert, eGL_COMPILE_STATUS, &status);
      if(status == 0)
      {
        gl.glGetShaderInfoLog(vert, 1024, NULL, buffer);
        RDCERR("Shader error: %s", buffer);
      }

      gl.glGetShaderiv(frag, eGL_COMPILE_STATUS, &status);
      if(status == 0)
      {
        gl.glGetShaderInfoLog(frag, 1024, NULL, buffer);
        RDCERR("Shader error: %s", buffer);
      }

      Program = gl.glCreateProgram();

      gl.glAttachShader(Program, vert);
      gl.glAttachShader(Program, frag);

      gl.glLinkProgram(Program);

      gl.glGetProgramiv(Program, eGL_LINK_STATUS, &status);
      if(status == 0)
      {
        gl.glGetProgramInfoLog(Program, 1024, NULL, buffer);
        RDCERR("Link error: %s", buffer);
      }

      gl.glDeleteShader(vert);
      gl.glDeleteShader(frag);
    }

    ready = true;
  }
}

void WrappedOpenGL::CreateContext(GLWindowingData winData, void *shareContext,
                                  GLInitParams initParams, bool core, bool attribsCreate)
{
  // TODO: support multiple GL contexts more explicitly
  m_InitParams = initParams;

  ContextData &ctxdata = m_ContextData[winData.ctx];
  ctxdata.ctx = winData.ctx;
  ctxdata.isCore = core;
  ctxdata.attribsCreate = attribsCreate;

  RenderDoc::Inst().AddDeviceFrameCapturer(ctxdata.ctx, this);
}

void WrappedOpenGL::RegisterContext(GLWindowingData winData, void *shareContext, bool core,
                                    bool attribsCreate)
{
  ContextData &ctxdata = m_ContextData[winData.ctx];
  ctxdata.ctx = winData.ctx;
  ctxdata.isCore = core;
  ctxdata.attribsCreate = attribsCreate;
}

void WrappedOpenGL::ActivateContext(GLWindowingData winData)
{
  m_ActiveContexts[Threading::GetCurrentID()] = winData;
  if(winData.ctx)
  {
    for(auto it = m_LastContexts.begin(); it != m_LastContexts.end(); ++it)
    {
      if(it->ctx == winData.ctx)
      {
        m_LastContexts.erase(it);
        break;
      }
    }

    m_LastContexts.push_back(winData);

    if(m_LastContexts.size() > 10)
      m_LastContexts.erase(m_LastContexts.begin());
  }

  // TODO: support multiple GL contexts more explicitly
  Keyboard::AddInputWindow((void *)winData.wnd);

  if(winData.ctx)
  {
    // if we're capturing, we need to serialise out the changed state vector
    if(m_State == WRITING_CAPFRAME)
    {
      // fetch any initial states needed. Note this is insufficient, and doesn't handle the case
      // where
      // we might just suddenly start getting commands on a thread that already has a context
      // active.
      // For now we assume we'll only get GL commands from a single thread
      QueuedInitialStateFetch fetch;
      fetch.res.Context = winData.ctx;
      auto it = std::lower_bound(m_QueuedInitialFetches.begin(), m_QueuedInitialFetches.end(), fetch);
      for(; it != m_QueuedInitialFetches.end();)
      {
        GetResourceManager()->Prepare_InitialState(it->res, it->blob);
        it = m_QueuedInitialFetches.erase(it);
      }

      SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_HEADER);
      Serialise_BeginCaptureFrame(false);
      m_ContextRecord->AddChunk(scope.Get());
    }

    ContextData &ctxdata = m_ContextData[winData.ctx];

    if(!ctxdata.built)
    {
      ctxdata.built = true;

      const vector<string> &globalExts = IsGLES ? m_GLESExtensions : m_GLExtensions;
      const GLHookSet &gl = m_Real;

      if(HasExt[KHR_debug] && gl.glDebugMessageCallback &&
         RenderDoc::Inst().GetCaptureOptions().APIValidation)
      {
        gl.glDebugMessageCallback(&DebugSnoopStatic, this);
        gl.glEnable(eGL_DEBUG_OUTPUT_SYNCHRONOUS);
      }

      vector<string> implExts;

      if(gl.glGetIntegerv && gl.glGetStringi)
      {
        GLuint numExts = 0;
        gl.glGetIntegerv(eGL_NUM_EXTENSIONS, (GLint *)&numExts);

        for(GLuint i = 0; i < numExts; i++)
          implExts.push_back((const char *)gl.glGetStringi(eGL_EXTENSIONS, i));
      }
      else if(gl.glGetString)
      {
        string implExtString = (const char *)gl.glGetString(eGL_EXTENSIONS);

        split(implExtString, implExts, ' ');
      }
      else
      {
        RDCERR("No functions to fetch implementation's extensions!");
      }

      std::sort(implExts.begin(), implExts.end());

      // intersection of implExts and globalExts into ctx.glExts
      {
        for(size_t i = 0, j = 0; i < implExts.size() && j < globalExts.size();)
        {
          const string &a = implExts[i];
          const string &b = globalExts[j];

          if(a == b)
          {
            ctxdata.glExts.push_back(a);
            i++;
            j++;
          }
          else if(a < b)
          {
            i++;
          }
          else if(b < a)
          {
            j++;
          }
        }
      }

      // this extension is something RenderDoc will support even if the impl
      // doesn't. https://renderdoc.org/debug_tool.txt
      ctxdata.glExts.push_back("GL_EXT_debug_tool");

      merge(ctxdata.glExts, ctxdata.glExtsString, ' ');

      if(gl.glGetIntegerv)
      {
        GLint mj = 0, mn = 0;
        gl.glGetIntegerv(eGL_MAJOR_VERSION, &mj);
        gl.glGetIntegerv(eGL_MINOR_VERSION, &mn);

        int ver = mj * 10 + mn;

        ctxdata.version = ver;

        if(ver > GLCoreVersion || (!GLIsCore && ctxdata.isCore))
        {
          GLCoreVersion = ver;
          GLIsCore = ctxdata.isCore;
          DoVendorChecks(gl, m_Platform, winData);
        }
      }

      if(m_State >= WRITING)
      {
        GLuint prevArrayBuffer = 0;
        glGetIntegerv(eGL_ARRAY_BUFFER_BINDING, (GLint *)&prevArrayBuffer);

        // Initialize VBOs used in case we copy from client memory.
        glGenBuffers(ARRAY_COUNT(ctxdata.m_ClientMemoryVBOs), ctxdata.m_ClientMemoryVBOs);
        for(size_t i = 0; i < ARRAY_COUNT(ctxdata.m_ClientMemoryVBOs); i++)
        {
          glBindBuffer(eGL_ARRAY_BUFFER, ctxdata.m_ClientMemoryVBOs[i]);
          glBufferData(eGL_ARRAY_BUFFER, 64, NULL, eGL_DYNAMIC_DRAW);
        }
        glBindBuffer(eGL_ARRAY_BUFFER, prevArrayBuffer);
        glGenBuffers(1, &ctxdata.m_ClientMemoryIBO);
      }
    }

    // this is hack but GL context creation is an *utter mess*. For first-frame captures, only
    // consider an attribs created context, to avoid starting capturing when the user is creating
    // dummy contexts to be able to create the real one.
    if(ctxdata.attribsCreate)
      FirstFrame(ctxdata.ctx, (void *)winData.wnd);
  }
}

void WrappedOpenGL::WindowSize(void *windowHandle, uint32_t w, uint32_t h)
{
  // TODO: support multiple window handles
  m_InitParams.width = w;
  m_InitParams.height = h;
}

// TODO this could be a general class for use elsewhere (ie. code that wants
// to push and pop would set state through the class, which records dirty bits
// and then restores).
struct RenderTextState
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
  GLuint ubo[3];
  GLuint prog;
  GLuint pipe;
  GLuint VAO;
  GLuint drawFBO;

  // if this context wasn't created with CreateContextAttribs we
  // do an immediate mode render, so fewer states are pushed/popped.
  // note we don't assume a 1.0 context since that would be painful to
  // handle. Instead we just skip bits of state we're not going to mess
  // with. In some cases this might cause problems e.g. we don't use
  // indexed enable states for blend and scissor test because we're
  // assuming there's no separate blending.
  //
  // In the end, this is just a best-effort to keep going without
  // crashing. Old GL versions aren't supported.
  void Push(const GLHookSet &gl, bool modern)
  {
    enableBits[0] = gl.glIsEnabled(eGL_DEPTH_TEST) != 0;
    enableBits[1] = gl.glIsEnabled(eGL_STENCIL_TEST) != 0;
    enableBits[2] = gl.glIsEnabled(eGL_CULL_FACE) != 0;
    if(modern)
    {
      if(!IsGLES)
        enableBits[3] = gl.glIsEnabled(eGL_DEPTH_CLAMP) != 0;

      if(HasExt[ARB_draw_buffers_blend])
        enableBits[4] = gl.glIsEnabledi(eGL_BLEND, 0) != 0;
      else
        enableBits[4] = gl.glIsEnabled(eGL_BLEND) != 0;

      if(HasExt[ARB_viewport_array])
        enableBits[5] = gl.glIsEnabledi(eGL_SCISSOR_TEST, 0) != 0;
      else
        enableBits[5] = gl.glIsEnabled(eGL_SCISSOR_TEST) != 0;
    }
    else
    {
      enableBits[3] = gl.glIsEnabled(eGL_BLEND) != 0;
      enableBits[4] = gl.glIsEnabled(eGL_SCISSOR_TEST) != 0;
      enableBits[5] = gl.glIsEnabled(eGL_TEXTURE_2D) != 0;
      enableBits[6] = gl.glIsEnabled(eGL_LIGHTING) != 0;
      enableBits[7] = gl.glIsEnabled(eGL_ALPHA_TEST) != 0;
    }

    if(modern && HasExt[ARB_clip_control])
    {
      gl.glGetIntegerv(eGL_CLIP_ORIGIN, (GLint *)&ClipOrigin);
      gl.glGetIntegerv(eGL_CLIP_DEPTH_MODE, (GLint *)&ClipDepth);
    }
    else
    {
      ClipOrigin = eGL_LOWER_LEFT;
      ClipDepth = eGL_NEGATIVE_ONE_TO_ONE;
    }

    if(modern && HasExt[ARB_draw_buffers_blend])
    {
      gl.glGetIntegeri_v(eGL_BLEND_EQUATION_RGB, 0, (GLint *)&EquationRGB);
      gl.glGetIntegeri_v(eGL_BLEND_EQUATION_ALPHA, 0, (GLint *)&EquationAlpha);

      gl.glGetIntegeri_v(eGL_BLEND_SRC_RGB, 0, (GLint *)&SourceRGB);
      gl.glGetIntegeri_v(eGL_BLEND_SRC_ALPHA, 0, (GLint *)&SourceAlpha);

      gl.glGetIntegeri_v(eGL_BLEND_DST_RGB, 0, (GLint *)&DestinationRGB);
      gl.glGetIntegeri_v(eGL_BLEND_DST_ALPHA, 0, (GLint *)&DestinationAlpha);
    }
    else
    {
      gl.glGetIntegerv(eGL_BLEND_EQUATION_RGB, (GLint *)&EquationRGB);
      gl.glGetIntegerv(eGL_BLEND_EQUATION_ALPHA, (GLint *)&EquationAlpha);

      gl.glGetIntegerv(eGL_BLEND_SRC_RGB, (GLint *)&SourceRGB);
      gl.glGetIntegerv(eGL_BLEND_SRC_ALPHA, (GLint *)&SourceAlpha);

      gl.glGetIntegerv(eGL_BLEND_DST_RGB, (GLint *)&DestinationRGB);
      gl.glGetIntegerv(eGL_BLEND_DST_ALPHA, (GLint *)&DestinationAlpha);
    }

    if(!VendorCheck[VendorCheck_AMD_polygon_mode_query] && !IsGLES)
    {
      GLenum dummy[2] = {eGL_FILL, eGL_FILL};
      // docs suggest this is enumeration[2] even though polygon mode can't be set independently for
      // front
      // and back faces.
      gl.glGetIntegerv(eGL_POLYGON_MODE, (GLint *)&dummy);
      PolygonMode = dummy[0];
    }
    else
    {
      PolygonMode = eGL_FILL;
    }

    if(modern && HasExt[ARB_viewport_array])
      gl.glGetFloati_v(eGL_VIEWPORT, 0, &Viewportf[0]);
    else
      gl.glGetIntegerv(eGL_VIEWPORT, &Viewport[0]);

    gl.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&ActiveTexture);
    gl.glActiveTexture(eGL_TEXTURE0);
    gl.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint *)&tex0);

    // we get the current program but only try to restore it if it's non-0
    prog = 0;
    if(modern)
      gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);

    drawFBO = 0;
    gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&drawFBO);

    // since we will use the fixed function pipeline, also need to check for
    // program pipeline bindings (if we weren't, our program would override)
    pipe = 0;
    if(modern && HasExt[ARB_separate_shader_objects])
      gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

    if(modern)
    {
      gl.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, 0, (GLint *)&ubo[0]);
      gl.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, 1, (GLint *)&ubo[1]);
      gl.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, 2, (GLint *)&ubo[2]);

      gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);
    }
  }

  void Pop(const GLHookSet &gl, bool modern)
  {
    if(enableBits[0])
      gl.glEnable(eGL_DEPTH_TEST);
    else
      gl.glDisable(eGL_DEPTH_TEST);
    if(enableBits[1])
      gl.glEnable(eGL_STENCIL_TEST);
    else
      gl.glDisable(eGL_STENCIL_TEST);
    if(enableBits[2])
      gl.glEnable(eGL_CULL_FACE);
    else
      gl.glDisable(eGL_CULL_FACE);

    if(modern)
    {
      if(!IsGLES)
      {
        if(enableBits[3])
          gl.glEnable(eGL_DEPTH_CLAMP);
        else
          gl.glDisable(eGL_DEPTH_CLAMP);
      }

      if(HasExt[ARB_draw_buffers_blend])
      {
        if(enableBits[4])
          gl.glEnablei(eGL_BLEND, 0);
        else
          gl.glDisablei(eGL_BLEND, 0);
      }
      else
      {
        if(enableBits[4])
          gl.glEnable(eGL_BLEND);
        else
          gl.glDisable(eGL_BLEND);
      }

      if(HasExt[ARB_viewport_array])
      {
        if(enableBits[5])
          gl.glEnablei(eGL_SCISSOR_TEST, 0);
        else
          gl.glDisablei(eGL_SCISSOR_TEST, 0);
      }
      else
      {
        if(enableBits[5])
          gl.glEnable(eGL_SCISSOR_TEST);
        else
          gl.glDisable(eGL_SCISSOR_TEST);
      }
    }
    else
    {
      if(enableBits[3])
        gl.glEnable(eGL_BLEND);
      else
        gl.glDisable(eGL_BLEND);
      if(enableBits[4])
        gl.glEnable(eGL_SCISSOR_TEST);
      else
        gl.glDisable(eGL_SCISSOR_TEST);
      if(enableBits[5])
        gl.glEnable(eGL_TEXTURE_2D);
      else
        gl.glDisable(eGL_TEXTURE_2D);
      if(enableBits[6])
        gl.glEnable(eGL_LIGHTING);
      else
        gl.glDisable(eGL_LIGHTING);
      if(enableBits[7])
        gl.glEnable(eGL_ALPHA_TEST);
      else
        gl.glDisable(eGL_ALPHA_TEST);
    }

    if(modern && gl.glClipControl && HasExt[ARB_clip_control])
      gl.glClipControl(ClipOrigin, ClipDepth);

    if(modern && HasExt[ARB_draw_buffers_blend])
    {
      gl.glBlendFuncSeparatei(0, SourceRGB, DestinationRGB, SourceAlpha, DestinationAlpha);
      gl.glBlendEquationSeparatei(0, EquationRGB, EquationAlpha);
    }
    else
    {
      gl.glBlendFuncSeparate(SourceRGB, DestinationRGB, SourceAlpha, DestinationAlpha);
      gl.glBlendEquationSeparate(EquationRGB, EquationAlpha);
    }

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, PolygonMode);

    if(modern && HasExt[ARB_viewport_array])
      gl.glViewportIndexedf(0, Viewportf[0], Viewportf[1], Viewportf[2], Viewportf[3]);
    else
      gl.glViewport(Viewport[0], Viewport[1], (GLsizei)Viewport[2], (GLsizei)Viewport[3]);

    gl.glActiveTexture(eGL_TEXTURE0);
    gl.glBindTexture(eGL_TEXTURE_2D, tex0);
    gl.glActiveTexture(ActiveTexture);

    if(drawFBO != 0 && gl.glBindFramebuffer)
      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFBO);

    if(modern)
    {
      gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, ubo[0]);
      gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 1, ubo[1]);
      gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, ubo[2]);

      gl.glUseProgram(prog);

      gl.glBindVertexArray(VAO);
    }
    else
    {
      // only restore these if there was a setting and the function pointer exists
      if(gl.glUseProgram && prog != 0)
        gl.glUseProgram(prog);
      if(gl.glBindProgramPipeline && pipe != 0)
        gl.glBindProgramPipeline(pipe);
    }
  }
};

void WrappedOpenGL::RenderOverlayText(float x, float y, const char *fmt, ...)
{
  static char tmpBuf[4096];

  va_list args;
  va_start(args, fmt);
  StringFormat::vsnprintf(tmpBuf, 4095, fmt, args);
  tmpBuf[4095] = '\0';
  va_end(args);

  RenderOverlayStr(x, y, tmpBuf);
}

void WrappedOpenGL::RenderOverlayStr(float x, float y, const char *text)
{
  if(char *t = strchr((char *)text, '\n'))
  {
    *t = 0;
    RenderOverlayStr(x, y, text);
    RenderOverlayStr(x, y + 1.0f, t + 1);
    *t = '\n';
    return;
  }

  if(strlen(text) == 0)
    return;

  const GLHookSet &gl = m_Real;

  RDCASSERT(strlen(text) < (size_t)FONT_MAX_CHARS);

  ContextData &ctxdata = m_ContextData[GetCtx()];

  if(!ctxdata.built || !ctxdata.ready)
    return;

  // if it's reasonably modern context, assume we can use buffers and UBOs
  if(ctxdata.Modern())
  {
    gl.glBindBuffer(eGL_UNIFORM_BUFFER, ctxdata.GeneralUBO);

    FontUBOData *ubo = (FontUBOData *)gl.glMapBufferRange(
        eGL_UNIFORM_BUFFER, 0, sizeof(FontUBOData), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    ubo->TextPosition.x = x;
    ubo->TextPosition.y = y;

    ubo->FontScreenAspect.x = 1.0f / float(m_InitParams.width);
    ubo->FontScreenAspect.y = 1.0f / float(m_InitParams.height);

    ubo->TextSize = ctxdata.CharSize;
    ubo->FontScreenAspect.x *= ctxdata.CharAspect;

    ubo->CharacterSize.x = 1.0f / float(FONT_TEX_WIDTH);
    ubo->CharacterSize.y = 1.0f / float(FONT_TEX_HEIGHT);

    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    size_t len = strlen(text);

    if((int)len > FONT_MAX_CHARS)
    {
      static bool printedWarning = false;

      // this could be called once a frame, don't want to spam the log
      if(!printedWarning)
      {
        printedWarning = true;
        RDCWARN("log string '%s' is too long", text, (int)len);
      }

      len = FONT_MAX_CHARS;
    }

    gl.glBindBuffer(eGL_UNIFORM_BUFFER, ctxdata.StringUBO);
    uint32_t *texs =
        (uint32_t *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, len * 4 * sizeof(uint32_t),
                                        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if(texs)
    {
      for(size_t i = 0; i < len; i++)
      {
        texs[i * 4 + 0] = text[i] - ' ';
        texs[i * 4 + 1] = text[i] - ' ';
        texs[i * 4 + 2] = text[i] - ' ';
        texs[i * 4 + 3] = text[i] - ' ';
      }
    }
    else
    {
      static bool printedWarning = false;

      // this could be called once a frame, don't want to spam the log
      if(!printedWarning)
      {
        printedWarning = true;
        RDCWARN("failed to map %d characters for '%s' (%d)", (int)len, text, ctxdata.StringUBO);
      }
    }

    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    //////////////////////////////////////////////////////////////////////////////////
    // Make sure if you change any other state in here, that you also update the push
    // and pop functions above (RenderTextState)

    // set blend state
    if(HasExt[ARB_draw_buffers_blend])
    {
      gl.glEnablei(eGL_BLEND, 0);
      gl.glBlendFuncSeparatei(0, eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA, eGL_SRC_ALPHA,
                              eGL_SRC_ALPHA);
      gl.glBlendEquationSeparatei(0, eGL_FUNC_ADD, eGL_FUNC_ADD);
    }
    else
    {
      gl.glEnable(eGL_BLEND);
      gl.glBlendFuncSeparate(eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA, eGL_SRC_ALPHA, eGL_SRC_ALPHA);
      gl.glBlendEquationSeparate(eGL_FUNC_ADD, eGL_FUNC_ADD);
    }

    // set depth & stencil
    gl.glDisable(eGL_DEPTH_TEST);
    if(!IsGLES)
      gl.glDisable(eGL_DEPTH_CLAMP);
    gl.glDisable(eGL_STENCIL_TEST);
    gl.glDisable(eGL_CULL_FACE);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, 0);

    // set viewport & scissor
    if(HasExt[ARB_viewport_array])
    {
      gl.glViewportIndexedf(0, 0.0f, 0.0f, (float)m_InitParams.width, (float)m_InitParams.height);
      gl.glDisablei(eGL_SCISSOR_TEST, 0);
    }
    else
    {
      gl.glViewport(0, 0, m_InitParams.width, m_InitParams.height);
      gl.glDisable(eGL_SCISSOR_TEST);
    }

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

    if(gl.glClipControl && HasExt[ARB_clip_control])
      gl.glClipControl(eGL_LOWER_LEFT, eGL_NEGATIVE_ONE_TO_ONE);

    // bind UBOs
    gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, ctxdata.GeneralUBO);
    gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 1, ctxdata.GlyphUBO);
    gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, ctxdata.StringUBO);

    // bind empty VAO just for valid rendering
    gl.glBindVertexArray(ctxdata.DummyVAO);

    // bind textures
    gl.glActiveTexture(eGL_TEXTURE0);
    gl.glBindTexture(eGL_TEXTURE_2D, ctxdata.GlyphTexture);

    // bind program
    gl.glUseProgram(ctxdata.Program);

    // draw string
    gl.glDrawArrays(eGL_TRIANGLES, 0, 6 * (GLsizei)len);
  }
  else
  {
    // if it wasn't created in modern fashion with createattribs, assume the worst
    // and draw with immediate mode (since it's impossible that the context is core
    // profile, this will always work)
    //
    // This isn't perfect since without a lot of fiddling we'd need to check if e.g.
    // indexed blending should be used or not. Since we're not too worried about
    // working in this situation, just doing something reasonable, we just assume
    // roughly ~2.0 functionality

    //////////////////////////////////////////////////////////////////////////////////
    // Make sure if you change any other state in here, that you also update the push
    // and pop functions above (RenderTextState)

    // disable blending and some old-style fixed function features
    gl.glDisable(eGL_BLEND);
    gl.glDisable(eGL_LIGHTING);
    gl.glDisable(eGL_ALPHA_TEST);

    // set depth & stencil
    gl.glDisable(eGL_DEPTH_TEST);
    gl.glDisable(eGL_STENCIL_TEST);
    gl.glDisable(eGL_CULL_FACE);

    // set viewport & scissor
    gl.glViewport(0, 0, (GLsizei)m_InitParams.width, (GLsizei)m_InitParams.height);
    gl.glDisable(eGL_SCISSOR_TEST);
    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

    // bind textures
    gl.glActiveTexture(eGL_TEXTURE0);
    gl.glBindTexture(eGL_TEXTURE_2D, ctxdata.GlyphTexture);
    gl.glEnable(eGL_TEXTURE_2D);

    if(gl.glBindFramebuffer)
      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, 0);

    // just in case, try to disable the programmable pipeline
    if(gl.glUseProgram)
      gl.glUseProgram(0);
    if(gl.glBindProgramPipeline)
      gl.glBindProgramPipeline(0);

    // draw string (based on sample code from stb_truetype.h)
    vector<Vec4f> vertices;
    {
      y += 1.0f;
      y *= charPixelHeight;

      float startx = x;
      float starty = y;

      float maxx = x, minx = x;
      float maxy = y, miny = y - charPixelHeight;

      stbtt_aligned_quad q;

      const char *prepass = text;
      while(*prepass)
      {
        char c = *prepass;
        if(c >= firstChar && c <= lastChar)
        {
          stbtt_GetBakedQuad(chardata, FONT_TEX_WIDTH, FONT_TEX_HEIGHT, c - firstChar, &x, &y, &q, 1);

          maxx = RDCMAX(maxx, RDCMAX(q.x0, q.x1));
          maxy = RDCMAX(maxy, RDCMAX(q.y0, q.y1));

          minx = RDCMIN(minx, RDCMIN(q.x0, q.x1));
          miny = RDCMIN(miny, RDCMIN(q.y0, q.y1));
        }
        else
        {
          x += chardata[0].xadvance;
        }
        prepass++;
      }

      x = startx;
      y = starty;

      // draw black bar behind text

      vertices.push_back(Vec4f(minx, maxy, 0.0f, 0.0f));
      vertices.push_back(Vec4f(maxx, maxy, 0.0f, 0.0f));
      vertices.push_back(Vec4f(maxx, miny, 0.0f, 0.0f));
      vertices.push_back(Vec4f(minx, miny, 0.0f, 0.0f));

      while(*text)
      {
        char c = *text;
        if(c >= firstChar && c <= lastChar)
        {
          stbtt_GetBakedQuad(chardata, FONT_TEX_WIDTH, FONT_TEX_HEIGHT, c - firstChar, &x, &y, &q, 1);

          vertices.push_back(Vec4f(q.x0, q.y0, q.s0, q.t0));
          vertices.push_back(Vec4f(q.x1, q.y0, q.s1, q.t0));
          vertices.push_back(Vec4f(q.x1, q.y1, q.s1, q.t1));
          vertices.push_back(Vec4f(q.x0, q.y1, q.s0, q.t1));

          maxx = RDCMAX(maxx, RDCMAX(q.x0, q.x1));
          maxy = RDCMAX(maxy, RDCMAX(q.y0, q.y1));
        }
        else
        {
          x += chardata[0].xadvance;
        }
        ++text;
      }
    }
    m_Platform.DrawQuads((float)m_InitParams.width, (float)m_InitParams.height, vertices);
  }
}

struct ReplacementSearch
{
  bool operator()(const pair<ResourceId, Replacement> &a, ResourceId b) { return a.first < b; }
};

void WrappedOpenGL::ReplaceResource(ResourceId from, ResourceId to)
{
  RemoveReplacement(from);

  if(GetResourceManager()->HasLiveResource(from))
  {
    GLResource resource = GetResourceManager()->GetLiveResource(to);
    ResourceId livefrom = GetResourceManager()->GetLiveID(from);

    if(resource.Namespace == eResShader)
    {
      // need to replace all programs that use this shader
      for(auto it = m_Programs.begin(); it != m_Programs.end(); ++it)
      {
        ResourceId progsrcid = it->first;
        ProgramData &progdata = it->second;

        // see if the shader is used
        for(int i = 0; i < 6; i++)
        {
          if(progdata.stageShaders[i] == livefrom)
          {
            GLuint progsrc = GetResourceManager()->GetCurrentResource(progsrcid).name;

            // make a new program
            GLuint progdst = glCreateProgram();

            ResourceId progdstid = GetResourceManager()->GetID(ProgramRes(GetCtx(), progdst));

            // attach all but the i'th shader
            for(int j = 0; j < 6; j++)
              if(i != j && progdata.stageShaders[j] != ResourceId())
                glAttachShader(
                    progdst, GetResourceManager()->GetCurrentResource(progdata.stageShaders[j]).name);

            // attach the new shader
            glAttachShader(progdst, resource.name);

            // mark separable if previous program was separable
            GLint sep = 0;
            glGetProgramiv(progsrc, eGL_PROGRAM_SEPARABLE, &sep);

            if(sep)
              glProgramParameteri(progdst, eGL_PROGRAM_SEPARABLE, GL_TRUE);

            ResourceId vs = progdata.stageShaders[0];
            ResourceId fs = progdata.stageShaders[4];

            if(vs != ResourceId())
              CopyProgramAttribBindings(m_Real, progsrc, progdst, &m_Shaders[vs].reflection);

            if(fs != ResourceId())
              CopyProgramFragDataBindings(m_Real, progsrc, progdst, &m_Shaders[fs].reflection);

            // link new program
            glLinkProgram(progdst);

            GLint status = 0;
            glGetProgramiv(progdst, eGL_LINK_STATUS, &status);

            if(status == 0)
            {
              GLint len = 1024;
              glGetProgramiv(progdst, eGL_INFO_LOG_LENGTH, &len);
              char *buffer = new char[len + 1];
              glGetProgramInfoLog(progdst, len, NULL, buffer);
              buffer[len] = 0;

              RDCWARN(
                  "When making program replacement for shader, program failed to link. Skipping "
                  "replacement:\n%s",
                  buffer);

              delete[] buffer;

              glDeleteProgram(progdst);
            }
            else
            {
              // copy uniforms
              CopyProgramUniforms(m_Real, progsrc, progdst);

              ResourceId origsrcid = GetResourceManager()->GetOriginalID(progsrcid);

              // recursively call to replaceresource (different type - these are programs)
              ReplaceResource(origsrcid, progdstid);

              // insert into m_DependentReplacements
              auto insertPos =
                  std::lower_bound(m_DependentReplacements.begin(), m_DependentReplacements.end(),
                                   from, ReplacementSearch());
              m_DependentReplacements.insert(
                  insertPos,
                  std::make_pair(from, Replacement(origsrcid, ProgramRes(GetCtx(), progdst))));
            }

            break;
          }
        }
      }
    }

    if(resource.Namespace == eResProgram)
    {
      // need to replace all pipelines that use this program
      for(auto it = m_Pipelines.begin(); it != m_Pipelines.end(); ++it)
      {
        ResourceId pipesrcid = it->first;
        PipelineData &pipedata = it->second;

        // see if the program is used
        for(int i = 0; i < 6; i++)
        {
          if(pipedata.stagePrograms[i] == livefrom)
          {
            // make a new pipeline
            GLuint pipedst = 0;
            glGenProgramPipelines(1, &pipedst);

            ResourceId pipedstid = GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), pipedst));

            // attach all but the i'th program
            for(int j = 0; j < 6; j++)
            {
              if(i != j && pipedata.stagePrograms[j] != ResourceId())
              {
                // if this stage was provided by the program we're replacing, use that instead
                if(pipedata.stagePrograms[i] == pipedata.stagePrograms[j])
                  glUseProgramStages(pipedst, ShaderBit(j), resource.name);
                else
                  glUseProgramStages(
                      pipedst, ShaderBit(j),
                      GetResourceManager()->GetCurrentResource(pipedata.stagePrograms[j]).name);
              }
            }

            // attach the new program in our stage
            glUseProgramStages(pipedst, ShaderBit(i), resource.name);

            ResourceId origsrcid = GetResourceManager()->GetOriginalID(pipesrcid);

            // recursively call to replaceresource (different type - these are programs)
            ReplaceResource(origsrcid, pipedstid);

            // insert into m_DependentReplacements
            auto insertPos =
                std::lower_bound(m_DependentReplacements.begin(), m_DependentReplacements.end(),
                                 from, ReplacementSearch());
            m_DependentReplacements.insert(
                insertPos,
                std::make_pair(from, Replacement(origsrcid, ProgramPipeRes(GetCtx(), pipedst))));
          }
        }
      }
    }

    // do actual replacement
    GLResource fromresource = GetResourceManager()->GetLiveResource(from);

    // if they're the same type it's easy, but it could be we want to replace a shader
    // inside a program which never had a shader (ie. glCreateShaderProgramv)
    if(fromresource.Namespace == resource.Namespace)
    {
      GetResourceManager()->ReplaceResource(from, to);
    }
    else if(fromresource.Namespace == eResProgram && resource.Namespace == eResShader)
    {
      // if we want to replace a program with a shader, assume it's just a program with only one
      // shader attached. This will have been handled above in the "programs dependent on this
      // shader", so we can just skip doing anything here
    }
    else
    {
      RDCERR("Unsupported replacement type from type %d to type %d", fromresource.Namespace,
             resource.Namespace);
    }
  }
}

void WrappedOpenGL::RemoveReplacement(ResourceId id)
{
  // do actual removal
  GetResourceManager()->RemoveReplacement(id);

  std::set<ResourceId> recurse;

  // check if there are any dependent replacements, remove if so
  auto it = std::lower_bound(m_DependentReplacements.begin(), m_DependentReplacements.end(), id,
                             ReplacementSearch());
  for(; it != m_DependentReplacements.end();)
  {
    GetResourceManager()->RemoveReplacement(it->second.id);
    recurse.insert(it->second.id);

    switch(it->second.res.Namespace)
    {
      case eResProgram: glDeleteProgram(it->second.res.name); break;
      case eResProgramPipe: glDeleteProgramPipelines(1, &it->second.res.name); break;
      default: RDCERR("Unexpected resource type to be freed"); break;
    }

    it = m_DependentReplacements.erase(it);
  }

  for(auto recurseit = recurse.begin(); recurseit != recurse.end(); ++recurseit)
  {
    // recursive call in case there are any dependents on this resource
    RemoveReplacement(*recurseit);
  }
}

void WrappedOpenGL::FreeTargetResource(ResourceId id)
{
  if(GetResourceManager()->HasLiveResource(id))
  {
    GLResource resource = GetResourceManager()->GetLiveResource(id);

    RDCASSERT(resource.Namespace != eResUnknown);

    switch(resource.Namespace)
    {
      case eResShader: glDeleteShader(resource.name); break;
      default: RDCERR("Unexpected resource type to be freed"); break;
    }
  }
}

void WrappedOpenGL::SwapBuffers(void *windowHandle)
{
  if(m_State == WRITING_IDLE)
    RenderDoc::Inst().Tick();

  // don't do anything if no context is active.
  if(GetCtx() == NULL)
  {
    m_NoCtxFrames++;
    if(m_NoCtxFrames == 100)
    {
      RDCERR(
          "Seen 100 frames with no context current. RenderDoc requires a context to be current "
          "during the call to SwapBuffers to display its overlay and start/stop captures on "
          "default keys.\nIf your GL use is elsewhere, consider using the in-application API to "
          "trigger captures manually");
    }
    return;
  }

  m_NoCtxFrames = 0;

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame

  GetResourceManager()->FlushPendingDirty();

  ContextData &ctxdata = GetCtxData();

  // we only handle context-window associations here as it's too common to
  // create invisible helper windows while creating contexts, that then
  // become the default window.
  // Since we only capture windows that do SwapBuffers (i.e. if you're doing
  // headless rendering then you must capture via the API anyway), this
  // isn't a big problem.
  //
  // Also we only set up associations for capturable windows.
  if(ctxdata.Modern())
  {
    for(auto it = m_ContextData.begin(); it != m_ContextData.end(); ++it)
      if(it->first != ctxdata.ctx)
        it->second.UnassociateWindow(windowHandle);

    ctxdata.AssociateWindow(this, windowHandle);
  }

  // do this as late as possible to avoid creating objects on contexts
  // that might be shared later (wglShareLists requires contexts to be
  // pristine, so can't create this from wglMakeCurrent)
  if(!ctxdata.ready)
    ctxdata.CreateDebugData(m_Real);

  bool activeWindow = RenderDoc::Inst().IsActiveWindow(ctxdata.ctx, windowHandle);

  // look at previous associations and decay any that are too old
  uint64_t ref = Timing::GetUnixTimestamp() - 5;    // 5 seconds

  for(auto cit = m_ContextData.begin(); cit != m_ContextData.end(); ++cit)
  {
    for(auto wit = cit->second.windows.begin(); wit != cit->second.windows.end();)
    {
      if(wit->second < ref)
      {
        auto remove = wit;
        ++wit;

        RenderDoc::Inst().RemoveFrameCapturer(cit->first, remove->first);

        cit->second.windows.erase(remove);
      }
      else
      {
        ++wit;
      }
    }
  }

  if(m_State == WRITING_IDLE)
  {
    uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

    if(overlay & eRENDERDOC_Overlay_Enabled)
    {
      RenderTextState textState;

      textState.Push(m_Real, ctxdata.Modern());

      int flags = activeWindow ? RenderDoc::eOverlay_ActiveWindow : 0;
      if(ctxdata.Legacy())
        flags |= RenderDoc::eOverlay_CaptureDisabled;
      string overlayText = RenderDoc::Inst().GetOverlayText(GetDriverType(), m_FrameCounter, flags);

      if(ctxdata.Legacy())
      {
        if(!ctxdata.attribsCreate)
          overlayText += "Context not created via CreateContextAttribs. Capturing disabled.\n";
        overlayText += "Only OpenGL 3.2+ contexts are supported.\n";
      }
      else if(!ctxdata.isCore)
      {
        overlayText += "WARNING: Non-core context in use. Compatibility profile not supported.\n";
      }

      if(activeWindow && m_FailedFrame > 0)
      {
        const char *reasonString = "Unknown reason";
        switch(m_FailedReason)
        {
          case CaptureFailed_UncappedUnmap: reasonString = "Uncapped Map()/Unmap()"; break;
          default: break;
        }

        overlayText += StringFormat::Fmt("Failed capture at frame %d:\n", m_FailedFrame);
        overlayText += StringFormat::Fmt("    %s\n", reasonString);
      }

      if(!overlayText.empty())
        RenderOverlayText(0.0f, 0.0f, overlayText.c_str());

      textState.Pop(m_Real, ctxdata.Modern());

      // swallow all errors we might have inadvertantly caused. This is
      // better than letting an error propagate and maybe screw up the
      // app (although it means we might swallow an error from before the
      // SwapBuffers call, it can't be helped.
      if(ctxdata.Legacy() && m_Real.glGetError)
        ClearGLErrors(m_Real);
    }
  }

  if(m_State == WRITING_CAPFRAME && m_AppControlledCapture)
    m_BackbufferImages[windowHandle] = SaveBackbufferImage();

  if(!activeWindow)
    return;

  RenderDoc::Inst().SetCurrentDriver(GetDriverType());

  // only allow capturing on 'modern' created contexts
  if(ctxdata.Legacy())
    return;

  // kill any current capture that isn't application defined
  if(m_State == WRITING_CAPFRAME && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture(ctxdata.ctx, windowHandle);

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && m_State == WRITING_IDLE)
  {
    RenderDoc::Inst().StartFrameCapture(ctxdata.ctx, windowHandle);

    m_AppControlledCapture = false;
  }
}

void WrappedOpenGL::CreateVRAPITextureSwapChain(GLuint tex, GLenum textureType,
                                                GLenum internalformat, GLsizei width, GLsizei height)
{
  GLResource res = TextureRes(GetCtx(), tex);
  ResourceId id = GetResourceManager()->RegisterResource(res);

  if(m_State >= WRITING)
  {
    Chunk *chunk = NULL;

    {
      SCOPED_SERIALISE_CONTEXT(GEN_TEXTURE);
      Serialise_glGenTextures(1, &tex);

      chunk = scope.Get();
    }

    GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    RDCASSERT(record);

    record->AddChunk(chunk);
  }
  else
  {
    GetResourceManager()->AddLiveResource(id, res);
  }

  if(textureType == eGL_TEXTURE_2D_ARRAY)
  {
    Common_glTextureImage3DEXT(id, eGL_TEXTURE_2D, 0, internalformat, width, height, 2, 0, eGL_RGBA,
                               eGL_UNSIGNED_BYTE, NULL);
  }
  else
  {
    Common_glTextureImage2DEXT(id, eGL_TEXTURE_2D, 0, internalformat, width, height, 0, eGL_RGBA,
                               eGL_UNSIGNED_BYTE, NULL);
  }
}

void WrappedOpenGL::MakeValidContextCurrent(GLWindowingData &prevctx, void *favourWnd)
{
  if(prevctx.ctx == NULL)
  {
    for(size_t i = m_LastContexts.size(); i > 0; i--)
    {
      // need to find a context for fetching most initial states
      GLWindowingData ctx = m_LastContexts[i - 1];

      // check this context isn't current elsewhere
      bool usedElsewhere = false;
      for(auto it = m_ActiveContexts.begin(); it != m_ActiveContexts.end(); ++it)
      {
        if(it->second.ctx == ctx.ctx)
        {
          usedElsewhere = true;
          break;
        }
      }

      if(!usedElsewhere)
      {
        prevctx = ctx;
        break;
      }
    }

    if(prevctx.ctx == NULL)
    {
      RDCERR("Couldn't find GL context to make current on this thread %llu.",
             Threading::GetCurrentID());
    }

    m_ActiveContexts[Threading::GetCurrentID()] = prevctx;
    m_Platform.MakeContextCurrent(prevctx);
  }
}

void WrappedOpenGL::StartFrameCapture(void *dev, void *wnd)
{
  if(m_State != WRITING_IDLE)
    return;

  SCOPED_LOCK(GetGLLock());

  RenderDoc::Inst().SetCurrentDriver(GetDriverType());

  m_State = WRITING_CAPFRAME;

  m_AppControlledCapture = true;

  m_Failures = 0;
  m_FailedFrame = 0;
  m_FailedReason = CaptureSucceeded;

  GLWindowingData prevctx = m_ActiveContexts[Threading::GetCurrentID()];
  GLWindowingData switchctx = prevctx;
  MakeValidContextCurrent(switchctx, wnd);

  m_FrameCounter = RDCMAX(1 + (uint32_t)m_CapturedFrames.size(), m_FrameCounter);

  FrameDescription frame;
  frame.frameNumber = m_FrameCounter + 1;
  frame.captureTime = Timing::GetUnixTimestamp();
  RDCEraseEl(frame.stats);
  m_CapturedFrames.push_back(frame);

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->MarkResourceFrameReferenced(m_DeviceResourceID, eFrameRef_Write);

  GLuint prevVAO = 0;
  m_Real.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

  m_Real.glBindVertexArray(m_FakeVAO);

  GetResourceManager()->MarkVAOReferenced(VertexArrayRes(NULL, m_FakeVAO), eFrameRef_Write, true);

  m_Real.glBindVertexArray(prevVAO);

  GetResourceManager()->PrepareInitialContents();

  FreeCaptureData();

  AttemptCapture();
  BeginCaptureFrame();

  if(switchctx.ctx != prevctx.ctx)
  {
    m_Platform.MakeContextCurrent(prevctx);
    m_ActiveContexts[Threading::GetCurrentID()] = prevctx;
  }

  RDCLOG("Starting capture, frame %u", m_FrameCounter);
}

bool WrappedOpenGL::EndFrameCapture(void *dev, void *wnd)
{
  if(m_State != WRITING_CAPFRAME)
    return true;

  SCOPED_LOCK(GetGLLock());

  CaptureFailReason reason = CaptureSucceeded;

  GLWindowingData prevctx = m_ActiveContexts[Threading::GetCurrentID()];
  GLWindowingData switchctx = prevctx;
  MakeValidContextCurrent(switchctx, wnd);

  if(HasSuccessfulCapture(reason))
  {
    RDCLOG("Finished capture, Frame %u", m_FrameCounter);

    m_Failures = 0;
    m_FailedFrame = 0;
    m_FailedReason = CaptureSucceeded;

    ContextEndFrame();
    FinishCapture();

    BackbufferImage *bbim = NULL;

    // if the specified context isn't current, try and see if we've saved
    // an appropriate backbuffer image during capture.
    if((dev != NULL && prevctx.ctx != dev) || (wnd != 0 && (void *)prevctx.wnd != wnd))
    {
      auto it = m_BackbufferImages.find(wnd);
      if(it != m_BackbufferImages.end())
      {
        // pop this backbuffer image out of the map
        bbim = it->second;
        m_BackbufferImages.erase(it);
      }
    }

    // if we don't have one selected, save the backbuffer image from the
    // current context
    if(bbim == NULL)
      bbim = SaveBackbufferImage();

    Serialiser *m_pFileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(
        m_FrameCounter, &m_InitParams, bbim->jpgbuf, bbim->len, bbim->thwidth, bbim->thheight);

    SAFE_DELETE(bbim);

    for(auto it = m_BackbufferImages.begin(); it != m_BackbufferImages.end(); ++it)
      delete it->second;
    m_BackbufferImages.clear();

    {
      SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

      SERIALISE_ELEMENT(ResourceId, immContextId, m_ContextResourceID);
      SERIALISE_ELEMENT(ResourceId, vaoId, m_FakeVAOID);

      m_pFileSerialiser->Insert(scope.Get(true));
    }

    RDCDEBUG("Inserting Resource Serialisers");

    GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);

    GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

    RDCDEBUG("Creating Capture Scope");

    {
      SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

      Serialise_CaptureScope(0);

      m_pFileSerialiser->Insert(scope.Get(true));
    }

    {
      RDCDEBUG("Getting Resource Record");

      GLResourceRecord *record = m_ResourceManager->GetResourceRecord(m_ContextResourceID);

      RDCDEBUG("Accumulating context resource list");

      map<int32_t, Chunk *> recordlist;
      record->Insert(recordlist);

      RDCDEBUG("Flushing %u records to file serialiser", (uint32_t)recordlist.size());

      for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
        m_pFileSerialiser->Insert(it->second);

      RDCDEBUG("Done");
    }

    m_pFileSerialiser->FlushToDisk();

    RenderDoc::Inst().SuccessfullyWrittenLog(m_FrameCounter);

    SAFE_DELETE(m_pFileSerialiser);

    m_State = WRITING_IDLE;

    GetResourceManager()->MarkUnwrittenResources();

    GetResourceManager()->ClearReferencedResources();

    if(switchctx.ctx != prevctx.ctx)
    {
      m_Platform.MakeContextCurrent(prevctx);
      m_ActiveContexts[Threading::GetCurrentID()] = prevctx;
    }

    return true;
  }
  else
  {
    const char *reasonString = "Unknown reason";
    switch(reason)
    {
      case CaptureFailed_UncappedUnmap: reasonString = "Uncapped Map()/Unmap()"; break;
      default: break;
    }

    RDCLOG("Failed to capture, frame %u: %s", m_FrameCounter, reasonString);

    m_Failures++;

    if((RenderDoc::Inst().GetOverlayBits() & eRENDERDOC_Overlay_Enabled))
    {
      ContextData &ctxdata = GetCtxData();

      RenderTextState textState;

      textState.Push(m_Real, ctxdata.Modern());

      RenderOverlayText(0.0f, 0.0f, "Failed to capture frame %u: %s", m_FrameCounter, reasonString);

      textState.Pop(m_Real, ctxdata.Modern());

      // swallow all errors we might have inadvertantly caused. This is
      // better than letting an error propagate and maybe screw up the
      // app (although it means we might swallow an error from before the
      // SwapBuffers call, it can't be helped.
      if(ctxdata.Legacy() && m_Real.glGetError)
        ClearGLErrors(m_Real);
    }

    m_CapturedFrames.back().frameNumber = m_FrameCounter + 1;

    CleanupCapture();

    GetResourceManager()->ClearReferencedResources();

    // if it's a capture triggered from application code, immediately
    // give up as it's not reasonable to expect applications to detect and retry.
    // otherwise we can retry in case the next frame works.
    if(m_Failures > 5 || m_AppControlledCapture)
    {
      FinishCapture();

      m_CapturedFrames.pop_back();

      FreeCaptureData();

      m_FailedFrame = m_FrameCounter;
      m_FailedReason = reason;

      m_State = WRITING_IDLE;

      GetResourceManager()->MarkUnwrittenResources();
    }
    else
    {
      GetResourceManager()->MarkResourceFrameReferenced(m_DeviceResourceID, eFrameRef_Write);
      GetResourceManager()->PrepareInitialContents();

      AttemptCapture();
      BeginCaptureFrame();
    }

    if(switchctx.ctx != prevctx.ctx)
    {
      m_Platform.MakeContextCurrent(prevctx);
      m_ActiveContexts[Threading::GetCurrentID()] = prevctx;
    }

    return false;
  }
}

void WrappedOpenGL::FirstFrame(void *ctx, void *wndHandle)
{
  // if we have to capture the first frame, begin capturing immediately
  if(m_FrameCounter == 0 && m_State == WRITING_IDLE && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    // since we haven't associated the window we can't capture by window, so we have to capture just
    // on the device - the very next present to any window on this context will end the capture.
    RenderDoc::Inst().StartFrameCapture(ctx, NULL);

    m_AppControlledCapture = false;
  }
}

WrappedOpenGL::BackbufferImage *WrappedOpenGL::SaveBackbufferImage()
{
  const uint32_t maxSize = 2048;

  byte *thpixels = NULL;
  uint32_t thwidth = 0;
  uint32_t thheight = 0;

  if(m_Real.glGetIntegerv && m_Real.glReadBuffer && m_Real.glBindFramebuffer &&
     m_Real.glBindBuffer && m_Real.glReadPixels)
  {
    RDCGLenum prevReadBuf = eGL_BACK;
    GLint prevBuf = 0;
    GLint packBufBind = 0;
    GLint prevPackRowLen = 0;
    GLint prevPackSkipRows = 0;
    GLint prevPackSkipPixels = 0;
    GLint prevPackAlignment = 0;
    m_Real.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&prevReadBuf);
    m_Real.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &prevBuf);
    m_Real.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, &packBufBind);
    m_Real.glGetIntegerv(eGL_PACK_ROW_LENGTH, &prevPackRowLen);
    m_Real.glGetIntegerv(eGL_PACK_SKIP_ROWS, &prevPackSkipRows);
    m_Real.glGetIntegerv(eGL_PACK_SKIP_PIXELS, &prevPackSkipPixels);
    m_Real.glGetIntegerv(eGL_PACK_ALIGNMENT, &prevPackAlignment);

    m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, 0);
    m_Real.glReadBuffer(eGL_BACK);
    m_Real.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
    m_Real.glPixelStorei(eGL_PACK_ROW_LENGTH, 0);
    m_Real.glPixelStorei(eGL_PACK_SKIP_ROWS, 0);
    m_Real.glPixelStorei(eGL_PACK_SKIP_PIXELS, 0);
    m_Real.glPixelStorei(eGL_PACK_ALIGNMENT, 1);

    thwidth = m_InitParams.width;
    thheight = m_InitParams.height;

    thpixels = new byte[thwidth * thheight * 4];

    // GLES only supports GL_RGBA
    m_Real.glReadPixels(0, 0, thwidth, thheight, eGL_RGBA, eGL_UNSIGNED_BYTE, thpixels);

    // RGBA -> RGB
    for(uint32_t y = 0; y < thheight; y++)
    {
      for(uint32_t x = 0; x < thwidth; x++)
      {
        thpixels[(y * thwidth + x) * 3 + 0] = thpixels[(y * thwidth + x) * 4 + 0];
        thpixels[(y * thwidth + x) * 3 + 1] = thpixels[(y * thwidth + x) * 4 + 1];
        thpixels[(y * thwidth + x) * 3 + 2] = thpixels[(y * thwidth + x) * 4 + 2];
      }
    }

    // flip the image in-place
    for(uint32_t y = 0; y <= thheight / 2; y++)
    {
      uint32_t flipY = (thheight - 1 - y);

      for(uint32_t x = 0; x < thwidth; x++)
      {
        byte save[3];
        save[0] = thpixels[(y * thwidth + x) * 3 + 0];
        save[1] = thpixels[(y * thwidth + x) * 3 + 1];
        save[2] = thpixels[(y * thwidth + x) * 3 + 2];

        thpixels[(y * thwidth + x) * 3 + 0] = thpixels[(flipY * thwidth + x) * 3 + 0];
        thpixels[(y * thwidth + x) * 3 + 1] = thpixels[(flipY * thwidth + x) * 3 + 1];
        thpixels[(y * thwidth + x) * 3 + 2] = thpixels[(flipY * thwidth + x) * 3 + 2];

        thpixels[(flipY * thwidth + x) * 3 + 0] = save[0];
        thpixels[(flipY * thwidth + x) * 3 + 1] = save[1];
        thpixels[(flipY * thwidth + x) * 3 + 2] = save[2];
      }
    }

    m_Real.glBindBuffer(eGL_PIXEL_PACK_BUFFER, packBufBind);
    m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevBuf);
    m_Real.glReadBuffer(prevReadBuf);
    m_Real.glPixelStorei(eGL_PACK_ROW_LENGTH, prevPackRowLen);
    m_Real.glPixelStorei(eGL_PACK_SKIP_ROWS, prevPackSkipRows);
    m_Real.glPixelStorei(eGL_PACK_SKIP_PIXELS, prevPackSkipPixels);
    m_Real.glPixelStorei(eGL_PACK_ALIGNMENT, prevPackAlignment);

    // scale down if necessary using simple point sampling
    if(thwidth > maxSize)
    {
      float widthf = float(thwidth);
      float heightf = float(thheight);

      float aspect = widthf / heightf;

      // clamp dimensions to a width of maxSize
      thwidth = maxSize;
      thheight = uint32_t(float(thwidth) / aspect);

      byte *src = thpixels;
      byte *dst = thpixels = new byte[3 * thwidth * thheight];

      for(uint32_t y = 0; y < thheight; y++)
      {
        for(uint32_t x = 0; x < thwidth; x++)
        {
          float xf = float(x) / float(thwidth);
          float yf = float(y) / float(thheight);

          byte *pixelsrc =
              &src[3 * uint32_t(xf * widthf) + m_InitParams.width * 3 * uint32_t(yf * heightf)];

          memcpy(dst, pixelsrc, 3);

          dst += 3;
        }
      }

      // src is the raw unscaled pixels, which is no longer needed
      SAFE_DELETE_ARRAY(src);
    }
  }

  byte *jpgbuf = NULL;
  int len = thwidth * thheight;

  if(len > 0)
  {
    // jpge::compress_image_to_jpeg_file_in_memory requires at least 1024 bytes
    len = len >= 1024 ? len : 1024;

    jpgbuf = new byte[len];

    jpge::params p;
    p.m_quality = 80;

    bool success =
        jpge::compress_image_to_jpeg_file_in_memory(jpgbuf, len, thwidth, thheight, 3, thpixels, p);

    if(!success)
    {
      RDCERR("Failed to compress to jpg");
      SAFE_DELETE_ARRAY(jpgbuf);
      thwidth = 0;
      thheight = 0;
    }
  }

  SAFE_DELETE_ARRAY(thpixels);

  BackbufferImage *bbim = new BackbufferImage();
  bbim->jpgbuf = jpgbuf;
  bbim->len = len;
  bbim->thwidth = thwidth;
  bbim->thheight = thheight;

  return bbim;
}

void WrappedOpenGL::Serialise_CaptureScope(uint64_t offset)
{
  SERIALISE_ELEMENT(uint32_t, FrameNumber, m_FrameCounter);

  if(m_State >= WRITING)
  {
    GetResourceManager()->Serialise_InitialContentsNeeded();
  }
  else
  {
    m_FrameRecord.frameInfo.fileOffset = offset;
    m_FrameRecord.frameInfo.frameNumber = FrameNumber;
    RDCEraseEl(m_FrameRecord.frameInfo.stats);

    GetResourceManager()->CreateInitialContents();
  }
}

void WrappedOpenGL::ContextEndFrame()
{
  SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_FOOTER);

  bool HasCallstack = RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks != 0;
  m_pSerialiser->Serialise("HasCallstack", HasCallstack);

  if(HasCallstack)
  {
    Callstack::Stackwalk *call = Callstack::Collect();

    uint32_t numLevels = (uint32_t)call->NumLevels();
    uint64_t *stack = (uint64_t *)call->GetAddrs();

    m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

    delete call;
  }

  m_ContextRecord->AddChunk(scope.Get());
}

void WrappedOpenGL::CleanupCapture()
{
  m_SuccessfulCapture = true;
  m_FailureReason = CaptureSucceeded;

  m_ContextRecord->LockChunks();
  while(m_ContextRecord->HasChunks())
  {
    Chunk *chunk = m_ContextRecord->GetLastChunk();

    SAFE_DELETE(chunk);
    m_ContextRecord->PopChunk();
  }
  m_ContextRecord->UnlockChunks();

  m_ContextRecord->FreeParents(GetResourceManager());

  for(auto it = m_MissingTracks.begin(); it != m_MissingTracks.end(); ++it)
  {
    if(GetResourceManager()->HasResourceRecord(*it))
      GetResourceManager()->MarkDirtyResource(*it);
  }

  m_MissingTracks.clear();
}

void WrappedOpenGL::FreeCaptureData()
{
}

void WrappedOpenGL::QueuePrepareInitialState(GLResource res, byte *blob)
{
  QueuedInitialStateFetch fetch;
  fetch.res = res;
  fetch.blob = blob;

  auto insertPos =
      std::lower_bound(m_QueuedInitialFetches.begin(), m_QueuedInitialFetches.end(), fetch);
  m_QueuedInitialFetches.insert(insertPos, fetch);
}

void WrappedOpenGL::AttemptCapture()
{
  m_State = WRITING_CAPFRAME;

  m_DebugMessages.clear();

  {
    RDCDEBUG("GL Context %llu Attempting capture", GetContextResourceID());

    m_SuccessfulCapture = true;
    m_FailureReason = CaptureSucceeded;

    m_ContextRecord->LockChunks();
    while(m_ContextRecord->HasChunks())
    {
      Chunk *chunk = m_ContextRecord->GetLastChunk();

      SAFE_DELETE(chunk);
      m_ContextRecord->PopChunk();
    }
    m_ContextRecord->UnlockChunks();
  }
}

bool WrappedOpenGL::Serialise_BeginCaptureFrame(bool applyInitialState)
{
  GLRenderState state(&m_Real, m_pSerialiser, m_State);

  if(m_State >= WRITING)
  {
    state.FetchState(GetCtx(), this);

    state.MarkReferenced(this, true);
  }

  state.Serialise(m_State, GetCtx(), this);

  if(m_State <= EXECUTING && applyInitialState)
  {
    state.ApplyState(GetCtx(), this);
  }

  return true;
}

void WrappedOpenGL::BeginCaptureFrame()
{
  SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_HEADER);

  Serialise_BeginCaptureFrame(false);

  m_ContextRecord->AddChunk(scope.Get(), 1);
}

void WrappedOpenGL::FinishCapture()
{
  m_State = WRITING_IDLE;

  m_DebugMessages.clear();

  // m_SuccessfulCapture = false;
}

void WrappedOpenGL::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                    std::string d)
{
  if(m_State == READING || src == MessageSource::RuntimeWarning)
  {
    DebugMessage msg;
    msg.eventID = m_CurEventID;
    msg.messageID = 0;
    msg.source = src;
    msg.category = c;
    msg.severity = sv;
    msg.description = d;
    m_DebugMessages.push_back(msg);
  }
}

vector<DebugMessage> WrappedOpenGL::GetDebugMessages()
{
  vector<DebugMessage> ret;
  ret.swap(m_DebugMessages);
  return ret;
}

void WrappedOpenGL::Serialise_DebugMessages()
{
  SCOPED_SERIALISE_CONTEXT(DEBUG_MESSAGES);

  vector<DebugMessage> debugMessages;

  if(m_State == WRITING_CAPFRAME)
  {
    debugMessages = m_DebugMessages;
    m_DebugMessages.clear();
  }

  SERIALISE_ELEMENT(bool, HasCallstack,
                    RenderDoc::Inst().GetCaptureOptions().CaptureCallstacksOnlyDraws != 0);

  if(HasCallstack)
  {
    if(m_State >= WRITING)
    {
      Callstack::Stackwalk *call = Callstack::Collect();

      RDCASSERT(call->NumLevels() < 0xff);

      uint32_t numLevels = (uint32_t)call->NumLevels();
      uint64_t *stack = (uint64_t *)call->GetAddrs();

      m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

      delete call;
    }
    else
    {
      uint32_t numLevels = 0;
      uint64_t *stack = NULL;

      m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

      m_pSerialiser->SetCallstack(stack, numLevels);

      SAFE_DELETE_ARRAY(stack);
    }
  }

  SERIALISE_ELEMENT(uint32_t, NumMessages, (uint32_t)debugMessages.size());

  for(uint32_t i = 0; i < NumMessages; i++)
  {
    ScopedContext msgscope(m_pSerialiser, "DebugMessage", "DebugMessage", 0, false);

    string desc;
    if(m_State >= WRITING)
      desc = debugMessages[i].description.elems;

    SERIALISE_ELEMENT(MessageCategory, Category, debugMessages[i].category);
    SERIALISE_ELEMENT(MessageSeverity, Severity, debugMessages[i].severity);
    SERIALISE_ELEMENT(uint32_t, ID, debugMessages[i].messageID);
    SERIALISE_ELEMENT(string, Description, desc);

    if(m_State == READING)
    {
      DebugMessage msg;
      msg.eventID = m_CurEventID;
      msg.source = MessageSource::API;
      msg.category = Category;
      msg.severity = Severity;
      msg.messageID = ID;
      msg.description = Description;

      m_DebugMessages.push_back(msg);
    }
  }
}

bool WrappedOpenGL::RecordUpdateCheck(GLResourceRecord *record)
{
  // if nothing is bound, don't serialise chunk
  if(record == NULL)
    return false;

  // if we've already stopped tracking this object, return as such
  if(record && record->UpdateCount > 64)
    return false;

  // increase update count
  record->UpdateCount++;

  // if update count is high, mark as dirty
  if(record && record->UpdateCount > 64)
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());

    return false;
  }

  return true;
}

void WrappedOpenGL::DebugSnoop(GLenum source, GLenum type, GLuint id, GLenum severity,
                               GLsizei length, const GLchar *message)
{
  if(type != eGL_DEBUG_TYPE_PUSH_GROUP && type != eGL_DEBUG_TYPE_POP_GROUP)
  {
    if(type != eGL_DEBUG_TYPE_PERFORMANCE && type != eGL_DEBUG_TYPE_OTHER)
    {
      RDCLOG("Got a Debug message from %s, type %s, ID %d, severity %s:\n'%s'",
             ToStr::Get(source).c_str(), ToStr::Get(type).c_str(), id, ToStr::Get(severity).c_str(),
             message);
      if(m_DebugMsgContext != "")
        RDCLOG("Debug Message context: \"%s\"", m_DebugMsgContext.c_str());
    }

    if(m_State == WRITING_CAPFRAME)
    {
      DebugMessage msg;

      msg.messageID = id;
      msg.description = string(message, message + length);

      switch(severity)
      {
        case eGL_DEBUG_SEVERITY_HIGH: msg.severity = MessageSeverity::High; break;
        case eGL_DEBUG_SEVERITY_MEDIUM: msg.severity = MessageSeverity::Medium; break;
        case eGL_DEBUG_SEVERITY_LOW: msg.severity = MessageSeverity::Low; break;
        case eGL_DEBUG_SEVERITY_NOTIFICATION:
        default: msg.severity = MessageSeverity::Info; break;
      }

      if(source == eGL_DEBUG_SOURCE_APPLICATION || type == eGL_DEBUG_TYPE_MARKER)
      {
        msg.category = MessageCategory::Application_Defined;
      }
      else if(source == eGL_DEBUG_SOURCE_SHADER_COMPILER)
      {
        msg.category = MessageCategory::Shaders;
      }
      else
      {
        switch(type)
        {
          case eGL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            msg.category = MessageCategory::Deprecated;
            break;
          case eGL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: msg.category = MessageCategory::Undefined; break;
          case eGL_DEBUG_TYPE_PORTABILITY: msg.category = MessageCategory::Portability; break;
          case eGL_DEBUG_TYPE_PERFORMANCE: msg.category = MessageCategory::Performance; break;
          case eGL_DEBUG_TYPE_ERROR:
          case eGL_DEBUG_TYPE_OTHER:
          default: msg.category = MessageCategory::Miscellaneous; break;
        }
      }

      m_DebugMessages.push_back(msg);
    }
  }

  if(m_RealDebugFunc && !RenderDoc::Inst().GetCaptureOptions().DebugOutputMute)
    m_RealDebugFunc(source, type, id, severity, length, message, m_RealDebugFuncParam);
}

void WrappedOpenGL::ReadLogInitialisation()
{
  uint64_t frameOffset = 0;

  m_pSerialiser->SetDebugText(true);

  m_pSerialiser->Rewind();

  int chunkIdx = 0;

  struct chunkinfo
  {
    chunkinfo() : count(0), totalsize(0), total(0.0) {}
    int count;
    uint64_t totalsize;
    double total;
  };

  map<GLChunkType, chunkinfo> chunkInfos;

  SCOPED_TIMER("chunk initialisation");

  for(;;)
  {
    PerformanceTimer timer;

    uint64_t offset = m_pSerialiser->GetOffset();

    GLChunkType context = (GLChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

    if(context == CAPTURE_SCOPE)
    {
      // immediately read rest of log into memory
      m_pSerialiser->SetPersistentBlock(offset);
    }

    chunkIdx++;

    ProcessChunk(offset, context);

    m_pSerialiser->PopContext(context);

    RenderDoc::Inst().SetProgress(FileInitialRead, float(offset) / float(m_pSerialiser->GetSize()));

    if(context == CAPTURE_SCOPE)
    {
      frameOffset = offset;

      GetResourceManager()->ApplyInitialContents();

      ContextReplayLog(READING, 0, 0, false);
    }

    uint64_t offset2 = m_pSerialiser->GetOffset();

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offset2 - offset;
    chunkInfos[context].count++;

    if(context == CAPTURE_SCOPE)
      break;

    if(m_pSerialiser->AtEnd())
      break;
  }

#if ENABLED(RDOC_DEVEL)
  for(auto it = chunkInfos.begin(); it != chunkInfos.end(); ++it)
  {
    double dcount = double(it->second.count);

    RDCDEBUG(
        "% 5d chunks - Time: %9.3fms total/%9.3fms avg - Size: %8.3fMB total/%7.3fMB avg - %s (%u)",
        it->second.count, it->second.total, it->second.total / dcount,
        double(it->second.totalsize) / (1024.0 * 1024.0),
        double(it->second.totalsize) / (dcount * 1024.0 * 1024.0), GetChunkName(it->first),
        uint32_t(it->first));
  }
#endif

  m_FrameRecord.frameInfo.uncompressedFileSize = m_pSerialiser->GetSize();
  m_FrameRecord.frameInfo.compressedFileSize = m_pSerialiser->GetFileSize();
  m_FrameRecord.frameInfo.persistentSize = m_pSerialiser->GetSize() - frameOffset;
  m_FrameRecord.frameInfo.initDataSize = chunkInfos[(GLChunkType)INITIAL_CONTENTS].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           m_pSerialiser->GetSize() - frameOffset);

  m_pSerialiser->SetDebugText(false);
}

void WrappedOpenGL::ProcessChunk(uint64_t offset, GLChunkType context)
{
  switch(context)
  {
    case DEVICE_INIT:
    {
      SERIALISE_ELEMENT(ResourceId, immContextId, ResourceId());
      SERIALISE_ELEMENT(ResourceId, vaoId, ResourceId());

      GetResourceManager()->AddLiveResource(immContextId,
                                            GLResource(NULL, eResSpecial, eSpecialResContext));
      GetResourceManager()->AddLiveResource(vaoId, VertexArrayRes(NULL, 0));
      break;
    }
    case GEN_TEXTURE: Serialise_glGenTextures(0, NULL); break;
    case CREATE_TEXTURE: Serialise_glCreateTextures(eGL_NONE, 0, NULL); break;
    case ACTIVE_TEXTURE: Serialise_glActiveTexture(eGL_NONE); break;
    case BIND_TEXTURE: Serialise_glBindTexture(eGL_NONE, 0); break;
    case BIND_TEXTURES: Serialise_glBindTextures(0, 0, NULL); break;
    case BIND_MULTI_TEX: Serialise_glBindMultiTextureEXT(eGL_NONE, eGL_NONE, 0); break;
    case BIND_TEXTURE_UNIT: Serialise_glBindTextureUnit(0, 0); break;
    case BIND_IMAGE_TEXTURE: Serialise_glBindImageTexture(0, 0, 0, 0, 0, eGL_NONE, eGL_NONE); break;
    case BIND_IMAGE_TEXTURES: Serialise_glBindImageTextures(0, 0, NULL); break;
    case TEXSTORAGE1D: Serialise_glTextureStorage1DEXT(0, eGL_NONE, 0, eGL_NONE, 0); break;
    case TEXSTORAGE2D: Serialise_glTextureStorage2DEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0); break;
    case TEXSTORAGE3D: Serialise_glTextureStorage3DEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0); break;
    case TEXSTORAGE2DMS:
      Serialise_glTextureStorage2DMultisampleEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0, GL_FALSE);
      break;
    case TEXSTORAGE3DMS:
      Serialise_glTextureStorage3DMultisampleEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, GL_FALSE);
      break;
    case TEXIMAGE1D:
      Serialise_glTextureImage1DEXT(0, eGL_NONE, 0, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
      break;
    case TEXIMAGE2D:
      Serialise_glTextureImage2DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
      break;
    case TEXIMAGE3D:
      Serialise_glTextureImage3DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
      break;
    case TEXSUBIMAGE1D:
      Serialise_glTextureSubImage1DEXT(0, eGL_NONE, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
      break;
    case TEXSUBIMAGE2D:
      Serialise_glTextureSubImage2DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
      break;
    case TEXSUBIMAGE3D:
      Serialise_glTextureSubImage3DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
      break;
    case TEXIMAGE1D_COMPRESSED:
      Serialise_glCompressedTextureImage1DEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, NULL);
      break;
    case TEXIMAGE2D_COMPRESSED:
      Serialise_glCompressedTextureImage2DEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0, NULL);
      break;
    case TEXIMAGE3D_COMPRESSED:
      Serialise_glCompressedTextureImage3DEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0, 0, NULL);
      break;
    case TEXSUBIMAGE1D_COMPRESSED:
      Serialise_glCompressedTextureSubImage1DEXT(0, eGL_NONE, 0, 0, 0, eGL_NONE, 0, NULL);
      break;
    case TEXSUBIMAGE2D_COMPRESSED:
      Serialise_glCompressedTextureSubImage2DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, 0, NULL);
      break;
    case TEXSUBIMAGE3D_COMPRESSED:
      Serialise_glCompressedTextureSubImage3DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, eGL_NONE, 0, NULL);
      break;
    case TEXBUFFER: Serialise_glTextureBufferEXT(0, eGL_NONE, eGL_NONE, 0); break;
    case TEXBUFFER_RANGE: Serialise_glTextureBufferRangeEXT(0, eGL_NONE, eGL_NONE, 0, 0, 0); break;
    case PIXELSTORE: Serialise_glPixelStorei(eGL_NONE, 0); break;
    case TEXPARAMETERF: Serialise_glTextureParameterfEXT(0, eGL_NONE, eGL_NONE, 0); break;
    case TEXPARAMETERFV: Serialise_glTextureParameterfvEXT(0, eGL_NONE, eGL_NONE, NULL); break;
    case TEXPARAMETERI: Serialise_glTextureParameteriEXT(0, eGL_NONE, eGL_NONE, 0); break;
    case TEXPARAMETERIV: Serialise_glTextureParameterivEXT(0, eGL_NONE, eGL_NONE, NULL); break;
    case TEXPARAMETERIIV: Serialise_glTextureParameterIivEXT(0, eGL_NONE, eGL_NONE, NULL); break;
    case TEXPARAMETERIUIV: Serialise_glTextureParameterIuivEXT(0, eGL_NONE, eGL_NONE, NULL); break;
    case GENERATE_MIPMAP: Serialise_glGenerateTextureMipmapEXT(0, eGL_NONE); break;
    case COPY_SUBIMAGE:
      Serialise_glCopyImageSubData(0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0);
      break;
    case COPY_IMAGE1D:
      Serialise_glCopyTextureImage1DEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
      break;
    case COPY_IMAGE2D:
      Serialise_glCopyTextureImage2DEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0, 0);
      break;
    case COPY_SUBIMAGE1D: Serialise_glCopyTextureSubImage1DEXT(0, eGL_NONE, 0, 0, 0, 0, 0); break;
    case COPY_SUBIMAGE2D:
      Serialise_glCopyTextureSubImage2DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0);
      break;
    case COPY_SUBIMAGE3D:
      Serialise_glCopyTextureSubImage3DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, 0);
      break;
    case TEXTURE_VIEW: Serialise_glTextureView(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0); break;

    case CREATE_SHADER: Serialise_glCreateShader(0, eGL_NONE); break;
    case CREATE_PROGRAM: Serialise_glCreateProgram(0); break;
    case CREATE_SHADERPROGRAM: Serialise_glCreateShaderProgramv(0, eGL_NONE, 0, NULL); break;
    case COMPILESHADER: Serialise_glCompileShader(0); break;
    case SHADERSOURCE: Serialise_glShaderSource(0, 0, NULL, NULL); break;
    case ATTACHSHADER: Serialise_glAttachShader(0, 0); break;
    case DETACHSHADER: Serialise_glDetachShader(0, 0); break;
    case USEPROGRAM: Serialise_glUseProgram(0); break;
    case PROGRAMPARAMETER: Serialise_glProgramParameteri(0, eGL_NONE, 0); break;
    case FEEDBACK_VARYINGS: Serialise_glTransformFeedbackVaryings(0, 0, NULL, eGL_NONE); break;
    case BINDATTRIB_LOCATION: Serialise_glBindAttribLocation(0, 0, NULL); break;
    case BINDFRAGDATA_LOCATION: Serialise_glBindFragDataLocation(0, 0, NULL); break;
    case BINDFRAGDATA_LOCATION_INDEXED:
      Serialise_glBindFragDataLocationIndexed(0, 0, 0, NULL);
      break;
    case UNIFORM_BLOCKBIND: Serialise_glUniformBlockBinding(0, 0, 0); break;
    case STORAGE_BLOCKBIND: Serialise_glShaderStorageBlockBinding(0, 0, 0); break;
    case UNIFORM_SUBROUTINE: Serialise_glUniformSubroutinesuiv(eGL_NONE, 0, NULL); break;
    case PROGRAMUNIFORM_VECTOR:
      Serialise_glProgramUniformVector(0, eGL_NONE, 0, 0, UNIFORM_UNKNOWN);
      break;
    case PROGRAMUNIFORM_MATRIX:
      Serialise_glProgramUniformMatrix(0, 0, 0, 0, NULL, UNIFORM_UNKNOWN);
      break;
    case LINKPROGRAM: Serialise_glLinkProgram(0); break;

    case NAMEDSTRING: Serialise_glNamedStringARB(eGL_NONE, 0, NULL, 0, NULL); break;
    case DELETENAMEDSTRING: Serialise_glDeleteNamedStringARB(0, NULL); break;
    case COMPILESHADERINCLUDE: Serialise_glCompileShaderIncludeARB(0, 0, NULL, NULL); break;

    case GEN_FEEDBACK: Serialise_glGenTransformFeedbacks(0, NULL); break;
    case CREATE_FEEDBACK: Serialise_glCreateTransformFeedbacks(0, NULL); break;
    case BIND_FEEDBACK: Serialise_glBindTransformFeedback(eGL_NONE, 0); break;
    case BEGIN_FEEDBACK: Serialise_glBeginTransformFeedback(eGL_NONE); break;
    case END_FEEDBACK: Serialise_glEndTransformFeedback(); break;
    case PAUSE_FEEDBACK: Serialise_glPauseTransformFeedback(); break;
    case RESUME_FEEDBACK: Serialise_glResumeTransformFeedback(); break;

    case GEN_PROGRAMPIPE: Serialise_glGenProgramPipelines(0, NULL); break;
    case CREATE_PROGRAMPIPE: Serialise_glCreateProgramPipelines(0, NULL); break;
    case USE_PROGRAMSTAGES: Serialise_glUseProgramStages(0, 0, 0); break;
    case BIND_PROGRAMPIPE: Serialise_glBindProgramPipeline(0); break;

    case FENCE_SYNC: Serialise_glFenceSync(NULL, eGL_NONE, 0); break;
    case CLIENTWAIT_SYNC: Serialise_glClientWaitSync(NULL, 0, 0); break;
    case WAIT_SYNC: Serialise_glWaitSync(NULL, 0, 0); break;

    case GEN_QUERIES: Serialise_glGenQueries(0, NULL); break;
    case CREATE_QUERIES: Serialise_glCreateQueries(eGL_NONE, 0, NULL); break;
    case BEGIN_QUERY: Serialise_glBeginQuery(eGL_NONE, 0); break;
    case BEGIN_QUERY_INDEXED: Serialise_glBeginQueryIndexed(eGL_NONE, 0, 0); break;
    case END_QUERY: Serialise_glEndQuery(eGL_NONE); break;
    case END_QUERY_INDEXED: Serialise_glEndQueryIndexed(eGL_NONE, 0); break;
    case BEGIN_CONDITIONAL: Serialise_glBeginConditionalRender(0, eGL_NONE); break;
    case END_CONDITIONAL: Serialise_glEndConditionalRender(); break;
    case QUERY_COUNTER: Serialise_glQueryCounter(0, eGL_NONE); break;

    case CLEAR_COLOR: Serialise_glClearColor(0, 0, 0, 0); break;
    case CLEAR_DEPTH: Serialise_glClearDepth(0); break;
    case CLEAR_STENCIL: Serialise_glClearStencil(0); break;
    case CLEAR: Serialise_glClear(0); break;
    case CLEARBUFFERF: Serialise_glClearNamedFramebufferfv(0, eGL_NONE, 0, NULL); break;
    case CLEARBUFFERI: Serialise_glClearNamedFramebufferiv(0, eGL_NONE, 0, NULL); break;
    case CLEARBUFFERUI: Serialise_glClearNamedFramebufferuiv(0, eGL_NONE, 0, NULL); break;
    case CLEARBUFFERFI: Serialise_glClearNamedFramebufferfi(0, eGL_NONE, 0, 0); break;
    case CLEARBUFFERDATA:
      Serialise_glClearNamedBufferDataEXT(0, eGL_NONE, eGL_NONE, eGL_NONE, NULL);
      break;
    case CLEARBUFFERSUBDATA:
      Serialise_glClearNamedBufferSubDataEXT(0, eGL_NONE, 0, 0, eGL_NONE, eGL_NONE, NULL);
      break;
    case CLEARTEXIMAGE: Serialise_glClearTexImage(0, 0, eGL_NONE, eGL_NONE, NULL); break;
    case CLEARTEXSUBIMAGE:
      Serialise_glClearTexSubImage(0, 0, 0, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
      break;
    case POLYGON_MODE: Serialise_glPolygonMode(eGL_NONE, eGL_NONE); break;
    case POLYGON_OFFSET: Serialise_glPolygonOffset(0, 0); break;
    case POLYGON_OFFSET_CLAMP: Serialise_glPolygonOffsetClampEXT(0, 0, 0); break;
    case CULL_FACE: Serialise_glCullFace(eGL_NONE); break;
    case HINT: Serialise_glHint(eGL_NONE, eGL_NONE); break;
    case ENABLE: Serialise_glEnable(eGL_NONE); break;
    case DISABLE: Serialise_glDisable(eGL_NONE); break;
    case ENABLEI: Serialise_glEnablei(eGL_NONE, 0); break;
    case DISABLEI: Serialise_glDisablei(eGL_NONE, 0); break;
    case FRONT_FACE: Serialise_glFrontFace(eGL_NONE); break;
    case BLEND_FUNC: Serialise_glBlendFunc(eGL_NONE, eGL_NONE); break;
    case BLEND_FUNCI: Serialise_glBlendFunci(0, eGL_NONE, eGL_NONE); break;
    case BLEND_COLOR: Serialise_glBlendColor(0, 0, 0, 0); break;
    case BLEND_FUNC_SEP:
      Serialise_glBlendFuncSeparate(eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
      break;
    case BLEND_FUNC_SEPI:
      Serialise_glBlendFuncSeparatei(0, eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
      break;
    case BLEND_EQ: Serialise_glBlendEquation(eGL_NONE); break;
    case BLEND_EQI: Serialise_glBlendEquationi(0, eGL_NONE); break;
    case BLEND_EQ_SEP: Serialise_glBlendEquationSeparate(eGL_NONE, eGL_NONE); break;
    case BLEND_EQ_SEPI: Serialise_glBlendEquationSeparatei(0, eGL_NONE, eGL_NONE); break;
    case BLEND_BARRIER: Serialise_glBlendBarrierKHR(); break;

    case LOGIC_OP: Serialise_glLogicOp(eGL_NONE); break;

    case STENCIL_OP: Serialise_glStencilOp(eGL_NONE, eGL_NONE, eGL_NONE); break;
    case STENCIL_OP_SEP:
      Serialise_glStencilOpSeparate(eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
      break;
    case STENCIL_FUNC: Serialise_glStencilFunc(eGL_NONE, 0, 0); break;
    case STENCIL_FUNC_SEP: Serialise_glStencilFuncSeparate(eGL_NONE, eGL_NONE, 0, 0); break;
    case STENCIL_MASK: Serialise_glStencilMask(0); break;
    case STENCIL_MASK_SEP: Serialise_glStencilMaskSeparate(eGL_NONE, 0); break;

    case COLOR_MASK: Serialise_glColorMask(0, 0, 0, 0); break;
    case COLOR_MASKI: Serialise_glColorMaski(0, 0, 0, 0, 0); break;
    case SAMPLE_MASK: Serialise_glSampleMaski(0, 0); break;
    case SAMPLE_COVERAGE: Serialise_glSampleCoverage(0.0f, 0); break;
    case MIN_SAMPLE_SHADING: Serialise_glMinSampleShading(0.0f); break;
    case RASTER_SAMPLES: Serialise_glRasterSamplesEXT(0, 0); break;
    case DEPTH_FUNC: Serialise_glDepthFunc(eGL_NONE); break;
    case DEPTH_MASK: Serialise_glDepthMask(0); break;
    case DEPTH_RANGE: Serialise_glDepthRange(0, 0); break;
    case DEPTH_RANGEF: Serialise_glDepthRangef(0, 0); break;
    case DEPTH_RANGE_IDX: Serialise_glDepthRangeIndexed(0, 0.0, 0.0); break;
    case DEPTH_RANGEARRAY: Serialise_glDepthRangeArrayv(0, 0, NULL); break;
    case DEPTH_BOUNDS: Serialise_glDepthBoundsEXT(0, 0); break;
    case CLIP_CONTROL: Serialise_glClipControl(eGL_NONE, eGL_NONE); break;
    case PROVOKING_VERTEX: Serialise_glProvokingVertex(eGL_NONE); break;
    case PRIMITIVE_RESTART: Serialise_glPrimitiveRestartIndex(0); break;
    case PATCH_PARAMI: Serialise_glPatchParameteri(eGL_NONE, 0); break;
    case PATCH_PARAMFV: Serialise_glPatchParameterfv(eGL_NONE, NULL); break;
    case LINE_WIDTH: Serialise_glLineWidth(0.0f); break;
    case POINT_SIZE: Serialise_glPointSize(0.0f); break;
    case POINT_PARAMF: Serialise_glPointParameterf(eGL_NONE, 0.0f); break;
    case POINT_PARAMFV: Serialise_glPointParameterfv(eGL_NONE, NULL); break;
    case POINT_PARAMI: Serialise_glPointParameteri(eGL_NONE, 0); break;
    case POINT_PARAMIV: Serialise_glPointParameteriv(eGL_NONE, NULL); break;
    case VIEWPORT: Serialise_glViewport(0, 0, 0, 0); break;
    case VIEWPORT_ARRAY: Serialise_glViewportArrayv(0, 0, 0); break;
    case SCISSOR: Serialise_glScissor(0, 0, 0, 0); break;
    case SCISSOR_ARRAY: Serialise_glScissorArrayv(0, 0, 0); break;
    case BIND_VERTEXBUFFER: Serialise_glVertexArrayBindVertexBufferEXT(0, 0, 0, 0, 0); break;
    case BIND_VERTEXBUFFERS: Serialise_glVertexArrayVertexBuffers(0, 0, 0, NULL, NULL, NULL); break;
    case VERTEXBINDINGDIVISOR: Serialise_glVertexArrayVertexBindingDivisorEXT(0, 0, 0); break;
    case DISPATCH_COMPUTE: Serialise_glDispatchCompute(0, 0, 0); break;
    case DISPATCH_COMPUTE_GROUP_SIZE:
      Serialise_glDispatchComputeGroupSizeARB(0, 0, 0, 0, 0, 0);
      break;
    case DISPATCH_COMPUTE_INDIRECT: Serialise_glDispatchComputeIndirect(0); break;
    case MEMORY_BARRIER: Serialise_glMemoryBarrier(0); break;
    case MEMORY_BARRIER_BY_REGION: Serialise_glMemoryBarrierByRegion(0); break;
    case TEXTURE_BARRIER: Serialise_glTextureBarrier(); break;
    case DRAWARRAYS: Serialise_glDrawArrays(eGL_NONE, 0, 0); break;
    case DRAWARRAYS_INDIRECT: Serialise_glDrawArraysIndirect(eGL_NONE, 0); break;
    case DRAWARRAYS_INSTANCED: Serialise_glDrawArraysInstanced(eGL_NONE, 0, 0, 0); break;
    case DRAWARRAYS_INSTANCEDBASEINSTANCE:
      Serialise_glDrawArraysInstancedBaseInstance(eGL_NONE, 0, 0, 0, 0);
      break;
    case DRAWELEMENTS: Serialise_glDrawElements(eGL_NONE, 0, eGL_NONE, NULL); break;
    case DRAWELEMENTS_INDIRECT: Serialise_glDrawElementsIndirect(eGL_NONE, eGL_NONE, 0); break;
    case DRAWRANGEELEMENTS: Serialise_glDrawRangeElements(eGL_NONE, 0, 0, 0, eGL_NONE, NULL); break;
    case DRAWRANGEELEMENTSBASEVERTEX:
      Serialise_glDrawRangeElementsBaseVertex(eGL_NONE, 0, 0, 0, eGL_NONE, NULL, 0);
      break;
    case DRAWELEMENTS_INSTANCED:
      Serialise_glDrawElementsInstanced(eGL_NONE, 0, eGL_NONE, NULL, 0);
      break;
    case DRAWELEMENTS_INSTANCEDBASEINSTANCE:
      Serialise_glDrawElementsInstancedBaseInstance(eGL_NONE, 0, eGL_NONE, NULL, 0, 0);
      break;
    case DRAWELEMENTS_BASEVERTEX:
      Serialise_glDrawElementsBaseVertex(eGL_NONE, 0, eGL_NONE, NULL, 0);
      break;
    case DRAWELEMENTS_INSTANCEDBASEVERTEX:
      Serialise_glDrawElementsInstancedBaseVertex(eGL_NONE, 0, eGL_NONE, NULL, 0, 0);
      break;
    case DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE:
      Serialise_glDrawElementsInstancedBaseVertexBaseInstance(eGL_NONE, 0, eGL_NONE, NULL, 0, 0, 0);
      break;
    case DRAW_FEEDBACK: Serialise_glDrawTransformFeedback(eGL_NONE, 0); break;
    case DRAW_FEEDBACK_INSTANCED: Serialise_glDrawTransformFeedbackInstanced(eGL_NONE, 0, 0); break;
    case DRAW_FEEDBACK_STREAM: Serialise_glDrawTransformFeedbackStream(eGL_NONE, 0, 0); break;
    case DRAW_FEEDBACK_STREAM_INSTANCED:
      Serialise_glDrawTransformFeedbackStreamInstanced(eGL_NONE, 0, 0, 0);
      break;
    case MULTI_DRAWARRAYS: Serialise_glMultiDrawArrays(eGL_NONE, NULL, NULL, 0); break;
    case MULTI_DRAWELEMENTS:
      Serialise_glMultiDrawElements(eGL_NONE, NULL, eGL_NONE, NULL, 0);
      break;
    case MULTI_DRAWELEMENTSBASEVERTEX:
      Serialise_glMultiDrawElementsBaseVertex(eGL_NONE, NULL, eGL_NONE, NULL, 0, NULL);
      break;
    case MULTI_DRAWARRAYS_INDIRECT:
      Serialise_glMultiDrawArraysIndirect(eGL_NONE, NULL, 0, 0);
      break;
    case MULTI_DRAWELEMENTS_INDIRECT:
      Serialise_glMultiDrawElementsIndirect(eGL_NONE, eGL_NONE, NULL, 0, 0);
      break;
    case MULTI_DRAWARRAYS_INDIRECT_COUNT:
      Serialise_glMultiDrawArraysIndirectCountARB(eGL_NONE, 0, 0, 0, 0);
      break;
    case MULTI_DRAWELEMENTS_INDIRECT_COUNT:
      Serialise_glMultiDrawElementsIndirectCountARB(eGL_NONE, eGL_NONE, 0, 0, 0, 0);
      break;

    case GEN_FRAMEBUFFERS: Serialise_glGenFramebuffers(0, NULL); break;
    case CREATE_FRAMEBUFFERS: Serialise_glCreateFramebuffers(0, NULL); break;
    case FRAMEBUFFER_TEX: Serialise_glNamedFramebufferTextureEXT(0, eGL_NONE, 0, 0); break;
    case FRAMEBUFFER_TEX1D:
      Serialise_glNamedFramebufferTexture1DEXT(0, eGL_NONE, eGL_NONE, 0, 0);
      break;
    case FRAMEBUFFER_TEX2D:
      Serialise_glNamedFramebufferTexture2DEXT(0, eGL_NONE, eGL_NONE, 0, 0);
      break;
    case FRAMEBUFFER_TEX2DMS:
      Serialise_glFramebufferTexture2DMultisampleEXT(0, eGL_NONE, eGL_NONE, eGL_NONE, 0, 0, 0);
      break;
    case FRAMEBUFFER_TEX3D:
      Serialise_glNamedFramebufferTexture3DEXT(0, eGL_NONE, eGL_NONE, 0, 0, 0);
      break;
    case FRAMEBUFFER_RENDBUF:
      Serialise_glNamedFramebufferRenderbufferEXT(0, eGL_NONE, eGL_NONE, 0);
      break;
    case FRAMEBUFFER_TEXLAYER:
      Serialise_glNamedFramebufferTextureLayerEXT(0, eGL_NONE, 0, 0, 0);
      break;
    case FRAMEBUFFER_PARAM: Serialise_glNamedFramebufferParameteriEXT(0, eGL_NONE, 0); break;
    case READ_BUFFER: Serialise_glFramebufferReadBufferEXT(0, eGL_NONE); break;
    case BIND_FRAMEBUFFER: Serialise_glBindFramebuffer(eGL_NONE, 0); break;
    case DRAW_BUFFER: Serialise_glFramebufferDrawBufferEXT(0, eGL_NONE); break;
    case DRAW_BUFFERS: Serialise_glFramebufferDrawBuffersEXT(0, 0, NULL); break;
    case BLIT_FRAMEBUFFER:
      Serialise_glBlitNamedFramebuffer(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, eGL_NONE);
      break;

    case GEN_RENDERBUFFERS: Serialise_glGenRenderbuffers(0, NULL); break;
    case CREATE_RENDERBUFFERS: Serialise_glCreateRenderbuffers(0, NULL); break;
    case RENDERBUFFER_STORAGE: Serialise_glNamedRenderbufferStorageEXT(0, eGL_NONE, 0, 0); break;
    case RENDERBUFFER_STORAGEMS:
      Serialise_glNamedRenderbufferStorageMultisampleEXT(0, 0, eGL_NONE, 0, 0);
      break;

    case GEN_SAMPLERS: Serialise_glGenSamplers(0, NULL); break;
    case CREATE_SAMPLERS: Serialise_glCreateSamplers(0, NULL); break;
    case SAMPLER_PARAMETERI: Serialise_glSamplerParameteri(0, eGL_NONE, 0); break;
    case SAMPLER_PARAMETERF: Serialise_glSamplerParameterf(0, eGL_NONE, 0); break;
    case SAMPLER_PARAMETERIV: Serialise_glSamplerParameteriv(0, eGL_NONE, NULL); break;
    case SAMPLER_PARAMETERFV: Serialise_glSamplerParameterfv(0, eGL_NONE, NULL); break;
    case SAMPLER_PARAMETERIIV: Serialise_glSamplerParameterIiv(0, eGL_NONE, NULL); break;
    case SAMPLER_PARAMETERIUIV: Serialise_glSamplerParameterIuiv(0, eGL_NONE, NULL); break;
    case BIND_SAMPLER: Serialise_glBindSampler(0, 0); break;
    case BIND_SAMPLERS: Serialise_glBindSamplers(0, 0, NULL); break;

    case GEN_BUFFER: Serialise_glGenBuffers(0, NULL); break;
    case CREATE_BUFFER: Serialise_glCreateBuffers(0, NULL); break;
    case BIND_BUFFER: Serialise_glBindBuffer(eGL_NONE, 0); break;
    case BIND_BUFFER_BASE: Serialise_glBindBufferBase(eGL_NONE, 0, 0); break;
    case BIND_BUFFER_RANGE: Serialise_glBindBufferRange(eGL_NONE, 0, 0, 0, 0); break;
    case BIND_BUFFERS_BASE: Serialise_glBindBuffersBase(eGL_NONE, 0, 0, NULL); break;
    case BIND_BUFFERS_RANGE: Serialise_glBindBuffersRange(eGL_NONE, 0, 0, NULL, NULL, NULL); break;
    case BUFFERSTORAGE: Serialise_glNamedBufferStorageEXT(0, 0, NULL, 0); break;
    case BUFFERDATA: Serialise_glNamedBufferDataEXT(eGL_NONE, 0, NULL, eGL_NONE); break;
    case BUFFERSUBDATA: Serialise_glNamedBufferSubDataEXT(0, 0, 0, NULL); break;
    case COPYBUFFERSUBDATA: Serialise_glNamedCopyBufferSubDataEXT(0, 0, 0, 0, 0); break;
    case UNMAP: Serialise_glUnmapNamedBufferEXT(eGL_NONE); break;
    case FLUSHMAP: Serialise_glFlushMappedNamedBufferRangeEXT(0, 0, 0); break;
    case GEN_VERTEXARRAY: Serialise_glGenVertexArrays(0, NULL); break;
    case CREATE_VERTEXARRAY: Serialise_glCreateVertexArrays(0, NULL); break;
    case BIND_VERTEXARRAY: Serialise_glBindVertexArray(0); break;
    case VERTEXATTRIBPOINTER:
      Serialise_glVertexArrayVertexAttribOffsetEXT(0, 0, 0, 0, eGL_NONE, 0, 0, 0);
      break;
    case VERTEXATTRIBIPOINTER:
      Serialise_glVertexArrayVertexAttribIOffsetEXT(0, 0, 0, 0, eGL_NONE, 0, 0);
      break;
    case VERTEXATTRIBLPOINTER:
      Serialise_glVertexArrayVertexAttribLOffsetEXT(0, 0, 0, 0, eGL_NONE, 0, 0);
      break;
    case ENABLEVERTEXATTRIBARRAY: Serialise_glEnableVertexArrayAttribEXT(0, 0); break;
    case DISABLEVERTEXATTRIBARRAY: Serialise_glDisableVertexArrayAttribEXT(0, 0); break;
    case VERTEXATTRIB_GENERIC:
      Serialise_glVertexAttrib(0, 0, eGL_NONE, GL_FALSE, NULL, Attrib_packed);
      break;
    case VERTEXATTRIBFORMAT:
      Serialise_glVertexArrayVertexAttribFormatEXT(0, 0, 0, eGL_NONE, 0, 0);
      break;
    case VERTEXATTRIBIFORMAT:
      Serialise_glVertexArrayVertexAttribIFormatEXT(0, 0, 0, eGL_NONE, 0);
      break;
    case VERTEXATTRIBLFORMAT:
      Serialise_glVertexArrayVertexAttribLFormatEXT(0, 0, 0, eGL_NONE, 0);
      break;
    case VERTEXATTRIBDIVISOR: Serialise_glVertexArrayVertexAttribDivisorEXT(0, 0, 0); break;
    case VERTEXATTRIBBINDING: Serialise_glVertexArrayVertexAttribBindingEXT(0, 0, 0); break;

    case VAO_ELEMENT_BUFFER: Serialise_glVertexArrayElementBuffer(0, 0); break;
    case FEEDBACK_BUFFER_BASE: Serialise_glTransformFeedbackBufferBase(0, 0, 0); break;
    case FEEDBACK_BUFFER_RANGE: Serialise_glTransformFeedbackBufferRange(0, 0, 0, 0, 0); break;

    case OBJECT_LABEL: Serialise_glObjectLabel(eGL_NONE, 0, 0, NULL); break;
    case BEGIN_EVENT: Serialise_glPushDebugGroup(eGL_NONE, 0, 0, NULL); break;
    case SET_MARKER:
      Serialise_glDebugMessageInsert(eGL_NONE, eGL_NONE, 0, eGL_NONE, 0, NULL);
      break;
    case END_EVENT: Serialise_glPopDebugGroup(); break;

    case CAPTURE_SCOPE: Serialise_CaptureScope(offset); break;
    case CONTEXT_CAPTURE_HEADER:
      // normally this would be handled as a special case when we start processing the frame,
      // but it can be emitted mid-frame if MakeCurrent is called on a different context.
      // when processed here, we always want to apply the contents
      Serialise_BeginCaptureFrame(true);
      break;
    case CONTEXT_CAPTURE_FOOTER:
    {
      bool HasCallstack = false;
      m_pSerialiser->Serialise("HasCallstack", HasCallstack);

      if(HasCallstack)
      {
        uint32_t numLevels = 0;
        uint64_t *stack = NULL;

        m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

        m_pSerialiser->SetCallstack(stack, numLevels);

        SAFE_DELETE_ARRAY(stack);
      }

      if(m_State == READING)
      {
        AddEvent("SwapBuffers()");

        DrawcallDescription draw;
        draw.name = "SwapBuffers()";
        draw.flags |= DrawFlags::Present;

        draw.copyDestination = GetResourceManager()->GetOriginalID(
            GetResourceManager()->GetID(TextureRes(GetCtx(), m_FakeBB_Color)));

        AddDrawcall(draw, true);
      }
    }
    break;
    case INTEROP_INIT:
      Serialise_wglDXRegisterObjectNV(GLResource(MakeNullResource), eGL_NONE, NULL);
      break;
    case INTEROP_DATA: Serialise_wglDXLockObjectsNV(GLResource(MakeNullResource)); break;
    case PRIMITIVE_BOUNDING_BOX: Serialise_glPrimitiveBoundingBox(0, 0, 0, 0, 0, 0, 0, 0); break;
    default:
      // ignore system chunks
      if((int)context == (int)INITIAL_CONTENTS)
        GetResourceManager()->Serialise_InitialState(ResourceId(), GLResource(MakeNullResource));
      else if((int)context < (int)FIRST_CHUNK_ID)
        m_pSerialiser->SkipCurrentChunk();
      else
        RDCERR("Unrecognised Chunk type %d", context);
      break;
  }
}

void WrappedOpenGL::ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID,
                                     bool partial)
{
  m_State = readType;

  GLChunkType header = (GLChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);
  RDCASSERTEQUAL(header, CONTEXT_CAPTURE_HEADER);

  if(m_State == EXECUTING && !partial)
  {
    for(size_t i = 0; i < 8; i++)
    {
      GLenum q = QueryEnum(i);
      if(q == eGL_NONE)
        break;

      int indices = IsGLES ? 1 : 8;    // GLES does not support indices
      for(int j = 0; j < indices; j++)
      {
        if(m_ActiveQueries[i][j])
        {
          if(IsGLES)
            m_Real.glEndQuery(q);
          else
            m_Real.glEndQueryIndexed(q, j);
          m_ActiveQueries[i][j] = false;
        }
      }
    }

    if(m_ActiveConditional)
    {
      m_Real.glEndConditionalRender();
      m_ActiveConditional = false;
    }

    if(m_ActiveFeedback)
    {
      m_Real.glEndTransformFeedback();
      m_ActiveFeedback = false;
    }
  }

  Serialise_BeginCaptureFrame(!partial);

  m_pSerialiser->PopContext(header);

  m_CurEvents.clear();

  if(m_State == EXECUTING)
  {
    APIEvent ev = GetEvent(startEventID);
    m_CurEventID = ev.eventID;
    m_pSerialiser->SetOffset(ev.fileOffset);
    m_FirstEventID = startEventID;
    m_LastEventID = endEventID;
  }
  else if(m_State == READING)
  {
    m_CurEventID = 1;
    m_CurDrawcallID = 1;
    m_FirstEventID = 0;
    m_LastEventID = ~0U;
  }

  GetResourceManager()->MarkInFrame(true);

  uint64_t startOffset = m_pSerialiser->GetOffset();

  for(;;)
  {
    if(m_State == EXECUTING && m_CurEventID > endEventID)
    {
      // we can just break out if we've done all the events desired.
      break;
    }

    uint64_t offset = m_pSerialiser->GetOffset();

    GLChunkType chunktype = (GLChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

    ContextProcessChunk(offset, chunktype);

    RenderDoc::Inst().SetProgress(FrameEventsRead,
                                  float(offset - startOffset) / float(m_pSerialiser->GetSize()));

    // for now just abort after capture scope. Really we'd need to support multiple frames
    // but for now this will do.
    if(chunktype == CONTEXT_CAPTURE_FOOTER)
      break;

    m_CurEventID++;
  }

  if(m_State == READING)
  {
    GetFrameRecord().drawcallList = m_ParentDrawcall.Bake();
    GetFrameRecord().frameInfo.debugMessages = GetDebugMessages();

    DrawcallDescription *previous = NULL;
    SetupDrawcallPointers(&m_Drawcalls, GetFrameRecord().drawcallList, NULL, previous);

    // it's easier to remove duplicate usages here than check it as we go.
    // this means if textures are bound in multiple places in the same draw
    // we don't have duplicate uses
    for(auto it = m_ResourceUses.begin(); it != m_ResourceUses.end(); ++it)
    {
      vector<EventUsage> &v = it->second;
      std::sort(v.begin(), v.end());
      v.erase(std::unique(v.begin(), v.end()), v.end());
    }
  }

  GetResourceManager()->MarkInFrame(false);

  m_State = READING;
}

void WrappedOpenGL::ContextProcessChunk(uint64_t offset, GLChunkType chunk)
{
  m_CurChunkOffset = offset;

  m_AddedDrawcall = false;

  ProcessChunk(offset, chunk);

  m_pSerialiser->PopContext(chunk);

  if(m_State == READING && chunk == SET_MARKER)
  {
    // no push/pop necessary
  }
  else if(m_State == READING && chunk == BEGIN_EVENT)
  {
    // push down the drawcallstack to the latest drawcall
    m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());
  }
  else if(m_State == READING && chunk == END_EVENT)
  {
    // refuse to pop off further than the root drawcall (mismatched begin/end events e.g.)
    if(m_DrawcallStack.size() > 1)
      m_DrawcallStack.pop_back();
  }
  else if(m_State == READING)
  {
    if(!m_AddedDrawcall)
      AddEvent(m_pSerialiser->GetDebugStr());
  }

  m_AddedDrawcall = false;
}

void WrappedOpenGL::AddUsage(const DrawcallDescription &d)
{
  DrawFlags DrawDispatchMask = DrawFlags::Drawcall | DrawFlags::Dispatch;
  if(!(d.flags & DrawDispatchMask))
    return;

  const GLHookSet &gl = m_Real;

  GLResourceManager *rm = GetResourceManager();

  void *ctx = GetCtx();

  uint32_t e = d.eventID;

  //////////////////////////////
  // Input

  if(d.flags & DrawFlags::UseIBuffer)
  {
    GLuint ibuffer = 0;
    gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&ibuffer);

    if(ibuffer)
      m_ResourceUses[rm->GetID(BufferRes(ctx, ibuffer))].push_back(
          EventUsage(e, ResourceUsage::IndexBuffer));
  }

  // Vertex buffers and attributes
  GLint numVBufferBindings = 16;
  gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIB_BINDINGS, &numVBufferBindings);

  for(GLuint i = 0; i < (GLuint)numVBufferBindings; i++)
  {
    GLuint buffer = GetBoundVertexBuffer(m_Real, i);

    if(buffer)
      m_ResourceUses[rm->GetID(BufferRes(ctx, buffer))].push_back(
          EventUsage(e, ResourceUsage::VertexBuffer));
  }

  //////////////////////////////
  // Shaders

  {
    GLRenderState rs(&m_Real, NULL, READING);
    rs.FetchState(ctx, this);

    ShaderReflection *refl[6] = {NULL};
    ShaderBindpointMapping mapping[6];

    GLuint curProg = 0;
    gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&curProg);

    if(curProg == 0)
    {
      gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&curProg);

      if(curProg == 0)
      {
        // no program bound at this draw
      }
      else
      {
        auto &pipeDetails = m_Pipelines[rm->GetID(ProgramPipeRes(ctx, curProg))];

        for(size_t i = 0; i < ARRAY_COUNT(pipeDetails.stageShaders); i++)
        {
          if(pipeDetails.stageShaders[i] != ResourceId())
          {
            curProg = rm->GetCurrentResource(pipeDetails.stagePrograms[i]).name;

            refl[i] = &m_Shaders[pipeDetails.stageShaders[i]].reflection;
            GetBindpointMapping(m_Real, curProg, (int)i, refl[i], mapping[i]);
          }
        }
      }
    }
    else
    {
      auto &progDetails = m_Programs[rm->GetID(ProgramRes(ctx, curProg))];

      for(size_t i = 0; i < ARRAY_COUNT(progDetails.stageShaders); i++)
      {
        if(progDetails.stageShaders[i] != ResourceId())
        {
          refl[i] = &m_Shaders[progDetails.stageShaders[i]].reflection;
          GetBindpointMapping(m_Real, curProg, (int)i, refl[i], mapping[i]);
        }
      }
    }

    for(size_t i = 0; i < ARRAY_COUNT(refl); i++)
    {
      EventUsage cb = EventUsage(e, CBUsage(i));
      EventUsage res = EventUsage(e, ResUsage(i));
      EventUsage rw = EventUsage(e, RWResUsage(i));

      if(refl[i])
      {
        for(int32_t c = 0; c < refl[i]->ConstantBlocks.count; c++)
        {
          if(!refl[i]->ConstantBlocks[c].bufferBacked)
            continue;
          if(refl[i]->ConstantBlocks[c].bindPoint < 0 ||
             refl[i]->ConstantBlocks[c].bindPoint >= mapping[i].ConstantBlocks.count)
            continue;

          int32_t bind = mapping[i].ConstantBlocks[refl[i]->ConstantBlocks[c].bindPoint].bind;

          if(rs.UniformBinding[bind].name)
            m_ResourceUses[rm->GetID(BufferRes(ctx, rs.UniformBinding[bind].name))].push_back(cb);
        }

        for(int32_t r = 0; r < refl[i]->ReadWriteResources.count; r++)
        {
          int32_t bind = mapping[i].ReadWriteResources[refl[i]->ReadWriteResources[r].bindPoint].bind;

          if(refl[i]->ReadWriteResources[r].IsTexture)
          {
            if(rs.Images[bind].name)
              m_ResourceUses[rm->GetID(TextureRes(ctx, rs.Images[bind].name))].push_back(rw);
          }
          else
          {
            if(refl[i]->ReadWriteResources[r].variableType.descriptor.cols == 1 &&
               refl[i]->ReadWriteResources[r].variableType.descriptor.rows == 1 &&
               refl[i]->ReadWriteResources[r].variableType.descriptor.type == VarType::UInt)
            {
              if(rs.AtomicCounter[bind].name)
                m_ResourceUses[rm->GetID(BufferRes(ctx, rs.AtomicCounter[bind].name))].push_back(rw);
            }
            else
            {
              if(rs.ShaderStorage[bind].name)
                m_ResourceUses[rm->GetID(BufferRes(ctx, rs.ShaderStorage[bind].name))].push_back(rw);
            }
          }
        }

        for(int32_t r = 0; r < refl[i]->ReadOnlyResources.count; r++)
        {
          int32_t bind = mapping[i].ReadOnlyResources[refl[i]->ReadOnlyResources[r].bindPoint].bind;

          uint32_t *texList = NULL;
          int32_t listSize = 0;

          switch(refl[i]->ReadOnlyResources[r].resType)
          {
            case TextureDim::Unknown: texList = NULL; break;
            case TextureDim::Buffer:
              texList = rs.TexBuffer;
              listSize = (int32_t)ARRAY_COUNT(rs.TexBuffer);
              break;
            case TextureDim::Texture1D:
              texList = rs.Tex1D;
              listSize = (int32_t)ARRAY_COUNT(rs.Tex1D);
              break;
            case TextureDim::Texture1DArray:
              texList = rs.Tex1DArray;
              listSize = (int32_t)ARRAY_COUNT(rs.Tex1DArray);
              break;
            case TextureDim::Texture2D:
              texList = rs.Tex2D;
              listSize = (int32_t)ARRAY_COUNT(rs.Tex2D);
              break;
            case TextureDim::TextureRect:
              texList = rs.TexRect;
              listSize = (int32_t)ARRAY_COUNT(rs.TexRect);
              break;
            case TextureDim::Texture2DArray:
              texList = rs.Tex2DArray;
              listSize = (int32_t)ARRAY_COUNT(rs.Tex2DArray);
              break;
            case TextureDim::Texture2DMS:
              texList = rs.Tex2DMS;
              listSize = (int32_t)ARRAY_COUNT(rs.Tex2DMS);
              break;
            case TextureDim::Texture2DMSArray:
              texList = rs.Tex2DMSArray;
              listSize = (int32_t)ARRAY_COUNT(rs.Tex2DMSArray);
              break;
            case TextureDim::Texture3D:
              texList = rs.Tex3D;
              listSize = (int32_t)ARRAY_COUNT(rs.Tex3D);
              break;
            case TextureDim::TextureCube:
              texList = rs.TexCube;
              listSize = (int32_t)ARRAY_COUNT(rs.TexCube);
              break;
            case TextureDim::TextureCubeArray:
              texList = rs.TexCubeArray;
              listSize = (int32_t)ARRAY_COUNT(rs.TexCubeArray);
              break;
            case TextureDim::Count: RDCERR("Invalid shader resource type"); break;
          }

          if(texList != NULL && bind >= 0 && bind < listSize && texList[bind] != 0)
            m_ResourceUses[rm->GetID(TextureRes(ctx, texList[bind]))].push_back(res);
        }
      }
    }
  }

  //////////////////////////////
  // Feedback

  GLint maxCount = 0;
  gl.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

  for(int i = 0; i < maxCount; i++)
  {
    GLuint buffer = 0;
    gl.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint *)&buffer);

    if(buffer)
      m_ResourceUses[rm->GetID(BufferRes(ctx, buffer))].push_back(
          EventUsage(e, ResourceUsage::StreamOut));
  }

  //////////////////////////////
  // FBO

  GLint numCols = 8;
  gl.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

  GLuint attachment = 0;
  GLenum type = eGL_TEXTURE;
  for(GLint i = 0; i < numCols; i++)
  {
    type = eGL_TEXTURE;

    gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                             (GLint *)&attachment);
    gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    if(attachment)
    {
      if(type == eGL_TEXTURE)
        m_ResourceUses[rm->GetID(TextureRes(ctx, attachment))].push_back(
            EventUsage(e, ResourceUsage::ColorTarget));
      else
        m_ResourceUses[rm->GetID(RenderbufferRes(ctx, attachment))].push_back(
            EventUsage(e, ResourceUsage::ColorTarget));
    }
  }

  gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                           (GLint *)&attachment);
  gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(attachment)
  {
    if(type == eGL_TEXTURE)
      m_ResourceUses[rm->GetID(TextureRes(ctx, attachment))].push_back(
          EventUsage(e, ResourceUsage::DepthStencilTarget));
    else
      m_ResourceUses[rm->GetID(RenderbufferRes(ctx, attachment))].push_back(
          EventUsage(e, ResourceUsage::DepthStencilTarget));
  }

  gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                           (GLint *)&attachment);
  gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(attachment)
  {
    if(type == eGL_TEXTURE)
      m_ResourceUses[rm->GetID(TextureRes(ctx, attachment))].push_back(
          EventUsage(e, ResourceUsage::DepthStencilTarget));
    else
      m_ResourceUses[rm->GetID(RenderbufferRes(ctx, attachment))].push_back(
          EventUsage(e, ResourceUsage::DepthStencilTarget));
  }
}

void WrappedOpenGL::AddDrawcall(const DrawcallDescription &d, bool hasEvents)
{
  m_AddedDrawcall = true;

  WrappedOpenGL *context = this;

  DrawcallDescription draw = d;
  draw.eventID = m_CurEventID;
  draw.drawcallID = m_CurDrawcallID;

  GLenum type;
  GLuint curCol[8] = {0};
  GLuint curDepth = 0;

  {
    GLint numCols = 8;
    m_Real.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

    RDCEraseEl(draw.outputs);

    for(GLint i = 0; i < RDCMIN(numCols, 8); i++)
    {
      type = eGL_TEXTURE;

      m_Real.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&curCol[i]);
      m_Real.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

      if(type == eGL_TEXTURE)
        draw.outputs[i] = GetResourceManager()->GetOriginalID(
            GetResourceManager()->GetID(TextureRes(GetCtx(), curCol[i])));
      else
        draw.outputs[i] = GetResourceManager()->GetOriginalID(
            GetResourceManager()->GetID(RenderbufferRes(GetCtx(), curCol[i])));
    }

    type = eGL_TEXTURE;

    m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                 eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                 (GLint *)&curDepth);
    m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                 eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                 (GLint *)&type);
    if(type == eGL_TEXTURE)
      draw.depthOut = GetResourceManager()->GetOriginalID(
          GetResourceManager()->GetID(TextureRes(GetCtx(), curDepth)));
    else
      draw.depthOut = GetResourceManager()->GetOriginalID(
          GetResourceManager()->GetID(RenderbufferRes(GetCtx(), curDepth)));
  }

  // markers don't increment drawcall ID
  DrawFlags MarkerMask = DrawFlags::SetMarker | DrawFlags::PushMarker | DrawFlags::MultiDraw;
  if(!(draw.flags & MarkerMask))
    m_CurDrawcallID++;

  if(hasEvents)
  {
    draw.events = m_CurEvents;
    m_CurEvents.clear();
  }

  AddUsage(draw);

  // should have at least the root drawcall here, push this drawcall
  // onto the back's children list.
  if(!context->m_DrawcallStack.empty())
  {
    DrawcallTreeNode node(draw);
    node.children.insert(node.children.begin(), draw.children.elems,
                         draw.children.elems + draw.children.count);
    context->m_DrawcallStack.back()->children.push_back(node);
  }
  else
    RDCERR("Somehow lost drawcall stack!");
}

void WrappedOpenGL::AddEvent(string description)
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventID = m_CurEventID;

  apievent.eventDesc = description;

  Callstack::Stackwalk *stack = m_pSerialiser->GetLastCallstack();
  if(stack)
  {
    create_array(apievent.callstack, stack->NumLevels());
    memcpy(apievent.callstack.elems, stack->GetAddrs(), sizeof(uint64_t) * stack->NumLevels());
  }

  m_CurEvents.push_back(apievent);

  if(m_State == READING)
    m_Events.push_back(apievent);
}

APIEvent WrappedOpenGL::GetEvent(uint32_t eventID)
{
  for(size_t i = m_Events.size() - 1; i > 0; i--)
  {
    if(m_Events[i].eventID <= eventID)
      return m_Events[i];
  }

  return m_Events[0];
}

const DrawcallDescription *WrappedOpenGL::GetDrawcall(uint32_t eventID)
{
  if(eventID >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventID];
}

void WrappedOpenGL::ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
  uint64_t offs = m_FrameRecord.frameInfo.fileOffset;

  m_pSerialiser->SetOffset(offs);

  bool partial = true;

  if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
  {
    startEventID = 1;
    partial = false;
  }

  GLChunkType header = (GLChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

  RDCASSERTEQUAL(header, CAPTURE_SCOPE);

  m_pSerialiser->SkipCurrentChunk();

  m_pSerialiser->PopContext(header);

  if(!partial)
  {
    GLMarkerRegion apply("ApplyInitialContents");
    GetResourceManager()->ApplyInitialContents();
    GetResourceManager()->ReleaseInFrameResources();
  }

  if(replayType == eReplay_Full)
  {
    GLMarkerRegion exec(
        StringFormat::Fmt("Replay: Full %u->%u (partial %u)", startEventID, endEventID, partial));
    ContextReplayLog(EXECUTING, startEventID, endEventID, partial);
  }
  else if(replayType == eReplay_WithoutDraw)
  {
    GLMarkerRegion exec(StringFormat::Fmt("Replay: W/O Draw %u->%u (partial %u)", startEventID,
                                          endEventID, partial));
    ContextReplayLog(EXECUTING, startEventID, RDCMAX(1U, endEventID) - 1, partial);
  }
  else if(replayType == eReplay_OnlyDraw)
  {
    GLMarkerRegion exec(StringFormat::Fmt("Replay: Draw Only %u->%u (partial %u)", endEventID,
                                          endEventID, partial));
    ContextReplayLog(EXECUTING, endEventID, endEventID, partial);
  }
  else
  {
    RDCFATAL("Unexpected replay type");
  }
}
