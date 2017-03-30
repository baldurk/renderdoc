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

#include "resource_manager.h"

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

bool ResourceRecord::MarkResourceFrameReferenced(ResourceId id, FrameRefType refType)
{
  if(id == ResourceId())
    return false;
  return ResourceManager<void *, void *, ResourceRecord>::MarkReferenced(m_FrameRefs, id, refType);
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

    for(auto it = m_FrameRefs.begin(); it != m_FrameRefs.end(); ++it)
    {
      if(it->second == eFrameRef_Write || it->second == eFrameRef_ReadAndWrite ||
         it->second == eFrameRef_ReadBeforeWrite)
      {
        // lost a write to this resource, must mark it as gpu dirty.
        mgr->MarkPendingDirty(it->first);
      }
    }

    DeleteChunks();

    if(ResID != ResourceId())
    {
      mgr->MarkCleanResource(ResID);
      mgr->RemoveResourceRecord(ResID);
    }

    mgr->DestroyResourceRecord(this);
  }
}
