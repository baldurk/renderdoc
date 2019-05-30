/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "common/wrapped_pool.h"
#include "core/bit_flag_iterator.h"
#include "core/intervals.h"
#include "core/resource_manager.h"
#include "vk_common.h"
#include "vk_hookset_defs.h"

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
  eResPipelineCache,
  eResPipelineLayout,
  eResPipeline,
  eResSampler,
  eResDescriptorPool,
  eResDescriptorSetLayout,
  eResDescriptorSet,
  eResCommandPool,
  eResCommandBuffer,
  eResFence,
  eResEvent,
  eResQueryPool,
  eResSemaphore,
  eResSwapchain,
  eResSurface,
  eResDescUpdateTemplate,
  eResSamplerConversion,
};

DECLARE_REFLECTION_ENUM(VkResourceType);

// VkDisplayKHR and VkDisplayModeKHR are both UNWRAPPED because there's no need to wrap them.
// The only thing we need to wrap VkSurfaceKHR for is to get back the window from it later.

// dummy standin for a typeless real resource
// stored in a uint64_t, with function to cast back
// if we know what type it is
struct RealVkRes
{
  RealVkRes() : handle((uint64_t)VK_NULL_HANDLE) {}
  RealVkRes(void *disp) : handle((uint64_t)disp) {}
  RealVkRes(uint64_t nondisp) : handle(nondisp) {}
  bool operator==(const RealVkRes o) const { return handle == o.handle; }
  bool operator!=(const RealVkRes o) const { return handle != o.handle; }
  bool operator<(const RealVkRes o) const { return handle < o.handle; }
  uint64_t handle;
  template <typename T>
  T As()
  {
    return (T)handle;
  }
  template <typename T>
  T *AsPtr()
  {
    return (T *)&handle;
  }
};

// this is defined in a custom modification to vulkan.h, where on 32-bit systems
// we gain type safety by using a C++ struct to wrap the uint64_t instead of using
// a naked uint64_t typedef
#ifdef VK_NON_DISPATCHABLE_WRAPPER_STRUCT

#define NON_DISP_TO_UINT64(obj) obj.handle

#else

#define NON_DISP_TO_UINT64(obj) (uint64_t) obj

#endif

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

  bool operator<(const TypedRealHandle o) const
  {
    if(type != o.type)
      return type < o.type;
    return real < o.real;
  }

  bool operator==(const TypedRealHandle o) const
  {
    // NULL compares as equal regardless of type.
    return (real.handle == 0 && o.real.handle == 0) || (type == o.type && real == o.real);
  }
  bool operator!=(const TypedRealHandle o) const { return !(*this == o); }
};

struct WrappedVkNonDispRes : public WrappedVkRes
{
  template <typename T>
  WrappedVkNonDispRes(T obj, ResourceId objId)
      : real(NON_DISP_TO_UINT64(obj)), id(objId), record(NULL)
  {
  }

  RealVkRes real;
  ResourceId id;
  VkResourceRecord *record;
};

class WrappedVulkan;

struct WrappedVkDispRes : public WrappedVkRes
{
  WrappedVkDispRes(VkInstance obj, ResourceId objId)
      : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
  {
    loaderTable = *(uintptr_t *)obj;
  }

  WrappedVkDispRes(VkPhysicalDevice obj, ResourceId objId)
      : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
  {
    loaderTable = *(uintptr_t *)obj;
  }

  WrappedVkDispRes(VkDevice obj, ResourceId objId)
      : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
  {
    loaderTable = *(uintptr_t *)obj;
  }

  WrappedVkDispRes(VkQueue obj, ResourceId objId)
      : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
  {
    loaderTable = *(uintptr_t *)obj;
  }

  WrappedVkDispRes(VkCommandBuffer obj, ResourceId objId)
      : table(0), real((void *)obj), id(objId), record(NULL), core(NULL)
  {
    loaderTable = *(uintptr_t *)obj;
  }

