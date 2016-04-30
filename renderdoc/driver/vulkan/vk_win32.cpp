/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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

#include "vk_replay.h"
#include "vk_core.h"

static int dllLocator=0;

void VulkanReplay::OutputWindow::SetWindowHandle(void *wn)
{
	wnd = (HWND)wn;
}

void VulkanReplay::OutputWindow::CreateSurface(VkInstance inst)
{
	VkWin32SurfaceCreateInfoKHR createInfo;

	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.hwnd = wnd;
	
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(const char *)&dllLocator, (HMODULE *)&createInfo.hinstance);

	VkResult vkr = ObjDisp(inst)->CreateWin32SurfaceKHR(Unwrap(inst), &createInfo, NULL, &surface);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void VulkanReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	OutputWindow &outw = m_OutputWindows[id];
	
	RECT rect = {0};
	GetClientRect(outw.wnd, &rect);
	w = rect.right-rect.left;
	h = rect.bottom-rect.top;
}

bool VulkanReplay::IsOutputWindowVisible(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return false;

	return (IsWindowVisible(m_OutputWindows[id].wnd) == TRUE);
}

#if !defined(VK_USE_PLATFORM_WIN32_KHR)
#error "Win32 KHR platform not defined"
#endif

VkResult WrappedVulkan::vkCreateWin32SurfaceKHR(
    VkInstance                                  instance,
		const VkWin32SurfaceCreateInfoKHR*          pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkSurfaceKHR*                               pSurface)
{
	// should not come in here at all on replay
	RDCASSERT(m_State >= WRITING);

	VkResult ret = ObjDisp(instance)->CreateWin32SurfaceKHR(Unwrap(instance), pCreateInfo, pAllocator, pSurface);

	if(ret == VK_SUCCESS)
	{
		GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);
		
		WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);
		
		// since there's no point in allocating a full resource record and storing the window
		// handle under there somewhere, we just cast. We won't use the resource record for anything
		wrapped->record = (VkResourceRecord *)pCreateInfo->hwnd;

		Keyboard::AddInputWindow((void *)pCreateInfo->hwnd);
	}

	return ret;
}

VkBool32 WrappedVulkan::vkGetPhysicalDeviceWin32PresentationSupportKHR(
			VkPhysicalDevice                            physicalDevice,
			uint32_t                                    queueFamilyIndex)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceWin32PresentationSupportKHR(Unwrap(physicalDevice), queueFamilyIndex);
}

const char *VulkanLibraryName = "vulkan-1.dll";
