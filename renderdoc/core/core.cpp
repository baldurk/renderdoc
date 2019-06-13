/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include <time.h>
#include <algorithm>
#include "api/replay/version.h"
#include "common/common.h"
#include "hooks/hooks.h"
#include "maths/formatpacking.h"
#include "replay/replay_driver.h"
#include "serialise/rdcfile.h"
#include "serialise/serialiser.h"
#include "stb/stb_image_write.h"
#include "strings/string_utils.h"
#include "crash_handler.h"

#include "api/replay/renderdoc_tostr.inl"

#include "api/replay/pipestate.inl"

#include "replay/renderdoc_serialise.inl"

// this one is done by hand as we format it
template <>
rdcstr DoStringise(const ResourceId &el)
{
  RDCCOMPILE_ASSERT(sizeof(el) == sizeof(uint64_t), "ResourceId is no longer 1:1 with uint64_t");

  // below is equivalent to:
  // return StringFormat::Fmt("ResourceId::%llu", el);

  uint64_t num = 0;
  memcpy(&num, &el, sizeof(uint64_t));

#define PREFIX "ResourceId::"

  // hardcode empty/null ResourceId to both avoid special case below and fast-path a common case as
  // a string literal.
  if(num == 0)
    return PREFIX "0";

  // enough for prefix and a 64-bit value in decimal
  char str[48] = {};

  RDCCOMPILE_ASSERT(ARRAY_COUNT(str) > sizeof(PREFIX) + 20, "Scratch buffer is not large enough");

  // ARRAY_COUNT(str) - 1 would point us at the last element, we go one further back to leave a
  // trailing NUL character
  char *c = str + ARRAY_COUNT(str) - 2;

  // build up digits in reverse order from the end of the buffer
  while(num)
  {
    *(c--) = char((num % 10) + '0');
    num /= 10;
  }

  // the length is sizeof(PREFIX) - 1, the index of the last actual character is - 2. Saves us a -1
  // in the loop below.
  const size_t prefixlast = sizeof(PREFIX) - 2;

  // add the prefix (in reverse order)
  for(size_t i = 0; i <= prefixlast; i++)
    *(c--) = PREFIX[prefixlast - i];

  // the loop will have stepped us to the first NULL before our string, so return c+1
  return c + 1;
}

BASIC_TYPE_SERIALISE_STRINGIFY(ResourceId, (uint64_t &)el, SDBasic::Resource, 8);

INSTANTIATE_SERIALISE_TYPE(ResourceId);

#if ENABLED(RDOC_LINUX) && ENABLED(RDOC_XLIB)
#include <X11/Xlib.h>
#endif

// from image_viewer.cpp
ReplayStatus IMG_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver);

template <>
rdcstr DoStringise(const RDCDriver &el)
{
  BEGIN_ENUM_STRINGISE(RDCDriver);
  {
    STRINGISE_ENUM_CLASS(Unknown);
    STRINGISE_ENUM_CLASS(OpenGL);
    STRINGISE_ENUM_CLASS(OpenGLES);
    STRINGISE_ENUM_CLASS(Mantle);
    STRINGISE_ENUM_CLASS(D3D12);
    STRINGISE_ENUM_CLASS(D3D11);
    STRINGISE_ENUM_CLASS(D3D10);
    STRINGISE_ENUM_CLASS(D3D9);
    STRINGISE_ENUM_CLASS(D3D8);
    STRINGISE_ENUM_CLASS(Image);
    STRINGISE_ENUM_CLASS(Vulkan);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ReplayLogType &el)
{
  BEGIN_ENUM_STRINGISE(ReplayLogType);
  {
    STRINGISE_ENUM_NAMED(eReplay_Full, "Full replay including draw");
    STRINGISE_ENUM_NAMED(eReplay_WithoutDraw, "Replay without draw");
    STRINGISE_ENUM_NAMED(eReplay_OnlyDraw, "Replay only draw");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const VendorExtensions &el)
{
  BEGIN_ENUM_STRINGISE(VendorExtensions);
  {
    STRINGISE_ENUM_CLASS(NvAPI);
    STRINGISE_ENUM_CLASS_NAMED(OpenGL_Ext, "Unsupported GL extensions");
    STRINGISE_ENUM_CLASS_NAMED(Vulkan_Ext, "Unsupported Vulkan extensions");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const RENDERDOC_InputButton &el)
{
  char alphanumericbuf[2] = {'A', 0};

  // enums map straight to ascii
  if((el >= eRENDERDOC_Key_A && el <= eRENDERDOC_Key_Z) ||
     (el >= eRENDERDOC_Key_0 && el <= eRENDERDOC_Key_9))
  {
    alphanumericbuf[0] = (char)el;
    return alphanumericbuf;
  }

  BEGIN_ENUM_STRINGISE(RENDERDOC_InputButton);
  {
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Divide, "/");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Multiply, "*");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Subtract, "-");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Plus, "+");

    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F1, "F1");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F2, "F2");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F3, "F3");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F4, "F4");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F5, "F5");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F6, "F6");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F7, "F7");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F8, "F8");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F9, "F9");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F10, "F10");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F11, "F11");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_F12, "F12");

    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Home, "Home");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_End, "End");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Insert, "Insert");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Delete, "Delete");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_PageUp, "PageUp");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_PageDn, "PageDn");

    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Backspace, "Backspace");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Tab, "Tab");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_PrtScrn, "PrtScrn");
    STRINGISE_ENUM_NAMED(eRENDERDOC_Key_Pause, "Pause");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const SystemChunk &el)
{
  BEGIN_ENUM_STRINGISE(SystemChunk);
  {
    STRINGISE_ENUM_CLASS_NAMED(DriverInit, "Driver Initialisation Parameters");
    STRINGISE_ENUM_CLASS_NAMED(InitialContentsList, "List of Initial Contents Resources");
    STRINGISE_ENUM_CLASS_NAMED(InitialContents, "Initial Contents");
    STRINGISE_ENUM_CLASS_NAMED(CaptureBegin, "Beginning of Capture");
    STRINGISE_ENUM_CLASS_NAMED(CaptureScope, "Frame Metadata");
    STRINGISE_ENUM_CLASS_NAMED(CaptureEnd, "End of Capture");
  }
  END_ENUM_STRINGISE();
}

