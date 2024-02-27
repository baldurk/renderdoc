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

#include "metal_device.h"
#include "metal_buffer.h"
#include "metal_command_queue.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_manager.h"
#include "metal_render_pipeline_state.h"
#include "metal_texture.h"

WrappedMTLDevice::WrappedMTLDevice(MTL::Device *realMTLDevice, ResourceId objId)
    : WrappedMTLObject(realMTLDevice, objId, this, GetStateRef()), m_Capturer(*this)
{
  AllocateObjCBridge(this);
  m_Device = this;

  if(RenderDoc::Inst().IsReplayApp())
  {
  }
  else
  {
    m_State = CaptureState::BackgroundCapturing;
  }

  m_SectionVersion = MetalInitParams::CurrentVersion;

  threadSerialiserTLSSlot = Threading::AllocateTLSSlot();

  m_ResourceManager = new MetalResourceManager(m_State, this);

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_FrameCaptureRecord->DataInSerialiser = false;
    m_FrameCaptureRecord->Length = 0;
    m_FrameCaptureRecord->InternalResource = true;
  }
  else
  {
    m_FrameCaptureRecord = NULL;

    ResourceIDGen::SetReplayResourceIDs();
  }

  RDCASSERT(m_Device == this);
  GetResourceManager()->AddCurrentResource(objId, this);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCreateSystemDefaultDevice);
      Serialise_MTLCreateSystemDefaultDevice(ser);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(this);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }

  RenderDoc::Inst().AddDeviceFrameCapturer(this, &m_Capturer);

  m_mtlCommandQueue = Unwrap(this)->newCommandQueue();
  FirstFrame();
}

IMP WrappedMTLDevice::g_real_CAMetalLayer_nextDrawable;
uint64_t WrappedMTLDevice::g_nextDrawableTLSSlot;

CA::MetalDrawable *hooked_CAMetalLayer_nextDrawable(id self, SEL _cmd)
{
  CA::MetalLayer *mtlLayer = (CA::MetalLayer *)self;
  MTL::Device *mtlDevice = mtlLayer->device();
  WrappedMTLDevice *device = GetWrapped(mtlDevice);
  RDCASSERT(object_getClass(mtlDevice) == objc_getClass("ObjCBridgeMTLDevice"));
  device->RegisterMetalLayer(mtlLayer);
  mtlLayer->setFramebufferOnly(false);

  RDCASSERTEQUAL(Threading::GetTLSValue(WrappedMTLDevice::g_nextDrawableTLSSlot), 0);
  Threading::SetTLSValue(WrappedMTLDevice::g_nextDrawableTLSSlot, (void *)(uintptr_t) true);
  CA::MetalDrawable *caMtlDrawable =
      ((CA::MetalDrawable * (*)(id, SEL)) WrappedMTLDevice::g_real_CAMetalLayer_nextDrawable)(self,
                                                                                              _cmd);
  device->RegisterDrawableInfo(caMtlDrawable);
  Threading::SetTLSValue(WrappedMTLDevice::g_nextDrawableTLSSlot, (void *)(uintptr_t) false);
  return caMtlDrawable;
}

void WrappedMTLDevice::MTLHookObjcMethods()
{
  static bool s_hookObjcMethods = false;
  if(s_hookObjcMethods)
    return;

  g_nextDrawableTLSSlot = Threading::AllocateTLSSlot();
  Threading::SetTLSValue(WrappedMTLDevice::g_nextDrawableTLSSlot, (void *)(uintptr_t) false);

  Method m =
      class_getInstanceMethod(objc_lookUpClass("CAMetalLayer"), sel_registerName("nextDrawable"));
  g_real_CAMetalLayer_nextDrawable =
      method_setImplementation(m, (IMP)hooked_CAMetalLayer_nextDrawable);
  s_hookObjcMethods = true;
}

void WrappedMTLDevice::MTLFixupForMetalDriverAssert()
{
  static bool s_fixupMetalDriverAssert = false;
  if(s_fixupMetalDriverAssert)
    return;

  RDCLOG(
      "Fixup for Metal Driver debug assert. Adding protocol `MTLTextureImplementation` to "
      "`ObjCBridgeMTLTexture`");
  class_addProtocol(objc_lookUpClass("ObjCBridgeMTLTexture"),
                    objc_getProtocol("MTLTextureImplementation"));
  s_fixupMetalDriverAssert = true;
}

