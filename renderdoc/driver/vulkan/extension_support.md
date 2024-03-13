# Vulkan extension support

This is a list of the currently supported vulkan extensions, in a bit more readable format than the code.

Maintainers can update this file by updating vk.xml in this folder and running `./check_extensions.sh` which will output any new extensions that haven't been filed in a category below. This will also update `all_exts.txt` which needs to be committed too when this file is updated to keep things in sync.

# Supported

* `VK_AMD_buffer_marker`
* `VK_AMD_device_coherent_memory`
* `VK_AMD_display_native_hdr`
* `VK_AMD_gcn_shader`
* `VK_AMD_gpu_shader_half_float`
* `VK_AMD_gpu_shader_int16`
* `VK_AMD_memory_overallocation_behavior`
* `VK_AMD_mixed_attachment_samples`
* `VK_AMD_negative_viewport_height`
* `VK_AMD_shader_ballot`
* `VK_AMD_shader_core_properties`
* `VK_AMD_shader_explicit_vertex_parameter`
* `VK_AMD_shader_fragment_mask`
* `VK_AMD_shader_image_load_store_lod`
* `VK_AMD_shader_trinary_minmax`
* `VK_AMD_texture_gather_bias_lod`
* `VK_ANDROID_external_memory_android_hardware_buffer`
* `VK_EXT_4444_formats`
* `VK_EXT_acquire_drm_display`
* `VK_EXT_acquire_xlib_display`
* `VK_EXT_astc_decode_mode`
* `VK_EXT_attachment_feedback_loop_dynamic_state`
* `VK_EXT_attachment_feedback_loop_layout`
* `VK_EXT_border_color_swizzle`
* `VK_EXT_buffer_device_address`
* `VK_EXT_calibrated_timestamps`
* `VK_EXT_color_write_enable`
* `VK_EXT_conditional_rendering`
* `VK_EXT_conservative_rasterization`
* `VK_EXT_custom_border_color`
* `VK_EXT_debug_marker`
* `VK_EXT_debug_report`
* `VK_EXT_debug_utils`
* `VK_EXT_depth_clamp_zero_one`
* `VK_EXT_depth_clip_control`
* `VK_EXT_depth_clip_enable`
* `VK_EXT_depth_range_unrestricted`
* `VK_EXT_descriptor_indexing`
* `VK_EXT_direct_mode_display`
* `VK_EXT_discard_rectangles`
* `VK_EXT_display_control`
* `VK_EXT_display_surface_counter`
* `VK_EXT_extended_dynamic_state`
* `VK_EXT_extended_dynamic_state2`
* `VK_EXT_extended_dynamic_state3`
* `VK_EXT_external_memory_dma_buf`
* `VK_EXT_filter_cubic`
* `VK_EXT_fragment_density_map`
* `VK_EXT_fragment_density_map2`
* `VK_EXT_fragment_shader_interlock`
* `VK_EXT_full_screen_exclusive`
* `VK_EXT_global_priority_query`
* `VK_EXT_global_priority`
* `VK_EXT_graphics_pipeline_library`
* `VK_EXT_hdr_metadata`
* `VK_EXT_headless_surface`
* `VK_EXT_host_query_reset`
* `VK_EXT_image_2d_view_of_3d`
* `VK_EXT_image_robustness`
* `VK_EXT_image_view_min_lod`
* `VK_EXT_index_type_uint8`
* `VK_EXT_inline_uniform_block`
* `VK_EXT_line_rasterization`
* `VK_EXT_load_store_op_none`
* `VK_EXT_memory_budget`
* `VK_EXT_memory_priority`
* `VK_EXT_mesh_shader`
* `VK_EXT_metal_surface`
* `VK_EXT_multisampled_render_to_single_sampled`
* `VK_EXT_mutable_descriptor_type`
* `VK_EXT_non_seamless_cube_map`
* `VK_EXT_pageable_device_local_memory`
* `VK_EXT_pci_bus_info`
* `VK_EXT_pipeline_creation_cache_control`
* `VK_EXT_pipeline_creation_feedback`
* `VK_EXT_post_depth_coverage`
* `VK_EXT_primitive_topology_list_restart`
* `VK_EXT_primitives_generated_query`
* `VK_EXT_private_data`
* `VK_EXT_provoking_vertex`
* `VK_EXT_queue_family_foreign`
* `VK_EXT_rasterization_order_attachment_access`
* `VK_EXT_rgba10x6_formats`
* `VK_EXT_robustness2`
* `VK_EXT_sample_locations`
* `VK_EXT_sampler_filter_minmax`
* `VK_EXT_scalar_block_layout`
* `VK_EXT_separate_stencil_usage`
* `VK_EXT_shader_atomic_float`
* `VK_EXT_shader_atomic_float2`
* `VK_EXT_shader_demote_to_helper_invocation`
* `VK_EXT_shader_image_atomic_int64`
* `VK_EXT_shader_stencil_export`
* `VK_EXT_shader_subgroup_ballot`
* `VK_EXT_shader_subgroup_vote`
* `VK_EXT_shader_viewport_index_layer`
* `VK_EXT_subgroup_size_control`
* `VK_EXT_surface_maintenance1`
* `VK_EXT_swapchain_colorspace`
* `VK_EXT_swapchain_maintenance1`
* `VK_EXT_texel_buffer_alignment`
* `VK_EXT_texture_compression_astc_hdr`
* `VK_EXT_tooling_info`
* `VK_EXT_transform_feedback`
* `VK_EXT_validation_cache`
* `VK_EXT_validation_features`
* `VK_EXT_validation_flags`
* `VK_EXT_vertex_attribute_divisor`
* `VK_EXT_vertex_input_dynamic_state`
* `VK_EXT_ycbcr_2plane_444_formats`
* `VK_EXT_ycbcr_image_arrays`
* `VK_GGP_frame_token`
* `VK_GGP_stream_descriptor_surface`
* `VK_GOOGLE_decorate_string`
* `VK_GOOGLE_display_timing`
* `VK_GOOGLE_hlsl_functionality1`
* `VK_GOOGLE_surfaceless_query`
* `VK_GOOGLE_user_type`
* `VK_IMG_filter_cubic`
* `VK_IMG_format_pvrtc`
* `VK_KHR_16bit_storage`
* `VK_KHR_8bit_storage`
* `VK_KHR_android_surface`
* `VK_KHR_bind_memory2`
* `VK_KHR_buffer_device_address`
* `VK_KHR_calibrated_timestamps`
* `VK_KHR_copy_commands2`
* `VK_KHR_create_renderpass2`
* `VK_KHR_dedicated_allocation`
* `VK_KHR_depth_stencil_resolve`
* `VK_KHR_descriptor_update_template`
* `VK_KHR_device_group_creation`
* `VK_KHR_device_group`
* `VK_KHR_display_swapchain`
* `VK_KHR_display`
* `VK_KHR_draw_indirect_count`
* `VK_KHR_driver_properties`
* `VK_KHR_dynamic_rendering`
* `VK_KHR_external_fence_capabilities`
* `VK_KHR_external_fence_fd`
* `VK_KHR_external_fence_win32`
* `VK_KHR_external_fence`
* `VK_KHR_external_memory_capabilities`
* `VK_KHR_external_memory_fd`
* `VK_KHR_external_memory_win32`
* `VK_KHR_external_memory`
* `VK_KHR_external_semaphore_capabilities`
* `VK_KHR_external_semaphore_fd`
* `VK_KHR_external_semaphore_win32`
* `VK_KHR_external_semaphore`
* `VK_KHR_format_feature_flags2`
* `VK_KHR_fragment_shader_barycentric`
* `VK_KHR_fragment_shading_rate`
* `VK_KHR_get_display_properties2`
* `VK_KHR_get_memory_requirements2`
* `VK_KHR_get_physical_device_properties2`
* `VK_KHR_get_surface_capabilities2`
* `VK_KHR_global_priority`
* `VK_KHR_image_format_list`
* `VK_KHR_imageless_framebuffer`
* `VK_KHR_incremental_present`
* `VK_KHR_index_type_uint8`
* `VK_KHR_line_rasterization`
* `VK_KHR_load_store_op_none`
* `VK_KHR_maintenance1`
* `VK_KHR_maintenance2`
* `VK_KHR_maintenance3`
* `VK_KHR_maintenance4`
* `VK_KHR_multiview`
* `VK_KHR_performance_query`
* `VK_KHR_pipeline_executable_properties`
* `VK_KHR_pipeline_library`
* `VK_KHR_present_id`
* `VK_KHR_present_wait`
* `VK_KHR_push_descriptor`
* `VK_KHR_relaxed_block_layout`
* `VK_KHR_sampler_mirror_clamp_to_edge`
* `VK_KHR_sampler_ycbcr_conversion`
* `VK_KHR_separate_depth_stencil_layouts`
* `VK_KHR_shader_atomic_int64`
* `VK_KHR_shader_clock`
* `VK_KHR_shader_draw_parameters`
* `VK_KHR_shader_float_controls`
* `VK_KHR_shader_float16_int8`
* `VK_KHR_shader_integer_dot_product`
* `VK_KHR_shader_non_semantic_info`
* `VK_KHR_shader_subgroup_extended_types`
* `VK_KHR_shader_subgroup_uniform_control_flow`
* `VK_KHR_shader_terminate_invocation`
* `VK_KHR_shared_presentable_image`
* `VK_KHR_spirv_1_4`
* `VK_KHR_storage_buffer_storage_class`
* `VK_KHR_surface_protected_capabilities`
* `VK_KHR_surface`
* `VK_KHR_swapchain_mutable_format`
* `VK_KHR_swapchain`
* `VK_KHR_synchronization2`
* `VK_KHR_timeline_semaphore`
* `VK_KHR_uniform_buffer_standard_layout`
* `VK_KHR_variable_pointers`
* `VK_KHR_vertex_attribute_divisor`
* `VK_KHR_vulkan_memory_model`
* `VK_KHR_wayland_surface`
* `VK_KHR_win32_keyed_mutex`
* `VK_KHR_win32_surface`
* `VK_KHR_workgroup_memory_explicit_layout`
* `VK_KHR_xcb_surface`
* `VK_KHR_xlib_surface`
* `VK_KHR_zero_initialize_workgroup_memory`
* `VK_MVK_macos_surface`
* `VK_NV_compute_shader_derivatives`
* `VK_NV_dedicated_allocation`
* `VK_NV_external_memory_capabilities`
* `VK_NV_external_memory_win32`
* `VK_NV_external_memory`
* `VK_NV_fragment_shader_barycentric`
* `VK_NV_geometry_shader_passthrough`
* `VK_NV_sample_mask_override_coverage`
* `VK_NV_shader_image_footprint`
* `VK_NV_shader_subgroup_partitioned`
* `VK_NV_viewport_array2`
* `VK_NV_win32_keyed_mutex`
* `VK_QCOM_fragment_density_map_offset`
* `VK_QCOM_render_pass_shader_resolve`
* `VK_QCOM_render_pass_store_ops`
* `VK_VALVE_mutable_descriptor_type`