  template <typename realtype>
  void RewrapObject(realtype obj)
  {
    real = (void *)obj;
    loaderTable = *(uintptr_t *)obj;
  }

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

struct WrappedVkDispResSizeCheck
{
  uintptr_t pad1[2];
  uint64_t pad2[2];
  void *pad3[2];
};

struct WrappedVkNonDispResSizeCheck
{
  uint64_t pad1[2];
  void *pad2[1];
};

// ensure the structs don't accidentally get made larger
RDCCOMPILE_ASSERT(sizeof(WrappedVkDispRes) == sizeof(WrappedVkDispResSizeCheck),
                  "Wrapped resource struct has changed size! This is bad");
RDCCOMPILE_ASSERT(sizeof(WrappedVkNonDispRes) == sizeof(WrappedVkNonDispResSizeCheck),
                  "Wrapped resource struct has changed size! This is bad");

// these are expanded out so that IDE autocompletion etc works without having to process through
// macros
struct WrappedVkInstance : WrappedVkDispRes
{
  WrappedVkInstance(VkInstance obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
  typedef VkInstance InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkInstance);
  typedef VkLayerInstanceDispatchTableExtended DispatchTableType;
  enum
  {
    UseInstanceDispatchTable = true,
  };
  enum
  {
    TypeEnum = eResInstance,
  };
};
struct WrappedVkPhysicalDevice : WrappedVkDispRes
{
  WrappedVkPhysicalDevice(VkPhysicalDevice obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
  typedef VkPhysicalDevice InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPhysicalDevice);
  typedef VkLayerInstanceDispatchTableExtended DispatchTableType;
  enum
  {
    UseInstanceDispatchTable = true,
  };
  enum
  {
    TypeEnum = eResPhysicalDevice,
  };
};
struct WrappedVkDevice : WrappedVkDispRes
{
  WrappedVkDevice(VkDevice obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
  typedef VkDevice InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDevice);
  typedef VkLayerDispatchTableExtended DispatchTableType;
  enum
  {
    UseInstanceDispatchTable = false,
  };
  enum
  {
    TypeEnum = eResDevice,
  };
};
struct WrappedVkQueue : WrappedVkDispRes
{
  WrappedVkQueue(VkQueue obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
  typedef VkQueue InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkQueue);
  typedef VkLayerDispatchTableExtended DispatchTableType;
  enum
  {
    UseInstanceDispatchTable = false,
  };
  enum
  {
    TypeEnum = eResQueue,
  };
};
struct WrappedVkCommandBuffer : WrappedVkDispRes
{
  WrappedVkCommandBuffer(VkCommandBuffer obj, ResourceId objId) : WrappedVkDispRes(obj, objId) {}
  typedef VkCommandBuffer InnerType;
  static const int AllocPoolCount = 32 * 1024;
  static const int AllocPoolMaxByteSize = 2 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkCommandBuffer, AllocPoolCount, AllocPoolMaxByteSize);
  typedef VkLayerDispatchTableExtended DispatchTableType;
  enum
  {
    UseInstanceDispatchTable = false,
  };
  enum
  {
    TypeEnum = eResCommandBuffer,
  };
};
struct WrappedVkFence : WrappedVkNonDispRes
{
  WrappedVkFence(VkFence obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkFence InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkFence);
  enum
  {
    TypeEnum = eResFence,
  };
};
struct WrappedVkDeviceMemory : WrappedVkNonDispRes
{
  WrappedVkDeviceMemory(VkDeviceMemory obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkDeviceMemory InnerType;
  static const int AllocPoolCount = 128 * 1024;
  static const int AllocPoolMaxByteSize = 3 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDeviceMemory, AllocPoolCount, AllocPoolMaxByteSize);
  enum
  {
    TypeEnum = eResDeviceMemory,
  };
};
struct WrappedVkBuffer : WrappedVkNonDispRes
{
  WrappedVkBuffer(VkBuffer obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkBuffer InnerType;
  static const int AllocPoolCount = 128 * 1024;
  static const int AllocPoolMaxByteSize = 3 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkBuffer, AllocPoolCount, AllocPoolMaxByteSize, false);
  enum
  {
    TypeEnum = eResBuffer,
  };
};
struct WrappedVkImage : WrappedVkNonDispRes
{
  WrappedVkImage(VkImage obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkImage InnerType;
  static const int AllocPoolCount = 128 * 1024;
  static const int AllocPoolMaxByteSize = 3 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkImage, AllocPoolCount, AllocPoolMaxByteSize);
  enum
  {
    TypeEnum = eResImage,
  };
};
struct WrappedVkSemaphore : WrappedVkNonDispRes
{
  WrappedVkSemaphore(VkSemaphore obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkSemaphore InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSemaphore);
  enum
  {
    TypeEnum = eResSemaphore,
  };
};
struct WrappedVkEvent : WrappedVkNonDispRes
{
  WrappedVkEvent(VkEvent obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkEvent InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkEvent);
  enum
  {
    TypeEnum = eResEvent,
  };
};
struct WrappedVkQueryPool : WrappedVkNonDispRes
{
  WrappedVkQueryPool(VkQueryPool obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkQueryPool InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkQueryPool);
  enum
  {
    TypeEnum = eResQueryPool,
  };
};
struct WrappedVkBufferView : WrappedVkNonDispRes
{
  WrappedVkBufferView(VkBufferView obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkBufferView InnerType;
  static const int AllocPoolCount = 128 * 1024;
  static const int AllocPoolMaxByteSize = 3 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkBufferView, AllocPoolCount, AllocPoolMaxByteSize, false);
  enum
  {
    TypeEnum = eResBufferView,
  };
};
struct WrappedVkImageView : WrappedVkNonDispRes
{
  WrappedVkImageView(VkImageView obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkImageView InnerType;
  static const int AllocPoolCount = 128 * 1024;
  static const int AllocPoolMaxByteSize = 3 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkImageView, AllocPoolCount, AllocPoolMaxByteSize, false);
  enum
  {
    TypeEnum = eResImageView,
  };
};
struct WrappedVkShaderModule : WrappedVkNonDispRes
{
  WrappedVkShaderModule(VkShaderModule obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkShaderModule InnerType;
  static const int AllocPoolCount = 32 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkShaderModule, AllocPoolCount);
  enum
  {
    TypeEnum = eResShaderModule,
  };
};
struct WrappedVkPipelineCache : WrappedVkNonDispRes
{
  WrappedVkPipelineCache(VkPipelineCache obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkPipelineCache InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipelineCache);
  enum
  {
    TypeEnum = eResPipelineCache,
  };
};
struct WrappedVkPipelineLayout : WrappedVkNonDispRes
{
  WrappedVkPipelineLayout(VkPipelineLayout obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId)
  {
  }
  typedef VkPipelineLayout InnerType;
  static const int AllocPoolCount = 32 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipelineLayout, AllocPoolCount);
  enum
  {
    TypeEnum = eResPipelineLayout,
  };
};
struct WrappedVkRenderPass : WrappedVkNonDispRes
{
  WrappedVkRenderPass(VkRenderPass obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkRenderPass InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkRenderPass);
  enum
  {
    TypeEnum = eResRenderPass,
  };
};
struct WrappedVkPipeline : WrappedVkNonDispRes
{
  WrappedVkPipeline(VkPipeline obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkPipeline InnerType;
  static const int AllocPoolCount = 32 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkPipeline, AllocPoolCount);
  enum
  {
    TypeEnum = eResPipeline,
  };
};
struct WrappedVkDescriptorSetLayout : WrappedVkNonDispRes
{
  WrappedVkDescriptorSetLayout(VkDescriptorSetLayout obj, ResourceId objId)
      : WrappedVkNonDispRes(obj, objId)
  {
  }
  typedef VkDescriptorSetLayout InnerType;
  static const int AllocPoolCount = 32 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorSetLayout, AllocPoolCount);
  enum
  {
    TypeEnum = eResDescriptorSetLayout,
  };
};
struct WrappedVkSampler : WrappedVkNonDispRes
{
  WrappedVkSampler(VkSampler obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  static const int AllocPoolCount = 8192;
  static const int AllocPoolMaxByteSize = 1024 * 1024;
  typedef VkSampler InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSampler, AllocPoolCount, AllocPoolMaxByteSize, false);
  enum
  {
    TypeEnum = eResSampler,
  };
};
struct WrappedVkDescriptorPool : WrappedVkNonDispRes
{
  WrappedVkDescriptorPool(VkDescriptorPool obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId)
  {
  }
  typedef VkDescriptorPool InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorPool);
  enum
  {
    TypeEnum = eResDescriptorPool,
  };
};
struct WrappedVkDescriptorSet : WrappedVkNonDispRes
{
  WrappedVkDescriptorSet(VkDescriptorSet obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkDescriptorSet InnerType;
  static const int AllocPoolCount = 256 * 1024;
  static const int AllocPoolMaxByteSize = 6 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorSet, AllocPoolCount, AllocPoolMaxByteSize);
  enum
  {
    TypeEnum = eResDescriptorSet,
  };
};
struct WrappedVkFramebuffer : WrappedVkNonDispRes
{
  WrappedVkFramebuffer(VkFramebuffer obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkFramebuffer InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkFramebuffer);
  enum
  {
    TypeEnum = eResFramebuffer,
  };
};
struct WrappedVkCommandPool : WrappedVkNonDispRes
{
  WrappedVkCommandPool(VkCommandPool obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkCommandPool InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkCommandPool);
  enum
  {
    TypeEnum = eResCommandPool,
  };
};
struct WrappedVkSwapchainKHR : WrappedVkNonDispRes
{
  WrappedVkSwapchainKHR(VkSwapchainKHR obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkSwapchainKHR InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSwapchainKHR);
  enum
  {
    TypeEnum = eResSwapchain,
  };
};
struct WrappedVkSurfaceKHR : WrappedVkNonDispRes
{
  WrappedVkSurfaceKHR(VkSurfaceKHR obj, ResourceId objId) : WrappedVkNonDispRes(obj, objId) {}
  typedef VkSurfaceKHR InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSurfaceKHR);
  enum
  {
    TypeEnum = eResSurface,
  };
};
struct WrappedVkDescriptorUpdateTemplate : WrappedVkNonDispRes
{
  WrappedVkDescriptorUpdateTemplate(VkDescriptorUpdateTemplate obj, ResourceId objId)
      : WrappedVkNonDispRes(obj, objId)
  {
  }
  typedef VkDescriptorUpdateTemplate InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkDescriptorUpdateTemplate);
  enum
  {
    TypeEnum = eResDescUpdateTemplate,
  };
};
struct WrappedVkSamplerYcbcrConversion : WrappedVkNonDispRes
{
  WrappedVkSamplerYcbcrConversion(VkSamplerYcbcrConversion obj, ResourceId objId)
      : WrappedVkNonDispRes(obj, objId)
  {
  }
  typedef VkSamplerYcbcrConversion InnerType;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedVkSamplerYcbcrConversion);
  enum
  {
    TypeEnum = eResSamplerConversion,
  };
};