// Serialised MTLDevice APIs

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_MTLCreateSystemDefaultDevice(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(Device, GetResID(this)).TypedAs("MTLDevice"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // TODO: implement RD MTL replay
  }
  return true;
}

WrappedMTLDevice *WrappedMTLDevice::MTLCreateSystemDefaultDevice(MTL::Device *realMTLDevice)
{
  MTLFixupForMetalDriverAssert();
  MTLHookObjcMethods();
  ResourceId objId = ResourceIDGen::GetNewUniqueID();
  WrappedMTLDevice *wrappedMTLDevice = new WrappedMTLDevice(realMTLDevice, objId);

  return wrappedMTLDevice;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newCommandQueue(SerialiserType &ser, WrappedMTLCommandQueue *queue)
{
  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(CommandQueue, GetResID(queue)).TypedAs("MTLCommandQueue"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::CommandQueue *realMTLCommandQueue = Unwrap(this)->newCommandQueue();
    WrappedMTLCommandQueue *wrappedMTLCommandQueue;
    GetResourceManager()->WrapResource(realMTLCommandQueue, wrappedMTLCommandQueue);
    GetResourceManager()->AddLiveResource(CommandQueue, wrappedMTLCommandQueue);

    AddResource(CommandQueue, ResourceType::Queue, "Queue");
    DerivedResource(this, CommandQueue);
  }
  return true;
}

WrappedMTLCommandQueue *WrappedMTLDevice::newCommandQueue()
{
  MTL::CommandQueue *realMTLCommandQueue;
  SERIALISE_TIME_CALL(realMTLCommandQueue = Unwrap(this)->newCommandQueue());
  WrappedMTLCommandQueue *wrappedMTLCommandQueue;
  ResourceId id = GetResourceManager()->WrapResource(realMTLCommandQueue, wrappedMTLCommandQueue);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newCommandQueue);
      Serialise_newCommandQueue(ser, wrappedMTLCommandQueue);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLCommandQueue);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLCommandQueue);
  }
  return wrappedMTLCommandQueue;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newDefaultLibrary(SerialiserType &ser, WrappedMTLLibrary *library)
{
  bytebuf data;
  if(ser.IsWriting())
  {
    NS::String *defaultType = NS::String::string("default", NS::UTF8StringEncoding);
    NS::String *metallibExt = NS::String::string("metallib", NS::UTF8StringEncoding);
    NS::Bundle *mainAppBundle = NS::Bundle::mainBundle();
    NS::String *defaultLibaryPath = mainAppBundle->pathForResource(defaultType, metallibExt);
    NS::Data *fileData = NS::Data::dataWithContentsOfFile(defaultLibaryPath);
    dispatch_data_t dispatchData =
        dispatch_data_create(fileData->bytes(), fileData->length(), dispatch_get_main_queue(),
                             DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NS::Data *nsData = (NS::Data *)dispatchData;
    data.assign((byte *)nsData->bytes(), nsData->length());
    dispatch_release(dispatchData);
    defaultType->release();
    metallibExt->release();
  }

  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(Library, GetResID(library)).TypedAs("MTLLibrary"_lit);
  SERIALISE_ELEMENT(data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    dispatch_data_t dispatchData = dispatch_data_create(
        data.data(), data.size(), dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NS::Error *error;
    MTL::Library *realMTLLibrary = Unwrap(this)->newLibrary(dispatchData, &error);
    dispatch_release(dispatchData);

    WrappedMTLLibrary *wrappedMTLLibrary;
    GetResourceManager()->WrapResource(realMTLLibrary, wrappedMTLLibrary);
    GetResourceManager()->AddLiveResource(Library, wrappedMTLLibrary);
    AddResource(Library, ResourceType::Pool, "Library");
    DerivedResource(this, Library);
  }
  return true;
}

WrappedMTLLibrary *WrappedMTLDevice::newDefaultLibrary()
{
  MTL::Library *realMTLLibrary;

  SERIALISE_TIME_CALL(realMTLLibrary = Unwrap(this)->newDefaultLibrary());
  WrappedMTLLibrary *wrappedMTLLibrary;
  ResourceId id = GetResourceManager()->WrapResource(realMTLLibrary, wrappedMTLLibrary);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newDefaultLibrary);
      Serialise_newDefaultLibrary(ser, wrappedMTLLibrary);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLLibrary);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLLibrary);
  }
  return wrappedMTLLibrary;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newLibraryWithSource(SerialiserType &ser,
                                                      WrappedMTLLibrary *library, NS::String *source,
                                                      MTL::CompileOptions *options, NS::Error **error)
{
  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(Library, GetResID(library)).TypedAs("MTLLibrary"_lit);
  SERIALISE_ELEMENT(source);
  // TODO:SERIALISE_ELEMENT(options);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    NS::Error *compileErrors = NULL;
    MTL::Library *realMTLLibrary = Unwrap(this)->newLibrary(source, options, &compileErrors);
    WrappedMTLLibrary *wrappedMTLLibrary;
    GetResourceManager()->WrapResource(realMTLLibrary, wrappedMTLLibrary);
    GetResourceManager()->AddLiveResource(Library, wrappedMTLLibrary);
    AddResource(Library, ResourceType::Pool, "Library");
    DerivedResource(this, Library);
  }
  return true;
}