# Unsupported

KHR extensions will definitely be implemented at some point, though KHR extensions that entail a large amount of work may be deferred. EXT extensions are likely to be implemented in future but current plans or priorities may vary. Vendor extensions likely won't be supported but could be upon request given how much demand there is and ease of implementation.

## KHR Extensions

* `VK_KHR_cooperative_matrix`
* `VK_KHR_dynamic_rendering_local_read`
* `VK_KHR_maintenance5`
* `VK_KHR_maintenance6`
* `VK_KHR_map_memory2`
* `VK_KHR_shader_expect_assume`
* `VK_KHR_shader_float_controls2`
* `VK_KHR_shader_maximal_reconvergence`
* `VK_KHR_shader_quad_control`
* `VK_KHR_shader_subgroup_rotate`

## KHR Portability

The portability subset is only relevant on mac, which is not a supported platform.

* `VK_KHR_portability_subset`
* `VK_KHR_portability_enumeration`

## KHR Ray tracing extensions

Ray tracing extensions are now standard and will likely be supported at some point in the future, but they are an immense amount of work to properly support with meaningful tooling - i.e. more than simple raw capture/replay. Because of this and because they are still quite a niche feature they are not a priority to support and will not be until there is enough resources to properly support them, either from myself or an external contributor.

* `VK_KHR_acceleration_structure`
* `VK_KHR_ray_tracing_pipeline`
* `VK_KHR_ray_tracing_position_fetch`
* `VK_KHR_ray_tracing_maintenance1`
* `VK_KHR_ray_query`
* `VK_KHR_deferred_host_operations`

