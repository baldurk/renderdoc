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

struct xcb_connection_t;
namespace Keyboard
{
	void UseConnection(xcb_connection_t *conn);
}

void VulkanReplay::OutputWindow::SetWindowHandle(void *wn)
{
	void **connectionScreenWindow = (void **)wn;

	connection = (xcb_connection_t *)connectionScreenWindow[0];
	int scr = (int)(uintptr_t)connectionScreenWindow[1];
	wnd = (xcb_window_t)(uintptr_t)connectionScreenWindow[2];

	const xcb_setup_t *setup = xcb_get_setup(connection);
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
	while (scr-- > 0) xcb_screen_next(&iter);

	screen = iter.data;
}

void VulkanReplay::OutputWindow::CreateSurface(VkInstance inst)
{
	VkXcbSurfaceCreateInfoKHR createInfo;

	createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.connection = connection;
	createInfo.window = wnd;

	VkResult vkr = ObjDisp(inst)->CreateXcbSurfaceKHR(Unwrap(inst), &createInfo, NULL, &surface);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void VulkanReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	OutputWindow &outw = m_OutputWindows[id];
	
	xcb_get_geometry_cookie_t  geomCookie = xcb_get_geometry (outw.connection, outw.wnd);  // window is a xcb_drawable_t
	xcb_get_geometry_reply_t  *geom       = xcb_get_geometry_reply (outw.connection, geomCookie, NULL);

	w = (int32_t)geom->width;
	h = (int32_t)geom->height;

	free(geom);
}

bool VulkanReplay::IsOutputWindowVisible(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return false;

	VULKANNOTIMP("Optimisation missing - output window always returning true");

	return true;
}

#if defined(VK_USE_PLATFORM_XCB_KHR)

VkBool32 WrappedVulkan::vkGetPhysicalDeviceXcbPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    xcb_connection_t*                           connection,
    xcb_visualid_t                              visual_id)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceXcbPresentationSupportKHR(Unwrap(physicalDevice), queueFamilyIndex, connection, visual_id);
}

VkResult WrappedVulkan::vkCreateXcbSurfaceKHR(
    VkInstance                                  instance,
    const VkXcbSurfaceCreateInfoKHR*            pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface)
{
	// should not come in here at all on replay
	RDCASSERT(m_State >= WRITING);

	VkResult ret = ObjDisp(instance)->CreateXcbSurfaceKHR(Unwrap(instance), pCreateInfo, pAllocator, pSurface);

	if(ret == VK_SUCCESS)
	{
		GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);
		
		WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);
		
		// since there's no point in allocating a full resource record and storing the window
		// handle under there somewhere, we just cast. We won't use the resource record for anything
		wrapped->record = (VkResourceRecord *)(uintptr_t)pCreateInfo->window;
		
		Keyboard::UseConnection(pCreateInfo->connection);
	}

	return ret;
}

#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)

VkBool32 WrappedVulkan::vkGetPhysicalDeviceXlibPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    Display*                                    dpy,
    VisualID                                    visualID)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceXlibPresentationSupportKHR(Unwrap(physicalDevice), queueFamilyIndex, dpy, visualID);
}

VkResult WrappedVulkan::vkCreateXlibSurfaceKHR(
    VkInstance                                  instance,
    const VkXlibSurfaceCreateInfoKHR*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface)
{
	// should not come in here at all on replay
	RDCASSERT(m_State >= WRITING);

	VkResult ret = ObjDisp(instance)->CreateXlibSurfaceKHR(Unwrap(instance), pCreateInfo, pAllocator, pSurface);

	if(ret == VK_SUCCESS)
	{
		GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);
		
		WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);
		
		// since there's no point in allocating a full resource record and storing the window
		// handle under there somewhere, we just cast. We won't use the resource record for anything
		wrapped->record = (VkResourceRecord *)pCreateInfo->window;
		
		// VKTODOLOW Should support Xlib here
		//Keyboard::UseConnection(pCreateInfo->dpy);
	}

	return ret;
}

#endif

const char *VulkanLibraryName = "libvulkan.so";
