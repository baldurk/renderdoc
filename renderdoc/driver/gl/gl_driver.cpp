/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "driver/shaders/spirv/spirv_compile.h"
#include "jpeg-compressor/jpge.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "gl_replay.h"

std::map<uint64_t, GLWindowingData> WrappedOpenGL::m_ActiveContexts;

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
  m_GLExtensions.push_back("GL_ARB_ES3_2_compatibility");
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
  m_GLExtensions.push_back("GL_ARB_gl_spirv");
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
  m_GLExtensions.push_back("GL_ARB_parallel_shader_compile");
  m_GLExtensions.push_back("GL_ARB_pixel_buffer_object");
  m_GLExtensions.push_back("GL_ARB_pipeline_statistics_query");
  m_GLExtensions.push_back("GL_ARB_point_parameters");
  m_GLExtensions.push_back("GL_ARB_point_sprite");
  m_GLExtensions.push_back("GL_ARB_polygon_offset_clamp");
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
  m_GLExtensions.push_back("GL_ARB_spirv_extensions");
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
  m_GLExtensions.push_back("GL_ARB_texture_filter_anisotropic");
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
  m_GLExtensions.push_back("GL_EXT_memory_object");
  m_GLExtensions.push_back("GL_EXT_memory_object_fd");
  m_GLExtensions.push_back("GL_EXT_memory_object_win32");
  m_GLExtensions.push_back("GL_EXT_multisample");
  m_GLExtensions.push_back("GL_EXT_multi_draw_arrays");
  m_GLExtensions.push_back("GL_EXT_packed_depth_stencil");
  m_GLExtensions.push_back("GL_EXT_packed_float");
  m_GLExtensions.push_back("GL_EXT_pixel_buffer_object");
  m_GLExtensions.push_back("GL_EXT_point_parameters");
  m_GLExtensions.push_back("GL_EXT_polygon_offset");
  m_GLExtensions.push_back("GL_EXT_polygon_offset_clamp");
  m_GLExtensions.push_back("GL_EXT_post_depth_coverage");
  m_GLExtensions.push_back("GL_EXT_provoking_vertex");
  m_GLExtensions.push_back("GL_EXT_raster_multisample");
  m_GLExtensions.push_back("GL_EXT_semaphore");
  m_GLExtensions.push_back("GL_EXT_semaphore_fd");
  m_GLExtensions.push_back("GL_EXT_semaphore_win32");
  m_GLExtensions.push_back("GL_EXT_shader_framebuffer_fetch");
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
  m_GLExtensions.push_back("GL_EXT_texture_shadow_lod");
  m_GLExtensions.push_back("GL_EXT_texture_shared_exponent");
  m_GLExtensions.push_back("GL_EXT_texture_snorm");
  m_GLExtensions.push_back("GL_EXT_texture_sRGB");
  m_GLExtensions.push_back("GL_EXT_texture_sRGB_decode");
  m_GLExtensions.push_back("GL_EXT_texture_sRGB_R8");
  m_GLExtensions.push_back("GL_EXT_texture_swizzle");
  m_GLExtensions.push_back("GL_EXT_texture3D");
  m_GLExtensions.push_back("GL_EXT_timer_query");
  m_GLExtensions.push_back("GL_EXT_transform_feedback");
  m_GLExtensions.push_back("GL_EXT_vertex_attrib_64bit");
  m_GLExtensions.push_back("GL_EXT_win32_keyed_mutex");
  m_GLExtensions.push_back("GL_GREMEDY_frame_terminator");
  m_GLExtensions.push_back("GL_GREMEDY_string_marker");
  m_GLExtensions.push_back("GL_KHR_blend_equation_advanced");
  m_GLExtensions.push_back("GL_KHR_blend_equation_advanced_coherent");
  m_GLExtensions.push_back("GL_KHR_context_flush_control");
  m_GLExtensions.push_back("GL_KHR_debug");
  m_GLExtensions.push_back("GL_KHR_no_error");
  m_GLExtensions.push_back("GL_KHR_parallel_shader_compile");
  m_GLExtensions.push_back("GL_KHR_robustness");
  m_GLExtensions.push_back("GL_KHR_robust_buffer_access_behavior");
  m_GLExtensions.push_back("GL_OVR_multiview");
  m_GLExtensions.push_back("GL_OVR_multiview2");
  m_GLExtensions.push_back("GL_OVR_multiview_multisampled_render_to_texture");

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
                                           support if it's supported on replaying driver'
  * GL_KHR_texture_compression_astc_sliced_3d
  * GL_ARB_gpu_shader_int64
  * GL_ARB_sample_locations
  * GL_ARB_texture_filter_minmax
  * GL_EXT_EGL_image_storage
  * GL_EXT_external_buffer
  * GL_EXT_window_rectangles
  * GL_EXT_texture_sRGB_R8
  * GL_EXT_shader_framebuffer_fetch
  * GL_EXT_shader_framebuffer_fetch_non_coherent
  * GL_EXT_multiview_timer_query
  * GL_EXT_multiview_texture_multisample
  * GL_EXT_multiview_tessellation_geometry_shader

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
  * GL_ARB_vertex_shader
  * GL_ARB_window_pos
  * GL_ATI_draw_buffers
  * GL_ATI_texture_float
  * GL_ATI_texture_mirror_once
  * GL_EXT_422_pixels
  * GL_EXT_abgr
  * GL_EXT_bindable_uniform
  * GL_EXT_blend_logic_op
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
  m_GLESExtensions.push_back("GL_EXT_blend_func_extended");
  m_GLESExtensions.push_back("GL_EXT_blend_minmax");
  m_GLESExtensions.push_back("GL_EXT_buffer_storage");
  m_GLESExtensions.push_back("GL_EXT_clear_texture");
  m_GLESExtensions.push_back("GL_EXT_clip_control");
  m_GLESExtensions.push_back("GL_EXT_clip_cull_distance");
  m_GLESExtensions.push_back("GL_EXT_color_buffer_float");
  m_GLESExtensions.push_back("GL_EXT_color_buffer_half_float");
  m_GLESExtensions.push_back("GL_EXT_conservative_depth");
  m_GLESExtensions.push_back("GL_EXT_copy_image");
  m_GLESExtensions.push_back("GL_EXT_debug_label");
  m_GLESExtensions.push_back("GL_EXT_debug_marker");
  m_GLESExtensions.push_back("GL_EXT_depth_clamp");
  m_GLESExtensions.push_back("GL_EXT_discard_framebuffer");
  m_GLESExtensions.push_back("GL_EXT_disjoint_timer_query");
  m_GLESExtensions.push_back("GL_EXT_draw_buffers");
  m_GLESExtensions.push_back("GL_EXT_draw_buffers_indexed");
  m_GLESExtensions.push_back("GL_EXT_draw_elements_base_vertex");
  m_GLESExtensions.push_back("GL_EXT_draw_instanced");
  m_GLESExtensions.push_back("GL_EXT_draw_transform_feedback");
  m_GLESExtensions.push_back("GL_EXT_float_blend");
  m_GLESExtensions.push_back("GL_EXT_frag_depth");
  m_GLESExtensions.push_back("GL_EXT_geometry_point_size");
  m_GLESExtensions.push_back("GL_EXT_geometry_shader");
  m_GLESExtensions.push_back("GL_EXT_gpu_shader5");
  m_GLESExtensions.push_back("GL_EXT_instanced_arrays");
  m_GLESExtensions.push_back("GL_EXT_map_buffer_range");
  m_GLESExtensions.push_back("GL_EXT_memory_object");
  m_GLESExtensions.push_back("GL_EXT_memory_object_fd");
  m_GLESExtensions.push_back("GL_EXT_memory_object_win32");
  m_GLESExtensions.push_back("GL_EXT_multisampled_render_to_texture");
  m_GLESExtensions.push_back("GL_EXT_multi_draw_arrays");
  m_GLESExtensions.push_back("GL_EXT_multi_draw_indirect");
  m_GLESExtensions.push_back("GL_EXT_multisample_compatibility");
  m_GLESExtensions.push_back("GL_EXT_multisampled_render_to_texture2");
  m_GLESExtensions.push_back("GL_EXT_occlusion_query_boolean");
  m_GLESExtensions.push_back("GL_EXT_polygon_offset_clamp");
  m_GLESExtensions.push_back("GL_EXT_post_depth_coverage");
  m_GLESExtensions.push_back("GL_EXT_primitive_bounding_box");
  m_GLESExtensions.push_back("GL_EXT_pvrtc_sRGB");
  m_GLESExtensions.push_back("GL_EXT_raster_multisample");
  m_GLESExtensions.push_back("GL_EXT_render_snorm");
  m_GLESExtensions.push_back("GL_EXT_robustness");
  m_GLESExtensions.push_back("GL_EXT_semaphore");
  m_GLESExtensions.push_back("GL_EXT_semaphore_fd");
  m_GLESExtensions.push_back("GL_EXT_semaphore_win32");
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
  m_GLESExtensions.push_back("GL_EXT_texture_compression_bptc");
  m_GLESExtensions.push_back("GL_EXT_texture_compression_dxt1");
  m_GLESExtensions.push_back("GL_EXT_texture_compression_rgtc");
  m_GLESExtensions.push_back("GL_EXT_texture_compression_s3tc");
  m_GLESExtensions.push_back("GL_EXT_texture_compression_s3tc_srgb");
  m_GLESExtensions.push_back("GL_EXT_texture_cube_map_array");
  m_GLESExtensions.push_back("GL_EXT_texture_filter_anisotropic");
  m_GLESExtensions.push_back("GL_EXT_texture_filter_minmax");
  m_GLESExtensions.push_back("GL_EXT_texture_format_BGRA8888");
  m_GLESExtensions.push_back("GL_EXT_texture_lod_bias");
  m_GLESExtensions.push_back("GL_EXT_texture_mirror_clamp_to_edge");
  m_GLESExtensions.push_back("GL_EXT_texture_norm16");
  m_GLESExtensions.push_back("GL_EXT_texture_query_lod");
  m_GLESExtensions.push_back("GL_EXT_texture_rg");
  m_GLESExtensions.push_back("GL_EXT_texture_shadow_lod");
  m_GLESExtensions.push_back("GL_EXT_texture_sRGB_decode");
  m_GLESExtensions.push_back("GL_EXT_texture_sRGB_R8");
  m_GLESExtensions.push_back("GL_EXT_texture_sRGB_RG8");
  m_GLESExtensions.push_back("GL_EXT_texture_storage");
  m_GLESExtensions.push_back("GL_EXT_texture_type_2_10_10_10_REV");
  m_GLESExtensions.push_back("GL_EXT_texture_view");
  m_GLESExtensions.push_back("GL_EXT_win32_keyed_mutex");
  m_GLESExtensions.push_back("GL_KHR_blend_equation_advanced");
  m_GLESExtensions.push_back("GL_KHR_blend_equation_advanced_coherent");
  m_GLESExtensions.push_back("GL_KHR_context_flush_control");
  m_GLESExtensions.push_back("GL_KHR_debug");
  m_GLESExtensions.push_back("GL_KHR_no_error");
  m_GLESExtensions.push_back("GL_KHR_parallel_shader_compile");
  m_GLESExtensions.push_back("GL_KHR_robustness");
  m_GLESExtensions.push_back("GL_KHR_robust_buffer_access_behavior");
  m_GLESExtensions.push_back("GL_KHR_texture_compression_astc_hdr");
  m_GLESExtensions.push_back("GL_KHR_texture_compression_astc_ldr");
  m_GLESExtensions.push_back("GL_KHR_texture_compression_astc_sliced_3d");
  m_GLESExtensions.push_back("GL_NV_viewport_array");
  m_GLESExtensions.push_back("GL_OES_blend_equation_separate");
  m_GLESExtensions.push_back("GL_OES_blend_func_separate");
  m_GLESExtensions.push_back("GL_OES_blend_subtract");
  m_GLESExtensions.push_back("GL_OES_compressed_ETC1_RGB8_texture");
  m_GLESExtensions.push_back("GL_OES_copy_image");
  m_GLESExtensions.push_back("GL_OES_depth24");
  m_GLESExtensions.push_back("GL_OES_depth32");
  m_GLESExtensions.push_back("GL_OES_depth_texture");
  m_GLESExtensions.push_back("GL_OES_depth_texture_cube_map");
  m_GLESExtensions.push_back("GL_OES_draw_buffers_indexed");
  m_GLESExtensions.push_back("GL_OES_draw_elements_base_vertex");
  m_GLESExtensions.push_back("GL_OES_element_index_uint");
  m_GLESExtensions.push_back("GL_OES_fbo_render_mipmap");
  m_GLESExtensions.push_back("GL_OES_framebuffer_object");
  m_GLESExtensions.push_back("GL_OES_geometry_shader");
  m_GLESExtensions.push_back("GL_OES_gpu_shader5");
  m_GLESExtensions.push_back("GL_OES_mapbuffer");
  m_GLESExtensions.push_back("GL_OES_packed_depth_stencil");
  m_GLESExtensions.push_back("GL_OES_primitive_bounding_box");
  m_GLESExtensions.push_back("GL_OES_rgb8_rgba8");
  m_GLESExtensions.push_back("GL_OES_sample_shading");
  m_GLESExtensions.push_back("GL_OES_standard_derivatives");
  m_GLESExtensions.push_back("GL_OES_surfaceless_context");
  m_GLESExtensions.push_back("GL_OES_tessellation_shader");
  m_GLESExtensions.push_back("GL_OES_texture_3D");
  m_GLESExtensions.push_back("GL_OES_texture_border_clamp");
  m_GLESExtensions.push_back("GL_OES_texture_buffer");
  m_GLESExtensions.push_back("GL_OES_texture_compression_astc");
  m_GLESExtensions.push_back("GL_OES_texture_cube_map");
  m_GLESExtensions.push_back("GL_OES_texture_cube_map_array");
  m_GLESExtensions.push_back("GL_OES_texture_float");
  m_GLESExtensions.push_back("GL_OES_texture_float_linear");
  m_GLESExtensions.push_back("GL_OES_texture_half_float");
  m_GLESExtensions.push_back("GL_OES_texture_half_float_linear");
  m_GLESExtensions.push_back("GL_OES_texture_mirrored_repeat");
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
  m_GLESExtensions.push_back("GL_QCOM_texture_foveated");

  // advertise EGL extensions in the gl ext string, just in case anyone is checking it for
  // this way.
  m_GLESExtensions.push_back("EGL_KHR_create_context");
  m_GLESExtensions.push_back("EGL_KHR_surfaceless_context");

  // we'll be sorting the implementation extension array, so make sure the
  // sorts are identical so we can do the intersection easily
  std::sort(m_GLESExtensions.begin(), m_GLESExtensions.end());

  /***********************************************************************

  Unsorted GLES extensions that are not yet supported. Nothing here says whether it's
  possible to support, will never be supported, or unlikely, etc.

  As above - only OES, KHR, and EXT extensions listed

  * GL_EXT_compressed_ETC1_RGB8_sub_texture
  * GL_EXT_EGL_image_array
  * GL_EXT_EGL_image_external_wrap_modes
  * GL_EXT_EGL_image_storage
  * GL_EXT_external_buffer
  * GL_EXT_multiview_draw_buffers
  * GL_EXT_multiview_tessellation_geometry_shader
  * GL_EXT_multiview_texture_multisample
  * GL_EXT_multiview_timer_query
  * GL_EXT_protected_textures
  * GL_EXT_read_format_bgra
  * GL_EXT_shader_framebuffer_fetch_non_coherent
  * GL_EXT_shader_pixel_local_storage
  * GL_EXT_shader_pixel_local_storage2
  * GL_EXT_sparse_texture
  * GL_EXT_sparse_texture2
  * GL_EXT_tessellation_point_size
  * GL_EXT_texture_compression_astc_decode_mode_rgb9e5
  * GL_EXT_texture_format_sRGB_override
  * GL_EXT_unpack_subimage
  * GL_EXT_window_rectangles
  * GL_EXT_YUV_target
  * GL_OES_byte_coordinates
  * GL_OES_compressed_paletted_texture
  * GL_OES_draw_texture
  * GL_OES_EGL_image
  * GL_OES_EGL_image_external
  * GL_OES_EGL_image_external_essl3
  * GL_OES_EGL_sync
  * GL_OES_extended_matrix_palette
  * GL_OES_fixed_point
  * GL_OES_fragment_precision_high
  * GL_OES_get_program_binary
  * GL_OES_matrix_get
  * GL_OES_matrix_palette
  * GL_OES_point_size_array
  * GL_OES_point_sprite
  * GL_OES_query_matrix
  * GL_OES_read_format
  * GL_OES_required_internalformat
  * GL_OES_sample_variables
  * GL_OES_shader_image_atomic
  * GL_OES_shader_io_blocks
  * GL_OES_shader_multisample_interpolation
  * GL_OES_single_precision
  * GL_OES_stencil_wrap
  * GL_OES_stencil1
  * GL_OES_stencil4
  * GL_OES_stencil8
  * GL_OES_texture_env_crossbar
  * GL_OES_vertex_type_10_10_10_2

  ************************************************************************/
}

WrappedOpenGL::WrappedOpenGL(GLPlatform &platform)
    : m_Platform(platform), m_ScratchSerialiser(new StreamWriter(1024), Ownership::Stream)
{
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(WrappedOpenGL));

  BuildGLExtensions();
  BuildGLESExtensions();
  // by default we assume OpenGL driver
  m_DriverType = RDCDriver::OpenGL;

  m_Replay = new GLReplay(this);

  m_StructuredFile = m_StoredStructuredData = new SDFile;

  uint32_t flags = WriteSerialiser::ChunkDuration | WriteSerialiser::ChunkTimestamp |
                   WriteSerialiser::ChunkThreadID;

  if(RenderDoc::Inst().GetCaptureOptions().captureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  m_ScratchSerialiser.SetChunkMetadataRecording(flags);
  m_ScratchSerialiser.SetVersion(GLInitParams::CurrentVersion);

  m_SectionVersion = GLInitParams::CurrentVersion;

  m_NoCtxFrames = 0;
  m_FailedFrame = 0;
  m_FailedReason = CaptureSucceeded;
  m_Failures = 0;
  m_SuccessfulCapture = true;
  m_FailureReason = CaptureSucceeded;

  m_UsesVRMarkers = false;

  m_SuppressDebugMessages = false;

  m_ActionStack.push_back(&m_ParentAction);

  m_CurEventID = 0;
  m_CurActionID = 0;
  m_FirstEventID = 0;
  m_LastEventID = ~0U;

  m_FetchCounters = false;

  RDCEraseEl(m_ActiveQueries);
  m_ActiveConditional = false;
  m_ActiveFeedback = false;

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = CaptureState::LoadingReplaying;
  }
  else
  {
    m_State = CaptureState::BackgroundCapturing;
  }

  m_DeviceRecord = NULL;

  m_ResourceManager = new GLResourceManager(m_State, this);

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
    m_DeviceRecord->InternalResource = true;

    m_ContextRecord = GetResourceManager()->AddResourceRecord(m_ContextResourceID);
    m_ContextRecord->DataInSerialiser = false;
    m_ContextRecord->Length = 0;
    m_ContextRecord->InternalResource = true;
  }
  else
  {
    m_DeviceRecord = m_ContextRecord = NULL;

    ResourceIDGen::SetReplayResourceIDs();

    m_DescriptorsID = GetResourceManager()->RegisterResource(
        GLResource(NULL, eResSpecial, eSpecialResDescriptorStorage));

    GetResourceManager()->AddLiveResource(
        m_DescriptorsID, GLResource(NULL, eResSpecial, eSpecialResDescriptorStorage));

    AddResource(m_DescriptorsID, ResourceType::DescriptorStore, "");
    GetReplay()->GetResourceDesc(m_DescriptorsID).SetCustomName("Context Bindings");
    GetReplay()->GetResourceDesc(m_DescriptorsID).initialisationChunks.clear();
  }

  rdcspv::Init();
  RenderDoc::Inst().RegisterShutdownFunction(&rdcspv::Shutdown);

  m_CurrentDefaultFBO = 0;

  m_CurChunkOffset = 0;
  m_AddedAction = false;

  m_CurCtxDataTLS = Threading::AllocateTLSSlot();
}

void WrappedOpenGL::Initialise(GLInitParams &params, uint64_t sectionVersion,
                               const ReplayOptions &opts)
{
  m_SectionVersion = sectionVersion;
  m_GlobalInitParams = params;
  m_ReplayOptions = opts;

  m_ArrayMS.Create();
}

void WrappedOpenGL::MarkReferencedWhileCapturing(GLResourceRecord *record, FrameRefType refType)
{
  if(!record || !IsCaptureMode(m_State))
    return;

  GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), refType);
}