## EXT Extensions

* `VK_EXT_blend_operation_advanced`
* `VK_EXT_depth_bias_control`
* `VK_EXT_descriptor_buffer`
* `VK_EXT_device_address_binding_report`
* `VK_EXT_device_fault`
* `VK_EXT_device_memory_report`
* `VK_EXT_dynamic_rendering_unused_attachments`
* `VK_EXT_external_memory_acquire_unmodified`
* `VK_EXT_external_memory_host`
* `VK_EXT_host_image_copy`
* `VK_EXT_image_compression_control_swapchain`
* `VK_EXT_image_compression_control`
* `VK_EXT_image_drm_format_modifier`
* `VK_EXT_image_sliced_view_of_3d`
* `VK_EXT_layer_settings`
* `VK_EXT_legacy_dithering`
* `VK_EXT_metal_objects`
* `VK_EXT_multi_draw`
* `VK_EXT_nested_command_buffer`
* `VK_EXT_opacity_micromap`
* `VK_EXT_physical_device_drm`
* `VK_EXT_pipeline_library_group_handles`
* `VK_EXT_pipeline_properties`
* `VK_EXT_pipeline_protected_access`
* `VK_EXT_pipeline_robustness`
* `VK_EXT_shader_module_identifier`
* `VK_EXT_shader_object`
* `VK_EXT_shader_tile_image`
* `VK_EXT_subpass_merge_feedback`

