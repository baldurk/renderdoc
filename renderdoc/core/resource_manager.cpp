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
std::string DoStringise(const FrameRefType &el)
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
  RDCASSERT(eFrameRef_Minimum <= first && first <= eFrameRef_Maximum);
  RDCASSERT(eFrameRef_Minimum <= second && second <= eFrameRef_Maximum);

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
      switch(second)
      {
        case eFrameRef_None:
        case eFrameRef_Read:
          // Only referenced as `Read` (and possibly `None`)
          return eFrameRef_Read;

        case eFrameRef_PartialWrite:
        case eFrameRef_CompleteWrite:
        case eFrameRef_ReadBeforeWrite:
          // First read, and then written
          return eFrameRef_ReadBeforeWrite;

        default: RDCERR("Unknown FrameRefType: %d", second); return eFrameRef_Maximum;
      }

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
  RDCASSERT(eFrameRef_Minimum <= first && first <= eFrameRef_Maximum);
  RDCASSERT(eFrameRef_Minimum <= second && second <= eFrameRef_Maximum);

  // The order of the reference types is irrelevant, so put them in a
  // consistent order (`first >= second`) to reduce the number of cases to
  // consider.
  if(first < second)
    std::swap(first, second);

  if(first == eFrameRef_Read &&
     (second == eFrameRef_PartialWrite || second == eFrameRef_CompleteWrite))
    // The resource is referenced both read and write/clear;
    // We don't know whether the read or write/clear occurs first;
    // if the write happens first, the final state would be Read or Clear;
    // if the read happens first, the final state would be ReadBeforeWrite.
    // We conservatively return ReadBeforeWrite, because this will force the
    // resource to be reset before each frame when replaying.
    return eFrameRef_ReadBeforeWrite;

  // In all other cases, we just return the more conservative reference type--
  // i.e. the reference type with the strongest (re)initialization
  // requirements for replay. Because larger values in the `FrameRefType` have
  // stronger (re)initialization requirements, this is simply the maximum
  // reference type; note that `first >= second` by the earlier swap.
  return first;
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
    {
      mgr->MarkCleanResource(ResID);
      mgr->RemoveResourceRecord(ResID);
    }

    mgr->DestroyResourceRecord(this);
  }
}