RenderDoc *RenderDoc::m_Inst = NULL;

RenderDoc &RenderDoc::Inst()
{
  static RenderDoc realInst;
  RenderDoc::m_Inst = &realInst;
  return realInst;
}

void RenderDoc::RecreateCrashHandler()
{
#if ENABLED(RDOC_CRASH_HANDLER)
  m_ExHandler = new CrashHandler(m_ExHandler);
#endif

  if(m_ExHandler)
    m_ExHandler->RegisterMemoryRegion(this, sizeof(RenderDoc));
}

void RenderDoc::UnloadCrashHandler()
{
  if(m_ExHandler)
    m_ExHandler->UnregisterMemoryRegion(this);

  SAFE_DELETE(m_ExHandler);
}

RenderDoc::RenderDoc()
{
  m_CaptureFileTemplate = "";
  m_MarkerIndentLevel = 0;

  m_CapturesActive = 0;

  m_RemoteIdent = 0;
  m_RemoteThread = 0;

  m_Replay = false;

  m_Cap = 0;

  m_FocusKeys.clear();
  m_FocusKeys.push_back(eRENDERDOC_Key_F11);

  m_CaptureKeys.clear();
  m_CaptureKeys.push_back(eRENDERDOC_Key_F12);
  m_CaptureKeys.push_back(eRENDERDOC_Key_PrtScrn);

  m_ExHandler = NULL;

  m_Overlay = eRENDERDOC_Overlay_Default;

  m_VulkanCheck = NULL;
  m_VulkanInstall = NULL;

  m_TargetControlThreadShutdown = false;
  m_ControlClientThreadShutdown = false;
}

void RenderDoc::Initialise()
{
  Callstack::Init();

  Network::Init();

  Threading::Init();

  m_RemoteIdent = 0;
  m_RemoteThread = 0;

  if(!IsReplayApp())
  {
    Process::ApplyEnvironmentModification();

    uint32_t port = RenderDoc_FirstTargetControlPort;

    Network::Socket *sock = Network::CreateServerSocket("0.0.0.0", port & 0xffff, 4);

    while(sock == NULL)
    {
      port++;
      if(port > RenderDoc_LastTargetControlPort)
      {
        m_RemoteIdent = 0;
        break;
      }

      sock = Network::CreateServerSocket("0.0.0.0", port & 0xffff, 4);
    }

    if(sock)
    {
      m_RemoteIdent = port;

      m_TargetControlThreadShutdown = false;
      m_RemoteThread = Threading::CreateThread([sock]() { TargetControlServerThread(sock); });

      RDCLOG("Listening for target control on %u", port);
    }
    else
    {
      RDCWARN("Couldn't open socket for target control");
    }
  }

  // set default capture log - useful for when hooks aren't setup
  // through the UI (and a log file isn't set manually)
  {
    std::string capture_filename;

    const char *base = "RenderDoc_app";
    if(IsReplayApp())
      base = "RenderDoc";

    FileIO::GetDefaultFiles(base, capture_filename, m_LoggingFilename, m_Target);

    if(m_CaptureFileTemplate.empty())
      SetCaptureFileTemplate(capture_filename.c_str());

    RDCLOGFILE(m_LoggingFilename.c_str());
  }

  const char *platform =
#if ENABLED(RDOC_WIN32)
      "Windows";
#elif ENABLED(RDOC_LINUX)
      "Linux";
#elif ENABLED(RDOC_ANDROID)
      "Android";
#elif ENABLED(RDOC_APPLE)
      "macOS";
#elif ENABLED(RDOC_GGP)
      "GGP";
#else
      "Unknown";
#endif

  RDCLOG("RenderDoc v%s %s %s %s (%s) %s", MAJOR_MINOR_VERSION_STRING, platform,
         sizeof(uintptr_t) == sizeof(uint64_t) ? "64-bit" : "32-bit",
         ENABLED(RDOC_RELEASE) ? "Release" : "Development", GitVersionHash,
         IsReplayApp() ? "loaded in replay application" : "capturing application");

#if defined(DISTRIBUTION_VERSION)
  RDCLOG("Packaged for %s (%s) - %s", DISTRIBUTION_NAME, DISTRIBUTION_VERSION, DISTRIBUTION_CONTACT);
#endif

  Keyboard::Init();

  m_FrameTimer.InitTimers();

  m_ExHandler = NULL;

  {
    std::string curFile;
    FileIO::GetExecutableFilename(curFile);

    std::string f = strlower(curFile);

    // only create crash handler when we're not in renderdoccmd.exe (to prevent infinite loop as
    // the crash handler itself launches renderdoccmd.exe)
    if(f.find("renderdoccmd.exe") == std::string::npos)
    {
      RecreateCrashHandler();
    }
  }

  // begin printing to stdout/stderr after this point, earlier logging is debugging
  // cruft that we don't want cluttering output.
  // However we don't want to print in captured applications, since they may be outputting important
  // information to stdout/stderr and being piped around and processed!
  if(IsReplayApp())
    RDCLOGOUTPUT();
}