## Platform/IHV Extensions

### Android

* `VK_ANDROID_external_format_resolve`

### ARM

* `VK_ARM_rasterization_order_attachment_access`
* `VK_ARM_render_pass_striped`
* `VK_ARM_scheduling_controls`
* `VK_ARM_shader_core_builtins`
* `VK_ARM_shader_core_properties`

### AMD

* `VK_AMD_pipeline_compiler_control`
* `VK_AMD_rasterization_order`
* `VK_AMD_shader_info`
* `VK_AMD_shader_core_properties2`
* `VK_AMD_shader_early_and_late_fragment_tests`

### Fuschia

* `VK_FUCHSIA_external_memory`
* `VK_FUCHSIA_external_semaphore`
* `VK_FUCHSIA_buffer_collection`

### Huawei

* `VK_HUAWEI_cluster_culling_shader`
* `VK_HUAWEI_subpass_shading`
* `VK_HUAWEI_invocation_mask`

### Imagination

* `VK_IMG_relaxed_line_rasterization`

### Intel

* `VK_INTEL_shader_integer_functions2`
* `VK_INTEL_performance_query`

### Microsoft

* `VK_MSFT_layered_driver`

### NV

* `VK_NV_clip_space_w_scaling`
* `VK_NV_cooperative_matrix`
* `VK_NV_copy_memory_indirect`
* `VK_NV_corner_sampled_image`
* `VK_NV_coverage_reduction_mode`
* `VK_NV_cuda_kernel_launch`
* `VK_NV_dedicated_allocation_image_aliasing`
* `VK_NV_descriptor_pool_overallocation`
* `VK_NV_device_diagnostic_checkpoints`
* `VK_NV_device_diagnostics_config`
* `VK_NV_device_generated_commands_compute`
* `VK_NV_device_generated_commands`
* `VK_NV_displacement_micromap`
* `VK_NV_extended_sparse_address_space`
* `VK_NV_external_memory_rdma`
* `VK_NV_fill_rectangle`
* `VK_NV_fragment_coverage_to_color`
* `VK_NV_fragment_shading_rate_enums`
* `VK_NV_framebuffer_mixed_samples`
* `VK_NV_inherited_viewport_scissor`
* `VK_NV_linear_color_attachment`
* `VK_NV_low_latency`
* `VK_NV_low_latency2`
* `VK_NV_memory_decompression`
* `VK_NV_mesh_shader`
* `VK_NV_optical_flow`
* `VK_NV_per_stage_descriptor_set`
* `VK_NV_present_barrier`
* `VK_NV_ray_tracing_invocation_reorder`
* `VK_NV_ray_tracing_motion_blur`
* `VK_NV_representative_fragment_test`
* `VK_NV_scissor_exclusive`
* `VK_NV_shader_sm_builtins`
* `VK_NV_shading_rate_image`
* `VK_NV_viewport_swizzle`

