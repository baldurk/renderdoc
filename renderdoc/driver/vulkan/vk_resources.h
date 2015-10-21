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

// empty base class for dispatchable/non-dispatchable. Unfortunately
// we can't put any members here as the base class is always first,
// and the dispatch tables that need to be first are the only difference
// between the two structs
struct WrappedVkRes
{
};

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
	eResCmdPool,
	eResCmdBuffer,
	eResFence,
	eResEvent,
	eResQueryPool,
	eResSemaphore,
	
	eResSwapchain,
};

// dummy standin for a typeless real resource
// stored in a uint64_t, with function to cast back
// if we know what type it is
struct RealVkRes
{
	RealVkRes() : handle((uint64_t)VK_NULL_HANDLE) {} 
	RealVkRes(void *disp) : handle((uint64_t)disp) {}
	RealVkRes(uint64_t nondisp) : handle(nondisp) {}

	bool operator ==(const RealVkRes o) const { return handle == o.handle; }
	bool operator !=(const RealVkRes o) const { return handle != o.handle; }
	bool operator < (const RealVkRes o) const { return handle <  o.handle; }

	uint64_t handle;
	template<typename T> T As() { return (T)handle; }
	template<typename T> T *AsPtr() { return (T*)&handle; }
};

// since handles can overlap (ie. handle 1 might be valid for many types
// if the ICD is using indexing or state packing instead of true pointers)
// when storing wrapper object <-> real object we have to store the type
// with the handle to avoid clashes
struct TypedRealHandle
{
	TypedRealHandle() : type(eResUnknown), real((void *)NULL) {}
	TypedRealHandle(unsigned int i) : type(eResUnknown), real((void *)NULL) {}

	VkResourceType type;
	RealVkRes real;

	bool operator <(const TypedRealHandle o) const
	{
		if(type != o.type)
			return type < o.type;
		return real < o.real;
	}