void WrappedOpenGL::CreateReplayBackbuffer(const GLInitParams &params, ResourceId fboOrigId,
                                           GLuint &fbo, rdcstr bbname)
{
  GLuint col = 0, depth = 0;

  WrappedOpenGL &drv = *this;

  GLuint unpackbuf = 0;
  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
  GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

  drv.glGenFramebuffers(1, &fbo);
  drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);

  m_CurrentDefaultFBO = fbo;

  GLenum colfmt = eGL_RGBA8;

  if(params.colorBits == 64)
  {
    colfmt = eGL_RGBA16F;
  }
  else if(params.colorBits == 32)
  {
    colfmt = params.isSRGB ? eGL_SRGB8_ALPHA8 : eGL_RGBA8;
  }
  else if(params.colorBits == 24)
  {
    colfmt = params.isSRGB ? eGL_SRGB8 : eGL_RGB8;
  }
  else if(params.colorBits == 16)
  {
    RDCASSERT(!params.isSRGB);
    // 5:6:5 is almost certainly not used in desktop GL as a backbuffer format, and is only required
    // to be supported from 4.2 onwards, so only replicate it on a GLES capture.
    if(IsGLES)
      colfmt = eGL_RGB565;
    else
      colfmt = eGL_RGB8;
  }
  else if(params.colorBits == 10)
  {
    colfmt = eGL_RGB10_A2;
  }
  else
  {
    RDCERR("Unexpected # colour bits: %d", params.colorBits);
  }

  GLenum target = eGL_TEXTURE_2D;
  if(params.multiSamples > 1)
    target = eGL_TEXTURE_2D_MULTISAMPLE;

  drv.glGenTextures(1, &col);
  drv.glBindTexture(target, col);

  m_Textures[GetResourceManager()->GetResID(TextureRes(GetCtx(), col))].creationFlags |=
      TextureCategory::SwapBuffer;

  uint32_t width = RDCMAX(1U, params.width);
  uint32_t height = RDCMAX(1U, params.height);

  if(params.multiSamples > 1)
  {
    drv.glTextureStorage2DMultisampleEXT(col, target, params.multiSamples, colfmt, width, height,
                                         true);
  }
  else
  {
    drv.glTextureImage2DEXT(col, target, 0, colfmt, width, height, 0, GetBaseFormat(colfmt),
                            GetDataType(colfmt), NULL);
    drv.glTextureParameteriEXT(col, target, eGL_TEXTURE_MAX_LEVEL, 0);
    drv.glTextureParameteriEXT(col, target, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    drv.glTextureParameteriEXT(col, target, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    drv.glTextureParameteriEXT(col, target, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    drv.glTextureParameteriEXT(col, target, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  }
  drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, target, col, 0);

  drv.glViewport(0, 0, width, height);

  if(params.depthBits > 0 || params.stencilBits > 0)
  {
    drv.glGenTextures(1, &depth);
    drv.glBindTexture(target, depth);

    GLenum depthfmt = eGL_DEPTH32F_STENCIL8;
    bool stencil = false;

    if(params.stencilBits == 8)
    {
      stencil = true;

      if(params.depthBits == 32)
        depthfmt = eGL_DEPTH32F_STENCIL8;
      else if(params.depthBits == 24)
        depthfmt = eGL_DEPTH24_STENCIL8;
      else if(params.depthBits == 0)
        depthfmt = eGL_STENCIL_INDEX8;
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

    m_Textures[GetResourceManager()->GetResID(TextureRes(GetCtx(), depth))].creationFlags |=
        TextureCategory::SwapBuffer;

    if(params.multiSamples > 1)
    {
      drv.glTextureStorage2DMultisampleEXT(depth, target, params.multiSamples, depthfmt, width,
                                           height, true);
    }
    else
    {
      drv.glTextureParameteriEXT(depth, target, eGL_TEXTURE_MAX_LEVEL, 0);
      drv.glTextureImage2DEXT(depth, target, 0, depthfmt, width, height, 0, GetBaseFormat(depthfmt),
                              GetDataType(depthfmt), NULL);
    }

    if(stencil && params.depthBits == 0)
      drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, target, depth, 0);
    else if(stencil)
      drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, target, depth, 0);
    else
      drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, target, depth, 0);
  }

  // give the backbuffer a default clear color
  float clearcol[] = {0.0f, 0.0f, 0.0f, 1.0f};
  drv.glClearBufferfv(eGL_COLOR, 0, clearcol);

  if(params.depthBits > 0 || params.stencilBits > 0)
    drv.glClearBufferfi(eGL_DEPTH_STENCIL, 0, 1.0f, 0);

  GetResourceManager()->AddLiveResource(fboOrigId, FramebufferRes(GetCtx(), fbo));
  AddResource(fboOrigId, ResourceType::SwapchainImage, "");
  GetReplay()->GetResourceDesc(fboOrigId).SetCustomName(bbname + " FBO");

  ResourceId colorId = GetResourceManager()->GetResID(TextureRes(GetCtx(), col));
  rdcstr name = bbname + " Color";

  GetResourceManager()->SetName(colorId, name);

  // we'll add the chunk later when we re-process it.
  AddResource(colorId, ResourceType::SwapchainImage, name.c_str());
  GetReplay()->GetResourceDesc(colorId).SetCustomName(name);

  GetReplay()->GetResourceDesc(fboOrigId).derivedResources.push_back(colorId);
  GetReplay()->GetResourceDesc(colorId).parentResources.push_back(fboOrigId);

  if(depth)
  {
    ResourceId depthId = GetResourceManager()->GetResID(TextureRes(GetCtx(), depth));
    name = bbname + (params.stencilBits > 0 ? " Depth-stencil" : " Depth");

    GetResourceManager()->SetName(depthId, name);

    // we'll add the chunk later when we re-process it.
    AddResource(depthId, ResourceType::SwapchainImage, name.c_str());
    GetReplay()->GetResourceDesc(depthId).SetCustomName(name);

    GetReplay()->GetResourceDesc(fboOrigId).derivedResources.push_back(depthId);
    GetReplay()->GetResourceDesc(depthId).parentResources.push_back(fboOrigId);
  }

  if(fbo == m_Global_FBO0)
  {
    GetReplay()->GetResourceDesc(fboOrigId).initialisationChunks.clear();
    GetReplay()->GetResourceDesc(fboOrigId).initialisationChunks.push_back(m_InitChunkIndex);

    GetReplay()->GetResourceDesc(colorId).initialisationChunks.clear();
    GetReplay()->GetResourceDesc(colorId).initialisationChunks.push_back(m_InitChunkIndex);

    if(depth)
    {
      ResourceId depthId = GetResourceManager()->GetResID(TextureRes(GetCtx(), depth));

      GetReplay()->GetResourceDesc(depthId).initialisationChunks.clear();
      GetReplay()->GetResourceDesc(depthId).initialisationChunks.push_back(m_InitChunkIndex);
    }
  }

  GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
}

rdcstr WrappedOpenGL::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((GLChunk)idx);
}

WrappedOpenGL::~WrappedOpenGL()
{
  if(m_IndirectBuffer)
    GL.glDeleteBuffers(1, &m_IndirectBuffer);

  m_ArrayMS.Destroy();

  SAFE_DELETE(m_FrameReader);

  SAFE_DELETE(m_StoredStructuredData);

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->ReleaseCurrentResource(m_DeviceResourceID);

  for(auto it = m_ContextData.begin(); it != m_ContextData.end(); ++it)
  {
    if(it->second.m_ContextDataRecord)
    {
      RDCASSERT(it->second.m_ContextDataRecord->GetRefCount() == 1);
      it->second.m_ContextDataRecord->Delete(GetResourceManager());
      GetResourceManager()->ReleaseCurrentResource(it->second.m_ContextDataResourceID);
    }
  }

  if(m_ContextRecord)
  {
    RDCASSERT(m_ContextRecord->GetRefCount() == 1);
    m_ContextRecord->Delete(GetResourceManager());
  }
  GetResourceManager()->ReleaseCurrentResource(m_ContextResourceID);

  if(m_DeviceRecord)
  {
    RDCASSERT(m_DeviceRecord->GetRefCount() == 1);
    m_DeviceRecord->Delete(GetResourceManager());
  }

  m_ResourceManager->Shutdown();

  SAFE_DELETE(m_ResourceManager);

  for(size_t i = 0; i < m_CtxDataVector.size(); i++)
    delete m_CtxDataVector[i];

  RenderDoc::Inst().UnregisterMemoryRegion(this);

  delete m_Replay;
}

void WrappedOpenGL::SetDriverType(RDCDriver type)
{
  m_DriverType = type;
  m_Platform.SetDriverType(m_DriverType);
}

ContextPair &WrappedOpenGL::GetCtx()
{
  GLContextTLSData *ret = (GLContextTLSData *)Threading::GetTLSValue(m_CurCtxDataTLS);
  if(ret)
    return ret->ctxPair;
  return m_EmptyTLSData.ctxPair;
}

GLResourceRecord *WrappedOpenGL::GetContextRecord()
{
  GLContextTLSData *ret = (GLContextTLSData *)Threading::GetTLSValue(m_CurCtxDataTLS);
  if(ret && ret->ctxRecord)
  {
    return ret->ctxRecord;
  }
  else
  {
    ContextData &dat = GetCtxData();
    dat.CreateResourceRecord(this, GetCtx().ctx);
    return dat.m_ContextDataRecord;
  }
}

void WrappedOpenGL::UseUnusedSupportedFunction(const char *name)
{
  // if this is the first time an unused function is called, remove all frame capturers immediately
  if(m_UnsupportedFunctions.empty())
  {
    for(auto it = m_ContextData.begin(); it != m_ContextData.end(); ++it)
    {
      if(it->second.Modern())
      {
        RenderDoc::Inst().RemoveDeviceFrameCapturer(it->second.ctx);
        for(auto wnd = it->second.windows.begin(); wnd != it->second.windows.end();)
        {
          void *wndHandle = wnd->first;
          wnd++;
          it->second.UnassociateWindow(this, wndHandle);
        }
      }
    }
  }

  size_t sz = m_UnsupportedFunctions.size();
  m_UnsupportedFunctions.insert(name);

  if(sz != m_UnsupportedFunctions.size())
  {
    RDCERR("Unsupported function %s used", name);

    rdcstr unsupportedStatus = StringFormat::Fmt(
        "Unsupported %s used:\n", m_UnsupportedFunctions.size() == 1 ? "function" : "functions");
    size_t i = 0;
    for(const char *func : m_UnsupportedFunctions)
    {
      i++;
      if(i > 4)
        break;
      unsupportedStatus += StringFormat::Fmt(" - %s\n", func);
    }
    if(m_UnsupportedFunctions.size() > i)
      unsupportedStatus += " - ...\n";

    RenderDoc::Inst().SetDriverUnsupportedMessage(RDCDriver::OpenGL, unsupportedStatus);
  }
}

void WrappedOpenGL::CheckImplicitThread()
{
  void *ctx = GetCtx().ctx;

  if(m_LastCtx != ctx)
  {
    m_LastCtx = ctx;

    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::ImplicitThreadSwitch);
      Serialise_ContextConfiguration(ser, m_LastCtx);
      Serialise_BeginCaptureFrame(ser);
      GetContextRecord()->AddChunk(scope.Get());
    }

    CheckQueuedInitialFetches(m_LastCtx);
  }
}

WrappedOpenGL::ContextData &WrappedOpenGL::GetCtxData()
{
  return m_ContextData[GetCtx().ctx];
}

////////////////////////////////////////////////////////////////
// Windowing/setup/etc
////////////////////////////////////////////////////////////////

void WrappedOpenGL::DeleteContext(void *contextHandle)
{
  ContextData &ctxdata = m_ContextData[contextHandle];

  RDCLOG("Deleting context %p", contextHandle);

  if(ctxdata.Modern())
    RenderDoc::Inst().RemoveDeviceFrameCapturer(ctxdata.ctx);

  // delete the context
  GetResourceManager()->DeleteContext(contextHandle);

  bool lastInGroup = true;
  for(auto it = m_ContextData.begin(); it != m_ContextData.end(); ++it)
  {
    // if we find another context that's not this one, but is in the same share group, we're note
    // the last
    if(it->second.shareGroup == ctxdata.shareGroup && it->second.ctx &&
       it->second.ctx != contextHandle)
    {
      lastInGroup = false;
      break;
    }
  }

  // if this is the last context in the share group, delete the group.
  if(lastInGroup)
  {
    RDCLOG("Deleting shader group %p", ctxdata.shareGroup);
    delete ctxdata.shareGroup;
  }

  if(ctxdata.built && ctxdata.ready)
  {
    ctxdata.ArrayMS.Destroy();
    if(ctxdata.Program)
      GL.glDeleteProgram(ctxdata.Program);
    if(ctxdata.ArrayBuffer)
      GL.glDeleteBuffers(1, &ctxdata.ArrayBuffer);
    if(ctxdata.GlyphTexture)
      GL.glDeleteTextures(1, &ctxdata.GlyphTexture);
  }

  if(ctxdata.m_ClientMemoryVBOs[0])
    glDeleteBuffers(ARRAY_COUNT(ctxdata.m_ClientMemoryVBOs), ctxdata.m_ClientMemoryVBOs);
  if(ctxdata.m_ClientMemoryIBO)
    glDeleteBuffers(1, &ctxdata.m_ClientMemoryIBO);

  if(ctxdata.m_ContextDataRecord)
  {
    RDCASSERT(ctxdata.m_ContextDataRecord->GetRefCount() == 1);
    ctxdata.m_ContextDataRecord->Delete(GetResourceManager());
    GetResourceManager()->ReleaseCurrentResource(ctxdata.m_ContextDataResourceID);
    ctxdata.m_ContextDataRecord = NULL;
  }

  m_LastContexts.removeOneIf(
      [contextHandle](const GLWindowingData &ctx) { return ctx.ctx == contextHandle; });

  for(auto it = ctxdata.windows.begin(); it != ctxdata.windows.end();)
  {
    void *wndHandle = it->first;
    it++;

    ctxdata.UnassociateWindow(this, wndHandle);
  }

  m_ContextData.erase(contextHandle);
}

void WrappedOpenGL::ContextData::UnassociateWindow(WrappedOpenGL *driver, void *wndHandle)
{
  auto it = windows.find(wndHandle);
  if(it != windows.end())
  {
    if(it->second.first != WindowingSystem::Headless && IsCaptureMode(driver->GetState()))
      Keyboard::RemoveInputWindow(it->second.first, wndHandle);

    windows.erase(wndHandle);
    RenderDoc::Inst().RemoveFrameCapturer(DeviceOwnedWindow(ctx, wndHandle));
  }
}

void WrappedOpenGL::ContextData::AssociateWindow(WrappedOpenGL *driver, WindowingSystem winSystem,
                                                 void *wndHandle)
{
  auto it = windows.find(wndHandle);
  if(it == windows.end())
  {
    RenderDoc::Inst().AddFrameCapturer(DeviceOwnedWindow(ctx, wndHandle), driver);

    if(winSystem != WindowingSystem::Headless && IsCaptureMode(driver->GetState()))
      Keyboard::AddInputWindow(winSystem, wndHandle);
  }

  windows[wndHandle] = {winSystem, Timing::GetUnixTimestamp()};
}

void WrappedOpenGL::ContextData::CreateResourceRecord(WrappedOpenGL *driver, void *suppliedCtx)
{
  if(m_ContextDataResourceID == ResourceId() ||
     !driver->GetResourceManager()->HasResourceRecord(m_ContextDataResourceID))
  {
    m_ContextDataResourceID = driver->GetResourceManager()->RegisterResource(
        GLResource(suppliedCtx, eResSpecial, eSpecialResContext));

    m_ContextDataRecord = driver->GetResourceManager()->AddResourceRecord(m_ContextDataResourceID);
    m_ContextDataRecord->DataInSerialiser = false;
    m_ContextDataRecord->Length = 0;
    m_ContextDataRecord->InternalResource = true;
  }
}

void WrappedOpenGL::CreateContext(GLWindowingData winData, void *shareContext,
                                  GLInitParams initParams, bool core, bool attribsCreate)
{
  RDCLOG("%s context %p created %s, sharing with context %p", core ? "Core" : "Compatibility",
         winData.ctx, attribsCreate ? "with attribs" : "without attribs", shareContext);

  ContextData &ctxdata = m_ContextData[winData.ctx];
  ctxdata.ctx = winData.ctx;
  ctxdata.isCore = core;
  ctxdata.attribsCreate = attribsCreate;
  ctxdata.initParams = initParams;

  if(shareContext == NULL)
  {
    // no sharing, allocate a new group
    ctxdata.shareGroup = new ContextShareGroup(m_Platform, winData);
    RDCLOG("Created new sharegroup %p", ctxdata.shareGroup);
  }
  else
  {
    // use the same shareGroup ID as the share context.
    ctxdata.shareGroup = GetShareGroup(shareContext);
    RDCLOG("Reusing old sharegroup %p", ctxdata.shareGroup);
  }

  // if the context was created with modern attribs create (whether or not it's explicitly core),
  // and no unsupported functions have been used, we can capture from this context
  if(attribsCreate && m_UnsupportedFunctions.empty())
    RenderDoc::Inst().AddDeviceFrameCapturer(ctxdata.ctx, this);

  // re-configure callstack capture, since WrappedOpenGL constructor may run too early
  uint32_t flags = m_ScratchSerialiser.GetChunkMetadataRecording();

  if(RenderDoc::Inst().GetCaptureOptions().captureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;
  else
    flags &= ~WriteSerialiser::ChunkCallstack;

  m_ScratchSerialiser.SetChunkMetadataRecording(flags);
}

bool WrappedOpenGL::ForceSharedObjects(void *oldContext, void *newContext)
{
  ContextData &olddata = m_ContextData[oldContext];
  ContextData &newdata = m_ContextData[newContext];

  RDCLOG("Forcibly sharing %p with %p", newContext, oldContext);

  if(newdata.built)
  {
    RDCERR("wglShareLists called after wglMakeCurrent - this is not supported and will break.");
    return false;
  }

  newdata.shareGroup = olddata.shareGroup;

  return true;
}

void WrappedOpenGL::RegisterReplayContext(GLWindowingData winData, void *shareContext, bool core,
                                          bool attribsCreate)
{
  ContextData &ctxdata = m_ContextData[winData.ctx];
  ctxdata.ctx = winData.ctx;
  ctxdata.isCore = core;
  ctxdata.attribsCreate = attribsCreate;

  if(shareContext == NULL)
  {
    // create the sharegroup
    ctxdata.shareGroup = new ContextShareGroup(m_Platform, winData);
  }
  else
  {
    // use the same shareGroup ID as the share context.
    ctxdata.shareGroup = GetShareGroup(shareContext);
  }

  ActivateContext(winData);
}

void WrappedOpenGL::UnregisterReplayContext(GLWindowingData windata)
{
  void *contextHandle = windata.ctx;

  ContextData &ctxdata = m_ContextData[contextHandle];

  m_Platform.DeleteReplayContext(windata);

  bool lastInGroup = true;
  for(auto it = m_ContextData.begin(); it != m_ContextData.end(); ++it)
  {
    // if we find another context that's not this one, but is in the same share group, we're note
    // the last
    if(it->second.shareGroup == ctxdata.shareGroup && it->second.ctx &&
       it->second.ctx != contextHandle)
    {
      lastInGroup = false;
      break;
    }
  }

  // if this is the last context in the share group, delete the group.
  if(lastInGroup)
  {
    delete ctxdata.shareGroup;
  }

  m_ContextData.erase(contextHandle);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_ContextConfiguration(SerialiserType &ser, void *ctx)
{
  SERIALISE_ELEMENT_LOCAL(Context, m_ContextData[ctx].m_ContextDataResourceID).Unimportant();
  SERIALISE_ELEMENT_LOCAL(FBO, m_ContextData[ctx].m_ContextFBOID).Unimportant();
  SERIALISE_ELEMENT_LOCAL(InitParams, m_ContextData[ctx].initParams).Unimportant();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && FBO != ResourceId())
  {
    // we might encounter multiple instances of this chunk per frame, so only do work on the first
    // one
    if(!GetResourceManager()->HasLiveResource(FBO))
    {
      rdcstr name;

      // also add a simple resource descriptor for the context
      AddResource(Context, ResourceType::Device, "Context");

      if(m_CurrentDefaultFBO == 0)
      {
        // if we haven't created a default FBO yet this is the first. Give it a nice friendly name
        name = "Backbuffer";
      }
      else
      {
        // if not, we have multiple FBOs and we want to distinguish them. Give the subsequent
        // backbuffers unique names
        name = GetReplay()->GetResourceDesc(Context).name + " Backbuffer";
      }

      GLuint fbo = 0;
      CreateReplayBackbuffer(InitParams, FBO, fbo, name);
    }

    m_CurrentDefaultFBO = GetResourceManager()->GetLiveResource(FBO).name;
  }

  return true;
}

void WrappedOpenGL::ActivateContext(GLWindowingData winData)
{
  m_ActiveContexts[Threading::GetCurrentID()] = winData;

  if(!winData.ctx)
    return;

  void *contextHandle = winData.ctx;
  m_LastContexts.removeOneIf(
      [contextHandle](const GLWindowingData &ctx) { return ctx.ctx == contextHandle; });

  m_LastContexts.push_back(winData);

  if(m_LastContexts.size() > 10)
    m_LastContexts.erase(0);

  CheckQueuedInitialFetches(winData.ctx);

  ContextData &ctxdata = m_ContextData[winData.ctx];

  ctxdata.CreateResourceRecord(this, winData.ctx);

  // update thread-local context pair
  {
    GLContextTLSData *tlsData = (GLContextTLSData *)Threading::GetTLSValue(m_CurCtxDataTLS);

    if(tlsData)
    {
      tlsData->ctxPair = {winData.ctx, GetShareGroup(winData.ctx)};
      tlsData->ctxRecord = ctxdata.m_ContextDataRecord;
    }
    else
    {
      tlsData = new GLContextTLSData(ContextPair({winData.ctx, GetShareGroup(winData.ctx)}),
                                     ctxdata.m_ContextDataRecord);
      m_CtxDataVector.push_back(tlsData);

      Threading::SetTLSValue(m_CurCtxDataTLS, tlsData);
    }
  }

  if(!ctxdata.built)
  {
    ctxdata.built = true;

    if(IsCaptureMode(m_State))
      RDCLOG("Activating new GL context: %s / %s / %s", GL.glGetString(eGL_VENDOR),
             GL.glGetString(eGL_RENDERER), GL.glGetString(eGL_VERSION));

    const rdcarray<rdcstr> &globalExts = IsGLES ? m_GLESExtensions : m_GLExtensions;

    if(HasExt[KHR_debug] && GL.glDebugMessageCallback &&
       RenderDoc::Inst().GetCaptureOptions().apiValidation)
    {
      GL.glDebugMessageCallback(&DebugSnoopStatic, this);
      GL.glEnable(eGL_DEBUG_OUTPUT_SYNCHRONOUS);
    }

    rdcarray<rdcstr> implExts;

    int ctxVersion = 0;
    bool ctxGLES = false;
    GetContextVersion(ctxGLES, ctxVersion);

    // only use glGetStringi on 3.0 contexts and above (ES and GL), even if we have the function
    // pointer
    if(GL.glGetIntegerv && GL.glGetStringi && ctxVersion >= 30)
    {
      GLuint numExts = 0;
      GL.glGetIntegerv(eGL_NUM_EXTENSIONS, (GLint *)&numExts);

      for(GLuint i = 0; i < numExts; i++)
        implExts.push_back((const char *)GL.glGetStringi(eGL_EXTENSIONS, i));
    }
    else if(GL.glGetString)
    {
      rdcstr implExtString = (const char *)GL.glGetString(eGL_EXTENSIONS);

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
        const rdcstr &a = implExts[i];
        const rdcstr &b = globalExts[j];

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

    // similarly we report all the debug extensions so that applications can use them freely - we
    // don't call into the driver so we don't need to care if the driver supports them
    if(!ctxdata.glExts.contains("GL_KHR_debug"))
      ctxdata.glExts.push_back("GL_KHR_debug");

    if(!ctxdata.glExts.contains("GL_EXT_debug_label"))
      ctxdata.glExts.push_back("GL_EXT_debug_label");

    if(!ctxdata.glExts.contains("GL_EXT_debug_marker"))
      ctxdata.glExts.push_back("GL_EXT_debug_marker");

    if(!IsGLES)
    {
      if(!ctxdata.glExts.contains("GL_GREMEDY_frame_terminator"))
        ctxdata.glExts.push_back("GL_GREMEDY_frame_terminator");

      if(!ctxdata.glExts.contains("GL_GREMEDY_string_marker"))
        ctxdata.glExts.push_back("GL_GREMEDY_string_marker");
    }

    merge(ctxdata.glExts, ctxdata.glExtsString, ' ');

    if(GL.glGetIntegerv)
    {
      GLint mj = 0, mn = 0;
      GL.glGetIntegerv(eGL_MAJOR_VERSION, &mj);
      GL.glGetIntegerv(eGL_MINOR_VERSION, &mn);

      int ver = mj * 10 + mn;

      ctxdata.version = ver;

      if(ver > GLCoreVersion || (!GLIsCore && ctxdata.isCore))
      {
        GLCoreVersion = ver;
        GLIsCore = ctxdata.isCore;
        DoVendorChecks(m_Platform, winData);
      }
    }

    if(IsCaptureMode(m_State))
    {
      // check if we already have VAO 0 registered for this context. This could be possible if
      // VAOs are shared and a previous context in the share group created it.
      GLResource vao0 = VertexArrayRes(GetCtx(), 0);

      if(!GetResourceManager()->HasCurrentResource(vao0))
      {
        ResourceId id = GetResourceManager()->RegisterResource(vao0);

        GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
        RDCASSERT(record);

        {
          USE_SCRATCH_SERIALISER();
          SCOPED_SERIALISE_CHUNK(GLChunk::glGenVertexArrays);
          GLuint zero = 0;
          Serialise_glGenVertexArrays(ser, 1, &zero);

          record->AddChunk(scope.Get());
        }

        // give it a name
        {
          USE_SCRATCH_SERIALISER();
          SCOPED_SERIALISE_CHUNK(GLChunk::glObjectLabel);
          Serialise_glObjectLabel(ser, eGL_VERTEX_ARRAY, 0, -1, "Default VAO");

          record->AddChunk(scope.Get());
        }

        // we immediately mark it dirty since the vertex array tracking functions expect a proper
        // VAO
        GetResourceManager()->MarkDirtyResource(id);
      }

      // we also do the same for FBO 0, but we must force it not to be shared as even if FBOs are
      // shared the FBO0 may not be :(.
      GLResource fbo0 = FramebufferRes({GetCtx().ctx, GetCtx().ctx}, 0);

      if(!GetResourceManager()->HasCurrentResource(fbo0))
        ctxdata.m_ContextFBOID = GetResourceManager()->RegisterResource(fbo0);
    }
  }

  // if we're capturing, we need to serialise out the changed state vector
  if(IsActiveCapturing(m_State))
  {
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::MakeContextCurrent);
      Serialise_BeginCaptureFrame(ser);
      GetContextRecord()->AddChunk(scope.Get());
    }

    // also serialise out this context's backbuffer params
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::ContextConfiguration);
      Serialise_ContextConfiguration(ser, winData.ctx);
      GetContextRecord()->AddChunk(scope.Get());
    }

    // update the last context so we don't record an implicit switch
    m_LastCtx = GetCtx().ctx;
  }

  // we create these buffers last after serialising the apply of the new state, so that in the
  // event that this context is created mid-capture, we don't serialise out buffer binding calls
  // that trash the state of the previous context while creating these buffers.
  if(ctxdata.m_ClientMemoryIBO == 0 && IsCaptureMode(m_State))
  {
    PUSH_CURRENT_CHUNK;
    GLuint prevArrayBuffer = 0;
    glGetIntegerv(eGL_ARRAY_BUFFER_BINDING, (GLint *)&prevArrayBuffer);

    GLuint prevElementArrayBuffer = 0;
    glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&prevElementArrayBuffer);

    // Initialize VBOs used in case we copy from client memory.
    gl_CurChunk = GLChunk::glGenBuffers;
    glGenBuffers(ARRAY_COUNT(ctxdata.m_ClientMemoryVBOs), ctxdata.m_ClientMemoryVBOs);

    for(size_t i = 0; i < ARRAY_COUNT(ctxdata.m_ClientMemoryVBOs); i++)
    {
      gl_CurChunk = GLChunk::glBindBuffer;
      glBindBuffer(eGL_ARRAY_BUFFER, ctxdata.m_ClientMemoryVBOs[i]);

      gl_CurChunk = GLChunk::glBufferData;
      glBufferData(eGL_ARRAY_BUFFER, 64, NULL, eGL_DYNAMIC_DRAW);

      // we mark these buffers as internal since initial contents are not needed - they're
      // entirely handled internally and buffer data is uploaded immediately before draws - and
      // we don't want them to be pulled in unless explicitly referenced.
      GetResourceManager()->SetInternalResource(BufferRes(GetCtx(), ctxdata.m_ClientMemoryVBOs[i]));

      if(HasExt[KHR_debug])
      {
        gl_CurChunk = GLChunk::glObjectLabel;
        glObjectLabel(eGL_BUFFER, ctxdata.m_ClientMemoryVBOs[i], -1,
                      StringFormat::Fmt("Client-memory pointer data (VB %zu)", i).c_str());
      }
    }

    gl_CurChunk = GLChunk::glGenBuffers;
    glGenBuffers(1, &ctxdata.m_ClientMemoryIBO);

    gl_CurChunk = GLChunk::glBindBuffer;
    glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ctxdata.m_ClientMemoryIBO);

    GetResourceManager()->SetInternalResource(BufferRes(GetCtx(), ctxdata.m_ClientMemoryIBO));

    gl_CurChunk = GLChunk::glBufferData;
    glBufferData(eGL_ELEMENT_ARRAY_BUFFER, 64, NULL, eGL_DYNAMIC_DRAW);

    if(HasExt[KHR_debug])
    {
      gl_CurChunk = GLChunk::glObjectLabel;
      glObjectLabel(eGL_BUFFER, ctxdata.m_ClientMemoryIBO, -1, "Client-memory pointer data (IB)");
    }

    gl_CurChunk = GLChunk::glBindBuffer;
    glBindBuffer(eGL_ARRAY_BUFFER, prevArrayBuffer);

    gl_CurChunk = GLChunk::glBindBuffer;
    glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, prevElementArrayBuffer);
  }

  // this is hack but GL context creation is an *utter mess*. For first-frame captures, only
  // consider an attribs created context, to avoid starting capturing when the user is creating
  // dummy contexts to be able to create the real one.
  if(ctxdata.attribsCreate)
    FirstFrame(ctxdata.ctx, (void *)winData.wnd);
}

