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

struct ReplayRenderer;

struct ReplayOutput : public IReplayOutput
{
public:
  bool SetOutputConfig(const OutputConfig &o);
  bool SetTextureDisplay(const TextureDisplay &o);
  bool SetMeshDisplay(const MeshDisplay &o);

  bool ClearThumbnails();
  bool AddThumbnail(WindowingSystem system, void *data, ResourceId texID,
                    FormatComponentType typeHint);

  bool Display();

  OutputType GetType() { return m_Config.m_Type; }
  bool SetPixelContext(WindowingSystem system, void *data);
  bool SetPixelContextLocation(uint32_t x, uint32_t y);
  void DisablePixelContext();

  bool GetMinMax(PixelValue *minval, PixelValue *maxval);
  bool GetHistogram(float minval, float maxval, bool channels[4],
                    rdctype::array<uint32_t> *histogram);

  ResourceId GetCustomShaderTexID() { return m_CustomShaderResourceId; }
  bool PickPixel(ResourceId texID, bool customShader, uint32_t x, uint32_t y, uint32_t sliceFace,
                 uint32_t mip, uint32_t sample, PixelValue *val);
  uint32_t PickVertex(uint32_t eventID, uint32_t x, uint32_t y);

private:
  ReplayOutput(ReplayRenderer *parent, WindowingSystem system, void *data, OutputType type);
  virtual ~ReplayOutput();

  void SetFrameEvent(int eventID);

  void RefreshOverlay();

  void DisplayContext();
  void DisplayTex();

  void DisplayMesh();

  ReplayRenderer *m_pRenderer;

  bool m_OverlayDirty;
  bool m_ForceOverlayRefresh;

  IReplayDriver *m_pDevice;

  struct OutputPair
  {
    ResourceId texture;
    bool depthMode;
    uint64_t wndHandle;
    FormatComponentType typeHint;
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
  OutputConfig m_Config;

  vector<uint32_t> passEvents;

  int32_t m_Width;
  int32_t m_Height;

  struct
  {
    TextureDisplay texDisplay;
    MeshDisplay meshDisplay;
  } m_RenderData;

  friend struct ReplayRenderer;
};

struct ReplayRenderer : public IReplayRenderer
{
public:
  ReplayRenderer();
  virtual ~ReplayRenderer();

  APIProperties GetAPIProperties();

  ReplayCreateStatus CreateDevice(const char *logfile);
  ReplayCreateStatus SetDevice(IReplayDriver *device);

  void FileChanged();

  bool HasCallstacks();
  bool InitResolver();

  bool SetFrameEvent(uint32_t eventID, bool force);

  void FetchPipelineState();

  bool GetD3D11PipelineState(D3D11PipelineState *state);
  bool GetD3D12PipelineState(D3D12PipelineState *state);
  bool GetGLPipelineState(GLPipelineState *state);
  bool GetVulkanPipelineState(VulkanPipelineState *state);

  ResourceId BuildCustomShader(const char *entry, const char *source, const uint32_t compileFlags,
                               ShaderStageType type, rdctype::str *errors);
  bool FreeCustomShader(ResourceId id);

  ResourceId BuildTargetShader(const char *entry, const char *source, const uint32_t compileFlags,
                               ShaderStageType type, rdctype::str *errors);
  bool ReplaceResource(ResourceId from, ResourceId to);
  bool RemoveReplacement(ResourceId id);
  bool FreeTargetResource(ResourceId id);

  bool GetFrameInfo(FetchFrameInfo *frame);
  bool GetDrawcalls(rdctype::array<FetchDrawcall> *draws);
  bool FetchCounters(uint32_t *counters, uint32_t numCounters,
                     rdctype::array<CounterResult> *results);
  bool EnumerateCounters(rdctype::array<uint32_t> *counters);
  bool DescribeCounter(uint32_t counterID, CounterDescription *desc);
  bool GetTextures(rdctype::array<FetchTexture> *texs);
  bool GetBuffers(rdctype::array<FetchBuffer> *bufs);
  bool GetResolve(uint64_t *callstack, uint32_t callstackLen, rdctype::array<rdctype::str> *trace);
  bool GetDebugMessages(rdctype::array<DebugMessage> *msgs);

  bool PixelHistory(ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip,
                    uint32_t sampleIdx, FormatComponentType typeHint,
                    rdctype::array<PixelModification> *history);
  bool DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset,
                   uint32_t vertOffset, ShaderDebugTrace *trace);
  bool DebugPixel(uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive,
                  ShaderDebugTrace *trace);
  bool DebugThread(uint32_t groupid[3], uint32_t threadid[3], ShaderDebugTrace *trace);

  bool GetPostVSData(uint32_t instID, MeshDataStage stage, MeshFormat *data);

  bool GetUsage(ResourceId id, rdctype::array<EventUsage> *usage);

  bool GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, rdctype::array<byte> *data);
  bool GetTextureData(ResourceId buff, uint32_t arrayIdx, uint32_t mip, rdctype::array<byte> *data);

  bool SaveTexture(const TextureSave &saveData, const char *path);

  bool GetCBufferVariableContents(ResourceId shader, const char *entryPoint, uint32_t cbufslot,
                                  ResourceId buffer, uint64_t offs,
                                  rdctype::array<ShaderVariable> *vars);

  void GetSupportedWindowSystems(rdctype::array<WindowingSystem> *systems);

  ReplayOutput *CreateOutput(WindowingSystem, void *data, OutputType type);

  void ShutdownOutput(ReplayOutput *output);
  void Shutdown();

private:
  ReplayCreateStatus PostCreateInit(IReplayDriver *device);

  FetchDrawcall *GetDrawcallByEID(uint32_t eventID);

  IReplayDriver *GetDevice() { return m_pDevice; }
  struct FrameRecord
  {
    FetchFrameInfo frameInfo;

    rdctype::array<FetchDrawcall> m_DrawCallList;
  };
  FrameRecord m_FrameRecord;
  vector<FetchDrawcall *> m_Drawcalls;

  uint32_t m_EventID;

  D3D11PipelineState m_D3D11PipelineState;
  D3D12PipelineState m_D3D12PipelineState;
  GLPipelineState m_GLPipelineState;
  VulkanPipelineState m_VulkanPipelineState;

  std::vector<ReplayOutput *> m_Outputs;

  std::vector<FetchBuffer> m_Buffers;
  std::vector<FetchTexture> m_Textures;

  IReplayDriver *m_pDevice;

  std::set<ResourceId> m_TargetResources;
  std::set<ResourceId> m_CustomShaders;

  friend struct ReplayOutput;
};