	bool operator ==(const TypedRealHandle o) const
	{
		return type == o.type && real == o.real;
	}
	bool operator !=(const TypedRealHandle o) const
	{
		return !(*this == o);
	}
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
	typedef VkLayerInstanceDispatchTable DispatchTableType;
	enum { UseInstanceDispatchTable = true, };
	enum { TypeEnum = eResInstance, };
};
struct WrappedVkPhysicalDevice : WrappedVkDispRes
{
	WrappedVkPhysicalDevice(VkPhysicalDevice obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
	typedef VkPhysicalDevice InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPhysicalDevice);
	typedef VkLayerInstanceDispatchTable DispatchTableType;
	enum { UseInstanceDispatchTable = true, };
	enum { TypeEnum = eResPhysicalDevice, };
};
struct WrappedVkDevice : WrappedVkDispRes
{
	WrappedVkDevice(VkDevice obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
	typedef VkDevice InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDevice);
	typedef VkLayerDispatchTable DispatchTableType;
	enum { UseInstanceDispatchTable = false, };
	enum { TypeEnum = eResDevice, };
};
struct WrappedVkQueue : WrappedVkDispRes
{
	WrappedVkQueue(VkQueue obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
	typedef VkQueue InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkQueue);
	typedef VkLayerDispatchTable DispatchTableType;
	enum { UseInstanceDispatchTable = false, };
	enum { TypeEnum = eResQueue, };
};
struct WrappedVkCmdBuffer : WrappedVkDispRes
{
	WrappedVkCmdBuffer(VkCmdBuffer obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
	typedef VkCmdBuffer InnerType;
	static const int AllocPoolCount = 32*1024;
	static const int AllocPoolMaxByteSize = 2*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkCmdBuffer, AllocPoolCount, AllocPoolMaxByteSize);
	typedef VkLayerDispatchTable DispatchTableType;
	enum { UseInstanceDispatchTable = false, };
	enum { TypeEnum = eResCmdBuffer, };
};
struct WrappedVkFence : WrappedVkNonDispRes
{
	WrappedVkFence(VkFence obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkFence InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkFence);
	enum { TypeEnum = eResFence, };
};
struct WrappedVkDeviceMemory : WrappedVkNonDispRes
{
	WrappedVkDeviceMemory(VkDeviceMemory obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDeviceMemory InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDeviceMemory, AllocPoolCount, AllocPoolMaxByteSize);
	enum { TypeEnum = eResDeviceMemory, };
};
struct WrappedVkBuffer : WrappedVkNonDispRes
{
	WrappedVkBuffer(VkBuffer obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkBuffer InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkBuffer, AllocPoolCount, AllocPoolMaxByteSize);
	enum { TypeEnum = eResBuffer, };
};
struct WrappedVkImage : WrappedVkNonDispRes
{
	WrappedVkImage(VkImage obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkImage InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkImage, AllocPoolCount, AllocPoolMaxByteSize);
	enum { TypeEnum = eResImage, };
};
struct WrappedVkSemaphore : WrappedVkNonDispRes
{
	WrappedVkSemaphore(VkSemaphore obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkSemaphore InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSemaphore);
	enum { TypeEnum = eResSemaphore, };
};
struct WrappedVkEvent : WrappedVkNonDispRes
{
	WrappedVkEvent(VkEvent obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkEvent InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkEvent);
	enum { TypeEnum = eResEvent, };
};
struct WrappedVkQueryPool : WrappedVkNonDispRes
{
	WrappedVkQueryPool(VkQueryPool obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkQueryPool InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkQueryPool);
	enum { TypeEnum = eResQueryPool, };
};
struct WrappedVkBufferView : WrappedVkNonDispRes
{
	WrappedVkBufferView(VkBufferView obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkBufferView InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkBufferView, AllocPoolCount, AllocPoolMaxByteSize, false);
	enum { TypeEnum = eResBufferView, };
};
struct WrappedVkImageView : WrappedVkNonDispRes
{
	WrappedVkImageView(VkImageView obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkImageView InnerType;
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkImageView, AllocPoolCount, AllocPoolMaxByteSize, false);
	enum { TypeEnum = eResImageView, };
};
struct WrappedVkShaderModule : WrappedVkNonDispRes
{
	WrappedVkShaderModule(VkShaderModule obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkShaderModule InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkShaderModule, AllocPoolCount);
	enum { TypeEnum = eResShaderModule, };
};
struct WrappedVkShader : WrappedVkNonDispRes
{
	WrappedVkShader(VkShader obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkShader InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkShader);
	enum { TypeEnum = eResShader, };
};
struct WrappedVkPipelineCache : WrappedVkNonDispRes
{
	WrappedVkPipelineCache(VkPipelineCache obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkPipelineCache InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipelineCache);
	enum { TypeEnum = eResPipelineCache, };
};
struct WrappedVkPipelineLayout : WrappedVkNonDispRes
{
	WrappedVkPipelineLayout(VkPipelineLayout obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkPipelineLayout InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipelineLayout, AllocPoolCount);
	enum { TypeEnum = eResPipelineLayout, };
};
struct WrappedVkRenderPass : WrappedVkNonDispRes
{
	WrappedVkRenderPass(VkRenderPass obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkRenderPass InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkRenderPass);
	enum { TypeEnum = eResRenderPass, };
};
struct WrappedVkPipeline : WrappedVkNonDispRes
{
	WrappedVkPipeline(VkPipeline obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkPipeline InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipeline, AllocPoolCount);
	enum { TypeEnum = eResPipeline, };
};
struct WrappedVkDescriptorSetLayout : WrappedVkNonDispRes
{
	WrappedVkDescriptorSetLayout(VkDescriptorSetLayout obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDescriptorSetLayout InnerType;
	static const int AllocPoolCount = 32*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorSetLayout, AllocPoolCount);
	enum { TypeEnum = eResDescriptorSetLayout, };
};
struct WrappedVkSampler : WrappedVkNonDispRes
{
	WrappedVkSampler(VkSampler obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	static const int AllocPoolCount = 8192;
	static const int AllocPoolMaxByteSize = 1024*1024;
	typedef VkSampler InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSampler, AllocPoolCount, AllocPoolMaxByteSize, false);
	enum { TypeEnum = eResSampler, };
};
struct WrappedVkDescriptorPool : WrappedVkNonDispRes
{
	WrappedVkDescriptorPool(VkDescriptorPool obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDescriptorPool InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorPool);
	enum { TypeEnum = eResDescriptorPool, };
};
struct WrappedVkDescriptorSet : WrappedVkNonDispRes
{
	WrappedVkDescriptorSet(VkDescriptorSet obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkDescriptorSet InnerType;
	static const int AllocPoolCount = 256*1024;
	static const int AllocPoolMaxByteSize = 6*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorSet, AllocPoolCount, AllocPoolMaxByteSize);
	enum { TypeEnum = eResDescriptorSet, };
};
struct WrappedVkFramebuffer : WrappedVkNonDispRes
{
	WrappedVkFramebuffer(VkFramebuffer obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkFramebuffer InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkFramebuffer);
	enum { TypeEnum = eResFramebuffer, };
};
struct WrappedVkCmdPool : WrappedVkNonDispRes
{
	WrappedVkCmdPool(VkCmdPool obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkCmdPool InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkCmdPool);
	enum { TypeEnum = eResCmdPool, };
};
struct WrappedVkSwapchainKHR : WrappedVkNonDispRes
{
	WrappedVkSwapchainKHR(VkSwapchainKHR obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
	typedef VkSwapchainKHR InnerType; ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSwapchainKHR);
	enum { TypeEnum = eResSwapchain, };
};

