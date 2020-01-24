/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
#include "vk_dispatch_defs.h"
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
  typedef VkInstDispatchTable DispatchTableType;
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
  typedef VkInstDispatchTable DispatchTableType;
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
  typedef VkDevDispatchTable DispatchTableType;
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
  typedef VkDevDispatchTable DispatchTableType;
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
  typedef VkDevDispatchTable DispatchTableType;
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
RealType ToUnwrappedHandle(WrappedVkRes *ptr)
{
  RealVkRes &res = ((typename UnwrapHelper<RealType>::Outer *)ptr)->real;

  return res.As<RealType>();
}

template <typename RealType>
RealType ToWrappedHandle(WrappedVkRes *ptr)
{
  return RealType(uint64_t(ptr));
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

VkImageAspectFlags FormatImageAspects(VkFormat f);

struct ImageSubresourceRange;

struct ImageInfo
{
  int layerCount = 0;
  int levelCount = 0;
  int sampleCount = 0;
  VkExtent3D extent = {0, 0, 0};
  VkImageType imageType = VK_IMAGE_TYPE_2D;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ImageInfo() {}
  ImageInfo(VkFormat format, VkExtent3D extent, int levelCount, int layerCount, int sampleCount,
            VkImageLayout initialLayout, VkSharingMode sharingMode)
      : format(format),
        extent(extent),
        levelCount(levelCount),
        layerCount(layerCount),
        sampleCount(sampleCount),
        initialLayout(initialLayout),
        sharingMode(sharingMode)
  {
  }
  ImageInfo(const VkImageCreateInfo &ci)
      : layerCount(ci.arrayLayers),
        levelCount(ci.mipLevels),
        sampleCount((int)ci.samples),
        extent(ci.extent),
        imageType(ci.imageType),
        format(ci.format),
        initialLayout(ci.initialLayout),
        sharingMode(ci.sharingMode)
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
  ImageInfo(const VkSwapchainCreateInfoKHR &ci)
      : layerCount(ci.imageArrayLayers),
        levelCount(1),
        sampleCount(1),
        format(ci.imageFormat),
        sharingMode(ci.imageSharingMode)
  {
    extent.width = ci.imageExtent.width;
    extent.height = ci.imageExtent.height;
    extent.depth = 1;
  }
  VkImageAspectFlags Aspects() const { return FormatImageAspects(format); }
  ImageSubresourceRange FullRange() const;
  inline bool operator==(const ImageInfo &other) const
  {
    return layerCount == other.layerCount && levelCount == other.levelCount &&
           sampleCount == other.sampleCount && extent.width == other.extent.width &&
           extent.height == other.extent.height && extent.depth == other.extent.depth &&
           imageType == other.imageType && format == other.format &&
           initialLayout == other.initialLayout && sharingMode == other.sharingMode;
  }
};

DECLARE_REFLECTION_STRUCT(ImageInfo);

struct PresentInfo
{
  VkQueue presentQueue;
  rdcarray<VkSemaphore> waitSemaphores;
  uint32_t imageIndex;
};

struct SwapchainInfo
{
  ImageInfo imageInfo;

  bool shared;

  VkRenderPass rp;

  RENDERDOC_WindowHandle wndHandle;

  struct SwapImage
  {
    VkImage im;

    VkImageView view;
    VkFramebuffer fb;
  };
  rdcarray<SwapImage> images;
  PresentInfo lastPresent;
};

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
  rdcarray<VkSparseMemoryBind> opaquemappings;

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

  rdcarray<rdcpair<ResourceId, ImageRegionState> > imgbarriers;

  // sparse resources referenced by this command buffer (at submit time
  // need to go through the sparse mapping and reference all memory)
  std::set<ResourceInfo *> sparse;

  // a list of all resources dirtied by this command buffer
  std::set<ResourceId> dirtied;

  // a list of descriptor sets that are bound at any point in this command buffer
  // used to look up all the frame refs per-desc set and apply them on queue
  // submit with latest binding refs.
  std::set<VkDescriptorSet> boundDescSets;

  // barriers to apply when the current render pass ends. Calculated at begin time in case the
  // framebuffer is imageless and we need to use the image views passed in at begin time to
  // construct the proper barriers.
  rdcarray<VkImageMemoryBarrier> rpbarriers;

  rdcarray<VkResourceRecord *> subcmds;

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
  rdcarray<DescriptorSetSlot *> descBindings;

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
  rdcarray<DescSetLayout> layouts;
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
  ImageRange(const VkBufferImageCopy &range)
      : aspectMask(range.imageSubresource.aspectMask),
        baseMipLevel(range.imageSubresource.mipLevel),
        levelCount(1),
        baseArrayLayer(range.imageSubresource.baseArrayLayer),
        layerCount(range.imageSubresource.layerCount),
        offset(range.imageOffset),
        extent(range.imageExtent)
  {
  }
  inline operator VkImageSubresourceRange() const
  {
    return {aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount};
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

bool IntervalsOverlap(uint32_t base1, uint32_t count1, uint32_t base2, uint32_t count2);

bool IntervalContainedIn(uint32_t base1, uint32_t count1, uint32_t base2, uint32_t count2);

bool SanitiseLevelRange(uint32_t &baseMipLevel, uint32_t &levelCount, uint32_t imageLevelCount);
bool SanitiseLayerRange(uint32_t &baseArrayLayer, uint32_t &layerCount, uint32_t imageLayerCount);
bool SanitiseSliceRange(uint32_t &baseSlice, uint32_t &sliceCount, uint32_t imageSliceCount);

struct ImageSubresourceRange
{
  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM >> 1;
  uint32_t baseMipLevel = 0;
  uint32_t levelCount = VK_REMAINING_MIP_LEVELS;
  uint32_t baseArrayLayer = 0;
  uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS;
  uint32_t baseDepthSlice = 0;
  uint32_t sliceCount = VK_REMAINING_ARRAY_LAYERS;

  inline ImageSubresourceRange() {}
  inline ImageSubresourceRange(const VkImageSubresourceRange &range)
      : aspectMask(range.aspectMask),
        baseMipLevel(range.baseMipLevel),
        levelCount(range.levelCount),
        baseArrayLayer(range.baseArrayLayer),
        layerCount(range.layerCount)
  {
  }
  inline ImageSubresourceRange(const VkImageSubresourceLayers &range)
      : aspectMask(range.aspectMask),
        baseMipLevel(range.mipLevel),
        levelCount(1),
        baseArrayLayer(range.baseArrayLayer),
        layerCount(range.layerCount)
  {
  }
  inline ImageSubresourceRange(const VkBufferImageCopy &range)
      : aspectMask(range.imageSubresource.aspectMask),
        baseMipLevel(range.imageSubresource.mipLevel),
        levelCount(1),
        baseArrayLayer(range.imageSubresource.baseArrayLayer),
        layerCount(range.imageSubresource.layerCount),
        baseDepthSlice(range.imageOffset.z),
        sliceCount(range.imageExtent.depth)
  {
  }
  inline ImageSubresourceRange(VkImageAspectFlags aspectMask, uint32_t baseMipLevel,
                               uint32_t levelCount, uint32_t baseArrayLayer, uint32_t layerCount,
                               uint32_t baseDepthSlice, uint32_t sliceCount)
      : aspectMask(aspectMask),
        baseMipLevel(baseMipLevel),
        levelCount(levelCount),
        baseArrayLayer(baseArrayLayer),
        layerCount(layerCount),
        baseDepthSlice(baseDepthSlice),
        sliceCount(sliceCount)
  {
  }
  inline ImageSubresourceRange(const ImageRange &other)
      : aspectMask(other.aspectMask),
        baseMipLevel(other.baseMipLevel),
        levelCount(other.levelCount),
        baseArrayLayer(other.baseArrayLayer),
        layerCount(other.layerCount),
        baseDepthSlice(other.offset.z),
        sliceCount(other.extent.depth)
  {
  }
  inline operator VkImageSubresourceRange() const
  {
    return {
        aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount,
    };
  }
  inline bool operator==(const ImageSubresourceRange &other) const
  {
    return aspectMask == other.aspectMask && baseMipLevel == other.baseMipLevel &&
           levelCount == other.levelCount && baseArrayLayer == other.baseArrayLayer &&
           layerCount == other.layerCount && baseDepthSlice == other.baseDepthSlice &&
           sliceCount == other.sliceCount;
  }

  inline bool operator!=(const ImageSubresourceRange &other) const { return !(*this == other); }
  inline bool Overlaps(const ImageSubresourceRange &other) const
  {
    return ((aspectMask & other.aspectMask) != 0) &&
           IntervalsOverlap(baseMipLevel, levelCount, other.baseMipLevel, other.levelCount) &&
           IntervalsOverlap(baseArrayLayer, layerCount, other.baseArrayLayer, other.layerCount) &&
           IntervalsOverlap(baseDepthSlice, sliceCount, other.baseDepthSlice, other.sliceCount);
  }
  inline bool ContainedIn(const ImageSubresourceRange &other) const
  {
    return ((aspectMask & ~other.aspectMask) == 0) &&
           IntervalContainedIn(baseMipLevel, levelCount, other.baseMipLevel, other.levelCount) &&
           IntervalContainedIn(baseArrayLayer, layerCount, other.baseArrayLayer, other.layerCount) &&
           IntervalContainedIn(baseDepthSlice, sliceCount, other.baseDepthSlice, other.sliceCount);
  }
  inline bool Contains(const ImageSubresourceRange &other) const
  {
    return other.ContainedIn(*this);
  }
  void Sanitise(const ImageInfo &info)
  {
    if(aspectMask & ~info.Aspects())
    {
      if(aspectMask != VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM)
      {
        RDCERR("Invalid aspect mask (%s) in image with aspects (%s)", ToStr(aspectMask).c_str(),
               ToStr(info.Aspects()).c_str());
      }
      aspectMask &= ~info.Aspects();
    }
    SanitiseLevelRange(baseMipLevel, levelCount, info.levelCount);
    SanitiseLayerRange(baseArrayLayer, layerCount, info.layerCount);
    SanitiseSliceRange(baseDepthSlice, sliceCount, info.extent.depth);
  }
};

struct ImageSubresourceState
{
  uint32_t oldQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  uint32_t newQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  VkImageLayout oldLayout = UNKNOWN_PREV_IMG_LAYOUT;
  VkImageLayout newLayout = UNKNOWN_PREV_IMG_LAYOUT;
  FrameRefType refType = eFrameRef_None;

  inline ImageSubresourceState() {}
  inline ImageSubresourceState(const VkImageMemoryBarrier &barrier)
      : oldQueueFamilyIndex(barrier.srcQueueFamilyIndex),
        newQueueFamilyIndex(barrier.dstQueueFamilyIndex),
        oldLayout(barrier.oldLayout),
        newLayout(barrier.newLayout)
  {
  }
  inline ImageSubresourceState(uint32_t queueFamilyIndex, VkImageLayout layout,
                               FrameRefType refType = eFrameRef_None)
      : oldQueueFamilyIndex(queueFamilyIndex),
        newQueueFamilyIndex(queueFamilyIndex),
        oldLayout(layout),
        newLayout(layout),
        refType(refType)
  {
  }
  inline bool operator==(const ImageSubresourceState &other) const
  {
    return oldQueueFamilyIndex == other.oldQueueFamilyIndex &&
           newQueueFamilyIndex == other.newQueueFamilyIndex && oldLayout == other.oldLayout &&
           newLayout == other.newLayout && refType == other.refType;
  }
  inline bool operator!=(const ImageSubresourceState &other) const { return !(*this == other); }
  void Update(const ImageSubresourceState &other, FrameRefCompFunc compose);
  bool Update(const ImageSubresourceState &other, ImageSubresourceState &result,
              FrameRefCompFunc compose) const;
};

class ImageSubresourceMap
{
  friend class SubresourceRangeIterator;

  ImageInfo m_imageInfo;

  // The states of the subresources, without explicit ranges.
  // The ranges associated with each state are determined by the index in
  // `m_values` and the `*Split` flags in `m_flags`.
  rdcarray<ImageSubresourceState> m_values;

  // The bit count of `m_aspectMask`
  uint16_t m_aspectCount = 0;

  enum class FlagBits : uint16_t
  {
    AreAspectsSplit = 0x1,
    AreLevelsSplit = 0x2,
    AreLayersSplit = 0x4,
    IsDepthSplit = 0x8,
    IsUninitialized = 0x8000,
  };
  uint16_t m_flags = 0;

  inline static bool AreAspectsSplit(uint16_t flags)
  {
    return (flags & (uint16_t)FlagBits::AreAspectsSplit) != 0;
  }
  inline static bool AreLevelsSplit(uint16_t flags)
  {
    return (flags & (uint16_t)FlagBits::AreLevelsSplit) != 0;
  }
  inline static bool AreLayersSplit(uint16_t flags)
  {
    return (flags & (uint16_t)FlagBits::AreLayersSplit) != 0;
  }
  inline static bool IsDepthSplit(uint16_t flags)
  {
    return (flags & (uint16_t)FlagBits::IsDepthSplit) != 0;
  }
  inline bool AreAspectsSplit() const { return AreAspectsSplit(m_flags); }
  inline bool AreLevelsSplit() const { return AreLevelsSplit(m_flags); }
  inline bool AreLayersSplit() const { return AreLayersSplit(m_flags); }
  inline bool IsDepthSplit() const { return IsDepthSplit(m_flags); }
  void Split(bool splitAspects, bool splitLevels, bool splitLayers, bool splitDepth);
  void Unsplit(bool unsplitAspects, bool unsplitLevels, bool unsplitLayers, bool unsplitDepth);
  size_t SubresourceIndex(uint32_t aspectIndex, uint32_t level, uint32_t layer, uint32_t z) const;

public:
  inline const ImageInfo &GetImageInfo() const { return m_imageInfo; }
  inline ImageSubresourceMap() {}
  inline ImageSubresourceMap(const ImageInfo &imageInfo, FrameRefType refType)
      : m_imageInfo(imageInfo)
  {
    for(auto it = ImageAspectFlagIter::begin(GetImageInfo().Aspects());
        it != ImageAspectFlagIter::end(); ++it)
      ++m_aspectCount;
    m_values.push_back(
        ImageSubresourceState(VK_QUEUE_FAMILY_IGNORED, UNKNOWN_PREV_IMG_LAYOUT, refType));
  }

  inline ImageSubresourceState &SubresourceValue(uint32_t aspectIndex, uint32_t level,
                                                 uint32_t layer, uint32_t slice)
  {
    return m_values[SubresourceIndex(aspectIndex, level, layer, slice)];
  }
  inline ImageSubresourceState &SubresourceValue(VkImageAspectFlagBits aspect, uint32_t level,
                                                 uint32_t layer, uint32_t slice)
  {
    uint32_t aspectIndex = 0;
    for(auto it = ImageAspectFlagIter::begin(GetImageInfo().Aspects());
        it != ImageAspectFlagIter::end() && *it != aspect; ++it, ++aspectIndex)
    {
    }
    return SubresourceValue(aspectIndex, level, layer, slice);
  }

  inline const ImageSubresourceState &SubresourceValue(uint32_t aspectIndex, uint32_t level,
                                                       uint32_t layer, uint32_t slice) const
  {
    return m_values[SubresourceIndex(aspectIndex, level, layer, slice)];
  }
  inline void Split(const ImageSubresourceRange &range)
  {
    Split(range.aspectMask != GetImageInfo().Aspects(),
          range.baseMipLevel != 0u || range.levelCount < (uint32_t)GetImageInfo().levelCount,
          range.baseArrayLayer != 0u || range.layerCount < (uint32_t)GetImageInfo().layerCount,
          range.baseDepthSlice != 0u || range.sliceCount < GetImageInfo().extent.depth);
  }
  void Unsplit();
  inline void Clear()
  {
    m_values.clear();
    m_values.resize(1);
    m_flags = 0;
  }
  FrameRefType Merge(const ImageSubresourceMap &other, FrameRefCompFunc compose);

  template <typename Map, typename Pair>
  class SubresourceRangeIterTemplate
  {
  public:
    inline bool operator==(const SubresourceRangeIterTemplate &other)
    {
      bool isValid = IsValid();
      bool otherIsValid = other.IsValid();
      return (!isValid && !otherIsValid) ||
             (isValid && otherIsValid &&
              (m_aspectIndex == other.m_aspectIndex || !m_map->AreAspectsSplit()) &&
              (m_level == other.m_level || !m_map->AreLevelsSplit()) &&
              (m_layer == other.m_layer || !m_map->AreLayersSplit()) &&
              (m_slice == other.m_slice || !m_map->IsDepthSplit()));
    }
    inline bool operator!=(const SubresourceRangeIterTemplate &other) { return !(*this == other); }
    SubresourceRangeIterTemplate &operator++();
    Pair *operator->();
    Pair &operator*();

    friend class ImageSubresourceMap;

  protected:
    Map *m_map = NULL;
    uint16_t m_splitFlags = 0u;
    ImageSubresourceRange m_range = {};
    uint32_t m_aspectIndex = 0u;
    uint32_t m_level = 0u;
    uint32_t m_layer = 0u;
    uint32_t m_slice = 0u;
    Pair m_value;
    SubresourceRangeIterTemplate() {}
    SubresourceRangeIterTemplate(Map &map, const ImageSubresourceRange &range);
    inline bool IsValid() const
    {
      return m_map && m_aspectIndex < m_map->m_aspectCount &&
             m_level < m_range.baseMipLevel + m_range.levelCount &&
             m_layer < m_range.baseArrayLayer + m_range.layerCount &&
             m_slice < m_range.baseDepthSlice + m_range.sliceCount;
    }
    void FixSubRange();
  };

  template <typename State>
  class SubresourcePairRefTemplate
  {
  public:
    inline const ImageSubresourceRange &range() const { return m_range; }
    inline State &state() { return *m_state; }
    inline const State &state() const { return *m_state; }
    SubresourcePairRefTemplate &operator=(const SubresourcePairRefTemplate &other) = delete;

  protected:
    template <typename Map, typename Pair>
    friend class SubresourceRangeIterTemplate;
    ImageSubresourceRange m_range;
    State *m_state;
  };

  class SubresourcePairRef : public SubresourcePairRefTemplate<ImageSubresourceState>
  {
  public:
    inline SubresourcePairRef &SetState(const ImageSubresourceState &state)
    {
      *m_state = state;
      return *this;
    }
  };
  using ConstSubresourcePairRef = SubresourcePairRefTemplate<const ImageSubresourceState>;

  using SubresourceRangeIter = SubresourceRangeIterTemplate<ImageSubresourceMap, SubresourcePairRef>;
  using SubresourceRangeConstIter =
      SubresourceRangeIterTemplate<const ImageSubresourceMap, ConstSubresourcePairRef>;

  inline SubresourceRangeIter RangeBegin(const ImageSubresourceRange &range)
  {
    return SubresourceRangeIter(*this, range);
  }
  inline SubresourceRangeConstIter RangeBegin(const ImageSubresourceRange &range) const
  {
    return SubresourceRangeConstIter(*this, range);
  }
  inline SubresourceRangeIter begin() { return RangeBegin(GetImageInfo().FullRange()); }
  inline SubresourceRangeConstIter begin() const { return RangeBegin(GetImageInfo().FullRange()); }
  inline SubresourceRangeIter end() { return SubresourceRangeIter(); }
  inline SubresourceRangeConstIter end() const { return SubresourceRangeConstIter(); }
  inline size_t size() const { return m_values.size(); }
};

template <typename Barrier>
struct BarrierSequence
{
  static const uint32_t MAX_BATCH_COUNT = 4;
  static const uint32_t MAX_QUEUE_FAMILY_COUNT = 3;

  // batches[batchIndex][queueFamilyIndex] = array of barriers to submit to queueFamilyIndex as part
  // of batchIndex
  rdcarray<Barrier> batches[MAX_BATCH_COUNT][MAX_QUEUE_FAMILY_COUNT];
  size_t barrierCount = 0;
  void AddWrapped(uint32_t batchIndex, uint32_t queueFamilyIndex, const Barrier &barrier);
  void Merge(const BarrierSequence<Barrier> &other);
  bool IsBatchEmpty(uint32_t batchIndex) const;
  void ExtractUnwrappedBatch(uint32_t batchIndex, uint32_t queueFamilyIndex,
                             rdcarray<Barrier> &result);
  void ExtractFirstUnwrappedBatchForQueue(uint32_t queueFamilyIndex, rdcarray<Barrier> &result);
  void ExtractLastUnwrappedBatchForQueue(uint32_t queueFamilyIndex, rdcarray<Barrier> &result);
  inline bool empty() const { return barrierCount == 0; }
  inline size_t size() const { return barrierCount; }
  static void UnwrapBarriers(rdcarray<Barrier> &barriers);
};

using ImageBarrierSequence = BarrierSequence<VkImageMemoryBarrier>;

struct ImageTransitionInfo
{
  CaptureState capState;
  uint32_t defaultQueueFamilyIndex;
  inline ImageTransitionInfo(CaptureState capState, uint32_t defaultQueueFamilyIndex)
      : capState(capState), defaultQueueFamilyIndex(defaultQueueFamilyIndex)
  {
  }
  inline FrameRefCompFunc GetFrameRefCompFunc()
  {
    if(IsCaptureMode(capState))
      return ComposeFrameRefs;
    else
      return KeepOldFrameRef;
  }
  inline FrameRefType GetDefaultRefType()
  {
    if(IsCaptureMode(capState))
      return eFrameRef_None;
    else
      return eFrameRef_Unknown;
  }
};

struct ImageState
{
  ImageSubresourceMap subresourceStates;
  rdcarray<VkImageMemoryBarrier> oldQueueFamilyTransfers;
  rdcarray<VkImageMemoryBarrier> newQueueFamilyTransfers;
  bool isMemoryBound = false;
  ResourceId boundMemory = ResourceId();
  VkDeviceSize boundMemoryOffset = 0ull;
  VkDeviceSize boundMemorySize = 0ull;
  FrameRefType maxRefType = eFrameRef_None;
  VkImage wrappedHandle = VK_NULL_HANDLE;

  inline const ImageInfo &GetImageInfo() const { return subresourceStates.GetImageInfo(); }
  inline ImageState() {}
  inline ImageState(VkImage wrappedHandle, const ImageInfo &imageInfo, FrameRefType refType)
      : wrappedHandle(wrappedHandle), subresourceStates(imageInfo, refType), maxRefType(refType)
  {
  }
  ImageState InitialState() const;
  void InitialState(ImageState &result) const;
  ImageState CommandBufferInitialState() const;
  ImageState UniformState(const ImageSubresourceState &sub) const;
  ImageState ContentInitializationState(InitPolicy policy, bool initialized,
                                        uint32_t queueFamilyIndex, VkImageLayout copyLayout,
                                        VkImageLayout clearLayout) const;
  void RemoveQueueFamilyTransfer(VkImageMemoryBarrier *it);
  void Update(ImageSubresourceRange range, const ImageSubresourceState &dst,
              FrameRefCompFunc compose);
  void Merge(const ImageState &other, ImageTransitionInfo info);
  void MergeCaptureBeginState(const ImageState &initialState);
  static void Merge(std::map<ResourceId, ImageState> &states,
                    const std::map<ResourceId, ImageState> &dstStates, ImageTransitionInfo info);
  void DiscardContents(const ImageSubresourceRange &range);
  inline void DiscardContents() { DiscardContents(GetImageInfo().FullRange()); }
  inline void RecordUse(const ImageSubresourceRange &range, FrameRefType refType,
                        uint32_t queueFamilyIndex)
  {
    Update(range, ImageSubresourceState(queueFamilyIndex, UNKNOWN_PREV_IMG_LAYOUT, refType),
           ComposeFrameRefs);
  }
  void RecordQueueFamilyRelease(const VkImageMemoryBarrier &barrier);
  void RecordQueueFamilyAcquire(const VkImageMemoryBarrier &barrier);
  void RecordBarrier(VkImageMemoryBarrier barrier, uint32_t queueFamilyIndex,
                     ImageTransitionInfo info);
  bool CloseTransfers(uint32_t batchIndex, VkAccessFlags dstAccessMask,
                      ImageBarrierSequence &barriers, ImageTransitionInfo info);
  bool RestoreTransfers(uint32_t batchIndex, const rdcarray<VkImageMemoryBarrier> &transfers,
                        VkAccessFlags srcAccessMask, ImageBarrierSequence &barriers,
                        ImageTransitionInfo info);
  void ResetToOldState(ImageBarrierSequence &barriers, ImageTransitionInfo info);
  void Transition(const ImageState &dstState, VkAccessFlags srcAccessMask,
                  VkAccessFlags dstAccessMask, ImageBarrierSequence &barriers,
                  ImageTransitionInfo info);
  void Transition(uint32_t queueFamilyIndex, VkImageLayout layout, VkAccessFlags srcAccessMask,
                  VkAccessFlags dstAccessMask, ImageBarrierSequence &barriers,
                  ImageTransitionInfo info);

  // Transitions the image state to `dstState` (via `preBarriers`) and back to the current state
  // (via `postBarriers`).
  // It is not always possible to return exactly to the current state, e.g. if the image is
  // VK_IMAGE_LAYOUT_PREINITIALIZED, it will be returned to VK_IMAGE_LAYOUT_GENERAL instead.
  void TempTransition(const ImageState &dstState, VkAccessFlags preSrcAccessMask,
                      VkAccessFlags preDstAccessMask, VkAccessFlags postSrcAccessmask,
                      VkAccessFlags postDstAccessMask, ImageBarrierSequence &setupBarriers,
                      ImageBarrierSequence &cleanupBarriers, ImageTransitionInfo info) const;
  void TempTransition(uint32_t queueFamilyIndex, VkImageLayout layout, VkAccessFlags accessMask,
                      ImageBarrierSequence &setupBarriers, ImageBarrierSequence &cleanupBarriers,
                      ImageTransitionInfo info) const;

  void InlineTransition(VkCommandBuffer cmd, uint32_t queueFamilyIndex, const ImageState &dstState,
                        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                        ImageTransitionInfo info);
  void InlineTransition(VkCommandBuffer cmd, uint32_t queueFamilyIndex, VkImageLayout layout,
                        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                        ImageTransitionInfo info);

  InitReqType MaxInitReq(const ImageSubresourceRange &range, InitPolicy policy,
                         bool initialized) const;
  VkImageLayout GetImageLayout(VkImageAspectFlagBits aspect, uint32_t mipLevel,
                               uint32_t arrayLayer) const;

  void BeginCapture();
};

DECLARE_REFLECTION_STRUCT(ImageState);

template <typename ImageStateT>
class LockedImageStateRefTemplate
{
public:
  LockedImageStateRefTemplate() = default;
  LockedImageStateRefTemplate(ImageStateT *state, Threading::SpinLock &spin)
      : m_state(state), m_lock(spin)
  {
  }
  inline ImageStateT &operator*() const { return *m_state; }
  inline ImageStateT *operator->() const { return m_state; }
  inline operator bool() const { return m_state != NULL; }
private:
  ImageStateT *m_state = NULL;
  Threading::ScopedSpinLock m_lock;
};

class LockedConstImageStateRef : public LockedImageStateRefTemplate<const ImageState>
{
public:
  LockedConstImageStateRef() = default;
  LockedConstImageStateRef(const ImageState *state, Threading::SpinLock &spin)
      : LockedImageStateRefTemplate<const ImageState>(state, spin)
  {
  }
};

class LockedImageStateRef : public LockedImageStateRefTemplate<ImageState>
{
public:
  LockedImageStateRef() = default;
  LockedImageStateRef(ImageState *state, Threading::SpinLock &spin)
      : LockedImageStateRefTemplate<ImageState>(state, spin)
  {
  }
};

class LockingImageState
{
public:
  LockingImageState() = default;
  LockingImageState(VkImage wrappedHandle, const ImageInfo &imageInfo, FrameRefType refType)
      : m_state(wrappedHandle, imageInfo, refType)
  {
  }
  LockingImageState(const ImageState &state) : m_state(state) {}
  LockedImageStateRef LockWrite() { return LockedImageStateRef(&m_state, m_lock); }
  LockedConstImageStateRef LockRead() { return LockedConstImageStateRef(&m_state, m_lock); }
  inline ImageState *state() { return &m_state; }
private:
  ImageState m_state;
  Threading::SpinLock m_lock;
};

struct ImgRefs
{
  rdcarray<FrameRefType> rangeRefs;
  WrappedVkRes *initializedLiveRes = NULL;
  ImageInfo imageInfo;
  VkImageAspectFlags aspectMask;

  bool areAspectsSplit = false;
  bool areLevelsSplit = false;
  bool areLayersSplit = false;

  int GetAspectCount() const;

  ImgRefs() : initializedLiveRes(NULL) {}
  inline ImgRefs(const ImageInfo &imageInfo)
      : imageInfo(imageInfo), aspectMask(FormatImageAspects(imageInfo.format))
  {
    rangeRefs.fill(1, eFrameRef_None);
    if(imageInfo.extent.depth > 1)
      // Depth slices of 3D views are treated as array layers
      this->imageInfo.layerCount = imageInfo.extent.depth;
  }
  int AspectIndex(VkImageAspectFlagBits aspect) const;
  int SubresourceIndex(int aspectIndex, int level, int layer) const;
  inline FrameRefType SubresourceRef(int aspectIndex, int level, int layer) const
  {
    return rangeRefs[SubresourceIndex(aspectIndex, level, layer)];
  }
  inline InitReqType SubresourceInitReq(int aspectIndex, int level, int layer, InitPolicy policy,
                                        bool initialized) const
  {
    return InitReq(SubresourceRef(aspectIndex, level, layer), policy, initialized);
  }
  InitReqType SubresourceRangeMaxInitReq(VkImageSubresourceRange range, InitPolicy policy,
                                         bool initialized) const;
  rdcarray<rdcpair<VkImageSubresourceRange, InitReqType> > SubresourceRangeInitReqs(
      VkImageSubresourceRange range, InitPolicy policy, bool initialized) const;
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

  rdcarray<VkImageAspectFlags> splitAspects;
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
        maxRefType = ComposeFrameRefsDisjoint(maxRefType, rangeRefs[index]);
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
        maxRefType = ComposeFrameRefsDisjoint(maxRefType, rangeRefs[index]);
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
    size = RDCMIN(size, UINT64_MAX - offset);
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
  size = RDCMIN(size, UINT64_MAX - offset);
  FrameRefType maxRefType = eFrameRef_None;
  rangeRefs.update(offset, offset + size, refType,
                   [&maxRefType, comp](FrameRefType oldRef, FrameRefType newRef) -> FrameRefType {
                     FrameRefType ref = comp(oldRef, newRef);
                     maxRefType = ComposeFrameRefsDisjoint(maxRefType, ref);
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
                    maxRefType = ComposeFrameRefsDisjoint(maxRefType, ref);
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
      : ResourceRecord(id, true), Resource(NULL), bakedCommands(NULL), pool(NULL), ptrunion(NULL)
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

    ImageRange imgRange = ImageRange((VkImageSubresourceRange)view->viewRange);
    imgRange.viewType = view->viewRange.viewType();

    FrameRefType maxRef = MarkImageReferenced(descInfo->bindImgRefs, view->baseResource,
                                              view->resInfo->imageInfo, imgRange, refType);

    p.second = ComposeFrameRefsDisjoint(p.second, maxRef);
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
    p.second = ComposeFrameRefsDisjoint(p.second, maxRef);
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
    void *ptrunion;                          // for initialisation to NULL
    InstanceDeviceInfo *instDevInfo;         // only for instances or physical/logical devices
    ResourceInfo *resInfo;                   // only for buffers, images, and views of them
    SwapchainInfo *swapInfo;                 // only for swapchains
    MemMapState *memMapState;                // only for device memory
    CmdBufferRecordingInfo *cmdInfo;         // only for command buffers
    AttachmentInfo *imageAttachments;        // only for framebuffers and render passes
    PipelineLayoutData *pipeLayoutInfo;      // only for pipeline layouts
    DescriptorSetData *descInfo;             // only for descriptor sets and descriptor set layouts
    DescUpdateTemplate *descTemplateInfo;    // only for descriptor update templates
    uint32_t queueFamilyIndex;               // only for queues and command pools
  };

  VkResourceRecord *bakedCommands;

  // pointer to either the pool this item is allocated from, or the children allocated
  // from this pool. Protected by the chunk lock
  VkResourceRecord *pool;
  rdcarray<VkResourceRecord *> pooledChildren;

  // we only need a couple of bytes to store the view's range,
  // so just pack/unpack into bitfields
  struct ViewRange
  {
    ViewRange()
    {
      aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      baseMipLevel = 0;
      packedLevelCount = 1;
      baseArrayLayer = 0;
      packedLayerCount = 1;
      packedViewType = 7;
    }

    ViewRange &operator=(const VkImageSubresourceRange &range)
    {
      aspectMask = (uint32_t)range.aspectMask;
      baseMipLevel = range.baseMipLevel;
      baseArrayLayer = range.baseArrayLayer;

      if(range.levelCount == VK_REMAINING_MIP_LEVELS)
        packedLevelCount = MipMaxValue;
      else
        packedLevelCount = range.levelCount;

      if(range.layerCount == VK_REMAINING_ARRAY_LAYERS)
        packedLayerCount = SliceMaxValue;
      else
        packedLayerCount = range.layerCount;

      return *this;
    }

    operator VkImageSubresourceRange() const
    {
      VkImageSubresourceRange ret;

      ret.aspectMask = (VkImageAspectFlags)aspectMask;
      ret.baseMipLevel = baseMipLevel;
      ret.baseArrayLayer = baseArrayLayer;
      ret.levelCount = levelCount();
      ret.layerCount = layerCount();
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

    inline uint32_t levelCount() const
    {
      if(packedLevelCount == MipMaxValue)
        return VK_REMAINING_MIP_LEVELS;
      else
        return packedLevelCount;
    }
    inline void setLevelCount(uint32_t levelCount)
    {
      if(levelCount == VK_REMAINING_MIP_LEVELS)
        packedLevelCount = MipMaxValue;
      else
        packedLevelCount = levelCount;
    }

    inline uint32_t layerCount() const
    {
      if(packedLayerCount == SliceMaxValue)
        return VK_REMAINING_ARRAY_LAYERS;
      else
        return packedLayerCount;
    }
    inline void setLayerCount(uint32_t layerCount)
    {
      if(layerCount == VK_REMAINING_ARRAY_LAYERS)
        packedLayerCount = SliceMaxValue;
      else
        packedLayerCount = layerCount;
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
    uint32_t packedLevelCount : 6;

    static const uint32_t MipMaxValue = 0x3f;

    // 16 bits = refer to up to 64k array layers. This is less
    // future proof than above, but at time of writing typical
    // maxImageArrayLayers is 2048.
    uint32_t baseArrayLayer : 16;
    uint32_t packedLayerCount : 16;

    static const uint32_t SliceMaxValue = 0xffff;
  } viewRange;
};

struct ImageLayouts
{
  uint32_t queueFamilyIndex = 0;
  rdcarray<ImageRegionState> subresourceStates;
  bool isMemoryBound = false;
  ResourceId boundMemory = ResourceId();
  VkDeviceSize boundMemoryOffset = 0ull;
  VkDeviceSize boundMemorySize = 0ull;
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
VkFormat GetViewCastedFormat(VkFormat f, CompType typeCast);

struct Vec4u;

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
