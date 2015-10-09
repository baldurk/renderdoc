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


#pragma once

#include "core/core.h"

#include "api/replay/renderdoc_replay.h"

#include "os/os_specific.h"

#include "serialise/serialiser.h"
#include "common/threading.h"

#include <set>
#include <map>
using std::set;
using std::map;

// in what way (read, write, etc) was a resource referenced in a frame -
// used to determine if initial contents are needed and to what degree
enum FrameRefType
{
	eFrameRef_Unknown, // for the initial start of frame pipeline state - can't be marked as written/read yet until first action.

	// Inputs
	eFrameRef_Read,
	eFrameRef_Write,

	// States
	eFrameRef_ReadOnly,
	eFrameRef_ReadAndWrite,
	eFrameRef_ReadBeforeWrite,
};

namespace ResourceIDGen
{
	ResourceId GetNewUniqueID();
	void SetReplayResourceIDs();
};

class ResourceRecordHandler
{
	public:
		virtual void MarkDirtyResource(ResourceId id) = 0;
		virtual void MarkPendingDirty(ResourceId id) = 0;
		virtual void RemoveResourceRecord(ResourceId id) = 0;
		virtual void MarkResourceFrameReferenced(ResourceId id, FrameRefType refType) = 0;
};

// This is a generic resource record, that APIs can inherit from and use.
// A resource is an API object that gets tracked on its own, has dependencies on other resources
// and has its own stream of chunks.
//
// This is used to track the necessary resources for a frame, and include only those required
// for the captured frame in its log. It also handles anything resource-specific such as
// shadow CPU copies of data.
struct ResourceRecord
{
	ResourceRecord(ResourceId id, bool lock)
		: RefCount(1), ResID(id), UpdateCount(0),
			DataInSerialiser(false), DataPtr(NULL), DataOffset(0),
			Length(-1), DataWritten(false), SpecialResource(false),
			NumSubResources(0), SubResources(NULL)
	{
		m_ChunkLock = NULL;

		if(lock)
			m_ChunkLock = new Threading::CriticalSection();
	}

	~ResourceRecord()
	{
		SAFE_DELETE(m_ChunkLock);
	}

	void AddParent(ResourceRecord *r)
	{
		if(Parents.find(r) == Parents.end())
		{
			r->AddRef();
			Parents.insert(r);
		}
	}

	void MarkParentsDirty(ResourceRecordHandler *mgr)
	{
		for(auto it = Parents.begin(); it != Parents.end(); ++it)
			mgr->MarkDirtyResource((*it)->GetResourceID());
	}

	void MarkParentsReferenced(ResourceRecordHandler *mgr, FrameRefType refType)
	{
		for(auto it = Parents.begin(); it != Parents.end(); ++it)
			mgr->MarkResourceFrameReferenced((*it)->GetResourceID(), refType);
	}

	void FreeParents(ResourceRecordHandler *mgr)
	{
		for(auto it = Parents.begin(); it != Parents.end(); ++it)
			(*it)->Delete(mgr);

		Parents.clear();
	}

	void MarkDataUnwritten()
	{
		DataWritten = false;
	}

	void Insert(map<int32_t, Chunk *> &recordlist)
	{
		bool dataWritten = DataWritten;

		DataWritten = true;

		for(auto it = Parents.begin(); it != Parents.end(); ++it)
		{
			if(!(*it)->DataWritten)
			{
				(*it)->Insert(recordlist);
			}
		}

		if(!dataWritten)
		{
			recordlist.insert(m_Chunks.begin(), m_Chunks.end());
			
			for(int i=0; i < NumSubResources; i++)
				SubResources[i]->Insert(recordlist);
		}
	}

	void AddRef() { RefCount++; }	
	int GetRefCount() const { return RefCount; }
	void Delete(ResourceRecordHandler *mgr);

	ResourceId GetResourceID() const { return ResID; }

	void RemoveChunk(Chunk *chunk)
	{
		LockChunks();
		for(auto it=m_Chunks.begin(); it != m_Chunks.end(); ++it)
		{
			if(it->second == chunk)
			{
				m_Chunks.erase(it);
				break;
			}
		}
		UnlockChunks();
	}

	void AddChunk(Chunk *chunk, int32_t ID = 0)
	{
		LockChunks();
		if(ID == 0) ID = GetID();
		m_Chunks[ID] = chunk;
		UnlockChunks();
	}

	void LockChunks() { if(m_ChunkLock) m_ChunkLock->Lock(); }
	void UnlockChunks() { if(m_ChunkLock) m_ChunkLock->Unlock(); }

	bool HasChunks() const
	{
		return !m_Chunks.empty();
	}

	size_t NumChunks() const
	{
		return m_Chunks.size();
	}

