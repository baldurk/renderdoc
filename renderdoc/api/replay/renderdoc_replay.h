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

template <typename T>
inline const char *TypeName();

#define DECLARE_REFLECTION_STRUCT(type) \
  template <>                           \
  inline const char *TypeName<type>()   \
  {                                     \
    return #type;                       \
  }

typedef uint8_t byte;
typedef uint32_t bool32;

#ifndef DOCUMENT
#define DOCUMENT(text)
#endif

#if defined(RENDERDOC_PLATFORM_WIN32)

#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API __declspec(dllexport)
#else
#define RENDERDOC_API __declspec(dllimport)
#endif
#define RENDERDOC_CC __cdecl

#elif defined(RENDERDOC_PLATFORM_LINUX) || defined(RENDERDOC_PLATFORM_APPLE) || \
    defined(RENDERDOC_PLATFORM_ANDROID)

#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API __attribute__((visibility("default")))
#else
#define RENDERDOC_API
#endif

#define RENDERDOC_CC

#else

#error "Unknown platform"

#endif

// windowing structures

#if defined(RENDERDOC_PLATFORM_WIN32)

// Win32 uses HWND

#endif

#if defined(RENDERDOC_WINDOWING_XLIB)

// can't include xlib.h here as it defines a ton of crap like None
// and Bool etc which can interfere with other headers
typedef struct _XDisplay Display;
typedef unsigned long Drawable;

struct XlibWindowData
{
  Display *display;
  Drawable window;
};

#endif

#if defined(RENDERDOC_WINDOWING_XCB)

struct xcb_connection_t;
typedef uint32_t xcb_window_t;

struct XCBWindowData
{
  xcb_connection_t *connection;
  xcb_window_t window;
};

#endif

#if defined(RENDERDOC_PLATFORM_ANDROID)

// android uses ANativeWindow*

#endif

enum class WindowingSystem : uint32_t
{
  Unknown,
  Win32,
  Xlib,
  XCB,
  Android,
};

// needs to be declared up here for reference in basic_types

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeArrayMem(const void *mem);
typedef void(RENDERDOC_CC *pRENDERDOC_FreeArrayMem)(const void *mem);

extern "C" RENDERDOC_API void *RENDERDOC_CC RENDERDOC_AllocArrayMem(uint64_t sz);
typedef void *(RENDERDOC_CC *pRENDERDOC_AllocArrayMem)(uint64_t sz);

#include "basic_types.h"

#ifdef RENDERDOC_EXPORTS
struct ResourceId;

namespace ResourceIDGen
{
// the only function allowed access to ResourceId internals, for allocating a new ID
ResourceId GetNewUniqueID();
};
#endif

// We give every resource a globally unique ID so that we can differentiate
// between two textures allocated in the same memory (after the first is freed)
//
// it's a struct around a uint64_t to aid in template selection
struct ResourceId
{
  ResourceId() : id() {}
  inline static ResourceId Null() { return ResourceId(); }
  bool operator==(const ResourceId u) const { return id == u.id; }
  bool operator!=(const ResourceId u) const { return id != u.id; }
  bool operator<(const ResourceId u) const { return id < u.id; }
private:
  uint64_t id;

#ifdef RENDERDOC_EXPORTS
  friend ResourceId ResourceIDGen::GetNewUniqueID();
#endif
};

DECLARE_REFLECTION_STRUCT(ResourceId);

#include "capture_options.h"
#include "control_types.h"
#include "d3d11_pipestate.h"
#include "d3d12_pipestate.h"
#include "data_types.h"
#include "gl_pipestate.h"
#include "replay_enums.h"
#include "shader_types.h"
#include "vk_pipestate.h"

struct IReplayOutput
{
  virtual bool SetTextureDisplay(const TextureDisplay &o) = 0;
  virtual bool SetMeshDisplay(const MeshDisplay &o) = 0;

