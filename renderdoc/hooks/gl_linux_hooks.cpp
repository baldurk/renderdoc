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

#include <dlfcn.h>
#include <stdio.h>

#include "hooks/hooks.h"

#include "driver/gl/gl_common.h"
#include "driver/gl/gl_hookset.h"
#include "driver/gl/gl_driver.h"

#include "common/string_utils.h"

// bit of a hack
namespace Keyboard { extern Display *CurrentXDisplay; }

typedef GLXContext (*PFNGLXCREATECONTEXTPROC)(Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct);
typedef const char *(*PFNGLXQUERYEXTENSIONSSTRING)(Display *dpy, int screen);
typedef Bool (*PFNGLXMAKECURRENTPROC)(Display *dpy, GLXDrawable drawable, GLXContext ctx);
typedef void (*PFNGLXSWAPBUFFERSPROC)(Display *dpy, GLXDrawable drawable);

#define HookInit(function) \
	GL.function = (CONCAT(function, _hooktype))dlsym(RTLD_NEXT, STRINGIZE(function));

#define HookExtension(funcPtrType, function) \
	if(!strcmp(func, STRINGIZE(function))) \
	{ \
		OpenGLHook::glhooks.GL.function = (funcPtrType)realFunc; \
		return (__GLXextFuncPtr)&function; \
	}

#define HookExtensionAlias(funcPtrType, function, alias) \
	if(!strcmp(func, STRINGIZE(alias))) \
	{ \
		OpenGLHook::glhooks.GL.function = (funcPtrType)realFunc; \
		return (__GLXextFuncPtr)&function; \
	}

/*
	in bash:

    function HookWrapper()
    {
        N=$1;
        echo -n "#define HookWrapper$N(ret, function";
            for I in `seq 1 $N`; do echo -n ", t$I, p$I"; done;
        echo ") \\";

        echo -en "\ttypedef ret (*CONCAT(function, _hooktype)) (";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); \\";
		
		echo -e "\textern \"C\" __attribute__ ((visibility (\"default\"))) \\";
        
        echo -en "\tret function(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo ") \\";
        
        echo -en "\t{ return OpenGLHook::glhooks.GetDriver()->function(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -n "); }";
    }

	for I in `seq 0 ...`; do HookWrapper $I; echo; done

	*/

#define HookWrapper0(ret, function) \
	typedef ret (*CONCAT(function, _hooktype)) (); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function() \
	{ return OpenGLHook::glhooks.GetDriver()->function(); }
#define HookWrapper1(ret, function, t1, p1) \
	typedef ret (*CONCAT(function, _hooktype)) (t1); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1); }
#define HookWrapper2(ret, function, t1, p1, t2, p2) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2); }
#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2, t3); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3); }
#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2, t3, t4); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4); }
#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5); }
#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6); }
#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7); }
#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8); }
#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9); }
#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); }
#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11) \
	typedef ret (*CONCAT(function, _hooktype)) (t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11); \
	extern "C" __attribute__ ((visibility ("default"))) \
	ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11) \
	{ return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); }


