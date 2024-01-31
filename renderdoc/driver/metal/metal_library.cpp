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

#include "metal_library.h"
#include "metal_device.h"
#include "metal_function.h"

WrappedMTLLibrary::WrappedMTLLibrary(MTL::Library *realMTLLibrary, ResourceId objId,
                                     WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLLibrary, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  if(realMTLLibrary && objId != ResourceId())
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLLibrary::Serialise_newFunctionWithName(SerialiserType &ser,
                                                      WrappedMTLFunction *function,
                                                      NS::String *FunctionName)
{
  SERIALISE_ELEMENT_LOCAL(Library, this);
  SERIALISE_ELEMENT_LOCAL(Function, GetResID(function)).TypedAs("MTLFunction"_lit);
  SERIALISE_ELEMENT(FunctionName).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    MTL::Function *realMTLFunction = Unwrap(Library)->newFunction(FunctionName);
    WrappedMTLFunction *wrappedMTLFunction;
    GetResourceManager()->WrapResource(realMTLFunction, wrappedMTLFunction);
    GetResourceManager()->AddLiveResource(Function, wrappedMTLFunction);
    m_Device->AddResource(Function, ResourceType::Shader, "Function");
    m_Device->DerivedResource(Library, Function);
  }
  return true;
}

WrappedMTLFunction *WrappedMTLLibrary::newFunctionWithName(NS::String *functionName)
{
  MTL::Function *realMTLFunction;
  SERIALISE_TIME_CALL(realMTLFunction = Unwrap(this)->newFunction(functionName));

  WrappedMTLFunction *wrappedMTLFunction;
  ResourceId id = GetResourceManager()->WrapResource(realMTLFunction, wrappedMTLFunction);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLLibrary_newFunctionWithName);
      Serialise_newFunctionWithName(ser, wrappedMTLFunction, functionName);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLFunction);
    record->AddChunk(chunk);
    record->AddParent(GetRecord(this));
  }
  else
  {
    // TODO: implement RD MTL replay
  }
  return wrappedMTLFunction;
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLLibrary, WrappedMTLFunction *function,
                                            newFunctionWithName, NS::String *functionName);
