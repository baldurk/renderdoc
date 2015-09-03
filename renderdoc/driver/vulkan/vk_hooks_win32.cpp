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

#include "vk_common.h"
#include "vk_hookset.h"
#include "vk_core.h"
#include "hooks/hooks.h"

#define DLL_NAME "vulkan.0.dll"

Threading::CriticalSection vulkanLock;

class VulkanHook : LibraryHook
{
	public:
		VulkanHook()
		{
			LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);

			m_Driver = NULL;

			m_EnabledHooks = true;
			m_PopulatedHooks = false;
		}
		~VulkanHook()
		{
			delete m_Driver;
		}
		
		bool CreateHooks(const char *dllName)
		{
			RDCEraseEl(VK);

			if(!m_EnabledHooks)
				return false;

			bool success = SetupHooks(VK);

			if(!success) return false;
			
			m_HasHooks = true;

			return true;
		}

		void EnableHooks(const char *dllName, bool enable)
		{
			m_EnabledHooks = enable;
		}
		
		static VulkanHook vkhooks;

		const VulkanFunctions &GetRealVKFunctions()
		{
			LoadLibraryA("vulkan.0.dll");
			if(!m_PopulatedHooks)
			{
				PopulateHooks();
				m_PopulatedHooks = true;
			}
			return VK;
		}

		void PopulateDeviceHooks(VkDevice d, VkInstance i)
		{
#define HACK_WSI(func) VK.func = (CONCAT(PFN_, func))GPA_Device()(d, STRINGIZE(func));
			HACK_WSI(vkCreateSwapChainWSI)
			HACK_WSI(vkDestroySwapChainWSI)
			HACK_WSI(vkGetSwapChainInfoWSI)
			HACK_WSI(vkAcquireNextImageWSI)
			HACK_WSI(vkQueuePresentWSI)
#undef HACK_WSI

#define HACK_DBG(func) VK.func = (CONCAT(PFN_, func))GPA_Instance()(i, STRINGIZE(func));
			HACK_DBG(vkDbgCreateMsgCallback)
			HACK_DBG(vkDbgDestroyMsgCallback)
#undef HACK_DBG
		}

	private:
		WrappedVulkan *GetDriver()
		{
			if(m_Driver == NULL)
				m_Driver = new WrappedVulkan(VK, "");

			GetRealVKFunctions();

			return m_Driver;
		}

		WrappedVulkan *m_Driver;
		
		VulkanFunctions VK;
		
		Hook<PFN_vkGetInstanceProcAddr> GPA_Instance;
		Hook<PFN_vkGetDeviceProcAddr> GPA_Device;

		bool m_PopulatedHooks;
		bool m_HasHooks;
		bool m_EnabledHooks;

		bool PopulateHooks()
		{
#undef HookInit
#define HookInit(funcname) \
	if(!VK.funcname) VK.funcname = (CONCAT(PFN_, funcname))Process::GetFunctionAddress(DLL_NAME, STRINGIZE(funcname));

			HookInitVulkan()

			if(GPA_Instance() == NULL) GPA_Instance.SetFuncPtr(Process::GetFunctionAddress(DLL_NAME, "vkGetInstanceProcAddr"));
			if(GPA_Device() == NULL)   GPA_Device.SetFuncPtr(Process::GetFunctionAddress(DLL_NAME, "vkGetDeviceProcAddr"));

			return true;
		}
		
		static PFN_vkVoidFunction VKAPI vkGetInstanceProcAddr_hooked(
			VkInstance                                  instance,
			const char*                                 pName)
		{
			PFN_vkVoidFunction realFunc = VulkanHook::vkhooks.GPA_Instance()(instance, pName);

#undef HookInit
#define HookInit(function) if(!strcmp(pName, STRINGIZE(function))) { if(vkhooks.VK.function == NULL) vkhooks.VK.function = (CONCAT(PFN_, function))realFunc; return (PFN_vkVoidFunction)CONCAT(hooked_, function); }

			// VKTODOLOW do we want to care about the case where different instances have
			// different function pointers? at the moment we assume they're all the
			// same.
			// Update - will be fixed by dispatch mechanism
			HookInitVulkan();

			RDCDEBUG("Instance GPA'd function '%s' is not hooked!", pName);
			return realFunc;
		}
		
		static PFN_vkVoidFunction VKAPI vkGetDeviceProcAddr_hooked(
    VkDevice                                    device,
    const char*                                 pName)
		{
			PFN_vkVoidFunction realFunc = VulkanHook::vkhooks.GPA_Device()(device, pName);

#undef HookInit
#define HookInit(function) if(!strcmp(pName, STRINGIZE(function))) { if(vkhooks.VK.function == NULL) vkhooks.VK.function = (CONCAT(PFN_, function))realFunc; return (PFN_vkVoidFunction)CONCAT(hooked_, function); }

			// VKTODOLOW do we want to care about the case where different instances have
			// different function pointers? at the moment we assume they're all the
			// same.
			// Update - will be fixed by dispatch mechanism
			HookInitVulkan();

			RDCDEBUG("Device GPA'd function '%s' is not hooked!", pName);
			return realFunc;
		}

		bool SetupHooks(VulkanFunctions &GL)
		{
			bool success = true;

#undef HookInit
#define HookInit(funcname) \
	success &= CONCAT(hook_, funcname).Initialize(STRINGIZE(funcname), DLL_NAME, &CONCAT(hooked_, funcname)); \
	VK.funcname = CONCAT(hook_, funcname)();

			HookInitVulkan()

			success &= GPA_Instance.Initialize("vkGetInstanceProcAddr", DLL_NAME, vkGetInstanceProcAddr_hooked);
			success &= GPA_Device.Initialize("vkGetDeviceProcAddr", DLL_NAME, vkGetDeviceProcAddr_hooked);

			return success;
		}
		
