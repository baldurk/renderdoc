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

#include "replay_proxy.h"
#include <list>
#include "lz4/lz4.h"
#include "replay/dummy_driver.h"
#include "serialise/lz4io.h"

template <>
rdcstr DoStringise(const ReplayProxyPacket &el)
{
  BEGIN_ENUM_STRINGISE(ReplayProxyPacket);
  {
    STRINGISE_ENUM_NAMED(eReplayProxy_RemoteExecutionKeepAlive, "RemoteExecutionKeepAlive");
    STRINGISE_ENUM_NAMED(eReplayProxy_RemoteExecutionFinished, "RemoteExecutionFinished");

    STRINGISE_ENUM_NAMED(eReplayProxy_ReplayLog, "ReplayLog");

    STRINGISE_ENUM_NAMED(eReplayProxy_CacheBufferData, "CacheBufferData");
    STRINGISE_ENUM_NAMED(eReplayProxy_CacheTextureData, "CacheTextureData");

    STRINGISE_ENUM_NAMED(eReplayProxy_GetAPIProperties, "GetAPIProperties");
    STRINGISE_ENUM_NAMED(eReplayProxy_FetchStructuredFile, "FetchStructuredFile");

    STRINGISE_ENUM_NAMED(eReplayProxy_GetPassEvents, "GetPassEvents");

    STRINGISE_ENUM_NAMED(eReplayProxy_GetResources, "GetResources");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetTextures, "GetTextures");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetTexture, "GetTexture");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetBuffers, "GetBuffers");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetBuffer, "GetBuffer");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetShaderEntryPoints, "GetShaderEntryPoints");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetShader, "GetShader");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetDebugMessages, "GetDebugMessages");

    STRINGISE_ENUM_NAMED(eReplayProxy_GetBufferData, "GetBufferData");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetTextureData, "GetTextureData");

    STRINGISE_ENUM_NAMED(eReplayProxy_SavePipelineState, "SavePipelineState");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetUsage, "GetUsage");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetLiveID, "GetLiveID");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetFrameRecord, "GetFrameRecord");
    STRINGISE_ENUM_NAMED(eReplayProxy_IsRenderOutput, "IsRenderOutput");
    STRINGISE_ENUM_NAMED(eReplayProxy_NeedRemapForFetch, "NeedRemapForFetch");

    STRINGISE_ENUM_NAMED(eReplayProxy_FreeTargetResource, "FreeTargetResource");

    STRINGISE_ENUM_NAMED(eReplayProxy_FetchCounters, "FetchCounters");
    STRINGISE_ENUM_NAMED(eReplayProxy_EnumerateCounters, "EnumerateCounters");
    STRINGISE_ENUM_NAMED(eReplayProxy_DescribeCounter, "DescribeCounter");
    STRINGISE_ENUM_NAMED(eReplayProxy_FillCBufferVariables, "FillCBufferVariables");

    STRINGISE_ENUM_NAMED(eReplayProxy_InitPostVS, "InitPostVS");
    STRINGISE_ENUM_NAMED(eReplayProxy_InitPostVSVec, "InitPostVSVec");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetPostVS, "GetPostVS");

    STRINGISE_ENUM_NAMED(eReplayProxy_BuildTargetShader, "BuildTargetShader");
    STRINGISE_ENUM_NAMED(eReplayProxy_ReplaceResource, "ReplaceResource");
    STRINGISE_ENUM_NAMED(eReplayProxy_RemoveReplacement, "RemoveReplacement");

    STRINGISE_ENUM_NAMED(eReplayProxy_DebugVertex, "DebugVertex");
    STRINGISE_ENUM_NAMED(eReplayProxy_DebugPixel, "DebugPixel");
    STRINGISE_ENUM_NAMED(eReplayProxy_DebugThread, "DebugThread");

    STRINGISE_ENUM_NAMED(eReplayProxy_RenderOverlay, "RenderOverlay");

    STRINGISE_ENUM_NAMED(eReplayProxy_PixelHistory, "PixelHistory");

    STRINGISE_ENUM_NAMED(eReplayProxy_DisassembleShader, "DisassembleShader");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetDisassemblyTargets, "GetDisassemblyTargets");
    STRINGISE_ENUM_NAMED(eReplayProxy_GetTargetShaderEncodings, "GetTargetShaderEncodings");

    STRINGISE_ENUM_NAMED(eReplayProxy_GetDriverInfo, "GetDriverInfo");

    STRINGISE_ENUM_NAMED(eReplayProxy_ContinueDebug, "ContinueDebug");
    STRINGISE_ENUM_NAMED(eReplayProxy_FreeDebugger, "FreeDebugger");
  }
  END_ENUM_STRINGISE();
}

// utility macros for implementing proxied functions

// begins a chunk with the given packet type, and if reading verifies that the
// read type was what was expected - otherwise sets an error flag
#define PACKET_HEADER(packet)                                         \
  ReplayProxyPacket p = (ReplayProxyPacket)ser.BeginChunk(packet, 0); \
  if(ser.IsReading() && p != packet)                                  \
    m_IsErrored = true;

// begins the set of parameters. Note that we only begin a chunk when writing (sending a request to
// the remote server), since on reading the chunk has already been begun to read the type to
// dispatch to the correct function.
#define BEGIN_PARAMS()             \
  ParamSerialiser &ser = paramser; \
  if(ser.IsWriting())              \
    ser.BeginChunk(packet, 0);

// end the set of parameters, and that chunk.
#define END_PARAMS()                                \
  {                                                 \
    GET_SERIALISER.Serialise("packet"_lit, packet); \
    ser.EndChunk();                                 \
    CheckError(packet, expectedPacket);             \
  }

// begin serialising a return value. We begin a chunk here in either the writing or reading case
// since this chunk is used purely to send/receive the return value and is fully handled within the
// function.
#define SERIALISE_RETURN(retval)                                                      \
  {                                                                                   \
    RDResult fatalStatus = ResultCode::Succeeded;                                     \
    if(m_RemoteServer)                                                                \
      fatalStatus = m_Remote->FatalErrorCheck();                                      \
    ReturnSerialiser &ser = retser;                                                   \
    PACKET_HEADER(packet);                                                            \
    SERIALISE_ELEMENT(retval);                                                        \
    GET_SERIALISER.Serialise("fatalStatus"_lit, fatalStatus);                         \
    GET_SERIALISER.Serialise("packet"_lit, packet);                                   \
    ser.EndChunk();                                                                   \
    if(fatalStatus != ResultCode::Succeeded && m_FatalError == ResultCode::Succeeded) \
      m_FatalError = fatalStatus;                                                     \
    CheckError(packet, expectedPacket);                                               \
  }

// similar to the above, but for void functions that don't return anything. We still want to check
// that both sides of the communication are on the same page.
#define SERIALISE_RETURN_VOID()                                                       \
  {                                                                                   \
    RDResult fatalStatus = ResultCode::Succeeded;                                     \
    if(m_RemoteServer)                                                                \
      fatalStatus = m_Remote->FatalErrorCheck();                                      \
    ReturnSerialiser &ser = retser;                                                   \
    PACKET_HEADER(packet);                                                            \
    SERIALISE_ELEMENT(packet);                                                        \
    GET_SERIALISER.Serialise("fatalStatus"_lit, fatalStatus);                         \
    ser.EndChunk();                                                                   \
    if(fatalStatus != ResultCode::Succeeded && m_FatalError == ResultCode::Succeeded) \
      m_FatalError = fatalStatus;                                                     \
    CheckError(packet, expectedPacket);                                               \
  }

// defines the area where we're executing on the remote host. To avoid timeouts, the remote side
// will pass over to a thread and begin sending periodic keepalive packets. Once complete, it will
// send a finished packet and continue. The host side will accept any keepalive packets and continue
// once it reads the finished packet. This is very synchronous, but by design the whole replay proxy
// is very synchronous.
// The host side will also abort under the usual circumstances - if some network error happens, or
// if there is a true timeout.
// Only the remote side needs a thread because the main thread will be busy inside the actual
// workload, on the host side our thread can do the packet searching itself.
struct RemoteExecution
{
  ReplayProxy *m_Proxy;
  RemoteExecution(ReplayProxy *proxy)
  {
    m_Proxy = proxy;
    m_Proxy->BeginRemoteExecution();
  }
  ~RemoteExecution() { m_Proxy->EndRemoteExecution(); }
};
#define REMOTE_EXECUTION() RemoteExecution exec(this);

// uncomment the following to print verbose debugging prints for the remote proxy packets
// #define PROXY_DEBUG(...) RDCDEBUG(__VA_ARGS__)

#if !defined(PROXY_DEBUG)
#define PROXY_DEBUG(...) \
  do                     \
  {                      \
  } while(0)
#endif