struct ReplacementSearch
{
  bool operator()(const rdcpair<ResourceId, Replacement> &a, ResourceId b) { return a.first < b; }
};

void WrappedOpenGL::ReplaceResource(ResourceId from, ResourceId to)
{
  if(GetResourceManager()->HasLiveResource(from))
  {
    GLResource fromresource = GetResourceManager()->GetLiveResource(from);
    GLResource toresource = GetResourceManager()->GetLiveResource(to);

    // do actual replacement

    if(fromresource.Namespace == toresource.Namespace)
    {
      GetResourceManager()->RemoveReplacement(from);

      // if they're the same type we can just replace directly
      GetResourceManager()->ReplaceResource(from, to);
    }
    else if(fromresource.Namespace == eResProgram && toresource.Namespace == eResShader)
    {
      // if we want to replace a program with a shader, this is a glCreateShaderProgramv so we need
      // to handle it specially. We take the source from the shader, delete the shader, and steal
      // its ID to create a glCreateShaderProgramv. This avoids the awkward problem where we have
      // two replacements (program and shader) for one resource.

      ResourceId targetId = GetResourceManager()->GetResID(toresource);

      // backup the shader data
      rdcarray<rdcstr> shaderSources = m_Shaders[targetId].sources;
      GLenum shaderType = m_Shaders[targetId].type;

      // delete the shader completely
      glDeleteShader(toresource.name);
      m_Shaders.erase(targetId);

      // create a new unwrapped/unregistered programshader. This must be created unwrapped so we can
      // assign the existing ID to it.
      const char *str = shaderSources[0].c_str();
      toresource = ProgramRes(GetCtx(), GL.glCreateShaderProgramv(shaderType, 1, &str));

      // re-register the programshader in the place of where the shader used to be
      GetResourceManager()->RegisterResource(toresource, targetId);

      ProgramData &progDetails = m_Programs[targetId];

      progDetails.linked = true;
      progDetails.shaders.push_back(targetId);
      progDetails.stageShaders[ShaderIdx(shaderType)] = targetId;
      progDetails.shaderProgramUnlinkable = true;

      ShaderData &shadDetails = m_Shaders[targetId];

      shadDetails.type = shaderType;
      shadDetails.sources = shaderSources;

      shadDetails.ProcessCompilation(*this, targetId, 0);

      GetResourceManager()->AddLiveResource(targetId, toresource);

      // finally since programs have state (sigh) we have to copy that across as well.
      GLuint progsrc = fromresource.name;
      GLuint progdst = toresource.name;

      if(shaderType == eGL_VERTEX_SHADER)
        CopyProgramAttribBindings(progsrc, progdst, shadDetails.reflection);

      if(shaderType == eGL_FRAGMENT_SHADER)
        CopyProgramFragDataBindings(progsrc, progdst, shadDetails.reflection);

      {
        PerStageReflections dstStages;
        FillReflectionArray(targetId, dstStages);

        std::map<GLint, GLint> translate;

        ResourceId progsrcid = GetResourceManager()->GetResID(fromresource);

        PerStageReflections stages;
        FillReflectionArray(progsrcid, stages);

        // copy uniforms and set up new location translation table
        CopyProgramUniforms(stages, progsrc, dstStages, progdst, &translate);

        // start with the original location translation table, to account for any
        // capture-replay translation
        m_Programs[targetId].locationTranslate = m_Programs[progsrcid].locationTranslate;

        // compose on the one from editing.
        for(auto lit = m_Programs[targetId].locationTranslate.begin();
            lit != m_Programs[targetId].locationTranslate.end(); lit++)
        {
          auto lit2 = translate.find(lit->second);
          if(lit2 != translate.end())
            lit->second = lit2->second;
          else
            lit->second = -1;
        }
      }

      // now finally we can do the replacement as normal
      GetResourceManager()->RemoveReplacement(from);
      GetResourceManager()->ReplaceResource(from, to);
    }
    else
    {
      RDCERR("Unsupported replacement type from type %d to type %d", fromresource.Namespace,
             toresource.Namespace);
    }

    RefreshDerivedReplacements();
  }
}

void WrappedOpenGL::RemoveReplacement(ResourceId id)
{
  if(GetResourceManager()->HasReplacement(id))
  {
    GetResourceManager()->RemoveReplacement(id);

    RefreshDerivedReplacements();
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
      // a compiled shader could have been promoted to a program if it were a glCreateShaderProgramv
      case eResProgram: glDeleteProgram(resource.name); break;
      default: RDCERR("Unexpected resource type to be freed"); break;
    }
  }
}

void WrappedOpenGL::RefreshDerivedReplacements()
{
  // we defer deletes of old replaced resources since it will invalidate elements in the vector
  // we're iterating
  rdcarray<GLuint> deletequeue;

  // first go through programs and replace any that need to be updated based on whether they have
  // any replaced shaders
  for(auto it = m_Programs.begin(); it != m_Programs.end(); ++it)
  {
    ResourceId progsrcid = it->first;
    const ProgramData &progdata = it->second;

    ResourceId origsrcid = GetResourceManager()->GetOriginalID(progsrcid);

    // only look at programs from the capture, no replay-time programs.
    if(origsrcid == progsrcid)
      continue;

    // skip glCreateShaderProgramv programs. We handled this above and we don't want to try and
    // create a dependent program or remove the replacement
    if(progdata.shaderProgramUnlinkable)
      continue;

    // if this program has a replacement, remove it and delete the program generated for it
    if(GetResourceManager()->HasReplacement(origsrcid))
    {
      deletequeue.push_back(GetResourceManager()->GetLiveResource(origsrcid).name);
      GetResourceManager()->RemoveReplacement(origsrcid);
    }

    bool usesReplacedShader = false;

    for(size_t i = 0; i < NumShaderStages; i++)
    {
      if(GetResourceManager()->HasReplacement(
             GetResourceManager()->GetOriginalID(progdata.stageShaders[i])))
      {
        usesReplacedShader = true;
        break;
      }
    }

    // if there are replaced shaders in use, create a new program with any/all replaced shaders.
    if(usesReplacedShader)
    {
      GLuint progsrc = GetResourceManager()->GetCurrentResource(progsrcid).name;

      // make a new program
      GLuint progdst = glCreateProgram();

      ResourceId progdstid = GetResourceManager()->GetResID(ProgramRes(GetCtx(), progdst));

      // attach shaders, going via the original ID to pick up replacements
      for(size_t i = 0; i < NumShaderStages; i++)
      {
        if(progdata.stageShaders[i] != ResourceId())
        {
          ResourceId shaderorigid = GetResourceManager()->GetOriginalID(progdata.stageShaders[i]);
          glAttachShader(progdst, GetResourceManager()->GetLiveResource(shaderorigid).name);
        }
      }

      // mark separable if previous program was separable
      GLint sep = 0;
      glGetProgramiv(progsrc, eGL_PROGRAM_SEPARABLE, &sep);

      if(sep)
        glProgramParameteri(progdst, eGL_PROGRAM_SEPARABLE, GL_TRUE);

      ResourceId vs = progdata.stageShaders[0];
      ResourceId fs = progdata.stageShaders[4];

      if(vs != ResourceId())
        CopyProgramAttribBindings(progsrc, progdst, m_Shaders[vs].reflection);

      if(fs != ResourceId())
        CopyProgramFragDataBindings(progsrc, progdst, m_Shaders[fs].reflection);

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
        PerStageReflections dstStages;
        FillReflectionArray(progdstid, dstStages);

        std::map<GLint, GLint> translate;

        PerStageReflections stages;
        FillReflectionArray(progsrcid, stages);

        // copy uniforms and set up new location translation table
        CopyProgramUniforms(stages, progsrc, dstStages, progdst, &translate);

        // start with the original location translation table, to account for any
        // capture-replay translation
        m_Programs[progdstid].locationTranslate = m_Programs[progsrcid].locationTranslate;

        // compose on the one from editing.
        for(auto lit = m_Programs[progdstid].locationTranslate.begin();
            lit != m_Programs[progdstid].locationTranslate.end(); lit++)
        {
          auto lit2 = translate.find(lit->second);
          if(lit2 != translate.end())
            lit->second = lit2->second;
          else
            lit->second = -1;
        }

        // replace the program
        GetResourceManager()->ReplaceResource(origsrcid, progdstid);
      }
    }
  }

  for(GLuint prog : deletequeue)
    glDeleteProgram(prog);

  deletequeue.clear();

  // then go through pipelines based on replaced programs, as above
  for(auto it = m_Pipelines.begin(); it != m_Pipelines.end(); ++it)
  {
    ResourceId pipesrcid = it->first;
    const PipelineData &pipedata = it->second;

    ResourceId origsrcid = GetResourceManager()->GetOriginalID(pipesrcid);

    // only look at programs from the capture, no replay-time programs.
    if(origsrcid == pipesrcid)
      continue;

    // if this pipeline has a replacement, remove it and delete the pipeline generated for it
    if(GetResourceManager()->HasReplacement(origsrcid))
    {
      deletequeue.push_back(GetResourceManager()->GetLiveResource(origsrcid).name);
      GetResourceManager()->RemoveReplacement(origsrcid);
    }

    bool usesReplacedProgram = false;

    for(size_t i = 0; i < NumShaderStages; i++)
    {
      if(GetResourceManager()->HasReplacement(
             GetResourceManager()->GetOriginalID(pipedata.stagePrograms[i])))
      {
        usesReplacedProgram = true;
        break;
      }
    }

    // if there are replaced shaders in use, create a new program with any/all replaced shaders.
    if(usesReplacedProgram)
    {
      // make a new pipeline
      GLuint pipedst = 0;
      glGenProgramPipelines(1, &pipedst);

      ResourceId pipedstid = GetResourceManager()->GetResID(ProgramPipeRes(GetCtx(), pipedst));

      // attach programs, going via the original ID to pick up replacements
      for(size_t i = 0; i < NumShaderStages; i++)
      {
        if(pipedata.stagePrograms[i] != ResourceId())
        {
          ResourceId progorigid = GetResourceManager()->GetOriginalID(pipedata.stagePrograms[i]);
          glUseProgramStages(pipedst, ShaderBit(i),
                             GetResourceManager()->GetLiveResource(progorigid).name);
        }
      }

      // replace the pipeline
      GetResourceManager()->ReplaceResource(origsrcid, pipedstid);
    }
  }

  for(GLuint prog : deletequeue)
    glDeleteProgramPipelines(1, &prog);

  deletequeue.clear();
}

void WrappedOpenGL::SwapBuffers(WindowingSystem winSystem, void *windowHandle)
{
  if(IsBackgroundCapturing(m_State))
    RenderDoc::Inst().Tick();

  // don't do anything if no context is active.
  if(m_ActiveContexts[Threading::GetCurrentID()].ctx == NULL)
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
        it->second.UnassociateWindow(this, windowHandle);

    // only associate windows if no unsupported functions have been used
    if(m_UnsupportedFunctions.empty())
      ctxdata.AssociateWindow(this, winSystem, windowHandle);
  }

  // we used to do this here so it was as late as possible to avoid creating objects on contexts
  // that might be shared later. wglShareLists requires contexts to have no objects and can be
  // called after wglMakeCurrent. However we also need other objects like client-memory buffers and
  // vendor checks inside makecurrent that it is not feasible to defer until later, since there's no
  // other sync point after wglMakeCurrent before we'll need the information. So we don't support
  // calling wglShareLists after wglMakeCurrent.
  if(!ctxdata.ready)
    ctxdata.CreateDebugData();

  DeviceOwnedWindow devWnd(ctxdata.ctx, windowHandle);

  bool activeWindow = RenderDoc::Inst().IsActiveWindow(devWnd);

  // look at previous associations and decay any that are too old
  uint64_t ref = Timing::GetUnixTimestamp() - 5;    // 5 seconds

  for(auto cit = m_ContextData.begin(); cit != m_ContextData.end(); ++cit)
  {
    for(auto wit = cit->second.windows.begin(); wit != cit->second.windows.end();)
    {
      if(wit->second.second < ref)
      {
        auto remove = wit;
        ++wit;

        cit->second.UnassociateWindow(this, remove->first);
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
      int flags = 0;
      // capturing is disabled if unsupported functions have been used, or this context is legacy
      if(ctxdata.Legacy() || !m_UnsupportedFunctions.empty())
        flags |= RenderDoc::eOverlay_CaptureDisabled;
      rdcstr overlayText =
          RenderDoc::Inst().GetOverlayText(GetDriverType(), devWnd, m_FrameCounter, flags);

      if(ctxdata.Legacy())
      {
        if(!ctxdata.attribsCreate)
          overlayText += "Context not created via CreateContextAttribs. Capturing disabled.\n";
        overlayText += "Only OpenGL 3.2+ contexts are supported.\n";
      }
      else if(!ctxdata.isCore)
      {
        overlayText +=
            "WARNING: Core profile not explicitly requested. Compatibility profile is not "
            "supported.\n";
      }

      // print the unsupported functions (up to a handful) to show
      if(!m_UnsupportedFunctions.empty())
      {
        overlayText +=
            StringFormat::Fmt("Captures disabled.\nUnsupported %s used:\n",
                              m_UnsupportedFunctions.size() == 1 ? "function" : "functions");
        size_t i = 0;
        for(const char *func : m_UnsupportedFunctions)
        {
          i++;
          if(i > 4)
            break;
          overlayText += StringFormat::Fmt(" - %s\n", func);
        }
        if(m_UnsupportedFunctions.size() > i)
          overlayText += " - ...\n";
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

      RenderText(0.0f, 0.0f, overlayText);

      // swallow all errors we might have inadvertantly caused. This is
      // better than letting an error propagate and maybe screw up the
      // app (although it means we might swallow an error from before the
      // SwapBuffers call, it can't be helped.
      if(ctxdata.Legacy() && GL.glGetError)
        ClearGLErrors();
    }
  }

  if(IsActiveCapturing(m_State) && m_AppControlledCapture)
  {
    delete m_BackbufferImages[windowHandle];
    m_BackbufferImages[windowHandle] = SaveBackbufferImage();
  }

  if(IsActiveCapturing(m_State) && gl_CurChunk != GLChunk::Max)
  {
    SERIALISE_TIME_CALL();

    USE_SCRATCH_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_Present(ser);

    GetContextRecord()->AddChunk(scope.Get());
  }

  RenderDoc::Inst().AddActiveDriver(GetDriverType(), true);

  GetResourceManager()->CleanBackgroundFrameReferences();

  if(!activeWindow)
  {
    // first present to *any* window, even inactive, terminates frame 0
    if(m_FirstFrameCapture && IsActiveCapturing(m_State))
    {
      RenderDoc::Inst().EndFrameCapture(DeviceOwnedWindow(m_FirstFrameCaptureContext, NULL));
      m_FirstFrameCapture = false;
      m_FirstFrameCaptureContext = NULL;
    }

    return;
  }

  // only allow capturing on 'modern' created contexts
  if(ctxdata.Legacy())
    return;

  // kill any current capture that isn't application defined
  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture(devWnd);

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(devWnd);

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = m_FrameCounter;
  }
}

GLWindowingData *WrappedOpenGL::MakeValidContextCurrent(GLWindowingData existing,
                                                        GLWindowingData &newContext)
{
  if(existing.ctx == NULL)
  {
    if(m_LastContexts.empty())
    {
      RDCERR("No GL context exists - can't make current, will likely crash");
      return NULL;
    }

    // take the last context used
    GLWindowingData ctx = m_LastContexts.back();

    // and use the backdoor context on it
    newContext = m_ContextData[ctx.ctx].shareGroup->m_BackDoor;

    GLWindowingData *saved = new GLWindowingData;
    m_ActiveContexts[Threading::GetCurrentID()] = newContext;
    m_Platform.PushChildContext(existing, newContext, saved);
    return saved;
  }

  return NULL;
}

void WrappedOpenGL::StartFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  RDCLOG("Starting capture");

  m_CaptureTimer.Restart();

  SCOPED_LOCK(glLock);

  m_State = CaptureState::ActiveCapturing;

  GetResourceManager()->ResetCaptureStartTime();

  m_AppControlledCapture = true;

  m_Failures = 0;
  m_FailedFrame = 0;
  m_FailedReason = CaptureSucceeded;

  GLWindowingData existing = m_ActiveContexts[Threading::GetCurrentID()];
  GLWindowingData newContext = existing;
  GLWindowingData *pushChildSaved = MakeValidContextCurrent(existing, newContext);

  FrameDescription frame;
  frame.frameNumber = m_AppControlledCapture ? ~0U : m_FrameCounter;
  frame.captureTime = Timing::GetUnixTimestamp();
  m_CapturedFrames.push_back(frame);

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->MarkResourceFrameReferenced(m_DeviceResourceID, eFrameRef_PartialWrite);

  GetResourceManager()->PrepareInitialContents();

  FreeCaptureData();

  AttemptCapture();
  BeginCaptureFrame();

  m_LastCtx = GetCtx().ctx;

  // serialise out the context configuration for this current context first
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(GLChunk::ContextConfiguration);
    Serialise_ContextConfiguration(ser, GetCtx().ctx);
    GetContextRecord()->AddChunk(scope.Get());
  }

  // if we changed contexts above, pop back to where we were
  if(pushChildSaved)
  {
    m_Platform.PopChildContext(existing, newContext, *pushChildSaved);
    delete pushChildSaved;

    m_ActiveContexts[Threading::GetCurrentID()] = existing;
  }
}

