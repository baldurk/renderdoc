# SPIRV-Registry

SPIR-V is a binary intermediate language for representing graphical-shader stages and compute kernels for multiple Khronos APIs, including OpenCL, OpenGL, and Vulkan.

[A complete registry of all official SPIR-V specifications is available at the
Khronos SPIR-V Registry](https://www.khronos.org/registry/spir-v/).

## This Project Contains

- A registry of SPIR-V extensions
- Issue tracking for all SPIR-V specifications
- Pull requests to add new SPIR-V extensions

## Publishing new extension

To publish a new extension, please create a pull request which includes:

- The extension document in the asciidoc format named following
  the `SPV_<vendor>_<name>.asciidoc` pattern. The document should be placed
  in the `extension/<vendor>` folder.
- README.md update with the link to the new extension once published

To publish a non-semantic extended instruction set,

- The instruction set in the asciidoc format named following
  the `NonSemantic.<name>.asciidoc` pattern. The document should be placed
  in the `nonsemantic` folder.
- README.md update with the link to the new extension once published

Please see [BUILD.md](BUILD.md) for instructions to create an HTML specification for this repo.

Note: we no longer push the HTML along side the extension.

## Extension Specifications

### KHR Extensions (Khronos)

* [SPV_KHR_16bit_storage                   ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_16bit_storage.html)
* [SPV_KHR_8bit_storage                    ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_8bit_storage.html)
* [SPV_KHR_bfloat16                        ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_bfloat16.html)
* [SPV_KHR_bit_instructions                ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_bit_instructions.html)
* [SPV_KHR_compute_shader_derivatives      ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_compute_shader_derivatives.html)
* [SPV_KHR_cooperative_matrix              ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_cooperative_matrix.html)
* [SPV_KHR_device_group                    ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_device_group.html)
* [SPV_KHR_expect_assume                   ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_expect_assume.html)
* [SPV_KHR_float_controls                  ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_float_controls.html)
* [SPV_KHR_float_controls2                 ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_float_controls2.html)
* [SPV_KHR_fma                             ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_fma.html)
* [SPV_KHR_fragment_shader_barycentric     ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_fragment_shader_barycentric.html)
* [SPV_KHR_fragment_shading_rate           ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_fragment_shading_rate.html)
* [SPV_KHR_integer_dot_product             ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_integer_dot_product.html)
* [SPV_KHR_linkonce_odr                    ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_linkonce_odr.html)
* [SPV_KHR_maximal_reconvergence           ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_maximal_reconvergence.html)
* [SPV_KHR_multiview                       ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_multiview.html)
* [SPV_KHR_no_integer_wrap_decoration      ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_no_integer_wrap_decoration.html)
* [SPV_KHR_non_semantic_info               ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_non_semantic_info.html)
* [SPV_KHR_physical_storage_buffer         ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_physical_storage_buffer.html)
* [SPV_KHR_post_depth_coverage             ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_post_depth_coverage.html)
* [SPV_KHR_quad_control                    ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_quad_control.html)
* [SPV_KHR_ray_cull_mask                   ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_ray_cull_mask.html)
* [SPV_KHR_ray_query                       ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_ray_query.html)
* [SPV_KHR_ray_tracing                     ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_ray_tracing.html)
* [SPV_KHR_ray_tracing_position_fetch      ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_ray_tracing_position_fetch.html)
* [SPV_KHR_relaxed_extended_instruction    ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_relaxed_extended_instruction.html)
* [SPV_KHR_shader_atomic_counter_ops       ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_shader_atomic_counter_ops.html)
* [SPV_KHR_shader_ballot                   ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_shader_ballot.html)
* [SPV_KHR_shader_clock                    ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_shader_clock.html)
* [SPV_KHR_shader_draw_parameters          ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_shader_draw_parameters.html)
* [SPV_KHR_storage_buffer_storage_class    ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_storage_buffer_storage_class.html)
* [SPV_KHR_subgroup_rotate                 ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_subgroup_rotate.html)
* [SPV_KHR_subgroup_uniform_control_flow   ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_subgroup_uniform_control_flow.html)
* [SPV_KHR_subgroup_vote                   ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_subgroup_vote.html)
* [SPV_KHR_terminate_invocation            ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_terminate_invocation.html)
* [SPV_KHR_uniform_group_instructions      ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_uniform_group_instructions.html)
* [SPV_KHR_untyped_pointers                ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_untyped_pointers.html)
* [SPV_KHR_variable_pointers               ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_variable_pointers.html)
* [SPV_KHR_vulkan_memory_model             ]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_vulkan_memory_model.html)
* [SPV_KHR_workgroup_memory_explicit_layout]( https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_workgroup_memory_explicit_layout.html)

