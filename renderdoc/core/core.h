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

#pragma once

#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "api/app/renderdoc_app.h"
#include "api/replay/renderdoc_replay.h"
#include "common/threading.h"
#include "common/timing.h"
#include "os/os_specific.h"

using std::string;
using std::vector;
using std::map;
using std::pair;
using std::set;

class Serialiser;
class Chunk;

// not provided by tinyexr, just do by hand
bool is_exr_file(FILE *f);

struct ICrashHandler
{
  virtual ~ICrashHandler() {}
  virtual void WriteMinidump() = 0;
  virtual void WriteMinidump(void *data) = 0;

  virtual void RegisterMemoryRegion(void *mem, size_t size) = 0;
  virtual void UnregisterMemoryRegion(void *mem) = 0;
};

struct IFrameCapturer
{
  virtual void StartFrameCapture(void *dev, void *wnd) = 0;
  virtual bool EndFrameCapture(void *dev, void *wnd) = 0;
};

// READING and EXECUTING are replay states.
// WRITING_IDLE and WRITING_CAPFRAME are capture states.
// WRITING isn't actually a state, it's just a midpoint in the enum,
// so it takes fewer characters to check which state we're in.
//
// on replay, m_State < WRITING is the same as
//(m_State == READING || m_State == EXECUTING)
//
// on capture, m_State >= WRITING is the same as
//(m_State == WRITING_IDLE || m_State == WRITING_CAPFRAME)
enum LogState
{
  READING = 0,
  EXECUTING,
  WRITING,
  WRITING_IDLE,
  WRITING_CAPFRAME,
};

enum SystemChunks
{
  // 0 is reserved as a 'null' chunk that is only for debug
  CREATE_PARAMS = 1,
  THUMBNAIL_DATA,
  DRIVER_INIT_PARAMS,
  INITIAL_CONTENTS,

  FIRST_CHUNK_ID,
};

enum RDCDriver
{
  RDC_Unknown = 0,
  RDC_D3D11 = 1,
  RDC_OpenGL = 2,
  RDC_Mantle = 3,
  RDC_D3D12 = 4,
  RDC_D3D10 = 5,
  RDC_D3D9 = 6,
  RDC_Image = 7,
  RDC_Vulkan = 8,
  RDC_OpenGLES = 9,
  RDC_D3D8 = 10,
  RDC_Custom = 100000,
  RDC_Custom0 = RDC_Custom,
  RDC_Custom1,
  RDC_Custom2,
  RDC_Custom3,
  RDC_Custom4,
  RDC_Custom5,
  RDC_Custom6,
  RDC_Custom7,
  RDC_Custom8,
  RDC_Custom9,
};

typedef uint32_t bool32;

namespace DXBC
{
class DXBCFile;
}
namespace Callstack
{
class StackResolver;
}

enum ReplayLogType
{
  eReplay_Full,
  eReplay_WithoutDraw,
  eReplay_OnlyDraw,
};

struct RDCInitParams
{
  RDCInitParams()
  {
    m_State = WRITING;
    m_pSerialiser = NULL;
  }
  virtual ~RDCInitParams() {}
  virtual ReplayStatus Serialise() = 0;

  LogState m_State;
  Serialiser *m_pSerialiser;

  Serialiser *GetSerialiser() { return m_pSerialiser; }
};

struct CaptureData
{
  CaptureData(string p, uint64_t t, uint32_t f)
      : path(p), timestamp(t), frameNumber(f), retrieved(false)
  {
  }
  string path;
  uint64_t timestamp;
  uint32_t frameNumber;
  bool retrieved;
};

enum LoadProgressSection
{
  DebugManagerInit,
  FileInitialRead,
  FrameEventsRead,
  NumSections,
};

class IRemoteDriver;
class IReplayDriver;

typedef ReplayStatus (*RemoteDriverProvider)(const char *logfile, IRemoteDriver **driver);
typedef ReplayStatus (*ReplayDriverProvider)(const char *logfile, IReplayDriver **driver);

typedef bool (*VulkanLayerCheck)(VulkanLayerFlags &flags, std::vector<std::string> &myJSONs,
                                 std::vector<std::string> &otherJSONs);
typedef void (*VulkanLayerInstall)(bool systemLevel);

typedef void (*ShutdownFunction)();

// this class mediates everything and owns any 'global' resources such as the crash handler.
//
// It acts as a central hub that registers any driver providers and can be asked to create one
// for a given logfile or type.
class RenderDoc
{
public:
  static RenderDoc &Inst();

  void SetProgressPtr(float *progress) { m_ProgressPtr = progress; }
  void SetProgress(LoadProgressSection section, float delta);

  // set from outside of the device creation interface
  void SetLogFile(const char *logFile);
  const char *GetLogFile() const { return m_LogFile.c_str(); }
  const char *GetCurrentTarget() const { return m_Target.c_str(); }
  void Initialise();
  void Shutdown();