bool WrappedOpenGL::EndFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  SCOPED_LOCK(glLock);

  CaptureFailReason reason = CaptureSucceeded;

  GLWindowingData existing = m_ActiveContexts[Threading::GetCurrentID()];
  GLWindowingData newContext = existing;
  GLWindowingData *pushChildSaved = MakeValidContextCurrent(existing, newContext);

  if(HasSuccessfulCapture(reason))
  {
    RDCLOG("Finished capture, Frame %u", m_CapturedFrames.back().frameNumber);

    m_Failures = 0;
    m_FailedFrame = 0;
    m_FailedReason = CaptureSucceeded;

    ContextEndFrame();
    FinishCapture();

    RenderDoc::FramePixels *bbim = NULL;

    // if the specified context isn't current, try and see if we've saved
    // an appropriate backbuffer image during capture.
    if((devWnd.device != NULL && existing.ctx != devWnd.device) ||
       (devWnd.windowHandle != 0 && (void *)existing.wnd != devWnd.windowHandle))
    {
      auto it = m_BackbufferImages.find(devWnd.windowHandle);
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

    RDCFile *rdc =
        RenderDoc::Inst().CreateRDC(GetDriverType(), m_CapturedFrames.back().frameNumber, bbim[0]);

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

    uint64_t captureSectionSize = 0;

    {
      WriteSerialiser ser(captureWriter, Ownership::Stream);

      ser.SetChunkMetadataRecording(m_ScratchSerialiser.GetChunkMetadataRecording());

      ser.SetUserData(GetResourceManager());

      {
        // we no longer use this one, but for ease of compatibility we still serialise it here. This
        // will be immediately overridden by the actual parameters by a
        // GLChunk::ContextConfiguration chunk
        GLInitParams init;

        // store renderer and version, though we can't do any meaningful device selection
        init.renderer = (const char *)GL.glGetString(eGL_RENDERER);
        init.version = (const char *)GL.glGetString(eGL_VERSION);

        SCOPED_SERIALISE_CHUNK(
            SystemChunk::DriverInit,
            sizeof(GLInitParams) + 16 + init.renderer.size() + init.version.size());

        SERIALISE_ELEMENT(init);
      }

      {
        // remember to update this estimated chunk length if you add more parameters
        SCOPED_SERIALISE_CHUNK(GLChunk::DeviceInitialisation, 32);

        // legacy behaviour where we had a single global VAO/FBO 0. Ignore, but preserve for easier
        // compatibility with old captures
        ResourceId vao, fbo;
        SERIALISE_ELEMENT(vao);
        SERIALISE_ELEMENT(fbo);
      }

      RDCDEBUG("Inserting Resource Serialisers");

      GetResourceManager()->InsertReferencedChunks(ser);

      GetResourceManager()->InsertInitialContentsChunks(ser);

      RDCDEBUG("Creating Capture Scope");

      GetResourceManager()->Serialise_InitialContentsNeeded(ser);

      {
        SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureScope, 16);

        Serialise_CaptureScope(ser);
      }

      {
        RDCDEBUG("Accumulating context resource list");

        std::map<int64_t, Chunk *> recordlist;
        m_ContextRecord->Insert(recordlist);

        for(auto it = m_ContextData.begin(); it != m_ContextData.end(); ++it)
        {
          if(m_AcceptedCtx.empty() || m_AcceptedCtx.find(it->first) != m_AcceptedCtx.end())
          {
            GLResourceRecord *record = it->second.m_ContextDataRecord;
            if(record)
            {
              RDCDEBUG("Getting Resource Record for context ID %s with %zu chunks",
                       ToStr(it->second.m_ContextDataResourceID).c_str(), record->NumChunks());
              record->Insert(recordlist);
            }
          }
        }

        RDCDEBUG("Flushing %u records to file serialiser", (uint32_t)recordlist.size());

        float num = float(recordlist.size());
        float idx = 0.0f;

        for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
        {
          RenderDoc::Inst().SetProgress(CaptureProgress::SerialiseFrameContents, idx / num);
          idx += 1.0f;
          it->second->Write(ser);
        }

        RDCDEBUG("Done");
      }

      captureSectionSize = captureWriter->GetOffset();
    }

    RDCLOG("Captured GL frame with %f MB capture section in %f seconds",
           double(captureSectionSize) / (1024.0 * 1024.0), m_CaptureTimer.GetMilliseconds() / 1000.0);

    RenderDoc::Inst().FinishCaptureWriting(rdc, m_CapturedFrames.back().frameNumber);

    m_State = CaptureState::BackgroundCapturing;

    for(const rdcpair<GLResourceRecord *, Chunk *> &r : m_BufferResizes)
    {
      r.first->AddChunk(r.second);
      r.first->SetDataPtr(r.second->GetData());
    }
    m_BufferResizes.clear();

    GetResourceManager()->ResetLastWriteTimes();

    GetResourceManager()->MarkUnwrittenResources();

    GetResourceManager()->ClearReferencedResources();

    GetResourceManager()->FreeInitialContents();

    for(auto it = m_CoherentMaps.begin(); it != m_CoherentMaps.end(); ++it)
    {
      GLResourceRecord *record = *it;

      record->FreeShadowStorage();
    }

    // if we changed contexts above, pop back to where we were
    if(pushChildSaved)
    {
      m_Platform.PopChildContext(existing, newContext, *pushChildSaved);
      delete pushChildSaved;

      m_ActiveContexts[Threading::GetCurrentID()] = existing;
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

    RDCLOG("Failed to capture, frame %u: %s", m_CapturedFrames.back().frameNumber, reasonString);

    m_Failures++;

    if((RenderDoc::Inst().GetOverlayBits() & eRENDERDOC_Overlay_Enabled))
    {
      ContextData &ctxdata = GetCtxData();

      RenderText(0.0f, 0.0f,
                 StringFormat::Fmt("Failed to capture frame %u: %s",
                                   m_CapturedFrames.back().frameNumber, reasonString));

      // swallow all errors we might have inadvertantly caused. This is
      // better than letting an error propagate and maybe screw up the
      // app (although it means we might swallow an error from before the
      // SwapBuffers call, it can't be helped.
      if(ctxdata.Legacy() && GL.glGetError)
        ClearGLErrors();
    }

    uint32_t failedFrame = m_CapturedFrames.back().frameNumber;

    m_CapturedFrames.back().frameNumber = m_AppControlledCapture ? ~0U : m_FrameCounter;

    for(const rdcpair<GLResourceRecord *, Chunk *> &r : m_BufferResizes)
    {
      r.first->AddChunk(r.second);
      r.first->SetDataPtr(r.second->GetData());
    }
    m_BufferResizes.clear();

    CleanupCapture();

    GetResourceManager()->ClearReferencedResources();

    GetResourceManager()->FreeInitialContents();

    for(auto it = m_CoherentMaps.begin(); it != m_CoherentMaps.end(); ++it)
    {
      GLResourceRecord *record = *it;

      record->FreeShadowStorage();
    }

    // if it's a capture triggered from application code, immediately
    // give up as it's not reasonable to expect applications to detect and retry.
    // otherwise we can retry in case the next frame works.
    if(m_Failures > 5 || m_AppControlledCapture)
    {
      FinishCapture();

      m_CapturedFrames.pop_back();

      FreeCaptureData();

      m_FailedFrame = failedFrame;
      m_FailedReason = reason;

      m_State = CaptureState::BackgroundCapturing;

      GetResourceManager()->MarkUnwrittenResources();
    }
    else
    {
      GetResourceManager()->MarkResourceFrameReferenced(m_DeviceResourceID, eFrameRef_PartialWrite);
      GetResourceManager()->PrepareInitialContents();

      AttemptCapture();
      BeginCaptureFrame();

      // serialise out the context configuration for this current context first
      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::ContextConfiguration);
        Serialise_ContextConfiguration(ser, GetCtx().ctx);
        GetContextRecord()->AddChunk(scope.Get());
      }
    }

    // if we changed contexts above, pop back to where we were
    if(pushChildSaved)
    {
      m_Platform.PopChildContext(existing, newContext, *pushChildSaved);
      delete pushChildSaved;

      m_ActiveContexts[Threading::GetCurrentID()] = existing;
    }

    return false;
  }
}

bool WrappedOpenGL::DiscardFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  RDCLOG("Discarding frame capture.");

  SCOPED_LOCK(glLock);

  RenderDoc::Inst().FinishCaptureWriting(NULL, m_CapturedFrames.back().frameNumber);

  for(const rdcpair<GLResourceRecord *, Chunk *> &r : m_BufferResizes)
  {
    r.first->AddChunk(r.second);
    r.first->SetDataPtr(r.second->GetData());
  }
  m_BufferResizes.clear();

  CleanupCapture();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  FinishCapture();

  for(auto it = m_CoherentMaps.begin(); it != m_CoherentMaps.end(); ++it)
  {
    GLResourceRecord *record = *it;

    record->FreeShadowStorage();
  }

  m_CapturedFrames.pop_back();

  FreeCaptureData();

  m_State = CaptureState::BackgroundCapturing;

  GetResourceManager()->MarkUnwrittenResources();

  for(auto it = m_BackbufferImages.begin(); it != m_BackbufferImages.end(); ++it)
    delete it->second;
  m_BackbufferImages.clear();

  return true;
}

void WrappedOpenGL::FirstFrame(void *ctx, void *wndHandle)
{
  // if we have to capture the first frame, begin capturing immediately
  if(m_FrameCounter == 0 && IsBackgroundCapturing(m_State) &&
     RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    // since we haven't associated the window we can't capture by window, so we have to capture just
    // on the device - the very next present to any window on this context will end the capture.
    RenderDoc::Inst().StartFrameCapture(DeviceOwnedWindow(ctx, NULL));

    m_FirstFrameCapture = true;
    m_FirstFrameCaptureContext = ctx;
    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = 0;
  }
}

RenderDoc::FramePixels *WrappedOpenGL::SaveBackbufferImage()
{
  const uint16_t maxSize = 2048;
  RenderDoc::FramePixels *fp = new RenderDoc::FramePixels();

  if(GL.glGetIntegerv && GL.glReadBuffer && GL.glBindFramebuffer && GL.glBindBuffer && GL.glReadPixels)
  {
    RDCGLenum prevReadBuf = eGL_BACK;
    GLint prevBuf = 0;
    GLint packBufBind = 0;
    GLint prevPackRowLen = 0;
    GLint prevPackSkipRows = 0;
    GLint prevPackSkipPixels = 0;
    GLint prevPackAlignment = 0;
    GL.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&prevReadBuf);
    GL.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &prevBuf);
    GL.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, &packBufBind);
    GL.glGetIntegerv(eGL_PACK_ROW_LENGTH, &prevPackRowLen);
    GL.glGetIntegerv(eGL_PACK_SKIP_ROWS, &prevPackSkipRows);
    GL.glGetIntegerv(eGL_PACK_SKIP_PIXELS, &prevPackSkipPixels);
    GL.glGetIntegerv(eGL_PACK_ALIGNMENT, &prevPackAlignment);

    GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, 0);
    GL.glReadBuffer(eGL_BACK);
    GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
    GL.glPixelStorei(eGL_PACK_ROW_LENGTH, 0);
    GL.glPixelStorei(eGL_PACK_SKIP_ROWS, 0);
    GL.glPixelStorei(eGL_PACK_SKIP_PIXELS, 0);
    GL.glPixelStorei(eGL_PACK_ALIGNMENT, 1);

    ContextData &dat = GetCtxData();

    fp->width = dat.initParams.width;
    fp->height = dat.initParams.height;
    fp->bpc = 1;
    fp->stride = fp->bpc * 4;
    fp->pitch = dat.initParams.width * fp->stride;
    fp->max_width = maxSize;
    fp->pitch_requirement = 4;
    fp->len = (uint32_t)fp->pitch * fp->height;
    fp->data = new uint8_t[fp->len];
    fp->is_y_flipped = dat.initParams.isYFlipped;

    // GLES only supports GL_RGBA
    GL.glReadPixels(0, 0, fp->width, fp->height, eGL_RGBA, eGL_UNSIGNED_BYTE, fp->data);

    GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, packBufBind);
    GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevBuf);
    GL.glReadBuffer(prevReadBuf);
    GL.glPixelStorei(eGL_PACK_ROW_LENGTH, prevPackRowLen);
    GL.glPixelStorei(eGL_PACK_SKIP_ROWS, prevPackSkipRows);
    GL.glPixelStorei(eGL_PACK_SKIP_PIXELS, prevPackSkipPixels);
    GL.glPixelStorei(eGL_PACK_ALIGNMENT, prevPackAlignment);
  }

  return fp;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_Present(SerialiserType &ser)
{
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && IsLoading(m_State))
  {
    AddEvent();

    ActionDescription action;

    GLuint col = 0;
    GL.glGetNamedFramebufferAttachmentParameterivEXT(m_CurrentDefaultFBO, eGL_COLOR_ATTACHMENT0,
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                     (GLint *)&col);

    action.copyDestination = GetResourceManager()->GetOriginalID(
        GetResourceManager()->GetResID(TextureRes(GetCtx(), col)));

    action.customName = StringFormat::Fmt("%s(%s)", ToStr(gl_CurChunk).c_str(),
                                          ToStr(action.copyDestination).c_str());
    action.flags |= ActionFlags::Present;

    AddAction(action);
  }

  return true;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_CaptureScope(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(frameNumber, m_CapturedFrames.back().frameNumber);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GetReplay()->WriteFrameRecord().frameInfo.frameNumber = frameNumber;
    RDCEraseEl(GetReplay()->WriteFrameRecord().frameInfo.stats);
  }

  return true;
}

bool WrappedOpenGL::Serialise_ContextInit(ReadSerialiser &ser)
{
  SERIALISE_ELEMENT_LOCAL(FBO0_ID, ResourceId());

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // this chunk has been replaced by the ContextConfiguration chunk. Previously this was used to
    // register the ID of a framebuffer on another context, so it can be redirected to a single
    // global FBO0. But now each context's FBO0 is unique. So if this is present, we also have the
    // global FBO0 to redirect to.
    ResourceId global_fbo0 = GetResourceManager()->GetResID(FramebufferRes(GetCtx(), m_Global_FBO0));

    GetReplay()->GetResourceDesc(global_fbo0).SetCustomName("Backbuffer FBO");

    GetResourceManager()->ReplaceResource(FBO0_ID, global_fbo0);

    AddResource(FBO0_ID, ResourceType::SwapchainImage, "");
    GetReplay()->GetResourceDesc(FBO0_ID).SetCustomName("Window FBO");

    // this is a hack, but we only support a single 'default' framebuffer so we set these
    // replacements up as derived resources
    GetReplay()->GetResourceDesc(global_fbo0).derivedResources.push_back(FBO0_ID);
    GetReplay()->GetResourceDesc(FBO0_ID).parentResources.push_back(global_fbo0);
  }

  return true;
}

void WrappedOpenGL::ContextEndFrame()
{
  USE_SCRATCH_SERIALISER();
  ser.SetActionChunk();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureEnd);

  m_ContextRecord->AddChunk(scope.Get());
}

void WrappedOpenGL::CleanupResourceRecord(GLResourceRecord *record, bool freeParents)
{
  if(record)
  {
    record->LockChunks();
    while(record->HasChunks())
    {
      Chunk *chunk = record->GetLastChunk();

      chunk->Delete();
      record->PopChunk();
    }
    record->UnlockChunks();
    if(freeParents)
      record->FreeParents(GetResourceManager());
  }
}

void WrappedOpenGL::CleanupCapture()
{
  m_SuccessfulCapture = true;
  m_FailureReason = CaptureSucceeded;

  CleanupResourceRecord(m_ContextRecord, true);

  for(auto it = m_ContextData.begin(); it != m_ContextData.end(); ++it)
  {
    CleanupResourceRecord(it->second.m_ContextDataRecord, true);
  }
}

void WrappedOpenGL::FreeCaptureData()
{
}

void WrappedOpenGL::QueuePrepareInitialState(GLResource res)
{
  QueuedResource q;
  q.res = res;

  auto insertPos = std::lower_bound(m_QueuedInitialFetches.begin(), m_QueuedInitialFetches.end(), q);
  m_QueuedInitialFetches.insert(insertPos - m_QueuedInitialFetches.begin(), q);
}

void WrappedOpenGL::QueueResourceRelease(GLResource res)
{
  QueuedResource q;
  q.res = res;

  auto insertPos = std::lower_bound(m_QueuedReleases.begin(), m_QueuedReleases.end(), q);
  m_QueuedReleases.insert(insertPos - m_QueuedReleases.begin(), q);
}

void WrappedOpenGL::CheckQueuedInitialFetches(void *checkCtx)
{
  if(IsActiveCapturing(m_State))
  {
    // fetch any initial states needed. Note this is insufficient, and doesn't handle the case
    // where we might just suddenly start getting commands on a thread that already has a context
    // active. For now we assume we'll only get GL commands from a single thread
    //
    // First we process any queued fetches from the context itself (i.e. non-shared resources),
    // then from the context's share group.
    for(void *ctx : {(void *)checkCtx, (void *)GetShareGroup(checkCtx)})
    {
      QueuedResource fetch;
      fetch.res.ContextShareGroup = ctx;
      size_t before = m_QueuedInitialFetches.size();
      auto it = std::lower_bound(m_QueuedInitialFetches.begin(), m_QueuedInitialFetches.end(), fetch);
      size_t i = it - m_QueuedInitialFetches.begin();
      while(i < m_QueuedInitialFetches.size() && it->res.ContextShareGroup == ctx)
      {
        GetResourceManager()->ContextPrepare_InitialState(it->res);
        m_QueuedInitialFetches.erase(i);
      }
      size_t after = m_QueuedInitialFetches.size();

      (void)before;
      (void)after;
      RDCDEBUG("Prepared %zu resources on context/sharegroup %p, %zu left", before - after, ctx,
               after);
    }
  }

  // also if there are any queued releases, process them now
  if(!m_QueuedReleases.empty())
  {
    for(void *ctx : {(void *)checkCtx, (void *)GetShareGroup(checkCtx)})
    {
      QueuedResource fetch;
      fetch.res.ContextShareGroup = ctx;
      size_t before = m_QueuedReleases.size();
      auto it = std::lower_bound(m_QueuedReleases.begin(), m_QueuedReleases.end(), fetch);
      size_t i = it - m_QueuedReleases.begin();
      while(it != m_QueuedReleases.end() && it->res.ContextShareGroup == ctx)
      {
        ReleaseResource(it->res);
        m_QueuedReleases.erase(i);
      }
      size_t after = m_QueuedReleases.size();

      (void)before;
      (void)after;
      RDCDEBUG("Released %zu resources on context/sharegroup %p, %zu left", before - after, ctx,
               after);
    }
  }
}

void WrappedOpenGL::CreateTextureImage(GLuint tex, GLenum internalFormat, GLenum initFormatHint,
                                       GLenum initTypeHint, GLenum textype, GLint dim, GLint width,
                                       GLint height, GLint depth, GLint samples, int mips)
{
  if(textype == eGL_TEXTURE_BUFFER)
  {
    return;
  }

  GLuint ppb = 0, pub = 0;

  GL.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&ppb);
  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pub);

  GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
  GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

  if(textype == eGL_TEXTURE_2D_MULTISAMPLE)
  {
    // we need a sized format for storage functions
    internalFormat = GetSizedFormat(internalFormat);

    GL.glTextureStorage2DMultisampleEXT(tex, textype, samples, internalFormat, width, height,
                                        GL_TRUE);
  }
  else if(textype == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
  {
    // we need a sized format for storage functions
    internalFormat = GetSizedFormat(internalFormat);

    GL.glTextureStorage3DMultisampleEXT(tex, textype, samples, internalFormat, width, height, depth,
                                        GL_TRUE);
  }
  else
  {
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_MAX_LEVEL, mips - 1);
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

    bool isCompressed = IsCompressedFormat(internalFormat);

    GLenum baseFormat = eGL_RGBA;
    GLenum dataType = eGL_UNSIGNED_BYTE;
    if(!isCompressed)
    {
      baseFormat = GetBaseFormat(internalFormat);
      dataType = GetDataType(internalFormat);
    }

    if(initFormatHint != eGL_NONE)
      baseFormat = initFormatHint;
    if(initTypeHint != eGL_NONE)
      dataType = initTypeHint;

    GLenum targets[] = {
        eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    };

    int count = ARRAY_COUNT(targets);

    if(textype != eGL_TEXTURE_CUBE_MAP)
    {
      targets[0] = textype;
      count = 1;
    }

    GLsizei w = (GLsizei)width;
    GLsizei h = (GLsizei)height;
    GLsizei d = (GLsizei)depth;

    for(int m = 0; m < mips; m++)
    {
      for(int t = 0; t < count; t++)
      {
        if(isCompressed)
        {
          GLsizei compSize = (GLsizei)GetCompressedByteSize(w, h, d, internalFormat);

          bytebuf dummy;
          dummy.resize(compSize);

          if(dim == 1)
            GL.glCompressedTextureImage1DEXT(tex, targets[t], m, internalFormat, w, 0, compSize,
                                             &dummy[0]);
          else if(dim == 2)
            GL.glCompressedTextureImage2DEXT(tex, targets[t], m, internalFormat, w, h, 0, compSize,
                                             &dummy[0]);
          else if(dim == 3)
            GL.glCompressedTextureImage3DEXT(tex, targets[t], m, internalFormat, w, h, d, 0,
                                             compSize, &dummy[0]);
        }
        else
        {
          if(dim == 1)
            GL.glTextureImage1DEXT(tex, targets[t], m, internalFormat, w, 0, baseFormat, dataType,
                                   NULL);
          else if(dim == 2)
            GL.glTextureImage2DEXT(tex, targets[t], m, internalFormat, w, h, 0, baseFormat,
                                   dataType, NULL);
          else if(dim == 3)
            GL.glTextureImage3DEXT(tex, targets[t], m, internalFormat, w, h, d, 0, baseFormat,
                                   dataType, NULL);
        }
      }

      w = RDCMAX(1, w >> 1);
      if(textype != eGL_TEXTURE_1D_ARRAY)
        h = RDCMAX(1, h >> 1);
      if(textype != eGL_TEXTURE_2D_ARRAY && textype != eGL_TEXTURE_CUBE_MAP_ARRAY)
        d = RDCMAX(1, d >> 1);
    }
  }

  if(IsCaptureMode(m_State))
  {
    // register this texture and set up its texture details, so it's available for emulation
    // readback.
    GLResource res = TextureRes(GetCtx(), tex);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    WrappedOpenGL::TextureData &details = m_Textures[id];

    details.resource = res;
    details.curType = textype;
    details.dimension = dim;
    details.emulated = details.view = false;
    details.width = width;
    details.height = height;
    details.depth = depth;
    details.samples = samples;
    details.creationFlags = TextureCategory::NoFlags;
    details.internalFormat = internalFormat;
    details.mipsValid = (1 << mips) - 1;
  }

  GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, ppb);
  GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pub);
}

void WrappedOpenGL::ReleaseResource(GLResource res)
{
  switch(res.Namespace)
  {
    default: RDCERR("Unknown namespace to release: %s", ToStr(res.Namespace).c_str()); return;
    case eResTexture: GL.glDeleteTextures(1, &res.name); break;
    case eResSampler: GL.glDeleteSamplers(1, &res.name); break;
    case eResFramebuffer: GL.glDeleteFramebuffers(1, &res.name); break;
    case eResRenderbuffer: GL.glDeleteRenderbuffers(1, &res.name); break;
    case eResBuffer: GL.glDeleteBuffers(1, &res.name); break;
    case eResVertexArray: GL.glDeleteVertexArrays(1, &res.name); break;
    case eResShader: GL.glDeleteShader(res.name); break;
    case eResProgram: GL.glDeleteProgram(res.name); break;
    case eResProgramPipe: GL.glDeleteProgramPipelines(1, &res.name); break;
    case eResFeedback: GL.glDeleteTransformFeedbacks(1, &res.name); break;
    case eResQuery: GL.glDeleteQueries(1, &res.name); break;
    case eResSync: GL.glDeleteSync(GetResourceManager()->GetSync(res.name)); break;
    case eResExternalMemory: GL.glDeleteMemoryObjectsEXT(1, &res.name); break;
    case eResExternalSemaphore: GL.glDeleteSemaphoresEXT(1, &res.name); break;
  }
}

