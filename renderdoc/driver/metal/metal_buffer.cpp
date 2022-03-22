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

#include "metal_buffer.h"
#include "core/core.h"
#include "metal_resources.h"

WrappedMTLBuffer::WrappedMTLBuffer(MTL::Buffer *realMTLBuffer, ResourceId objId,
                                   WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLBuffer, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  objcBridge = AllocateObjCBridge(this);
}

WrappedMTLBuffer::WrappedMTLBuffer(WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  objcBridge = NULL;
}

template <typename SerialiserType>
bool WrappedMTLBuffer::Serialise_contents(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void *WrappedMTLBuffer::contents()
{
  void *data;
  SERIALISE_TIME_CALL(data = Unwrap(this)->contents());

  if(IsCaptureMode(m_State))
  {
    //    Chunk *chunk = NULL;
    //    {
    //      CACHE_THREAD_SERIALISER();
    //      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_contents);
    //      Serialise_contents(ser, this);
    //      chunk = scope.Get();
    //    }
    //    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
  return data;
}

NS::UInteger WrappedMTLBuffer::length()
{
  NS::UInteger length;
  SERIALISE_TIME_CALL(length = Unwrap(this)->length());

  return length;
}

void WrappedMTLBuffer::didModifyRange(NS::Range &range)
{
  SERIALISE_TIME_CALL(Unwrap(this)->didModifyRange(range));
  if(IsCaptureMode(m_State))
  {
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBuffer, void *, contents);
