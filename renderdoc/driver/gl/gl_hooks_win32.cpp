/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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


#include "driver/gl/gl_common.h"
#include "driver/gl/gl_hookset.h"
#include "driver/gl/gl_driver.h"

#include "driver/gl/gl_hookset_defs.h"

#include "common/threading.h"
#include "serialise/string_utils.h"

#include "hooks/hooks.h"

namespace glEmulate { void EmulateUnsupportedFunctions(GLHookSet *hooks); }

#define DLL_NAME "opengl32.dll"

#define HookInit(function) \
	bool CONCAT(function, _success) = CONCAT(function, _hook).Initialize(STRINGIZE(function), DLL_NAME, CONCAT(function, _hooked)); \
	if(!CONCAT(function, _success)) RDCWARN("Couldn't hook %s", STRINGIZE(function)); \
	success &= CONCAT(function, _success); \
	GL.function = CONCAT(function, _hook)();

#define HookExtension(funcPtrType, function) \
	if(!strcmp(func, STRINGIZE(function))) \
	{ \
		glhooks.GL.function = (funcPtrType)realFunc; \
		return (PROC)&glhooks.CONCAT(function,_hooked); \
	}

#define HookExtensionAlias(funcPtrType, function, alias) \
	if(!strcmp(func, STRINGIZE(alias))) \
	{ \
		glhooks.GL.function = (funcPtrType)realFunc; \
		return (PROC)&glhooks.CONCAT(function,_hooked); \
	}

#if 0 // debug print for each unsupported function requested (but not used)
#define HandleUnsupported(funcPtrType, function) \
	if(lowername == STRINGIZE(function)) \
	{ \
		glhooks.CONCAT(unsupported_real_,function) = (CONCAT(function, _hooktype))realFunc; \
		RDCDEBUG("Requesting function pointer for unsupported function " STRINGIZE(function)); \
		return (PROC)&glhooks.CONCAT(function,_hooked); \
	}
#else
#define HandleUnsupported(funcPtrType, function) \
	if(lowername == STRINGIZE(function)) \
	{ \
		glhooks.CONCAT(unsupported_real_,function) = (CONCAT(function, _hooktype))realFunc; \
		return (PROC)&glhooks.CONCAT(function,_hooked); \
	}
#endif

/*
	in bash:

    function HookWrapper()
    {
        N=$1;
        echo -n "#define HookWrapper$N(ret, function";
            for I in `seq 1 $N`; do echo -n ", t$I, p$I"; done;
        echo ") \\";

        echo -en "\tHook<ret (WINAPI *) (";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo ")> CONCAT(function, _hook); \\";
        
        echo -en "\ttypedef ret (WINAPI *CONCAT(function, _hooktype)) (";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); \\";
        
        echo -en "\tstatic ret WINAPI CONCAT(function, _hooked)(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo ") \\";
        
        echo -en "\t{ SCOPED_LOCK(glLock);";
        echo -en "if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -en "); return glhooks.GetDriver()->function(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -n "); }";
    }

  for I in `seq 0 15`; do HookWrapper $I; echo; done

	*/

// don't want these definitions, the only place we'll use these is as parameter/variable names
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

#define HookWrapper0(ret, function) \
	Hook<ret (WINAPI *) ()> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (); \
	static ret WINAPI CONCAT(function, _hooked)() \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(); return glhooks.GetDriver()->function(); }
#define HookWrapper1(ret, function, t1, p1) \
	Hook<ret (WINAPI *) (t1)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1); return glhooks.GetDriver()->function(p1); }
#define HookWrapper2(ret, function, t1, p1, t2, p2) \
	Hook<ret (WINAPI *) (t1, t2)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2); return glhooks.GetDriver()->function(p1, p2); }
#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3) \
	Hook<ret (WINAPI *) (t1, t2, t3)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3); return glhooks.GetDriver()->function(p1, p2, p3); }
