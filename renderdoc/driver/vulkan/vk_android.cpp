/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "vk_core.h"
#include "vk_replay.h"

void VulkanReplay::OutputWindow::SetWindowHandle(WindowingSystem system, void *data)
{
  RDCASSERT(system == WindowingSystem::Android, system);
  wnd = (ANativeWindow *)data;
  m_WindowSystem = system;
}

void VulkanReplay::OutputWindow::CreateSurface(VkInstance inst)
{
  VkAndroidSurfaceCreateInfoKHR createInfo;

  createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  createInfo.pNext = NULL;
  createInfo.flags = 0;
  createInfo.window = wnd;

  VkResult vkr = ObjDisp(inst)->CreateAndroidSurfaceKHR(Unwrap(inst), &createInfo, NULL, &surface);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void VulkanReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  w = ANativeWindow_getWidth(outw.wnd);
  h = ANativeWindow_getHeight(outw.wnd);
}

const char *VulkanLibraryName = "libvulkan.so";

bool VulkanReplay::CheckVulkanLayer(VulkanLayerFlags &flags, std::vector<std::string> &myJSONs,
                                    std::vector<std::string> &otherJSONs)
{
  // nothing to do
  return false;
}

void VulkanReplay::InstallVulkanLayer(bool systemLevel)
{
}
