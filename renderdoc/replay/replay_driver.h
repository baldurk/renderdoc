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
#include "maths/vec.h"

struct FrameRecord
{
  FrameDescription frameInfo;

  rdctype::array<DrawcallDescription> drawcallList;
};

enum RemapTextureEnum
{
  eRemap_None,
  eRemap_RGBA8,
  eRemap_RGBA16,
  eRemap_RGBA32,
  eRemap_D32S8
};

struct GetTextureDataParams
{
  bool forDiskSave;
  CompType typeHint;
  bool resolve;
  RemapTextureEnum remap;
  float blackPoint;
  float whitePoint;

  GetTextureDataParams()
      : forDiskSave(false),
        typeHint(CompType::Typeless),
        resolve(false),
        remap(eRemap_None),
        blackPoint(0.0f),
        whitePoint(0.0f)
  {
  }
};

// these two interfaces define what an API driver implementation must provide
// to the replay. At minimum it must implement IRemoteDriver which contains
// all of the functionality that cannot be achieved elsewhere. An IReplayDriver
// is more powerful and can be used as a local replay (with an IRemoteDriver
// proxied elsewhere if necessary).
//
// In this sense, IRemoteDriver is a strict subset of IReplayDriver functionality.
// Wherever at all possible functionality should be added as part of IReplayDriver,
// *not* as part of IRemoteDriver, to keep the burden on remote drivers to a minimum.

class IRemoteDriver
{
public:
  virtual void Shutdown() = 0;

  virtual APIProperties GetAPIProperties() = 0;

  virtual vector<ResourceId> GetBuffers() = 0;
  virtual BufferDescription GetBuffer(ResourceId id) = 0;

  virtual vector<ResourceId> GetTextures() = 0;
  virtual TextureDescription GetTexture(ResourceId id) = 0;

  virtual vector<DebugMessage> GetDebugMessages() = 0;

  virtual ShaderReflection *GetShader(ResourceId shader, string entryPoint) = 0;

  virtual vector<string> GetDisassemblyTargets() = 0;
  virtual string DisassembleShader(const ShaderReflection *refl, const string &target) = 0;

  virtual vector<EventUsage> GetUsage(ResourceId id) = 0;

  virtual void SavePipelineState() = 0;
  virtual D3D11Pipe::State GetD3D11PipelineState() = 0;
  virtual D3D12Pipe::State GetD3D12PipelineState() = 0;
  virtual GLPipe::State GetGLPipelineState() = 0;
  virtual VKPipe::State GetVulkanPipelineState() = 0;

  virtual FrameRecord GetFrameRecord() = 0;

  virtual void ReadLogInitialisation() = 0;
  virtual void ReplayLog(uint32_t endEventID, ReplayLogType replayType) = 0;

  virtual vector<uint32_t> GetPassEvents(uint32_t eventID) = 0;

  virtual void InitPostVSBuffers(uint32_t eventID) = 0;
  virtual void InitPostVSBuffers(const vector<uint32_t> &passEvents) = 0;

  virtual ResourceId GetLiveID(ResourceId id) = 0;

  virtual MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage) = 0;

  virtual void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len,
                             vector<byte> &retData) = 0;
  virtual byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                               const GetTextureDataParams &params, size_t &dataSize) = 0;

  virtual void BuildTargetShader(string source, string entry, const uint32_t compileFlags,
                                 ShaderStage type, ResourceId *id, string *errors) = 0;
  virtual void ReplaceResource(ResourceId from, ResourceId to) = 0;
  virtual void RemoveReplacement(ResourceId id) = 0;
  virtual void FreeTargetResource(ResourceId id) = 0;

  virtual vector<GPUCounter> EnumerateCounters() = 0;
  virtual void DescribeCounter(GPUCounter counterID, CounterDescription &desc) = 0;
  virtual vector<CounterResult> FetchCounters(const vector<GPUCounter> &counterID) = 0;

  virtual void FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                    vector<ShaderVariable> &outvars, const vector<byte> &data) = 0;

  virtual vector<PixelModification> PixelHistory(vector<EventUsage> events, ResourceId target,
                                                 uint32_t x, uint32_t y, uint32_t slice, uint32_t mip,
                                                 uint32_t sampleIdx, CompType typeHint) = 0;
  virtual ShaderDebugTrace DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid,
                                       uint32_t idx, uint32_t instOffset, uint32_t vertOffset) = 0;
  virtual ShaderDebugTrace DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                                      uint32_t primitive) = 0;
  virtual ShaderDebugTrace DebugThread(uint32_t eventID, const uint32_t groupid[3],
                                       const uint32_t threadid[3]) = 0;

  virtual ResourceId RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                   uint32_t eventID, const vector<uint32_t> &passEvents) = 0;

  virtual bool IsRenderOutput(ResourceId id) = 0;

  virtual void FileChanged() = 0;

  virtual void InitCallstackResolver() = 0;
  virtual bool HasCallstacks() = 0;
  virtual Callstack::StackResolver *GetCallstackResolver() = 0;
};

