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

// VKTODOLOW move layer dispatch table stuff to vk_common.h
struct VkLayerDispatchTable_;
struct VkLayerInstanceDispatchTable_;
VkLayerDispatchTable_ *device_dispatch_table(void* object);
VkLayerInstanceDispatchTable_ *instance_dispatch_table(void* object);

// empty base class for dispatchable/non-dispatchable. Unfortunately
// we can't put any members here as the base class is always first,
// and the dispatch tables that need to be first are the only difference
// between the two structs
struct WrappedVkRes
{
};

// dummy standin for a typeless real resource
// stored in a uint64_t, with function to cast back
// if we know what type it is
struct RealVkRes
{
	RealVkRes() : handle(VK_NULL_HANDLE) {} 
	RealVkRes(void *disp) : handle((uint64_t)disp) {}
	RealVkRes(uint64_t nondisp) : handle(nondisp) {}

	bool operator ==(const RealVkRes o) const { return handle == o.handle; }
	bool operator !=(const RealVkRes o) const { return handle != o.handle; }
	bool operator < (const RealVkRes o) const { return handle <  o.handle; }

	uint64_t handle;
	template<typename T> T As() { return (T)handle; }
};

struct WrappedVkNonDispRes : public WrappedVkRes
{
	template<typename T> WrappedVkNonDispRes(T obj, ResourceId objId) : real(obj.handle), id(objId), record(NULL) {}
	
	RealVkRes real;
	ResourceId id;
	union
	{
		VkResourceRecord *record;
		// Do we need something here for per-object replay information?
		void *replayInformation;
	};
};

struct WrappedVkDispRes : public WrappedVkRes
{
	WrappedVkDispRes(VkInstance obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL)
	{ loaderTable = *(uintptr_t*)obj; }

	WrappedVkDispRes(VkPhysicalDevice obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL)
	{ loaderTable = *(uintptr_t*)obj; }
	
	WrappedVkDispRes(VkDevice obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL)
	{ loaderTable = *(uintptr_t*)obj; }

	WrappedVkDispRes(VkQueue obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL)
	{ loaderTable = *(uintptr_t*)obj; }

	WrappedVkDispRes(VkCmdBuffer obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL)
	{ loaderTable = *(uintptr_t*)obj; }

	// VKTODOLOW there's padding here on 32-bit but I don't know if I really care about 32-bit.

	// preserve dispatch table pointer in dispatchable objects
	uintptr_t loaderTable, table;
	RealVkRes real;
	ResourceId id;
	union
	{
		VkResourceRecord *record;
		// Do we need something here for per-object replay information?
		void *replayInformation;
	};
};

// ensure the structs don't accidentally get made larger
RDCCOMPILE_ASSERT(sizeof(WrappedVkDispRes) == (sizeof(uint64_t)*3 + sizeof(uintptr_t)*2), "Wrapped resource struct has changed size! This is bad");
RDCCOMPILE_ASSERT(sizeof(WrappedVkNonDispRes) == sizeof(uint64_t)*3, "Wrapped resource struct has changed size! This is bad");

// VKTODOLOW check that the pool counts approximated below are good for typical applications

