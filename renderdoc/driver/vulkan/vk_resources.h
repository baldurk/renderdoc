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

#include "core/resource_manager.h"

#include "vk_common.h"

enum VkNamespace
{
	eResUnknown = 0,
	eResSpecial,
	eResGPU,
	eResInstance,
	eResDevice,
	eResQueue,
	eResDeviceMemory,
	eResBuffer,
	eResBufferView,
	eResImage,
	eResImageView,
	eResAttachmentView,
	eResFramebuffer,
	eResRenderPass,
	eResShader,
	eResShaderModule,
	eResPipeline,
	eResPipelineCache,
	eResPipelineLayout,
	eResSampler,
	eResDescriptorSet,
	eResDescriptorPool,
	eResDescriptorSetLayout,
	eResViewportState,
	eResRasterState,
	eResMSAAState,
	eResColorBlendState,
	eResDepthStencilState,
	eResCmdPool,
	eResCmdBuffer,
	eResCmdBufferBake,
	eResFence,
	eResSemaphore,
	eResEvent,
	eResWaitEvent,
	eResQueryPool,
	
	eResWSISwapChain,
};

enum NullInitialiser { MakeNullResource };

struct VkResource
{
	VkResource() { Namespace = eResUnknown; handle = VK_NULL_HANDLE; }
	VkResource(NullInitialiser) { Namespace = eResUnknown; handle = VK_NULL_HANDLE; }
	VkResource(VkNamespace n, uint64_t o) { Namespace = n; handle = o; }
	VkNamespace Namespace;
	uint64_t handle;

	bool operator ==(const VkResource &o) const
	{
		return Namespace == o.Namespace && handle == o.handle;
	}

	bool operator !=(const VkResource &o) const
	{
		return !(*this == o);
	}

	bool operator <(const VkResource &o) const
	{
		if(Namespace != o.Namespace) return Namespace < o.Namespace;
		return handle < o.handle;
	}
};

inline VkResource MakeRes(VkPhysicalDevice o) { return VkResource(eResGPU, (uint64_t)o); }
inline VkResource MakeRes(VkInstance o) { return VkResource(eResInstance, (uint64_t)o); }
inline VkResource MakeRes(VkDevice o) { return VkResource(eResDevice, (uint64_t)o); }
inline VkResource MakeRes(VkQueue o) { return VkResource(eResQueue, (uint64_t)o); }
inline VkResource MakeRes(VkCmdBuffer o) { return VkResource(eResCmdBuffer, (uint64_t)o); }
inline VkResource MakeRes(VkCmdPool o) { return VkResource(eResCmdPool, o.handle); }
inline VkResource MakeRes(VkDeviceMemory o) { return VkResource(eResDeviceMemory, o.handle); }
inline VkResource MakeRes(VkBuffer o) { return VkResource(eResBuffer, o.handle); }
inline VkResource MakeRes(VkBufferView o) { return VkResource(eResBufferView, o.handle); }
inline VkResource MakeRes(VkImage o) { return VkResource(eResImage, o.handle); }
inline VkResource MakeRes(VkImageView o) { return VkResource(eResImageView, o.handle); }
inline VkResource MakeRes(VkAttachmentView o) { return VkResource(eResAttachmentView, o.handle); }
inline VkResource MakeRes(VkFramebuffer o) { return VkResource(eResFramebuffer, o.handle); }
inline VkResource MakeRes(VkRenderPass o) { return VkResource(eResRenderPass, o.handle); }
inline VkResource MakeRes(VkShader o) { return VkResource(eResShader, o.handle); }
inline VkResource MakeRes(VkShaderModule o) { return VkResource(eResShaderModule, o.handle); }
inline VkResource MakeRes(VkPipeline o) { return VkResource(eResPipeline, o.handle); }
inline VkResource MakeRes(VkPipelineCache o) { return VkResource(eResPipelineCache, o.handle); }
inline VkResource MakeRes(VkPipelineLayout o) { return VkResource(eResPipelineLayout, o.handle); }
inline VkResource MakeRes(VkSampler o) { return VkResource(eResSampler, o.handle); }
inline VkResource MakeRes(VkDescriptorSet o) { return VkResource(eResDescriptorSet, o.handle); }
inline VkResource MakeRes(VkDescriptorPool o) { return VkResource(eResDescriptorPool, o.handle); }
inline VkResource MakeRes(VkDescriptorSetLayout o) { return VkResource(eResDescriptorSetLayout, o.handle); }
inline VkResource MakeRes(VkDynamicViewportState o) { return VkResource(eResViewportState, o.handle); }
inline VkResource MakeRes(VkDynamicRasterState o) { return VkResource(eResRasterState, o.handle); }
inline VkResource MakeRes(VkDynamicColorBlendState o) { return VkResource(eResColorBlendState, o.handle); }
inline VkResource MakeRes(VkDynamicDepthStencilState o) { return VkResource(eResDepthStencilState, o.handle); }
inline VkResource MakeRes(VkFence o) { return VkResource(eResFence, o.handle); }
inline VkResource MakeRes(VkSemaphore o) { return VkResource(eResSemaphore, o.handle); }
inline VkResource MakeRes(VkEvent o) { return VkResource(eResEvent, o.handle); }
inline VkResource MakeRes(VkQueryPool o) { return VkResource(eResQueryPool, o.handle); }

