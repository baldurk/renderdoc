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

#include "os/os_specific.h"
#include "replay/replay_driver.h"
#include "serialise/serialiser.h"
#include "socket_helpers.h"

enum ReplayProxyPacket
{
  // we offset these packet numbers so that it can co-exist
  // peacefully with remote server packet numbers
  eReplayProxy_First = 0x1000,

  eReplayProxy_ReplayLog = eReplayProxy_First,

  eReplayProxy_GetPassEvents,

  eReplayProxy_GetTextures,
  eReplayProxy_GetTexture,
  eReplayProxy_GetBuffers,
  eReplayProxy_GetBuffer,
  eReplayProxy_GetShader,
  eReplayProxy_GetDebugMessages,

  eReplayProxy_GetBufferData,
  eReplayProxy_GetTextureData,

  eReplayProxy_SavePipelineState,
  eReplayProxy_GetUsage,
  eReplayProxy_GetLiveID,
  eReplayProxy_GetFrameRecord,
  eReplayProxy_IsRenderOutput,

  eReplayProxy_FreeResource,
  eReplayProxy_HasResolver,

  eReplayProxy_FetchCounters,
  eReplayProxy_EnumerateCounters,
  eReplayProxy_DescribeCounter,
  eReplayProxy_FillCBufferVariables,

  eReplayProxy_InitPostVS,
  eReplayProxy_InitPostVSVec,
  eReplayProxy_GetPostVS,

  eReplayProxy_InitStackResolver,
  eReplayProxy_HasStackResolver,
  eReplayProxy_GetAddressDetails,

  eReplayProxy_BuildTargetShader,
  eReplayProxy_ReplaceResource,
  eReplayProxy_RemoveReplacement,

  eReplayProxy_DebugVertex,
  eReplayProxy_DebugPixel,
  eReplayProxy_DebugThread,

  eReplayProxy_RenderOverlay,

  eReplayProxy_GetAPIProperties,

  eReplayProxy_PixelHistory,

  eReplayProxy_DisassembleShader,
  eReplayProxy_GetISATargets,
};

// This class implements IReplayDriver and StackResolver. On the local machine where the UI
// is, this can then act like a full local replay by farming out over the network to a remote
// replay where necessary to implement some functions, and using a local proxy where necessary.
//
// This class is also used on the remote replay just so we can re-use the serialisation logic
// across the network before and after implementing the IRemoteDriver parts.
class ReplayProxy : public IReplayDriver, Callstack::StackResolver
{
public:
  ReplayProxy(Network::Socket *sock, IReplayDriver *proxy)
      : m_Socket(sock), m_Proxy(proxy), m_Remote(NULL), m_RemoteServer(false)
  {
    m_FromReplaySerialiser = NULL;
    m_ToReplaySerialiser = new Serialiser(NULL, Serialiser::WRITING, false);
    m_RemoteHasResolver = false;

    GetAPIProperties();
  }

  ReplayProxy(Network::Socket *sock, IRemoteDriver *remote)
      : m_Socket(sock), m_Proxy(NULL), m_Remote(remote), m_RemoteServer(true)
  {
    m_ToReplaySerialiser = NULL;
    m_FromReplaySerialiser = new Serialiser(NULL, Serialiser::WRITING, false);
    m_RemoteHasResolver = false;

    RDCEraseEl(m_APIProps);
  }

  virtual ~ReplayProxy();

