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

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "d3d11_common.h"

class WrappedID3D11Device;

class D3D11Replay : public IReplayDriver
{
public:
  D3D11Replay();

  void SetProxy(bool p, bool warp)
  {
    m_Proxy = p;
    m_WARP = warp;
  }
  bool IsRemoteProxy() { return m_Proxy; }
  void Shutdown();

  void SetDevice(WrappedID3D11Device *d) { m_pDevice = d; }
  APIProperties GetAPIProperties();

  vector<ResourceId> GetBuffers();
  BufferDescription GetBuffer(ResourceId id);

  vector<ResourceId> GetTextures();
  TextureDescription GetTexture(ResourceId id);

  vector<DebugMessage> GetDebugMessages();

  ShaderReflection *GetShader(ResourceId shader, string entryPoint);

  vector<string> GetDisassemblyTargets();
  string DisassembleShader(const ShaderReflection *refl, const string &target);

  vector<EventUsage> GetUsage(ResourceId id);

  FrameRecord GetFrameRecord();

  void SavePipelineState() { m_CurPipelineState = MakePipelineState(); }
  D3D11Pipe::State GetD3D11PipelineState() { return m_CurPipelineState; }
  D3D12Pipe::State GetD3D12PipelineState() { return D3D12Pipe::State(); }
  GLPipe::State GetGLPipelineState() { return GLPipe::State(); }
  VKPipe::State GetVulkanPipelineState() { return VKPipe::State(); }
  void FreeTargetResource(ResourceId id);
  void FreeCustomShader(ResourceId id);

  void ReadLogInitialisation();
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);

  vector<uint32_t> GetPassEvents(uint32_t eventID);

  vector<WindowingSystem> GetSupportedWindowSystems()
  {
    vector<WindowingSystem> ret;
    ret.push_back(WindowingSystem::Win32);
    return ret;
  }

  uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void ClearOutputWindowColor(uint64_t id, float col[4]);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void InitPostVSBuffers(uint32_t eventID);
  void InitPostVSBuffers(const vector<uint32_t> &passEvents);

  ResourceId GetLiveID(ResourceId id);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval);
  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    vector<uint32_t> &histogram);

  MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData);
  byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                       const GetTextureDataParams &params, size_t &dataSize);

  void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                         ResourceId *id, string *errors);
  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  vector<GPUCounter> EnumerateCounters();
  void DescribeCounter(GPUCounter counterID, CounterDescription &desc);
  vector<CounterResult> FetchCounters(const vector<GPUCounter> &counters);

  ResourceId CreateProxyTexture(const TextureDescription &templateTex);
  void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                           size_t dataSize);
  bool IsTextureSupported(const ResourceFormat &format);

  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf);
  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);

  void RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg);

  bool RenderTexture(TextureDisplay cfg);

  void RenderCheckerboard(Vec3f light, Vec3f dark);

  void RenderHighlightBox(float w, float h, float scale);

  void FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                            vector<ShaderVariable> &outvars, const vector<byte> &data);

  vector<PixelModification> PixelHistory(vector<EventUsage> events, ResourceId target, uint32_t x,
                                         uint32_t y, uint32_t slice, uint32_t mip,
                                         uint32_t sampleIdx, CompType typeHint);
  ShaderDebugTrace DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx,
                               uint32_t instOffset, uint32_t vertOffset);
  ShaderDebugTrace DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                              uint32_t primitive);
  ShaderDebugTrace DebugThread(uint32_t eventID, const uint32_t groupid[3],
                               const uint32_t threadid[3]);
  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, CompType typeHint, float pixel[4]);
  uint32_t PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y);

  ResourceId RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                           uint32_t eventID, const vector<uint32_t> &passEvents);

  void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                         ResourceId *id, string *errors);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint);

  bool IsRenderOutput(ResourceId id);

  void FileChanged() {}
  void InitCallstackResolver();
  bool HasCallstacks();
  Callstack::StackResolver *GetCallstackResolver();

private:
  D3D11Pipe::State MakePipelineState();

  bool m_WARP;
  bool m_Proxy;

  vector<ID3D11Resource *> m_ProxyResources;

  WrappedID3D11Device *m_pDevice;

  D3D11Pipe::State m_CurPipelineState;
};
