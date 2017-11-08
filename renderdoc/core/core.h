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
#include "maths/vec.h"
#include "os/os_specific.h"

using std::string;
using std::vector;
using std::map;
using std::pair;
using std::set;

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

// In most cases you don't need to check these individually, use the utility functions below
// to determine if you're in a capture or replay state. There are utility functions for each
// state as well.
// See the comments on each state to understand their purpose.
enum class CaptureState
{
  // This is the state while the initial load of a capture is happening and the replay is
  // initialising available resources. This is where any heavy one-off analysis can happen like
  // noting down the details of a drawcall, tracking statistics about resource use and drawcall
  // types, and creating resources that will be needed later in ActiveReplaying.
  //
  // After leaving this state, the capture enters ActiveReplaying and remains there until the
  // capture is closed down.
  LoadingReplaying,

  // After loading, this state is used throughout replay. Whether replaying the frame whole or in
  // part this state indicates that replaying is happening for analysis without the heavy-weight
  // loading process.
  ActiveReplaying,

  // This is the state when no processing is happening - either record or replay - apart from
  // serialising the data. Used with a 'virtual' driver to be able to interpret the contents of a
  // frame capture for structured export without needing to have the API initialised.
  //
  // The idea is that the existing serialisation infrastructure for a driver can be used to decode
  // the raw bits and chunks inside a capture without actually having to be able to initialise the
  // API, and the structured data can then be exported to another format.
  StructuredExport,

  // This is the state while injected into a program for capturing, but no frame is actively being
  // captured at present. Immediately after injection this state is active, and only the minimum
  // necessary work happens to prepare for a frame capture at some later point.
  //
  // When a frame capture is triggered, we immediately transition to the ActiveCapturing state
  // below, where we stay until the frame has been successfully captured, then transition back into
  // this state to continue capturing necessary work in the background for further frame captures.
  BackgroundCapturing,

  // This is the state while injected into a program for capturing and a frame capture is actively
  // ongoing. We transition into this state from BackgroundCapturing on frame capture begin, then
  // stay here until the frame capture is complete and transition back.
  //
  // Note: This state is entered into immediately when a capture is triggered, so it doesn't imply
  // anything about where in the frame we are.
  ActiveCapturing,
};

constexpr inline bool IsReplayMode(CaptureState state)
{
  return state == CaptureState::LoadingReplaying || state == CaptureState::ActiveReplaying;
}

constexpr inline bool IsCaptureMode(CaptureState state)
{
  return state == CaptureState::BackgroundCapturing || state == CaptureState::ActiveCapturing;
}

constexpr inline bool IsLoading(CaptureState state)
{
  return state == CaptureState::LoadingReplaying;
}

constexpr inline bool IsActiveReplaying(CaptureState state)
{
  return state == CaptureState::ActiveReplaying;
}

constexpr inline bool IsBackgroundCapturing(CaptureState state)
{
  return state == CaptureState::BackgroundCapturing;
}

constexpr inline bool IsActiveCapturing(CaptureState state)
{
  return state == CaptureState::ActiveCapturing;
}

constexpr inline bool IsStructuredExporting(CaptureState state)
{
  return state == CaptureState::StructuredExport;
}

enum class SystemChunk : uint32_t
{
  // 0 is reserved as a 'null' chunk that is only for debug
  DriverInit = 1,
  InitialContentsList,
  InitialContents,

  FirstDriverChunk = 1000,
};

DECLARE_REFLECTION_ENUM(SystemChunk);

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
  RDC_MaxBuiltin,
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

DECLARE_REFLECTION_ENUM(RDCDriver);

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

DECLARE_REFLECTION_ENUM(ReplayLogType);

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

class StreamReader;
class RDCFile;

class RDCFile;

typedef ReplayStatus (*RemoteDriverProvider)(RDCFile *rdc, IRemoteDriver **driver);
typedef ReplayStatus (*ReplayDriverProvider)(RDCFile *rdc, IReplayDriver **driver);

typedef void (*StructuredProcessor)(RDCFile *rdc, SDFile &structData);

typedef ReplayStatus (*CaptureImporter)(const char *filename, StreamReader &reader, RDCFile *rdc,
                                        SDFile &structData);
