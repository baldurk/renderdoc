/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "d3d8_resources.h"

#undef D3D8_TYPE_MACRO
#define D3D8_TYPE_MACRO(iface) WRAPPED_POOL_INST(CONCAT(Wrapped, iface));

ALL_D3D8_TYPES;

std::map<ResourceId, WrappedIDirect3DVertexBuffer8::BufferEntry>
    WrappedD3DBuffer8<IDirect3DVertexBuffer8, D3DVERTEXBUFFER_DESC>::m_BufferList;
std::map<ResourceId, WrappedIDirect3DIndexBuffer8::BufferEntry>
    WrappedD3DBuffer8<IDirect3DIndexBuffer8, D3DINDEXBUFFER_DESC>::m_BufferList;

D3D8ResourceType IdentifyTypeByPtr(IUnknown *ptr)
{
  if(ptr == NULL)
    return Resource_Unknown;

#undef D3D8_TYPE_MACRO
#define D3D8_TYPE_MACRO(iface)          \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return UnwrapHelper<iface>::GetTypeEnum();

  ALL_D3D8_TYPES;

  RDCERR("Unknown type for ptr 0x%p", ptr);

  return Resource_Unknown;
}

TrackedResource8 *GetTracked(IUnknown *ptr)
{
  if(ptr == NULL)
    return NULL;

#undef D3D8_TYPE_MACRO
#define D3D8_TYPE_MACRO(iface)          \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return (TrackedResource8 *)GetWrapped((iface *)ptr);

  ALL_D3D8_TYPES;

  return NULL;
}

template <>
IUnknown *Unwrap(IUnknown *ptr)
{
  if(ptr == NULL)
    return NULL;

#undef D3D8_TYPE_MACRO
#define D3D8_TYPE_MACRO(iface)          \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return (IUnknown *)GetWrapped((iface *)ptr)->GetReal();

  ALL_D3D8_TYPES;

  RDCERR("Unknown type of ptr 0x%p", ptr);

  return NULL;
}

template <>
ResourceId GetResID(IUnknown *ptr)
{
  if(ptr == NULL)
    return ResourceId();

  TrackedResource8 *res = GetTracked(ptr);

  if(res == NULL)
  {
    RDCERR("Unknown type of ptr 0x%p", ptr);

    return ResourceId();
  }

  return res->GetResourceID();
}

template <>
D3D8ResourceRecord *GetRecord(IUnknown *ptr)
{
  if(ptr == NULL)
    return NULL;

  TrackedResource8 *res = GetTracked(ptr);

  if(res == NULL)
  {
    RDCERR("Unknown type of ptr 0x%p", ptr);

    return NULL;
  }

  return res->GetResourceRecord();
}