	void SwapChunks(ResourceRecord *other)
	{
		LockChunks();
		other->LockChunks();
		m_Chunks.swap(other->m_Chunks);
		m_FrameRefs.swap(other->m_FrameRefs);
		other->UnlockChunks();
		UnlockChunks();
	}

	void DeleteChunks()
	{
		LockChunks();
		for(auto it=m_Chunks.begin(); it != m_Chunks.end(); ++it)
			SAFE_DELETE(it->second);
		m_Chunks.clear();
		UnlockChunks();
	}

	Chunk *GetLastChunk() const
	{
		RDCASSERT(HasChunks());
		return m_Chunks.rbegin()->second;
	}

	int32_t GetLastChunkID() const
	{
		RDCASSERT(HasChunks());
		return m_Chunks.rbegin()->first;
	}

	void PopChunk()
	{
		m_Chunks.erase(m_Chunks.rbegin()->first);
	}

	byte *GetDataPtr()
	{
		return DataPtr + DataOffset;
	}

	bool HasDataPtr()
	{
		return DataPtr != NULL;
	}

	void SetDataOffset(uint64_t offs)
	{
		DataOffset = offs;
	}

	void SetDataPtr(byte *ptr)
	{
		DataPtr = ptr;

		for(int i=0; i < NumSubResources; i++)
			SubResources[i]->SetDataPtr(ptr);
	}

	void MarkResourceFrameReferenced(ResourceId id, FrameRefType refType);
	void AddResourceReferences(ResourceRecordHandler *mgr);

	int UpdateCount;

	bool DataInSerialiser;
	int32_t Length;
	
	bool SpecialResource; // like the swap chain back buffers

	int NumSubResources;
	ResourceRecord **SubResources;
	
protected:
	byte *DataPtr;
	uint64_t DataOffset;
	bool DataWritten;
	
	int RefCount;

	ResourceId ResID;

	std::set<ResourceRecord*> Parents;

	int32_t GetID()
	{
		static volatile int32_t globalIDCounter = 10;

		return Atomic::Inc32(&globalIDCounter);
	}

	map<ResourceId, FrameRefType> m_FrameRefs;
	
	std::map<int32_t, Chunk *> m_Chunks;
	Threading::CriticalSection *m_ChunkLock;
};

// the resource manager is a utility class that's not required but is likely wanted by any API implementation.
// It keeps track of resource records, which resources are alive and allows you to query for them by ID. It tracks
// which resources are marked as dirty (needing their initial contents fetched before capture).
//
// For APIs that wrap their resources it provides tracking for that.
// 
// In the replay application it will also track which 'live' resources are representing which 'original'
// resources from the application when it was captured.
template<typename ResourceType, typename RecordType>
class ResourceManager : public ResourceRecordHandler
{
	public:
		ResourceManager(LogState state, Serialiser *ser);
		virtual ~ResourceManager();

		void Shutdown();

		struct InitialContentData
		{
			InitialContentData(ResourceType r, uint32_t n, byte *b) : resource(r), num(n), blob(b) {}
			InitialContentData() : resource((ResourceType)RecordType::NullResource), num(0), blob(NULL) {}
			ResourceType resource;
			uint32_t num;
			byte *blob;
		};

		///////////////////////////////////////////
		// Capture-side methods
		
		// while capturing, resource records containing chunk streams and metadata for resources
		RecordType *GetResourceRecord(ResourceId id);
		bool HasResourceRecord(ResourceId id);
		RecordType *AddResourceRecord(ResourceId id);
		inline void RemoveResourceRecord(ResourceId id);
		
		// while capturing or replaying, resources and their live IDs
		void AddCurrentResource(ResourceId id, ResourceType res);
		bool HasCurrentResource(ResourceId id);
		ResourceType GetCurrentResource(ResourceId id);
		void ReleaseCurrentResource(ResourceId id);

		void MarkInFrame(bool inFrame) { m_InFrame = inFrame; }
		void ReleaseInFrameResources();
		
		// insert the chunks for the resources referenced in the frame
		void InsertReferencedChunks(Serialiser *fileSer);

		// mark resource records as unwritten, ready to be written to a new logfile.
		void MarkUnwrittenResources();

		// clear the list of frame-referenced resources - e.g. if you're about to recapture a frame
		void ClearReferencedResources();
		
		// indicates this resource could have been modified by the GPU,
		// so it's now suspect and the data we have on it might well be out of date
		// and to be correct its contents should be serialised out at the start
		// of the frame.
		inline void MarkDirtyResource(ResourceId res);

		// for use when we might be mid-capture, this will get flushed to dirty state before the
		// next frame but is safe to use mid-capture
		void MarkPendingDirty(ResourceId res);
		void FlushPendingDirty();
		
		// this can be used when the resource is cleared or similar and it's in a known state
		void MarkCleanResource(ResourceId res);