#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4); return glhooks.GetDriver()->function(p1, p2, p3, p4); }
#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5); }
#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6); }
#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6, p7); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7); }
#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6, p7, p8); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8); }
#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6, p7, p8, p9); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9); }
#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); }
#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); }
#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); }
#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); }
#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14); }
#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14, t15 p15) \
	{ SCOPED_LOCK(glLock);if(!glhooks.m_HaveContextCreation) return glhooks.GL.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); }

Threading::CriticalSection glLock;

class OpenGLHook : LibraryHook
{
	public:
		OpenGLHook()
		{
			LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);

			m_GLDriver = NULL;

			m_HaveContextCreation = false;

			m_EnabledHooks = true;
			m_PopulatedHooks = false;

			m_CreatingContext = false;
		}
		~OpenGLHook()
		{
			delete m_GLDriver;
		}

		bool CreateHooks(const char *libName)
		{
			RDCEraseEl(GL);

			if(!m_EnabledHooks)
				return false;
			
			bool success = SetupHooks();

			if(!success) return false;
			
			m_HasHooks = true;

			return true;
		}

		void EnableHooks(const char *libName, bool enable)
		{
			m_EnabledHooks = enable;
		}

		void OptionsUpdated(const char *libName) {}
		
		static OpenGLHook glhooks;

		const GLHookSet &GetRealGLFunctions()
		{
			if(!m_PopulatedHooks)
				m_PopulatedHooks = PopulateHooks();
			return GL;
		}

		void MakeContextCurrent(GLWindowingData data)
		{
			if(wglMakeCurrent_hook())
				wglMakeCurrent_hook()(data.DC, data.ctx);
		}
		
		GLWindowingData MakeContext(GLWindowingData share)
		{
			GLWindowingData ret;
			if(wglCreateContextAttribsARB_realfunc)
			{
				const int attribs[] = {
					WGL_CONTEXT_MAJOR_VERSION_ARB,
					3,
					WGL_CONTEXT_MINOR_VERSION_ARB,
					2,
					WGL_CONTEXT_FLAGS_ARB,
					0,
					WGL_CONTEXT_PROFILE_MASK_ARB,
					WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
					0, 0,
				};
				ret.DC = share.DC;
				m_CreatingContext = true;
				ret.ctx = wglCreateContextAttribsARB_realfunc(share.DC, share.ctx, attribs);
				m_CreatingContext = false;
			}
			return ret;
		}

		void DeleteContext(GLWindowingData context)
		{
			if(context.ctx && wglDeleteContext_hook())
				wglDeleteContext_hook()(context.ctx);
		}

	private:
		WrappedOpenGL *GetDriver()
		{
			if(m_GLDriver == NULL)
				m_GLDriver = new WrappedOpenGL("", GL);

			return m_GLDriver;
		}

		// we use this to check if we've seen a context be created.
		// If we HAVEN'T then RenderDoc was probably injected after
		// the start of the application so we should not call our
		// hooked functions - things will go wrong like missing
		// context data, references to resources we don't know about
		// and hooked functions via wglGetProcAddress being NULL
		// and never being called by the app.
		bool m_HaveContextCreation;

		Hook<HGLRC (WINAPI*)(HDC)> wglCreateContext_hook;
		Hook<BOOL (WINAPI*)(HGLRC)> wglDeleteContext_hook;
		Hook<HGLRC (WINAPI*)(HDC,int)> wglCreateLayerContext_hook;
		Hook<BOOL (WINAPI*)(HDC, HGLRC)> wglMakeCurrent_hook;
		Hook<PROC (WINAPI*)(const char*)> wglGetProcAddress_hook;
		Hook<BOOL (WINAPI*)(HDC)> SwapBuffers_hook;
		Hook<BOOL (WINAPI*)(HDC)> wglSwapBuffers_hook;
		Hook<BOOL (WINAPI*)(HDC,UINT)> wglSwapLayerBuffers_hook;
		Hook<BOOL (WINAPI*)(UINT,CONST WGLSWAP*)> wglSwapMultipleBuffers_hook;
		Hook<LONG (WINAPI*)(DEVMODEA*,DWORD)> ChangeDisplaySettingsA_hook;
		Hook<LONG (WINAPI*)(DEVMODEW*,DWORD)> ChangeDisplaySettingsW_hook;
		Hook<LONG (WINAPI*)(LPCSTR,DEVMODEA*,HWND,DWORD,LPVOID)> ChangeDisplaySettingsExA_hook;
		Hook<LONG (WINAPI*)(LPCWSTR,DEVMODEW*,HWND,DWORD,LPVOID)> ChangeDisplaySettingsExW_hook;

		PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB_realfunc;
		PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB_realfunc;
		PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB_realfunc;
		PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB_realfunc;

		static GLInitParams GetInitParamsForDC(HDC dc)
		{
			GLInitParams ret;

			int pf = GetPixelFormat(dc); 

			PIXELFORMATDESCRIPTOR pfd;
			DescribePixelFormat(dc, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

			HWND w = WindowFromDC(dc);

			RECT r;
			GetClientRect(w, &r);

			RDCLOG("dc %p. PFD: type %d, %d color bits, %d depth bits, %d stencil bits. Win: %dx%d",
			dc,
			pfd.iPixelType, pfd.cColorBits, pfd.cDepthBits, pfd.cStencilBits,
			r.right-r.left, r.bottom-r.top);

			ret.colorBits = pfd.cColorBits;
			ret.depthBits = pfd.cDepthBits;
			ret.stencilBits = pfd.cStencilBits;
			ret.width = (r.right-r.left);
			ret.height = (r.bottom-r.top);

			ret.isSRGB = true;

			if(glhooks.wglGetProcAddress_hook() == NULL)
				glhooks.wglGetProcAddress_hook.SetFuncPtr(Process::GetFunctionAddress(DLL_NAME, "wglGetProcAddress"));

			if(glhooks.wglGetPixelFormatAttribivARB_realfunc == NULL)
				glhooks.wglGetProcAddress_hook()("wglGetPixelFormatAttribivARB");

			if(glhooks.wglGetPixelFormatAttribivARB_realfunc)
			{
				int attrname = eWGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
				int srgb = 1;
				glhooks.wglGetPixelFormatAttribivARB_realfunc(dc, pf, 0, 1, &attrname, &srgb);
				ret.isSRGB = srgb;
				
				attrname = eWGL_SAMPLES_ARB;
				int ms = 1;
				glhooks.wglGetPixelFormatAttribivARB_realfunc(dc, pf, 0, 1, &attrname, &ms);
				ret.multiSamples = RDCMAX(1, ms);
			}

			if(pfd.iPixelType != PFD_TYPE_RGBA)
			{
				RDCERR("Unsupported OpenGL pixel type");
			}

			return ret;
		}

		bool m_CreatingContext;

		static HGLRC WINAPI wglCreateContext_hooked(HDC dc)
		{
			HGLRC ret = glhooks.wglCreateContext_hook()(dc);

			DWORD err = GetLastError();

			// don't recurse and don't continue if creation failed
			if(glhooks.m_CreatingContext || ret == NULL)
				return ret;

			glhooks.m_CreatingContext = true;

			GLWindowingData data;
			data.DC = dc;
			data.wnd = WindowFromDC(dc);
			data.ctx = ret;

			glhooks.GetDriver()->CreateContext(data, NULL, GetInitParamsForDC(dc), false, false);

			glhooks.m_HaveContextCreation = true;

			SetLastError(err);

			glhooks.m_CreatingContext = false;

			return ret;
		}

		static BOOL WINAPI wglDeleteContext_hooked(HGLRC rc)
		{
			if(glhooks.m_HaveContextCreation)
				glhooks.GetDriver()->DeleteContext(rc);

			SetLastError(0);

			return glhooks.wglDeleteContext_hook()(rc);
		}

		static HGLRC WINAPI wglCreateLayerContext_hooked(HDC dc, int iLayerPlane)
		{
			HGLRC ret = glhooks.wglCreateLayerContext_hook()(dc, iLayerPlane);

			DWORD err = GetLastError();

			// don't recurse and don't continue if creation failed
			if(glhooks.m_CreatingContext || ret == NULL)
				return ret;

			glhooks.m_CreatingContext = true;

			GLWindowingData data;
			data.DC = dc;
			data.wnd = WindowFromDC(dc);
			data.ctx = ret;

			glhooks.GetDriver()->CreateContext(data, NULL, GetInitParamsForDC(dc), false, false);

			glhooks.m_HaveContextCreation = true;

			SetLastError(err);

			glhooks.m_CreatingContext = false;

			return ret;
		}

		static HGLRC WINAPI wglCreateContextAttribsARB_hooked(HDC dc, HGLRC hShareContext, const int *attribList)
		{
			// don't recurse
			if(glhooks.m_CreatingContext)
				return glhooks.wglCreateContextAttribsARB_realfunc(dc, hShareContext, attribList);

			glhooks.m_CreatingContext = true;

			int defaultAttribList[] = { 0 };

			const int *attribs = attribList ? attribList : defaultAttribList;
			vector<int> attribVec;

			// modify attribs to our liking
			{
				bool flagsFound = false;
				const int *a = attribs;
				while(*a)
				{
					int name = *a++;
					int val = *a++;

					if(name == WGL_CONTEXT_FLAGS_ARB)
					{
						if(RenderDoc::Inst().GetCaptureOptions().DebugDeviceMode)
							val |= WGL_CONTEXT_DEBUG_BIT_ARB;
						else
							val &= ~WGL_CONTEXT_DEBUG_BIT_ARB;

						// remove NO_ERROR bit
						val &= ~GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR;

						flagsFound = true;
					}
					
					attribVec.push_back(name);
					attribVec.push_back(val);
				}

				if(!flagsFound && RenderDoc::Inst().GetCaptureOptions().DebugDeviceMode)
				{
					attribVec.push_back(WGL_CONTEXT_FLAGS_ARB);
					attribVec.push_back(WGL_CONTEXT_DEBUG_BIT_ARB);
				}

				attribVec.push_back(0);

				attribs = &attribVec[0];
			}

			RDCDEBUG("wglCreateContextAttribsARB:");

			bool core = false;

			int *a = (int *)attribs;
			while(*a)
			{
				RDCDEBUG("%x: %d", a[0], a[1]);

				if(a[0] == WGL_CONTEXT_PROFILE_MASK_ARB)
					core = (a[1] & WGL_CONTEXT_CORE_PROFILE_BIT_ARB);
				
				a += 2;
			}
			
			SetLastError(0);

			HGLRC ret = glhooks.wglCreateContextAttribsARB_realfunc(dc, hShareContext, attribs);

			DWORD err = GetLastError();

			// don't continue if creation failed
			if(ret == NULL)
			{
				glhooks.m_CreatingContext = false;
				return ret;
			}

			GLWindowingData data;
			data.DC = dc;
			data.wnd = WindowFromDC(dc);
			data.ctx = ret;

			glhooks.GetDriver()->CreateContext(data, hShareContext, GetInitParamsForDC(dc), core, true);

			glhooks.m_HaveContextCreation = true;

			SetLastError(err);

			glhooks.m_CreatingContext = false;

			return ret;
		}
		
		static BOOL WINAPI wglChoosePixelFormatARB_hooked(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
		{
			return glhooks.wglChoosePixelFormatARB_realfunc(hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats);
		}
		static BOOL WINAPI wglGetPixelFormatAttribfvARB_hooked(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues)
		{
			return glhooks.wglGetPixelFormatAttribfvARB_realfunc(hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, pfValues);
		}
		static BOOL WINAPI wglGetPixelFormatAttribivARB_hooked(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues)
		{
			return glhooks.wglGetPixelFormatAttribivARB_realfunc(hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, piValues);
		}

		// wglShareLists_hooked ?

		static BOOL WINAPI wglMakeCurrent_hooked(HDC dc, HGLRC rc)
		{
			BOOL ret = glhooks.wglMakeCurrent_hook()(dc, rc);

			DWORD err = GetLastError();

			if(rc && glhooks.m_HaveContextCreation && glhooks.m_Contexts.find(rc) == glhooks.m_Contexts.end())
			{
				glhooks.m_Contexts.insert(rc);

				glhooks.PopulateHooks();
			}

			GLWindowingData data;
			data.DC = dc;
			data.wnd = WindowFromDC(dc);
			data.ctx = rc;
			
			if(glhooks.m_HaveContextCreation)
				glhooks.GetDriver()->ActivateContext(data);

			SetLastError(err);

			return ret;
		}
		
		static void ProcessSwapBuffers(HDC dc)
		{
			HWND w = WindowFromDC(dc);

			if(w != NULL && glhooks.m_HaveContextCreation)
			{
				RECT r;
				GetClientRect(w, &r);

				glhooks.GetDriver()->WindowSize(w, r.right - r.left, r.bottom - r.top);

				glhooks.GetDriver()->SwapBuffers(w);

				SetLastError(0);
			}
		}

		static BOOL WINAPI SwapBuffers_hooked(HDC dc)
		{
			ProcessSwapBuffers(dc);

			return glhooks.SwapBuffers_hook()(dc);
		}

		static BOOL WINAPI wglSwapBuffers_hooked(HDC dc)
		{
			ProcessSwapBuffers(dc);

			return glhooks.wglSwapBuffers_hook()(dc);
		}

		static BOOL WINAPI wglSwapLayerBuffers_hooked(HDC dc, UINT planes)
		{
			ProcessSwapBuffers(dc);

			return glhooks.wglSwapLayerBuffers_hook()(dc, planes);
		}

		static BOOL WINAPI wglSwapMultipleBuffers_hooked(UINT numSwaps, CONST WGLSWAP *pSwaps)
		{
			for(UINT i=0; pSwaps && i < numSwaps; i++)
				ProcessSwapBuffers(pSwaps[i].hdc);

			return glhooks.wglSwapMultipleBuffers_hook()(numSwaps, pSwaps);
		}

		static LONG WINAPI ChangeDisplaySettingsA_hooked(DEVMODEA *mode, DWORD flags)
		{
			if((flags & CDS_FULLSCREEN) == 0 || RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
				return glhooks.ChangeDisplaySettingsA_hook()(mode, flags);

			return DISP_CHANGE_SUCCESSFUL;
		}

		static LONG WINAPI ChangeDisplaySettingsW_hooked(DEVMODEW *mode, DWORD flags)
		{
			if((flags & CDS_FULLSCREEN) == 0 || RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
				return glhooks.ChangeDisplaySettingsW_hook()(mode, flags);

			return DISP_CHANGE_SUCCESSFUL;
		}

		static LONG WINAPI ChangeDisplaySettingsExA_hooked(LPCSTR devname, DEVMODEA *mode, HWND wnd, DWORD flags, LPVOID param)
		{
			if((flags & CDS_FULLSCREEN) == 0 || RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
				return glhooks.ChangeDisplaySettingsExA_hook()(devname, mode, wnd, flags, param);

			return DISP_CHANGE_SUCCESSFUL;
		}

		static LONG WINAPI ChangeDisplaySettingsExW_hooked(LPCWSTR devname, DEVMODEW *mode, HWND wnd, DWORD flags, LPVOID param)
		{
			if((flags & CDS_FULLSCREEN) == 0 || RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
				return glhooks.ChangeDisplaySettingsExW_hook()(devname, mode, wnd, flags, param);

			return DISP_CHANGE_SUCCESSFUL;
		}

		static PROC WINAPI wglGetProcAddress_hooked(const char *func)
		{
			PROC realFunc = glhooks.wglGetProcAddress_hook()(func);
			
#if 0
			RDCDEBUG("Checking for extension - %s - real function is %p", func, realFunc);
#endif

			// if the real RC doesn't support this function, don't bother hooking
			if(realFunc == NULL)
				return realFunc;
			
			if(!strcmp(func, "wglCreateContextAttribsARB"))
			{
				glhooks.wglCreateContextAttribsARB_realfunc = (PFNWGLCREATECONTEXTATTRIBSARBPROC)realFunc;
				return (PROC)&wglCreateContextAttribsARB_hooked;
			}
			if(!strcmp(func, "wglChoosePixelFormatARB"))
			{
				glhooks.wglChoosePixelFormatARB_realfunc = (PFNWGLCHOOSEPIXELFORMATARBPROC)realFunc;
				return (PROC)&wglChoosePixelFormatARB_hooked;
			}
			if(!strcmp(func, "wglGetPixelFormatAttribfvARB"))
			{
				glhooks.wglGetPixelFormatAttribfvARB_realfunc = (PFNWGLGETPIXELFORMATATTRIBFVARBPROC)realFunc;
				return (PROC)&wglGetPixelFormatAttribfvARB_hooked;
			}
			if(!strcmp(func, "wglGetPixelFormatAttribivARB"))
			{
				glhooks.wglGetPixelFormatAttribivARB_realfunc = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)realFunc;
				return (PROC)&wglGetPixelFormatAttribivARB_hooked;
			}
			if(!strncmp(func, "wgl", 3)) // assume wgl functions are safe to just pass straight through
			{
				return realFunc;
			}

			HookCheckGLExtensions();

			// at the moment the unsupported functions are all lowercase (as their name is generated from the
			// typedef name).
			string lowername = strlower(string(func));

			CheckUnsupported();

			// for any other function, if it's not a core or extension function we know about,
			// just return NULL
			return NULL;
		}

		WrappedOpenGL *m_GLDriver;
		
		GLHookSet GL;

		bool m_PopulatedHooks;
		bool m_HasHooks;
		bool m_EnabledHooks;

		set<HGLRC> m_Contexts;

		bool SetupHooks()
		{
			bool success = true;
			
			success &= wglCreateContext_hook.Initialize("wglCreateContext", DLL_NAME, wglCreateContext_hooked);
			success &= wglDeleteContext_hook.Initialize("wglDeleteContext", DLL_NAME, wglDeleteContext_hooked);
			success &= wglCreateLayerContext_hook.Initialize("wglCreateLayerContext", DLL_NAME, wglCreateLayerContext_hooked);
			success &= wglMakeCurrent_hook.Initialize("wglMakeCurrent", DLL_NAME, wglMakeCurrent_hooked);
			success &= wglGetProcAddress_hook.Initialize("wglGetProcAddress", DLL_NAME, wglGetProcAddress_hooked);
			success &= wglSwapBuffers_hook.Initialize("wglSwapBuffers", DLL_NAME, wglSwapBuffers_hooked);
			success &= wglSwapLayerBuffers_hook.Initialize("wglSwapLayerBuffers", DLL_NAME, wglSwapLayerBuffers_hooked);
			success &= wglSwapMultipleBuffers_hook.Initialize("wglSwapMultipleBuffers", DLL_NAME, wglSwapMultipleBuffers_hooked);
			success &= SwapBuffers_hook.Initialize("SwapBuffers", "gdi32.dll", SwapBuffers_hooked);
			success &= ChangeDisplaySettingsA_hook.Initialize("ChangeDisplaySettingsA", "user32.dll", ChangeDisplaySettingsA_hooked);
			success &= ChangeDisplaySettingsW_hook.Initialize("ChangeDisplaySettingsW", "user32.dll", ChangeDisplaySettingsW_hooked);
			success &= ChangeDisplaySettingsExA_hook.Initialize("ChangeDisplaySettingsExA", "user32.dll", ChangeDisplaySettingsExA_hooked);
			success &= ChangeDisplaySettingsExW_hook.Initialize("ChangeDisplaySettingsExW", "user32.dll", ChangeDisplaySettingsExW_hooked);

			DLLExportHooks();

			return success;
		}
		
		bool PopulateHooks()
		{
			void *moduleHandle = Process::LoadModule(DLL_NAME);

			if(wglGetProcAddress_hook() == NULL)
				wglGetProcAddress_hook.SetFuncPtr(Process::GetFunctionAddress(moduleHandle, "wglGetProcAddress"));

			wglGetProcAddress_hooked("wglCreateContextAttribsARB");
			
#undef HookInit
#define HookInit(function) if(GL.function == NULL) GL.function = (CONCAT(function, _hooktype)) Process::GetFunctionAddress(moduleHandle, STRINGIZE(function));
			
			// cheeky
#undef HookExtension
#define HookExtension(funcPtrType, function) wglGetProcAddress_hooked(STRINGIZE(function))
#undef HookExtensionAlias
#define HookExtensionAlias(funcPtrType, function, alias)

			DLLExportHooks();
			HookCheckGLExtensions();

			// see gl_emulated.cpp
			if(RenderDoc::Inst().IsReplayApp()) glEmulate::EmulateUnsupportedFunctions(&GL);

			return true;
		}

		DefineDLLExportHooks();
		DefineGLExtensionHooks();

/*
       in bash:

    function HookWrapper()
    {
        N=$1;
        echo "#undef HookWrapper$N";
        echo -n "#define HookWrapper$N(ret, function";
            for I in `seq 1 $N`; do echo -n ", t$I, p$I"; done;
        echo ") \\";

        echo -en "\ttypedef ret (WINAPI *CONCAT(function, _hooktype)) (";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); \\";

        echo -en "\tCONCAT(function, _hooktype) CONCAT(unsupported_real_,function);";


        echo -en "\tstatic ret WINAPI CONCAT(function, _hooked)(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo ") \\";

        echo -e "\t{ \\";
        echo -e "\tstatic bool hit = false; if(hit == false) { RDCERR(\"Function \" STRINGIZE(function) \" not supported - capture may be broken\"); hit = true; } \\";
        echo -en "\treturn glhooks.CONCAT(unsupported_real_,function)(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -e "); \\";
        echo -e "\t}";
    }

  for I in `seq 0 15`; do HookWrapper $I; echo; done

       */


#undef HookWrapper0
#define HookWrapper0(ret, function) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)() \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(); \
		}

#undef HookWrapper1
#define HookWrapper1(ret, function, t1, p1) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1); \
		}

#undef HookWrapper2
#define HookWrapper2(ret, function, t1, p1, t2, p2) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2); \
		}

#undef HookWrapper3
#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3); \
		}

#undef HookWrapper4
#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4); \
		}

