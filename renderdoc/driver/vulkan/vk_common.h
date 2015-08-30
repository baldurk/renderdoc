/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

#include "serialise/serialiser.h"

#include "core/core.h"

#include <vulkan.h>
#include <vk_debug_marker_lunarg.h>
#include <vk_debug_report_lunarg.h>
#include <vk_wsi_swapchain.h>
#include <vk_wsi_device_swapchain.h>

// structure for casting to easily iterate and template specialising Serialise
struct VkGenericStruct
{
	VkStructureType type;
	const VkGenericStruct *pNext;
};

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) ret func; bool CONCAT(Serialise_, func);

template<> void Serialiser::Serialise(const char *name, VkRect2D &el);
template<> void Serialiser::Serialise(const char *name, VkRect3D &el);
template<> void Serialiser::Serialise(const char *name, VkDeviceQueueCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkDeviceCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkBufferCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkBufferViewCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkImageCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkImageViewCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkFramebufferCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkRenderPassCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkRenderPassBeginInfo &el);
template<> void Serialiser::Serialise(const char *name, VkAttachmentViewCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkDynamicViewportStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkDynamicRasterStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkDynamicColorBlendStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkDynamicDepthStencilStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPipelineInputAssemblyStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPipelineTessellationStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPipelineViewportStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPipelineRasterStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPipelineMultisampleStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPipelineDepthStencilStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPipelineColorBlendStateCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPipelineLayoutCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPushConstantRange &el);
template<> void Serialiser::Serialise(const char *name, VkDescriptorSetLayoutBinding &el);
template<> void Serialiser::Serialise(const char *name, VkDescriptorSetLayoutCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkDescriptorPoolCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkWriteDescriptorSet &el);
template<> void Serialiser::Serialise(const char *name, VkCopyDescriptorSet &el);
template<> void Serialiser::Serialise(const char *name, VkClearColorValue &el);
template<> void Serialiser::Serialise(const char *name, VkCmdPoolCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkCmdBufferCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkCmdBufferBeginInfo &el);
template<> void Serialiser::Serialise(const char *name, VkStencilOpState &el);
template<> void Serialiser::Serialise(const char *name, VkSamplerCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkPipelineCacheCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkShaderCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkShaderModuleCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkImageSubresourceRange &el);
template<> void Serialiser::Serialise(const char *name, VkImageSubresource &el);
template<> void Serialiser::Serialise(const char *name, VkMemoryAllocInfo &el);
template<> void Serialiser::Serialise(const char *name, VkMemoryBarrier &el);
template<> void Serialiser::Serialise(const char *name, VkBufferMemoryBarrier &el);
template<> void Serialiser::Serialise(const char *name, VkImageMemoryBarrier &el);
template<> void Serialiser::Serialise(const char *name, VkGraphicsPipelineCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkComputePipelineCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkChannelMapping &el);
template<> void Serialiser::Serialise(const char *name, VkComputePipelineCreateInfo &el);
template<> void Serialiser::Serialise(const char *name, VkBufferImageCopy &el);
template<> void Serialiser::Serialise(const char *name, VkBufferCopy &el);
template<> void Serialiser::Serialise(const char *name, VkImageCopy &el);
template<> void Serialiser::Serialise(const char *name, VkImageBlit &el);
template<> void Serialiser::Serialise(const char *name, VkImageResolve &el);

template<> void Serialiser::Serialise(const char *name, VkSwapChainCreateInfoWSI &el);

#pragma region Chunks

enum VulkanChunkType
{
	DEVICE_INIT = FIRST_CHUNK_ID,
	CREATE_INSTANCE,
	ENUM_PHYSICALS,
	CREATE_DEVICE,
	GET_DEVICE_QUEUE,

	ALLOC_MEM,
	UNMAP_MEM,
	FREE_MEM,
	
	CREATE_CMD_POOL,
	RESET_CMD_POOL,

	CREATE_CMD_BUFFER,
	CREATE_FRAMEBUFFER,
	CREATE_RENDERPASS,
	CREATE_DESCRIPTOR_POOL,
	CREATE_DESCRIPTOR_SET,
	CREATE_DESCRIPTOR_SET_LAYOUT,
	CREATE_BUFFER,
	CREATE_BUFFER_VIEW,
	CREATE_IMAGE,
	CREATE_IMAGE_VIEW,
	CREATE_ATTACHMENT_VIEW,
	CREATE_DEPTH_TARGET_VIEW,
	CREATE_VIEWPORT_STATE,
	CREATE_RASTER_STATE,
	CREATE_BLEND_STATE,
	CREATE_DEPTH_STATE,
	CREATE_SAMPLER,
	CREATE_SHADER,
	CREATE_SHADER_MODULE,
	CREATE_PIPE_LAYOUT,
	CREATE_PIPE_CACHE,
	CREATE_GRAPHICS_PIPE,
	CREATE_COMPUTE_PIPE,
	PRESENT_IMAGE,

	CREATE_FENCE,
	GET_FENCE_STATUS,
	WAIT_FENCES,

	ALLOC_DESC_SET,
	UPDATE_DESC_SET,

	RESET_CMD_BUFFER,
	BEGIN_CMD_BUFFER,
	END_CMD_BUFFER,

	QUEUE_WAIT_IDLE,
	DEVICE_WAIT_IDLE,

	QUEUE_SUBMIT,
	BIND_BUFFER_MEM,
	BIND_IMAGE_MEM,

	BEGIN_RENDERPASS,
	END_RENDERPASS,

	BIND_PIPELINE,
	BIND_VP_STATE,
	BIND_RS_STATE,
	BIND_CB_STATE,
	BIND_DS_STATE,
	BIND_DESCRIPTOR_SET,
	BIND_INDEX_BUFFER,
	COPY_BUF2IMG,
	COPY_IMG2BUF,
	COPY_BUF,
	COPY_IMG,
	BLIT_IMG,
	CLEAR_COLOR,
	CLEAR_DEPTHSTENCIL,
	CLEAR_COLOR_ATTACH,
	CLEAR_DEPTHSTENCIL_ATTACH,
	PIPELINE_BARRIER,
	RESOLVE_IMAGE,
	WRITE_TIMESTAMP,
	DRAW,
	DRAW_INDIRECT,
	DRAW_INDEXED,
	DRAW_INDEXED_INDIRECT,
	DISPATCH,
	DISPATCH_INDIRECT,

	BEGIN_EVENT,
	SET_MARKER,
	END_EVENT,

	CREATE_SWAP_BUFFER,

	CAPTURE_SCOPE,
	CONTEXT_CAPTURE_HEADER,
	CONTEXT_CAPTURE_FOOTER,

	NUM_VULKAN_CHUNKS,
};

#pragma endregion Chunks
