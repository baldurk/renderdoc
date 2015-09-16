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
#include "common/wrapped_pool.h"

#include "vk_common.h"

struct VkResourceRecord;

struct WrappedVkRes
{
	WrappedVkRes() : real(0), id(), record(NULL) {}

	// constructor for non-dispatchable types
	template<typename T> WrappedVkRes(T obj, ResourceId objId) : real(obj.handle), id(objId), record(NULL) {}
	// constructors for dispatchable types
	WrappedVkRes(VkInstance obj, ResourceId objId) : real((uint64_t)(uintptr_t)obj), id(objId), record(NULL) {}
	WrappedVkRes(VkPhysicalDevice obj, ResourceId objId) : real((uint64_t)(uintptr_t)obj), id(objId), record(NULL) {}
	WrappedVkRes(VkDevice obj, ResourceId objId) : real((uint64_t)(uintptr_t)obj), id(objId), record(NULL) {}
	WrappedVkRes(VkQueue obj, ResourceId objId) : real((uint64_t)(uintptr_t)obj), id(objId), record(NULL) {}
	WrappedVkRes(VkCmdBuffer obj, ResourceId objId) : real((uint64_t)(uintptr_t)obj), id(objId), record(NULL) {}

	uint64_t real;
	ResourceId id;
	union
	{
		VkResourceRecord *record;
		// Do we need something here for per-object replay information?
		void *replayInformation;
	};
	// dispatch table pointer could be here, at the cost of an extra pointer
};

// ensure the struct doesn't accidentally get made larger
RDCCOMPILE_ASSERT(sizeof(WrappedVkRes) == sizeof(uint64_t)*3, "VkWrappedRes has changed size! This is bad");

// these are expanded out so that IDE autocompletion etc works without having to process through macros
struct WrappedVkInstance : WrappedVkRes
{
	WrappedVkInstance(VkInstance obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkInstance InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkInstance);
};
struct WrappedVkPhysicalDevice : WrappedVkRes
{
	WrappedVkPhysicalDevice(VkPhysicalDevice obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkPhysicalDevice InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPhysicalDevice);
};
struct WrappedVkDevice : WrappedVkRes
{
	WrappedVkDevice(VkDevice obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkDevice InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDevice);
};
struct WrappedVkQueue : WrappedVkRes
{
	WrappedVkQueue(VkQueue obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkQueue InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkQueue);
};
struct WrappedVkCmdBuffer : WrappedVkRes
{
	WrappedVkCmdBuffer(VkCmdBuffer obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkCmdBuffer InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkCmdBuffer);
};
struct WrappedVkFence : WrappedVkRes
{
	WrappedVkFence(VkFence obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkFence InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkFence);
};
struct WrappedVkDeviceMemory : WrappedVkRes
{
	WrappedVkDeviceMemory(VkDeviceMemory obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkDeviceMemory InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDeviceMemory);
};
struct WrappedVkBuffer : WrappedVkRes
{
	WrappedVkBuffer(VkBuffer obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkBuffer InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkBuffer);
};
struct WrappedVkImage : WrappedVkRes
{
	WrappedVkImage(VkImage obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkImage InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkImage);
};
struct WrappedVkSemaphore : WrappedVkRes
{
	WrappedVkSemaphore(VkSemaphore obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkSemaphore InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSemaphore);
};
struct WrappedVkEvent : WrappedVkRes
{
	WrappedVkEvent(VkEvent obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkEvent InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkEvent);
};
struct WrappedVkQueryPool : WrappedVkRes
{
	WrappedVkQueryPool(VkQueryPool obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkQueryPool InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkQueryPool);
};
struct WrappedVkBufferView : WrappedVkRes
{
	WrappedVkBufferView(VkBufferView obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkBufferView InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkBufferView);
};
struct WrappedVkImageView : WrappedVkRes
{
	WrappedVkImageView(VkImageView obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkImageView InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkImageView);
};
struct WrappedVkAttachmentView : WrappedVkRes
{
	WrappedVkAttachmentView(VkAttachmentView obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkAttachmentView InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkAttachmentView);
};
struct WrappedVkShaderModule : WrappedVkRes
{
	WrappedVkShaderModule(VkShaderModule obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkShaderModule InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkShaderModule);
};
struct WrappedVkShader : WrappedVkRes
{
	WrappedVkShader(VkShader obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkShader InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkShader);
};
struct WrappedVkPipelineCache : WrappedVkRes
{
	WrappedVkPipelineCache(VkPipelineCache obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkPipelineCache InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipelineCache);
};
struct WrappedVkPipelineLayout : WrappedVkRes
{
	WrappedVkPipelineLayout(VkPipelineLayout obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkPipelineLayout InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipelineLayout);
};
struct WrappedVkRenderPass : WrappedVkRes
{
	WrappedVkRenderPass(VkRenderPass obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkRenderPass InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkRenderPass);
};
struct WrappedVkPipeline : WrappedVkRes
{
	WrappedVkPipeline(VkPipeline obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkPipeline InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipeline);
};
struct WrappedVkDescriptorSetLayout : WrappedVkRes
{
	WrappedVkDescriptorSetLayout(VkDescriptorSetLayout obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkDescriptorSetLayout InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorSetLayout);
};
struct WrappedVkSampler : WrappedVkRes
{
	WrappedVkSampler(VkSampler obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkSampler InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSampler);
};
struct WrappedVkDescriptorPool : WrappedVkRes
{
	WrappedVkDescriptorPool(VkDescriptorPool obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkDescriptorPool InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorPool);
};
struct WrappedVkDescriptorSet : WrappedVkRes
{
	WrappedVkDescriptorSet(VkDescriptorSet obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkDescriptorSet InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorSet);
};
struct WrappedVkDynamicViewportState : WrappedVkRes
{
	WrappedVkDynamicViewportState(VkDynamicViewportState obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkDynamicViewportState InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDynamicViewportState);
};
struct WrappedVkDynamicRasterState : WrappedVkRes
{
	WrappedVkDynamicRasterState(VkDynamicRasterState obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkDynamicRasterState InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDynamicRasterState);
};
struct WrappedVkDynamicColorBlendState : WrappedVkRes
{
	WrappedVkDynamicColorBlendState(VkDynamicColorBlendState obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkDynamicColorBlendState InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDynamicColorBlendState);
};
struct WrappedVkDynamicDepthStencilState : WrappedVkRes
{
	WrappedVkDynamicDepthStencilState(VkDynamicDepthStencilState obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkDynamicDepthStencilState InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDynamicDepthStencilState);
};
struct WrappedVkFramebuffer : WrappedVkRes
{
	WrappedVkFramebuffer(VkFramebuffer obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkFramebuffer InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkFramebuffer);
};
struct WrappedVkCmdPool : WrappedVkRes
{
	WrappedVkCmdPool(VkCmdPool obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkCmdPool InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkCmdPool);
};
struct WrappedVkSwapChainWSI : WrappedVkRes
{
	WrappedVkSwapChainWSI(VkSwapChainWSI obj, ResourceId objId) : WrappedVkRes(obj, objId) {}
	typedef VkSwapChainWSI InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSwapChainWSI);
};

