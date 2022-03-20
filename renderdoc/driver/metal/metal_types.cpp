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

#include "metal_types.h"
#include "metal_command_buffer.h"
#include "metal_command_queue.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_manager.h"
#include "metal_resources.h"

RDCCOMPILE_ASSERT(sizeof(NS::Integer) == sizeof(std::intptr_t), "NS::Integer size does not match");
RDCCOMPILE_ASSERT(sizeof(NS::UInteger) == sizeof(std::uintptr_t),
                  "NS::UInteger size does not match");

// serialisation of object handles via IDs.
template <class SerialiserType, class type>
void DoSerialiseViaResourceId(SerialiserType &ser, type &el)
{
  MetalResourceManager *rm = (MetalResourceManager *)ser.GetUserData();

  ResourceId id;

  if(ser.IsWriting() && rm)
    id = GetResID(el);
  if(ser.IsStructurising() && rm)
    id = rm->GetOriginalID(GetResID(el));

  DoSerialise(ser, id);

  if(ser.IsReading() && rm && !IsStructuredExporting(rm->GetState()))
  {
    el = NULL;

    if(id != ResourceId() && rm)
    {
      if(rm->HasLiveResource(id))
      {
        // we leave this wrapped.
        el = (type)rm->GetLiveResource(id);
      }
    }
  }
}

#define IMPLEMENT_WRAPPED_TYPE_SERIALISE(CPPTYPE)                 \
  template <class SerialiserType>                                 \
  void DoSerialise(SerialiserType &ser, WrappedMTL##CPPTYPE *&el) \
  {                                                               \
    DoSerialiseViaResourceId(ser, el);                            \
  }                                                               \
  INSTANTIATE_SERIALISE_TYPE(WrappedMTL##CPPTYPE *);

METALCPP_WRAPPED_PROTOCOLS(IMPLEMENT_WRAPPED_TYPE_SERIALISE);
#undef IMPLEMENT_WRAPPED_TYPE_SERIALISE

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, NS::String *&el)
{
  rdcstr rdcStr;
  if(el)
  {
    rdcStr = el->utf8String();
  }
  DoSerialise(ser, rdcStr);

  if(ser.IsReading())
  {
    el = NS::String::string(rdcStr.data(), NS::UTF8StringEncoding);
  }
}

INSTANTIATE_SERIALISE_TYPE(NS::String *);