### EXT Extensions (Multivendor)

* [SPV_EXT_arithmetic_fence                ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_arithmetic_fence.html)
* [SPV_EXT_demote_to_helper_invocation     ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_demote_to_helper_invocation.html)
* [SPV_EXT_descriptor_indexing             ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_descriptor_indexing.html)
* [SPV_EXT_float8                          ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_float8.html)
* [SPV_EXT_fragment_fully_covered          ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_fragment_fully_covered.html)
* [SPV_EXT_fragment_invocation_density     ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_fragment_invocation_density.html)
* [SPV_EXT_fragment_shader_interlock       ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_fragment_shader_interlock.html)
* [SPV_EXT_image_raw10_raw12               ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_image_raw10_raw12.html)
* [SPV_EXT_mesh_shader                     ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_mesh_shader.html)
* [SPV_EXT_opacity_micromap                ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_opacity_micromap.html)
* [SPV_EXT_optnone                         ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_optnone.html)
* [SPV_EXT_physical_storage_buffer         ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_physical_storage_buffer.html)
* [SPV_EXT_relaxed_printf_string_address_space]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_relaxed_printf_string_address_space.html)
* [SPV_EXT_replicated_composites           ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_replicated_composites.html)
* [SPV_EXT_shader_64bit_indexing           ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_64bit_indexing.html)
* [SPV_EXT_shader_atomic_float_add         ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_atomic_float_add.html)
* [SPV_EXT_shader_atomic_float_min_max     ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_atomic_float_min_max.html)
* [SPV_EXT_shader_atomic_float16_add       ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_atomic_float16_add.html)
* [SPV_EXT_shader_image_int64              ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_image_int64.html)
* [SPV_EXT_shader_stencil_export           ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_stencil_export.html)
* [SPV_EXT_shader_tile_image               ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_tile_image.html)
* [SPV_EXT_shader_viewport_index_layer     ]( https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_viewport_index_layer.html)

### Vendor Extensions