RenderDoc::~RenderDoc()
{
  if(m_ExHandler)
  {
    UnloadCrashHandler();
  }

  for(auto it = m_ShutdownFunctions.begin(); it != m_ShutdownFunctions.end(); ++it)
    (*it)();

  for(size_t i = 0; i < m_Captures.size(); i++)
  {
    if(m_Captures[i].retrieved)
    {
      RDCLOG("Removing remotely retrieved capture %s", m_Captures[i].path.c_str());
      FileIO::Delete(m_Captures[i].path.c_str());
    }
    else
    {
      RDCLOG("'Leaking' unretrieved capture %s", m_Captures[i].path.c_str());
    }
  }

  RDCSTOPLOGGING(m_LoggingFilename.c_str());

  if(m_RemoteThread)
  {
    m_TargetControlThreadShutdown = true;
    // On windows we can't join to this thread as it could lead to deadlocks, since we're
    // performing this destructor in the middle of module unloading. However we want to
    // ensure that the thread gets properly tidied up and closes its socket, so wait a little
    // while to give it time to notice the shutdown signal and close itself.
    Threading::Sleep(50);
    Threading::CloseThread(m_RemoteThread);
    m_RemoteThread = 0;
  }

  Process::Shutdown();

  Network::Shutdown();

  Threading::Shutdown();

  StringFormat::Shutdown();
}

void RenderDoc::Shutdown()
{
  if(m_ExHandler)
  {
    UnloadCrashHandler();
  }

  if(m_RemoteThread)
  {
    // explicitly wait for thread to shutdown, this call is not from module unloading and
    // we want to be sure everything is gone before we remove our module & hooks
    m_TargetControlThreadShutdown = true;
    Threading::JoinThread(m_RemoteThread);
    Threading::CloseThread(m_RemoteThread);
    m_RemoteThread = 0;
  }
}

void RenderDoc::ProcessGlobalEnvironment(GlobalEnvironment env, const std::vector<std::string> &args)
{
  m_GlobalEnv = env;

#if ENABLED(RDOC_LINUX) && ENABLED(RDOC_XLIB)
  if(!m_GlobalEnv.xlibDisplay)
    m_GlobalEnv.xlibDisplay = XOpenDisplay(NULL);
#endif

  if(!args.empty())
  {
    RDCDEBUG("Replay application launched with parameters:");
    for(size_t i = 0; i < args.size(); i++)
      RDCDEBUG("[%u]: %s", (uint32_t)i, args[i].c_str());
  }
}

bool RenderDoc::MatchClosestWindow(void *&dev, void *&wnd)
{
  DeviceWnd dw(dev, wnd);

  // lower_bound and the DeviceWnd ordering (pointer compares, dev over wnd) means that if either
  // element in dw is NULL we can go forward from this iterator and find the first wildcardMatch
  // note that if dev is specified and wnd is NULL, this will actually point at the first
  // wildcardMatch already and we can use it immediately (since which window of multiple we
  // choose is undefined, so up to us). If dev is NULL there is no window ordering (since dev is
  // the primary sorting value) so we just iterate through the whole map. It should be small in
  // the majority of cases
  auto it = m_WindowFrameCapturers.lower_bound(dw);

  while(it != m_WindowFrameCapturers.end())
  {
    if(it->first.wildcardMatch(dw))
      break;
    ++it;
  }

  if(it != m_WindowFrameCapturers.end())
  {
    dev = it->first.dev;
    wnd = it->first.wnd;
    return true;
  }

  return false;
}

IFrameCapturer *RenderDoc::MatchFrameCapturer(void *dev, void *wnd)
{
  DeviceWnd dw(dev, wnd);

  // try and find the closest frame capture registered, and update
  // the values in dw to point to it precisely
  bool exactMatch = MatchClosestWindow(dw.dev, dw.wnd);

  if(!exactMatch)
  {
    // handle off-screen rendering where there are no device/window pairs in
    // m_WindowFrameCapturers, instead we use the first matching device frame capturer
    if(wnd == NULL)
    {
      auto defaultit = m_DeviceFrameCapturers.find(dev);
      if(defaultit == m_DeviceFrameCapturers.end() && !m_DeviceFrameCapturers.empty())
        defaultit = m_DeviceFrameCapturers.begin();

      if(defaultit != m_DeviceFrameCapturers.end())
        return defaultit->second;
    }

    RDCERR("Couldn't find matching frame capturer for device %p window %p", dev, wnd);
    return NULL;
  }

  auto it = m_WindowFrameCapturers.find(dw);

  if(it == m_WindowFrameCapturers.end())
  {
    RDCERR("Couldn't find frame capturer after exact match!");
    return NULL;
  }

  return it->second.FrameCapturer;
}

void RenderDoc::StartFrameCapture(void *dev, void *wnd)
{
  IFrameCapturer *frameCap = MatchFrameCapturer(dev, wnd);
  if(frameCap)
  {
    frameCap->StartFrameCapture(dev, wnd);
    m_CapturesActive++;
  }
}

void RenderDoc::SetActiveWindow(void *dev, void *wnd)
{
  DeviceWnd dw(dev, wnd);

  auto it = m_WindowFrameCapturers.find(dw);
  if(it == m_WindowFrameCapturers.end())
  {
    RDCERR("Couldn't find frame capturer for device %p window %p", dev, wnd);
    return;
  }

  m_ActiveWindow = dw;
}

bool RenderDoc::EndFrameCapture(void *dev, void *wnd)
{
  IFrameCapturer *frameCap = MatchFrameCapturer(dev, wnd);
  if(frameCap)
  {
    bool ret = frameCap->EndFrameCapture(dev, wnd);
    m_CapturesActive--;
    return ret;
  }
  return false;
}

bool RenderDoc::DiscardFrameCapture(void *dev, void *wnd)
{
  IFrameCapturer *frameCap = MatchFrameCapturer(dev, wnd);
  if(frameCap)
  {
    bool ret = frameCap->DiscardFrameCapture(dev, wnd);
    m_CapturesActive--;
    return ret;
  }
  return false;
}

bool RenderDoc::IsTargetControlConnected()
{
  SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
  return !RenderDoc::Inst().m_SingleClientName.empty();
}