#undef HookWrapper5
#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5); \
		}

#undef HookWrapper6
#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6); \
		}

#undef HookWrapper7
#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6, p7); \
		}

#undef HookWrapper8
#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6, p7, p8); \
		}

#undef HookWrapper9
#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6, p7, p8, p9); \
		}

#undef HookWrapper10
#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); \
		}

#undef HookWrapper11
#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); \
		}

#undef HookWrapper12
#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); \
		}

#undef HookWrapper13
#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); \
		}

#undef HookWrapper14
#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14); \
		}

#undef HookWrapper15
#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15) \
		typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15); \
		CONCAT(function, _hooktype) CONCAT(unsupported_real_,function); static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14, t15 p15) \
		{ \
		static bool hit = false; if(hit == false) { RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); hit = true; } \
		return glhooks.CONCAT(unsupported_real_,function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); \
		}

		DefineUnsupportedDummies();
};

OpenGLHook OpenGLHook::glhooks;

const GLHookSet &GetRealGLFunctions() { return OpenGLHook::glhooks.GetRealGLFunctions(); }

void MakeContextCurrent(GLWindowingData data)
{
	OpenGLHook::glhooks.MakeContextCurrent(data);
}

GLWindowingData MakeContext(GLWindowingData share)
{
	return OpenGLHook::glhooks.MakeContext(share);
}

