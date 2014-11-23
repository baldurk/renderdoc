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

#include <string>

#include <string.h>
#include <locale.h>
#include <iconv.h>

#include <X11/X.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

#include <unistd.h>

#include <replay/renderdoc_replay.h>

using std::string;

string GetUsername()
{
	char buf[256] = {0};
	getlogin_r(buf, 255);

	return string(buf, buf+strlen(buf));
}

void DisplayRendererPreview(ReplayRenderer *renderer, TextureDisplay displayCfg)
{
	Display *dpy = XOpenDisplay(NULL);

	if(dpy == NULL) return;

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
	GLXFBConfig *fbcfg = glXChooseFBConfig(dpy, DefaultScreen(dpy), visAttribs, &numCfgs);

	if(fbcfg == NULL)
	{
		XCloseDisplay(dpy);
		return;
	}

	XVisualInfo *vInfo = glXGetVisualFromFBConfig(dpy, fbcfg[0]);

	XSetWindowAttributes swa = {0};
	swa.event_mask = StructureNotifyMask;
	swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vInfo->screen), vInfo->visual, AllocNone);

	Window win = XCreateWindow(dpy, RootWindow(dpy, vInfo->screen), 200, 200, 1280, 720,
	                           0, vInfo->depth, InputOutput, vInfo->visual,
	                           CWBorderPixel | CWColormap | CWEventMask, &swa);

	XStoreName(dpy, win, "renderdoccmd");
	XMapWindow(dpy, win);

	GLXWindow glwnd = glXCreateWindow(dpy, fbcfg[0], win, NULL);

	void *displayAndDrawable[] = { (void *)dpy, (void *)glwnd };

	ReplayOutput *out = ReplayRenderer_CreateOutput(renderer, displayAndDrawable);

	OutputConfig c = { eOutputType_TexDisplay };

	ReplayOutput_SetOutputConfig(out, c);
	ReplayOutput_SetTextureDisplay(out, displayCfg);

	bool done = false;
	while(!done)
	{
		while(XPending(dpy) > 0)
		{
			XEvent event = {0};
			XNextEvent(dpy, &event);
			switch(event.type)
			{
				case ButtonPress: done = true;
				default:          break;
			}
		}

		ReplayRenderer_SetFrameEvent(renderer, 0, 10000000+rand()%1000);
		ReplayOutput_Display(out);

		usleep(40000);
	}
}

// symbol defined in libGL but not librenderdoc.
// Forces link of libGL after renderdoc (otherwise all symbols would
// be resolved and libGL wouldn't link, meaning dlsym(RTLD_NEXT) would fai
extern "C" void glXWaitGL();

int renderdoccmd(int argc, char **argv);

int main(int argc, char *argv[])
{
	std::setlocale(LC_CTYPE, "");

	volatile bool never_run = false; if(never_run) glXWaitGL();

	// do any linux-specific setup here

	// process any linux-specific arguments here

	return renderdoccmd(argc, argv);
}