std::string RenderDoc::GetTargetControlUsername()
{
  SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
  return RenderDoc::Inst().m_SingleClientName;
}

void RenderDoc::Tick()
{
  static bool prev_focus = false;
  static bool prev_cap = false;

  bool cur_focus = false;
  for(size_t i = 0; i < m_FocusKeys.size(); i++)
    cur_focus |= Keyboard::GetKeyState(m_FocusKeys[i]);

  bool cur_cap = false;
  for(size_t i = 0; i < m_CaptureKeys.size(); i++)
    cur_cap |= Keyboard::GetKeyState(m_CaptureKeys[i]);

  m_FrameTimer.UpdateTimers();

  if(!prev_focus && cur_focus)
  {
    CycleActiveWindow();
  }
  if(!prev_cap && cur_cap)
  {
    TriggerCapture(1);
  }

  prev_focus = cur_focus;
  prev_cap = cur_cap;
}

void RenderDoc::CycleActiveWindow()
{
  m_Cap = 0;

  // can only shift focus if we have multiple windows
  if(m_WindowFrameCapturers.size() > 1)
  {
    for(auto it = m_WindowFrameCapturers.begin(); it != m_WindowFrameCapturers.end(); ++it)
    {
      if(it->first == m_ActiveWindow)
      {
        auto nextit = it;
        ++nextit;

        if(nextit != m_WindowFrameCapturers.end())
          m_ActiveWindow = nextit->first;
        else
          m_ActiveWindow = m_WindowFrameCapturers.begin()->first;

        break;
      }
    }
  }
}

std::string RenderDoc::GetOverlayText(RDCDriver driver, uint32_t frameNumber, int flags)
{
  const bool activeWindow = (flags & eOverlay_ActiveWindow);
  const bool capturesEnabled = (flags & eOverlay_CaptureDisabled) == 0;

  uint32_t overlay = GetOverlayBits();

  std::string overlayText = ToStr(driver) + ". ";

  if(activeWindow)
  {
    std::vector<RENDERDOC_InputButton> keys = GetCaptureKeys();

    if(capturesEnabled)
    {
      if(Keyboard::PlatformHasKeyInput())
      {
        for(size_t i = 0; i < keys.size(); i++)
        {
          if(i > 0)
            overlayText += ", ";

          overlayText += ToStr(keys[i]);
        }

        if(!keys.empty())
          overlayText += " to capture.";
      }
      else
      {
        if(IsTargetControlConnected())
          overlayText += "Connected by " + GetTargetControlUsername() + ".";
        else
          overlayText += "No remote access connection.";
      }
    }

    if(overlay & eRENDERDOC_Overlay_FrameNumber)
    {
      overlayText += StringFormat::Fmt(" Frame: %d.", frameNumber);
    }
    if(overlay & eRENDERDOC_Overlay_FrameRate)
    {
      overlayText +=
          StringFormat::Fmt(" %.2lf ms (%.2lf .. %.2lf) (%.0lf FPS)", m_FrameTimer.GetAvgFrameTime(),
                            m_FrameTimer.GetMinFrameTime(), m_FrameTimer.GetMaxFrameTime(),
                            // max with 0.01ms so that we don't divide by zero
                            1000.0f / RDCMAX(0.01, m_FrameTimer.GetAvgFrameTime()));
    }

    overlayText += "\n";

    if((overlay & eRENDERDOC_Overlay_CaptureList) && capturesEnabled)
    {
      overlayText += StringFormat::Fmt("%d Captures saved.\n", (uint32_t)m_Captures.size());

      uint64_t now = Timing::GetUnixTimestamp();
      for(size_t i = 0; i < m_Captures.size(); i++)
      {
        if(now - m_Captures[i].timestamp < 20)
        {
          overlayText += StringFormat::Fmt("Captured frame %d.\n", m_Captures[i].frameNumber);
        }
      }
    }

#if ENABLED(RDOC_DEVEL)
    overlayText += StringFormat::Fmt("%llu chunks - %.2f MB\n", Chunk::NumLiveChunks(),
                                     float(Chunk::TotalMem()) / 1024.0f / 1024.0f);
#endif
  }
  else if(capturesEnabled)
  {
    std::vector<RENDERDOC_InputButton> keys = GetFocusKeys();

    overlayText += "Inactive window.";

    for(size_t i = 0; i < keys.size(); i++)
    {
      if(i == 0)
        overlayText += " ";
      else
        overlayText += ", ";

      overlayText += ToStr(keys[i]);
    }

    if(!keys.empty())
      overlayText += " to cycle between windows";

    overlayText += "\n";
  }

  return overlayText;
}

bool RenderDoc::ShouldTriggerCapture(uint32_t frameNumber)
{
  bool ret = m_Cap > 0;

  if(m_Cap > 0)
    m_Cap--;

  std::set<uint32_t> frames;
  frames.swap(m_QueuedFrameCaptures);
  for(auto it = frames.begin(); it != frames.end(); ++it)
  {
    if(*it < frameNumber)
    {
      // discard, this frame is past.
    }
    else if((*it) == frameNumber)
    {
      // we want to capture the next frame
      ret = true;
    }
    else
    {
      // not hit this yet, keep it around
      m_QueuedFrameCaptures.insert(*it);
    }
  }

  return ret;
}

