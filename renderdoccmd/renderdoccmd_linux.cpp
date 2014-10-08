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

#include <unistd.h>

#include <replay/renderdoc_replay.h>

using std::wstring;

wstring GetUsername()
{
	char buf[256] = {0};
	getlogin_r(buf, 255);

	return wstring(buf, buf+strlen(buf));
}

void DisplayRendererPreview(ReplayRenderer *renderer)
{
	if(renderer == NULL) return;

	// TODO!
}

// symbol defined in libGL but not librenderdoc.
// Forces link of libGL after renderdoc (otherwise all symbols would
// be resolved and libGL wouldn't link, meaning dlsym(RTLD_NEXT) would fai
extern "C" void glXWaitGL();

int renderdoccmd(int argc, wchar_t **argv);

int main(int argc, char *argv[])
{
	std::setlocale(LC_CTYPE, "");

	volatile bool never_run = false; if(never_run) glXWaitGL();

	// do any linux-specific setup here

	// process any linux-specific arguments here

	wchar_t **wargv = new wchar_t*[argc];

	iconv_t ic = iconv_open("WCHAR_T", "UTF-8");

	for(int i=0; i < argc; i++)
	{
		size_t len = strlen(argv[i]);
		wargv[i] = new wchar_t[len+2];
		memset(wargv[i], 0, (len+2)*sizeof(wchar_t));

		char *inbuf = argv[i];
		size_t insize = len+1; // include null terminator
		char *outbuf = (char *)wargv[i];
		size_t outsize = len*sizeof(wchar_t);
		iconv(ic, &inbuf, &insize, &outbuf, &outsize);
	}

	iconv_close(ic);

	int ret = renderdoccmd(argc, wargv);

	for(int i=0; i < argc; i++)
		delete[] wargv[i];
	delete[] wargv;

	return ret;
}
