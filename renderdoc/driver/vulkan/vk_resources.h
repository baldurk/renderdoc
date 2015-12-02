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
	VkResourceRecord *record;
};

class WrappedVulkan;

struct WrappedVkDispRes : public WrappedVkRes
{
	WrappedVkDispRes(VkInstance obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
	{ loaderTable = *(uintptr_t*)obj; }

	WrappedVkDispRes(VkPhysicalDevice obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
	{ loaderTable = *(uintptr_t*)obj; }
	
	WrappedVkDispRes(VkDevice obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
	{ loaderTable = *(uintptr_t*)obj; }

	WrappedVkDispRes(VkQueue obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
	{ loaderTable = *(uintptr_t*)obj; }

	WrappedVkDispRes(VkCmdBuffer obj, ResourceId objId) : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
	{ loaderTable = *(uintptr_t*)obj; }

	// preserve dispatch table pointer in dispatchable objects
	uintptr_t loaderTable, table;
	RealVkRes real;
	ResourceId id;
	VkResourceRecord *record;
	// we store this here so that any entry point with a dispatchable object can find the
	// write instance to invoke into, without needing to keep any around. Its lifetime is
	// tied to the VkInstance
	WrappedVulkan *core;
};

// ensure the structs don't accidentally get made larger
RDCCOMPILE_ASSERT(sizeof(WrappedVkDispRes) == (sizeof(uint64_t)*2 + sizeof(uintptr_t)*4), "Wrapped resource struct has changed size! This is bad");
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
typename UnwrapHelper<RealType>::Outer::DispatchTableType *LayerDisp(RealType obj)
{
	return (typename UnwrapHelper<RealType>::Outer::DispatchTableType *)GetWrapped(obj)->loaderTable;
}

template<typename RealType>
typename UnwrapHelper<RealType>::Outer::DispatchTableType *ObjDisp(RealType obj)
{
	return (typename UnwrapHelper<RealType>::Outer::DispatchTableType *)GetWrapped(obj)->table;
}

template<typename RealType>
WrappedVulkan *CoreDisp(RealType obj)
{
	return (WrappedVulkan *)GetWrapped(obj)->core;
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
inline void SetTableIfDispatchable(bool writing, parenttype parent, WrappedVulkan *core, wrappedtype *obj) {}
template<> inline void SetTableIfDispatchable(bool writing, VkInstance parent, WrappedVulkan *core, WrappedVkInstance *obj)
{ SetDispatchTable(writing, parent, core, obj); }
template<> inline void SetTableIfDispatchable(bool writing, VkInstance parent, WrappedVulkan *core, WrappedVkPhysicalDevice *obj)
{ SetDispatchTable(writing, parent, core, obj); }
template<> inline void SetTableIfDispatchable(bool writing, VkDevice parent, WrappedVulkan *core, WrappedVkDevice *obj)
{ SetDispatchTable(writing, parent, core, obj); }
template<> inline void SetTableIfDispatchable(bool writing, VkDevice parent, WrappedVulkan *core, WrappedVkQueue *obj)
{ SetDispatchTable(writing, parent, core, obj); }
template<> inline void SetTableIfDispatchable(bool writing, VkDevice parent, WrappedVulkan *core, WrappedVkCmdBuffer *obj)
{ SetDispatchTable(writing, parent, core, obj); }

bool IsDispatchableRes(WrappedVkRes *ptr);
VkResourceType IdentifyTypeByPtr(WrappedVkRes *ptr);

#define UNKNOWN_PREV_IMG_LAYOUT ((VkImageLayout)0xffffffff)

struct ImageRegionState
{
	ImageRegionState()
		: oldLayout(UNKNOWN_PREV_IMG_LAYOUT), newLayout(UNKNOWN_PREV_IMG_LAYOUT)
	{
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseArrayLayer = 0; subresourceRange.arraySize = 0;
		subresourceRange.baseMipLevel = 0; subresourceRange.mipLevels = 0;
	}
	ImageRegionState(VkImageSubresourceRange r, VkImageLayout pr, VkImageLayout st)
	    : subresourceRange(r), oldLayout(pr), newLayout(st) {}

	VkImageSubresourceRange subresourceRange;
	VkImageLayout oldLayout;
	VkImageLayout newLayout;
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

	RENDERDOC_WindowHandle wndHandle;

	struct SwapImage
	{
		VkImage im;

		VkImageView view;
		VkFramebuffer fb;
	};
	vector<SwapImage> images;
	uint32_t lastPresent;
};

struct SparseMapping
{
	SparseMapping()
	{
		RDCEraseEl(imgdim);
		RDCEraseEl(pagedim);
		RDCEraseEl(pages);
	}

	// for buffers or non-sparse-resident images (bound with opaque mappings)
	vector<VkSparseMemoryBindInfo> opaquemappings;

	// for sparse resident images:
	// total image size (in pages)
	VkExtent3D imgdim;
	// size of a page
	VkExtent3D pagedim;
	// pagetable per image aspect (some may be NULL)
	// in order of width first, then height, then depth
	pair<VkDeviceMemory, VkDeviceSize> *pages[VK_IMAGE_ASPECT_NUM];

	void Update(uint32_t numBindings, const VkSparseImageMemoryBindInfo *pBindings);
	void Update(uint32_t numBindings, const VkSparseMemoryBindInfo *pBindings);
};

struct CmdBufferRecordingInfo
{
	VkDevice device;
	VkCmdBufferCreateInfo createInfo;

	vector< pair<ResourceId, ImageRegionState> > imgbarriers;

	// sparse resources referenced by this command buffer (at submit time
	// need to go through the sparse mapping and reference all memory)
	set<SparseMapping *> sparse;

	// a list of all resources dirtied by this command buffer
	set<ResourceId> dirtied;

	// a list of descriptor sets that are bound at any point in this command buffer
	// used to look up all the frame refs per-desc set and apply them on queue
	// submit with latest binding refs.
	set<VkDescriptorSet> boundDescSets;

	vector<VkResourceRecord *> subcmds;
};

struct DescSetLayout;

struct DescriptorSetData
{
	DescriptorSetData() : layout(NULL) {}

	~DescriptorSetData()
	{
		for(size_t i=0; i < descBindings.size(); i++)
			delete[] descBindings[i];
		descBindings.clear();
	}

	DescSetLayout *layout;

	// descriptor set bindings for this descriptor set. Filled out on
	// create from the layout.
	vector<VkDescriptorInfo *> descBindings;

	// contains the framerefs (ref counted) for the bound resources
	// in the binding slots. Updated when updating descriptor sets
	// and then applied in a block on descriptor set bind.
	// the refcount has the high-bit set if this resource has sparse
	// mapping information
	static const uint32_t SPARSE_REF_BIT = 0x80000000;
	map<ResourceId, pair<uint32_t, FrameRefType> > bindFrameRefs;
};

struct MemMapState
{
	MemMapState()
		: mapOffset(0), mapSize(0)
		, mapFlushed(false), mapCoherent(false), mappedPtr(NULL), refData(NULL)
	{ }
	VkDeviceSize mapOffset, mapSize;
	bool mapFlushed;
	bool mapCoherent;
	byte *mappedPtr;
	byte *refData;
};

struct VkResourceRecord : public ResourceRecord
{
	public:
		enum { NullResource = (unsigned int)NULL };

		static byte markerValue[32];

		VkResourceRecord(ResourceId id) :
			ResourceRecord(id, true),
			Resource(NULL),
			bakedCommands(NULL),
			pool(NULL),
			memIdxMap(NULL),
			ptrunion(NULL)
		{
		}

		~VkResourceRecord();

		void Bake()
		{
			RDCASSERT(cmdInfo);
			SwapChunks(bakedCommands);
			cmdInfo->dirtied.swap(bakedCommands->cmdInfo->dirtied);
			cmdInfo->boundDescSets.swap(bakedCommands->cmdInfo->boundDescSets);
			cmdInfo->imgbarriers.swap(bakedCommands->cmdInfo->imgbarriers);
			cmdInfo->subcmds.swap(bakedCommands->cmdInfo->subcmds);
			cmdInfo->sparse.swap(bakedCommands->cmdInfo->sparse);
		}

		void AddBindFrameRef(ResourceId id, FrameRefType ref, bool hasSparse = false)
		{
			if(id == ResourceId())
			{
				RDCERR("Unexpected NULL resource ID being added as a bind frame ref");
				return;
			}

			if((descInfo->bindFrameRefs[id].first & ~DescriptorSetData::SPARSE_REF_BIT) == 0)
			{
				descInfo->bindFrameRefs[id] = std::make_pair(1 | (hasSparse ? DescriptorSetData::SPARSE_REF_BIT : 0), ref);
			}
			else
			{
				// be conservative - mark refs as read before write if we see a write and a read ref on it
				if(ref == eFrameRef_Write && descInfo->bindFrameRefs[id].second == eFrameRef_Read)
					descInfo->bindFrameRefs[id].second = eFrameRef_ReadBeforeWrite;
				descInfo->bindFrameRefs[id].first++;
			}
		}

		void RemoveBindFrameRef(ResourceId id)
		{
			// ignore any NULL IDs - probably an object that was
			// deleted since it was bound.
			if(id == ResourceId()) return;

			auto it = descInfo->bindFrameRefs.find(id);
			
			// in the case of re-used handles bound to descriptor sets,
			// it's possible to try and remove a frameref on something we
			// don't have (which means we'll have a corresponding stale ref)
			// but this is harmless so we can ignore it.
			if(it == descInfo->bindFrameRefs.end()) return;

			it->second.first--;
			
			if((it->second.first & ~DescriptorSetData::SPARSE_REF_BIT) == 0)
				descInfo->bindFrameRefs.erase(it);
		}

		// we have a lot of 'cold' data in the resource record, as it can be accessed
		// through the wrapped objects without locking any lookup structures.
		// To save on object size, the data is union'd as much as possible where only
		// one type of object's record will contain some data, disjoint with another.
		// Some of these are pointers to resource-specific data (often STL structures),
		// which means a lot of pointer chasing - need to determine if this is a
		// performance issue

		WrappedVkRes *Resource;

		// externally allocated/freed, a mapping from memory idx
		// in our modified properties that were passed to the app
		// to the memory indices that actually exist
		uint32_t *memIdxMap;
		
		// this points to the base resource, either memory or an image - 
		// ie. the resource that can be modified or changes (or can become dirty)
		// since typical memory bindings are immutable and must happen before
		// creation or use, this can always be determined
		ResourceId baseResource;
		ResourceId baseResourceMem; // for image views, we need to point to both the image and mem

		// these are all disjoint, so only a record of the right type will have each
		// Note some of these need to be deleted in the constructor, so we check the
		// allocation type of the Resource
		union
		{
			void *ptrunion;                               // for initialisation to NULL
			VkPhysicalDeviceMemoryProperties *memProps;   // only for physical devices
			SparseMapping *sparseInfo;                    // only for buffers, images, and views of them
			SwapchainInfo *swapInfo;                      // only for swapchains
			MemMapState *memMapState;                     // only for device memory
			CmdBufferRecordingInfo *cmdInfo;              // only for command buffers
			VkResourceRecord **imageAttachments;          // only for framebuffers
			DescriptorSetData *descInfo;                  // only for descriptor sets and descriptor set layouts
		};

		VkResourceRecord *bakedCommands;

		static const int MaxImageAttachments = 8;

		// pointer to either the pool this item is allocated from, or the children allocated
		// from this pool. Protected by the chunk lock 
		VkResourceRecord *pool;
		vector<VkResourceRecord *> pooledChildren;
};

struct ImageLayouts
{
	ImageLayouts() : arraySize(1), mipLevels(1) {}

	vector<ImageRegionState> subresourceStates;
	int arraySize, mipLevels;
	VkExtent3D extent;
	VkFormat format;
};

bool IsBlockFormat(VkFormat f);
bool IsDepthStencilFormat(VkFormat f);
bool IsDepthOnlyFormat(VkFormat f);
bool IsSRGBFormat(VkFormat f);

uint32_t GetByteSize(uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format, uint32_t mip);