// VKTODOMED Need to find out which resources can validly return duplicate
// handles for unique creates. E.g. if there are the same input parameters
// to multiple create calls - perhaps it is valid for any handle to be
// returned twice.

// template magic voodoo to unwrap types
template<typename inner> struct UnwrapHelper {};

#define UNWRAP_HELPER(vulkantype) \
	template<> struct UnwrapHelper<vulkantype> \
	{ \
		typedef WrappedVkDispRes ParentType; \
		typedef CONCAT(Wrapped, vulkantype) Outer; \
		static TypedRealHandle ToTypedHandle(vulkantype real) \
		{ TypedRealHandle h; h.type = (VkResourceType)Outer::TypeEnum; h.real = RealVkRes((void *)real); return h; } \
		static Outer *FromHandle(vulkantype wrapped) { return (Outer *) wrapped; } \
	};

#define UNWRAP_NONDISP_HELPER(vulkantype) \
	template<> struct UnwrapHelper<vulkantype> \
	{ \
		typedef WrappedVkNonDispRes ParentType; \
		typedef CONCAT(Wrapped, vulkantype) Outer; \
		static TypedRealHandle ToTypedHandle(vulkantype real) \
		{ TypedRealHandle h; h.type = (VkResourceType)Outer::TypeEnum; h.real = RealVkRes(real.handle); return h; } \
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
UNWRAP_NONDISP_HELPER(VkFramebuffer)
UNWRAP_NONDISP_HELPER(VkCmdPool)
UNWRAP_NONDISP_HELPER(VkSwapchainKHR)

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

	RealVkRes &res = GetWrapped(obj)->real;

	return res.As<RealType>();
}

template<typename RealType>
RealType *UnwrapPtr(RealType obj)
{
	if(obj == VK_NULL_HANDLE) return NULL;

	RealVkRes &res = GetWrapped(obj)->real;

	return res.AsPtr<RealType>();
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
	RealVkRes &res = ((typename UnwrapHelper<RealType>::Outer *)ptr)->real;

	return res.As<RealType>();
}

template<typename RealType>
TypedRealHandle ToTypedHandle(RealType obj)
{
	return UnwrapHelper<RealType>::ToTypedHandle(obj);
}

template<typename parenttype, typename wrappedtype>
inline void SetTableIfDispatchable(bool writing, parenttype parent, wrappedtype *obj) {}
template<> inline void SetTableIfDispatchable(bool writing, VkInstance parent, WrappedVkInstance *obj)
{ SetDispatchTable(writing, parent, obj); }
template<> inline void SetTableIfDispatchable(bool writing, VkInstance parent, WrappedVkPhysicalDevice *obj)
{ SetDispatchTable(writing, parent, obj); }
template<> inline void SetTableIfDispatchable(bool writing, VkDevice parent, WrappedVkDevice *obj)
{ SetDispatchTable(writing, parent, obj); }
template<> inline void SetTableIfDispatchable(bool writing, VkDevice parent, WrappedVkQueue *obj)
{ SetDispatchTable(writing, parent, obj); }
template<> inline void SetTableIfDispatchable(bool writing, VkDevice parent, WrappedVkCmdBuffer *obj)
{ SetDispatchTable(writing, parent, obj); }