WrappedMTLLibrary *WrappedMTLDevice::newLibraryWithSource(NS::String *source,
                                                          MTL::CompileOptions *options,
                                                          NS::Error **error)
{
  MTL::Library *realMTLLibrary;
  SERIALISE_TIME_CALL(realMTLLibrary = Unwrap(this)->newLibrary(source, options, error));
  WrappedMTLLibrary *wrappedMTLLibrary;
  ResourceId id = GetResourceManager()->WrapResource(realMTLLibrary, wrappedMTLLibrary);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newLibraryWithSource);
      Serialise_newLibraryWithSource(ser, wrappedMTLLibrary, source, options, error);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLLibrary);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLLibrary);
  }
  return wrappedMTLLibrary;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newBufferWithBytes(SerialiserType &ser, WrappedMTLBuffer *buffer,
                                                    const void *pointer, NS::UInteger length,
                                                    MTL::ResourceOptions options)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, GetResID(buffer)).TypedAs("MTLBuffer"_lit);
  bytebuf initialData;
  if(pointer)
  {
    initialData.assign((byte *)pointer, length);
  }
  SERIALISE_ELEMENT(initialData);
  SERIALISE_ELEMENT(length).Important();
  SERIALISE_ELEMENT(options);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    MTL::Buffer *realMTLBuffer;
    if(initialData.isEmpty())
    {
      realMTLBuffer = Unwrap(this)->newBuffer(length, options);
    }
    else
    {
      RDCASSERT(initialData.size() == length);
      realMTLBuffer = Unwrap(this)->newBuffer(initialData.data(), initialData.size(), options);
    }
    WrappedMTLBuffer *wrappedMTLBuffer;
    GetResourceManager()->WrapResource(realMTLBuffer, wrappedMTLBuffer);
    GetResourceManager()->AddLiveResource(Buffer, wrappedMTLBuffer);

    AddResource(Buffer, ResourceType::Buffer, "Buffer");
    DerivedResource(this, Buffer);
  }
  return true;
}

WrappedMTLBuffer *WrappedMTLDevice::newBufferWithBytes(const void *pointer, NS::UInteger length,
                                                       MTL::ResourceOptions options)
{
  return Common_NewBuffer(true, pointer, length, options);
}

WrappedMTLBuffer *WrappedMTLDevice::newBufferWithLength(NS::UInteger length,
                                                        MTL::ResourceOptions options)
{
  return Common_NewBuffer(false, NULL, length, options);
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newRenderPipelineStateWithDescriptor(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipelineState,
    RDMTL::RenderPipelineDescriptor &descriptor, NS::Error **error)
{
  SERIALISE_ELEMENT_LOCAL(RenderPipelineState, GetResID(pipelineState))
      .TypedAs("MTLRenderPipelineState"_lit);
  SERIALISE_ELEMENT(descriptor);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    ResourceId liveID;

    MTL::RenderPipelineDescriptor *mtlDescriptor(descriptor);
    MTL::RenderPipelineState *realMTLRenderPipelineState =
        Unwrap(this)->newRenderPipelineState(mtlDescriptor, error);
    mtlDescriptor->release();
    WrappedMTLRenderPipelineState *wrappedMTLRenderPipelineState;
    liveID = GetResourceManager()->WrapResource(realMTLRenderPipelineState,
                                                wrappedMTLRenderPipelineState);
    GetResourceManager()->AddLiveResource(RenderPipelineState, wrappedMTLRenderPipelineState);
    AddResource(RenderPipelineState, ResourceType::PipelineState, "Pipeline State");
    DerivedResource(this, RenderPipelineState);
  }
  return true;
}