void DeleteContext(GLWindowingData context)
{
	OpenGLHook::glhooks.DeleteContext(context);
}

// dirty immediate mode rendering functions for backwards compatible
// rendering of overlay text
typedef void (WINAPI *GLGETINTEGERVPROC)(GLenum,GLint*);
typedef void (WINAPI *GLPUSHMATRIXPROC)();
typedef void (WINAPI *GLLOADIDENTITYPROC)();
typedef void (WINAPI *GLMATRIXMODEPROC)(GLenum);
typedef void (WINAPI *GLORTHOPROC)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble);
typedef void (WINAPI *GLPOPMATRIXPROC)();
typedef void (WINAPI *GLBEGINPROC)(GLenum);
typedef void (WINAPI *GLVERTEX2FPROC)(float,float);
typedef void (WINAPI *GLTEXCOORD2FPROC)(float,float);
typedef void (WINAPI *GLENDPROC)();

static GLGETINTEGERVPROC getInt = NULL;
static GLPUSHMATRIXPROC pushm = NULL;
static GLLOADIDENTITYPROC loadident = NULL;
static GLMATRIXMODEPROC matMode = NULL;
static GLORTHOPROC ortho = NULL;
static GLPOPMATRIXPROC popm = NULL;
static GLBEGINPROC begin = NULL;
static GLVERTEX2FPROC v2f = NULL;
static GLTEXCOORD2FPROC t2f = NULL;
static GLENDPROC end = NULL;

