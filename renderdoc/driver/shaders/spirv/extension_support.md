# SPIR-V extension and capability support

This is a list of the currently supported SPIR-V extensions and capabilities in the shader debugger, in a bit more readable format than the code.

Maintainers can update this file by updating `spirv.core.grammar.json` and `spirv_registry.md` (the SPIRV-Registry README.md) in this folder and running `./check_extensions.sh` which will output any new extensions that haven't been filed in a category below. This will also update `all_exts.txt` and `all_capabilities.txt` which needs to be committed too when this file is updated to keep things in sync.

Each extension references the relevant capabilities below it.

# Supported

* `SPIR-V 1.0`
  * `AtomicStorage`
  * `ClipDistance`
  * `CullDistance`
  * `DerivativeControl`
  * `Float16`
  * `Float64`
  * `Geometry`
  * `GeometryPointSize`
  * `GeometryStreams`
  * `Image1D`
  * `ImageBuffer`
  * `ImageCubeArray`
  * `ImageGatherExtended`
  * `ImageMSArray`
  * `ImageQuery`
  * `ImageRect`
  * `InputAttachment`
  * `Int16`
  * `Int64`
  * `Int64Atomics`
  * `Int8`
  * `Matrix`
  * `MinLod`
  * `MultiViewport`
  * `Sampled1D`
  * `SampledBuffer`
  * `SampledCubeArray`
  * `SampledImageArrayDynamicIndexing`
  * `SampledRect`
  * `SampleRateShading`
  * `Shader`
  * `StorageBufferArrayDynamicIndexing`
  * `StorageImageArrayDynamicIndexing`
  * `StorageImageExtendedFormats`
  * `StorageImageMultisample`
  * `StorageImageReadWithoutFormat`
  * `StorageImageWriteWithoutFormat`
  * `Tessellation`
  * `TessellationPointSize`
  * `TransformFeedback`
  * `UniformBufferArrayDynamicIndexing`
* `SPIR-V 1.1`
* `SPIR-V 1.2`
* `SPIR-V 1.3`
  * `DrawParameters`
  * `GroupNonUniform`
  * `GroupNonUniformArithmetic`
  * `GroupNonUniformBallot`
  * `GroupNonUniformClustered`
  * `GroupNonUniformQuad`
  * `GroupNonUniformShuffle`
  * `GroupNonUniformShuffleRelative`
  * `GroupNonUniformVote`
* `SPIR-V 1.4`
  * `SignedZeroInfNanPreserve`
* `SPIR-V 1.5`
  * `InputAttachmentArrayDynamicIndexing`
  * `InputAttachmentArrayNonUniformIndexing`
  * `RuntimeDescriptorArray`
  * `SampledImageArrayNonUniformIndexing`
  * `ShaderLayer`
  * `ShaderNonUniform`
  * `ShaderViewportIndex`
  * `StorageBufferArrayNonUniformIndexing`
  * `StorageImageArrayNonUniformIndexing`
  * `StorageTexelBufferArrayDynamicIndexing`
  * `StorageTexelBufferArrayNonUniformIndexing`
  * `UniformBufferArrayNonUniformIndexing`
  * `UniformTexelBufferArrayDynamicIndexing`
  * `UniformTexelBufferArrayNonUniformIndexing`
  * `VulkanMemoryModel`
  * `VulkanMemoryModelDeviceScope`
* `SPIR-V 1.6`
  * `DemoteToHelperInvocation`
  * `UniformDecoration`
* `SPV_KHR_16bit_storage`
  * `StorageBuffer16BitAccess`
  * `StorageInputOutput16`
  * `StoragePushConstant16`
  * `StorageUniform16`
  * `StorageUniformBufferBlock16`
  * `UniformAndStorageBuffer16BitAccess`
* `SPV_KHR_8bit_storage`
  * `StorageBuffer8BitAccess`
  * `StoragePushConstant8`
  * `UniformAndStorageBuffer8BitAccess`
