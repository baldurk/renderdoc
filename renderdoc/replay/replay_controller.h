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

#include <set>
#include <vector>
#include "api/replay/renderdoc_replay.h"
#include "common/common.h"
#include "core/core.h"
#include "replay/replay_driver.h"

#define CHECK_REPLAY_THREAD() RDCASSERT(Threading::GetCurrentID() == m_ThreadID);

struct ReplayController;

struct ReplayOutput : public IReplayOutput
{
public:
  void Shutdown();
  void SetTextureDisplay(const TextureDisplay &o);
  void SetMeshDisplay(const MeshDisplay &o);
  void SetDimensions(int32_t width, int32_t height);
  bytebuf ReadbackOutputTexture();
  rdcpair<int32_t, int32_t> GetDimensions();

  void ClearThumbnails();
  bool AddThumbnail(WindowingData window, ResourceId texID, CompType typeHint, uint32_t mip,
                    uint32_t slice);

  void Display();

  ReplayOutputType GetType() { return m_Type; }
  bool SetPixelContext(WindowingData window);
  void SetPixelContextLocation(uint32_t x, uint32_t y);
  void DisablePixelContext();

  rdcpair<PixelValue, PixelValue> GetMinMax();
  rdcarray<uint32_t> GetHistogram(float minval, float maxval, bool channels[4]);

  ResourceId GetCustomShaderTexID();
  ResourceId GetDebugOverlayTexID();
  PixelValue PickPixel(ResourceId texID, bool customShader, uint32_t x, uint32_t y,
                       uint32_t sliceFace, uint32_t mip, uint32_t sample);
  rdcpair<uint32_t, uint32_t> PickVertex(uint32_t eventId, uint32_t x, uint32_t y);

private:
  ReplayOutput(ReplayController *parent, WindowingData window, ReplayOutputType type);
  virtual ~ReplayOutput();

  void SetFrameEvent(int eventId);

  void RefreshOverlay();

  void ClearBackground(uint64_t outputID, const FloatVector &backgroundColor);

  void DisplayContext();
  void DisplayTex();

  void DisplayMesh();

  uint64_t m_ThreadID;

  ReplayController *m_pRenderer;

  bool m_OverlayDirty;
  bool m_ForceOverlayRefresh;

  IReplayDriver *m_pDevice;

  struct OutputPair
  {
    ResourceId texture;
    bool depthMode;
    uint32_t mip;
    uint32_t slice;
    uint64_t wndHandle;
    CompType typeHint;
    uint64_t outputID;

    bool dirty;
  } m_MainOutput;

  ResourceId m_OverlayResourceId;
  ResourceId m_CustomShaderResourceId;

  std::vector<OutputPair> m_Thumbnails;

  float m_ContextX;
  float m_ContextY;
  OutputPair m_PixelContext;

  uint32_t m_EventID;
  ReplayOutputType m_Type;

  std::vector<uint32_t> passEvents;

  int32_t m_Width;
  int32_t m_Height;

  struct
  {
    TextureDisplay texDisplay;
    MeshDisplay meshDisplay;
  } m_RenderData;

  friend struct ReplayController;
};

struct ReplayController : public IReplayController
{
public:
  ReplayController();
  virtual ~ReplayController();

  APIProperties GetAPIProperties();

  ReplayStatus CreateDevice(RDCFile *rdc);
  ReplayStatus SetDevice(IReplayDriver *device);

  void FileChanged();

  void SetFrameEvent(uint32_t eventId, bool force);

  const D3D11Pipe::State *GetD3D11PipelineState();
  const D3D12Pipe::State *GetD3D12PipelineState();
  const GLPipe::State *GetGLPipelineState();
  const VKPipe::State *GetVulkanPipelineState();
  const PipeState &GetPipelineState();

  rdcarray<rdcstr> GetDisassemblyTargets();
  rdcstr DisassembleShader(ResourceId pipeline, const ShaderReflection *refl, const char *target);

  rdcpair<ResourceId, rdcstr> BuildCustomShader(const char *entry, ShaderEncoding sourceEncoding,
                                                bytebuf source,
                                                const ShaderCompileFlags &compileFlags,
                                                ShaderStage type);
  void FreeCustomShader(ResourceId id);

