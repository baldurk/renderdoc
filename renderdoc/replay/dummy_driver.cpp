/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include "dummy_driver.h"

DummyDriver::DummyDriver(IReplayDriver *original, const rdcarray<ShaderReflection *> &shaders,
                         SDFile *sdfile)
{
  m_Shaders = shaders;
  m_SDFile = sdfile;

  m_Props = original->GetAPIProperties();
  m_Resources = original->GetResources();
  m_DescriptorStores = original->GetDescriptorStores();
  m_Buffers = original->GetBuffers();
  m_Textures = original->GetTextures();
  m_FrameRecord = original->GetFrameRecord();
  m_TargetEncodings = original->GetTargetShaderEncodings();
  m_DriverInfo = original->GetDriverInfo();

  m_Proxy = original->IsRemoteProxy();
  m_GPUs = original->GetAvailableGPUs();
  m_WindowSystems = original->GetSupportedWindowSystems();
  m_CustomEncodings = original->GetCustomShaderEncodings();
  m_CustomPrefixes = original->GetCustomShaderSourcePrefixes();
}

DummyDriver::~DummyDriver()
{
  // we own the shaders
  for(ShaderReflection *refl : m_Shaders)
    delete refl;

  // and we own the structured file
  delete m_SDFile;
}

void DummyDriver::Shutdown()
{
  delete this;
}

APIProperties DummyDriver::GetAPIProperties()
{
  return m_Props;
}

rdcarray<ResourceDescription> DummyDriver::GetResources()
{
  return m_Resources;
}

rdcarray<DescriptorStoreDescription> DummyDriver::GetDescriptorStores()
{
  return m_DescriptorStores;
}

rdcarray<BufferDescription> DummyDriver::GetBuffers()
{
  return m_Buffers;
}

BufferDescription DummyDriver::GetBuffer(ResourceId id)
{
  for(const BufferDescription &buf : m_Buffers)
  {
    if(buf.resourceId == id)
      return buf;
  }

  return {};
}

rdcarray<TextureDescription> DummyDriver::GetTextures()
{
  return m_Textures;
}

TextureDescription DummyDriver::GetTexture(ResourceId id)
{
  for(const TextureDescription &tex : m_Textures)
  {
    if(tex.resourceId == id)
      return tex;
  }

  return {};
}

rdcarray<DebugMessage> DummyDriver::GetDebugMessages()
{
  return {};
}

rdcarray<ShaderEntryPoint> DummyDriver::GetShaderEntryPoints(ResourceId shader)
{
  return {{"main", ShaderStage::Vertex}};
}

ShaderReflection *DummyDriver::GetShader(ResourceId pipeline, ResourceId shader,
                                         ShaderEntryPoint entry)
{
  return NULL;
}

rdcarray<rdcstr> DummyDriver::GetDisassemblyTargets(bool withPipeline)
{
  return {"Disassembly"};
}

rdcstr DummyDriver::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                      const rdcstr &target)
{
  return "; No disassembly available due to unrecoverable error analysing capture.";
}

rdcarray<EventUsage> DummyDriver::GetUsage(ResourceId id)
{
  return {};
}

void DummyDriver::SetPipelineStates(D3D11Pipe::State *d3d11, D3D12Pipe::State *d3d12,
                                    GLPipe::State *gl, VKPipe::State *vk)
{
}

void DummyDriver::SavePipelineState(uint32_t eventId)
{
}

rdcarray<Descriptor> DummyDriver::GetDescriptors(ResourceId descriptorStore,
                                                 const rdcarray<DescriptorRange> &ranges)
{
  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  rdcarray<Descriptor> ret;
  ret.resize(count);
  return ret;
}

rdcarray<SamplerDescriptor> DummyDriver::GetSamplerDescriptors(ResourceId descriptorStore,
                                                               const rdcarray<DescriptorRange> &ranges)
{
  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  rdcarray<SamplerDescriptor> ret;
  ret.resize(count);
  return ret;
}

rdcarray<DescriptorAccess> DummyDriver::GetDescriptorAccess(uint32_t eventId)
{
  return {};
}

