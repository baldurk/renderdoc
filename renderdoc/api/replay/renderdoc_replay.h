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

typedef uint8_t byte;
typedef uint32_t bool32;

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

enum WindowingSystem
{
  eWindowingSystem_Unknown,
  eWindowingSystem_Win32,
  eWindowingSystem_Xlib,
  eWindowingSystem_XCB,
  eWindowingSystem_Android,
};

// needs to be declared up here for reference in basic_types

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeArrayMem(const void *mem);
typedef void(RENDERDOC_CC *pRENDERDOC_FreeArrayMem)(const void *mem);

extern "C" RENDERDOC_API void *RENDERDOC_CC RENDERDOC_AllocArrayMem(uint64_t sz);
typedef void *(RENDERDOC_CC *pRENDERDOC_AllocArrayMem)(uint64_t sz);

#include "basic_types.h"

// We give every resource a globally unique ID so that we can differentiate
// between two textures allocated in the same memory (after the first is freed)
//
// it's a struct around a uint64_t to aid in template selection
struct ResourceId
{
  uint64_t id;

#ifdef __cplusplus
  ResourceId() : id() {}
  ResourceId(uint64_t val, bool) { id = val; }
  bool operator==(const ResourceId u) const { return id == u.id; }
  bool operator!=(const ResourceId u) const { return id != u.id; }
  bool operator<(const ResourceId u) const { return id < u.id; }
#endif
};

#include "capture_options.h"
#include "control_types.h"
#include "d3d11_pipestate.h"
#include "d3d12_pipestate.h"
#include "data_types.h"
#include "gl_pipestate.h"
#include "replay_enums.h"
#include "shader_types.h"
#include "vk_pipestate.h"

// for C++ expose the interface as a virtual interface
#ifdef __cplusplus

struct IReplayOutput
{
  virtual bool SetOutputConfig(const OutputConfig &o) = 0;
  virtual bool SetTextureDisplay(const TextureDisplay &o) = 0;
  virtual bool SetMeshDisplay(const MeshDisplay &o) = 0;

  virtual bool ClearThumbnails() = 0;
  virtual bool AddThumbnail(WindowingSystem system, void *data, ResourceId texID,
                            FormatComponentType typeHint) = 0;

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
  virtual uint32_t PickVertex(uint32_t eventID, uint32_t x, uint32_t y) = 0;
};

#endif

#ifdef RENDERDOC_EXPORTS
struct ReplayOutput;
#else
#ifdef __cplusplus
typedef IReplayOutput ReplayOutput;
#else
struct ReplayOutput
{
};
#endif
#endif

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetOutputConfig(ReplayOutput *output,
                                                                          const OutputConfig &o);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetTextureDisplay(ReplayOutput *output,
                                                                            const TextureDisplay &o);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetMeshDisplay(ReplayOutput *output,
                                                                         const MeshDisplay &o);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_ClearThumbnails(ReplayOutput *output);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_AddThumbnail(ReplayOutput *output,
                                                                       WindowingSystem system,
                                                                       void *data, ResourceId texID,
                                                                       FormatComponentType typeHint);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_Display(ReplayOutput *output);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetPixelContext(ReplayOutput *output,
                                                                          WindowingSystem system,
                                                                          void *data);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayOutput_SetPixelContextLocation(ReplayOutput *output, uint32_t x, uint32_t y);
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_DisablePixelContext(ReplayOutput *output);

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_GetCustomShaderTexID(ReplayOutput *output,
                                                                             ResourceId *id);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_GetMinMax(ReplayOutput *output,
                                                                    PixelValue *minval,
                                                                    PixelValue *maxval);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayOutput_GetHistogram(ReplayOutput *output, float minval, float maxval, bool32 channels[4],
                          rdctype::array<uint32_t> *histogram);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_PickPixel(
    ReplayOutput *output, ResourceId texID, bool32 customShader, uint32_t x, uint32_t y,
    uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *val);
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC ReplayOutput_PickVertex(ReplayOutput *output,
                                                                       uint32_t eventID, uint32_t x,
                                                                       uint32_t y);

// for C++ expose the interface as a virtual interface
#ifdef __cplusplus

struct IReplayRenderer
{
  virtual APIProperties GetAPIProperties() = 0;

  virtual void GetSupportedWindowSystems(rdctype::array<WindowingSystem> *systems) = 0;

  virtual ReplayOutput *CreateOutput(WindowingSystem system, void *data, OutputType type) = 0;
  virtual void Shutdown() = 0;
  virtual void ShutdownOutput(ReplayOutput *output) = 0;

