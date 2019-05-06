/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

#include "api/replay/renderdoc_replay.h"

#include "vk_core.h"
#include "vk_replay.h"

VkResult WrappedVulkan::vkCreateStreamDescriptorSurfaceGGP(
    const VkInstance instance, const VkStreamDescriptorSurfaceCreateInfoGGP *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
  // should not come in here at all on replay
  RDCASSERT(IsCaptureMode(m_State));

  VkResult ret = ObjDisp(instance)->CreateStreamDescriptorSurfaceGGP(Unwrap(instance), pCreateInfo,
                                                                     pAllocator, pSurface);

  if(ret == VK_SUCCESS)
  {
    GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);

    WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);

    wrapped->record = (VkResourceRecord *)(uintptr_t)pCreateInfo->streamDescriptor;
  }
  return ret;
}

void VulkanReplay::OutputWindow::SetWindowHandle(WindowingData window)
{
  RDCASSERT(window.system == WindowingSystem::GGP, window.system);
  return;    // there are no OS specific handles to save.
}

void VulkanReplay::OutputWindow::CreateSurface(VkInstance inst)
{
  VkStreamDescriptorSurfaceCreateInfoGGP createInfo;

  createInfo.sType = VK_STRUCTURE_TYPE_STREAM_DESCRIPTOR_SURFACE_CREATE_INFO_GGP;
  createInfo.pNext = NULL;
  createInfo.streamDescriptor = 1;

  VkResult vkr =
      ObjDisp(inst)->CreateStreamDescriptorSurfaceGGP(Unwrap(inst), &createInfo, NULL, &surface);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  return;
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

  RDCLOG("Window system is GGP (%d), size is %d, %d", outw.m_WindowSystem, outw.width, outw.height);
  // No window, specify default resolution.
  w = outw.width != 0 ? outw.width : 1920;
  h = outw.height != 0 ? outw.height : 1080;
  return;
}

void *LoadVulkanLibrary()
{
  return Process::LoadModule("libvulkan.so.1");
}