class OpenGLHook : LibraryHook
{
	public:
		OpenGLHook()
		{
			LibraryHooks::GetInstance().RegisterHook("libGL.so", this);
			
			// TODO: need to check against implementation to ensure we don't claim to support
			// an extension that it doesn't!

			glXExts.push_back("GLX_ARB_extensions_string");
			//glXExts.push_back("GLX_ARB_multisample");
			glXExts.push_back("GLX_ARB_create_context");
			glXExts.push_back("GLX_ARB_create_context_profile");
			
			merge(glXExts, glXExtsString, ' ');

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

		WrappedOpenGL *GetDriver()
		{
			if(m_GLDriver == NULL)
				m_GLDriver = new WrappedOpenGL(L"", GL);

			return m_GLDriver;
		}

		PFNGLXCREATECONTEXTPROC glXCreateContext_real;
		PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB_real;
		PFNGLXGETPROCADDRESSPROC glXGetProcAddress_real;
		PFNGLXMAKECURRENTPROC glXMakeCurrent_real;
		PFNGLXSWAPBUFFERSPROC glXSwapBuffers_real;

		WrappedOpenGL *m_GLDriver;
		
		GLHookSet GL;
		
		vector<string> glXExts;
		string glXExtsString;

		bool m_PopulatedHooks;
		bool m_HasHooks;
		bool m_EnabledHooks;

		bool SetupHooks(GLHookSet &GL);
		bool PopulateHooks();
};

DefineDLLExportHooks();
DefineGLExtensionHooks();

typedef XVisualInfo* (*PFNGLXGETVISUALFROMFBCONFIGPROC)(Display *dpy, GLXFBConfig config);
typedef int (*PFNGLXGETCONFIGPROC)(Display *dpy, XVisualInfo *vis, int attrib, int * value);

static PFNGLXGETCONFIGPROC getVisualInfoAttrib = NULL;

__attribute__ ((visibility ("default")))
GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct)
{
	GLXContext ret = OpenGLHook::glhooks.glXCreateContext_real(dpy, vis, shareList, direct);
	
	GLInitParams init;
	
	init.width = 0;
	init.height = 0;
	
	int value = 0;
	
	if(Keyboard::CurrentXDisplay == NULL) Keyboard::CurrentXDisplay = dpy;	
	if(getVisualInfoAttrib == NULL) getVisualInfoAttrib = (PFNGLXGETCONFIGPROC)dlsym(RTLD_NEXT, "glXGetConfig");
	
	getVisualInfoAttrib(dpy, vis, GLX_BUFFER_SIZE, &value); init.colorBits = value;
	getVisualInfoAttrib(dpy, vis, GLX_DEPTH_SIZE, &value); init.depthBits = value;
	getVisualInfoAttrib(dpy, vis, GLX_STENCIL_SIZE, &value); init.stencilBits = value;

	OpenGLHook::glhooks.GetDriver()->CreateContext(NULL, ret, shareList, init);

	return ret;
}

__attribute__ ((visibility ("default")))
GLXContext glXCreateContextAttribsARB(Display *dpy, GLXFBConfig config, GLXContext shareList, Bool direct, const int *attribList)
{
	GLXContext ret = OpenGLHook::glhooks.glXCreateContextAttribsARB_real(dpy, config, shareList, direct, attribList);
	
	PFNGLXGETVISUALFROMFBCONFIGPROC getVisual = (PFNGLXGETVISUALFROMFBCONFIGPROC)dlsym(RTLD_NEXT, "glXGetVisualFromFBConfig");
	XVisualInfo *vis = getVisual(dpy, config);	
	
	GLInitParams init;
	
	init.width = 0;
	init.height = 0;
	
	int value = 0;
	
	if(Keyboard::CurrentXDisplay == NULL) Keyboard::CurrentXDisplay = dpy;
	if(getVisualInfoAttrib == NULL) getVisualInfoAttrib = (PFNGLXGETCONFIGPROC)dlsym(RTLD_NEXT, "glXGetConfig");
	
	getVisualInfoAttrib(dpy, vis, GLX_BUFFER_SIZE, &value); init.colorBits = value;
	getVisualInfoAttrib(dpy, vis, GLX_DEPTH_SIZE, &value); init.depthBits = value;
	getVisualInfoAttrib(dpy, vis, GLX_STENCIL_SIZE, &value); init.stencilBits = value;

	XFree(vis);
	
	OpenGLHook::glhooks.GetDriver()->CreateContext(NULL, ret, shareList, init);

	return ret;
}

__attribute__ ((visibility ("default")))
Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
	Bool ret = OpenGLHook::glhooks.glXMakeCurrent_real(dpy, drawable, ctx);
	
	OpenGLHook::glhooks.GetDriver()->ActivateContext((void *)drawable, ctx);
	
	return ret;
}