// template magic voodoo to unwrap types
template<typename inner> struct UnwrapHelper {};

#define UNWRAP_HELPER(vulkantype) \
	template<> struct UnwrapHelper<vulkantype> \
	{ \
		typedef CONCAT(Wrapped, vulkantype) Outer; \
		static vulkantype ToHandle(uint64_t real) { return (vulkantype) (uintptr_t)real; } \
		static Outer *FromHandle(vulkantype wrapped) { return (Outer *) wrapped; } \
	};

#define UNWRAP_NONDISP_HELPER(vulkantype) \
	template<> struct UnwrapHelper<vulkantype> \
	{ \
		typedef CONCAT(Wrapped, vulkantype) Outer; \
		static vulkantype ToHandle(uint64_t real) { return vulkantype(real); } \
		static Outer *FromHandle(vulkantype wrapped) { return (Outer *) (uintptr_t)wrapped.handle; } \
	};

UNWRAP_HELPER(VkInstance)
UNWRAP_HELPER(VkPhysicalDevice)
UNWRAP_HELPER(VkDevice)
UNWRAP_HELPER(VkQueue)
UNWRAP_HELPER(VkCmdBuffer)
UNWRAP_NONDISP_HELPER(VkFence)
UNWRAP_NONDISP_HELPER(VkDeviceMemory)
UNWRAP_NONDISP_HELPER(VkBuffer)
UNWRAP_NONDISP_HELPER(VkImage)
UNWRAP_NONDISP_HELPER(VkSemaphore)
UNWRAP_NONDISP_HELPER(VkEvent)
UNWRAP_NONDISP_HELPER(VkQueryPool)
UNWRAP_NONDISP_HELPER(VkBufferView)
UNWRAP_NONDISP_HELPER(VkImageView)
UNWRAP_NONDISP_HELPER(VkAttachmentView)
UNWRAP_NONDISP_HELPER(VkShaderModule)
UNWRAP_NONDISP_HELPER(VkShader)
UNWRAP_NONDISP_HELPER(VkPipelineCache)
UNWRAP_NONDISP_HELPER(VkPipelineLayout)
UNWRAP_NONDISP_HELPER(VkRenderPass)
UNWRAP_NONDISP_HELPER(VkPipeline)
UNWRAP_NONDISP_HELPER(VkDescriptorSetLayout)
UNWRAP_NONDISP_HELPER(VkSampler)
UNWRAP_NONDISP_HELPER(VkDescriptorPool)
UNWRAP_NONDISP_HELPER(VkDescriptorSet)
UNWRAP_NONDISP_HELPER(VkDynamicViewportState)
UNWRAP_NONDISP_HELPER(VkDynamicRasterState)
UNWRAP_NONDISP_HELPER(VkDynamicColorBlendState)
UNWRAP_NONDISP_HELPER(VkDynamicDepthStencilState)
UNWRAP_NONDISP_HELPER(VkFramebuffer)
UNWRAP_NONDISP_HELPER(VkCmdPool)
UNWRAP_NONDISP_HELPER(VkSwapChainWSI)

