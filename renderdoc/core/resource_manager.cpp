/******************************************************************************
 * The MIT License (MIT)
 * 
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
		return ResourceId(Atomic::Inc64(&globalIDCounter), true);
	}

	void SetReplayResourceIDs()
	{
		globalIDCounter = RDCMAX(uint64_t(globalIDCounter), uint64_t(globalIDCounter|0x1000000000000000ULL));
	}
};

void ResourceRecord::MarkResourceFrameReferenced(ResourceId id, FrameRefType refType)
{
	ResourceManager<void*,ResourceRecord>::MarkReferenced(m_FrameRefs, id, refType);
}

void ResourceRecord::AddResourceReferences(ResourceRecordHandler *mgr)
{
	for(auto it=m_FrameRefs.begin(); it != m_FrameRefs.end(); ++it)
	{
		mgr->MarkResourceFrameReferenced(it->first, it->second);
	}
}

void ResourceRecord::Delete(ResourceRecordHandler *mgr)
{
	RefCount--;
	RDCASSERT(RefCount >= 0);
	if(RefCount <= 0)
	{
		for(auto it = Parents.begin(); it != Parents.end(); ++it)
			(*it)->Delete(mgr);

		Parents.clear();
		Length = -1;
		DataPtr = NULL;

		for(auto it=m_FrameRefs.begin(); it != m_FrameRefs.end(); ++it)
		{
			if(it->second == eFrameRef_Write ||
				it->second == eFrameRef_ReadAndWrite ||
				it->second == eFrameRef_ReadBeforeWrite)
			{
				// lost a write to this resource, must mark it as gpu dirty.
				mgr->MarkPendingDirty(it->first);
			}
		}

		DeleteChunks();

		for(int i=0; i < NumSubResources; i++)
		{
			for(auto it=SubResources[i]->m_Chunks.begin(); it != SubResources[i]->m_Chunks.end(); ++it)
				SAFE_DELETE(it->second);

			SAFE_DELETE(SubResources[i]);
		}

		SAFE_DELETE_ARRAY(SubResources);

		if(ResID != ResourceId())
			mgr->RemoveResourceRecord(ResID);

		delete this;
	}
}