// dispatches to the right implementation of the Proxied_ function, depending on whether we're on
// the remote server or not.
#define PROXY_FUNCTION(name, ...)                                     \
  PROXY_DEBUG("Proxying out %s", #name);                              \
  if(m_RemoteServer)                                                  \
    return CONCAT(Proxied_, name)(m_Reader, m_Writer, ##__VA_ARGS__); \
  else                                                                \
    return CONCAT(Proxied_, name)(m_Writer, m_Reader, ##__VA_ARGS__);

ReplayProxy::ReplayProxy(ReadSerialiser &reader, WriteSerialiser &writer, IRemoteDriver *remoteDriver,
                         IReplayDriver *replayDriver, RENDERDOC_PreviewWindowCallback previewWindow)
    : m_Reader(reader),
      m_Writer(writer),
      m_Proxy(NULL),
      m_Remote(remoteDriver),
      m_Replay(replayDriver),
      m_PreviewWindow(previewWindow),
      m_RemoteServer(true)
{
  m_StructuredFile = new SDFile;

  m_APIProps = m_Remote->GetAPIProperties();

  InitRemoteExecutionThread();

  if(m_Replay)
    InitPreviewWindow();

  if(m_APIProps.pipelineType == GraphicsAPI::D3D11)
    m_D3D11PipelineState = new D3D11Pipe::State;
  else if(m_APIProps.pipelineType == GraphicsAPI::D3D12)
    m_D3D12PipelineState = new D3D12Pipe::State;
  else if(m_APIProps.pipelineType == GraphicsAPI::OpenGL)
    m_GLPipelineState = new GLPipe::State;
  else if(m_APIProps.pipelineType == GraphicsAPI::Vulkan)
    m_VulkanPipelineState = new VKPipe::State;

  m_Remote->SetPipelineStates(m_D3D11PipelineState, m_D3D12PipelineState, m_GLPipelineState,
                              m_VulkanPipelineState);
}

ReplayProxy::ReplayProxy(ReadSerialiser &reader, WriteSerialiser &writer, IReplayDriver *proxy)
    : m_Reader(reader),
      m_Writer(writer),
      m_Proxy(proxy),
      m_Remote(NULL),
      m_Replay(NULL),
      m_RemoteServer(false)
{
  m_StructuredFile = new SDFile;

  ReplayProxy::GetAPIProperties();
  ReplayProxy::FetchStructuredFile();
}

ReplayProxy::~ReplayProxy()
{
  SAFE_DELETE(m_StructuredFile);
  if(m_Remote)
  {
    SAFE_DELETE(m_D3D11PipelineState);
    SAFE_DELETE(m_D3D12PipelineState);
    SAFE_DELETE(m_GLPipelineState);
    SAFE_DELETE(m_VulkanPipelineState);
  }

  ShutdownRemoteExecutionThread();

  ShutdownPreviewWindow();

  if(m_Proxy)
    m_Proxy->Shutdown();
  m_Proxy = NULL;

  for(auto it = m_ShaderReflectionCache.begin(); it != m_ShaderReflectionCache.end(); ++it)
    delete it->second;
}

#pragma region Proxied Functions

template <typename ParamSerialiser, typename ReturnSerialiser>
bool ReplayProxy::Proxied_NeedRemapForFetch(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            const ResourceFormat &format)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_NeedRemapForFetch;
  ReplayProxyPacket packet = eReplayProxy_NeedRemapForFetch;
  bool ret = false;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(format);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->NeedRemapForFetch(format);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

bool ReplayProxy::NeedRemapForFetch(const ResourceFormat &fmt)
{
  PROXY_FUNCTION(NeedRemapForFetch, fmt);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
bool ReplayProxy::Proxied_IsRenderOutput(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                         ResourceId id)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_IsRenderOutput;
  ReplayProxyPacket packet = eReplayProxy_IsRenderOutput;
  bool ret = false;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->IsRenderOutput(id);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

bool ReplayProxy::IsRenderOutput(ResourceId id)
{
  PROXY_FUNCTION(IsRenderOutput, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
APIProperties ReplayProxy::Proxied_GetAPIProperties(ParamSerialiser &paramser,
                                                    ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetAPIProperties;
  ReplayProxyPacket packet = eReplayProxy_GetAPIProperties;
  APIProperties ret = {};

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetAPIProperties();
  }

  SERIALISE_RETURN(ret);

  if(!m_RemoteServer)
  {
    ret.localRenderer = m_Proxy->GetAPIProperties().localRenderer;
    ret.remoteReplay = true;
  }

  m_APIProps = ret;

  return ret;
}

APIProperties ReplayProxy::GetAPIProperties()
{
  PROXY_FUNCTION(GetAPIProperties);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
DriverInformation ReplayProxy::Proxied_GetDriverInfo(ParamSerialiser &paramser,
                                                     ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetDriverInfo;
  ReplayProxyPacket packet = eReplayProxy_GetDriverInfo;
  DriverInformation ret = {};

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetDriverInfo();
  }

  SERIALISE_RETURN(ret);

  return ret;
}

DriverInformation ReplayProxy::GetDriverInfo()
{
  PROXY_FUNCTION(GetDriverInfo);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<GPUDevice> ReplayProxy::Proxied_GetAvailableGPUs(ParamSerialiser &paramser,
                                                          ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetAvailableGPUs;
  ReplayProxyPacket packet = eReplayProxy_GetAvailableGPUs;
  rdcarray<GPUDevice> ret = {};

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetAvailableGPUs();
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<GPUDevice> ReplayProxy::GetAvailableGPUs()
{
  PROXY_FUNCTION(GetAvailableGPUs);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<DebugMessage> ReplayProxy::Proxied_GetDebugMessages(ParamSerialiser &paramser,
                                                             ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetDebugMessages;
  ReplayProxyPacket packet = eReplayProxy_GetDebugMessages;
  rdcarray<DebugMessage> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetDebugMessages();
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<DebugMessage> ReplayProxy::GetDebugMessages()
{
  PROXY_FUNCTION(GetDebugMessages);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<TextureDescription> ReplayProxy::Proxied_GetTextures(ParamSerialiser &paramser,
                                                              ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetTextures;
  ReplayProxyPacket packet = eReplayProxy_GetTextures;
  rdcarray<TextureDescription> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetTextures();
  }

  SERIALISE_RETURN(ret);

  if(retser.IsReading())
  {
    for(const TextureDescription &tex : ret)
      m_TextureInfo[tex.resourceId] = tex;
  }

  return ret;
}

rdcarray<TextureDescription> ReplayProxy::GetTextures()
{
  PROXY_FUNCTION(GetTextures);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
TextureDescription ReplayProxy::Proxied_GetTexture(ParamSerialiser &paramser,
                                                   ReturnSerialiser &retser, ResourceId id)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetTexture;
  ReplayProxyPacket packet = eReplayProxy_GetTexture;
  TextureDescription ret = {};

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetTexture(id);
  }

  SERIALISE_RETURN(ret);

  if(retser.IsReading())
    m_TextureInfo[id] = ret;

  return ret;
}

TextureDescription ReplayProxy::GetTexture(ResourceId id)
{
  PROXY_FUNCTION(GetTexture, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<BufferDescription> ReplayProxy::Proxied_GetBuffers(ParamSerialiser &paramser,
                                                            ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetBuffers;
  ReplayProxyPacket packet = eReplayProxy_GetBuffers;
  rdcarray<BufferDescription> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetBuffers();
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<BufferDescription> ReplayProxy::GetBuffers()
{
  PROXY_FUNCTION(GetBuffers);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<ResourceDescription> ReplayProxy::Proxied_GetResources(ParamSerialiser &paramser,
                                                                ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetResources;
  ReplayProxyPacket packet = eReplayProxy_GetResources;
  rdcarray<ResourceDescription> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetResources();
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<ResourceDescription> ReplayProxy::GetResources()
{
  PROXY_FUNCTION(GetResources);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
BufferDescription ReplayProxy::Proxied_GetBuffer(ParamSerialiser &paramser,
                                                 ReturnSerialiser &retser, ResourceId id)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetBuffer;
  ReplayProxyPacket packet = eReplayProxy_GetBuffer;
  BufferDescription ret = {};

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetBuffer(id);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

BufferDescription ReplayProxy::GetBuffer(ResourceId id)
{
  PROXY_FUNCTION(GetBuffer, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<uint32_t> ReplayProxy::Proxied_GetPassEvents(ParamSerialiser &paramser,
                                                      ReturnSerialiser &retser, uint32_t eventId)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetPassEvents;
  ReplayProxyPacket packet = eReplayProxy_GetPassEvents;
  rdcarray<uint32_t> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventId);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetPassEvents(eventId);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<uint32_t> ReplayProxy::GetPassEvents(uint32_t eventId)
{
  PROXY_FUNCTION(GetPassEvents, eventId);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<EventUsage> ReplayProxy::Proxied_GetUsage(ParamSerialiser &paramser,
                                                   ReturnSerialiser &retser, ResourceId id)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetUsage;
  ReplayProxyPacket packet = eReplayProxy_GetUsage;
  rdcarray<EventUsage> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetUsage(id);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<EventUsage> ReplayProxy::GetUsage(ResourceId id)
{
  PROXY_FUNCTION(GetUsage, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
FrameRecord ReplayProxy::Proxied_GetFrameRecord(ParamSerialiser &paramser, ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetFrameRecord;
  ReplayProxyPacket packet = eReplayProxy_GetFrameRecord;
  FrameRecord ret = {};

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetFrameRecord();
  }

  SERIALISE_RETURN(ret);

  if(paramser.IsWriting())
  {
    // re-configure the action pointers, since they will be invalid
    SetupActionPointers(m_Actions, ret.actionList);
  }

  return ret;
}

FrameRecord ReplayProxy::GetFrameRecord()
{
  PROXY_FUNCTION(GetFrameRecord);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ResourceId ReplayProxy::Proxied_GetLiveID(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                          ResourceId id)
{
  if(paramser.IsWriting())
  {
    if(m_LiveIDs.find(id) != m_LiveIDs.end())
      return m_LiveIDs[id];

    if(m_LocalTextures.find(id) != m_LocalTextures.end())
      return id;
  }

  if(paramser.IsErrored() || retser.IsErrored() || m_IsErrored)
    return ResourceId();

  const ReplayProxyPacket expectedPacket = eReplayProxy_GetLiveID;
  ReplayProxyPacket packet = eReplayProxy_GetLiveID;
  ResourceId ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetLiveID(id);
  }

  SERIALISE_RETURN(ret);

  if(paramser.IsWriting())
    m_LiveIDs[id] = ret;

  return ret;
}

ResourceId ReplayProxy::GetLiveID(ResourceId id)
{
  PROXY_FUNCTION(GetLiveID, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<CounterResult> ReplayProxy::Proxied_FetchCounters(ParamSerialiser &paramser,
                                                           ReturnSerialiser &retser,
                                                           const rdcarray<GPUCounter> &counters)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_FetchCounters;
  ReplayProxyPacket packet = eReplayProxy_FetchCounters;
  rdcarray<CounterResult> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(counters);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->FetchCounters(counters);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<CounterResult> ReplayProxy::FetchCounters(const rdcarray<GPUCounter> &counters)
{
  PROXY_FUNCTION(FetchCounters, counters);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<GPUCounter> ReplayProxy::Proxied_EnumerateCounters(ParamSerialiser &paramser,
                                                            ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_EnumerateCounters;
  ReplayProxyPacket packet = eReplayProxy_EnumerateCounters;
  rdcarray<GPUCounter> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->EnumerateCounters();
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<GPUCounter> ReplayProxy::EnumerateCounters()
{
  PROXY_FUNCTION(EnumerateCounters);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
CounterDescription ReplayProxy::Proxied_DescribeCounter(ParamSerialiser &paramser,
                                                        ReturnSerialiser &retser,
                                                        GPUCounter counterID)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_DescribeCounter;
  ReplayProxyPacket packet = eReplayProxy_DescribeCounter;
  CounterDescription ret = {};

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(counterID);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->DescribeCounter(counterID);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

CounterDescription ReplayProxy::DescribeCounter(GPUCounter counterID)
{
  PROXY_FUNCTION(DescribeCounter, counterID);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_FillCBufferVariables(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                               ResourceId pipeline, ResourceId shader,
                                               ShaderStage stage, rdcstr entryPoint,
                                               uint32_t cbufSlot, rdcarray<ShaderVariable> &outvars,
                                               const bytebuf &data)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_FillCBufferVariables;
  ReplayProxyPacket packet = eReplayProxy_FillCBufferVariables;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(pipeline);
    SERIALISE_ELEMENT(shader);
    SERIALISE_ELEMENT(stage);
    SERIALISE_ELEMENT(entryPoint);
    SERIALISE_ELEMENT(cbufSlot);
    SERIALISE_ELEMENT(data);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->FillCBufferVariables(pipeline, shader, stage, entryPoint, cbufSlot, outvars, data);
  }

  SERIALISE_RETURN(outvars);
}

void ReplayProxy::FillCBufferVariables(ResourceId pipeline, ResourceId shader, ShaderStage stage,
                                       rdcstr entryPoint, uint32_t cbufSlot,
                                       rdcarray<ShaderVariable> &outvars, const bytebuf &data)
{
  PROXY_FUNCTION(FillCBufferVariables, pipeline, shader, stage, entryPoint, cbufSlot, outvars, data);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_GetBufferData(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                        ResourceId buff, uint64_t offset, uint64_t len,
                                        bytebuf &retData)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetBufferData;
  ReplayProxyPacket packet = eReplayProxy_GetBufferData;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(buff);
    SERIALISE_ELEMENT(offset);
    SERIALISE_ELEMENT(len);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->GetBufferData(buff, offset, len, retData);
  }

  // over-estimate of total uncompressed data written. Since the decompression chain needs to know
  // the exact uncompressed size, we over-estimate (to allow for length/padding/etc) and then pad
  // to this amount.
  uint64_t dataSize = retData.size() + 2 * retser.GetChunkAlignment();

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    SERIALISE_ELEMENT(packet);
    SERIALISE_ELEMENT(dataSize);
  }

  char empty[128] = {};

  // lz4 compress
  if(retser.IsReading())
  {
    ReadSerialiser ser(new StreamReader(new LZ4Decompressor(retser.GetReader(), Ownership::Nothing),
                                        dataSize, Ownership::Stream),
                       Ownership::Stream);

    SERIALISE_ELEMENT(retData);

    uint64_t offs = ser.GetReader()->GetOffset();
    RDCASSERT(offs <= dataSize, offs, dataSize);
    RDCASSERT(dataSize - offs < sizeof(empty), offs, dataSize);

    if(offs < dataSize)
      ser.GetReader()->Read(empty, dataSize - offs);
  }
  else
  {
    WriteSerialiser ser(new StreamWriter(new LZ4Compressor(retser.GetWriter(), Ownership::Nothing),
                                         Ownership::Stream),
                        Ownership::Stream);

    SERIALISE_ELEMENT(retData);

    uint64_t offs = ser.GetWriter()->GetOffset();
    RDCASSERT(offs <= dataSize, offs, dataSize);
    RDCASSERT(dataSize - offs < sizeof(empty), offs, dataSize);

    if(offs < dataSize)
      ser.GetWriter()->Write(empty, dataSize - offs);
  }

  retser.EndChunk();

  CheckError(packet, expectedPacket);
}

void ReplayProxy::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData)
{
  PROXY_FUNCTION(GetBufferData, buff, offset, len, retData);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_GetTextureData(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                         ResourceId tex, const Subresource &sub,
                                         const GetTextureDataParams &params, bytebuf &data)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetTextureData;
  ReplayProxyPacket packet = eReplayProxy_GetTextureData;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(tex);
    SERIALISE_ELEMENT(sub);
    SERIALISE_ELEMENT(params);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->GetTextureData(tex, sub, params, data);
  }

  // over-estimate of total uncompressed data written. Since the decompression chain needs to know
  // the exact uncompressed size, we over-estimate (to allow for length/padding/etc) and then pad
  // to this amount.
  uint64_t dataSize = data.size() + 2 * retser.GetChunkAlignment();

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    SERIALISE_ELEMENT(packet);
    SERIALISE_ELEMENT(dataSize);
  }

  char empty[128] = {};

  // lz4 compress
  if(retser.IsReading())
  {
    ReadSerialiser ser(new StreamReader(new LZ4Decompressor(retser.GetReader(), Ownership::Nothing),
                                        dataSize, Ownership::Stream),
                       Ownership::Stream);

    SERIALISE_ELEMENT(data);

    uint64_t offs = ser.GetReader()->GetOffset();
    RDCASSERT(offs <= dataSize, offs, dataSize);
    RDCASSERT(dataSize - offs < sizeof(empty), offs, dataSize);

    if(offs < dataSize)
      ser.GetReader()->Read(empty, dataSize - offs);
  }
  else
  {
    WriteSerialiser ser(new StreamWriter(new LZ4Compressor(retser.GetWriter(), Ownership::Nothing),
                                         Ownership::Stream),
                        Ownership::Stream);

    SERIALISE_ELEMENT(data);

    uint64_t offs = ser.GetWriter()->GetOffset();
    RDCASSERT(offs <= dataSize, offs, dataSize);
    RDCASSERT(dataSize - offs < sizeof(empty), offs, dataSize);

    if(offs < dataSize)
      ser.GetWriter()->Write(empty, dataSize - offs);
  }

  retser.EndChunk();

  CheckError(packet, expectedPacket);
}

void ReplayProxy::GetTextureData(ResourceId tex, const Subresource &sub,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  PROXY_FUNCTION(GetTextureData, tex, sub, params, data);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_InitPostVSBuffers(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            uint32_t eventId)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_InitPostVS;
  ReplayProxyPacket packet = eReplayProxy_InitPostVS;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventId);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->InitPostVSBuffers(eventId);
  }

  SERIALISE_RETURN_VOID();
}

void ReplayProxy::InitPostVSBuffers(uint32_t eventId)
{
  PROXY_FUNCTION(InitPostVSBuffers, eventId);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_InitPostVSBuffers(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            const rdcarray<uint32_t> &events)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_InitPostVSVec;
  ReplayProxyPacket packet = eReplayProxy_InitPostVSVec;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(events);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->InitPostVSBuffers(events);
  }

  SERIALISE_RETURN_VOID();
}

void ReplayProxy::InitPostVSBuffers(const rdcarray<uint32_t> &events)
{
  PROXY_FUNCTION(InitPostVSBuffers, events);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<MeshFormat> ReplayProxy::Proxied_GetBatchPostVSBuffers(ParamSerialiser &paramser,
                                                                ReturnSerialiser &retser,
                                                                uint32_t eventId,
                                                                const rdcarray<uint32_t> &instIDs,
                                                                uint32_t viewID, MeshDataStage stage)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetPostVS;
  ReplayProxyPacket packet = eReplayProxy_GetPostVS;
  rdcarray<MeshFormat> ret = {};

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventId);
    SERIALISE_ELEMENT(instIDs);
    SERIALISE_ELEMENT(viewID);
    SERIALISE_ELEMENT(stage);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetBatchPostVSBuffers(eventId, instIDs, viewID, stage);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<MeshFormat> ReplayProxy::GetBatchPostVSBuffers(uint32_t eventId,
                                                        const rdcarray<uint32_t> &instIDs,
                                                        uint32_t viewID, MeshDataStage stage)
{
  PROXY_FUNCTION(GetBatchPostVSBuffers, eventId, instIDs, viewID, stage);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ResourceId ReplayProxy::Proxied_RenderOverlay(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                              ResourceId texid, FloatVector clearCol,
                                              DebugOverlay overlay, uint32_t eventId,
                                              const rdcarray<uint32_t> &passEvents)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_RenderOverlay;
  ReplayProxyPacket packet = eReplayProxy_RenderOverlay;
  ResourceId ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(texid);
    SERIALISE_ELEMENT(overlay);
    SERIALISE_ELEMENT(clearCol);
    SERIALISE_ELEMENT(eventId);
    SERIALISE_ELEMENT(passEvents);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->RenderOverlay(texid, clearCol, overlay, eventId, passEvents);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

ResourceId ReplayProxy::RenderOverlay(ResourceId texid, FloatVector clearCol, DebugOverlay overlay,
                                      uint32_t eventId, const rdcarray<uint32_t> &passEvents)
{
  PROXY_FUNCTION(RenderOverlay, texid, clearCol, overlay, eventId, passEvents);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<ShaderEntryPoint> ReplayProxy::Proxied_GetShaderEntryPoints(ParamSerialiser &paramser,
                                                                     ReturnSerialiser &retser,
                                                                     ResourceId id)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetShaderEntryPoints;
  ReplayProxyPacket packet = eReplayProxy_GetShaderEntryPoints;
  rdcarray<ShaderEntryPoint> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetShaderEntryPoints(id);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<ShaderEntryPoint> ReplayProxy::GetShaderEntryPoints(ResourceId id)
{
  PROXY_FUNCTION(GetShaderEntryPoints, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ShaderReflection *ReplayProxy::Proxied_GetShader(ParamSerialiser &paramser,
                                                 ReturnSerialiser &retser, ResourceId pipeline,
                                                 ResourceId shader, ShaderEntryPoint entry)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetShader;
  ReplayProxyPacket packet = eReplayProxy_GetShader;
  ShaderReflection *ret = NULL;

  // only consider eventID part of the key on APIs where shaders are mutable
  ShaderReflKey key(m_APIProps.shadersMutable ? m_EventID : 0, pipeline, shader, entry);

  if(retser.IsReading() && m_ShaderReflectionCache.find(key) != m_ShaderReflectionCache.end())
    return m_ShaderReflectionCache[key];

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(pipeline);
    SERIALISE_ELEMENT(shader);
    SERIALISE_ELEMENT(entry);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetShader(pipeline, shader, entry);
  }

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    SERIALISE_ELEMENT_OPT(ret);
    SERIALISE_ELEMENT(packet);
    ser.EndChunk();

    // if we're reading, we should have checked the cache above. If we didn't, we need to steal the
    // serialised pointer here into our cache
    if(ser.IsReading())
    {
      m_ShaderReflectionCache[key] = ret;
      ret = NULL;
    }
  }

  CheckError(packet, expectedPacket);

  return m_ShaderReflectionCache[key];
}

ShaderReflection *ReplayProxy::GetShader(ResourceId pipeline, ResourceId shader,
                                         ShaderEntryPoint entry)
{
  PROXY_FUNCTION(GetShader, pipeline, shader, entry);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcstr ReplayProxy::Proxied_DisassembleShader(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                              ResourceId pipeline, const ShaderReflection *refl,
                                              const rdcstr &target)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_DisassembleShader;
  ReplayProxyPacket packet = eReplayProxy_DisassembleShader;
  ResourceId Shader;
  ShaderEntryPoint EntryPoint;
  rdcstr ret;

  if(refl)
  {
    Shader = refl->resourceId;
    EntryPoint.name = refl->entryPoint;
    EntryPoint.stage = refl->stage;
  }

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(pipeline);
    SERIALISE_ELEMENT(Shader);
    SERIALISE_ELEMENT(EntryPoint);
    SERIALISE_ELEMENT(target);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
  {
    refl =
        m_Remote->GetShader(m_Remote->GetLiveID(pipeline), m_Remote->GetLiveID(Shader), EntryPoint);
    ret = m_Remote->DisassembleShader(pipeline, refl, target);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcstr ReplayProxy::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                      const rdcstr &target)
{
  PROXY_FUNCTION(DisassembleShader, pipeline, refl, target);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<rdcstr> ReplayProxy::Proxied_GetDisassemblyTargets(ParamSerialiser &paramser,
                                                            ReturnSerialiser &retser,
                                                            bool withPipeline)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetDisassemblyTargets;
  ReplayProxyPacket packet = eReplayProxy_GetDisassemblyTargets;
  rdcarray<rdcstr> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(withPipeline);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetDisassemblyTargets(withPipeline);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<rdcstr> ReplayProxy::GetDisassemblyTargets(bool withPipeline)
{
  PROXY_FUNCTION(GetDisassemblyTargets, withPipeline);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_FreeTargetResource(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                             ResourceId id)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_FreeTargetResource;
  ReplayProxyPacket packet = eReplayProxy_FreeTargetResource;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->FreeTargetResource(id);
  }

  SERIALISE_RETURN_VOID();
}

void ReplayProxy::FreeTargetResource(ResourceId id)
{
  PROXY_FUNCTION(FreeTargetResource, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<ShaderEncoding> ReplayProxy::Proxied_GetTargetShaderEncodings(ParamSerialiser &paramser,
                                                                       ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_GetTargetShaderEncodings;
  ReplayProxyPacket packet = eReplayProxy_GetTargetShaderEncodings;
  rdcarray<ShaderEncoding> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->GetTargetShaderEncodings();
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<ShaderEncoding> ReplayProxy::GetTargetShaderEncodings()
{
  PROXY_FUNCTION(GetTargetShaderEncodings);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_BuildTargetShader(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            ShaderEncoding sourceEncoding, const bytebuf &source,
                                            const rdcstr &entry,
                                            const ShaderCompileFlags &compileFlags,
                                            ShaderStage type, ResourceId &id, rdcstr &errors)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_BuildTargetShader;
  ReplayProxyPacket packet = eReplayProxy_BuildTargetShader;
  ResourceId ret_id;
  rdcstr ret_errors;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(sourceEncoding);
    SERIALISE_ELEMENT(source);
    SERIALISE_ELEMENT(entry);
    SERIALISE_ELEMENT(compileFlags);
    SERIALISE_ELEMENT(type);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->BuildTargetShader(sourceEncoding, source, entry, compileFlags, type, ret_id,
                                  ret_errors);
  }

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    SERIALISE_ELEMENT(ret_id);
    SERIALISE_ELEMENT(ret_errors);
    SERIALISE_ELEMENT(packet);
    ser.EndChunk();

    id = ret_id;
    errors = ret_errors;
  }

  CheckError(packet, expectedPacket);
}

void ReplayProxy::BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  PROXY_FUNCTION(BuildTargetShader, sourceEncoding, source, entry, compileFlags, type, id, errors);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_ReplaceResource(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                          ResourceId from, ResourceId to)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_ReplaceResource;
  ReplayProxyPacket packet = eReplayProxy_ReplaceResource;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(from);
    SERIALISE_ELEMENT(to);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->ReplaceResource(from, to);
  }

  if(paramser.IsWriting())
    m_LiveIDs.clear();

  SERIALISE_RETURN_VOID();
}

void ReplayProxy::ReplaceResource(ResourceId from, ResourceId to)
{
  PROXY_FUNCTION(ReplaceResource, from, to);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_RemoveReplacement(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            ResourceId id)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_RemoveReplacement;
  ReplayProxyPacket packet = eReplayProxy_RemoveReplacement;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->RemoveReplacement(id);
  }

  if(paramser.IsWriting())
    m_LiveIDs.clear();

  SERIALISE_RETURN_VOID();
}

void ReplayProxy::RemoveReplacement(ResourceId id)
{
  PROXY_FUNCTION(RemoveReplacement, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<PixelModification> ReplayProxy::Proxied_PixelHistory(
    ParamSerialiser &paramser, ReturnSerialiser &retser, rdcarray<EventUsage> events,
    ResourceId target, uint32_t x, uint32_t y, const Subresource &sub, CompType typeCast)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_PixelHistory;
  ReplayProxyPacket packet = eReplayProxy_PixelHistory;
  rdcarray<PixelModification> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(events);
    SERIALISE_ELEMENT(target);
    SERIALISE_ELEMENT(x);
    SERIALISE_ELEMENT(y);
    SERIALISE_ELEMENT(sub);
    SERIALISE_ELEMENT(typeCast);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->PixelHistory(events, target, x, y, sub, typeCast);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<PixelModification> ReplayProxy::PixelHistory(rdcarray<EventUsage> events,
                                                      ResourceId target, uint32_t x, uint32_t y,
                                                      const Subresource &sub, CompType typeCast)
{
  PROXY_FUNCTION(PixelHistory, events, target, x, y, sub, typeCast);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ShaderDebugTrace *ReplayProxy::Proxied_DebugVertex(ParamSerialiser &paramser,
                                                   ReturnSerialiser &retser, uint32_t eventId,
                                                   uint32_t vertid, uint32_t instid, uint32_t idx,
                                                   uint32_t view)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_DebugVertex;
  ReplayProxyPacket packet = eReplayProxy_DebugVertex;
  ShaderDebugTrace *ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventId);
    SERIALISE_ELEMENT(vertid);
    SERIALISE_ELEMENT(instid);
    SERIALISE_ELEMENT(idx);
    SERIALISE_ELEMENT(view);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->DebugVertex(eventId, vertid, instid, idx, view);
    else
      ret = new ShaderDebugTrace;
  }

  SERIALISE_RETURN(*ret);

  return ret;
}

ShaderDebugTrace *ReplayProxy::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                           uint32_t idx, uint32_t view)
{
  PROXY_FUNCTION(DebugVertex, eventId, vertid, instid, idx, view);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ShaderDebugTrace *ReplayProxy::Proxied_DebugPixel(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                                  uint32_t eventId, uint32_t x, uint32_t y,
                                                  const DebugPixelInputs &inputs)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_DebugPixel;
  ReplayProxyPacket packet = eReplayProxy_DebugPixel;
  ShaderDebugTrace *ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventId);
    SERIALISE_ELEMENT(x);
    SERIALISE_ELEMENT(y);
    SERIALISE_ELEMENT(inputs);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->DebugPixel(eventId, x, y, inputs);
    else
      ret = new ShaderDebugTrace;
  }

  SERIALISE_RETURN(*ret);

  return ret;
}

ShaderDebugTrace *ReplayProxy::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                          const DebugPixelInputs &inputs)
{
  PROXY_FUNCTION(DebugPixel, eventId, x, y, inputs);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ShaderDebugTrace *ReplayProxy::Proxied_DebugThread(ParamSerialiser &paramser,
                                                   ReturnSerialiser &retser, uint32_t eventId,
                                                   const rdcfixedarray<uint32_t, 3> &groupid,
                                                   const rdcfixedarray<uint32_t, 3> &threadid)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_DebugThread;
  ReplayProxyPacket packet = eReplayProxy_DebugThread;
  ShaderDebugTrace *ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventId);
    SERIALISE_ELEMENT(groupid);
    SERIALISE_ELEMENT(threadid);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->DebugThread(eventId, groupid, threadid);
    else
      ret = new ShaderDebugTrace;
  }

  SERIALISE_RETURN(*ret);

  return ret;
}

ShaderDebugTrace *ReplayProxy::DebugThread(uint32_t eventId,
                                           const rdcfixedarray<uint32_t, 3> &groupid,
                                           const rdcfixedarray<uint32_t, 3> &threadid)
{
  PROXY_FUNCTION(DebugThread, eventId, groupid, threadid);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
rdcarray<ShaderDebugState> ReplayProxy::Proxied_ContinueDebug(ParamSerialiser &paramser,
                                                              ReturnSerialiser &retser,
                                                              ShaderDebugger *debugger)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_ContinueDebug;
  ReplayProxyPacket packet = eReplayProxy_ContinueDebug;
  rdcarray<ShaderDebugState> ret;

  {
    BEGIN_PARAMS();
    uint64_t debugger_ptr = (uint64_t)(uintptr_t)debugger;
    SERIALISE_ELEMENT(debugger_ptr);
    debugger = (ShaderDebugger *)(uintptr_t)debugger_ptr;
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      ret = m_Remote->ContinueDebug(debugger);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

rdcarray<ShaderDebugState> ReplayProxy::ContinueDebug(ShaderDebugger *debugger)
{
  PROXY_FUNCTION(ContinueDebug, debugger);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_FreeDebugger(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                       ShaderDebugger *debugger)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_FreeDebugger;
  ReplayProxyPacket packet = eReplayProxy_FreeDebugger;

  {
    BEGIN_PARAMS();
    uint64_t debugger_ptr = (uint64_t)(uintptr_t)debugger;
    SERIALISE_ELEMENT(debugger_ptr);
    debugger = (ShaderDebugger *)(uintptr_t)debugger_ptr;
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->FreeDebugger(debugger);
  }

  CheckError(packet, expectedPacket);
}

void ReplayProxy::FreeDebugger(ShaderDebugger *debugger)
{
  PROXY_FUNCTION(FreeDebugger, debugger);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_SavePipelineState(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            uint32_t eventId)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_SavePipelineState;
  ReplayProxyPacket packet = eReplayProxy_SavePipelineState;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventId);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    {
      m_Remote->SavePipelineState(eventId);
    }
  }

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    if(m_APIProps.pipelineType == GraphicsAPI::D3D11)
    {
      SERIALISE_ELEMENT(*m_D3D11PipelineState);
    }
    else if(m_APIProps.pipelineType == GraphicsAPI::D3D12)
    {
      SERIALISE_ELEMENT(*m_D3D12PipelineState);
    }
    else if(m_APIProps.pipelineType == GraphicsAPI::OpenGL)
    {
      SERIALISE_ELEMENT(*m_GLPipelineState);
    }
    else if(m_APIProps.pipelineType == GraphicsAPI::Vulkan)
    {
      SERIALISE_ELEMENT(*m_VulkanPipelineState);
    }
    SERIALISE_ELEMENT(packet);
    ser.EndChunk();

    if(retser.IsReading())
    {
      if(m_APIProps.pipelineType == GraphicsAPI::D3D11 && m_D3D11PipelineState)
      {
        D3D11Pipe::Shader *stages[] = {
            &m_D3D11PipelineState->vertexShader, &m_D3D11PipelineState->hullShader,
            &m_D3D11PipelineState->domainShader, &m_D3D11PipelineState->geometryShader,
            &m_D3D11PipelineState->pixelShader,  &m_D3D11PipelineState->computeShader,
        };

        for(size_t i = 0; i < ARRAY_COUNT(stages); i++)
          if(stages[i]->resourceId != ResourceId())
            stages[i]->reflection =
                GetShader(ResourceId(), GetLiveID(stages[i]->resourceId), ShaderEntryPoint());

        if(m_D3D11PipelineState->inputAssembly.resourceId != ResourceId())
          m_D3D11PipelineState->inputAssembly.bytecode =
              GetShader(ResourceId(), GetLiveID(m_D3D11PipelineState->inputAssembly.resourceId),
                        ShaderEntryPoint());
      }
      else if(m_APIProps.pipelineType == GraphicsAPI::D3D12 && m_D3D12PipelineState)
      {
        D3D12Pipe::Shader *stages[] = {
            &m_D3D12PipelineState->vertexShader, &m_D3D12PipelineState->hullShader,
            &m_D3D12PipelineState->domainShader, &m_D3D12PipelineState->geometryShader,
            &m_D3D12PipelineState->pixelShader,  &m_D3D12PipelineState->computeShader,
            &m_D3D12PipelineState->ampShader,    &m_D3D12PipelineState->meshShader,
        };

        ResourceId pipe = GetLiveID(m_D3D12PipelineState->pipelineResourceId);

        for(size_t i = 0; i < ARRAY_COUNT(stages); i++)
          if(stages[i]->resourceId != ResourceId())
            stages[i]->reflection =
                GetShader(pipe, GetLiveID(stages[i]->resourceId), ShaderEntryPoint());
      }
      else if(m_APIProps.pipelineType == GraphicsAPI::OpenGL && m_GLPipelineState)
      {
        GLPipe::Shader *stages[] = {
            &m_GLPipelineState->vertexShader,   &m_GLPipelineState->tessControlShader,
            &m_GLPipelineState->tessEvalShader, &m_GLPipelineState->geometryShader,
            &m_GLPipelineState->fragmentShader, &m_GLPipelineState->computeShader,
        };

        for(size_t i = 0; i < ARRAY_COUNT(stages); i++)
          if(stages[i]->shaderResourceId != ResourceId())
            stages[i]->reflection =
                GetShader(ResourceId(), GetLiveID(stages[i]->shaderResourceId), ShaderEntryPoint());
      }
      else if(m_APIProps.pipelineType == GraphicsAPI::Vulkan && m_VulkanPipelineState)
      {
        VKPipe::Shader *stages[] = {
            &m_VulkanPipelineState->vertexShader,   &m_VulkanPipelineState->tessControlShader,
            &m_VulkanPipelineState->tessEvalShader, &m_VulkanPipelineState->geometryShader,
            &m_VulkanPipelineState->fragmentShader, &m_VulkanPipelineState->computeShader,
            &m_VulkanPipelineState->taskShader,     &m_VulkanPipelineState->meshShader,
        };

        ResourceId pipe = GetLiveID(m_VulkanPipelineState->graphics.pipelineResourceId);

        for(size_t i = 0; i < ARRAY_COUNT(stages); i++)
        {
          if(i == 5)
            pipe = GetLiveID(m_VulkanPipelineState->compute.pipelineResourceId);

          if(stages[i]->resourceId != ResourceId())
            stages[i]->reflection =
                GetShader(pipe, GetLiveID(stages[i]->resourceId),
                          ShaderEntryPoint(stages[i]->entryPoint, stages[i]->stage));
        }
      }
    }
  }

  CheckError(packet, expectedPacket);
}

void ReplayProxy::SavePipelineState(uint32_t eventId)
{
  PROXY_FUNCTION(SavePipelineState, eventId);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_ReplayLog(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                    uint32_t endEventID, ReplayLogType replayType)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_ReplayLog;
  ReplayProxyPacket packet = eReplayProxy_ReplayLog;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(endEventID);
    SERIALISE_ELEMENT(replayType);
    END_PARAMS();
  }

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->ReplayLog(endEventID, replayType);
  }

  if(retser.IsReading())
  {
    m_TextureProxyCache.clear();
    m_BufferProxyCache.clear();
  }

  m_EventID = endEventID;

  SERIALISE_RETURN_VOID();
}

void ReplayProxy::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  PROXY_FUNCTION(ReplayLog, endEventID, replayType);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_FetchStructuredFile(ParamSerialiser &paramser, ReturnSerialiser &retser)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_FetchStructuredFile;
  ReplayProxyPacket packet = eReplayProxy_FetchStructuredFile;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  SDFile *file = m_StructuredFile;

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      file = (SDFile *)m_Remote->GetStructuredFile();
  }

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);

    uint64_t chunkCount = file->chunks.size();
    SERIALISE_ELEMENT(chunkCount);

    if(retser.IsReading())
      file->chunks.resize((size_t)chunkCount);

    for(size_t c = 0; c < (size_t)chunkCount; c++)
    {
      if(retser.IsReading())
        file->chunks[c] = new SDChunk(""_lit);

      ser.Serialise("chunk"_lit, *file->chunks[c]);
    }

    uint64_t bufferCount = file->buffers.size();
    SERIALISE_ELEMENT(bufferCount);

    if(retser.IsReading())
      file->buffers.resize((size_t)bufferCount);

    for(size_t b = 0; b < (size_t)bufferCount; b++)
    {
      if(retser.IsReading())
        file->buffers[b] = new bytebuf;

      bytebuf *buf = file->buffers[b];

      ser.Serialise("buffer"_lit, *buf);
    }

    SERIALISE_ELEMENT(packet);

    ser.EndChunk();
  }

  CheckError(packet, expectedPacket);
}

