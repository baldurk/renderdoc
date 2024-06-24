/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "vk_info.h"
#include "core/settings.h"
#include "lz4/lz4.h"
#include "vk_core.h"

// for compatibility we use the same DXBC name since it's now configured by the UI
RDOC_EXTERN_CONFIG(rdcarray<rdcstr>, DXBC_Debug_SearchDirPaths);

VkDynamicState ConvertDynamicState(VulkanDynamicStateIndex idx)
{
  switch(idx)
  {
    case VkDynamicViewport: return VK_DYNAMIC_STATE_VIEWPORT;
    case VkDynamicScissor: return VK_DYNAMIC_STATE_SCISSOR;
    case VkDynamicLineWidth: return VK_DYNAMIC_STATE_LINE_WIDTH;
    case VkDynamicDepthBias: return VK_DYNAMIC_STATE_DEPTH_BIAS;
    case VkDynamicBlendConstants: return VK_DYNAMIC_STATE_BLEND_CONSTANTS;
    case VkDynamicDepthBounds: return VK_DYNAMIC_STATE_DEPTH_BOUNDS;
    case VkDynamicStencilCompareMask: return VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
    case VkDynamicStencilWriteMask: return VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
    case VkDynamicStencilReference: return VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    case VkDynamicViewportWScalingNV: return VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV;
    case VkDynamicDiscardRectangleEXT: return VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT;
    case VkDynamicDiscardRectangleEnableEXT: return VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT;
    case VkDynamicDiscardRectangleModeEXT: return VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT;
    case VkDynamicSampleLocationsEXT: return VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT;
    case VkDynamicRayTracingStackSizeKHR:
      return VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR;
    case VkDynamicViewportShadingRatePaletteNV:
      return VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV;
    case VkDynamicViewportCoarseSampleOrderNV:
      return VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV;
    case VkDynamicExclusiveScissorNV: return VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV;
    case VkDynamicExclusiveScissorEnableNV: return VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV;
    case VkDynamicShadingRateKHR: return VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR;
    case VkDynamicLineStippleKHR: return VK_DYNAMIC_STATE_LINE_STIPPLE_KHR;
    case VkDynamicCullMode: return VK_DYNAMIC_STATE_CULL_MODE;
    case VkDynamicFrontFace: return VK_DYNAMIC_STATE_FRONT_FACE;
    case VkDynamicPrimitiveTopology: return VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;
    case VkDynamicViewportCount: return VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;
    case VkDynamicScissorCount: return VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT;
    case VkDynamicVertexInputBindingStride: return VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE;
    case VkDynamicDepthTestEnable: return VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE;
    case VkDynamicDepthWriteEnable: return VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE;
    case VkDynamicDepthCompareOp: return VK_DYNAMIC_STATE_DEPTH_COMPARE_OP;
    case VkDynamicDepthBoundsTestEnable: return VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE;
    case VkDynamicStencilTestEnable: return VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE;
    case VkDynamicStencilOp: return VK_DYNAMIC_STATE_STENCIL_OP;
    case VkDynamicVertexInputEXT: return VK_DYNAMIC_STATE_VERTEX_INPUT_EXT;
    case VkDynamicControlPointsEXT: return VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT;
    case VkDynamicRastDiscard: return VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE;
    case VkDynamicDepthBiasEnable: return VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE;
    case VkDynamicLogicOpEXT: return VK_DYNAMIC_STATE_LOGIC_OP_EXT;
    case VkDynamicPrimRestart: return VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE;
    case VkDynamicColorWriteEXT: return VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT;
    case VkDynamicTessDomainOriginEXT: return VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT;
    case VkDynamicDepthClampEnableEXT: return VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT;
    case VkDynamicPolygonModeEXT: return VK_DYNAMIC_STATE_POLYGON_MODE_EXT;
    case VkDynamicRasterizationSamplesEXT: return VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT;
    case VkDynamicSampleMaskEXT: return VK_DYNAMIC_STATE_SAMPLE_MASK_EXT;
    case VkDynamicAlphaToCoverageEXT: return VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT;
    case VkDynamicAlphaToOneEXT: return VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT;
    case VkDynamicLogicOpEnableEXT: return VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT;
    case VkDynamicColorBlendEnableEXT: return VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT;
    case VkDynamicColorBlendEquationEXT: return VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT;
    case VkDynamicColorWriteMaskEXT: return VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT;
    case VkDynamicRasterizationStreamEXT: return VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT;
    case VkDynamicConservativeRastModeEXT:
      return VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT;
    case VkDynamicOverstimationSizeEXT:
      return VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT;
    case VkDynamicDepthClipEnableEXT: return VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT;
    case VkDynamicSampleLocationsEnableEXT: return VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT;
    case VkDynamicStateColorBlendAdvancedEXT: return VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT;
    case VkDynamicProvokingVertexModeEXT: return VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT;
    case VkDynamicLineRastModeEXT: return VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT;
    case VkDynamicLineStippleEnableEXT: return VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT;
    case VkDynamicDepthClipNegativeOneEXT:
      return VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT;
    case VkDynamicViewportWScalingEXT: return VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV;
    case VkDynamicViewportSwizzleEXT: return VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV;
    case VkDynamicCoverageToColorEnableEXT: return VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV;
    case VkDynamicCoverageToColorLocationEXT: return VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV;
    case VkDynamicCoverageModulationModeEXT: return VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV;
    case VkDynamicCoverageModulationTableEnableEXT:
      return VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV;
    case VkDynamicCoverageModulationTableEXT: return VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV;
    case VkDynamicShadingRateImageEnableEXT: return VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV;
    case VkDynamicRepresentativeFragTestEXT:
      return VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV;
    case VkDynamicCoverageReductionModeEXT: return VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV;
    case VkDynamicAttachmentFeedbackLoopEnableEXT:
      return VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT;
    case VkDynamicCount: break;
  }

  RDCERR("Unexpected vulkan dynamic state index %u", idx);

  return VK_DYNAMIC_STATE_MAX_ENUM;
}

VulkanDynamicStateIndex ConvertDynamicState(VkDynamicState state)
{
  switch(state)
  {
    case VK_DYNAMIC_STATE_VIEWPORT: return VkDynamicViewport;
    case VK_DYNAMIC_STATE_SCISSOR: return VkDynamicScissor;
    case VK_DYNAMIC_STATE_LINE_WIDTH: return VkDynamicLineWidth;
    case VK_DYNAMIC_STATE_DEPTH_BIAS: return VkDynamicDepthBias;
    case VK_DYNAMIC_STATE_BLEND_CONSTANTS: return VkDynamicBlendConstants;
    case VK_DYNAMIC_STATE_DEPTH_BOUNDS: return VkDynamicDepthBounds;
    case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK: return VkDynamicStencilCompareMask;
    case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK: return VkDynamicStencilWriteMask;
    case VK_DYNAMIC_STATE_STENCIL_REFERENCE: return VkDynamicStencilReference;
    case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV: return VkDynamicViewportWScalingNV;
    case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT: return VkDynamicDiscardRectangleEXT;
    case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT: return VkDynamicDiscardRectangleEnableEXT;
    case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT: return VkDynamicDiscardRectangleModeEXT;
    case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT: return VkDynamicSampleLocationsEXT;
    case VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR:
      return VkDynamicRayTracingStackSizeKHR;
    case VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV:
      return VkDynamicViewportShadingRatePaletteNV;
    case VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV:
      return VkDynamicViewportCoarseSampleOrderNV;
    case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV: return VkDynamicExclusiveScissorNV;
    case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV: return VkDynamicExclusiveScissorEnableNV;
    case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR: return VkDynamicShadingRateKHR;
    case VK_DYNAMIC_STATE_LINE_STIPPLE_KHR: return VkDynamicLineStippleKHR;
    case VK_DYNAMIC_STATE_CULL_MODE: return VkDynamicCullMode;
    case VK_DYNAMIC_STATE_FRONT_FACE: return VkDynamicFrontFace;
    case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY: return VkDynamicPrimitiveTopology;
    case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT: return VkDynamicViewportCount;
    case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT: return VkDynamicScissorCount;
    case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE: return VkDynamicVertexInputBindingStride;
    case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE: return VkDynamicDepthTestEnable;
    case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE: return VkDynamicDepthWriteEnable;
    case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP: return VkDynamicDepthCompareOp;
    case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE: return VkDynamicDepthBoundsTestEnable;
    case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE: return VkDynamicStencilTestEnable;
    case VK_DYNAMIC_STATE_STENCIL_OP: return VkDynamicStencilOp;
    case VK_DYNAMIC_STATE_VERTEX_INPUT_EXT: return VkDynamicVertexInputEXT;
    case VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT: return VkDynamicControlPointsEXT;
    case VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE: return VkDynamicRastDiscard;
    case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE: return VkDynamicDepthBiasEnable;
    case VK_DYNAMIC_STATE_LOGIC_OP_EXT: return VkDynamicLogicOpEXT;
    case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE: return VkDynamicPrimRestart;
    case VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT: return VkDynamicColorWriteEXT;
    case VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT: return VkDynamicTessDomainOriginEXT;
    case VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT: return VkDynamicDepthClampEnableEXT;
    case VK_DYNAMIC_STATE_POLYGON_MODE_EXT: return VkDynamicPolygonModeEXT;
    case VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT: return VkDynamicRasterizationSamplesEXT;
    case VK_DYNAMIC_STATE_SAMPLE_MASK_EXT: return VkDynamicSampleMaskEXT;
    case VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT: return VkDynamicAlphaToCoverageEXT;
    case VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT: return VkDynamicAlphaToOneEXT;
    case VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT: return VkDynamicLogicOpEnableEXT;
    case VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT: return VkDynamicColorBlendEnableEXT;
    case VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT: return VkDynamicColorBlendEquationEXT;
    case VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT: return VkDynamicColorWriteMaskEXT;
    case VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT: return VkDynamicRasterizationStreamEXT;
    case VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT:
      return VkDynamicConservativeRastModeEXT;
    case VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT:
      return VkDynamicOverstimationSizeEXT;
    case VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT: return VkDynamicDepthClipEnableEXT;
    case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT: return VkDynamicSampleLocationsEnableEXT;
    case VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT: return VkDynamicStateColorBlendAdvancedEXT;
    case VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT: return VkDynamicProvokingVertexModeEXT;
    case VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT: return VkDynamicLineRastModeEXT;
    case VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT: return VkDynamicLineStippleEnableEXT;
    case VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT:
      return VkDynamicDepthClipNegativeOneEXT;
    case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV: return VkDynamicViewportWScalingEXT;
    case VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV: return VkDynamicViewportSwizzleEXT;
    case VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV: return VkDynamicCoverageToColorEnableEXT;
    case VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV: return VkDynamicCoverageToColorLocationEXT;
    case VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV: return VkDynamicCoverageModulationModeEXT;
    case VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV:
      return VkDynamicCoverageModulationTableEnableEXT;
    case VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV: return VkDynamicCoverageModulationTableEXT;
    case VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV: return VkDynamicShadingRateImageEnableEXT;
    case VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV:
      return VkDynamicRepresentativeFragTestEXT;
    case VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV: return VkDynamicCoverageReductionModeEXT;
    case VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT:
      return VkDynamicAttachmentFeedbackLoopEnableEXT;
    case VK_DYNAMIC_STATE_MAX_ENUM: break;
  }

  RDCERR("Unexpected vulkan state %u", state);

  return VkDynamicCount;
}

static VkGraphicsPipelineLibraryFlagsEXT DynamicStateValidState(VkDynamicState state)
{
  const VkGraphicsPipelineLibraryFlagsEXT vinput =
      VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
  const VkGraphicsPipelineLibraryFlagsEXT vert =
      VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
  const VkGraphicsPipelineLibraryFlagsEXT frag = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
  const VkGraphicsPipelineLibraryFlagsEXT colout =
      VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

  switch(state)
  {
    case VK_DYNAMIC_STATE_VIEWPORT: return vert;
    case VK_DYNAMIC_STATE_SCISSOR: return vert;
    case VK_DYNAMIC_STATE_LINE_WIDTH: return frag;
    case VK_DYNAMIC_STATE_DEPTH_BIAS: return frag;
    case VK_DYNAMIC_STATE_BLEND_CONSTANTS: return colout;
    case VK_DYNAMIC_STATE_DEPTH_BOUNDS: return frag;
    case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK: return frag;
    case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK: return frag;
    case VK_DYNAMIC_STATE_STENCIL_REFERENCE: return frag;
    case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV: return vert;
    case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT: return vert;
    case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT: return vert;
    case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT: return vert;
    case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT: return frag | colout;
    case VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR:
      return (VkGraphicsPipelineLibraryFlagsEXT)0;
    case VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV: return vert;
    case VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV: return vert;
    case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV: return vert;
    case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV: return vert;
    case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR: return vert | frag;
    case VK_DYNAMIC_STATE_LINE_STIPPLE_KHR: return vert;
    case VK_DYNAMIC_STATE_CULL_MODE: return vert;
    case VK_DYNAMIC_STATE_FRONT_FACE: return vert;
    case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY: return vinput;
    case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT: return vert;
    case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT: return vert;
    case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE: return vinput;
    case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE: return frag;
    case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE: return frag;
    case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP: return frag;
    case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE: return frag;
    case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE: return frag;
    case VK_DYNAMIC_STATE_STENCIL_OP: return frag;
    case VK_DYNAMIC_STATE_VERTEX_INPUT_EXT: return vinput;
    case VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT: return vert;
    case VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE: return vert;
    case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE: return frag;
    case VK_DYNAMIC_STATE_LOGIC_OP_EXT: return colout;
    case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE: return vinput;
    case VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT: return colout;
    case VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT: return vert;
    case VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT: return vert;
    case VK_DYNAMIC_STATE_POLYGON_MODE_EXT: return vert;
    case VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT: return frag | colout;
    case VK_DYNAMIC_STATE_SAMPLE_MASK_EXT: return frag | colout;
    case VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT: return frag | colout;
    case VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT: return frag | colout;
    case VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT: return colout;
    case VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT: return colout;
    case VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT: return colout;
    case VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT: return colout;
    case VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT: return vert;
    case VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT: return vert;
    case VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT: return vert;
    case VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT: return vert;
    case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT: return frag | colout;
    case VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT: return colout;
    case VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT: return vert;
    case VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT: return vert;
    case VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT: return vert;
    case VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT: return vert;
    case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV: return vert;
    case VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV: return vert;
    case VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV: return frag | colout;
    case VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV: return frag | colout;
    case VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV: return frag | colout;
    case VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV: return frag | colout;
    case VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV: return frag | colout;
    case VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV: return vert;
    case VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV: return frag;
    case VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV: return frag | colout;
    case VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT: return colout;
    case VK_DYNAMIC_STATE_MAX_ENUM: break;
  }

  RDCERR("Unexpected vulkan state %u", state);

  return VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
         VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
         VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
         VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
}