  rdcarray<ShaderEncoding> GetCustomShaderEncodings();
  rdcarray<ShaderEncoding> GetTargetShaderEncodings();
  rdcpair<ResourceId, rdcstr> BuildTargetShader(const char *entry, ShaderEncoding sourceEncoding,
                                                bytebuf source,
                                                const ShaderCompileFlags &compileFlags,
                                                ShaderStage type);
  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);
  void FreeTargetResource(ResourceId id);

  FrameDescription GetFrameInfo();
  const SDFile &GetStructuredFile();
  const rdcarray<DrawcallDescription> &GetDrawcalls();
  void AddFakeMarkers();
  rdcarray<CounterResult> FetchCounters(const rdcarray<GPUCounter> &counters);
  rdcarray<GPUCounter> EnumerateCounters();
  CounterDescription DescribeCounter(GPUCounter counterID);
  const rdcarray<TextureDescription> &GetTextures();
  const rdcarray<BufferDescription> &GetBuffers();
  const rdcarray<ResourceDescription> &GetResources();
  rdcarray<DebugMessage> GetDebugMessages();

  rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader);
  ShaderReflection *GetShader(ResourceId shader, ShaderEntryPoint entry);

  rdcarray<PixelModification> PixelHistory(ResourceId target, uint32_t x, uint32_t y, uint32_t slice,
                                           uint32_t mip, uint32_t sampleIdx, CompType typeHint);
  ShaderDebugTrace *DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset,
                                uint32_t vertOffset);
  ShaderDebugTrace *DebugPixel(uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive);
  ShaderDebugTrace *DebugThread(const uint32_t groupid[3], const uint32_t threadid[3]);
  void FreeTrace(ShaderDebugTrace *trace);

  MeshFormat GetPostVSData(uint32_t instID, uint32_t viewID, MeshDataStage stage);

  rdcarray<EventUsage> GetUsage(ResourceId id);

  bytebuf GetBufferData(ResourceId buff, uint64_t offset, uint64_t len);
  bytebuf GetTextureData(ResourceId buff, uint32_t arrayIdx, uint32_t mip);

  bool SaveTexture(const TextureSave &saveData, const char *path);

  rdcarray<ShaderVariable> GetCBufferVariableContents(ResourceId shader, const char *entryPoint,
                                                      uint32_t cbufslot, ResourceId buffer,
                                                      uint64_t offs);

  rdcarray<WindowingSystem> GetSupportedWindowSystems();

  void ReplayLoop(WindowingData window, ResourceId texid);
  void CancelReplayLoop();

  rdcstr CreateRGPProfile(WindowingData window);

  ReplayOutput *CreateOutput(WindowingData window, ReplayOutputType type);

  void ShutdownOutput(IReplayOutput *output);
  void Shutdown();

private:
  ReplayStatus PostCreateInit(IReplayDriver *device, RDCFile *rdc);

  void FetchPipelineState(uint32_t eventId);

  DrawcallDescription *GetDrawcallByEID(uint32_t eventId);
  bool ContainsMarker(const rdcarray<DrawcallDescription> &draws);
  bool PassEquivalent(const DrawcallDescription &a, const DrawcallDescription &b);

  IReplayDriver *GetDevice() { return m_pDevice; }
  FrameRecord m_FrameRecord;
  std::vector<DrawcallDescription *> m_Drawcalls;

  uint64_t m_ThreadID;

  APIProperties m_APIProps;
  std::vector<std::string> m_GCNTargets;

  volatile int32_t m_ReplayLoopCancel = 0;
  volatile int32_t m_ReplayLoopFinished = 0;

  uint32_t m_EventID;

  const D3D11Pipe::State *m_D3D11PipelineState;
  const D3D12Pipe::State *m_D3D12PipelineState;
  const GLPipe::State *m_GLPipelineState;
  const VKPipe::State *m_VulkanPipelineState;
  PipeState m_PipeState;

  std::vector<ReplayOutput *> m_Outputs;

  rdcarray<ResourceDescription> m_Resources;
  rdcarray<BufferDescription> m_Buffers;
  rdcarray<TextureDescription> m_Textures;

  IReplayDriver *m_pDevice;

  std::set<ResourceId> m_TargetResources;
  std::set<ResourceId> m_CustomShaders;

  friend struct ReplayOutput;
};