void RenderDoc::ResamplePixels(const FramePixels &in, RDCThumb &out)
{
  // code below assumes pitch_requirement is a power of 2 number
  RDCASSERT((in.pitch_requirement & (in.pitch_requirement - 1)) == 0);

  out.width = (uint16_t)RDCMIN(in.max_width, in.width);
  out.width &= ~(in.pitch_requirement - 1);    // align down to multiple of in.
  out.height = uint16_t(out.width * in.height / in.width);
  out.len = 3 * out.width * out.height;
  out.pixels = new byte[out.len];
  out.format = FileType::Raw;

  byte *dst = (byte *)out.pixels;
  byte *source = (byte *)in.data;

  for(uint32_t y = 0; y < out.height; y++)
  {
    for(uint32_t x = 0; x < out.width; x++)
    {
      uint32_t xSource = x * in.width / out.width;
      uint32_t ySource = y * in.height / out.height;
      byte *src = &source[in.stride * xSource + in.pitch * ySource];

      if(in.buf1010102)
      {
        uint32_t *src1010102 = (uint32_t *)src;
        Vec4f unorm = ConvertFromR10G10B10A2(*src1010102);
        dst[0] = (byte)(unorm.x * 255.0f);
        dst[1] = (byte)(unorm.y * 255.0f);
        dst[2] = (byte)(unorm.z * 255.0f);
      }
      else if(in.buf565)
      {
        uint16_t *src565 = (uint16_t *)src;
        Vec3f unorm = ConvertFromB5G6R5(*src565);
        dst[0] = (byte)(unorm.z * 255.0f);
        dst[1] = (byte)(unorm.y * 255.0f);
        dst[2] = (byte)(unorm.x * 255.0f);
      }
      else if(in.buf5551)
      {
        uint16_t *src5551 = (uint16_t *)src;
        Vec4f unorm = ConvertFromB5G5R5A1(*src5551);
        dst[0] = (byte)(unorm.z * 255.0f);
        dst[1] = (byte)(unorm.y * 255.0f);
        dst[2] = (byte)(unorm.x * 255.0f);
      }
      else if(in.bgra)
      {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
      }
      else if(in.bpc == 2)    // R16G16B16A16 backbuffer
      {
        uint16_t *src16 = (uint16_t *)src;

        float linearR = RDCCLAMP(ConvertFromHalf(src16[0]), 0.0f, 1.0f);
        float linearG = RDCCLAMP(ConvertFromHalf(src16[1]), 0.0f, 1.0f);
        float linearB = RDCCLAMP(ConvertFromHalf(src16[2]), 0.0f, 1.0f);

        if(linearR < 0.0031308f)
          dst[0] = byte(255.0f * (12.92f * linearR));
        else
          dst[0] = byte(255.0f * (1.055f * powf(linearR, 1.0f / 2.4f) - 0.055f));

        if(linearG < 0.0031308f)
          dst[1] = byte(255.0f * (12.92f * linearG));
        else
          dst[1] = byte(255.0f * (1.055f * powf(linearG, 1.0f / 2.4f) - 0.055f));

        if(linearB < 0.0031308f)
          dst[2] = byte(255.0f * (12.92f * linearB));
        else
          dst[2] = byte(255.0f * (1.055f * powf(linearB, 1.0f / 2.4f) - 0.055f));
      }
      else
      {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
      }
      dst += 3;
    }
  }

  if(!in.is_y_flipped)
  {
    for(uint16_t y = 0; y <= out.height / 2; y++)
    {
      uint16_t flipY = (out.height - 1 - y);
      for(uint16_t x = 0; x < out.width; x++)
      {
        byte *src = (byte *)out.pixels;
        byte save[3];
        save[0] = src[(y * out.width + x) * 3 + 0];
        save[1] = src[(y * out.width + x) * 3 + 1];
        save[2] = src[(y * out.width + x) * 3 + 2];

        src[(y * out.width + x) * 3 + 0] = src[(flipY * out.width + x) * 3 + 0];
        src[(y * out.width + x) * 3 + 1] = src[(flipY * out.width + x) * 3 + 1];
        src[(y * out.width + x) * 3 + 2] = src[(flipY * out.width + x) * 3 + 2];

        src[(flipY * out.width + x) * 3 + 0] = save[0];
        src[(flipY * out.width + x) * 3 + 1] = save[1];
        src[(flipY * out.width + x) * 3 + 2] = save[2];
      }
    }
  }
}
void RenderDoc::EncodePixelsPNG(const RDCThumb &in, RDCThumb &out)
{
  struct WriteCallbackData
  {
    std::vector<byte> buffer;

    static void writeData(void *context, void *data, int size)
    {
      WriteCallbackData *pThis = (WriteCallbackData *)context;
      const byte *start = (const byte *)data;
      pThis->buffer.insert(pThis->buffer.end(), start, start + size);
    }
  };

  WriteCallbackData callbackData;
  stbi_write_png_to_func(&WriteCallbackData::writeData, &callbackData, in.width, in.height, 3,
                         in.pixels, 0);
  out.width = in.width;
  out.height = in.height;
  out.pixels = new byte[callbackData.buffer.size()];
  memcpy((void *)out.pixels, callbackData.buffer.data(), callbackData.buffer.size());
  out.len = (uint32_t)callbackData.buffer.size();
  out.format = FileType::PNG;
}

RDCFile *RenderDoc::CreateRDC(RDCDriver driver, uint32_t frameNum, const FramePixels &fp)
{
  RDCFile *ret = new RDCFile;

  m_CurrentLogFile = StringFormat::Fmt("%s_frame%u.rdc", m_CaptureFileTemplate.c_str(), frameNum);

  // make sure we don't stomp another capture if we make multiple captures in the same frame.
  {
    SCOPED_LOCK(m_CaptureLock);
    int altnum = 2;
    while(std::find_if(m_Captures.begin(), m_Captures.end(), [this](const CaptureData &o) {
            return o.path == m_CurrentLogFile;
          }) != m_Captures.end())
    {
      m_CurrentLogFile =
          StringFormat::Fmt("%s_frame%u_%d.rdc", m_CaptureFileTemplate.c_str(), frameNum, altnum);
      altnum++;
    }
  }

  RDCThumb outRaw, outPng;
  if(fp.data)
  {
    // point sample info into raw buffer
    ResamplePixels(fp, outRaw);
    EncodePixelsPNG(outRaw, outPng);
  }

  ret->SetData(driver, ToStr(driver).c_str(), OSUtility::GetMachineIdent(), &outPng);

  FileIO::CreateParentDirectory(m_CurrentLogFile);

  ret->Create(m_CurrentLogFile.c_str());

  if(ret->ErrorCode() != ContainerError::NoError)
  {
    RDCERR("Error creating RDC at '%s'", m_CurrentLogFile.c_str());
    SAFE_DELETE(ret);
  }

  SAFE_DELETE_ARRAY(outRaw.pixels);
  SAFE_DELETE_ARRAY(outPng.pixels);

  return ret;
}