void WrappedOpenGL::AttemptCapture()
{
  m_State = CaptureState::ActiveCapturing;

  m_DebugMessages.clear();

  if(!HasExt[KHR_debug] && RenderDoc::Inst().GetCaptureOptions().apiValidation)
  {
    DebugMessage msg = {};

    msg.category = MessageCategory::Portability;
    msg.severity = MessageSeverity::High;
    msg.source = MessageSource::RuntimeWarning;
    msg.description =
        "API Validation was enabled, but KHR_debug was not available in this driver so no "
        "validation messages could be retrieved";

    m_DebugMessages.push_back(msg);
  }

  {
    RDCDEBUG("GL Context %s Attempting capture", ToStr(m_ContextResourceID).c_str());

    m_SuccessfulCapture = true;
    m_FailureReason = CaptureSucceeded;

    CleanupResourceRecord(m_ContextRecord, false);

    for(auto it = m_ContextData.begin(); it != m_ContextData.end(); ++it)
    {
      CleanupResourceRecord(it->second.m_ContextDataRecord, false);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_BeginCaptureFrame(SerialiserType &ser)
{
  GLRenderState state;

  if(ser.IsWriting())
  {
    rdcarray<DebugMessage> savedDebugMessages;

    // save any debug messages we built up
    savedDebugMessages.swap(m_DebugMessages);

    state.FetchState(this);
    state.MarkReferenced(this, true);

    // restore saved messages - which implicitly discards any generated while fetching state
    savedDebugMessages.swap(m_DebugMessages);
  }

  SERIALISE_ELEMENT(state).Unimportant();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    rdcarray<DebugMessage> savedDebugMessages;

    // save any debug messages we built up
    savedDebugMessages.swap(m_DebugMessages);

    state.ApplyState(this);

    // restore saved messages - which implicitly discards any generated while applying state
    savedDebugMessages.swap(m_DebugMessages);
  }

  return true;
}

void WrappedOpenGL::BeginCaptureFrame()
{
  USE_SCRATCH_SERIALISER();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureBegin);

  Serialise_BeginCaptureFrame(ser);

  m_ContextRecord->AddChunk(scope.Get(), 1);

  // mark VAO 0 on this context as referenced
  {
    GLuint prevVAO = 0;
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    GL.glBindVertexArray(0);

    GetResourceManager()->MarkVAOReferenced(VertexArrayRes(GetCtx(), 0), eFrameRef_PartialWrite,
                                            true);

    GL.glBindVertexArray(prevVAO);
  }
}

void WrappedOpenGL::FinishCapture()
{
  m_State = CaptureState::BackgroundCapturing;

  m_DebugMessages.clear();

  // m_SuccessfulCapture = false;
}

void WrappedOpenGL::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d)
{
  if(IsLoading(m_State) || src == MessageSource::RuntimeWarning)
  {
    DebugMessage msg;
    msg.eventId = m_CurEventID;
    msg.messageID = 0;
    msg.source = src;
    msg.category = c;
    msg.severity = sv;
    msg.description = d;
    m_DebugMessages.push_back(msg);
  }
}

rdcarray<DebugMessage> WrappedOpenGL::GetDebugMessages()
{
  rdcarray<DebugMessage> ret;
  ret.swap(m_DebugMessages);
  return ret;
}

template <typename SerialiserType>
void WrappedOpenGL::Serialise_DebugMessages(SerialiserType &ser)
{
  rdcarray<DebugMessage> DebugMessages;

  if(ser.IsWriting())
  {
    DebugMessages.swap(m_DebugMessages);
  }

  SERIALISE_ELEMENT(DebugMessages).Unimportant();

  // if we're using replay-time API validation, fetch messages at replay time and ignore any
  // serialised ones
  if(ser.IsReading() && IsLoading(m_State) && m_ReplayOptions.apiValidation)
  {
    DebugMessages = m_DebugMessages;
    m_DebugMessages.removeIf([](const DebugMessage &msg) { return msg.eventId == 0; });
    DebugMessages.removeIf([](const DebugMessage &msg) { return msg.eventId != 0; });
  }

  // hide empty sets of messages.
  if(ser.IsReading() && DebugMessages.empty())
    ser.Hidden();

  if(ser.IsReading() && IsLoading(m_State))
  {
    for(DebugMessage &msg : DebugMessages)
    {
      msg.eventId = m_CurEventID;
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
  if(record->UpdateCount > 64)
    return false;

  // increase update count
  record->UpdateCount++;

  // if update count is high, mark as dirty
  if(record->UpdateCount > 64)
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());

    return false;
  }

  return true;
}

void WrappedOpenGL::RegisterDebugCallback()
{
  // once GL driver is more tested, this can be disabled
  if(HasExt[KHR_debug] && GL.glDebugMessageCallback)
  {
    GL.glDebugMessageCallback(&DebugSnoopStatic, this);
    GL.glEnable(eGL_DEBUG_OUTPUT_SYNCHRONOUS);
  }
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

    if(IsActiveCapturing(m_State) || (IsLoading(m_State) && m_ReplayOptions.apiValidation))
    {
      DebugMessage msg;

      msg.eventId = 0;
      msg.messageID = id;
      msg.description = rdcstr(message, length);
      msg.source = MessageSource::API;

      switch(severity)
      {
        case eGL_DEBUG_SEVERITY_HIGH: msg.severity = MessageSeverity::High; break;
        case eGL_DEBUG_SEVERITY_MEDIUM: msg.severity = MessageSeverity::Medium; break;
        case eGL_DEBUG_SEVERITY_LOW: msg.severity = MessageSeverity::Low; break;
        case eGL_DEBUG_SEVERITY_NOTIFICATION:
        default: msg.severity = MessageSeverity::Info; break;
      }

      if(source == eGL_DEBUG_SOURCE_APPLICATION)
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

  if(GetCtxData().m_RealDebugFunc && !RenderDoc::Inst().GetCaptureOptions().debugOutputMute)
    GetCtxData().m_RealDebugFunc(source, type, id, severity, length, message,
                                 GetCtxData().m_RealDebugFuncParam);
}

void WrappedOpenGL::AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix)
{
  ResourceDescription &descr = GetReplay()->GetResourceDesc(id);

  uint64_t num;
  memcpy(&num, &id, sizeof(uint64_t));
  descr.name = defaultNamePrefix + (" " + ToStr(num));
  descr.autogeneratedName = true;
  descr.type = type;
  AddResourceCurChunk(descr);
}

void WrappedOpenGL::DerivedResource(GLResource parent, ResourceId child)
{
  ResourceId parentId = GetResourceManager()->GetOriginalID(GetResourceManager()->GetResID(parent));

  if(GetReplay()->GetResourceDesc(parentId).derivedResources.contains(child))
    return;

  GetReplay()->GetResourceDesc(parentId).derivedResources.push_back(child);
  GetReplay()->GetResourceDesc(child).parentResources.push_back(parentId);
}

void WrappedOpenGL::AddResourceCurChunk(ResourceDescription &descr)
{
  descr.initialisationChunks.push_back((uint32_t)m_StructuredFile->chunks.size() - 1);
}

void WrappedOpenGL::AddResourceCurChunk(ResourceId id)
{
  AddResourceCurChunk(GetReplay()->GetResourceDesc(id));
}

void WrappedOpenGL::AddResourceInitChunk(GLResource res)
{
  // don't add chunks that were recorded (some chunks are ambiguous)
  if(m_CurEventID == 0)
  {
    GLResourceManager *rm = GetResourceManager();
    AddResourceCurChunk(rm->GetOriginalID(rm->GetResID(res)));
  }
}

RDResult WrappedOpenGL::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  StreamReader *reader = rdc->ReadSection(sectionIdx);

  if(IsStructuredExporting(m_State))
  {
    // when structured exporting don't do any timebase conversion
    m_TimeBase = 0;
    m_TimeFrequency = 1.0;
  }
  else
  {
    m_TimeBase = rdc->GetTimestampBase();
    m_TimeFrequency = rdc->GetTimestampFrequency();
  }

  if(reader->IsErrored())
  {
    RDResult result = reader->GetError();
    delete reader;
    return result;
  }

  ReadSerialiser ser(reader, Ownership::Stream);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers, m_TimeBase, m_TimeFrequency);

  m_StructuredFile = &ser.GetStructuredFile();

  m_StoredStructuredData->version = m_StructuredFile->version = m_SectionVersion;

  ser.SetVersion(m_SectionVersion);

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

    if(reader->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    bool success = ProcessChunk(ser, context);

    ser.EndChunk();

    if(reader->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
      return m_FailedReplayResult;

    uint64_t offsetEnd = reader->GetOffset();

    RenderDoc::Inst().SetProgress(LoadProgress::FileInitialRead,
                                  float(offsetEnd) / float(reader->GetSize()));

    if((SystemChunk)context == SystemChunk::CaptureScope)
    {
      GetReplay()->WriteFrameRecord().frameInfo.fileOffset = offsetStart;

      // read the remaining data into memory and pass to immediate context
      frameDataSize = reader->GetSize() - reader->GetOffset();

      m_FrameReader = new StreamReader(reader, frameDataSize);

      rdcarray<DebugMessage> savedDebugMessages;

      // save any debug messages we built up
      savedDebugMessages.swap(m_DebugMessages);

      GetResourceManager()->ApplyInitialContents();

      // restore saved messages - which implicitly discards any generated while applying initial
      // contents
      savedDebugMessages.swap(m_DebugMessages);

      RDResult status = ContextReplayLog(m_State, 0, 0, false);

      if(status != ResultCode::Succeeded)
        return status;
    }

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offsetEnd - offsetStart;
    chunkInfos[context].count++;

    if((SystemChunk)context == SystemChunk::CaptureScope || reader->IsErrored() || reader->AtEnd())
      break;
  }

  if(m_ImplicitThreadSwitches > 2)
  {
    AddDebugMessage(
        MessageCategory::Performance, MessageSeverity::Medium, MessageSource::GeneralPerformance,
        StringFormat::Fmt(
            "%d implicit thread switches detected. Multithreaded submission from GL is not "
            "generally supported and is very inefficient to capture and replay.",
            m_ImplicitThreadSwitches));
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
  m_StructuredFile->Swap(*m_StoredStructuredData);

  // and in future use this file.
  m_StructuredFile = m_StoredStructuredData;

  GetReplay()->WriteFrameRecord().frameInfo.uncompressedFileSize =
      rdc->GetSectionProperties(sectionIdx).uncompressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.compressedFileSize =
      rdc->GetSectionProperties(sectionIdx).compressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.persistentSize = frameDataSize;
  GetReplay()->WriteFrameRecord().frameInfo.initDataSize =
      chunkInfos[(GLChunk)SystemChunk::InitialContents].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           GetReplay()->WriteFrameRecord().frameInfo.persistentSize);

  return ResultCode::Succeeded;
}