// VkDisplayKHR and VkDisplayModeKHR are both UNWRAPPED because there's no need to wrap them.
// The only thing we need to wrap VkSurfaceKHR for is to get back the window from it later.

// Note: we assume only the following resources can return duplicate handles (and so
// on replay we need to handle two distinct ids with the same handle.
// Other resources are discounted because they have 'state' or otherwise wouldn't make
// sense to alias. Some of these are very *unlikely* to alias/duplicate, but they could
// in theory do so e.g. if the inputs to the create function were hashed.
// Listed in order from least likely to most likely to alias:
//
// * VkPipeline
// * VkShaderModule
// * VkRenderPass
// * VkPipelineLayout
// * VkDescriptorSetLayout
// * VkFramebuffer
// * VkBufferView
// * VkImageView
// * VkSampler

// template magic voodoo to unwrap types
template <typename inner>
struct UnwrapHelper
{
};

#define UNWRAP_HELPER(vulkantype)                                             \
  template <>                                                                 \
  struct UnwrapHelper<vulkantype>                                             \
  {                                                                           \
    typedef WrappedVkDispRes ParentType;                                      \
    enum                                                                      \
    {                                                                         \
      DispatchableType = 1                                                    \
    };                                                                        \
    typedef CONCAT(Wrapped, vulkantype) Outer;                                \
    static TypedRealHandle ToTypedHandle(vulkantype real)                     \
    {                                                                         \
      TypedRealHandle h;                                                      \
      h.type = (VkResourceType)Outer::TypeEnum;                               \
      h.real = RealVkRes((void *)real);                                       \
      return h;                                                               \
    }                                                                         \
    static Outer *FromHandle(vulkantype wrapped) { return (Outer *)wrapped; } \
  };

#define UNWRAP_NONDISP_HELPER(vulkantype)                     \
  template <>                                                 \
  struct UnwrapHelper<vulkantype>                             \
  {                                                           \
    typedef WrappedVkNonDispRes ParentType;                   \
    enum                                                      \
    {                                                         \
      DispatchableType = 0                                    \
    };                                                        \
    typedef CONCAT(Wrapped, vulkantype) Outer;                \
    static TypedRealHandle ToTypedHandle(vulkantype real)     \
    {                                                         \
      TypedRealHandle h;                                      \
      h.type = (VkResourceType)Outer::TypeEnum;               \
      h.real = RealVkRes(NON_DISP_TO_UINT64(real));           \
      return h;                                               \
    }                                                         \
    static Outer *FromHandle(vulkantype wrapped)              \
    {                                                         \
      return (Outer *)(uintptr_t)NON_DISP_TO_UINT64(wrapped); \
    }                                                         \
  };

UNWRAP_HELPER(VkInstance)
UNWRAP_HELPER(VkPhysicalDevice)
UNWRAP_HELPER(VkDevice)
UNWRAP_HELPER(VkQueue)
UNWRAP_HELPER(VkCommandBuffer)
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
UNWRAP_NONDISP_HELPER(VkPipelineCache)
UNWRAP_NONDISP_HELPER(VkPipelineLayout)
UNWRAP_NONDISP_HELPER(VkRenderPass)
UNWRAP_NONDISP_HELPER(VkPipeline)
UNWRAP_NONDISP_HELPER(VkDescriptorSetLayout)
UNWRAP_NONDISP_HELPER(VkSampler)
UNWRAP_NONDISP_HELPER(VkDescriptorPool)
UNWRAP_NONDISP_HELPER(VkDescriptorSet)
UNWRAP_NONDISP_HELPER(VkFramebuffer)
UNWRAP_NONDISP_HELPER(VkCommandPool)
UNWRAP_NONDISP_HELPER(VkSwapchainKHR)
UNWRAP_NONDISP_HELPER(VkSurfaceKHR)
UNWRAP_NONDISP_HELPER(VkDescriptorUpdateTemplate)
UNWRAP_NONDISP_HELPER(VkSamplerYcbcrConversion)

// VkDisplayKHR and VkDisplayModeKHR are both UNWRAPPED because there's no need to wrap them.
// The only thing we need to wrap VkSurfaceKHR for is to get back the window from it later.

#define WRAPPING_DEBUG 0

