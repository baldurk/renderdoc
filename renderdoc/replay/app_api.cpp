/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "serialise/rdcfile.h"

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
  LibraryHooks::RemoveHooks();
}

static void UnloadCrashHandler()
{
  RenderDoc::Inst().UnloadCrashHandler();
}

static void SetCaptureFilePathTemplate(const char *pathtemplate)
{
  RDCLOG("Using capture file template %s", pathtemplate);
  RenderDoc::Inst().SetCaptureFileTemplate(pathtemplate);
}

static const char *GetCaptureFilePathTemplate()
{
  return RenderDoc::Inst().GetCaptureFileTemplate();
}

static uint32_t GetNumCaptures()
{
  return (uint32_t)RenderDoc::Inst().GetCaptures().size();
}

static uint32_t GetCapture(uint32_t idx, char *filename, uint32_t *pathlength, uint64_t *timestamp)
{
  std::vector<CaptureData> caps = RenderDoc::Inst().GetCaptures();

  if(idx >= (uint32_t)caps.size())
  {
    if(filename)
      filename[0] = 0;
    if(pathlength)
      *pathlength = 0;
    if(timestamp)
      *timestamp = 0;
    return 0;
  }

  CaptureData &c = caps[idx];

  if(filename)
    memcpy(filename, c.path.c_str(), sizeof(char) * (c.path.size() + 1));
  if(pathlength)
    *pathlength = uint32_t(c.path.size() + 1);
  if(timestamp)
    *timestamp = c.timestamp;

  return 1;
}

static void SetCaptureFileComments(const char *filePath, const char *comments)
{
  std::string path;
  if(filePath == NULL || filePath[0] == 0)
  {
    std::vector<CaptureData> caps = RenderDoc::Inst().GetCaptures();
    if(caps.empty())
    {
      RDCERR(
          "SetCaptureFileComments called with NULL/empty filePath, but no captures have been made");
      return;
    }

    path = caps.back().path;
  }
  else
  {
    path = filePath;
  }

  RDCFile rdc;
  rdc.Open(path.c_str());
  if(rdc.ErrorCode() != ContainerError::NoError)
  {
    RDCERR("Error opening '%s' to add capture comments", path.c_str());
    return;
  }

  SectionProperties props;
  props.type = SectionType::Notes;
  props.version = 1;

  StreamWriter *writer = rdc.WriteSection(props);

  if(comments)
  {
    std::string commentsjson = "{\"comments\":\"";

    commentsjson.reserve(strlen(comments));

    const char *c = comments;

    while(*c)
    {
      // escape some characters
      if(*c == '"')
        commentsjson += "\\\"";
      else if(*c == '\\')
        commentsjson += "\\\\";
      else if(*c == '\b')
        commentsjson += "\\b";
      else if(*c == '\f')
        commentsjson += "\\f";
      else if(*c == '\n')
        commentsjson += "\\n";
      else if(*c == '\r')
        commentsjson += "\\r";
      else if(*c == '\t')
        commentsjson += "\\t";
      else
        commentsjson.push_back(*c);

      c++;
    }

    commentsjson += "\"}";

    writer->Write(commentsjson.c_str(), commentsjson.size());
  }

  delete writer;
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
  std::string replayapp = FileIO::GetReplayAppFilename();

  if(replayapp.empty())
    return 0;

  std::string cmd = cmdline ? cmdline : "";
  if(connectTargetControl)
    cmd += StringFormat::Fmt(" --targetcontrol localhost:%u",
                             RenderDoc::Inst().GetTargetControlIdent());

  return Process::LaunchProcess(replayapp.c_str(), "", cmd.c_str(), false);
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

static uint32_t DiscardFrameCapture(void *device, void *wndHandle)
{
  return RenderDoc::Inst().DiscardFrameCapture(device, wndHandle) ? 1 : 0;
}

// defined in capture_options.cpp
int RENDERDOC_CC SetCaptureOptionU32(RENDERDOC_CaptureOption opt, uint32_t val);
int RENDERDOC_CC SetCaptureOptionF32(RENDERDOC_CaptureOption opt, float val);
uint32_t RENDERDOC_CC GetCaptureOptionU32(RENDERDOC_CaptureOption opt);
float RENDERDOC_CC GetCaptureOptionF32(RENDERDOC_CaptureOption opt);

void RENDERDOC_CC GetAPIVersion_1_4_0(int *major, int *minor, int *patch)
{
  if(major)
    *major = 1;
  if(minor)
    *minor = 4;
  if(patch)
    *patch = 0;
}

RENDERDOC_API_1_4_0 api_1_4_0;
void Init_1_4_0()
{
  RENDERDOC_API_1_4_0 &api = api_1_4_0;

  api.GetAPIVersion = &GetAPIVersion_1_4_0;

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

  api.SetCaptureFilePathTemplate = &SetCaptureFilePathTemplate;
  api.GetCaptureFilePathTemplate = &GetCaptureFilePathTemplate;

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

  api.SetCaptureFileComments = &SetCaptureFileComments;

  api.DiscardFrameCapture = &DiscardFrameCapture;
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

  std::string supportedVersions = "";

#define API_VERSION_HANDLE(enumver, actualver)                     \
  supportedVersions += " " STRINGIZE(CONCAT(API_, enumver));       \
  if(version == CONCAT(eRENDERDOC_API_Version_, enumver))          \
  {                                                                \
    CONCAT(Init_, actualver)();                                    \
    *outAPIPointers = &CONCAT(api_, actualver);                    \
    CONCAT(api_, actualver).GetAPIVersion(&major, &minor, &patch); \
    ret = 1;                                                       \
  }

  API_VERSION_HANDLE(1_0_0, 1_4_0);
  API_VERSION_HANDLE(1_0_1, 1_4_0);
  API_VERSION_HANDLE(1_0_2, 1_4_0);
  API_VERSION_HANDLE(1_1_0, 1_4_0);
  API_VERSION_HANDLE(1_1_1, 1_4_0);
  API_VERSION_HANDLE(1_1_2, 1_4_0);
  API_VERSION_HANDLE(1_2_0, 1_4_0);
  API_VERSION_HANDLE(1_3_0, 1_4_0);
  API_VERSION_HANDLE(1_4_0, 1_4_0);

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