rdcarray<DescriptorLogicalLocation> DummyDriver::GetDescriptorLocations(
    ResourceId descriptorStore, const rdcarray<DescriptorRange> &ranges)
{
  return {};
}

FrameRecord DummyDriver::GetFrameRecord()
{
  return m_FrameRecord;
}

RDResult DummyDriver::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  return ResultCode::APIReplayFailed;
}

void DummyDriver::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
}

SDFile *DummyDriver::GetStructuredFile()
{
  return m_SDFile;
}

rdcarray<uint32_t> DummyDriver::GetPassEvents(uint32_t eventId)
{
  return {eventId};
}

void DummyDriver::InitPostVSBuffers(uint32_t eventId)
{
}

void DummyDriver::InitPostVSBuffers(const rdcarray<uint32_t> &passEvents)
{
}

ResourceId DummyDriver::GetLiveID(ResourceId id)
{
  return id;
}

MeshFormat DummyDriver::GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                                         MeshDataStage stage)
{
  return {};
}

void DummyDriver::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData)
{
  retData.clear();
}

void DummyDriver::GetTextureData(ResourceId tex, const Subresource &sub,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  data.clear();
}

void DummyDriver::BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  id = ResourceId();
  errors = "Unrecoverable error encountered while analysing capture";
}

rdcarray<ShaderEncoding> DummyDriver::GetTargetShaderEncodings()
{
  return {ShaderEncoding::HLSL, ShaderEncoding::GLSL};
}

void DummyDriver::ReplaceResource(ResourceId from, ResourceId to)
{
}

void DummyDriver::RemoveReplacement(ResourceId id)
{
}

void DummyDriver::FreeTargetResource(ResourceId id)
{
}

rdcarray<GPUCounter> DummyDriver::EnumerateCounters()
{
  return {};
}

CounterDescription DummyDriver::DescribeCounter(GPUCounter counterID)
{
  return {};
}

rdcarray<CounterResult> DummyDriver::FetchCounters(const rdcarray<GPUCounter> &counterID)
{
  return {};
}

void DummyDriver::FillCBufferVariables(ResourceId pipeline, ResourceId shader, ShaderStage stage,
                                       rdcstr entryPoint, uint32_t cbufSlot,
                                       rdcarray<ShaderVariable> &outvars, const bytebuf &data)
{
  outvars.clear();
}

rdcarray<PixelModification> DummyDriver::PixelHistory(rdcarray<EventUsage> events,
                                                      ResourceId target, uint32_t x, uint32_t y,
                                                      const Subresource &sub, CompType typeCast)
{
  return {};
}

ShaderDebugTrace *DummyDriver::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                           uint32_t idx, uint32_t view)
{
  return new ShaderDebugTrace;
}

ShaderDebugTrace *DummyDriver::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                          const DebugPixelInputs &inputs)
{
  return new ShaderDebugTrace;
}

ShaderDebugTrace *DummyDriver::DebugThread(uint32_t eventId,
                                           const rdcfixedarray<uint32_t, 3> &groupid,
                                           const rdcfixedarray<uint32_t, 3> &threadid)
{
  return new ShaderDebugTrace;
}

rdcarray<ShaderDebugState> DummyDriver::ContinueDebug(ShaderDebugger *debugger)
{
  return {};
}

void DummyDriver::FreeDebugger(ShaderDebugger *debugger)
{
}

ResourceId DummyDriver::RenderOverlay(ResourceId texid, FloatVector clearCol, DebugOverlay overlay,
                                      uint32_t eventId, const rdcarray<uint32_t> &passEvents)
{
  return ResourceId();
}

bool DummyDriver::IsRenderOutput(ResourceId id)
{
  return false;
}

void DummyDriver::FileChanged()
{
}

bool DummyDriver::NeedRemapForFetch(const ResourceFormat &format)
{
  return false;
}

DriverInformation DummyDriver::GetDriverInfo()
{
  return m_DriverInfo;
}

rdcarray<GPUDevice> DummyDriver::GetAvailableGPUs()
{
  return m_GPUs;
}

bool DummyDriver::IsRemoteProxy()
{
  return m_Proxy;
}

RDResult DummyDriver::FatalErrorCheck()
{
  return ResultCode::Succeeded;
}