template <typename RealType>
typename UnwrapHelper<RealType>::Outer *GetWrapped(RealType obj)
{
  if(obj == VK_NULL_HANDLE)
    return VK_NULL_HANDLE;

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

template <typename RealType>
typename UnwrapHelper<RealType>::Outer::DispatchTableType *LayerDisp(RealType obj)
{
  return (typename UnwrapHelper<RealType>::Outer::DispatchTableType *)GetWrapped(obj)->loaderTable;
}

template <typename RealType>
typename UnwrapHelper<RealType>::Outer::DispatchTableType *ObjDisp(RealType obj)
{
  return (typename UnwrapHelper<RealType>::Outer::DispatchTableType *)GetWrapped(obj)->table;
}

template <typename RealType>
void SetDispatchTableOverMagicNumber(VkDevice parent, RealType obj)
{
  // since we wrap this object, the loader won't have a chance to write the loader table into it
  // over the magic number. Instead, we do it ourselves.
  typename UnwrapHelper<RealType>::Outer *wrapped = GetWrapped(obj);
  if(wrapped->loaderTable == 0x01CDC0DE)
    wrapped->loaderTable = GetWrapped(parent)->loaderTable;
}

template <typename RealType>
bool IsDispatchable(RealType obj)
{
  return (UnwrapHelper<RealType>::DispatchableType) == 1;
}

template <typename RealType>
WrappedVulkan *CoreDisp(RealType obj)
{
  return (WrappedVulkan *)GetWrapped(obj)->core;
}

template <typename RealType>
RealType Unwrap(RealType obj)
{
  if(obj == VK_NULL_HANDLE)
    return VK_NULL_HANDLE;

  RealVkRes &res = GetWrapped(obj)->real;

  return res.As<RealType>();
}

template <typename RealType>
RealType *UnwrapPtr(RealType obj)
{
  if(obj == VK_NULL_HANDLE)
    return NULL;

  RealVkRes &res = GetWrapped(obj)->real;

  return res.AsPtr<RealType>();
}

template <typename RealType>
ResourceId GetResID(RealType obj)
{
  if(obj == VK_NULL_HANDLE)
    return ResourceId();

  return GetWrapped(obj)->id;
}

template <typename RealType>
VkResourceRecord *GetRecord(RealType obj)
{
  if(obj == VK_NULL_HANDLE)
    return NULL;

  return GetWrapped(obj)->record;
}

template <typename RealType>
RealType ToHandle(WrappedVkRes *ptr)
{
  RealVkRes &res = ((typename UnwrapHelper<RealType>::Outer *)ptr)->real;

  return res.As<RealType>();
}

template <typename RealType>
TypedRealHandle ToTypedHandle(RealType obj)
{
  return UnwrapHelper<RealType>::ToTypedHandle(obj);
}

template <typename parenttype, typename wrappedtype>
inline void SetTableIfDispatchable(bool writing, parenttype parent, WrappedVulkan *core,
                                   wrappedtype *obj)
{
}
template <>
inline void SetTableIfDispatchable(bool writing, VkInstance parent, WrappedVulkan *core,
                                   WrappedVkInstance *obj)
{
  SetDispatchTable(writing, parent, core, obj);
}
template <>
inline void SetTableIfDispatchable(bool writing, VkInstance parent, WrappedVulkan *core,
                                   WrappedVkPhysicalDevice *obj)
{
  SetDispatchTable(writing, parent, core, obj);
}
template <>
inline void SetTableIfDispatchable(bool writing, VkDevice parent, WrappedVulkan *core,
                                   WrappedVkDevice *obj)
{
  SetDispatchTable(writing, parent, core, obj);
}
template <>
inline void SetTableIfDispatchable(bool writing, VkDevice parent, WrappedVulkan *core,
                                   WrappedVkQueue *obj)
{
  SetDispatchTable(writing, parent, core, obj);
}
template <>
inline void SetTableIfDispatchable(bool writing, VkDevice parent, WrappedVulkan *core,
                                   WrappedVkCommandBuffer *obj)
{
  SetDispatchTable(writing, parent, core, obj);
}

bool IsDispatchableRes(WrappedVkRes *ptr);
VkResourceType IdentifyTypeByPtr(WrappedVkRes *ptr);

#define UNKNOWN_PREV_IMG_LAYOUT ((VkImageLayout)0xffffffff)

struct ImageRegionState
{
  ImageRegionState()
      : dstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED),
        oldLayout(UNKNOWN_PREV_IMG_LAYOUT),
        newLayout(UNKNOWN_PREV_IMG_LAYOUT)
  {
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 0;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 0;
  }
  ImageRegionState(uint32_t queueIndex, VkImageSubresourceRange r, VkImageLayout pr, VkImageLayout st)
      : dstQueueFamilyIndex(queueIndex), subresourceRange(r), oldLayout(pr), newLayout(st)
  {
  }

  uint32_t dstQueueFamilyIndex;
  VkImageSubresourceRange subresourceRange;
  VkImageLayout oldLayout;
  VkImageLayout newLayout;
};

DECLARE_REFLECTION_STRUCT(ImageRegionState);

struct SwapchainInfo
{
  VkFormat format;
  VkExtent2D extent;
  int arraySize;

  bool shared;

  VkRenderPass rp;

  RENDERDOC_WindowHandle wndHandle;

  struct SwapImage
  {
    VkImage im;

    VkImageView view;
    VkFramebuffer fb;
  };
  std::vector<SwapImage> images;
  uint32_t lastPresent;
};

struct ImageInfo
{
  int layerCount = 0;
  int levelCount = 0;
  int sampleCount = 0;
  VkExtent3D extent = {0, 0, 0};
  VkFormat format = VK_FORMAT_UNDEFINED;
  ImageInfo() {}
  ImageInfo(VkFormat format, VkExtent3D extent, int levelCount, int layerCount, int sampleCount)
      : format(format),
        extent(extent),
        levelCount(levelCount),
        layerCount(layerCount),
        sampleCount(sampleCount)
  {
  }
  ImageInfo(const VkImageCreateInfo &ci)
      : layerCount(ci.arrayLayers),
        levelCount(ci.mipLevels),
        sampleCount((int)ci.samples),
        extent(ci.extent),
        format(ci.format)
  {
    // The Vulkan spec Valid Usage for `VkImageCreateInfo` specifies that the height and depth of 1D
    // images, and the depth of 2D images must equal 1. We need to ensure this holds, even if the
    // application is invalid, since we rely on `depth>1` to detect 3D images and correctly handle
    // 2D views of 3D images.
    if(ci.imageType == VK_IMAGE_TYPE_1D)
    {
      extent.height = extent.depth = 1;
    }
    else if(ci.imageType == VK_IMAGE_TYPE_2D)
    {
      extent.depth = 1;
    }
  }
  ImageInfo(const SwapchainInfo &swapInfo)
      : layerCount(swapInfo.arraySize), levelCount(1), sampleCount(1), format(swapInfo.format)
  {
    extent.width = swapInfo.extent.width;
    extent.height = swapInfo.extent.height;
    extent.depth = 1;
  }
};

DECLARE_REFLECTION_STRUCT(ImageInfo);

// these structs are allocated for images and buffers, then pointed to (non-owning) by views
struct ResourceInfo
{
  ResourceInfo()
  {
    RDCEraseEl(imgdim);
    RDCEraseEl(pagedim);
    RDCEraseEl(pages);
  }

  // for buffers or non-sparse-resident images (bound with opaque mappings)
  std::vector<VkSparseMemoryBind> opaquemappings;

  VkMemoryRequirements memreqs;

  // for sparse resident images:
  // total image size (in pages)
  VkExtent3D imgdim;
  // size of a page
  VkExtent3D pagedim;
  // pagetable per image aspect (some may be NULL) color, depth, stencil, metadata
  // in order of width first, then height, then depth
  rdcpair<VkDeviceMemory, VkDeviceSize> *pages[NUM_VK_IMAGE_ASPECTS];

  ImageInfo imageInfo;

