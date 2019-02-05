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

// layer includes

#if ENABLED(RDOC_WIN32)
// undefined clashing windows #defines
#undef CreateEvent
#undef CreateSemaphore
#endif

#include "vk_hookset_defs.h"

void InitReplayTables(void *vulkanModule);

struct InstanceDeviceInfo
{
#undef DeclExt
#define DeclExt(name) bool ext_##name = false;

  bool brokenGetDeviceProcAddr = false;

  int vulkanVersion = VK_API_VERSION_1_0;

  DeclExts();
};

void InitInstanceExtensionTables(VkInstance instance, InstanceDeviceInfo *info);
void InitDeviceExtensionTables(VkDevice device, InstanceDeviceInfo *info);

VkLayerDispatchTableExtended *GetDeviceDispatchTable(void *device);
VkLayerInstanceDispatchTableExtended *GetInstanceDispatchTable(void *instance);

class WrappedVulkan;

template <typename parenttype, typename wrappedtype>
void SetDispatchTable(bool writing, parenttype parent, WrappedVulkan *core, wrappedtype *wrapped)
{
  wrapped->core = core;
  if(writing)
  {
    wrapped->table = wrappedtype::UseInstanceDispatchTable
                         ? (uintptr_t)GetInstanceDispatchTable((void *)parent)
                         : (uintptr_t)GetDeviceDispatchTable((void *)parent);
  }
  else
  {
    wrapped->table = wrappedtype::UseInstanceDispatchTable
                         ? (uintptr_t)GetInstanceDispatchTable(NULL)
                         : (uintptr_t)GetDeviceDispatchTable(NULL);
  }
}
