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
  RDCUNIMPLEMENTED("SetWindowHandle");
  m_WindowSystem = system;
}

void VulkanReplay::OutputWindow::CreateSurface(VkInstance inst)
{
  RDCUNIMPLEMENTED("CreateSurface");
}

void VulkanReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  RDCUNIMPLEMENTED("GetOutputWindowDimensions");
}

const char *VulkanLibraryName = "libvulkan.so";

bool VulkanReplay::CheckVulkanLayer(VulkanLayerFlags &flags, std::vector<std::string> &myJSONs,
                                    std::vector<std::string> &otherJSONs)
{
  return false;
}

void VulkanReplay::InstallVulkanLayer(bool systemLevel)
{
}