  bool IsSparse() const { return pages[0] != NULL; }
  void Update(uint32_t numBindings, const VkSparseMemoryBind *pBindings);
  void Update(uint32_t numBindings, const VkSparseImageMemoryBind *pBindings);
};

struct MemRefs;
struct ImgRefs;

struct CmdBufferRecordingInfo
{
  VkDevice device;
  VkCommandBufferAllocateInfo allocInfo;

  VkResourceRecord *framebuffer = NULL;
  VkResourceRecord *allocRecord = NULL;

  std::vector<rdcpair<ResourceId, ImageRegionState> > imgbarriers;

  // sparse resources referenced by this command buffer (at submit time
  // need to go through the sparse mapping and reference all memory)
  std::set<ResourceInfo *> sparse;

  // a list of all resources dirtied by this command buffer
  std::set<ResourceId> dirtied;

  // a list of descriptor sets that are bound at any point in this command buffer
  // used to look up all the frame refs per-desc set and apply them on queue
  // submit with latest binding refs.
  std::set<VkDescriptorSet> boundDescSets;

  std::vector<VkResourceRecord *> subcmds;

  std::map<ResourceId, ImgRefs> imgFrameRefs;
  std::map<ResourceId, MemRefs> memFrameRefs;

  // AdvanceFrame/Present should be called after this buffer is submitted
  bool present;
};

struct DescSetLayout;

struct DescriptorSetData
{
  DescriptorSetData() : layout(NULL) {}
  ~DescriptorSetData()
  {
    for(size_t i = 0; i < descBindings.size(); i++)
      delete[] descBindings[i];
    descBindings.clear();
  }

  DescSetLayout *layout;

  // descriptor set bindings for this descriptor set. Filled out on
  // create from the layout.
  std::vector<DescriptorSetBindingElement *> descBindings;

  // lock protecting bindFrameRefs and bindMemRefs
  Threading::CriticalSection refLock;

  // contains the framerefs (ref counted) for the bound resources
  // in the binding slots. Updated when updating descriptor sets
  // and then applied in a block on descriptor set bind.
  // the refcount has the high-bit set if this resource has sparse
  // mapping information
  static const uint32_t SPARSE_REF_BIT = 0x80000000;
  std::map<ResourceId, rdcpair<uint32_t, FrameRefType> > bindFrameRefs;
  std::map<ResourceId, MemRefs> bindMemRefs;
  std::map<ResourceId, ImgRefs> bindImgRefs;
};

struct PipelineLayoutData
{
  std::vector<DescSetLayout> layouts;
};

struct MemMapState
{
  MemMapState()
      : mapOffset(0),
        mapSize(0),
        needRefData(false),
        mapFlushed(false),
        mapCoherent(false),
        mappedPtr(NULL),
        refData(NULL)
  {
  }
  VkDeviceSize mapOffset, mapSize;
  bool needRefData;
  bool mapFlushed;
  bool mapCoherent;
  byte *mappedPtr;
  byte *refData;
};

struct AttachmentInfo
{
  VkResourceRecord *record;

  // the implicit barrier applied from initialLayout to finalLayout across a render pass
  // for render passes this is partial (doesn't contain the image pointer), the image
  // and subresource range are filled in when creating the framebuffer, which is what is
  // used to apply the barrier in EndRenderPass
  VkImageMemoryBarrier barrier;
};

struct ImageRange
{
  ImageRange() {}
  ImageRange(const VkImageSubresourceRange &range)
      : aspectMask(range.aspectMask),
        baseMipLevel(range.baseMipLevel),
        levelCount(range.levelCount),
        baseArrayLayer(range.baseArrayLayer),
        layerCount(range.layerCount)
  {
  }
  ImageRange(const VkImageSubresourceLayers &range)
      : aspectMask(range.aspectMask),
        baseMipLevel(range.mipLevel),
        levelCount(1),
        baseArrayLayer(range.baseArrayLayer),
        layerCount(range.layerCount)
  {
  }
  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
  uint32_t baseMipLevel = 0;
  uint32_t levelCount = VK_REMAINING_MIP_LEVELS;
  uint32_t baseArrayLayer = 0;
  uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS;
  VkOffset3D offset = {0, 0, 0};
  VkExtent3D extent = {~0u, ~0u, ~0u};
  VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
};

typedef BitFlagIterator<VkImageAspectFlagBits, VkImageAspectFlags, int32_t> ImageAspectFlagIter;

VkImageAspectFlags FormatImageAspects(VkFormat fmt);

struct ImgRefs
{
  std::vector<FrameRefType> rangeRefs;
  WrappedVkRes *initializedLiveRes = NULL;
  ImageInfo imageInfo;
  VkImageAspectFlags aspectMask;

  bool areAspectsSplit = false;
  bool areLevelsSplit = false;
  bool areLayersSplit = false;

  int GetAspectCount() const;

  ImgRefs() : initializedLiveRes(NULL) {}
  inline ImgRefs(const ImageInfo &imageInfo)
      : rangeRefs(1, eFrameRef_None),
        imageInfo(imageInfo),
        aspectMask(FormatImageAspects(imageInfo.format))
  {
    if(imageInfo.extent.depth > 1)
      // Depth slices of 3D views are treated as array layers
      this->imageInfo.layerCount = imageInfo.extent.depth;
  }
  int SubresourceIndex(VkImageAspectFlagBits aspect, int level, int layer) const;
  int SubresourceIndex(int aspectIndex, int level, int layer) const;
  inline FrameRefType SubresourceRef(VkImageAspectFlagBits aspect, int level, int layer) const
  {
    return rangeRefs[SubresourceIndex(aspect, level, layer)];
  }
  inline FrameRefType SubresourceRef(int aspectIndex, int level, int layer) const
  {
    return rangeRefs[SubresourceIndex(aspectIndex, level, layer)];
  }
  void Split(bool splitAspects, bool splitLevels, bool splitLayers);
  template <typename Compose>
  FrameRefType Update(ImageRange range, FrameRefType refType, Compose comp);
  inline FrameRefType Update(const ImageRange &range, FrameRefType refType)
  {
    return Update(range, refType, ComposeFrameRefs);
  }
  template <typename Compose>
  FrameRefType Merge(const ImgRefs &other, Compose comp);
  inline FrameRefType Merge(const ImgRefs &other) { return Merge(other, ComposeFrameRefs); }
};

DECLARE_REFLECTION_STRUCT(ImgRefs);

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, ImgRefs &el)
{
  SERIALISE_MEMBER(rangeRefs);
  SERIALISE_MEMBER(imageInfo);
  SERIALISE_MEMBER(aspectMask);
  SERIALISE_MEMBER(areAspectsSplit);
  SERIALISE_MEMBER(areLevelsSplit);
  SERIALISE_MEMBER(areLayersSplit);
}

struct ImgRefsPair
{
  ResourceId image;
  ImgRefs imgRefs;
};