  virtual bool ClearThumbnails() = 0;
  virtual bool AddThumbnail(WindowingSystem system, void *data, ResourceId texID,
                            CompType typeHint) = 0;

  virtual bool Display() = 0;

  virtual bool SetPixelContext(WindowingSystem system, void *data) = 0;
  virtual bool SetPixelContextLocation(uint32_t x, uint32_t y) = 0;
  virtual void DisablePixelContext() = 0;

  virtual bool GetMinMax(PixelValue *minval, PixelValue *maxval) = 0;
  virtual bool GetHistogram(float minval, float maxval, bool channels[4],
                            rdctype::array<uint32_t> *histogram) = 0;

  virtual ResourceId GetCustomShaderTexID() = 0;
  virtual bool PickPixel(ResourceId texID, bool customShader, uint32_t x, uint32_t y,
                         uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *val) = 0;
  virtual uint32_t PickVertex(uint32_t eventID, uint32_t x, uint32_t y, uint32_t *pickedInstance) = 0;

protected:
  IReplayOutput() = default;
  ~IReplayOutput() = default;
};

struct IReplayRenderer
{
  virtual APIProperties GetAPIProperties() = 0;

  virtual void GetSupportedWindowSystems(rdctype::array<WindowingSystem> *systems) = 0;

  virtual IReplayOutput *CreateOutput(WindowingSystem system, void *data, ReplayOutputType type) = 0;
  virtual void Shutdown() = 0;
  virtual void ShutdownOutput(IReplayOutput *output) = 0;

  virtual void FileChanged() = 0;

  virtual bool HasCallstacks() = 0;
  virtual bool InitResolver() = 0;

  virtual bool SetFrameEvent(uint32_t eventID, bool force) = 0;
  virtual bool GetD3D11PipelineState(D3D11Pipe::State *state) = 0;
  virtual bool GetD3D12PipelineState(D3D12Pipe::State *state) = 0;
  virtual bool GetGLPipelineState(GLPipe::State *state) = 0;
  virtual bool GetVulkanPipelineState(VKPipe::State *state) = 0;

  virtual ResourceId BuildCustomShader(const char *entry, const char *source,
                                       const uint32_t compileFlags, ShaderStage type,
                                       rdctype::str *errors) = 0;
  virtual bool FreeCustomShader(ResourceId id) = 0;

  virtual ResourceId BuildTargetShader(const char *entry, const char *source,
                                       const uint32_t compileFlags, ShaderStage type,
                                       rdctype::str *errors) = 0;
  virtual bool ReplaceResource(ResourceId from, ResourceId to) = 0;
  virtual bool RemoveReplacement(ResourceId id) = 0;
  virtual bool FreeTargetResource(ResourceId id) = 0;

  virtual bool GetFrameInfo(FrameDescription *frame) = 0;
  virtual bool GetDrawcalls(rdctype::array<DrawcallDescription> *draws) = 0;
  virtual bool FetchCounters(GPUCounter *counters, uint32_t numCounters,
                             rdctype::array<CounterResult> *results) = 0;
  virtual bool EnumerateCounters(rdctype::array<GPUCounter> *counters) = 0;
  virtual bool DescribeCounter(GPUCounter counterID, CounterDescription *desc) = 0;
  virtual bool GetTextures(rdctype::array<TextureDescription> *texs) = 0;
  virtual bool GetBuffers(rdctype::array<BufferDescription> *bufs) = 0;
  virtual bool GetResolve(uint64_t *callstack, uint32_t callstackLen,
                          rdctype::array<rdctype::str> *trace) = 0;
  virtual bool GetDebugMessages(rdctype::array<DebugMessage> *msgs) = 0;

