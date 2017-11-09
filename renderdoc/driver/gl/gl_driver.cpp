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
#include "serialise/rdcfile.h"
#include "stb/stb_truetype.h"
#include "strings/string_utils.h"

#define OPENGL 1
#include "data/glsl/debuguniforms.h"

std::map<uint64_t, GLWindowingData> WrappedOpenGL::m_ActiveContexts;

const int firstChar = int(' ') + 1;
const int lastChar = 127;
const int numChars = lastChar - firstChar;
const float charPixelHeight = 20.0f;

stbtt_bakedchar chardata[numChars];

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
  m_GLESExtensions.push_back("GL_EXT_texture_view");
  m_GLESExtensions.push_back("GL_KHR_blend_equation_advanced");
  m_GLESExtensions.push_back("GL_KHR_blend_equation_advanced_coherent");
  m_GLESExtensions.push_back("GL_KHR_context_flush_control");
  m_GLESExtensions.push_back("GL_KHR_debug");
  m_GLESExtensions.push_back("GL_KHR_no_error");
  m_GLESExtensions.push_back("GL_KHR_robust_buffer_access_behavior");
  m_GLESExtensions.push_back("GL_KHR_texture_compression_astc_hdr");
  m_GLESExtensions.push_back("GL_KHR_texture_compression_astc_ldr");
  m_GLESExtensions.push_back("GL_KHR_texture_compression_astc_sliced_3d");
  m_GLESExtensions.push_back("GL_NV_viewport_array");
  m_GLESExtensions.push_back("GL_OES_compressed_ETC1_RGB8_texture");
  m_GLESExtensions.push_back("GL_OES_copy_image");
  m_GLESExtensions.push_back("GL_OES_depth24");
  m_GLESExtensions.push_back("GL_OES_depth32");
  m_GLESExtensions.push_back("GL_OES_depth_texture");
  m_GLESExtensions.push_back("GL_OES_depth_texture_cube_map");
  m_GLESExtensions.push_back("GL_OES_draw_buffers_indexed");
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
  m_GLESExtensions.push_back("GL_OES_texture_cube_map_array");
  m_GLESExtensions.push_back("GL_OES_texture_float");
  m_GLESExtensions.push_back("GL_OES_texture_float_linear");
  m_GLESExtensions.push_back("GL_OES_texture_half_float");
  m_GLESExtensions.push_back("GL_OES_texture_half_float_linear");
  m_GLESExtensions.push_back("GL_OES_texture_npot");
  m_GLESExtensions.push_back("GL_OES_texture_stencil8");
  m_GLESExtensions.push_back("GL_OES_texture_storage_multisample_2d_array");
  m_GLESExtensions.push_back("GL_OES_texture_view");
  m_GLESExtensions.push_back("GL_OES_vertex_array_object");
  m_GLESExtensions.push_back("GL_OES_vertex_half_float");
  m_GLESExtensions.push_back("GL_OES_viewport_array");
  m_GLESExtensions.push_back("GL_OVR_multiview");
  m_GLESExtensions.push_back("GL_OVR_multiview2");
  m_GLESExtensions.push_back("GL_OVR_multiview_multisampled_render_to_texture");

  // advertise EGL extensions in the gl ext string, just in case anyone is checking it for
  // this way.
  m_GLESExtensions.push_back("EGL_KHR_create_context");
  m_GLESExtensions.push_back("EGL_KHR_surfaceless_context");

  // we'll be sorting the implementation extension array, so make sure the
  // sorts are identical so we can do the intersection easily
  std::sort(m_GLESExtensions.begin(), m_GLESExtensions.end());
}

WrappedOpenGL::WrappedOpenGL(const GLHookSet &funcs, GLPlatform &platform)
    : m_Real(funcs),
      m_Platform(platform),
      m_ScratchSerialiser(new StreamWriter(1024), Ownership::Stream)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedOpenGL));

  BuildGLExtensions();
  BuildGLESExtensions();
  // by default we assume OpenGL driver
  SetDriverType(RDC_OpenGL);

  m_Replay.SetDriver(this);

  m_StructuredFile = &m_StoredStructuredData;

  uint32_t flags = 0;

  if(RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  m_ScratchSerialiser.SetChunkMetadataRecording(flags);

  m_SectionVersion = GLInitParams::CurrentVersion;

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
    m_State = CaptureState::LoadingReplaying;

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
    m_State = CaptureState::BackgroundCapturing;
  }

  m_DeviceRecord = NULL;

  m_ResourceManager = new GLResourceManager(this);

  m_ScratchSerialiser.SetUserData(GetResourceManager());

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
}

void WrappedOpenGL::Initialise(GLInitParams &params, uint64_t sectionVersion)
{
  // deliberately want to go through our own wrappers to set up e.g. m_Textures members
  WrappedOpenGL &gl = *this;

  m_InitParams = params;
  m_SectionVersion = sectionVersion;

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

std::string WrappedOpenGL::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((GLChunk)idx);
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

  SAFE_DELETE(m_FrameReader);

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
    if(IsActiveCapturing(m_State))
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

      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::CaptureBegin);
      Serialise_BeginCaptureFrame(ser);
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

      if(IsCaptureMode(m_State))
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
  if(IsBackgroundCapturing(m_State))
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

  if(IsBackgroundCapturing(m_State))
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

  if(IsActiveCapturing(m_State) && m_AppControlledCapture)
    m_BackbufferImages[windowHandle] = SaveBackbufferImage();

  if(!activeWindow)
    return;

  RenderDoc::Inst().SetCurrentDriver(GetDriverType());

  // only allow capturing on 'modern' created contexts
  if(ctxdata.Legacy())
    return;

  // kill any current capture that isn't application defined
  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture(ctxdata.ctx, windowHandle);

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(ctxdata.ctx, windowHandle);

    m_AppControlledCapture = false;
  }
}

void WrappedOpenGL::CreateVRAPITextureSwapChain(GLuint tex, GLenum textureType, GLenum internalformat,
                                                GLsizei width, GLsizei height, GLint levels)
{
  GLResource res = TextureRes(GetCtx(), tex);
  ResourceId id = GetResourceManager()->RegisterResource(res);

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    RDCASSERT(record);

    // this chunk is just a dummy, to indicate that this is where the emulated calls below come from
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);

      record->AddChunk(scope.Get());
    }

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::glGenTextures);
      Serialise_glGenTextures(ser, 1, &tex);

      record->AddChunk(scope.Get());
    }

    gl_CurChunk = GLChunk::glTexParameteri;
    Common_glTextureParameteriEXT(record, textureType, eGL_TEXTURE_MAX_LEVEL, levels);
  }
  else
  {
    GetResourceManager()->AddLiveResource(id, res);
  }

  for(GLint i = 0; i < levels; ++i)
  {
    if(textureType == eGL_TEXTURE_2D_ARRAY)
    {
      gl_CurChunk = GLChunk::glTexImage3D;
      Common_glTextureImage3DEXT(id, eGL_TEXTURE_2D_ARRAY, i, internalformat, width, height, 2, 0,
                                 eGL_RGBA, eGL_UNSIGNED_BYTE, NULL);
    }
    else if(textureType == eGL_TEXTURE_2D)
    {
      gl_CurChunk = GLChunk::glTexImage2D;
      Common_glTextureImage2DEXT(id, eGL_TEXTURE_2D, i, internalformat, width, height, 0, eGL_RGBA,
                                 eGL_UNSIGNED_BYTE, NULL);
    }
    else
    {
      RDCERR("Unexpected textureType (%u) in CreateVRAPITextureSwapChain", textureType);
    }

    width = RDCMAX(1, (width / 2));
    height = RDCMAX(1, (height / 2));
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
  if(!IsBackgroundCapturing(m_State))
    return;

  SCOPED_LOCK(GetGLLock());

  RenderDoc::Inst().SetCurrentDriver(GetDriverType());

  m_State = CaptureState::ActiveCapturing;

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
  if(!IsActiveCapturing(m_State))
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

    RDCFile *rdc = RenderDoc::Inst().CreateRDC(m_FrameCounter, bbim->jpgbuf, bbim->len,
                                               bbim->thwidth, bbim->thheight);

    SAFE_DELETE(bbim);

    for(auto it = m_BackbufferImages.begin(); it != m_BackbufferImages.end(); ++it)
      delete it->second;
    m_BackbufferImages.clear();

    StreamWriter *captureWriter = NULL;

    if(rdc)
    {
      SectionProperties props;

      // Compress with LZ4 so that it's fast
      props.flags = SectionFlags::LZ4Compressed;
      props.version = m_SectionVersion;
      props.type = SectionType::FrameCapture;

      captureWriter = rdc->WriteSection(props);
    }
    else
    {
      captureWriter = new StreamWriter(StreamWriter::InvalidStream);
    }

    {
      WriteSerialiser ser(captureWriter, Ownership::Stream);

      ser.SetUserData(GetResourceManager());

      {
        SCOPED_SERIALISE_CHUNK(SystemChunk::DriverInit, sizeof(GLInitParams) + 16);

        SERIALISE_ELEMENT(m_InitParams);
      }

      {
        // remember to update this estimated chunk length if you add more parameters
        SCOPED_SERIALISE_CHUNK(GLChunk::DeviceInitialisation, 32);

        SERIALISE_ELEMENT(m_FakeVAOID);
      }

      RDCDEBUG("Inserting Resource Serialisers");

      GetResourceManager()->InsertReferencedChunks(ser);

      GetResourceManager()->InsertInitialContentsChunks(ser);

      RDCDEBUG("Creating Capture Scope");

      GetResourceManager()->Serialise_InitialContentsNeeded(ser);

      {
        SCOPED_SERIALISE_CHUNK(GLChunk::CaptureScope, 16);

        Serialise_CaptureScope(ser);
      }

      {
        RDCDEBUG("Getting Resource Record");

        GLResourceRecord *record = m_ResourceManager->GetResourceRecord(m_ContextResourceID);

        RDCDEBUG("Accumulating context resource list");

        map<int32_t, Chunk *> recordlist;
        record->Insert(recordlist);

        RDCDEBUG("Flushing %u records to file serialiser", (uint32_t)recordlist.size());

        for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
          it->second->Write(ser);

        RDCDEBUG("Done");
      }
    }

    RenderDoc::Inst().FinishCaptureWriting(rdc, m_FrameCounter);

    m_State = CaptureState::BackgroundCapturing;

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

      m_State = CaptureState::BackgroundCapturing;

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
  if(m_FrameCounter == 0 && IsBackgroundCapturing(m_State) &&
     RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    // since we haven't associated the window we can't capture by window, so we have to capture just
    // on the device - the very next present to any window on this context will end the capture.
    RenderDoc::Inst().StartFrameCapture(ctx, NULL);

    m_AppControlledCapture = false;
  }
}