DECLARE_REFLECTION_STRUCT(ImgRefsPair);

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, ImgRefsPair &el)
{
  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(imgRefs);
}

template <typename Compose>
FrameRefType ImgRefs::Update(ImageRange range, FrameRefType refType, Compose comp)
{
  range.extent.width = RDCMIN(range.extent.width, imageInfo.extent.width - range.offset.x);
  range.extent.height = RDCMIN(range.extent.height, imageInfo.extent.height - range.offset.y);

  if(imageInfo.extent.depth > 1 && range.viewType != VK_IMAGE_VIEW_TYPE_2D &&
     range.viewType != VK_IMAGE_VIEW_TYPE_2D_ARRAY)
  {
    // The Vulkan spec allows 2D `VkImageView`s of 3D `VkImage`s--the depth slices of the images are
    // interpreted as array layers in the 2D view. Note that the spec does not allow 3D images to
    // have array layers, so 3D images always have exactly 1 array layer when not accessed using a
    // 2D view.
    //
    // `ImgRefs` treats the depth slices of 3D images as array layers (as if accessed through a 2D
    // view). When a 3D image is accessed without a 2D view, we need to translate the Z axis into
    // array layer indices.

    range.extent.depth = RDCMIN(range.extent.depth, imageInfo.extent.depth - range.offset.z);
    range.baseArrayLayer = range.offset.z;
    range.layerCount = range.extent.depth;
  }
  else if(range.layerCount == VK_REMAINING_ARRAY_LAYERS)
  {
    range.layerCount = imageInfo.layerCount - range.baseArrayLayer;
  }

  if(range.levelCount == VK_REMAINING_MIP_LEVELS)
    range.levelCount = imageInfo.levelCount - range.baseMipLevel;

  if(refType == eFrameRef_CompleteWrite &&
     (range.offset.x != 0 || range.offset.y != 0 || range.extent.width != imageInfo.extent.width ||
      range.extent.height != imageInfo.extent.height))
    // Complete write, but only to part of the image.
    // We don't track writes at the pixel level, so turn this into a partial write
    refType = eFrameRef_PartialWrite;

  if(range.aspectMask != aspectMask)
  {
    if(range.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
    {
      // For multi-planar images, the color aspect can be an alias for the plane aspects
      range.aspectMask |=
          (VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT | VK_IMAGE_ASPECT_PLANE_2_BIT) &
          aspectMask;
    }
    range.aspectMask &= aspectMask;
  }

  Split(range.aspectMask != aspectMask,
        range.baseMipLevel != 0 || (int)range.levelCount != imageInfo.levelCount,
        range.baseArrayLayer != 0 || (int)range.layerCount != imageInfo.layerCount);

  std::vector<VkImageAspectFlags> splitAspects;
  if(areAspectsSplit)
  {
    for(auto aspectIt = ImageAspectFlagIter::begin(aspectMask);
        aspectIt != ImageAspectFlagIter::end(); ++aspectIt)
    {
      splitAspects.push_back(*aspectIt);
    }
  }
  else
  {
    splitAspects.push_back(aspectMask);
  }

  int splitLevelCount = 1;
  int levelEnd = 1;
  if(areLevelsSplit)
  {
    splitLevelCount = imageInfo.levelCount;
    levelEnd = (int)(range.baseMipLevel + range.levelCount);
  }

  int splitLayerCount = 1;
  int layerEnd = 1;
  if(areLayersSplit)
  {
    splitLayerCount = imageInfo.layerCount;
    layerEnd = (int)(range.baseArrayLayer + range.layerCount);
  }

  FrameRefType maxRefType = eFrameRef_None;
  for(int aspectIndex = 0; aspectIndex < (int)splitAspects.size(); ++aspectIndex)
  {
    VkImageAspectFlags aspect = splitAspects[aspectIndex];
    if((aspect & range.aspectMask) == 0)
      continue;
    for(int level = (int)range.baseMipLevel; level < levelEnd; ++level)
    {
      for(int layer = (int)range.baseArrayLayer; layer < layerEnd; ++layer)
      {
        int index = (aspectIndex * splitLevelCount + level) * splitLayerCount + layer;
        rangeRefs[index] = comp(rangeRefs[index], refType);
        maxRefType = RDCMAX(maxRefType, rangeRefs[index]);
      }
    }
  }
  return maxRefType;
}

template <typename Compose>
FrameRefType ImgRefs::Merge(const ImgRefs &other, Compose comp)
{
  Split(other.areAspectsSplit, other.areLevelsSplit, other.areLayersSplit);

  int splitAspectCount = 1;
  if(areAspectsSplit)
    splitAspectCount = GetAspectCount();

  int splitLevelCount = areLevelsSplit ? imageInfo.levelCount : 1;

  int splitLayerCount = areLayersSplit ? imageInfo.layerCount : 1;

  FrameRefType maxRefType = eFrameRef_None;
  for(int aspectIndex = 0; aspectIndex < splitAspectCount; ++aspectIndex)
  {
    for(int level = 0; level < splitLevelCount; ++level)
    {
      for(int layer = 0; layer < splitLayerCount; ++layer)
      {
        int index = SubresourceIndex(aspectIndex, level, layer);
        rangeRefs[index] = comp(rangeRefs[index], other.SubresourceRef(aspectIndex, level, layer));
        maxRefType = RDCMAX(maxRefType, rangeRefs[index]);
      }
    }
  }
  return maxRefType;
}

struct MemRefs
{
  Intervals<FrameRefType> rangeRefs;
  WrappedVkRes *initializedLiveRes;
  inline MemRefs() : initializedLiveRes(NULL) {}
  inline MemRefs(VkDeviceSize offset, VkDeviceSize size, FrameRefType refType)
      : initializedLiveRes(NULL)
  {
    rangeRefs.update(offset, offset + size, refType, ComposeFrameRefs);
  }
  template <typename Compose>
  FrameRefType Update(VkDeviceSize offset, VkDeviceSize size, FrameRefType refType, Compose comp);
  inline FrameRefType Update(VkDeviceSize offset, VkDeviceSize size, FrameRefType refType)
  {
    return Update(offset, size, refType, ComposeFrameRefs);
  }
  template <typename Compose>
  FrameRefType Merge(MemRefs &other, Compose comp);
  inline FrameRefType Merge(MemRefs &other) { return Merge(other, ComposeFrameRefs); }
};

template <typename Compose>
FrameRefType MemRefs::Update(VkDeviceSize offset, VkDeviceSize size, FrameRefType refType,
                             Compose comp)
{
  FrameRefType maxRefType = eFrameRef_None;
  rangeRefs.update(offset, offset + size, refType,
                   [&maxRefType, comp](FrameRefType oldRef, FrameRefType newRef) -> FrameRefType {
                     FrameRefType ref = comp(oldRef, newRef);
                     maxRefType = std::max(maxRefType, ref);
                     return ref;
                   });
  return maxRefType;
}

template <typename Compose>
FrameRefType MemRefs::Merge(MemRefs &other, Compose comp)
{
  FrameRefType maxRefType = eFrameRef_None;
  rangeRefs.merge(other.rangeRefs,
                  [&maxRefType, comp](FrameRefType oldRef, FrameRefType newRef) -> FrameRefType {
                    FrameRefType ref = comp(oldRef, newRef);
                    maxRefType = std::max(maxRefType, ref);
                    return ref;
                  });
  return maxRefType;
}

struct ImageLayouts;

template <typename Compose>
FrameRefType MarkImageReferenced(std::map<ResourceId, ImgRefs> &imgRefs, ResourceId img,
                                 const ImageInfo &imageInfo, const ImageRange &range,
                                 FrameRefType refType, Compose comp);

inline FrameRefType MarkImageReferenced(std::map<ResourceId, ImgRefs> &imgRefs, ResourceId img,
                                        const ImageInfo &imageInfo, const ImageRange &range,
                                        FrameRefType refType)
{
  return MarkImageReferenced(imgRefs, img, imageInfo, range, refType, ComposeFrameRefs);
}

template <typename Compose>
FrameRefType MarkMemoryReferenced(std::map<ResourceId, MemRefs> &memRefs, ResourceId mem,
                                  VkDeviceSize offset, VkDeviceSize size, FrameRefType refType,
                                  Compose comp)
{
  if(refType == eFrameRef_None)
    return refType;
  auto refs = memRefs.find(mem);
  if(refs == memRefs.end())
  {
    memRefs.insert(std::pair<ResourceId, MemRefs>(mem, MemRefs(offset, size, refType)));
    return refType;
  }
  else
  {
    return refs->second.Update(offset, size, refType, comp);
  }
}

inline FrameRefType MarkMemoryReferenced(std::map<ResourceId, MemRefs> &memRefs, ResourceId mem,
                                         VkDeviceSize offset, VkDeviceSize size, FrameRefType refType)
{
  return MarkMemoryReferenced(memRefs, mem, offset, size, refType, ComposeFrameRefs);
}

struct DescUpdateTemplate;
struct ImageLayouts;

struct VkResourceRecord : public ResourceRecord
{
public:
  enum
  {
    NullResource = VK_NULL_HANDLE
  };

  static byte markerValue[32];

  VkResourceRecord(ResourceId id)
      : ResourceRecord(id, true),
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
    cmdInfo->imgFrameRefs.swap(bakedCommands->cmdInfo->imgFrameRefs);
    cmdInfo->memFrameRefs.swap(bakedCommands->cmdInfo->memFrameRefs);
  }

  void AddBindFrameRef(ResourceId id, FrameRefType ref, bool hasSparse = false)
  {
    if(id == ResourceId())
    {
      RDCERR("Unexpected NULL resource ID being added as a bind frame ref");
      return;
    }
    rdcpair<uint32_t, FrameRefType> &p = descInfo->bindFrameRefs[id];
    if((p.first & ~DescriptorSetData::SPARSE_REF_BIT) == 0)
    {
      p.second = ref;
      p.first = 1 | (hasSparse ? DescriptorSetData::SPARSE_REF_BIT : 0);
    }
    else
    {
      // be conservative - mark refs as read before write if we see a write and a read ref on it
      p.second = ComposeFrameRefsUnordered(descInfo->bindFrameRefs[id].second, ref);
      p.first++;
      p.first |= (hasSparse ? DescriptorSetData::SPARSE_REF_BIT : 0);
    }
  }

  void AddImgFrameRef(VkResourceRecord *view, FrameRefType refType)
  {
    AddBindFrameRef(view->GetResourceID(), eFrameRef_Read,
                    view->resInfo && view->resInfo->IsSparse());
    if(view->baseResourceMem != ResourceId())
      AddBindFrameRef(view->baseResourceMem, eFrameRef_Read, false);

    rdcpair<uint32_t, FrameRefType> &p = descInfo->bindFrameRefs[view->baseResource];
    if((p.first & ~DescriptorSetData::SPARSE_REF_BIT) == 0)
    {
      descInfo->bindImgRefs.erase(view->baseResource);
      p.first = 1;
      p.second = eFrameRef_None;
    }
    else
    {
      p.first++;
    }

    ImageRange imgRange;
    imgRange.aspectMask = view->viewRange.aspectMask;
    imgRange.baseMipLevel = view->viewRange.baseMipLevel;
    imgRange.levelCount = view->viewRange.levelCount;
    imgRange.baseArrayLayer = view->viewRange.baseArrayLayer;
    imgRange.layerCount = view->viewRange.layerCount;
    imgRange.viewType = view->viewRange.viewType();

    FrameRefType maxRef = MarkImageReferenced(descInfo->bindImgRefs, view->baseResource,
                                              view->resInfo->imageInfo, imgRange, refType);

    p.second = std::max(p.second, maxRef);
  }

  void AddMemFrameRef(ResourceId mem, VkDeviceSize offset, VkDeviceSize size, FrameRefType refType)
  {
    if(mem == ResourceId())
    {
      RDCERR("Unexpected NULL resource ID being added as a bind frame ref");
      return;
    }
    rdcpair<uint32_t, FrameRefType> &p = descInfo->bindFrameRefs[mem];
    if((p.first & ~DescriptorSetData::SPARSE_REF_BIT) == 0)
    {
      descInfo->bindMemRefs.erase(mem);
      p.first = 1;
      p.second = eFrameRef_None;
    }
    else
    {
      p.first++;
    }
    FrameRefType maxRef = MarkMemoryReferenced(descInfo->bindMemRefs, mem, offset, size, refType,
                                               ComposeFrameRefsUnordered);
    p.second = std::max(p.second, maxRef);
  }

  void RemoveBindFrameRef(ResourceId id)
  {
    // ignore any NULL IDs - probably an object that was
    // deleted since it was bound.
    if(id == ResourceId())
      return;

    auto it = descInfo->bindFrameRefs.find(id);

    // in the case of re-used handles bound to descriptor sets,
    // it's possible to try and remove a frameref on something we
    // don't have (which means we'll have a corresponding stale ref)
    // but this is harmless so we can ignore it.
    if(it == descInfo->bindFrameRefs.end())
      return;

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
  ResourceId baseResourceMem;    // for image views, we need to point to both the image and mem

  VkDeviceSize memOffset;
  VkDeviceSize memSize;

  void MarkMemoryFrameReferenced(ResourceId mem, VkDeviceSize offset, VkDeviceSize size,
                                 FrameRefType refType);
  void MarkImageFrameReferenced(VkResourceRecord *img, const ImageRange &range, FrameRefType refType);
  void MarkImageViewFrameReferenced(VkResourceRecord *view, const ImageRange &range,
                                    FrameRefType refType);
  void MarkBufferFrameReferenced(VkResourceRecord *buf, VkDeviceSize offset, VkDeviceSize size,
                                 FrameRefType refType);
  void MarkBufferImageCopyFrameReferenced(VkResourceRecord *buf, VkResourceRecord *img,
                                          uint32_t regionCount, const VkBufferImageCopy *regions,
                                          FrameRefType bufRefType, FrameRefType imgRefType);
  void MarkBufferViewFrameReferenced(VkResourceRecord *buf, FrameRefType refType);
  // these are all disjoint, so only a record of the right type will have each
  // Note some of these need to be deleted in the constructor, so we check the
  // allocation type of the Resource
  union
  {
    void *ptrunion;                                // for initialisation to NULL
    VkPhysicalDeviceMemoryProperties *memProps;    // only for physical devices
    InstanceDeviceInfo *instDevInfo;               // only for logical devices or instances
    ResourceInfo *resInfo;                         // only for buffers, images, and views of them
    SwapchainInfo *swapInfo;                       // only for swapchains
    MemMapState *memMapState;                      // only for device memory
    CmdBufferRecordingInfo *cmdInfo;               // only for command buffers
    AttachmentInfo *imageAttachments;              // only for framebuffers and render passes
    PipelineLayoutData *pipeLayoutInfo;            // only for pipeline layouts
    DescriptorSetData *descInfo;             // only for descriptor sets and descriptor set layouts
    DescUpdateTemplate *descTemplateInfo;    // only for descriptor update templates
    uint32_t queueFamilyIndex;               // only for queues
  };

  VkResourceRecord *bakedCommands;

  // pointer to either the pool this item is allocated from, or the children allocated
  // from this pool. Protected by the chunk lock
  VkResourceRecord *pool;
  std::vector<VkResourceRecord *> pooledChildren;

  // we only need a couple of bytes to store the view's range,
  // so just pack/unpack into bitfields
  struct ViewRange
  {
    ViewRange()
    {
      aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      baseMipLevel = 0;
      levelCount = 1;
      baseArrayLayer = 0;
      layerCount = 1;
      packedViewType = 7;
    }

    ViewRange &operator=(const VkImageSubresourceRange &range)
    {
      aspectMask = (uint32_t)range.aspectMask;
      baseMipLevel = range.baseMipLevel;
      baseArrayLayer = range.baseArrayLayer;

      if(range.levelCount == VK_REMAINING_MIP_LEVELS)
        levelCount = MipMaxValue;
      else
        levelCount = range.levelCount;

      if(range.layerCount == VK_REMAINING_ARRAY_LAYERS)
        layerCount = SliceMaxValue;
      else
        layerCount = range.layerCount;

      return *this;
    }

    operator VkImageSubresourceRange() const
    {
      VkImageSubresourceRange ret;

      ret.aspectMask = (VkImageAspectFlags)aspectMask;
      ret.baseMipLevel = baseMipLevel;
      ret.baseArrayLayer = baseArrayLayer;

      if(levelCount == MipMaxValue)
        ret.levelCount = VK_REMAINING_MIP_LEVELS;
      else
        ret.levelCount = levelCount;

      if(layerCount == SliceMaxValue)
        ret.layerCount = VK_REMAINING_ARRAY_LAYERS;
      else
        ret.layerCount = layerCount;

      return ret;
    }

    inline VkImageViewType viewType() const
    {
      if(packedViewType <= VK_IMAGE_VIEW_TYPE_END_RANGE)
        return (VkImageViewType)packedViewType;
      else
        return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }

    inline void setViewType(VkImageViewType t)
    {
      if(t <= VK_IMAGE_VIEW_TYPE_END_RANGE)
        packedViewType = t;
      else
        packedViewType = 7;
    }

    // View type (VkImageViewType).
    // Values <= 6, fits in 3 bits; 7 encodes an unknown/uninitialized view type.
    // Stored as uint32_t instead of VkImageViewType to prevent signed extension.
    uint32_t packedViewType : 3;

    // only need 4 bits for the aspects
    uint32_t aspectMask : 4;

    // 6 bits = refer to up to 62 mips = bloody huge textures.
    // note we also need to pack in VK_REMAINING_MIPS etc so we can't
    // necessarily use the maximum levelCount value (64)
    uint32_t baseMipLevel : 6;
    uint32_t levelCount : 6;

    static const uint32_t MipMaxValue = 0x3f;

    // 16 bits = refer to up to 64k array layers. This is less
    // future proof than above, but at time of writing typical
    // maxImageArrayLayers is 2048.
    uint32_t baseArrayLayer : 16;
    uint32_t layerCount : 16;

    static const uint32_t SliceMaxValue = 0xffff;
  } viewRange;
};

struct ImageLayouts
{
  uint32_t queueFamilyIndex = 0;
  std::vector<ImageRegionState> subresourceStates;
  bool memoryBound = false;
  VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ImageInfo imageInfo;
};

DECLARE_REFLECTION_STRUCT(ImageLayouts);

bool IsBlockFormat(VkFormat f);
bool IsDepthOrStencilFormat(VkFormat f);
bool IsDepthAndStencilFormat(VkFormat f);
bool IsDepthOnlyFormat(VkFormat f);
bool IsStencilFormat(VkFormat f);
bool IsStencilOnlyFormat(VkFormat f);
bool IsSRGBFormat(VkFormat f);
bool IsUIntFormat(VkFormat f);
bool IsDoubleFormat(VkFormat f);
bool IsSIntFormat(VkFormat f);
bool IsYUVFormat(VkFormat f);
VkImageAspectFlags FormatImageAspects(VkFormat f);

uint32_t GetYUVPlaneCount(VkFormat f);
uint32_t GetYUVNumRows(VkFormat f, uint32_t height);
VkFormat GetYUVViewPlaneFormat(VkFormat f, uint32_t plane);
VkFormat GetDepthOnlyFormat(VkFormat f);
VkFormat GetViewCastedFormat(VkFormat f, CompType typeHint);

void GetYUVShaderParameters(VkFormat f, Vec4u &YUVDownsampleRate, Vec4u &YUVAChannels);

uint32_t GetByteSize(uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format, uint32_t mip);
uint32_t GetPlaneByteSize(uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format,
                          uint32_t mip, uint32_t plane);

template <typename Compose>
FrameRefType MarkImageReferenced(std::map<ResourceId, ImgRefs> &imgRefs, ResourceId img,
                                 const ImageInfo &imageInfo, const ImageRange &range,
                                 FrameRefType refType, Compose comp)
{
  if(refType == eFrameRef_None)
    return refType;
  auto refs = imgRefs.find(img);
  if(refs == imgRefs.end())
  {
    refs = imgRefs.insert(std::make_pair(img, ImgRefs(imageInfo))).first;
  }
  return refs->second.Update(range, refType, comp);
}