WrappedMTLRenderPipelineState *WrappedMTLDevice::newRenderPipelineStateWithDescriptor(
    RDMTL::RenderPipelineDescriptor &descriptor, NS::Error **error)
{
  MTL::RenderPipelineDescriptor *realDescriptor(descriptor);
  MTL::RenderPipelineState *realMTLRenderPipelineState;
  SERIALISE_TIME_CALL(realMTLRenderPipelineState =
                          Unwrap(this)->newRenderPipelineState(realDescriptor, error));
  realDescriptor->release();

  WrappedMTLRenderPipelineState *wrappedMTLRenderPipelineState;
  ResourceId id =
      GetResourceManager()->WrapResource(realMTLRenderPipelineState, wrappedMTLRenderPipelineState);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor);
      Serialise_newRenderPipelineStateWithDescriptor(ser, wrappedMTLRenderPipelineState, descriptor,
                                                     error);
      chunk = scope.Get();
    }

    MetalResourceRecord *record =
        GetResourceManager()->AddResourceRecord(wrappedMTLRenderPipelineState);
    record->AddChunk(chunk);
    if(descriptor.vertexFunction)
    {
      record->AddParent(GetRecord(descriptor.vertexFunction));
    }
    if(descriptor.fragmentFunction)
    {
      record->AddParent(GetRecord(descriptor.fragmentFunction));
    }
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, *wrappedMTLRenderPipelineState);
  }
  return wrappedMTLRenderPipelineState;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newTextureWithDescriptor(SerialiserType &ser,
                                                          WrappedMTLTexture *texture,
                                                          RDMTL::TextureDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(Texture, GetResID(texture)).TypedAs("MTLTexture"_lit);
  SERIALISE_ELEMENT(descriptor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // Ensure the created textures can be read by a shader
    // Metal driver will treat TextureUsageUnknown as all options
    if(descriptor.usage != MTL::TextureUsageUnknown)
      descriptor.usage = (MTL::TextureUsage)(descriptor.usage | MTL::TextureUsageShaderRead);

    MTL::TextureDescriptor *mtlDescriptor(descriptor);
    MTL::Texture *realMTLTexture = Unwrap(this)->newTexture(mtlDescriptor);
    mtlDescriptor->release();
    WrappedMTLTexture *wrappedMTLTexture;
    ResourceId liveID = GetResourceManager()->WrapResource(realMTLTexture, wrappedMTLTexture);
    GetResourceManager()->AddLiveResource(Texture, wrappedMTLTexture);

    AddResource(Texture, ResourceType::Texture, "Texture");
    DerivedResource(this, Texture);
  }
  return true;
}

WrappedMTLTexture *WrappedMTLDevice::newTextureWithDescriptor(RDMTL::TextureDescriptor &descriptor)
{
  return Common_NewTexture(descriptor, MetalChunk::MTLDevice_newTextureWithDescriptor, false, NULL,
                           0);
}

WrappedMTLTexture *WrappedMTLDevice::newTextureWithDescriptor(RDMTL::TextureDescriptor &descriptor,
                                                              IOSurfaceRef iosurface,
                                                              NS::UInteger plane)
{
  bool nextDrawable = (bool)(uintptr_t)Threading::GetTLSValue(g_nextDrawableTLSSlot);
  return Common_NewTexture(descriptor,
                           nextDrawable ? MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable
                                        : MetalChunk::MTLDevice_newTextureWithDescriptor_iosurface,
                           true, iosurface, plane);
}

// Non-Serialised MTLDevice APIs

bool WrappedMTLDevice::isDepth24Stencil8PixelFormatSupported()
{
  return Unwrap(this)->depth24Stencil8PixelFormatSupported();
}