  virtual bool PixelHistory(ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip,
                            uint32_t sampleIdx, CompType typeHint,
                            rdctype::array<PixelModification> *history) = 0;
  virtual bool DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset,
                           uint32_t vertOffset, ShaderDebugTrace *trace) = 0;
  virtual bool DebugPixel(uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive,
                          ShaderDebugTrace *trace) = 0;
  virtual bool DebugThread(uint32_t groupid[3], uint32_t threadid[3], ShaderDebugTrace *trace) = 0;

  virtual bool GetUsage(ResourceId id, rdctype::array<EventUsage> *usage) = 0;

  virtual bool GetCBufferVariableContents(ResourceId shader, const char *entryPoint,
                                          uint32_t cbufslot, ResourceId buffer, uint64_t offs,
                                          rdctype::array<ShaderVariable> *vars) = 0;

  virtual bool SaveTexture(const TextureSave &saveData, const char *path) = 0;

  virtual bool GetPostVSData(uint32_t instID, MeshDataStage stage, MeshFormat *data) = 0;

  virtual bool GetBufferData(ResourceId buff, uint64_t offset, uint64_t len,
                             rdctype::array<byte> *data) = 0;
  virtual bool GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                              rdctype::array<byte> *data) = 0;

protected:
  IReplayRenderer() = default;
  ~IReplayRenderer() = default;
};

DECLARE_REFLECTION_STRUCT(IReplayRenderer);

struct ITargetControl
{
  virtual void Shutdown() = 0;

  virtual bool Connected() = 0;

  virtual const char *GetTarget() = 0;
  virtual const char *GetAPI() = 0;
  virtual uint32_t GetPID() = 0;
  virtual const char *GetBusyClient() = 0;

  virtual void TriggerCapture(uint32_t numFrames) = 0;
  virtual void QueueCapture(uint32_t frameNumber) = 0;
  virtual void CopyCapture(uint32_t remoteID, const char *localpath) = 0;
  virtual void DeleteCapture(uint32_t remoteID) = 0;

  virtual void ReceiveMessage(TargetControlMessage *msg) = 0;

protected:
  ITargetControl() = default;
  ~ITargetControl() = default;
};

struct IRemoteServer
{
  virtual void ShutdownConnection() = 0;

  virtual void ShutdownServerAndConnection() = 0;

  virtual bool Ping() = 0;

  virtual bool LocalProxies(rdctype::array<rdctype::str> *out) = 0;
  virtual bool RemoteSupportedReplays(rdctype::array<rdctype::str> *out) = 0;

  virtual rdctype::str GetHomeFolder() = 0;
  virtual rdctype::array<PathEntry> ListFolder(const char *path) = 0;

  virtual uint32_t ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
                                    void *env, const CaptureOptions *opts) = 0;

  virtual void TakeOwnershipCapture(const char *filename) = 0;
  virtual rdctype::str CopyCaptureToRemote(const char *filename, float *progress) = 0;
  virtual void CopyCaptureFromRemote(const char *remotepath, const char *localpath,
                                     float *progress) = 0;

  virtual ReplayStatus OpenCapture(uint32_t proxyid, const char *logfile, float *progress,
                                   IReplayRenderer **rend) = 0;
  virtual void CloseCapture(IReplayRenderer *rend) = 0;

protected:
  IRemoteServer() = default;
  ~IRemoteServer() = default;
};

//////////////////////////////////////////////////////////////////////////
// camera
//////////////////////////////////////////////////////////////////////////

class Camera;

extern "C" RENDERDOC_API Camera *RENDERDOC_CC Camera_InitArcball();
extern "C" RENDERDOC_API Camera *RENDERDOC_CC Camera_InitFPSLook();
extern "C" RENDERDOC_API void RENDERDOC_CC Camera_Shutdown(Camera *c);

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_SetPosition(Camera *c, float x, float y, float z);

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_SetFPSRotation(Camera *c, float x, float y,
                                                                 float z);

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_SetArcballDistance(Camera *c, float dist);
extern "C" RENDERDOC_API void RENDERDOC_CC Camera_ResetArcball(Camera *c);
extern "C" RENDERDOC_API void RENDERDOC_CC Camera_RotateArcball(Camera *c, float ax, float ay,
                                                                float bx, float by);

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_GetBasis(Camera *c, FloatVector *pos,
                                                           FloatVector *fwd, FloatVector *right,
                                                           FloatVector *up);