  bool IsRemoteProxy() { return !m_RemoteServer; }
  void Shutdown() { delete this; }
  void ReadLogInitialisation() {}
  vector<WindowingSystem> GetSupportedWindowSystems()
  {
    if(m_Proxy)
      return m_Proxy->GetSupportedWindowSystems();
    return vector<WindowingSystem>();
  }
  uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth)
  {
    if(m_Proxy)
      return m_Proxy->MakeOutputWindow(system, data, depth);
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
  void ClearOutputWindowColor(uint64_t id, float col[4])
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

  void RenderCheckerboard(Vec3f light, Vec3f dark)
  {
    if(m_Proxy)
      return m_Proxy->RenderCheckerboard(light, dark);
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
                    vector<uint32_t> &histogram)
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
      EnsureTexCached(cfg.texid, cfg.sliceFace, cfg.mip);
      if(cfg.texid == ResourceId() || m_ProxyTextures[cfg.texid] == ResourceId())
        return false;
      cfg.texid = m_ProxyTextures[cfg.texid];

      // due to OpenGL having origin bottom-left compared to the rest of the world,
      // we need to flip going in or out of GL.
      if((m_APIProps.pipelineType == GraphicsAPI::OpenGL) !=
         (m_APIProps.localRenderer == GraphicsAPI::OpenGL))
      {
        cfg.FlipY = !cfg.FlipY;
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

  void RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg)
  {
    if(m_Proxy && cfg.position.buf != ResourceId())
    {
      MeshDisplay proxiedCfg = cfg;

      EnsureBufCached(proxiedCfg.position.buf);
      if(proxiedCfg.position.buf == ResourceId() ||
         m_ProxyBufferIds[proxiedCfg.position.buf] == ResourceId())
        return;
      proxiedCfg.position.buf = m_ProxyBufferIds[proxiedCfg.position.buf];

      if(proxiedCfg.second.buf != ResourceId())
      {
        EnsureBufCached(proxiedCfg.second.buf);
        proxiedCfg.second.buf = m_ProxyBufferIds[proxiedCfg.second.buf];
      }

      if(proxiedCfg.position.idxbuf != ResourceId())
      {
        EnsureBufCached(proxiedCfg.position.idxbuf);
        proxiedCfg.position.idxbuf = m_ProxyBufferIds[proxiedCfg.position.idxbuf];
      }

      vector<MeshFormat> secDraws = secondaryDraws;

      for(size_t i = 0; i < secDraws.size(); i++)
      {
        if(secDraws[i].buf != ResourceId())
        {
          EnsureBufCached(secDraws[i].buf);
          secDraws[i].buf = m_ProxyBufferIds[secDraws[i].buf];
        }
        if(secDraws[i].idxbuf != ResourceId())
        {
          EnsureBufCached(secDraws[i].idxbuf);
          secDraws[i].idxbuf = m_ProxyBufferIds[secDraws[i].idxbuf];
        }
      }

      m_Proxy->RenderMesh(eventID, secDraws, proxiedCfg);
    }
  }

  uint32_t PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y)
  {
    if(m_Proxy && cfg.position.buf != ResourceId())
    {
      MeshDisplay proxiedCfg = cfg;

      EnsureBufCached(proxiedCfg.position.buf);
      if(proxiedCfg.position.buf == ResourceId() ||
         m_ProxyBufferIds[proxiedCfg.position.buf] == ResourceId())
        return ~0U;
      proxiedCfg.position.buf = m_ProxyBufferIds[proxiedCfg.position.buf];

      if(proxiedCfg.second.buf != ResourceId())
      {
        EnsureBufCached(proxiedCfg.second.buf);
        proxiedCfg.second.buf = m_ProxyBufferIds[proxiedCfg.second.buf];
      }

      if(proxiedCfg.position.idxbuf != ResourceId())
      {
        EnsureBufCached(proxiedCfg.position.idxbuf);
        proxiedCfg.position.idxbuf = m_ProxyBufferIds[proxiedCfg.position.idxbuf];
      }

      return m_Proxy->PickVertex(eventID, proxiedCfg, x, y);
    }

    return ~0U;
  }

  void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                         ResourceId *id, string *errors)
  {
    if(m_Proxy)
    {
      m_Proxy->BuildCustomShader(source, entry, compileFlags, type, id, errors);
    }
    else
    {
      if(id)
        *id = ResourceId();
      if(errors)
        *errors = "Unsupported BuildShader call on proxy without local renderer";
    }
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

  bool Tick(int type, Serialiser *incomingPacket);

  vector<ResourceId> GetBuffers();
  BufferDescription GetBuffer(ResourceId id);

  vector<ResourceId> GetTextures();
  TextureDescription GetTexture(ResourceId id);

  APIProperties GetAPIProperties();

  vector<DebugMessage> GetDebugMessages();

  void SavePipelineState();
  D3D11Pipe::State GetD3D11PipelineState() { return m_D3D11PipelineState; }
  D3D12Pipe::State GetD3D12PipelineState() { return m_D3D12PipelineState; }
  GLPipe::State GetGLPipelineState() { return m_GLPipelineState; }
  VKPipe::State GetVulkanPipelineState() { return m_VulkanPipelineState; }
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);

  vector<uint32_t> GetPassEvents(uint32_t eventID);

  vector<EventUsage> GetUsage(ResourceId id);
  FrameRecord GetFrameRecord();

  bool IsRenderOutput(ResourceId id);

  ResourceId GetLiveID(ResourceId id);

  vector<GPUCounter> EnumerateCounters();
  void DescribeCounter(GPUCounter counterID, CounterDescription &desc);
  vector<CounterResult> FetchCounters(const vector<GPUCounter> &counterID);

  void FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                            vector<ShaderVariable> &outvars, const vector<byte> &data);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData);
  byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                       const GetTextureDataParams &params, size_t &dataSize);

  void InitPostVSBuffers(uint32_t eventID);
  void InitPostVSBuffers(const vector<uint32_t> &passEvents);
  MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage);

  ResourceId RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                           uint32_t eventID, const vector<uint32_t> &passEvents);

  ShaderReflection *GetShader(ResourceId shader, string entryPoint);

  vector<string> GetDisassemblyTargets();
  string DisassembleShader(const ShaderReflection *refl, const string &target);

  bool HasCallstacks();
  void InitCallstackResolver();
  Callstack::StackResolver *GetCallstackResolver();
  // implementing Callstack::StackResolver
  Callstack::AddressDetails GetAddr(uint64_t addr);

  void FreeTargetResource(ResourceId id);

  vector<PixelModification> PixelHistory(vector<EventUsage> events, ResourceId target, uint32_t x,
                                         uint32_t y, uint32_t slice, uint32_t mip,
                                         uint32_t sampleIdx, CompType typeHint);
  ShaderDebugTrace DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx,
                               uint32_t instOffset, uint32_t vertOffset);
  ShaderDebugTrace DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                              uint32_t primitive);
  ShaderDebugTrace DebugThread(uint32_t eventID, const uint32_t groupid[3],
                               const uint32_t threadid[3]);

  void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                         ResourceId *id, string *errors);
  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

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
  bool SendReplayCommand(ReplayProxyPacket type);

  void EnsureTexCached(ResourceId texid, uint32_t arrayIdx, uint32_t mip);
  void RemapProxyTextureIfNeeded(ResourceFormat &format, GetTextureDataParams &params);
  void EnsureBufCached(ResourceId bufid);

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
  set<TextureCacheEntry> m_TextureProxyCache;
  set<ResourceId> m_LocalTextures;

  struct ProxyTextureProperties
  {
    ResourceId id;
    GetTextureDataParams params;

    ProxyTextureProperties() {}
    // Create a proxy Id with the default get-data parameters.
    ProxyTextureProperties(ResourceId proxyid) : id(proxyid) {}
    operator ResourceId() const { return id; }
    bool operator==(const ResourceId &other) const { return id == other; }
  };
  map<ResourceId, ProxyTextureProperties> m_ProxyTextures;

  set<ResourceId> m_BufferProxyCache;
  map<ResourceId, ResourceId> m_ProxyBufferIds;

  map<ResourceId, ResourceId> m_LiveIDs;

  struct ShaderReflKey
  {
    ShaderReflKey() {}
    ShaderReflKey(ResourceId i, string e) : id(i), entryPoint(e) {}
    ResourceId id;
    string entryPoint;
    bool operator<(const ShaderReflKey &o) const
    {
      if(id != o.id)
        return id < o.id;

      return entryPoint < o.entryPoint;
    }
  };

  map<ShaderReflKey, ShaderReflection *> m_ShaderReflectionCache;

  Network::Socket *m_Socket;
  Serialiser *m_FromReplaySerialiser;
  Serialiser *m_ToReplaySerialiser;
  IReplayDriver *m_Proxy;
  IRemoteDriver *m_Remote;
  bool m_RemoteServer;

  bool m_RemoteHasResolver;

  APIProperties m_APIProps;

  D3D11Pipe::State m_D3D11PipelineState;
  D3D12Pipe::State m_D3D12PipelineState;
  GLPipe::State m_GLPipelineState;
  VKPipe::State m_VulkanPipelineState;
};
