/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include "api/app/renderdoc_app.h"
#include "api/replay/renderdoc_replay.h"    // for RENDERDOC_API to export the RENDERDOC_GetAPI function
#include "common/common.h"
#include "core/core.h"
#include "hooks/hooks.h"

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
    if(logfile)
      logfile[0] = 0;
    if(pathlength)
      *pathlength = 0;
    if(timestamp)
      *timestamp = 0;
    return 0;
  }

  CaptureData &c = caps[idx];

  if(logfile)
    memcpy(logfile, c.path.c_str(), sizeof(char) * (c.path.size() + 1));
  if(pathlength)
    *pathlength = uint32_t(c.path.size() + 1);
  if(timestamp)
    *timestamp = c.timestamp;

  return 1;
}

static void TriggerCapture()
{
  RenderDoc::Inst().TriggerCapture(1);
}

static void TriggerMultiFrameCapture(uint32_t numFrames)
{
  RenderDoc::Inst().TriggerCapture(numFrames);
}

static uint32_t IsTargetControlConnected()
{
  return RenderDoc::Inst().IsTargetControlConnected();
}

static uint32_t LaunchReplayUI(uint32_t connectTargetControl, const char *cmdline)
{
  string replayapp = FileIO::GetReplayAppFilename();

  if(replayapp.empty())
    return 0;

  string cmd = cmdline ? cmdline : "";
  if(connectTargetControl)
    cmd += StringFormat::Fmt(" --targetcontrol localhost:%u",
                             RenderDoc::Inst().GetTargetControlIdent());

  return Process::LaunchProcess(replayapp.c_str(), "", cmd.c_str());
}

static void SetActiveWindow(void *device, void *wndHandle)
{
  RenderDoc::Inst().SetActiveWindow(device, wndHandle);
}

static void StartFrameCapture(void *device, void *wndHandle)
{
  RenderDoc::Inst().StartFrameCapture(device, wndHandle);

  if(device == NULL || wndHandle == NULL)
    RenderDoc::Inst().MatchClosestWindow(device, wndHandle);

  if(device != NULL && wndHandle != NULL)
    RenderDoc::Inst().SetActiveWindow(device, wndHandle);
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

void RENDERDOC_CC GetAPIVersion_1_1_1(int *major, int *minor, int *patch)
{
  if(major)
    *major = 1;
  if(minor)
    *minor = 1;
  if(patch)
    *patch = 1;
}

RENDERDOC_API_1_1_1 api_1_1_1;
void Init_1_1_1()
{
  RENDERDOC_API_1_1_1 &api = api_1_1_1;

  api.GetAPIVersion = &GetAPIVersion_1_1_1;

  api.SetCaptureOptionU32 = &SetCaptureOptionU32;
  api.SetCaptureOptionF32 = &SetCaptureOptionF32;

  api.GetCaptureOptionU32 = &GetCaptureOptionU32;
  api.GetCaptureOptionF32 = &GetCaptureOptionF32;

  api.SetFocusToggleKeys = &SetFocusToggleKeys;
  api.SetCaptureKeys = &SetCaptureKeys;

  api.GetOverlayBits = &GetOverlayBits;
  api.MaskOverlayBits = &MaskOverlayBits;

  api.Shutdown = &Shutdown;
  api.UnloadCrashHandler = &UnloadCrashHandler;

  api.SetLogFilePathTemplate = &SetLogFilePathTemplate;
  api.GetLogFilePathTemplate = &GetLogFilePathTemplate;

  api.GetNumCaptures = &GetNumCaptures;
  api.GetCapture = &GetCapture;

  api.TriggerCapture = &TriggerCapture;

  api.IsTargetControlConnected = &IsTargetControlConnected;
  api.LaunchReplayUI = &LaunchReplayUI;

  api.SetActiveWindow = &SetActiveWindow;

  api.StartFrameCapture = &StartFrameCapture;
  api.IsFrameCapturing = &IsFrameCapturing;
  api.EndFrameCapture = &EndFrameCapture;

  api.TriggerMultiFrameCapture = &TriggerMultiFrameCapture;
}

extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_GetAPI(RENDERDOC_Version version,
                                                           void **outAPIPointers)
{
  if(outAPIPointers == NULL)
  {
    RDCERR("Invalid call to RENDERDOC_GetAPI with NULL outAPIPointers");
    return 0;
  }

  int ret = 0;
  int major = 0, minor = 0, patch = 0;

  string supportedVersions = "";

#define API_VERSION_HANDLE(enumver, actualver)                     \
  supportedVersions += " " STRINGIZE(CONCAT(API_, enumver));       \
  if(version == CONCAT(eRENDERDOC_API_Version_, enumver))          \
  {                                                                \
    CONCAT(Init_, actualver)();                                    \
    *outAPIPointers = &CONCAT(api_, actualver);                    \
    CONCAT(api_, actualver).GetAPIVersion(&major, &minor, &patch); \
    ret = 1;                                                       \
  }

  API_VERSION_HANDLE(1_0_0, 1_1_1);
  API_VERSION_HANDLE(1_0_1, 1_1_1);
  API_VERSION_HANDLE(1_0_2, 1_1_1);
  API_VERSION_HANDLE(1_1_0, 1_1_1);
  API_VERSION_HANDLE(1_1_1, 1_1_1);

#undef API_VERSION_HANDLE

  if(ret)
  {
    RDCLOG("Initialising RenderDoc API version %d.%d.%d for requested version %d", major, minor,
           patch, version);
    return 1;
  }

  RDCERR("Unrecognised API version '%d'. Supported versions:%s", version, supportedVersions.c_str());

  return 0;
}
