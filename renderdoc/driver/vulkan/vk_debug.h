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

class VulkanDebugManager
{
	public:
		VulkanDebugManager(WrappedVulkan *driver, VkDevice dev, VkImageView fakeBBView);
		~VulkanDebugManager();
		
		struct UBO
		{
			UBO() : buf(VK_NULL_HANDLE), mem(VK_NULL_HANDLE), view(VK_NULL_HANDLE) {}
			void Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size);
			void Destroy(const VkLayerDispatchTable *vt, VkDevice dev);

			void *Map(const VkLayerDispatchTable *vt, VkDevice dev, VkDeviceSize offset = 0, VkDeviceSize size = 0);
			void Unmap(const VkLayerDispatchTable *vt, VkDevice dev);

			VkBuffer buf;
			VkDeviceMemory mem;
			VkBufferView view;
		};

		// VKTODOLOW make this all private/wrapped up
		VkPipelineCache m_PipelineCache;
		VkDescriptorPool m_DescriptorPool;
		VkDynamicColorBlendState m_DynamicCBStateWhite;
		VkDynamicRasterState m_DynamicRSState;
		VkDynamicDepthStencilState m_DynamicDSStateDisabled;
		VkSampler m_LinearSampler, m_PointSampler;

		VkImageView m_FakeBBImView;

		VkDescriptorSetLayout m_CheckerboardDescSetLayout;
		VkPipelineLayout m_CheckerboardPipeLayout;
		VkDescriptorSet m_CheckerboardDescSet;
		VkPipeline m_CheckerboardPipeline;
		UBO m_CheckerboardUBO;

		VkDescriptorSetLayout m_TexDisplayDescSetLayout;
		VkPipelineLayout m_TexDisplayPipeLayout;
		VkDescriptorSet m_TexDisplayDescSet;
		VkPipeline m_TexDisplayPipeline, m_TexDisplayBlendPipeline;
		UBO m_TexDisplayUBO;

	private:
		void InitDebugData();
		void ShutdownDebugData();
		
		WrappedVulkan *m_pDriver;

		VkDevice m_Device;
};