void ReplayProxy::FetchStructuredFile()
{
  PROXY_FUNCTION(FetchStructuredFile);
}

struct DeltaSection
{
  uint64_t offs = 0;
  bytebuf contents;

  void swap(DeltaSection &o)
  {
    std::swap(offs, o.offs);
    contents.swap(o.contents);
  }
};

DECLARE_REFLECTION_STRUCT(DeltaSection);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DeltaSection &el)
{
  SERIALISE_MEMBER(offs);
  SERIALISE_MEMBER(contents);
}

template <typename SerialiserType>
void ReplayProxy::DeltaTransferBytes(SerialiserType &xferser, bytebuf &referenceData, bytebuf &newData)
{
  // lz4 compress
  if(xferser.IsReading())
  {
    uint64_t uncompSize = 0;
    xferser.Serialise("uncompSize"_lit, uncompSize);

    if(uncompSize == 0)
    {
      // fast path - no changes.
      RDCDEBUG("Unchanged");
      return;
    }
    else
    {
      rdcarray<DeltaSection> deltas;

      {
        ReadSerialiser ser(
            new StreamReader(new LZ4Decompressor(xferser.GetReader(), Ownership::Nothing),
                             uncompSize, Ownership::Stream),
            Ownership::Stream);

        SERIALISE_ELEMENT(deltas);

        // add any necessary padding.
        uint64_t offs = ser.GetReader()->GetOffset();
        RDCASSERT(offs <= uncompSize, offs, uncompSize);

        if(offs < uncompSize)
        {
          if(uncompSize - offs > 128)
          {
            RDCERR("Unexpected amount of padding: %llu", uncompSize - offs);
            m_IsErrored = true;
          }
          ser.GetReader()->Read(NULL, uncompSize - offs);
        }
      }

      if(deltas.empty())
      {
        RDCERR("Unexpected empty delta list");
        m_IsErrored = true;
      }
      else if(referenceData.empty())
      {
        // if we don't have reference data we blat the whole contents.
        // in this case we only expect one delta with the whole range
        if(deltas.size() != 1)
          RDCERR("Got more than one delta with no reference data - taking first delta.");

        referenceData = deltas.front().contents;
        RDCDEBUG("Creating new reference data, %llu bytes", (uint64_t)referenceData.size());
      }
      else
      {
        uint64_t deltaBytes = 0;

        // apply deltas to refData
        for(const DeltaSection &delta : deltas)
        {
          if(delta.offs + delta.contents.size() > referenceData.size())
          {
            RDCERR("{%llu, %llu} larger than reference data (%llu bytes) - expanding to fit.",
                   delta.offs, (uint64_t)delta.contents.size(), (uint64_t)referenceData.size());

            referenceData.resize(size_t(delta.offs + delta.contents.size()));
          }

          byte *dst = referenceData.data() + (ptrdiff_t)delta.offs;
          const byte *src = delta.contents.data();

          memcpy(dst, src, delta.contents.size());

          deltaBytes += (uint64_t)delta.contents.size();
        }

        RDCDEBUG("Applied %u deltas data, %llu total delta bytes to %llu resource size",
                 (uint32_t)deltas.size(), deltaBytes, (uint64_t)referenceData.size());
      }
    }
  }
  else
  {
    uint64_t uncompSize = 0;

    // we use a list so that we don't have to reserve and pushing new sections will never cause
    // previous ones to be reallocated and move around lots of data.
    std::list<DeltaSection> deltasList;

    if(referenceData.empty())
    {
      // no previous reference data, need to transfer the whole object.
      deltasList.resize(1);
      deltasList.back().contents = newData;
    }
    else
    {
      if(referenceData.size() != newData.size())
      {
        RDCERR("Reference data existed at %llu bytes, but new data is now %llu bytes",
               referenceData.size(), newData.size());

        // re-transfer the whole block, something went seriously wrong if the resource changed size.
        deltasList.resize(1);
        deltasList.back().contents = newData;
      }
      else
      {
        // do actual diff.
        const byte *srcBegin = newData.data();
        const byte *src = srcBegin;
        const byte *dst = referenceData.data();
        size_t bytesRemain = newData.size();

        // we only care about large-ish chunks at a time. This prevents us generating lots of tiny
        // deltas where we could batch changes together. This is tuned to not be too large (and
        // thus causing us to miss too many sections we could skip) and not too small (causing us
        // to devolve into lots of byte-wise deltas). The current value as of this comment of 128
        // is definitely on the small end of the range, but consider e.g. an android image of
        // 1440x2560 and a pixel-wide line that goes vertically from top to bottom. Reading
        // horizontally that will mean 2560 different diffs, and only actually one pixel changed.
        // The larger this value gets, the more redundant data we'll send along with.
        const size_t chunkSize = 128;

        // we use a simple state machine. Start in state 1
        //
        // State 1: No active delta. Look at the current chunk, if there's no difference move to the
        //          next chunk and stay in this state. If there is a difference, push a delta onto
        //          the list at the current offset. Copy the current chunk into the contents of the
        //          delta. Move to state 2.
        // State 2. Active delta. Look at the current chunk, if there is a difference then append
        //          the current chunk to the last delta's contents, move to the next chunk, and stay
        //          in this state. If there isn't a difference, move back to state 1 (the delta is
        //          already 'finished' so we have no need to do anything more on it).
        //
        // At any point we can end the loop, both states are 'complete' at all points.

        enum DeltaState
        {
          None,
          Active
        };
        DeltaState state = DeltaState::None;

        // loop over whole chunks
        while(bytesRemain > chunkSize)
        {
          // check if there's a difference in this chunk.
          bool chunkDiff = memcmp(src, dst, chunkSize) != 0;

          // if we're in state 1
          if(state == DeltaState::None)
          {
            // if there's a difference, append a new delta with the current offset and chunk
            // contents and move to state 2
            if(chunkDiff)
            {
              deltasList.push_back(DeltaSection());
              deltasList.back().offs = src - srcBegin;
              deltasList.back().contents.append(src, chunkSize);

              state = DeltaState::Active;
            }
          }
          // if we're in state 2
          else if(state == DeltaState::Active)
          {
            // continue to append to the delta if there's another difference in this chunk.
            if(chunkDiff)
            {
              deltasList.back().contents.append(src, chunkSize);
            }
            else
            {
              state = DeltaState::None;
            }
          }

          // move to the next chunk
          bytesRemain -= chunkSize;
          src += chunkSize;
          dst += chunkSize;
        }

        // if there are still some bytes remaining at the end of the image, smaller than the chunk
        // size, just diff directly and send if needed. We could combine this with the last delta if
        // we ended in the active state.
        if(bytesRemain > 0 && memcmp(src, dst, bytesRemain) != 0)
        {
          deltasList.push_back(DeltaSection());
          deltasList.back().offs = src - srcBegin;
          deltasList.back().contents.append(src, bytesRemain);
        }
      }
    }

    // serialise as an array, move the storage from the list into here
    rdcarray<DeltaSection> deltas;
    deltas.resize(deltasList.size());

    {
      // swap between the list and array, so all the buffers just move storage
      size_t i = 0;
      for(auto it = deltasList.begin(); it != deltasList.end(); it++)
      {
        deltas[i].swap(*it);
        i++;
      }
    }

    // fast path - no changes.
    if(deltas.empty())
    {
      uncompSize = 0;
    }
    else
    {
      // serialise to an invalid writer, to get the size of the data that will be written.
      WriteSerialiser ser(new StreamWriter(StreamWriter::InvalidStream), Ownership::Stream);

      SERIALISE_ELEMENT(deltas);

      uncompSize = ser.GetWriter()->GetOffset() + ser.GetChunkAlignment();
    }

    xferser.Serialise("uncompSize"_lit, uncompSize);

    if(uncompSize > 0)
    {
      WriteSerialiser ser(new StreamWriter(new LZ4Compressor(xferser.GetWriter(), Ownership::Nothing),
                                           Ownership::Stream),
                          Ownership::Stream);

      SERIALISE_ELEMENT(deltas);

      char empty[128] = {};

      // add any necessary padding.
      uint64_t offs = ser.GetWriter()->GetOffset();
      RDCASSERT(offs <= uncompSize, offs, uncompSize);
      RDCASSERT(uncompSize - offs < sizeof(empty), offs, uncompSize);

      if(offs < uncompSize)
        ser.GetWriter()->Write(empty, uncompSize - offs);
    }

    // This is the proxy side, so we have the complete newest contents in data. Swap the new data
    // into refData for next time.
    referenceData.swap(newData);
  }
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_CacheBufferData(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                          ResourceId buff)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_CacheBufferData;
  ReplayProxyPacket packet = eReplayProxy_CacheBufferData;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(buff);
    END_PARAMS();
  }

  bytebuf data;

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->GetBufferData(buff, 0, 0, data);
  }

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    SERIALISE_ELEMENT(packet);
  }

  DeltaTransferBytes(retser, m_ProxyBufferData[buff], data);

  retser.EndChunk();

  CheckError(packet, expectedPacket);
}

