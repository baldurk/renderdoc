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

#include <unordered_map>
#include "common/threading.h"
#include "os/os_specific.h"
#include "vk_common.h"
#include "vk_hookset_defs.h"
#include "vk_resources.h"

static VkLayerDispatchTableExtended replayDeviceTable;
static VkLayerInstanceDispatchTableExtended replayInstanceTable;

static bool replay = false;

void InitReplayTables(void *vulkanModule)
{
  replay = true;

// not all functions will succeed - some need to be fetched through the below
// InitDeviceReplayTable()

#undef HookInit
#define HookInit(name) \
  table.name =         \
      (CONCAT(PFN_vk, name))Process::GetFunctionAddress(vulkanModule, STRINGIZE(CONCAT(vk, name)))

  {
    VkLayerDispatchTableExtended &table = replayDeviceTable;
    memset(&table, 0, sizeof(table));
    HookInit(GetDeviceProcAddr);
    HookInitVulkanDevice();
  }

  {
    VkLayerInstanceDispatchTableExtended &table = replayInstanceTable;
    memset(&table, 0, sizeof(table));
    HookInit(GetInstanceProcAddr);
    HookInit(EnumerateInstanceExtensionProperties);
    HookInit(EnumerateInstanceLayerProperties);
    HookInitVulkanInstance();
  }
}

#define InstanceGPA(func) \
  table->func =           \
      (CONCAT(PFN_vk, func))table->GetInstanceProcAddr(instance, STRINGIZE(CONCAT(vk, func)))

void InitInstanceExtensionTables(VkInstance instance, InstanceDeviceInfo *info)
{
  VkLayerInstanceDispatchTableExtended *table = GetInstanceDispatchTable(instance);
  RDCASSERT(table);

  instance = Unwrap(instance);

#undef DeclExt
#define DeclExt(name) \
  bool name = false;  \
  (void)name;

#undef CheckExt
#define CheckExt(name, ver) name = info->ext_##name;

#undef HookInitExtension
#define HookInitExtension(cond, func) \
  if(cond)                            \
  {                                   \
    InstanceGPA(func);                \
  }

#undef HookInitPromotedExtension
#define HookInitPromotedExtension(cond, func, suffix) \
  if(cond)                                            \
  {                                                   \
    InstanceGPA(func);                                \
    InstanceGPA(CONCAT(func, suffix));                \
    if(table->func == NULL)                           \
      table->func = table->CONCAT(func, suffix);      \
    if(table->CONCAT(func, suffix) == NULL)           \
      table->CONCAT(func, suffix) = table->func;      \
  }

  DeclExts();

  CheckInstanceExts();

  InstanceGPA(EnumerateDeviceExtensionProperties);
  InstanceGPA(EnumerateDeviceLayerProperties);

  HookInitVulkanInstanceExts();
}

#undef InstanceGPA

#define DeviceGPA(func) \
  table->func = (CONCAT(PFN_vk, func))table->GetDeviceProcAddr(device, STRINGIZE(CONCAT(vk, func)));

void InitDeviceExtensionTables(VkDevice device, InstanceDeviceInfo *info)
{
  VkLayerDispatchTableExtended *table = GetDeviceDispatchTable(device);
  RDCASSERT(table);

  device = Unwrap(device);

#undef HookInitExtension
#define HookInitExtension(cond, func) \
  if(cond)                            \
  {                                   \
    DeviceGPA(func);                  \
  }

#undef HookInitPromotedExtension
#define HookInitPromotedExtension(cond, func, suffix) \
  if(cond)                                            \
  {                                                   \
    DeviceGPA(func);                                  \
    DeviceGPA(CONCAT(func, suffix));                  \
    if(table->func == NULL)                           \
      table->func = table->CONCAT(func, suffix);      \
    if(table->CONCAT(func, suffix) == NULL)           \
      table->CONCAT(func, suffix) = table->func;      \
  }

  DeclExts();

  CheckInstanceExts();
  CheckDeviceExts();

  HookInitVulkanDeviceExts();
}

#undef DeviceGPA

static Threading::CriticalSection devlock;
std::map<void *, VkLayerDispatchTableExtended> devlookup;

static Threading::CriticalSection instlock;
std::map<void *, VkLayerInstanceDispatchTableExtended> instlookup;

static void *GetKey(void *obj)
{
  VkLayerDispatchTable **tablePtr = (VkLayerDispatchTable **)obj;
  return (void *)*tablePtr;
}

void InitDeviceTable(VkDevice dev, PFN_vkGetDeviceProcAddr gpa)
{
  void *key = GetKey(dev);

  VkLayerDispatchTableExtended *table = NULL;

  {
    SCOPED_LOCK(devlock);
    RDCEraseEl(devlookup[key]);
    table = &devlookup[key];
  }

  table->GetDeviceProcAddr = gpa;

// fetch the rest of the functions
#undef HookInit
#define HookInit(name)    \
  if(table->name == NULL) \
  table->name = (CONCAT(PFN_vk, name))gpa(dev, STRINGIZE(CONCAT(vk, name)))

  HookInitVulkanDevice();
}

void InitInstanceTable(VkInstance inst, PFN_vkGetInstanceProcAddr gpa)
{
  void *key = GetKey(inst);

  VkLayerInstanceDispatchTableExtended *table = NULL;

  {
    SCOPED_LOCK(instlock);
    RDCEraseEl(instlookup[key]);
    table = &instlookup[key];
  }

  // init the GetInstanceProcAddr function first
  table->GetInstanceProcAddr = gpa;

// fetch the rest of the functions
#undef HookInit
#define HookInit(name)    \
  if(table->name == NULL) \
  table->name = (CONCAT(PFN_vk, name))gpa(inst, STRINGIZE(CONCAT(vk, name)))

  HookInitVulkanInstance();

  // we also need these functions for layer handling
  HookInit(EnumerateDeviceExtensionProperties);
  HookInit(EnumerateDeviceLayerProperties);
}

VkLayerDispatchTableExtended *GetDeviceDispatchTable(void *device)
{
  if(replay)
    return &replayDeviceTable;

  void *key = GetKey(device);

  {
    SCOPED_LOCK(devlock);

    auto it = devlookup.find(key);

    if(it == devlookup.end())
      RDCFATAL("Bad device pointer");

    return &it->second;
  }
}

VkLayerInstanceDispatchTableExtended *GetInstanceDispatchTable(void *instance)
{
  if(replay)
    return &replayInstanceTable;

  void *key = GetKey(instance);

  {
    SCOPED_LOCK(instlock);

    auto it = instlookup.find(key);

    if(it == instlookup.end())
      RDCFATAL("Bad device pointer");

    return &it->second;
  }
}