		// returns if the resource has been marked as dirty
		bool IsResourceDirty(ResourceId res);
		
		// call callbacks to prepare initial contents for dirty resources
		void PrepareInitialContents();
		
		InitialContentData GetInitialContents(ResourceId id);
		void SetInitialContents(ResourceId id, InitialContentData contents);
		void SetInitialChunk(ResourceId id, Chunk *chunk);

		// generate chunks for initial contents and insert.
		void InsertInitialContentsChunks(Serialiser *fileSer);
		
		// Serialise out which resources need initial contents, along with whether their
		// initial contents are in the serialised stream (e.g. RTs might still want to be
		// cleared on frame init).
		void Serialise_InitialContentsNeeded();


		// handle marking a resource referenced for read or write and storing RAW access etc.
		static bool MarkReferenced(map<ResourceId, FrameRefType> &refs, ResourceId id, FrameRefType refType);
		
		// mark resource referenced somewhere in the main frame-affecting calls.
		// That means this resource should be included in the final serialise out
		inline void MarkResourceFrameReferenced(ResourceId id, FrameRefType refType);
		
		// check if this resource was read before being written to - can be used to detect if
		// initial states are necessary
		bool ReadBeforeWrite(ResourceId id);
		
		///////////////////////////////////////////
		// Replay-side methods
		
		// Live resources to replace serialised IDs
		void AddLiveResource(ResourceId origid, ResourceType livePtr);
		bool HasLiveResource(ResourceId origid);
		ResourceType GetLiveResource(ResourceId origid);
		void EraseLiveResource(ResourceId origid);
		
		// when asked for a given id, return the resource for a replacement id
		void ReplaceResource(ResourceId from, ResourceId to);
		void RemoveReplacement(ResourceId id);

		// fetch original ID for a real ID or vice-versa.
		ResourceId GetOriginalID(ResourceId id);
		ResourceId GetLiveID(ResourceId id);

		// Serialise in which resources need initial contents and set them up.
		void CreateInitialContents();

		// Apply the initial contents for the resources that need them, used at the start of a frame
		void ApplyInitialContents();

		// Resource wrapping, allows for querying and adding/removing of wrapper layers around resources
		bool AddWrapper(ResourceType wrap, ResourceType real);
		bool HasWrapper(ResourceType real);
		ResourceType GetWrapper(ResourceType real);
		void RemoveWrapper(ResourceType real);

	protected:
		// 'interface' to implement by derived classes
		virtual bool SerialisableResource(ResourceId id, RecordType *record) = 0;
		virtual ResourceId GetID(ResourceType res) = 0;
		
		virtual bool ResourceTypeRelease(ResourceType res) = 0;

		virtual bool Force_InitialState(ResourceType res) = 0;
		virtual bool Need_InitialStateChunk(ResourceType res) = 0;
		virtual bool Prepare_InitialState(ResourceType res) = 0;
		virtual bool Serialise_InitialState(ResourceType res) = 0;
		virtual void Create_InitialState(ResourceId id, ResourceType live, bool hasData) = 0;
		virtual void Apply_InitialState(ResourceType live, InitialContentData initial) = 0;

		LogState m_State;
		Serialiser *m_pSerialiser;

	private:
		bool m_InFrame;

		// very coarse lock, protects EVERYTHING. This could certainly be improved and it may be a bottleneck
		// for performance. Given that the main use cases are write-rarely read-often the lock should be optimised
		// for that as we only want to make sure we're not modifying the objects together, by far the most common
		// operation is looking up data.
		Threading::CriticalSection m_Lock;

		// easy optimisation win - don't use maps everywhere. It's convenient but not optimal, and profiling will
		// likely prove that some or all of these could be a problem
		
		// used during capture - map from real resource to its wrapper (other way can be done just with an Unwrap)
		map<ResourceType, ResourceType> m_WrapperMap;

		// used during capture - holds resources referenced in current frame (and how they're referenced)
		map<ResourceId, FrameRefType> m_FrameReferencedResources;

		// used during capture - holds resources marked as dirty, needing initial contents
		set<ResourceId> m_DirtyResources;
		set<ResourceId> m_PendingDirtyResources;

		// used during capture or replay - holds initial contents
		map<ResourceId, InitialContentData> m_InitialContents;
		// on capture, if a chunk was prepared in Prepare_InitialContents and added, don't re-serialise.
		// Some initial contents may not need the delayed readback.
		map<ResourceId, Chunk*> m_InitialChunks;

		// used during capture or replay - map of resources currently alive with their real IDs, used in capture and replay.
		map<ResourceId, ResourceType> m_CurrentResourceMap;

		// used during replay - maps back and forth from original id to live id and vice-versa
		map<ResourceId, ResourceId> m_OriginalIDs, m_LiveIDs;
		