void ReplayProxy::CacheBufferData(ResourceId buff)
{
  PROXY_FUNCTION(CacheBufferData, buff);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_CacheTextureData(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                           ResourceId tex, const Subresource &sub,
                                           const GetTextureDataParams &params)
{
  const ReplayProxyPacket expectedPacket = eReplayProxy_CacheTextureData;
  ReplayProxyPacket packet = eReplayProxy_CacheTextureData;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(tex);
    SERIALISE_ELEMENT(sub);
    SERIALISE_ELEMENT(params);
    END_PARAMS();
  }

  bytebuf data;

  {
    REMOTE_EXECUTION();
    if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
      m_Remote->GetTextureData(tex, sub, params, data);
  }

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    SERIALISE_ELEMENT(packet);
  }

  TextureCacheEntry entry = {tex, sub};
  DeltaTransferBytes(retser, m_ProxyTextureData[entry], data);

  retser.EndChunk();

  CheckError(packet, expectedPacket);
}

void ReplayProxy::CacheTextureData(ResourceId tex, const Subresource &sub,
                                   const GetTextureDataParams &params)
{
  PROXY_FUNCTION(CacheTextureData, tex, sub, params);
}

#pragma endregion Proxied Functions

// If a remap is required, modify the params that are used when getting the proxy texture data
// for replay on the current driver.
void ReplayProxy::RemapProxyTextureIfNeeded(TextureDescription &tex, GetTextureDataParams &params)
{
  if(NeedRemapForFetch(tex.format))
  {
    // currently only OpenGL ES need to remap all the depth formats for fetch
    // when depth read is not supported
    params.remap = RemapTexture::RGBA32;
    tex.format.compCount = 4;
    tex.format.compByteWidth = 4;
    tex.format.compType = CompType::Float;
    tex.format.type = ResourceFormatType::Regular;
    tex.creationFlags &= ~TextureCategory::DepthTarget;
    return;
  }

  if(m_Proxy->IsTextureSupported(tex))
    return;

  if(tex.format.Special())
  {
    switch(tex.format.type)
    {
      case ResourceFormatType::S8:
        tex.format.compType = CompType::UInt;
        params.remap = RemapTexture::RGBA8;
        tex.creationFlags &= ~TextureCategory::DepthTarget;
        break;
      case ResourceFormatType::D16S8:
      case ResourceFormatType::D24S8:
      case ResourceFormatType::D32S8:
        tex.format.compType = CompType::Float;
        params.remap = RemapTexture::RGBA32;
        tex.creationFlags &= ~TextureCategory::DepthTarget;
        break;
      case ResourceFormatType::A8:
        tex.format.compType = CompType::UNorm;
        params.remap = RemapTexture::RGBA8;
        break;
      case ResourceFormatType::BC1:
      case ResourceFormatType::BC2:
      case ResourceFormatType::BC3:
      case ResourceFormatType::BC7:
        tex.format.compType = CompType::UNorm;
        params.remap = RemapTexture::RGBA8;
        break;
      case ResourceFormatType::BC4:
      case ResourceFormatType::BC5:
      case ResourceFormatType::BC6:
        tex.format.compType = CompType::Float;
        params.remap = RemapTexture::RGBA16;
        break;
      case ResourceFormatType::ASTC:
        tex.format.compType = CompType::Float;
        params.remap = RemapTexture::RGBA16;
        break;
      case ResourceFormatType::EAC:
      case ResourceFormatType::R5G6B5:
      case ResourceFormatType::R5G5B5A1:
      case ResourceFormatType::R4G4:
      case ResourceFormatType::R4G4B4A4:
      case ResourceFormatType::ETC2: params.remap = RemapTexture::RGBA8; break;
      case ResourceFormatType::R10G10B10A2: params.remap = RemapTexture::RGBA16; break;
      default:
        RDCERR("Don't know how to remap resource format type %u, falling back to RGBA32",
               tex.format.type);
        tex.format.compType = CompType::Float;
        params.remap = RemapTexture::RGBA32;
        break;
    }
  }
  else
  {
    if(tex.format.compByteWidth == 1)
    {
      params.remap = RemapTexture::RGBA8;

      if(tex.format.compType == CompType::SNorm || tex.format.compType == CompType::UNormSRGB)
      {
        params.remap = RemapTexture::RGBA16;
        tex.format.compType = CompType::Float;
      }
    }
    else if(tex.format.compByteWidth == 2)
    {
      params.remap = RemapTexture::RGBA16;
    }
    else
    {
      params.remap = RemapTexture::RGBA32;
    }

    // always remap depth to RGBA32F, because D16_UNORM will lose precision if remapped to R16_FLOAT
    if(tex.format.compType == CompType::Depth)
    {
      params.remap = RemapTexture::RGBA32;
      tex.format.compType = CompType::Float;
    }
  }

  tex.format.SetBGRAOrder(false);
  tex.format.type = ResourceFormatType::Regular;
  tex.format.compCount = 4;

  switch(params.remap)
  {
    case RemapTexture::NoRemap: RDCERR("IsTextureSupported == false, but we have no remap"); break;
    case RemapTexture::RGBA8: tex.format.compByteWidth = 1; break;
    case RemapTexture::RGBA16: tex.format.compByteWidth = 2; break;
    case RemapTexture::RGBA32: tex.format.compByteWidth = 4; break;
  }
}