// these are expanded out so that IDE autocompletion etc works without having to process through macros
struct WrappedVkInstance : WrappedVkDispRes
{
	WrappedVkInstance(VkInstance obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
	typedef VkInstance InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkInstance);
	typedef VkLayerInstanceDispatchTable_ DispatchTableType;
	enum { UseInstanceDispatchTable = true, };
};
struct WrappedVkPhysicalDevice : WrappedVkDispRes
{
	WrappedVkPhysicalDevice(VkPhysicalDevice obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
	typedef VkPhysicalDevice InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPhysicalDevice);
	typedef VkLayerInstanceDispatchTable_ DispatchTableType;
	enum { UseInstanceDispatchTable = true, };
};
struct WrappedVkDevice : WrappedVkDispRes
{
	WrappedVkDevice(VkDevice obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
	typedef VkDevice InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDevice);
	typedef VkLayerDispatchTable_ DispatchTableType;
	enum { UseInstanceDispatchTable = false, };
};
struct WrappedVkQueue : WrappedVkDispRes
{
	WrappedVkQueue(VkQueue obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
	typedef VkQueue InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkQueue);
	typedef VkLayerDispatchTable_ DispatchTableType;
	enum { UseInstanceDispatchTable = false, };
};
struct WrappedVkCmdBuffer : WrappedVkDispRes
{
	WrappedVkCmdBuffer(VkCmdBuffer obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
	typedef VkCmdBuffer InnerType;
	static const int AllocPoolCount = 32*1024;
	static const int AllocPoolMaxByteSize = 2*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkCmdBuffer, AllocPoolCount, AllocPoolMaxByteSize);
	typedef VkLayerDispatchTable_ DispatchTableType;
	enum { UseInstanceDispatchTable = false, };
};
struct WrappedVkFence : WrappedVkNonDispRes
{
	WrappedVkFence(VkFence obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkFence InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkFence);
};
struct WrappedVkDeviceMemory : WrappedVkNonDispRes
{
	WrappedVkDeviceMemory(VkDeviceMemory obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDeviceMemory InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDeviceMemory, AllocPoolCount, AllocPoolMaxByteSize);
};
struct WrappedVkBuffer : WrappedVkNonDispRes
{
	WrappedVkBuffer(VkBuffer obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkBuffer InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkBuffer, AllocPoolCount, AllocPoolMaxByteSize);
};
struct WrappedVkImage : WrappedVkNonDispRes
{
	WrappedVkImage(VkImage obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkImage InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkImage, AllocPoolCount, AllocPoolMaxByteSize);
};
struct WrappedVkSemaphore : WrappedVkNonDispRes
{
	WrappedVkSemaphore(VkSemaphore obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkSemaphore InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSemaphore);
};
struct WrappedVkEvent : WrappedVkNonDispRes
{
	WrappedVkEvent(VkEvent obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkEvent InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkEvent);
};
struct WrappedVkQueryPool : WrappedVkNonDispRes
{
	WrappedVkQueryPool(VkQueryPool obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkQueryPool InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkQueryPool);
};
struct WrappedVkBufferView : WrappedVkNonDispRes
{
	WrappedVkBufferView(VkBufferView obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkBufferView InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkBufferView, AllocPoolCount, AllocPoolMaxByteSize);
};
struct WrappedVkImageView : WrappedVkNonDispRes
{
	WrappedVkImageView(VkImageView obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkImageView InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkImageView, AllocPoolCount, AllocPoolMaxByteSize);
};
struct WrappedVkAttachmentView : WrappedVkNonDispRes
{
	WrappedVkAttachmentView(VkAttachmentView obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkAttachmentView InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkAttachmentView, AllocPoolCount, AllocPoolMaxByteSize);
};
struct WrappedVkShaderModule : WrappedVkNonDispRes
{
	WrappedVkShaderModule(VkShaderModule obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkShaderModule InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkShaderModule, AllocPoolCount);
};
struct WrappedVkShader : WrappedVkNonDispRes
{
	WrappedVkShader(VkShader obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkShader InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkShader);
};
struct WrappedVkPipelineCache : WrappedVkNonDispRes
{
	WrappedVkPipelineCache(VkPipelineCache obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkPipelineCache InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipelineCache);
};
struct WrappedVkPipelineLayout : WrappedVkNonDispRes
{
	WrappedVkPipelineLayout(VkPipelineLayout obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkPipelineLayout InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipelineLayout, AllocPoolCount);
};
struct WrappedVkRenderPass : WrappedVkNonDispRes
{
	WrappedVkRenderPass(VkRenderPass obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkRenderPass InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkRenderPass);
};
struct WrappedVkPipeline : WrappedVkNonDispRes
{
	WrappedVkPipeline(VkPipeline obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkPipeline InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipeline, AllocPoolCount);
};
struct WrappedVkDescriptorSetLayout : WrappedVkNonDispRes
{
	WrappedVkDescriptorSetLayout(VkDescriptorSetLayout obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDescriptorSetLayout InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorSetLayout, AllocPoolCount);
};
struct WrappedVkSampler : WrappedVkNonDispRes
{
	WrappedVkSampler(VkSampler obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkSampler InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSampler);
};
struct WrappedVkDescriptorPool : WrappedVkNonDispRes
{
	WrappedVkDescriptorPool(VkDescriptorPool obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDescriptorPool InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorPool);
};
struct WrappedVkDescriptorSet : WrappedVkNonDispRes
{
	WrappedVkDescriptorSet(VkDescriptorSet obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDescriptorSet InnerType;
	static const int AllocPoolCount = 256*1024;
	static const int AllocPoolMaxByteSize = 6*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorSet, AllocPoolCount, AllocPoolMaxByteSize);
};
struct WrappedVkDynamicViewportState : WrappedVkNonDispRes
{
	WrappedVkDynamicViewportState(VkDynamicViewportState obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDynamicViewportState InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDynamicViewportState, AllocPoolCount);
};
struct WrappedVkDynamicRasterState : WrappedVkNonDispRes
{
	WrappedVkDynamicRasterState(VkDynamicRasterState obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDynamicRasterState InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDynamicRasterState);
};
struct WrappedVkDynamicColorBlendState : WrappedVkNonDispRes
{
	WrappedVkDynamicColorBlendState(VkDynamicColorBlendState obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDynamicColorBlendState InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDynamicColorBlendState);
};
struct WrappedVkDynamicDepthStencilState : WrappedVkNonDispRes
{
	WrappedVkDynamicDepthStencilState(VkDynamicDepthStencilState obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDynamicDepthStencilState InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDynamicDepthStencilState);
};
struct WrappedVkFramebuffer : WrappedVkNonDispRes
{
	WrappedVkFramebuffer(VkFramebuffer obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkFramebuffer InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkFramebuffer);
};
struct WrappedVkCmdPool : WrappedVkNonDispRes
{
	WrappedVkCmdPool(VkCmdPool obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkCmdPool InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkCmdPool);
};
struct WrappedVkSwapChainWSI : WrappedVkNonDispRes
{
	WrappedVkSwapChainWSI(VkSwapChainWSI obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkSwapChainWSI InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSwapChainWSI);
};