* `SPV_KHR_bit_instructions`
  * `BitInstructions`
* `SPV_KHR_device_group`
  * `DeviceGroup`
* `SPV_KHR_expect_assume`
  * `ExpectAssumeKHR`
* `SPV_KHR_float_controls`
* `SPV_KHR_maximal_reconvergence`
* `SPV_KHR_multiview`
  * `MultiView`
* `SPV_KHR_no_integer_wrap_decoration`
* `SPV_KHR_non_semantic_info`
* `SPV_KHR_physical_storage_buffer`
  * `PhysicalStorageBufferAddresses`
* `SPV_KHR_post_depth_coverage`
  * `SampleMaskPostDepthCoverage`
* `SPV_KHR_quad_control`
  * `QuadControlKHR`
* `SPV_KHR_relaxed_extended_instruction`
* `SPV_KHR_shader_atomic_counter_ops`
  * `AtomicStorageOps`
* `SPV_KHR_shader_ballot`
  * `SubgroupBallotKHR`
* `SPV_KHR_shader_clock`
  * `ShaderClockKHR`
* `SPV_KHR_shader_draw_parameters`
* `SPV_KHR_storage_buffer_storage_class`
* `SPV_KHR_subgroup_rotate`
  * `GroupNonUniformRotateKHR`
* `SPV_KHR_subgroup_uniform_control_flow`
* `SPV_KHR_subgroup_vote`
  * `SubgroupVoteKHR`
* `SPV_KHR_terminate_invocation`
* `SPV_KHR_vulkan_memory_model`
  * `VulkanMemoryModelKHR`
  * `VulkanMemoryModelDeviceScopeKHR`
* `SPV_EXT_demote_to_helper_invocation`
  * `DemoteToHelperInvocationEXT`
* `SPV_EXT_descriptor_indexing`
  * `InputAttachmentArrayDynamicIndexingEXT`
  * `InputAttachmentArrayNonUniformIndexingEXT`
  * `RuntimeDescriptorArrayEXT`
  * `SampledImageArrayNonUniformIndexingEXT`
  * `ShaderNonUniformEXT`
  * `StorageBufferArrayNonUniformIndexingEXT`
  * `StorageImageArrayNonUniformIndexingEXT`
  * `StorageTexelBufferArrayDynamicIndexingEXT`
  * `StorageTexelBufferArrayNonUniformIndexingEXT`
  * `UniformBufferArrayNonUniformIndexingEXT`
  * `UniformTexelBufferArrayDynamicIndexingEXT`
  * `UniformTexelBufferArrayNonUniformIndexingEXT`
* `SPV_EXT_fragment_fully_covered`
  * `FragmentFullyCoveredEXT`
* `SPV_EXT_fragment_invocation_density`
  * `FragmentDensityEXT`
* `SPV_EXT_mesh_shader`
  * `MeshShadingEXT`
* `SPV_EXT_physical_storage_buffer`
  * `PhysicalStorageBufferAddressesEXT`
* `SPV_EXT_shader_atomic_float_add`
  * `AtomicFloat32AddEXT`
  * `AtomicFloat64AddEXT`
* `SPV_EXT_shader_atomic_float_min_max`
  * `AtomicFloat16MinMaxEXT`
  * `AtomicFloat32MinMaxEXT`
  * `AtomicFloat64MinMaxEXT`
* `SPV_EXT_shader_atomic_float16_add`
  * `AtomicFloat16AddEXT`
* `SPV_EXT_shader_image_int64`
  * `Int64ImageEXT`
* `SPV_EXT_shader_stencil_export`
  * `StencilExportEXT`
* `SPV_EXT_shader_viewport_index_layer`
  * `ShaderViewportIndexLayerEXT`
* `SPV_GOOGLE_decorate_string`
* `SPV_GOOGLE_hlsl_functionality1`
* `SPV_GOOGLE_user_type`