void ReplayProxy::EnsureTexCached(ResourceId &texid, CompType &typeCast, const Subresource &sub)
{
  if(m_Reader.IsErrored() || m_Writer.IsErrored())
    return;

  if(m_LocalTextures.find(texid) != m_LocalTextures.end())
    return;

  if(texid == ResourceId())
    return;

  TextureCacheEntry entry = {texid, sub};

  // ignore parameters in the key which don't matter for this texture
  {
    auto it = m_TextureInfo.find(texid);
    if(it != m_TextureInfo.end())
    {
      if(it->second.mips <= 1)
        entry.sub.mip = 0;

      if(it->second.dimension == 3 || it->second.arraysize <= 1)
        entry.sub.slice = 0;

      if(it->second.msSamp <= 1)
        entry.sub.sample = 0;
    }
  }

  auto proxyit = m_ProxyTextures.find(texid);

  if(m_TextureProxyCache.find(entry) == m_TextureProxyCache.end())
  {
    if(proxyit == m_ProxyTextures.end())
    {
      TextureDescription tex = GetTexture(texid);

      ProxyTextureProperties proxy;
      RemapProxyTextureIfNeeded(tex, proxy.params);

      proxy.id = m_Proxy->CreateProxyTexture(tex);
      proxy.msSamp = RDCMAX(1U, tex.msSamp);
      proxyit = m_ProxyTextures.insert(std::make_pair(texid, proxy)).first;
    }

    const ProxyTextureProperties &proxy = proxyit->second;
    const bool allSamples = sub.sample == ~0U;

    uint32_t numSamplesToFetch = allSamples ? proxy.msSamp : 1;
    for(uint32_t sample = 0; sample < numSamplesToFetch; sample++)
    {
      Subresource s = sub;
      if(allSamples)
        s.sample = sample;

      TextureCacheEntry sampleArrayEntry = {texid, s};

      GetTextureDataParams params = proxy.params;

      params.typeCast = typeCast;
      params.standardLayout = true;

#if ENABLED(TRANSFER_RESOURCE_CONTENTS_DELTAS)
      CacheTextureData(texid, s, params);
#else
      GetTextureData(texid, s, params, m_ProxyTextureData[entry]);
#endif

      auto it = m_ProxyTextureData.find(sampleArrayEntry);
      if(it != m_ProxyTextureData.end())
        m_Proxy->SetProxyTextureData(proxy.id, s, it->second.data(), it->second.size());
    }

    m_TextureProxyCache.insert(entry);
  }

  if(proxyit->second.params.remap != RemapTexture::NoRemap)
    typeCast = BaseRemapType(proxyit->second.params.remap, typeCast);

  // change texid to the proxy texture's ID for passing to our proxy renderer
  texid = proxyit->second.id;
}

