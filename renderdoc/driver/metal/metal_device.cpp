/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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
#include "metal_core.h"
#include "metal_function.h"
#include "metal_helpers_bridge.h"
#include "metal_library.h"
#include "metal_manager.h"
#include "metal_render_pipeline_state.h"
#include "metal_texture.h"

WrappedMTLDevice::WrappedMTLDevice(MTL::Device *realMTLDevice, ResourceId objId)
    : WrappedMTLObject(realMTLDevice, objId, this, GetStateRef())
{
  objcBridge = AllocateObjCBridge(this);
  m_WrappedMTLDevice = this;

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

  m_HeaderChunk = NULL;

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

  RDCASSERT(m_WrappedMTLDevice == this);
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

  FirstFrame();
}

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
    // TODO: implement RD MTL replay
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
    ObjC::Get_defaultLibraryData(data);
  }

  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(Library, GetResID(library)).TypedAs("MTLLibrary"_lit);
  SERIALISE_ELEMENT(data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // TODO: implement RD MTL replay
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
    // TODO: implement RD MTL replay
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
bool WrappedMTLDevice::Serialise_newBuffer(SerialiserType &ser, WrappedMTLBuffer *buffer,
                                           const void *pointer, NS::UInteger length,
                                           MTL::ResourceOptions options)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, GetResID(buffer)).TypedAs("MTLBuffer"_lit);
  SERIALISE_ELEMENT_ARRAY(pointer, length);
  SERIALISE_ELEMENT(length);
  SERIALISE_ELEMENT(options);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

WrappedMTLBuffer *WrappedMTLDevice::newBufferWithLength(NS::UInteger length,
                                                        MTL::ResourceOptions options)
{
  MTL::Buffer *realMTLBuffer;
  SERIALISE_TIME_CALL(realMTLBuffer = Unwrap(this)->newBuffer(length, options));

  WrappedMTLBuffer *wrappedMTLBuffer;
  ResourceId id = GetResourceManager()->WrapResource(realMTLBuffer, wrappedMTLBuffer);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newBufferWithLength);
      Serialise_newBuffer(ser, wrappedMTLBuffer, NULL, length, options);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLBuffer);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLBuffer);
  }
  return wrappedMTLBuffer;
}

WrappedMTLBuffer *WrappedMTLDevice::newBufferWithBytes(const void *pointer, NS::UInteger length,
                                                       MTL::ResourceOptions options)
{
  MTL::Buffer *realMTLBuffer;
  SERIALISE_TIME_CALL(realMTLBuffer = Unwrap(this)->newBuffer(pointer, length, options));

  WrappedMTLBuffer *wrappedMTLBuffer;
  ResourceId id = GetResourceManager()->WrapResource(realMTLBuffer, wrappedMTLBuffer);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newBufferWithBytes);
      Serialise_newBuffer(ser, wrappedMTLBuffer, pointer, length, options);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLBuffer);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLBuffer);
  }
  return wrappedMTLBuffer;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newRenderPipelineStateWithDescriptor(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipelineState,
    MTL::RenderPipelineDescriptor *descriptor, NS::Error **error)
{
  SERIALISE_ELEMENT_LOCAL(RenderPipelineState, GetResID(pipelineState))
      .TypedAs("MTLRenderPipelineState"_lit);
  SERIALISE_ELEMENT(descriptor);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

WrappedMTLRenderPipelineState *WrappedMTLDevice::newRenderPipelineStateWithDescriptor(
    MTL::RenderPipelineDescriptor *descriptor, NS::Error **error)
{
  MTL::RenderPipelineDescriptor *realDescriptor = descriptor->copy();

  // The source descriptor contains wrapped MTLFunction resources
  // These need to be unwrapped in the clone which is used when calling real API
  MTL::Function *wrappedVertexFunction = descriptor->vertexFunction();
  if(wrappedVertexFunction != NULL)
  {
    realDescriptor->setVertexFunction(GetReal(wrappedVertexFunction));
  }

  MTL::Function *wrappedFragmentFunction = descriptor->fragmentFunction();
  if(wrappedFragmentFunction != NULL)
  {
    realDescriptor->setFragmentFunction(GetReal(wrappedFragmentFunction));
  }

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
    record->AddParent(GetRecord(GetWrapped((MTL::Function *)descriptor->vertexFunction())));
    record->AddParent(GetRecord(GetWrapped((MTL::Function *)descriptor->fragmentFunction())));
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
                                                          MTL::TextureDescriptor *descriptor,
                                                          IOSurfaceRef iosurface, NS::UInteger plane)
{
  SERIALISE_ELEMENT_LOCAL(Texture, GetResID(texture)).TypedAs("MTLTexture"_lit);
  SERIALISE_ELEMENT(descriptor);
  SERIALISE_ELEMENT_LOCAL(IOSurfaceRef, (uint64_t)iosurface).TypedAs("IOSurfaceRef"_lit);
  SERIALISE_ELEMENT(plane);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
  }
  return true;
}

WrappedMTLTexture *WrappedMTLDevice::newTextureWithDescriptor(MTL::TextureDescriptor *descriptor)
{
  MTL::Texture *realMTLTexture;
  SERIALISE_TIME_CALL(realMTLTexture = Unwrap(this)->newTexture(descriptor));
  WrappedMTLTexture *wrappedMTLTexture;
  ResourceId id = GetResourceManager()->WrapResource(realMTLTexture, wrappedMTLTexture);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newTextureWithDescriptor);
      Serialise_newTextureWithDescriptor(ser, wrappedMTLTexture, descriptor, NULL, ~0);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLTexture);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLCommandQueue);
  }
  return wrappedMTLTexture;
}

WrappedMTLTexture *WrappedMTLDevice::newTextureWithDescriptor(MTL::TextureDescriptor *descriptor,
                                                              IOSurfaceRef iosurface,
                                                              NS::UInteger plane)
{
  void *window = (void *)this;
  RenderDoc::Inst().AddFrameCapturer(this, window, this);

  // TODO: this is modifying in place the application descriptor
  // TODO: should only modify this when Capturing
  // TODO: what about other usage types ie. MTLTextureUsageShaderRead
  if(descriptor->usage() == MTL::TextureUsageRenderTarget)
    descriptor->setUsage(MTL::TextureUsageUnknown);

  MTL::Texture *realMTLTexture;
  SERIALISE_TIME_CALL(realMTLTexture = Unwrap(this)->newTexture(descriptor, iosurface, plane));
  WrappedMTLTexture *wrappedMTLTexture;
  ResourceId id = GetResourceManager()->WrapResource(realMTLTexture, wrappedMTLTexture);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newTextureWithDescriptor);
      Serialise_newTextureWithDescriptor(ser, wrappedMTLTexture, descriptor, iosurface, plane);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLTexture);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLCommandQueue);
  }
  return wrappedMTLTexture;
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
                                            MTL::RenderPipelineDescriptor *descriptor,
                                            NS::Error **error);
template bool WrappedMTLDevice::Serialise_newBuffer(ReadSerialiser &ser, WrappedMTLBuffer *buffer,
                                                    const void *pointer, NS::UInteger length,
                                                    MTL::ResourceOptions options);
template bool WrappedMTLDevice::Serialise_newBuffer(WriteSerialiser &ser, WrappedMTLBuffer *buffer,
                                                    const void *pointer, NS::UInteger length,
                                                    MTL::ResourceOptions options);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLTexture *texture,
                                            newTextureWithDescriptor,
                                            MTL::TextureDescriptor *descriptor,
                                            IOSurfaceRef iosurface, NS::UInteger plane);