bool RenderDoc::HasReplayDriver(RDCDriver driver) const
{
  // Image driver is handled specially and isn't registered in the map
  if(driver == RDCDriver::Image)
    return true;

  return m_ReplayDriverProviders.find(driver) != m_ReplayDriverProviders.end();
}

bool RenderDoc::HasRemoteDriver(RDCDriver driver) const
{
  if(m_RemoteDriverProviders.find(driver) != m_RemoteDriverProviders.end())
    return true;

  return HasReplayDriver(driver);
}

void RenderDoc::RegisterReplayProvider(RDCDriver driver, ReplayDriverProvider provider)
{
  if(HasReplayDriver(driver))
    RDCERR("Re-registering provider for %s", ToStr(driver).c_str());
  if(HasRemoteDriver(driver))
    RDCWARN("Registering local provider for existing remote provider %s", ToStr(driver).c_str());

  m_ReplayDriverProviders[driver] = provider;
}

void RenderDoc::RegisterRemoteProvider(RDCDriver driver, RemoteDriverProvider provider)
{
  if(HasRemoteDriver(driver))
    RDCERR("Re-registering provider for %s", ToStr(driver).c_str());
  if(HasReplayDriver(driver))
    RDCWARN("Registering remote provider for existing local provider %s", ToStr(driver).c_str());

  m_RemoteDriverProviders[driver] = provider;
}

void RenderDoc::RegisterStructuredProcessor(RDCDriver driver, StructuredProcessor provider)
{
  RDCASSERT(m_StructProcesssors.find(driver) == m_StructProcesssors.end());

  m_StructProcesssors[driver] = provider;
}

void RenderDoc::RegisterCaptureExporter(CaptureExporter exporter, CaptureFileFormat description)
{
  std::string filetype = description.extension;

  for(const CaptureFileFormat &fmt : m_ImportExportFormats)
  {
    if(fmt.extension == filetype)
    {
      RDCERR("Duplicate exporter for '%s' found", filetype.c_str());
      return;
    }
  }

  description.openSupported = false;
  description.convertSupported = true;

  m_ImportExportFormats.push_back(description);

  m_Exporters[filetype] = exporter;
}

void RenderDoc::RegisterCaptureImportExporter(CaptureImporter importer, CaptureExporter exporter,
                                              CaptureFileFormat description)
{
  std::string filetype = description.extension;

  for(const CaptureFileFormat &fmt : m_ImportExportFormats)
  {
    if(fmt.extension == filetype)
    {
      RDCERR("Duplicate import/exporter for '%s' found", filetype.c_str());
      return;
    }
  }

  description.openSupported = true;
  description.convertSupported = true;

  m_ImportExportFormats.push_back(description);

  m_Importers[filetype] = importer;
  m_Exporters[filetype] = exporter;
}

StructuredProcessor RenderDoc::GetStructuredProcessor(RDCDriver driver)
{
  auto it = m_StructProcesssors.find(driver);

  if(it == m_StructProcesssors.end())
    return NULL;

  return it->second;
}

CaptureExporter RenderDoc::GetCaptureExporter(const char *filetype)
{
  if(!filetype)
    return NULL;

  auto it = m_Exporters.find(filetype);

  if(it == m_Exporters.end())
    return NULL;

  return it->second;
}

CaptureImporter RenderDoc::GetCaptureImporter(const char *filetype)
{
  if(!filetype)
    return NULL;

  auto it = m_Importers.find(filetype);

  if(it == m_Importers.end())
    return NULL;

  return it->second;
}

std::vector<CaptureFileFormat> RenderDoc::GetCaptureFileFormats()
{
  std::vector<CaptureFileFormat> ret = m_ImportExportFormats;

  std::sort(ret.begin(), ret.end());

  {
    CaptureFileFormat rdc;
    rdc.extension = "rdc";
    rdc.name = "Native RDC capture file format.";
    rdc.description = "The format produced by frame-captures from applications directly.";
    rdc.openSupported = true;
    rdc.convertSupported = true;

    ret.insert(ret.begin(), rdc);
  }

  return ret;
}

bool RenderDoc::HasReplaySupport(RDCDriver driverType)
{
  if(driverType == RDCDriver::Image)
    return true;

  if(driverType == RDCDriver::Unknown && !m_ReplayDriverProviders.empty())
    return true;

  return m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end();
}

ReplayStatus RenderDoc::CreateProxyReplayDriver(RDCDriver proxyDriver, IReplayDriver **driver)
{
  // passing RDCDriver::Unknown means 'I don't care, give me a proxy driver of any type'
  if(proxyDriver == RDCDriver::Unknown)
  {
    if(!m_ReplayDriverProviders.empty())
      return m_ReplayDriverProviders.begin()->second(NULL, driver);
  }

  if(m_ReplayDriverProviders.find(proxyDriver) != m_ReplayDriverProviders.end())
    return m_ReplayDriverProviders[proxyDriver](NULL, driver);

  RDCERR("Unsupported replay driver requested: %s", ToStr(proxyDriver).c_str());
  return ReplayStatus::APIUnsupported;
}

