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

#include "metal_buffer.h"
#include "metal_common.h"
#include "metal_device.h"

static rdcliteral NameOfType(MetalResourceType type)
{
  switch(type)
  {
    case eResBuffer: return "MTLBuffer"_lit;
    default: break;
  }
  return "MTLResource"_lit;
}

bool WrappedMTLDevice::Prepare_InitialState(WrappedMTLObject *res)
{
  ResourceId id = GetResourceManager()->GetID(res);

  MetalResourceType type = res->m_Record->m_Type;

  if(type == eResBuffer)
  {
    WrappedMTLBuffer *buffer = (WrappedMTLBuffer *)res;
    MTL::Buffer *mtlBuffer = Unwrap(buffer);
    MTL::Buffer *mtlSharedBuffer = NULL;
    MTL::StorageMode storageMode = mtlBuffer->storageMode();
    size_t len = mtlBuffer->length();
    byte *data = NULL;
    if(storageMode == MTL::StorageModeShared)
    {
      // MTLStorageModeShared buffers are automatically synchronized
      data = (byte *)mtlBuffer->contents();
    }
    else if(storageMode == MTL::StorageModeManaged)
    {
      // MTLStorageModeManaged buffers need to call MTLBlitCommandEncoder::synchronizeResource
      MTL::CommandBuffer *mtlCommandBuffer = m_mtlCommandQueue->commandBuffer();
      MTL::BlitCommandEncoder *mtlBlitEncoder = mtlCommandBuffer->blitCommandEncoder();
      mtlBlitEncoder->synchronizeResource(mtlBuffer);
      mtlBlitEncoder->endEncoding();
      mtlCommandBuffer->commit();
      mtlCommandBuffer->waitUntilCompleted();
      data = (byte *)mtlBuffer->contents();
    }
    else if(storageMode == MTL::StorageModePrivate)
    {
      // TODO: postpone readback until data is required
      // TODO: batch readback for multiple resources to avoid sync per resource
      // MTLStorageModePrivate buffer need to copy into a temporary MTLStorageModeShared buffer
      mtlSharedBuffer = Unwrap(this)->newBuffer(len, MTL::ResourceStorageModeShared);
      MTL::CommandBuffer *mtlCommandBuffer = m_mtlCommandQueue->commandBuffer();
      MTL::BlitCommandEncoder *mtlBlitEncoder = mtlCommandBuffer->blitCommandEncoder();
      mtlBlitEncoder->copyFromBuffer(mtlBuffer, 0, mtlSharedBuffer, 0, len);
      mtlBlitEncoder->endEncoding();
      mtlCommandBuffer->commit();
      mtlCommandBuffer->waitUntilCompleted();
      data = (byte *)mtlSharedBuffer->contents();
    }
    else
    {
      RDCERR("Unhandled buffer storage mode 0x%X", storageMode);
    }

    bytebuf bufferContents(data, len);
    MetalInitialContents initialContents(type, bufferContents);
    GetResourceManager()->SetInitialContents(id, initialContents);
    if(mtlSharedBuffer)
    {
      mtlSharedBuffer->release();
    }
    if(storageMode == MTL::StorageModeShared)
    {
      // Set the base snapshot to match the initial contents
      MetalBufferInfo *bufInfo = res->m_Record->bufInfo;
      if(bufInfo->baseSnapshot.isEmpty())
        bufInfo->baseSnapshot.resize(len);
      RDCASSERTEQUAL(bufInfo->baseSnapshot.size(), len);
      memcpy(bufInfo->baseSnapshot.data(), bufferContents.data(), len);
    }
    return true;
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }

  return false;
}

uint64_t WrappedMTLDevice::GetSize_InitialState(ResourceId id, const MetalInitialContents &initial)
{
  uint64_t ret = 128;

  if(initial.type == eResBuffer)
  {
    ret += uint64_t(initial.resourceContents.size() + WriteSerialiser::GetChunkAlignment());
    return ret;
  }

  RDCERR("Unhandled resource type %s", ToStr(initial.type).c_str());
  return 0;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_InitialState(SerialiserType &ser, ResourceId id,
                                              MetalResourceRecord *record,
                                              const MetalInitialContents *initial)
{
  SERIALISE_ELEMENT_LOCAL(type, initial->type);
  SERIALISE_ELEMENT(id).TypedAs(NameOfType(type)).Important();
  if(type == eResBuffer)
  {
    SERIALISE_CHECK_READ_ERRORS();

    bytebuf contents;
    if(ser.IsWriting())
    {
      ser.Serialise("Contents"_lit, initial->resourceContents);
    }
    else
    {
      ser.Serialise("Contents"_lit, contents);
    }

    if(IsReplayingAndReading())
    {
      // TODO: implement RD MTL replay
    }
    return true;
  }
  RDCERR("Unhandled resource type %d", type);
  return false;
}

void WrappedMTLDevice::Create_InitialState(ResourceId id, WrappedMTLObject *live, bool hasData)
{
  METAL_NOT_IMPLEMENTED();
}

void WrappedMTLDevice::Apply_InitialState(WrappedMTLObject *live, const MetalInitialContents &initial)
{
  METAL_NOT_IMPLEMENTED();
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLDevice, void, InitialState, ResourceId id,
                                MetalResourceRecord *record, const MetalInitialContents *initial);
