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

#include "vk_common.h"
#include "vk_core.h"
#include "api/replay/renderdoc_replay.h"
#include "replay/replay_driver.h"
#include "core/core.h"

struct TextPrintState
{
	VkCmdBuffer cmd;
	VkRenderPass rp;
	VkFramebuffer fb;
	int32_t w, h;
};

class VulkanDebugManager
{
	public:
		VulkanDebugManager(WrappedVulkan *driver, VkDevice dev);
		~VulkanDebugManager();

		void RenderText(const TextPrintState &textstate, float x, float y, const char *fmt, ...);

		struct GPUBuffer
		{
			enum CreateFlags
			{
				eGPUBufferReadback = 0x1,
			};
			GPUBuffer() : buf(VK_NULL_HANDLE), mem(VK_NULL_HANDLE) {}
			void Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size, uint32_t flags);
			void Destroy(const VkLayerDispatchTable *vt, VkDevice dev);

			void FillDescriptor(VkDescriptorInfo &desc);

			void *Map(const VkLayerDispatchTable *vt, VkDevice dev, VkDeviceSize offset = 0, VkDeviceSize size = 0);
			void Unmap(const VkLayerDispatchTable *vt, VkDevice dev);

			VkDeviceSize sz;
			VkBuffer buf;
			VkDeviceMemory mem;
		};

		// VKTODOLOW make this all private/wrapped up
		VkDescriptorPool m_DescriptorPool;
		VkSampler m_LinearSampler, m_PointSampler;

		VkDescriptorSetLayout m_CheckerboardDescSetLayout;
		VkPipelineLayout m_CheckerboardPipeLayout;
		VkDescriptorSet m_CheckerboardDescSet;
		VkPipeline m_CheckerboardPipeline;
		GPUBuffer m_CheckerboardUBO;

		VkDescriptorSetLayout m_GenericDescSetLayout;
		VkPipelineLayout m_GenericPipeLayout;
		VkDescriptorSet m_GenericDescSet;
		VkPipeline m_GenericPipeline;
		GPUBuffer m_OutlineStripVBO;
		GPUBuffer m_GenericUBO;

		VkDescriptorSetLayout m_TexDisplayDescSetLayout;
		VkPipelineLayout m_TexDisplayPipeLayout;
		VkDescriptorSet m_TexDisplayDescSet;
		VkPipeline m_TexDisplayPipeline, m_TexDisplayBlendPipeline;
		GPUBuffer m_TexDisplayUBO;

		VkDeviceMemory m_PickPixelImageMem;
		VkImage m_PickPixelImage;
		VkImageView m_PickPixelImageView;
		GPUBuffer m_PickPixelReadbackBuffer;
		VkFramebuffer m_PickPixelFB;
		VkRenderPass m_PickPixelRP;
		
		VkDescriptorSetLayout m_TextDescSetLayout;
		VkPipelineLayout m_TextPipeLayout;
		VkDescriptorSet m_TextDescSet;
		VkPipeline m_TextPipeline;
		GPUBuffer m_TextGeneralUBO;
		GPUBuffer m_TextGlyphUBO;
		GPUBuffer m_TextStringUBO;
		VkImage m_TextAtlas;
		VkDeviceMemory m_TextAtlasMem;
		VkImageView m_TextAtlasView;

	private:
		void InitDebugData();
		void ShutdownDebugData();
		
		void RenderTextInternal(const TextPrintState &textstate, float x, float y, const char *text);
		static const int FONT_TEX_WIDTH = 256;
		static const int FONT_TEX_HEIGHT = 128;

		float m_FontCharAspect;
		float m_FontCharSize;

		
		WrappedVulkan *m_pDriver;

		VkDevice m_Device;
};