// template magic voodoo to unwrap types
template<typename inner> struct UnwrapHelper {};

#define UNWRAP_HELPER(vulkantype) \
	template<> struct UnwrapHelper<vulkantype> \
	{ \
		typedef WrappedVkDispRes ParentType; \
		typedef CONCAT(Wrapped, vulkantype) Outer; \
		static RealVkRes ToRealRes(vulkantype real) { return RealVkRes((void *)real); } \
		static Outer *FromHandle(vulkantype wrapped) { return (Outer *) wrapped; } \
	};

#define UNWRAP_NONDISP_HELPER(vulkantype) \
	template<> struct UnwrapHelper<vulkantype> \
	{ \
		typedef WrappedVkNonDispRes ParentType; \
		typedef CONCAT(Wrapped, vulkantype) Outer; \
		static RealVkRes ToRealRes(vulkantype real) { return RealVkRes(real.handle); } \
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
typename UnwrapHelper<RealType>::Outer *GetWrapped(RealType obj)
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
typename UnwrapHelper<RealType>::Outer::DispatchTableType *ObjDisp(RealType obj)
{
	return (typename UnwrapHelper<RealType>::Outer::DispatchTableType *)GetWrapped(obj)->table;
}

template<typename RealType>
RealType Unwrap(RealType obj)
{
	if(obj == VK_NULL_HANDLE) return VK_NULL_HANDLE;

	RealVkRes res = GetWrapped(obj)->real;

	return res.As<RealType>();
}

template<typename RealType>
ResourceId GetResID(RealType obj)
{
	if(obj == VK_NULL_HANDLE) return ResourceId();

	return GetWrapped(obj)->id;
}

template<typename RealType>
VkResourceRecord *GetRecord(RealType obj)
{
	if(obj == VK_NULL_HANDLE) return NULL;

	return GetWrapped(obj)->record;
}

template<typename RealType>
RealType ToHandle(WrappedVkRes *ptr)
{
	RealVkRes res = ((typename UnwrapHelper<RealType>::Outer *)ptr)->real;

	return res.As<RealType>();
}

enum VkResourceType
{
	eResUnknown = 0,
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
	eResFence,
	eResSemaphore,
	
	eResWSISwapChain,
};

bool IsDispatchableRes(WrappedVkRes *ptr);
VkResourceType IdentifyTypeByPtr(WrappedVkRes *ptr);

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

inline bool operator <(const VkDescriptorSet a, const VkDescriptorSet b)
{
	return a.handle < b.handle;
}

struct VkResourceRecord : public ResourceRecord
{
	public:
		enum { NullResource = (unsigned int)NULL };

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
		set<VkDescriptorSet> boundDescSets;

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
