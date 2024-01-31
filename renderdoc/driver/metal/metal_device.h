/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
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

#include "metal_common.h"
#include "metal_core.h"
#include "metal_manager.h"

class WrappedMTLDevice;
class MetalReplay;

struct MetalDrawableInfo
{
  CA::MetalLayer *mtlLayer;
  WrappedMTLTexture *texture;
  NS::UInteger drawableID;
};

class MetalCapturer : public IFrameCapturer
{
public:
  MetalCapturer(WrappedMTLDevice &device) : m_Device(device) {}
  // IFrameCapturer interface
  RDCDriver GetFrameCaptureDriver() { return RDCDriver::Metal; }
  void StartFrameCapture(DeviceOwnedWindow devWnd);
  bool EndFrameCapture(DeviceOwnedWindow devWnd);
  bool DiscardFrameCapture(DeviceOwnedWindow devWnd);
  // IFrameCapturer interface

private:
  WrappedMTLDevice &m_Device;
};

class WrappedMTLDevice : public WrappedMTLObject
{
public:
  WrappedMTLDevice(MTL::Device *realMTLDevice, ResourceId objId);
  ~WrappedMTLDevice() {}
  template <typename SerialiserType>
  bool Serialise_MTLCreateSystemDefaultDevice(SerialiserType &ser);
  static WrappedMTLDevice *MTLCreateSystemDefaultDevice(MTL::Device *realMTLDevice);

  // Serialised MTLDevice APIs
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandQueue *, newCommandQueue);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLLibrary *, newDefaultLibrary);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLLibrary *, newLibraryWithSource,
                                          NS::String *source, MTL::CompileOptions *options,
                                          NS::Error **error);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLRenderPipelineState *,
                                          newRenderPipelineStateWithDescriptor,
                                          RDMTL::RenderPipelineDescriptor &descriptor,
                                          NS::Error **error);
  WrappedMTLTexture *newTextureWithDescriptor(RDMTL::TextureDescriptor &descriptor,
                                              IOSurfaceRef iosurface, NS::UInteger plane);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLTexture *, newTextureWithDescriptor,
                                          RDMTL::TextureDescriptor &descriptor);
  WrappedMTLBuffer *newBufferWithLength(NS::UInteger length, MTL::ResourceOptions options);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLBuffer *, newBufferWithBytes, const void *pointer,
                                          NS::UInteger length, MTL::ResourceOptions options);

  // Non-Serialised MTLDevice APIs
  bool isDepth24Stencil8PixelFormatSupported();
  MTL::ReadWriteTextureTier readWriteTextureSupport();
  MTL::ArgumentBuffersTier argumentBuffersSupport();
  bool areRasterOrderGroupsSupported();
  bool supports32BitFloatFiltering();
  bool supports32BitMSAA();
  bool supportsQueryTextureLOD();
  bool supportsBCTextureCompression();
  bool supportsPullModelInterpolation();
  bool areBarycentricCoordsSupported();
  bool supportsShaderBarycentricCoordinates();
  bool supportsFeatureSet(MTL::FeatureSet featureSet);
  bool supportsFamily(MTL::GPUFamily gpuFamily);
  bool supportsTextureSampleCount(NS::UInteger sampleCount);
  bool areProgrammableSamplePositionsSupported();
  bool supportsRasterizationRateMapWithLayerCount(NS::UInteger layerCount);
  bool supportsCounterSampling(MTL::CounterSamplingPoint samplingPoint);
  bool supportsVertexAmplificationCount(NS::UInteger count);
  bool supportsDynamicLibraries();
  bool supportsRenderDynamicLibraries();
  bool supportsRaytracing();
  bool supportsFunctionPointers();
  bool supportsFunctionPointersFromRender();
  bool supportsRaytracingFromRender();
  bool supportsPrimitiveMotionBlur();
  bool shouldMaximizeConcurrentCompilation();
  NS::UInteger maximumConcurrentCompilationTaskCount();
  // End of MTLDevice APIs

  CaptureState &GetStateRef() { return m_State; }
  CaptureState GetState() { return m_State; }
  MetalResourceManager *GetResourceManager() { return m_ResourceManager; };
  void WaitForGPU();
  WriteSerialiser &GetThreadSerialiser();

  // IFrameCapturer interface
  RDCDriver GetFrameCaptureDriver() { return RDCDriver::Metal; }
  void StartFrameCapture(DeviceOwnedWindow devWnd);
  bool EndFrameCapture(DeviceOwnedWindow devWnd);
  bool DiscardFrameCapture(DeviceOwnedWindow devWnd);
  // IFrameCapturer interface

  void CaptureCmdBufCommit(MetalResourceRecord *cbRecord);
  void CaptureCmdBufEnqueue(MetalResourceRecord *cbRecord);

  void AddFrameCaptureRecordChunk(Chunk *chunk) { m_FrameCaptureRecord->AddChunk(chunk); }
  // From ResourceManager interface
  bool Prepare_InitialState(WrappedMTLObject *res);
  uint64_t GetSize_InitialState(ResourceId id, const MetalInitialContents &initial);
  template <typename SerialiserType>
  bool Serialise_InitialState(SerialiserType &ser, ResourceId id, MetalResourceRecord *record,
                              const MetalInitialContents *initial);
  void Create_InitialState(ResourceId id, WrappedMTLObject *live, bool hasData);
  void Apply_InitialState(WrappedMTLObject *live, const MetalInitialContents &initial);
  // From ResourceManager interface

  void RegisterMetalLayer(CA::MetalLayer *mtlLayer);
  void UnregisterMetalLayer(CA::MetalLayer *mtlLayer);

  void RegisterDrawableInfo(CA::MetalDrawable *caMtlDrawable);
  MetalDrawableInfo UnregisterDrawableInfo(MTL::Drawable *mtlDrawable);

  void AddEvent();
  void AddAction(const ActionDescription &a);

  MetalReplay *GetReplay() { return m_Replay; }
  void AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix);
  void DerivedResource(ResourceId parentLive, ResourceId child);
  template <typename MetalType>
  void DerivedResource(MetalType parent, ResourceId child)
  {
    DerivedResource(GetResID(parent), child);
  }

  void SetLastPresentedIamge(ResourceId lastPresentedImage)
  {
    m_LastPresentedImage = lastPresentedImage;
  }

  enum
  {
    TypeEnum = eResDevice
  };

  static uint64_t g_nextDrawableTLSSlot;
  static IMP g_real_CAMetalLayer_nextDrawable;

