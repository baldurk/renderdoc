/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "resource_manager.h"

#include <algorithm>

namespace ResourceIDGen
{
static volatile int64_t globalIDCounter = 1;

ResourceId GetNewUniqueID()
{
  ResourceId ret;
  ret.id = Atomic::Inc64(&globalIDCounter);
  return ret;
}

void SetReplayResourceIDs()
{
  // separate replay IDs from live IDs by adding a value when replaying.
  // 1000000000000000000 live IDs before we overlap replay IDs gives
  // almost 32 years generating 100000 IDs per frame at 10000 FPS.

  // only add this value once (since we're not |'ing on a bit)
  if(globalIDCounter < 1000000000000000000LL)
    globalIDCounter =
        RDCMAX(int64_t(globalIDCounter), int64_t(globalIDCounter + 1000000000000000000LL));
}
};

INSTANTIATE_SERIALISE_TYPE(ResourceManagerInternal::WrittenRecord);

template <>
rdcstr DoStringise(const FrameRefType &el)
{
  BEGIN_ENUM_STRINGISE(FrameRefType)
  {
    STRINGISE_ENUM_CLASS_NAMED(eFrameRef_None, "None");
    STRINGISE_ENUM_CLASS_NAMED(eFrameRef_PartialWrite, "Partial Write");
    STRINGISE_ENUM_CLASS_NAMED(eFrameRef_CompleteWrite, "Complete Write");
    STRINGISE_ENUM_CLASS_NAMED(eFrameRef_Read, "Read");
    STRINGISE_ENUM_CLASS_NAMED(eFrameRef_ReadBeforeWrite, "Read Before Write");
  }
  END_ENUM_STRINGISE()
}

FrameRefType ComposeFrameRefs(FrameRefType first, FrameRefType second)
{
  switch(first)
  {
    case eFrameRef_None:
    case eFrameRef_PartialWrite:
      if(second == eFrameRef_None)
        // A `None` reference after any other reference type does not change
        // the first reference type
        return first;
      else
        // A `None` or `Write` reference before any non-`None` reference type
        // does not change the reference type.
        return second;

    case eFrameRef_Read:
    case eFrameRef_WriteBeforeRead:
      if(IncludesWrite(second))
        // `first` reads before `second` writes
        return eFrameRef_ReadBeforeWrite;
      else
        return first;

    case eFrameRef_CompleteWrite:
    case eFrameRef_ReadBeforeWrite:
      // These reference types are both locked in, and cannot be affected by
      // later references.
      return first;

    default: RDCERR("Unknown FrameRefType: %d", first); return eFrameRef_Maximum;
  }
}

FrameRefType ComposeFrameRefsUnordered(FrameRefType first, FrameRefType second)
{
  if((IncludesRead(first) && IncludesWrite(second)) || (IncludesRead(second) && IncludesWrite(first)))
    // There is a way to order these references so that the resource is read
    // and then written. Since read-before-write is the worst case in terms of
    // reset requirements, we conservatively assume this is the case.
    return eFrameRef_ReadBeforeWrite;
  else
    // Otherwise either:
    // - first and second are each Read or None, or
    // - first and second are each CompleteWrite, PartialWrite or None.
    // In either case, Compose(first,second) = Compose(second,first) = max(first,second).
    return RDCMAX(first, second);
}

FrameRefType ComposeFrameRefsDisjoint(FrameRefType x, FrameRefType y)
{
  if(x == eFrameRef_ReadBeforeWrite || y == eFrameRef_ReadBeforeWrite)
    // If any subresource is `ReadBeforeWrite`, then the whole resource is.
    return eFrameRef_ReadBeforeWrite;
  else
    // For all other cases, just return the larger value.
    return RDCMAX(x, y);
}

bool IncludesRead(FrameRefType refType)
{
  switch(refType)
  {
    case eFrameRef_Read:
    case eFrameRef_WriteBeforeRead:
    case eFrameRef_ReadBeforeWrite: return true;
    default: return false;
  }
}

bool IncludesWrite(FrameRefType refType)
{
  switch(refType)
  {
    case eFrameRef_PartialWrite:
    case eFrameRef_CompleteWrite:
    case eFrameRef_WriteBeforeRead:
    case eFrameRef_ReadBeforeWrite: return true;
    default: return false;
  }
}

bool IsDirtyFrameRef(FrameRefType refType)
{
  return (refType != eFrameRef_None && refType != eFrameRef_Read);
}

void ResourceRecord::AddResourceReferences(ResourceRecordHandler *mgr)
{
  for(auto it = m_FrameRefs.begin(); it != m_FrameRefs.end(); ++it)
  {
    mgr->MarkResourceFrameReferenced(it->first, it->second);
  }
}

void ResourceRecord::Delete(ResourceRecordHandler *mgr)
{
  int32_t ref = Atomic::Dec32(&RefCount);
  RDCASSERT(ref >= 0);
  if(ref <= 0)
  {
    for(auto it = Parents.begin(); it != Parents.end(); ++it)
      (*it)->Delete(mgr);

    Parents.clear();
    Length = 0;
    DataPtr = NULL;

    DeleteChunks();

    if(ResID != ResourceId())
      mgr->RemoveResourceRecord(ResID);

    mgr->DestroyResourceRecord(this);
  }
}
