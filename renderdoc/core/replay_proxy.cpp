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

#include "replay_proxy.h"
#include "3rdparty/lz4/lz4.h"
#include "serialise/lz4io.h"

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
#define END_PARAMS() ser.EndChunk();

// begin serialising a return value. We begin a chunk here in either the writing or reading case
// since this chunk is used purely to send/receive the return value and is fully handled within the
// function.
#define SERIALISE_RETURN(retval)    \
  {                                 \
    ReturnSerialiser &ser = retser; \
    PACKET_HEADER(packet);          \
    SERIALISE_ELEMENT(retval);      \
    ser.EndChunk();                 \
  }

// dispatches to the right implementation of the Proxied_ function, depending on whether we're on
// the remote server or not.
#define PROXY_FUNCTION(name, ...)                                     \
  if(m_RemoteServer)                                                  \
    return CONCAT(Proxied_, name)(m_Reader, m_Writer, ##__VA_ARGS__); \
  else                                                                \
    return CONCAT(Proxied_, name)(m_Writer, m_Reader, ##__VA_ARGS__);

ReplayProxy::~ReplayProxy()
{
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
  const ReplayProxyPacket packet = eReplayProxy_NeedRemapForFetch;
  bool ret = false;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(format);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->NeedRemapForFetch(format);

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
  const ReplayProxyPacket packet = eReplayProxy_IsRenderOutput;
  bool ret = false;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->IsRenderOutput(id);

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
  const ReplayProxyPacket packet = eReplayProxy_GetAPIProperties;
  APIProperties ret = {};

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetAPIProperties();

  SERIALISE_RETURN(ret);

  if(!m_RemoteServer)
    ret.localRenderer = m_Proxy->GetAPIProperties().localRenderer;

  m_APIProps = ret;

  return ret;
}

APIProperties ReplayProxy::GetAPIProperties()
{
  PROXY_FUNCTION(GetAPIProperties);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
std::vector<DebugMessage> ReplayProxy::Proxied_GetDebugMessages(ParamSerialiser &paramser,
                                                                ReturnSerialiser &retser)
{
  const ReplayProxyPacket packet = eReplayProxy_GetDebugMessages;
  std::vector<DebugMessage> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetDebugMessages();

  SERIALISE_RETURN(ret);

  return ret;
}

std::vector<DebugMessage> ReplayProxy::GetDebugMessages()
{
  PROXY_FUNCTION(GetDebugMessages);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
std::vector<ResourceId> ReplayProxy::Proxied_GetTextures(ParamSerialiser &paramser,
                                                         ReturnSerialiser &retser)
{
  const ReplayProxyPacket packet = eReplayProxy_GetTextures;
  std::vector<ResourceId> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetTextures();

  SERIALISE_RETURN(ret);

  return ret;
}

std::vector<ResourceId> ReplayProxy::GetTextures()
{
  PROXY_FUNCTION(GetTextures);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
TextureDescription ReplayProxy::Proxied_GetTexture(ParamSerialiser &paramser,
                                                   ReturnSerialiser &retser, ResourceId id)
{
  const ReplayProxyPacket packet = eReplayProxy_GetTexture;
  TextureDescription ret = {};

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetTexture(id);

  SERIALISE_RETURN(ret);

  return ret;
}

TextureDescription ReplayProxy::GetTexture(ResourceId id)
{
  PROXY_FUNCTION(GetTexture, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
std::vector<ResourceId> ReplayProxy::Proxied_GetBuffers(ParamSerialiser &paramser,
                                                        ReturnSerialiser &retser)
{
  const ReplayProxyPacket packet = eReplayProxy_GetBuffers;
  std::vector<ResourceId> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetBuffers();

  SERIALISE_RETURN(ret);

  return ret;
}

std::vector<ResourceId> ReplayProxy::GetBuffers()
{
  PROXY_FUNCTION(GetBuffers);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
BufferDescription ReplayProxy::Proxied_GetBuffer(ParamSerialiser &paramser,
                                                 ReturnSerialiser &retser, ResourceId id)
{
  const ReplayProxyPacket packet = eReplayProxy_GetBuffer;
  BufferDescription ret = {};

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetBuffer(id);

  SERIALISE_RETURN(ret);

  return ret;
}

BufferDescription ReplayProxy::GetBuffer(ResourceId id)
{
  PROXY_FUNCTION(GetBuffer, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
std::vector<uint32_t> ReplayProxy::Proxied_GetPassEvents(ParamSerialiser &paramser,
                                                         ReturnSerialiser &retser, uint32_t eventID)
{
  const ReplayProxyPacket packet = eReplayProxy_GetPassEvents;
  std::vector<uint32_t> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventID);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetPassEvents(eventID);

  SERIALISE_RETURN(ret);

  return ret;
}

std::vector<uint32_t> ReplayProxy::GetPassEvents(uint32_t eventID)
{
  PROXY_FUNCTION(GetPassEvents, eventID);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
std::vector<EventUsage> ReplayProxy::Proxied_GetUsage(ParamSerialiser &paramser,
                                                      ReturnSerialiser &retser, ResourceId id)
{
  const ReplayProxyPacket packet = eReplayProxy_GetUsage;
  std::vector<EventUsage> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetUsage(id);

  SERIALISE_RETURN(ret);

  return ret;
}

std::vector<EventUsage> ReplayProxy::GetUsage(ResourceId id)
{
  PROXY_FUNCTION(GetUsage, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
FrameRecord ReplayProxy::Proxied_GetFrameRecord(ParamSerialiser &paramser, ReturnSerialiser &retser)
{
  const ReplayProxyPacket packet = eReplayProxy_GetFrameRecord;
  FrameRecord ret = {};

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetFrameRecord();

  SERIALISE_RETURN(ret);

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

  const ReplayProxyPacket packet = eReplayProxy_GetLiveID;
  ResourceId ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetLiveID(id);

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
std::vector<CounterResult> ReplayProxy::Proxied_FetchCounters(ParamSerialiser &paramser,
                                                              ReturnSerialiser &retser,
                                                              const std::vector<GPUCounter> &counters)
{
  const ReplayProxyPacket packet = eReplayProxy_FetchCounters;
  std::vector<CounterResult> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(counters);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->FetchCounters(counters);

  SERIALISE_RETURN(ret);

  return ret;
}

std::vector<CounterResult> ReplayProxy::FetchCounters(const std::vector<GPUCounter> &counters)
{
  PROXY_FUNCTION(FetchCounters, counters);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
std::vector<GPUCounter> ReplayProxy::Proxied_EnumerateCounters(ParamSerialiser &paramser,
                                                               ReturnSerialiser &retser)
{
  const ReplayProxyPacket packet = eReplayProxy_EnumerateCounters;
  std::vector<GPUCounter> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->EnumerateCounters();

  SERIALISE_RETURN(ret);

  return ret;
}

std::vector<GPUCounter> ReplayProxy::EnumerateCounters()
{
  PROXY_FUNCTION(EnumerateCounters);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
CounterDescription ReplayProxy::Proxied_DescribeCounter(ParamSerialiser &paramser,
                                                        ReturnSerialiser &retser,
                                                        GPUCounter counterID)
{
  const ReplayProxyPacket packet = eReplayProxy_DescribeCounter;
  CounterDescription ret = {};

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(counterID);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->DescribeCounter(counterID);

  SERIALISE_RETURN(ret);

  return ret;
}

CounterDescription ReplayProxy::DescribeCounter(GPUCounter counterID)
{
  PROXY_FUNCTION(DescribeCounter, counterID);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_FillCBufferVariables(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                               ResourceId shader, std::string entryPoint,
                                               uint32_t cbufSlot,
                                               std::vector<ShaderVariable> &outvars,
                                               const std::vector<byte> &data)
{
  const ReplayProxyPacket packet = eReplayProxy_FillCBufferVariables;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(shader);
    SERIALISE_ELEMENT(entryPoint);
    SERIALISE_ELEMENT(cbufSlot);
    SERIALISE_ELEMENT(data);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->FillCBufferVariables(shader, entryPoint, cbufSlot, outvars, data);

  SERIALISE_RETURN(outvars);
}

void ReplayProxy::FillCBufferVariables(ResourceId shader, std::string entryPoint, uint32_t cbufSlot,
                                       std::vector<ShaderVariable> &outvars,
                                       const std::vector<byte> &data)
{
  PROXY_FUNCTION(FillCBufferVariables, shader, entryPoint, cbufSlot, outvars, data);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_GetBufferData(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                        ResourceId buff, uint64_t offset, uint64_t len,
                                        std::vector<byte> &retData)
{
  const ReplayProxyPacket packet = eReplayProxy_GetBufferData;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(buff);
    SERIALISE_ELEMENT(offset);
    SERIALISE_ELEMENT(len);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->GetBufferData(buff, offset, len, retData);

  // over-estimate of total uncompressed data written. Since the decompression chain needs to know
  // the exact uncompressed size, we over-estimate (to allow for length/padding/etc) and then pad
  // to this amount.
  uint64_t dataSize = retData.size() + 2 * retser.GetChunkAlignment();

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
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

    ser.GetWriter()->Write(empty, dataSize - offs);
  }

  retser.EndChunk();
}

void ReplayProxy::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len,
                                std::vector<byte> &retData)
{
  PROXY_FUNCTION(GetBufferData, buff, offset, len, retData);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_GetTextureData(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                         ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                         const GetTextureDataParams &params, bytebuf &data)
{
  const ReplayProxyPacket packet = eReplayProxy_GetTextureData;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(tex);
    SERIALISE_ELEMENT(arrayIdx);
    SERIALISE_ELEMENT(mip);
    SERIALISE_ELEMENT(params);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->GetTextureData(tex, arrayIdx, mip, params, data);

  // over-estimate of total uncompressed data written. Since the decompression chain needs to know
  // the exact uncompressed size, we over-estimate (to allow for length/padding/etc) and then pad
  // to this amount.
  uint64_t dataSize = data.size() + 2 * retser.GetChunkAlignment();

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
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

    ser.GetWriter()->Write(empty, dataSize - offs);
  }

  retser.EndChunk();
}

void ReplayProxy::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  PROXY_FUNCTION(GetTextureData, tex, arrayIdx, mip, params, data);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_InitPostVSBuffers(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            uint32_t eventID)
{
  const ReplayProxyPacket packet = eReplayProxy_InitPostVS;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventID);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->InitPostVSBuffers(eventID);
}

void ReplayProxy::InitPostVSBuffers(uint32_t eventID)
{
  PROXY_FUNCTION(InitPostVSBuffers, eventID);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_InitPostVSBuffers(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            const std::vector<uint32_t> &events)
{
  const ReplayProxyPacket packet = eReplayProxy_InitPostVSVec;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(events);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->InitPostVSBuffers(events);
}

void ReplayProxy::InitPostVSBuffers(const std::vector<uint32_t> &events)
{
  PROXY_FUNCTION(InitPostVSBuffers, events);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
MeshFormat ReplayProxy::Proxied_GetPostVSBuffers(ParamSerialiser &paramser,
                                                 ReturnSerialiser &retser, uint32_t eventID,
                                                 uint32_t instID, MeshDataStage stage)
{
  const ReplayProxyPacket packet = eReplayProxy_GetPostVS;
  MeshFormat ret = {};

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventID);
    SERIALISE_ELEMENT(instID);
    SERIALISE_ELEMENT(stage);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetPostVSBuffers(eventID, instID, stage);

  SERIALISE_RETURN(ret);

  return ret;
}

MeshFormat ReplayProxy::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
  PROXY_FUNCTION(GetPostVSBuffers, eventID, instID, stage);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ResourceId ReplayProxy::Proxied_RenderOverlay(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                              ResourceId texid, CompType typeHint,
                                              DebugOverlay overlay, uint32_t eventID,
                                              const std::vector<uint32_t> &passEvents)
{
  const ReplayProxyPacket packet = eReplayProxy_RenderOverlay;
  ResourceId ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(texid);
    SERIALISE_ELEMENT(typeHint);
    SERIALISE_ELEMENT(overlay);
    SERIALISE_ELEMENT(eventID);
    SERIALISE_ELEMENT(passEvents);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->RenderOverlay(texid, typeHint, overlay, eventID, passEvents);

  SERIALISE_RETURN(ret);

  return ret;
}

ResourceId ReplayProxy::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                      uint32_t eventID, const std::vector<uint32_t> &passEvents)
{
  PROXY_FUNCTION(RenderOverlay, texid, typeHint, overlay, eventID, passEvents);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ShaderReflection *ReplayProxy::Proxied_GetShader(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                                 ResourceId id, std::string entryPoint)
{
  const ReplayProxyPacket packet = eReplayProxy_GetShader;
  ShaderReflection *ret;

  ShaderReflKey key(id, entryPoint);

  if(retser.IsReading() && m_ShaderReflectionCache.find(key) != m_ShaderReflectionCache.end())
    return m_ShaderReflectionCache[key];

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    SERIALISE_ELEMENT(entryPoint);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetShader(id, entryPoint);

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    SERIALISE_ELEMENT_OPT(ret);
    ser.EndChunk();

    // if we're reading, we should have checked the cache above. If we didn't, we need to steal the
    // serialised pointer here into our cache
    if(ser.IsReading())
    {
      m_ShaderReflectionCache[key] = ret;
      ret = NULL;
    }
  }

  return m_ShaderReflectionCache[key];
}

ShaderReflection *ReplayProxy::GetShader(ResourceId id, std::string entryPoint)
{
  PROXY_FUNCTION(GetShader, id, entryPoint);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
std::string ReplayProxy::Proxied_DisassembleShader(ParamSerialiser &paramser,
                                                   ReturnSerialiser &retser, ResourceId pipeline,
                                                   const ShaderReflection *refl,
                                                   const std::string &target)
{
  const ReplayProxyPacket packet = eReplayProxy_DisassembleShader;
  ResourceId Shader;
  std::string EntryPoint;
  std::string ret;

  if(refl)
  {
    Shader = refl->ID;
    EntryPoint = refl->EntryPoint;
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
    refl = m_Remote->GetShader(m_Remote->GetLiveID(Shader), EntryPoint);
    ret = m_Remote->DisassembleShader(pipeline, refl, target);
  }

  SERIALISE_RETURN(ret);

  return ret;
}

std::string ReplayProxy::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                           const std::string &target)
{
  PROXY_FUNCTION(DisassembleShader, pipeline, refl, target);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
std::vector<std::string> ReplayProxy::Proxied_GetDisassemblyTargets(ParamSerialiser &paramser,
                                                                    ReturnSerialiser &retser)
{
  const ReplayProxyPacket packet = eReplayProxy_GetDisassemblyTargets;
  std::vector<std::string> ret;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->GetDisassemblyTargets();

  SERIALISE_RETURN(ret);

  return ret;
}

std::vector<std::string> ReplayProxy::GetDisassemblyTargets()
{
  PROXY_FUNCTION(GetDisassemblyTargets);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_FreeTargetResource(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                             ResourceId id)
{
  const ReplayProxyPacket packet = eReplayProxy_FreeTargetResource;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->FreeTargetResource(id);
}

void ReplayProxy::FreeTargetResource(ResourceId id)
{
  PROXY_FUNCTION(FreeTargetResource, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_BuildTargetShader(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            std::string source, std::string entry,
                                            const ShaderCompileFlags &compileFlags,
                                            ShaderStage type, ResourceId *id, std::string *errors)
{
  const ReplayProxyPacket packet = eReplayProxy_BuildTargetShader;
  ResourceId ret_id;
  std::string ret_errors;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(source);
    SERIALISE_ELEMENT(entry);
    SERIALISE_ELEMENT(compileFlags);
    SERIALISE_ELEMENT(type);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->BuildTargetShader(source, entry, compileFlags, type, &ret_id, &ret_errors);

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    SERIALISE_ELEMENT(ret_id);
    SERIALISE_ELEMENT(ret_errors);
    ser.EndChunk();

    if(id)
      *id = ret_id;
    if(errors)
      *errors = ret_errors;
  }
}

void ReplayProxy::BuildTargetShader(std::string source, std::string entry,
                                    const ShaderCompileFlags &compileFlags, ShaderStage type,
                                    ResourceId *id, std::string *errors)
{
  PROXY_FUNCTION(BuildTargetShader, source, entry, compileFlags, type, id, errors);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_ReplaceResource(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                          ResourceId from, ResourceId to)
{
  const ReplayProxyPacket packet = eReplayProxy_ReplaceResource;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(from);
    SERIALISE_ELEMENT(to);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->ReplaceResource(from, to);
}

void ReplayProxy::ReplaceResource(ResourceId from, ResourceId to)
{
  PROXY_FUNCTION(ReplaceResource, from, to);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_RemoveReplacement(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                            ResourceId id)
{
  const ReplayProxyPacket packet = eReplayProxy_RemoveReplacement;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(id);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->RemoveReplacement(id);
}

void ReplayProxy::RemoveReplacement(ResourceId id)
{
  PROXY_FUNCTION(RemoveReplacement, id);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
std::vector<PixelModification> ReplayProxy::Proxied_PixelHistory(
    ParamSerialiser &paramser, ReturnSerialiser &retser, std::vector<EventUsage> events,
    ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx,
    CompType typeHint)
{
  const ReplayProxyPacket packet = eReplayProxy_PixelHistory;
  std::vector<PixelModification> ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(events);
    SERIALISE_ELEMENT(target);
    SERIALISE_ELEMENT(x);
    SERIALISE_ELEMENT(y);
    SERIALISE_ELEMENT(slice);
    SERIALISE_ELEMENT(mip);
    SERIALISE_ELEMENT(sampleIdx);
    SERIALISE_ELEMENT(typeHint);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->PixelHistory(events, target, x, y, slice, mip, sampleIdx, typeHint);

  SERIALISE_RETURN(ret);

  return ret;
}

std::vector<PixelModification> ReplayProxy::PixelHistory(std::vector<EventUsage> events,
                                                         ResourceId target, uint32_t x, uint32_t y,
                                                         uint32_t slice, uint32_t mip,
                                                         uint32_t sampleIdx, CompType typeHint)
{
  PROXY_FUNCTION(PixelHistory, events, target, x, y, slice, mip, sampleIdx, typeHint);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ShaderDebugTrace ReplayProxy::Proxied_DebugVertex(ParamSerialiser &paramser,
                                                  ReturnSerialiser &retser, uint32_t eventID,
                                                  uint32_t vertid, uint32_t instid, uint32_t idx,
                                                  uint32_t instOffset, uint32_t vertOffset)
{
  const ReplayProxyPacket packet = eReplayProxy_DebugVertex;
  ShaderDebugTrace ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventID);
    SERIALISE_ELEMENT(vertid);
    SERIALISE_ELEMENT(instid);
    SERIALISE_ELEMENT(idx);
    SERIALISE_ELEMENT(instOffset);
    SERIALISE_ELEMENT(vertOffset);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->DebugVertex(eventID, vertid, instid, idx, instOffset, vertOffset);

  SERIALISE_RETURN(ret);

  return ret;
}

ShaderDebugTrace ReplayProxy::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  PROXY_FUNCTION(DebugVertex, eventID, vertid, instid, idx, instOffset, vertOffset);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ShaderDebugTrace ReplayProxy::Proxied_DebugPixel(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                                 uint32_t eventID, uint32_t x, uint32_t y,
                                                 uint32_t sample, uint32_t primitive)
{
  const ReplayProxyPacket packet = eReplayProxy_DebugPixel;
  ShaderDebugTrace ret;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventID);
    SERIALISE_ELEMENT(x);
    SERIALISE_ELEMENT(y);
    SERIALISE_ELEMENT(sample);
    SERIALISE_ELEMENT(primitive);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->DebugPixel(eventID, x, y, sample, primitive);

  SERIALISE_RETURN(ret);

  return ret;
}

ShaderDebugTrace ReplayProxy::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  PROXY_FUNCTION(DebugPixel, eventID, x, y, sample, primitive);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
ShaderDebugTrace ReplayProxy::Proxied_DebugThread(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                                  uint32_t eventID, const uint32_t groupid[3],
                                                  const uint32_t threadid[3])
{
  const ReplayProxyPacket packet = eReplayProxy_DebugThread;
  ShaderDebugTrace ret;

  uint32_t GroupID[3] = {groupid[0], groupid[1], groupid[2]};
  uint32_t ThreadID[3] = {threadid[0], threadid[1], threadid[2]};

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(eventID);
    SERIALISE_ELEMENT(GroupID);
    SERIALISE_ELEMENT(ThreadID);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    ret = m_Remote->DebugThread(eventID, GroupID, ThreadID);

  SERIALISE_RETURN(ret);

  return ret;
}

ShaderDebugTrace ReplayProxy::DebugThread(uint32_t eventID, const uint32_t groupid[3],
                                          const uint32_t threadid[3])
{
  PROXY_FUNCTION(DebugThread, eventID, groupid, threadid);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_SavePipelineState(ParamSerialiser &paramser, ReturnSerialiser &retser)
{
  const ReplayProxyPacket packet = eReplayProxy_SavePipelineState;

  {
    BEGIN_PARAMS();
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
  {
    m_Remote->SavePipelineState();

    if(m_APIProps.pipelineType == GraphicsAPI::D3D11)
      m_D3D11PipelineState = m_Remote->GetD3D11PipelineState();
    else if(m_APIProps.pipelineType == GraphicsAPI::D3D12)
      m_D3D12PipelineState = m_Remote->GetD3D12PipelineState();
    else if(m_APIProps.pipelineType == GraphicsAPI::OpenGL)
      m_GLPipelineState = m_Remote->GetGLPipelineState();
    else if(m_APIProps.pipelineType == GraphicsAPI::Vulkan)
      m_VulkanPipelineState = m_Remote->GetVulkanPipelineState();
  }

  {
    ReturnSerialiser &ser = retser;
    PACKET_HEADER(packet);
    if(m_APIProps.pipelineType == GraphicsAPI::D3D11)
    {
      SERIALISE_ELEMENT(m_D3D11PipelineState);
    }
    else if(m_APIProps.pipelineType == GraphicsAPI::D3D12)
    {
      SERIALISE_ELEMENT(m_D3D12PipelineState);
    }
    else if(m_APIProps.pipelineType == GraphicsAPI::OpenGL)
    {
      SERIALISE_ELEMENT(m_GLPipelineState);
    }
    else if(m_APIProps.pipelineType == GraphicsAPI::Vulkan)
    {
      SERIALISE_ELEMENT(m_VulkanPipelineState);
    }
    ser.EndChunk();

    if(retser.IsReading())
    {
      if(m_APIProps.pipelineType == GraphicsAPI::D3D11)
      {
        D3D11Pipe::Shader *stages[] = {
            &m_D3D11PipelineState.m_VS, &m_D3D11PipelineState.m_HS, &m_D3D11PipelineState.m_DS,
            &m_D3D11PipelineState.m_GS, &m_D3D11PipelineState.m_PS, &m_D3D11PipelineState.m_CS,
        };

        for(int i = 0; i < 6; i++)
          if(stages[i]->Object != ResourceId())
            stages[i]->ShaderDetails = GetShader(GetLiveID(stages[i]->Object), "");

        if(m_D3D11PipelineState.m_IA.layout != ResourceId())
          m_D3D11PipelineState.m_IA.Bytecode =
              GetShader(GetLiveID(m_D3D11PipelineState.m_IA.layout), "");
      }
      else if(m_APIProps.pipelineType == GraphicsAPI::D3D12)
      {
        D3D12Pipe::Shader *stages[] = {
            &m_D3D12PipelineState.m_VS, &m_D3D12PipelineState.m_HS, &m_D3D12PipelineState.m_DS,
            &m_D3D12PipelineState.m_GS, &m_D3D12PipelineState.m_PS, &m_D3D12PipelineState.m_CS,
        };

        for(int i = 0; i < 6; i++)
          if(stages[i]->Object != ResourceId())
            stages[i]->ShaderDetails = GetShader(GetLiveID(stages[i]->Object), "");
      }
      else if(m_APIProps.pipelineType == GraphicsAPI::OpenGL)
      {
        GLPipe::Shader *stages[] = {
            &m_GLPipelineState.m_VS, &m_GLPipelineState.m_TCS, &m_GLPipelineState.m_TES,
            &m_GLPipelineState.m_GS, &m_GLPipelineState.m_FS,  &m_GLPipelineState.m_CS,
        };

        for(int i = 0; i < 6; i++)
          if(stages[i]->Object != ResourceId())
            stages[i]->ShaderDetails = GetShader(GetLiveID(stages[i]->Object), "");
      }
      else if(m_APIProps.pipelineType == GraphicsAPI::Vulkan)
      {
        VKPipe::Shader *stages[] = {
            &m_VulkanPipelineState.m_VS, &m_VulkanPipelineState.m_TCS, &m_VulkanPipelineState.m_TES,
            &m_VulkanPipelineState.m_GS, &m_VulkanPipelineState.m_FS,  &m_VulkanPipelineState.m_CS,
        };

        for(int i = 0; i < 6; i++)
          if(stages[i]->Object != ResourceId())
            stages[i]->ShaderDetails = GetShader(GetLiveID(stages[i]->Object), stages[i]->entryPoint);
      }
    }
  }
}

void ReplayProxy::SavePipelineState()
{
  PROXY_FUNCTION(SavePipelineState);
}

template <typename ParamSerialiser, typename ReturnSerialiser>
void ReplayProxy::Proxied_ReplayLog(ParamSerialiser &paramser, ReturnSerialiser &retser,
                                    uint32_t endEventID, ReplayLogType replayType)
{
  const ReplayProxyPacket packet = eReplayProxy_ReplayLog;

  {
    BEGIN_PARAMS();
    SERIALISE_ELEMENT(endEventID);
    SERIALISE_ELEMENT(replayType);
    END_PARAMS();
  }

  if(paramser.IsReading() && !paramser.IsErrored() && !m_IsErrored)
    m_Remote->ReplayLog(endEventID, replayType);

  if(retser.IsReading())
  {
    m_TextureProxyCache.clear();
    m_BufferProxyCache.clear();

    if(m_APIProps.shadersMutable)
    {
      for(auto it = m_ShaderReflectionCache.begin(); it != m_ShaderReflectionCache.end(); ++it)
        delete it->second;
      m_ShaderReflectionCache.clear();
    }
  }
}

void ReplayProxy::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  PROXY_FUNCTION(ReplayLog, endEventID, replayType);
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

  if(m_Proxy->IsTextureSupported(tex.format))
    return;

  if(tex.format.Special())
  {
    switch(tex.format.type)
    {
      case ResourceFormatType::S8:
      case ResourceFormatType::D16S8: params.remap = RemapTexture::D32S8; break;
      case ResourceFormatType::ASTC: params.remap = RemapTexture::RGBA16; break;
      case ResourceFormatType::EAC:
      case ResourceFormatType::R5G6B5:
      case ResourceFormatType::ETC2: params.remap = RemapTexture::RGBA8; break;
      default:
        RDCERR("Don't know how to remap resource format type %u, falling back to RGBA32",
               tex.format.type);
        params.remap = RemapTexture::RGBA32;
        break;
    }
    tex.format.type = ResourceFormatType::Regular;
  }
  else
  {
    if(tex.format.compByteWidth == 4)
      params.remap = RemapTexture::RGBA32;
    else if(tex.format.compByteWidth == 2)
      params.remap = RemapTexture::RGBA16;
    else if(tex.format.compByteWidth == 1)
      params.remap = RemapTexture::RGBA8;
  }

  switch(params.remap)
  {
    case RemapTexture::NoRemap: RDCERR("IsTextureSupported == false, but we have no remap"); break;
    case RemapTexture::RGBA8:
      tex.format.compCount = 4;
      tex.format.compByteWidth = 1;
      tex.format.compType = CompType::UNorm;
      // Range adaptation is only needed when remapping a higher precision format down to RGBA8.
      params.whitePoint = 1.0f;
      break;
    case RemapTexture::RGBA16:
      tex.format.compCount = 4;
      tex.format.compByteWidth = 2;
      tex.format.compType = CompType::Float;
      break;
    case RemapTexture::RGBA32:
      tex.format.compCount = 4;
      tex.format.compByteWidth = 4;
      tex.format.compType = CompType::Float;
      break;
    case RemapTexture::D32S8: RDCERR("Remapping depth/stencil formats not implemented."); break;
  }
}

void ReplayProxy::EnsureTexCached(ResourceId texid, uint32_t arrayIdx, uint32_t mip)
{
  if(m_Reader.IsErrored() || m_Writer.IsErrored())
    return;

  TextureCacheEntry entry = {texid, arrayIdx, mip};

  if(m_LocalTextures.find(texid) != m_LocalTextures.end())
    return;

  if(m_TextureProxyCache.find(entry) == m_TextureProxyCache.end())
  {
    if(m_ProxyTextures.find(texid) == m_ProxyTextures.end())
    {
      TextureDescription tex = GetTexture(texid);

      ProxyTextureProperties proxy;
      RemapProxyTextureIfNeeded(tex, proxy.params);

      proxy.id = m_Proxy->CreateProxyTexture(tex);
      m_ProxyTextures[texid] = proxy;
    }

    const ProxyTextureProperties &proxy = m_ProxyTextures[texid];

    bytebuf data;
    GetTextureData(texid, arrayIdx, mip, proxy.params, data);

    if(!data.empty())
      m_Proxy->SetProxyTextureData(proxy.id, arrayIdx, mip, data.data(), data.size());

    m_TextureProxyCache.insert(entry);
  }
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

    vector<byte> data;
    GetBufferData(bufid, 0, 0, data);

    if(!data.empty())
      m_Proxy->SetProxyBufferData(proxyid, &data[0], data.size());

    m_BufferProxyCache.insert(bufid);
  }
}

bool ReplayProxy::Tick(int type)
{
  if(!m_RemoteServer)
    return true;

  if(m_Writer.IsErrored() || m_Reader.IsErrored() || m_IsErrored)
    return false;

  switch(type)
  {
    case eReplayProxy_ReplayLog: ReplayLog(0, (ReplayLogType)0); break;
    case eReplayProxy_GetAPIProperties: GetAPIProperties(); break;
    case eReplayProxy_GetPassEvents: GetPassEvents(0); break;
    case eReplayProxy_GetTextures: GetTextures(); break;
    case eReplayProxy_GetTexture: GetTexture(ResourceId()); break;
    case eReplayProxy_GetBuffers: GetBuffers(); break;
    case eReplayProxy_GetBuffer: GetBuffer(ResourceId()); break;
    case eReplayProxy_GetShader: GetShader(ResourceId(), ""); break;
    case eReplayProxy_GetDebugMessages: GetDebugMessages(); break;
    case eReplayProxy_GetBufferData:
    {
      std::vector<byte> dummy;
      GetBufferData(ResourceId(), 0, 0, dummy);
      break;
    }
    case eReplayProxy_GetTextureData:
    {
      bytebuf dummy;
      GetTextureData(ResourceId(), 0, 0, GetTextureDataParams(), dummy);
      break;
    }
    case eReplayProxy_SavePipelineState: SavePipelineState(); break;
    case eReplayProxy_GetUsage: GetUsage(ResourceId()); break;
    case eReplayProxy_GetLiveID: GetLiveID(ResourceId()); break;
    case eReplayProxy_GetFrameRecord: GetFrameRecord(); break;
    case eReplayProxy_IsRenderOutput: IsRenderOutput(ResourceId()); break;
    case eReplayProxy_NeedRemapForFetch: NeedRemapForFetch(ResourceFormat()); break;
    case eReplayProxy_FreeTargetResource: FreeTargetResource(ResourceId()); break;
    case eReplayProxy_FetchCounters:
    {
      std::vector<GPUCounter> counters;
      FetchCounters(counters);
      break;
    }
    case eReplayProxy_EnumerateCounters: EnumerateCounters(); break;
    case eReplayProxy_DescribeCounter: DescribeCounter(GPUCounter::EventGPUDuration); break;
    case eReplayProxy_FillCBufferVariables:
    {
      std::vector<ShaderVariable> vars;
      std::vector<byte> data;
      FillCBufferVariables(ResourceId(), "", 0, vars, data);
      break;
    }
    case eReplayProxy_InitPostVS: InitPostVSBuffers(0); break;
    case eReplayProxy_InitPostVSVec:
    {
      std::vector<uint32_t> dummy;
      InitPostVSBuffers(dummy);
      break;
    }
    case eReplayProxy_GetPostVS: GetPostVSBuffers(0, 0, MeshDataStage::Unknown); break;
    case eReplayProxy_BuildTargetShader:
      BuildTargetShader("", "", ShaderCompileFlags(), ShaderStage::Vertex, NULL, NULL);
      break;
    case eReplayProxy_ReplaceResource: ReplaceResource(ResourceId(), ResourceId()); break;
    case eReplayProxy_RemoveReplacement: RemoveReplacement(ResourceId()); break;
    case eReplayProxy_DebugVertex: DebugVertex(0, 0, 0, 0, 0, 0); break;
    case eReplayProxy_DebugPixel: DebugPixel(0, 0, 0, 0, 0); break;
    case eReplayProxy_DebugThread:
    {
      uint32_t dummy1[3] = {0};
      uint32_t dummy2[3] = {0};
      DebugThread(0, dummy1, dummy2);
      break;
    }
    case eReplayProxy_RenderOverlay:
      RenderOverlay(ResourceId(), CompType::Typeless, DebugOverlay::NoOverlay, 0, vector<uint32_t>());
      break;
    case eReplayProxy_PixelHistory:
      PixelHistory(vector<EventUsage>(), ResourceId(), 0, 0, 0, 0, 0, CompType::Typeless);
      break;
    case eReplayProxy_DisassembleShader: DisassembleShader(ResourceId(), NULL, ""); break;
    case eReplayProxy_GetDisassemblyTargets: GetDisassemblyTargets(); break;
    default: RDCERR("Unexpected command %u", type); return false;
  }

  if(m_Writer.IsErrored() || m_Reader.IsErrored() || m_IsErrored)
    return false;

  return true;
}