MTL::ReadWriteTextureTier WrappedMTLDevice::readWriteTextureSupport()
{
  return Unwrap(this)->readWriteTextureSupport();
}

MTL::ArgumentBuffersTier WrappedMTLDevice::argumentBuffersSupport()
{
  return Unwrap(this)->argumentBuffersSupport();
}

bool WrappedMTLDevice::areRasterOrderGroupsSupported()
{
  return Unwrap(this)->rasterOrderGroupsSupported();
}

bool WrappedMTLDevice::supports32BitFloatFiltering()
{
  return Unwrap(this)->supports32BitFloatFiltering();
}

bool WrappedMTLDevice::supports32BitMSAA()
{
  return Unwrap(this)->supports32BitMSAA();
}

bool WrappedMTLDevice::supportsQueryTextureLOD()
{
  return Unwrap(this)->supportsQueryTextureLOD();
}

bool WrappedMTLDevice::supportsBCTextureCompression()
{
  return Unwrap(this)->supportsBCTextureCompression();
}

bool WrappedMTLDevice::supportsPullModelInterpolation()
{
  return Unwrap(this)->supportsPullModelInterpolation();
}

bool WrappedMTLDevice::areBarycentricCoordsSupported()
{
  return Unwrap(this)->barycentricCoordsSupported();
}

bool WrappedMTLDevice::supportsShaderBarycentricCoordinates()
{
  return Unwrap(this)->supportsShaderBarycentricCoordinates();
}

bool WrappedMTLDevice::supportsFeatureSet(MTL::FeatureSet featureSet)
{
  return Unwrap(this)->supportsFeatureSet(featureSet);
}

bool WrappedMTLDevice::supportsFamily(MTL::GPUFamily gpuFamily)
{
  return Unwrap(this)->supportsFamily(gpuFamily);
}

bool WrappedMTLDevice::supportsTextureSampleCount(NS::UInteger sampleCount)
{
  return Unwrap(this)->supportsTextureSampleCount(sampleCount);
}

bool WrappedMTLDevice::areProgrammableSamplePositionsSupported()
{
  return Unwrap(this)->programmableSamplePositionsSupported();
}

bool WrappedMTLDevice::supportsRasterizationRateMapWithLayerCount(NS::UInteger layerCount)
{
  return Unwrap(this)->supportsRasterizationRateMap(layerCount);
}

bool WrappedMTLDevice::supportsCounterSampling(MTL::CounterSamplingPoint samplingPoint)
{
  return Unwrap(this)->supportsCounterSampling(samplingPoint);
}

bool WrappedMTLDevice::supportsVertexAmplificationCount(NS::UInteger count)
{
  return Unwrap(this)->supportsVertexAmplificationCount(count);
}

bool WrappedMTLDevice::supportsDynamicLibraries()
{
  return Unwrap(this)->supportsDynamicLibraries();
}

bool WrappedMTLDevice::supportsRenderDynamicLibraries()
{
  return Unwrap(this)->supportsRenderDynamicLibraries();
}

bool WrappedMTLDevice::supportsRaytracing()
{
  // RD device does not support ray tracing
  return false;
}

bool WrappedMTLDevice::supportsFunctionPointers()
{
  return Unwrap(this)->supportsFunctionPointers();
}

bool WrappedMTLDevice::supportsFunctionPointersFromRender()
{
  return Unwrap(this)->supportsFunctionPointersFromRender();
}

bool WrappedMTLDevice::supportsRaytracingFromRender()
{
  // RD device does not support ray tracing
  return false;
}

bool WrappedMTLDevice::supportsPrimitiveMotionBlur()
{
  return Unwrap(this)->supportsPrimitiveMotionBlur();
}

bool WrappedMTLDevice::shouldMaximizeConcurrentCompilation()
{
  return Unwrap(this)->shouldMaximizeConcurrentCompilation();
}

NS::UInteger WrappedMTLDevice::maximumConcurrentCompilationTaskCount()
{
  return Unwrap(this)->maximumConcurrentCompilationTaskCount();
}

// End of MTLDevice APIs

