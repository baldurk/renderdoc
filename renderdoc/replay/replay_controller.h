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

#pragma once

#include <set>
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
  ResultDetails AddThumbnail(WindowingData window, ResourceId texID, const Subresource &sub,
                             CompType typeCast);
  bytebuf DrawThumbnail(int32_t width, int32_t height, ResourceId textureId, const Subresource &sub,
                        CompType typeCast);

  void Display();

  ReplayOutputType GetType() { return m_Type; }
  ResultDetails SetPixelContext(WindowingData window);
  void SetPixelContextLocation(uint32_t x, uint32_t y);
  void DisablePixelContext();

  ResourceId GetCustomShaderTexID();
  ResourceId GetDebugOverlayTexID();
  rdcpair<uint32_t, uint32_t> PickVertex(uint32_t x, uint32_t y);

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

  ReplayController *m_pController;

  bool m_CustomDirty;
  bool m_OverlayDirty;
  bool m_ForceOverlayRefresh;

  IReplayDriver *m_pDevice;

  struct OutputPair
  {
    ResourceId texture;
    bool depthMode;
    Subresource sub;
    uint64_t wndHandle;
    CompType typeCast;
    uint64_t outputID;

    bool dirty;
  } m_MainOutput;

  rdcpair<uint32_t, uint32_t> m_TextureDim = {0, 0};

  ResourceId m_OverlayResourceId;
  ResourceId m_CustomShaderResourceId;

  rdcarray<OutputPair> m_Thumbnails;
  rdcarray<rdcpair<uint64_t, uint64_t>> m_ThumbnailGenerators;
  // keep 8 generators to avoid churn, but most thumbnails should be the same size so this means
  // during resize we don't create and destroy too many
  static const size_t MaxThumbnailGenerators = 8;

  float m_ContextX;
  float m_ContextY;
  OutputPair m_PixelContext;

  uint32_t m_EventID;
  ReplayOutputType m_Type;

  rdcarray<uint32_t> passEvents;

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

  APIProperties GetAPIProperties();

  RDResult CreateDevice(RDCFile *rdc, const ReplayOptions &opts);
  RDResult SetDevice(IReplayDriver *device);

  void FileChanged();

  void SetFrameEvent(uint32_t eventId, bool force);

  const D3D11Pipe::State *GetD3D11PipelineState();
  const D3D12Pipe::State *GetD3D12PipelineState();
  const GLPipe::State *GetGLPipelineState();
  const VKPipe::State *GetVulkanPipelineState();
  const PipeState &GetPipelineState();
  rdcarray<Descriptor> GetDescriptors(ResourceId descriptorStore,
                                      const rdcarray<DescriptorRange> &ranges);
  rdcarray<SamplerDescriptor> GetSamplerDescriptors(ResourceId descriptorStore,
                                                    const rdcarray<DescriptorRange> &ranges);
  const rdcarray<DescriptorAccess> &GetDescriptorAccess();
  rdcarray<DescriptorLogicalLocation> GetDescriptorLocations(ResourceId descriptorStore,
                                                             const rdcarray<DescriptorRange> &ranges);

  rdcarray<rdcstr> GetDisassemblyTargets(bool withPipeline);
  rdcstr DisassembleShader(ResourceId pipeline, const ShaderReflection *refl, const rdcstr &target);

  void SetCustomShaderIncludes(const rdcarray<rdcstr> &directories);
  rdcpair<ResourceId, rdcstr> BuildCustomShader(const rdcstr &entry, ShaderEncoding sourceEncoding,
                                                bytebuf source,
                                                const ShaderCompileFlags &compileFlags,
                                                ShaderStage type);
  void FreeCustomShader(ResourceId id);

  rdcarray<ShaderEncoding> GetCustomShaderEncodings();
  rdcarray<ShaderSourcePrefix> GetCustomShaderSourcePrefixes();
  rdcarray<ShaderEncoding> GetTargetShaderEncodings();
  rdcpair<ResourceId, rdcstr> BuildTargetShader(const rdcstr &entry, ShaderEncoding sourceEncoding,
                                                bytebuf source,
                                                const ShaderCompileFlags &compileFlags,
                                                ShaderStage type);
  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);
  void FreeTargetResource(ResourceId id);

  FrameDescription GetFrameInfo();
  const SDFile &GetStructuredFile();
  const rdcarray<ActionDescription> &GetRootActions();
  void AddFakeMarkers();
  rdcarray<CounterResult> FetchCounters(const rdcarray<GPUCounter> &counters);
  rdcarray<GPUCounter> EnumerateCounters();
  CounterDescription DescribeCounter(GPUCounter counterID);
  const rdcarray<TextureDescription> &GetTextures();
  const rdcarray<BufferDescription> &GetBuffers();
  const rdcarray<DescriptorStoreDescription> &GetDescriptorStores();
  const rdcarray<ResourceDescription> &GetResources();
  rdcarray<DebugMessage> GetDebugMessages();
  ResultDetails GetFatalErrorStatus()
  {
    // don't reconvert this on return every time, or we would cache many strings if this function is
    // repeatedly called (which it is intended to be)
    if(m_FatalError.code != m_FatalErrorResult.code)
      m_FatalErrorResult = m_FatalError;
    return m_FatalErrorResult;
  }
  rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader);
  const ShaderReflection *GetShader(ResourceId pipeline, ResourceId shader, ShaderEntryPoint entry);

  PixelValue PickPixel(ResourceId textureId, uint32_t x, uint32_t y, const Subresource &sub,
                       CompType typeCast);
  rdcpair<PixelValue, PixelValue> GetMinMax(ResourceId textureId, const Subresource &sub,
                                            CompType typeCast);
  rdcarray<uint32_t> GetHistogram(ResourceId textureId, const Subresource &sub, CompType typeCast,
                                  float minval, float maxval, const rdcfixedarray<bool, 4> &channels);
  rdcarray<PixelModification> PixelHistory(ResourceId target, uint32_t x, uint32_t y,
                                           const Subresource &sub, CompType typeCast);
  ShaderDebugTrace *DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t view);
  ShaderDebugTrace *DebugPixel(uint32_t x, uint32_t y, const DebugPixelInputs &inputs);
  ShaderDebugTrace *DebugThread(const rdcfixedarray<uint32_t, 3> &groupid,
                                const rdcfixedarray<uint32_t, 3> &threadid);
  rdcarray<ShaderDebugState> ContinueDebug(ShaderDebugger *debugger);
  void FreeTrace(ShaderDebugTrace *trace);

  MeshFormat GetPostVSData(uint32_t instID, uint32_t viewID, MeshDataStage stage);

  rdcarray<EventUsage> GetUsage(ResourceId id);

  bytebuf GetBufferData(ResourceId buff, uint64_t offset, uint64_t len);
  bytebuf GetTextureData(ResourceId buff, const Subresource &sub);

  ResultDetails SaveTexture(const TextureSave &saveData, const rdcstr &path);

  rdcarray<ShaderVariable> GetCBufferVariableContents(ResourceId pipeline, ResourceId shader,
                                                      ShaderStage stage, const rdcstr &entryPoint,
                                                      uint32_t cbufslot, ResourceId buffer,
                                                      uint64_t offset, uint64_t length);

  rdcarray<WindowingSystem> GetSupportedWindowSystems();

  void ReplayLoop(WindowingData window, ResourceId texid);
  void CancelReplayLoop();

  rdcstr CreateRGPProfile(WindowingData window);

  ReplayOutput *CreateOutput(WindowingData window, ReplayOutputType type);

  void ShutdownOutput(IReplayOutput *output);
  void Shutdown();

  bool FatalErrorCheck();

