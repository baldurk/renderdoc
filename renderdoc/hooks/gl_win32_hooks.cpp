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

#include "common/string_utils.h"

#include "hooks.h"

#define DLL_NAME "opengl32.dll"

#define HookInit(function) \
	success &= CONCAT(function, _hook).Initialize(STRINGIZE(function), DLL_NAME, CONCAT(function, _hooked)); \
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
        
        echo -en "\t{ return glhooks.GetDriver()->function(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -n "); }";
    }

	for I in `seq 0 ...`; do HookWrapper $I; echo; done

	*/

#define HookWrapper0(ret, function) \
        Hook<ret (WINAPI *) ()> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (); \
        static ret WINAPI CONCAT(function, _hooked)() \
        { return glhooks.GetDriver()->function(); }

#define HookWrapper1(ret, function, t1, p1) \
        Hook<ret (WINAPI *) (t1)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1) \
        { return glhooks.GetDriver()->function(p1); }

#define HookWrapper2(ret, function, t1, p1, t2, p2) \
        Hook<ret (WINAPI *) (t1, t2)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2) \
        { return glhooks.GetDriver()->function(p1, p2); }

#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3) \
        Hook<ret (WINAPI *) (t1, t2, t3)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3) \
        { return glhooks.GetDriver()->function(p1, p2, p3); }

#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
        Hook<ret (WINAPI *) (t1, t2, t3, t4)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4) \
        { return glhooks.GetDriver()->function(p1, p2, p3, p4); }

#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
        Hook<ret (WINAPI *) (t1, t2, t3, t4, t5)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
        { return glhooks.GetDriver()->function(p1, p2, p3, p4, p5); }

#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
        Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
        { return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6); }

#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
        Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
        { return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7); }

#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
        Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
        { return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8); }

#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9) \
        Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) \
        { return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9); }

#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10) \
        Hook<ret (WINAPI *) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10)> CONCAT(function, _hook); \
        typedef ret (WINAPI *CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10); \
        static ret WINAPI CONCAT(function, _hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10) \
        { return glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); }

class OpenGLHook : LibraryHook
{
	public:
		OpenGLHook()
		{
			LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);
			
			// TODO: need to check against implementation to ensure we don't claim to support
			// an extension that it doesn't!

			wglExts.push_back("WGL_ARB_extensions_string");
			//wglExts.push_back("WGL_ARB_multisample");
			wglExts.push_back("WGL_ARB_create_context");
			wglExts.push_back("WGL_ARB_create_context_profile");
			
			merge(wglExts, wglExtsString, ' ');

			m_GLDriver = NULL;

#if !DXGL
			m_EnabledHooks = true;
#endif
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

	private:
		WrappedOpenGL *GetDriver()
		{
			if(m_GLDriver == NULL)
				m_GLDriver = new WrappedOpenGL(L"", GL);

			return m_GLDriver;
		}

		Hook<HGLRC (WINAPI*)(HDC)> wglCreateContext_hook;
		Hook<BOOL (WINAPI*)(HDC, HGLRC)> wglMakeCurrent_hook;
		Hook<PROC (WINAPI*)(const char*)> wglGetProcAddress_hook;
		Hook<BOOL (WINAPI*)(HDC)> SwapBuffers_hook;

		PROC (WINAPI* wglGetProcAddress_realfunc)(const char*);
		PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB_realfunc;

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

			if(pfd.iPixelType != PFD_TYPE_RGBA)
			{
				RDCERR("Unsupported OpenGL pixel type");
			}

			return ret;
		}

		static HGLRC WINAPI wglCreateContext_hooked(HDC dc)
		{
			HGLRC ret = glhooks.wglCreateContext_hook()(dc);

			glhooks.GetDriver()->CreateContext(WindowFromDC(dc), ret, NULL, GetInitParamsForDC(dc));

			return ret;
		}

		static HGLRC WINAPI wglCreateContextAttribsARB_hooked(HDC dc, HGLRC hShareContext, const int *attribList)
		{
			HGLRC ret = glhooks.wglCreateContextAttribsARB_realfunc(dc, hShareContext, attribList);
			
			glhooks.GetDriver()->CreateContext(WindowFromDC(dc), ret, hShareContext, GetInitParamsForDC(dc));

			return ret;
		}

		// wglShareLists_hooked ?

		static BOOL WINAPI wglMakeCurrent_hooked(HDC dc, HGLRC rc)
		{
			BOOL ret = glhooks.wglMakeCurrent_hook()(dc, rc);

			glhooks.GetDriver()->ActivateContext(WindowFromDC(dc), rc);

			return ret;
		}

		static BOOL WINAPI SwapBuffers_hooked(HDC dc)
		{
			HWND w = WindowFromDC(dc);

			RECT r;
			GetClientRect(w, &r);

			glhooks.GetDriver()->WindowSize(w, r.right-r.left, r.bottom-r.top);

			glhooks.GetDriver()->Present(dc);

			return glhooks.SwapBuffers_hook()(dc);
		}
		
		static const char * WINAPI wglGetExtensionsStringARB_hooked(HDC dc)
		{
#if !defined(_RELEASE)
			PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtStrARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)glhooks.wglGetProcAddress_realfunc("wglGetExtensionsStringARB");
			string realExtsString = wglGetExtStrARB(dc);
			vector<string> realExts;
			split(realExtsString, realExts, ' ');