WrappedMTLTexture *WrappedMTLDevice::Common_NewTexture(RDMTL::TextureDescriptor &descriptor,
                                                       MetalChunk chunkType, bool ioSurfaceTexture,
                                                       IOSurfaceRef iosurface, NS::UInteger plane)
{
  MTL::Texture *realMTLTexture;
  MTL::TextureDescriptor *realDescriptor(descriptor);
  // Ensure the created textures can be read by a shader
  // Metal driver will treat TextureUsageUnknown as all options
  MTL::TextureUsage usage = realDescriptor->usage();
  if(usage != MTL::TextureUsageUnknown)
    realDescriptor->setUsage((MTL::TextureUsage)(usage | MTL::TextureUsageShaderRead));

  SERIALISE_TIME_CALL(realMTLTexture = !ioSurfaceTexture ? Unwrap(this)->newTexture(realDescriptor)
                                                         : Unwrap(this)->newTexture(
                                                               realDescriptor, iosurface, plane));
  realDescriptor->release();
  WrappedMTLTexture *wrappedMTLTexture;
  ResourceId id = GetResourceManager()->WrapResource(realMTLTexture, wrappedMTLTexture);
  if(IsCaptureMode(m_State))
  {
    RDMTL::TextureDescriptor rdDescriptor(descriptor);
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(chunkType);
      Serialise_newTextureWithDescriptor(ser, wrappedMTLTexture, rdDescriptor);
      chunk = scope.Get();
    }
    MetalResourceRecord *textureRecord = GetResourceManager()->AddResourceRecord(wrappedMTLTexture);
    textureRecord->AddChunk(chunk);
  }
  if(ioSurfaceTexture)
  {
    if(IsCaptureMode(m_State))
    {
      {
        SCOPED_LOCK(m_CapturePotentialBackBuffersLock);
        m_CapturePotentialBackBuffers.insert(wrappedMTLTexture);
      }
    }
  }
  return wrappedMTLTexture;
}

WrappedMTLBuffer *WrappedMTLDevice::Common_NewBuffer(bool withBytes, const void *pointer,
                                                     NS::UInteger length,
                                                     MTL::ResourceOptions options)
{
  MTL::Buffer *realMTLBuffer;
  SERIALISE_TIME_CALL(realMTLBuffer = withBytes ? Unwrap(this)->newBuffer(pointer, length, options)
                                                : Unwrap(this)->newBuffer(length, options));

  WrappedMTLBuffer *wrappedMTLBuffer;
  ResourceId id = GetResourceManager()->WrapResource(realMTLBuffer, wrappedMTLBuffer);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(withBytes ? MetalChunk::MTLDevice_newBufferWithBytes
                                       : MetalChunk::MTLDevice_newBufferWithLength);
      Serialise_newBufferWithBytes(ser, wrappedMTLBuffer, pointer, length, options);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLBuffer);
    record->AddChunk(chunk);

    MTL::StorageMode mode = realMTLBuffer->storageMode();
    record->bufInfo = new MetalBufferInfo(mode);

    // Create CPU side tracking info for CPU shared buffers
    if(mode == MTL::StorageModeShared)
    {
      record->bufInfo->data = (byte *)realMTLBuffer->contents();
      record->bufInfo->length = realMTLBuffer->length();
    }
    // Snapshot GPU only buffers
    else if(mode == MTL::StorageModePrivate)
    {
      GetResourceManager()->MarkDirtyResource(id);
    }
  }
  else
  {
    // TODO: implement RD MTL replay
  }
  return wrappedMTLBuffer;
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLDevice, bool, MTLCreateSystemDefaultDevice);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLCommandQueue *,
                                            newCommandQueue);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLLibrary *, newDefaultLibrary);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLLibrary *,
                                            newLibraryWithSource, NS::String *source,
                                            MTL::CompileOptions *options, NS::Error **error);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice,
                                            WrappedMTLRenderPipelineState *renderPipelineState,
                                            newRenderPipelineStateWithDescriptor,
                                            RDMTL::RenderPipelineDescriptor &descriptor,
                                            NS::Error **error);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLTexture *,
                                            newTextureWithDescriptor,
                                            RDMTL::TextureDescriptor &descriptor);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLBuffer *,
                                            newBufferWithBytes, const void *pointer,
                                            NS::UInteger length, MTL::ResourceOptions options);