const GLenum MAT_MODE = (GLenum)0x0BA0;
const GLenum MAT_MDVW = (GLenum)0x1700;
const GLenum MAT_PROJ = (GLenum)0x1701;

static bool immediateInited = false;

bool immediateBegin(GLenum mode, float width, float height)
{
	if(!immediateInited)
	{
		HMODULE mod = GetModuleHandleA("opengl32.dll");

		if(mod == NULL) return false;

		getInt = (GLGETINTEGERVPROC)GetProcAddress(mod, "glGetIntegerv"); if(!getInt) return false;
		pushm = (GLPUSHMATRIXPROC)GetProcAddress(mod, "glPushMatrix"); if(!pushm) return false;
		loadident = (GLLOADIDENTITYPROC)GetProcAddress(mod, "glLoadIdentity"); if(!loadident) return false;
		matMode = (GLMATRIXMODEPROC)GetProcAddress(mod, "glMatrixMode"); if(!matMode) return false;
		ortho = (GLORTHOPROC)GetProcAddress(mod, "glOrtho"); if(!ortho) return false;
		popm = (GLPOPMATRIXPROC)GetProcAddress(mod, "glPopMatrix"); if(!popm) return false;
		begin = (GLBEGINPROC)GetProcAddress(mod, "glBegin"); if(!begin) return false;
		v2f = (GLVERTEX2FPROC)GetProcAddress(mod, "glVertex2f"); if(!v2f) return false;
		t2f = (GLTEXCOORD2FPROC)GetProcAddress(mod, "glTexCoord2f"); if(!t2f) return false;
		end = (GLENDPROC)GetProcAddress(mod, "glEnd"); if(!end) return false;

		immediateInited = true;
	}
	
	GLenum prevMatMode = eGL_NONE;
	getInt(MAT_MODE, (GLint *)&prevMatMode);

	matMode(MAT_PROJ);
	pushm();
	loadident();
	ortho(0.0, width, height, 0.0, -1.0, 1.0);

	matMode(MAT_MDVW);
	pushm();
	loadident();

	matMode(prevMatMode);

	begin(mode);

	return true;
}

void immediateVert(float x, float y, float u, float v)
{
	t2f(u,v); v2f(x,y);
}

void immediateEnd()
{
	end();
	
	GLenum prevMatMode = eGL_NONE;
	getInt(MAT_MODE, (GLint *)&prevMatMode);

	matMode(MAT_PROJ);
	popm();
	matMode(MAT_MDVW);
	popm();

	matMode(prevMatMode);
}