  const GlobalEnvironment GetGlobalEnvironment() { return m_GlobalEnv; }
  void ProcessGlobalEnvironment(GlobalEnvironment env, const std::vector<std::string> &args);

  void RegisterShutdownFunction(ShutdownFunction func) { m_ShutdownFunctions.insert(func); }
  void SetReplayApp(bool replay) { m_Replay = replay; }
  bool IsReplayApp() const { return m_Replay; }
  const string &GetConfigSetting(string name) { return m_ConfigSettings[name]; }
  void SetConfigSetting(string name, string value) { m_ConfigSettings[name] = value; }
  void BecomeRemoteServer(const char *listenhost, uint16_t port, volatile uint32_t &killReplay);

  void SetCaptureOptions(const CaptureOptions &opts);
  const CaptureOptions &GetCaptureOptions() const { return m_Options; }
  void RecreateCrashHandler();
  void UnloadCrashHandler();
  ICrashHandler *GetCrashHandler() const { return m_ExHandler; }
  Serialiser *OpenWriteSerialiser(uint32_t frameNum, RDCInitParams *params, void *thpixels,
                                  size_t thlen, uint32_t thwidth, uint32_t thheight);
  void SuccessfullyWrittenLog(uint32_t frameNumber);

  void AddChildProcess(uint32_t pid, uint32_t ident)
  {
    SCOPED_LOCK(m_ChildLock);
    m_Children.push_back(std::make_pair(pid, ident));
  }
  vector<pair<uint32_t, uint32_t> > GetChildProcesses()
  {
    SCOPED_LOCK(m_ChildLock);
    return m_Children;
  }

  vector<CaptureData> GetCaptures()
  {
    SCOPED_LOCK(m_CaptureLock);
    return m_Captures;
  }

  void MarkCaptureRetrieved(uint32_t idx)
  {
    SCOPED_LOCK(m_CaptureLock);
    if(idx < m_Captures.size())
    {
      m_Captures[idx].retrieved = true;
    }
  }

  ReplayStatus FillInitParams(const char *logfile, RDCDriver &driverType, string &driverName,
                              uint64_t &fileMachineIdent, RDCInitParams *params);

  void RegisterReplayProvider(RDCDriver driver, const char *name, ReplayDriverProvider provider);
  void RegisterRemoteProvider(RDCDriver driver, const char *name, RemoteDriverProvider provider);

  void SetVulkanLayerCheck(VulkanLayerCheck callback) { m_VulkanCheck = callback; }
  void SetVulkanLayerInstall(VulkanLayerInstall callback) { m_VulkanInstall = callback; }
  bool NeedVulkanLayerRegistration(VulkanLayerFlags &flags, std::vector<std::string> &myJSONs,
                                   std::vector<std::string> &otherJSONs)
  {
    if(m_VulkanCheck)
      return m_VulkanCheck(flags, myJSONs, otherJSONs);

    return false;
  }

  void UpdateVulkanLayerRegistration(bool systemLevel)
  {
    if(m_VulkanInstall)
      m_VulkanInstall(systemLevel);
  }

  ReplayStatus CreateReplayDriver(RDCDriver driverType, const char *logfile, IReplayDriver **driver);
  ReplayStatus CreateRemoteDriver(RDCDriver driverType, const char *logfile, IRemoteDriver **driver);

  map<RDCDriver, string> GetReplayDrivers();
  map<RDCDriver, string> GetRemoteDrivers();

  bool HasReplayDriver(RDCDriver driver) const;
  bool HasRemoteDriver(RDCDriver driver) const;

  void SetCurrentDriver(RDCDriver driver);
  void GetCurrentDriver(RDCDriver &driver, string &name);

  uint32_t GetTargetControlIdent() const { return m_RemoteIdent; }
  bool IsTargetControlConnected();
  string GetTargetControlUsername();

  void Tick();

  void AddFrameCapturer(void *dev, void *wnd, IFrameCapturer *cap);
  void RemoveFrameCapturer(void *dev, void *wnd);

  // add window-less frame capturers for use via users capturing
  // manually through the renderdoc API with NULL device/window handles
  void AddDeviceFrameCapturer(void *dev, IFrameCapturer *cap);
  void RemoveDeviceFrameCapturer(void *dev);

  void StartFrameCapture(void *dev, void *wnd);
  bool IsFrameCapturing() { return m_CapturesActive > 0; }
  void SetActiveWindow(void *dev, void *wnd);
  bool EndFrameCapture(void *dev, void *wnd);

  bool MatchClosestWindow(void *&dev, void *&wnd);

  bool IsActiveWindow(void *dev, void *wnd)
  {
    return dev == m_ActiveWindow.dev && wnd == m_ActiveWindow.wnd;
  }

