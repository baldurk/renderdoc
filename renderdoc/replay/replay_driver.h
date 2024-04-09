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

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "maths/vec.h"

template <typename T, BucketRecordType bucketType = T::BucketType>
struct BucketForRecord
{
  static size_t Get(size_t value);
};

template <typename T>
struct BucketForRecord<T, BucketRecordType::Linear>
{
  static size_t Get(size_t value)
  {
    const size_t size = T::BucketSize;
    const size_t count = T::BucketCount;
    const size_t maximum = size * count;
    const size_t index = (value < maximum) ? (value / size) : (count - 1);
    return index;
  }
};

template <typename T>
struct BucketForRecord<T, BucketRecordType::Pow2>
{
  static size_t Get(size_t value)
  {
    const size_t count = T::BucketCount;
    static_assert(count <= (sizeof(size_t) * 8),
                  "Unexpected correspondence between bucket size and sizeof(size_t)");
    const size_t maximum = (size_t)1 << count;
    const size_t index = (value < maximum) ? (size_t)(Log2Floor(value)) : (count - 1);
    return index;
  }
};

struct FrameRecord
{
  FrameDescription frameInfo;

  rdcarray<ActionDescription> actionList;
};

DECLARE_REFLECTION_STRUCT(FrameRecord);

enum class RemapTexture : uint32_t
{
  NoRemap,
  RGBA8,
  RGBA16,
  RGBA32
};

DECLARE_REFLECTION_ENUM(RemapTexture);

struct GetTextureDataParams
{
  // this data is going to be saved to disk, so prepare it as needed. E.g. on GL flip Y order to
  // match conventional axis for file formats.
  bool forDiskSave = false;
  // this data is going to be transferred cross-API e.g. in replay proxying, so standardise bit
  // layout of any packed formats where API conventions differ (mostly only RGBA4 or other awkward
  // ones where our resource formats don't enumerate all possible iterations). Saving to disk is
  // also standardised to ensure the data matches any format description we also write to the
  // format.
  bool standardLayout = false;
  CompType typeCast = CompType::Typeless;
  bool resolve = false;
  RemapTexture remap = RemapTexture::NoRemap;
  float blackPoint = 0.0f;
  float whitePoint = 1.0f;
};

DECLARE_REFLECTION_STRUCT(GetTextureDataParams);

CompType BaseRemapType(RemapTexture remap, CompType typeCast);
inline CompType BaseRemapType(const GetTextureDataParams &params)
{
  return BaseRemapType(params.remap, params.typeCast);
}

class RDCFile;

class AMDRGPControl;

struct RenderOutputSubresource
{
  RenderOutputSubresource(uint32_t mip, uint32_t slice, uint32_t numSlices)
      : mip(mip), slice(slice), numSlices(numSlices)
  {
  }

  uint32_t mip, slice, numSlices;
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

  virtual rdcarray<ResourceDescription> GetResources() = 0;

  virtual rdcarray<DescriptorStoreDescription> GetDescriptorStores() = 0;

  virtual rdcarray<BufferDescription> GetBuffers() = 0;
  virtual BufferDescription GetBuffer(ResourceId id) = 0;

  virtual rdcarray<TextureDescription> GetTextures() = 0;
  virtual TextureDescription GetTexture(ResourceId id) = 0;

  virtual rdcarray<DebugMessage> GetDebugMessages() = 0;

