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

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "maths/vec.h"

struct FrameRecord
{
  FrameDescription frameInfo;

  rdcarray<DrawcallDescription> drawcallList;
};

DECLARE_REFLECTION_STRUCT(FrameRecord);

enum class RemapTexture : uint32_t
{
  NoRemap,
  RGBA8,
  RGBA16,
  RGBA32,
  D32S8
};

DECLARE_REFLECTION_ENUM(RemapTexture);

struct GetTextureDataParams
{
  bool forDiskSave;
  CompType typeHint;
  bool resolve;
  RemapTexture remap;
  float blackPoint;
  float whitePoint;

  GetTextureDataParams()
      : forDiskSave(false),
        typeHint(CompType::Typeless),
        resolve(false),
        remap(RemapTexture::NoRemap),
        blackPoint(0.0f),
        whitePoint(1.0f)
  {
  }
};

DECLARE_REFLECTION_STRUCT(GetTextureDataParams);

class RDCFile;

class AMDRGPControl;

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

  virtual const std::vector<ResourceDescription> &GetResources() = 0;

  virtual std::vector<ResourceId> GetBuffers() = 0;
  virtual BufferDescription GetBuffer(ResourceId id) = 0;

  virtual std::vector<ResourceId> GetTextures() = 0;
  virtual TextureDescription GetTexture(ResourceId id) = 0;

  virtual std::vector<DebugMessage> GetDebugMessages() = 0;

  virtual rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader) = 0;
  virtual ShaderReflection *GetShader(ResourceId shader, ShaderEntryPoint entry) = 0;

  virtual std::vector<std::string> GetDisassemblyTargets() = 0;
  virtual std::string DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                        const std::string &target) = 0;

  virtual std::vector<EventUsage> GetUsage(ResourceId id) = 0;

  virtual void SavePipelineState(uint32_t eventId) = 0;
  virtual const D3D11Pipe::State *GetD3D11PipelineState() = 0;
  virtual const D3D12Pipe::State *GetD3D12PipelineState() = 0;
  virtual const GLPipe::State *GetGLPipelineState() = 0;
  virtual const VKPipe::State *GetVulkanPipelineState() = 0;

  virtual FrameRecord GetFrameRecord() = 0;

  virtual ReplayStatus ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers) = 0;
  virtual void ReplayLog(uint32_t endEventID, ReplayLogType replayType) = 0;
  virtual const SDFile &GetStructuredFile() = 0;

  virtual std::vector<uint32_t> GetPassEvents(uint32_t eventId) = 0;

  virtual void InitPostVSBuffers(uint32_t eventId) = 0;
  virtual void InitPostVSBuffers(const std::vector<uint32_t> &passEvents) = 0;

  virtual ResourceId GetLiveID(ResourceId id) = 0;

  virtual MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                                      MeshDataStage stage) = 0;

  virtual void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData) = 0;
  virtual void GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                              const GetTextureDataParams &params, bytebuf &data) = 0;

  virtual void BuildTargetShader(ShaderEncoding sourceEncoding, bytebuf source,
                                 const std::string &entry, const ShaderCompileFlags &compileFlags,
                                 ShaderStage type, ResourceId *id, std::string *errors) = 0;
  virtual rdcarray<ShaderEncoding> GetTargetShaderEncodings() = 0;
  virtual void ReplaceResource(ResourceId from, ResourceId to) = 0;
  virtual void RemoveReplacement(ResourceId id) = 0;
  virtual void FreeTargetResource(ResourceId id) = 0;

  virtual std::vector<GPUCounter> EnumerateCounters() = 0;
  virtual CounterDescription DescribeCounter(GPUCounter counterID) = 0;
  virtual std::vector<CounterResult> FetchCounters(const std::vector<GPUCounter> &counterID) = 0;

  virtual void FillCBufferVariables(ResourceId shader, std::string entryPoint, uint32_t cbufSlot,
                                    rdcarray<ShaderVariable> &outvars, const bytebuf &data) = 0;

  virtual std::vector<PixelModification> PixelHistory(std::vector<EventUsage> events,
                                                      ResourceId target, uint32_t x, uint32_t y,
                                                      uint32_t slice, uint32_t mip,
                                                      uint32_t sampleIdx, CompType typeHint) = 0;
  virtual ShaderDebugTrace DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                       uint32_t idx, uint32_t instOffset, uint32_t vertOffset) = 0;
  virtual ShaderDebugTrace DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                      uint32_t primitive) = 0;
  virtual ShaderDebugTrace DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                       const uint32_t threadid[3]) = 0;

  virtual ResourceId RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                   uint32_t eventId, const std::vector<uint32_t> &passEvents) = 0;

  virtual bool IsRenderOutput(ResourceId id) = 0;

  virtual void FileChanged() = 0;

  virtual bool NeedRemapForFetch(const ResourceFormat &format) = 0;

  virtual DriverInformation GetDriverInfo() = 0;
};