WrappedOpenGL::BackbufferImage *WrappedOpenGL::SaveBackbufferImage()
{
  const uint16_t maxSize = 2048;

  byte *thpixels = NULL;
  uint16_t thwidth = 0;
  uint16_t thheight = 0;

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

    thwidth = (uint16_t)m_InitParams.width;
    thheight = (uint16_t)m_InitParams.height;

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
    for(uint16_t y = 0; y <= thheight / 2; y++)
    {
      uint16_t flipY = (thheight - 1 - y);

      for(uint16_t x = 0; x < thwidth; x++)
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
    uint16_t resample_width = RDCMIN(maxSize, thwidth);
    resample_width &= ~3;    // JPEG encoder gives shear distortion if width is not divisible by 4.
    if(thwidth != resample_width)
    {
      float widthf = float(thwidth);
      float heightf = float(thheight);

      float aspect = widthf / heightf;

      // clamp dimensions to a width of resample_width
      thwidth = resample_width;
      thheight = uint16_t(float(thwidth) / aspect);

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

template <typename SerialiserType>
void WrappedOpenGL::Serialise_CaptureScope(SerialiserType &ser)
{
  SERIALISE_ELEMENT(m_FrameCounter);

  if(IsReplayingAndReading())
  {
    m_FrameRecord.frameInfo.frameNumber = m_FrameCounter;
    RDCEraseEl(m_FrameRecord.frameInfo.stats);
  }
}

void WrappedOpenGL::ContextEndFrame()
{
  USE_SCRATCH_SERIALISER();
  ser.SetDrawChunk();
  SCOPED_SERIALISE_CHUNK(GLChunk::CaptureEnd);

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
  m_State = CaptureState::ActiveCapturing;

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_BeginCaptureFrame(SerialiserType &ser)
{
  GLRenderState state(&m_Real);

  if(ser.IsWriting())
  {
    state.FetchState(this);
    state.MarkReferenced(this, true);
  }

  SERIALISE_ELEMENT(state);

  if(IsReplayingAndReading())
  {
    state.ApplyState(this);
  }

  return true;
}

void WrappedOpenGL::BeginCaptureFrame()
{
  USE_SCRATCH_SERIALISER();
  SCOPED_SERIALISE_CHUNK(GLChunk::CaptureBegin);

  Serialise_BeginCaptureFrame(ser);

  m_ContextRecord->AddChunk(scope.Get(), 1);
}

void WrappedOpenGL::FinishCapture()
{
  m_State = CaptureState::BackgroundCapturing;

  m_DebugMessages.clear();

  // m_SuccessfulCapture = false;
}

void WrappedOpenGL::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                    std::string d)
{
  if(IsLoading(m_State) || src == MessageSource::RuntimeWarning)
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

std::vector<DebugMessage> WrappedOpenGL::GetDebugMessages()
{
  std::vector<DebugMessage> ret;
  ret.swap(m_DebugMessages);
  return ret;
}

template <typename SerialiserType>
void WrappedOpenGL::Serialise_DebugMessages(SerialiserType &ser)
{
  std::vector<DebugMessage> DebugMessages;

  if(ser.IsWriting())
  {
    DebugMessages.swap(m_DebugMessages);
  }

  SERIALISE_ELEMENT(DebugMessages);

  // hide empty sets of messages.
  if(ser.IsReading() && DebugMessages.empty())
    ser.Hidden();

  if(ser.IsReading() && IsLoading(m_State))
  {
    for(DebugMessage &msg : DebugMessages)
    {
      msg.eventID = m_CurEventID;
      AddDebugMessage(msg);
    }
  }
}

template void WrappedOpenGL::Serialise_DebugMessages(WriteSerialiser &ser);
template void WrappedOpenGL::Serialise_DebugMessages(ReadSerialiser &ser);

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
  if(type != eGL_DEBUG_TYPE_PUSH_GROUP && type != eGL_DEBUG_TYPE_POP_GROUP &&
     type != eGL_DEBUG_TYPE_MARKER)
  {
    if(type != eGL_DEBUG_TYPE_PERFORMANCE && type != eGL_DEBUG_TYPE_OTHER)
    {
      RDCLOG("Got a Debug message from %s, type %s, ID %d, severity %s:\n'%s'",
             ToStr(source).c_str(), ToStr(type).c_str(), id, ToStr(severity).c_str(), message);
      if(m_DebugMsgContext != "")
        RDCLOG("Debug Message context: \"%s\"", m_DebugMsgContext.c_str());
    }

    if(IsActiveCapturing(m_State))
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

void WrappedOpenGL::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  StreamReader *reader = rdc->ReadSection(sectionIdx);

  if(reader->IsErrored())
    return;

  ReadSerialiser ser(reader, Ownership::Stream);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers);

  m_StructuredFile = &ser.GetStructuredFile();

  m_StoredStructuredData.version = m_StructuredFile->version = m_SectionVersion;

  int chunkIdx = 0;

  struct chunkinfo
  {
    chunkinfo() : count(0), totalsize(0), total(0.0) {}
    int count;
    uint64_t totalsize;
    double total;
  };

  std::map<GLChunk, chunkinfo> chunkInfos;

  SCOPED_TIMER("chunk initialisation");

  uint64_t frameDataSize = 0;

  for(;;)
  {
    PerformanceTimer timer;

    uint64_t offsetStart = reader->GetOffset();

    GLChunk context = ser.ReadChunk<GLChunk>();

    chunkIdx++;

    ProcessChunk(ser, context);

    ser.EndChunk();

    uint64_t offsetEnd = reader->GetOffset();

    RenderDoc::Inst().SetProgress(FileInitialRead, float(offsetEnd) / float(reader->GetSize()));

    if(context == GLChunk::CaptureScope)
    {
      m_FrameRecord.frameInfo.fileOffset = offsetStart;

      // read the remaining data into memory and pass to immediate context
      frameDataSize = reader->GetSize() - reader->GetOffset();

      m_FrameReader = new StreamReader(reader, frameDataSize);

      GetResourceManager()->ApplyInitialContents();

      ContextReplayLog(m_State, 0, 0, false);
    }

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offsetEnd - offsetStart;
    chunkInfos[context].count++;

    if(context == GLChunk::CaptureScope || reader->IsErrored() || reader->AtEnd())
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
        double(it->second.totalsize) / (dcount * 1024.0 * 1024.0),
        GetChunkName((uint32_t)it->first).c_str(), uint32_t(it->first));
  }
#endif

  // steal the structured data for ourselves
  m_StructuredFile->swap(m_StoredStructuredData);

  // and in future use this file.
  m_StructuredFile = &m_StoredStructuredData;

  m_FrameRecord.frameInfo.uncompressedFileSize =
      rdc->GetSectionProperties(sectionIdx).uncompressedSize;
  m_FrameRecord.frameInfo.compressedFileSize = rdc->GetSectionProperties(sectionIdx).compressedSize;
  m_FrameRecord.frameInfo.persistentSize = frameDataSize;
  m_FrameRecord.frameInfo.initDataSize = chunkInfos[(GLChunk)SystemChunk::InitialContents].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           m_FrameRecord.frameInfo.persistentSize);
}

void WrappedOpenGL::ProcessChunk(ReadSerialiser &ser, GLChunk chunk)
{
  gl_CurChunk = chunk;

  // there are unfortunately too many special cases with serialisation to be able to re-use the hook
  // definition macros here. Aliases forward to their 'real' functions, but we also share
  // serialisation between EXT_dsa, ARB_dsa and non-dsa functions. Likewise for the horrible
  // glUniform variants where there are loads of functions that serialise the same way with slight
  // type differences.

  // we handle this here as we don't want a default: in the switch() below - that means we get a
  // warning if any GL chunk is missed.
  {
    SystemChunk system = (SystemChunk)chunk;
    if(system == SystemChunk::DriverInit)
    {
      GLInitParams InitParams;
      SERIALISE_ELEMENT(InitParams);
    }
    else if(system == SystemChunk::InitialContentsList)
    {
      GetResourceManager()->CreateInitialContents(ser);
    }
    else if(system == SystemChunk::InitialContents)
    {
      GetResourceManager()->Serialise_InitialState(ser, ResourceId(), GLResource(MakeNullResource));
    }
    else if(system < SystemChunk::FirstDriverChunk)
    {
      RDCERR("Unexpected system chunk in capture data: %u", system);
      ser.SkipCurrentChunk();
    }
  }

  switch(chunk)
  {
    case GLChunk::DeviceInitialisation:
    {
      SERIALISE_ELEMENT(m_FakeVAOID).Named("VAO 0 ID");

      GetResourceManager()->AddLiveResource(m_FakeVAOID, VertexArrayRes(NULL, 0));
      break;
    }

    case GLChunk::glGenBuffersARB:
    case GLChunk::glGenBuffers: Serialise_glGenBuffers(ser, 0, 0); break;
    case GLChunk::glCreateBuffers: Serialise_glCreateBuffers(ser, 0, 0); break;

    case GLChunk::glBufferStorage:
    case GLChunk::glNamedBufferStorage:
    case GLChunk::glNamedBufferStorageEXT:
      Serialise_glNamedBufferStorageEXT(ser, 0, 0, 0, 0);
      break;
    case GLChunk::glBufferData:
    case GLChunk::glBufferDataARB:
    case GLChunk::glNamedBufferData:
    case GLChunk::glNamedBufferDataEXT:
      Serialise_glNamedBufferDataEXT(ser, 0, 0, 0, eGL_NONE);
      break;
    case GLChunk::glBufferSubData:
    case GLChunk::glBufferSubDataARB:
    case GLChunk::glNamedBufferSubData:
    case GLChunk::glNamedBufferSubDataEXT:
      Serialise_glNamedBufferSubDataEXT(ser, 0, 0, 0, 0);
      break;
    case GLChunk::glCopyBufferSubData:
    case GLChunk::glCopyNamedBufferSubData:
    case GLChunk::glNamedCopyBufferSubDataEXT:
      Serialise_glNamedCopyBufferSubDataEXT(ser, 0, 0, 0, 0, 0);
      break;

    case GLChunk::glBindBufferARB:
    case GLChunk::glBindBuffer: Serialise_glBindBuffer(ser, eGL_NONE, 0); break;
    case GLChunk::glBindBufferBaseEXT:
    case GLChunk::glBindBufferBase: Serialise_glBindBufferBase(ser, eGL_NONE, 0, 0); break;
    case GLChunk::glBindBufferRangeEXT:
    case GLChunk::glBindBufferRange: Serialise_glBindBufferRange(ser, eGL_NONE, 0, 0, 0, 0); break;
    case GLChunk::glBindBuffersBase: Serialise_glBindBuffersBase(ser, eGL_NONE, 0, 0, 0); break;
    case GLChunk::glBindBuffersRange:
      Serialise_glBindBuffersRange(ser, eGL_NONE, 0, 0, 0, 0, 0);
      break;

    case GLChunk::glUnmapBuffer:
    case GLChunk::glUnmapBufferARB:
    case GLChunk::glUnmapBufferOES:
    case GLChunk::glUnmapNamedBuffer:
    case GLChunk::glUnmapNamedBufferEXT: Serialise_glUnmapNamedBufferEXT(ser, 0); break;
    case GLChunk::glFlushMappedBufferRange:
    case GLChunk::glFlushMappedNamedBufferRange:
    case GLChunk::glFlushMappedNamedBufferRangeEXT:
      Serialise_glFlushMappedNamedBufferRangeEXT(ser, 0, 0, 0);
      break;

    case GLChunk::glGenTransformFeedbacks: Serialise_glGenTransformFeedbacks(ser, 0, 0); break;
    case GLChunk::glCreateTransformFeedbacks:
      Serialise_glCreateTransformFeedbacks(ser, 0, 0);
      break;
    case GLChunk::glTransformFeedbackBufferBase:
      Serialise_glTransformFeedbackBufferBase(ser, 0, 0, 0);
      break;
    case GLChunk::glTransformFeedbackBufferRange:
      Serialise_glTransformFeedbackBufferRange(ser, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glBindTransformFeedback:
      Serialise_glBindTransformFeedback(ser, eGL_NONE, 0);
      break;
    case GLChunk::glBeginTransformFeedbackEXT:
    case GLChunk::glBeginTransformFeedback:
      Serialise_glBeginTransformFeedback(ser, eGL_NONE);
      break;
    case GLChunk::glPauseTransformFeedback: Serialise_glPauseTransformFeedback(ser); break;
    case GLChunk::glResumeTransformFeedback: Serialise_glResumeTransformFeedback(ser); break;
    case GLChunk::glEndTransformFeedbackEXT:
    case GLChunk::glEndTransformFeedback: Serialise_glEndTransformFeedback(ser); break;

    case GLChunk::glVertexAttribPointer:
    case GLChunk::glVertexAttribPointerARB:
    case GLChunk::glVertexArrayVertexAttribOffsetEXT:
      Serialise_glVertexArrayVertexAttribOffsetEXT(ser, 0, 0, 0, 0, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glVertexAttribIPointer:
    case GLChunk::glVertexAttribIPointerEXT:
    case GLChunk::glVertexArrayVertexAttribIOffsetEXT:
      Serialise_glVertexArrayVertexAttribIOffsetEXT(ser, 0, 0, 0, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glVertexAttribLPointer:
    case GLChunk::glVertexAttribLPointerEXT:
    case GLChunk::glVertexArrayVertexAttribLOffsetEXT:
      Serialise_glVertexArrayVertexAttribLOffsetEXT(ser, 0, 0, 0, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glVertexAttribBinding:
    case GLChunk::glVertexArrayAttribBinding:
    case GLChunk::glVertexArrayVertexAttribBindingEXT:
      Serialise_glVertexArrayVertexAttribBindingEXT(ser, 0, 0, 0);
      break;
    case GLChunk::glVertexAttribFormat:
    case GLChunk::glVertexArrayAttribFormat:
    case GLChunk::glVertexArrayVertexAttribFormatEXT:
      Serialise_glVertexArrayVertexAttribFormatEXT(ser, 0, 0, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glVertexAttribIFormat:
    case GLChunk::glVertexArrayAttribIFormat:
    case GLChunk::glVertexArrayVertexAttribIFormatEXT:
      Serialise_glVertexArrayVertexAttribIFormatEXT(ser, 0, 0, 0, eGL_NONE, 0);
      break;
    case GLChunk::glVertexAttribLFormat:
    case GLChunk::glVertexArrayAttribLFormat:
    case GLChunk::glVertexArrayVertexAttribLFormatEXT:
      Serialise_glVertexArrayVertexAttribLFormatEXT(ser, 0, 0, 0, eGL_NONE, 0);
      break;
    case GLChunk::glVertexAttribDivisor:
    case GLChunk::glVertexAttribDivisorARB:
    case GLChunk::glVertexArrayVertexAttribDivisorEXT:
      Serialise_glVertexArrayVertexAttribDivisorEXT(ser, 0, 0, 0);
      break;
    case GLChunk::glEnableVertexAttribArray:
    case GLChunk::glEnableVertexAttribArrayARB:
    case GLChunk::glEnableVertexArrayAttrib:
    case GLChunk::glEnableVertexArrayAttribEXT:
      Serialise_glEnableVertexArrayAttribEXT(ser, 0, 0);
      break;
    case GLChunk::glDisableVertexAttribArray:
    case GLChunk::glDisableVertexAttribArrayARB:
    case GLChunk::glDisableVertexArrayAttrib:
    case GLChunk::glDisableVertexArrayAttribEXT:
      Serialise_glDisableVertexArrayAttribEXT(ser, 0, 0);
      break;
    case GLChunk::glGenVertexArraysOES:
    case GLChunk::glGenVertexArrays: Serialise_glGenVertexArrays(ser, 0, 0); break;
    case GLChunk::glCreateVertexArrays: Serialise_glCreateVertexArrays(ser, 0, 0); break;
    case GLChunk::glBindVertexArrayOES:
    case GLChunk::glBindVertexArray: Serialise_glBindVertexArray(ser, 0); break;
    case GLChunk::glVertexArrayElementBuffer:
      Serialise_glVertexArrayElementBuffer(ser, 0, 0);
      break;
    case GLChunk::glBindVertexBuffer:
    case GLChunk::glVertexArrayVertexBuffer:
    case GLChunk::glVertexArrayBindVertexBufferEXT:
      Serialise_glVertexArrayBindVertexBufferEXT(ser, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glBindVertexBuffers:
    case GLChunk::glVertexArrayVertexBuffers:

      Serialise_glVertexArrayVertexBuffers(ser, 0, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glVertexBindingDivisor:
    case GLChunk::glVertexArrayBindingDivisor:
    case GLChunk::glVertexArrayVertexBindingDivisorEXT:
      Serialise_glVertexArrayVertexBindingDivisorEXT(ser, 0, 0, 0);
      break;

    case GLChunk::glVertexAttrib1d:
    case GLChunk::glVertexAttrib1dARB:
    case GLChunk::glVertexAttrib1dv:
    case GLChunk::glVertexAttrib1dvARB:
    case GLChunk::glVertexAttrib1f:
    case GLChunk::glVertexAttrib1fARB:
    case GLChunk::glVertexAttrib1fv:
    case GLChunk::glVertexAttrib1fvARB:
    case GLChunk::glVertexAttrib1s:
    case GLChunk::glVertexAttrib1sARB:
    case GLChunk::glVertexAttrib1sv:
    case GLChunk::glVertexAttrib1svARB:
    case GLChunk::glVertexAttrib2d:
    case GLChunk::glVertexAttrib2dARB:
    case GLChunk::glVertexAttrib2dv:
    case GLChunk::glVertexAttrib2dvARB:
    case GLChunk::glVertexAttrib2f:
    case GLChunk::glVertexAttrib2fARB:
    case GLChunk::glVertexAttrib2fv:
    case GLChunk::glVertexAttrib2fvARB:
    case GLChunk::glVertexAttrib2s:
    case GLChunk::glVertexAttrib2sARB:
    case GLChunk::glVertexAttrib2sv:
    case GLChunk::glVertexAttrib2svARB:
    case GLChunk::glVertexAttrib3d:
    case GLChunk::glVertexAttrib3dARB:
    case GLChunk::glVertexAttrib3dv:
    case GLChunk::glVertexAttrib3dvARB:
    case GLChunk::glVertexAttrib3f:
    case GLChunk::glVertexAttrib3fARB:
    case GLChunk::glVertexAttrib3fv:
    case GLChunk::glVertexAttrib3fvARB:
    case GLChunk::glVertexAttrib3s:
    case GLChunk::glVertexAttrib3sARB:
    case GLChunk::glVertexAttrib3sv:
    case GLChunk::glVertexAttrib3svARB:
    case GLChunk::glVertexAttrib4bv:
    case GLChunk::glVertexAttrib4bvARB:
    case GLChunk::glVertexAttrib4d:
    case GLChunk::glVertexAttrib4dARB:
    case GLChunk::glVertexAttrib4dv:
    case GLChunk::glVertexAttrib4dvARB:
    case GLChunk::glVertexAttrib4f:
    case GLChunk::glVertexAttrib4fARB:
    case GLChunk::glVertexAttrib4fv:
    case GLChunk::glVertexAttrib4fvARB:
    case GLChunk::glVertexAttrib4iv:
    case GLChunk::glVertexAttrib4ivARB:
    case GLChunk::glVertexAttrib4Nbv:
    case GLChunk::glVertexAttrib4NbvARB:
    case GLChunk::glVertexAttrib4Niv:
    case GLChunk::glVertexAttrib4NivARB:
    case GLChunk::glVertexAttrib4Nsv:
    case GLChunk::glVertexAttrib4NsvARB:
    case GLChunk::glVertexAttrib4Nub:
    case GLChunk::glVertexAttrib4Nubv:
    case GLChunk::glVertexAttrib4NubvARB:
    case GLChunk::glVertexAttrib4Nuiv:
    case GLChunk::glVertexAttrib4NuivARB:
    case GLChunk::glVertexAttrib4Nusv:
    case GLChunk::glVertexAttrib4NusvARB:
    case GLChunk::glVertexAttrib4s:
    case GLChunk::glVertexAttrib4sARB:
    case GLChunk::glVertexAttrib4sv:
    case GLChunk::glVertexAttrib4svARB:
    case GLChunk::glVertexAttrib4ubv:
    case GLChunk::glVertexAttrib4ubvARB:
    case GLChunk::glVertexAttrib4uiv:
    case GLChunk::glVertexAttrib4uivARB:
    case GLChunk::glVertexAttrib4usv:
    case GLChunk::glVertexAttrib4usvARB:
    case GLChunk::glVertexAttribI1i:
    case GLChunk::glVertexAttribI1iEXT:
    case GLChunk::glVertexAttribI1iv:
    case GLChunk::glVertexAttribI1ivEXT:
    case GLChunk::glVertexAttribI1ui:
    case GLChunk::glVertexAttribI1uiEXT:
    case GLChunk::glVertexAttribI1uiv:
    case GLChunk::glVertexAttribI1uivEXT:
    case GLChunk::glVertexAttribI2i:
    case GLChunk::glVertexAttribI2iEXT:
    case GLChunk::glVertexAttribI2iv:
    case GLChunk::glVertexAttribI2ivEXT:
    case GLChunk::glVertexAttribI2ui:
    case GLChunk::glVertexAttribI2uiEXT:
    case GLChunk::glVertexAttribI2uiv:
    case GLChunk::glVertexAttribI2uivEXT:
    case GLChunk::glVertexAttribI3i:
    case GLChunk::glVertexAttribI3iEXT:
    case GLChunk::glVertexAttribI3iv:
    case GLChunk::glVertexAttribI3ivEXT:
    case GLChunk::glVertexAttribI3ui:
    case GLChunk::glVertexAttribI3uiEXT:
    case GLChunk::glVertexAttribI3uiv:
    case GLChunk::glVertexAttribI3uivEXT:
    case GLChunk::glVertexAttribI4bv:
    case GLChunk::glVertexAttribI4bvEXT:
    case GLChunk::glVertexAttribI4i:
    case GLChunk::glVertexAttribI4iEXT:
    case GLChunk::glVertexAttribI4iv:
    case GLChunk::glVertexAttribI4ivEXT:
    case GLChunk::glVertexAttribI4sv:
    case GLChunk::glVertexAttribI4svEXT:
    case GLChunk::glVertexAttribI4ubv:
    case GLChunk::glVertexAttribI4ubvEXT:
    case GLChunk::glVertexAttribI4ui:
    case GLChunk::glVertexAttribI4uiEXT:
    case GLChunk::glVertexAttribI4uiv:
    case GLChunk::glVertexAttribI4uivEXT:
    case GLChunk::glVertexAttribI4usv:
    case GLChunk::glVertexAttribI4usvEXT:
    case GLChunk::glVertexAttribL1d:
    case GLChunk::glVertexAttribL1dEXT:
    case GLChunk::glVertexAttribL1dv:
    case GLChunk::glVertexAttribL1dvEXT:
    case GLChunk::glVertexAttribL2d:
    case GLChunk::glVertexAttribL2dEXT:
    case GLChunk::glVertexAttribL2dv:
    case GLChunk::glVertexAttribL2dvEXT:
    case GLChunk::glVertexAttribL3d:
    case GLChunk::glVertexAttribL3dEXT:
    case GLChunk::glVertexAttribL3dv:
    case GLChunk::glVertexAttribL3dvEXT:
    case GLChunk::glVertexAttribL4d:
    case GLChunk::glVertexAttribL4dEXT:
    case GLChunk::glVertexAttribL4dv:
    case GLChunk::glVertexAttribL4dvEXT:
    case GLChunk::glVertexAttribP1ui:
    case GLChunk::glVertexAttribP1uiv:
    case GLChunk::glVertexAttribP2ui:
    case GLChunk::glVertexAttribP2uiv:
    case GLChunk::glVertexAttribP3ui:
    case GLChunk::glVertexAttribP3uiv:
    case GLChunk::glVertexAttribP4ui:
    case GLChunk::glVertexAttribP4uiv:
      Serialise_glVertexAttrib(ser, 0, 0, eGL_NONE, 0, 0, Attrib_typemask);
      break;

    case GLChunk::glLabelObjectEXT:
    case GLChunk::glObjectLabelKHR:
    case GLChunk::glObjectPtrLabel:
    case GLChunk::glObjectPtrLabelKHR:
    case GLChunk::glObjectLabel: Serialise_glObjectLabel(ser, eGL_NONE, 0, 0, 0); break;
    case GLChunk::glDebugMessageInsertARB:
    case GLChunk::glDebugMessageInsertKHR:
    case GLChunk::glDebugMessageInsert:
      Serialise_glDebugMessageInsert(ser, eGL_NONE, eGL_NONE, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glStringMarkerGREMEDY:
    case GLChunk::glInsertEventMarkerEXT: Serialise_glInsertEventMarkerEXT(ser, 0, 0); break;
    case GLChunk::glPushGroupMarkerEXT:
    case GLChunk::glPushDebugGroupKHR:
    case GLChunk::glPushDebugGroup: Serialise_glPushDebugGroup(ser, eGL_NONE, 0, 0, 0); break;
    case GLChunk::glPopGroupMarkerEXT:
    case GLChunk::glPopDebugGroupKHR:
    case GLChunk::glPopDebugGroup: Serialise_glPopDebugGroup(ser); break;

    case GLChunk::glDispatchCompute: Serialise_glDispatchCompute(ser, 0, 0, 0); break;
    case GLChunk::glDispatchComputeGroupSizeARB:
      Serialise_glDispatchComputeGroupSizeARB(ser, 0, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glDispatchComputeIndirect: Serialise_glDispatchComputeIndirect(ser, 0); break;
    case GLChunk::glMemoryBarrierEXT:
    case GLChunk::glMemoryBarrier: Serialise_glMemoryBarrier(ser, 0); break;
    case GLChunk::glMemoryBarrierByRegion: Serialise_glMemoryBarrierByRegion(ser, 0); break;
    case GLChunk::glTextureBarrier: Serialise_glTextureBarrier(ser); break;
    case GLChunk::glDrawTransformFeedback:
      Serialise_glDrawTransformFeedback(ser, eGL_NONE, 0);
      break;
    case GLChunk::glDrawTransformFeedbackInstanced:
      Serialise_glDrawTransformFeedbackInstanced(ser, eGL_NONE, 0, 0);
      break;
    case GLChunk::glDrawTransformFeedbackStream:
      Serialise_glDrawTransformFeedbackStream(ser, eGL_NONE, 0, 0);
      break;
    case GLChunk::glDrawTransformFeedbackStreamInstanced:
      Serialise_glDrawTransformFeedbackStreamInstanced(ser, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glDrawArrays: Serialise_glDrawArrays(ser, eGL_NONE, 0, 0); break;
    case GLChunk::glDrawArraysIndirect: Serialise_glDrawArraysIndirect(ser, eGL_NONE, 0); break;
    case GLChunk::glDrawArraysInstancedARB:
    case GLChunk::glDrawArraysInstancedEXT:
    case GLChunk::glDrawArraysInstanced:
      Serialise_glDrawArraysInstanced(ser, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glDrawArraysInstancedBaseInstanceEXT:
    case GLChunk::glDrawArraysInstancedBaseInstance:
      Serialise_glDrawArraysInstancedBaseInstance(ser, eGL_NONE, 0, 0, 0, 0);
      break;
    case GLChunk::glDrawElements: Serialise_glDrawElements(ser, eGL_NONE, 0, eGL_NONE, 0); break;
    case GLChunk::glDrawElementsIndirect:
      Serialise_glDrawElementsIndirect(ser, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glDrawRangeElementsEXT:
    case GLChunk::glDrawRangeElements:
      Serialise_glDrawRangeElements(ser, eGL_NONE, 0, 0, 0, eGL_NONE, 0);
      break;
    case GLChunk::glDrawRangeElementsBaseVertexEXT:
    case GLChunk::glDrawRangeElementsBaseVertexOES:
    case GLChunk::glDrawRangeElementsBaseVertex:
      Serialise_glDrawRangeElementsBaseVertex(ser, eGL_NONE, 0, 0, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glDrawElementsBaseVertexEXT:
    case GLChunk::glDrawElementsBaseVertexOES:
    case GLChunk::glDrawElementsBaseVertex:
      Serialise_glDrawElementsBaseVertex(ser, eGL_NONE, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glDrawElementsInstancedARB:
    case GLChunk::glDrawElementsInstancedEXT:
    case GLChunk::glDrawElementsInstanced:
      Serialise_glDrawElementsInstanced(ser, eGL_NONE, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glDrawElementsInstancedBaseInstanceEXT:
    case GLChunk::glDrawElementsInstancedBaseInstance:
      Serialise_glDrawElementsInstancedBaseInstance(ser, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glDrawElementsInstancedBaseVertexEXT:
    case GLChunk::glDrawElementsInstancedBaseVertexOES:
    case GLChunk::glDrawElementsInstancedBaseVertex:
      Serialise_glDrawElementsInstancedBaseVertex(ser, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glDrawElementsInstancedBaseVertexBaseInstanceEXT:
    case GLChunk::glDrawElementsInstancedBaseVertexBaseInstance:
      Serialise_glDrawElementsInstancedBaseVertexBaseInstance(ser, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
      break;
    case GLChunk::glMultiDrawArraysEXT:
    case GLChunk::glMultiDrawArrays: Serialise_glMultiDrawArrays(ser, eGL_NONE, 0, 0, 0); break;
    case GLChunk::glMultiDrawElements:
      Serialise_glMultiDrawElements(ser, eGL_NONE, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glMultiDrawElementsBaseVertexEXT:
    case GLChunk::glMultiDrawElementsBaseVertexOES:
    case GLChunk::glMultiDrawElementsBaseVertex:
      Serialise_glMultiDrawElementsBaseVertex(ser, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glMultiDrawArraysIndirect:
      Serialise_glMultiDrawArraysIndirect(ser, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glMultiDrawElementsIndirect:
      Serialise_glMultiDrawElementsIndirect(ser, eGL_NONE, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glMultiDrawArraysIndirectCountARB:
      Serialise_glMultiDrawArraysIndirectCountARB(ser, eGL_NONE, 0, 0, 0, 0);
      break;
    case GLChunk::glMultiDrawElementsIndirectCountARB:
      Serialise_glMultiDrawElementsIndirectCountARB(ser, eGL_NONE, eGL_NONE, 0, 0, 0, 0);
      break;
    case GLChunk::glClearBufferfv:
    case GLChunk::glClearNamedFramebufferfv:
      Serialise_glClearNamedFramebufferfv(ser, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glClearBufferiv:
    case GLChunk::glClearNamedFramebufferiv:
      Serialise_glClearNamedFramebufferiv(ser, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glClearBufferuiv:
    case GLChunk::glClearNamedFramebufferuiv:
      Serialise_glClearNamedFramebufferuiv(ser, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glClearBufferfi:
    case GLChunk::glClearNamedFramebufferfi:
      Serialise_glClearNamedFramebufferfi(ser, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glClearBufferData:
    case GLChunk::glClearNamedBufferData:
    case GLChunk::glClearNamedBufferDataEXT:
      Serialise_glClearNamedBufferDataEXT(ser, 0, eGL_NONE, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glClearBufferSubData:
    case GLChunk::glClearNamedBufferSubData:
    case GLChunk::glClearNamedBufferSubDataEXT:
      Serialise_glClearNamedBufferSubDataEXT(ser, 0, eGL_NONE, 0, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glClear: Serialise_glClear(ser, 0); break;
    case GLChunk::glClearTexImage:
      Serialise_glClearTexImage(ser, 0, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glClearTexSubImage:
      Serialise_glClearTexSubImage(ser, 0, 0, 0, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
      break;

    case GLChunk::glGenFramebuffersEXT:
    case GLChunk::glGenFramebuffers: Serialise_glGenFramebuffers(ser, 0, 0); break;
    case GLChunk::glCreateFramebuffers: Serialise_glCreateFramebuffers(ser, 0, 0); break;
    case GLChunk::glFramebufferTexture:
    case GLChunk::glFramebufferTextureOES:
    case GLChunk::glFramebufferTextureARB:
    case GLChunk::glFramebufferTextureEXT:
    case GLChunk::glNamedFramebufferTexture:
    case GLChunk::glNamedFramebufferTextureEXT:
      Serialise_glNamedFramebufferTextureEXT(ser, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glFramebufferTexture1D:
    case GLChunk::glFramebufferTexture1DEXT:
    case GLChunk::glNamedFramebufferTexture1DEXT:
      Serialise_glNamedFramebufferTexture1DEXT(ser, 0, eGL_NONE, eGL_NONE, 0, 0);
      break;
    case GLChunk::glFramebufferTexture2D:
    case GLChunk::glFramebufferTexture2DEXT:
    case GLChunk::glNamedFramebufferTexture2DEXT:
      Serialise_glNamedFramebufferTexture2DEXT(ser, 0, eGL_NONE, eGL_NONE, 0, 0);
      break;
    case GLChunk::glFramebufferTexture2DMultisampleEXT:
      Serialise_glFramebufferTexture2DMultisampleEXT(ser, 0, eGL_NONE, eGL_NONE, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glFramebufferTexture3D:
    case GLChunk::glFramebufferTexture3DEXT:
    case GLChunk::glFramebufferTexture3DOES:
    case GLChunk::glNamedFramebufferTexture3DEXT:
      Serialise_glNamedFramebufferTexture3DEXT(ser, 0, eGL_NONE, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glFramebufferRenderbuffer:
    case GLChunk::glFramebufferRenderbufferEXT:
    case GLChunk::glNamedFramebufferRenderbuffer:
    case GLChunk::glNamedFramebufferRenderbufferEXT:
      Serialise_glNamedFramebufferRenderbufferEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glFramebufferTextureLayer:
    case GLChunk::glFramebufferTextureLayerARB:
    case GLChunk::glFramebufferTextureLayerEXT:
    case GLChunk::glNamedFramebufferTextureLayer:
    case GLChunk::glNamedFramebufferTextureLayerEXT:
      Serialise_glNamedFramebufferTextureLayerEXT(ser, 0, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glFramebufferTextureMultiviewOVR:
      Serialise_glFramebufferTextureMultiviewOVR(ser, eGL_NONE, eGL_NONE, 0, 0, 0, 0);
      break;
    case GLChunk::glFramebufferTextureMultisampleMultiviewOVR:
      Serialise_glFramebufferTextureMultisampleMultiviewOVR(ser, eGL_NONE, eGL_NONE, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glFramebufferParameteri:
    case GLChunk::glNamedFramebufferParameteri:
    case GLChunk::glNamedFramebufferParameteriEXT:
      Serialise_glNamedFramebufferParameteriEXT(ser, 0, eGL_NONE, 0);
      break;
    case GLChunk::glReadBuffer:
    case GLChunk::glNamedFramebufferReadBuffer:
    case GLChunk::glFramebufferReadBufferEXT:
      Serialise_glFramebufferReadBufferEXT(ser, 0, eGL_NONE);
      break;
    case GLChunk::glBindFramebufferEXT:
    case GLChunk::glBindFramebuffer: Serialise_glBindFramebuffer(ser, eGL_NONE, 0); break;
    case GLChunk::glDrawBuffer:
    case GLChunk::glNamedFramebufferDrawBuffer:
    case GLChunk::glFramebufferDrawBufferEXT:
      Serialise_glFramebufferDrawBufferEXT(ser, 0, eGL_NONE);
      break;
    case GLChunk::glDrawBuffers:
    case GLChunk::glDrawBuffersARB:
    case GLChunk::glDrawBuffersEXT:
    case GLChunk::glNamedFramebufferDrawBuffers:
    case GLChunk::glFramebufferDrawBuffersEXT:
      Serialise_glFramebufferDrawBuffersEXT(ser, 0, 0, 0);
      break;
    case GLChunk::glBlitFramebuffer:
    case GLChunk::glBlitFramebufferEXT:
    case GLChunk::glBlitNamedFramebuffer:
      Serialise_glBlitNamedFramebuffer(ser, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, eGL_NONE);
      break;
    case GLChunk::glGenRenderbuffersEXT:
    case GLChunk::glGenRenderbuffers: Serialise_glGenRenderbuffers(ser, 0, 0); break;
    case GLChunk::glCreateRenderbuffers: Serialise_glCreateRenderbuffers(ser, 0, 0); break;
    case GLChunk::glRenderbufferStorage:
    case GLChunk::glRenderbufferStorageEXT:
    case GLChunk::glNamedRenderbufferStorage:
    case GLChunk::glNamedRenderbufferStorageEXT:
      Serialise_glNamedRenderbufferStorageEXT(ser, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glRenderbufferStorageMultisample:
    case GLChunk::glRenderbufferStorageMultisampleEXT:
    case GLChunk::glNamedRenderbufferStorageMultisample:
    case GLChunk::glNamedRenderbufferStorageMultisampleEXT:
      Serialise_glNamedRenderbufferStorageMultisampleEXT(ser, 0, 0, eGL_NONE, 0, 0);
      break;

    case GLChunk::wglDXRegisterObjectNV:
      Serialise_wglDXRegisterObjectNV(ser, GLResource(MakeNullResource), eGL_NONE, 0);
      break;
    case GLChunk::wglDXLockObjectsNV:
      Serialise_wglDXLockObjectsNV(ser, GLResource(MakeNullResource));
      break;

    case GLChunk::glFenceSync: Serialise_glFenceSync(ser, 0, eGL_NONE, 0); break;
    case GLChunk::glClientWaitSync: Serialise_glClientWaitSync(ser, 0, 0, 0); break;
    case GLChunk::glWaitSync: Serialise_glWaitSync(ser, 0, 0, 0); break;
    case GLChunk::glGenQueriesARB:
    case GLChunk::glGenQueriesEXT:
    case GLChunk::glGenQueries: Serialise_glGenQueries(ser, 0, 0); break;
    case GLChunk::glCreateQueries: Serialise_glCreateQueries(ser, eGL_NONE, 0, 0); break;
    case GLChunk::glBeginQueryARB:
    case GLChunk::glBeginQueryEXT:
    case GLChunk::glBeginQuery: Serialise_glBeginQuery(ser, eGL_NONE, 0); break;
    case GLChunk::glBeginQueryIndexed: Serialise_glBeginQueryIndexed(ser, eGL_NONE, 0, 0); break;
    case GLChunk::glEndQueryARB:
    case GLChunk::glEndQueryEXT:
    case GLChunk::glEndQuery: Serialise_glEndQuery(ser, eGL_NONE); break;
    case GLChunk::glEndQueryIndexed: Serialise_glEndQueryIndexed(ser, eGL_NONE, 0); break;
    case GLChunk::glBeginConditionalRender:
      Serialise_glBeginConditionalRender(ser, 0, eGL_NONE);
      break;
    case GLChunk::glEndConditionalRender: Serialise_glEndConditionalRender(ser); break;
    case GLChunk::glQueryCounterEXT:
    case GLChunk::glQueryCounter: Serialise_glQueryCounter(ser, 0, eGL_NONE); break;

    case GLChunk::glGenSamplers: Serialise_glGenSamplers(ser, 0, 0); break;
    case GLChunk::glCreateSamplers: Serialise_glCreateSamplers(ser, 0, 0); break;
    case GLChunk::glBindSampler: Serialise_glBindSampler(ser, 0, 0); break;
    case GLChunk::glBindSamplers: Serialise_glBindSamplers(ser, 0, 0, 0); break;
    case GLChunk::glSamplerParameteri: Serialise_glSamplerParameteri(ser, 0, eGL_NONE, 0); break;
    case GLChunk::glSamplerParameterf: Serialise_glSamplerParameterf(ser, 0, eGL_NONE, 0); break;
    case GLChunk::glSamplerParameteriv: Serialise_glSamplerParameteriv(ser, 0, eGL_NONE, 0); break;
    case GLChunk::glSamplerParameterfv: Serialise_glSamplerParameterfv(ser, 0, eGL_NONE, 0); break;
    case GLChunk::glSamplerParameterIivEXT:
    case GLChunk::glSamplerParameterIivOES:
    case GLChunk::glSamplerParameterIiv:
      Serialise_glSamplerParameterIiv(ser, 0, eGL_NONE, 0);
      break;
    case GLChunk::glSamplerParameterIuivEXT:
    case GLChunk::glSamplerParameterIuivOES:
    case GLChunk::glSamplerParameterIuiv:
      Serialise_glSamplerParameterIuiv(ser, 0, eGL_NONE, 0);
      break;

    case GLChunk::glCreateShader: Serialise_glCreateShader(ser, 0, eGL_NONE); break;
    case GLChunk::glShaderSource: Serialise_glShaderSource(ser, 0, 0, 0, 0); break;
    case GLChunk::glCompileShader: Serialise_glCompileShader(ser, 0); break;
    case GLChunk::glAttachShader: Serialise_glAttachShader(ser, 0, 0); break;
    case GLChunk::glDetachShader: Serialise_glDetachShader(ser, 0, 0); break;
    case GLChunk::glCreateShaderProgramvEXT:
    case GLChunk::glCreateShaderProgramv:
      Serialise_glCreateShaderProgramv(ser, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glCreateProgram: Serialise_glCreateProgram(ser, 0); break;
    case GLChunk::glLinkProgram: Serialise_glLinkProgram(ser, 0); break;
    case GLChunk::glUniformBlockBinding: Serialise_glUniformBlockBinding(ser, 0, 0, 0); break;
    case GLChunk::glShaderStorageBlockBinding:
      Serialise_glShaderStorageBlockBinding(ser, 0, 0, 0);
      break;
    case GLChunk::glBindAttribLocation: Serialise_glBindAttribLocation(ser, 0, 0, 0); break;
    case GLChunk::glBindFragDataLocationEXT:
    case GLChunk::glBindFragDataLocation: Serialise_glBindFragDataLocation(ser, 0, 0, 0); break;
    case GLChunk::glUniformSubroutinesuiv:
      Serialise_glUniformSubroutinesuiv(ser, eGL_NONE, 0, 0);
      break;
    case GLChunk::glBindFragDataLocationIndexed:
      Serialise_glBindFragDataLocationIndexed(ser, 0, 0, 0, 0);
      break;
    case GLChunk::glTransformFeedbackVaryingsEXT:
    case GLChunk::glTransformFeedbackVaryings:
      Serialise_glTransformFeedbackVaryings(ser, 0, 0, 0, eGL_NONE);
      break;
    case GLChunk::glProgramParameteriARB:
    case GLChunk::glProgramParameteriEXT:
    case GLChunk::glProgramParameteri: Serialise_glProgramParameteri(ser, 0, eGL_NONE, 0); break;
    case GLChunk::glUseProgram: Serialise_glUseProgram(ser, 0); break;
    case GLChunk::glUseProgramStagesEXT:
    case GLChunk::glUseProgramStages: Serialise_glUseProgramStages(ser, 0, 0, 0); break;
    case GLChunk::glGenProgramPipelinesEXT:
    case GLChunk::glGenProgramPipelines: Serialise_glGenProgramPipelines(ser, 0, 0); break;
    case GLChunk::glCreateProgramPipelines: Serialise_glCreateProgramPipelines(ser, 0, 0); break;
    case GLChunk::glBindProgramPipelineEXT:
    case GLChunk::glBindProgramPipeline: Serialise_glBindProgramPipeline(ser, 0); break;
    case GLChunk::glCompileShaderIncludeARB:
      Serialise_glCompileShaderIncludeARB(ser, 0, 0, 0, 0);
      break;
    case GLChunk::glNamedStringARB: Serialise_glNamedStringARB(ser, eGL_NONE, 0, 0, 0, 0); break;
    case GLChunk::glDeleteNamedStringARB: Serialise_glDeleteNamedStringARB(ser, 0, 0); break;

    case GLChunk::glBlendFunc: Serialise_glBlendFunc(ser, eGL_NONE, eGL_NONE); break;
    case GLChunk::glBlendFunciARB:
    case GLChunk::glBlendFunciEXT:
    case GLChunk::glBlendFunciOES:
    case GLChunk::glBlendFunci: Serialise_glBlendFunci(ser, 0, eGL_NONE, eGL_NONE); break;
    case GLChunk::glBlendColorEXT:
    case GLChunk::glBlendColor: Serialise_glBlendColor(ser, 0, 0, 0, 0); break;
    case GLChunk::glBlendFuncSeparateARB:
    case GLChunk::glBlendFuncSeparate:
      Serialise_glBlendFuncSeparate(ser, eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
      break;
    case GLChunk::glBlendFuncSeparateiARB:
    case GLChunk::glBlendFuncSeparateiEXT:
    case GLChunk::glBlendFuncSeparateiOES:
    case GLChunk::glBlendFuncSeparatei:
      Serialise_glBlendFuncSeparatei(ser, 0, eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
      break;
    case GLChunk::glBlendEquationEXT:
    case GLChunk::glBlendEquation: Serialise_glBlendEquation(ser, eGL_NONE); break;
    case GLChunk::glBlendEquationiARB:
    case GLChunk::glBlendEquationiEXT:
    case GLChunk::glBlendEquationiOES:
    case GLChunk::glBlendEquationi: Serialise_glBlendEquationi(ser, 0, eGL_NONE); break;
    case GLChunk::glBlendEquationSeparateARB:
    case GLChunk::glBlendEquationSeparateEXT:
    case GLChunk::glBlendEquationSeparate:
      Serialise_glBlendEquationSeparate(ser, eGL_NONE, eGL_NONE);
      break;
    case GLChunk::glBlendEquationSeparateiARB:
    case GLChunk::glBlendEquationSeparateiEXT:
    case GLChunk::glBlendEquationSeparateiOES:
    case GLChunk::glBlendEquationSeparatei:
      Serialise_glBlendEquationSeparatei(ser, 0, eGL_NONE, eGL_NONE);
      break;
    case GLChunk::glBlendBarrier:
    case GLChunk::glBlendBarrierKHR: Serialise_glBlendBarrierKHR(ser); break;
    case GLChunk::glLogicOp: Serialise_glLogicOp(ser, eGL_NONE); break;
    case GLChunk::glStencilFunc: Serialise_glStencilFunc(ser, eGL_NONE, 0, 0); break;
    case GLChunk::glStencilFuncSeparate:
      Serialise_glStencilFuncSeparate(ser, eGL_NONE, eGL_NONE, 0, 0);
      break;
    case GLChunk::glStencilMask: Serialise_glStencilMask(ser, 0); break;
    case GLChunk::glStencilMaskSeparate: Serialise_glStencilMaskSeparate(ser, eGL_NONE, 0); break;
    case GLChunk::glStencilOp: Serialise_glStencilOp(ser, eGL_NONE, eGL_NONE, eGL_NONE); break;
    case GLChunk::glStencilOpSeparate:
      Serialise_glStencilOpSeparate(ser, eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
      break;
    case GLChunk::glClearColor: Serialise_glClearColor(ser, 0, 0, 0, 0); break;
    case GLChunk::glClearStencil: Serialise_glClearStencil(ser, 0); break;
    case GLChunk::glClearDepthf:
    case GLChunk::glClearDepth: Serialise_glClearDepth(ser, 0); break;
    case GLChunk::glDepthFunc: Serialise_glDepthFunc(ser, eGL_NONE); break;
    case GLChunk::glDepthMask: Serialise_glDepthMask(ser, 0); break;
    case GLChunk::glDepthRange: Serialise_glDepthRange(ser, 0, 0); break;
    case GLChunk::glDepthRangef: Serialise_glDepthRangef(ser, 0, 0); break;
    case GLChunk::glDepthRangeIndexedfNV:
    case GLChunk::glDepthRangeIndexedfOES:
    case GLChunk::glDepthRangeIndexed: Serialise_glDepthRangeIndexed(ser, 0, 0, 0); break;
    case GLChunk::glDepthRangeArrayfvNV:
    case GLChunk::glDepthRangeArrayfvOES:
    case GLChunk::glDepthRangeArrayv: Serialise_glDepthRangeArrayv(ser, 0, 0, 0); break;
    case GLChunk::glDepthBoundsEXT: Serialise_glDepthBoundsEXT(ser, 0, 0); break;
    case GLChunk::glClipControl: Serialise_glClipControl(ser, eGL_NONE, eGL_NONE); break;
    case GLChunk::glProvokingVertexEXT:
    case GLChunk::glProvokingVertex: Serialise_glProvokingVertex(ser, eGL_NONE); break;
    case GLChunk::glPrimitiveRestartIndex: Serialise_glPrimitiveRestartIndex(ser, 0); break;
    case GLChunk::glDisable: Serialise_glDisable(ser, eGL_NONE); break;
    case GLChunk::glEnable: Serialise_glEnable(ser, eGL_NONE); break;
    case GLChunk::glDisableiEXT:
    case GLChunk::glDisableIndexedEXT:
    case GLChunk::glDisableiNV:
    case GLChunk::glDisableiOES:
    case GLChunk::glDisablei: Serialise_glDisablei(ser, eGL_NONE, 0); break;
    case GLChunk::glEnableiEXT:
    case GLChunk::glEnableIndexedEXT:
    case GLChunk::glEnableiNV:
    case GLChunk::glEnableiOES:
    case GLChunk::glEnablei: Serialise_glEnablei(ser, eGL_NONE, 0); break;
    case GLChunk::glFrontFace: Serialise_glFrontFace(ser, eGL_NONE); break;
    case GLChunk::glCullFace: Serialise_glCullFace(ser, eGL_NONE); break;
    case GLChunk::glHint: Serialise_glHint(ser, eGL_NONE, eGL_NONE); break;
    case GLChunk::glColorMask: Serialise_glColorMask(ser, 0, 0, 0, 0); break;
    case GLChunk::glColorMaskiEXT:
    case GLChunk::glColorMaskIndexedEXT:
    case GLChunk::glColorMaskiOES:
    case GLChunk::glColorMaski: Serialise_glColorMaski(ser, 0, 0, 0, 0, 0); break;
    case GLChunk::glSampleMaski: Serialise_glSampleMaski(ser, 0, 0); break;
    case GLChunk::glSampleCoverageARB:
    case GLChunk::glSampleCoverage: Serialise_glSampleCoverage(ser, 0, 0); break;
    case GLChunk::glMinSampleShadingARB:
    case GLChunk::glMinSampleShadingOES:
    case GLChunk::glMinSampleShading: Serialise_glMinSampleShading(ser, 0); break;
    case GLChunk::glRasterSamplesEXT: Serialise_glRasterSamplesEXT(ser, 0, 0); break;
    case GLChunk::glPatchParameteri: Serialise_glPatchParameteri(ser, eGL_NONE, 0); break;
    case GLChunk::glPatchParameterfv: Serialise_glPatchParameterfv(ser, eGL_NONE, 0); break;
    case GLChunk::glLineWidth: Serialise_glLineWidth(ser, 0); break;
    case GLChunk::glPointSize: Serialise_glPointSize(ser, 0); break;
    case GLChunk::glPatchParameteriEXT:
    case GLChunk::glPatchParameteriOES:
    case GLChunk::glPointParameteri: Serialise_glPointParameteri(ser, eGL_NONE, 0); break;
    case GLChunk::glPointParameteriv: Serialise_glPointParameteriv(ser, eGL_NONE, 0); break;
    case GLChunk::glPointParameterfARB:
    case GLChunk::glPointParameterfEXT:
    case GLChunk::glPointParameterf: Serialise_glPointParameterf(ser, eGL_NONE, 0); break;
    case GLChunk::glPointParameterfvARB:
    case GLChunk::glPointParameterfvEXT:
    case GLChunk::glPointParameterfv: Serialise_glPointParameterfv(ser, eGL_NONE, 0); break;
    case GLChunk::glViewport: Serialise_glViewport(ser, 0, 0, 0, 0); break;
    case GLChunk::glViewportArrayvNV:
    case GLChunk::glViewportArrayvOES:
    case GLChunk::glViewportIndexedf:
    case GLChunk::glViewportIndexedfNV:
    case GLChunk::glViewportIndexedfOES:
    case GLChunk::glViewportIndexedfv:
    case GLChunk::glViewportIndexedfvNV:
    case GLChunk::glViewportIndexedfvOES:
    case GLChunk::glViewportArrayv: Serialise_glViewportArrayv(ser, 0, 0, 0); break;
    case GLChunk::glScissor: Serialise_glScissor(ser, 0, 0, 0, 0); break;
    case GLChunk::glScissorArrayvNV:
    case GLChunk::glScissorArrayvOES:
    case GLChunk::glScissorIndexed:
    case GLChunk::glScissorIndexedNV:
    case GLChunk::glScissorIndexedOES:
    case GLChunk::glScissorIndexedv:
    case GLChunk::glScissorIndexedvNV:
    case GLChunk::glScissorIndexedvOES:
    case GLChunk::glScissorArrayv: Serialise_glScissorArrayv(ser, 0, 0, 0); break;
    case GLChunk::glPolygonMode: Serialise_glPolygonMode(ser, eGL_NONE, eGL_NONE); break;
    case GLChunk::glPolygonOffset: Serialise_glPolygonOffset(ser, 0, 0); break;
    case GLChunk::glPolygonOffsetClampEXT: Serialise_glPolygonOffsetClampEXT(ser, 0, 0, 0); break;
    case GLChunk::glPrimitiveBoundingBoxEXT:
    case GLChunk::glPrimitiveBoundingBoxOES:
    case GLChunk::glPrimitiveBoundingBox:
      Serialise_glPrimitiveBoundingBox(ser, 0, 0, 0, 0, 0, 0, 0, 0);
      break;

    case GLChunk::glGenTextures: Serialise_glGenTextures(ser, 0, 0); break;
    case GLChunk::glCreateTextures: Serialise_glCreateTextures(ser, eGL_NONE, 0, 0); break;
    case GLChunk::glBindTexture: Serialise_glBindTexture(ser, eGL_NONE, 0); break;
    case GLChunk::glBindTextures: Serialise_glBindTextures(ser, 0, 0, 0); break;
    case GLChunk::glBindMultiTextureEXT:
      Serialise_glBindMultiTextureEXT(ser, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glBindTextureUnit: Serialise_glBindTextureUnit(ser, 0, 0); break;
    case GLChunk::glBindImageTextureEXT:
    case GLChunk::glBindImageTexture:
      Serialise_glBindImageTexture(ser, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE);
      break;
    case GLChunk::glBindImageTextures: Serialise_glBindImageTextures(ser, 0, 0, 0); break;
    case GLChunk::glTextureViewEXT:
    case GLChunk::glTextureViewOES:
    case GLChunk::glTextureView:
      Serialise_glTextureView(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
      break;
    case GLChunk::glGenerateMipmap:
    case GLChunk::glGenerateMipmapEXT:
    case GLChunk::glGenerateMultiTexMipmapEXT:
    case GLChunk::glGenerateTextureMipmap:
    case GLChunk::glGenerateTextureMipmapEXT:
      Serialise_glGenerateTextureMipmapEXT(ser, 0, eGL_NONE);
      break;
    case GLChunk::glCopyImageSubDataEXT:
    case GLChunk::glCopyImageSubDataOES:
    case GLChunk::glCopyImageSubData:
      Serialise_glCopyImageSubData(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glCopyMultiTexSubImage1DEXT:
    case GLChunk::glCopyTexSubImage1D:
    case GLChunk::glCopyTextureSubImage1D:
    case GLChunk::glCopyTextureSubImage1DEXT:
      Serialise_glCopyTextureSubImage1DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glCopyTexSubImage2D:
    case GLChunk::glCopyTextureSubImage2D:
    case GLChunk::glCopyMultiTexSubImage2DEXT:
    case GLChunk::glCopyTextureSubImage2DEXT:
      Serialise_glCopyTextureSubImage2DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glCopyMultiTexSubImage3DEXT:
    case GLChunk::glCopyTexSubImage3D:
    case GLChunk::glCopyTexSubImage3DOES:
    case GLChunk::glCopyTextureSubImage3D:
    case GLChunk::glCopyTextureSubImage3DEXT:
      Serialise_glCopyTextureSubImage3DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glMultiTexParameteriEXT:
    case GLChunk::glTexParameteri:
    case GLChunk::glTextureParameteri:
    case GLChunk::glTextureParameteriEXT:
      Serialise_glTextureParameteriEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glMultiTexParameterivEXT:
    case GLChunk::glTexParameteriv:
    case GLChunk::glTextureParameteriv:
    case GLChunk::glTextureParameterivEXT:
      Serialise_glTextureParameterivEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glMultiTexParameterIivEXT:
    case GLChunk::glTexParameterIiv:
    case GLChunk::glTexParameterIivEXT:
    case GLChunk::glTexParameterIivOES:
    case GLChunk::glTextureParameterIiv:
    case GLChunk::glTextureParameterIivEXT:
      Serialise_glTextureParameterIivEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glMultiTexParameterIuivEXT:
    case GLChunk::glTexParameterIuiv:
    case GLChunk::glTexParameterIuivEXT:
    case GLChunk::glTexParameterIuivOES:
    case GLChunk::glTextureParameterIuiv:
    case GLChunk::glTextureParameterIuivEXT:
      Serialise_glTextureParameterIuivEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glMultiTexParameterfEXT:
    case GLChunk::glTexParameterf:
    case GLChunk::glTextureParameterf:
    case GLChunk::glTextureParameterfEXT:
      Serialise_glTextureParameterfEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glMultiTexParameterfvEXT:
    case GLChunk::glTexParameterfv:
    case GLChunk::glTextureParameterfv:
    case GLChunk::glTextureParameterfvEXT:
      Serialise_glTextureParameterfvEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
      break;

    case GLChunk::glPixelStoref:
    case GLChunk::glPixelStorei: Serialise_glPixelStorei(ser, eGL_NONE, 0); break;
    case GLChunk::glActiveTextureARB:
    case GLChunk::glActiveTexture: Serialise_glActiveTexture(ser, eGL_NONE); break;
    case GLChunk::glMultiTexImage1DEXT:
    case GLChunk::glTexImage1D:
    case GLChunk::glTextureImage1DEXT:
      Serialise_glTextureImage1DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glMultiTexImage2DEXT:
    case GLChunk::glTexImage2D:
    case GLChunk::glTextureImage2DEXT:
      Serialise_glTextureImage2DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glMultiTexImage3DEXT:
    case GLChunk::glTexImage3D:
    case GLChunk::glTexImage3DEXT:
    case GLChunk::glTexImage3DOES:
    case GLChunk::glTextureImage3DEXT:
      Serialise_glTextureImage3DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
      break;

    case GLChunk::glCompressedMultiTexImage1DEXT:
    case GLChunk::glCompressedTexImage1D:
    case GLChunk::glCompressedTexImage1DARB:
    case GLChunk::glCompressedTextureImage1DEXT:
      Serialise_glCompressedTextureImage1DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
      break;
    case GLChunk::glCompressedMultiTexImage2DEXT:
    case GLChunk::glCompressedTexImage2D:
    case GLChunk::glCompressedTexImage2DARB:
    case GLChunk::glCompressedTextureImage2DEXT:
      Serialise_glCompressedTextureImage2DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glCompressedMultiTexImage3DEXT:
    case GLChunk::glCompressedTexImage3D:
    case GLChunk::glCompressedTexImage3DARB:
    case GLChunk::glCompressedTexImage3DOES:
    case GLChunk::glCompressedTextureImage3DEXT:
      Serialise_glCompressedTextureImage3DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glCopyTexImage1D:
    case GLChunk::glCopyMultiTexImage1DEXT:
    case GLChunk::glCopyTextureImage1DEXT:
      Serialise_glCopyTextureImage1DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
      break;
    case GLChunk::glCopyTexImage2D:
    case GLChunk::glCopyMultiTexImage2DEXT:
    case GLChunk::glCopyTextureImage2DEXT:
      Serialise_glCopyTextureImage2DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0, 0);
      break;
    case GLChunk::glTexStorage1D:
    case GLChunk::glTexStorage1DEXT:
    case GLChunk::glTextureStorage1D:
    case GLChunk::glTextureStorage1DEXT:
      Serialise_glTextureStorage1DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0);
      break;
    case GLChunk::glTexStorage2D:
    case GLChunk::glTexStorage2DEXT:
    case GLChunk::glTextureStorage2D:
    case GLChunk::glTextureStorage2DEXT:
      Serialise_glTextureStorage2DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glTexStorage3D:
    case GLChunk::glTexStorage3DEXT:
    case GLChunk::glTextureStorage3D:
    case GLChunk::glTextureStorage3DEXT:
      Serialise_glTextureStorage3DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glTexImage2DMultisample:
    // technically this isn't equivalent to storage, but we treat it as such because there's no DSA
    // variant of this teximage
    case GLChunk::glTexStorage2DMultisample:
    case GLChunk::glTextureStorage2DMultisample:
    case GLChunk::glTextureStorage2DMultisampleEXT:
      Serialise_glTextureStorage2DMultisampleEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glTexImage3DMultisample:
    // technically this isn't equivalent to storage, but we treat it as such because there's no DSA
    // variant of this teximage
    case GLChunk::glTexStorage3DMultisample:
    case GLChunk::glTexStorage3DMultisampleOES:
    case GLChunk::glTextureStorage3DMultisample:
    case GLChunk::glTextureStorage3DMultisampleEXT:
      Serialise_glTextureStorage3DMultisampleEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
      break;
    case GLChunk::glMultiTexSubImage1DEXT:
    case GLChunk::glTexSubImage1D:
    case GLChunk::glTextureSubImage1D:
    case GLChunk::glTextureSubImage1DEXT:
      Serialise_glTextureSubImage1DEXT(ser, 0, eGL_NONE, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glMultiTexSubImage2DEXT:
    case GLChunk::glTexSubImage2D:
    case GLChunk::glTextureSubImage2D:
    case GLChunk::glTextureSubImage2DEXT:
      Serialise_glTextureSubImage2DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glMultiTexSubImage3DEXT:
    case GLChunk::glTexSubImage3D:
    case GLChunk::glTexSubImage3DOES:
    case GLChunk::glTextureSubImage3D:
    case GLChunk::glTextureSubImage3DEXT:
      Serialise_glTextureSubImage3DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
      break;
    case GLChunk::glCompressedMultiTexSubImage1DEXT:
    case GLChunk::glCompressedTexSubImage1D:
    case GLChunk::glCompressedTexSubImage1DARB:
    case GLChunk::glCompressedTextureSubImage1D:
    case GLChunk::glCompressedTextureSubImage1DEXT:
      Serialise_glCompressedTextureSubImage1DEXT(ser, 0, eGL_NONE, 0, 0, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glCompressedMultiTexSubImage2DEXT:
    case GLChunk::glCompressedTexSubImage2D:
    case GLChunk::glCompressedTexSubImage2DARB:
    case GLChunk::glCompressedTextureSubImage2D:
    case GLChunk::glCompressedTextureSubImage2DEXT:
      Serialise_glCompressedTextureSubImage2DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, 0, 0);
      break;
    case GLChunk::glCompressedMultiTexSubImage3DEXT:
    case GLChunk::glCompressedTexSubImage3D:
    case GLChunk::glCompressedTexSubImage3DARB:
    case GLChunk::glCompressedTexSubImage3DOES:
    case GLChunk::glCompressedTextureSubImage3D:
    case GLChunk::glCompressedTextureSubImage3DEXT:
      Serialise_glCompressedTextureSubImage3DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, eGL_NONE, 0,
                                                 0);
      break;
    case GLChunk::glTexBufferRange:
    case GLChunk::glTexBufferRangeEXT:
    case GLChunk::glTexBufferRangeOES:
    case GLChunk::glTextureBufferRange:
    case GLChunk::glTextureBufferRangeEXT:
      Serialise_glTextureBufferRangeEXT(ser, 0, eGL_NONE, eGL_NONE, 0, 0, 0);
      break;
    case GLChunk::glMultiTexBufferEXT:
    case GLChunk::glTexBuffer:
    case GLChunk::glTexBufferARB:
    case GLChunk::glTexBufferEXT:
    case GLChunk::glTexBufferOES:
    case GLChunk::glTextureBuffer:
    case GLChunk::glTextureBufferEXT:
      Serialise_glTextureBufferEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
      break;

    case GLChunk::glProgramUniform1d:
    case GLChunk::glProgramUniform1dEXT:
    case GLChunk::glProgramUniform1dv:
    case GLChunk::glProgramUniform1dvEXT:
    case GLChunk::glProgramUniform1f:
    case GLChunk::glProgramUniform1fEXT:
    case GLChunk::glProgramUniform1fv:
    case GLChunk::glProgramUniform1fvEXT:
    case GLChunk::glProgramUniform1i:
    case GLChunk::glProgramUniform1iEXT:
    case GLChunk::glProgramUniform1iv:
    case GLChunk::glProgramUniform1ivEXT:
    case GLChunk::glProgramUniform1ui:
    case GLChunk::glProgramUniform1uiEXT:
    case GLChunk::glProgramUniform1uiv:
    case GLChunk::glProgramUniform1uivEXT:
    case GLChunk::glProgramUniform2d:
    case GLChunk::glProgramUniform2dEXT:
    case GLChunk::glProgramUniform2dv:
    case GLChunk::glProgramUniform2dvEXT:
    case GLChunk::glProgramUniform2f:
    case GLChunk::glProgramUniform2fEXT:
    case GLChunk::glProgramUniform2fv:
    case GLChunk::glProgramUniform2fvEXT:
    case GLChunk::glProgramUniform2i:
    case GLChunk::glProgramUniform2iEXT:
    case GLChunk::glProgramUniform2iv:
    case GLChunk::glProgramUniform2ivEXT:
    case GLChunk::glProgramUniform2ui:
    case GLChunk::glProgramUniform2uiEXT:
    case GLChunk::glProgramUniform2uiv:
    case GLChunk::glProgramUniform2uivEXT:
    case GLChunk::glProgramUniform3d:
    case GLChunk::glProgramUniform3dEXT:
    case GLChunk::glProgramUniform3dv:
    case GLChunk::glProgramUniform3dvEXT:
    case GLChunk::glProgramUniform3f:
    case GLChunk::glProgramUniform3fEXT:
    case GLChunk::glProgramUniform3fv:
    case GLChunk::glProgramUniform3fvEXT:
    case GLChunk::glProgramUniform3i:
    case GLChunk::glProgramUniform3iEXT:
    case GLChunk::glProgramUniform3iv:
    case GLChunk::glProgramUniform3ivEXT:
    case GLChunk::glProgramUniform3ui:
    case GLChunk::glProgramUniform3uiEXT:
    case GLChunk::glProgramUniform3uiv:
    case GLChunk::glProgramUniform3uivEXT:
    case GLChunk::glProgramUniform4d:
    case GLChunk::glProgramUniform4dEXT:
    case GLChunk::glProgramUniform4dv:
    case GLChunk::glProgramUniform4dvEXT:
    case GLChunk::glProgramUniform4f:
    case GLChunk::glProgramUniform4fEXT:
    case GLChunk::glProgramUniform4fv:
    case GLChunk::glProgramUniform4fvEXT:
    case GLChunk::glProgramUniform4i:
    case GLChunk::glProgramUniform4iEXT:
    case GLChunk::glProgramUniform4iv:
    case GLChunk::glProgramUniform4ivEXT:
    case GLChunk::glProgramUniform4ui:
    case GLChunk::glProgramUniform4uiEXT:
    case GLChunk::glProgramUniform4uiv:
    case GLChunk::glProgramUniform4uivEXT:
    case GLChunk::glUniform1d:
    case GLChunk::glUniform1dv:
    case GLChunk::glUniform1f:
    case GLChunk::glUniform1fv:
    case GLChunk::glUniform1i:
    case GLChunk::glUniform1iv:
    case GLChunk::glUniform1ui:
    case GLChunk::glUniform1uiEXT:
    case GLChunk::glUniform1uiv:
    case GLChunk::glUniform1uivEXT:
    case GLChunk::glUniform2d:
    case GLChunk::glUniform2dv:
    case GLChunk::glUniform2f:
    case GLChunk::glUniform2fv:
    case GLChunk::glUniform2i:
    case GLChunk::glUniform2iv:
    case GLChunk::glUniform2ui:
    case GLChunk::glUniform2uiEXT:
    case GLChunk::glUniform2uiv:
    case GLChunk::glUniform2uivEXT:
    case GLChunk::glUniform3d:
    case GLChunk::glUniform3dv:
    case GLChunk::glUniform3f:
    case GLChunk::glUniform3fv:
    case GLChunk::glUniform3i:
    case GLChunk::glUniform3iv:
    case GLChunk::glUniform3ui:
    case GLChunk::glUniform3uiEXT:
    case GLChunk::glUniform3uiv:
    case GLChunk::glUniform3uivEXT:
    case GLChunk::glUniform4d:
    case GLChunk::glUniform4dv:
    case GLChunk::glUniform4f:
    case GLChunk::glUniform4fv:
    case GLChunk::glUniform4i:
    case GLChunk::glUniform4iv:
    case GLChunk::glUniform4ui:
    case GLChunk::glUniform4uiEXT:
    case GLChunk::glUniform4uiv:
    case GLChunk::glUniform4uivEXT:
      Serialise_glProgramUniformVector(ser, 0, 0, 0, 0, UNIFORM_UNKNOWN);
      break;

    case GLChunk::glProgramUniformMatrix2dv:
    case GLChunk::glProgramUniformMatrix2dvEXT:
    case GLChunk::glProgramUniformMatrix2fv:
    case GLChunk::glProgramUniformMatrix2fvEXT:
    case GLChunk::glProgramUniformMatrix2x3dv:
    case GLChunk::glProgramUniformMatrix2x3dvEXT:
    case GLChunk::glProgramUniformMatrix2x3fv:
    case GLChunk::glProgramUniformMatrix2x3fvEXT:
    case GLChunk::glProgramUniformMatrix2x4dv:
    case GLChunk::glProgramUniformMatrix2x4dvEXT:
    case GLChunk::glProgramUniformMatrix2x4fv:
    case GLChunk::glProgramUniformMatrix2x4fvEXT:
    case GLChunk::glProgramUniformMatrix3dv:
    case GLChunk::glProgramUniformMatrix3dvEXT:
    case GLChunk::glProgramUniformMatrix3fv:
    case GLChunk::glProgramUniformMatrix3fvEXT:
    case GLChunk::glProgramUniformMatrix3x2dv:
    case GLChunk::glProgramUniformMatrix3x2dvEXT:
    case GLChunk::glProgramUniformMatrix3x2fv:
    case GLChunk::glProgramUniformMatrix3x2fvEXT:
    case GLChunk::glProgramUniformMatrix3x4dv:
    case GLChunk::glProgramUniformMatrix3x4dvEXT:
    case GLChunk::glProgramUniformMatrix3x4fv:
    case GLChunk::glProgramUniformMatrix3x4fvEXT:
    case GLChunk::glProgramUniformMatrix4dv:
    case GLChunk::glProgramUniformMatrix4dvEXT:
    case GLChunk::glProgramUniformMatrix4fv:
    case GLChunk::glProgramUniformMatrix4fvEXT:
    case GLChunk::glProgramUniformMatrix4x2dv:
    case GLChunk::glProgramUniformMatrix4x2dvEXT:
    case GLChunk::glProgramUniformMatrix4x2fv:
    case GLChunk::glProgramUniformMatrix4x2fvEXT:
    case GLChunk::glProgramUniformMatrix4x3dv:
    case GLChunk::glProgramUniformMatrix4x3dvEXT:
    case GLChunk::glProgramUniformMatrix4x3fv:
    case GLChunk::glProgramUniformMatrix4x3fvEXT:
    case GLChunk::glUniformMatrix2dv:
    case GLChunk::glUniformMatrix2fv:
    case GLChunk::glUniformMatrix2x3dv:
    case GLChunk::glUniformMatrix2x3fv:
    case GLChunk::glUniformMatrix2x4dv:
    case GLChunk::glUniformMatrix2x4fv:
    case GLChunk::glUniformMatrix3dv:
    case GLChunk::glUniformMatrix3fv:
    case GLChunk::glUniformMatrix3x2dv:
    case GLChunk::glUniformMatrix3x2fv:
    case GLChunk::glUniformMatrix3x4dv:
    case GLChunk::glUniformMatrix3x4fv:
    case GLChunk::glUniformMatrix4dv:
    case GLChunk::glUniformMatrix4fv:
    case GLChunk::glUniformMatrix4x2dv:
    case GLChunk::glUniformMatrix4x2fv:
    case GLChunk::glUniformMatrix4x3dv:
    case GLChunk::glUniformMatrix4x3fv:
      Serialise_glProgramUniformMatrix(ser, 0, 0, 0, 0, 0, UNIFORM_UNKNOWN);
      break;

    case GLChunk::vrapi_CreateTextureSwapChain:
    case GLChunk::vrapi_CreateTextureSwapChain2:
      // nothing to do, these chunks are just markers
      break;

    // these functions are not currently serialised - they do nothing on replay and are not
    // serialised for information (it would be harmless and perhaps useful for the user to see
    // where and how they're called).
    case GLChunk::glGetActiveAtomicCounterBufferiv:
    case GLChunk::glGetActiveAttrib:
    case GLChunk::glGetActiveSubroutineName:
    case GLChunk::glGetActiveSubroutineUniformiv:
    case GLChunk::glGetActiveSubroutineUniformName:
    case GLChunk::glGetActiveUniform:
    case GLChunk::glGetActiveUniformBlockiv:
    case GLChunk::glGetActiveUniformBlockName:
    case GLChunk::glGetActiveUniformName:
    case GLChunk::glGetActiveUniformsiv:
    case GLChunk::glGetAttachedShaders:
    case GLChunk::glGetAttribLocation:
    case GLChunk::glGetBooleani_v:
    case GLChunk::glGetBooleanIndexedvEXT:
    case GLChunk::glGetBooleanv:
    case GLChunk::glGetBufferParameteri64v:
    case GLChunk::glGetBufferParameteriv:
    case GLChunk::glGetBufferParameterivARB:
    case GLChunk::glGetBufferPointerv:
    case GLChunk::glGetBufferPointervARB:
    case GLChunk::glGetBufferPointervOES:
    case GLChunk::glGetBufferSubData:
    case GLChunk::glGetBufferSubDataARB:
    case GLChunk::glGetCompressedMultiTexImageEXT:
    case GLChunk::glGetCompressedTexImage:
    case GLChunk::glGetCompressedTexImageARB:
    case GLChunk::glGetCompressedTextureImage:
    case GLChunk::glGetCompressedTextureImageEXT:
    case GLChunk::glGetCompressedTextureSubImage:
    case GLChunk::glGetDebugMessageLog:
    case GLChunk::glGetDebugMessageLogARB:
    case GLChunk::glGetDebugMessageLogKHR:
    case GLChunk::glGetDoublei_v:
    case GLChunk::glGetDoublei_vEXT:
    case GLChunk::glGetDoubleIndexedvEXT:
    case GLChunk::glGetDoublev:
    case GLChunk::glGetError:
    case GLChunk::glGetFloati_v:
    case GLChunk::glGetFloati_vEXT:
    case GLChunk::glGetFloati_vNV:
    case GLChunk::glGetFloati_vOES:
    case GLChunk::glGetFloatIndexedvEXT:
    case GLChunk::glGetFloatv:
    case GLChunk::glGetFragDataIndex:
    case GLChunk::glGetFragDataLocation:
    case GLChunk::glGetFragDataLocationEXT:
    case GLChunk::glGetFramebufferAttachmentParameteriv:
    case GLChunk::glGetFramebufferAttachmentParameterivEXT:
    case GLChunk::glGetFramebufferParameteriv:
    case GLChunk::glGetFramebufferParameterivEXT:
    case GLChunk::glGetGraphicsResetStatus:
    case GLChunk::glGetGraphicsResetStatusARB:
    case GLChunk::glGetGraphicsResetStatusEXT:
    case GLChunk::glGetInteger64i_v:
    case GLChunk::glGetInteger64v:
    case GLChunk::glGetIntegeri_v:
    case GLChunk::glGetIntegerIndexedvEXT:
    case GLChunk::glGetIntegerv:
    case GLChunk::glGetInternalformati64v:
    case GLChunk::glGetInternalformativ:
    case GLChunk::glGetMultisamplefv:
    case GLChunk::glGetMultiTexImageEXT:
    case GLChunk::glGetMultiTexLevelParameterfvEXT:
    case GLChunk::glGetMultiTexLevelParameterivEXT:
    case GLChunk::glGetMultiTexParameterfvEXT:
    case GLChunk::glGetMultiTexParameterIivEXT:
    case GLChunk::glGetMultiTexParameterIuivEXT:
    case GLChunk::glGetMultiTexParameterivEXT:
    case GLChunk::glGetNamedBufferParameteri64v:
    case GLChunk::glGetNamedBufferParameteriv:
    case GLChunk::glGetNamedBufferParameterivEXT:
    case GLChunk::glGetNamedBufferPointerv:
    case GLChunk::glGetNamedBufferPointervEXT:
    case GLChunk::glGetNamedBufferSubData:
    case GLChunk::glGetNamedBufferSubDataEXT:
    case GLChunk::glGetNamedFramebufferAttachmentParameteriv:
    case GLChunk::glGetNamedFramebufferAttachmentParameterivEXT:
    case GLChunk::glGetNamedFramebufferParameteriv:
    case GLChunk::glGetNamedFramebufferParameterivEXT:
    case GLChunk::glGetNamedProgramivEXT:
    case GLChunk::glGetNamedRenderbufferParameteriv:
    case GLChunk::glGetNamedRenderbufferParameterivEXT:
    case GLChunk::glGetNamedStringARB:
    case GLChunk::glGetNamedStringivARB:
    case GLChunk::glGetnCompressedTexImage:
    case GLChunk::glGetnCompressedTexImageARB:
    case GLChunk::glGetnTexImage:
    case GLChunk::glGetnTexImageARB:
    case GLChunk::glGetnUniformdv:
    case GLChunk::glGetnUniformdvARB:
    case GLChunk::glGetnUniformfv:
    case GLChunk::glGetnUniformfvARB:
    case GLChunk::glGetnUniformfvEXT:
    case GLChunk::glGetnUniformiv:
    case GLChunk::glGetnUniformivARB:
    case GLChunk::glGetnUniformivEXT:
    case GLChunk::glGetnUniformuiv:
    case GLChunk::glGetnUniformuivARB:
    case GLChunk::glGetObjectLabel:
    case GLChunk::glGetObjectLabelEXT:
    case GLChunk::glGetObjectLabelKHR:
    case GLChunk::glGetObjectPtrLabel:
    case GLChunk::glGetObjectPtrLabelKHR:
    case GLChunk::glGetPointeri_vEXT:
    case GLChunk::glGetPointerIndexedvEXT:
    case GLChunk::glGetPointerv:
    case GLChunk::glGetPointervKHR:
    case GLChunk::glGetProgramBinary:
    case GLChunk::glGetProgramInfoLog:
    case GLChunk::glGetProgramInterfaceiv:
    case GLChunk::glGetProgramiv:
    case GLChunk::glGetProgramPipelineInfoLog:
    case GLChunk::glGetProgramPipelineInfoLogEXT:
    case GLChunk::glGetProgramPipelineiv:
    case GLChunk::glGetProgramPipelineivEXT:
    case GLChunk::glGetProgramResourceIndex:
    case GLChunk::glGetProgramResourceiv:
    case GLChunk::glGetProgramResourceLocation:
    case GLChunk::glGetProgramResourceLocationIndex:
    case GLChunk::glGetProgramResourceName:
    case GLChunk::glGetProgramStageiv:
    case GLChunk::glGetQueryBufferObjecti64v:
    case GLChunk::glGetQueryBufferObjectiv:
    case GLChunk::glGetQueryBufferObjectui64v:
    case GLChunk::glGetQueryBufferObjectuiv:
    case GLChunk::glGetQueryIndexediv:
    case GLChunk::glGetQueryiv:
    case GLChunk::glGetQueryivARB:
    case GLChunk::glGetQueryivEXT:
    case GLChunk::glGetQueryObjecti64v:
    case GLChunk::glGetQueryObjecti64vEXT:
    case GLChunk::glGetQueryObjectiv:
    case GLChunk::glGetQueryObjectivARB:
    case GLChunk::glGetQueryObjectivEXT:
    case GLChunk::glGetQueryObjectui64v:
    case GLChunk::glGetQueryObjectui64vEXT:
    case GLChunk::glGetQueryObjectuiv:
    case GLChunk::glGetQueryObjectuivARB:
    case GLChunk::glGetQueryObjectuivEXT:
    case GLChunk::glGetRenderbufferParameteriv:
    case GLChunk::glGetRenderbufferParameterivEXT:
    case GLChunk::glGetSamplerParameterfv:
    case GLChunk::glGetSamplerParameterIiv:
    case GLChunk::glGetSamplerParameterIivEXT:
    case GLChunk::glGetSamplerParameterIivOES:
    case GLChunk::glGetSamplerParameterIuiv:
    case GLChunk::glGetSamplerParameterIuivEXT:
    case GLChunk::glGetSamplerParameterIuivOES:
    case GLChunk::glGetSamplerParameteriv:
    case GLChunk::glGetShaderInfoLog:
    case GLChunk::glGetShaderiv:
    case GLChunk::glGetShaderPrecisionFormat:
    case GLChunk::glGetShaderSource:
    case GLChunk::glGetString:
    case GLChunk::glGetStringi:
    case GLChunk::glGetSubroutineIndex:
    case GLChunk::glGetSubroutineUniformLocation:
    case GLChunk::glGetSynciv:
    case GLChunk::glGetTexImage:
    case GLChunk::glGetTexLevelParameterfv:
    case GLChunk::glGetTexLevelParameteriv:
    case GLChunk::glGetTexParameterfv:
    case GLChunk::glGetTexParameterIiv:
    case GLChunk::glGetTexParameterIivEXT:
    case GLChunk::glGetTexParameterIivOES:
    case GLChunk::glGetTexParameterIuiv:
    case GLChunk::glGetTexParameterIuivEXT:
    case GLChunk::glGetTexParameterIuivOES:
    case GLChunk::glGetTexParameteriv:
    case GLChunk::glGetTextureImage:
    case GLChunk::glGetTextureImageEXT:
    case GLChunk::glGetTextureLevelParameterfv:
    case GLChunk::glGetTextureLevelParameterfvEXT:
    case GLChunk::glGetTextureLevelParameteriv:
    case GLChunk::glGetTextureLevelParameterivEXT:
    case GLChunk::glGetTextureParameterfv:
    case GLChunk::glGetTextureParameterfvEXT:
    case GLChunk::glGetTextureParameterIiv:
    case GLChunk::glGetTextureParameterIivEXT:
    case GLChunk::glGetTextureParameterIuiv:
    case GLChunk::glGetTextureParameterIuivEXT:
    case GLChunk::glGetTextureParameteriv:
    case GLChunk::glGetTextureParameterivEXT:
    case GLChunk::glGetTextureSubImage:
    case GLChunk::glGetTransformFeedbacki_v:
    case GLChunk::glGetTransformFeedbacki64_v:
    case GLChunk::glGetTransformFeedbackiv:
    case GLChunk::glGetTransformFeedbackVarying:
    case GLChunk::glGetTransformFeedbackVaryingEXT:
    case GLChunk::glGetUniformBlockIndex:
    case GLChunk::glGetUniformdv:
    case GLChunk::glGetUniformfv:
    case GLChunk::glGetUniformIndices:
    case GLChunk::glGetUniformiv:
    case GLChunk::glGetUniformLocation:
    case GLChunk::glGetUniformSubroutineuiv:
    case GLChunk::glGetUniformuiv:
    case GLChunk::glGetUniformuivEXT:
    case GLChunk::glGetVertexArrayIndexed64iv:
    case GLChunk::glGetVertexArrayIndexediv:
    case GLChunk::glGetVertexArrayIntegeri_vEXT:
    case GLChunk::glGetVertexArrayIntegervEXT:
    case GLChunk::glGetVertexArrayiv:
    case GLChunk::glGetVertexArrayPointeri_vEXT:
    case GLChunk::glGetVertexArrayPointervEXT:
    case GLChunk::glGetVertexAttribdv:
    case GLChunk::glGetVertexAttribfv:
    case GLChunk::glGetVertexAttribIiv:
    case GLChunk::glGetVertexAttribIivEXT:
    case GLChunk::glGetVertexAttribIuiv:
    case GLChunk::glGetVertexAttribIuivEXT:
    case GLChunk::glGetVertexAttribiv:
    case GLChunk::glGetVertexAttribLdv:
    case GLChunk::glGetVertexAttribLdvEXT:
    case GLChunk::glGetVertexAttribPointerv:
    case GLChunk::glIsBuffer:
    case GLChunk::glIsBufferARB:
    case GLChunk::glIsEnabled:
    case GLChunk::glIsEnabledi:
    case GLChunk::glIsEnablediEXT:
    case GLChunk::glIsEnabledIndexedEXT:
    case GLChunk::glIsEnablediNV:
    case GLChunk::glIsEnablediOES:
    case GLChunk::glIsFramebuffer:
    case GLChunk::glIsFramebufferEXT:
    case GLChunk::glIsNamedStringARB:
    case GLChunk::glIsProgram:
    case GLChunk::glIsProgramPipeline:
    case GLChunk::glIsProgramPipelineEXT:
    case GLChunk::glIsQuery:
    case GLChunk::glIsQueryARB:
    case GLChunk::glIsQueryEXT:
    case GLChunk::glIsRenderbuffer:
    case GLChunk::glIsRenderbufferEXT:
    case GLChunk::glIsSampler:
    case GLChunk::glIsShader:
    case GLChunk::glIsSync:
    case GLChunk::glIsTexture:
    case GLChunk::glIsTransformFeedback:
    case GLChunk::glIsVertexArray:
    case GLChunk::glIsVertexArrayOES:
    case GLChunk::glValidateProgram:
    case GLChunk::glValidateProgramPipeline:
    case GLChunk::glValidateProgramPipelineEXT:
    case GLChunk::glCheckFramebufferStatus:
    case GLChunk::glCheckFramebufferStatusEXT:
    case GLChunk::glCheckNamedFramebufferStatus:
    case GLChunk::glCheckNamedFramebufferStatusEXT:
    case GLChunk::glReadnPixels:
    case GLChunk::glReadnPixelsARB:
    case GLChunk::glReadnPixelsEXT:
    case GLChunk::glClampColor:
    case GLChunk::glClampColorARB:
    case GLChunk::glReadPixels:
    case GLChunk::glDeleteBuffers:
    case GLChunk::glDeleteBuffersARB:
    case GLChunk::glDeleteFramebuffers:
    case GLChunk::glDeleteFramebuffersEXT:
    case GLChunk::glDeleteProgram:
    case GLChunk::glDeleteProgramPipelines:
    case GLChunk::glDeleteProgramPipelinesEXT:
    case GLChunk::glDeleteQueries:
    case GLChunk::glDeleteQueriesARB:
    case GLChunk::glDeleteQueriesEXT:
    case GLChunk::glDeleteRenderbuffers:
    case GLChunk::glDeleteRenderbuffersEXT:
    case GLChunk::glDeleteSamplers:
    case GLChunk::glDeleteShader:
    case GLChunk::glDeleteSync:
    case GLChunk::glDeleteTextures:
    case GLChunk::glDeleteTransformFeedbacks:
    case GLChunk::glDeleteVertexArrays:
    case GLChunk::glDeleteVertexArraysOES:
    case GLChunk::glBindRenderbufferEXT:
    case GLChunk::glBindRenderbuffer:
    case GLChunk::glActiveShaderProgram:
    case GLChunk::glActiveShaderProgramEXT:
    case GLChunk::glProgramBinary:
    case GLChunk::glShaderBinary:
    case GLChunk::glReleaseShaderCompiler:
    case GLChunk::glFrameTerminatorGREMEDY:
    case GLChunk::glDiscardFramebufferEXT:
    case GLChunk::glFinish:
    case GLChunk::glFlush:
    case GLChunk::glInvalidateBufferData:
    case GLChunk::glInvalidateBufferSubData:
    case GLChunk::glInvalidateFramebuffer:
    case GLChunk::glInvalidateNamedFramebufferData:
    case GLChunk::glInvalidateNamedFramebufferSubData:
    case GLChunk::glInvalidateSubFramebuffer:
    case GLChunk::glInvalidateTexImage:
    case GLChunk::glInvalidateTexSubImage:
    case GLChunk::glDebugMessageCallback:
    case GLChunk::glDebugMessageCallbackARB:
    case GLChunk::glDebugMessageCallbackKHR:
    case GLChunk::glDebugMessageControl:
    case GLChunk::glDebugMessageControlARB:
    case GLChunk::glDebugMessageControlKHR:
    case GLChunk::glMapBuffer:
    case GLChunk::glMapBufferARB:
    case GLChunk::glMapBufferOES:
    case GLChunk::glMapBufferRange:
    case GLChunk::glMapNamedBuffer:
    case GLChunk::glMapNamedBufferEXT:
    case GLChunk::glMapNamedBufferRange:
    case GLChunk::glMapNamedBufferRangeEXT:
    case GLChunk::wglDXSetResourceShareHandleNV:
    case GLChunk::wglDXOpenDeviceNV:
    case GLChunk::wglDXCloseDeviceNV:
    case GLChunk::wglDXUnregisterObjectNV:
    case GLChunk::wglDXObjectAccessNV:
    case GLChunk::wglDXUnlockObjectsNV:
    case GLChunk::Max:
      RDCERR("Unexpected chunk, or missing case for processing! Skipping...");
      ser.SkipCurrentChunk();
      break;

    case GLChunk::CaptureScope: Serialise_CaptureScope(ser); break;
    case GLChunk::CaptureBegin:
      // normally this would be handled as a special case when we start processing the frame,
      // but it can be emitted mid-frame if MakeCurrent is called on a different context.
      // when processed here, we always want to apply the contents
      Serialise_BeginCaptureFrame(ser);
      break;
    case GLChunk::CaptureEnd:
    {
      if(IsLoading(m_State))
      {
        AddEvent();

        DrawcallDescription draw;
        draw.name = "SwapBuffers()";
        draw.flags |= DrawFlags::Present;

        draw.copyDestination = GetResourceManager()->GetOriginalID(
            GetResourceManager()->GetID(TextureRes(GetCtx(), m_FakeBB_Color)));

        AddDrawcall(draw, true);
      }
      break;
    }
  }
}

void WrappedOpenGL::ContextReplayLog(CaptureState readType, uint32_t startEventID,
                                     uint32_t endEventID, bool partial)
{
  m_FrameReader->SetOffset(0);

  ReadSerialiser ser(m_FrameReader, Ownership::Nothing);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  SDFile *prevFile = m_StructuredFile;

  if(IsLoading(m_State) || IsStructuredExporting(m_State))
  {
    ser.ConfigureStructuredExport(&GetChunkName, IsStructuredExporting(m_State));

    ser.GetStructuredFile().swap(*m_StructuredFile);

    m_StructuredFile = &ser.GetStructuredFile();
  }

  GLChunk header = ser.ReadChunk<GLChunk>();
  RDCASSERTEQUAL(header, GLChunk::CaptureBegin);

  if(IsActiveReplaying(m_State) && !partial)
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

  if(partial)
    ser.SkipCurrentChunk();
  else
    Serialise_BeginCaptureFrame(ser);

  ser.EndChunk();

  m_CurEvents.clear();

  if(IsActiveReplaying(m_State))
  {
    APIEvent ev = GetEvent(startEventID);
    m_CurEventID = ev.eventID;
    if(partial)
      ser.GetReader()->SetOffset(ev.fileOffset);
    m_FirstEventID = startEventID;
    m_LastEventID = endEventID;
  }
  else
  {
    m_CurEventID = 1;
    m_CurDrawcallID = 1;
    m_FirstEventID = 0;
    m_LastEventID = ~0U;
  }

  GetResourceManager()->MarkInFrame(true);

  uint64_t startOffset = ser.GetReader()->GetOffset();

  for(;;)
  {
    if(IsActiveReplaying(m_State) && m_CurEventID > endEventID)
    {
      // we can just break out if we've done all the events desired.
      break;
    }

    m_CurChunkOffset = ser.GetReader()->GetOffset();

    GLChunk chunktype = ser.ReadChunk<GLChunk>();

    m_ChunkMetadata = ser.ChunkMetadata();

    ContextProcessChunk(ser, chunktype);

    ser.EndChunk();

    RenderDoc::Inst().SetProgress(
        FileInitialRead, float(m_CurChunkOffset - startOffset) / float(ser.GetReader()->GetSize()));

    if(chunktype == GLChunk::CaptureEnd)
      break;

    m_CurEventID++;
  }

  // swap the structure back now that we've accumulated the frame as well.
  if(IsLoading(m_State) || IsStructuredExporting(m_State))
    ser.GetStructuredFile().swap(*prevFile);

  m_StructuredFile = prevFile;

  if(IsLoading(m_State))
  {
    GetFrameRecord().drawcallList = m_ParentDrawcall.children;
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
}

void WrappedOpenGL::ContextProcessChunk(ReadSerialiser &ser, GLChunk chunk)
{
  m_AddedDrawcall = false;

  ProcessChunk(ser, chunk);

  if(IsLoading(m_State))
  {
    switch(chunk)
    {
      case GLChunk::glStringMarkerGREMEDY:
      case GLChunk::glInsertEventMarkerEXT:
      case GLChunk::glDebugMessageInsert:
      case GLChunk::glDebugMessageInsertARB:
      case GLChunk::glDebugMessageInsertKHR:
        // no push/pop necessary
        break;
      case GLChunk::glPushGroupMarkerEXT:
      case GLChunk::glPushDebugGroup:
      case GLChunk::glPushDebugGroupKHR:
      {
        // push down the drawcallstack to the latest drawcall
        m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());
        break;
      }
      case GLChunk::glPopGroupMarkerEXT:
      case GLChunk::glPopDebugGroup:
      case GLChunk::glPopDebugGroupKHR:
      {
        // refuse to pop off further than the root drawcall (mismatched begin/end events e.g.)
        if(m_DrawcallStack.size() > 1)
          m_DrawcallStack.pop_back();
        break;
      }
      default:
      {
        if(!m_AddedDrawcall)
          AddEvent();
      }
    }
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
    GLRenderState rs(&m_Real);
    rs.FetchState(this);

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
      EventUsage ro = EventUsage(e, ResUsage(i));
      EventUsage rw = EventUsage(e, RWResUsage(i));

      if(refl[i])
      {
        for(const ConstantBlock &cblock : refl[i]->ConstantBlocks)
        {
          if(!cblock.bufferBacked)
            continue;
          if(cblock.bindPoint < 0 || cblock.bindPoint >= mapping[i].ConstantBlocks.count())
            continue;

          int32_t bind = mapping[i].ConstantBlocks[cblock.bindPoint].bind;

          if(rs.UniformBinding[bind].res.name)
            m_ResourceUses[rm->GetID(rs.UniformBinding[bind].res)].push_back(cb);
        }

        for(const ShaderResource &res : refl[i]->ReadWriteResources)
        {
          int32_t bind = mapping[i].ReadWriteResources[res.bindPoint].bind;

          if(res.IsTexture)
          {
            if(rs.Images[bind].res.name)
              m_ResourceUses[rm->GetID(rs.Images[bind].res)].push_back(rw);
          }
          else
          {
            if(res.variableType.descriptor.cols == 1 && res.variableType.descriptor.rows == 1 &&
               res.variableType.descriptor.type == VarType::UInt)
            {
              if(rs.AtomicCounter[bind].res.name)
                m_ResourceUses[rm->GetID(rs.AtomicCounter[bind].res)].push_back(rw);
            }
            else
            {
              if(rs.ShaderStorage[bind].res.name)
                m_ResourceUses[rm->GetID(rs.ShaderStorage[bind].res)].push_back(rw);
            }
          }
        }

        for(const ShaderResource &res : refl[i]->ReadOnlyResources)
        {
          int32_t bind = mapping[i].ReadOnlyResources[res.bindPoint].bind;

          GLResource *texList = NULL;
          const int32_t listSize = (int32_t)ARRAY_COUNT(rs.Tex2D);
          ;

          switch(res.resType)
          {
            case TextureDim::Unknown: texList = NULL; break;
            case TextureDim::Buffer: texList = rs.TexBuffer; break;
            case TextureDim::Texture1D: texList = rs.Tex1D; break;
            case TextureDim::Texture1DArray: texList = rs.Tex1DArray; break;
            case TextureDim::Texture2D: texList = rs.Tex2D; break;
            case TextureDim::TextureRect: texList = rs.TexRect; break;
            case TextureDim::Texture2DArray: texList = rs.Tex2DArray; break;
            case TextureDim::Texture2DMS: texList = rs.Tex2DMS; break;
            case TextureDim::Texture2DMSArray: texList = rs.Tex2DMSArray; break;
            case TextureDim::Texture3D: texList = rs.Tex3D; break;
            case TextureDim::TextureCube: texList = rs.TexCube; break;
            case TextureDim::TextureCubeArray: texList = rs.TexCubeArray; break;
            case TextureDim::Count: RDCERR("Invalid shader resource type"); break;
          }

          if(texList != NULL && bind >= 0 && bind < listSize && texList[bind].name != 0)
            m_ResourceUses[rm->GetID(texList[bind])].push_back(ro);
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
    m_DrawcallStack.back()->children.push_back(draw);
  else
    RDCERR("Somehow lost drawcall stack!");
}

void WrappedOpenGL::AddEvent()
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventID = m_CurEventID;

  apievent.chunkIndex = uint32_t(m_StructuredFile->chunks.size() - 1);

  apievent.callstack = m_ChunkMetadata.callstack;

  m_CurEvents.push_back(apievent);

  if(IsLoading(m_State))
    m_Events.push_back(apievent);
}

const APIEvent &WrappedOpenGL::GetEvent(uint32_t eventID)
{
  for(const APIEvent &e : m_Events)
  {
    if(e.eventID >= eventID)
      return e;
  }

  return m_Events.back();
}

const DrawcallDescription *WrappedOpenGL::GetDrawcall(uint32_t eventID)
{
  if(eventID >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventID];
}

void WrappedOpenGL::ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
  bool partial = true;

  if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
  {
    startEventID = 1;
    partial = false;
  }

  if(!partial)
  {
    GLMarkerRegion apply("!!!!RenderDoc Internal: ApplyInitialContents");
    GetResourceManager()->ApplyInitialContents();
    GetResourceManager()->ReleaseInFrameResources();
  }

  m_State = CaptureState::ActiveReplaying;

  GLMarkerRegion::Set(StringFormat::Fmt("!!!!RenderDoc Internal:  Replay %d (%d): %u->%u",
                                        (int)replayType, (int)partial, startEventID, endEventID));

  m_ReplayEventCount = 0;

  if(replayType == eReplay_Full)
    ContextReplayLog(m_State, startEventID, endEventID, partial);
  else if(replayType == eReplay_WithoutDraw)
    ContextReplayLog(m_State, startEventID, RDCMAX(1U, endEventID) - 1, partial);
  else if(replayType == eReplay_OnlyDraw)
    ContextReplayLog(m_State, endEventID, endEventID, partial);
  else
    RDCFATAL("Unexpected replay type");

  // make sure to end any unbalanced replay events if we stopped in the middle of a frame
  for(int i = 0; i < m_ReplayEventCount; i++)
    GLMarkerRegion::End();

  GLMarkerRegion::Set("!!!!RenderDoc Internal: Done replay");
}