IReplayDriver *DummyDriver::MakeDummyDriver()
{
  return NULL;
}

rdcarray<WindowingSystem> DummyDriver::GetSupportedWindowSystems()
{
  rdcarray<WindowingSystem> ret;
#if ENABLED(RDOC_LINUX)

#if ENABLED(RDOC_XLIB)
  ret.push_back(WindowingSystem::Xlib);
#endif

#if ENABLED(RDOC_XCB)
  ret.push_back(WindowingSystem::XCB);
#endif

#if ENABLED(RDOC_WAYLAND)
  ret.push_back(WindowingSystem::Wayland);
#endif

#elif ENABLED(RDOC_WIN32)

  ret.push_back(WindowingSystem::Win32);

#elif ENABLED(RDOC_ANDROID)

  ret.push_back(WindowingSystem::Android);

#elif ENABLED(RDOC_APPLE)

  ret.push_back(WindowingSystem::MacOS);

#endif

  return ret;
}

AMDRGPControl *DummyDriver::GetRGPControl()
{
  return NULL;
}

uint64_t DummyDriver::MakeOutputWindow(WindowingData window, bool depth)
{
  return 1;
}

void DummyDriver::DestroyOutputWindow(uint64_t id)
{
}

bool DummyDriver::CheckResizeOutputWindow(uint64_t id)
{
  return false;
}

void DummyDriver::SetOutputWindowDimensions(uint64_t id, int32_t w, int32_t h)
{
}

void DummyDriver::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
}

void DummyDriver::GetOutputWindowData(uint64_t id, bytebuf &retData)
{
}

void DummyDriver::ClearOutputWindowColor(uint64_t id, FloatVector col)
{
}

void DummyDriver::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
}

void DummyDriver::BindOutputWindow(uint64_t id, bool depth)
{
}

bool DummyDriver::IsOutputWindowVisible(uint64_t id)
{
  return true;
}

void DummyDriver::FlipOutputWindow(uint64_t id)
{
}

bool DummyDriver::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast,
                            float *minval, float *maxval)
{
  *minval = 0.0f;
  *maxval = 1.0f;
  return false;
}

bool DummyDriver::GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                               float minval, float maxval, const rdcfixedarray<bool, 4> &channels,
                               rdcarray<uint32_t> &histogram)
{
  histogram.fill(256, 0);
  return false;
}

void DummyDriver::PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                            CompType typeCast, float pixel[4])
{
}

ResourceId DummyDriver::CreateProxyTexture(const TextureDescription &templateTex)
{
  return ResourceId();
}

void DummyDriver::SetProxyTextureData(ResourceId texid, const Subresource &sub, byte *data,
                                      size_t dataSize)
{
}

bool DummyDriver::IsTextureSupported(const TextureDescription &tex)
{
  return true;
}

ResourceId DummyDriver::CreateProxyBuffer(const BufferDescription &templateBuf)
{
  return ResourceId();
}

void DummyDriver::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
}

void DummyDriver::RenderMesh(uint32_t eventId, const rdcarray<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
}

bool DummyDriver::RenderTexture(TextureDisplay cfg)
{
  return false;
}

void DummyDriver::SetCustomShaderIncludes(const rdcarray<rdcstr> &directories)
{
}

void DummyDriver::BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  id = ResourceId();
  errors = "Unrecoverable error encountered while analysing capture";
}

rdcarray<ShaderEncoding> DummyDriver::GetCustomShaderEncodings()
{
  return m_CustomEncodings;
}

rdcarray<ShaderSourcePrefix> DummyDriver::GetCustomShaderSourcePrefixes()
{
  return m_CustomPrefixes;
}

ResourceId DummyDriver::ApplyCustomShader(TextureDisplay &display)
{
  return ResourceId();
}

void DummyDriver::FreeCustomShader(ResourceId id)
{
}

void DummyDriver::RenderCheckerboard(FloatVector dark, FloatVector light)
{
}

void DummyDriver::RenderHighlightBox(float w, float h, float scale)
{
}

uint32_t DummyDriver::PickVertex(uint32_t eventId, int32_t width, int32_t height,
                                 const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  return ~0U;
}