### Qualcomm

* `VK_QCOM_filter_cubic_clamp`
* `VK_QCOM_filter_cubic_weights`
* `VK_QCOM_image_processing`
* `VK_QCOM_image_processing2`
* `VK_QCOM_multiview_per_view_render_areas`
* `VK_QCOM_multiview_per_view_viewports`
* `VK_QCOM_render_pass_transform`
* `VK_QCOM_rotated_copy_commands`
* `VK_QCOM_tile_properties`
* `VK_QCOM_ycbcr_degamma`

### Samsung

* `VK_SEC_amigo_profiling`

### Valve

* `VK_VALVE_descriptor_set_host_mapping`

## WSI for other platforms

These only make sense to implement if the platform as a whole is supported.

* `VK_EXT_directfb_surface`
* `VK_FUCHSIA_imagepipe_surface`
* `VK_NN_vi_surface`
* `VK_NV_acquire_winrt_display`
* `VK_QNX_screen_surface`
* `VK_QNX_external_memory_screen_buffer`

## Deliberately unsupported extensions

These are expected to never be implemented in their current form.

### Complex IHV extensions

* `VK_NV_ray_tracing`

### Vulkan Video extensions

* `VK_KHR_video_decode_av1`
* `VK_KHR_video_decode_h264`
* `VK_KHR_video_decode_h265`
* `VK_KHR_video_decode_queue`
* `VK_KHR_video_encode_h264`
* `VK_KHR_video_encode_h265`
* `VK_KHR_video_maintenance1`
* `VK_KHR_video_queue`

### System/Other tool extensions

* `VK_EXT_frame_boundary`
* `VK_LUNARG_direct_driver_loading`

### Deprecated / experimental / IHV

* `VK_AMD_draw_indirect_count`
* `VK_AMDX_shader_enqueue`
* `VK_KHR_video_encode_queue`
* `VK_EXT_video_encode_h264`
* `VK_EXT_video_encode_h265`
* `VK_EXT_video_decode_h264`
* `VK_EXT_video_decode_h265`
* `VK_MVK_ios_surface`
* `VK_NV_glsl_shader`
* `VK_NVX_binary_import`
* `VK_NVX_multiview_per_view_attributes`
* `VK_NVX_image_view_handle`