__attribute__ ((visibility ("default")))
void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
	Window root;
	int x, y;
	unsigned int width, height, border_width, depth;
	XGetGeometry(dpy, drawable, &root, &x, &y, &width, &height, &border_width, &depth);	
	
	OpenGLHook::glhooks.GetDriver()->WindowSize((void *)drawable, width, height);
	
	OpenGLHook::glhooks.GetDriver()->Present((void *)drawable);

	OpenGLHook::glhooks.glXSwapBuffers_real(dpy, drawable);
}

__attribute__ ((visibility ("default")))
const char *glXQueryExtensionsString(Display *dpy, int screen)
{
#if !defined(_RELEASE)
	PFNGLXQUERYEXTENSIONSSTRING glXGetExtStr = (PFNGLXQUERYEXTENSIONSSTRING)dlsym(RTLD_NEXT, "glXQueryExtensionsString");
	string realExtsString = glXGetExtStr(dpy, screen);
	vector<string> realExts;
	split(realExtsString, realExts, ' ');
#endif
	return OpenGLHook::glhooks.glXExtsString.c_str();
}

bool OpenGLHook::SetupHooks(GLHookSet &GL)
{
	bool success = true;
	
	glXGetProcAddress_real = (PFNGLXGETPROCADDRESSPROC)dlsym(RTLD_NEXT, "glXGetProcAddress");
	glXCreateContext_real = (PFNGLXCREATECONTEXTPROC)dlsym(RTLD_NEXT, "glXCreateContext");
	glXCreateContextAttribsARB_real = (PFNGLXCREATECONTEXTATTRIBSARBPROC)dlsym(RTLD_NEXT, "glXCreateContextAttribsARB");
	glXMakeCurrent_real = (PFNGLXMAKECURRENTPROC)dlsym(RTLD_NEXT, "glXMakeCurrent");
	glXSwapBuffers_real = (PFNGLXSWAPBUFFERSPROC)dlsym(RTLD_NEXT, "glXSwapBuffers");
	
	DLLExportHooks();

	return success;
}

__attribute__ ((visibility ("default")))
__GLXextFuncPtr glXGetProcAddress(const GLubyte *f)
{
	__GLXextFuncPtr realFunc = OpenGLHook::glhooks.glXGetProcAddress_real(f);
	const char *func = (const char *)f;

	// if the real RC doesn't support this function, don't bother hooking
	if(realFunc == NULL)
		return realFunc;
	
	if(!strcmp(func, "glXCreateContextAttribsARB"))
	{
		OpenGLHook::glhooks.glXCreateContextAttribsARB_real = (PFNGLXCREATECONTEXTATTRIBSARBPROC)realFunc;
		return (__GLXextFuncPtr)&glXCreateContextAttribsARB;
	}

	HookCheckGLExtensions();

	// claim not to know this extension!
	RDCWARN("Claiming not to know extension that is available - %s", func);
	return NULL;
}

bool OpenGLHook::PopulateHooks()
{
	bool success = true;

	if(glXGetProcAddress_real == NULL)
		glXGetProcAddress_real = (PFNGLXGETPROCADDRESSPROC)dlsym(RTLD_NEXT, "glXGetProcAddress");

	glXGetProcAddress_real((const GLubyte *)"glXCreateContextAttribsARB");
	
#undef HookInit
#define HookInit(function) if(GL.function == NULL) GL.function = (CONCAT(function, _hooktype))dlsym(RTLD_NEXT, "glXGetProcAddress");
	
	// cheeky
#undef HookExtension
#define HookExtension(funcPtrType, function) glXGetProcAddress((const GLubyte *)STRINGIZE(function))
#undef HookExtensionAlias
#define HookExtensionAlias(funcPtrType, function, alias)

	DLLExportHooks();
	HookCheckGLExtensions();

	return true;
}

OpenGLHook OpenGLHook::glhooks;

const GLHookSet &GetRealFunctions() { return OpenGLHook::glhooks.GetRealFunctions(); }