  virtual rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader) = 0;
  virtual ShaderReflection *GetShader(ResourceId pipeline, ResourceId shader,
                                      ShaderEntryPoint entry) = 0;

  virtual rdcarray<rdcstr> GetDisassemblyTargets(bool withPipeline) = 0;
  virtual rdcstr DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                   const rdcstr &target) = 0;

  virtual rdcarray<EventUsage> GetUsage(ResourceId id) = 0;

  virtual void SetPipelineStates(D3D11Pipe::State *d3d11, D3D12Pipe::State *d3d12,
                                 GLPipe::State *gl, VKPipe::State *vk) = 0;
  virtual void SavePipelineState(uint32_t eventId) = 0;

  virtual rdcarray<Descriptor> GetDescriptors(ResourceId descriptorStore,
                                              const rdcarray<DescriptorRange> &ranges) = 0;
  virtual rdcarray<SamplerDescriptor> GetSamplerDescriptors(
      ResourceId descriptorStore, const rdcarray<DescriptorRange> &ranges) = 0;
  virtual rdcarray<DescriptorAccess> GetDescriptorAccess(uint32_t eventId) = 0;
  virtual rdcarray<DescriptorLogicalLocation> GetDescriptorLocations(
      ResourceId descriptorStore, const rdcarray<DescriptorRange> &ranges) = 0;

  virtual FrameRecord GetFrameRecord() = 0;

  virtual RDResult ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers) = 0;
  virtual void ReplayLog(uint32_t endEventID, ReplayLogType replayType) = 0;
  virtual SDFile *GetStructuredFile() = 0;

  virtual rdcarray<uint32_t> GetPassEvents(uint32_t eventId) = 0;

  virtual void InitPostVSBuffers(uint32_t eventId) = 0;
  virtual void InitPostVSBuffers(const rdcarray<uint32_t> &passEvents) = 0;

  virtual ResourceId GetLiveID(ResourceId id) = 0;

  virtual MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                                      MeshDataStage stage) = 0;

  // this is a helper/batch query for the above, that's only necessary because Android is a shit
  // platform and its proxying has significant per-call overhead. This is overridden in the proxy,
  // but otherwise will call to this default implementation
  virtual rdcarray<MeshFormat> GetBatchPostVSBuffers(uint32_t eventId,
                                                     const rdcarray<uint32_t> &instIDs,
                                                     uint32_t viewID, MeshDataStage stage)
  {
    rdcarray<MeshFormat> ret;
    ret.reserve(instIDs.size());
    for(uint32_t instID : instIDs)
      ret.push_back(GetPostVSBuffers(eventId, instID, viewID, stage));
    return ret;
  }

  virtual void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData) = 0;
  virtual void GetTextureData(ResourceId tex, const Subresource &sub,
                              const GetTextureDataParams &params, bytebuf &data) = 0;

  virtual void BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                 const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                 ShaderStage type, ResourceId &id, rdcstr &errors) = 0;
  virtual rdcarray<ShaderEncoding> GetTargetShaderEncodings() = 0;
  virtual void ReplaceResource(ResourceId from, ResourceId to) = 0;
  virtual void RemoveReplacement(ResourceId id) = 0;
  virtual void FreeTargetResource(ResourceId id) = 0;

  virtual rdcarray<GPUCounter> EnumerateCounters() = 0;
  virtual CounterDescription DescribeCounter(GPUCounter counterID) = 0;
  virtual rdcarray<CounterResult> FetchCounters(const rdcarray<GPUCounter> &counterID) = 0;

  virtual void FillCBufferVariables(ResourceId pipeline, ResourceId shader, ShaderStage stage,
                                    rdcstr entryPoint, uint32_t cbufSlot,
                                    rdcarray<ShaderVariable> &outvars, const bytebuf &data) = 0;

  virtual rdcarray<PixelModification> PixelHistory(rdcarray<EventUsage> events, ResourceId target,
                                                   uint32_t x, uint32_t y, const Subresource &sub,
                                                   CompType typeCast) = 0;
  virtual ShaderDebugTrace *DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                        uint32_t idx, uint32_t view) = 0;
  virtual ShaderDebugTrace *DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                       const DebugPixelInputs &inputs) = 0;
  virtual ShaderDebugTrace *DebugThread(uint32_t eventId, const rdcfixedarray<uint32_t, 3> &groupid,
                                        const rdcfixedarray<uint32_t, 3> &threadid) = 0;
  virtual rdcarray<ShaderDebugState> ContinueDebug(ShaderDebugger *debugger) = 0;
  virtual void FreeDebugger(ShaderDebugger *debugger) = 0;

  virtual ResourceId RenderOverlay(ResourceId texid, FloatVector clearCol, DebugOverlay overlay,
                                   uint32_t eventId, const rdcarray<uint32_t> &passEvents) = 0;

  virtual bool IsRenderOutput(ResourceId id) = 0;

  virtual void FileChanged() = 0;
  virtual RDResult FatalErrorCheck() = 0;

  virtual bool NeedRemapForFetch(const ResourceFormat &format) = 0;

  virtual DriverInformation GetDriverInfo() = 0;

  virtual rdcarray<GPUDevice> GetAvailableGPUs() = 0;
};

class IReplayDriver : public IRemoteDriver
{
public:
  virtual bool IsRemoteProxy() = 0;

  virtual IReplayDriver *MakeDummyDriver() = 0;

  virtual rdcarray<WindowingSystem> GetSupportedWindowSystems() = 0;

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

