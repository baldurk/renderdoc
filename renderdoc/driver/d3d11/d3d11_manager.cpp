/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "driver/d3d11/d3d11_manager.h"
#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_resources.h"

byte D3D11ResourceRecord::markerValue[32] = {
    0xaa, 0xbb, 0xcc, 0xdd, 0x88, 0x77, 0x66, 0x55, 0x01, 0x23, 0x45, 0x67, 0x98, 0x76, 0x54, 0x32,
};

ID3D11DeviceChild *D3D11ResourceManager::UnwrapResource(ID3D11DeviceChild *res)
{
  if(res == NULL)
    return res;

  if(WrappedID3D11Buffer::IsAlloc(res))
    return UNWRAP(WrappedID3D11Buffer, res);
  else if(WrappedID3D11Texture1D::IsAlloc(res))
    return UNWRAP(WrappedID3D11Texture1D, res);
  else if(WrappedID3D11Texture2D1::IsAlloc(res))
    return UNWRAP(WrappedID3D11Texture2D1, res);
  else if(WrappedID3D11Texture3D1::IsAlloc(res))
    return UNWRAP(WrappedID3D11Texture3D1, res);

  RDCERR("UnwrapResource(): Unexpected non-wrapped resource");
  return res;
}

bool D3D11ResourceManager::SerialisableResource(ResourceId id, D3D11ResourceRecord *record)
{
  if(id == m_Device->GetImmediateContext()->GetResourceID())
    return false;

  if(id == m_Device->GetResourceID())
    return true;

  if(record->ignoreSerialise)
    return false;

  bool skip = false;
  for(size_t i = 0; i < m_Device->GetNumDeferredContexts(); i++)
  {
    if(id == m_Device->GetDeferredContext(i)->GetResourceID())
    {
      skip = true;
      break;
    }
  }

  if(skip)
    return false;

  return true;
}

ResourceId D3D11ResourceManager::GetID(ID3D11DeviceChild *res)
{
  return GetIDForResource(res);
}

bool D3D11ResourceManager::ResourceTypeRelease(ID3D11DeviceChild *res)
{
  if(res)
    res->Release();

  return true;
}

bool D3D11ResourceManager::Force_InitialState(ID3D11DeviceChild *res, bool prepare)
{
  return IdentifyTypeByPtr(res) == Resource_UnorderedAccessView;
}

bool D3D11ResourceManager::Need_InitialStateChunk(ID3D11DeviceChild *res)
{
  return IdentifyTypeByPtr(res) != Resource_Buffer;
}

bool D3D11ResourceManager::Prepare_InitialState(ID3D11DeviceChild *res)
{
  return m_Device->Prepare_InitialState(res);
}

bool D3D11ResourceManager::Serialise_InitialState(ResourceId id, ID3D11DeviceChild *res)
{
  return m_Device->Serialise_InitialState(id, res);
}

void D3D11ResourceManager::Create_InitialState(ResourceId id, ID3D11DeviceChild *live, bool hasData)
{
  m_Device->Create_InitialState(id, live, hasData);
}

void D3D11ResourceManager::Apply_InitialState(ID3D11DeviceChild *live, InitialContentData data)
{
  m_Device->Apply_InitialState(live, data);
}