* [SPV_ALTERA_arbitrary_precision_fixed_point]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_arbitrary_precision_fixed_point.html)
* [SPV_ALTERA_arbitrary_precision_floating_point]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_arbitrary_precision_floating_point.html)
* [SPV_ALTERA_arbitrary_precision_integers ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_arbitrary_precision_integers.html)
* [SPV_ALTERA_blocking_pipes               ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_blocking_pipes.html)
* [SPV_ALTERA_fpga_argument_interfaces     ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_argument_interfaces.html)
* [SPV_ALTERA_fpga_buffer_location         ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_buffer_location.html)
* [SPV_ALTERA_fpga_cluster_attributes      ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_cluster_attributes.html)
* [SPV_ALTERA_fpga_dsp_control             ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_dsp_control.html)
* [SPV_ALTERA_fpga_invocation_pipelining_attributes]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_invocation_pipelining_attributes.html)
* [SPV_ALTERA_fpga_latency_control         ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_latency_control.html)
* [SPV_ALTERA_fpga_loop_controls           ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_loop_controls.html)
* [SPV_ALTERA_fpga_memory_accesses         ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_memory_accesses.html)
* [SPV_ALTERA_fpga_memory_attributes       ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_memory_attributes.html)
* [SPV_ALTERA_fpga_reg                     ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_fpga_reg.html)
* [SPV_ALTERA_global_variable_fpga_decorations]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_global_variable_fpga_decorations.html)
* [SPV_ALTERA_io_pipes                     ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_io_pipes.html)
* [SPV_ALTERA_loop_fuse                    ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_loop_fuse.html)
* [SPV_ALTERA_runtime_aligned              ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_runtime_aligned.html)
* [SPV_ALTERA_task_sequence                ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_task_sequence.html)
* [SPV_ALTERA_usm_storage_classes          ]( https://github.khronos.org/SPIRV-Registry/extensions/ALTERA/SPV_ALTERA_usm_storage_classes.html)
* [SPV_AMD_gcn_shader                      ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_gcn_shader.html)
* [SPV_AMD_gpu_shader_half_float           ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_gpu_shader_half_float.html)
* [SPV_AMD_gpu_shader_half_float_fetch     ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_gpu_shader_half_float_fetch.html)
* [SPV_AMD_gpu_shader_int16                ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_gpu_shader_int16.html)
* [SPV_AMD_shader_ballot                   ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_shader_ballot.html)
* [SPV_AMD_shader_early_and_late_fragment_tests]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_shader_early_and_late_fragment_tests.html)
* [SPV_AMD_shader_explicit_vertex_parameter]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_shader_explicit_vertex_parameter.html)
* [SPV_AMD_shader_fragment_mask            ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_shader_fragment_mask.html)
* [SPV_AMD_shader_image_load_store_lod     ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_shader_image_load_store_lod.html)
* [SPV_AMD_shader_trinary_minmax           ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_shader_trinary_minmax.html)
* [SPV_AMD_texture_gather_bias_lod         ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMD_texture_gather_bias_lod.html)
* [SPV_AMDX_shader_enqueue                 ]( https://github.khronos.org/SPIRV-Registry/extensions/AMD/SPV_AMDX_shader_enqueue.html)
* [SPV_ARM_cooperative_matrix_layouts      ]( https://github.khronos.org/SPIRV-Registry/extensions/ARM/SPV_ARM_cooperative_matrix_layouts.html)
* [SPV_ARM_core_builtins                   ]( https://github.khronos.org/SPIRV-Registry/extensions/ARM/SPV_ARM_core_builtins.html)
* [SPV_ARM_graph                           ]( https://github.khronos.org/SPIRV-Registry/extensions/ARM/SPV_ARM_graph.html)
* [SPV_ARM_tensors                         ]( https://github.khronos.org/SPIRV-Registry/extensions/ARM/SPV_ARM_tensors.html)
* [SPV_GOOGLE_decorate_string              ]( https://github.khronos.org/SPIRV-Registry/extensions/GOOGLE/SPV_GOOGLE_decorate_string.html)
* [SPV_GOOGLE_hlsl_functionality1          ]( https://github.khronos.org/SPIRV-Registry/extensions/GOOGLE/SPV_GOOGLE_hlsl_functionality1.html)
* [SPV_GOOGLE_user_type                    ]( https://github.khronos.org/SPIRV-Registry/extensions/GOOGLE/SPV_GOOGLE_user_type.html)
* [SPV_HUAWEI_cluster_culling_shader       ]( https://github.khronos.org/SPIRV-Registry/extensions/HUAWEI/SPV_HUAWEI_cluster_culling_shader.html)
* [SPV_HUAWEI_subpass_shading              ]( https://github.khronos.org/SPIRV-Registry/extensions/HUAWEI/SPV_HUAWEI_subpass_shading.html)
* [SPV_INTEL_2d_block_io                   ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_2d_block_io.html)
* [SPV_INTEL_bfloat16_conversion           ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_bfloat16_conversion.html)
* [SPV_INTEL_cache_controls                ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_cache_controls.html)
* [SPV_INTEL_device_side_avc_motion_estimation]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_device_side_avc_motion_estimation.html)
* [SPV_INTEL_fp_fast_math_mode             ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_fp_fast_math_mode.html)
* [SPV_INTEL_fp_max_error                  ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_fp_max_error.html)
* [SPV_INTEL_global_variable_host_access   ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_global_variable_host_access.html)
* [SPV_INTEL_int4                          ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_int4.html)
* [SPV_INTEL_kernel_attributes             ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_kernel_attributes.html)
* [SPV_INTEL_long_composites               ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_long_composites.html)
* [SPV_INTEL_masked_gather_scatter         ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_masked_gather_scatter.html)
* [SPV_INTEL_maximum_registers             ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_maximum_registers.html)
* [SPV_INTEL_media_block_io                ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_media_block_io.html)
* [SPV_INTEL_shader_integer_functions2     ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_shader_integer_functions2.html)
* [SPV_INTEL_split_barrier                 ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_split_barrier.html)
* [SPV_INTEL_subgroups                     ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_subgroups.html)
* [SPV_INTEL_subgroup_buffer_prefetch      ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_subgroup_buffer_prefetch.html)
* [SPV_INTEL_subgroup_matrix_multiply_accumulate]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_subgroup_matrix_multiply_accumulate.html)
* [SPV_INTEL_tensor_float32_conversion     ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_tensor_float32_conversion.html)
* [SPV_INTEL_ternary_bitwise_function      ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_ternary_bitwise_function.html)
* [SPV_INTEL_unstructured_loop_controls    ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_unstructured_loop_controls.html)
* [SPV_INTEL_variable_length_array         ]( https://github.khronos.org/SPIRV-Registry/extensions/INTEL/SPV_INTEL_variable_length_array.html)
* [SPV_NV_bindless_texture                 ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_bindless_texture.html)
* [SPV_NV_cluster_acceleration_structure   ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_cluster_acceleration_structure.html)
* [SPV_NV_compute_shader_derivatives       ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_compute_shader_derivatives.html)
* [SPV_NV_cooperative_matrix               ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_cooperative_matrix.html)
* [SPV_NV_cooperative_matrix2              ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_cooperative_matrix2.html)
* [SPV_NV_cooperative_vector               ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_cooperative_vector.html)
* [SPV_NV_displacement_micromap            ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_displacement_micromap.html)
* [SPV_NV_fragment_shader_barycentric      ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_fragment_shader_barycentric.html)
* [SPV_NV_geometry_shader_passthrough      ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_geometry_shader_passthrough.html)
* [SPV_NV_linear_swept_spheres             ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_linear_swept_spheres.html)
* [SPV_NV_mesh_shader                      ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_mesh_shader.html)
* [SPV_NV_raw_access_chains                ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_raw_access_chains.html)
* [SPV_NV_ray_tracing                      ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_ray_tracing.html)
* [SPV_NV_ray_tracing_motion_blur          ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_ray_tracing_motion_blur.html)
* [SPV_NV_sample_mask_override_coverage    ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_sample_mask_override_coverage.html)
* [SPV_NV_shader_atomic_fp16_vector        ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_shader_atomic_fp16_vector.html)
* [SPV_NV_shader_image_footprint           ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_shader_image_footprint.html)
* [SPV_NV_shader_invocation_reorder        ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_shader_invocation_reorder.html)
* [SPV_NV_shader_sm_builtins               ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_shader_sm_builtins.html)
* [SPV_NV_shader_subgroup_partitioned      ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_shader_subgroup_partitioned.html)
* [SPV_NV_shading_rate                     ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_shading_rate.html)
* [SPV_NV_stereo_view_rendering            ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_stereo_view_rendering.html)
* [SPV_NV_tensor_addressing                ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_tensor_addressing.html)
* [SPV_NV_viewport_array2                  ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NV_viewport_array2.html)
* [SPV_NVX_multiview_per_view_attributes   ]( https://github.khronos.org/SPIRV-Registry/extensions/NV/SPV_NVX_multiview_per_view_attributes.html)
* [SPV_QCOM_cooperative_matrix_conversion  ]( https://github.khronos.org/SPIRV-Registry/extensions/QCOM/SPV_QCOM_cooperative_matrix_conversion.html)
* [SPV_QCOM_image_processing               ]( https://github.khronos.org/SPIRV-Registry/extensions/QCOM/SPV_QCOM_image_processing.html)
* [SPV_QCOM_image_processing2              ]( https://github.khronos.org/SPIRV-Registry/extensions/QCOM/SPV_QCOM_image_processing2.html)
* [SPV_QCOM_tile_shading                   ]( https://github.khronos.org/SPIRV-Registry/extensions/QCOM/SPV_QCOM_tile_shading.html)

## Non-Semantic Extended Instruction Set Specifications

* [NonSemantic.ClspvReflection             ]( https://github.khronos.org/SPIRV-Registry/nonsemantic/NonSemantic.ClspvReflection.html)
* [NonSemantic.DebugBreak                  ]( https://github.khronos.org/SPIRV-Registry/nonsemantic/NonSemantic.DebugBreak.html)
* [NonSemantic.DebugPrintf                 ]( https://github.khronos.org/SPIRV-Registry/nonsemantic/NonSemantic.DebugPrintf.html)
* [NonSemantic.Shader.DebugInfo.100        ]( https://github.khronos.org/SPIRV-Registry/nonsemantic/NonSemantic.Shader.DebugInfo.100.html)

## Extended Instruction Set Specifications

* [TOSA.001000.1                           ]( https://github.khronos.org/SPIRV-Registry/extended/TOSA.001000.1.html)
