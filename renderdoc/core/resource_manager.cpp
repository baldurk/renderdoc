/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
static int64_t globalIDCounter = 1;

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
    STRINGISE_ENUM_CLASS_NAMED(eFrameRef_CompleteWriteAndDiscard, "Complete Write and Discard");
    STRINGISE_ENUM_CLASS_NAMED(eFrameRef_Unknown, "Unknown");
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
      else if(first == eFrameRef_PartialWrite && second == eFrameRef_Read)
        // a `Read` reference after a partial write means we need to store `WriteBeforeRead` instead
        // of just `Read`.
        return eFrameRef_WriteBeforeRead;
      else
        // Otherwise a `None` or `Write` reference before any non-`None` reference type
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
    case eFrameRef_CompleteWriteAndDiscard:
    case eFrameRef_ReadBeforeWrite:
      // These reference types are both locked in, and cannot be affected by
      // later references.
      return first;

    default: RDCERR("Unknown FrameRefType: %d", first); return eFrameRef_ReadBeforeWrite;
  }
}

FrameRefType ComposeFrameRefsUnordered(FrameRefType first, FrameRefType second)
{
  if((IncludesRead(first) && IncludesWrite(second)) || (IncludesRead(second) && IncludesWrite(first)))
  {
    // There is a way to order these references so that the resource is read
    // and then written. Since read-before-write is the worst case in terms of
    // reset requirements, we conservatively assume this is the case.
    return eFrameRef_ReadBeforeWrite;
  }
  else
  {
    // first patch CompleteWriteAndDiscard to CompleteWrite so the values are as expected.
    if(first == eFrameRef_CompleteWriteAndDiscard)
      first = eFrameRef_CompleteWrite;
    if(second == eFrameRef_CompleteWriteAndDiscard)
      second = eFrameRef_CompleteWrite;

    // Otherwise either:
    // - first and second are each Read or None, or
    // - first and second are each CompleteWrite, PartialWrite or None.
    // In either case, Compose(first,second) = Compose(second,first) = max(first,second).
    return RDCMAX(first, second);
  }
}

FrameRefType ComposeFrameRefsDisjoint(FrameRefType x, FrameRefType y)
{
  if(x == eFrameRef_ReadBeforeWrite || y == eFrameRef_ReadBeforeWrite)
  {
    // If any subresource is `ReadBeforeWrite`, then the whole resource is.
    return eFrameRef_ReadBeforeWrite;
  }
  else
  {
    // first patch CompleteWriteAndDiscard to CompleteWrite so the values are as expected.
    if(x == eFrameRef_CompleteWriteAndDiscard)
      x = eFrameRef_CompleteWrite;
    if(y == eFrameRef_CompleteWriteAndDiscard)
      y = eFrameRef_CompleteWrite;

    // For all other cases, just return the larger value.
    return RDCMAX(x, y);
  }
}

FrameRefType ComposeFrameRefsFirstKnown(FrameRefType first, FrameRefType second)
{
  if(eFrameRef_Minimum <= first && first <= eFrameRef_Maximum)
    return first;
  else
    return second;
}

FrameRefType KeepOldFrameRef(FrameRefType first, FrameRefType second)
{
  return first;
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
    case eFrameRef_CompleteWriteAndDiscard:
    case eFrameRef_WriteBeforeRead:
    case eFrameRef_ReadBeforeWrite: return true;
    default: return false;
  }
}

bool IsDirtyFrameRef(FrameRefType refType)
{
  return (refType != eFrameRef_None && refType != eFrameRef_Read);
}

bool IsCompleteWriteFrameRef(FrameRefType refType)
{
  return refType == eFrameRef_CompleteWrite || refType == eFrameRef_CompleteWriteAndDiscard;
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
