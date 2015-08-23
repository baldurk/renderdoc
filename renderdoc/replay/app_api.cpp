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

#include <string.h>

#include "common/common.h"
#include "core/core.h"
#include "hooks/hooks.h"
#include "api/app/renderdoc_app.h"

static void SetFocusToggleKeys(RENDERDOC_InputButton *keys, int num)
{
	RenderDoc::Inst().SetFocusKeys(keys, num);
}

static void SetCaptureKeys(RENDERDOC_InputButton *keys, int num)
{
	RenderDoc::Inst().SetCaptureKeys(keys, num);
}

static uint32_t GetOverlayBits()
{
	return RenderDoc::Inst().GetOverlayBits();
}

static void MaskOverlayBits(uint32_t And, uint32_t Or)
{
	RenderDoc::Inst().MaskOverlayBits(And, Or);
}

static void Shutdown()
{
	RenderDoc::Inst().Shutdown();
	LibraryHooks::GetInstance().RemoveHooks();
}

static void UnloadCrashHandler()
{
	RenderDoc::Inst().UnloadCrashHandler();
}

static void SetLogFilePathTemplate(const char *logfile)
{
	RDCLOG("Using logfile %s", logfile);
	RenderDoc::Inst().SetLogFile(logfile);
}

static const char *GetLogFilePathTemplate()
{
	return RenderDoc::Inst().GetLogFile();
}

static uint32_t GetNumCaptures()
{
	return (uint32_t)RenderDoc::Inst().GetCaptures().size();
}

static uint32_t GetCapture(uint32_t idx, char *logfile, uint32_t *pathlength, uint64_t *timestamp)
{
	vector<CaptureData> caps = RenderDoc::Inst().GetCaptures();

	if(idx >= (uint32_t)caps.size())
	{
		if(logfile) logfile[0] = 0;
		if(pathlength) *pathlength = 0;
		if(timestamp) *timestamp = 0;
		return 0;
	}

	CaptureData &c = caps[idx];

	if(logfile)
		memcpy(logfile, c.path.c_str(), sizeof(char)*(c.path.size()+1));
	if(pathlength)
		*pathlength = uint32_t(c.path.size()+1);
	if(timestamp)
		*timestamp = c.timestamp;

	return 1;
}

static void TriggerCapture()
{
	RenderDoc::Inst().TriggerCapture();
}

static uint32_t IsRemoteAccessConnected()
{
	return RenderDoc::Inst().IsRemoteAccessConnected();
}

static uint32_t LaunchReplayUI(uint32_t connectRemoteAccess, const char *cmdline)
{
	string replayapp = FileIO::GetReplayAppFilename();

	if(replayapp.empty())
		return 0;

	string cmd = cmdline ? cmdline : "";
	if(connectRemoteAccess)
		cmd += StringFormat::Fmt(" --remoteaccess localhost:%u", RenderDoc::Inst().GetRemoteAccessIdent());

	return Process::LaunchProcess(replayapp.c_str(), "", cmd.c_str());
}

static void SetActiveWindow(void *device, void *wndHandle)
{
	RenderDoc::Inst().SetActiveWindow(device, wndHandle);
}

static void StartFrameCapture(void *device, void *wndHandle)
{
	RenderDoc::Inst().StartFrameCapture(device, wndHandle);
}

static uint32_t IsFrameCapturing()
{
	return RenderDoc::Inst().IsFrameCapturing() ? 1 : 0;
}

static uint32_t EndFrameCapture(void *device, void *wndHandle)
{
	return RenderDoc::Inst().EndFrameCapture(device, wndHandle) ? 1 : 0;
}

// defined in capture_options.cpp
int RENDERDOC_CC SetCaptureOptionU32(RENDERDOC_CaptureOption opt, uint32_t val);
int RENDERDOC_CC SetCaptureOptionF32(RENDERDOC_CaptureOption opt, float val);
uint32_t RENDERDOC_CC GetCaptureOptionU32(RENDERDOC_CaptureOption opt);
float RENDERDOC_CC GetCaptureOptionF32(RENDERDOC_CaptureOption opt);

void RENDERDOC_CC GetAPIVersion_1_0_0(int *major, int *minor, int *patch)
{
	if(major) *major = 1;
	if(minor) *minor = 0;
	if(patch) *patch = 0;
}

RENDERDOC_API_1_0_0 api_1_0_0;
void Init_1_0_0()
{
	api_1_0_0.GetAPIVersion = &GetAPIVersion_1_0_0;

	api_1_0_0.SetCaptureOptionU32 = &SetCaptureOptionU32;
	api_1_0_0.SetCaptureOptionF32 = &SetCaptureOptionF32;

	api_1_0_0.GetCaptureOptionU32 = &GetCaptureOptionU32;
	api_1_0_0.GetCaptureOptionF32 = &GetCaptureOptionF32;

	api_1_0_0.SetFocusToggleKeys = &SetFocusToggleKeys;
	api_1_0_0.SetCaptureKeys = &SetCaptureKeys;

	api_1_0_0.GetOverlayBits = &GetOverlayBits;
	api_1_0_0.MaskOverlayBits = &MaskOverlayBits;

	api_1_0_0.Shutdown = &Shutdown;
	api_1_0_0.UnloadCrashHandler = &UnloadCrashHandler;

	api_1_0_0.SetLogFilePathTemplate = &SetLogFilePathTemplate;
	api_1_0_0.GetLogFilePathTemplate = &GetLogFilePathTemplate;

	api_1_0_0.GetNumCaptures = &GetNumCaptures;
	api_1_0_0.GetCapture = &GetCapture;

	api_1_0_0.TriggerCapture = &TriggerCapture;

	api_1_0_0.IsRemoteAccessConnected = &IsRemoteAccessConnected;
	api_1_0_0.LaunchReplayUI = &LaunchReplayUI;

	api_1_0_0.SetActiveWindow = &SetActiveWindow;

	api_1_0_0.StartFrameCapture = &StartFrameCapture;
	api_1_0_0.IsFrameCapturing = &IsFrameCapturing;
	api_1_0_0.EndFrameCapture = &EndFrameCapture;
}

extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_GetAPI(RENDERDOC_Version version, void **outAPIPointers)
{
	if(outAPIPointers == NULL)
	{
		RDCERR("Invalid call to RENDERDOC_GetAPI with NULL outAPIPointers");
		return 0;
	}

	int ret = 0;
	int major = 0, minor = 0, patch = 0;

#define API_VERSION_HANDLE(enumver, actualver) \
	if(version == CONCAT(eRENDERDOC_API_Version_, enumver)) \
	{ \
		CONCAT(Init_, actualver)(); \
		*outAPIPointers = &CONCAT(api_, actualver); \
		CONCAT(api_, actualver).GetAPIVersion(&major, &minor, &patch); \
		ret = 1; \
	}

	API_VERSION_HANDLE(1_0_0, 1_0_0);

#undef API_VERSION_HANDLE

	if(ret)
	{
		RDCLOG("Initialising RenderDoc API version %d.%d.%d for requested version %d", major, minor, patch, version);
		return 1;
	}

	RDCERR("Unrecognised API version '%d'", version);
	return 0;
}