void ReplayProxy::EnsureBufCached(ResourceId bufid)
{
  if(m_Reader.IsErrored() || m_Writer.IsErrored())
    return;

  if(m_BufferProxyCache.find(bufid) == m_BufferProxyCache.end())
  {
    if(m_ProxyBufferIds.find(bufid) == m_ProxyBufferIds.end())
    {
      BufferDescription buf = GetBuffer(bufid);
      m_ProxyBufferIds[bufid] = m_Proxy->CreateProxyBuffer(buf);
    }

    ResourceId proxyid = m_ProxyBufferIds[bufid];

#if ENABLED(TRANSFER_RESOURCE_CONTENTS_DELTAS)
    CacheBufferData(bufid);
#else
    GetBufferData(bufid, 0, 0, m_ProxyBufferData[bufid]);
#endif

    auto it = m_ProxyBufferData.find(bufid);
    if(it != m_ProxyBufferData.end())
      m_Proxy->SetProxyBufferData(proxyid, it->second.data(), it->second.size());

    m_BufferProxyCache.insert(bufid);
  }
}

const ActionDescription *ReplayProxy::FindAction(const rdcarray<ActionDescription> &actionList,
                                                 uint32_t eventId)
{
  for(const ActionDescription &a : actionList)
  {
    if(!a.children.empty())
    {
      const ActionDescription *action = FindAction(a.children, eventId);
      if(action != NULL)
        return action;
    }

    if(a.eventId == eventId)
      return &a;
  }

  return NULL;
}

