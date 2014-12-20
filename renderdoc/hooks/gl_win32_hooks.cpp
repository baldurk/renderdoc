/******************************************************************************
 * The MIT License (MIT)
 * 
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

#include "hooks.h"

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
        
        echo -en "\t{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(";
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
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(); }

#define HookWrapper1(ret, function, t1, p1) \
	Hook<ret (WINAPI *) (t1)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1); }

#define HookWrapper2(ret, function, t1, p1, t2, p2) \
	Hook<ret (WINAPI *) (t1, t2)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2); }

#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3) \
	Hook<ret (WINAPI *) (t1, t2, t3)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3); }

#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4); }

#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5); }

#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6); }

#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7); }

#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8); }

#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9); }

#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); }

#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); }

#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); }

#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); }

#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14); }

#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15) \
	Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15)> CONCAT(function, _hook); \
	typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15); \
	static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14, t15 p15) \
	{ SCOPED_LOCK(glLock); return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); }

Threading::CriticalSection glLock;

class OpenGLHook : LibraryHook
{
	public:
		OpenGLHook()
		{
			LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);

			m_GLDriver = NULL;

			m_EnabledHooks = true;
			m_PopulatedHooks = false;
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
			
			bool success = SetupHooks(GL);

			if(!success) return false;
			
			m_HasHooks = true;

			return true;
		}

		void EnableHooks(const char *libName, bool enable)
		{
			m_EnabledHooks = enable;
		}
		
		static OpenGLHook glhooks;

		const GLHookSet &GetRealFunctions()
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
			GLWindowingData ret = {0};
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
				ret.ctx = wglCreateContextAttribsARB_realfunc(share.DC, share.ctx, attribs);
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

		Hook<HGLRC (WINAPI*)(HDC)> wglCreateContext_hook;
		Hook<BOOL (WINAPI*)(HGLRC)> wglDeleteContext_hook;
		Hook<HGLRC (WINAPI*)(HDC,int)> wglCreateLayerContext_hook;
		Hook<BOOL (WINAPI*)(HDC, HGLRC)> wglMakeCurrent_hook;
		Hook<PROC (WINAPI*)(const char*)> wglGetProcAddress_hook;
		Hook<BOOL (WINAPI*)(HDC)> SwapBuffers_hook;
		Hook<LONG (WINAPI*)(DEVMODEA*,DWORD)> ChangeDisplaySettingsA_hook;
		Hook<LONG (WINAPI*)(DEVMODEW*,DWORD)> ChangeDisplaySettingsW_hook;
		Hook<LONG (WINAPI*)(LPCSTR,DEVMODEA*,HWND,DWORD,LPVOID)> ChangeDisplaySettingsExA_hook;
		Hook<LONG (WINAPI*)(LPCWSTR,DEVMODEW*,HWND,DWORD,LPVOID)> ChangeDisplaySettingsExW_hook;

		PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB_realfunc;
		PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB_realfunc;
		PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB_realfunc;
		PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB_realfunc;

		static private GLInitParams GetInitParamsForDC(HDC dc)
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

		static HGLRC WINAPI wglCreateContext_hooked(HDC dc)
		{
			HGLRC ret = glhooks.wglCreateContext_hook()(dc);

			GLWindowingData data;
			data.DC = dc;
			data.wnd = WindowFromDC(dc);
			data.ctx = ret;

			glhooks.GetDriver()->CreateContext(data, NULL, GetInitParamsForDC(dc), false);

			return ret;
		}

		static BOOL WINAPI wglDeleteContext_hooked(HGLRC rc)
		{
			glhooks.GetDriver()->DeleteContext(rc);

			return glhooks.wglDeleteContext_hook()(rc);
		}

		static HGLRC WINAPI wglCreateLayerContext_hooked(HDC dc, int iLayerPlane)
		{
			HGLRC ret = glhooks.wglCreateLayerContext_hook()(dc, iLayerPlane);

			GLWindowingData data;
			data.DC = dc;
			data.wnd = WindowFromDC(dc);
			data.ctx = ret;

			glhooks.GetDriver()->CreateContext(data, NULL, GetInitParamsForDC(dc), false);

			return ret;
		}

		static HGLRC WINAPI wglCreateContextAttribsARB_hooked(HDC dc, HGLRC hShareContext, const int *attribList)
		{
			const int *attribs = attribList;
			vector<int> attribVec;

			if(RenderDoc::Inst().GetCaptureOptions().DebugDeviceMode)
			{
				bool flagsNext = false;
				bool flagsFound = false;
				const int *a = attribList;
				while(*a)
				{
					int val = *a;

					if(flagsNext)
					{
						val |= WGL_CONTEXT_DEBUG_BIT_ARB;
						flagsNext = false;
					}

					if(val == WGL_CONTEXT_FLAGS_ARB)
					{
						flagsNext = true;
						flagsFound = true;
					}
					
					attribVec.push_back(val);

					a++;
				}

				if(!flagsFound)
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
				a += 2;

				if(a[0] == WGL_CONTEXT_PROFILE_MASK_ARB)
					core = (a[1] & WGL_CONTEXT_CORE_PROFILE_BIT_ARB);
			}
			
			HGLRC ret = glhooks.wglCreateContextAttribsARB_realfunc(dc, hShareContext, attribs);

			GLWindowingData data;
			data.DC = dc;
			data.wnd = WindowFromDC(dc);
			data.ctx = ret;

			glhooks.GetDriver()->CreateContext(data, hShareContext, GetInitParamsForDC(dc), core);

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

			if(rc && glhooks.m_Contexts.find(rc) == glhooks.m_Contexts.end())
			{
				glhooks.m_Contexts.insert(rc);

				glhooks.PopulateHooks();
			}

			GLWindowingData data;
			data.DC = dc;
			data.wnd = WindowFromDC(dc);
			data.ctx = rc;

			glhooks.GetDriver()->ActivateContext(data);

			return ret;
		}

		static BOOL WINAPI SwapBuffers_hooked(HDC dc)
		{
			HWND w = WindowFromDC(dc);

			RECT r;
			GetClientRect(w, &r);

			glhooks.GetDriver()->WindowSize(w, r.right-r.left, r.bottom-r.top);

			glhooks.GetDriver()->Present(w);

			return glhooks.SwapBuffers_hook()(dc);
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

		bool SetupHooks(GLHookSet &GL)
		{
			bool success = true;
			
			success &= wglCreateContext_hook.Initialize("wglCreateContext", DLL_NAME, wglCreateContext_hooked);
			success &= wglDeleteContext_hook.Initialize("wglDeleteContext", DLL_NAME, wglDeleteContext_hooked);
			success &= wglCreateLayerContext_hook.Initialize("wglCreateLayerContext", DLL_NAME, wglCreateLayerContext_hooked);
			success &= wglMakeCurrent_hook.Initialize("wglMakeCurrent", DLL_NAME, wglMakeCurrent_hooked);
			success &= wglGetProcAddress_hook.Initialize("wglGetProcAddress", DLL_NAME, wglGetProcAddress_hooked);
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
			bool success = true;

			if(wglGetProcAddress_hook() == NULL)
				wglGetProcAddress_hook.SetFuncPtr(Process::GetFunctionAddress(DLL_NAME, "wglGetProcAddress"));

			wglGetProcAddress_hooked("wglCreateContextAttribsARB");
			
#undef HookInit
#define HookInit(function) if(GL.function == NULL) GL.function = (CONCAT(function, _hooktype)) Process::GetFunctionAddress(DLL_NAME, STRINGIZE(function));
			
			// cheeky
#undef HookExtension
#define HookExtension(funcPtrType, function) wglGetProcAddress_hooked(STRINGIZE(function))
#undef HookExtensionAlias
#define HookExtensionAlias(funcPtrType, function, alias)

			DLLExportHooks();
			HookCheckGLExtensions();

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

const GLHookSet &GetRealFunctions() { return OpenGLHook::glhooks.GetRealFunctions(); }

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