private:
  static void MTLFixupForMetalDriverAssert();
  static void MTLHookObjcMethods();
  void FirstFrame();
  void AdvanceFrame();
  void Present(MetalResourceRecord *record);

  void CaptureClearSubmittedCmdBuffers();
  void CaptureCmdBufSubmit(MetalResourceRecord *record);
  void EndCaptureFrame(ResourceId backbuffer);

  template <typename SerialiserType>
  bool Serialise_CaptureScope(SerialiserType &ser);
  template <typename SerialiserType>
  bool Serialise_BeginCaptureFrame(SerialiserType &ser);

  void AddResourceCurChunk(ResourceDescription &descr);

  bool ProcessChunk(ReadSerialiser &ser, MetalChunk chunk);
  WrappedMTLTexture *Common_NewTexture(RDMTL::TextureDescriptor &descriptor, MetalChunk chunkType,
                                       bool ioSurfaceTexture, IOSurfaceRef iosurface,
                                       NS::UInteger plane);
  WrappedMTLBuffer *Common_NewBuffer(bool withBytes, const void *pointer, NS::UInteger length,
                                     MTL::ResourceOptions options);

  MetalResourceManager *m_ResourceManager = NULL;
  ResourceId m_LastPresentedImage;

  // Dummy objects used for serialisation replay
  WrappedMTLBuffer *m_DummyBuffer = NULL;
  WrappedMTLCommandBuffer *m_DummyReplayCommandBuffer = NULL;
  WrappedMTLCommandQueue *m_DummyReplayCommandQueue = NULL;
  WrappedMTLLibrary *m_DummyReplayLibrary = NULL;
  WrappedMTLRenderCommandEncoder *m_DummyReplayRenderCommandEncoder = NULL;
  WrappedMTLBlitCommandEncoder *m_DummyReplayBlitCommandEncoder = NULL;

  MetalReplay *m_Replay = NULL;

  // Back buffer and swap chain emulation
  Threading::CriticalSection m_CapturePotentialBackBuffersLock;
  std::unordered_set<WrappedMTLTexture *> m_CapturePotentialBackBuffers;
  Threading::CriticalSection m_CaptureOutputLayersLock;
  std::unordered_set<CA::MetalLayer *> m_CaptureOutputLayers;
  WrappedMTLTexture *m_CapturedBackbuffer = NULL;
  Threading::CriticalSection m_CaptureDrawablesLock;
  rdcflatmap<MTL::Drawable *, MetalDrawableInfo> m_CaptureDrawableInfos;

  CaptureState m_State;
  bool m_AppControlledCapture = false;
  SDFile *m_StructuredFile = NULL;

  uint64_t threadSerialiserTLSSlot;
  Threading::CriticalSection m_ThreadSerialisersLock;
  rdcarray<WriteSerialiser *> m_ThreadSerialisers;
  uint64_t m_SectionVersion = 0;

  MetalCapturer m_Capturer;
  uint32_t m_FrameCounter = 0;
  rdcarray<FrameDescription> m_CapturedFrames;
  Threading::RWLock m_CapTransitionLock;
  MetalResourceRecord *m_FrameCaptureRecord = NULL;

  // record the command buffer records to insert them individually
  // (even if they were recorded locklessly in parallel)
  // queue submit order will enforce/display ordering, record order is not important
  Threading::CriticalSection m_CaptureCommandBuffersLock;
  rdcarray<MetalResourceRecord *> m_CaptureCommandBuffersEnqueued;
  rdcarray<MetalResourceRecord *> m_CaptureCommandBuffersSubmitted;

  PerformanceTimer m_CaptureTimer;
  MetalInitParams m_InitParams;

  MTL::CommandQueue *m_mtlCommandQueue = NULL;
};

inline void MetalCapturer::StartFrameCapture(DeviceOwnedWindow devWnd)
{
  return m_Device.StartFrameCapture(devWnd);
}
inline bool MetalCapturer::EndFrameCapture(DeviceOwnedWindow devWnd)
{
  return m_Device.EndFrameCapture(devWnd);
}
inline bool MetalCapturer::DiscardFrameCapture(DeviceOwnedWindow devWnd)
{
  return m_Device.DiscardFrameCapture(devWnd);
}
