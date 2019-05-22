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

#pragma once

#include "os/os_specific.h"
#include "replay/replay_driver.h"
#include "serialise/serialiser.h"

// turns on/off the feature to transfer resource contents (cached textures and buffers) as a series
// of deltas to a shared view of the previous resource contents.
#define TRANSFER_RESOURCE_CONTENTS_DELTAS OPTION_ON

enum ReplayProxyPacket
{
  // we offset these packet numbers so that it can co-exist
  // peacefully with remote server packet numbers
  eReplayProxy_First = 0x1000,

  eReplayProxy_RemoteExecutionKeepAlive = eReplayProxy_First,
  eReplayProxy_RemoteExecutionFinished,

  eReplayProxy_ReplayLog,

  eReplayProxy_CacheBufferData,
  eReplayProxy_CacheTextureData,

  eReplayProxy_GetAPIProperties,
  eReplayProxy_FetchStructuredFile,

  eReplayProxy_GetPassEvents,

  eReplayProxy_GetResources,
  eReplayProxy_GetTextures,
  eReplayProxy_GetTexture,
  eReplayProxy_GetBuffers,
  eReplayProxy_GetBuffer,
  eReplayProxy_GetShaderEntryPoints,
  eReplayProxy_GetShader,
  eReplayProxy_GetDebugMessages,

  eReplayProxy_GetBufferData,
  eReplayProxy_GetTextureData,

  eReplayProxy_SavePipelineState,
  eReplayProxy_GetUsage,
  eReplayProxy_GetLiveID,
  eReplayProxy_GetFrameRecord,
  eReplayProxy_IsRenderOutput,
  eReplayProxy_NeedRemapForFetch,

  eReplayProxy_FreeTargetResource,

  eReplayProxy_FetchCounters,
  eReplayProxy_EnumerateCounters,
  eReplayProxy_DescribeCounter,
  eReplayProxy_FillCBufferVariables,

  eReplayProxy_InitPostVS,
  eReplayProxy_InitPostVSVec,
  eReplayProxy_GetPostVS,

  eReplayProxy_BuildTargetShader,
  eReplayProxy_ReplaceResource,
  eReplayProxy_RemoveReplacement,

  eReplayProxy_DebugVertex,
  eReplayProxy_DebugPixel,
  eReplayProxy_DebugThread,

  eReplayProxy_RenderOverlay,

  eReplayProxy_PixelHistory,

  eReplayProxy_DisassembleShader,
  eReplayProxy_GetDisassemblyTargets,
  eReplayProxy_GetTargetShaderEncodings,

  eReplayProxy_GetDriverInfo,
};

DECLARE_REFLECTION_ENUM(ReplayProxyPacket);

