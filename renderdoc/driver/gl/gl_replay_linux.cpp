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


#include "gl_replay.h"
#include "gl_driver.h"
#include "gl_resources.h"

#include <dlfcn.h>

typedef Bool (*PFNGLXMAKECURRENTPROC)(Display *dpy, GLXDrawable drawable, GLXContext ctx);
typedef void (*PFNGLXDESTROYCONTEXTPROC)(Display *dpy, GLXContext ctx);
typedef void (*PFNGLXSWAPBUFFERSPROC)(Display *dpy, GLXDrawable drawable);

PFNGLXCHOOSEFBCONFIGPROC glXChooseFBConfigProc = NULL;
PFNGLXCREATEPBUFFERPROC glXCreatePbufferProc = NULL;
PFNGLXDESTROYPBUFFERPROC glXDestroyPbufferProc = NULL;
PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsProc = NULL;
PFNGLXGETPROCADDRESSPROC glXGetFuncProc = NULL;
PFNGLXMAKECONTEXTCURRENTPROC glXMakeContextCurrentProc = NULL;
PFNGLXQUERYDRAWABLEPROC glXQueryDrawableProc = NULL;
PFNGLXDESTROYCONTEXTPROC glXDestroyCtxProc = NULL;
PFNGLXSWAPBUFFERSPROC glXSwapProc = NULL;

void GLReplay::MakeCurrentReplayContext(GLWindowingData *ctx)
{
	static GLWindowingData *prev = NULL;

	if(glXMakeContextCurrentProc && ctx && ctx != prev)
	{
		prev = ctx;
		glXMakeContextCurrentProc(ctx->dpy, ctx->wnd, ctx->wnd, ctx->ctx);
		m_pDriver->ActivateContext(ctx->wnd, ctx->ctx);
	}
}

void GLReplay::SwapBuffers(GLWindowingData *ctx)
{
	glXSwapProc(ctx->dpy, ctx->wnd);
}

void GLReplay::CloseReplayContext()
{
	if(glXDestroyCtxProc)
	{
		glXDestroyCtxProc(m_ReplayCtx.dpy, m_ReplayCtx.ctx);
	}
}

uint64_t GLReplay::MakeOutputWindow(void *wn, bool depth)
{
	void **displayAndDrawable = (void **)wn;

	Display *dpy = NULL;
	GLXDrawable wnd = 0;

	if(wn)
	{
		dpy = (Display *)displayAndDrawable[0];
		wnd = (GLXDrawable)displayAndDrawable[1];
	}
	else
	{
		dpy = XOpenDisplay(NULL);

		if(dpy == NULL)
			return 0;
	}

	static int visAttribs[] = { 
		GLX_X_RENDERABLE, True,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_DOUBLEBUFFER, True,
		0
	};
	int numCfgs = 0;
	GLXFBConfig *fbcfg = glXChooseFBConfigProc(dpy, DefaultScreen(dpy), visAttribs, &numCfgs);

	if(fbcfg == NULL)
	{
		XCloseDisplay(dpy);
		RDCERR("Couldn't choose default framebuffer config");
		return eReplayCreate_APIInitFailed;
	}

	int attribs[64] = {0};
	int i=0;

	attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
	attribs[i++] = 4;
	attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
	attribs[i++] = 3;
	attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
	attribs[i++] = GLX_CONTEXT_DEBUG_BIT_ARB;

	GLXContext ctx = glXCreateContextAttribsProc(dpy, fbcfg[0], 0, true, attribs);

	if(ctx == NULL)
	{
		XCloseDisplay(dpy);
		RDCERR("Couldn't create 4.3 context - RenderDoc requires OpenGL 4.3 availability");
		return 0;
	}

	if(wnd == 0)
	{
		// don't care about pbuffer properties as we won't render directly to this
		int pbAttribs[] = { GLX_PBUFFER_WIDTH, 32, GLX_PBUFFER_HEIGHT, 32, 0 };

		wnd = glXCreatePbufferProc(dpy, fbcfg[0], pbAttribs);
	}

	XFree(fbcfg);

	OutputWindow win;
	win.dpy = dpy;
	win.ctx = ctx;
	win.wnd = wnd;

	glXQueryDrawableProc(dpy, wnd, GLX_WIDTH, (unsigned int *)&win.width);
	glXQueryDrawableProc(dpy, wnd, GLX_HEIGHT, (unsigned int *)&win.height);

	MakeCurrentReplayContext(&win);

	InitOutputWindow(win);
	CreateOutputWindowBackbuffer(win);

	uint64_t ret = m_OutputWindowID++;

	m_OutputWindows[ret] = win;

	return ret;
}

void GLReplay::DestroyOutputWindow(uint64_t id)
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	glXDestroyCtxProc(outw.dpy, outw.ctx);

	m_OutputWindows.erase(it);
}

void GLReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	OutputWindow &outw = m_OutputWindows[id];
	
	glXQueryDrawableProc(outw.dpy, outw.wnd, GLX_WIDTH, (unsigned int *)&w);
	glXQueryDrawableProc(outw.dpy, outw.wnd, GLX_HEIGHT, (unsigned int *)&h);
}

bool GLReplay::IsOutputWindowVisible(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return false;

	GLNOTIMP("Optimisation missing - output window always returning true");

	return true;
}

const GLHookSet &GetRealFunctions();

