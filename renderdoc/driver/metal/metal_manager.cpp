/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include "metal_manager.h"
#include "metal_device.h"

bool MetalResourceManager::ResourceTypeRelease(WrappedResourceType res)
{
  METAL_NOT_IMPLEMENTED();
  return false;
}

bool MetalResourceManager::Prepare_InitialState(WrappedMTLObject *res)
{
  return m_Device->Prepare_InitialState(res);
}

uint64_t MetalResourceManager::GetSize_InitialState(ResourceId id, const MetalInitialContents &initial)
{
  return m_Device->GetSize_InitialState(id, initial);
}

bool MetalResourceManager::Serialise_InitialState(WriteSerialiser &ser, ResourceId id,
                                                  MetalResourceRecord *record,
                                                  const MetalInitialContents *initial)
{
  return m_Device->Serialise_InitialState(ser, id, record, initial);
}

void MetalResourceManager::Create_InitialState(ResourceId id, WrappedMTLObject *live, bool hasData)
{
  return m_Device->Create_InitialState(id, live, hasData);
}

void MetalResourceManager::Apply_InitialState(WrappedMTLObject *live,
                                              const MetalInitialContents &initial)
{
  return m_Device->Apply_InitialState(live, initial);
}
