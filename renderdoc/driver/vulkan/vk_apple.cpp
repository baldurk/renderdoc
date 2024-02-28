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

#include "strings/string_utils.h"
#include "vk_core.h"
#include "vk_replay.h"

#include <dlfcn.h>

// helpers defined in vk_apple_helpers.mm
void getMetalLayerSize(void *layerHandle, int &width, int &height);

#if defined(VK_USE_PLATFORM_MACOS_MVK)

VkResult WrappedVulkan::vkCreateMacOSSurfaceMVK(VkInstance instance,
                                                const VkMacOSSurfaceCreateInfoMVK *pCreateInfo,
                                                const VkAllocationCallbacks *, VkSurfaceKHR *pSurface)
{
  // should not come in here at all on replay
  RDCASSERT(IsCaptureMode(m_State));

  VkResult ret =
      ObjDisp(instance)->CreateMacOSSurfaceMVK(Unwrap(instance), pCreateInfo, NULL, pSurface);

  if(ret == VK_SUCCESS)
  {
    GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);

    WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);

    wrapped->record = RegisterSurface(WindowingSystem::MacOS, (void *)(uintptr_t)pCreateInfo->pView);
  }

  return ret;
}

#endif

#if defined(VK_USE_PLATFORM_METAL_EXT)

VkResult WrappedVulkan::vkCreateMetalSurfaceEXT(VkInstance instance,
                                                const VkMetalSurfaceCreateInfoEXT *pCreateInfo,
                                                const VkAllocationCallbacks *, VkSurfaceKHR *pSurface)
{
  // should not come in here at all on replay
  RDCASSERT(IsCaptureMode(m_State));

  VkResult ret =
      ObjDisp(instance)->CreateMetalSurfaceEXT(Unwrap(instance), pCreateInfo, NULL, pSurface);

  if(ret == VK_SUCCESS)
  {
    GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);

    WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);

    wrapped->record = RegisterSurface(WindowingSystem::MacOS, (void *)(uintptr_t)pCreateInfo->pLayer);
  }

  return ret;
}

#endif

void VulkanReplay::OutputWindow::SetWindowHandle(WindowingData window)
{
  RDCASSERT(window.system == WindowingSystem::MacOS, window.system);
  cocoa.view = window.macOS.view;
  cocoa.layer = window.macOS.layer;
}

void VulkanReplay::OutputWindow::CreateSurface(WrappedVulkan *driver, VkInstance inst)
{
#if defined(VK_USE_PLATFORM_METAL_EXT)
  if(driver->GetExtensions(GetRecord(inst)).ext_EXT_metal_surface)
  {
    VkMetalSurfaceCreateInfoEXT createInfo;

    createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.pLayer = cocoa.layer;

    RDCDEBUG("Creating macOS surface with EXT_metal_surface");

    VkResult vkr = ObjDisp(inst)->CreateMetalSurfaceEXT(Unwrap(inst), &createInfo, NULL, &surface);
    driver->CheckVkResult(vkr);
    return;
  }
#endif

#if defined(VK_USE_PLATFORM_MACOS_MVK)
  if(driver->GetExtensions(GetRecord(inst)).ext_MVK_macos_surface)
  {
    VkMacOSSurfaceCreateInfoMVK createInfo;

    createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.pView = cocoa.view;

    RDCDEBUG("Creating macOS surface with MVK_macos_surface");

    VkResult vkr = ObjDisp(inst)->CreateMacOSSurfaceMVK(Unwrap(inst), &createInfo, NULL, &surface);
    driver->CheckVkResult(vkr);
    return;
  }
#endif

  RDCERR("No enabled macOS surface extension");
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

  getMetalLayerSize(outw.cocoa.layer, w, h);
}

static const rdcstr VulkanLibraryName = "libvulkan.1.dylib"_lit;

void *LoadVulkanLibrary()
{
  // first try to load the module globally. If so we assume the user has a global (or at least
  // user-wide) configuration that we should use.
  void *ret = Process::LoadModule(VulkanLibraryName);

  if(ret)
  {
    RDCLOG("Loaded global libvulkan.1.dylib, using default MoltenVK environment");
    return ret;
  }

  // then try the standard SDK install path under /usr/local/lib
  ret = Process::LoadModule("/usr/local/lib/" + VulkanLibraryName);

  if(ret)
  {
    RDCLOG("Loaded /usr/local/lib/libvulkan.1.dylib, using installed MoltenVK environment");
    return ret;
  }

  // if not, we fall back to our embedded libvulkan and also force use of our embedded ICD.
  rdcstr libpath;
  FileIO::GetLibraryFilename(libpath);
  libpath = get_dirname(libpath) + "/../plugins/MoltenVK/";

  RDCLOG("Couldn't load global libvulkan.1.dylib, falling back to bundled MoltenVK in %s",
         libpath.c_str());

  Process::RegisterEnvironmentModification(EnvironmentModification(
      EnvMod::Set, EnvSep::NoSep, "VK_ICD_FILENAMES", libpath + "MoltenVK_icd.json"));

  Process::ApplyEnvironmentModification();

  return Process::LoadModule(libpath + VulkanLibraryName);
}