  void TriggerCapture(uint32_t numFrames) { m_Cap = numFrames; }
  uint32_t GetOverlayBits() { return m_Overlay; }
  void MaskOverlayBits(uint32_t And, uint32_t Or) { m_Overlay = (m_Overlay & And) | Or; }
  void QueueCapture(uint32_t frameNumber) { m_QueuedFrameCaptures.insert(frameNumber); }
  void SetFocusKeys(RENDERDOC_InputButton *keys, int num)
  {
    m_FocusKeys.resize(num);
    for(int i = 0; i < num && keys; i++)
      m_FocusKeys[i] = keys[i];
  }
  void SetCaptureKeys(RENDERDOC_InputButton *keys, int num)
  {
    m_CaptureKeys.resize(num);
    for(int i = 0; i < num && keys; i++)
      m_CaptureKeys[i] = keys[i];
  }

  const vector<RENDERDOC_InputButton> &GetFocusKeys() { return m_FocusKeys; }
  const vector<RENDERDOC_InputButton> &GetCaptureKeys() { return m_CaptureKeys; }
  bool ShouldTriggerCapture(uint32_t frameNumber);

  enum
  {
    eOverlay_ActiveWindow = 0x1,
    eOverlay_CaptureDisabled = 0x2,
  };

  string GetOverlayText(RDCDriver driver, uint32_t frameNumber, int flags);

private:
  RenderDoc();
  ~RenderDoc();

  static RenderDoc *m_Inst;

  bool m_Replay;

  uint32_t m_Cap;

  vector<RENDERDOC_InputButton> m_FocusKeys;
  vector<RENDERDOC_InputButton> m_CaptureKeys;

  GlobalEnvironment m_GlobalEnv;

  FrameTimer m_FrameTimer;

  string m_LoggingFilename;

  string m_Target;
  string m_LogFile;
  string m_CurrentLogFile;
  CaptureOptions m_Options;
  uint32_t m_Overlay;

  set<uint32_t> m_QueuedFrameCaptures;

  uint32_t m_RemoteIdent;
  Threading::ThreadHandle m_RemoteThread;

  int32_t m_MarkerIndentLevel;
  RDCDriver m_CurrentDriver;
  string m_CurrentDriverName;

  float *m_ProgressPtr;

  Threading::CriticalSection m_CaptureLock;
  vector<CaptureData> m_Captures;

  Threading::CriticalSection m_ChildLock;
  vector<pair<uint32_t, uint32_t> > m_Children;

  map<string, string> m_ConfigSettings;

  map<RDCDriver, string> m_DriverNames;
  map<RDCDriver, ReplayDriverProvider> m_ReplayDriverProviders;
  map<RDCDriver, RemoteDriverProvider> m_RemoteDriverProviders;

  VulkanLayerCheck m_VulkanCheck;
  VulkanLayerInstall m_VulkanInstall;

  set<ShutdownFunction> m_ShutdownFunctions;

  struct FrameCap
  {
    FrameCap() : FrameCapturer(NULL), RefCount(1) {}
    IFrameCapturer *FrameCapturer;
    int RefCount;
  };

  struct DeviceWnd
  {
    DeviceWnd() : dev(NULL), wnd(NULL) {}
    DeviceWnd(void *d, void *w) : dev(d), wnd(w) {}
    void *dev;
    void *wnd;

    bool operator==(const DeviceWnd &o) const { return dev == o.dev && wnd == o.wnd; }
    bool operator<(const DeviceWnd &o) const
    {
      if(dev != o.dev)
        return dev < o.dev;
      return wnd < o.wnd;
    }

    bool wildcardMatch(const DeviceWnd &o) const
    {
      if(dev == NULL || o.dev == NULL)
        return wnd == NULL || o.wnd == NULL || wnd == o.wnd;

      if(wnd == NULL || o.wnd == NULL)
        return dev == NULL || o.dev == NULL || dev == o.dev;

      return *this == o;
    }
  };

  int m_CapturesActive;

  map<DeviceWnd, FrameCap> m_WindowFrameCapturers;
  DeviceWnd m_ActiveWindow;
  map<void *, IFrameCapturer *> m_DeviceFrameCapturers;

  IFrameCapturer *MatchFrameCapturer(void *dev, void *wnd);

  volatile bool m_TargetControlThreadShutdown;
  volatile bool m_ControlClientThreadShutdown;
  Threading::CriticalSection m_SingleClientLock;
  string m_SingleClientName;

  static void TargetControlServerThread(void *s);
  static void TargetControlClientThread(void *s);

  ICrashHandler *m_ExHandler;
};

struct DriverRegistration
{
  DriverRegistration(RDCDriver driver, const char *name, ReplayDriverProvider provider)
  {
    RenderDoc::Inst().RegisterReplayProvider(driver, name, provider);
  }
  DriverRegistration(RDCDriver driver, const char *name, RemoteDriverProvider provider)
  {
    RenderDoc::Inst().RegisterRemoteProvider(driver, name, provider);
  }
};