  virtual bool GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast, float *minval,
                         float *maxval) = 0;
  virtual bool GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                            float minval, float maxval, const rdcfixedarray<bool, 4> &channels,
                            rdcarray<uint32_t> &histogram) = 0;
  virtual void PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                         CompType typeCast, float pixel[4]) = 0;

  virtual ResourceId CreateProxyTexture(const TextureDescription &templateTex) = 0;
  virtual void SetProxyTextureData(ResourceId texid, const Subresource &sub, byte *data,
                                   size_t dataSize) = 0;
  virtual bool IsTextureSupported(const TextureDescription &tex) = 0;

  virtual ResourceId CreateProxyBuffer(const BufferDescription &templateBuf) = 0;
  virtual void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize) = 0;

  virtual void RenderMesh(uint32_t eventId, const rdcarray<MeshFormat> &secondaryDraws,
                          const MeshDisplay &cfg) = 0;
  virtual bool RenderTexture(TextureDisplay cfg) = 0;

  virtual void SetCustomShaderIncludes(const rdcarray<rdcstr> &directories) = 0;
  virtual void BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                 const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                 ShaderStage type, ResourceId &id, rdcstr &errors) = 0;
  virtual rdcarray<ShaderEncoding> GetCustomShaderEncodings() = 0;
  virtual rdcarray<ShaderSourcePrefix> GetCustomShaderSourcePrefixes() = 0;
  virtual ResourceId ApplyCustomShader(TextureDisplay &display) = 0;
  virtual void FreeCustomShader(ResourceId id) = 0;

  virtual void RenderCheckerboard(FloatVector dark, FloatVector light) = 0;

  virtual void RenderHighlightBox(float w, float h, float scale) = 0;

  virtual uint32_t PickVertex(uint32_t eventId, int32_t width, int32_t height,
                              const MeshDisplay &cfg, uint32_t x, uint32_t y) = 0;
};

// for protocols, we extend the public interface a bit to add callbacks for remapping connection
// ports (typically to some port forwarded on localhost)
struct IDeviceProtocolHandler : public IDeviceProtocolController
{
  virtual rdcstr GetDeviceID(const rdcstr &URL)
  {
    rdcstr ret = URL;
    int offs = ret.find("://");
    if(offs > 0)
      ret.erase(0, offs + 3);
    return ret;
  }

  virtual rdcstr RemapHostname(const rdcstr &deviceID) = 0;
  virtual uint16_t RemapPort(const rdcstr &deviceID, uint16_t srcPort) = 0;
  virtual IRemoteServer *CreateRemoteServer(Network::Socket *sock, const rdcstr &deviceID) = 0;
};

// utility functions useful in any driver implementation
void SetupActionPointers(rdcarray<ActionDescription *> &actionTable,
                         rdcarray<ActionDescription> &actions);

// for hardware/APIs that can't do line rasterization, manually expand any triangle input topology
// to a linestrip with strip restart indices.
void PatchLineStripIndexBuffer(const ActionDescription *action, Topology topology, uint8_t *idx8,
                               uint16_t *idx16, uint32_t *idx32, rdcarray<uint32_t> &patchedIndices);

void PatchTriangleFanRestartIndexBufer(rdcarray<uint32_t> &patchedIndices, uint32_t restartIndex);

uint64_t CalcMeshOutputSize(uint64_t curSize, uint64_t requiredOutput);

void StandardFillCBufferVariable(ResourceId shader, const ShaderConstantType &desc,
                                 uint32_t dataOffset, const bytebuf &data, ShaderVariable &outvar,
                                 uint32_t matStride);
void StandardFillCBufferVariables(ResourceId shader, const rdcarray<ShaderConstant> &invars,
                                  rdcarray<ShaderVariable> &outvars, const bytebuf &data);
void PreprocessLineDirectives(rdcarray<ShaderSourceFile> &sourceFiles);

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
  rdcarray<uint32_t> indices;

  void CacheHighlightingData(uint32_t eventId, const MeshDisplay &cfg);

  bool FetchHighlightPositions(const MeshDisplay &cfg, FloatVector &activeVertex,
                               rdcarray<FloatVector> &activePrim,
                               rdcarray<FloatVector> &adjacentPrimVertices,
                               rdcarray<FloatVector> &inactiveVertices);

  static FloatVector InterpretVertex(const byte *data, uint32_t vert, uint32_t vertexByteStride,
                                     const ResourceFormat &fmt, const byte *end, bool &valid);

  FloatVector InterpretVertex(const byte *data, uint32_t vert, const MeshDisplay &cfg,
                              const byte *end, bool useidx, bool &valid);
};

extern const Vec4f colorRamp[22];
extern const uint32_t uniqueColors[48];

enum class DiscardType : int
{
  RenderPassLoad,         // discarded on renderpass load
  RenderPassStore,        // discarded after renderpass store
  UndefinedTransition,    // transition from undefined layout
  DiscardCall,            // explicit Discard() type API call
  InvalidateCall,         // explicit Invalidate() type API call
  Count,
};

static constexpr uint32_t DiscardPatternWidth = 64;
static constexpr uint32_t DiscardPatternHeight = 8;

// returns a pattern to fill the texture with
bytebuf GetDiscardPattern(DiscardType type, const ResourceFormat &fmt, uint32_t rowPitch = 1,
                          bool invert = false);

void DeriveNearFar(Vec4f pos, Vec4f pos0, float &nearp, float &farp, bool &found);