//////////////////////////////////////////////////////////////////////////
// Maths/format/misc related exports
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API float RENDERDOC_CC Maths_HalfToFloat(uint16_t half);
extern "C" RENDERDOC_API uint16_t RENDERDOC_CC Maths_FloatToHalf(float f);

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC Topology_NumVerticesPerPrimitive(Topology topology);
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC Topology_VertexOffset(Topology topology,
                                                                     uint32_t primitive);

//////////////////////////////////////////////////////////////////////////
// Create a replay renderer, for playback and analysis.
//
// Takes the filename of the log. Returns NULL in the case of any error.
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API ReplaySupport RENDERDOC_CC RENDERDOC_SupportLocalReplay(
    const char *logfile, rdctype::str *driverName, rdctype::str *recordMachineIdent);
extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC
RENDERDOC_CreateReplayRenderer(const char *logfile, float *progress, IReplayRenderer **rend);

//////////////////////////////////////////////////////////////////////////
// Target Control
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API ITargetControl *RENDERDOC_CC RENDERDOC_CreateTargetControl(
    const char *host, uint32_t ident, const char *clientName, bool32 forceConnection);
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_EnumerateRemoteTargets(const char *host,
                                                                                uint32_t nextIdent);

//////////////////////////////////////////////////////////////////////////
// Remote server
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_GetDefaultRemoteServerPort();
extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC
RENDERDOC_CreateRemoteServerConnection(const char *host, uint32_t port, IRemoteServer **rend);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_BecomeRemoteServer(const char *listenhost,
                                                                        uint32_t port,
                                                                        volatile bool32 *killReplay);

//////////////////////////////////////////////////////////////////////////
// Injection/execution capture functions.
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetDefaultCaptureOptions(CaptureOptions *opts);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartGlobalHook(const char *pathmatch,
                                                                     const char *logfile,
                                                                     const CaptureOptions *opts);
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC
RENDERDOC_ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine, void *env,
                           const char *logfile, const CaptureOptions *opts, bool32 waitForExit);
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_InjectIntoProcess(
    uint32_t pid, void *env, const char *logfile, const CaptureOptions *opts, bool32 waitForExit);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartSelfHostCapture(const char *dllname);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EndSelfHostCapture(const char *dllname);

//////////////////////////////////////////////////////////////////////////
// Vulkan layer handling
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API bool RENDERDOC_CC
RENDERDOC_NeedVulkanLayerRegistration(VulkanLayerFlags *flags, rdctype::array<rdctype::str> *myJSONs,
                                      rdctype::array<rdctype::str> *otherJSONs);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateVulkanLayerRegistration(bool systemLevel);

//////////////////////////////////////////////////////////////////////////
// Miscellaneous!
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_TriggerExceptionHandler(void *exceptionPtrs,
                                                                             bool32 crashed);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetDebugLogFile(const char *filename);
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetLogFile();
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogText(const char *text);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogMessage(LogType type, const char *project,
                                                                const char *file, unsigned int line,
                                                                const char *text);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC RENDERDOC_GetThumbnail(const char *filename,
                                                                    FileType type, uint32_t maxsize,
                                                                    rdctype::array<byte> *buf);
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetVersionString();
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetCommitHash();
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetConfigSetting(const char *name);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetConfigSetting(const char *name,
                                                                      const char *value);

extern "C" RENDERDOC_API void *RENDERDOC_CC RENDERDOC_MakeEnvironmentModificationList(int numElems);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetEnvironmentModification(
    void *mem, int idx, const char *variable, const char *value, EnvMod type, EnvSep separator);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeEnvironmentModificationList(void *mem);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EnumerateAndroidDevices(rdctype::str *deviceList);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartAndroidRemoteServer();