#define WRAPPING_DEBUG 0

template<typename RealType>
typename UnwrapHelper<RealType>::Outer *GetWrapped(typename RealType obj)
{
	if(obj == VK_NULL_HANDLE) return VK_NULL_HANDLE;

	typename UnwrapHelper<RealType>::Outer *wrapped = UnwrapHelper<RealType>::FromHandle(obj);

#if WRAPPING_DEBUG
	if(obj != VK_NULL_HANDLE && !wrapped->IsAlloc(wrapped))
	{
		RDCERR("Trying to unwrap invalid type");
		return NULL;
	}
#endif

	return wrapped;
}

template<typename RealType>
typename RealType Unwrap(typename RealType obj)
{
	if(obj == VK_NULL_HANDLE) return VK_NULL_HANDLE;

	return UnwrapHelper<RealType>::ToHandle(GetWrapped(obj)->real);
}

template<typename RealType>
typename ResourceId GetResID(typename RealType obj)
{
	if(obj == VK_NULL_HANDLE) return ResourceId();

	return GetWrapped(obj)->id;
}

template<typename RealType>
typename VkResourceRecord *GetRecord(typename RealType obj)
{
	if(obj == VK_NULL_HANDLE) return NULL;

	return GetWrapped(obj)->record;
}

enum VkNamespace
{
	eResUnknown = 0,
	eResSpecial,
	eResPhysicalDevice,
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
	eResShaderModule,
	eResShader,
	eResPipelineCache,
	eResPipelineLayout,
	eResPipeline,
	eResSampler,
	eResDescriptorPool,
	eResDescriptorSetLayout,
	eResDescriptorSet,
	eResViewportState,
	eResRasterState,
	eResColorBlendState,
	eResDepthStencilState,
	eResCmdPool,
	eResCmdBuffer,
	eResCmdBufferBake,
	eResFence,
	eResSemaphore,
	
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

inline VkResource MakeRes(VkPhysicalDevice o) { return VkResource(eResPhysicalDevice, (uint64_t)o); }
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
			boundDescSets.swap(bakedCommands->boundDescSets);
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

		void AddBindFrameRef(ResourceId id, FrameRefType ref)
		{
			if(id == ResourceId())
			{
				RDCERR("Unexpected NULL resource ID being added as a bind frame ref");
				return;
			}

			if(bindFrameRefs[id].first == 0)
			{
				bindFrameRefs[id] = std::make_pair(1, ref);
			}
			else
			{
				// be conservative - mark refs as read before write if we see a write and a read ref on it
				if(ref == eFrameRef_Write && bindFrameRefs[id].second == eFrameRef_Read)
					bindFrameRefs[id].second = eFrameRef_ReadBeforeWrite;
				bindFrameRefs[id].first++;
			}
		}

		void RemoveBindFrameRef(ResourceId id)
		{
			// ignore any NULL IDs - probably an object that was
			// deleted since it was bound.
			if(id == ResourceId()) return;

			if(--bindFrameRefs[id].first == 0)
				bindFrameRefs.erase(id);
		}

		VkResourceRecord *bakedCommands;

		// a list of resources that are made dirty by submitting this command buffer
		set<ResourceId> dirtied;

		// a list of descriptor sets that are bound at any point in this command buffer
		// used to look up all the frame refs per-desc set and apply them on queue
		// submit with latest binding refs.
		set<ResourceId> boundDescSets;

		// descriptor set bindings for this descriptor set. Filled out on
		// create from the layout.
		ResourceId layout;
		vector<VkDescriptorInfo *> descBindings;

		// contains the framerefs (ref counted) for the bound resources
		// in the binding slots. Updated when updating descriptor sets
		// and then applied in a block on descriptor set bind.
		map<ResourceId, pair<int, FrameRefType> > bindFrameRefs;

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

bool IsBlockFormat(VkFormat f);
bool IsDepthStencilFormat(VkFormat f);