  virtual void FileChanged() = 0;

  virtual bool HasCallstacks() = 0;
  virtual bool InitResolver() = 0;

  virtual bool SetFrameEvent(uint32_t eventID, bool force) = 0;
  virtual bool GetD3D11PipelineState(D3D11PipelineState *state) = 0;
  virtual bool GetD3D12PipelineState(D3D12PipelineState *state) = 0;
  virtual bool GetGLPipelineState(GLPipelineState *state) = 0;
  virtual bool GetVulkanPipelineState(VulkanPipelineState *state) = 0;

  virtual ResourceId BuildCustomShader(const char *entry, const char *source,
                                       const uint32_t compileFlags, ShaderStageType type,
                                       rdctype::str *errors) = 0;
  virtual bool FreeCustomShader(ResourceId id) = 0;

  virtual ResourceId BuildTargetShader(const char *entry, const char *source,
                                       const uint32_t compileFlags, ShaderStageType type,
                                       rdctype::str *errors) = 0;
  virtual bool ReplaceResource(ResourceId from, ResourceId to) = 0;
  virtual bool RemoveReplacement(ResourceId id) = 0;
  virtual bool FreeTargetResource(ResourceId id) = 0;

  virtual bool GetFrameInfo(FetchFrameInfo *frame) = 0;
  virtual bool GetDrawcalls(rdctype::array<FetchDrawcall> *draws) = 0;
  virtual bool FetchCounters(uint32_t *counters, uint32_t numCounters,
                             rdctype::array<CounterResult> *results) = 0;
  virtual bool EnumerateCounters(rdctype::array<uint32_t> *counters) = 0;
  virtual bool DescribeCounter(uint32_t counterID, CounterDescription *desc) = 0;
  virtual bool GetTextures(rdctype::array<FetchTexture> *texs) = 0;
  virtual bool GetBuffers(rdctype::array<FetchBuffer> *bufs) = 0;
  virtual bool GetResolve(uint64_t *callstack, uint32_t callstackLen,
                          rdctype::array<rdctype::str> *trace) = 0;
  virtual bool GetDebugMessages(rdctype::array<DebugMessage> *msgs) = 0;