# Unsupported

KHR extensions will definitely be implemented at some point, though KHR extensions that entail a large amount of work may be deferred. EXT extensions are likely to be implemented in future but current plans or priorities may vary. Vendor extensions likely won't be supported but could be upon request given how much demand there is and ease of implementation.

## KHR Extensions

* `SPIR-V 1.0`
  * `Groups`
  * `InterpolationFunction`
  * `SparseResidency`
* `SPIR-V 1.4`
  * `DenormFlushToZero`
  * `DenormPreserve`
  * `RoundingModeRTE`
  * `RoundingModeRTZ`
* `SPIR-V 1.6`
  * `DotProduct`
  * `DotProductInput4x8Bit`
  * `DotProductInput4x8BitPacked`
  * `DotProductInputAll`
* `SPV_KHR_bfloat16`
  * `BFloat16TypeKHR`
  * `BFloat16DotProductKHR`
  * `BFloat16CooperativeMatrixKHR`
* `SPV_KHR_compute_shader_derivatives`
  * `ComputeDerivativeGroupQuadsKHR`
  * `ComputeDerivativeGroupLinearKHR`
* `SPV_KHR_cooperative_matrix`
  * `CooperativeMatrixKHR`
* `SPV_KHR_float_controls2`
  * `FloatControls2`
* `SPV_KHR_fma`
  * `FMAKHR`
* `SPV_KHR_fragment_shader_barycentric`
  * `FragmentBarycentricKHR`
* `SPV_KHR_fragment_shading_rate`
  * `FragmentShadingRateKHR`
* `SPV_KHR_integer_dot_product`
* `SPV_KHR_untyped_pointers`
  * `UntypedPointersKHR`
* `SPV_KHR_variable_pointers`
  * `VariablePointers`
  * `VariablePointersStorageBuffer`
* `SPV_KHR_workgroup_memory_explicit_layout`
  * `WorkgroupMemoryExplicitLayout16BitAccessKHR`
  * `WorkgroupMemoryExplicitLayout8BitAccessKHR`
  * `WorkgroupMemoryExplicitLayoutKHR`

## KHR Ray tracing extensions

* `SPV_KHR_ray_cull_mask`
  * `RayCullMaskKHR`
* `SPV_KHR_ray_query`
  * `RayQueryKHR`
* `SPV_KHR_ray_tracing_position_fetch`
  * `RayTracingPositionFetchKHR`
  * `RayQueryPositionFetchKHR`
* `SPV_KHR_ray_tracing`
  * `RayTracingKHR`
  * `RayTraversalPrimitiveCullingKHR`
* `SPV_EXT_opacity_micromap`
  * `RayTracingOpacityMicromapEXT`

## EXT

* `SPV_EXT_float8`
  * `Float8EXT`
  * `Float8CooperativeMatrixEXT`
* `SPV_EXT_fragment_shader_interlock`
  * `FragmentShaderSampleInterlockEXT`
  * `FragmentShaderShadingRateInterlockEXT`
  * `FragmentShaderPixelInterlockEXT`
* `SPV_EXT_replicated_composites`
  * `ReplicatedCompositesEXT`
* `SPV_EXT_shader_64bit_indexing`
  * `Shader64BitIndexingEXT`
* `SPV_EXT_shader_tile_image`
  * `TileImageColorReadAccessEXT`
  * `TileImageDepthReadAccessEXT`
  * `TileImageStencilReadAccessEXT`

## Platform/IHV Extensions

### Altera

* `SPV_ALTERA_arbitrary_precision_fixed_point`
    * `ArbitraryPrecisionFixedPointALTERA`
* `SPV_ALTERA_arbitrary_precision_floating_point`
    * `ArbitraryPrecisionFloatingPointALTERA`
* `SPV_ALTERA_arbitrary_precision_integers`
  * `ArbitraryPrecisionIntegersALTERA`