class IReplayDriver : public IRemoteDriver
{
public:
  virtual bool IsRemoteProxy() = 0;

  virtual std::vector<WindowingSystem> GetSupportedWindowSystems() = 0;

  virtual AMDRGPControl *GetRGPControl() = 0;

  virtual uint64_t MakeOutputWindow(WindowingData window, bool depth) = 0;
  virtual void DestroyOutputWindow(uint64_t id) = 0;
  virtual bool CheckResizeOutputWindow(uint64_t id) = 0;
  virtual void SetOutputWindowDimensions(uint64_t id, int32_t w, int32_t h) = 0;
  virtual void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h) = 0;
  virtual void GetOutputWindowData(uint64_t id, bytebuf &retData) = 0;
  virtual void ClearOutputWindowColor(uint64_t id, FloatVector col) = 0;
  virtual void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil) = 0;
  virtual void BindOutputWindow(uint64_t id, bool depth) = 0;
  virtual bool IsOutputWindowVisible(uint64_t id) = 0;
  virtual void FlipOutputWindow(uint64_t id) = 0;

  virtual bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                         CompType typeHint, float *minval, float *maxval) = 0;
  virtual bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                            CompType typeHint, float minval, float maxval, bool channels[4],
                            std::vector<uint32_t> &histogram) = 0;

  virtual ResourceId CreateProxyTexture(const TextureDescription &templateTex) = 0;
  virtual void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                                   size_t dataSize) = 0;
  virtual bool IsTextureSupported(const ResourceFormat &format) = 0;

  virtual ResourceId CreateProxyBuffer(const BufferDescription &templateBuf) = 0;
  virtual void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize) = 0;

  virtual void RenderMesh(uint32_t eventId, const std::vector<MeshFormat> &secondaryDraws,
                          const MeshDisplay &cfg) = 0;
  virtual bool RenderTexture(TextureDisplay cfg) = 0;

  virtual void BuildCustomShader(ShaderEncoding sourceEncoding, bytebuf source,
                                 const std::string &entry, const ShaderCompileFlags &compileFlags,
                                 ShaderStage type, ResourceId *id, std::string *errors) = 0;
  virtual rdcarray<ShaderEncoding> GetCustomShaderEncodings() = 0;
  virtual ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                       uint32_t arrayIdx, uint32_t sampleIdx, CompType typeHint) = 0;
  virtual void FreeCustomShader(ResourceId id) = 0;

  virtual void RenderCheckerboard() = 0;

  virtual void RenderHighlightBox(float w, float h, float scale) = 0;

  virtual void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                         uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4]) = 0;
  virtual uint32_t PickVertex(uint32_t eventId, int32_t width, int32_t height,
                              const MeshDisplay &cfg, uint32_t x, uint32_t y) = 0;
};

// utility functions useful in any driver implementation
void SetupDrawcallPointers(std::vector<DrawcallDescription *> &drawcallTable,
                           rdcarray<DrawcallDescription> &draws);

// for hardware/APIs that can't do line rasterization, manually expand any triangle input topology
// to a linestrip with strip restart indices.
void PatchLineStripIndexBuffer(const DrawcallDescription *draw, uint8_t *idx8, uint16_t *idx16,
                               uint32_t *idx32, std::vector<uint32_t> &patchedIndices);

uint64_t CalcMeshOutputSize(uint64_t curSize, uint64_t requiredOutput);

void StandardFillCBufferVariable(uint32_t dataOffset, const bytebuf &data, ShaderVariable &outvar,
                                 uint32_t matStride);
void StandardFillCBufferVariables(const rdcarray<ShaderConstant> &invars,
                                  rdcarray<ShaderVariable> &outvars, const bytebuf &data);

// simple cache for when we need buffer data for highlighting
// vertices, typical use will be lots of vertices in the same
// mesh, not jumping back and forth much between meshes.
struct HighlightCache
{
  HighlightCache() : cacheKey(0), idxData(false) {}
  IRemoteDriver *driver = NULL;

  uint64_t cacheKey;

  bool idxData;
  bytebuf vertexData;
  std::vector<uint32_t> indices;

  void CacheHighlightingData(uint32_t eventId, const MeshDisplay &cfg);

  bool FetchHighlightPositions(const MeshDisplay &cfg, FloatVector &activeVertex,
                               std::vector<FloatVector> &activePrim,
                               std::vector<FloatVector> &adjacentPrimVertices,
                               std::vector<FloatVector> &inactiveVertices);

  static FloatVector InterpretVertex(const byte *data, uint32_t vert, uint32_t vertexByteStride,
                                     const ResourceFormat &fmt, const byte *end, bool &valid);

  FloatVector InterpretVertex(const byte *data, uint32_t vert, const MeshDisplay &cfg,
                              const byte *end, bool useidx, bool &valid);
};

extern const Vec4f colorRamp[22];