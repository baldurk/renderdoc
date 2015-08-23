/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

#include <dlfcn.h>
#include <stdio.h>

#include "hooks/hooks.h"

#include "driver/vulkan/vk_common.h"
#include "driver/vulkan/vk_hookset.h"
#include "driver/vulkan/vk_core.h"

#include "common/threading.h"
#include "serialise/string_utils.h"

// bit of a hack
namespace Keyboard { void CloneDisplay(Display *dpy); }

void *libvulkandlsymHandle = RTLD_NEXT; // default to RTLD_NEXT, but overwritten if app calls dlopen() on real libvulkan

// don't want these definitions, the only place we'll use these is as parameter/variable names
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

// the _renderdoc_hooked variants are to make sure we always have a function symbol
// exported that we can return from GetProcAddr. If another library (or the app)
// creates a symbol called 'vkCreateImage' we'll return the address of that, and break
// badly. Instead we leave the 'naked' versions for applications trying to import those
// symbols, and declare the _renderdoc_hooked for returning as a func pointer.

#define HookDefine0(ret, function) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function() \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(); } \
	ret CONCAT(function,_renderdoc_hooked)() \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(); }
#define HookDefine1(ret, function, t1, p1) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1); }
#define HookDefine2(ret, function, t1, p1, t2, p2) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2); }
#define HookDefine3(ret, function, t1, p1, t2, p2, t3, p3) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3); }
#define HookDefine4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4); }
#define HookDefine5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5); }
#define HookDefine6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6); }
#define HookDefine7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7); }
#define HookDefine8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8); }
#define HookDefine9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9); }
#define HookDefine10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); }
#define HookDefine11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); }
#define HookDefine12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); }
#define HookDefine13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); }
#define HookDefine14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14); }
#define HookDefine15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15) \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14, t15 p15) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); } \
	ret CONCAT(function,_renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14, t15 p15) \
	{ SCOPED_LOCK(vkLock); return VulkanHook::vkhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); }

Threading::CriticalSection vkLock;

class VulkanHook : LibraryHook
{
	public:
		VulkanHook()
		{
			LibraryHooks::GetInstance().RegisterHook("libvulkan.so", this);
			
			RDCEraseEl(VK);

			GPA_Instance = (PFN_vkGetInstanceProcAddr)NULL;
			GPA_Device = (PFN_vkGetDeviceProcAddr)NULL;

			m_Vulkan = NULL;

			m_EnabledHooks = true;
			m_PopulatedHooks = false;
		}
		~VulkanHook()
		{
			delete m_Vulkan;
		}

		static void libHooked(void *realLib)
		{
			libvulkandlsymHandle = realLib;
			VulkanHook::vkhooks.CreateHooks(NULL);
		}

		bool CreateHooks(const char *libName)
		{
			if(!m_EnabledHooks)
				return false;

			if(libName)
				LinuxHookLibrary("libvulkan.so", &libHooked);

			// SUUUUPer hack. I guess Keyboard needs to support
			// xcb connections as well, and init it whenever
			// a WSI swapchain gets created on it
			Keyboard::CloneDisplay(XOpenDisplay(NULL));

			bool success = SetupHooks(VK);

			if(!success) return false;
			
			m_HasHooks = true;

			return true;
		}

		void EnableHooks(const char *libName, bool enable)
		{
			m_EnabledHooks = enable;
		}
		
		static VulkanHook vkhooks;

		const VulkanFunctions &GetRealVKFunctions()
		{
			return VK;
		}
		
		WrappedVulkan *GetDriver()
		{
			if(m_Vulkan == NULL)
				m_Vulkan = new WrappedVulkan(VK, "");

			return m_Vulkan;
		}

		WrappedVulkan *m_Vulkan;
		
		VulkanFunctions VK;

		PFN_vkGetInstanceProcAddr GPA_Instance;
		PFN_vkGetDeviceProcAddr GPA_Device;

		bool m_PopulatedHooks;
		bool m_HasHooks;
		bool m_EnabledHooks;

		bool SetupHooks(VulkanFunctions &VK);
};

DefineHooks();

bool VulkanHook::SetupHooks(VulkanFunctions &VK)
{
	bool success = true;
	
#undef HookInit
#define HookInit(function) if(VK.function == NULL) { VK.function = (CONCAT(PFN_, function))dlsym(libvulkandlsymHandle, STRINGIZE(function)); }
	
	HookInitVulkan();

	if(GPA_Instance == NULL) GPA_Instance = (PFN_vkGetInstanceProcAddr)dlsym(libvulkandlsymHandle, "vkGetInstanceProcAddr");
	if(GPA_Device == NULL)   GPA_Device = (PFN_vkGetDeviceProcAddr)dlsym(libvulkandlsymHandle, "vkGetDeviceProcAddr");

	return success;
}

extern "C" __attribute__ ((visibility ("default")))
PFN_vkVoidFunction vkGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
	PFN_vkVoidFunction realFunc = VulkanHook::vkhooks.GPA_Instance(instance, pName);
	
#undef HookInit
#define HookInit(function) if(!strcmp(pName, STRINGIZE(function))) { if(VulkanHook::vkhooks.VK.function == NULL) VulkanHook::vkhooks.VK.function = (CONCAT(PFN_, function))realFunc; return (PFN_vkVoidFunction)CONCAT(function, _renderdoc_hooked); }
	
	// VKTODO do we want to care about the case where different instances have
	// different function pointers? at the moment we assume they're all the
	// same.
	HookInitVulkan();

	RDCDEBUG("Instance GPA'd function '%s' is not hooked!", pName);
	return realFunc;
}

extern "C" __attribute__ ((visibility ("default")))
PFN_vkVoidFunction vkGetDeviceProcAddr(
    VkDevice                                    device,
    const char*                                 pName)
{
	PFN_vkVoidFunction realFunc = VulkanHook::vkhooks.GPA_Device(device, pName);

#undef HookInit
#define HookInit(function) if(!strcmp(pName, STRINGIZE(function))) { if(VulkanHook::vkhooks.VK.function == NULL) VulkanHook::vkhooks.VK.function = (CONCAT(PFN_, function))realFunc; return (PFN_vkVoidFunction)CONCAT(function, _renderdoc_hooked); }
	
	// VKTODO do we want to care about the case where different devices have
	// different function pointers? at the moment we assume they're all the
	// same.
	HookInitVulkan();

	RDCDEBUG("Device GPA'd function '%s' is not hooked!", pName);
	return realFunc;
}

VulkanHook VulkanHook::vkhooks;

const VulkanFunctions &GetRealVKFunctions() { return VulkanHook::vkhooks.GetRealVKFunctions(); }