* `SPV_ALTERA_blocking_pipes`
    * `BlockingPipesALTERA`
* `SPV_ALTERA_fpga_argument_interfaces`
    * `FPGAArgumentInterfacesALTERA`
* `SPV_ALTERA_fpga_buffer_location`
    * `FPGABufferLocationALTERA`
* `SPV_ALTERA_fpga_cluster_attributes`
    * `FPGAClusterAttributesALTERA`
    * `FPGAClusterAttributesV2ALTERA`
* `SPV_ALTERA_fpga_dsp_control`
    * `FPGADSPControlALTERA`
* `SPV_ALTERA_fpga_invocation_pipelining_attributes`
    * `FPGAInvocationPipeliningAttributesALTERA`
* `SPV_ALTERA_fpga_latency_control`
    * `FPGALatencyControlALTERA`
* `SPV_ALTERA_fpga_loop_controls`
    * `FPGALoopControlsALTERA`
* `SPV_ALTERA_fpga_memory_accesses`
    * `FPGAMemoryAccessesALTERA`
* `SPV_ALTERA_fpga_memory_attributes`
    * `FPGAMemoryAttributesALTERA`
* `SPV_ALTERA_fpga_reg`
    * `FPGARegALTERA`
* `SPV_ALTERA_global_variable_fpga_decorations`
    * `GlobalVariableFPGADecorationsALTERA`
* `SPV_ALTERA_io_pipes`
    * `IOPipesALTERA`
* `SPV_ALTERA_loop_fuse`
  * `LoopFuseALTERA`
* `SPV_ALTERA_runtime_aligned`
    * `RuntimeAlignedAttributeALTERA`
* `SPV_ALTERA_task_sequence`
    * `TaskSequenceALTERA`
* `SPV_ALTERA_usm_storage_classes`
    * `USMStorageClassesALTERA`

### AMD

* `SPV_AMD_gcn_shader`
* `SPV_AMD_gpu_shader_half_float_fetch`
  * `Float16ImageAMD`
* `SPV_AMD_gpu_shader_half_float`
* `SPV_AMD_gpu_shader_int16`
* `SPV_AMD_shader_ballot`
* `SPV_AMD_shader_early_and_late_fragment_tests`
  * `EarlyAndLateFragmentTestsAMD`
  * `StencilRefUnchangedFrontAMD`
  * `StencilRefGreaterFrontAMD`
  * `StencilRefLessFrontAMD`
  * `StencilRefUnchangedBackAMD`
  * `StencilRefGreaterBackAMD`
  * `StencilRefLessBackAMD`
* `SPV_AMD_shader_explicit_vertex_parameter`
* `SPV_AMD_shader_fragment_mask`
  * `FragmentMaskAMD`
* `SPV_AMD_shader_image_load_store_lod`
  * `ImageReadWriteLodAMD`
* `SPV_AMD_shader_trinary_minmax`
* `SPV_AMD_texture_gather_bias_lod`
  * `ImageGatherBiasLodAMD`

### ARM

* `SPV_ARM_cooperative_matrix_layouts`
  * `CooperativeMatrixLayoutsARM`
* `SPV_ARM_core_builtins`
  * `CoreBuiltinsARM`
* `SPV_ARM_graph`
  * `GraphARM`
* `SPV_ARM_tensors`
  * `TensorsARM`
  * `StorageTensorArrayDynamicIndexingARM`
  * `StorageTensorArrayNonUniformIndexingARM`

### Huawei

* `SPV_HUAWEI_cluster_culling_shader`
* `SPV_HUAWEI_subpass_shading`

### Intel

* `SPV_INTEL_2d_block_io`
  * `Subgroup2DBlockIOINTEL`
  * `Subgroup2DBlockTransformINTEL`
  * `Subgroup2DBlockTransposeINTEL`
* `SPV_INTEL_bfloat16_conversion`
  * `BFloat16ConversionINTEL`
