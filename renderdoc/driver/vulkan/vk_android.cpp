/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <android/native_window.h>
#include "vk_core.h"
#include "vk_replay.h"

VkResult WrappedVulkan::vkCreateAndroidSurfaceKHR(VkInstance instance,
                                                  const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                                                  const VkAllocationCallbacks *,
                                                  VkSurfaceKHR *pSurface)
{
  // should not come in here at all on replay
  RDCASSERT(IsCaptureMode(m_State));

  VkResult ret =
      ObjDisp(instance)->CreateAndroidSurfaceKHR(Unwrap(instance), pCreateInfo, NULL, pSurface);

  if(ret == VK_SUCCESS)
  {
    GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);

    WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);

    wrapped->record =
        RegisterSurface(WindowingSystem::Android, (void *)(uintptr_t)pCreateInfo->window);
  }

  return ret;
}

// VK_ANDROID_external_memory_android_hardware_buffer
VkResult WrappedVulkan::vkGetAndroidHardwareBufferPropertiesANDROID(
    VkDevice device, const struct AHardwareBuffer *buffer,
    VkAndroidHardwareBufferPropertiesANDROID *pProperties)
{
  return ObjDisp(device)->GetAndroidHardwareBufferPropertiesANDROID(Unwrap(device), buffer,
                                                                    pProperties);
}

// VK_ANDROID_external_memory_android_hardware_buffer
VkResult WrappedVulkan::vkGetMemoryAndroidHardwareBufferANDROID(
    VkDevice device, const VkMemoryGetAndroidHardwareBufferInfoANDROID *pInfo,
    struct AHardwareBuffer **pBuffer)
{
  VkMemoryGetAndroidHardwareBufferInfoANDROID unwrappedInfo = *pInfo;
  unwrappedInfo.memory = Unwrap(unwrappedInfo.memory);
  return ObjDisp(device)->GetMemoryAndroidHardwareBufferANDROID(Unwrap(device), &unwrappedInfo,
                                                                pBuffer);
}

void VulkanReplay::OutputWindow::SetWindowHandle(WindowingData window)
{
  RDCASSERT(window.system == WindowingSystem::Android, window.system);
  wnd = window.android.window;
}

void VulkanReplay::OutputWindow::CreateSurface(WrappedVulkan *driver, VkInstance inst)
{
  VkAndroidSurfaceCreateInfoKHR createInfo;

  createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  createInfo.pNext = NULL;
  createInfo.flags = 0;
  createInfo.window = wnd;

  VkResult vkr = ObjDisp(inst)->CreateAndroidSurfaceKHR(Unwrap(inst), &createInfo, NULL, &surface);
  driver->CheckVkResult(vkr);
}

void VulkanReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  if(outw.m_WindowSystem == WindowingSystem::Headless)
  {
    w = outw.width;
    h = outw.height;
    return;
  }

  w = ANativeWindow_getWidth(outw.wnd);
  h = ANativeWindow_getHeight(outw.wnd);
}

void *LoadVulkanLibrary()
{
  return Process::LoadModule("libvulkan.so");
}