void ReplayProxy::InitPreviewWindow()
{
  if(m_Replay && m_PreviewWindow)
  {
    WindowingData data = m_PreviewWindow(true, m_Replay->GetSupportedWindowSystems());

    if(data.system != WindowingSystem::Unknown)
    {
      // if the data has changed, destroy the old window so we'll recreate
      if(m_PreviewWindow == 0 || memcmp(&m_PreviewWindowingData, &data, sizeof(data)) != 0)
      {
        if(m_PreviewWindow)
        {
          RDCDEBUG("Re-creating preview window due to change in data");
          m_Replay->DestroyOutputWindow(m_PreviewOutput);
        }

        m_PreviewOutput = m_Replay->MakeOutputWindow(data, false);

        m_PreviewWindowingData = data;
      }
    }

    if(m_FrameRecord.actionList.empty())
      m_FrameRecord = m_Replay->GetFrameRecord();
  }
}

void ReplayProxy::ShutdownPreviewWindow()
{
  if(m_Replay && m_PreviewOutput)
  {
    m_Replay->DestroyOutputWindow(m_PreviewOutput);
    m_PreviewOutput = 0;
  }

  if(m_PreviewWindow)
    m_PreviewWindow(false, {});
}

void ReplayProxy::RefreshPreviewWindow()
{
  InitPreviewWindow();

  if(m_Replay && m_PreviewOutput)
  {
    m_Replay->BindOutputWindow(m_PreviewOutput, false);
    m_Replay->ClearOutputWindowColor(m_PreviewOutput, FloatVector(0.0f, 0.0f, 0.0f, 1.0f));

    int32_t winWidth = 1;
    int32_t winHeight = 1;
    m_Replay->GetOutputWindowDimensions(m_PreviewOutput, winWidth, winHeight);

    m_Replay->RenderCheckerboard(RenderDoc::Inst().DarkCheckerboardColor(),
                                 RenderDoc::Inst().LightCheckerboardColor());

    const ActionDescription *curDraw = FindAction(m_FrameRecord.actionList, m_EventID);

    if(curDraw)
    {
      TextureDisplay cfg = {};

      cfg.red = cfg.green = cfg.blue = true;
      cfg.alpha = false;

      for(ResourceId id : curDraw->outputs)
      {
        if(id != ResourceId())
        {
          cfg.resourceId = id;
          break;
        }
      }

      // if we didn't get a colour target, try the depth target
      if(cfg.resourceId == ResourceId() && curDraw->depthOut != ResourceId())
      {
        cfg.resourceId = curDraw->depthOut;
        // red only for depth textures
        cfg.green = cfg.blue = false;
      }

      // if we didn't get any target, use the copy destination
      if(cfg.resourceId == ResourceId())
        cfg.resourceId = curDraw->copyDestination;

      // if we did get a texture, get the live ID for it
      if(cfg.resourceId != ResourceId())
        cfg.resourceId = m_Replay->GetLiveID(cfg.resourceId);

      if(cfg.resourceId != ResourceId())
      {
        TextureDescription texInfo = m_Replay->GetTexture(cfg.resourceId);

        cfg.typeCast = CompType::Typeless;
        cfg.rangeMin = 0.0f;
        cfg.rangeMax = 1.0f;
        cfg.flipY = false;
        cfg.hdrMultiplier = -1.0f;
        cfg.linearDisplayAsGamma = true;
        cfg.customShaderId = ResourceId();
        cfg.subresource = {0, 0, 0};
        cfg.rawOutput = false;
        cfg.backgroundColor = FloatVector(0, 0, 0, 0);
        cfg.overlay = DebugOverlay::NoOverlay;
        cfg.xOffset = 0.0f;
        cfg.yOffset = 0.0f;

        float xScale = float(winWidth) / float(texInfo.width);
        float yScale = float(winHeight) / float(texInfo.height);

        // use the smaller scale, and shrink a little so we don't display it fullscreen - makes it a
        // little clearer that this is the replay, not the original application
        cfg.scale = RDCMIN(xScale, yScale) * 0.9f;

        // centre the texture
        cfg.xOffset = (float(winWidth) - float(texInfo.width) * cfg.scale) / 2.0f;
        cfg.yOffset = (float(winHeight) - float(texInfo.height) * cfg.scale) / 2.0f;

        m_Replay->RenderTexture(cfg);
      }
    }

    m_Replay->FlipOutputWindow(m_PreviewOutput);

    m_PreviewWindow(true, m_Replay->GetSupportedWindowSystems());
  }
}

void ReplayProxy::InitRemoteExecutionThread()
{
  m_RemoteExecutionThread = Threading::CreateThread([this]() { RemoteExecutionThreadEntry(); });
}

void ReplayProxy::ShutdownRemoteExecutionThread()
{
  if(m_RemoteExecutionThread)
  {
    Atomic::Inc32(&m_RemoteExecutionKill);

    Threading::JoinThread(m_RemoteExecutionThread);
    Threading::CloseThread(m_RemoteExecutionThread);
    m_RemoteExecutionThread = 0;
  }
}

void ReplayProxy::BeginRemoteExecution()
{
  if(m_RemoteServer)
  {
    // m_RemoteExecutionActive must be inactive because it starts inactive, and we synchronise it in
    // EndRemoteExecution
    Atomic::CmpExch32(&m_RemoteExecutionState, RemoteExecution_Inactive, RemoteExecution_ThreadIdle);
  }
  else
  {
    // don't do anything, we go immediately to EndRemoteExecution and start reading packets
  }
}

