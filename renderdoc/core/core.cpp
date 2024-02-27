/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "common/threading.h"
#include "core/settings.h"
#include "hooks/hooks.h"
#include "maths/formatpacking.h"
#include "replay/replay_driver.h"
#include "serialise/rdcfile.h"
#include "serialise/serialiser.h"
#include "stb/stb_image_write.h"
#include "strings/string_utils.h"
#include "superluminal/superluminal.h"
#include "crash_handler.h"

#include "api/replay/renderdoc_tostr.inl"

#include "api/replay/pipestate.inl"

#include "replay/renderdoc_serialise.inl"

extern "C" const rdcstr VulkanLayerJSONBasename = STRINGIZE(RDOC_BASE_NAME);

RDOC_DEBUG_CONFIG(bool, Capture_Debug_SnapshotDiagnosticLog, false,
                  "Snapshot the diagnostic log at capture time and embed in the capture.");

// this is declared centrally so it can be shared with any backend - the name is a misnomer but kept
// for backwards compatibility reasons.
RDOC_CONFIG(rdcarray<rdcstr>, DXBC_Debug_SearchDirPaths, {},
            "Paths to search for separated shader debug PDBs.");

void LogReplayOptions(const ReplayOptions &opts)
{
  RDCLOG("%s API validation during replay", (opts.apiValidation ? "Enabling" : "Not enabling"));

  if(opts.forceGPUVendor == GPUVendor::Unknown && opts.forceGPUDeviceID == 0 &&
     opts.forceGPUDriverName.empty())
  {
    RDCLOG("Using default GPU replay selection algorithm");
  }
  else
  {
    RDCLOG("Overriding GPU replay selection:");
    RDCLOG("  Vendor %s, device %u, driver \"%s\"", ToStr(opts.forceGPUVendor).c_str(),
           opts.forceGPUDeviceID, opts.forceGPUDriverName.c_str());
  }

  RDCLOG("Replay optimisation level: %s", ToStr(opts.optimisation).c_str());
}

// these one is done by hand as we format it
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

#undef PREFIX

  // the loop will have stepped us to the first NULL before our string, so return c+1
  return c + 1;
}

template <>
rdcstr DoStringise(const PointerVal &el)
{
  if(el.shader != ResourceId())
  {
    // we don't want to format as an ID, we need to encode the raw value
    uint64_t num;
    memcpy(&num, &el.shader, sizeof(num));

    return StringFormat::Fmt("GPUAddress::%llu::%llu::%u", el.pointer, num, el.pointerTypeID);
  }
  else
  {
    return StringFormat::Fmt("GPUAddress::%llu", el.pointer);
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, ResourceId &el)
{
  ser.SerialiseValue(SDBasic::Resource, 8, (uint64_t &)el);
}

INSTANTIATE_SERIALISE_TYPE(ResourceId);

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, RDResult &el)
{
  SERIALISE_MEMBER(code);
  SERIALISE_MEMBER(message);

  SIZE_CHECK(16);
}

INSTANTIATE_SERIALISE_TYPE(RDResult);

#if ENABLED(RDOC_LINUX) && ENABLED(RDOC_XLIB)
#include <X11/Xlib.h>
#endif

// from image_viewer.cpp
RDResult IMG_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver);

template <>
rdcstr DoStringise(const CaptureState &el)
{
  BEGIN_ENUM_STRINGISE(CaptureState);
  {
    STRINGISE_ENUM_CLASS(LoadingReplaying);
    STRINGISE_ENUM_CLASS(ActiveReplaying);
    STRINGISE_ENUM_CLASS(StructuredExport);
    STRINGISE_ENUM_CLASS(BackgroundCapturing);
    STRINGISE_ENUM_CLASS(ActiveCapturing);
  }
  END_ENUM_STRINGISE();
}

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
    STRINGISE_ENUM_CLASS(Metal);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ReplayLogType &el)
{
  BEGIN_ENUM_STRINGISE(ReplayLogType);
  {
    STRINGISE_ENUM_NAMED(eReplay_Full, "Full replay including action");
    STRINGISE_ENUM_NAMED(eReplay_WithoutDraw, "Replay without action");
    STRINGISE_ENUM_NAMED(eReplay_OnlyDraw, "Replay only action");
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
    STRINGISE_ENUM_CLASS_NAMED(DriverInit, "Internal::Driver Initialisation Parameters");
    STRINGISE_ENUM_CLASS_NAMED(InitialContentsList, "Internal::List of Initial Contents Resources");
    STRINGISE_ENUM_CLASS_NAMED(InitialContents, "Internal::Initial Contents");
    STRINGISE_ENUM_CLASS_NAMED(CaptureBegin, "Internal::Beginning of Capture");
    STRINGISE_ENUM_CLASS_NAMED(CaptureScope, "Internal::Frame Metadata");
    STRINGISE_ENUM_CLASS_NAMED(CaptureEnd, "Internal::End of Capture");
  }
  END_ENUM_STRINGISE();
}

RenderDoc &RenderDoc::Inst()
{
  static RenderDoc realInst;
  return realInst;
}

void RenderDoc::RecreateCrashHandler()
{
  SCOPED_WRITELOCK(m_ExHandlerLock);

#if ENABLED(RDOC_CRASH_HANDLER)

  rdcstr exename;
  FileIO::GetExecutableFilename(exename);
  exename = strlower(exename);

  // only create crash handler when we're not in renderdoccmd (to prevent infinite loop as
  // the crash handler itself launches renderdoccmd)
  if(exename.contains("renderdoccmd"))
    return;

#if ENABLED(RDOC_WIN32)
  // there are way too many invalid reports coming from chrome, completely disable the crash handler
  // in that case.
  if(exename.find("chrome.exe") &&
     (GetModuleHandleA("chrome_elf.dll") || GetModuleHandleA("chrome_child.dll")))
  {
    RDCWARN("Disabling crash handling server due to detected chrome.");
    return;
  }

  // some people use vivaldi which is just chrome
  if(exename.find("vivaldi.exe") &&
     (GetModuleHandleA("vivaldi_elf.dll") || GetModuleHandleA("vivaldi_child.dll")))
  {
    RDCWARN("Disabling crash handling server due to detected chrome.");
    return;
  }

  // ditto opera
  if(exename.find("opera.exe") && GetModuleHandleA("opera_browser.dll"))
  {
    RDCWARN("Disabling crash handling server due to detected chrome.");
    return;
  }

  // ditto edge
  if(exename.find("msedge.exe") && GetModuleHandleA("msedge.dll"))
  {
    RDCWARN("Disabling crash handling server due to detected chrome.");
    return;
  }
#endif

  m_ExHandler = new CrashHandler(m_ExHandler);

  m_ExHandler->RegisterMemoryRegion(this, sizeof(RenderDoc));
#endif
}