void DescSetLayout::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                         const VkDescriptorSetLayoutCreateInfo *pCreateInfo)
{
  dynamicCount = 0;
  inlineCount = 0;
  inlineByteSize = 0;
  accelerationStructureCount = 0;
  accelerationStructureWriteCount = 0;

  const VkMutableDescriptorTypeCreateInfoEXT *mutableInfo =
      (const VkMutableDescriptorTypeCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT);

  flags = pCreateInfo->flags;

  anyStageFlags = 0;

  VkDescriptorSetLayoutBindingFlagsCreateInfo *bindingFlags =
      (VkDescriptorSetLayoutBindingFlagsCreateInfo *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO);

  // ignore degenerate struct
  if(bindingFlags && bindingFlags->bindingCount == 0)
    bindingFlags = NULL;

  // descriptor set layouts can be sparse, such that only three bindings exist
  // but they are at 0, 5 and 10.
  // We assume here that while the layouts may be sparse that's mostly to allow
  // multiple layouts to co-exist nicely, and that we can allocate our bindings
  // array to cover the whole size, and leave some elements unused.

  // will be at least this size.
  bindings.resize(pCreateInfo->bindingCount);
  for(uint32_t i = 0; i < pCreateInfo->bindingCount; i++)
  {
    uint32_t b = pCreateInfo->pBindings[i].binding;
    // expand to fit the binding
    bindings.resize_for_index(b);

    if(mutableInfo && i < mutableInfo->mutableDescriptorTypeListCount)
    {
      const VkMutableDescriptorTypeListEXT &mutableTypes =
          mutableInfo->pMutableDescriptorTypeLists[i];

      RDCCOMPILE_ASSERT(uint64_t(DescriptorSlotType::Count) < 64,
                        "Descriptor types don't fit in 64-bit bitmask anymore");

      // encode a bitmask with the available types for this descriptor
      uint64_t mask = 0;
      for(uint32_t m = 0; m < mutableTypes.descriptorTypeCount; m++)
        mask |= (1ULL << uint64_t(convert(mutableTypes.pDescriptorTypes[m])));

      mutableBitmasks.resize_for_index(b);
      mutableBitmasks[b] = mask;
    }

    VkDescriptorType type = pCreateInfo->pBindings[i].descriptorType;

    bindings[b].descriptorCount = pCreateInfo->pBindings[i].descriptorCount;
    bindings[b].layoutDescType = type;
    bindings[b].stageFlags = pCreateInfo->pBindings[i].stageFlags;

    anyStageFlags |= bindings[b].stageFlags;

    if(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
       type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
      dynamicCount += bindings[b].descriptorCount;

    if(type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    {
      inlineCount++;
      inlineByteSize = AlignUp4(inlineByteSize + bindings[b].descriptorCount);
    }
    else if(type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
    {
      accelerationStructureWriteCount++;
      accelerationStructureCount += bindings[b].descriptorCount;
    }

    if((type == VK_DESCRIPTOR_TYPE_SAMPLER || type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
       pCreateInfo->pBindings[i].pImmutableSamplers)
    {
      bindings[b].immutableSampler = new ResourceId[bindings[b].descriptorCount];

      for(uint32_t s = 0; s < bindings[b].descriptorCount; s++)
        bindings[b].immutableSampler[s] = GetResID(pCreateInfo->pBindings[i].pImmutableSamplers[s]);
    }

    if(bindingFlags &&
       (bindingFlags->pBindingFlags[i] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT))
      bindings[b].variableSize = 1;
    else
      bindings[b].variableSize = 0;
  }

  // assign offsets in sorted bindings order, as the bindings we were provided by the application
  // don't have to appear in bindings order
  uint32_t elemOffset = 0;

  for(size_t b = 0; b < bindings.size(); b++)
  {
    bindings[b].elemOffset = elemOffset;

    // don't count the descriptors in the variable size array. We'll add on the allocated size after
    // this
    if(bindings[b].variableSize)
      break;

    if(bindings[b].layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    {
      elemOffset++;
    }
    else
    {
      elemOffset += bindings[b].descriptorCount;
    }
  }

  totalElems = elemOffset;
}

void DescSetLayout::CreateBindingsArray(BindingStorage &bindingStorage, uint32_t variableAllocSize) const
{
  bindingStorage.variableDescriptorCount = variableAllocSize;

  if(!bindings.empty())
  {
    bindingStorage.elems.resize(totalElems + variableAllocSize);
    bindingStorage.binds.resize(bindings.size());

    if(inlineByteSize == 0)
    {
      for(size_t i = 0; i < bindings.size(); i++)
      {
        bindingStorage.binds[i] = bindingStorage.elems.data() + bindings[i].elemOffset;

        if(bindings[i].immutableSampler)
        {
          for(uint32_t a = 0; a < bindings[i].descriptorCount; a++)
          {
            // set immutable samplers here so it's always present in the descriptor and we don't
            // have to do a per-descriptor lookup of immutable samplers later
            bindingStorage.binds[i][a].sampler = bindings[i].immutableSampler[a];

            // immutable samplers cannot be used with mutable descriptors, so if we have immutable
            // samplers set the type from the layout. That way even if the descriptor is never
            // written we still process immutable samplers properly.
            bindingStorage.binds[i][a].type = convert(bindings[i].layoutDescType);

            bindingStorage.binds[i][a].offset = 1;
          }
        }

        // set the type for dynamic descriptors so we always know which descriptors consume dynamic
        // offsets, even if they are unwritten.
        if(bindings[i].layoutDescType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
           bindings[i].layoutDescType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
        {
          for(uint32_t a = 0; a < bindings[i].descriptorCount; a++)
            bindingStorage.binds[i][a].type = convert(bindings[i].layoutDescType);
        }
      }

      bindingStorage.inlineBytes.clear();
    }
    else
    {
      uint32_t inlineOffset = 0;
      for(size_t i = 0; i < bindings.size(); i++)
      {
        bindingStorage.binds[i] = bindingStorage.elems.data() + bindings[i].elemOffset;

        if(bindings[i].layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          bindingStorage.binds[i]->type = DescriptorSlotType::InlineBlock;
          bindingStorage.binds[i]->offset = inlineOffset;
          bindingStorage.binds[i]->range = bindings[i].descriptorCount;
          inlineOffset = AlignUp4(inlineOffset + bindings[i].descriptorCount);

          // update range with variable allocation here
          if(bindings[i].variableSize)
            bindingStorage.binds[i]->range = variableAllocSize;
        }
      }

      bindingStorage.inlineBytes.resize(inlineByteSize);
    }
  }
}

void DescSetLayout::UpdateBindingsArray(const DescSetLayout &prevLayout,
                                        BindingStorage &bindingStorage) const
{
  if(bindings.empty())
  {
    bindingStorage.clear();
  }
  else
  {
    rdcarray<DescriptorSetSlot> newElems;
    newElems.resize(totalElems);

    // resize to the new size, discarding any excess we don't need anymore
    bindingStorage.binds.resize(bindings.size());

    uint32_t inlineOffset = 0;
    for(size_t i = 0; i < bindings.size(); i++)
    {
      DescriptorSetSlot *newSlots = newElems.data() + bindings[i].elemOffset;

      if(bindings[i].layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        bindingStorage.binds[i]->type = DescriptorSlotType::InlineBlock;
        bindingStorage.binds[i]->offset = inlineOffset;
        bindingStorage.binds[i]->range = bindings[i].descriptorCount;
        inlineOffset = AlignUp4(inlineOffset + bindings[i].descriptorCount);
      }
      else
      {
        // copy over any previous bindings that overlapped
        if(i < prevLayout.bindings.size())
          memcpy(newSlots, bindingStorage.binds[i],
                 sizeof(DescriptorSetSlot) *
                     RDCMIN(prevLayout.bindings[i].descriptorCount, bindings[i].descriptorCount));
      }

      bindingStorage.binds[i] = newSlots;
    }

    bindingStorage.inlineBytes.resize(inlineByteSize);
    bindingStorage.elems.swap(newElems);
  }
}

bool DescSetLayout::isCompatible(const DescSetLayout &other) const
{
  // shortcut for equality to ourselves
  if(this == &other)
    return true;

  // descriptor set layouts are different if they have different set of bindings.
  if(bindings.size() != other.bindings.size())
    return false;

  // iterate over each binding (we know this loop indexes validly in both arrays
  for(size_t i = 0; i < bindings.size(); i++)
  {
    const Binding &a = bindings[i];
    const Binding &b = other.bindings[i];

    // if the type/stages/count are different, the layout is different
    if(a.descriptorCount != b.descriptorCount || a.layoutDescType != b.layoutDescType ||
       a.stageFlags != b.stageFlags)
      return false;

    // if one has immutable samplers but the other doesn't, they're different
    if((a.immutableSampler && !b.immutableSampler) || (!a.immutableSampler && b.immutableSampler))
      return false;

    // if we DO have immutable samplers, they must all point to the same sampler objects.
    if(a.immutableSampler)
    {
      for(uint32_t s = 0; s < a.descriptorCount; s++)
      {
        if(a.immutableSampler[s] != b.immutableSampler[s])
          return false;
      }
    }
  }

  return true;
}

bool IsValid(bool allowNULLDescriptors, const VkWriteDescriptorSet &write, uint32_t arrayElement)
{
  if(write.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    return true;

  // this makes assumptions that only hold within the context of Serialise_InitialState below,
  // specifically that if pTexelBufferView/pBufferInfo is set then we are using them. In the general
  // case they can be garbage and we must ignore them based on the descriptorType

  if(write.pTexelBufferView)
    return allowNULLDescriptors ? true : write.pTexelBufferView[arrayElement] != VK_NULL_HANDLE;

  if(write.pBufferInfo)
    return allowNULLDescriptors ? true : write.pBufferInfo[arrayElement].buffer != VK_NULL_HANDLE;

  if(write.pImageInfo)
  {
    // only these two types need samplers
    bool needSampler = (write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                        write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // but all types that aren't just a sampler need an image
    bool needImage = (write.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER);

    if(allowNULLDescriptors)
      needImage = false;

    if(needSampler && write.pImageInfo[arrayElement].sampler == VK_NULL_HANDLE)
      return false;

    if(needImage && write.pImageInfo[arrayElement].imageView == VK_NULL_HANDLE)
      return false;

    return true;
  }

  if(write.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
  {
    if(allowNULLDescriptors)
      return true;

    const VkWriteDescriptorSetAccelerationStructureKHR *asDesc =
        (const VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
            &write, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
    return asDesc != VK_NULL_HANDLE;
  }

  RDCERR("Encountered VkWriteDescriptorSet with no data!");

  return false;
}

bool CreateDescriptorWritesForSlotData(WrappedVulkan *vk, rdcarray<VkWriteDescriptorSet> &writes,
                                       VkDescriptorBufferInfo *&writeScratch,
                                       const DescriptorSetSlot *slots, uint32_t descriptorCount,
                                       VkDescriptorSet set, uint32_t dstBind,
                                       const DescSetLayout::Binding &layoutBind)
{
  bool ret = true;

  RDCCOMPILE_ASSERT(sizeof(VkDescriptorImageInfo) <= sizeof(VkDescriptorBufferInfo),
                    "VkDescriptorBufferInfo should be large enough for all descriptor write types");
  RDCCOMPILE_ASSERT(sizeof(VkBufferView) <= sizeof(VkDescriptorBufferInfo),
                    "VkDescriptorBufferInfo should be large enough for all descriptor write types");

  ResourceId *immutableSamplers = layoutBind.immutableSampler;

  VkWriteDescriptorSet templateWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  templateWrite.dstSet = set;
  templateWrite.dstBinding = dstBind;

  writes.push_back(templateWrite);

  VulkanResourceManager *rm = vk->GetResourceManager();

  // loop over every slot in this descriptor array
  for(uint32_t slot = 0; slot < descriptorCount; slot++)
  {
    VkDescriptorType descType = convert(slots[slot].type);

    // if the previous write had some contents, and is a different type to this one,
    // finish
    // it off and start a new one. In theory if this descriptor were invalid and NULL
    // descriptors are supported we could write NULL and continue to combine, but we don't
    // do that.
    // note that the previous write is from THIS descriptor as we don't batch across
    // descriptors - even though there is 'rollover' behaviour allowing this.
    if(writes.back().descriptorCount > 0 && writes.back().descriptorType != descType)
    {
      // consume the write arrays used
      writeScratch += writes.back().descriptorCount;

      writes.push_back(templateWrite);
      writes.back().dstArrayElement = slot;
    }

    // skip unwritten descriptors
    if(descType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
      continue;

    // skip immutable sampler-only descriptors
    if(immutableSamplers && descType == VK_DESCRIPTOR_TYPE_SAMPLER)
      continue;

    // the current write is either empty, in which case we can just set it to what we
    // want,
    // or it's the same type in which case we're appending to its array

    VkDescriptorBufferInfo *writeBuffer = writeScratch;
    VkDescriptorImageInfo *writeImage = (VkDescriptorImageInfo *)writeScratch;
    VkBufferView *writeTexelBuffer = (VkBufferView *)writeScratch;

    // set the type and base of this write if the previous one is as-yet unused.
    if(writes.back().descriptorCount == 0)
    {
      writes.back().descriptorType = descType;
      writes.back().dstArrayElement = slot;
    }

    // array index in the above writeArrays, relative to the start of the current
    // contiguous
    // range starting at dstArrayElement
    uint32_t arrayIdx = slot - writes.back().dstArrayElement;

    ResourceId resId = slots[slot].resource;
    if(rm->HasLiveResource(resId))
      resId = rm->GetLiveID(resId);
    ResourceId sampId = slots[slot].sampler;
    if(rm->HasLiveResource(sampId))
      sampId = rm->GetLiveID(sampId);

    switch(descType)
    {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      {
        if(descType != VK_DESCRIPTOR_TYPE_SAMPLER && rm->HasCurrentResource(resId))
          writeImage[arrayIdx].imageView = rm->GetCurrentHandle<VkImageView>(resId);
        else
          writeImage[arrayIdx].imageView = VK_NULL_HANDLE;

        if((descType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            descType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
           rm->HasCurrentResource(sampId))
          writeImage[arrayIdx].sampler = rm->GetCurrentHandle<VkSampler>(sampId);
        else
          writeImage[arrayIdx].sampler = VK_NULL_HANDLE;

        writeImage[arrayIdx].imageLayout = convert(slots[slot].imageLayout);

        // if we're not updating a SAMPLER descriptor fill in immutable samplers so that
        // our
        // validity checking doesn't have to look them up.
        if(immutableSamplers && descType != VK_DESCRIPTOR_TYPE_SAMPLER)
        {
          writeImage[arrayIdx].sampler = rm->GetCurrentHandle<VkSampler>(immutableSamplers[slot]);
        }

        // set the write array (possibly redundant if we're collating as writeImage only
        // moves when we commit some writes)
        writes.back().pImageInfo = writeImage;
        break;
      }
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      {
        if(rm->HasCurrentResource(resId))
          writeTexelBuffer[arrayIdx] = rm->GetCurrentHandle<VkBufferView>(resId);
        else
          writeTexelBuffer[arrayIdx] = VK_NULL_HANDLE;

        writes.back().pTexelBufferView = writeTexelBuffer;
        break;
      }
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      {
        if(rm->HasCurrentResource(resId))
          writeBuffer[arrayIdx].buffer = rm->GetCurrentHandle<VkBuffer>(resId);
        else
          writeBuffer[arrayIdx].buffer = VK_NULL_HANDLE;
        writeBuffer[arrayIdx].offset = slots[slot].offset;
        writeBuffer[arrayIdx].range = slots[slot].GetRange();

        writes.back().pBufferInfo = writeBuffer;
        break;
      }
      default:
      {
        RDCERR("Unexpected descriptor type %d", descType);
        ret = false;
      }
    }

    // if we made a valid write, count it
    if(IsValid(vk->NULLDescriptorsAllowed(), writes.back(), arrayIdx))
    {
      writes.back().descriptorCount++;
    }
    else
    {
      // otherwise this is an invalid write. If we previously had valid writes we need to
      // end them to ensure we don't skip
      if(writes.back().descriptorCount > 0)
      {
        // consume the write arrays used
        writeScratch += writes.back().descriptorCount;

        writes.push_back(templateWrite);
        writes.back().dstArrayElement = slot;
      }
    }
  }

  // take any final descriptor write that had descriptors and commit them. E.g. if all
  // writes succeeded and were the same type then the above loop won't have committed them
  // yet, since it only commits when it needs to break a range of writes (due to one being
  // unwritten, or due to types differing)
  if(writes.back().descriptorCount > 0)
  {
    // consume the write arrays used
    writeScratch += writes.back().descriptorCount;
  }
  else if(writes.back().descriptorCount == 0)
  {
    // similarly remove the last write if it was unused
    writes.pop_back();
  }

  return ret;
}

void VulkanCreationInfo::ShaderEntry::ProcessStaticDescriptorAccess(
    ResourceId pushStorage, ResourceId specStorage, rdcarray<DescriptorAccess> &descriptorAccess,
    rdcarray<const DescSetLayout *> setLayoutInfos) const
{
  if(!refl)
    return;

  const uint32_t descSetLayoutsCount = (uint32_t)setLayoutInfos.size();

  DescriptorAccess access;
  access.stage = refl->stage;

  // we will store the descriptor set in byteSize to be decoded into descriptorStore later
  access.byteSize = 0;

  descriptorAccess.reserve(descriptorAccess.size() + refl->constantBlocks.size() +
                           refl->samplers.size() + refl->readOnlyResources.size() +
                           refl->readWriteResources.size());

  RDCASSERT(refl->constantBlocks.size() < 0xffff, refl->constantBlocks.size());
  for(uint16_t i = 0; i < refl->constantBlocks.size(); i++)
  {
    const ConstantBlock &bind = refl->constantBlocks[i];
    // arrayed descriptors will be handled with bindless feedback
    if(bind.bindArraySize > 1)
      continue;

    access.type = DescriptorType::ConstantBuffer;
    access.index = i;

    if(!bind.bufferBacked)
    {
      if(bind.compileConstants)
      {
        // spec constants
        access.descriptorStore = specStorage;
        access.byteSize = 1;
        access.byteOffset = 0;
        descriptorAccess.push_back(access);
      }
      else
      {
        // push constants
        access.descriptorStore = pushStorage;
        access.byteSize = 1;
        access.byteOffset = 0;
        descriptorAccess.push_back(access);
      }
    }
    else
    {
      // Ignore bindings which are not in the descriptor set layouts
      if((bind.fixedBindSetOrSpace >= descSetLayoutsCount) ||
         bind.fixedBindNumber >= setLayoutInfos[bind.fixedBindSetOrSpace]->bindings.size())
        continue;

      access.descriptorStore = ResourceId();

      // VkShaderStageFlagBits and ShaderStageMask are identical bit-for-bit.
      // this might be deliberate if the binding is never actually used dynamically, only
      // statically used bindings must be declared
      if((setLayoutInfos[bind.fixedBindSetOrSpace]->bindings[bind.fixedBindNumber].stageFlags &
          (VkShaderStageFlags)MaskForStage(refl->stage)) == 0)
        continue;

      access.byteSize = bind.fixedBindSetOrSpace;
      access.byteOffset =
          setLayoutInfos[bind.fixedBindSetOrSpace]->bindings[bind.fixedBindNumber].elemOffset +
          setLayoutInfos[bind.fixedBindSetOrSpace]->inlineByteSize;
      descriptorAccess.push_back(access);
    }
  }

  access.descriptorStore = ResourceId();

  RDCASSERT(refl->samplers.size() < 0xffff, refl->samplers.size());
  for(uint16_t i = 0; i < refl->samplers.size(); i++)
  {
    const ShaderSampler &bind = refl->samplers[i];
    // arrayed descriptors will be handled with bindless feedback
    if(bind.bindArraySize > 1)
      continue;

    // Ignore bindings which are not in the descriptor set layouts
    if((bind.fixedBindSetOrSpace >= descSetLayoutsCount) ||
       bind.fixedBindNumber >= setLayoutInfos[bind.fixedBindSetOrSpace]->bindings.size())
      continue;

    // VkShaderStageFlagBits and ShaderStageMask are identical bit-for-bit.
    // this might be deliberate if the binding is never actually used dynamically, only
    // statically used bindings must be declared
    if((setLayoutInfos[bind.fixedBindSetOrSpace]->bindings[bind.fixedBindNumber].stageFlags &
        (VkShaderStageFlags)MaskForStage(refl->stage)) == 0)
      continue;

    access.type = DescriptorType::Sampler;
    access.index = i;
    access.byteSize = bind.fixedBindSetOrSpace;
    access.byteOffset =
        setLayoutInfos[bind.fixedBindSetOrSpace]->bindings[bind.fixedBindNumber].elemOffset;
    descriptorAccess.push_back(access);
  }

  RDCASSERT(refl->readOnlyResources.size() < 0xffff, refl->readOnlyResources.size());
  for(uint16_t i = 0; i < refl->readOnlyResources.size(); i++)
  {
    const ShaderResource &bind = refl->readOnlyResources[i];
    // arrayed descriptors will be handled with bindless feedback
    if(bind.bindArraySize > 1)
      continue;

    // Ignore bindings which are not in the descriptor set layouts
    if((bind.fixedBindSetOrSpace >= descSetLayoutsCount) ||
       bind.fixedBindNumber >= setLayoutInfos[bind.fixedBindSetOrSpace]->bindings.size())
      continue;

    // VkShaderStageFlagBits and ShaderStageMask are identical bit-for-bit.
    // this might be deliberate if the binding is never actually used dynamically, only
    // statically used bindings must be declared
    if((setLayoutInfos[bind.fixedBindSetOrSpace]->bindings[bind.fixedBindNumber].stageFlags &
        (VkShaderStageFlags)MaskForStage(refl->stage)) == 0)
      continue;

    access.type = refl->readOnlyResources[i].descriptorType;
    access.index = i;
    access.byteSize = bind.fixedBindSetOrSpace;
    access.byteOffset =
        setLayoutInfos[bind.fixedBindSetOrSpace]->bindings[bind.fixedBindNumber].elemOffset;
    descriptorAccess.push_back(access);
  }

  RDCASSERT(refl->readWriteResources.size() < 0xffff, refl->readWriteResources.size());
  for(uint16_t i = 0; i < refl->readWriteResources.size(); i++)
  {
    const ShaderResource &bind = refl->readWriteResources[i];
    // arrayed descriptors will be handled with bindless feedback
    if(bind.bindArraySize > 1)
      continue;

    // Ignore bindings which are not in the descriptor set layouts
    if((bind.fixedBindSetOrSpace >= descSetLayoutsCount) ||
       bind.fixedBindNumber >= setLayoutInfos[bind.fixedBindSetOrSpace]->bindings.size())
      continue;

    // VkShaderStageFlagBits and ShaderStageMask are identical bit-for-bit.
    // this might be deliberate if the binding is never actually used dynamically, only
    // statically used bindings must be declared
    if((setLayoutInfos[bind.fixedBindSetOrSpace]->bindings[bind.fixedBindNumber].stageFlags &
        (VkShaderStageFlags)MaskForStage(refl->stage)) == 0)
      continue;

    access.type = refl->readWriteResources[i].descriptorType;
    access.index = i;
    access.byteSize = bind.fixedBindSetOrSpace;
    access.byteOffset =
        setLayoutInfos[bind.fixedBindSetOrSpace]->bindings[bind.fixedBindNumber].elemOffset;
    descriptorAccess.push_back(access);
  }
}

void VulkanCreationInfo::ShaderObject::Init(VulkanResourceManager *resourceMan,
                                            VulkanCreationInfo &info, ResourceId id,
                                            const VkShaderCreateInfoEXT *pCreateInfo)
{
  flags = pCreateInfo->flags;

  shad.module = id;

  shad.stage = StageFromIndex(StageIndex(pCreateInfo->stage));

  nextStage = pCreateInfo->nextStage;

  codeType = pCreateInfo->codeType;

  // fake ShaderModule for SPIR-V processing and reflection
  VkShaderModuleCreateInfo smInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0};

  if(codeType & VK_SHADER_CODE_TYPE_SPIRV_EXT)
  {
    smInfo.codeSize = pCreateInfo->codeSize;
    smInfo.pCode = (const uint32_t *)pCreateInfo->pCode;
  }

  info.m_ShaderModule[id].Init(resourceMan, info, &smInfo);

  shad.entryPoint = pCreateInfo->pName;

  // descriptor set layouts
  if(pCreateInfo->pSetLayouts)
  {
    descSetLayouts.resize(pCreateInfo->setLayoutCount);
    for(uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++)
      descSetLayouts[i] = GetResID(pCreateInfo->pSetLayouts[i]);
  }

  // push constants
  if(pCreateInfo->pPushConstantRanges)
  {
    pushRanges.reserve(pCreateInfo->pushConstantRangeCount);
    for(uint32_t i = 0; i < pCreateInfo->pushConstantRangeCount; i++)
      pushRanges.push_back(pCreateInfo->pPushConstantRanges[i]);
  }

  ShaderModuleReflectionKey key(shad.stage, shad.entryPoint, ResourceId());

  // specialization info
  if(pCreateInfo->pSpecializationInfo)
  {
    key = ShaderModuleReflectionKey(shad.stage, shad.entryPoint, id);

    const byte *data = (const byte *)pCreateInfo->pSpecializationInfo->pData;

    const VkSpecializationMapEntry *maps = pCreateInfo->pSpecializationInfo->pMapEntries;
    for(uint32_t s = 0; s < pCreateInfo->pSpecializationInfo->mapEntryCount; s++)
    {
      SpecConstant spec;
      spec.specID = maps[s].constantID;
      memcpy(&spec.value, data + maps[s].offset, maps[s].size);
      spec.dataSize = maps[s].size;
      shad.specialization.push_back(spec);

      virtualSpecialisationByteSize =
          RDCMAX(virtualSpecialisationByteSize, uint32_t((spec.specID + 1) * sizeof(uint64_t)));
    }
  }

  ShaderModuleReflection &reflData = info.m_ShaderModule[id].m_Reflections[key];

  reflData.Init(resourceMan, id, info.m_ShaderModule[id].spirv, shad.entryPoint, pCreateInfo->stage,
                shad.specialization);

  shad.refl = reflData.refl;
  shad.patchData = &reflData.patchData;

  rdcarray<const DescSetLayout *> setLayoutInfos;
  for(ResourceId setLayout : descSetLayouts)
    setLayoutInfos.push_back(&info.m_DescSetLayout[setLayout]);

  shad.ProcessStaticDescriptorAccess(info.pushConstantDescriptorStorage,
                                     resourceMan->GetOriginalID(id), staticDescriptorAccess,
                                     setLayoutInfos);
}

void VulkanCreationInfo::Pipeline::Init(VulkanResourceManager *resourceMan,
                                        VulkanCreationInfo &info, ResourceId id,
                                        const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
  flags = pCreateInfo->flags;

  graphicsPipe = true;

  // this is used to e.g. filter specified dynamic states so we only consider the ones valid for
  // this pipeline. If we're not using libraries, all states are valid
  availStages = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
                VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
                VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
                VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

  const VkGraphicsPipelineLibraryCreateInfoEXT *graphicsLibraryCreate =
      (const VkGraphicsPipelineLibraryCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT);
  if(graphicsLibraryCreate)
    availStages = graphicsLibraryCreate->flags;

  vertLayout = fragLayout = GetResID(pCreateInfo->layout);
  renderpass = GetResID(pCreateInfo->renderPass);
  subpass = pCreateInfo->subpass;

  const VkPipelineRenderingCreateInfo *dynRenderCreate =
      (const VkPipelineRenderingCreateInfo *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
  if(dynRenderCreate)
  {
    viewMask = dynRenderCreate->viewMask;
    colorFormats.assign(dynRenderCreate->pColorAttachmentFormats,
                        dynRenderCreate->colorAttachmentCount);
    depthFormat = dynRenderCreate->depthAttachmentFormat;
    stencilFormat = dynRenderCreate->stencilAttachmentFormat;
  }
  else
  {
    viewMask = 0;
    colorFormats.clear();
    depthFormat = VK_FORMAT_UNDEFINED;
    stencilFormat = VK_FORMAT_UNDEFINED;
  }

  RDCEraseEl(dynamicStates);
  if(pCreateInfo->pDynamicState)
  {
    for(uint32_t i = 0; i < pCreateInfo->pDynamicState->dynamicStateCount; i++)
    {
      VkDynamicState d = pCreateInfo->pDynamicState->pDynamicStates[i];

      // ignore dynamic states not available for this library (e.g.
      // VK_DYNAMIC_STATE_VERTEX_INPUT_EXT in a library with only FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
      if((DynamicStateValidState(d) & availStages) == 0)
        continue;

      dynamicStates[ConvertDynamicState(d)] = true;
    }

    // if the viewports and counts are dynamic this supersets the viewport only being dynamic. For
    // ease of code elsewhere, turn off the older one if both are specified so that we don't call
    // vkCmdSetViewports when the count is also dynamic.
    if(dynamicStates[VkDynamicViewportCount])
      dynamicStates[VkDynamicViewport] = false;
    if(dynamicStates[VkDynamicScissorCount])
      dynamicStates[VkDynamicScissor] = false;
  }

  // VkPipelineShaderStageCreateInfo
  for(uint32_t i = 0; i < pCreateInfo->stageCount; i++)
  {
    ResourceId shadid = GetResID(pCreateInfo->pStages[i].module);

    // convert shader bit to shader index
    int stageIndex = StageIndex(pCreateInfo->pStages[i].stage);

    ShaderEntry &shad = shaders[stageIndex];

    VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *subgroupSize =
        (VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *)FindNextStruct(
            &pCreateInfo->pStages[i],
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO);
    if(subgroupSize)
      shad.requiredSubgroupSize = subgroupSize->requiredSubgroupSize;

    shad.module = shadid;
    shad.entryPoint = pCreateInfo->pStages[i].pName;
    shad.stage = ShaderStage(stageIndex);

    ShaderModuleReflectionKey key(shad.stage, shad.entryPoint, ResourceId());

    if(pCreateInfo->pStages[i].pSpecializationInfo)
    {
      key = ShaderModuleReflectionKey(shad.stage, shad.entryPoint, id);

      const byte *data = (const byte *)pCreateInfo->pStages[i].pSpecializationInfo->pData;

      const VkSpecializationMapEntry *maps = pCreateInfo->pStages[i].pSpecializationInfo->pMapEntries;
      for(uint32_t s = 0; s < pCreateInfo->pStages[i].pSpecializationInfo->mapEntryCount; s++)
      {
        SpecConstant spec;
        spec.specID = maps[s].constantID;
        memcpy(&spec.value, data + maps[s].offset, maps[s].size);
        spec.dataSize = maps[s].size;
        shad.specialization.push_back(spec);

        virtualSpecialisationByteSize =
            RDCMAX(virtualSpecialisationByteSize, uint32_t((spec.specID + 1) * sizeof(uint64_t)));
      }
    }

    ShaderModuleReflection &reflData = info.m_ShaderModule[shadid].m_Reflections[key];

    reflData.Init(resourceMan, shadid, info.m_ShaderModule[shadid].spirv, shad.entryPoint,
                  pCreateInfo->pStages[i].stage, shad.specialization);

    shad.refl = reflData.refl;
    shad.patchData = &reflData.patchData;
  }

  if(pCreateInfo->pVertexInputState)
  {
    vertexBindings.resize(pCreateInfo->pVertexInputState->vertexBindingDescriptionCount);
    for(uint32_t i = 0; i < pCreateInfo->pVertexInputState->vertexBindingDescriptionCount; i++)
    {
      vertexBindings[i].vbufferBinding =
          pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].binding;
      vertexBindings[i].bytestride =
          pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].stride;
      vertexBindings[i].perInstance =
          pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].inputRate ==
          VK_VERTEX_INPUT_RATE_INSTANCE;
      vertexBindings[i].instanceDivisor = 1;
    }

    // if there's a divisors struct, apply them now
    const VkPipelineVertexInputDivisorStateCreateInfoKHR *divisors =
        (const VkPipelineVertexInputDivisorStateCreateInfoKHR *)FindNextStruct(
            pCreateInfo->pVertexInputState,
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_KHR);
    if(divisors)
    {
      for(uint32_t b = 0; b < divisors->vertexBindingDivisorCount; b++)
      {
        const VkVertexInputBindingDivisorDescriptionKHR &div = divisors->pVertexBindingDivisors[b];

        if(div.binding < vertexBindings.size())
          vertexBindings[div.binding].instanceDivisor = div.divisor;
      }
    }

    vertexAttrs.resize(pCreateInfo->pVertexInputState->vertexAttributeDescriptionCount);
    for(uint32_t i = 0; i < pCreateInfo->pVertexInputState->vertexAttributeDescriptionCount; i++)
    {
      vertexAttrs[i].binding =
          pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].binding;
      vertexAttrs[i].location =
          pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].location;
      vertexAttrs[i].format = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].format;
      vertexAttrs[i].byteoffset =
          pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].offset;
    }
  }

  if(pCreateInfo->pInputAssemblyState)
  {
    topology = pCreateInfo->pInputAssemblyState->topology;
    primitiveRestartEnable = pCreateInfo->pInputAssemblyState->primitiveRestartEnable ? true : false;
  }
  else
  {
    topology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    primitiveRestartEnable = false;
  }

  if(pCreateInfo->pTessellationState)
    patchControlPoints = pCreateInfo->pTessellationState->patchControlPoints;
  else
    patchControlPoints = 0;

  tessellationDomainOrigin = VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;

  const VkPipelineTessellationDomainOriginStateCreateInfo *tessDomain =
      (const VkPipelineTessellationDomainOriginStateCreateInfo *)FindNextStruct(
          pCreateInfo->pTessellationState,
          VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO);
  if(tessDomain)
    tessellationDomainOrigin = tessDomain->domainOrigin;

  if(pCreateInfo->pViewportState)
    viewportCount = pCreateInfo->pViewportState->viewportCount;
  else
    viewportCount = 0;

  viewports.resize(viewportCount);
  scissors.resize(viewportCount);

  for(uint32_t i = 0; i < viewportCount; i++)
  {
    if(pCreateInfo->pViewportState->pViewports)
      viewports[i] = pCreateInfo->pViewportState->pViewports[i];

    if(pCreateInfo->pViewportState->pScissors)
      scissors[i] = pCreateInfo->pViewportState->pScissors[i];
  }

  // VkPipelineFragmentShadingRateStateCreateInfoKHR
  shadingRate = {1, 1};
  shadingRateCombiners[0] = shadingRateCombiners[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
  const VkPipelineFragmentShadingRateStateCreateInfoKHR *shadingRateInfo =
      (const VkPipelineFragmentShadingRateStateCreateInfoKHR *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR);
  if(shadingRateInfo)
  {
    shadingRate = shadingRateInfo->fragmentSize;
    shadingRateCombiners[0] = shadingRateInfo->combinerOps[0];
    shadingRateCombiners[1] = shadingRateInfo->combinerOps[1];
  }

  // VkPipelineDiscardRectangleStateCreateInfoEXT
  discardMode = VK_DISCARD_RECTANGLE_MODE_EXCLUSIVE_EXT;

  const VkPipelineDiscardRectangleStateCreateInfoEXT *discardRects =
      (const VkPipelineDiscardRectangleStateCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT);
  if(discardRects)
  {
    discardRectangles.resize(discardRects->discardRectangleCount);

    if(discardRects->pDiscardRectangles)
    {
      for(uint32_t i = 0; i < discardRects->discardRectangleCount; i++)
        discardRectangles[i] = discardRects->pDiscardRectangles[i];
    }

    discardMode = discardRects->discardRectangleMode;
  }

  // VkPipelineRasterStateCreateInfo
  if(pCreateInfo->pRasterizationState)
  {
    depthClampEnable = pCreateInfo->pRasterizationState->depthClampEnable ? true : false;
    rasterizerDiscardEnable =
        pCreateInfo->pRasterizationState->rasterizerDiscardEnable ? true : false;
    polygonMode = pCreateInfo->pRasterizationState->polygonMode;
    cullMode = pCreateInfo->pRasterizationState->cullMode;
    frontFace = pCreateInfo->pRasterizationState->frontFace;
    depthBiasEnable = pCreateInfo->pRasterizationState->depthBiasEnable ? true : false;
    depthBiasConstantFactor = pCreateInfo->pRasterizationState->depthBiasConstantFactor;
    depthBiasClamp = pCreateInfo->pRasterizationState->depthBiasClamp;
    depthBiasSlopeFactor = pCreateInfo->pRasterizationState->depthBiasSlopeFactor;
    lineWidth = pCreateInfo->pRasterizationState->lineWidth;
  }
  else
  {
    depthClampEnable = false;
    rasterizerDiscardEnable = false;
    polygonMode = VK_POLYGON_MODE_FILL;
    cullMode = VK_CULL_MODE_NONE;
    frontFace = VK_FRONT_FACE_CLOCKWISE;
    depthBiasEnable = false;
    depthBiasConstantFactor = 0.0f;
    depthBiasClamp = 0.0f;
    depthBiasSlopeFactor = 0.0f;
    lineWidth = 1.0f;
  }

  // VkPipelineRasterizationStateStreamCreateInfoEXT
  rasterizationStream = 0;

  const VkPipelineRasterizationStateStreamCreateInfoEXT *rastStream =
      (const VkPipelineRasterizationStateStreamCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pRasterizationState,
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT);
  if(rastStream)
    rasterizationStream = rastStream->rasterizationStream;

  // VkPipelineRasterizationDepthClipStateCreateInfoEXT

  // default to the opposite of depthClampEnable
  depthClipEnable = !depthClampEnable;

  const VkPipelineRasterizationDepthClipStateCreateInfoEXT *depthClipState =
      (const VkPipelineRasterizationDepthClipStateCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pRasterizationState,
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT);
  if(depthClipState)
    depthClipEnable = depthClipState->depthClipEnable != VK_FALSE;

  // VkPipelineRasterizationConservativeStateCreateInfoEXT
  conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
  extraPrimitiveOverestimationSize = 0.0f;

  const VkPipelineRasterizationConservativeStateCreateInfoEXT *conservRast =
      (const VkPipelineRasterizationConservativeStateCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pRasterizationState,
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT);
  if(conservRast)
  {
    conservativeRasterizationMode = conservRast->conservativeRasterizationMode;
    extraPrimitiveOverestimationSize = conservRast->extraPrimitiveOverestimationSize;
  }

  // VkPipelineRasterizationLineStateCreateInfoKHR
  lineRasterMode = VK_LINE_RASTERIZATION_MODE_DEFAULT_KHR;
  stippleEnabled = false;
  stippleFactor = stipplePattern = 0;

  const VkPipelineRasterizationLineStateCreateInfoKHR *lineRasterState =
      (const VkPipelineRasterizationLineStateCreateInfoKHR *)FindNextStruct(
          pCreateInfo->pRasterizationState,
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_KHR);
  if(lineRasterState)
  {
    lineRasterMode = lineRasterState->lineRasterizationMode;
    stippleEnabled = lineRasterState->stippledLineEnable ? true : false;
    if(lineRasterState->stippledLineEnable)
    {
      stippleFactor = lineRasterState->lineStippleFactor;
      stipplePattern = lineRasterState->lineStipplePattern;
    }
  }

  // VkPipelineMultisampleStateCreateInfo
  if(pCreateInfo->pMultisampleState)
  {
    rasterizationSamples = pCreateInfo->pMultisampleState->rasterizationSamples;
    sampleShadingEnable = pCreateInfo->pMultisampleState->sampleShadingEnable ? true : false;
    minSampleShading = pCreateInfo->pMultisampleState->minSampleShading;
    sampleMask = pCreateInfo->pMultisampleState->pSampleMask
                     ? *pCreateInfo->pMultisampleState->pSampleMask
                     : ~0U;
    alphaToCoverageEnable = pCreateInfo->pMultisampleState->alphaToCoverageEnable ? true : false;
    alphaToOneEnable = pCreateInfo->pMultisampleState->alphaToOneEnable ? true : false;

    // VkPipelineSampleLocationsStateCreateInfoEXT
    sampleLocations.enabled = false;
    sampleLocations.gridSize.width = 1;
    sampleLocations.gridSize.height = 1;

    const VkPipelineSampleLocationsStateCreateInfoEXT *sampleLoc =
        (const VkPipelineSampleLocationsStateCreateInfoEXT *)FindNextStruct(
            pCreateInfo->pMultisampleState,
            VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT);
    if(sampleLoc)
    {
      sampleLocations.enabled = sampleLoc->sampleLocationsEnable ? true : false;

      if(!dynamicStates[VkDynamicSampleLocationsEXT])
      {
        sampleLocations.gridSize = sampleLoc->sampleLocationsInfo.sampleLocationGridSize;
        sampleLocations.locations.assign(sampleLoc->sampleLocationsInfo.pSampleLocations,
                                         sampleLoc->sampleLocationsInfo.sampleLocationsCount);

        RDCASSERTEQUAL(sampleLoc->sampleLocationsInfo.sampleLocationsPerPixel, rasterizationSamples);
      }
    }
  }
  else
  {
    rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    sampleShadingEnable = false;
    minSampleShading = 1.0f;
    sampleMask = ~0U;
    alphaToCoverageEnable = false;
    alphaToOneEnable = false;
  }

  // VkPipelineDepthStencilStateCreateInfo
  if(pCreateInfo->pDepthStencilState)
  {
    depthTestEnable = pCreateInfo->pDepthStencilState->depthTestEnable ? true : false;
    depthWriteEnable = pCreateInfo->pDepthStencilState->depthWriteEnable ? true : false;
    depthCompareOp = pCreateInfo->pDepthStencilState->depthCompareOp;
    depthBoundsEnable = pCreateInfo->pDepthStencilState->depthBoundsTestEnable ? true : false;
    stencilTestEnable = pCreateInfo->pDepthStencilState->stencilTestEnable ? true : false;
    front = pCreateInfo->pDepthStencilState->front;
    back = pCreateInfo->pDepthStencilState->back;
    minDepthBounds = pCreateInfo->pDepthStencilState->minDepthBounds;
    maxDepthBounds = pCreateInfo->pDepthStencilState->maxDepthBounds;
  }
  else
  {
    depthTestEnable = false;
    depthWriteEnable = false;
    depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depthBoundsEnable = false;
    stencilTestEnable = false;
    front.failOp = VK_STENCIL_OP_KEEP;
    front.passOp = VK_STENCIL_OP_KEEP;
    front.depthFailOp = VK_STENCIL_OP_KEEP;
    front.compareOp = VK_COMPARE_OP_ALWAYS;
    front.compareMask = 0xff;
    front.writeMask = 0xff;
    front.reference = 0;
    back = front;
    minDepthBounds = 0.0f;
    maxDepthBounds = 1.0f;
  }

  // VkPipelineColorBlendStateCreateInfo
  if(pCreateInfo->pColorBlendState)
  {
    logicOpEnable = pCreateInfo->pColorBlendState->logicOpEnable ? true : false;
    logicOp = pCreateInfo->pColorBlendState->logicOp;
    memcpy(blendConst, pCreateInfo->pColorBlendState->blendConstants, sizeof(blendConst));

    attachments.resize(pCreateInfo->pColorBlendState->attachmentCount);

    for(uint32_t i = 0; i < pCreateInfo->pColorBlendState->attachmentCount; i++)
    {
      attachments[i].blendEnable =
          pCreateInfo->pColorBlendState->pAttachments[i].blendEnable ? true : false;

      attachments[i].blend.Source =
          pCreateInfo->pColorBlendState->pAttachments[i].srcColorBlendFactor;
      attachments[i].blend.Destination =
          pCreateInfo->pColorBlendState->pAttachments[i].dstColorBlendFactor;
      attachments[i].blend.Operation = pCreateInfo->pColorBlendState->pAttachments[i].colorBlendOp;

      attachments[i].alphaBlend.Source =
          pCreateInfo->pColorBlendState->pAttachments[i].srcAlphaBlendFactor;
      attachments[i].alphaBlend.Destination =
          pCreateInfo->pColorBlendState->pAttachments[i].dstAlphaBlendFactor;
      attachments[i].alphaBlend.Operation =
          pCreateInfo->pColorBlendState->pAttachments[i].alphaBlendOp;

      attachments[i].channelWriteMask =
          (uint8_t)pCreateInfo->pColorBlendState->pAttachments[i].colorWriteMask;
    }
  }
  else
  {
    logicOpEnable = false;
    logicOp = VK_LOGIC_OP_NO_OP;
    RDCEraseEl(blendConst);

    attachments.clear();
  }

  // this struct probably will never get used, since the user could just set the colorWriteMask
  // above to 0. It's really only useful for specifying how the dynamic state works. However just
  // for completeness...
  const VkPipelineColorWriteCreateInfoEXT *colorWriteEnable =
      (const VkPipelineColorWriteCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pRasterizationState, VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT);
  if(colorWriteEnable)
  {
    RDCASSERTEQUAL(attachments.size(), colorWriteEnable->attachmentCount);
    for(size_t i = 0; i < attachments.size() && i < colorWriteEnable->attachmentCount; i++)
    {
      if(!colorWriteEnable->pColorWriteEnables[i])
        attachments[i].channelWriteMask = 0;
    }
  }

  // VkPipelineViewportDepthClipControlCreateInfoEXT
  negativeOneToOne = false;

  const VkPipelineViewportDepthClipControlCreateInfoEXT *depthClipControl =
      (const VkPipelineViewportDepthClipControlCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pViewportState,
          VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT);
  if(depthClipControl)
    negativeOneToOne = depthClipControl->negativeOneToOne != VK_FALSE;

  // VkPipelineRasterizationProvokingVertexStateCreateInfoEXT
  provokingVertex = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;

  const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT *provokeInfo =
      (const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pRasterizationState,
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT);
  if(provokeInfo)
    provokingVertex = provokeInfo->provokingVertexMode;

  const VkPipelineLibraryCreateInfoKHR *libraryReference =
      (const VkPipelineLibraryCreateInfoKHR *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR);
  if(libraryReference)
  {
    // unconditionally pull in state from libraries - the state must not overlap (or must be
    // identical where overlap is inevitable)
    for(uint32_t l = 0; l < libraryReference->libraryCount; l++)
    {
      ResourceId pipeid = GetResID(libraryReference->pLibraries[l]);

      parentLibraries.push_back(pipeid);

      const Pipeline &pipeInfo = info.m_Pipeline[pipeid];

      for(size_t i = 0; i < VkDynamicCount; i++)
        dynamicStates[i] |= pipeInfo.dynamicStates[i];

      if(pipeInfo.availStages & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
      {
        vertexBindings = pipeInfo.vertexBindings;
        vertexAttrs = pipeInfo.vertexAttrs;

        topology = pipeInfo.topology;
        primitiveRestartEnable = pipeInfo.primitiveRestartEnable;
      }

      if(pipeInfo.availStages & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
      {
        renderpass = pipeInfo.renderpass;
        subpass = pipeInfo.subpass;

        for(uint32_t i = 0; i < 4; i++)
        {
          shaders[i] = pipeInfo.shaders[i];
          info.m_ShaderModule[shaders[i].module].m_PipeReferences[id] = pipeid;
        }

        vertLayout = pipeInfo.vertLayout;

        viewportCount = pipeInfo.viewportCount;
        viewports = pipeInfo.viewports;
        scissors = pipeInfo.scissors;

        depthClampEnable = pipeInfo.depthClampEnable;
        rasterizerDiscardEnable = pipeInfo.rasterizerDiscardEnable;
        polygonMode = pipeInfo.polygonMode;
        cullMode = pipeInfo.cullMode;
        frontFace = pipeInfo.frontFace;
        depthBiasEnable = pipeInfo.depthBiasEnable;
        depthBiasConstantFactor = pipeInfo.depthBiasConstantFactor;
        depthBiasClamp = pipeInfo.depthBiasClamp;
        depthBiasSlopeFactor = pipeInfo.depthBiasSlopeFactor;
        lineWidth = pipeInfo.lineWidth;

        rasterizationStream = pipeInfo.rasterizationStream;
        depthClipEnable = pipeInfo.depthClipEnable;
        patchControlPoints = pipeInfo.patchControlPoints;
        tessellationDomainOrigin = pipeInfo.tessellationDomainOrigin;

        conservativeRasterizationMode = pipeInfo.conservativeRasterizationMode;
        extraPrimitiveOverestimationSize = pipeInfo.extraPrimitiveOverestimationSize;

        lineRasterMode = pipeInfo.lineRasterMode;
        stippleEnabled = pipeInfo.stippleEnabled;
        stippleFactor = pipeInfo.stippleFactor;
        stipplePattern = pipeInfo.stipplePattern;

        discardRectangles = pipeInfo.discardRectangles;
        discardMode = pipeInfo.discardMode;

        negativeOneToOne = pipeInfo.negativeOneToOne;

        provokingVertex = pipeInfo.provokingVertex;

        flags |= pipeInfo.flags;
      }

      if(pipeInfo.availStages & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
      {
        renderpass = pipeInfo.renderpass;
        subpass = pipeInfo.subpass;

        shaders[4] = pipeInfo.shaders[4];
        info.m_ShaderModule[shaders[4].module].m_PipeReferences[id] = pipeid;

        fragLayout = pipeInfo.fragLayout;

        sampleLocations = pipeInfo.sampleLocations;

        depthTestEnable = pipeInfo.depthTestEnable;
        depthWriteEnable = pipeInfo.depthWriteEnable;
        depthCompareOp = pipeInfo.depthCompareOp;
        depthBoundsEnable = pipeInfo.depthBoundsEnable;
        stencilTestEnable = pipeInfo.stencilTestEnable;
        front = pipeInfo.front;
        back = pipeInfo.back;
        minDepthBounds = pipeInfo.minDepthBounds;
        maxDepthBounds = pipeInfo.maxDepthBounds;

        shadingRate = pipeInfo.shadingRate;
        shadingRateCombiners[0] = pipeInfo.shadingRateCombiners[0];
        shadingRateCombiners[1] = pipeInfo.shadingRateCombiners[1];

        flags |= pipeInfo.flags;
      }

      if(pipeInfo.availStages & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
      {
        renderpass = pipeInfo.renderpass;
        subpass = pipeInfo.subpass;

        // this is duplicated in the fragment shader interface "if sample shading is enabled or
        // renderpass is not VK_NULL_HANDLE", but since it must be identical in both places we just
        // treat this as canonical so we don't have to figure out if that condition is satisfied.
        rasterizationSamples = pipeInfo.rasterizationSamples;
        sampleShadingEnable = pipeInfo.sampleShadingEnable;
        minSampleShading = pipeInfo.minSampleShading;
        sampleMask = pipeInfo.sampleMask;
        alphaToCoverageEnable = pipeInfo.alphaToCoverageEnable;
        alphaToOneEnable = pipeInfo.alphaToOneEnable;

        logicOpEnable = pipeInfo.logicOpEnable;
        logicOp = pipeInfo.logicOp;
        memcpy(blendConst, pipeInfo.blendConst, sizeof(blendConst));

        attachments = pipeInfo.attachments;

        viewMask = pipeInfo.viewMask;
        colorFormats = pipeInfo.colorFormats;
        depthFormat = pipeInfo.depthFormat;
        stencilFormat = pipeInfo.stencilFormat;

        flags |= pipeInfo.flags;
      }
    }
  }

  // calculate descSetLayouts. If only one layout is set, just copy the layouts from it
  if(vertLayout == ResourceId())
  {
    descSetLayouts = info.m_PipelineLayout[fragLayout].descSetLayouts;
  }
  else if(fragLayout == ResourceId())
  {
    descSetLayouts = info.m_PipelineLayout[vertLayout].descSetLayouts;
  }
  // if they're both the same (both must be non-empty or we would have hit a case above) it doesn't
  // matter
  else if(vertLayout == fragLayout)
  {
    descSetLayouts = info.m_PipelineLayout[vertLayout].descSetLayouts;
  }
  else
  {
    // in this case vertLayout is not the same as fragLayout, so we have independent sets and this
    // is the linked pipeline
    // fortunately one of the requirements of independent set is that any descriptor sets which
    // contain any fragment visible descriptors are present in the fragment layout, and vice-versa
    // for non-fragment. Any sets which contain both must be identical in both.
    // That means we can start by picking all the set layouts from the fragment pipeline layout that
    // reference fragments (ignoring any others that may be empty or not but are ignored), then for
    // all other sets unconditionally pick the one from the vertex layout

    const rdcarray<ResourceId> &vSets = info.m_PipelineLayout[vertLayout].descSetLayouts;
    const rdcarray<ResourceId> &fSets = info.m_PipelineLayout[fragLayout].descSetLayouts;

    descSetLayouts.resize(RDCMAX(vSets.size(), fSets.size()));

    for(size_t i = 0; i < fSets.size(); i++)
    {
      if((info.m_DescSetLayout[fSets[i]].anyStageFlags & VK_SHADER_STAGE_FRAGMENT_BIT) != 0)
        descSetLayouts[i] = fSets[i];
    }

    for(size_t i = 0; i < vSets.size(); i++)
    {
      if(descSetLayouts[i] == ResourceId())
        descSetLayouts[i] = vSets[i];
    }

    // it's possible we have sets which are unused by both - maybe empty, dummy, or they only appear
    // in the layout which ignores them. Pick from whichever layout contained that element as it
    // doesn't matter.
    for(size_t i = 0; i < descSetLayouts.size(); i++)
    {
      if(descSetLayouts[i] == ResourceId())
      {
        if(i < vSets.size())
          descSetLayouts[i] = vSets[i];
        else
          descSetLayouts[i] = fSets[i];
      }
    }
  }

  rdcarray<const DescSetLayout *> setLayoutInfos;
  for(ResourceId setLayout : descSetLayouts)
    setLayoutInfos.push_back(&info.m_DescSetLayout[setLayout]);

  for(const ShaderEntry &shad : shaders)
    shad.ProcessStaticDescriptorAccess(info.pushConstantDescriptorStorage,
                                       resourceMan->GetOriginalID(id), staticDescriptorAccess,
                                       setLayoutInfos);
}

void VulkanCreationInfo::Pipeline::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                        ResourceId id, const VkComputePipelineCreateInfo *pCreateInfo)
{
  flags = pCreateInfo->flags;

  graphicsPipe = false;

  compLayout = GetResID(pCreateInfo->layout);

  descSetLayouts = info.m_PipelineLayout[compLayout].descSetLayouts;

  // need to figure out which states are valid to be NULL

  // VkPipelineShaderStageCreateInfo
  {
    ResourceId shadid = GetResID(pCreateInfo->stage.module);
    ShaderEntry &shad = shaders[5];    // 5 is the compute shader's index (VS, TCS, TES, GS, FS, CS)

    VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *subgroupSize =
        (VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *)FindNextStruct(
            &pCreateInfo->stage,
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO);
    if(subgroupSize)
      shad.requiredSubgroupSize = subgroupSize->requiredSubgroupSize;

    shad.module = shadid;
    shad.entryPoint = pCreateInfo->stage.pName;
    shad.stage = ShaderStage::Compute;

    ShaderModuleReflectionKey key(ShaderStage::Compute, shad.entryPoint, ResourceId());

    if(pCreateInfo->stage.pSpecializationInfo)
    {
      key = ShaderModuleReflectionKey(ShaderStage::Compute, shad.entryPoint, id);

      const byte *data = (const byte *)pCreateInfo->stage.pSpecializationInfo->pData;

      const VkSpecializationMapEntry *maps = pCreateInfo->stage.pSpecializationInfo->pMapEntries;
      for(uint32_t s = 0; s < pCreateInfo->stage.pSpecializationInfo->mapEntryCount; s++)
      {
        SpecConstant spec;
        spec.specID = maps[s].constantID;
        memcpy(&spec.value, data + maps[s].offset, maps[s].size);
        spec.dataSize = maps[s].size;
        shad.specialization.push_back(spec);

        virtualSpecialisationByteSize =
            RDCMAX(virtualSpecialisationByteSize, uint32_t((spec.specID + 1) * sizeof(uint64_t)));
      }
    }

    ShaderModuleReflection &reflData = info.m_ShaderModule[shadid].m_Reflections[key];

    reflData.Init(resourceMan, shadid, info.m_ShaderModule[shadid].spirv, shad.entryPoint,
                  pCreateInfo->stage.stage, shad.specialization);

    shad.refl = reflData.refl;
    shad.patchData = &reflData.patchData;
  }

  topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  primitiveRestartEnable = false;

  patchControlPoints = 0;

  tessellationDomainOrigin = VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;

  viewportCount = 0;

  // VkPipelineRasterStateCreateInfo
  depthClampEnable = false;
  rasterizerDiscardEnable = false;
  polygonMode = VK_POLYGON_MODE_FILL;
  cullMode = VK_CULL_MODE_NONE;
  frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  // VkPipelineRasterizationConservativeStateCreateInfoEXT
  conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
  extraPrimitiveOverestimationSize = 0.0f;

  // VkPipelineMultisampleStateCreateInfo
  rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  sampleShadingEnable = false;
  minSampleShading = 1.0f;
  sampleMask = ~0U;

  // VkPipelineDepthStencilStateCreateInfo
  depthTestEnable = false;
  depthWriteEnable = false;
  depthCompareOp = VK_COMPARE_OP_ALWAYS;
  depthBoundsEnable = false;
  stencilTestEnable = false;
  RDCEraseEl(front);
  RDCEraseEl(back);

  // VkPipelineColorBlendStateCreateInfo
  alphaToCoverageEnable = false;
  logicOpEnable = false;
  logicOp = VK_LOGIC_OP_NO_OP;

  rdcarray<const DescSetLayout *> setLayoutInfos;
  for(ResourceId setLayout : descSetLayouts)
    setLayoutInfos.push_back(&info.m_DescSetLayout[setLayout]);

  for(const ShaderEntry &shad : shaders)
    shad.ProcessStaticDescriptorAccess(info.pushConstantDescriptorStorage,
                                       resourceMan->GetOriginalID(id), staticDescriptorAccess,
                                       setLayoutInfos);
}

void VulkanCreationInfo::Pipeline::Init(VulkanResourceManager *resourceMan,
                                        VulkanCreationInfo &info, ResourceId id,
                                        const VkRayTracingPipelineCreateInfoKHR *pCreateInfo)
{
  flags = pCreateInfo->flags;

  graphicsPipe = false;

  compLayout = GetResID(pCreateInfo->layout);

  descSetLayouts = info.m_PipelineLayout[compLayout].descSetLayouts;

  topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  primitiveRestartEnable = false;

  patchControlPoints = 0;

  tessellationDomainOrigin = VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;

  viewportCount = 0;

  // VkPipelineRasterStateCreateInfo
  depthClampEnable = false;
  rasterizerDiscardEnable = false;
  polygonMode = VK_POLYGON_MODE_FILL;
  cullMode = VK_CULL_MODE_NONE;
  frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  // VkPipelineRasterizationConservativeStateCreateInfoEXT
  conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
  extraPrimitiveOverestimationSize = 0.0f;

  // VkPipelineMultisampleStateCreateInfo
  rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  sampleShadingEnable = false;
  minSampleShading = 1.0f;
  sampleMask = ~0U;

  // VkPipelineDepthStencilStateCreateInfo
  depthTestEnable = false;
  depthWriteEnable = false;
  depthCompareOp = VK_COMPARE_OP_ALWAYS;
  depthBoundsEnable = false;
  stencilTestEnable = false;
  RDCEraseEl(front);
  RDCEraseEl(back);

  // VkPipelineColorBlendStateCreateInfo
  alphaToCoverageEnable = false;
  logicOpEnable = false;
  logicOp = VK_LOGIC_OP_NO_OP;

  rdcarray<const DescSetLayout *> setLayoutInfos;
  for(ResourceId setLayout : descSetLayouts)
    setLayoutInfos.push_back(&info.m_DescSetLayout[setLayout]);
}

void VulkanCreationInfo::PipelineLayout::Init(VulkanResourceManager *resourceMan,
                                              VulkanCreationInfo &info,
                                              const VkPipelineLayoutCreateInfo *pCreateInfo)
{
  flags = pCreateInfo->flags;

  if(pCreateInfo->pSetLayouts)
  {
    descSetLayouts.resize(pCreateInfo->setLayoutCount);
    for(uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++)
      descSetLayouts[i] = GetResID(pCreateInfo->pSetLayouts[i]);
  }

  if(pCreateInfo->pPushConstantRanges)
  {
    pushRanges.reserve(pCreateInfo->pushConstantRangeCount);
    for(uint32_t i = 0; i < pCreateInfo->pushConstantRangeCount; i++)
      pushRanges.push_back(pCreateInfo->pPushConstantRanges[i]);
  }
}

void VulkanCreationInfo::RenderPass::Init(VulkanResourceManager *resourceMan,
                                          VulkanCreationInfo &info,
                                          const VkRenderPassCreateInfo *pCreateInfo)
{
  attachments.resize(pCreateInfo->attachmentCount);
  for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
  {
    Attachment &dst = attachments[i];
    dst.used = false;
    dst.flags = pCreateInfo->pAttachments[i].flags;
    dst.format = pCreateInfo->pAttachments[i].format;
    dst.samples = pCreateInfo->pAttachments[i].samples;
    dst.loadOp = pCreateInfo->pAttachments[i].loadOp;
    dst.storeOp = pCreateInfo->pAttachments[i].storeOp;
    dst.stencilLoadOp = pCreateInfo->pAttachments[i].stencilLoadOp;
    dst.stencilStoreOp = pCreateInfo->pAttachments[i].stencilStoreOp;
    dst.stencilInitialLayout = dst.initialLayout = pCreateInfo->pAttachments[i].initialLayout;
    dst.stencilFinalLayout = dst.finalLayout = pCreateInfo->pAttachments[i].finalLayout;
  }

  // VK_KHR_multiview
  const VkRenderPassMultiviewCreateInfo *multiview =
      (const VkRenderPassMultiviewCreateInfo *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);

  // VK_EXT_fragment_density_map
  const VkRenderPassFragmentDensityMapCreateInfoEXT *fragmentDensity =
      (const VkRenderPassFragmentDensityMapCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);

  subpasses.resize(pCreateInfo->subpassCount);
  for(uint32_t subp = 0; subp < pCreateInfo->subpassCount; subp++)
  {
    const VkSubpassDescription &src = pCreateInfo->pSubpasses[subp];
    Subpass &dst = subpasses[subp];

    dst.inputAttachments.resize(src.inputAttachmentCount);
    dst.inputLayouts.resize(src.inputAttachmentCount);
    dst.inputStencilLayouts.resize(src.inputAttachmentCount);
    for(uint32_t i = 0; i < src.inputAttachmentCount; i++)
    {
      dst.inputAttachments[i] = src.pInputAttachments[i].attachment;
      dst.inputStencilLayouts[i] = dst.inputLayouts[i] = src.pInputAttachments[i].layout;
    }

    dst.colorAttachments.resize(src.colorAttachmentCount);
    dst.resolveAttachments.resize(src.colorAttachmentCount);
    dst.colorLayouts.resize(src.colorAttachmentCount);
    for(uint32_t i = 0; i < src.colorAttachmentCount; i++)
    {
      dst.resolveAttachments[i] =
          src.pResolveAttachments ? src.pResolveAttachments[i].attachment : ~0U;
      dst.colorAttachments[i] = src.pColorAttachments[i].attachment;
      dst.colorLayouts[i] = src.pColorAttachments[i].layout;

      if(dst.resolveAttachments[i] != VK_ATTACHMENT_UNUSED)
        attachments[dst.resolveAttachments[i]].used = true;
      if(dst.colorAttachments[i] != VK_ATTACHMENT_UNUSED)
        attachments[dst.colorAttachments[i]].used = true;
    }

    dst.depthstencilAttachment =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? (int32_t)src.pDepthStencilAttachment->attachment
             : -1);
    dst.stencilLayout = dst.depthLayout =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? src.pDepthStencilAttachment->layout
             : VK_IMAGE_LAYOUT_UNDEFINED);

    if(dst.depthstencilAttachment >= 0)
      attachments[dst.depthstencilAttachment].used = true;

    dst.depthstencilResolveAttachment = -1;

    dst.fragmentDensityAttachment =
        (fragmentDensity &&
                 fragmentDensity->fragmentDensityMapAttachment.attachment != VK_ATTACHMENT_UNUSED
             ? fragmentDensity->fragmentDensityMapAttachment.attachment
             : -1);

    dst.fragmentDensityLayout =
        (fragmentDensity &&
                 fragmentDensity->fragmentDensityMapAttachment.attachment != VK_ATTACHMENT_UNUSED
             ? fragmentDensity->fragmentDensityMapAttachment.layout
             : VK_IMAGE_LAYOUT_UNDEFINED);

    dst.shadingRateAttachment = -1;
    dst.shadingRateLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dst.shadingRateTexelSize = VkExtent2D({1, 1});

    dst.feedbackLoop = false;

    dst.tileOnlyMSAAEnable = false;
    dst.tileOnlyMSAASampleCount = VK_SAMPLE_COUNT_1_BIT;

    if(multiview && multiview->subpassCount > 0)
    {
      uint32_t mask = multiview->pViewMasks[subp];
      for(uint32_t i = 0; i < 32; i++)
      {
        if(mask & (1 << i))
          dst.multiviews.push_back(i);
      }
    }
  }

  for(uint32_t i = 0; i < pCreateInfo->dependencyCount; i++)
  {
    if(pCreateInfo->pDependencies[i].dependencyFlags & VK_DEPENDENCY_FEEDBACK_LOOP_BIT_EXT)
    {
      subpasses[pCreateInfo->pDependencies[i].dstSubpass].feedbackLoop = true;
    }
  }
}

void VulkanCreationInfo::RenderPass::Init(VulkanResourceManager *resourceMan,
                                          VulkanCreationInfo &info,
                                          const VkRenderPassCreateInfo2 *pCreateInfo)
{
  attachments.resize(pCreateInfo->attachmentCount);
  for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
  {
    Attachment &dst = attachments[i];
    dst.used = false;
    dst.flags = pCreateInfo->pAttachments[i].flags;
    dst.format = pCreateInfo->pAttachments[i].format;
    dst.samples = pCreateInfo->pAttachments[i].samples;
    dst.loadOp = pCreateInfo->pAttachments[i].loadOp;
    dst.storeOp = pCreateInfo->pAttachments[i].storeOp;
    dst.stencilLoadOp = pCreateInfo->pAttachments[i].stencilLoadOp;
    dst.stencilStoreOp = pCreateInfo->pAttachments[i].stencilStoreOp;
    dst.stencilInitialLayout = dst.initialLayout = pCreateInfo->pAttachments[i].initialLayout;
    dst.stencilFinalLayout = dst.finalLayout = pCreateInfo->pAttachments[i].finalLayout;

    // VK_KHR_separate_depth_stencil_layouts
    const VkAttachmentDescriptionStencilLayout *separateStencil =
        (const VkAttachmentDescriptionStencilLayout *)FindNextStruct(
            &pCreateInfo->pAttachments[i], VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT);

    if(separateStencil)
    {
      dst.stencilInitialLayout = separateStencil->stencilInitialLayout;
      dst.stencilFinalLayout = separateStencil->stencilFinalLayout;
    }
  }

  // VK_EXT_fragment_density_map
  const VkRenderPassFragmentDensityMapCreateInfoEXT *fragmentDensity =
      (const VkRenderPassFragmentDensityMapCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);

  subpasses.resize(pCreateInfo->subpassCount);
  for(uint32_t subp = 0; subp < pCreateInfo->subpassCount; subp++)
  {
    const VkSubpassDescription2 &src = pCreateInfo->pSubpasses[subp];
    Subpass &dst = subpasses[subp];

    dst.inputAttachments.resize(src.inputAttachmentCount);
    dst.inputLayouts.resize(src.inputAttachmentCount);
    dst.inputStencilLayouts.resize(src.inputAttachmentCount);
    for(uint32_t i = 0; i < src.inputAttachmentCount; i++)
    {
      dst.inputAttachments[i] = src.pInputAttachments[i].attachment;
      dst.inputStencilLayouts[i] = dst.inputLayouts[i] = src.pInputAttachments[i].layout;
      const VkAttachmentReferenceStencilLayout *stencilLayout =
          (const VkAttachmentReferenceStencilLayout *)FindNextStruct(
              &src.pInputAttachments[i], VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT);
      if(stencilLayout != NULL)
        dst.inputStencilLayouts[i] = stencilLayout->stencilLayout;
    }

    dst.colorAttachments.resize(src.colorAttachmentCount);
    dst.resolveAttachments.resize(src.colorAttachmentCount);
    dst.colorLayouts.resize(src.colorAttachmentCount);
    for(uint32_t i = 0; i < src.colorAttachmentCount; i++)
    {
      dst.resolveAttachments[i] =
          src.pResolveAttachments ? src.pResolveAttachments[i].attachment : ~0U;
      dst.colorAttachments[i] = src.pColorAttachments[i].attachment;
      dst.colorLayouts[i] = src.pColorAttachments[i].layout;

      if(dst.resolveAttachments[i] != VK_ATTACHMENT_UNUSED)
        attachments[dst.resolveAttachments[i]].used = true;
      if(dst.colorAttachments[i] != VK_ATTACHMENT_UNUSED)
        attachments[dst.colorAttachments[i]].used = true;
    }

    dst.depthstencilAttachment =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? (int32_t)src.pDepthStencilAttachment->attachment
             : -1);
    dst.stencilLayout = dst.depthLayout =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? src.pDepthStencilAttachment->layout
             : VK_IMAGE_LAYOUT_UNDEFINED);

    if(dst.depthstencilAttachment >= 0)
      attachments[dst.depthstencilAttachment].used = true;

    // VK_KHR_separate_depth_stencil_layouts
    const VkAttachmentReferenceStencilLayout *separateStencil =
        (const VkAttachmentReferenceStencilLayout *)FindNextStruct(
            src.pDepthStencilAttachment, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT);

    if(separateStencil)
      dst.stencilLayout = separateStencil->stencilLayout;

    // VK_KHR_depth_stencil_resolve
    const VkSubpassDescriptionDepthStencilResolve *depthstencilResolve =
        (const VkSubpassDescriptionDepthStencilResolve *)FindNextStruct(
            &src, VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE);

    // Note: with VK_EXT_multisampled_render_to_single_sampled, it's possible for
    // VkSubpassDescriptionDepthStencilResolve to be provided (for the sake of specifying the
    // resolve mode) without an attachment.
    dst.depthstencilResolveAttachment =
        (depthstencilResolve && depthstencilResolve->pDepthStencilResolveAttachment &&
                 depthstencilResolve->pDepthStencilResolveAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? depthstencilResolve->pDepthStencilResolveAttachment->attachment
             : -1);

    // VK_EXT_fragment_density_map
    dst.fragmentDensityAttachment =
        (fragmentDensity &&
                 fragmentDensity->fragmentDensityMapAttachment.attachment != VK_ATTACHMENT_UNUSED
             ? fragmentDensity->fragmentDensityMapAttachment.attachment
             : -1);

    dst.fragmentDensityLayout =
        (fragmentDensity &&
                 fragmentDensity->fragmentDensityMapAttachment.attachment != VK_ATTACHMENT_UNUSED
             ? fragmentDensity->fragmentDensityMapAttachment.layout
             : VK_IMAGE_LAYOUT_UNDEFINED);

    // VK_KHR_fragment_shading_rate
    const VkFragmentShadingRateAttachmentInfoKHR *shadingRate =
        (const VkFragmentShadingRateAttachmentInfoKHR *)FindNextStruct(
            &src, VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);
    dst.shadingRateAttachment =
        (shadingRate && shadingRate->pFragmentShadingRateAttachment &&
                 shadingRate->pFragmentShadingRateAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? shadingRate->pFragmentShadingRateAttachment->attachment
             : -1);

    dst.shadingRateLayout =
        (shadingRate && shadingRate->pFragmentShadingRateAttachment &&
                 shadingRate->pFragmentShadingRateAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? shadingRate->pFragmentShadingRateAttachment->layout
             : VK_IMAGE_LAYOUT_UNDEFINED);

    dst.shadingRateTexelSize =
        shadingRate ? shadingRate->shadingRateAttachmentTexelSize : VkExtent2D({1, 1});

    // VK_EXT_multisampled_render_to_single_sampled
    const VkMultisampledRenderToSingleSampledInfoEXT *tileOnlyMSAA =
        (const VkMultisampledRenderToSingleSampledInfoEXT *)FindNextStruct(
            &src, VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT);

    dst.tileOnlyMSAAEnable =
        tileOnlyMSAA ? tileOnlyMSAA->multisampledRenderToSingleSampledEnable != VK_FALSE : false;
    dst.tileOnlyMSAASampleCount =
        tileOnlyMSAA ? tileOnlyMSAA->rasterizationSamples : VK_SAMPLE_COUNT_1_BIT;

    for(uint32_t i = 0; i < 32; i++)
    {
      if(src.viewMask & (1 << i))
        dst.multiviews.push_back(i);
    }
  }
}

void VulkanCreationInfo::Framebuffer::Init(VulkanResourceManager *resourceMan,
                                           VulkanCreationInfo &info,
                                           const VkFramebufferCreateInfo *pCreateInfo)
{
  width = pCreateInfo->width;
  height = pCreateInfo->height;
  layers = pCreateInfo->layers;

  imageless = false;

  attachments.resize(pCreateInfo->attachmentCount);
  if(pCreateInfo->flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT)
  {
    imageless = true;

    // VK_KHR_imageless_framebuffer
    const VkFramebufferAttachmentsCreateInfo *attachmentsInfo =
        (const VkFramebufferAttachmentsCreateInfo *)FindNextStruct(
            pCreateInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO);

    RDCASSERTEQUAL(pCreateInfo->attachmentCount, attachmentsInfo->attachmentImageInfoCount);

    for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
    {
      attachments[i].createdView = ResourceId();
      // there must be at least one format in the list so we can safely look at [0].
      // also all entries must be compatible, so if [0] doesn't have stencil then none of them do,
      // and vice-versa.
      attachments[i].hasStencil =
          IsStencilFormat(attachmentsInfo->pAttachmentImageInfos[i].pViewFormats[0]);
    }
  }
  else
  {
    for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
    {
      attachments[i].createdView = GetResID(pCreateInfo->pAttachments[i]);
      attachments[i].hasStencil =
          IsStencilFormat(info.m_ImageView[attachments[i].createdView].format);
    }
  }
}

void VulkanCreationInfo::Memory::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                      const VkMemoryAllocateInfo *pAllocInfo)
{
  memoryTypeIndex = pAllocInfo->memoryTypeIndex;
  allocSize = wholeMemBufSize = pAllocInfo->allocationSize;
}

void VulkanCreationInfo::Memory::SimplifyBindings()
{
  // after initialisation we're likely to end up with a lot of gaps of 'none' in between tiled or
  // linear resources. Regions of memory with no bindings are not visible in any meaningful way
  // (memory can only be read with an image or buffer bound to it) so we perform a pass collapsing
  // any 'None' intervals into the previous to be able to simplify the set of intervals. This means
  // we might promote some regions to tiled, but that's fine since as above their contents are
  // essentially meaningless.

  // if the first entry is None and we have a second entry, then set the first to whatever the
  // second is
  if(bindings.size() > 1 && bindings.begin()->value() == VulkanCreationInfo::Memory::None)
  {
    auto it = bindings.begin();
    it++;
    bindings.begin()->setValue(it->value());
  }

  for(auto it = bindings.begin(); it != bindings.end(); it++)
  {
    // if we're not at the begining and the current range is None, copy whatever was in the previous
    // range
    if(it != bindings.begin() && it->value() == VulkanCreationInfo::Memory::None)
    {
      auto previt = it;
      previt--;

      it->setValue(previt->value());
    }

    // merge left when possible
    it->mergeLeft();
  }
}

void VulkanCreationInfo::Buffer::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                      const VkBufferCreateInfo *pCreateInfo,
                                      VkMemoryRequirements origMrq)
{
  usage = pCreateInfo->usage;
  size = pCreateInfo->size;
  gpuAddress = 0;

  mrq = origMrq;

  external = false;

  if(FindNextStruct(pCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO))
  {
    external = true;
  }
}

void VulkanCreationInfo::BufferView::Init(VulkanResourceManager *resourceMan,
                                          VulkanCreationInfo &info,
                                          const VkBufferViewCreateInfo *pCreateInfo)
{
  buffer = GetResID(pCreateInfo->buffer);
  format = pCreateInfo->format;
  offset = pCreateInfo->offset;
  size = pCreateInfo->range;
}

void VulkanCreationInfo::Image::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                     const VkImageCreateInfo *pCreateInfo,
                                     VkMemoryRequirements origMrq)
{
  type = pCreateInfo->imageType;
  format = pCreateInfo->format;
  extent = pCreateInfo->extent;
  arrayLayers = pCreateInfo->arrayLayers;
  mipLevels = pCreateInfo->mipLevels;
  samples = RDCMAX(VK_SAMPLE_COUNT_1_BIT, pCreateInfo->samples);

  mrq = origMrq;

  linear = pCreateInfo->tiling == VK_IMAGE_TILING_LINEAR;

  external = false;

  if(FindNextStruct(pCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV) ||
     FindNextStruct(pCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO) ||
     FindNextStruct(pCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID))
  {
    external = true;
  }

  creationFlags = TextureCategory::NoFlags;

  if(pCreateInfo->usage & VK_IMAGE_USAGE_SAMPLED_BIT)
    creationFlags |= TextureCategory::ShaderRead;
  if(pCreateInfo->usage &
     (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT))
    creationFlags |= TextureCategory::ColorTarget;
  if(pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    creationFlags |= TextureCategory::DepthTarget;
  if(pCreateInfo->usage & VK_IMAGE_USAGE_STORAGE_BIT)
    creationFlags |= TextureCategory::ShaderReadWrite;

  cube = (pCreateInfo->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) ? true : false;
}

void VulkanCreationInfo::Sampler::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                       const VkSamplerCreateInfo *pCreateInfo)
{
  magFilter = pCreateInfo->magFilter;
  minFilter = pCreateInfo->minFilter;
  mipmapMode = pCreateInfo->mipmapMode;
  address[0] = pCreateInfo->addressModeU;
  address[1] = pCreateInfo->addressModeV;
  address[2] = pCreateInfo->addressModeW;
  mipLodBias = pCreateInfo->mipLodBias;
  maxAnisotropy = pCreateInfo->anisotropyEnable ? pCreateInfo->maxAnisotropy : 0.0f;
  compareEnable = pCreateInfo->compareEnable != 0;
  compareOp = pCreateInfo->compareOp;
  minLod = pCreateInfo->minLod;
  maxLod = pCreateInfo->maxLod;
  borderColor = pCreateInfo->borderColor;
  unnormalizedCoordinates = pCreateInfo->unnormalizedCoordinates != 0;

  seamless = true;
  if((pCreateInfo->flags & VK_SAMPLER_CREATE_NON_SEAMLESS_CUBE_MAP_BIT_EXT) != 0)
    seamless = false;

  reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

  const VkSamplerReductionModeCreateInfo *reduction =
      (const VkSamplerReductionModeCreateInfo *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO);
  if(reduction)
  {
    reductionMode = reduction->reductionMode;
  }

  const VkSamplerYcbcrConversionInfo *ycbcrInfo =
      (const VkSamplerYcbcrConversionInfo *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
  if(ycbcrInfo)
  {
    ycbcr = GetResID(ycbcrInfo->conversion);
  }

  customBorder = false;
  RDCEraseEl(customBorderColor);
  customBorderFormat = VK_FORMAT_UNDEFINED;

  const VkSamplerCustomBorderColorCreateInfoEXT *border =
      (const VkSamplerCustomBorderColorCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT);
  if(border)
  {
    customBorder = true;
    customBorderColor = border->customBorderColor;
    customBorderFormat = border->format;
  }

  RDCEraseEl(componentMapping);
  srgbBorder = false;

  const VkSamplerBorderColorComponentMappingCreateInfoEXT *borderSwizzle =
      (const VkSamplerBorderColorComponentMappingCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_BORDER_COLOR_COMPONENT_MAPPING_CREATE_INFO_EXT);
  if(borderSwizzle)
  {
    componentMapping = borderSwizzle->components;
    srgbBorder = borderSwizzle->srgb != VK_FALSE;
  }
}

void VulkanCreationInfo::YCbCrSampler::Init(VulkanResourceManager *resourceMan,
                                            VulkanCreationInfo &info,
                                            const VkSamplerYcbcrConversionCreateInfo *pCreateInfo)
{
  switch(pCreateInfo->ycbcrModel)
  {
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY: ycbcrModel = YcbcrConversion::Raw; break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY:
      ycbcrModel = YcbcrConversion::RangeOnly;
      break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709: ycbcrModel = YcbcrConversion::BT709; break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601: ycbcrModel = YcbcrConversion::BT601; break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020: ycbcrModel = YcbcrConversion::BT2020; break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_MAX_ENUM: break;
  }

  switch(pCreateInfo->ycbcrRange)
  {
    case VK_SAMPLER_YCBCR_RANGE_ITU_FULL: ycbcrRange = YcbcrRange::ITUFull; break;
    case VK_SAMPLER_YCBCR_RANGE_ITU_NARROW: ycbcrRange = YcbcrRange::ITUNarrow; break;
    case VK_SAMPLER_YCBCR_RANGE_MAX_ENUM: break;
  }

  switch(pCreateInfo->xChromaOffset)
  {
    case VK_CHROMA_LOCATION_COSITED_EVEN: xChromaOffset = ChromaSampleLocation::CositedEven; break;
    case VK_CHROMA_LOCATION_MIDPOINT: xChromaOffset = ChromaSampleLocation::Midpoint; break;
    case VK_CHROMA_LOCATION_MAX_ENUM: break;
  }

  switch(pCreateInfo->yChromaOffset)
  {
    case VK_CHROMA_LOCATION_COSITED_EVEN: yChromaOffset = ChromaSampleLocation::CositedEven; break;
    case VK_CHROMA_LOCATION_MIDPOINT: yChromaOffset = ChromaSampleLocation::Midpoint; break;
    case VK_CHROMA_LOCATION_MAX_ENUM: break;
  }

  componentMapping = pCreateInfo->components;
  chromaFilter = MakeFilterMode(pCreateInfo->chromaFilter);
  forceExplicitReconstruction = pCreateInfo->forceExplicitReconstruction != 0;
}

void VulkanCreationInfo::ImageView::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                         const VkImageViewCreateInfo *pCreateInfo)
{
  image = GetResID(pCreateInfo->image);
  format = pCreateInfo->format;
  range = pCreateInfo->subresourceRange;
  viewType = pCreateInfo->viewType;

  if(range.levelCount == VK_REMAINING_MIP_LEVELS)
    range.levelCount = info.m_Image[image].mipLevels - range.baseMipLevel;

  if(range.layerCount == VK_REMAINING_ARRAY_LAYERS)
    range.layerCount = info.m_Image[image].arrayLayers - range.baseArrayLayer;

  componentMapping = pCreateInfo->components;

  minLOD = 0.0f;

  const VkImageViewMinLodCreateInfoEXT *minLODInfo =
      (const VkImageViewMinLodCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT);
  if(minLODInfo)
  {
    minLOD = minLODInfo->minLod;
  }
}

void VulkanCreationInfo::ShaderModule::Init(VulkanResourceManager *resourceMan,
                                            VulkanCreationInfo &info,
                                            const VkShaderModuleCreateInfo *pCreateInfo)
{
  const uint32_t SPIRVMagic = 0x07230203;
  if(pCreateInfo->codeSize < 4 || memcmp(pCreateInfo->pCode, &SPIRVMagic, sizeof(SPIRVMagic)) != 0)
  {
    RDCWARN("Shader not provided with SPIR-V");
  }
  else
  {
    RDCASSERT(pCreateInfo->codeSize % sizeof(uint32_t) == 0);
    spirv.Parse(rdcarray<uint32_t>((uint32_t *)(pCreateInfo->pCode),
                                   pCreateInfo->codeSize / sizeof(uint32_t)));
  }
}

void VulkanCreationInfo::ShaderModule::Reinit()
{
  bool lz4 = false;

  rdcstr originalPath = unstrippedPath;

  if(!strncmp(originalPath.c_str(), "lz4#", 4))
  {
    originalPath = originalPath.substr(4);
    lz4 = true;
  }
  // could support more if we're willing to compile in the decompressor

  FILE *originalShaderFile = NULL;

  const rdcarray<rdcstr> &searchPaths = DXBC_Debug_SearchDirPaths();

  size_t numSearchPaths = searchPaths.size();

  rdcstr foundPath;

  // keep searching until we've exhausted all possible path options, or we've found a file that
  // opens
  while(originalShaderFile == NULL && !originalPath.empty())
  {
    // while we haven't found a file, keep trying through the search paths. For i==0
    // check the path on its own, in case it's an absolute path.
    for(size_t i = 0; originalShaderFile == NULL && i <= numSearchPaths; i++)
    {
      if(i == 0)
      {
        originalShaderFile = FileIO::fopen(originalPath, FileIO::ReadBinary);
        foundPath = originalPath;
        continue;
      }
      else
      {
        const rdcstr &searchPath = searchPaths[i - 1];
        foundPath = searchPath + "/" + originalPath;
        originalShaderFile = FileIO::fopen(foundPath, FileIO::ReadBinary);
      }
    }

    if(originalShaderFile == NULL)
    {
      // follow D3D's search behaviour for consistency: when presented with a
      // relative path containing subfolders like foo/bar/blah.pdb then we should first try to
      // append it to all search paths as-is, then strip off the top-level subdirectory to get
      // bar/blah.pdb and try that in all search directories, and keep going. So if we got here
      // and didn't open a file, try to strip off the the top directory and continue.
      int32_t offs = originalPath.find_first_of("\\/");

      // if we couldn't find a directory separator there's nothing to do, stop looking
      if(offs == -1)
        break;

      // otherwise strip up to there and keep going
      originalPath.erase(0, offs + 1);
    }
  }

  if(originalShaderFile == NULL)
    return;

  FileIO::fseek64(originalShaderFile, 0L, SEEK_END);
  uint64_t originalShaderSize = FileIO::ftell64(originalShaderFile);
  FileIO::fseek64(originalShaderFile, 0, SEEK_SET);

  {
    bytebuf debugBytecode;

    debugBytecode.resize((size_t)originalShaderSize);
    FileIO::fread(&debugBytecode[0], sizeof(byte), (size_t)originalShaderSize, originalShaderFile);

    if(lz4)
    {
      rdcarray<byte> decompressed;

      // first try decompressing to 1MB flat
      decompressed.resize(100 * 1024);

      int ret = LZ4_decompress_safe((const char *)&debugBytecode[0], (char *)&decompressed[0],
                                    (int)debugBytecode.size(), (int)decompressed.size());

      if(ret < 0)
      {
        // if it failed, either source is corrupt or we didn't allocate enough space.
        // Just allocate 255x compressed size since it can't need any more than that.
        decompressed.resize(255 * debugBytecode.size());

        ret = LZ4_decompress_safe((const char *)&debugBytecode[0], (char *)&decompressed[0],
                                  (int)debugBytecode.size(), (int)decompressed.size());

        if(ret < 0)
        {
          RDCERR("Failed to decompress LZ4 data from %s", foundPath.c_str());
          return;
        }
      }

      RDCASSERT(ret > 0, ret);

      // we resize and memcpy instead of just doing .swap() because that would
      // transfer over the over-large pessimistic capacity needed for decompression
      debugBytecode.resize(ret);
      memcpy(&debugBytecode[0], &decompressed[0], debugBytecode.size());
    }

    rdcspv::Reflector reflTest;
    reflTest.Parse(rdcarray<uint32_t>((uint32_t *)(debugBytecode.data()),
                                      debugBytecode.size() / sizeof(uint32_t)));

    if(!reflTest.GetSPIRV().empty())
    {
      spirv = reflTest;
    }
  }

  FileIO::fclose(originalShaderFile);
}

void VulkanCreationInfo::ShaderModuleReflection::Init(VulkanResourceManager *resourceMan,
                                                      ResourceId id, const rdcspv::Reflector &spv,
                                                      const rdcstr &entry,
                                                      VkShaderStageFlagBits stage,
                                                      const rdcarray<SpecConstant> &specInfo)
{
  if(entryPoint.empty())
  {
    entryPoint = entry;
    stageIndex = StageIndex(stage);

    spv.MakeReflection(GraphicsAPI::Vulkan, ShaderStage(stageIndex), entryPoint, specInfo, *refl,
                       patchData);

    refl->resourceId = resourceMan->GetOriginalID(id);
  }
}

void VulkanCreationInfo::ShaderModuleReflection::PopulateDisassembly(const rdcspv::Reflector &spirv)
{
  if(disassembly.empty())
    disassembly = spirv.Disassemble(refl->entryPoint, instructionLines);
}

void VulkanCreationInfo::QueryPool::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                         const VkQueryPoolCreateInfo *pCreateInfo)
{
  queryType = pCreateInfo->queryType;
  queryCount = pCreateInfo->queryCount;
  pipelineStatistics = pCreateInfo->pipelineStatistics;
}

void VulkanCreationInfo::DescSetPool::Init(VulkanResourceManager *resourceMan,
                                           VulkanCreationInfo &info,
                                           const VkDescriptorPoolCreateInfo *pCreateInfo)
{
  maxSets = pCreateInfo->maxSets;
  poolSizes.assign(pCreateInfo->pPoolSizes, pCreateInfo->poolSizeCount);

  const VkMutableDescriptorTypeCreateInfoEXT *mutableInfo =
      (const VkMutableDescriptorTypeCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT);

  if(mutableInfo)
  {
    for(uint32_t i = 0;
        i < RDCMIN(pCreateInfo->poolSizeCount, mutableInfo->mutableDescriptorTypeListCount); i++)
    {
      const VkMutableDescriptorTypeListEXT &mutableTypes =
          mutableInfo->pMutableDescriptorTypeLists[i];

      RDCCOMPILE_ASSERT(uint64_t(DescriptorSlotType::Count) < 64,
                        "Descriptor types don't fit in 64-bit bitmask anymore");

      mutableBitmasks.resize_for_index(i);

      // encode a bitmask with the available types for this descriptor
      for(uint32_t m = 0; m < mutableTypes.descriptorTypeCount; m++)
        mutableBitmasks[i] |= (1ULL << uint64_t(convert(mutableTypes.pDescriptorTypes[m])));
    }
  }
}

void VulkanCreationInfo::DescSetPool::CreateOverflow(VkDevice device,
                                                     VulkanResourceManager *resourceMan)
{
  VkDescriptorPoolCreateInfo poolInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      NULL,
      0,
      maxSets,
      (uint32_t)poolSizes.size(),
      &poolSizes[0],
  };

  VkMutableDescriptorTypeCreateInfoEXT mutableCreateInfo = {
      VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
  };

  // conservative resizing, not every descriptor type will be used but they can't overlap so at
  // most we'll see 64 different types
  VkDescriptorType mutableTypes[64];
  rdcarray<VkMutableDescriptorTypeListEXT> mutableLists;

  if(!mutableBitmasks.empty())
  {
    poolInfo.pNext = &mutableCreateInfo;

    mutableLists.resize(poolInfo.poolSizeCount);

    VkDescriptorType *cur = mutableTypes;
    for(size_t i = 0; i < mutableBitmasks.size(); i++)
    {
      // list of descriptors starts here
      mutableLists[i].pDescriptorTypes = cur;
      mutableLists[i].descriptorTypeCount = 0;

      // loop over every type
      for(uint64_t m = 0; m < 64; m++)
      {
        // skip types not in this bitmask
        if(((1ULL << m) & mutableBitmasks[i]) == 0)
          continue;

        // for types included, write them into the list and increment
        *(cur++) = convert(DescriptorSlotType(m));
        mutableLists[i].descriptorTypeCount++;
      }
    }

    mutableCreateInfo.mutableDescriptorTypeListCount = poolInfo.poolSizeCount;
    mutableCreateInfo.pMutableDescriptorTypeLists = mutableLists.data();
  }

  VkDescriptorPool pool;

  VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), &poolInfo, NULL, &pool);
  RDCASSERTEQUAL(ret, VK_SUCCESS);

  ResourceId poolid = resourceMan->WrapResource(Unwrap(device), pool);

  // register as a live-only resource, so it is cleaned up properly
  resourceMan->AddLiveResource(poolid, pool);

  overflow.push_back(pool);
}