  virtual bool PixelHistory(ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip,
                            uint32_t sampleIdx, FormatComponentType typeHint,
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
};

#endif

#ifdef RENDERDOC_EXPORTS
struct ReplayRenderer;
#else
#ifdef __cplusplus
typedef IReplayRenderer ReplayRenderer;
#else
struct ReplayRenderer
{
};
#endif
#endif

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_GetAPIProperties(ReplayRenderer *rend,
                                                                           APIProperties *props);

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_GetSupportedWindowSystems(
    ReplayRenderer *rend, rdctype::array<WindowingSystem> *systems);

extern "C" RENDERDOC_API ReplayOutput *RENDERDOC_CC ReplayRenderer_CreateOutput(
    ReplayRenderer *rend, WindowingSystem system, void *data, OutputType type);
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_Shutdown(ReplayRenderer *rend);
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_ShutdownOutput(ReplayRenderer *rend,
                                                                         ReplayOutput *output);

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_FileChanged(ReplayRenderer *rend);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_HasCallstacks(ReplayRenderer *rend);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_InitResolver(ReplayRenderer *rend);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_SetFrameEvent(ReplayRenderer *rend,
                                                                          uint32_t eventID,
                                                                          bool32 force);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetD3D11PipelineState(ReplayRenderer *rend, D3D11PipelineState *state);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetD3D12PipelineState(ReplayRenderer *rend, D3D12PipelineState *state);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetGLPipelineState(ReplayRenderer *rend,
                                                                               GLPipelineState *state);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetVulkanPipelineState(ReplayRenderer *rend, VulkanPipelineState *state);

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_BuildCustomShader(
    ReplayRenderer *rend, const char *entry, const char *source, const uint32_t compileFlags,
    ShaderStageType type, ResourceId *shaderID, rdctype::str *errors);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_FreeCustomShader(ReplayRenderer *rend,
                                                                             ResourceId id);

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_BuildTargetShader(
    ReplayRenderer *rend, const char *entry, const char *source, const uint32_t compileFlags,
    ShaderStageType type, ResourceId *shaderID, rdctype::str *errors);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_ReplaceResource(ReplayRenderer *rend,
                                                                            ResourceId from,
                                                                            ResourceId to);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_RemoveReplacement(ReplayRenderer *rend,
                                                                              ResourceId id);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_FreeTargetResource(ReplayRenderer *rend,
                                                                               ResourceId id);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetFrameInfo(ReplayRenderer *rend,
                                                                         FetchFrameInfo *frame);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetDrawcalls(ReplayRenderer *rend, rdctype::array<FetchDrawcall> *draws);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_FetchCounters(ReplayRenderer *rend, uint32_t *counters, uint32_t numCounters,
                             rdctype::array<CounterResult> *results);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_EnumerateCounters(ReplayRenderer *rend, rdctype::array<uint32_t> *counters);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DescribeCounter(ReplayRenderer *rend,
                                                                            uint32_t counterID,
                                                                            CounterDescription *desc);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetTextures(ReplayRenderer *rend, rdctype::array<FetchTexture> *texs);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetBuffers(ReplayRenderer *rend, rdctype::array<FetchBuffer> *bufs);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetResolve(ReplayRenderer *rend, uint64_t *callstack, uint32_t callstackLen,
                          rdctype::array<rdctype::str> *trace);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetDebugMessages(ReplayRenderer *rend, rdctype::array<DebugMessage> *msgs);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_PixelHistory(
    ReplayRenderer *rend, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip,
    uint32_t sampleIdx, FormatComponentType typeHint, rdctype::array<PixelModification> *history);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_DebugVertex(ReplayRenderer *rend, uint32_t vertid, uint32_t instid, uint32_t idx,
                           uint32_t instOffset, uint32_t vertOffset, ShaderDebugTrace *trace);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DebugPixel(ReplayRenderer *rend,
                                                                       uint32_t x, uint32_t y,
                                                                       uint32_t sample,
                                                                       uint32_t primitive,
                                                                       ShaderDebugTrace *trace);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DebugThread(ReplayRenderer *rend,
                                                                        uint32_t groupid[3],
                                                                        uint32_t threadid[3],
                                                                        ShaderDebugTrace *trace);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetUsage(ReplayRenderer *rend, ResourceId id, rdctype::array<EventUsage> *usage);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetCBufferVariableContents(
    ReplayRenderer *rend, ResourceId shader, const char *entryPoint, uint32_t cbufslot,
    ResourceId buffer, uint64_t offs, rdctype::array<ShaderVariable> *vars);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_SaveTexture(ReplayRenderer *rend,
                                                                        const TextureSave &saveData,
                                                                        const char *path);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetPostVSData(ReplayRenderer *rend,
                                                                          uint32_t instID,
                                                                          MeshDataStage stage,
                                                                          MeshFormat *data);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetBufferData(
    ReplayRenderer *rend, ResourceId buff, uint64_t offset, uint64_t len, rdctype::array<byte> *data);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetTextureData(ReplayRenderer *rend, ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                              rdctype::array<byte> *data);

// for C++ expose the interface as a virtual interface
#ifdef __cplusplus

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
};

#endif

#ifdef RENDERDOC_EXPORTS
struct TargetControl;
#else
#ifdef __cplusplus
typedef ITargetControl TargetControl;
#else
struct TargetControl
{
};
#endif
#endif

extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_Shutdown(TargetControl *control);

extern "C" RENDERDOC_API const char *RENDERDOC_CC TargetControl_GetTarget(TargetControl *control);
extern "C" RENDERDOC_API const char *RENDERDOC_CC TargetControl_GetAPI(TargetControl *control);
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC TargetControl_GetPID(TargetControl *control);
extern "C" RENDERDOC_API const char *RENDERDOC_CC TargetControl_GetBusyClient(TargetControl *control);

extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_TriggerCapture(TargetControl *control,
                                                                        uint32_t numFrames);
extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_QueueCapture(TargetControl *control,
                                                                      uint32_t frameNumber);
extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_CopyCapture(TargetControl *control,
                                                                     uint32_t remoteID,
                                                                     const char *localpath);
extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_DeleteCapture(TargetControl *control,
                                                                       uint32_t remoteID);

extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_ReceiveMessage(TargetControl *control,
                                                                        TargetControlMessage *msg);

// for C++ expose the interface as a virtual interface
#ifdef __cplusplus

struct IRemoteServer
{
  virtual void ShutdownConnection() = 0;

  virtual void ShutdownServerAndConnection() = 0;

  virtual bool Ping() = 0;

  virtual bool LocalProxies(rdctype::array<rdctype::str> *out) = 0;
  virtual bool RemoteSupportedReplays(rdctype::array<rdctype::str> *out) = 0;