#define IMPLEMENT_FUNCTION_PROXIED(rettype, name, ...)                                  \
  rettype name(__VA_ARGS__);                                                            \
  template <typename ParamSerialiser, typename ReturnSerialiser>                        \
  rettype CONCAT(Proxied_, name)(ParamSerialiser & paramser, ReturnSerialiser & retser, \
                                 ##__VA_ARGS__);

// This class implements IReplayDriver. On the local machine where the UI is, this can then act like
// a full local replay by farming out over the network to a remote replay where necessary to
// implement some functions, and using a local proxy where necessary.
//
// This class is also used on the remote replay just so we can re-use the serialisation logic across
// the network before and after implementing the IRemoteDriver parts.
class ReplayProxy : public IReplayDriver
{
public:
  ReplayProxy(ReadSerialiser &reader, WriteSerialiser &writer, IReplayDriver *proxy)
      : m_Reader(reader),
        m_Writer(writer),
        m_Proxy(proxy),
        m_Remote(NULL),
        m_Replay(NULL),
        m_RemoteServer(false)
  {
    GetAPIProperties();
    FetchStructuredFile();
  }

  ReplayProxy(ReadSerialiser &reader, WriteSerialiser &writer, IRemoteDriver *remoteDriver,
              IReplayDriver *replayDriver, RENDERDOC_PreviewWindowCallback previewWindow)
      : m_Reader(reader),
        m_Writer(writer),
        m_Proxy(NULL),
        m_Remote(remoteDriver),
        m_Replay(replayDriver),
        m_PreviewWindow(previewWindow),
        m_RemoteServer(true)
  {
    RDCEraseEl(m_APIProps);

    InitRemoteExecutionThread();

    if(m_Replay)
      InitPreviewWindow();
  }

  virtual ~ReplayProxy();

  void InitPreviewWindow();
  void ShutdownPreviewWindow();
  void RefreshPreviewWindow();

  void InitRemoteExecutionThread();
  void ShutdownRemoteExecutionThread();
  void BeginRemoteExecution();
  void EndRemoteExecution();
  void RemoteExecutionThreadEntry();

  bool IsRemoteProxy() { return !m_RemoteServer; }
  void Shutdown() { delete this; }
  ReplayStatus ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
  {
    return ReplayStatus::Succeeded;
  }
  AMDRGPControl *GetRGPControl() { return NULL; }
  std::vector<WindowingSystem> GetSupportedWindowSystems()
  {
    if(m_Proxy)
      return m_Proxy->GetSupportedWindowSystems();
    return std::vector<WindowingSystem>();
  }
  uint64_t MakeOutputWindow(WindowingData window, bool depth)
  {
    if(m_Proxy)
      return m_Proxy->MakeOutputWindow(window, depth);
    return 0;
  }
  void DestroyOutputWindow(uint64_t id)
  {
    if(m_Proxy)
      return m_Proxy->DestroyOutputWindow(id);
  }
  bool CheckResizeOutputWindow(uint64_t id)
  {
    if(m_Proxy)
      return m_Proxy->CheckResizeOutputWindow(id);
    return false;
  }
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
  {
    if(m_Proxy)
      return m_Proxy->GetOutputWindowDimensions(id, w, h);
  }
  void SetOutputWindowDimensions(uint64_t id, int32_t w, int32_t h)
  {
    if(m_Proxy)
      m_Proxy->SetOutputWindowDimensions(id, w, h);
  }
  void GetOutputWindowData(uint64_t id, bytebuf &retData)
  {
    if(m_Proxy)
      m_Proxy->GetOutputWindowData(id, retData);
  }
  void ClearOutputWindowColor(uint64_t id, FloatVector col)
  {
    if(m_Proxy)
      return m_Proxy->ClearOutputWindowColor(id, col);
  }
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
  {
    if(m_Proxy)
      return m_Proxy->ClearOutputWindowDepth(id, depth, stencil);
  }
  void BindOutputWindow(uint64_t id, bool depth)
  {
    if(m_Proxy)
      return m_Proxy->BindOutputWindow(id, depth);
  }
  bool IsOutputWindowVisible(uint64_t id)
  {
    if(m_Proxy)
      return m_Proxy->IsOutputWindowVisible(id);
    return false;
  }
  void FlipOutputWindow(uint64_t id)
  {
    if(m_Proxy)
      return m_Proxy->FlipOutputWindow(id);
  }

  void RenderCheckerboard()
  {
    if(m_Proxy)
      return m_Proxy->RenderCheckerboard();
  }

  void RenderHighlightBox(float w, float h, float scale)
  {
    if(m_Proxy)
      return m_Proxy->RenderHighlightBox(w, h, scale);
  }

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval)
  {
    if(m_Proxy)
    {
      EnsureTexCached(texid, sliceFace, mip);
      if(texid == ResourceId() || m_ProxyTextures[texid] == ResourceId())
        return false;
      return m_Proxy->GetMinMax(m_ProxyTextures[texid], sliceFace, mip, sample, typeHint, minval,
                                maxval);
    }

    return false;
  }

  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    std::vector<uint32_t> &histogram)
  {
    if(m_Proxy)
    {
      EnsureTexCached(texid, sliceFace, mip);
      if(texid == ResourceId() || m_ProxyTextures[texid] == ResourceId())
        return false;
      return m_Proxy->GetHistogram(m_ProxyTextures[texid], sliceFace, mip, sample, typeHint, minval,
                                   maxval, channels, histogram);
    }

    return false;
  }

  bool RenderTexture(TextureDisplay cfg)
  {
    if(m_Proxy)
    {
      EnsureTexCached(cfg.resourceId, cfg.sliceFace, cfg.mip);
      if(cfg.resourceId == ResourceId() || m_ProxyTextures[cfg.resourceId] == ResourceId())
        return false;
      cfg.resourceId = m_ProxyTextures[cfg.resourceId];

      // due to OpenGL having origin bottom-left compared to the rest of the world,
      // we need to flip going in or out of GL.
      if((m_APIProps.pipelineType == GraphicsAPI::OpenGL) !=
         (m_APIProps.localRenderer == GraphicsAPI::OpenGL))
      {
        cfg.flipY = !cfg.flipY;
      }

      return m_Proxy->RenderTexture(cfg);
    }

    return false;
  }

  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, CompType typeHint, float pixel[4])
  {
    if(m_Proxy)
    {
      EnsureTexCached(texture, sliceFace, mip);
      if(texture == ResourceId() || m_ProxyTextures[texture] == ResourceId())
        return;

      texture = m_ProxyTextures[texture];

      // due to OpenGL having origin bottom-left compared to the rest of the world,
      // we need to flip going in or out of GL.
      // This is a bit more annoying here as we don't have a bool to flip, we need to
      // manually adjust y
      if((m_APIProps.pipelineType == GraphicsAPI::OpenGL) !=
         (m_APIProps.localRenderer == GraphicsAPI::OpenGL))
      {
        TextureDescription tex = m_Proxy->GetTexture(texture);
        uint32_t mipHeight = RDCMAX(1U, tex.height >> mip);
        y = (mipHeight - 1) - y;
      }

      m_Proxy->PickPixel(texture, x, y, sliceFace, mip, sample, typeHint, pixel);
    }
  }

  void RenderMesh(uint32_t eventId, const std::vector<MeshFormat> &secondaryDraws,
                  const MeshDisplay &cfg)
  {
    if(m_Proxy && cfg.position.vertexResourceId != ResourceId())
    {
      MeshDisplay proxiedCfg = cfg;

      EnsureBufCached(proxiedCfg.position.vertexResourceId);
      if(proxiedCfg.position.vertexResourceId == ResourceId() ||
         m_ProxyBufferIds[proxiedCfg.position.vertexResourceId] == ResourceId())
        return;
      proxiedCfg.position.vertexResourceId = m_ProxyBufferIds[proxiedCfg.position.vertexResourceId];

      if(proxiedCfg.second.vertexResourceId != ResourceId())
      {
        EnsureBufCached(proxiedCfg.second.vertexResourceId);
        proxiedCfg.second.vertexResourceId = m_ProxyBufferIds[proxiedCfg.second.vertexResourceId];
      }

      if(proxiedCfg.position.indexResourceId != ResourceId())
      {
        EnsureBufCached(proxiedCfg.position.indexResourceId);
        proxiedCfg.position.indexResourceId = m_ProxyBufferIds[proxiedCfg.position.indexResourceId];
      }

      std::vector<MeshFormat> secDraws = secondaryDraws;

      for(size_t i = 0; i < secDraws.size(); i++)
      {
        if(secDraws[i].vertexResourceId != ResourceId())
        {
          EnsureBufCached(secDraws[i].vertexResourceId);
          secDraws[i].vertexResourceId = m_ProxyBufferIds[secDraws[i].vertexResourceId];
        }
        if(secDraws[i].indexResourceId != ResourceId())
        {
          EnsureBufCached(secDraws[i].indexResourceId);
          secDraws[i].indexResourceId = m_ProxyBufferIds[secDraws[i].indexResourceId];
        }
      }

      m_Proxy->RenderMesh(eventId, secDraws, proxiedCfg);
    }
  }

  uint32_t PickVertex(uint32_t eventId, int32_t width, int32_t height, const MeshDisplay &cfg,
                      uint32_t x, uint32_t y)
  {
    if(m_Proxy && cfg.position.vertexResourceId != ResourceId())
    {
      MeshDisplay proxiedCfg = cfg;

      EnsureBufCached(proxiedCfg.position.vertexResourceId);
      if(proxiedCfg.position.vertexResourceId == ResourceId() ||
         m_ProxyBufferIds[proxiedCfg.position.vertexResourceId] == ResourceId())
        return ~0U;
      proxiedCfg.position.vertexResourceId = m_ProxyBufferIds[proxiedCfg.position.vertexResourceId];

      if(proxiedCfg.second.vertexResourceId != ResourceId())
      {
        EnsureBufCached(proxiedCfg.second.vertexResourceId);
        proxiedCfg.second.vertexResourceId = m_ProxyBufferIds[proxiedCfg.second.vertexResourceId];
      }

      if(proxiedCfg.position.indexResourceId != ResourceId())
      {
        EnsureBufCached(proxiedCfg.position.indexResourceId);
        proxiedCfg.position.indexResourceId = m_ProxyBufferIds[proxiedCfg.position.indexResourceId];
      }

      return m_Proxy->PickVertex(eventId, width, height, proxiedCfg, x, y);
    }

    return ~0U;
  }

  void BuildCustomShader(ShaderEncoding sourceEncoding, bytebuf source, const std::string &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId *id,
                         std::string *errors)
  {
    if(m_Proxy)
    {
      m_Proxy->BuildCustomShader(sourceEncoding, source, entry, compileFlags, type, id, errors);
    }
    else
    {
      if(id)
        *id = ResourceId();
      if(errors)
        *errors = "Unsupported BuildShader call on proxy without local renderer";
    }
  }

  rdcarray<ShaderEncoding> GetCustomShaderEncodings()
  {
    if(m_Proxy)
      return m_Proxy->GetCustomShaderEncodings();

    return {};
  }

  void FreeCustomShader(ResourceId id)
  {
    if(m_Proxy)
      m_Proxy->FreeTargetResource(id);
  }

  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint)
  {
    if(m_Proxy)
    {
      EnsureTexCached(texid, 0, mip);
      if(texid == ResourceId() || m_ProxyTextures[texid] == ResourceId())
        return ResourceId();
      texid = m_ProxyTextures[texid];
      ResourceId customResourceId =
          m_Proxy->ApplyCustomShader(shader, texid, mip, arrayIdx, sampleIdx, typeHint);
      m_LocalTextures.insert(customResourceId);
      m_ProxyTextures[customResourceId] = customResourceId;
      return customResourceId;
    }

    return ResourceId();
  }

  bool Tick(int type);

  const D3D11Pipe::State *GetD3D11PipelineState() { return &m_D3D11PipelineState; }
  const D3D12Pipe::State *GetD3D12PipelineState() { return &m_D3D12PipelineState; }
  const GLPipe::State *GetGLPipelineState() { return &m_GLPipelineState; }
  const VKPipe::State *GetVulkanPipelineState() { return &m_VulkanPipelineState; }
  const SDFile &GetStructuredFile() { return m_StructuredFile; }
  IMPLEMENT_FUNCTION_PROXIED(void, FetchStructuredFile);

  IMPLEMENT_FUNCTION_PROXIED(const std::vector<ResourceDescription> &, GetResources);

  IMPLEMENT_FUNCTION_PROXIED(std::vector<ResourceId>, GetBuffers);
  IMPLEMENT_FUNCTION_PROXIED(BufferDescription, GetBuffer, ResourceId id);

  IMPLEMENT_FUNCTION_PROXIED(std::vector<ResourceId>, GetTextures);
  IMPLEMENT_FUNCTION_PROXIED(TextureDescription, GetTexture, ResourceId id);

  IMPLEMENT_FUNCTION_PROXIED(APIProperties, GetAPIProperties);
  IMPLEMENT_FUNCTION_PROXIED(DriverInformation, GetDriverInfo);

  IMPLEMENT_FUNCTION_PROXIED(std::vector<DebugMessage>, GetDebugMessages);

  IMPLEMENT_FUNCTION_PROXIED(void, SavePipelineState, uint32_t eventId);
  IMPLEMENT_FUNCTION_PROXIED(void, ReplayLog, uint32_t endEventID, ReplayLogType replayType);

  IMPLEMENT_FUNCTION_PROXIED(std::vector<uint32_t>, GetPassEvents, uint32_t eventId);

  IMPLEMENT_FUNCTION_PROXIED(std::vector<EventUsage>, GetUsage, ResourceId id);
  IMPLEMENT_FUNCTION_PROXIED(FrameRecord, GetFrameRecord);

  IMPLEMENT_FUNCTION_PROXIED(bool, IsRenderOutput, ResourceId id);

  IMPLEMENT_FUNCTION_PROXIED(ResourceId, GetLiveID, ResourceId id);

  IMPLEMENT_FUNCTION_PROXIED(std::vector<GPUCounter>, EnumerateCounters);
  IMPLEMENT_FUNCTION_PROXIED(CounterDescription, DescribeCounter, GPUCounter counterID);
  IMPLEMENT_FUNCTION_PROXIED(std::vector<CounterResult>, FetchCounters,
                             const std::vector<GPUCounter> &counterID);

  IMPLEMENT_FUNCTION_PROXIED(void, FillCBufferVariables, ResourceId shader, std::string entryPoint,
                             uint32_t cbufSlot, rdcarray<ShaderVariable> &outvars,
                             const bytebuf &data);

  IMPLEMENT_FUNCTION_PROXIED(void, GetBufferData, ResourceId buff, uint64_t offset, uint64_t len,
                             bytebuf &retData);
  IMPLEMENT_FUNCTION_PROXIED(void, GetTextureData, ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                             const GetTextureDataParams &params, bytebuf &data);

  IMPLEMENT_FUNCTION_PROXIED(void, InitPostVSBuffers, uint32_t eventId);
  IMPLEMENT_FUNCTION_PROXIED(void, InitPostVSBuffers, const std::vector<uint32_t> &passEvents);
  IMPLEMENT_FUNCTION_PROXIED(MeshFormat, GetPostVSBuffers, uint32_t eventId, uint32_t instID,
                             uint32_t viewID, MeshDataStage stage);

  IMPLEMENT_FUNCTION_PROXIED(ResourceId, RenderOverlay, ResourceId texid, CompType typeHint,
                             DebugOverlay overlay, uint32_t eventId,
                             const std::vector<uint32_t> &passEvents);

  IMPLEMENT_FUNCTION_PROXIED(rdcarray<ShaderEntryPoint>, GetShaderEntryPoints, ResourceId shader);
  IMPLEMENT_FUNCTION_PROXIED(ShaderReflection *, GetShader, ResourceId shader,
                             ShaderEntryPoint entry);

  IMPLEMENT_FUNCTION_PROXIED(std::vector<std::string>, GetDisassemblyTargets);
  IMPLEMENT_FUNCTION_PROXIED(std::string, DisassembleShader, ResourceId pipeline,
                             const ShaderReflection *refl, const std::string &target);

  IMPLEMENT_FUNCTION_PROXIED(void, FreeTargetResource, ResourceId id);

  IMPLEMENT_FUNCTION_PROXIED(std::vector<PixelModification>, PixelHistory,
                             std::vector<EventUsage> events, ResourceId target, uint32_t x,
                             uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx,
                             CompType typeHint);
  IMPLEMENT_FUNCTION_PROXIED(ShaderDebugTrace, DebugVertex, uint32_t eventId, uint32_t vertid,
                             uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset);
  IMPLEMENT_FUNCTION_PROXIED(ShaderDebugTrace, DebugPixel, uint32_t eventId, uint32_t x, uint32_t y,
                             uint32_t sample, uint32_t primitive);
  IMPLEMENT_FUNCTION_PROXIED(ShaderDebugTrace, DebugThread, uint32_t eventId,
                             const uint32_t groupid[3], const uint32_t threadid[3]);

  IMPLEMENT_FUNCTION_PROXIED(rdcarray<ShaderEncoding>, GetTargetShaderEncodings);
  IMPLEMENT_FUNCTION_PROXIED(void, BuildTargetShader, ShaderEncoding sourceEncoding, bytebuf source,
                             const std::string &entry, const ShaderCompileFlags &compileFlags,
                             ShaderStage type, ResourceId *id, std::string *errors);
  IMPLEMENT_FUNCTION_PROXIED(void, ReplaceResource, ResourceId from, ResourceId to);
  IMPLEMENT_FUNCTION_PROXIED(void, RemoveReplacement, ResourceId id);

  // these functions are not part of the replay driver interface - they are similar to GetBufferData
  // and GetTextureData, but they do extra work to try and optimise transfer by delta-encoding the
  // difference in the returned data to the last time the resource was cached
  IMPLEMENT_FUNCTION_PROXIED(void, CacheBufferData, ResourceId buff);
  IMPLEMENT_FUNCTION_PROXIED(void, CacheTextureData, ResourceId tex, uint32_t arrayIdx,
                             uint32_t mip, const GetTextureDataParams &params);

  // utility function to serialise the contents of a byte array given the previous contents that's
  // available on both sides of the communication.
  template <typename SerialiserType>
  void DeltaTransferBytes(SerialiserType &xferser, bytebuf &referenceData, bytebuf &newData);

  void FileChanged() {}
  // will never be used
  ResourceId CreateProxyTexture(const TextureDescription &templateTex)
  {
    RDCERR("Calling proxy-render functions on a proxy serialiser");
    return ResourceId();
  }

  void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                           size_t dataSize)
  {
    RDCERR("Calling proxy-render functions on a proxy serialiser");
  }

  bool IsTextureSupported(const ResourceFormat &format) { return true; }
  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf)
  {
    RDCERR("Calling proxy-render functions on a proxy serialiser");
    return ResourceId();
  }

  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
  {
    RDCERR("Calling proxy-render functions on a proxy serialiser");
  }