bool IsDispatchableRes(WrappedVkRes *ptr);
VkResourceType IdentifyTypeByPtr(WrappedVkRes *ptr);

#define UNTRANSITIONED_IMG_STATE ((VkImageLayout)0xffffffff)

struct ImageRegionState
{
	ImageRegionState()
		: prevstate(UNTRANSITIONED_IMG_STATE), state(UNTRANSITIONED_IMG_STATE)
	{
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseArrayLayer = 0; range.arraySize = 0;
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

struct SwapchainInfo
{
	VkFormat format;
	VkExtent2D extent;
	int arraySize;

	VkRenderPass rp;

	struct SwapImage
	{
		VkImage im;

		VkImageView view;
		VkFramebuffer fb;
	};
	vector<SwapImage> images;
};
	
struct CmdBufferRecordingInfo
{
	VkDevice device;
	VkCmdBufferCreateInfo createInfo;

	vector< pair<ResourceId, ImageRegionState> > imgtransitions;

	// a list of all resources dirtied by this command buffer
	set<ResourceId> dirtied;

	// a list of descriptor sets that are bound at any point in this command buffer
	// used to look up all the frame refs per-desc set and apply them on queue
	// submit with latest binding refs.
	set<VkDescriptorSet> boundDescSets;
};

struct DescSetLayout;

struct VkResourceRecord : public ResourceRecord
{
	public:
		enum { NullResource = (unsigned int)NULL };

		VkResourceRecord(ResourceId id) :
			ResourceRecord(id, true),
			bakedCommands(NULL),
			pool(NULL),
			memory(NULL),
			layout(NULL),
			swapInfo(NULL),
			cmdInfo(NULL)
		{
		}

		~VkResourceRecord();

		void Bake()
		{
			RDCASSERT(cmdInfo);
			SwapChunks(bakedCommands);
			cmdInfo->dirtied.swap(bakedCommands->cmdInfo->dirtied);
			cmdInfo->boundDescSets.swap(bakedCommands->cmdInfo->boundDescSets);
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

		WrappedVkRes *Resource;
	
		VkResourceRecord *bakedCommands;

		SwapchainInfo *swapInfo;
		CmdBufferRecordingInfo *cmdInfo;

		// queues associated with this instance, so they can be shut down on destruction
		vector<VkQueue> queues;

		// pointer to either the pool this item is allocated from, or the children allocated
		// from this pool. Protected by the chunk lock 
		VkResourceRecord *pool;
		vector<VkResourceRecord *> pooledChildren;

		// descriptor set bindings for this descriptor set. Filled out on
		// create from the layout.
		DescSetLayout *layout;
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
		: device(VK_NULL_HANDLE), mapOffset(0), mapSize(0), size(0), mapFlags(0), mapFrame(0), mappedPtr(NULL), mapFlushed(false), refData(NULL)
	{ }
	VkDevice device;
	VkDeviceSize mapOffset, mapSize;
	VkDeviceSize size;
	VkMemoryMapFlags mapFlags;
	uint32_t mapFrame;
	bool mapFlushed;
	void *mappedPtr;
	byte *refData;
};
struct ImgState
{
	ImgState()
		: view(VK_NULL_HANDLE), arraySize(0), mipLevels(0), samples(0), cube(false), creationFlags(0)
	{
		type = VK_IMAGE_TYPE_MAX_ENUM;
		format = VK_FORMAT_UNDEFINED;
		extent.width = extent.height = extent.depth = 0;
	}

	VkImageView view;
	vector<ImageRegionState> subresourceStates;

	VkImageType type;
	VkFormat format;
	VkExtent3D extent;
	int arraySize, mipLevels, samples;

	bool cube;
	uint32_t creationFlags;
};

bool IsBlockFormat(VkFormat f);
bool IsDepthStencilFormat(VkFormat f);
bool IsSRGBFormat(VkFormat f);

uint32_t GetByteSize(uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format, uint32_t mip);
