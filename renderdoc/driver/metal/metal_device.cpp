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
#include "metal_helpers_bridge.h"
#include "metal_library.h"
#include "metal_manager.h"

WrappedMTLDevice::WrappedMTLDevice(MTL::Device *realMTLDevice, ResourceId objId)
    : WrappedMTLObject(realMTLDevice, objId, this, GetStateRef())
{
  objcBridge = AllocateObjCBridge(this);
  m_WrappedMTLDevice = this;
  threadSerialiserTLSSlot = Threading::AllocateTLSSlot();

  m_ResourceManager = new MetalResourceManager(m_State, this);
  RDCASSERT(m_WrappedMTLDevice == this);
  GetResourceManager()->AddCurrentResource(objId, this);
}

WrappedMTLDevice *WrappedMTLDevice::MTLCreateSystemDefaultDevice(MTL::Device *realMTLDevice)
{
  ResourceId objId = ResourceIDGen::GetNewUniqueID();
  WrappedMTLDevice *wrappedMTLDevice = new WrappedMTLDevice(realMTLDevice, objId);

  return wrappedMTLDevice;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newDefaultLibrary(SerialiserType &ser, WrappedMTLLibrary *library)
{
  void *pData;
  uint32_t bytesCount;
  if(ser.IsWriting())
  {
    ObjC::Get_defaultLibraryData(pData, bytesCount);
  }

  SERIALISE_ELEMENT_LOCAL(Library, GetResID(library)).TypedAs("MTLLibrary"_lit);
  SERIALISE_ELEMENT(bytesCount);
  SERIALISE_ELEMENT_ARRAY(pData, bytesCount);

  if(ser.IsWriting())
  {
    free(pData);
  }

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

  SERIALISE_TIME_CALL(realMTLLibrary = GetReal()->newDefaultLibrary());
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
    GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_Read);
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
                                                      MTL::CompileOptions *options)
{
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
  SERIALISE_TIME_CALL(realMTLLibrary = GetReal()->newLibrary(source, options, error));
  WrappedMTLLibrary *wrappedMTLLibrary;
  ResourceId id = GetResourceManager()->WrapResource(realMTLLibrary, wrappedMTLLibrary);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newLibraryWithSource);
      Serialise_newLibraryWithSource(ser, wrappedMTLLibrary, source, options);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLLibrary);
    record->AddChunk(chunk);
    GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLLibrary);
  }
  return wrappedMTLLibrary;
}

template bool WrappedMTLDevice::Serialise_newDefaultLibrary(ReadSerialiser &ser,
                                                            WrappedMTLLibrary *library);
template bool WrappedMTLDevice::Serialise_newDefaultLibrary(WriteSerialiser &ser,
                                                            WrappedMTLLibrary *library);

template bool WrappedMTLDevice::Serialise_newLibraryWithSource(ReadSerialiser &ser,
                                                               WrappedMTLLibrary *library,
                                                               NS::String *source,
                                                               MTL::CompileOptions *options);
template bool WrappedMTLDevice::Serialise_newLibraryWithSource(WriteSerialiser &ser,
                                                               WrappedMTLLibrary *library,
                                                               NS::String *source,
                                                               MTL::CompileOptions *options);