void ReplayProxy::EndRemoteExecution()
{
  if(m_RemoteServer)
  {
    // wait until the thread is idle, and move it to inactive. This is unlikely to contend because
    // the thread only becomes active when it's sending a keepalive packet.
    while(Atomic::CmpExch32(&m_RemoteExecutionState, RemoteExecution_ThreadIdle,
                            RemoteExecution_Inactive) == RemoteExecution_ThreadIdle)
      Threading::Sleep(0);

    // send the finished packet
    m_Writer.BeginChunk(eReplayProxy_RemoteExecutionFinished, 0);
    m_Writer.EndChunk();
  }
  else
  {
    while(!m_Writer.IsErrored() && !m_Reader.IsErrored() && !m_IsErrored)
    {
      ReplayProxyPacket packet = m_Reader.ReadChunk<ReplayProxyPacket>();
      m_Reader.EndChunk();

      if(packet == eReplayProxy_RemoteExecutionKeepAlive)
      {
        RDCDEBUG("Got keepalive packet");
        continue;
      }

      if(packet != eReplayProxy_RemoteExecutionFinished)
      {
        CheckError(packet, eReplayProxy_RemoteExecutionFinished);
        return;
      }

      break;
    }

    CheckError(eReplayProxy_RemoteExecutionFinished, eReplayProxy_RemoteExecutionFinished);
  }
}

void ReplayProxy::RemoteExecutionThreadEntry()
{
  Threading::SetCurrentThreadName("RemoteExecutionThreadEntry");

  // while we're alive
  while(Atomic::CmpExch32(&m_RemoteExecutionKill, 0, 0) == 0)
  {
    if(IsThreadIdle())
    {
      // while we're still idle (rather than inactive), do a period of:
      // 1. Wait for 1 second, aborting if we've been moved to inactive
      // 2. Send a keepalive packet
      //
      // The wait we tradeoff between busy-waits (to catch very short-lived executions) with waits
      // to avoid spinning too much.

      while(IsThreadIdle())
      {
        PerformanceTimer waitTimer;
        while(waitTimer.GetMilliseconds() < 5 && IsThreadIdle())
          (void)waitTimer;    // wait

        if(!IsThreadIdle())
          break;

        // 5ms has elapsed, wait up to 100ms in 5ms chunks
        while(waitTimer.GetMilliseconds() < 100 && IsThreadIdle())
          Threading::Sleep(5);

        if(!IsThreadIdle())
          break;

        // 100ms has elapsed, wait up to 1000ms in 50ms chunks
        while(waitTimer.GetMilliseconds() < 1000 && IsThreadIdle())
          Threading::Sleep(50);

        if(!IsThreadIdle())
          break;

        // if we got here, it's time to send a keepalive packet. First become active so that
        // EndRemoteExecution() blocks until this finishes
        if(Atomic::CmpExch32(&m_RemoteExecutionState, RemoteExecution_ThreadIdle,
                             RemoteExecution_ThreadActive) == RemoteExecution_ThreadIdle)
        {
          m_Writer.BeginChunk(eReplayProxy_RemoteExecutionKeepAlive, 0);
          m_Writer.EndChunk();

          Atomic::CmpExch32(&m_RemoteExecutionState, RemoteExecution_ThreadActive,
                            RemoteExecution_ThreadIdle);
        }
      }
    }

    // don't busy-wait
    Threading::Sleep(0);
  }
}

RDResult ReplayProxy::FatalErrorCheck()
{
  // this isn't proxied since it's called at relatively high frequency. Whenever we proxy a
  // function, we also return the the remote side's status
  if(m_IsErrored)
  {
    // if we're error'd due to a network issue (i.e. the other side crashed and disconnected) we
    // won't have a status, set a generic one
    if(m_FatalError == ResultCode::Succeeded)
      m_FatalError = ResultCode::RemoteServerConnectionLost;

    return m_FatalError;
  }

  return ResultCode::Succeeded;
}

IReplayDriver *ReplayProxy::MakeDummyDriver()
{
  // gather up the shaders we've allocated to pass to the dummy driver
  rdcarray<ShaderReflection *> shaders;
  for(auto it = m_ShaderReflectionCache.begin(); it != m_ShaderReflectionCache.end(); ++it)
    shaders.push_back(it->second);
  m_ShaderReflectionCache.clear();

  IReplayDriver *dummy = new DummyDriver(this, shaders, m_StructuredFile);

  // the dummy driver now owns the file, remove our reference
  m_StructuredFile = NULL;

  return dummy;
}

bool ReplayProxy::CheckError(ReplayProxyPacket receivedPacket, ReplayProxyPacket expectedPacket)
{
  if(m_FatalError != ResultCode::Succeeded)
  {
    RDCERR("Fatal error detected while processing %s: %s", ToStr(expectedPacket).c_str(),
           ResultDetails(m_FatalError).Message().c_str());
    m_IsErrored = true;
    return true;
  }

  if(m_Writer.IsErrored() || m_Reader.IsErrored() || m_IsErrored)
  {
    RDCERR("Error during processing of %s", ToStr(expectedPacket).c_str());
    m_IsErrored = true;
    return true;
  }

  if(receivedPacket != expectedPacket)
  {
    RDCERR("Expected %s, received %s", ToStr(expectedPacket).c_str(), ToStr(receivedPacket).c_str());
    m_IsErrored = true;
    return true;
  }

  return false;
}

bool ReplayProxy::Tick(int type)
{
  if(!m_RemoteServer)
    return true;

  if(m_Writer.IsErrored() || m_Reader.IsErrored() || m_IsErrored)
    return false;

  ReplayProxyPacket packet = (ReplayProxyPacket)type;

  PROXY_DEBUG("Received %s", ToStr(packet).c_str());

  switch(packet)
  {
    case eReplayProxy_CacheBufferData: CacheBufferData(ResourceId()); break;
    case eReplayProxy_CacheTextureData:
      CacheTextureData(ResourceId(), Subresource(), GetTextureDataParams());
      break;
    case eReplayProxy_ReplayLog: ReplayLog(0, (ReplayLogType)0); break;
    case eReplayProxy_FetchStructuredFile: FetchStructuredFile(); break;
    case eReplayProxy_GetAPIProperties: GetAPIProperties(); break;
    case eReplayProxy_GetPassEvents: GetPassEvents(0); break;
    case eReplayProxy_GetResources: GetResources(); break;
    case eReplayProxy_GetTextures: GetTextures(); break;
    case eReplayProxy_GetTexture: GetTexture(ResourceId()); break;
    case eReplayProxy_GetBuffers: GetBuffers(); break;
    case eReplayProxy_GetBuffer: GetBuffer(ResourceId()); break;
    case eReplayProxy_GetShaderEntryPoints: GetShaderEntryPoints(ResourceId()); break;
    case eReplayProxy_GetShader: GetShader(ResourceId(), ResourceId(), ShaderEntryPoint()); break;
    case eReplayProxy_GetDebugMessages: GetDebugMessages(); break;
    case eReplayProxy_GetBufferData:
    {
      bytebuf dummy;
      GetBufferData(ResourceId(), 0, 0, dummy);
      break;
    }
    case eReplayProxy_GetTextureData:
    {
      bytebuf dummy;
      GetTextureData(ResourceId(), Subresource(), GetTextureDataParams(), dummy);
      break;
    }
    case eReplayProxy_SavePipelineState: SavePipelineState(0); break;
    case eReplayProxy_GetUsage: GetUsage(ResourceId()); break;
    case eReplayProxy_GetLiveID: GetLiveID(ResourceId()); break;
    case eReplayProxy_GetFrameRecord: GetFrameRecord(); break;
    case eReplayProxy_IsRenderOutput: IsRenderOutput(ResourceId()); break;
    case eReplayProxy_NeedRemapForFetch: NeedRemapForFetch(ResourceFormat()); break;
    case eReplayProxy_FreeTargetResource: FreeTargetResource(ResourceId()); break;
    case eReplayProxy_FetchCounters:
    {
      rdcarray<GPUCounter> counters;
      FetchCounters(counters);
      break;
    }
    case eReplayProxy_EnumerateCounters: EnumerateCounters(); break;
    case eReplayProxy_DescribeCounter: DescribeCounter(GPUCounter::EventGPUDuration); break;
    case eReplayProxy_FillCBufferVariables:
    {
      rdcarray<ShaderVariable> vars;
      bytebuf data;
      FillCBufferVariables(ResourceId(), ResourceId(), ShaderStage::Count, "", 0, vars, data);
      break;
    }
    case eReplayProxy_InitPostVS: InitPostVSBuffers(0); break;
    case eReplayProxy_InitPostVSVec:
    {
      rdcarray<uint32_t> dummy;
      InitPostVSBuffers(dummy);
      break;
    }
    case eReplayProxy_GetPostVS: GetBatchPostVSBuffers(0, {}, 0, MeshDataStage::VSIn); break;
    case eReplayProxy_BuildTargetShader:
    {
      rdcstr entry;
      ResourceId id;
      rdcstr errors;
      BuildTargetShader(ShaderEncoding::Unknown, bytebuf(), entry, ShaderCompileFlags(),
                        ShaderStage::Vertex, id, errors);
      break;
    }
    case eReplayProxy_ReplaceResource: ReplaceResource(ResourceId(), ResourceId()); break;
    case eReplayProxy_RemoveReplacement: RemoveReplacement(ResourceId()); break;
    case eReplayProxy_DebugVertex: DebugVertex(0, 0, 0, 0, 0); break;
    case eReplayProxy_DebugPixel: DebugPixel(0, 0, 0, DebugPixelInputs()); break;
    case eReplayProxy_DebugThread:
    {
      DebugThread(0, {}, {});
      break;
    }
    case eReplayProxy_ContinueDebug: ContinueDebug(NULL); break;
    case eReplayProxy_FreeDebugger: FreeDebugger(NULL); break;
    case eReplayProxy_RenderOverlay:
      RenderOverlay(ResourceId(), FloatVector(), DebugOverlay::NoOverlay, 0, rdcarray<uint32_t>());
      break;
    case eReplayProxy_PixelHistory:
      PixelHistory(rdcarray<EventUsage>(), ResourceId(), 0, 0, Subresource(), CompType::Typeless);
      break;
    case eReplayProxy_DisassembleShader: DisassembleShader(ResourceId(), NULL, ""); break;
    case eReplayProxy_GetDisassemblyTargets: GetDisassemblyTargets(false); break;
    case eReplayProxy_GetTargetShaderEncodings: GetTargetShaderEncodings(); break;
    case eReplayProxy_GetDriverInfo: GetDriverInfo(); break;
    case eReplayProxy_GetAvailableGPUs: GetAvailableGPUs(); break;
    default: RDCERR("Unexpected command %u", type); return false;
  }

  PROXY_DEBUG("Processed %s", ToStr(packet).c_str());

  if(packet == eReplayProxy_ReplayLog)
    RefreshPreviewWindow();

  if(CheckError(packet, packet))
    return false;

  return true;
}