		// used during replay - holds resources allocated and the original id that they represent
		// for a) in-frame creations and b) pre-frame creations respectively.
		map<ResourceId, ResourceType> m_InframeResourceMap, m_LiveResourceMap;

		// used during capture - holds resource records by id.
		map<ResourceId, RecordType*> m_ResourceRecords;

		// used during replay - holds current resource replacements
		map<ResourceId, ResourceId> m_Replacements;
};

template<typename ResourceType, typename RecordType>
ResourceManager<ResourceType, RecordType>::ResourceManager(LogState state, Serialiser *ser)
{
	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(ResourceManager));

	m_State = state;
	m_pSerialiser = ser;
	
	m_InFrame = false;
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::Shutdown()
{
	while(!m_LiveResourceMap.empty())
	{
		auto it = m_LiveResourceMap.begin();
		ResourceTypeRelease(it->second);
		if(!m_LiveResourceMap.empty())
			m_LiveResourceMap.erase(m_LiveResourceMap.begin());
	}
	
	while(!m_InframeResourceMap.empty())
	{
		auto it = m_InframeResourceMap.begin();
		ResourceTypeRelease(it->second);
		if(!m_InframeResourceMap.empty())
			m_InframeResourceMap.erase(m_InframeResourceMap.begin());
	}
	
	while(!m_InitialContents.empty())
	{
		auto it = m_InitialContents.begin();
		ResourceTypeRelease(it->second.resource);
		Serialiser::FreeAlignedBuffer(it->second.blob);
		if(!m_InitialContents.empty())
			m_InitialContents.erase(m_InitialContents.begin());
	}

	RDCASSERT(m_ResourceRecords.empty());
}