ReplayCreateStatus GL_CreateReplayDevice(const wchar_t *logfile, IReplayDriver **driver)
{
	RDCDEBUG("Creating an OpenGL replay device");

	if(glXCreateContextAttribsProc == NULL)
	{
		glXGetFuncProc = (PFNGLXGETPROCADDRESSPROC)dlsym(RTLD_NEXT, "glXGetProcAddress");
		glXDestroyCtxProc = (PFNGLXDESTROYCONTEXTPROC)dlsym(RTLD_NEXT, "glXDestroyContext");
		glXSwapProc = (PFNGLXSWAPBUFFERSPROC)dlsym(RTLD_NEXT, "glXSwapBuffers");
		glXChooseFBConfigProc = (PFNGLXCHOOSEFBCONFIGPROC)dlsym(RTLD_NEXT, "glXChooseFBConfig");
		glXCreatePbufferProc = (PFNGLXCREATEPBUFFERPROC)dlsym(RTLD_NEXT, "glXCreatePbuffer");
		glXDestroyPbufferProc = (PFNGLXDESTROYPBUFFERPROC)dlsym(RTLD_NEXT, "glXDestroyPbuffer");
		glXQueryDrawableProc = (PFNGLXQUERYDRAWABLEPROC)dlsym(RTLD_NEXT, "glXQueryDrawable");

		if(glXGetFuncProc == NULL || glXDestroyCtxProc == NULL ||
			 glXSwapProc == NULL || glXChooseFBConfigProc == NULL ||
			 glXCreatePbufferProc == NULL || glXDestroyPbufferProc == NULL ||
			 glXQueryDrawableProc == NULL)
		{
			RDCERR("Couldn't find required entry points, glXGetProcAddress glXDestroyContext glXSwapBuffers");
			return eReplayCreate_APIInitFailed;
		}

		glXCreateContextAttribsProc = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetFuncProc((const GLubyte*)"glXCreateContextAttribsARB");
		glXMakeContextCurrentProc = (PFNGLXMAKECONTEXTCURRENTPROC)glXGetFuncProc((const GLubyte*)"glXMakeContextCurrent");

		if(glXCreateContextAttribsProc == NULL || glXMakeContextCurrentProc == NULL)
		{
			RDCERR("Couldn't get glx function addresses, glXCreateContextAttribsARB glXMakeContextCurrent");
			return eReplayCreate_APIInitFailed;
		}
	}

	GLInitParams initParams;
	RDCDriver driverType = RDC_OpenGL;
	wstring driverName = L"OpenGL";
	if(logfile)
		RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, (RDCInitParams *)&initParams);

	if(initParams.SerialiseVersion != GLInitParams::GL_SERIALISE_VERSION)
	{
		RDCERR("Incompatible OpenGL serialise version, expected %d got %d", GLInitParams::GL_SERIALISE_VERSION, initParams.SerialiseVersion);
		return eReplayCreate_APIIncompatibleVersion;
	}

	int attribs[64] = {0};
	int i=0;

	attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
	attribs[i++] = 4;
	attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
	attribs[i++] = 3;
	attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
	attribs[i++] = GLX_CONTEXT_DEBUG_BIT_ARB;

	Display *dpy = XOpenDisplay(NULL);

	if(dpy == NULL)
	{
		RDCERR("Couldn't open default X display");
		return eReplayCreate_APIInitFailed;
	}

	// don't need to care about the fb config as we won't be using the default framebuffer (backbuffer)
	static int visAttribs[] = { 0 };
	int numCfgs = 0;
	GLXFBConfig *fbcfg = glXChooseFBConfigProc(dpy, DefaultScreen(dpy), visAttribs, &numCfgs);

	if(fbcfg == NULL)
	{
		XCloseDisplay(dpy);
		RDCERR("Couldn't choose default framebuffer config");
		return eReplayCreate_APIInitFailed;
	}

	GLXContext ctx = glXCreateContextAttribsProc(dpy, fbcfg[0], 0, true, attribs);

	if(ctx == NULL)
	{
		XCloseDisplay(dpy);
		RDCERR("Couldn't create 4.3 context - RenderDoc requires OpenGL 4.3 availability");
		return eReplayCreate_APIHardwareUnsupported;
	}

	// don't care about pbuffer properties for same reason as backbuffer
	int pbAttribs[] = { GLX_PBUFFER_WIDTH, 32, GLX_PBUFFER_HEIGHT, 32, 0 };

	GLXPbuffer pbuffer = glXCreatePbufferProc(dpy, fbcfg[0], pbAttribs);

	XFree(fbcfg);

	Bool res = glXMakeContextCurrentProc(dpy, pbuffer, pbuffer, ctx);

	if(!res)
	{
		glXDestroyPbufferProc(dpy, pbuffer);
		glXDestroyCtxProc(dpy, ctx);
		XCloseDisplay(dpy);
		RDCERR("Couldn't make pbuffer & context current");
		return eReplayCreate_APIInitFailed;
	}

	WrappedOpenGL *gl = new WrappedOpenGL(logfile, GetRealFunctions());
	gl->Initialise(initParams);

	RDCLOG("Created device.");
	GLReplay *replay = gl->GetReplay();
	replay->SetProxy(logfile == NULL);
	GLWindowingData data;
	data.dpy = dpy;
	data.ctx = ctx;
	data.wnd = pbuffer;
	replay->SetReplayData(data);

	*driver = (IReplayDriver *)replay;
	return eReplayCreate_Success;
}