ReplayStatus RenderDoc::CreateReplayDriver(RDCFile *rdc, IReplayDriver **driver)
{
  if(driver == NULL)
    return ReplayStatus::InternalError;

  // allows passing NULL rdcfile as 'I don't care, give me a proxy driver of any type'
  if(rdc == NULL)
  {
    if(!m_ReplayDriverProviders.empty())
      return m_ReplayDriverProviders.begin()->second(NULL, driver);

    RDCERR("Request for proxy replay device, but no replay providers are available.");
    return ReplayStatus::InternalError;
  }

  RDCDriver driverType = rdc->GetDriver();

  // image support is special, handle it here
  if(driverType == RDCDriver::Image)
    return IMG_CreateReplayDevice(rdc, driver);

  if(m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end())
    return m_ReplayDriverProviders[driverType](rdc, driver);

  RDCERR("Unsupported replay driver requested: %s", ToStr(driverType).c_str());
  return ReplayStatus::APIUnsupported;
}

ReplayStatus RenderDoc::CreateRemoteDriver(RDCFile *rdc, IRemoteDriver **driver)
{
  if(rdc == NULL || driver == NULL)
    return ReplayStatus::InternalError;

  RDCDriver driverType = rdc->GetDriver();

  if(m_RemoteDriverProviders.find(driverType) != m_RemoteDriverProviders.end())
    return m_RemoteDriverProviders[driverType](rdc, driver);

  // replay drivers are remote drivers, fall back and try them
  if(m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end())
  {
    IReplayDriver *dr = NULL;
    ReplayStatus status = m_ReplayDriverProviders[driverType](rdc, &dr);

    if(status == ReplayStatus::Succeeded)
      *driver = (IRemoteDriver *)dr;
    else
      RDCASSERT(dr == NULL);

    return status;
  }

  RDCERR("Unsupported replay driver requested: %s", ToStr(driverType).c_str());
  return ReplayStatus::APIUnsupported;
}

void RenderDoc::AddActiveDriver(RDCDriver driver, bool present)
{
  if(driver == RDCDriver::Unknown)
    return;

  uint64_t timestamp = present ? Timing::GetUnixTimestamp() : 0;

  {
    SCOPED_LOCK(m_DriverLock);

    uint64_t &active = m_ActiveDrivers[driver];
    active = RDCMAX(active, timestamp);
  }
}

std::map<RDCDriver, bool> RenderDoc::GetActiveDrivers()
{
  std::map<RDCDriver, uint64_t> drivers;

  {
    SCOPED_LOCK(m_DriverLock);
    drivers = m_ActiveDrivers;
  }

  std::map<RDCDriver, bool> ret;

  for(auto it = drivers.begin(); it != drivers.end(); ++it)
  {
    // driver is presenting if the timestamp is greater than 0 and less than 10 seconds ago (gives a
    // little leeway for loading screens or something where the presentation stops temporarily).
    // we also assume that during a capture if it was presenting, then it's still capturing.
    // Otherwise a long capture would temporarily set it as not presenting.
    bool presenting = it->second > 0;

    if(presenting && !IsFrameCapturing() && it->second < Timing::GetUnixTimestamp() - 10)
      presenting = false;

    ret[it->first] = presenting;
  }

  return ret;
}

std::map<RDCDriver, std::string> RenderDoc::GetReplayDrivers()
{
  std::map<RDCDriver, std::string> ret;
  for(auto it = m_ReplayDriverProviders.begin(); it != m_ReplayDriverProviders.end(); ++it)
    ret[it->first] = ToStr(it->first);
  return ret;
}

std::map<RDCDriver, std::string> RenderDoc::GetRemoteDrivers()
{
  std::map<RDCDriver, std::string> ret;

  for(auto it = m_RemoteDriverProviders.begin(); it != m_RemoteDriverProviders.end(); ++it)
    ret[it->first] = ToStr(it->first);

  // replay drivers are remote drivers.
  for(auto it = m_ReplayDriverProviders.begin(); it != m_ReplayDriverProviders.end(); ++it)
    ret[it->first] = ToStr(it->first);

  return ret;
}

DriverInformation RenderDoc::GetDriverInformation(GraphicsAPI api)
{
  DriverInformation ret = {};

  RDCDriver driverType = RDCDriver::Unknown;
  switch(api)
  {
    case GraphicsAPI::D3D11: driverType = RDCDriver::D3D11; break;
    case GraphicsAPI::D3D12: driverType = RDCDriver::D3D12; break;
    case GraphicsAPI::OpenGL: driverType = RDCDriver::OpenGL; break;
    case GraphicsAPI::Vulkan: driverType = RDCDriver::Vulkan; break;
  }

  if(driverType == RDCDriver::Unknown || !HasReplayDriver(driverType))
    return ret;

  IReplayDriver *driver = NULL;
  ReplayStatus status = CreateProxyReplayDriver(driverType, &driver);

  if(status == ReplayStatus::Succeeded)
  {
    ret = driver->GetDriverInfo();
  }
  else
  {
    RDCERR("Couldn't create proxy replay driver for %s: %s", ToStr(driverType).c_str(),
           ToStr(status).c_str());
  }

  if(driver)
    driver->Shutdown();

  return ret;
}

