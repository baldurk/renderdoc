/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include "common/dds_readwrite.h"
#include "hooks/hooks.h"
#include "replay/replay_driver.h"
#include "serialise/serialiser.h"
#include "serialise/string_utils.h"
#include "stb/stb_image.h"
#include "crash_handler.h"

#if ENABLED(RDOC_LINUX) && ENABLED(RDOC_XLIB)
#include <X11/Xlib.h>
#endif

// from image_viewer.cpp
ReplayStatus IMG_CreateReplayDevice(const char *logfile, IReplayDriver **driver);

// not provided by tinyexr, just do by hand
bool is_exr_file(FILE *f)
{
  FileIO::fseek64(f, 0, SEEK_SET);

  const uint32_t openexr_magic = MAKE_FOURCC(0x76, 0x2f, 0x31, 0x01);

  uint32_t magic = 0;
  size_t bytesRead = FileIO::fread(&magic, 1, sizeof(magic), f);

  FileIO::fseek64(f, 0, SEEK_SET);

  return bytesRead == sizeof(magic) && magic == openexr_magic;
}

template <>
string ToStrHelper<false, RDCDriver>::Get(const RDCDriver &el)
{
  switch(el)
  {
    case RDC_Unknown: return "Unknown";
    case RDC_OpenGL: return "OpenGL";
    case RDC_OpenGLES: return "OpenGLES";
    case RDC_Mantle: return "Mantle";
    case RDC_D3D12: return "D3D12";
    case RDC_D3D11: return "D3D11";
    case RDC_D3D10: return "D3D10";
    case RDC_D3D9: return "D3D9";
    case RDC_D3D8: return "D3D8";
    case RDC_Image: return "Image";
    case RDC_Vulkan: return "Vulkan";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "RDCDriver<%d>", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, WindowingSystem>::Get(const WindowingSystem &el)
{
  switch(el)
  {
    case WindowingSystem::Unknown: return "Unknown";
    case WindowingSystem::Win32: return "Win32";
    case WindowingSystem::Xlib: return "Xlib";
    case WindowingSystem::XCB: return "XCB";
    case WindowingSystem::Android: return "Android";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "WindowingSystem<%d>", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, ReplayStatus>::Get(const ReplayStatus &el)
{
  switch(el)
  {
    case ReplayStatus::Succeeded: return "Succeeded";
    case ReplayStatus::UnknownError: return "Unknown error";
    case ReplayStatus::InternalError: return "Internal error";
    case ReplayStatus::FileNotFound: return "File not found";
    case ReplayStatus::InjectionFailed: return "RenderDoc injection failed";
    case ReplayStatus::IncompatibleProcess: return "Process is incompatible";
    case ReplayStatus::NetworkIOFailed: return "Network I/O operation failed";
    case ReplayStatus::NetworkRemoteBusy: return "Remote side of network connection is busy";
    case ReplayStatus::NetworkVersionMismatch: return "Version mismatch between network clients";
    case ReplayStatus::FileIOFailed: return "File I/O failed";
    case ReplayStatus::FileIncompatibleVersion: return "File of incompatible version";
    case ReplayStatus::FileCorrupted: return "File corrupted";
    case ReplayStatus::APIUnsupported: return "API unsupported";
    case ReplayStatus::APIInitFailed: return "API initialisation failed";
    case ReplayStatus::APIIncompatibleVersion: return "API incompatible version";
    case ReplayStatus::APIHardwareUnsupported: return "API hardware unsupported";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "StatusCode<%d>", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, RENDERDOC_InputButton>::Get(const RENDERDOC_InputButton &el)
{
  char alphanumericbuf[2] = {'A', 0};

  // enums map straight to ascii
  if((el >= eRENDERDOC_Key_A && el <= eRENDERDOC_Key_Z) ||
     (el >= eRENDERDOC_Key_0 && el <= eRENDERDOC_Key_9))
  {
    alphanumericbuf[0] = (char)el;
    return alphanumericbuf;
  }

  switch(el)
  {
    case eRENDERDOC_Key_Divide: return "/";
    case eRENDERDOC_Key_Multiply: return "*";
    case eRENDERDOC_Key_Subtract: return "-";
    case eRENDERDOC_Key_Plus: return "+";

    case eRENDERDOC_Key_F1: return "F1";
    case eRENDERDOC_Key_F2: return "F2";
    case eRENDERDOC_Key_F3: return "F3";
    case eRENDERDOC_Key_F4: return "F4";
    case eRENDERDOC_Key_F5: return "F5";
    case eRENDERDOC_Key_F6: return "F6";
    case eRENDERDOC_Key_F7: return "F7";
    case eRENDERDOC_Key_F8: return "F8";
    case eRENDERDOC_Key_F9: return "F9";
    case eRENDERDOC_Key_F10: return "F10";
    case eRENDERDOC_Key_F11: return "F11";
    case eRENDERDOC_Key_F12: return "F12";

    case eRENDERDOC_Key_Home: return "Home";
    case eRENDERDOC_Key_End: return "End";
    case eRENDERDOC_Key_Insert: return "Insert";
    case eRENDERDOC_Key_Delete: return "Delete";
    case eRENDERDOC_Key_PageUp: return "PageUp";
    case eRENDERDOC_Key_PageDn: return "PageDn";

    case eRENDERDOC_Key_Backspace: return "Backspace";
    case eRENDERDOC_Key_Tab: return "Tab";
    case eRENDERDOC_Key_PrtScrn: return "PrtScrn";
    case eRENDERDOC_Key_Pause: return "Pause";
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "RENDERDOC_InputButton<%d>", el);

  return tostrBuf;
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
  UnloadCrashHandler();

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
  m_LogFile = "";
  m_MarkerIndentLevel = 0;
  m_CurrentDriver = RDC_Unknown;

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

  m_ProgressPtr = NULL;

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
      m_RemoteThread = Threading::CreateThread(TargetControlServerThread, (void *)sock);

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
    string capture_filename;

    const char *base = "RenderDoc_app";
    if(IsReplayApp())
      base = "RenderDoc";

    FileIO::GetDefaultFiles(base, capture_filename, m_LoggingFilename, m_Target);

    if(m_LogFile.empty())
      SetLogFile(capture_filename.c_str());

    RDCLOGFILE(m_LoggingFilename.c_str());
  }

  RDCLOG("RenderDoc v%s %s %s (%s) %s", MAJOR_MINOR_VERSION_STRING,
         sizeof(uintptr_t) == sizeof(uint64_t) ? "x64" : "x86",
         ENABLED(RDOC_RELEASE) ? "Release" : "Development", GIT_COMMIT_HASH,
         IsReplayApp() ? "loaded in replay application" : "capturing application");

#if defined(DISTRIBUTION_VERSION)
  RDCLOG("Packaged for %s (%s) - %s", DISTRIBUTION_NAME, DISTRIBUTION_VERSION, DISTRIBUTION_CONTACT);
#endif

  Keyboard::Init();

  m_FrameTimer.InitTimers();

  m_ExHandler = NULL;

  {
    string curFile;
    FileIO::GetExecutableFilename(curFile);

    string f = strlower(curFile);

    // only create crash handler when we're not in renderdoccmd.exe (to prevent infinite loop as
    // the crash handler itself launches renderdoccmd.exe)
    if(f.find("renderdoccmd.exe") == string::npos)
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

  Network::Shutdown();

  Threading::Shutdown();
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
    m_CapturesActive--;
    return frameCap->EndFrameCapture(dev, wnd);
  }
  return false;
}

bool RenderDoc::IsTargetControlConnected()
{
  SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
  return !RenderDoc::Inst().m_SingleClientName.empty();
}

string RenderDoc::GetTargetControlUsername()
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
  if(!prev_cap && cur_cap)
  {
    TriggerCapture(1);
  }

  prev_focus = cur_focus;
  prev_cap = cur_cap;
}

string RenderDoc::GetOverlayText(RDCDriver driver, uint32_t frameNumber, int flags)
{
  const bool activeWindow = (flags & eOverlay_ActiveWindow);
  const bool capturesEnabled = (flags & eOverlay_CaptureDisabled) == 0;

  uint32_t overlay = GetOverlayBits();

  string overlayText = ToStr::Get(driver) + ". ";

  if(activeWindow)
  {
    vector<RENDERDOC_InputButton> keys = GetCaptureKeys();

    if(capturesEnabled)
    {
      if(Keyboard::PlatformHasKeyInput())
      {
        for(size_t i = 0; i < keys.size(); i++)
        {
          if(i > 0)
            overlayText += ", ";

          overlayText += ToStr::Get(keys[i]);
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
    vector<RENDERDOC_InputButton> keys = GetFocusKeys();

    overlayText += "Inactive window.";

    for(size_t i = 0; i < keys.size(); i++)
    {
      if(i == 0)
        overlayText += " ";
      else
        overlayText += ", ";

      overlayText += ToStr::Get(keys[i]);
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

  set<uint32_t> frames;
  frames.swap(m_QueuedFrameCaptures);
  for(auto it = frames.begin(); it != frames.end(); ++it)
  {
    if(*it < frameNumber)
    {
      // discard, this frame is past.
    }
    else if((*it) - 1 == frameNumber)
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

Serialiser *RenderDoc::OpenWriteSerialiser(uint32_t frameNum, RDCInitParams *params, void *thpixels,
                                           size_t thlen, uint32_t thwidth, uint32_t thheight)
{
  RDCASSERT(m_CurrentDriver != RDC_Unknown);

#if ENABLED(RDOC_RELEASE)
  const bool debugSerialiser = false;
#else
  const bool debugSerialiser = true;
#endif

  m_CurrentLogFile = StringFormat::Fmt("%s_frame%u.rdc", m_LogFile.c_str(), frameNum);

  // make sure we don't stomp another capture if we make multiple captures in the same frame.
  {
    SCOPED_LOCK(m_CaptureLock);
    int altnum = 2;
    while(std::find_if(m_Captures.begin(), m_Captures.end(), [this](const CaptureData &o) {
            return o.path == m_CurrentLogFile;
          }) != m_Captures.end())
    {
      m_CurrentLogFile = StringFormat::Fmt("%s_frame%u_%d.rdc", m_LogFile.c_str(), frameNum, altnum);
      altnum++;
    }
  }

  Serialiser *fileSerialiser =
      new Serialiser(m_CurrentLogFile.c_str(), Serialiser::WRITING, debugSerialiser);

  Serialiser *chunkSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);

  {
    ScopedContext scope(chunkSerialiser, "Thumbnail", THUMBNAIL_DATA, false);

    bool HasThumbnail = (thpixels != NULL && thwidth > 0 && thheight > 0);
    chunkSerialiser->Serialise("HasThumbnail", HasThumbnail);

    if(HasThumbnail)
    {
      byte *buf = (byte *)thpixels;
      chunkSerialiser->Serialise("ThumbWidth", thwidth);
      chunkSerialiser->Serialise("ThumbHeight", thheight);
      chunkSerialiser->SerialiseBuffer("ThumbnailPixels", buf, thlen);
    }

    fileSerialiser->Insert(scope.Get(true));
  }

  {
    ScopedContext scope(chunkSerialiser, "Capture Create Parameters", CREATE_PARAMS, false);

    chunkSerialiser->Serialise("DriverType", m_CurrentDriver);
    chunkSerialiser->SerialiseString("DriverName", m_CurrentDriverName);

    {
      ScopedContext driverparams(chunkSerialiser, "Driver Specific", DRIVER_INIT_PARAMS, false);

      params->m_pSerialiser = chunkSerialiser;
      params->m_State = WRITING;
      params->Serialise();
    }

    fileSerialiser->Insert(scope.Get(true));
  }

  SAFE_DELETE(chunkSerialiser);

  return fileSerialiser;
}

ReplayStatus RenderDoc::FillInitParams(const char *logFile, RDCDriver &driverType, string &driverName,
                                       uint64_t &fileMachineIdent, RDCInitParams *params)
{
  Serialiser ser(logFile, Serialiser::READING, true);

  if(ser.HasError())
  {
    FILE *f = FileIO::fopen(logFile, "rb");
    if(f)
    {
      int x = 0, y = 0, comp = 0;
      int ret = stbi_info_from_file(f, &x, &y, &comp);

      FileIO::fseek64(f, 0, SEEK_SET);

      if(is_dds_file(f))
        ret = x = y = comp = 1;

      if(is_exr_file(f))
        ret = x = y = comp = 1;

      FileIO::fclose(f);

      if(ret == 1 && x > 0 && y > 0 && comp > 0)
      {
        driverType = RDC_Image;
        driverName = "Image";
        fileMachineIdent = 0;
        return ReplayStatus::Succeeded;
      }
    }

    RDCERR("Couldn't open '%s'", logFile);

    switch(ser.ErrorCode())
    {
      case Serialiser::eSerError_FileIO: return ReplayStatus::FileIOFailed;
      case Serialiser::eSerError_Corrupt: return ReplayStatus::FileCorrupted;
      case Serialiser::eSerError_UnsupportedVersion: return ReplayStatus::FileIncompatibleVersion;
      default: break;
    }

    return ReplayStatus::InternalError;
  }

  ser.Rewind();

  fileMachineIdent = ser.GetSavedMachineIdent();

  {
    int chunkType = ser.PushContext(NULL, NULL, 1, false);

    if(chunkType != THUMBNAIL_DATA)
    {
      RDCERR("Malformed logfile '%s', first chunk isn't thumbnail data", logFile);
      return ReplayStatus::FileCorrupted;
    }

    ser.SkipCurrentChunk();

    ser.PopContext(1);
  }

  {
    int chunkType = ser.PushContext(NULL, NULL, 1, false);

    if(chunkType != CREATE_PARAMS)
    {
      RDCERR("Malformed logfile '%s', second chunk isn't create params", logFile);
      return ReplayStatus::FileCorrupted;
    }

    ser.Serialise("DriverType", driverType);
    ser.SerialiseString("DriverName", driverName);

    chunkType = ser.PushContext(NULL, NULL, 1, false);

    if(chunkType != DRIVER_INIT_PARAMS)
    {
      RDCERR("Malformed logfile '%s', chunk doesn't contain driver init params", logFile);
      return ReplayStatus::FileCorrupted;
    }

    if(params)
    {
      params->m_State = READING;
      params->m_pSerialiser = &ser;
      return params->Serialise();
    }
  }

  // we can just throw away the serialiser, don't need to care about closing/popping contexts
  return ReplayStatus::Succeeded;
}

bool RenderDoc::HasReplayDriver(RDCDriver driver) const
{
  // Image driver is handled specially and isn't registered in the map
  if(driver == RDC_Image)
    return true;

  return m_ReplayDriverProviders.find(driver) != m_ReplayDriverProviders.end();
}

bool RenderDoc::HasRemoteDriver(RDCDriver driver) const
{
  if(m_RemoteDriverProviders.find(driver) != m_RemoteDriverProviders.end())
    return true;

  return HasReplayDriver(driver);
}

void RenderDoc::RegisterReplayProvider(RDCDriver driver, const char *name,
                                       ReplayDriverProvider provider)
{
  if(HasReplayDriver(driver))
    RDCERR("Re-registering provider for %s (was %s)", name, m_DriverNames[driver].c_str());
  if(HasRemoteDriver(driver))
    RDCWARN("Registering local provider %s for existing remote provider %s", name,
            m_DriverNames[driver].c_str());

  m_DriverNames[driver] = name;
  m_ReplayDriverProviders[driver] = provider;
}

void RenderDoc::RegisterRemoteProvider(RDCDriver driver, const char *name,
                                       RemoteDriverProvider provider)
{
  if(HasRemoteDriver(driver))
    RDCERR("Re-registering provider for %s (was %s)", name, m_DriverNames[driver].c_str());
  if(HasReplayDriver(driver))
    RDCWARN("Registering remote provider %s for existing local provider %s", name,
            m_DriverNames[driver].c_str());

  m_DriverNames[driver] = name;
  m_RemoteDriverProviders[driver] = provider;
}

ReplayStatus RenderDoc::CreateReplayDriver(RDCDriver driverType, const char *logfile,
                                           IReplayDriver **driver)
{
  if(driver == NULL)
    return ReplayStatus::InternalError;

  // allows passing RDC_Unknown as 'I don't care, give me a proxy driver of any type'
  // only valid if logfile is NULL and it will be used as a proxy, not to process a log
  if(driverType == RDC_Unknown && logfile == NULL && !m_ReplayDriverProviders.empty())
    return m_ReplayDriverProviders.begin()->second(logfile, driver);

  // image support is special, handle it here
  if(driverType == RDC_Image && logfile != NULL)
    return IMG_CreateReplayDevice(logfile, driver);

  if(m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end())
    return m_ReplayDriverProviders[driverType](logfile, driver);

  RDCERR("Unsupported replay driver requested: %d", driverType);
  return ReplayStatus::APIUnsupported;
}

ReplayStatus RenderDoc::CreateRemoteDriver(RDCDriver driverType, const char *logfile,
                                           IRemoteDriver **driver)
{
  if(driver == NULL)
    return ReplayStatus::InternalError;

  if(m_RemoteDriverProviders.find(driverType) != m_RemoteDriverProviders.end())
    return m_RemoteDriverProviders[driverType](logfile, driver);

  // replay drivers are remote drivers, fall back and try them
  if(m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end())
  {
    IReplayDriver *dr = NULL;
    auto status = m_ReplayDriverProviders[driverType](logfile, &dr);

    if(status == ReplayStatus::Succeeded)
      *driver = (IRemoteDriver *)dr;
    else
      RDCASSERT(dr == NULL);

    return status;
  }

  RDCERR("Unsupported replay driver requested: %d", driverType);
  return ReplayStatus::APIUnsupported;
}

void RenderDoc::SetCurrentDriver(RDCDriver driver)
{
  if(!HasReplayDriver(driver) && !HasRemoteDriver(driver))
  {
    RDCFATAL("Trying to register unsupported driver!");
  }
  m_CurrentDriver = driver;
  m_CurrentDriverName = m_DriverNames[driver];
}

void RenderDoc::GetCurrentDriver(RDCDriver &driver, string &name)
{
  driver = m_CurrentDriver;
  name = m_CurrentDriverName;
}

map<RDCDriver, string> RenderDoc::GetReplayDrivers()
{
  map<RDCDriver, string> ret;
  for(auto it = m_ReplayDriverProviders.begin(); it != m_ReplayDriverProviders.end(); ++it)
    ret[it->first] = m_DriverNames[it->first];
  return ret;
}

map<RDCDriver, string> RenderDoc::GetRemoteDrivers()
{
  map<RDCDriver, string> ret;

  for(auto it = m_RemoteDriverProviders.begin(); it != m_RemoteDriverProviders.end(); ++it)
    ret[it->first] = m_DriverNames[it->first];

  // replay drivers are remote drivers.
  for(auto it = m_ReplayDriverProviders.begin(); it != m_ReplayDriverProviders.end(); ++it)
    ret[it->first] = m_DriverNames[it->first];

  return ret;
}

void RenderDoc::SetCaptureOptions(const CaptureOptions &opts)
{
  m_Options = opts;

  LibraryHooks::GetInstance().OptionsUpdated();
}

void RenderDoc::SetLogFile(const char *logFile)
{
  if(logFile == NULL || logFile[0] == '\0')
    return;

  m_LogFile = logFile;

  if(m_LogFile.length() > 4 && m_LogFile.substr(m_LogFile.length() - 4) == ".rdc")
    m_LogFile = m_LogFile.substr(0, m_LogFile.length() - 4);

  FileIO::CreateParentDirectory(m_LogFile);
}

void RenderDoc::SetProgress(LoadProgressSection section, float delta)
{
  if(m_ProgressPtr == NULL || section < 0 || section >= NumSections)
    return;

  float weights[NumSections];

  // must sum to 1.0
  weights[DebugManagerInit] = 0.1f;
  weights[FileInitialRead] = 0.75f;
  weights[FrameEventsRead] = 0.15f;

  float progress = 0.0f;
  for(int i = 0; i < section; i++)
  {
    progress += weights[i];
  }

  progress += weights[section] * delta;

  *m_ProgressPtr = progress;
}

void RenderDoc::SuccessfullyWrittenLog(uint32_t frameNumber)
{
  RDCLOG("Written to disk: %s", m_CurrentLogFile.c_str());

  CaptureData cap(m_CurrentLogFile, Timing::GetUnixTimestamp(), frameNumber);
  {
    SCOPED_LOCK(m_CaptureLock);
    m_Captures.push_back(cap);
  }
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
    RDCERR("Invalid device pointer: %#p / %#p", dev);
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