bool WrappedOpenGL::ProcessChunk(ReadSerialiser &ser, GLChunk chunk)
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

      SERIALISE_CHECK_READ_ERRORS();

      m_InitChunkIndex = (uint32_t)m_StructuredFile->chunks.size() - 1;

      return true;
    }
    else if(system == SystemChunk::InitialContentsList)
    {
      GetResourceManager()->CreateInitialContents(ser);

      SERIALISE_CHECK_READ_ERRORS();

      return true;
    }
    else if(system == SystemChunk::InitialContents)
    {
      return GetResourceManager()->Serialise_InitialState(ser, ResourceId(), NULL, NULL);
    }
    else if(system == SystemChunk::CaptureScope)
    {
      return Serialise_CaptureScope(ser);
    }
    else if(system == SystemChunk::CaptureEnd)
    {
      bool lastSwap =
          m_LastChunk == GLChunk::SwapBuffers || m_LastChunk == GLChunk::wglSwapBuffers ||
          m_LastChunk == GLChunk::glXSwapBuffers || m_LastChunk == GLChunk::CGLFlushDrawable ||
          m_LastChunk == GLChunk::eglSwapBuffers || m_LastChunk == GLChunk::eglPostSubBufferNV ||
          m_LastChunk == GLChunk::eglSwapBuffersWithDamageEXT ||
          m_LastChunk == GLChunk::eglSwapBuffersWithDamageKHR;

      if(IsLoading(m_State) && !lastSwap)
      {
        AddEvent();

        ActionDescription action;
        action.customName = "End of Capture";
        action.flags |= ActionFlags::Present;

        GLuint col = 0;
        GL.glGetNamedFramebufferAttachmentParameterivEXT(m_CurrentDefaultFBO, eGL_COLOR_ATTACHMENT0,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                         (GLint *)&col);

        action.copyDestination = GetResourceManager()->GetOriginalID(
            GetResourceManager()->GetResID(TextureRes(GetCtx(), col)));

        AddAction(action);
      }
      return true;
    }
    else if(system < SystemChunk::FirstDriverChunk)
    {
      RDCERR("Unexpected system chunk in capture data: %u", system);
      ser.SkipCurrentChunk();

      SERIALISE_CHECK_READ_ERRORS();

      return true;
    }
  }

  switch(chunk)
  {
    case GLChunk::DeviceInitialisation:
    {
      ResourceId vao, fbo;
      SERIALISE_ELEMENT(vao).Hidden();
      SERIALISE_ELEMENT(fbo).Named("FBO 0 ID"_lit);

      SERIALISE_CHECK_READ_ERRORS();

      if(IsReplayingAndReading())
      {
        // legacy behaviour where we had a single global VAO 0. Create a corresponding VAO so that
        // it can be bound and have initial contents applied to it
        if(vao != ResourceId())
        {
          glGenVertexArrays(1, &m_Global_VAO0);
          glBindVertexArray(m_Global_VAO0);
          GetResourceManager()->AddLiveResource(vao, VertexArrayRes(GetCtx(), m_Global_VAO0));
          AddResource(vao, ResourceType::StateObject, "Vertex Array");
          GetReplay()->GetResourceDesc(vao).SetCustomName("Default VAO");

          GetReplay()->GetResourceDesc(vao).initialisationChunks.push_back(m_InitChunkIndex);
        }

        // similar behaviour for a single global FBO 0.
        if(fbo != ResourceId())
          CreateReplayBackbuffer(m_GlobalInitParams, fbo, m_Global_FBO0, "Backbuffer");
      }

      return true;
    }
    case GLChunk::glContextInit: return Serialise_ContextInit(ser);

    case GLChunk::glGenBuffersARB:
    case GLChunk::glGenBuffers: return Serialise_glGenBuffers(ser, 0, 0);
    case GLChunk::glCreateBuffers: return Serialise_glCreateBuffers(ser, 0, 0);

    case GLChunk::glBufferStorage:
    case GLChunk::glBufferStorageEXT:
    case GLChunk::glNamedBufferStorage:
    case GLChunk::glNamedBufferStorageEXT:
      return Serialise_glNamedBufferStorageEXT(ser, 0, 0, 0, 0);
    case GLChunk::glBufferData:
    case GLChunk::glBufferDataARB:
    case GLChunk::glNamedBufferData:
    case GLChunk::glNamedBufferDataEXT:
      return Serialise_glNamedBufferDataEXT(ser, 0, 0, 0, eGL_NONE);
    case GLChunk::glBufferSubData:
    case GLChunk::glBufferSubDataARB:
    case GLChunk::glNamedBufferSubData:
    case GLChunk::glNamedBufferSubDataEXT:
      return Serialise_glNamedBufferSubDataEXT(ser, 0, 0, 0, 0);
    case GLChunk::glCopyBufferSubData:
    case GLChunk::glCopyNamedBufferSubData:
    case GLChunk::glNamedCopyBufferSubDataEXT:
      return Serialise_glNamedCopyBufferSubDataEXT(ser, 0, 0, 0, 0, 0);

    case GLChunk::glBindBufferARB:
    case GLChunk::glBindBuffer: return Serialise_glBindBuffer(ser, eGL_NONE, 0);
    case GLChunk::glBindBufferBaseEXT:
    case GLChunk::glBindBufferBase: return Serialise_glBindBufferBase(ser, eGL_NONE, 0, 0);
    case GLChunk::glBindBufferRangeEXT:
    case GLChunk::glBindBufferRange: return Serialise_glBindBufferRange(ser, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glBindBuffersBase: return Serialise_glBindBuffersBase(ser, eGL_NONE, 0, 0, 0);
    case GLChunk::glBindBuffersRange:
      return Serialise_glBindBuffersRange(ser, eGL_NONE, 0, 0, 0, 0, 0);

    case GLChunk::glUnmapBuffer:
    case GLChunk::glUnmapBufferARB:
    case GLChunk::glUnmapBufferOES:
    case GLChunk::glUnmapNamedBuffer:
    case GLChunk::glUnmapNamedBufferEXT: return Serialise_glUnmapNamedBufferEXT(ser, 0);
    case GLChunk::CoherentMapWrite:
    case GLChunk::glFlushMappedBufferRange:
    case GLChunk::glFlushMappedBufferRangeEXT:
    case GLChunk::glFlushMappedNamedBufferRange:
    case GLChunk::glFlushMappedNamedBufferRangeEXT:
      return Serialise_glFlushMappedNamedBufferRangeEXT(ser, 0, 0, 0);

    case GLChunk::glGenTransformFeedbacks: return Serialise_glGenTransformFeedbacks(ser, 0, 0);
    case GLChunk::glCreateTransformFeedbacks:
      return Serialise_glCreateTransformFeedbacks(ser, 0, 0);
    case GLChunk::glTransformFeedbackBufferBase:
      return Serialise_glTransformFeedbackBufferBase(ser, 0, 0, 0);
    case GLChunk::glTransformFeedbackBufferRange:
      return Serialise_glTransformFeedbackBufferRange(ser, 0, 0, 0, 0, 0);
    case GLChunk::glBindTransformFeedback:
      return Serialise_glBindTransformFeedback(ser, eGL_NONE, 0);
    case GLChunk::glBeginTransformFeedbackEXT:
    case GLChunk::glBeginTransformFeedback:
      return Serialise_glBeginTransformFeedback(ser, eGL_NONE);
    case GLChunk::glPauseTransformFeedback: return Serialise_glPauseTransformFeedback(ser);
    case GLChunk::glResumeTransformFeedback: return Serialise_glResumeTransformFeedback(ser);
    case GLChunk::glEndTransformFeedbackEXT:
    case GLChunk::glEndTransformFeedback: return Serialise_glEndTransformFeedback(ser);

    case GLChunk::glVertexAttribPointer:
    case GLChunk::glVertexAttribPointerARB:
    case GLChunk::glVertexArrayVertexAttribOffsetEXT:
      return Serialise_glVertexArrayVertexAttribOffsetEXT(ser, 0, 0, 0, 0, eGL_NONE, 0, 0, 0);
    case GLChunk::glVertexAttribIPointer:
    case GLChunk::glVertexAttribIPointerEXT:
    case GLChunk::glVertexArrayVertexAttribIOffsetEXT:
      return Serialise_glVertexArrayVertexAttribIOffsetEXT(ser, 0, 0, 0, 0, eGL_NONE, 0, 0);
    case GLChunk::glVertexAttribLPointer:
    case GLChunk::glVertexAttribLPointerEXT:
    case GLChunk::glVertexArrayVertexAttribLOffsetEXT:
      return Serialise_glVertexArrayVertexAttribLOffsetEXT(ser, 0, 0, 0, 0, eGL_NONE, 0, 0);
    case GLChunk::glVertexAttribBinding:
    case GLChunk::glVertexArrayAttribBinding:
    case GLChunk::glVertexArrayVertexAttribBindingEXT:
      return Serialise_glVertexArrayVertexAttribBindingEXT(ser, 0, 0, 0);
    case GLChunk::glVertexAttribFormat:
    case GLChunk::glVertexArrayAttribFormat:
    case GLChunk::glVertexArrayVertexAttribFormatEXT:
      return Serialise_glVertexArrayVertexAttribFormatEXT(ser, 0, 0, 0, eGL_NONE, 0, 0);
    case GLChunk::glVertexAttribIFormat:
    case GLChunk::glVertexArrayAttribIFormat:
    case GLChunk::glVertexArrayVertexAttribIFormatEXT:
      return Serialise_glVertexArrayVertexAttribIFormatEXT(ser, 0, 0, 0, eGL_NONE, 0);
    case GLChunk::glVertexAttribLFormat:
    case GLChunk::glVertexArrayAttribLFormat:
    case GLChunk::glVertexArrayVertexAttribLFormatEXT:
      return Serialise_glVertexArrayVertexAttribLFormatEXT(ser, 0, 0, 0, eGL_NONE, 0);
    case GLChunk::glVertexAttribDivisor:
    case GLChunk::glVertexAttribDivisorARB:
    case GLChunk::glVertexArrayVertexAttribDivisorEXT:
      return Serialise_glVertexArrayVertexAttribDivisorEXT(ser, 0, 0, 0);
    case GLChunk::glEnableVertexAttribArray:
    case GLChunk::glEnableVertexAttribArrayARB:
    case GLChunk::glEnableVertexArrayAttrib:
    case GLChunk::glEnableVertexArrayAttribEXT:
      return Serialise_glEnableVertexArrayAttribEXT(ser, 0, 0);
    case GLChunk::glDisableVertexAttribArray:
    case GLChunk::glDisableVertexAttribArrayARB:
    case GLChunk::glDisableVertexArrayAttrib:
    case GLChunk::glDisableVertexArrayAttribEXT:
      return Serialise_glDisableVertexArrayAttribEXT(ser, 0, 0);
    case GLChunk::glGenVertexArraysOES:
    case GLChunk::glGenVertexArrays: return Serialise_glGenVertexArrays(ser, 0, 0);
    case GLChunk::glCreateVertexArrays: return Serialise_glCreateVertexArrays(ser, 0, 0);
    case GLChunk::glBindVertexArrayOES:
    case GLChunk::glBindVertexArray: return Serialise_glBindVertexArray(ser, 0);
    case GLChunk::glVertexArrayElementBuffer:
      return Serialise_glVertexArrayElementBuffer(ser, 0, 0);
    case GLChunk::glBindVertexBuffer:
    case GLChunk::glVertexArrayVertexBuffer:
    case GLChunk::glVertexArrayBindVertexBufferEXT:
      return Serialise_glVertexArrayBindVertexBufferEXT(ser, 0, 0, 0, 0, 0);
    case GLChunk::glBindVertexBuffers:
    case GLChunk::glVertexArrayVertexBuffers:

      return Serialise_glVertexArrayVertexBuffers(ser, 0, 0, 0, 0, 0, 0);
    case GLChunk::glVertexBindingDivisor:
    case GLChunk::glVertexArrayBindingDivisor:
    case GLChunk::glVertexArrayVertexBindingDivisorEXT:
      return Serialise_glVertexArrayVertexBindingDivisorEXT(ser, 0, 0, 0);

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
      return Serialise_glVertexAttrib(ser, 0, 0, eGL_NONE, 0, 0, Attrib_typemask);

    case GLChunk::glLabelObjectEXT:
    case GLChunk::glObjectLabelKHR:
    case GLChunk::glObjectPtrLabel:
    case GLChunk::glObjectPtrLabelKHR:
    case GLChunk::glObjectLabel: return Serialise_glObjectLabel(ser, eGL_NONE, 0, 0, 0);
    case GLChunk::glDebugMessageInsertARB:
    case GLChunk::glDebugMessageInsertKHR:
    case GLChunk::glDebugMessageInsert:
      return Serialise_glDebugMessageInsert(ser, eGL_NONE, eGL_NONE, 0, eGL_NONE, 0, 0);
    case GLChunk::glStringMarkerGREMEDY:
    case GLChunk::glInsertEventMarkerEXT: return Serialise_glInsertEventMarkerEXT(ser, 0, 0);
    case GLChunk::glPushGroupMarkerEXT:
    case GLChunk::glPushDebugGroupKHR:
    case GLChunk::glPushDebugGroup: return Serialise_glPushDebugGroup(ser, eGL_NONE, 0, 0, 0);
    case GLChunk::glPopGroupMarkerEXT:
    case GLChunk::glPopDebugGroupKHR:
    case GLChunk::glPopDebugGroup: return Serialise_glPopDebugGroup(ser);

    case GLChunk::glDispatchCompute: return Serialise_glDispatchCompute(ser, 0, 0, 0);
    case GLChunk::glDispatchComputeGroupSizeARB:
      return Serialise_glDispatchComputeGroupSizeARB(ser, 0, 0, 0, 0, 0, 0);
    case GLChunk::glDispatchComputeIndirect: return Serialise_glDispatchComputeIndirect(ser, 0);
    case GLChunk::glMemoryBarrierEXT:
    case GLChunk::glMemoryBarrier: return Serialise_glMemoryBarrier(ser, 0);
    case GLChunk::glMemoryBarrierByRegion: return Serialise_glMemoryBarrierByRegion(ser, 0);
    case GLChunk::glTextureBarrier: return Serialise_glTextureBarrier(ser);
    case GLChunk::glDrawTransformFeedback:
      return Serialise_glDrawTransformFeedback(ser, eGL_NONE, 0);
    case GLChunk::glDrawTransformFeedbackInstanced:
      return Serialise_glDrawTransformFeedbackInstanced(ser, eGL_NONE, 0, 0);
    case GLChunk::glDrawTransformFeedbackStream:
      return Serialise_glDrawTransformFeedbackStream(ser, eGL_NONE, 0, 0);
    case GLChunk::glDrawTransformFeedbackStreamInstanced:
      return Serialise_glDrawTransformFeedbackStreamInstanced(ser, eGL_NONE, 0, 0, 0);
    case GLChunk::glDrawArrays: return Serialise_glDrawArrays(ser, eGL_NONE, 0, 0);
    case GLChunk::glDrawArraysIndirect: return Serialise_glDrawArraysIndirect(ser, eGL_NONE, 0);
    case GLChunk::glDrawArraysInstancedARB:
    case GLChunk::glDrawArraysInstancedEXT:
    case GLChunk::glDrawArraysInstanced:
      return Serialise_glDrawArraysInstanced(ser, eGL_NONE, 0, 0, 0);
    case GLChunk::glDrawArraysInstancedBaseInstanceEXT:
    case GLChunk::glDrawArraysInstancedBaseInstance:
      return Serialise_glDrawArraysInstancedBaseInstance(ser, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glDrawElements: return Serialise_glDrawElements(ser, eGL_NONE, 0, eGL_NONE, 0);
    case GLChunk::glDrawElementsIndirect:
      return Serialise_glDrawElementsIndirect(ser, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glDrawRangeElementsEXT:
    case GLChunk::glDrawRangeElements:
      return Serialise_glDrawRangeElements(ser, eGL_NONE, 0, 0, 0, eGL_NONE, 0);
    case GLChunk::glDrawRangeElementsBaseVertexEXT:
    case GLChunk::glDrawRangeElementsBaseVertexOES:
    case GLChunk::glDrawRangeElementsBaseVertex:
      return Serialise_glDrawRangeElementsBaseVertex(ser, eGL_NONE, 0, 0, 0, eGL_NONE, 0, 0);
    case GLChunk::glDrawElementsBaseVertexEXT:
    case GLChunk::glDrawElementsBaseVertexOES:
    case GLChunk::glDrawElementsBaseVertex:
      return Serialise_glDrawElementsBaseVertex(ser, eGL_NONE, 0, eGL_NONE, 0, 0);
    case GLChunk::glDrawElementsInstancedARB:
    case GLChunk::glDrawElementsInstancedEXT:
    case GLChunk::glDrawElementsInstanced:
      return Serialise_glDrawElementsInstanced(ser, eGL_NONE, 0, eGL_NONE, 0, 0);
    case GLChunk::glDrawElementsInstancedBaseInstanceEXT:
    case GLChunk::glDrawElementsInstancedBaseInstance:
      return Serialise_glDrawElementsInstancedBaseInstance(ser, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
    case GLChunk::glDrawElementsInstancedBaseVertexEXT:
    case GLChunk::glDrawElementsInstancedBaseVertexOES:
    case GLChunk::glDrawElementsInstancedBaseVertex:
      return Serialise_glDrawElementsInstancedBaseVertex(ser, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
    case GLChunk::glDrawElementsInstancedBaseVertexBaseInstanceEXT:
    case GLChunk::glDrawElementsInstancedBaseVertexBaseInstance:
      return Serialise_glDrawElementsInstancedBaseVertexBaseInstance(ser, eGL_NONE, 0, eGL_NONE, 0,
                                                                     0, 0, 0);
    case GLChunk::glMultiDrawArraysEXT:
    case GLChunk::glMultiDrawArrays: return Serialise_glMultiDrawArrays(ser, eGL_NONE, 0, 0, 0);
    case GLChunk::glMultiDrawElements:
      return Serialise_glMultiDrawElements(ser, eGL_NONE, 0, eGL_NONE, 0, 0);
    case GLChunk::glMultiDrawElementsBaseVertexEXT:
    case GLChunk::glMultiDrawElementsBaseVertexOES:
    case GLChunk::glMultiDrawElementsBaseVertex:
      return Serialise_glMultiDrawElementsBaseVertex(ser, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
    case GLChunk::glMultiDrawArraysIndirect:
      return Serialise_glMultiDrawArraysIndirect(ser, eGL_NONE, 0, 0, 0);
    case GLChunk::glMultiDrawElementsIndirect:
      return Serialise_glMultiDrawElementsIndirect(ser, eGL_NONE, eGL_NONE, 0, 0, 0);
    case GLChunk::glMultiDrawArraysIndirectCountARB:
    case GLChunk::glMultiDrawArraysIndirectCount:
      return Serialise_glMultiDrawArraysIndirectCount(ser, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glMultiDrawElementsIndirectCountARB:
    case GLChunk::glMultiDrawElementsIndirectCount:
      return Serialise_glMultiDrawElementsIndirectCount(ser, eGL_NONE, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glClearBufferfv:
    case GLChunk::glClearNamedFramebufferfv:
      return Serialise_glClearNamedFramebufferfv(ser, 0, eGL_NONE, 0, 0);
    case GLChunk::glClearBufferiv:
    case GLChunk::glClearNamedFramebufferiv:
      return Serialise_glClearNamedFramebufferiv(ser, 0, eGL_NONE, 0, 0);
    case GLChunk::glClearBufferuiv:
    case GLChunk::glClearNamedFramebufferuiv:
      return Serialise_glClearNamedFramebufferuiv(ser, 0, eGL_NONE, 0, 0);
    case GLChunk::glClearBufferfi:
    case GLChunk::glClearNamedFramebufferfi:
      return Serialise_glClearNamedFramebufferfi(ser, 0, eGL_NONE, 0, 0, 0);
    case GLChunk::glClearBufferData:
    case GLChunk::glClearNamedBufferData:
    case GLChunk::glClearNamedBufferDataEXT:
      return Serialise_glClearNamedBufferDataEXT(ser, 0, eGL_NONE, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glClearBufferSubData:
    case GLChunk::glClearNamedBufferSubData:
    case GLChunk::glClearNamedBufferSubDataEXT:
      return Serialise_glClearNamedBufferSubDataEXT(ser, 0, eGL_NONE, 0, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glClear: return Serialise_glClear(ser, 0);
    case GLChunk::glClearTexImage:
      return Serialise_glClearTexImage(ser, 0, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glClearTexSubImage:
      return Serialise_glClearTexSubImage(ser, 0, 0, 0, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);

    case GLChunk::glGenFramebuffersEXT:
    case GLChunk::glGenFramebuffers: return Serialise_glGenFramebuffers(ser, 0, 0);
    case GLChunk::glCreateFramebuffers: return Serialise_glCreateFramebuffers(ser, 0, 0);
    case GLChunk::glFramebufferTexture:
    case GLChunk::glFramebufferTextureOES:
    case GLChunk::glFramebufferTextureARB:
    case GLChunk::glFramebufferTextureEXT:
    case GLChunk::glNamedFramebufferTexture:
    case GLChunk::glNamedFramebufferTextureEXT:
      return Serialise_glNamedFramebufferTextureEXT(ser, 0, eGL_NONE, 0, 0);
    case GLChunk::glFramebufferTexture1D:
    case GLChunk::glFramebufferTexture1DEXT:
    case GLChunk::glNamedFramebufferTexture1DEXT:
      return Serialise_glNamedFramebufferTexture1DEXT(ser, 0, eGL_NONE, eGL_NONE, 0, 0);
    case GLChunk::glFramebufferTexture2D:
    case GLChunk::glFramebufferTexture2DEXT:
    case GLChunk::glNamedFramebufferTexture2DEXT:
      return Serialise_glNamedFramebufferTexture2DEXT(ser, 0, eGL_NONE, eGL_NONE, 0, 0);
    case GLChunk::glFramebufferTexture2DMultisampleEXT:
      return Serialise_glFramebufferTexture2DMultisampleEXT(ser, 0, eGL_NONE, eGL_NONE, eGL_NONE, 0,
                                                            0, 0);
    case GLChunk::glFramebufferTexture3D:
    case GLChunk::glFramebufferTexture3DEXT:
    case GLChunk::glFramebufferTexture3DOES:
    case GLChunk::glNamedFramebufferTexture3DEXT:
      return Serialise_glNamedFramebufferTexture3DEXT(ser, 0, eGL_NONE, eGL_NONE, 0, 0, 0);
    case GLChunk::glFramebufferRenderbuffer:
    case GLChunk::glFramebufferRenderbufferEXT:
    case GLChunk::glNamedFramebufferRenderbuffer:
    case GLChunk::glNamedFramebufferRenderbufferEXT:
      return Serialise_glNamedFramebufferRenderbufferEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glFramebufferTextureLayer:
    case GLChunk::glFramebufferTextureLayerARB:
    case GLChunk::glFramebufferTextureLayerEXT:
    case GLChunk::glNamedFramebufferTextureLayer:
    case GLChunk::glNamedFramebufferTextureLayerEXT:
      return Serialise_glNamedFramebufferTextureLayerEXT(ser, 0, eGL_NONE, 0, 0, 0);
    case GLChunk::glFramebufferTextureMultiviewOVR:
      return Serialise_glFramebufferTextureMultiviewOVR(ser, eGL_NONE, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glFramebufferTextureMultisampleMultiviewOVR:
      return Serialise_glFramebufferTextureMultisampleMultiviewOVR(ser, eGL_NONE, eGL_NONE, 0, 0, 0,
                                                                   0, 0);
    case GLChunk::glTextureFoveationParametersQCOM:
      return Serialise_glTextureFoveationParametersQCOM(ser, eGL_NONE, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
                                                        0.0f);
    case GLChunk::glFramebufferParameteri:
    case GLChunk::glNamedFramebufferParameteri:
    case GLChunk::glNamedFramebufferParameteriEXT:
      return Serialise_glNamedFramebufferParameteriEXT(ser, 0, eGL_NONE, 0);
    case GLChunk::glReadBuffer:
    case GLChunk::glNamedFramebufferReadBuffer:
    case GLChunk::glFramebufferReadBufferEXT:
      return Serialise_glFramebufferReadBufferEXT(ser, 0, eGL_NONE);
    case GLChunk::glBindFramebufferEXT:
    case GLChunk::glBindFramebuffer: return Serialise_glBindFramebuffer(ser, eGL_NONE, 0);
    case GLChunk::glDiscardFramebufferEXT:
    case GLChunk::glInvalidateFramebuffer:
    case GLChunk::glInvalidateNamedFramebufferData:
      return Serialise_glInvalidateNamedFramebufferData(ser, 0, 0, 0);
    case GLChunk::glDrawBuffer:
    case GLChunk::glNamedFramebufferDrawBuffer:
    case GLChunk::glFramebufferDrawBufferEXT:
      return Serialise_glFramebufferDrawBufferEXT(ser, 0, eGL_NONE);
    case GLChunk::glDrawBuffers:
    case GLChunk::glDrawBuffersARB:
    case GLChunk::glDrawBuffersEXT:
    case GLChunk::glNamedFramebufferDrawBuffers:
    case GLChunk::glFramebufferDrawBuffersEXT:
      return Serialise_glFramebufferDrawBuffersEXT(ser, 0, 0, 0);
    case GLChunk::glBlitFramebuffer:
    case GLChunk::glBlitFramebufferEXT:
    case GLChunk::glBlitNamedFramebuffer:
      return Serialise_glBlitNamedFramebuffer(ser, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, eGL_NONE);
    case GLChunk::glGenRenderbuffersEXT:
    case GLChunk::glGenRenderbuffers: return Serialise_glGenRenderbuffers(ser, 0, 0);
    case GLChunk::glCreateRenderbuffers: return Serialise_glCreateRenderbuffers(ser, 0, 0);
    case GLChunk::glRenderbufferStorage:
    case GLChunk::glRenderbufferStorageEXT:
    case GLChunk::glNamedRenderbufferStorage:
    case GLChunk::glNamedRenderbufferStorageEXT:
      return Serialise_glNamedRenderbufferStorageEXT(ser, 0, eGL_NONE, 0, 0);
    case GLChunk::glRenderbufferStorageMultisample:
    case GLChunk::glNamedRenderbufferStorageMultisample:
    case GLChunk::glNamedRenderbufferStorageMultisampleEXT:
      return Serialise_glNamedRenderbufferStorageMultisampleEXT(ser, 0, 0, eGL_NONE, 0, 0);

    // needs to be separate from glRenderbufferStorageMultisample due to driver issues
    case GLChunk::glRenderbufferStorageMultisampleEXT:
      return Serialise_glRenderbufferStorageMultisampleEXT(ser, 0, 0, eGL_NONE, 0, 0);

    case GLChunk::wglDXRegisterObjectNV:
      return Serialise_wglDXRegisterObjectNV(ser, GLResource(MakeNullResource), eGL_NONE, 0);
    case GLChunk::wglDXLockObjectsNV:
      return Serialise_wglDXLockObjectsNV(ser, GLResource(MakeNullResource));

    case GLChunk::glFenceSync: return Serialise_glFenceSync(ser, 0, eGL_NONE, 0);
    case GLChunk::glClientWaitSync: return Serialise_glClientWaitSync(ser, 0, 0, 0);
    case GLChunk::glWaitSync: return Serialise_glWaitSync(ser, 0, 0, 0);
    case GLChunk::glGenQueriesARB:
    case GLChunk::glGenQueriesEXT:
    case GLChunk::glGenQueries: return Serialise_glGenQueries(ser, 0, 0);
    case GLChunk::glCreateQueries: return Serialise_glCreateQueries(ser, eGL_NONE, 0, 0);
    case GLChunk::glBeginQueryARB:
    case GLChunk::glBeginQueryEXT:
    case GLChunk::glBeginQuery: return Serialise_glBeginQuery(ser, eGL_NONE, 0);
    case GLChunk::glBeginQueryIndexed: return Serialise_glBeginQueryIndexed(ser, eGL_NONE, 0, 0);
    case GLChunk::glEndQueryARB:
    case GLChunk::glEndQueryEXT:
    case GLChunk::glEndQuery: return Serialise_glEndQuery(ser, eGL_NONE);
    case GLChunk::glEndQueryIndexed: return Serialise_glEndQueryIndexed(ser, eGL_NONE, 0);
    case GLChunk::glBeginConditionalRender:
      return Serialise_glBeginConditionalRender(ser, 0, eGL_NONE);
    case GLChunk::glEndConditionalRender: return Serialise_glEndConditionalRender(ser);
    case GLChunk::glQueryCounterEXT:
    case GLChunk::glQueryCounter: return Serialise_glQueryCounter(ser, 0, eGL_NONE);

    case GLChunk::glGenSamplers: return Serialise_glGenSamplers(ser, 0, 0);
    case GLChunk::glCreateSamplers: return Serialise_glCreateSamplers(ser, 0, 0);
    case GLChunk::glBindSampler: return Serialise_glBindSampler(ser, 0, 0);
    case GLChunk::glBindSamplers: return Serialise_glBindSamplers(ser, 0, 0, 0);
    case GLChunk::glSamplerParameteri: return Serialise_glSamplerParameteri(ser, 0, eGL_NONE, 0);
    case GLChunk::glSamplerParameterf: return Serialise_glSamplerParameterf(ser, 0, eGL_NONE, 0);
    case GLChunk::glSamplerParameteriv: return Serialise_glSamplerParameteriv(ser, 0, eGL_NONE, 0);
    case GLChunk::glSamplerParameterfv: return Serialise_glSamplerParameterfv(ser, 0, eGL_NONE, 0);
    case GLChunk::glSamplerParameterIivEXT:
    case GLChunk::glSamplerParameterIivOES:
    case GLChunk::glSamplerParameterIiv:
      return Serialise_glSamplerParameterIiv(ser, 0, eGL_NONE, 0);
    case GLChunk::glSamplerParameterIuivEXT:
    case GLChunk::glSamplerParameterIuivOES:
    case GLChunk::glSamplerParameterIuiv:
      return Serialise_glSamplerParameterIuiv(ser, 0, eGL_NONE, 0);

    case GLChunk::glCreateShader: return Serialise_glCreateShader(ser, eGL_NONE, 0);
    case GLChunk::glShaderSource: return Serialise_glShaderSource(ser, 0, 0, 0, 0);
    case GLChunk::glCompileShader: return Serialise_glCompileShader(ser, 0);
    case GLChunk::glAttachShader: return Serialise_glAttachShader(ser, 0, 0);
    case GLChunk::glDetachShader: return Serialise_glDetachShader(ser, 0, 0);
    case GLChunk::glCreateShaderProgramvEXT:
    case GLChunk::glCreateShaderProgramv:
      return Serialise_glCreateShaderProgramv(ser, eGL_NONE, 0, 0, 0);
    case GLChunk::glCreateProgram: return Serialise_glCreateProgram(ser, 0);
    case GLChunk::glLinkProgram: return Serialise_glLinkProgram(ser, 0);
    case GLChunk::glUniformBlockBinding: return Serialise_glUniformBlockBinding(ser, 0, 0, 0);
    case GLChunk::glShaderStorageBlockBinding:
      return Serialise_glShaderStorageBlockBinding(ser, 0, 0, 0);
    case GLChunk::glBindAttribLocation: return Serialise_glBindAttribLocation(ser, 0, 0, 0);
    case GLChunk::glBindFragDataLocationEXT:
    case GLChunk::glBindFragDataLocation: return Serialise_glBindFragDataLocation(ser, 0, 0, 0);
    case GLChunk::glUniformSubroutinesuiv:
      return Serialise_glUniformSubroutinesuiv(ser, eGL_NONE, 0, 0);
    case GLChunk::glBindFragDataLocationIndexed:
      return Serialise_glBindFragDataLocationIndexed(ser, 0, 0, 0, 0);
    case GLChunk::glTransformFeedbackVaryingsEXT:
    case GLChunk::glTransformFeedbackVaryings:
      return Serialise_glTransformFeedbackVaryings(ser, 0, 0, 0, eGL_NONE);
    case GLChunk::glProgramParameteriARB:
    case GLChunk::glProgramParameteriEXT:
    case GLChunk::glProgramParameteri: return Serialise_glProgramParameteri(ser, 0, eGL_NONE, 0);
    case GLChunk::glUseProgram: return Serialise_glUseProgram(ser, 0);
    case GLChunk::glUseProgramStagesEXT:
    case GLChunk::glUseProgramStages: return Serialise_glUseProgramStages(ser, 0, 0, 0);
    case GLChunk::glGenProgramPipelinesEXT:
    case GLChunk::glGenProgramPipelines: return Serialise_glGenProgramPipelines(ser, 0, 0);
    case GLChunk::glCreateProgramPipelines: return Serialise_glCreateProgramPipelines(ser, 0, 0);
    case GLChunk::glBindProgramPipelineEXT:
    case GLChunk::glBindProgramPipeline: return Serialise_glBindProgramPipeline(ser, 0);
    case GLChunk::glCompileShaderIncludeARB:
      return Serialise_glCompileShaderIncludeARB(ser, 0, 0, 0, 0);
    case GLChunk::glNamedStringARB: return Serialise_glNamedStringARB(ser, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glDeleteNamedStringARB: return Serialise_glDeleteNamedStringARB(ser, 0, 0);

    case GLChunk::glBlendFunc: return Serialise_glBlendFunc(ser, eGL_NONE, eGL_NONE);
    case GLChunk::glBlendFunciARB:
    case GLChunk::glBlendFunciEXT:
    case GLChunk::glBlendFunciOES:
    case GLChunk::glBlendFunci: return Serialise_glBlendFunci(ser, 0, eGL_NONE, eGL_NONE);
    case GLChunk::glBlendColorEXT:
    case GLChunk::glBlendColor: return Serialise_glBlendColor(ser, 0, 0, 0, 0);
    case GLChunk::glBlendFuncSeparateARB:
    case GLChunk::glBlendFuncSeparate:
      return Serialise_glBlendFuncSeparate(ser, eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
    case GLChunk::glBlendFuncSeparateiARB:
    case GLChunk::glBlendFuncSeparateiEXT:
    case GLChunk::glBlendFuncSeparateiOES:
    case GLChunk::glBlendFuncSeparatei:
      return Serialise_glBlendFuncSeparatei(ser, 0, eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
    case GLChunk::glBlendEquationEXT:
    case GLChunk::glBlendEquationARB:
    case GLChunk::glBlendEquation: return Serialise_glBlendEquation(ser, eGL_NONE);
    case GLChunk::glBlendEquationiARB:
    case GLChunk::glBlendEquationiEXT:
    case GLChunk::glBlendEquationiOES:
    case GLChunk::glBlendEquationi: return Serialise_glBlendEquationi(ser, 0, eGL_NONE);
    case GLChunk::glBlendEquationSeparateARB:
    case GLChunk::glBlendEquationSeparateEXT:
    case GLChunk::glBlendEquationSeparate:
      return Serialise_glBlendEquationSeparate(ser, eGL_NONE, eGL_NONE);
    case GLChunk::glBlendEquationSeparateiARB:
    case GLChunk::glBlendEquationSeparateiEXT:
    case GLChunk::glBlendEquationSeparateiOES:
    case GLChunk::glBlendEquationSeparatei:
      return Serialise_glBlendEquationSeparatei(ser, 0, eGL_NONE, eGL_NONE);
    case GLChunk::glBlendBarrier:
    case GLChunk::glBlendBarrierKHR: return Serialise_glBlendBarrierKHR(ser);
    case GLChunk::glLogicOp: return Serialise_glLogicOp(ser, eGL_NONE);
    case GLChunk::glStencilFunc: return Serialise_glStencilFunc(ser, eGL_NONE, 0, 0);
    case GLChunk::glStencilFuncSeparate:
      return Serialise_glStencilFuncSeparate(ser, eGL_NONE, eGL_NONE, 0, 0);
    case GLChunk::glStencilMask: return Serialise_glStencilMask(ser, 0);
    case GLChunk::glStencilMaskSeparate: return Serialise_glStencilMaskSeparate(ser, eGL_NONE, 0);
    case GLChunk::glStencilOp: return Serialise_glStencilOp(ser, eGL_NONE, eGL_NONE, eGL_NONE);
    case GLChunk::glStencilOpSeparate:
      return Serialise_glStencilOpSeparate(ser, eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
    case GLChunk::glClearColor: return Serialise_glClearColor(ser, 0, 0, 0, 0);
    case GLChunk::glClearStencil: return Serialise_glClearStencil(ser, 0);
    case GLChunk::glClearDepthf:
    case GLChunk::glClearDepth: return Serialise_glClearDepth(ser, 0);
    case GLChunk::glDepthFunc: return Serialise_glDepthFunc(ser, eGL_NONE);
    case GLChunk::glDepthMask: return Serialise_glDepthMask(ser, 0);
    case GLChunk::glDepthRange: return Serialise_glDepthRange(ser, 0, 0);
    case GLChunk::glDepthRangef: return Serialise_glDepthRangef(ser, 0, 0);
    case GLChunk::glDepthRangeIndexedfNV:
    case GLChunk::glDepthRangeIndexedfOES:
    case GLChunk::glDepthRangeIndexed: return Serialise_glDepthRangeIndexed(ser, 0, 0, 0);
    case GLChunk::glDepthRangeArrayfvNV:
    case GLChunk::glDepthRangeArrayfvOES:
    case GLChunk::glDepthRangeArrayv: return Serialise_glDepthRangeArrayv(ser, 0, 0, 0);
    case GLChunk::glDepthBoundsEXT: return Serialise_glDepthBoundsEXT(ser, 0, 0);
    case GLChunk::glClipControl:
    case GLChunk::glClipControlEXT: return Serialise_glClipControl(ser, eGL_NONE, eGL_NONE);
    case GLChunk::glProvokingVertexEXT:
    case GLChunk::glProvokingVertex: return Serialise_glProvokingVertex(ser, eGL_NONE);
    case GLChunk::glPrimitiveRestartIndex: return Serialise_glPrimitiveRestartIndex(ser, 0);
    case GLChunk::glDisable: return Serialise_glDisable(ser, eGL_NONE);
    case GLChunk::glEnable: return Serialise_glEnable(ser, eGL_NONE);
    case GLChunk::glDisableiEXT:
    case GLChunk::glDisableIndexedEXT:
    case GLChunk::glDisableiNV:
    case GLChunk::glDisableiOES:
    case GLChunk::glDisablei: return Serialise_glDisablei(ser, eGL_NONE, 0);
    case GLChunk::glEnableiEXT:
    case GLChunk::glEnableIndexedEXT:
    case GLChunk::glEnableiNV:
    case GLChunk::glEnableiOES:
    case GLChunk::glEnablei: return Serialise_glEnablei(ser, eGL_NONE, 0);
    case GLChunk::glFrontFace: return Serialise_glFrontFace(ser, eGL_NONE);
    case GLChunk::glCullFace: return Serialise_glCullFace(ser, eGL_NONE);
    case GLChunk::glHint: return Serialise_glHint(ser, eGL_NONE, eGL_NONE);
    case GLChunk::glColorMask: return Serialise_glColorMask(ser, 0, 0, 0, 0);
    case GLChunk::glColorMaskiEXT:
    case GLChunk::glColorMaskIndexedEXT:
    case GLChunk::glColorMaskiOES:
    case GLChunk::glColorMaski: return Serialise_glColorMaski(ser, 0, 0, 0, 0, 0);
    case GLChunk::glSampleMaski: return Serialise_glSampleMaski(ser, 0, 0);
    case GLChunk::glSampleCoverageARB:
    case GLChunk::glSampleCoverage: return Serialise_glSampleCoverage(ser, 0, 0);
    case GLChunk::glMinSampleShadingARB:
    case GLChunk::glMinSampleShadingOES:
    case GLChunk::glMinSampleShading: return Serialise_glMinSampleShading(ser, 0);
    case GLChunk::glRasterSamplesEXT: return Serialise_glRasterSamplesEXT(ser, 0, 0);
    case GLChunk::glPatchParameteri: return Serialise_glPatchParameteri(ser, eGL_NONE, 0);
    case GLChunk::glPatchParameterfv: return Serialise_glPatchParameterfv(ser, eGL_NONE, 0);
    case GLChunk::glLineWidth: return Serialise_glLineWidth(ser, 0);
    case GLChunk::glPointSize: return Serialise_glPointSize(ser, 0);
    case GLChunk::glPatchParameteriEXT:
    case GLChunk::glPatchParameteriOES:
    case GLChunk::glPointParameteri: return Serialise_glPointParameteri(ser, eGL_NONE, 0);
    case GLChunk::glPointParameteriv: return Serialise_glPointParameteriv(ser, eGL_NONE, 0);
    case GLChunk::glPointParameterfARB:
    case GLChunk::glPointParameterfEXT:
    case GLChunk::glPointParameterf: return Serialise_glPointParameterf(ser, eGL_NONE, 0);
    case GLChunk::glPointParameterfvARB:
    case GLChunk::glPointParameterfvEXT:
    case GLChunk::glPointParameterfv: return Serialise_glPointParameterfv(ser, eGL_NONE, 0);
    case GLChunk::glViewport: return Serialise_glViewport(ser, 0, 0, 0, 0);
    case GLChunk::glViewportArrayvNV:
    case GLChunk::glViewportArrayvOES:
    case GLChunk::glViewportIndexedf:
    case GLChunk::glViewportIndexedfNV:
    case GLChunk::glViewportIndexedfOES:
    case GLChunk::glViewportIndexedfv:
    case GLChunk::glViewportIndexedfvNV:
    case GLChunk::glViewportIndexedfvOES:
    case GLChunk::glViewportArrayv: return Serialise_glViewportArrayv(ser, 0, 0, 0);
    case GLChunk::glScissor: return Serialise_glScissor(ser, 0, 0, 0, 0);
    case GLChunk::glScissorArrayvNV:
    case GLChunk::glScissorArrayvOES:
    case GLChunk::glScissorIndexed:
    case GLChunk::glScissorIndexedNV:
    case GLChunk::glScissorIndexedOES:
    case GLChunk::glScissorIndexedv:
    case GLChunk::glScissorIndexedvNV:
    case GLChunk::glScissorIndexedvOES:
    case GLChunk::glScissorArrayv: return Serialise_glScissorArrayv(ser, 0, 0, 0);
    case GLChunk::glPolygonMode: return Serialise_glPolygonMode(ser, eGL_NONE, eGL_NONE);
    case GLChunk::glPolygonOffset: return Serialise_glPolygonOffset(ser, 0, 0);
    case GLChunk::glPolygonOffsetClampEXT:
    case GLChunk::glPolygonOffsetClamp: return Serialise_glPolygonOffsetClamp(ser, 0, 0, 0);
    case GLChunk::glPrimitiveBoundingBoxEXT:
    case GLChunk::glPrimitiveBoundingBoxOES:
    case GLChunk::glPrimitiveBoundingBoxARB:
    case GLChunk::glPrimitiveBoundingBox:
      return Serialise_glPrimitiveBoundingBox(ser, 0, 0, 0, 0, 0, 0, 0, 0);

    case GLChunk::glGenTextures: return Serialise_glGenTextures(ser, 0, 0);
    case GLChunk::glCreateTextures: return Serialise_glCreateTextures(ser, eGL_NONE, 0, 0);
    case GLChunk::glBindTexture: return Serialise_glBindTexture(ser, eGL_NONE, 0);
    case GLChunk::glBindTextures: return Serialise_glBindTextures(ser, 0, 0, 0);
    case GLChunk::glBindMultiTextureEXT:
      return Serialise_glBindMultiTextureEXT(ser, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glBindTextureUnit: return Serialise_glBindTextureUnit(ser, 0, 0);
    case GLChunk::glBindImageTextureEXT:
    case GLChunk::glBindImageTexture:
      return Serialise_glBindImageTexture(ser, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE);
    case GLChunk::glBindImageTextures: return Serialise_glBindImageTextures(ser, 0, 0, 0);
    case GLChunk::glTextureViewEXT:
    case GLChunk::glTextureViewOES:
    case GLChunk::glTextureView:
      return Serialise_glTextureView(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glGenerateMipmap:
    case GLChunk::glGenerateMipmapEXT:
    case GLChunk::glGenerateMultiTexMipmapEXT:
    case GLChunk::glGenerateTextureMipmap:
    case GLChunk::glGenerateTextureMipmapEXT:
      return Serialise_glGenerateTextureMipmapEXT(ser, 0, eGL_NONE);
    case GLChunk::glCopyImageSubDataEXT:
    case GLChunk::glCopyImageSubDataOES:
    case GLChunk::glCopyImageSubData:
      return Serialise_glCopyImageSubData(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, 0, 0, 0, 0, 0,
                                          0, 0);
    case GLChunk::glCopyMultiTexSubImage1DEXT:
    case GLChunk::glCopyTexSubImage1D:
    case GLChunk::glCopyTextureSubImage1D:
    case GLChunk::glCopyTextureSubImage1DEXT:
      return Serialise_glCopyTextureSubImage1DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0);
    case GLChunk::glCopyTexSubImage2D:
    case GLChunk::glCopyTextureSubImage2D:
    case GLChunk::glCopyMultiTexSubImage2DEXT:
    case GLChunk::glCopyTextureSubImage2DEXT:
      return Serialise_glCopyTextureSubImage2DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0);
    case GLChunk::glCopyMultiTexSubImage3DEXT:
    case GLChunk::glCopyTexSubImage3D:
    case GLChunk::glCopyTexSubImage3DOES:
    case GLChunk::glCopyTextureSubImage3D:
    case GLChunk::glCopyTextureSubImage3DEXT:
      return Serialise_glCopyTextureSubImage3DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, 0);
    case GLChunk::glMultiTexParameteriEXT:
    case GLChunk::glTexParameteri:
    case GLChunk::glTextureParameteri:
    case GLChunk::glTextureParameteriEXT:
      return Serialise_glTextureParameteriEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glMultiTexParameterivEXT:
    case GLChunk::glTexParameteriv:
    case GLChunk::glTextureParameteriv:
    case GLChunk::glTextureParameterivEXT:
      return Serialise_glTextureParameterivEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glMultiTexParameterIivEXT:
    case GLChunk::glTexParameterIiv:
    case GLChunk::glTexParameterIivEXT:
    case GLChunk::glTexParameterIivOES:
    case GLChunk::glTextureParameterIiv:
    case GLChunk::glTextureParameterIivEXT:
      return Serialise_glTextureParameterIivEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glMultiTexParameterIuivEXT:
    case GLChunk::glTexParameterIuiv:
    case GLChunk::glTexParameterIuivEXT:
    case GLChunk::glTexParameterIuivOES:
    case GLChunk::glTextureParameterIuiv:
    case GLChunk::glTextureParameterIuivEXT:
      return Serialise_glTextureParameterIuivEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glMultiTexParameterfEXT:
    case GLChunk::glTexParameterf:
    case GLChunk::glTextureParameterf:
    case GLChunk::glTextureParameterfEXT:
      return Serialise_glTextureParameterfEXT(ser, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glMultiTexParameterfvEXT:
    case GLChunk::glTexParameterfv:
    case GLChunk::glTextureParameterfv:
    case GLChunk::glTextureParameterfvEXT:
      return Serialise_glTextureParameterfvEXT(ser, 0, eGL_NONE, eGL_NONE, 0);

    case GLChunk::glPixelStoref:
    case GLChunk::glPixelStorei: return Serialise_glPixelStorei(ser, eGL_NONE, 0);
    case GLChunk::glActiveTextureARB:
    case GLChunk::glActiveTexture: return Serialise_glActiveTexture(ser, eGL_NONE);
    case GLChunk::glMultiTexImage1DEXT:
    case GLChunk::glTexImage1D:
    case GLChunk::glTextureImage1DEXT:
      return Serialise_glTextureImage1DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glMultiTexImage2DEXT:
    case GLChunk::glTexImage2D:
    case GLChunk::glTextureImage2DEXT:
      return Serialise_glTextureImage2DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glMultiTexImage3DEXT:
    case GLChunk::glTexImage3D:
    case GLChunk::glTexImage3DEXT:
    case GLChunk::glTexImage3DOES:
    case GLChunk::glTextureImage3DEXT:
      return Serialise_glTextureImage3DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);

    case GLChunk::glCompressedMultiTexImage1DEXT:
    case GLChunk::glCompressedTexImage1D:
    case GLChunk::glCompressedTexImage1DARB:
    case GLChunk::glCompressedTextureImage1DEXT:
      return Serialise_glCompressedTextureImage1DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glCompressedMultiTexImage2DEXT:
    case GLChunk::glCompressedTexImage2D:
    case GLChunk::glCompressedTexImage2DARB:
    case GLChunk::glCompressedTextureImage2DEXT:
      return Serialise_glCompressedTextureImage2DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0, 0);
    case GLChunk::glCompressedMultiTexImage3DEXT:
    case GLChunk::glCompressedTexImage3D:
    case GLChunk::glCompressedTexImage3DARB:
    case GLChunk::glCompressedTexImage3DOES:
    case GLChunk::glCompressedTextureImage3DEXT:
      return Serialise_glCompressedTextureImage3DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0, 0, 0);
    case GLChunk::glCopyTexImage1D:
    case GLChunk::glCopyMultiTexImage1DEXT:
    case GLChunk::glCopyTextureImage1DEXT:
      return Serialise_glCopyTextureImage1DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glCopyTexImage2D:
    case GLChunk::glCopyMultiTexImage2DEXT:
    case GLChunk::glCopyTextureImage2DEXT:
      return Serialise_glCopyTextureImage2DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0, 0);
    case GLChunk::glTexStorage1D:
    case GLChunk::glTexStorage1DEXT:
    case GLChunk::glTextureStorage1D:
    case GLChunk::glTextureStorage1DEXT:
      return Serialise_glTextureStorage1DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0);
    case GLChunk::glTexStorage2D:
    case GLChunk::glTexStorage2DEXT:
    case GLChunk::glTextureStorage2D:
    case GLChunk::glTextureStorage2DEXT:
      return Serialise_glTextureStorage2DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0);
    case GLChunk::glTexStorage3D:
    case GLChunk::glTexStorage3DEXT:
    case GLChunk::glTextureStorage3D:
    case GLChunk::glTextureStorage3DEXT:
      return Serialise_glTextureStorage3DEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
    case GLChunk::glTexImage2DMultisample:
    // technically this isn't equivalent to storage, but we treat it as such because there's no DSA
    // variant of this teximage
    case GLChunk::glTexStorage2DMultisample:
    case GLChunk::glTextureStorage2DMultisample:
    case GLChunk::glTextureStorage2DMultisampleEXT:
      return Serialise_glTextureStorage2DMultisampleEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
    case GLChunk::glTexImage3DMultisample:
    // technically this isn't equivalent to storage, but we treat it as such because there's no DSA
    // variant of this teximage
    case GLChunk::glTexStorage3DMultisample:
    case GLChunk::glTexStorage3DMultisampleOES:
    case GLChunk::glTextureStorage3DMultisample:
    case GLChunk::glTextureStorage3DMultisampleEXT:
      return Serialise_glTextureStorage3DMultisampleEXT(ser, 0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glMultiTexSubImage1DEXT:
    case GLChunk::glTexSubImage1D:
    case GLChunk::glTextureSubImage1D:
    case GLChunk::glTextureSubImage1DEXT:
      return Serialise_glTextureSubImage1DEXT(ser, 0, eGL_NONE, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glMultiTexSubImage2DEXT:
    case GLChunk::glTexSubImage2D:
    case GLChunk::glTextureSubImage2D:
    case GLChunk::glTextureSubImage2DEXT:
      return Serialise_glTextureSubImage2DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, 0);
    case GLChunk::glMultiTexSubImage3DEXT:
    case GLChunk::glTexSubImage3D:
    case GLChunk::glTexSubImage3DOES:
    case GLChunk::glTextureSubImage3D:
    case GLChunk::glTextureSubImage3DEXT:
      return Serialise_glTextureSubImage3DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, eGL_NONE,
                                              eGL_NONE, 0);
    case GLChunk::glCompressedMultiTexSubImage1DEXT:
    case GLChunk::glCompressedTexSubImage1D:
    case GLChunk::glCompressedTexSubImage1DARB:
    case GLChunk::glCompressedTextureSubImage1D:
    case GLChunk::glCompressedTextureSubImage1DEXT:
      return Serialise_glCompressedTextureSubImage1DEXT(ser, 0, eGL_NONE, 0, 0, 0, eGL_NONE, 0, 0);
    case GLChunk::glCompressedMultiTexSubImage2DEXT:
    case GLChunk::glCompressedTexSubImage2D:
    case GLChunk::glCompressedTexSubImage2DARB:
    case GLChunk::glCompressedTextureSubImage2D:
    case GLChunk::glCompressedTextureSubImage2DEXT:
      return Serialise_glCompressedTextureSubImage2DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE,
                                                        0, 0);
    case GLChunk::glCompressedMultiTexSubImage3DEXT:
    case GLChunk::glCompressedTexSubImage3D:
    case GLChunk::glCompressedTexSubImage3DARB:
    case GLChunk::glCompressedTexSubImage3DOES:
    case GLChunk::glCompressedTextureSubImage3D:
    case GLChunk::glCompressedTextureSubImage3DEXT:
      return Serialise_glCompressedTextureSubImage3DEXT(ser, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0,
                                                        eGL_NONE, 0, 0);
    case GLChunk::glTexBufferRange:
    case GLChunk::glTexBufferRangeEXT:
    case GLChunk::glTexBufferRangeOES:
    case GLChunk::glTextureBufferRange:
    case GLChunk::glTextureBufferRangeEXT:
      return Serialise_glTextureBufferRangeEXT(ser, 0, eGL_NONE, eGL_NONE, 0, 0, 0);
    case GLChunk::glMultiTexBufferEXT:
    case GLChunk::glTexBuffer:
    case GLChunk::glTexBufferARB:
    case GLChunk::glTexBufferEXT:
    case GLChunk::glTexBufferOES:
    case GLChunk::glTextureBuffer:
    case GLChunk::glTextureBufferEXT:
      return Serialise_glTextureBufferEXT(ser, 0, eGL_NONE, eGL_NONE, 0);

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
    case GLChunk::glUniform1fARB:
    case GLChunk::glUniform1fv:
    case GLChunk::glUniform1fvARB:
    case GLChunk::glUniform1i:
    case GLChunk::glUniform1iARB:
    case GLChunk::glUniform1iv:
    case GLChunk::glUniform1ivARB:
    case GLChunk::glUniform1ui:
    case GLChunk::glUniform1uiEXT:
    case GLChunk::glUniform1uiv:
    case GLChunk::glUniform1uivEXT:
    case GLChunk::glUniform2d:
    case GLChunk::glUniform2dv:
    case GLChunk::glUniform2f:
    case GLChunk::glUniform2fARB:
    case GLChunk::glUniform2fv:
    case GLChunk::glUniform2fvARB:
    case GLChunk::glUniform2i:
    case GLChunk::glUniform2iARB:
    case GLChunk::glUniform2iv:
    case GLChunk::glUniform2ivARB:
    case GLChunk::glUniform2ui:
    case GLChunk::glUniform2uiEXT:
    case GLChunk::glUniform2uiv:
    case GLChunk::glUniform2uivEXT:
    case GLChunk::glUniform3d:
    case GLChunk::glUniform3dv:
    case GLChunk::glUniform3f:
    case GLChunk::glUniform3fARB:
    case GLChunk::glUniform3fv:
    case GLChunk::glUniform3fvARB:
    case GLChunk::glUniform3i:
    case GLChunk::glUniform3iARB:
    case GLChunk::glUniform3iv:
    case GLChunk::glUniform3ivARB:
    case GLChunk::glUniform3ui:
    case GLChunk::glUniform3uiEXT:
    case GLChunk::glUniform3uiv:
    case GLChunk::glUniform3uivEXT:
    case GLChunk::glUniform4d:
    case GLChunk::glUniform4dv:
    case GLChunk::glUniform4f:
    case GLChunk::glUniform4fARB:
    case GLChunk::glUniform4fv:
    case GLChunk::glUniform4fvARB:
    case GLChunk::glUniform4i:
    case GLChunk::glUniform4iARB:
    case GLChunk::glUniform4iv:
    case GLChunk::glUniform4ivARB:
    case GLChunk::glUniform4ui:
    case GLChunk::glUniform4uiEXT:
    case GLChunk::glUniform4uiv:
    case GLChunk::glUniform4uivEXT:
      return Serialise_glProgramUniformVector(ser, 0, 0, 0, 0, UNIFORM_UNKNOWN);

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
    case GLChunk::glUniformMatrix2fvARB:
    case GLChunk::glUniformMatrix2x3dv:
    case GLChunk::glUniformMatrix2x3fv:
    case GLChunk::glUniformMatrix2x4dv:
    case GLChunk::glUniformMatrix2x4fv:
    case GLChunk::glUniformMatrix3dv:
    case GLChunk::glUniformMatrix3fv:
    case GLChunk::glUniformMatrix3fvARB:
    case GLChunk::glUniformMatrix3x2dv:
    case GLChunk::glUniformMatrix3x2fv:
    case GLChunk::glUniformMatrix3x4dv:
    case GLChunk::glUniformMatrix3x4fv:
    case GLChunk::glUniformMatrix4dv:
    case GLChunk::glUniformMatrix4fv:
    case GLChunk::glUniformMatrix4fvARB:
    case GLChunk::glUniformMatrix4x2dv:
    case GLChunk::glUniformMatrix4x2fv:
    case GLChunk::glUniformMatrix4x3dv:
    case GLChunk::glUniformMatrix4x3fv:
      return Serialise_glProgramUniformMatrix(ser, 0, 0, 0, 0, 0, UNIFORM_UNKNOWN);

    case GLChunk::vrapi_CreateTextureSwapChain:
    case GLChunk::vrapi_CreateTextureSwapChain2:
      // nothing to do, these chunks are just markers
      return true;

    case GLChunk::MakeContextCurrent:
      // re-use the serialisation for beginning of the frame
      return Serialise_BeginCaptureFrame(ser);

    case GLChunk::ImplicitThreadSwitch:
    {
      m_ImplicitThreadSwitches++;
      bool ret = Serialise_ContextConfiguration(ser, NULL);
      if(!ret)
        return false;
      return Serialise_BeginCaptureFrame(ser);
    }

    case GLChunk::ContextConfiguration: return Serialise_ContextConfiguration(ser, NULL);

    case GLChunk::glIndirectSubCommand:
      // this is a fake chunk generated at runtime as part of indirect draws.
      // Just in case it gets exported and imported, completely ignore it.
      return true;

    case GLChunk::glShaderBinary: return Serialise_glShaderBinary(ser, 0, NULL, eGL_NONE, NULL, 0);

    case GLChunk::glSpecializeShaderARB:
    case GLChunk::glSpecializeShader:
      return Serialise_glSpecializeShader(ser, 0, NULL, 0, NULL, NULL);

    case GLChunk::glFinish: return Serialise_glFinish(ser);
    case GLChunk::glFlush: return Serialise_glFlush(ser);

    case GLChunk::glCreateMemoryObjectsEXT: return Serialise_glCreateMemoryObjectsEXT(ser, 0, NULL);
    case GLChunk::glMemoryObjectParameterivEXT:
      return Serialise_glMemoryObjectParameterivEXT(ser, 0, eGL_NONE, 0);
    case GLChunk::glTexStorageMem1DEXT:
    case GLChunk::glTextureStorageMem1DEXT:
      return Serialise_glTextureStorageMem1DEXT(ser, 0, 0, eGL_NONE, 0, 0, 0);
    case GLChunk::glTexStorageMem2DEXT:
    case GLChunk::glTextureStorageMem2DEXT:
      return Serialise_glTextureStorageMem2DEXT(ser, 0, 0, eGL_NONE, 0, 0, 0, 0);
    case GLChunk::glTexStorageMem2DMultisampleEXT:
    case GLChunk::glTextureStorageMem2DMultisampleEXT:
      return Serialise_glTextureStorageMem2DMultisampleEXT(ser, 0, 0, eGL_NONE, 0, 0, GL_FALSE, 0, 0);
    case GLChunk::glTexStorageMem3DEXT:
    case GLChunk::glTextureStorageMem3DEXT:
      return Serialise_glTextureStorageMem3DEXT(ser, 0, 0, eGL_NONE, 0, 0, 0, 0, 0);
    case GLChunk::glTexStorageMem3DMultisampleEXT:
    case GLChunk::glTextureStorageMem3DMultisampleEXT:
      return Serialise_glTextureStorageMem3DMultisampleEXT(ser, 0, 0, eGL_NONE, 0, 0, 0, GL_FALSE,
                                                           0, 0);
    case GLChunk::glBufferStorageMemEXT:
    case GLChunk::glNamedBufferStorageMemEXT:
      return Serialise_glNamedBufferStorageMemEXT(ser, 0, 0, 0, 0);
    case GLChunk::glGenSemaphoresEXT: return Serialise_glGenSemaphoresEXT(ser, 0, NULL);
    case GLChunk::glSemaphoreParameterui64vEXT:
      return Serialise_glSemaphoreParameterui64vEXT(ser, 0, eGL_NONE, NULL);
    case GLChunk::glWaitSemaphoreEXT:
      return Serialise_glWaitSemaphoreEXT(ser, 0, 0, NULL, 0, NULL, NULL);
    case GLChunk::glSignalSemaphoreEXT:
      return Serialise_glSignalSemaphoreEXT(ser, 0, 0, NULL, 0, NULL, NULL);
    case GLChunk::glImportMemoryFdEXT: return Serialise_glImportMemoryFdEXT(ser, 0, 0, eGL_NONE, 0);
    case GLChunk::glImportSemaphoreFdEXT:
      return Serialise_glImportSemaphoreFdEXT(ser, 0, eGL_NONE, 0);
    case GLChunk::glImportMemoryWin32HandleEXT:
      return Serialise_glImportMemoryWin32HandleEXT(ser, 0, 0, eGL_NONE, NULL);
    case GLChunk::glImportMemoryWin32NameEXT:
      return Serialise_glImportMemoryWin32NameEXT(ser, 0, 0, eGL_NONE, NULL);
    case GLChunk::glImportSemaphoreWin32HandleEXT:
      return Serialise_glImportSemaphoreWin32HandleEXT(ser, 0, eGL_NONE, NULL);
    case GLChunk::glImportSemaphoreWin32NameEXT:
      return Serialise_glImportSemaphoreWin32NameEXT(ser, 0, eGL_NONE, NULL);
    case GLChunk::glAcquireKeyedMutexWin32EXT:
      return Serialise_glAcquireKeyedMutexWin32EXT(ser, 0, 0, 0);
    case GLChunk::glReleaseKeyedMutexWin32EXT:
      return Serialise_glReleaseKeyedMutexWin32EXT(ser, 0, 0);

    case GLChunk::SwapBuffers:
    case GLChunk::wglSwapBuffers:
    case GLChunk::glXSwapBuffers:
    case GLChunk::CGLFlushDrawable:
    case GLChunk::eglSwapBuffers:
    case GLChunk::eglPostSubBufferNV:
    case GLChunk::eglSwapBuffersWithDamageEXT:
    case GLChunk::eglSwapBuffersWithDamageKHR: return Serialise_Present(ser);

    case GLChunk::glInvalidateNamedFramebufferSubData:
    case GLChunk::glInvalidateSubFramebuffer:
      return Serialise_glInvalidateNamedFramebufferSubData(ser, 0, 0, NULL, 0, 0, 0, 0);
    case GLChunk::glInvalidateTexImage: return Serialise_glInvalidateTexImage(ser, 0, 0);
    case GLChunk::glInvalidateTexSubImage:
      return Serialise_glInvalidateTexSubImage(ser, 0, 0, 0, 0, 0, 0, 0, 0);
    case GLChunk::glInvalidateBufferData: return Serialise_glInvalidateBufferData(ser, 0);
    case GLChunk::glInvalidateBufferSubData:
      return Serialise_glInvalidateBufferSubData(ser, 0, 0, 0);

    case GLChunk::glGetQueryObjecti64v:
    case GLChunk::glGetQueryObjecti64vEXT:
    case GLChunk::glGetQueryBufferObjecti64v:
      return Serialise_glGetQueryBufferObjecti64v(ser, 0, 0, eGL_NONE, 0);
    case GLChunk::glGetQueryObjectiv:
    case GLChunk::glGetQueryObjectivARB:
    case GLChunk::glGetQueryObjectivEXT:
    case GLChunk::glGetQueryBufferObjectiv:
      return Serialise_glGetQueryBufferObjectiv(ser, 0, 0, eGL_NONE, 0);
    case GLChunk::glGetQueryObjectui64v:
    case GLChunk::glGetQueryObjectui64vEXT:
    case GLChunk::glGetQueryBufferObjectui64v:
      return Serialise_glGetQueryBufferObjectui64v(ser, 0, 0, eGL_NONE, 0);
    case GLChunk::glGetQueryObjectuiv:
    case GLChunk::glGetQueryObjectuivARB:
    case GLChunk::glGetQueryObjectuivEXT:
    case GLChunk::glGetQueryBufferObjectuiv:
      return Serialise_glGetQueryBufferObjectuiv(ser, 0, 0, eGL_NONE, 0);

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
    case GLChunk::glGetQueryIndexediv:
    case GLChunk::glGetQueryiv:
    case GLChunk::glGetQueryivARB:
    case GLChunk::glGetQueryivEXT:
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
    case GLChunk::glReleaseShaderCompiler:
    case GLChunk::glFrameTerminatorGREMEDY:
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
    case GLChunk::glMapBufferRangeEXT:
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
    case GLChunk::glMaxShaderCompilerThreadsARB:
    case GLChunk::glMaxShaderCompilerThreadsKHR:

    case GLChunk::glGetUnsignedBytevEXT:
    case GLChunk::glGetUnsignedBytei_vEXT:
    case GLChunk::glDeleteMemoryObjectsEXT:
    case GLChunk::glIsMemoryObjectEXT:
    case GLChunk::glGetMemoryObjectParameterivEXT:
    case GLChunk::glDeleteSemaphoresEXT:
    case GLChunk::glIsSemaphoreEXT:
    case GLChunk::glGetSemaphoreParameterui64vEXT:
    case GLChunk::glBeginPerfQueryINTEL:
    case GLChunk::glCreatePerfQueryINTEL:
    case GLChunk::glDeletePerfQueryINTEL:
    case GLChunk::glEndPerfQueryINTEL:
    case GLChunk::glGetFirstPerfQueryIdINTEL:
    case GLChunk::glGetNextPerfQueryIdINTEL:
    case GLChunk::glGetPerfCounterInfoINTEL:
    case GLChunk::glGetPerfQueryDataINTEL:
    case GLChunk::glGetPerfQueryIdByNameINTEL:
    case GLChunk::glGetPerfQueryInfoINTEL:

    case GLChunk::Max:
      RDCERR("Unexpected chunk %s, or missing case for processing! Skipping...",
             ToStr(chunk).c_str());
      ser.SkipCurrentChunk();
      return false;
  }

  return false;
}

RDResult WrappedOpenGL::ContextReplayLog(CaptureState readType, uint32_t startEventID,
                                         uint32_t endEventID, bool partial)
{
  m_FrameReader->SetOffset(0);

  ReadSerialiser ser(m_FrameReader, Ownership::Nothing);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());
  ser.SetVersion(m_SectionVersion);

  SDFile *prevFile = m_StructuredFile;

  if(IsLoading(m_State) || IsStructuredExporting(m_State))
  {
    ser.ConfigureStructuredExport(&GetChunkName, IsStructuredExporting(m_State), m_TimeBase,
                                  m_TimeFrequency);

    ser.GetStructuredFile().Swap(*m_StructuredFile);

    m_StructuredFile = &ser.GetStructuredFile();
  }

  SystemChunk header = ser.ReadChunk<SystemChunk>();
  RDCASSERTEQUAL(header, SystemChunk::CaptureBegin);

  if(IsActiveReplaying(m_State) && !partial && !m_FetchCounters)
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
            GL.glEndQuery(q);
          else
            GL.glEndQueryIndexed(q, j);
          m_ActiveQueries[i][j] = false;
        }
      }
    }

    if(m_ActiveConditional)
    {
      GL.glEndConditionalRender();
      m_ActiveConditional = false;
    }

    if(m_ActiveFeedback)
    {
      GL.glEndTransformFeedback();
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
    m_CurEventID = ev.eventId;
    if(partial)
      ser.GetReader()->SetOffset(ev.fileOffset);
    m_FirstEventID = startEventID;
    m_LastEventID = endEventID;
  }
  else
  {
    m_CurEventID = 1;
    m_CurActionID = 1;
    m_FirstEventID = 0;
    m_LastEventID = ~0U;
  }

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

    if(ser.GetReader()->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    m_ChunkMetadata = ser.ChunkMetadata();

    bool success = ContextProcessChunk(ser, chunktype);

    ser.EndChunk();

    if(ser.GetReader()->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
      return m_FailedReplayResult;

    RenderDoc::Inst().SetProgress(
        LoadProgress::FrameEventsRead,
        float(m_CurChunkOffset - startOffset) / float(ser.GetReader()->GetSize()));

    if((SystemChunk)chunktype == SystemChunk::CaptureEnd || ser.GetReader()->AtEnd())
      break;

    m_LastChunk = chunktype;
    m_CurEventID++;
  }

  // swap the structure back now that we've accumulated the frame as well.
  if(IsLoading(m_State) || IsStructuredExporting(m_State))
    ser.GetStructuredFile().Swap(*prevFile);

  m_StructuredFile = prevFile;

  if(IsLoading(m_State))
  {
    GetReplay()->WriteFrameRecord().actionList = m_ParentAction.children;
    GetReplay()->WriteFrameRecord().frameInfo.debugMessages = GetDebugMessages();

    SetupActionPointers(m_Actions, GetReplay()->WriteFrameRecord().actionList);

    // it's easier to remove duplicate usages here than check it as we go.
    // this means if textures are bound in multiple places in the same action
    // we don't have duplicate uses
    for(auto it = m_ResourceUses.begin(); it != m_ResourceUses.end(); ++it)
    {
      rdcarray<EventUsage> &v = it->second;
      std::sort(v.begin(), v.end());
      v.erase(std::unique(v.begin(), v.end()) - v.begin(), ~0U);
    }
  }

  if(IsActiveReplaying(m_State) && !m_FetchCounters)
  {
    for(size_t i = 0; i < MAX_QUERIES; i++)
    {
      GLenum q = QueryEnum(i);
      if(q == eGL_NONE)
        break;

      int indices = IsGLES ? 1 : MAX_QUERY_INDICES;    // GLES does not support indices
      for(int j = 0; j < indices; j++)
      {
        if(m_ActiveQueries[i][j])
        {
          if(IsGLES)
            GL.glEndQuery(q);
          else
            GL.glEndQueryIndexed(q, j);
          m_ActiveQueries[i][j] = false;
        }
      }
    }

    if(m_ActiveConditional)
    {
      GL.glEndConditionalRender();
      m_ActiveConditional = false;
    }

    if(m_ActiveFeedback)
    {
      GL.glEndTransformFeedback();
      m_ActiveFeedback = false;
    }
  }

  return ResultCode::Succeeded;
}

bool WrappedOpenGL::ContextProcessChunk(ReadSerialiser &ser, GLChunk chunk)
{
  m_AddedAction = false;

  bool success = ProcessChunk(ser, chunk);

  if(!success)
    return false;

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
        // push down the action stack to the latest action
        m_ActionStack.push_back(&m_ActionStack.back()->children.back());
        break;
      }
      case GLChunk::glPopGroupMarkerEXT:
      case GLChunk::glPopDebugGroup:
      case GLChunk::glPopDebugGroupKHR:
      {
        // refuse to pop off further than the root action (mismatched begin/end events e.g.)
        if(m_ActionStack.size() > 1)
          m_ActionStack.pop_back();
        break;
      }
      default: break;
    }

    if(!m_AddedAction)
      AddEvent();
  }

  m_AddedAction = false;

  return true;
}

void WrappedOpenGL::AddUsage(const ActionDescription &a)
{
  ActionFlags DrawDispatchMask = ActionFlags::Drawcall | ActionFlags::Dispatch;
  if(!(a.flags & DrawDispatchMask))
    return;

  GLResourceManager *rm = GetResourceManager();

  ContextPair &ctx = GetCtx();

  uint32_t e = a.eventId;

  //////////////////////////////
  // Input

  if(a.flags & ActionFlags::Indexed)
  {
    GLuint ibuffer = 0;
    GL.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&ibuffer);

    if(ibuffer)
      m_ResourceUses[rm->GetResID(BufferRes(ctx, ibuffer))].push_back(
          EventUsage(e, ResourceUsage::IndexBuffer));
  }

  // Vertex buffers and attributes
  GLint numVBufferBindings = GetNumVertexBuffers();

  for(GLuint i = 0; i < (GLuint)numVBufferBindings; i++)
  {
    GLuint buffer = GetBoundVertexBuffer(i);

    if(buffer)
      m_ResourceUses[rm->GetResID(BufferRes(ctx, buffer))].push_back(
          EventUsage(e, ResourceUsage::VertexBuffer));
  }

  //////////////////////////////
  // Shaders

  {
    GLRenderState rs;
    rs.FetchState(this);

    ShaderReflection *refl[NumShaderStages] = {NULL};
    GLuint progForStage[NumShaderStages] = {};

    GLuint curProg = 0;
    GL.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&curProg);

    if(curProg == 0)
    {
      GL.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&curProg);

      if(curProg == 0)
      {
        // no program bound at this action
      }
      else
      {
        auto &pipeDetails = m_Pipelines[rm->GetResID(ProgramPipeRes(ctx, curProg))];

        for(size_t i = 0; i < ARRAY_COUNT(pipeDetails.stageShaders); i++)
        {
          if(pipeDetails.stageShaders[i] != ResourceId())
          {
            curProg = rm->GetCurrentResource(pipeDetails.stagePrograms[i]).name;

            refl[i] = m_Shaders[pipeDetails.stageShaders[i]].reflection;
            progForStage[i] = curProg;
          }
        }
      }
    }
    else
    {
      auto &progDetails = m_Programs[rm->GetResID(ProgramRes(ctx, curProg))];

      for(size_t i = 0; i < ARRAY_COUNT(progDetails.stageShaders); i++)
      {
        if(progDetails.stageShaders[i] != ResourceId())
        {
          refl[i] = m_Shaders[progDetails.stageShaders[i]].reflection;
          progForStage[i] = curProg;
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
        for(const ConstantBlock &cblock : refl[i]->constantBlocks)
        {
          if(!cblock.bufferBacked)
            continue;

          uint32_t slot = 0;
          bool used = false;
          GetCurrentBinding(progForStage[i], refl[i], cblock, slot, used);

          if(!used)
            continue;

          if(rs.UniformBinding[slot].res.name)
            m_ResourceUses[rm->GetResID(rs.UniformBinding[slot].res)].push_back(cb);
        }

        for(const ShaderResource &res : refl[i]->readWriteResources)
        {
          uint32_t slot = 0;
          bool used = false;
          GetCurrentBinding(progForStage[i], refl[i], res, slot, used);

          if(!used)
            continue;

          if(res.isTexture)
          {
            if(slot < ARRAY_COUNT(rs.Images) && rs.Images[slot].res.name)
              m_ResourceUses[rm->GetResID(rs.Images[slot].res)].push_back(rw);
          }
          else
          {
            if(res.variableType.columns == 1 && res.variableType.rows == 1 &&
               res.variableType.baseType == VarType::UInt)
            {
              if(slot < ARRAY_COUNT(rs.AtomicCounter) && rs.AtomicCounter[slot].res.name)
                m_ResourceUses[rm->GetResID(rs.AtomicCounter[slot].res)].push_back(rw);
            }
            else
            {
              if(slot < ARRAY_COUNT(rs.ShaderStorage) && rs.ShaderStorage[slot].res.name)
                m_ResourceUses[rm->GetResID(rs.ShaderStorage[slot].res)].push_back(rw);
            }
          }
        }

        for(const ShaderResource &res : refl[i]->readOnlyResources)
        {
          uint32_t slot = 0;
          bool used = false;
          GetCurrentBinding(progForStage[i], refl[i], res, slot, used);

          if(!used)
            continue;

          GLResource *texList = NULL;
          const int32_t listSize = (int32_t)ARRAY_COUNT(rs.Tex2D);

          switch(res.textureType)
          {
            case TextureType::Unknown: texList = NULL; break;
            case TextureType::Buffer: texList = rs.TexBuffer; break;
            case TextureType::Texture1D: texList = rs.Tex1D; break;
            case TextureType::Texture1DArray: texList = rs.Tex1DArray; break;
            case TextureType::Texture2D: texList = rs.Tex2D; break;
            case TextureType::TextureRect: texList = rs.TexRect; break;
            case TextureType::Texture2DArray: texList = rs.Tex2DArray; break;
            case TextureType::Texture2DMS: texList = rs.Tex2DMS; break;
            case TextureType::Texture2DMSArray: texList = rs.Tex2DMSArray; break;
            case TextureType::Texture3D: texList = rs.Tex3D; break;
            case TextureType::TextureCube: texList = rs.TexCube; break;
            case TextureType::TextureCubeArray: texList = rs.TexCubeArray; break;
            case TextureType::Count: RDCERR("Invalid shader resource type"); break;
          }

          if(texList != NULL && slot < listSize && texList[slot].name != 0)
            m_ResourceUses[rm->GetResID(texList[slot])].push_back(ro);
        }
      }
    }
  }

  //////////////////////////////
  // Feedback

  GLint maxCount = 0;
  GL.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

  for(int i = 0; i < maxCount; i++)
  {
    GLuint buffer = 0;
    GL.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint *)&buffer);

    if(buffer)
      m_ResourceUses[rm->GetResID(BufferRes(ctx, buffer))].push_back(
          EventUsage(e, ResourceUsage::StreamOut));
  }

  //////////////////////////////
  // FBO

  GLint numCols = 8;
  GL.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

  GLuint attachment = 0;
  GLenum type = eGL_TEXTURE;
  for(GLint i = 0; i < numCols; i++)
  {
    GLenum dbEnum = eGL_NONE;
    GL.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&dbEnum);

    if(dbEnum == eGL_NONE)
      continue;

    type = eGL_TEXTURE;

    GL.glGetFramebufferAttachmentParameteriv(
        eGL_DRAW_FRAMEBUFFER, dbEnum, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&attachment);
    GL.glGetFramebufferAttachmentParameteriv(
        eGL_DRAW_FRAMEBUFFER, dbEnum, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    if(attachment)
    {
      if(type == eGL_TEXTURE)
        m_ResourceUses[rm->GetResID(TextureRes(ctx, attachment))].push_back(
            EventUsage(e, ResourceUsage::ColorTarget));
      else
        m_ResourceUses[rm->GetResID(RenderbufferRes(ctx, attachment))].push_back(
            EventUsage(e, ResourceUsage::ColorTarget));
    }
  }

  GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                           (GLint *)&attachment);
  GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(attachment)
  {
    if(type == eGL_TEXTURE)
      m_ResourceUses[rm->GetResID(TextureRes(ctx, attachment))].push_back(
          EventUsage(e, ResourceUsage::DepthStencilTarget));
    else
      m_ResourceUses[rm->GetResID(RenderbufferRes(ctx, attachment))].push_back(
          EventUsage(e, ResourceUsage::DepthStencilTarget));
  }

  GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                           (GLint *)&attachment);
  GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(attachment)
  {
    if(type == eGL_TEXTURE)
      m_ResourceUses[rm->GetResID(TextureRes(ctx, attachment))].push_back(
          EventUsage(e, ResourceUsage::DepthStencilTarget));
    else
      m_ResourceUses[rm->GetResID(RenderbufferRes(ctx, attachment))].push_back(
          EventUsage(e, ResourceUsage::DepthStencilTarget));
  }
}