private:
  virtual ~ReplayController();
  RDResult PostCreateInit(IReplayDriver *device, RDCFile *rdc);

  void FetchPipelineState(uint32_t eventId);

  ActionDescription *GetActionByEID(uint32_t eventId);
  bool ContainsMarker(const rdcarray<ActionDescription> &actions);
  bool PassEquivalent(const ActionDescription &a, const ActionDescription &b);

  IReplayDriver *GetDevice() { return m_pDevice; }
  FrameRecord m_FrameRecord;
  rdcarray<ActionDescription *> m_Actions;

  uint64_t m_ThreadID;

  APIProperties m_APIProps;
  rdcarray<rdcstr> m_GCNTargets;

  int32_t m_ReplayLoopCancel = 0;
  int32_t m_ReplayLoopFinished = 0;

  RDResult m_FatalError = ResultCode::Succeeded;
  ResultDetails m_FatalErrorResult = {ResultCode::Succeeded};

  uint32_t m_EventID;

  std::map<uint32_t, uint32_t> m_EventRemap;

  D3D11Pipe::State m_D3D11PipelineState;
  D3D12Pipe::State m_D3D12PipelineState;
  GLPipe::State m_GLPipelineState;
  VKPipe::State m_VulkanPipelineState;
  PipeState m_PipeState;

  rdcarray<ReplayOutput *> m_Outputs;

  rdcarray<ResourceDescription> m_Resources;
  rdcarray<BufferDescription> m_Buffers;
  rdcarray<DescriptorStoreDescription> m_DescriptorStores;
  rdcarray<TextureDescription> m_Textures;

  IReplayDriver *m_pDevice;

  rdcarray<ShaderDebugger *> m_Debuggers;

  std::set<ResourceId> m_TargetResources;
  std::set<ResourceId> m_CustomShaders;

  friend struct ReplayOutput;
};
