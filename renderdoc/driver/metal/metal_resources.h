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
#include "metal_types.h"

struct MetalResourceRecord;
class WrappedMTLDevice;
class MetalResourceManager;

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
      : objcBridge(NULL),
        real(NULL),
        record(NULL),
        m_WrappedMTLDevice(wrappedMTLDevice),
        m_State(captureState)
  {
  }
  WrappedMTLObject(void *mtlObject, ResourceId objId, WrappedMTLDevice *wrappedMTLDevice,
                   CaptureState &captureState)
      : objcBridge(NULL),
        real(mtlObject),
        id(objId),
        record(NULL),
        m_WrappedMTLDevice(wrappedMTLDevice),
        m_State(captureState)
  {
  }
  ~WrappedMTLObject() = default;

  void Dealloc();

  MTL::Device *GetObjCBridgeMTLDevice();

  MetalResourceManager *GetResourceManager();

  void *objcBridge;
  void *real;
  ResourceId id;
  MetalResourceRecord *record;
  WrappedMTLDevice *m_WrappedMTLDevice;
  CaptureState &m_State;
};

ResourceId GetResID(WrappedMTLObject *obj);

template <typename WrappedType>
MetalResourceRecord *GetRecord(WrappedType *obj)
{
  if(obj == NULL)
    return NULL;

  return obj->record;
}

template <typename RealType>
RealType Unwrap(WrappedMTLObject *obj)
{
  if(obj == NULL)
    return RealType();

  return (RealType)obj->real;
}

template <typename RealType>
RealType GetObjCBridge(WrappedMTLObject *obj)
{
  if(obj == NULL)
    return RealType();

  return (RealType)obj->objcBridge;
}

// template magic voodoo to unwrap types
template <typename inner>
struct UnwrapHelper
{
};

#define UNWRAP_HELPER(CPPTYPE)                 \
  template <>                                  \
  struct UnwrapHelper<MTL::CPPTYPE *>          \
  {                                            \
    typedef CONCAT(WrappedMTL, CPPTYPE) Outer; \
  };

METALCPP_WRAPPED_PROTOCOLS(UNWRAP_HELPER)
#undef UNWRAP_HELPER

#define IMPLEMENT_WRAPPED_TYPE_UNWRAP(CPPTYPE)              \
  inline MTL::CPPTYPE *Unwrap(WrappedMTL##CPPTYPE *obj)     \
  {                                                         \
    return Unwrap<MTL::CPPTYPE *>((WrappedMTLObject *)obj); \
  }

METALCPP_WRAPPED_PROTOCOLS(IMPLEMENT_WRAPPED_TYPE_UNWRAP)
#undef IMPLEMENT_WRAPPED_TYPE_UNWRAP

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