void VulkanCreationInfo::AccelerationStructure::Init(
    VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
    const VkAccelerationStructureCreateInfoKHR *pCreateInfo)
{
  buffer = GetResID(pCreateInfo->buffer);
  offset = pCreateInfo->offset;
  size = pCreateInfo->size;
  type = pCreateInfo->type;
}

void DescUpdateTemplate::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                              const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo)
{
  updates.assign(pCreateInfo->pDescriptorUpdateEntries, pCreateInfo->descriptorUpdateEntryCount);

  bindPoint = pCreateInfo->pipelineBindPoint;

  unwrapByteSize = 0;

  texelBufferViewCount = 0;
  bufferInfoCount = 0;
  imageInfoCount = 0;
  inlineInfoCount = 0;
  inlineByteSize = 0;
  accelerationStructureCount = 0;

  for(const VkDescriptorUpdateTemplateEntry &entry : updates)
  {
    uint32_t entrySize = 4;

    size_t stride = entry.stride;

    if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
       entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
    {
      entrySize = sizeof(VkBufferView);

      texelBufferViewCount += entry.descriptorCount;
    }
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
    {
      entrySize = sizeof(VkDescriptorImageInfo);

      imageInfoCount += entry.descriptorCount;
    }
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    {
      // a bit of magic handling. The calculation is stride * descriptorCount bytes for the data,
      // plus the size of the 'base' structure. For inline uniform blocks there's no base structure
      // and the data is in bytes, so stride 1.
      stride = 1;

      entrySize = 0;

      inlineInfoCount++;
      inlineByteSize += entry.descriptorCount;
      inlineByteSize = AlignUp4(inlineByteSize);
    }
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
    {
      entrySize = sizeof(VkAccelerationStructureKHR);

      accelerationStructureWriteCount++;
      accelerationStructureCount += entry.descriptorCount;
    }
    else
    {
      entrySize = sizeof(VkDescriptorBufferInfo);

      bufferInfoCount += entry.descriptorCount;
    }

    unwrapByteSize =
        RDCMAX(unwrapByteSize, entry.offset + stride * entry.descriptorCount + entrySize);
  }

  if(pCreateInfo->templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET)
  {
    if(IsCaptureMode(resourceMan->GetState()))
    {
      layout = *GetRecord(pCreateInfo->descriptorSetLayout)->descInfo->layout;
    }
    else
    {
      layout = info.m_DescSetLayout[GetResID(pCreateInfo->descriptorSetLayout)];
    }
  }
  else
  {
    if(IsCaptureMode(resourceMan->GetState()))
    {
      layout = GetRecord(pCreateInfo->pipelineLayout)->pipeLayoutInfo->layouts[pCreateInfo->set];
    }
    else
    {
      const rdcarray<ResourceId> &descSetLayouts =
          info.m_PipelineLayout[GetResID(pCreateInfo->pipelineLayout)].descSetLayouts;

      layout = info.m_DescSetLayout[descSetLayouts[pCreateInfo->set]];
    }
  }
}