void RenderDoc::UnloadCrashHandler()
{
  SCOPED_WRITELOCK(m_ExHandlerLock);

  if(!m_ExHandler)
    return;

  m_ExHandler->UnregisterMemoryRegion(this);

  SAFE_DELETE(m_ExHandler);
}

void RenderDoc::RegisterMemoryRegion(void *mem, size_t size)
{
  SCOPED_READLOCK(m_ExHandlerLock);

  if(m_ExHandler)
    m_ExHandler->RegisterMemoryRegion(mem, size);
}

void RenderDoc::UnregisterMemoryRegion(void *mem)
{
  SCOPED_READLOCK(m_ExHandlerLock);

  if(m_ExHandler)
    m_ExHandler->UnregisterMemoryRegion(mem);
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

#if !RENDERDOC_STABLE_BUILD
  Superluminal::Init();
#endif

  m_RemoteIdent = 0;
  m_RemoteThread = 0;

  m_TimeBase = 0;
  m_TimeFrequency = 1.0;

  if(!IsReplayApp())
  {
    m_TimeBase = Timing::GetTick();
    m_TimeFrequency = Timing::GetTickFrequency() / 1000.0;

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
    rdcstr capture_filename;

    const rdcstr base = IsReplayApp() ? "RenderDoc" : "RenderDoc_app";

    FileIO::GetDefaultFiles(base, capture_filename, m_LoggingFilename, m_Target);

    if(m_CaptureFileTemplate.empty())
      SetCaptureFileTemplate(capture_filename);

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

#if defined(RENDERDOC_HOOK_DLSYM)
  RDCWARN("dlsym() hooking enabled!");
#endif

  if(!IsReplayApp())
  {
    if(m_RemoteIdent == 0)
      RDCWARN("Couldn't open socket for target control");
    else
      RDCDEBUG("Listening for target control on %u", m_RemoteIdent);
  }

  Keyboard::Init();

  m_FrameTimer.InitTimers();

  m_ExHandler = NULL;

  RecreateCrashHandler();

  // begin printing to stdout/stderr after this point, earlier logging is debugging
  // cruft that we don't want cluttering output.
  // However we don't want to print in captured applications, since they may be outputting important
  // information to stdout/stderr and being piped around and processed!
  if(IsReplayApp())
    RDCLOGOUTPUT();

  ProcessConfig();
}

RenderDoc::~RenderDoc()
{
  if(m_ExHandler)
  {
    UnloadCrashHandler();
  }

  for(auto it = m_ShutdownFunctions.begin(); it != m_ShutdownFunctions.end(); ++it)
    (*it)();
  m_ShutdownFunctions.clear();

  for(size_t i = 0; i < m_Captures.size(); i++)
  {
    if(m_Captures[i].retrieved)
    {
      RDCLOG("Removing remotely retrieved capture %s", m_Captures[i].path.c_str());
      FileIO::Delete(m_Captures[i].path);
    }
    else
    {
      RDCLOG("'Leaking' unretrieved capture %s", m_Captures[i].path.c_str());
    }
  }

  RDCSTOPLOGGING();

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

  delete m_Config;

  Process::Shutdown();

  Network::Shutdown();

  Threading::Shutdown();

  StringFormat::Shutdown();
}

void RenderDoc::RemoveHooks()
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

void RenderDoc::InitialiseReplay(GlobalEnvironment env, const rdcarray<rdcstr> &args)
{
  if(!IsReplayApp())
  {
    RDCERR(
        "Initialising replay within non-replaying app. Did you properly export replay marker in "
        "host executable or library, or are you trying to replay directly with a self-hosted "
        "capture build?");
  }

  m_GlobalEnv = env;

#if ENABLED(RDOC_LINUX) && ENABLED(RDOC_XLIB)
  if(!m_GlobalEnv.xlibDisplay)
    m_GlobalEnv.xlibDisplay = XOpenDisplay(NULL);
#endif

  rdcstr exename;
  FileIO::GetExecutableFilename(exename);
  RDCLOG("Replay application '%s' launched", exename.c_str());
  if(!args.empty())
  {
    for(size_t i = 0; i < args.size(); i++)
      RDCLOG("Parameter [%u]: %s", (uint32_t)i, args[i].c_str());
  }

  if(args.contains("--crash"))
    UnloadCrashHandler();
  else
    RecreateCrashHandler();

  if(env.enumerateGPUs)
  {
    m_AvailableGPUThread = Threading::CreateThread([this]() {
      for(GraphicsAPI api : {GraphicsAPI::D3D11, GraphicsAPI::D3D12, GraphicsAPI::Vulkan})
      {
        RDCDriver driverType = RDCDriver::Unknown;

        switch(api)
        {
          case GraphicsAPI::D3D11: driverType = RDCDriver::D3D11; break;
          case GraphicsAPI::D3D12: driverType = RDCDriver::D3D12; break;
          case GraphicsAPI::OpenGL: break;
          case GraphicsAPI::Vulkan: driverType = RDCDriver::Vulkan; break;
        }

        if(driverType == RDCDriver::Unknown || !HasReplayDriver(driverType))
          continue;

        IReplayDriver *driver = NULL;
        RDResult result = m_ReplayDriverProviders[driverType](NULL, ReplayOptions(), &driver);

        if(result == ResultCode::Succeeded)
        {
          rdcarray<GPUDevice> gpus = driver->GetAvailableGPUs();

          for(const GPUDevice &newgpu : gpus)
          {
            bool addnew = true;

            for(GPUDevice &oldgpu : m_AvailableGPUs)
            {
              // if we have this GPU listed already, just add its API to the previous list
              if(oldgpu == newgpu)
              {
                oldgpu.apis.push_back(api);
                addnew = false;
              }
            }

            if(addnew)
              m_AvailableGPUs.push_back(newgpu);
          }
        }
        else
        {
          RDCWARN("Couldn't create proxy replay driver for %s: %s", ToStr(driverType).c_str(),
                  ResultDetails(result).Message().c_str());
        }

        if(driver)
          driver->Shutdown();
      }

      // we now have a list of GPUs, however we might have some duplicates if some APIs have
      // multiple drivers for a single device. To compact this list, for each GPU with no driver
      // we find all matching multi-drive GPUs and merge it into all matching copies.
      bool hasDriverNames = false;
      for(size_t i = 0; i < m_AvailableGPUs.size(); i++)
        hasDriverNames |= !m_AvailableGPUs[i].driver.empty();

      if(hasDriverNames)
      {
        for(size_t i = 0; i < m_AvailableGPUs.size();)
        {
          bool applied = false;

          if(!m_AvailableGPUs[i].driver.empty())
          {
            i++;
            continue;
          }

          // scan all subsequent GPUs, if we find a duplicate, merge the APIs
          for(size_t j = i + 1; j < m_AvailableGPUs.size(); j++)
          {
            if(m_AvailableGPUs[i].vendor == m_AvailableGPUs[j].vendor &&
               m_AvailableGPUs[i].deviceID == m_AvailableGPUs[j].deviceID)
            {
              RDCASSERT(!m_AvailableGPUs[j].driver.empty());
              for(GraphicsAPI a : m_AvailableGPUs[i].apis)
              {
                if(m_AvailableGPUs[j].apis.indexOf(a) == -1)
                  m_AvailableGPUs[j].apis.push_back(a);
              }
              applied = true;
            }
          }

          // we "applied" this GPU to all its driver-based duplicates, so we can remove it now
          if(applied)
          {
            m_AvailableGPUs.erase(i);
          }
          else
          {
            i++;
          }
        }
      }

      // sort the APIs list in each GPU, and sort the GPUs
      std::sort(m_AvailableGPUs.begin(), m_AvailableGPUs.end());
      for(GPUDevice &dev : m_AvailableGPUs)
      {
        std::sort(dev.apis.begin(), dev.apis.end());
      }
    });
  }
}

void RenderDoc::ShutdownReplay()
{
  SyncAvailableGPUThread();

  // call shutdown functions early, as we only want to do these in the RenderDoc destructor if we
  // have no other choice (i.e. we're capturing).
  for(auto it = m_ShutdownFunctions.begin(); it != m_ShutdownFunctions.end(); ++it)
    (*it)();
  m_ShutdownFunctions.clear();
}

void RenderDoc::RegisterShutdownFunction(ShutdownFunction func)
{
  auto it = std::lower_bound(m_ShutdownFunctions.begin(), m_ShutdownFunctions.end(), func);
  if(it == m_ShutdownFunctions.end() || *it != func)
    m_ShutdownFunctions.insert(it - m_ShutdownFunctions.begin(), func);
}

bool RenderDoc::MatchClosestWindow(DeviceOwnedWindow &devWnd)
{
  SCOPED_LOCK(m_CapturerListLock);

  // lower_bound and the DeviceWnd ordering (pointer compares, dev over wnd) means that if either
  // element in devWnd is NULL we can go forward from this iterator and find the first wildcardMatch
  // note that if dev is specified and wnd is NULL, this will actually point at the first
  // wildcardMatch already and we can use it immediately (since which window of multiple we
  // choose is undefined, so up to us). If dev is NULL there is no window ordering (since dev is
  // the primary sorting value) so we just iterate through the whole map. It should be small in
  // the majority of cases
  auto it = m_WindowFrameCapturers.lower_bound(devWnd);

  while(it != m_WindowFrameCapturers.end())
  {
    if(it->first.wildcardMatch(devWnd))
      break;
    ++it;
  }

  if(it != m_WindowFrameCapturers.end())
  {
    devWnd = it->first;
    return true;
  }

  return false;
}

bool RenderDoc::IsActiveWindow(DeviceOwnedWindow devWnd)
{
  SCOPED_LOCK(m_CapturerListLock);
  return devWnd == m_ActiveWindow;
}

void RenderDoc::GetActiveWindow(DeviceOwnedWindow &devWnd)
{
  SCOPED_LOCK(m_CapturerListLock);
  devWnd = m_ActiveWindow;
}

IFrameCapturer *RenderDoc::MatchFrameCapturer(DeviceOwnedWindow devWnd)
{
  // try and find the closest frame capture registered, and update
  // the values in devWnd to point to it precisely
  bool exactMatch = MatchClosestWindow(devWnd);

  SCOPED_LOCK(m_CapturerListLock);

  if(!exactMatch)
  {
    // handle off-screen rendering where there are no device/window pairs in
    // m_WindowFrameCapturers, instead we use the first matching device frame capturer
    if(devWnd.windowHandle == NULL)
    {
      auto defaultit = m_DeviceFrameCapturers.find(devWnd.device);
      if(defaultit == m_DeviceFrameCapturers.end() && !m_DeviceFrameCapturers.empty())
        defaultit = m_DeviceFrameCapturers.begin();

      if(defaultit != m_DeviceFrameCapturers.end())
        return defaultit->second;
    }

    RDCERR(
        "Couldn't find matching frame capturer for device %p window %p "
        "from %zu device frame capturers and %zu frame capturers",
        devWnd.device, devWnd.windowHandle, m_DeviceFrameCapturers.size(),
        m_WindowFrameCapturers.size());
    return NULL;
  }

  auto it = m_WindowFrameCapturers.find(devWnd);

  if(it == m_WindowFrameCapturers.end())
  {
    RDCERR("Couldn't find frame capturer after exact match!");
    return NULL;
  }

  return it->second.FrameCapturer;
}

void RenderDoc::StartFrameCapture(DeviceOwnedWindow devWnd)
{
  m_CaptureTitle.clear();
  IFrameCapturer *frameCap = MatchFrameCapturer(devWnd);
  if(frameCap)
  {
    frameCap->StartFrameCapture(devWnd);
    m_CapturesActive++;
  }
}

void RenderDoc::SetActiveWindow(DeviceOwnedWindow devWnd)
{
  SCOPED_LOCK(m_CapturerListLock);

  auto it = m_WindowFrameCapturers.find(devWnd);
  if(it == m_WindowFrameCapturers.end())
  {
    RDCERR("Couldn't find frame capturer for device %p window %p", devWnd.device,
           devWnd.windowHandle);
    return;
  }

  m_ActiveWindow = devWnd;
}

void RenderDoc::SetCaptureTitle(const rdcstr &title)
{
  m_CaptureTitle = title;
}

bool RenderDoc::EndFrameCapture(DeviceOwnedWindow devWnd)
{
  IFrameCapturer *frameCap = MatchFrameCapturer(devWnd);
  if(frameCap)
  {
    bool ret = frameCap->EndFrameCapture(devWnd);
    m_CapturesActive--;
    return ret;
  }
  return false;
}

bool RenderDoc::DiscardFrameCapture(DeviceOwnedWindow devWnd)
{
  IFrameCapturer *frameCap = MatchFrameCapturer(devWnd);
  if(frameCap)
  {
    bool ret = frameCap->DiscardFrameCapture(devWnd);
    m_CapturesActive--;
    return ret;
  }
  return false;
}

bool RenderDoc::IsTargetControlConnected()
{
  SCOPED_LOCK(m_SingleClientLock);
  return !m_SingleClientName.empty();
}

rdcstr RenderDoc::GetTargetControlUsername()
{
  SCOPED_LOCK(m_SingleClientLock);
  return m_SingleClientName;
}

bool RenderDoc::ShowReplayUI()
{
  SCOPED_LOCK(m_SingleClientLock);
  if(m_SingleClientName.empty())
    return false;

  m_RequestControllerShow = true;
  return true;
}

void RenderDoc::Tick()
{
  bool cur_focus = false;
  for(size_t i = 0; i < m_FocusKeys.size(); i++)
    cur_focus |= Keyboard::GetKeyState(m_FocusKeys[i]);

  bool cur_cap = false;
  for(size_t i = 0; i < m_CaptureKeys.size(); i++)
    cur_cap |= Keyboard::GetKeyState(m_CaptureKeys[i]);

  m_FrameTimer.UpdateTimers();

  if(!m_PrevFocus && cur_focus)
  {
    CycleActiveWindow();
  }
  if(!m_PrevCap && cur_cap)
  {
    TriggerCapture(1);
  }

  m_PrevFocus = cur_focus;
  m_PrevCap = cur_cap;

  // check for any child threads that need to be waited on, remove them from the list
  rdcarray<Threading::ThreadHandle> waitThreads;
  {
    SCOPED_LOCK(m_ChildLock);
    for(rdcpair<uint32_t, Threading::ThreadHandle> &c : m_ChildThreads)
    {
      if(c.first == 0)
        waitThreads.push_back(c.second);
    }

    m_ChildThreads.removeIf(
        [](const rdcpair<uint32_t, Threading::ThreadHandle> &c) { return c.first == 0; });
  }

  // clean up the threads now
  for(Threading::ThreadHandle t : waitThreads)
  {
    Threading::JoinThread(t);
    Threading::CloseThread(t);
  }
}

void RenderDoc::CycleActiveWindow()
{
  SCOPED_LOCK(m_CapturerListLock);

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

uint32_t RenderDoc::GetCapturableWindowCount()
{
  SCOPED_LOCK(m_CapturerListLock);
  return (uint32_t)m_WindowFrameCapturers.size();
}

rdcstr RenderDoc::GetOverlayText(RDCDriver driver, DeviceOwnedWindow devWnd, uint32_t frameNumber,
                                 int flags)
{
  bool activeWindow;
  const bool capturesEnabled = (flags & eOverlay_CaptureDisabled) == 0;

  uint32_t overlay = GetOverlayBits();

  RDCDriver activeDriver = RDCDriver::Unknown;
  RDCDriver curDriver = RDCDriver::Unknown;

  int activeIdx = -1, curIdx = -1, idx = 0;
  size_t numWindows;
  {
    SCOPED_LOCK(m_CapturerListLock);

    activeWindow = (devWnd == m_ActiveWindow);

    for(auto it = m_WindowFrameCapturers.begin(); it != m_WindowFrameCapturers.end(); ++it, ++idx)
    {
      if(it->first == m_ActiveWindow)
      {
        activeIdx = idx;
        activeDriver = it->second.FrameCapturer->GetFrameCaptureDriver();
      }
      if(it->first == devWnd)
      {
        curIdx = idx;
        curDriver = it->second.FrameCapturer->GetFrameCaptureDriver();
      }
    }

    numWindows = m_WindowFrameCapturers.size();
  }

  if(activeDriver == RDCDriver::Unknown)
    activeDriver = curDriver;

  if(activeDriver == RDCDriver::Unknown)
    activeDriver = driver;

  // example layout:
  //
  // Capturing D3D11.  Frame: 1234. 33ms (30 FPS)
  // F12, PrtScrn to capture. 3 Captures saved.
  // Captured frame 1200.
  //
  // Frame number, FPS, capture list are optional. If capture list is disabled
  // the second line still displays the keys as long as capturing is allowed.
  // if capturing is disabled, only the first line displays.
  //
  // On platforms without keyboards, the keys are replaced by a remote access connection status
  // message.
  //
  // with multiple windows the active window will look like:
  //
  // Capturing D3D11.  Window 1 active. Frame: 1234. 33ms (30 FPS)
  // F12, PrtScrn to capture. 3 Captures saved.
  // Captured frame 1200.
  //
  // Inactive windows will look like:
  //
  // Capturing D3D11.  Window 1 active.
  // F11 to cycle. OpenGL window 2.

  rdcstr overlayText = ToStr(activeDriver) + ".";

  // pad this so it's the same length regardless of API length
  while(overlayText.length() < 8)
    overlayText.push_back(' ');

  overlayText = "Capturing " + overlayText;

  if(numWindows > 1)
  {
    if(activeIdx >= 0)
      overlayText += StringFormat::Fmt(" Window %d active.", activeIdx);
    else
      overlayText += " No window active.";
  }

  if(activeWindow)
  {
    if(overlay & eRENDERDOC_Overlay_FrameNumber)
      overlayText += StringFormat::Fmt(" Frame: %d.", frameNumber);

    if(overlay & eRENDERDOC_Overlay_FrameRate)
    {
      const double frameTime = m_FrameTimer.GetAvgFrameTime();
      // max with 0.01ms so that we don't divide by zero
      const double fps = 1000.0f / RDCMAX(0.01, frameTime);

      if(frameTime < 0.0001)
      {
        overlayText += " --- ms (--- FPS)";
      }
      else
      {
        // only display frametime fractions if it's relevant (sub-integer frame time or FPS)

        if(frameTime < 1.0)
          overlayText += StringFormat::Fmt(" %.2lf ms", m_FrameTimer.GetAvgFrameTime());
        else
          overlayText += StringFormat::Fmt(" %d ms", int(m_FrameTimer.GetAvgFrameTime()));

        if(fps < 1.0)
          overlayText += StringFormat::Fmt(" (%.2lf FPS)", fps);
        else
          overlayText += StringFormat::Fmt(" (%d FPS)", int(fps));
      }
    }
  }

  overlayText += "\n";

#if ENABLED(RDOC_DEVEL)
  {
    overlayText += StringFormat::Fmt("%llu chunks - %.2f MB\n", Chunk::NumLiveChunks(),
                                     float(Chunk::TotalMem()) / 1024.0f / 1024.0f);
  }
#endif

  if(capturesEnabled)
  {
    if(activeWindow)
    {
      rdcarray<RENDERDOC_InputButton> keys = GetCaptureKeys();

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

      if(overlay & eRENDERDOC_Overlay_CaptureList)
      {
        overlayText += StringFormat::Fmt(" %d Captures saved.\n", (uint32_t)m_Captures.size());

        uint64_t now = Timing::GetUnixTimestamp();
        for(size_t i = 0; i < m_Captures.size(); i++)
        {
          if(now - m_Captures[i].timestamp < 20)
          {
            if(m_Captures[i].frameNumber == ~0U)
              overlayText += "Captured user-defined capture.\n";
            else
              overlayText += StringFormat::Fmt("Captured frame %d.\n", m_Captures[i].frameNumber);
          }
        }
      }
    }
    else
    {
      rdcarray<RENDERDOC_InputButton> keys = GetFocusKeys();

      if(Keyboard::PlatformHasKeyInput())
      {
        for(size_t i = 0; i < keys.size(); i++)
        {
          if(i > 0)
            overlayText += ", ";

          overlayText += ToStr(keys[i]);
        }

        if(!keys.empty())
          overlayText += " to cycle.";
      }
      else
      {
        if(IsTargetControlConnected())
          overlayText += "Connected by " + GetTargetControlUsername() + ".";
        else
          overlayText += "No remote access connection.";
      }

      if(curIdx >= 0)
        overlayText += StringFormat::Fmt(" %s window %d.", ToStr(curDriver).c_str(), curIdx);
      else if(curDriver != RDCDriver::Unknown)
        overlayText += StringFormat::Fmt(" Unknown %s window.", ToStr(curDriver).c_str());
      else
        overlayText += " Unknown window.";
    }
  }

  return overlayText;
}

void RenderDoc::QueueCapture(uint32_t frameNumber)
{
  auto it = std::lower_bound(m_QueuedFrameCaptures.begin(), m_QueuedFrameCaptures.end(), frameNumber);
  if(it == m_QueuedFrameCaptures.end() || *it != frameNumber)
    m_QueuedFrameCaptures.insert(it - m_QueuedFrameCaptures.begin(), frameNumber);
}

bool RenderDoc::ShouldTriggerCapture(uint32_t frameNumber)
{
  bool ret = m_Cap > 0;

  if(m_Cap > 0)
    m_Cap--;

  rdcarray<uint32_t> frames;
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
      m_QueuedFrameCaptures.push_back(*it);
    }
  }

  return ret;
}