inline VkResource MakeRes(VkSwapChainWSI o) { return VkResource(eResWSISwapChain, o.handle); }

#define UNTRANSITIONED_IMG_STATE ((VkImageLayout)0xffffffff)

struct ImageRegionState
{
	ImageRegionState()
		: prevstate(UNTRANSITIONED_IMG_STATE), state(UNTRANSITIONED_IMG_STATE)
	{
		range.aspect = VK_IMAGE_ASPECT_COLOR;
		range.baseArraySlice = 0; range.arraySize = 0;
		range.baseMipLevel = 0; range.mipLevels = 0;
	}
	ImageRegionState(VkImageSubresourceRange r, VkImageLayout pr, VkImageLayout st)
	    : range(r), prevstate(pr), state(st) {}

	VkImageSubresourceRange range;
	VkImageLayout prevstate;
	VkImageLayout state;
};

struct VkResourceRecord : public ResourceRecord
{
	public:
		static const NullInitialiser NullResource = MakeNullResource;

		VkResourceRecord(ResourceId id) :
			ResourceRecord(id, true),
			bakedCommands(NULL),
			memory(NULL)
		{
		}

		~VkResourceRecord()
		{
			for(size_t i=0; i < descBindings.size(); i++)
				delete[] descBindings[i];
			descBindings.clear();
		}

		void Bake()
		{
			SwapChunks(bakedCommands);
			dirtied.swap(bakedCommands->dirtied);
		}

		// need to only track current memory binding,
		// so we don't have parents on every memory record
		// that we were ever bound to. But we also want
		// the record to be immediately in Parents without
		// needing an extra step to insert it at the last
		// minute.
		void SetMemoryRecord(VkResourceRecord *r)
		{
			if(memory != NULL)
				Parents.erase((ResourceRecord *)memory);

			memory = r;

			if(memory != NULL)
				AddParent(memory);
		}

		VkResourceRecord *GetMemoryRecord()
		{
			return memory;
		}

		VkResourceRecord *bakedCommands;

		// a list of resources that are made dirty by submitting this command buffer
		set<ResourceId> dirtied;

		// descriptor set bindings for this descriptor set. Filled out on
		// create from the layout.
		ResourceId layout;
		vector<VkDescriptorInfo *> descBindings;

	private:
		VkResourceRecord *memory;
};

struct MemState
{
	MemState()
		: mapOffset(0), mapSize(0), size(0), mapFlags(0), mappedPtr(0)
	{ }
	VkDeviceSize mapOffset, mapSize;
	VkDeviceSize size;
	VkMemoryMapFlags mapFlags;
	void *mappedPtr;
};
struct ImgState
{
	ImgState()
		: mem(VK_NULL_HANDLE), arraySize(0), mipLevels(0)
	{
		type = VK_IMAGE_TYPE_MAX_ENUM;
		format = VK_FORMAT_UNDEFINED;
		extent.width = extent.height = extent.depth = 0;
	}

	VkDeviceMemory mem;
	vector<ImageRegionState> subresourceStates;

	VkImageType type;
	VkFormat format;
	VkExtent3D extent;
	int arraySize, mipLevels;
};

enum DescriptorSlotType
{
	DescSetSlot_None = 0,
	DescSetSlot_Sampler,
	DescSetSlot_Image,
	DescSetSlot_Memory,
	DescSetSlot_DescSet,
};

bool IsBlockFormat(VkFormat f);
bool IsDepthStencilFormat(VkFormat f);