* `SPV_INTEL_cache_controls`
  * `CacheControlsINTEL`
* `SPV_INTEL_device_side_avc_motion_estimation`
  * `SubgroupAvcMotionEstimationChromaINTEL`
  * `SubgroupAvcMotionEstimationINTEL`
  * `SubgroupAvcMotionEstimationIntraINTEL`
* `SPV_INTEL_fp_fast_math_mode`
  * `FPFastMathModeINTEL`
* `SPV_INTEL_fp_max_error`
  * `FPMaxErrorINTEL`
* `SPV_INTEL_global_variable_host_access`
  * `GlobalVariableHostAccessINTEL`
* `SPV_INTEL_int4`
  * `Int4TypeINTEL`
  * `Int4CooperativeMatrixINTEL`
* `SPV_INTEL_kernel_attributes`
  * `FPGAKernelAttributesINTEL`
  * `FPGAKernelAttributesv2INTEL`
  * `KernelAttributesINTEL`
* `SPV_INTEL_long_composites`
  * `LongCompositesINTEL`
* `SPV_INTEL_masked_gather_scatter`
  * `MaskedGatherScatterINTEL`
* `SPV_INTEL_maximum_registers`
  * `RegisterLimitsINTEL`
* `SPV_INTEL_media_block_io`
  * `SubgroupImageMediaBlockIOINTEL`
* `SPV_INTEL_shader_integer_functions2`
  * `IntegerFunctions2INTEL`
* `SPV_INTEL_split_barrier`
  * `SplitBarrierINTEL`
* `SPV_INTEL_subgroups`
  * `SubgroupBufferBlockIOINTEL`
  * `SubgroupImageBlockIOINTEL`
  * `SubgroupShuffleINTEL`
* `SPV_INTEL_subgroup_buffer_prefetch`
  * `SubgroupBufferPrefetchINTEL`
* `SPV_INTEL_subgroup_matrix_multiply_accumulate`
  * `SubgroupMatrixMultiplyAccumulateINTEL`
* `SPV_INTEL_tensor_float32_conversion`
  * `TensorFloat32RoundingINTEL`
* `SPV_INTEL_ternary_bitwise_function`
  * `TernaryBitwiseFunctionINTEL`
* `SPV_INTEL_unstructured_loop_controls`
  * `UnstructuredLoopControlsINTEL`
* `SPV_INTEL_variable_length_array`
  * `VariableLengthArrayINTEL`
  * `UntypedVariableLengthArrayINTEL`

### NV

* `SPV_NV_bindless_texture`
  * `BindlessTextureNV`
* `SPV_NV_cluster_acceleration_structure`
  * `RayTracingClusterAccelerationStructureNV`
* `SPV_NV_compute_shader_derivatives`
  * `ComputeDerivativeGroupLinearNV`
  * `ComputeDerivativeGroupQuadsNV`
* `SPV_NV_cooperative_matrix`
  * `CooperativeMatrixNV`
* `SPV_NV_cooperative_matrix2`
  * `CooperativeMatrixReductionsNV`
  * `CooperativeMatrixConversionsNV`
  * `CooperativeMatrixPerElementOperationsNV`
  * `CooperativeMatrixTensorAddressingNV`
  * `CooperativeMatrixBlockLoadsNV`
* `SPV_NV_cooperative_vector`
  * `CooperativeVectorNV`
  * `CooperativeVectorTrainingNV`
* `SPV_NV_displacement_micromap`
  * `RayTracingDisplacementMicromapNV`
  * `DisplacementMicromapNV`
* `SPV_NV_fragment_shader_barycentric`
  * `FragmentBarycentricNV`
* `SPV_NV_geometry_shader_passthrough`
  * `GeometryShaderPassthroughNV`
* `SPV_NV_linear_swept_spheres`
  * `RayTracingSpheresGeometryNV`
  * `RayTracingLinearSweptSpheresGeometryNV`
