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

#include "metal_command_buffer.h"
#include "metal_device.h"
#include "metal_render_command_encoder.h"
#include "metal_resources.h"
#include "metal_texture.h"

WrappedMTLCommandBuffer::WrappedMTLCommandBuffer(MTL::CommandBuffer *realMTLCommandBuffer,
                                                 ResourceId objId, WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLCommandBuffer, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_renderCommandEncoderWithDescriptor(
    SerialiserType &ser, WrappedMTLRenderCommandEncoder *encoder,
    RDMTL::RenderPassDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, GetResID(encoder))
      .TypedAs("MTLRenderCommandEncoder"_lit);
  SERIALISE_ELEMENT(descriptor).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // TODO: implement RD MTL replay
  }
  return true;
}

WrappedMTLRenderCommandEncoder *WrappedMTLCommandBuffer::renderCommandEncoderWithDescriptor(
    RDMTL::RenderPassDescriptor &descriptor)
{
  MTL::RenderCommandEncoder *realMTLRenderCommandEncoder;
  MTL::RenderPassDescriptor *mtlDescriptor(descriptor);
  SERIALISE_TIME_CALL(realMTLRenderCommandEncoder =
                          Unwrap(this)->renderCommandEncoder(mtlDescriptor));
  mtlDescriptor->release();
  WrappedMTLRenderCommandEncoder *wrappedMTLRenderCommandEncoder;
  ResourceId id = GetResourceManager()->WrapResource(realMTLRenderCommandEncoder,
                                                     wrappedMTLRenderCommandEncoder);
  wrappedMTLRenderCommandEncoder->SetCommandBuffer(this);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_renderCommandEncoderWithDescriptor);
      Serialise_renderCommandEncoderWithDescriptor(ser, wrappedMTLRenderCommandEncoder, descriptor);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(this);
    bufferRecord->AddChunk(chunk);

    MetalResourceRecord *encoderRecord =
        GetResourceManager()->AddResourceRecord(wrappedMTLRenderCommandEncoder);

    for(int i = 0; i < descriptor.colorAttachments.count(); ++i)
    {
      WrappedMTLTexture *texture = descriptor.colorAttachments[i].texture;
      if(texture != NULL)
      {
        bufferRecord->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_Read);
      }
    }
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, *wrappedMTLLibrary);
  }
  return wrappedMTLRenderCommandEncoder;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_presentDrawable(SerialiserType &ser, MTL::Drawable *drawable)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLCommandBuffer::presentDrawable(MTL::Drawable *drawable)
{
  SERIALISE_TIME_CALL(Unwrap(this)->presentDrawable(drawable));
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_presentDrawable);
      Serialise_presentDrawable(ser, drawable);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(this);
    record->AddChunk(chunk);
    record->cmdInfo->present = true;
    record->cmdInfo->drawable = drawable;
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_commit(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    CommandBuffer->commit();
  }
  return true;
}

void WrappedMTLCommandBuffer::commit()
{
  SERIALISE_TIME_CALL(Unwrap(this)->commit());
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_commit);
      Serialise_commit(ser);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(this);
    bufferRecord->AddChunk(chunk);

    bool capframe = IsActiveCapturing(m_State);
    if(capframe)
    {
      bufferRecord->AddRef();
      bufferRecord->MarkResourceFrameReferenced(GetResID(m_CommandQueue), eFrameRef_Read);
      // pull in frame refs from this command buffer
      bufferRecord->AddResourceReferences(GetResourceManager());
    }
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandBuffer,
                                            WrappedMTLRenderCommandEncoder *encoder,
                                            renderCommandEncoderWithDescriptor,
                                            RDMTL::RenderPassDescriptor &descriptor);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, presentDrawable,
                                MTL::Drawable *drawable);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, commit);
