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

#include "metal_resources.h"
#include "metal_blit_command_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_command_queue.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_render_command_encoder.h"
#include "metal_render_pipeline_state.h"
#include "metal_texture.h"

ResourceId GetResID(WrappedMTLObject *obj)
{
  if(obj == NULL)
    return ResourceId();

  return obj->m_ID;
}

#define IMPLEMENT_WRAPPED_TYPE_HELPERS(CPPTYPE)  \
  MTL::CPPTYPE *Unwrap(WrappedMTL##CPPTYPE *obj) \
  {                                              \
    return Unwrap<MTL::CPPTYPE *>(obj);          \
  }
METALCPP_WRAPPED_PROTOCOLS(IMPLEMENT_WRAPPED_TYPE_HELPERS)
#undef IMPLEMENT_WRAPPED_TYPE_HELPERS

MetalResourceManager *WrappedMTLObject::GetResourceManager()
{
  return m_Device->GetResourceManager();
}

MetalResourceRecord::~MetalResourceRecord()
{
  if(m_Type == eResCommandBuffer)
    SAFE_DELETE(cmdInfo);
  else if(m_Type == eResBuffer)
    SAFE_DELETE(bufInfo);
}

void WrappedMTLObject::AddEvent()
{
  m_Device->AddEvent();
}

void WrappedMTLObject::AddAction(const ActionDescription &a)
{
  m_Device->AddAction(a);
}
