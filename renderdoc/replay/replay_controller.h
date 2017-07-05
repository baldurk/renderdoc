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

#include <set>
#include <vector>
#include "api/replay/renderdoc_replay.h"
#include "common/common.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "type_helpers.h"

struct ReplayController;

struct ReplayOutput : public IReplayOutput
{
public:
  void SetTextureDisplay(const TextureDisplay &o);
  void SetMeshDisplay(const MeshDisplay &o);

  void ClearThumbnails();
  bool AddThumbnail(WindowingSystem system, void *data, ResourceId texID, CompType typeHint);

  void Display();

  ReplayOutputType GetType() { return m_Type; }
  bool SetPixelContext(WindowingSystem system, void *data);
  void SetPixelContextLocation(uint32_t x, uint32_t y);
  void DisablePixelContext();

  rdctype::pair<PixelValue, PixelValue> GetMinMax();
  rdctype::array<uint32_t> GetHistogram(float minval, float maxval, bool channels[4]);

  ResourceId GetCustomShaderTexID() { return m_CustomShaderResourceId; }
  ResourceId GetDebugOverlayTexID() { return m_OverlayResourceId; }
  PixelValue PickPixel(ResourceId texID, bool customShader, uint32_t x, uint32_t y,
                       uint32_t sliceFace, uint32_t mip, uint32_t sample);
  rdctype::pair<uint32_t, uint32_t> PickVertex(uint32_t eventID, uint32_t x, uint32_t y);

private:
  ReplayOutput(ReplayController *parent, WindowingSystem system, void *data, ReplayOutputType type);
  virtual ~ReplayOutput();

  void SetFrameEvent(int eventID);

  void RefreshOverlay();

  void DisplayContext();
  void DisplayTex();

  void DisplayMesh();

  ReplayController *m_pRenderer;

  bool m_OverlayDirty;
  bool m_ForceOverlayRefresh;

  IReplayDriver *m_pDevice;

  struct OutputPair
  {
    ResourceId texture;
    bool depthMode;
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

  vector<uint32_t> passEvents;

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

  ReplayStatus CreateDevice(const char *logfile);
  ReplayStatus SetDevice(IReplayDriver *device);

  void FileChanged();

  bool HasCallstacks();
  bool InitResolver();

  void SetFrameEvent(uint32_t eventID, bool force);

  void FetchPipelineState();

  D3D11Pipe::State GetD3D11PipelineState();
  D3D12Pipe::State GetD3D12PipelineState();
  GLPipe::State GetGLPipelineState();
  VKPipe::State GetVulkanPipelineState();

  rdctype::array<rdctype::str> GetDisassemblyTargets();
  rdctype::str DisassembleShader(const ShaderReflection *refl, const char *target);

  rdctype::pair<ResourceId, rdctype::str> BuildCustomShader(const char *entry, const char *source,
                                                            const uint32_t compileFlags,
                                                            ShaderStage type);
  void FreeCustomShader(ResourceId id);

  rdctype::pair<ResourceId, rdctype::str> BuildTargetShader(const char *entry, const char *source,
                                                            const uint32_t compileFlags,
                                                            ShaderStage type);
  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);
  void FreeTargetResource(ResourceId id);

  FrameDescription GetFrameInfo();
  rdctype::array<DrawcallDescription> GetDrawcalls();
  rdctype::array<CounterResult> FetchCounters(const rdctype::array<GPUCounter> &counters);
  rdctype::array<GPUCounter> EnumerateCounters();
  CounterDescription DescribeCounter(GPUCounter counterID);
  rdctype::array<TextureDescription> GetTextures();
  rdctype::array<BufferDescription> GetBuffers();
  rdctype::array<rdctype::str> GetResolve(const rdctype::array<uint64_t> &callstack);
  rdctype::array<DebugMessage> GetDebugMessages();

  rdctype::array<PixelModification> PixelHistory(ResourceId target, uint32_t x, uint32_t y,
                                                 uint32_t slice, uint32_t mip, uint32_t sampleIdx,
                                                 CompType typeHint);
  ShaderDebugTrace *DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset,
                                uint32_t vertOffset);
  ShaderDebugTrace *DebugPixel(uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive);
  ShaderDebugTrace *DebugThread(const uint32_t groupid[3], const uint32_t threadid[3]);
  void FreeTrace(ShaderDebugTrace *trace);

  MeshFormat GetPostVSData(uint32_t instID, MeshDataStage stage);

  rdctype::array<EventUsage> GetUsage(ResourceId id);

  rdctype::array<byte> GetBufferData(ResourceId buff, uint64_t offset, uint64_t len);
  rdctype::array<byte> GetTextureData(ResourceId buff, uint32_t arrayIdx, uint32_t mip);

  bool SaveTexture(const TextureSave &saveData, const char *path);

  rdctype::array<ShaderVariable> GetCBufferVariableContents(ResourceId shader, const char *entryPoint,
                                                            uint32_t cbufslot, ResourceId buffer,
                                                            uint64_t offs);

  rdctype::array<WindowingSystem> GetSupportedWindowSystems();

  ReplayOutput *CreateOutput(WindowingSystem, void *data, ReplayOutputType type);

  void ShutdownOutput(IReplayOutput *output);
  void Shutdown();

private:
  ReplayStatus PostCreateInit(IReplayDriver *device);

  DrawcallDescription *GetDrawcallByEID(uint32_t eventID);

  IReplayDriver *GetDevice() { return m_pDevice; }
  FrameRecord m_FrameRecord;
  vector<DrawcallDescription *> m_Drawcalls;

  uint32_t m_EventID;

  D3D11Pipe::State m_D3D11PipelineState;
  D3D12Pipe::State m_D3D12PipelineState;
  GLPipe::State m_GLPipelineState;
  VKPipe::State m_VulkanPipelineState;

  std::vector<ReplayOutput *> m_Outputs;

  std::vector<BufferDescription> m_Buffers;
  std::vector<TextureDescription> m_Textures;

  IReplayDriver *m_pDevice;

  std::set<ResourceId> m_TargetResources;
  std::set<ResourceId> m_CustomShaders;

  friend struct ReplayOutput;
};