typedef ReplayStatus (*CaptureExporter)(const char *filename, const RDCFile &rdc,
                                        const SDFile &structData);

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

  uint64_t GetMicrosecondTimestamp() { return uint64_t(m_Timer.GetMicroseconds()); }
  const GlobalEnvironment GetGlobalEnvironment() { return m_GlobalEnv; }
  void ProcessGlobalEnvironment(GlobalEnvironment env, const std::vector<std::string> &args);

  void RegisterShutdownFunction(ShutdownFunction func) { m_ShutdownFunctions.insert(func); }
  void SetReplayApp(bool replay) { m_Replay = replay; }
  bool IsReplayApp() const { return m_Replay; }
  const string &GetConfigSetting(string name) { return m_ConfigSettings[name]; }
  void SetConfigSetting(string name, string value) { m_ConfigSettings[name] = value; }
  void BecomeRemoteServer(const char *listenhost, uint16_t port, volatile bool &killReplay);

  void SetCaptureOptions(const CaptureOptions &opts);
  const CaptureOptions &GetCaptureOptions() const { return m_Options; }
  void RecreateCrashHandler();
  void UnloadCrashHandler();
  ICrashHandler *GetCrashHandler() const { return m_ExHandler; }
  RDCFile *CreateRDC(uint32_t frameNum, void *thpixels, size_t thlen, uint16_t thwidth,
                     uint16_t thheight);
  void FinishCaptureWriting(RDCFile *rdc, uint32_t frameNumber);

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

  void RegisterReplayProvider(RDCDriver driver, const char *name, ReplayDriverProvider provider);
  void RegisterRemoteProvider(RDCDriver driver, const char *name, RemoteDriverProvider provider);

  void RegisterStructuredProcessor(RDCDriver driver, StructuredProcessor provider);

  void RegisterCaptureExporter(const char *filetype, const char *description,
                               CaptureExporter exporter);
  void RegisterCaptureImportExporter(const char *filetype, const char *description,
                                     CaptureImporter importer, CaptureExporter exporter);

  StructuredProcessor GetStructuredProcessor(RDCDriver driver);

  CaptureExporter GetCaptureExporter(const char *filetype);
  CaptureImporter GetCaptureImporter(const char *filetype);

  std::vector<CaptureFileFormat> GetCaptureFileFormats();

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

  Vec4f LightCheckerboardColor() { return m_LightChecker; }
  Vec4f DarkCheckerboardColor() { return m_DarkChecker; }
  void SetLightCheckerboardColor(const Vec4f &col) { m_LightChecker = col; }
  void SetDarkCheckerboardColor(const Vec4f &col) { m_DarkChecker = col; }
  bool IsDarkTheme() { return m_DarkTheme; }
  void SetDarkTheme(bool dark) { m_DarkTheme = dark; }
  ReplayStatus CreateProxyReplayDriver(RDCDriver proxyDriver, IReplayDriver **driver);
  ReplayStatus CreateReplayDriver(RDCFile *rdc, IReplayDriver **driver);
  ReplayStatus CreateRemoteDriver(RDCFile *rdc, IRemoteDriver **driver);

  bool HasReplaySupport(RDCDriver driverType);

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

  std::map<RDCDriver, StructuredProcessor> m_StructProcesssors;

  std::map<std::string, std::string> m_ImportExportFormats;
  std::map<std::string, CaptureImporter> m_Importers;
  std::map<std::string, CaptureExporter> m_Exporters;

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

  Vec4f m_LightChecker = Vec4f(0.81f, 0.81f, 0.81f, 1.0f);
  Vec4f m_DarkChecker = Vec4f(0.57f, 0.57f, 0.57f, 1.0f);
  bool m_DarkTheme = false;

  int m_CapturesActive;

  map<DeviceWnd, FrameCap> m_WindowFrameCapturers;
  DeviceWnd m_ActiveWindow;
  map<void *, IFrameCapturer *> m_DeviceFrameCapturers;

  IFrameCapturer *MatchFrameCapturer(void *dev, void *wnd);

  volatile bool m_TargetControlThreadShutdown;
  volatile bool m_ControlClientThreadShutdown;
  Threading::CriticalSection m_SingleClientLock;
  string m_SingleClientName;

  PerformanceTimer m_Timer;

  static void TargetControlServerThread(Network::Socket *sock);
  static void TargetControlClientThread(Network::Socket *client);

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

struct StructuredProcessRegistration
{
  StructuredProcessRegistration(RDCDriver driver, StructuredProcessor provider)
  {
    RenderDoc::Inst().RegisterStructuredProcessor(driver, provider);
  }
};

struct ConversionRegistration
{
  ConversionRegistration(const char *filetype, const char *description, CaptureImporter importer,
                         CaptureExporter exporter)
  {
    RenderDoc::Inst().RegisterCaptureImportExporter(filetype, description, importer, exporter);
  }
  ConversionRegistration(const char *filetype, const char *description, CaptureExporter exporter)
  {
    RenderDoc::Inst().RegisterCaptureExporter(filetype, description, exporter);
  }
};