void WrappedOpenGL::AddAction(const ActionDescription &a)
{
  m_AddedAction = true;

  WrappedOpenGL *context = this;

  ActionDescription action = a;
  action.eventId = m_CurEventID;
  action.actionId = m_CurActionID;

  m_DrawcallParams.resize_for_index(m_CurEventID);
  m_DrawcallParams[m_CurEventID].indexWidth = m_LastIndexWidth;
  m_DrawcallParams[m_CurEventID].topo = m_LastTopology;

  {
    GLint numCols = 8;
    GL.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

    RDCEraseEl(action.outputs);

    GLenum type;

    for(GLint i = 0, att = 0; i < RDCMIN(numCols, 8); i++)
    {
      type = eGL_TEXTURE;

      GLenum dbEnum = eGL_NONE;
      GL.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&dbEnum);

      if(dbEnum == eGL_NONE)
        continue;

      GLuint depth = 0;
      GL.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, dbEnum, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&depth);
      GL.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, dbEnum, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

      if(type == eGL_TEXTURE)
        action.outputs[att] = GetResourceManager()->GetOriginalID(
            GetResourceManager()->GetResID(TextureRes(GetCtx(), depth)));
      else
        action.outputs[att] = GetResourceManager()->GetOriginalID(
            GetResourceManager()->GetResID(RenderbufferRes(GetCtx(), depth)));
      att++;
    }

    type = eGL_TEXTURE;

    GLuint depth = 0;
    GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&depth);
    GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
    if(type == eGL_TEXTURE)
      action.depthOut = GetResourceManager()->GetOriginalID(
          GetResourceManager()->GetResID(TextureRes(GetCtx(), depth)));
    else
      action.depthOut = GetResourceManager()->GetOriginalID(
          GetResourceManager()->GetResID(RenderbufferRes(GetCtx(), depth)));
  }

  // markers don't increment action ID
  ActionFlags MarkerMask = ActionFlags::SetMarker | ActionFlags::PushMarker |
                           ActionFlags::PopMarker | ActionFlags::MultiAction;
  if(!(action.flags & MarkerMask))
    m_CurActionID++;

  action.events.swap(m_CurEvents);

  AddUsage(action);

  // should have at least the root action here, push this action
  // onto the back's children list.
  if(!context->m_ActionStack.empty())
    m_ActionStack.back()->children.push_back(action);
  else
    RDCERR("Somehow lost action stack!");
}