void RenderDoc::EnableVendorExtensions(VendorExtensions ext)
{
  m_VendorExts[(int)ext] = true;

  RDCWARN("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  RDCWARN("!!! Vendor Extension enabled: %s", ToStr(ext).c_str());
  RDCWARN("!!! ");
  RDCWARN("!!! This can cause crashes, incorrect replay, or other problems and");
  RDCWARN("!!! is explicitly unsupported. Do not enable without understanding.");
  RDCWARN("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

void RenderDoc::SetCaptureOptions(const CaptureOptions &opts)
{
  m_Options = opts;

  LibraryHooks::OptionsUpdated();
}

void RenderDoc::SetCaptureFileTemplate(const char *pathtemplate)
{
  if(pathtemplate == NULL || pathtemplate[0] == '\0')
    return;

  m_CaptureFileTemplate = pathtemplate;

  if(m_CaptureFileTemplate.length() > 4 &&
     m_CaptureFileTemplate.substr(m_CaptureFileTemplate.length() - 4) == ".rdc")
    m_CaptureFileTemplate = m_CaptureFileTemplate.substr(0, m_CaptureFileTemplate.length() - 4);

  FileIO::CreateParentDirectory(m_CaptureFileTemplate);
}

void RenderDoc::FinishCaptureWriting(RDCFile *rdc, uint32_t frameNumber)
{
  RenderDoc::Inst().SetProgress(CaptureProgress::FileWriting, 0.0f);

  if(rdc)
  {
    // add the resolve database if we were capturing callstacks.
    if(m_Options.captureCallstacks)
    {
      SectionProperties props = {};
      props.type = SectionType::ResolveDatabase;
      props.version = 1;
      StreamWriter *w = rdc->WriteSection(props);

      size_t sz = 0;
      Callstack::GetLoadedModules(NULL, sz);

      byte *buf = new byte[sz];
      Callstack::GetLoadedModules(buf, sz);

      w->Write(buf, sz);

      w->Finish();

      delete w;
    }

    const RDCThumb &thumb = rdc->GetThumbnail();
    if(thumb.format != FileType::JPG && thumb.width > 0 && thumb.height > 0)
    {
      SectionProperties props = {};
      props.type = SectionType::ExtendedThumbnail;
      props.version = 1;
      StreamWriter *w = rdc->WriteSection(props);

      // if this file format ever changes, be sure to update the XML export which has a special
      // handling for this case.

      ExtThumbnailHeader header;
      header.width = thumb.width;
      header.height = thumb.height;
      header.len = thumb.len;
      header.format = thumb.format;
      w->Write(header);
      w->Write(thumb.pixels, thumb.len);

      w->Finish();

      delete w;
    }

    RDCLOG("Written to disk: %s", m_CurrentLogFile.c_str());

    CaptureData cap(m_CurrentLogFile, Timing::GetUnixTimestamp(), rdc->GetDriver(), frameNumber);
    {
      SCOPED_LOCK(m_CaptureLock);
      m_Captures.push_back(cap);
    }

    delete rdc;
  }
  else
  {
    RDCLOG("Discarded capture, Frame %u", frameNumber);
  }

  RenderDoc::Inst().SetProgress(CaptureProgress::FileWriting, 1.0f);
}

void RenderDoc::AddDeviceFrameCapturer(void *dev, IFrameCapturer *cap)
{
  if(dev == NULL || cap == NULL)
  {
    RDCERR("Invalid FrameCapturer combination: %#p / %#p", dev, cap);
    return;
  }

  m_DeviceFrameCapturers[dev] = cap;
}

void RenderDoc::RemoveDeviceFrameCapturer(void *dev)
{
  if(dev == NULL)
  {
    RDCERR("Invalid device pointer: %#p", dev);
    return;
  }

  m_DeviceFrameCapturers.erase(dev);
}

void RenderDoc::AddFrameCapturer(void *dev, void *wnd, IFrameCapturer *cap)
{
  if(dev == NULL || wnd == NULL || cap == NULL)
  {
    RDCERR("Invalid FrameCapturer combination: %#p / %#p", wnd, cap);
    return;
  }

  DeviceWnd dw(dev, wnd);

  auto it = m_WindowFrameCapturers.find(dw);
  if(it != m_WindowFrameCapturers.end())
  {
    if(it->second.FrameCapturer != cap)
      RDCERR("New different FrameCapturer being registered for known device/window pair!");

    it->second.RefCount++;
  }
  else
  {
    m_WindowFrameCapturers[dw].FrameCapturer = cap;
  }

  // the first one we see becomes the default
  if(m_ActiveWindow == DeviceWnd())
    m_ActiveWindow = dw;
}

void RenderDoc::RemoveFrameCapturer(void *dev, void *wnd)
{
  DeviceWnd dw(dev, wnd);

  auto it = m_WindowFrameCapturers.find(dw);
  if(it != m_WindowFrameCapturers.end())
  {
    it->second.RefCount--;

    if(it->second.RefCount <= 0)
    {
      if(m_ActiveWindow == dw)
      {
        if(m_WindowFrameCapturers.size() == 1)
        {
          m_ActiveWindow = DeviceWnd();
        }
        else
        {
          auto newactive = m_WindowFrameCapturers.begin();
          // active window could be the first in our list, move
          // to second (we know from above there are at least 2)
          if(m_ActiveWindow == newactive->first)
            newactive++;
          m_ActiveWindow = newactive->first;
        }
      }

      m_WindowFrameCapturers.erase(it);
    }
  }
  else
  {
    RDCERR("Removing FrameCapturer for unknown window!");
  }
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Check ResourceId tostr", "[tostr]")
{
  union
  {
    ResourceId *id;
    uint64_t *num;
  } u;

  uint64_t data = 0;
  u.num = &data;

  *u.num = 0;
  CHECK(ToStr(*u.id) == "ResourceId::0");

  *u.num = 1;
  CHECK(ToStr(*u.id) == "ResourceId::1");

  *u.num = 7;
  CHECK(ToStr(*u.id) == "ResourceId::7");

  *u.num = 17;
  CHECK(ToStr(*u.id) == "ResourceId::17");

  *u.num = 32;
  CHECK(ToStr(*u.id) == "ResourceId::32");

  *u.num = 913;
  CHECK(ToStr(*u.id) == "ResourceId::913");

  *u.num = 454;
  CHECK(ToStr(*u.id) == "ResourceId::454");

  *u.num = 123456;
  CHECK(ToStr(*u.id) == "ResourceId::123456");

  *u.num = 1234567;
  CHECK(ToStr(*u.id) == "ResourceId::1234567");

  *u.num = 0x1234567812345678ULL;
  CHECK(ToStr(*u.id) == "ResourceId::1311768465173141112");
}

#endif
