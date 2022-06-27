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

#pragma once

#include "metal_command_queue.h"
#include "metal_common.h"
#include "metal_device.h"
#include "metal_resources.h"

class WrappedMTLCommandBuffer : public WrappedMTLObject
{
public:
  WrappedMTLCommandBuffer(MTL::CommandBuffer *realMTLCommandBuffer, ResourceId objId,
                          WrappedMTLDevice *wrappedMTLDevice);

  void SetCommandQueue(WrappedMTLCommandQueue *commandQueue) { m_CommandQueue = commandQueue; }
  MTL::CommandQueue *GetCommandQueue() { return (MTL::CommandQueue *)m_CommandQueue; }
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLBlitCommandEncoder *, blitCommandEncoder);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLRenderCommandEncoder *,
                                          renderCommandEncoderWithDescriptor,
                                          RDMTL::RenderPassDescriptor &descriptor);
  DECLARE_FUNCTION_SERIALISED(void, presentDrawable, MTL::Drawable *drawable);
  DECLARE_FUNCTION_SERIALISED(void, commit);

  enum
  {
    TypeEnum = eResCommandBuffer
  };

private:
  WrappedMTLCommandQueue *m_CommandQueue;
};