  virtual rdctype::str GetHomeFolder() = 0;
  virtual rdctype::array<DirectoryFile> ListFolder(const char *path) = 0;

  virtual uint32_t ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
                                    void *env, const CaptureOptions *opts) = 0;

  virtual void TakeOwnershipCapture(const char *filename) = 0;
  virtual rdctype::str CopyCaptureToRemote(const char *filename, float *progress) = 0;
  virtual void CopyCaptureFromRemote(const char *remotepath, const char *localpath,
                                     float *progress) = 0;

  virtual ReplayCreateStatus OpenCapture(uint32_t proxyid, const char *logfile, float *progress,
                                         ReplayRenderer **rend) = 0;
  virtual void CloseCapture(ReplayRenderer *rend) = 0;
};

#endif

#ifdef RENDERDOC_EXPORTS
struct RemoteServer;
#else
#ifdef __cplusplus
typedef IRemoteServer RemoteServer;
#else
struct RemoteServer
{
};
#endif
#endif

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_ShutdownConnection(RemoteServer *remote);
extern "C" RENDERDOC_API void RENDERDOC_CC
RemoteServer_ShutdownServerAndConnection(RemoteServer *remote);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC RemoteServer_Ping(RemoteServer *remote);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC
RemoteServer_LocalProxies(RemoteServer *remote, rdctype::array<rdctype::str> *out);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
RemoteServer_RemoteSupportedReplays(RemoteServer *remote, rdctype::array<rdctype::str> *out);

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_GetHomeFolder(RemoteServer *remote,
                                                                      rdctype::str *home);
extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_ListFolder(
    RemoteServer *remote, const char *path, rdctype::array<DirectoryFile> *dirlist);

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC
RemoteServer_ExecuteAndInject(RemoteServer *remote, const char *app, const char *workingDir,
                              const char *cmdLine, void *env, const CaptureOptions *opts);

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_TakeOwnershipCapture(RemoteServer *remote,
                                                                             const char *filename);
extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_CopyCaptureToRemote(
    RemoteServer *remote, const char *filename, float *progress, rdctype::str *remotepath);
extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_CopyCaptureFromRemote(RemoteServer *remote,
                                                                              const char *remotepath,
                                                                              const char *localpath,
                                                                              float *progress);

extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC
RemoteServer_OpenCapture(RemoteServer *remote, uint32_t proxyid, const char *logfile,
                         float *progress, ReplayRenderer **rend);
extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_CloseCapture(RemoteServer *remote,
                                                                     ReplayRenderer *rend);

//////////////////////////////////////////////////////////////////////////
// camera
//////////////////////////////////////////////////////////////////////////

#ifdef RENDERDOC_EXPORTS
class Camera;
#else
class Camera
{
};
#endif

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

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC
Topology_NumVerticesPerPrimitive(PrimitiveTopology topology);
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC Topology_VertexOffset(PrimitiveTopology topology,
                                                                     uint32_t primitive);

//////////////////////////////////////////////////////////////////////////
// Create a replay renderer, for playback and analysis.
//
// Takes the filename of the log. Returns NULL in the case of any error.
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API ReplaySupport RENDERDOC_CC RENDERDOC_SupportLocalReplay(
    const char *logfile, rdctype::str *driverName, rdctype::str *recordMachineIdent);
extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC
RENDERDOC_CreateReplayRenderer(const char *logfile, float *progress, ReplayRenderer **rend);

//////////////////////////////////////////////////////////////////////////
// Target Control
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API TargetControl *RENDERDOC_CC RENDERDOC_CreateTargetControl(
    const char *host, uint32_t ident, const char *clientName, bool32 forceConnection);
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_EnumerateRemoteTargets(const char *host,
                                                                                uint32_t nextIdent);

//////////////////////////////////////////////////////////////////////////
// Remote server
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_GetDefaultRemoteServerPort();
extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC
RENDERDOC_CreateRemoteServerConnection(const char *host, uint32_t port, RemoteServer **rend);
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

//////////////////////////////////////////////////////////////////////////
// Miscellaneous!
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_TriggerExceptionHandler(void *exceptionPtrs,
                                                                             bool32 crashed);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetDebugLogFile(const char *filename);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogText(const char *text);
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogMessage(LogMessageType type,
                                                                const char *project, const char *file,
                                                                unsigned int line, const char *text);
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
    void *mem, int idx, const char *variable, const char *value, EnvironmentModificationType type,
    EnvironmentSeparator separator);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeEnvironmentModificationList(void *mem);
