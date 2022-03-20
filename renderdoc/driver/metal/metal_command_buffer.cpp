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
#include "core/settings.h"
#include "metal_device.h"
#include "metal_render_command_encoder.h"
#include "metal_resources.h"

WrappedMTLCommandBuffer::WrappedMTLCommandBuffer(MTL::CommandBuffer *realMTLCommandBuffer,
                                                 ResourceId objId, WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLCommandBuffer, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  objcBridge = AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_renderCommandEncoderWithDescriptor(
    SerialiserType &ser, WrappedMTLRenderCommandEncoder *encoder,
    MTL::RenderPassDescriptor *descriptor)
{
  RDCASSERT(m_WrappedMTLCommandQueue);
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, GetResID(encoder))
      .TypedAs("MTLRenderCommandEncoder"_lit);
  SERIALISE_ELEMENT(descriptor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // TODO: implement RD MTL replay
  }
  return true;
}

WrappedMTLRenderCommandEncoder *WrappedMTLCommandBuffer::renderCommandEncoderWithDescriptor(
    MTL::RenderPassDescriptor *descriptor)
{
  MTL::RenderCommandEncoder *realMTLRenderCommandEncoder;
  SERIALISE_TIME_CALL(realMTLRenderCommandEncoder =
                          CreateRenderCommandEncoderWithDescriptor(descriptor));
  WrappedMTLRenderCommandEncoder *wrappedMTLRenderCommandEncoder;
  ResourceId id = GetResourceManager()->WrapResource(realMTLRenderCommandEncoder,
                                                     wrappedMTLRenderCommandEncoder);
  wrappedMTLRenderCommandEncoder->SetWrappedMTLCommandBuffer(this);
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
    RDCASSERT(!bufferRecord->cmdInfo->isEncoding);
    bufferRecord->AddChunk(chunk);

    MetalResourceRecord *encoderRecord =
        GetResourceManager()->AddResourceRecord(wrappedMTLRenderCommandEncoder);
    // TODO: mark texture attachments as frame referenced
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
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_WrappedMTLCommandQueue),
                                                      eFrameRef_Read);
    MetalResourceRecord *queueRecord = GetRecord(m_WrappedMTLCommandQueue);
    MetalResourceRecord *record = GetRecord(this);
    record->AddParent(queueRecord);
    record->AddChunk(chunk);

    bool capframe = IsActiveCapturing(m_State);

    if(capframe)
    {
      record->AddRef();
    }
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

MTL::RenderCommandEncoder *WrappedMTLCommandBuffer::CreateRenderCommandEncoderWithDescriptor(
    MTL::RenderPassDescriptor *descriptor)
{
  MTL::RenderPassDescriptor *realDescriptor = descriptor->copy();

  // The source descriptor contains wrapped MTLTexture resources
  // Need to unwrap them to the real resource before calling real API
  for(uint32_t i = 0; i < MAX_RENDER_PASS_COLOR_ATTACHMENTS; ++i)
  {
    MTL::Texture *wrappedTexture = descriptor->colorAttachments()->object(i)->texture();
    if(wrappedTexture != NULL)
    {
      if(IsObjCBridge(wrappedTexture))
      {
        realDescriptor->colorAttachments()->object(i)->setTexture(GetReal(wrappedTexture));
      }
    }
  }

  MTL::RenderCommandEncoder *realMTLRenderCommandEncoder =
      Unwrap(this)->renderCommandEncoder(realDescriptor);
  realDescriptor->release();

  return realMTLRenderCommandEncoder;
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandBuffer,
                                            WrappedMTLRenderCommandEncoder *encoder,
                                            renderCommandEncoderWithDescriptor,
                                            MTL::RenderPassDescriptor *descriptor);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, presentDrawable,
                                MTL::Drawable *drawable);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, commit);