void WrappedOpenGL::AddEvent()
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventId = m_CurEventID;

  apievent.chunkIndex = uint32_t(m_StructuredFile->chunks.size() - 1);

  m_CurEvents.push_back(apievent);

  if(IsLoading(m_State))
  {
    m_Events.resize(apievent.eventId + 1);
    m_Events[apievent.eventId] = apievent;
  }
}

const APIEvent &WrappedOpenGL::GetEvent(uint32_t eventId)
{
  // start at where the requested eventId would be
  size_t idx = eventId;

  // find the next valid event (some may be skipped)
  while(idx < m_Events.size() - 1 && m_Events[idx].eventId == 0)
    idx++;

  return m_Events[RDCMIN(idx, m_Events.size() - 1)];
}

const ActionDescription *WrappedOpenGL::GetAction(uint32_t eventId)
{
  if(eventId >= m_Actions.size())
    return NULL;

  return m_Actions[eventId];
}

const GLDrawParams &WrappedOpenGL::GetDrawParameters(uint32_t eventId)
{
  m_DrawcallParams.resize_for_index(eventId);
  return m_DrawcallParams[eventId];
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
    RENDERDOC_PROFILEREGION("ApplyInitialContents");
    GLMarkerRegion apply("!!!!RenderDoc Internal: ApplyInitialContents");
    GetResourceManager()->ApplyInitialContents();

    m_WasActiveFeedback = false;
  }

  m_State = CaptureState::ActiveReplaying;

  GLMarkerRegion::Set(StringFormat::Fmt("!!!!RenderDoc Internal:  Replay %d (%d): %u->%u",
                                        (int)replayType, (int)partial, startEventID, endEventID));

  m_ReplayEventCount = 0;

  RDResult status = ResultCode::Succeeded;

  if(replayType == eReplay_Full)
    status = ContextReplayLog(m_State, startEventID, endEventID, partial);
  else if(replayType == eReplay_WithoutDraw)
    status = ContextReplayLog(m_State, startEventID, RDCMAX(1U, endEventID) - 1, partial);
  else if(replayType == eReplay_OnlyDraw)
    status = ContextReplayLog(m_State, endEventID, endEventID, partial);
  else
    RDCFATAL("Unexpected replay type");

  RDCASSERTEQUAL(status.code, ResultCode::Succeeded);

  // make sure to end any unbalanced replay events if we stopped in the middle of a frame
  for(int i = 0; m_ReplayMarkers && i < m_ReplayEventCount; i++)
    GLMarkerRegion::End();

  GLMarkerRegion::Set("!!!!RenderDoc Internal: Done replay");
}