* `SPV_NV_mesh_shader`
  * `MeshShadingNV`
* `SPV_NV_raw_access_chains`
  * `RawAccessChainsNV`
* `SPV_NV_ray_tracing_motion_blur`
  * `RayTracingMotionBlurNV`
* `SPV_NV_ray_tracing`
  * `RayTracingNV`
* `SPV_NV_sample_mask_override_coverage`
  * `SampleMaskOverrideCoverageNV`
* `SPV_NV_shader_atomic_fp16_vector`
  * `AtomicFloat16VectorNV`
* `SPV_NV_shader_image_footprint`
  * `ImageFootprintNV`
* `SPV_NV_shader_invocation_reorder`
  * `ShaderInvocationReorderNV`
* `SPV_NV_shader_sm_builtins`
  * `ShaderSMBuiltinsNV`
* `SPV_NV_shader_subgroup_partitioned`
  * `GroupNonUniformPartitionedNV`
* `SPV_NV_shading_rate`
  * `ShadingRateNV`
* `SPV_NV_stereo_view_rendering`
  * `ShaderStereoViewNV`
* `SPV_NV_tensor_addressing`
  * `TensorAddressingNV`
* `SPV_NV_viewport_array2`
  * `ShaderViewportIndexLayerNV`
  * `ShaderViewportMaskNV`

### Qualcomm

* `SPV_QCOM_cooperative_matrix_conversion`
  * `CooperativeMatrixConversionQCOM`
* `SPV_QCOM_image_processing`
  * `TextureSampleWeightedQCOM`
  * `TextureBoxFilterQCOM`
  * `TextureBlockMatchQCOM`
* `SPV_QCOM_image_processing2`
  * `TextureBlockMatch2QCOM`
* `SPV_QCOM_tile_shading`
  * `TileShadingQCOM`

### Deprecated / experimental / undocumented / kernel only

* `SPIR-V 1.0`
  * `Addresses`
  * `DeviceEnqueue`
  * `Float16Buffer`
  * `GenericPointer`
  * `ImageBasic`
  * `ImageMipmap`
  * `ImageReadWrite`
  * `Kernel`
  * `Linkage`
  * `LiteralSampler`
  * `Pipes`
  * `Vector16`
* `SPIR-V 1.1`
  * `NamedBarrier`
  * `PipeStorage`
  * `SubgroupDispatch`
* Provisional ray tracing
  * `RayQueryProvisionalKHR`
  * `RayTracingProvisionalKHR`
* `SPV_KHR_linkonce_odr`
* `SPV_KHR_uniform_group_instructions`
  * `GroupUniformArithmeticKHR`
* `SPV_EXT_arithmetic_fence`
  * `ArithmeticFenceEXT`
* `SPV_EXT_image_raw10_raw12`
* `SPV_EXT_optnone`
  * `OptNoneEXT`
* `SPV_EXT_relaxed_printf_string_address_space`
* `SPV_AMDX_shader_enqueue`
  * `ShaderEnqueueAMDX`
* `SPV_NVX_multiview_per_view_attributes`
  * `PerViewAttributesNV`

#### Dead Capabilities

For some reason Intel has dumped a load of capabilities/extensions that are undocumented into the registry. Listed here to keep the scripts happy.

  * `DebugInfoModuleINTEL`
  * `FunctionPointersINTEL`
  * `IndirectReferencesINTEL`
  * `AsmINTEL`
  * `SpecConditionalINTEL`
  * `FunctionVariantsINTEL`
  * `AsmINTEL`
  * `VectorAnyINTEL`
  * `VectorComputeINTEL`
  * `RoundToInfinityINTEL`
  * `FloatingPointModeINTEL`
  * `VariableLengthArrayINTEL`
  * `FunctionFloatControlINTEL`
  * `MemoryAccessAliasingINTEL`
  * `BindlessImagesINTEL`

