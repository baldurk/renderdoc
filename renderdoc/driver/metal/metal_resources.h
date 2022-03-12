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

#include "core/resource_manager.h"
#include "metal_common.h"

struct MetalResourceRecord;
class WrappedMTLDevice;

enum MetalResourceType
{
  eResUnknown = 0,
  eResDevice,
  eResLibrary,
  eResFunction,
};

DECLARE_REFLECTION_ENUM(MetalResourceType);

struct WrappedMTLObject
{
  WrappedMTLObject() = delete;
  WrappedMTLObject(WrappedMTLDevice *wrappedMTLDevice, CaptureState &captureState)
      : wrappedObjC(NULL),
        real(NULL),
        record(NULL),
        m_WrappedMTLDevice(wrappedMTLDevice),
        m_State(captureState)
  {
  }
  WrappedMTLObject(void *mtlObject, ResourceId objId, WrappedMTLDevice *wrappedMTLDevice,
                   CaptureState &captureState)
      : wrappedObjC(NULL),
        real(mtlObject),
        id(objId),
        record(NULL),
        m_WrappedMTLDevice(wrappedMTLDevice),
        m_State(captureState)
  {
  }
  ~WrappedMTLObject() = default;

  void Dealloc();

  MTL::Device *GetObjCWrappedMTLDevice();

  void *wrappedObjC;
  void *real;
  ResourceId id;
  MetalResourceRecord *record;
  WrappedMTLDevice *m_WrappedMTLDevice;
  CaptureState &m_State;
};

ResourceId GetResID(WrappedMTLObject *obj);

template <typename RealType>
RealType Unwrap(WrappedMTLObject *obj)
{
  if(obj == NULL)
    return RealType();

  return (RealType)obj->real;
}

template <typename RealType>
RealType UnwrapObjC(WrappedMTLObject *obj)
{
  if(obj == NULL)
    return RealType();

  return (RealType)obj->wrappedObjC;
}

struct MetalResourceRecord : public ResourceRecord
{
public:
  enum
  {
    NullResource = NULL
  };

  MetalResourceRecord(ResourceId id)
      : ResourceRecord(id, true), Resource(NULL), resType(eResUnknown), ptrUnion(NULL)
  {
  }
  ~MetalResourceRecord();
  WrappedMTLObject *Resource;
  MetalResourceType resType;

  // Each entry is only used by specific record types
  union
  {
    void *ptrUnion;    // for initialisation to NULL
  };
};