template<typename ResourceType, typename RecordType>
ResourceManager<ResourceType, RecordType>::~ResourceManager()
{
	RDCASSERT(m_LiveResourceMap.empty());
	RDCASSERT(m_InframeResourceMap.empty());
	RDCASSERT(m_InitialContents.empty());
	RDCASSERT(m_ResourceRecords.empty());

	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

template<typename ResourceType, typename RecordType>
bool ResourceManager<ResourceType, RecordType>::MarkReferenced(map<ResourceId, FrameRefType> &refs, ResourceId id, FrameRefType refType)
{
	if(refs.find(id) == refs.end())
	{
		if(refType == eFrameRef_Read)
			refs[id] = eFrameRef_ReadOnly;
		else if(refType == eFrameRef_Write)
			refs[id] = eFrameRef_ReadAndWrite;
		else // unknown or existing state
			refs[id] = refType;

		return true;
	}
	else
	{
		if(refType == eFrameRef_Unknown)
		{
			// nothing
		}
		else if(refType == eFrameRef_ReadBeforeWrite)
		{
			// special case, explicitly set to ReadBeforeWrite for when
			// we know that this use will likely be a partial-write
			refs[id] = eFrameRef_ReadBeforeWrite;
		}
		else if(refs[id] == eFrameRef_Unknown)
		{
			if(refType == eFrameRef_ReadBeforeWrite)
				refs[id] = eFrameRef_ReadBeforeWrite;
			else if(refType == eFrameRef_Read || refType == eFrameRef_ReadOnly)
				refs[id] = eFrameRef_ReadOnly;
			else
				refs[id] = eFrameRef_ReadAndWrite;
		}
		else if(refs[id] == eFrameRef_ReadOnly && refType == eFrameRef_Write)
		{
			refs[id] = eFrameRef_ReadBeforeWrite;
		}
	}

	return false;
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::MarkResourceFrameReferenced(ResourceId id, FrameRefType refType)
{
	SCOPED_LOCK(m_Lock);

	if(id == ResourceId())
		return;

	bool newRef = MarkReferenced(m_FrameReferencedResources, id, refType);

	if(newRef)
	{
		RecordType *record = GetResourceRecord(id);

		if(record)
			record->AddRef();
	}
}

template<typename ResourceType, typename RecordType>
bool ResourceManager<ResourceType, RecordType>::ReadBeforeWrite(ResourceId id)
{
	if(m_FrameReferencedResources.find(id) != m_FrameReferencedResources.end())
		return m_FrameReferencedResources[id] == eFrameRef_ReadBeforeWrite ||
				m_FrameReferencedResources[id] == eFrameRef_ReadOnly;

	return false;
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::MarkDirtyResource(ResourceId res)
{
	SCOPED_LOCK(m_Lock);

	if(res == ResourceId())
		return;

	m_DirtyResources.insert(res);
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::MarkPendingDirty(ResourceId res)
{
	SCOPED_LOCK(m_Lock);

	if(res == ResourceId())
		return;

	m_PendingDirtyResources.insert(res);
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::FlushPendingDirty()
{
	SCOPED_LOCK(m_Lock);

	m_DirtyResources.insert(m_PendingDirtyResources.begin(), m_PendingDirtyResources.end());
	m_PendingDirtyResources.clear();
}

template<typename ResourceType, typename RecordType>
bool ResourceManager<ResourceType, RecordType>::IsResourceDirty(ResourceId res)
{
	SCOPED_LOCK(m_Lock);

	if(res == ResourceId())
		return false;

	return m_DirtyResources.find(res) != m_DirtyResources.end();
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::MarkCleanResource(ResourceId res)
{
	SCOPED_LOCK(m_Lock);
	
	if(res == ResourceId())
		return;

	if(IsResourceDirty(res))
	{
		m_DirtyResources.erase(res);
	}
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::SetInitialContents(ResourceId id, InitialContentData contents)
{
	SCOPED_LOCK(m_Lock);

	RDCASSERT(id != ResourceId());

	auto it = m_InitialContents.find(id);
	
	if(it != m_InitialContents.end())
	{
		ResourceTypeRelease(it->second.resource);
		Serialiser::FreeAlignedBuffer(it->second.blob);
		m_InitialContents.erase(it);
	}
	
	m_InitialContents[id] = contents;
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::SetInitialChunk(ResourceId id, Chunk *chunk)
{
	SCOPED_LOCK(m_Lock);

	RDCASSERT(id != ResourceId());

	auto it = m_InitialChunks.find(id);

	RDCASSERT(chunk->GetChunkType() == INITIAL_CONTENTS);
	
	if(it != m_InitialChunks.end())
	{
		RDCERR("Initial chunk set for ID %llu twice", id);
		delete chunk;
		return;
	}
	
	m_InitialChunks[id] = chunk;
}

template<typename ResourceType, typename RecordType>
typename ResourceManager<ResourceType, RecordType>::InitialContentData ResourceManager<ResourceType, RecordType>::GetInitialContents(ResourceId id)
{
	SCOPED_LOCK(m_Lock);

	if(id == ResourceId())
		return InitialContentData();

	if(m_InitialContents.find(id) != m_InitialContents.end())
		return m_InitialContents[id];

	return InitialContentData();
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::Serialise_InitialContentsNeeded()
{
	SCOPED_LOCK(m_Lock);

	struct WrittenRecord { ResourceId id; bool written; };
	vector<WrittenRecord> written;

	// reasonable estimate, and these records are small
	written.reserve(m_FrameReferencedResources.size());

	for(auto it=m_FrameReferencedResources.begin(); it != m_FrameReferencedResources.end(); ++it)
	{
		RecordType *record = GetResourceRecord(it->first);

		if(it->second != eFrameRef_ReadOnly && it->second != eFrameRef_Unknown)
		{
			WrittenRecord wr = { it->first, record ? record->DataInSerialiser : true };

			written.push_back(wr);
		}
	}
	
	for(auto it=m_DirtyResources.begin(); it != m_DirtyResources.end(); ++it)
	{
		ResourceId id = *it;
		auto ref = m_FrameReferencedResources.find(id);
		if(ref == m_FrameReferencedResources.end() || ref->second == eFrameRef_ReadOnly)
		{
			WrittenRecord wr = { id, true };

			written.push_back(wr);
		}
	}
	
	uint32_t numWritten = (uint32_t)written.size();
	m_pSerialiser->Serialise("NumWrittenResources", numWritten);

	for(auto it=written.begin(); it != written.end(); ++it)
	{
		m_pSerialiser->Serialise("id", it->id);
		m_pSerialiser->Serialise("WrittenData", it->written);
	}
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::CreateInitialContents()
{
	set<ResourceId> neededInitials;

	uint32_t NumWrittenResources = 0;
	m_pSerialiser->Serialise("NumWrittenResources", NumWrittenResources);

	for(uint32_t i=0; i < NumWrittenResources; i++)
	{
		ResourceId id = ResourceId();
		bool WrittenData = false;

		m_pSerialiser->Serialise("id", id);
		m_pSerialiser->Serialise("WrittenData", WrittenData);

		neededInitials.insert(id);

		if(HasLiveResource(id) && m_InitialContents.find(id) == m_InitialContents.end())
			Create_InitialState(id, GetLiveResource(id), WrittenData);
	}

	for(auto it=m_InitialContents.begin(); it != m_InitialContents.end(); )
	{
		ResourceId id = it->first;

		if(neededInitials.find(id) == neededInitials.end())
		{
			ResourceTypeRelease(it->second.resource);
			Serialiser::FreeAlignedBuffer(it->second.blob);
			++it;
			m_InitialContents.erase(id);
		}
		else
		{
			++it;
		}
	}
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::ApplyInitialContents()
{
	RDCDEBUG("Applying initial contents");
	uint32_t numContents = 0;
	for(auto it=m_InitialContents.begin(); it != m_InitialContents.end(); ++it)
	{
		ResourceId id = it->first;
		
		if(HasLiveResource(id))
		{
			ResourceType live = GetLiveResource(id);

			numContents++;

			Apply_InitialState(live, it->second);
		}
	}
	RDCDEBUG("Applied %d", numContents);
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::MarkUnwrittenResources()
{
	SCOPED_LOCK(m_Lock);
	
	for(auto it=m_ResourceRecords.begin(); it != m_ResourceRecords.end(); ++it)
	{
		it->second->MarkDataUnwritten();
	}
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::InsertReferencedChunks(Serialiser *fileSer)
{
	map<int32_t,Chunk*> sortedChunks;

	SCOPED_LOCK(m_Lock);

	RDCDEBUG("%u frame resource records", (uint32_t)m_FrameReferencedResources.size());

	if(RenderDoc::Inst().GetCaptureOptions().RefAllResources)
	{
		for(auto it=m_ResourceRecords.begin(); it != m_ResourceRecords.end(); ++it)
		{
			if(!SerialisableResource(it->first, it->second))
				continue;

			it->second->Insert(sortedChunks);
		}
	}
	else
	{
		for(auto it=m_FrameReferencedResources.begin(); it != m_FrameReferencedResources.end(); ++it)
		{
			RecordType *record = GetResourceRecord(it->first);
			if(record)
				record->Insert(sortedChunks);
		}
	}

	RDCDEBUG("%u frame resource chunks", (uint32_t)sortedChunks.size());

	for(auto it = sortedChunks.begin(); it != sortedChunks.end(); it++)
	{
		fileSer->Insert(it->second);
	}

	RDCDEBUG("inserted to serialiser");
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::PrepareInitialContents()
{
	for(auto it=m_DirtyResources.begin(); it != m_DirtyResources.end(); ++it)
	{
		ResourceId id = *it;
		
		if(!HasCurrentResource(id)) continue;

		RecordType *record = GetResourceRecord(id);
		ResourceType res = GetCurrentResource(id);

		if(record == NULL || record->SpecialResource) continue;
		
		RDCDEBUG("Dirty Resource %llu", id);

		Prepare_InitialState(res);
	}

	for(auto it=m_CurrentResourceMap.begin(); it != m_CurrentResourceMap.end(); ++it)
	{
		if(it->second == (ResourceType)RecordType::NullResource) continue;

		if(Force_InitialState(it->second))
		{
			Prepare_InitialState(it->second);
		}
	}
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::InsertInitialContentsChunks(Serialiser *fileSerialiser)
{
	SCOPED_LOCK(m_Lock);

	for(auto it=m_DirtyResources.begin(); it != m_DirtyResources.end(); ++it)
	{
		ResourceId id = *it;
		
		if(m_FrameReferencedResources.find(id) == m_FrameReferencedResources.end() &&
			 !RenderDoc::Inst().GetCaptureOptions().RefAllResources)
		{
			RDCDEBUG("Resource %llu is GPU dirty but not referenced - skipping", id);
			continue;
		}
		
		if(!HasCurrentResource(id)) continue;

		RecordType *record = GetResourceRecord(id);
		ResourceType res = GetCurrentResource(id);

		if(record == NULL || record->SpecialResource) continue;

		RDCDEBUG("Dirty Resource %llu", id);

		if(!Need_InitialStateChunk(res))
		{
			// just need to grab data, don't create chunk
			Serialise_InitialState(res);
			continue;
		}

		auto preparedChunk = m_InitialChunks.find(id);
		if(preparedChunk != m_InitialChunks.end())
		{
			fileSerialiser->Insert(preparedChunk->second);
			m_InitialChunks.erase(preparedChunk);
		}
		else
		{
			ScopedContext scope(m_pSerialiser, NULL, "Initial Contents", "Initial Contents", INITIAL_CONTENTS, false);

			Serialise_InitialState(res);

			fileSerialiser->Insert(scope.Get(true));
		}
	}

	for(auto it=m_CurrentResourceMap.begin(); it != m_CurrentResourceMap.end(); ++it)
	{
		if(it->second == (ResourceType)RecordType::NullResource) continue;

		if(Force_InitialState(it->second))
		{
			auto preparedChunk = m_InitialChunks.find(it->first);
			if(preparedChunk != m_InitialChunks.end())
			{
				fileSerialiser->Insert(preparedChunk->second);
				m_InitialChunks.erase(preparedChunk);
			}
			else
			{
				ScopedContext scope(m_pSerialiser, NULL, "Initial Contents", "Initial Contents", INITIAL_CONTENTS, false);

				Serialise_InitialState(it->second);

				fileSerialiser->Insert(scope.Get(true));
			}
		}
	}

	// delete/cleanup any chunks that weren't used (maybe the resource was not
	// referenced).
	for(auto it=m_InitialChunks.begin(); it != m_InitialChunks.end(); ++it)
		delete it->second;

	m_InitialChunks.clear();
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::ReleaseInFrameResources()
{	
	SCOPED_LOCK(m_Lock);
	
	// clean up last frame's temporaries - we needed to keep them around so they were valid for
	// pipeline inspection etc after replaying the last log.
	for(auto it = m_InframeResourceMap.begin(); it != m_InframeResourceMap.end(); ++it)
	{
		ResourceTypeRelease(it->second);
	}

	m_InframeResourceMap.clear();
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::ClearReferencedResources()
{	
	SCOPED_LOCK(m_Lock);
	
	for(auto it=m_FrameReferencedResources.begin(); it != m_FrameReferencedResources.end(); ++it)
	{
		RecordType *record = GetResourceRecord(it->first);

		if(record)
			record->Delete(this);
	}

	m_FrameReferencedResources.clear();
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::ReplaceResource(ResourceId from, ResourceId to)
{
	SCOPED_LOCK(m_Lock);

	if(HasLiveResource(to))
		m_Replacements[from] = to;
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::RemoveReplacement(ResourceId id)
{
	SCOPED_LOCK(m_Lock);
	
	auto it = m_Replacements.find(id);
	
	if(it == m_Replacements.end())
		return;

	m_Replacements.erase(it);
}

template<typename ResourceType, typename RecordType>
RecordType *ResourceManager<ResourceType, RecordType>::GetResourceRecord(ResourceId id)
{
	SCOPED_LOCK(m_Lock);

	auto it = m_ResourceRecords.find(id);

	if(it == m_ResourceRecords.end())
		return NULL;

	return it->second;
}

template<typename ResourceType, typename RecordType>
bool ResourceManager<ResourceType, RecordType>::HasResourceRecord(ResourceId id)
{
	SCOPED_LOCK(m_Lock);

	auto it = m_ResourceRecords.find(id);

	if(it == m_ResourceRecords.end())
		return false;

	return true;
}

template<typename ResourceType, typename RecordType>
RecordType *ResourceManager<ResourceType, RecordType>::AddResourceRecord(ResourceId id)
{
	SCOPED_LOCK(m_Lock);

	RDCASSERT(m_ResourceRecords.find(id) == m_ResourceRecords.end());

	return (m_ResourceRecords[id] = new RecordType(id));
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::RemoveResourceRecord(ResourceId id)
{
	SCOPED_LOCK(m_Lock);

	RDCASSERT(m_ResourceRecords.find(id) != m_ResourceRecords.end());
	
	m_ResourceRecords.erase(id);
}

template<typename ResourceType, typename RecordType>
bool ResourceManager<ResourceType, RecordType>::AddWrapper(ResourceType wrap, ResourceType real)
{
	SCOPED_LOCK(m_Lock);

	bool ret = true;

	if(wrap == (ResourceType)RecordType::NullResource || real == (ResourceType)RecordType::NullResource)
	{
		RDCERR("Invalid state creating resource wrapper - wrapped or real resource is NULL");
		ret = false;
	}

	if(m_WrapperMap[real] != (ResourceType)RecordType::NullResource)
	{
		RDCERR("Overriding wrapper for resource");
		ret = false;
	}

	m_WrapperMap[real] = wrap;

	return ret;
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::RemoveWrapper(ResourceType real)
{
	SCOPED_LOCK(m_Lock);
	
	if(real == (ResourceType)RecordType::NullResource || !HasWrapper(real))
	{
		RDCERR("Invalid state removing resource wrapper - real resource is NULL or doesn't have wrapper");
		return;
	}

	m_WrapperMap.erase( m_WrapperMap.find(real) );
}

template<typename ResourceType, typename RecordType>
bool ResourceManager<ResourceType, RecordType>::HasWrapper(ResourceType real)
{
	SCOPED_LOCK(m_Lock);

	if(real == (ResourceType)RecordType::NullResource)
		return false;

	return (m_WrapperMap.find(real) != m_WrapperMap.end());
}

template<typename ResourceType, typename RecordType>
ResourceType ResourceManager<ResourceType, RecordType>::GetWrapper(ResourceType real)
{
	SCOPED_LOCK(m_Lock);
	
	if(real == (ResourceType)RecordType::NullResource)
		return (ResourceType)RecordType::NullResource;

	if(real != (ResourceType)RecordType::NullResource && !HasWrapper(real))
	{
		RDCERR("Invalid state removing resource wrapper - real resource isn't NULL and doesn't have wrapper");
	}

	return m_WrapperMap[real];
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::AddLiveResource(ResourceId origid, ResourceType livePtr)
{
	SCOPED_LOCK(m_Lock);
	
	if(origid == ResourceId() || livePtr == (ResourceType)RecordType::NullResource)
	{
		RDCERR("Invalid state adding resource mapping - id is invalid or live pointer is NULL");
	}

	m_OriginalIDs[GetID(livePtr)] = origid;
	m_LiveIDs[origid] = GetID(livePtr);

	if(m_InFrame && m_InframeResourceMap.find(origid) != m_InframeResourceMap.end())
	{
		ResourceTypeRelease(m_InframeResourceMap[origid]);
		m_InframeResourceMap.erase(origid);
	}
	else if(!m_InFrame && m_LiveResourceMap.find(origid) != m_LiveResourceMap.end())
	{
		RDCERR("Releasing live resource for duplicate creation: %llu", origid);
		ResourceTypeRelease(m_LiveResourceMap[origid]);
		m_LiveResourceMap.erase(origid);
	}

	if(m_InFrame)
		m_InframeResourceMap[origid] = livePtr;
	else
		m_LiveResourceMap[origid] = livePtr;
}

template<typename ResourceType, typename RecordType>
bool ResourceManager<ResourceType, RecordType>::HasLiveResource(ResourceId origid)
{
	SCOPED_LOCK(m_Lock);

	if(origid == ResourceId())
		return false;

	return (m_Replacements.find(origid) != m_Replacements.end() ||
			m_InframeResourceMap.find(origid) != m_InframeResourceMap.end() ||
			m_LiveResourceMap.find(origid) != m_LiveResourceMap.end());
}

template<typename ResourceType, typename RecordType>
ResourceType ResourceManager<ResourceType, RecordType>::GetLiveResource(ResourceId origid)
{
	SCOPED_LOCK(m_Lock);

	if(origid == ResourceId())
		return (ResourceType)RecordType::NullResource;

	RDCASSERT(HasLiveResource(origid));
	
	if(m_Replacements.find(origid) != m_Replacements.end())
		return GetLiveResource(m_Replacements[origid]);

	if(m_InframeResourceMap.find(origid) != m_InframeResourceMap.end())
		return m_InframeResourceMap[origid];
	
	if(m_LiveResourceMap.find(origid) != m_LiveResourceMap.end())
		return m_LiveResourceMap[origid];

	return (ResourceType)RecordType::NullResource;
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::EraseLiveResource(ResourceId origid)
{
	SCOPED_LOCK(m_Lock);

	RDCASSERT(HasLiveResource(origid));

	if(m_InframeResourceMap.find(origid) != m_InframeResourceMap.end())
	{
		m_InframeResourceMap.erase(origid);
	}
	else
	{
		m_LiveResourceMap.erase(origid);
	}
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::AddCurrentResource(ResourceId id, ResourceType res)
{
	SCOPED_LOCK(m_Lock);

	RDCASSERT(m_CurrentResourceMap.find(id) == m_CurrentResourceMap.end());
	m_CurrentResourceMap[id] = res;
}

template<typename ResourceType, typename RecordType>
bool ResourceManager<ResourceType, RecordType>::HasCurrentResource(ResourceId id)
{
	SCOPED_LOCK(m_Lock);

	return m_CurrentResourceMap.find(id) != m_CurrentResourceMap.end();
}

template<typename ResourceType, typename RecordType>
ResourceType ResourceManager<ResourceType, RecordType>::GetCurrentResource(ResourceId id)
{
	SCOPED_LOCK(m_Lock);

	RDCASSERT(m_CurrentResourceMap.find(id) != m_CurrentResourceMap.end());
	return m_CurrentResourceMap[id];
}

template<typename ResourceType, typename RecordType>
void ResourceManager<ResourceType, RecordType>::ReleaseCurrentResource(ResourceId id)
{
	SCOPED_LOCK(m_Lock);

	RDCASSERT(m_CurrentResourceMap.find(id) != m_CurrentResourceMap.end());
	m_CurrentResourceMap.erase(id);
}

template<typename ResourceType, typename RecordType>
ResourceId ResourceManager<ResourceType, RecordType>::GetOriginalID(ResourceId id)
{
	if(id == ResourceId())
		return id;

	RDCASSERT(m_OriginalIDs.find(id) != m_OriginalIDs.end());
	return m_OriginalIDs[id];
}

template<typename ResourceType, typename RecordType>
ResourceId ResourceManager<ResourceType, RecordType>::GetLiveID(ResourceId id)
{
	if(id == ResourceId())
		return id;

	RDCASSERT(m_LiveIDs.find(id) != m_LiveIDs.end());
	return m_LiveIDs[id];
}