void RenderDoc::ResamplePixels(const FramePixels &in, RDCThumb &out)
{
  if(in.width == 0 || in.height == 0)
  {
    out = RDCThumb();
    return;
  }

  // code below assumes pitch_requirement is a power of 2 number
  RDCASSERT((in.pitch_requirement & (in.pitch_requirement - 1)) == 0);

  out.width = (uint16_t)RDCMIN(in.max_width, in.width);
  out.width &= ~(in.pitch_requirement - 1);    // align down to multiple of in.
  out.height = uint16_t(out.width * in.height / in.width);
  out.pixels.resize(3 * out.width * out.height);
  out.format = FileType::Raw;

  byte *dst = (byte *)out.pixels.data();
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
        dst[0] = (byte)(unorm.x * 255.0f);
        dst[1] = (byte)(unorm.y * 255.0f);
        dst[2] = (byte)(unorm.z * 255.0f);
      }
      else if(in.buf5551)
      {
        uint16_t *src5551 = (uint16_t *)src;
        Vec4f unorm = ConvertFromB5G5R5A1(*src5551);
        dst[0] = (byte)(unorm.x * 255.0f);
        dst[1] = (byte)(unorm.y * 255.0f);
        dst[2] = (byte)(unorm.z * 255.0f);
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

        dst[0] = byte(255.0f * ConvertLinearToSRGB(linearR));
        dst[1] = byte(255.0f * ConvertLinearToSRGB(linearG));
        dst[2] = byte(255.0f * ConvertLinearToSRGB(linearB));
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
    for(uint16_t y = 0; y < out.height / 2; y++)
    {
      uint16_t flipY = (out.height - 1 - y);
      for(uint16_t x = 0; x < out.width; x++)
      {
        byte *src = (byte *)out.pixels.data();
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
  if(in.width == 0 || in.height == 0)
  {
    out = RDCThumb();
    return;
  }

  struct WriteCallbackData
  {
    bytebuf buffer;

    static void writeData(void *context, void *data, int size)
    {
      WriteCallbackData *pThis = (WriteCallbackData *)context;
      pThis->buffer.append((const byte *)data, size);
    }
  };

  WriteCallbackData callbackData;
  stbi_write_png_to_func(&WriteCallbackData::writeData, &callbackData, in.width, in.height, 3,
                         in.pixels.data(), 0);
  out.width = in.width;
  out.height = in.height;
  out.pixels.swap(callbackData.buffer);
  out.format = FileType::PNG;
}

RDCFile *RenderDoc::CreateRDC(RDCDriver driver, uint32_t frameNum, const FramePixels &fp)
{
  RDCFile *ret = new RDCFile;

  rdcstr suffix = StringFormat::Fmt("_frame%u", frameNum);

  if(frameNum == ~0U)
    suffix = "_capture";

  m_CurrentLogFile = StringFormat::Fmt("%s%s.rdc", m_CaptureFileTemplate.c_str(), suffix.c_str());

  // make sure we don't stomp another capture if we make multiple captures in the same frame.
  {
    SCOPED_LOCK(m_CaptureLock);
    int altnum = 2;
    while(std::find_if(m_Captures.begin(), m_Captures.end(), [this](const CaptureData &o) {
            return o.path == m_CurrentLogFile;
          }) != m_Captures.end())
    {
      m_CurrentLogFile =
          StringFormat::Fmt("%s%s_%d.rdc", m_CaptureFileTemplate.c_str(), suffix.c_str(), altnum);
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

  ret->SetData(driver, ToStr(driver).c_str(), OSUtility::GetMachineIdent(), &outPng, m_TimeBase,
               m_TimeFrequency);

  FileIO::CreateParentDirectory(m_CurrentLogFile);

  ret->Create(m_CurrentLogFile.c_str());

  if(ret->Error() != ResultCode::Succeeded)
    SAFE_DELETE(ret);

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
  rdcstr filetype = description.extension;

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
  rdcstr filetype = description.extension;

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

void RenderDoc::RegisterDeviceProtocol(const rdcstr &protocol, ProtocolHandler handler)
{
  if(m_Protocols[protocol] != NULL)
  {
    RDCERR("Duplicate protocol registration: %s", protocol.c_str());
    return;
  }
  m_Protocols[protocol] = handler;
}

StructuredProcessor RenderDoc::GetStructuredProcessor(RDCDriver driver)
{
  auto it = m_StructProcesssors.find(driver);

  if(it == m_StructProcesssors.end())
    return NULL;

  return it->second;
}

CaptureExporter RenderDoc::GetCaptureExporter(const rdcstr &filetype)
{
  auto it = m_Exporters.find(filetype);

  if(it == m_Exporters.end())
    return NULL;

  return it->second;
}

CaptureImporter RenderDoc::GetCaptureImporter(const rdcstr &filetype)
{
  auto it = m_Importers.find(filetype);

  if(it == m_Importers.end())
    return NULL;

  return it->second;
}

rdcarray<rdcstr> RenderDoc::GetSupportedDeviceProtocols()
{
  rdcarray<rdcstr> ret;

  for(auto it = m_Protocols.begin(); it != m_Protocols.end(); ++it)
    ret.push_back(it->first);

  return ret;
}

IDeviceProtocolHandler *RenderDoc::GetDeviceProtocol(const rdcstr &protocol)
{
  rdcstr p = protocol;

  // allow passing in an URL with ://
  int32_t offs = p.find("://");
  if(offs >= 0)
    p.erase(offs, p.size() - offs);

  auto it = m_Protocols.find(p);

  if(it != m_Protocols.end())
    return it->second();

  return NULL;
}

rdcarray<CaptureFileFormat> RenderDoc::GetCaptureFileFormats()
{
  rdcarray<CaptureFileFormat> ret = m_ImportExportFormats;

  std::sort(ret.begin(), ret.end());

  {
    CaptureFileFormat rdc;
    rdc.extension = "rdc";
    rdc.name = "Native RDC capture file format.";
    rdc.description = "The format produced by frame-captures from applications directly.";
    rdc.openSupported = true;
    rdc.convertSupported = true;

    ret.insert(0, rdc);
  }

  return ret;
}

rdcarray<GPUDevice> RenderDoc::GetAvailableGPUs()
{
  SyncAvailableGPUThread();

  return m_AvailableGPUs;
}

void RenderDoc::SyncAvailableGPUThread()
{
  if(m_AvailableGPUThread)
  {
    Threading::JoinThread(m_AvailableGPUThread);
    Threading::CloseThread(m_AvailableGPUThread);
    m_AvailableGPUThread = 0;
  }
}

bool RenderDoc::HasReplaySupport(RDCDriver driverType)
{
  if(driverType == RDCDriver::Image)
    return true;

  if(driverType == RDCDriver::Unknown && !m_ReplayDriverProviders.empty())
    return true;

  return m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end();
}

RDResult RenderDoc::CreateProxyReplayDriver(RDCDriver proxyDriver, IReplayDriver **driver)
{
  SyncAvailableGPUThread();

  // passing RDCDriver::Unknown means 'I don't care, give me a proxy driver of any type'
  if(proxyDriver == RDCDriver::Unknown)
  {
    if(!m_ReplayDriverProviders.empty())
      return m_ReplayDriverProviders.begin()->second(NULL, ReplayOptions(), driver);
  }

  if(m_ReplayDriverProviders.find(proxyDriver) != m_ReplayDriverProviders.end())
    return m_ReplayDriverProviders[proxyDriver](NULL, ReplayOptions(), driver);

  RETURN_ERROR_RESULT(ResultCode::APIUnsupported, "Unsupported replay driver requested: %s",
                      ToStr(proxyDriver).c_str());
}

RDResult RenderDoc::CreateReplayDriver(RDCFile *rdc, const ReplayOptions &opts, IReplayDriver **driver)
{
  if(driver == NULL)
    return ResultCode::InvalidParameter;

  SyncAvailableGPUThread();

  // allows passing NULL rdcfile as 'I don't care, give me a proxy driver of any type'
  if(rdc == NULL)
  {
    if(!m_ReplayDriverProviders.empty())
      return m_ReplayDriverProviders.begin()->second(NULL, opts, driver);

    RETURN_ERROR_RESULT(ResultCode::APIUnsupported,
                        "Request for proxy replay device, but no replay providers are available.");
  }

  RDCDriver driverType = rdc->GetDriver();

  // image support is special, handle it here
  if(driverType == RDCDriver::Image)
    return IMG_CreateReplayDevice(rdc, driver);

  if(m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end())
    return m_ReplayDriverProviders[driverType](rdc, opts, driver);

  RDCERR("Unsupported replay driver requested: %s", ToStr(driverType).c_str());
  return ResultCode::APIUnsupported;
}

RDResult RenderDoc::CreateRemoteDriver(RDCFile *rdc, const ReplayOptions &opts, IRemoteDriver **driver)
{
  if(rdc == NULL || driver == NULL)
    return ResultCode::InvalidParameter;

  SyncAvailableGPUThread();

  RDCDriver driverType = rdc->GetDriver();

  if(m_RemoteDriverProviders.find(driverType) != m_RemoteDriverProviders.end())
    return m_RemoteDriverProviders[driverType](rdc, opts, driver);

  // replay drivers are remote drivers, fall back and try them
  if(m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end())
  {
    IReplayDriver *dr = NULL;
    RDResult result = m_ReplayDriverProviders[driverType](rdc, opts, &dr);

    if(result == ResultCode::Succeeded)
      *driver = (IRemoteDriver *)dr;
    else
      RDCASSERT(dr == NULL);

    return result;
  }

  RETURN_ERROR_RESULT(ResultCode::APIUnsupported, "Unsupported replay driver requested: %s",
                      ToStr(driverType).c_str());
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

void RenderDoc::SetDriverUnsupportedMessage(RDCDriver driver, rdcstr message)
{
  if(driver == RDCDriver::Unknown)
    return;

  SCOPED_LOCK(m_DriverLock);
  m_APISupportMessages[driver] = message;
}

std::map<RDCDriver, RDCDriverStatus> RenderDoc::GetActiveDrivers()
{
  std::map<RDCDriver, uint64_t> drivers;

  {
    SCOPED_LOCK(m_DriverLock);
    drivers = m_ActiveDrivers;
  }

  std::map<RDCDriver, RDCDriverStatus> ret;

  for(auto it = drivers.begin(); it != drivers.end(); ++it)
  {
    RDCDriverStatus &status = ret[it->first];
    // driver is presenting if the timestamp is greater than 0 and less than 10 seconds ago (gives a
    // little leeway for loading screens or something where the presentation stops temporarily).
    // we also assume that during a capture if it was presenting, then it's still capturing.
    // Otherwise a long capture would temporarily set it as not presenting.
    status.presenting = it->second > 0;

    if(status.presenting && !IsFrameCapturing() && it->second < Timing::GetUnixTimestamp() - 10)
      status.presenting = false;

    status.supported = (HasRemoteDriver(it->first) || HasReplayDriver(it->first)) &&
                       HasActiveFrameCapturer(it->first);

    if(!status.supported)
    {
      SCOPED_LOCK(m_DriverLock);
      status.supportMessage = m_APISupportMessages[it->first];
    }
  }

  return ret;
}

std::map<RDCDriver, rdcstr> RenderDoc::GetReplayDrivers()
{
  std::map<RDCDriver, rdcstr> ret;
  for(auto it = m_ReplayDriverProviders.begin(); it != m_ReplayDriverProviders.end(); ++it)
    ret[it->first] = ToStr(it->first);
  return ret;
}

std::map<RDCDriver, rdcstr> RenderDoc::GetRemoteDrivers()
{
  std::map<RDCDriver, rdcstr> ret;

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
  RDResult result = CreateProxyReplayDriver(driverType, &driver);

  if(result == ResultCode::Succeeded)
  {
    ret = driver->GetDriverInfo();
  }
  else
  {
    RDCERR("Couldn't create proxy replay driver for %s: %s", ToStr(driverType).c_str(),
           ResultDetails(result).Message().c_str());
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

void RenderDoc::SetCaptureFileTemplate(const rdcstr &pathtemplate)
{
  if(pathtemplate.empty())
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
      header.format = thumb.format;
      header.len = (uint32_t)thumb.pixels.size();
      w->Write(header);
      w->Write(thumb.pixels.data(), thumb.pixels.size());

      w->Finish();

      delete w;
    }

    if(Capture_Debug_SnapshotDiagnosticLog())
    {
      rdcstr logcontents = FileIO::logfile_readall(0, RDCGETLOGFILE());

      SectionProperties props = {};
      props.type = SectionType::EmbeddedLogfile;
      props.version = 1;
      props.flags = SectionFlags::LZ4Compressed;
      StreamWriter *w = rdc->WriteSection(props);

      w->Write(logcontents.data(), logcontents.size());

      w->Finish();

      delete w;
    }

    RDCLOG("Written to disk: %s", m_CurrentLogFile.c_str());

    CaptureData cap;
    cap.path = m_CurrentLogFile;
    cap.title = m_CaptureTitle;
    cap.timestamp = Timing::GetUnixTimestamp();
    cap.driver = rdc->GetDriver();
    cap.frameNumber = frameNumber;
    m_CaptureTitle.clear();
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

void RenderDoc::AddChildProcess(uint32_t pid, uint32_t ident)
{
  if(ident == 0 || ident == m_RemoteIdent)
  {
    RDCERR("Child process %u returned invalid ident %u. Possibly too many listen sockets in use!",
           pid, ident);
    return;
  }

  SCOPED_LOCK(m_ChildLock);
  m_Children.push_back(make_rdcpair(pid, ident));
}

rdcarray<rdcpair<uint32_t, uint32_t>> RenderDoc::GetChildProcesses()
{
  SCOPED_LOCK(m_ChildLock);
  return m_Children;
}

void RenderDoc::CompleteChildThread(uint32_t pid)
{
  SCOPED_LOCK(m_ChildLock);
  // the thread for this PID is done, mark it as ready to wait on by zero-ing out the PID
  for(rdcpair<uint32_t, Threading::ThreadHandle> &c : m_ChildThreads)
  {
    if(c.first == pid)
      c.first = 0;
  }
}

void RenderDoc::AddChildThread(uint32_t pid, Threading::ThreadHandle thread)
{
  SCOPED_LOCK(m_ChildLock);
  m_ChildThreads.push_back(make_rdcpair(pid, thread));
}

void RenderDoc::ValidateCaptures()
{
  SCOPED_LOCK(m_CaptureLock);
  m_Captures.removeIf([](const CaptureData &cap) { return !FileIO::exists(cap.path); });
}

rdcarray<CaptureData> RenderDoc::GetCaptures()
{
  SCOPED_LOCK(m_CaptureLock);
  return m_Captures;
}

void RenderDoc::MarkCaptureRetrieved(uint32_t idx)
{
  SCOPED_LOCK(m_CaptureLock);
  if(idx < m_Captures.size())
  {
    m_Captures[idx].retrieved = true;
  }
}

void RenderDoc::AddDeviceFrameCapturer(void *dev, IFrameCapturer *cap)
{
  if(IsReplayApp())
    return;

  if(dev == NULL || cap == NULL)
  {
    RDCERR("Invalid FrameCapturer %#p for device: %#p", cap, dev);
    return;
  }

  RDCLOG("Adding %s device frame capturer for %#p", ToStr(cap->GetFrameCaptureDriver()).c_str(), dev);

  SCOPED_LOCK(m_CapturerListLock);
  m_DeviceFrameCapturers[dev] = cap;
}

void RenderDoc::RemoveDeviceFrameCapturer(void *dev)
{
  if(IsReplayApp())
    return;

  if(dev == NULL)
  {
    RDCERR("Invalid device pointer: %#p", dev);
    return;
  }

  RDCLOG("Removing device frame capturer for %#p", dev);

  SCOPED_LOCK(m_CapturerListLock);
  m_DeviceFrameCapturers.erase(dev);
}

void RenderDoc::AddFrameCapturer(DeviceOwnedWindow devWnd, IFrameCapturer *cap)
{
  if(IsReplayApp())
    return;

  if(devWnd.device == NULL || devWnd.windowHandle == NULL || cap == NULL)
  {
    RDCERR("Invalid FrameCapturer %#p for combination: %#p / %#p", cap, devWnd.device,
           devWnd.windowHandle);
    return;
  }

  RDCLOG("Adding %s frame capturer for %#p / %#p", ToStr(cap->GetFrameCaptureDriver()).c_str(),
         devWnd.device, devWnd.windowHandle);

  SCOPED_LOCK(m_CapturerListLock);

  auto it = m_WindowFrameCapturers.find(devWnd);
  if(it != m_WindowFrameCapturers.end())
  {
    if(it->second.FrameCapturer != cap)
      RDCERR("New different FrameCapturer being registered for known device/window pair!");

    it->second.RefCount++;
  }
  else
  {
    m_WindowFrameCapturers[devWnd].FrameCapturer = cap;
  }

  // the first one we see becomes the default
  if(m_ActiveWindow == DeviceOwnedWindow())
    m_ActiveWindow = devWnd;
}

void RenderDoc::RemoveFrameCapturer(DeviceOwnedWindow devWnd)
{
  if(IsReplayApp())
    return;

  RDCLOG("Removing frame capturer for %#p / %#p", devWnd.device, devWnd.windowHandle);

  SCOPED_LOCK(m_CapturerListLock);

  auto it = m_WindowFrameCapturers.find(devWnd);
  if(it != m_WindowFrameCapturers.end())
  {
    it->second.RefCount--;

    if(it->second.RefCount <= 0)
    {
      RDCLOG("Removed last refcount");

      if(m_ActiveWindow == devWnd)
      {
        RDCLOG("Removed active window");

        if(m_WindowFrameCapturers.size() == 1)
        {
          m_ActiveWindow = DeviceOwnedWindow();
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

bool RenderDoc::HasActiveFrameCapturer(RDCDriver driver)
{
  SCOPED_LOCK(m_CapturerListLock);

  for(auto cap = m_WindowFrameCapturers.begin(); cap != m_WindowFrameCapturers.end(); cap++)
    if(cap->second.FrameCapturer->GetFrameCaptureDriver() == driver)
      return true;

  for(auto cap = m_DeviceFrameCapturers.begin(); cap != m_DeviceFrameCapturers.end(); cap++)
    if(cap->second->GetFrameCaptureDriver() == driver)
      return true;

  return false;
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None
#undef Always

#include "catch/catch.hpp"

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

TEST_CASE("Check ResamplePixels", "[core][resamplepixels]")
{
  RenderDoc::FramePixels sourcePixels;
  uint32_t height = 4;
  uint32_t width = 4;
  uint32_t bytesPerComponent = 1;
  uint32_t countComponents = 3;
  uint32_t stride = bytesPerComponent * countComponents;
  uint32_t pitch = width * stride;
  sourcePixels.data = new byte[pitch * height];
  sourcePixels.len = 0;
  sourcePixels.width = width;
  sourcePixels.pitch = width * stride;
  sourcePixels.height = height;
  sourcePixels.stride = stride;
  sourcePixels.bpc = bytesPerComponent;
  sourcePixels.buf1010102 = false;
  sourcePixels.buf565 = false;
  sourcePixels.buf5551 = false;
  sourcePixels.bgra = false;
  sourcePixels.pitch_requirement = width;
  sourcePixels.max_width = width;

  byte *source = (byte *)sourcePixels.data;
  for(uint32_t y = 0; y < height; ++y)
  {
    for(uint32_t x = 0; x < width; ++x)
    {
      byte *src = &source[stride * x + pitch * y];
      src[0] = (byte)(y + x);
      src[1] = (byte)(y + x * 2);
      src[2] = (byte)(y + x * 3);
    }
  }

  RDCThumb thumbOutYNotFlipped;
  sourcePixels.is_y_flipped = false;
  RenderDoc::Inst().ResamplePixels(sourcePixels, thumbOutYNotFlipped);
  CHECK(thumbOutYNotFlipped.width == width);
  CHECK(thumbOutYNotFlipped.height == height);

  byte *dest = (byte *)thumbOutYNotFlipped.pixels.data();
  for(uint32_t y = 0; y < height; ++y)
  {
    for(uint32_t x = 0; x < width; ++x)
    {
      byte *src = &source[stride * x + pitch * y];
      byte *dst = &dest[stride * x + pitch * (height - y - 1)];
      CHECK((uint32_t)src[0] == (uint32_t)dst[0]);
      CHECK((uint32_t)src[1] == (uint32_t)dst[1]);
      CHECK((uint32_t)src[2] == (uint32_t)dst[2]);
    }
  }

  RDCThumb thumbOutYFlipped;
  sourcePixels.is_y_flipped = true;
  RenderDoc::Inst().ResamplePixels(sourcePixels, thumbOutYFlipped);
  CHECK(thumbOutYFlipped.width == width);
  CHECK(thumbOutYFlipped.height == height);
  dest = (byte *)thumbOutYFlipped.pixels.data();
  for(uint32_t y = 0; y < height; ++y)
  {
    for(uint32_t x = 0; x < width; ++x)
    {
      byte *src = &source[stride * x + pitch * y];
      byte *dst = &dest[stride * x + pitch * y];
      CHECK((uint32_t)src[0] == (uint32_t)dst[0]);
      CHECK((uint32_t)src[1] == (uint32_t)dst[1]);
      CHECK((uint32_t)src[2] == (uint32_t)dst[2]);
    }
  }

  RDCThumb thumbOutBGRA;
  sourcePixels.bgra = true;
  RenderDoc::Inst().ResamplePixels(sourcePixels, thumbOutBGRA);
  CHECK(thumbOutBGRA.width == width);
  CHECK(thumbOutBGRA.height == height);
  dest = (byte *)thumbOutBGRA.pixels.data();
  for(uint32_t y = 0; y < height; ++y)
  {
    for(uint32_t x = 0; x < width; ++x)
    {
      byte *src = &source[stride * x + pitch * y];
      byte *dst = &dest[stride * x + pitch * y];
      CHECK((uint32_t)src[0] == (uint32_t)dst[2]);
      CHECK((uint32_t)src[1] == (uint32_t)dst[1]);
      CHECK((uint32_t)src[2] == (uint32_t)dst[0]);
    }
  }

  RDCThumb thumbOutDownsample;
  sourcePixels.bgra = false;
  sourcePixels.max_width = 2;
  sourcePixels.pitch_requirement = 2;
  RenderDoc::Inst().ResamplePixels(sourcePixels, thumbOutDownsample);
  CHECK(thumbOutDownsample.width == 2);
  CHECK(thumbOutDownsample.height == 2);
  dest = (byte *)thumbOutDownsample.pixels.data();
  for(uint32_t y = 0; y < 2; ++y)
  {
    for(uint32_t x = 0; x < 2; ++x)
    {
      byte *src = &source[stride * x * 2 + pitch * y * 2];
      byte *dst = &dest[stride * x + pitch / 2 * y];
      CHECK((uint32_t)src[0] == (uint32_t)dst[0]);
      CHECK((uint32_t)src[1] == (uint32_t)dst[1]);
      CHECK((uint32_t)src[2] == (uint32_t)dst[2]);
    }
  }
}

#endif
