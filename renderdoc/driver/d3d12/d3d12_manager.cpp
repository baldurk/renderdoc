/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "d3d12_manager.h"
#include "d3d12_device.h"

bool D3D12ResourceManager::SerialisableResource(ResourceId id, D3D12ResourceRecord *record)
{
  if(id == m_Device->GetResourceID())
    return true;

  if(record->ignoreSerialise)
    return false;

  return true;
}

ResourceId D3D12ResourceManager::GetID(ID3D12DeviceChild *res)
{
  return GetID(res);
}

bool D3D12ResourceManager::ResourceTypeRelease(ID3D12DeviceChild *res)
{
  if(res)
    res->Release();

  return true;
}

bool D3D12ResourceManager::Force_InitialState(ID3D12DeviceChild *res)
{
  return false;
}

bool D3D12ResourceManager::Need_InitialStateChunk(ID3D12DeviceChild *res)
{
  return true;
}

bool D3D12ResourceManager::Prepare_InitialState(ID3D12DeviceChild *res)
{
  RDCUNIMPLEMENTED("init states");
  return false;
}

bool D3D12ResourceManager::Serialise_InitialState(ResourceId id, ID3D12DeviceChild *res)
{
  RDCUNIMPLEMENTED("init states");
  return false;
}

void D3D12ResourceManager::Create_InitialState(ResourceId id, ID3D12DeviceChild *live, bool hasData)
{
  RDCUNIMPLEMENTED("init states");
}

void D3D12ResourceManager::Apply_InitialState(ID3D12DeviceChild *live, InitialContentData data)
{
  RDCUNIMPLEMENTED("init states");
}
