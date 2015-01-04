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

#include "core/core.h"
#include "hooks/hooks.h"
#include "os/os_specific.h"

// DllMain equivalent
__attribute__((constructor))
void library_loaded()
{
	string curfile;
	FileIO::GetExecutableFilename(curfile);
	
	if(curfile.find("/renderdoccmd") != string::npos ||
	   curfile.find("/renderdocui") != string::npos)
	{
		RDCDEBUG("Not creating hooks - in replay app");
		
		RenderDoc::Inst().SetReplayApp(true);
		
		RenderDoc::Inst().Initialise();
		
		return;
	}
	else
	{
		RenderDoc::Inst().Initialise();

		char *logfile = getenv("RENDERDOC_LOGFILE");
		char *opts = getenv("RENDERDOC_CAPTUREOPTS");

		if(opts)
		{
			string optstr = opts;

			CaptureOptions optstruct;
			optstruct.FromString(optstr);

			RenderDoc::Inst().SetCaptureOptions(&optstruct);
		}

		if(logfile)
		{
			RenderDoc::Inst().SetLogFile(logfile);
		}
	
		RDCLOG("Loading into %s", curfile.c_str());
	
		LibraryHooks::GetInstance().CreateHooks();
	}
}
