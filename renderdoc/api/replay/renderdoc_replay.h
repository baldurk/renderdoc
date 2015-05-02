/******************************************************************************
 * The MIT License (MIT)
 * 
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

#include "basic_types.h"

#ifdef WIN32

#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API __declspec(dllexport)
#else
#define RENDERDOC_API __declspec(dllimport)
#endif
#define RENDERDOC_CC __cdecl

#elif defined(__linux__)

#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API __attribute__ ((visibility ("default")))
#else
#define RENDERDOC_API
#endif

#define RENDERDOC_CC

#else

#error "Unknown platform"

#endif

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

	bool operator ==(const ResourceId u) const
	{
		return id == u.id;
	}

	bool operator !=(const ResourceId u) const
	{
		return id != u.id;
	}

	bool operator <(const ResourceId u) const
	{
		return id < u.id;
	}
#endif
};

#include "replay_enums.h"

#include "shader_types.h"
#include "data_types.h"
#include "control_types.h"

#include "d3d11_pipestate.h"
#include "gl_pipestate.h"

#ifdef RENDERDOC_EXPORTS
struct ReplayOutput;
#else
struct ReplayOutput { };
#endif

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetOutputConfig(ReplayOutput *output, const OutputConfig &o);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetTextureDisplay(ReplayOutput *output, const TextureDisplay &o);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetMeshDisplay(ReplayOutput *output, const MeshDisplay &o);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_ClearThumbnails(ReplayOutput *output);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_AddThumbnail(ReplayOutput *output, void *wnd, ResourceId texID);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_Display(ReplayOutput *output);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetPixelContext(ReplayOutput *output, void *wnd);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetPixelContextLocation(ReplayOutput *output, uint32_t x, uint32_t y);
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_DisablePixelContext(ReplayOutput *output);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_PickPixel(ReplayOutput *output, ResourceId texID, bool32 customShader,
														uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *val);

#ifdef RENDERDOC_EXPORTS
struct ReplayRenderer;
#else
struct ReplayRenderer { };
#endif

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_GetAPIProperties(ReplayRenderer *rend, APIProperties *props);

extern "C" RENDERDOC_API ReplayOutput* RENDERDOC_CC ReplayRenderer_CreateOutput(ReplayRenderer *rend, void *handle);
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_Shutdown(ReplayRenderer *rend);
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_ShutdownOutput(ReplayRenderer *rend, ReplayOutput *output);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_HasCallstacks(ReplayRenderer *rend);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_InitResolver(ReplayRenderer *rend);
 
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_SetContextFilter(ReplayRenderer *rend, ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_SetFrameEvent(ReplayRenderer *rend, uint32_t frameID, uint32_t eventID);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetD3D11PipelineState(ReplayRenderer *rend, D3D11PipelineState *state);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetGLPipelineState(ReplayRenderer *rend, GLPipelineState *state);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_BuildCustomShader(ReplayRenderer *rend, const char *entry, const char *source, const uint32_t compileFlags, ShaderStageType type, ResourceId *shaderID, rdctype::str *errors);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_FreeCustomShader(ReplayRenderer *rend, ResourceId id);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_BuildTargetShader(ReplayRenderer *rend, const char *entry, const char *source, const uint32_t compileFlags, ShaderStageType type, ResourceId *shaderID, rdctype::str *errors);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_ReplaceResource(ReplayRenderer *rend, ResourceId from, ResourceId to);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_RemoveReplacement(ReplayRenderer *rend, ResourceId id);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_FreeTargetResource(ReplayRenderer *rend, ResourceId id);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetFrameInfo(ReplayRenderer *rend, rdctype::array<FetchFrameInfo> *frame);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetDrawcalls(ReplayRenderer *rend, uint32_t frameID, rdctype::array<FetchDrawcall> *draws);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_FetchCounters(ReplayRenderer *rend, uint32_t frameID, uint32_t minEventID, uint32_t maxEventID, uint32_t *counters, uint32_t numCounters, rdctype::array<CounterResult> *results);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_EnumerateCounters(ReplayRenderer *rend, rdctype::array<uint32_t> *counters);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DescribeCounter(ReplayRenderer *rend, uint32_t counterID, CounterDescription *desc);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetTextures(ReplayRenderer *rend, rdctype::array<FetchTexture> *texs);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetBuffers(ReplayRenderer *rend, rdctype::array<FetchBuffer> *bufs);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetResolve(ReplayRenderer *rend, uint64_t *callstack, uint32_t callstackLen, rdctype::array<rdctype::str> *trace);
extern "C" RENDERDOC_API ShaderReflection* RENDERDOC_CC ReplayRenderer_GetShaderDetails(ReplayRenderer *rend, ResourceId shader);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetDebugMessages(ReplayRenderer *rend, rdctype::array<DebugMessage> *msgs);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_PixelHistory(ReplayRenderer *rend, ResourceId target, uint32_t x, uint32_t y, uint32_t sampleIdx, rdctype::array<PixelModification> *history);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DebugVertex(ReplayRenderer *rend, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset, ShaderDebugTrace *trace);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DebugPixel(ReplayRenderer *rend, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive, ShaderDebugTrace *trace);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DebugThread(ReplayRenderer *rend, uint32_t groupid[3], uint32_t threadid[3], ShaderDebugTrace *trace);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetUsage(ReplayRenderer *rend, ResourceId id, rdctype::array<EventUsage> *usage);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetCBufferVariableContents(ReplayRenderer *rend, ResourceId shader, uint32_t cbufslot, ResourceId buffer, uint32_t offs, rdctype::array<ShaderVariable> *vars);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_SaveTexture(ReplayRenderer *rend, const TextureSave &saveData, const char *path);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetPostVSData(ReplayRenderer *rend, uint32_t instID, MeshDataStage stage, MeshFormat *data);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetMinMax(ReplayRenderer *rend, ResourceId tex, uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *minval, PixelValue *maxval);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetHistogram(ReplayRenderer *rend, ResourceId tex, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool32 channels[4], rdctype::array<uint32_t> *histogram);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetBufferData(ReplayRenderer *rend, ResourceId buff, uint32_t offset, uint32_t len, rdctype::array<byte> *data);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetTextureData(ReplayRenderer *rend, ResourceId tex, uint32_t arrayIdx, uint32_t mip, rdctype::array<byte> *data);

#ifdef RENDERDOC_EXPORTS
struct RemoteAccess;
#else
struct RemoteAccess { };
#endif

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_Shutdown(RemoteAccess *access);
 
extern "C" RENDERDOC_API const char* RENDERDOC_CC RemoteAccess_GetTarget(RemoteAccess *access);
extern "C" RENDERDOC_API const char* RENDERDOC_CC RemoteAccess_GetAPI(RemoteAccess *access);
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RemoteAccess_GetPID(RemoteAccess *access);
extern "C" RENDERDOC_API const char* RENDERDOC_CC RemoteAccess_GetBusyClient(RemoteAccess *access);

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_TriggerCapture(RemoteAccess *access);
extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_QueueCapture(RemoteAccess *access, uint32_t frameNumber);
extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_CopyCapture(RemoteAccess *access, uint32_t remoteID, const char *localpath);

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_ReceiveMessage(RemoteAccess *access, RemoteMessage *msg);

#ifdef RENDERDOC_EXPORTS
struct RemoteRenderer;
#else
struct RemoteRenderer { };
#endif

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteRenderer_Shutdown(RemoteRenderer *remote);
 
extern "C" RENDERDOC_API bool32 RENDERDOC_CC RemoteRenderer_LocalProxies(RemoteRenderer *remote, rdctype::array<rdctype::str> *out);
extern "C" RENDERDOC_API bool32 RENDERDOC_CC RemoteRenderer_RemoteSupportedReplays(RemoteRenderer *remote, rdctype::array<rdctype::str> *out);

extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC RemoteRenderer_CreateProxyRenderer(RemoteRenderer *remote, uint32_t proxyid, const char *logfile, float *progress, ReplayRenderer **rend);

//////////////////////////////////////////////////////////////////////////
// Maths/format related exports
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API float RENDERDOC_CC Maths_HalfToFloat(uint16_t half);
extern "C" RENDERDOC_API uint16_t RENDERDOC_CC Maths_FloatToHalf(float f);

extern "C" RENDERDOC_API void RENDERDOC_CC Maths_CameraArcball(float dist, const FloatVector &rot, FloatVector *pos, FloatVector *fwd, FloatVector *right);
extern "C" RENDERDOC_API void RENDERDOC_CC Maths_CameraFPSLook(const FloatVector &lookpos, const FloatVector &rot, FloatVector *pos, FloatVector *fwd, FloatVector *right);

//////////////////////////////////////////////////////////////////////////
// Create a replay renderer, for playback and analysis.
//
// Takes the filename of the log. Returns NULL in the case of any error.
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API bool32 RENDERDOC_CC RENDERDOC_SupportLocalReplay(const char *logfile, rdctype::str *driverName);
typedef bool32 (RENDERDOC_CC *pRENDERDOC_SupportLocalReplay)(const char *logfile, rdctype::str *driverName);

extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC RENDERDOC_CreateReplayRenderer(const char *logfile, float *progress, ReplayRenderer **rend);
typedef ReplayCreateStatus (RENDERDOC_CC *pRENDERDOC_CreateReplayRenderer)(const char *logfile, float *progress, ReplayRenderer **rend);

//////////////////////////////////////////////////////////////////////////
// Remote access and control
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API RemoteAccess* RENDERDOC_CC RENDERDOC_CreateRemoteAccessConnection(const char *host, uint32_t ident, const char *clientName, bool32 forceConnection);
typedef RemoteAccess* (RENDERDOC_CC *pRENDERDOC_CreateRemoteAccessConnection)(const char *host, uint32_t ident, const char *clientName, bool32 forceConnection);

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_EnumerateRemoteConnections(const char *host, uint32_t *idents);
typedef uint32_t (RENDERDOC_CC *pRENDERDOC_EnumerateRemoteConnections)(const char *host, uint32_t *idents);

extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC RENDERDOC_CreateRemoteReplayConnection(const char *host, RemoteRenderer **rend);
typedef ReplayCreateStatus (RENDERDOC_CC *pRENDERDOC_CreateRemoteReplayConnection)(const char *host, RemoteRenderer **rend);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SpawnReplayHost(volatile bool32 *killReplay);
typedef void (RENDERDOC_CC *pRENDERDOC_SpawnReplayHost)(volatile bool32 *killReplay);

//////////////////////////////////////////////////////////////////////////
// Injection/execution capture functions.
//////////////////////////////////////////////////////////////////////////

struct CaptureOptions;

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartGlobalHook(const char *pathmatch, const char *logfile, const CaptureOptions *opts);
typedef void (RENDERDOC_CC *pRENDERDOC_StartGlobalHook)(const char *pathmatch, const char *logfile, const CaptureOptions *opts);

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
																	const char *logfile, const CaptureOptions *opts, bool32 waitForExit);
typedef uint32_t (RENDERDOC_CC *pRENDERDOC_ExecuteAndInject)(const char *app, const char *workingDir, const char *cmdLine,
														 const char *logfile, const CaptureOptions *opts, bool32 waitForExit);
     
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_InjectIntoProcess(uint32_t pid, const char *logfile, const CaptureOptions *opts, bool32 waitForExit);
typedef uint32_t (RENDERDOC_CC *pRENDERDOC_InjectIntoProcess)(uint32_t pid, const char *logfile, const CaptureOptions *opts, bool32 waitForExit);

//////////////////////////////////////////////////////////////////////////
// Miscellaneous!
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_TriggerExceptionHandler(void *exceptionPtrs, bool32 crashed);
typedef void (RENDERDOC_CC *pRENDERDOC_TriggerExceptionHandler)(void *exceptionPtrs, bool32 crashed);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogText(const char *text);
typedef void (RENDERDOC_CC *pRENDERDOC_LogText)(const char *text);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC RENDERDOC_GetThumbnail(const char *filename, byte *buf, uint32_t &len);
typedef bool32 (RENDERDOC_CC *pRENDERDOC_GetThumbnail)(const char *filename, byte *buf, uint32_t &len);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeArrayMem(const void *mem);
typedef void (RENDERDOC_CC *pRENDERDOC_FreeArrayMem)(const void *mem);