// implement hooks
#undef HookDefine1
#undef HookDefine2
#undef HookDefine3
#undef HookDefine4
#undef HookDefine5
#undef HookDefine6
#undef HookDefine7

#define HookDefine1(ret, funcname, t0, p0) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0) \
		{ \
			SCOPED_LOCK(vulkanLock); return vkhooks.GetDriver()->funcname(p0); \
		}
#define HookDefine2(ret, funcname, t0, p0, t1, p1) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1) \
		{ \
			SCOPED_LOCK(vulkanLock); return vkhooks.GetDriver()->funcname(p0, p1); \
		}
#define HookDefine3(ret, funcname, t0, p0, t1, p1, t2, p2) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2) \
		{ \
			SCOPED_LOCK(vulkanLock); return vkhooks.GetDriver()->funcname(p0, p1, p2); \
		}
#define HookDefine4(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3) \
		{ \
			SCOPED_LOCK(vulkanLock); return vkhooks.GetDriver()->funcname(p0, p1, p2, p3); \
		}
#define HookDefine5(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4) \
		{ \
			SCOPED_LOCK(vulkanLock); return vkhooks.GetDriver()->funcname(p0, p1, p2, p3, p4); \
		}
#define HookDefine6(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
		{ \
			SCOPED_LOCK(vulkanLock); return vkhooks.GetDriver()->funcname(p0, p1, p2, p3, p4, p5); \
		}
#define HookDefine7(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
		{ \
			SCOPED_LOCK(vulkanLock); return vkhooks.GetDriver()->funcname(p0, p1, p2, p3, p4, p5, p6); \
		}
#define HookDefine8(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
		{ \
			SCOPED_LOCK(vulkanLock); return vkhooks.GetDriver()->funcname(p0, p1, p2, p3, p4, p5, p6, p7); \
		}

		DefineHooks()
};

VulkanHook VulkanHook::vkhooks;

void PopulateDeviceHooks(VkDevice d, VkInstance i)
{
	VulkanHook::vkhooks.PopulateDeviceHooks(d, i);
}

const VulkanFunctions &GetRealVKFunctions() { return VulkanHook::vkhooks.GetRealVKFunctions(); }