private:
  void EnsureTexCached(ResourceId texid, uint32_t arrayIdx, uint32_t mip);
  void RemapProxyTextureIfNeeded(TextureDescription &tex, GetTextureDataParams &params);
  void EnsureBufCached(ResourceId bufid);
  IMPLEMENT_FUNCTION_PROXIED(bool, NeedRemapForFetch, const ResourceFormat &format);

  const DrawcallDescription *FindDraw(const rdcarray<DrawcallDescription> &drawcallList,
                                      uint32_t eventId);

  bool CheckError(ReplayProxyPacket receivedPacket, ReplayProxyPacket expectedPacket);

  struct TextureCacheEntry
  {
    ResourceId replayid;
    uint32_t arrayIdx;
    uint32_t mip;

    bool operator<(const TextureCacheEntry &o) const
    {
      if(replayid != o.replayid)
        return replayid < o.replayid;
      if(arrayIdx != o.arrayIdx)
        return arrayIdx < o.arrayIdx;
      return mip < o.mip;
    }
  };
  // this cache only exists on the client side, with the proxy renderer. This denotes cases where we
  // already have up-to-date texture data for the current event so we don't need to check for any
  // deltas. It is cleared any time we set event.
  std::set<TextureCacheEntry> m_TextureProxyCache;
  std::set<ResourceId> m_BufferProxyCache;

  struct ProxyTextureProperties
  {
    ResourceId id;
    uint32_t msSamp;
    GetTextureDataParams params;

    ProxyTextureProperties() {}
    // Create a proxy Id with the default get-data parameters.
    ProxyTextureProperties(ResourceId proxyid) : id(proxyid) {}
    operator ResourceId() const { return id; }
    bool operator==(const ResourceId &other) const { return id == other; }
  };
  // this cache only exists on the client side, with the proxy renderer. It contains the created
  // proxy textures to stand-in for remote real textures.
  std::map<ResourceId, ProxyTextureProperties> m_ProxyTextures;
  std::map<ResourceId, ResourceId> m_ProxyBufferIds;

  // this cache exists on *both* sides of the proxy connection, and must be kept in sync. It is used
  // on the remote side to determine which deltas are necessary, and then each time on the client
  // side the data is uploaded into the proxy textures above.
  std::map<TextureCacheEntry, bytebuf> m_ProxyTextureData;
  std::map<ResourceId, bytebuf> m_ProxyBufferData;

  // this lists any textures which are only created locally (e.g. custom visualisation shaders) and
  // should not be treated as proxied.
  std::set<ResourceId> m_LocalTextures;

  std::map<ResourceId, ResourceId> m_LiveIDs;

  struct ShaderReflKey
  {
    ShaderReflKey() {}
    ShaderReflKey(uint32_t eid, ResourceId i, ShaderEntryPoint e) : eventId(eid), id(i), entry(e) {}
    uint32_t eventId;
    ResourceId id;
    ShaderEntryPoint entry;
    bool operator<(const ShaderReflKey &o) const
    {
      if(eventId != o.eventId)
        return eventId < o.eventId;

      if(id != o.id)
        return id < o.id;

      return entry < o.entry;
    }
  };

  std::map<ShaderReflKey, ShaderReflection *> m_ShaderReflectionCache;

  // reader from the other side of the host <-> remote connection
  ReadSerialiser &m_Reader;
  // writer to the other side of the host <-> remote connection
  WriteSerialiser &m_Writer;

  // the local proxy replay driver when on the host side, NULL on the remote server
  IReplayDriver *m_Proxy;
  // the remote driver on the remote server, NULL on the host side
  IRemoteDriver *m_Remote;
  // an *optional* replay driver on the remote server, could be NULL if not supported by the system
  // on the remote server. Always NULL on the host side.
  // This allows us to do some extra things on the remote server such as displaying a preview window
  IReplayDriver *m_Replay;

  // true if we're the remote server, false if we're the host
  bool m_RemoteServer;

  // The callback (if provided) that handles creating and ticking a preview window on the remote
  // host.
  RENDERDOC_PreviewWindowCallback m_PreviewWindow;
  // the ID of the output window to use for previewing on the remote host. Only valid/useful if
  // m_Replay is set
  uint64_t m_PreviewOutput = 0;
  // The previous windowing data, so we can detect changes and recreate the window
  WindowingData m_PreviewWindowingData = {WindowingSystem::Unknown};

  uint32_t m_EventID = 0;

  enum RemoteExecutionState
  {
    RemoteExecution_Inactive = 0,
    RemoteExecution_ThreadIdle = 1,
    RemoteExecution_ThreadActive = 2,
  };

  volatile int32_t m_RemoteExecutionKill = 0;
  volatile int32_t m_RemoteExecutionState = RemoteExecution_Inactive;

  bool IsThreadIdle()
  {
    return Atomic::CmpExch32(&m_RemoteExecutionState, RemoteExecution_ThreadIdle,
                             RemoteExecution_ThreadIdle) == RemoteExecution_ThreadIdle;
  }

  Threading::ThreadHandle m_RemoteExecutionThread = 0;

  bool m_IsErrored = false;

  FrameRecord m_FrameRecord;
  APIProperties m_APIProps;
  std::map<ResourceId, TextureDescription> m_TextureInfo;

  std::vector<DrawcallDescription *> m_Drawcalls;

  SDFile m_StructuredFile;

  std::vector<ResourceDescription> m_Resources;

  D3D11Pipe::State m_D3D11PipelineState;
  D3D12Pipe::State m_D3D12PipelineState;
  GLPipe::State m_GLPipelineState;
  VKPipe::State m_VulkanPipelineState;
};