class IReplayDriver : public IRemoteDriver
{
public:
  virtual bool IsRemoteProxy() = 0;

  virtual vector<WindowingSystem> GetSupportedWindowSystems() = 0;

  virtual uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth) = 0;
  virtual void DestroyOutputWindow(uint64_t id) = 0;
  virtual bool CheckResizeOutputWindow(uint64_t id) = 0;
  virtual void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h) = 0;
  virtual void ClearOutputWindowColor(uint64_t id, float col[4]) = 0;
  virtual void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil) = 0;
  virtual void BindOutputWindow(uint64_t id, bool depth) = 0;
  virtual bool IsOutputWindowVisible(uint64_t id) = 0;
  virtual void FlipOutputWindow(uint64_t id) = 0;

  virtual bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                         CompType typeHint, float *minval, float *maxval) = 0;
  virtual bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                            CompType typeHint, float minval, float maxval, bool channels[4],
                            vector<uint32_t> &histogram) = 0;

  virtual ResourceId CreateProxyTexture(const TextureDescription &templateTex) = 0;
  virtual void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                                   size_t dataSize) = 0;
  virtual bool IsTextureSupported(const ResourceFormat &format) = 0;

  virtual ResourceId CreateProxyBuffer(const BufferDescription &templateBuf) = 0;
  virtual void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize) = 0;

  virtual void RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws,
                          const MeshDisplay &cfg) = 0;
  virtual bool RenderTexture(TextureDisplay cfg) = 0;

  virtual void BuildCustomShader(string source, string entry, const uint32_t compileFlags,
                                 ShaderStage type, ResourceId *id, string *errors) = 0;
  virtual ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                       uint32_t arrayIdx, uint32_t sampleIdx, CompType typeHint) = 0;
  virtual void FreeCustomShader(ResourceId id) = 0;

  virtual void RenderCheckerboard(Vec3f light, Vec3f dark) = 0;

  virtual void RenderHighlightBox(float w, float h, float scale) = 0;

  virtual void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                         uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4]) = 0;
  virtual uint32_t PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y) = 0;
};

// utility functions useful in any driver implementation
DrawcallDescription *SetupDrawcallPointers(std::vector<DrawcallDescription *> *drawcallTable,
                                           rdctype::array<DrawcallDescription> &draws,
                                           DrawcallDescription *parent,
                                           DrawcallDescription *&previous);

// simple cache for when we need buffer data for highlighting
// vertices, typical use will be lots of vertices in the same
// mesh, not jumping back and forth much between meshes.
struct HighlightCache
{
  HighlightCache() : EID(0), buf(), offs(0), stage(MeshDataStage::Unknown), idxData(false) {}
  IRemoteDriver *driver = NULL;

  uint32_t EID;
  ResourceId buf;
  uint64_t offs;
  MeshDataStage stage;
  bool idxData;

  std::vector<byte> vertexData;
  std::vector<uint32_t> indices;

  void CacheHighlightingData(uint32_t eventID, const MeshDisplay &cfg);

  bool FetchHighlightPositions(const MeshDisplay &cfg, FloatVector &activeVertex,
                               vector<FloatVector> &activePrim,
                               vector<FloatVector> &adjacentPrimVertices,
                               vector<FloatVector> &inactiveVertices);

  static FloatVector InterpretVertex(byte *data, uint32_t vert, const MeshDisplay &cfg, byte *end,
                                     bool &valid);

  FloatVector InterpretVertex(byte *data, uint32_t vert, const MeshDisplay &cfg, byte *end,
                              bool useidx, bool &valid);
};