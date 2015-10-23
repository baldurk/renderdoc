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

class VulkanResourceManager;

class VulkanDebugManager
{
	public:
		VulkanDebugManager(WrappedVulkan *driver, VkDevice dev);
		~VulkanDebugManager();

		void BeginText(const TextPrintState &textstate);
		void RenderText(const TextPrintState &textstate, float x, float y, const char *fmt, ...);
		void EndText(const TextPrintState &textstate);

		ResourceId RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents);

		struct GPUBuffer
		{
			enum CreateFlags
			{
				eGPUBufferReadback = 0x1,
			};
			GPUBuffer() : buf(VK_NULL_HANDLE), mem(VK_NULL_HANDLE) {}
			void Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size, uint32_t ringSize, uint32_t flags);
			void Destroy(const VkLayerDispatchTable *vt, VkDevice dev);

			void FillDescriptor(VkDescriptorInfo &desc);

			void *Map(const VkLayerDispatchTable *vt, VkDevice dev, uint32_t *bindoffset, VkDeviceSize usedsize = 0);
			void Unmap(const VkLayerDispatchTable *vt, VkDevice dev);

			VkDeviceSize sz;
			VkBuffer buf;
			VkDeviceMemory mem;

			// for handling ring allocations
			VkDeviceSize totalsize;
			VkDeviceSize curoffset;
			
			VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
			VulkanResourceManager *m_ResourceManager;
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
		VkDescriptorSet m_TexDisplayDescSet[16]; // ring buffered to allow multiple texture renders between flushes
		uint32_t m_TexDisplayNextSet;
		VkPipeline m_TexDisplayPipeline, m_TexDisplayBlendPipeline, m_TexDisplayF32Pipeline;
		GPUBuffer m_TexDisplayUBO;

		VkDescriptorSet GetTexDisplayDescSet()
		{
			m_TexDisplayNextSet = (m_TexDisplayNextSet+1)%ARRAY_COUNT(m_TexDisplayDescSet);
			return m_TexDisplayDescSet[m_TexDisplayNextSet];
		}

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
		
		VkDeviceMemory m_OverlayImageMem;
		VkImage m_OverlayImage;
		VkImageView m_OverlayImageView;
		VkFramebuffer m_OverlayNoDepthFB;
		VkRenderPass m_OverlayNoDepthRP;
		VkExtent2D m_OverlayDim;
		VkDeviceSize m_OverlayMemSize;

	private:
		void InitDebugData();
		void ShutdownDebugData();

		VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
		
		void PatchFixedColShader(VkShaderModule &mod, VkShader &shad, float col[4]);
		
		void RenderTextInternal(const TextPrintState &textstate, float x, float y, const char *text);
		void MakeGraphicsPipelineInfo( VkGraphicsPipelineCreateInfo &pipeCreateInfo, ResourceId pipeline );
		static const int FONT_TEX_WIDTH = 256;
		static const int FONT_TEX_HEIGHT = 128;

		float m_FontCharAspect;
		float m_FontCharSize;
		
		WrappedVulkan *m_pDriver;
		VulkanResourceManager *m_ResourceManager;

		VkDevice m_Device;
};