#endif
			return glhooks.wglExtsString.c_str();
		}
		static const char * WINAPI wglGetExtensionsStringEXT_hooked()
		{
#if !defined(_RELEASE)
			PFNWGLGETEXTENSIONSSTRINGEXTPROC wglGetExtStrEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)glhooks.wglGetProcAddress_realfunc("wglGetExtensionsStringEXT");
			string realExtsString = wglGetExtStrEXT();
			vector<string> realExts;
			split(realExtsString, realExts, ' ');
#endif
			return glhooks.wglExtsString.c_str();
		}

		static PROC WINAPI wglGetProcAddress_hooked(const char *func)
		{
			PROC realFunc = glhooks.wglGetProcAddress_realfunc(func);

			// if the real RC doesn't support this function, don't bother hooking
			if(realFunc == NULL)
				return realFunc;
			
			if(!strcmp(func, "wglGetExtensionsStringEXT"))
			{
				return (PROC)&wglGetExtensionsStringEXT_hooked;
			}
			if(!strcmp(func, "wglGetExtensionsStringARB"))
			{
				return (PROC)&wglGetExtensionsStringARB_hooked;
			}
			if(!strcmp(func, "wglCreateContextAttribsARB"))
			{
				glhooks.wglCreateContextAttribsARB_realfunc = (PFNWGLCREATECONTEXTATTRIBSARBPROC)realFunc;
				return (PROC)&wglCreateContextAttribsARB_hooked;
			}

			HookCheckWGLExtensions();
			HookCheckGLExtensions();

			// claim not to know this extension!
			RDCWARN("Claiming not to know extension that is available - %hs", func);
			return NULL;
		}

		WrappedOpenGL *m_GLDriver;
		
		GLHookSet GL;
		
		vector<string> wglExts;
		string wglExtsString;

		bool m_PopulatedHooks;
		bool m_HasHooks;
		bool m_EnabledHooks;

		bool SetupHooks(GLHookSet &GL)
		{
			bool success = true;
			
			success &= wglCreateContext_hook.Initialize("wglCreateContext", DLL_NAME, wglCreateContext_hooked);
			success &= wglMakeCurrent_hook.Initialize("wglMakeCurrent", DLL_NAME, wglMakeCurrent_hooked);
			success &= wglGetProcAddress_hook.Initialize("wglGetProcAddress", DLL_NAME, wglGetProcAddress_hooked);
			success &= SwapBuffers_hook.Initialize("SwapBuffers", "gdi32.dll", SwapBuffers_hooked);

			wglGetProcAddress_realfunc = wglGetProcAddress_hook();
			
			DLLExportHooks();

			return success;
		}
		
		bool PopulateHooks()
		{
			bool success = true;

			if(wglGetProcAddress_realfunc == NULL)
				wglGetProcAddress_realfunc = (PROC (WINAPI*)(const char*))Process::GetFunctionAddress(DLL_NAME, "wglGetProcAddress");

			wglGetProcAddress_hooked("wglCreateContextAttribsARB");
			
#undef HookInit
#define HookInit(function) if(GL.function == NULL) GL.function = (CONCAT(function, _hooktype)) Process::GetFunctionAddress(DLL_NAME, STRINGIZE(function));
			
			// cheeky
#undef HookExtension
#define HookExtension(funcPtrType, function) wglGetProcAddress_hooked(STRINGIZE(function))
#undef HookExtensionAlias
#define HookExtensionAlias(funcPtrType, function, alias)

			DLLExportHooks();
			HookCheckWGLExtensions();
			HookCheckGLExtensions();

			return true;
		}

		DefineDLLExportHooks();
		DefineWGLExtensionHooks();
		DefineGLExtensionHooks();
};

OpenGLHook OpenGLHook::glhooks;

const GLHookSet &GetRealFunctions() { return OpenGLHook::glhooks.GetRealFunctions(); }