void DescUpdateTemplate::Apply(const void *pData, DescUpdateTemplateApplication &application)
{
  application.bufView.reserve(texelBufferViewCount);
  application.bufInfo.reserve(bufferInfoCount);
  application.imgInfo.reserve(imageInfoCount);
  application.inlineData.resize(inlineByteSize);
  application.inlineUniform.reserve(inlineInfoCount);
  application.accelerationStructureWrite.reserve(accelerationStructureWriteCount);
  application.accelerationStructure.reserve(accelerationStructureCount);

  uint32_t inlineOffset = 0;
  for(const VkDescriptorUpdateTemplateEntry &entry : updates)
  {
    VkWriteDescriptorSet write = {};

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = VK_NULL_HANDLE;    // set externally for non-push descriptor template updates.
    write.dstBinding = entry.dstBinding;
    write.dstArrayElement = entry.dstArrayElement;
    write.descriptorType = entry.descriptorType;
    write.descriptorCount = entry.descriptorCount;

    const byte *src = (const byte *)pData + entry.offset;

    if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
       entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
    {
      size_t idx = application.bufView.size();

      application.bufView.resize(idx + entry.descriptorCount);

      for(uint32_t d = 0; d < entry.descriptorCount; d++)
      {
        memcpy(&application.bufView[idx + d], src, sizeof(VkBufferView));
        src += entry.stride;
      }

      write.pTexelBufferView = &application.bufView[idx];
    }
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
    {
      size_t idx = application.imgInfo.size();

      application.imgInfo.resize(idx + entry.descriptorCount);

      for(uint32_t d = 0; d < entry.descriptorCount; d++)
      {
        memcpy(&application.imgInfo[idx + d], src, sizeof(VkDescriptorImageInfo));
        src += entry.stride;
      }

      write.pImageInfo = &application.imgInfo[idx];
    }
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    {
      application.inlineUniform.push_back({});

      VkWriteDescriptorSetInlineUniformBlock &inlineWrite = application.inlineUniform.back();
      inlineWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK;
      inlineWrite.pNext = NULL;
      inlineWrite.dataSize = entry.descriptorCount;

      void *dst = application.inlineData.data() + inlineOffset;
      memcpy(dst, src, inlineWrite.dataSize);
      inlineWrite.pData = dst;

      write.pNext = &inlineWrite;
      write.descriptorCount = entry.descriptorCount;
    }
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
    {
      application.accelerationStructureWrite.push_back({});

      VkWriteDescriptorSetAccelerationStructureKHR &asWrite =
          application.accelerationStructureWrite.back();
      asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
      asWrite.pNext = NULL;
      asWrite.accelerationStructureCount = entry.descriptorCount;

      const size_t idx = application.accelerationStructure.size();
      application.accelerationStructure.resize(idx + entry.descriptorCount);
      for(uint32_t d = 0; d < entry.descriptorCount; d++)
      {
        memcpy(&application.accelerationStructure[idx + d], src, sizeof(VkAccelerationStructureKHR));
        src += entry.stride;
      }
      asWrite.pAccelerationStructures = &application.accelerationStructure[idx];

      write.pNext = &asWrite;
      write.descriptorCount = entry.descriptorCount;
    }
    else
    {
      size_t idx = application.bufInfo.size();

      application.bufInfo.resize(idx + entry.descriptorCount);

      for(uint32_t d = 0; d < entry.descriptorCount; d++)
      {
        memcpy(&application.bufInfo[idx + d], src, sizeof(VkDescriptorBufferInfo));
        src += entry.stride;
      }

      write.pBufferInfo = &application.bufInfo[idx];
    }

    if(write.descriptorCount != 0)
      application.writes.push_back(write);
  }
}
