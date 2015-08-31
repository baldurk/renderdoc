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

#define DLL_NAME "vulkan.dll"

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
			RDCEraseEl(real);

			if(!m_EnabledHooks)
				return false;

			bool success = SetupHooks(real);

			if(!success) return false;
			
			m_HasHooks = true;

			return true;
		}

		void EnableHooks(const char *dllName, bool enable)
		{
			m_EnabledHooks = enable;
		}
		
		static VulkanHook vulkanhooks;

		const VulkanFunctions &GetRealVKFunctions()
		{
			if(!m_PopulatedHooks)
				m_PopulatedHooks = PopulateHooks();
			return real;
		}

	private:
		WrappedVulkan *GetDriver()
		{
			if(m_Driver == NULL)
				m_Driver = new WrappedVulkan(real, "");

			GetRealVKFunctions();

			return m_Driver;
		}

		WrappedVulkan *m_Driver;
		
		VulkanFunctions real;

		bool m_PopulatedHooks;
		bool m_HasHooks;
		bool m_EnabledHooks;
		
		bool SetupHooks(VulkanFunctions &GL)
		{
			bool success = true;

#undef HookInit
#define HookInit(funcname) \
	success &= CONCAT(hook_, funcname).Initialize(STRINGIZE(funcname), DLL_NAME, &CONCAT(hooked_, funcname)); \
	real.funcname = CONCAT(hook_, funcname)();

			HookInitVulkan()

			return success;
		}
		
		bool PopulateHooks()
		{
			bool success = true;

			HMODULE mod = GetModuleHandleA(DLL_NAME);

			if(mod == NULL)
				return false;
			
			// fetch real pointers via GetProcAddress
#undef HookInit
#define HookInit(function) \
				if(real.function == NULL) real.function = (CONCAT(PFN_, function)) Process::GetFunctionAddress(DLL_NAME, STRINGIZE(function)); \
				success &= (real.function != NULL);

			HookInitVulkan()

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
			SCOPED_LOCK(vulkanLock); return vulkanhooks.GetDriver()->funcname(p0); \
		}
#define HookDefine2(ret, funcname, t0, p0, t1, p1) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1) \
		{ \
			SCOPED_LOCK(vulkanLock); return vulkanhooks.GetDriver()->funcname(p0, p1); \
		}
#define HookDefine3(ret, funcname, t0, p0, t1, p1, t2, p2) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2) \
		{ \
			SCOPED_LOCK(vulkanLock); return vulkanhooks.GetDriver()->funcname(p0, p1, p2); \
		}
#define HookDefine4(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3) \
		{ \
			SCOPED_LOCK(vulkanLock); return vulkanhooks.GetDriver()->funcname(p0, p1, p2, p3); \
		}
#define HookDefine5(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4) \
		{ \
			SCOPED_LOCK(vulkanLock); return vulkanhooks.GetDriver()->funcname(p0, p1, p2, p3, p4); \
		}
#define HookDefine6(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
		{ \
			SCOPED_LOCK(vulkanLock); return vulkanhooks.GetDriver()->funcname(p0, p1, p2, p3, p4, p5); \
		}
#define HookDefine7(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
		{ \
			SCOPED_LOCK(vulkanLock); return vulkanhooks.GetDriver()->funcname(p0, p1, p2, p3, p4, p5, p6); \
		}
#define HookDefine8(ret, funcname, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
		Hook<CONCAT(PFN_, funcname)> CONCAT(hook_, funcname); \
		static ret VKAPI CONCAT(hooked_, funcname)(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
		{ \
			SCOPED_LOCK(vulkanLock); return vulkanhooks.GetDriver()->funcname(p0, p1, p2, p3, p4, p5, p6, p7); \
		}

		DefineHooks()
};

VulkanHook VulkanHook::vulkanhooks;

void PopulateDeviceHooks(VkDevice d, VkInstance i)
{
	// don't need to bother with an impl because this file will be replaced
	// before we ever run on windows
}

const VulkanFunctions &GetRealVKFunctions() { return VulkanHook::vulkanhooks.GetRealVKFunctions(); }
