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

#pragma once

#include <map>
#include <set>
#include "api/replay/renderdoc_replay.h"
#include "common/threading.h"
#include "core/core.h"
#include "os/os_specific.h"
#include "serialise/serialiser.h"

// In what way (read, write, etc) was a resource referenced in a frame -
// used to determine if initial contents are needed and to what degree.
// These values are used both as states (representing the cumulative previous
// accesses to the resource), and state transitions (access by a single
// command, modifying the state). This state machine is illustrated below,
// with states represented in caps, and transitions in lower case.
//
//        +------------ NONE -------------+
//        |              |                |
//       read          write            clear
//        |              |                |
//        V              V                V
//      READ <--read-- WRITE --clear--> CLEAR
//        |
//   write/clear
//        |
//        V
//  READBEFOREWRITE
//
// Note:
//  * All resources begin implicitly in the None state.
//  * The state transitions for ReadBeforeWrite are simply the composition of
//    the transition for read, followed by the transition for write (e.g.
//    ReadBeforeWrite moves from NONE state to READBEFOREWRITE state).
//  * All other transitions (excluding ReadBeforeWrite) that are not explicitly
//    shown leave the state unchanged (e.g. a read in the CLEAR state remains
//    in the CLEAR state).
//
// The enum values are ordered so that larger values correspond to greater
// requirements for (re)initialization of the resource during replay.
enum FrameRefType
{
  // Initial state, no reads or writes
  eFrameRef_None = 0,

  // Write to some unknown subset of resource.
  // As a state, this represents that unlike clear, some part of the
  // initial contents might still be visible to later reads.
  eFrameRef_PartialWrite = 1,

  // Write to the entire resource.
  // As a state, this represents that no later reads will even be able to see
  // the initial contents, and therefore, the initial contents need not be
  // restored for replay.
  eFrameRef_CompleteWrite = 2,

  // Read from the resource;
  // As a state, this represents a read that could have seen the resource's
  // initial contents, but the value seen by the read has not been overwritten;
  // therefore, the initial contents needs to be restored before the first time
  // we replay, but doesn't need to be reset between subsequent replays.
  eFrameRef_Read = 3,

  // Read followed by a write;
  // As a state, this represents a read that could have seen the resource
  // initial contents, followed by a write that could have modified that
  // initial contents; therefore, the initial contents will need to be reset
  // before each time we replay the frame.
  eFrameRef_ReadBeforeWrite = 4,
};

const FrameRefType eFrameRef_Minimum = eFrameRef_None;
const FrameRefType eFrameRef_Maximum = eFrameRef_ReadBeforeWrite;

DECLARE_REFLECTION_ENUM(FrameRefType);

// Compose frame refs that occur in a known order.
// This can be thought of as a state (`first`) and a transition from that state
// (`second`), returning the new state (see the state diagram for
// `FrameRefType` above)
FrameRefType ComposeFrameRefs(FrameRefType first, FrameRefType second);

// Compose frame refs when the order is unknown.
// This is conservative, in that, if there is both a Read and a Write/Clear, it
// assumes the Read occurs before the Write/Clear, forcing that resource to be
// reset for replay.
FrameRefType ComposeFrameRefsUnordered(FrameRefType first, FrameRefType second);

bool IsDirtyFrameRef(FrameRefType refType);

// Captures the possible initialization/reset requirements for resources.
// These requirements are entirely determined by the resource's FrameRefType,
// but this type improves the readability of the code that checks
// init/reset requirements.
enum InitReqType
{
  // Initial contents of the resource are not used, and also not modified.
  // No initialization/reset is required for correct replay, but may be helpful
  // to avoid user confusion when analyzing the resource.
  // Corresponds to `None` ref type.
  eInitReq_None,

  // Initial contents of the resource are not used, but the contents are
  // potentially modified during the frame.
  // No initialization/reset is required for correct replay, but may be helpful
  // to avoid user confusion when analyzing the resource.
  // Corresponds to `PartialWrite` and `CompleteWrite` ref types.
  eInitReq_Clear,

  // Initial contents of the resource are read, but not overwritten;
  // the resource needs to be initialized before the first replay, but need not
  // be reset before subsequent replays.
  // Corresponds to `Read` ref type.
  eInitReq_InitOnce,

  // Initial contents of the resource are read, and later overwritten;
  // the resource needs to be reset before each replay.
  // Corresponds to `ReadBeforeWrite` ref type.
  eInitReq_Reset,
};

// Return the initialization/reset requirements for a FrameRefType
inline InitReqType InitReq(FrameRefType refType)
{
  switch(refType)
  {
    case eFrameRef_None: return eInitReq_None;
    case eFrameRef_Read: return eInitReq_InitOnce;
    case eFrameRef_ReadBeforeWrite: return eInitReq_Reset;
    default: return eInitReq_Clear;
  }
}

// handle marking a resource referenced for read or write and storing RAW access etc.
template <typename Compose>
bool MarkReferenced(std::map<ResourceId, FrameRefType> &refs, ResourceId id, FrameRefType refType,
                    Compose comp)
{
  auto refit = refs.find(id);
  if(refit == refs.end())
  {
    refs[id] = refType;
    return true;
  }
  else
  {
    refit->second = comp(refit->second, refType);
  }
  return false;
}

inline bool MarkReferenced(std::map<ResourceId, FrameRefType> &refs, ResourceId id,
                           FrameRefType refType)
{
  return MarkReferenced(refs, id, refType, ComposeFrameRefs);
}

// verbose prints with IDs of each dirty resource and whether it was prepared,
// and whether it was serialised.
#define VERBOSE_DIRTY_RESOURCES OPTION_OFF

namespace ResourceIDGen
{
ResourceId GetNewUniqueID();
void SetReplayResourceIDs();
};

struct ResourceRecord;

class ResourceRecordHandler
{
public:
  virtual void MarkDirtyResource(ResourceId id) = 0;
  virtual void RemoveResourceRecord(ResourceId id) = 0;
  virtual void MarkResourceFrameReferenced(ResourceId id, FrameRefType refType) = 0;
  virtual void DestroyResourceRecord(ResourceRecord *record) = 0;
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
      : RefCount(1),
        ResID(id),
        UpdateCount(0),
        DataInSerialiser(false),
        DataPtr(NULL),
        DataOffset(0),
        Length(0),
        DataWritten(false),
        InternalResource(false)
  {
    m_ChunkLock = NULL;

    if(lock)
      m_ChunkLock = new Threading::CriticalSection();
  }

  ~ResourceRecord() { SAFE_DELETE(m_ChunkLock); }
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

  void MarkDataUnwritten() { DataWritten = false; }
  void Insert(std::map<int32_t, Chunk *> &recordlist)
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
      for(auto it = m_Chunks.begin(); it != m_Chunks.end(); ++it)
        recordlist[it->first] = it->second;
    }
  }

  void AddRef() { Atomic::Inc32(&RefCount); }
  int GetRefCount() const { return RefCount; }
  void Delete(ResourceRecordHandler *mgr);

  ResourceId GetResourceID() const { return ResID; }
  void RemoveChunk(Chunk *chunk)
  {
    LockChunks();
    for(auto it = m_Chunks.begin(); it != m_Chunks.end(); ++it)
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
    if(ID == 0)
      ID = GetID();
    LockChunks();
    m_Chunks.push_back({ID, chunk});
    UnlockChunks();
  }

  void LockChunks()
  {
    if(m_ChunkLock)
      m_ChunkLock->Lock();
  }
  void UnlockChunks()
  {
    if(m_ChunkLock)
      m_ChunkLock->Unlock();
  }

  bool HasChunks() const { return !m_Chunks.empty(); }
  size_t NumChunks() const { return m_Chunks.size(); }
  void SwapChunks(ResourceRecord *other)
  {
    LockChunks();
    other->LockChunks();
    m_Chunks.swap(other->m_Chunks);
    m_FrameRefs.swap(other->m_FrameRefs);
    other->UnlockChunks();
    UnlockChunks();
  }

  void AppendFrom(ResourceRecord *other)
  {
    LockChunks();
    other->LockChunks();

    for(auto it = other->m_Chunks.begin(); it != other->m_Chunks.end(); ++it)
      AddChunk(it->second->Duplicate());

    for(auto it = other->Parents.begin(); it != other->Parents.end(); ++it)
      AddParent(*it);

    other->UnlockChunks();
    UnlockChunks();
  }

  void DeleteChunks()
  {
    LockChunks();
    for(auto it = m_Chunks.begin(); it != m_Chunks.end(); ++it)
      SAFE_DELETE(it->second);
    m_Chunks.clear();
    UnlockChunks();
  }

  Chunk *GetLastChunk() const
  {
    RDCASSERT(HasChunks());
    return m_Chunks.back().second;
  }

  int32_t GetLastChunkID() const
  {
    RDCASSERT(HasChunks());
    return m_Chunks.back().first;
  }

  void PopChunk() { m_Chunks.pop_back(); }
  byte *GetDataPtr() { return DataPtr + DataOffset; }
  bool HasDataPtr() { return DataPtr != NULL; }
  void SetDataOffset(uint64_t offs) { DataOffset = offs; }
  void SetDataPtr(byte *ptr) { DataPtr = ptr; }
  template <typename Compose>
  bool MarkResourceFrameReferenced(ResourceId id, FrameRefType refType, Compose comp);
  inline bool MarkResourceFrameReferenced(ResourceId id, FrameRefType refType)
  {
    return MarkResourceFrameReferenced(id, refType, ComposeFrameRefs);
  }
  void AddResourceReferences(ResourceRecordHandler *mgr);
  void AddReferencedIDs(std::set<ResourceId> &ids)
  {
    for(auto it = m_FrameRefs.begin(); it != m_FrameRefs.end(); ++it)
      ids.insert(it->first);
  }

  uint64_t Length;

  int UpdateCount;
  bool DataInSerialiser;

  // anything internal that shouldn't be automatically pulled in by 'Ref All Resources' or have
  // initial contents stored. This could either be a type of object that would break if its chunks
  // were inserted into the initialisation phase (like an D3D11 DeviceContext which contains
  // commands) or just debug objects created during capture as helpers which shouldn't be included.
  //
  // The implication is that they are handled specially for being inserted into the capture, or
  // aren't inserted at all. Note that if a resource is frame-referenced, it will be included
  // regardless but still without initial contents, capture drivers should be careful.
  bool InternalResource;
  bool DataWritten;

protected:
  volatile int32_t RefCount;

  byte *DataPtr;
  uint64_t DataOffset;

  ResourceId ResID;

  std::set<ResourceRecord *> Parents;

  int32_t GetID()
  {
    static volatile int32_t globalIDCounter = 10;

    return Atomic::Inc32(&globalIDCounter);
  }

  std::vector<rdcpair<int32_t, Chunk *>> m_Chunks;
  Threading::CriticalSection *m_ChunkLock;

  std::map<ResourceId, FrameRefType> m_FrameRefs;
};

template <typename Compose>
bool ResourceRecord::MarkResourceFrameReferenced(ResourceId id, FrameRefType refType, Compose comp)
{
  if(id == ResourceId())
    return false;
  return MarkReferenced(m_FrameRefs, id, refType, comp);
}

// the resource manager is a utility class that's not required but is likely wanted by any API
// implementation.
// It keeps track of resource records, which resources are alive and allows you to query for them by
// ID. It tracks
// which resources are marked as dirty (needing their initial contents fetched before capture).
//
// For APIs that wrap their resources it provides tracking for that.
//
// In the replay application it will also track which 'live' resources are representing which
// 'original'
// resources from the application when it was captured.
template <typename Configuration>
class ResourceManager : public ResourceRecordHandler
{
public:
  typedef typename Configuration::WrappedResourceType WrappedResourceType;
  typedef typename Configuration::RealResourceType RealResourceType;
  typedef typename Configuration::RecordType RecordType;
  typedef typename Configuration::InitialContentData InitialContentData;

  ResourceManager();
  virtual ~ResourceManager();

  void Shutdown();

  ///////////////////////////////////////////
  // Capture-side methods

  // while capturing, resource records containing chunk streams and metadata for resources
  RecordType *GetResourceRecord(ResourceId id);
  bool HasResourceRecord(ResourceId id);
  RecordType *AddResourceRecord(ResourceId id);
  inline void RemoveResourceRecord(ResourceId id);
  void DestroyResourceRecord(ResourceRecord *record);

  // while capturing or replaying, resources and their live IDs
  void AddCurrentResource(ResourceId id, WrappedResourceType res);
  bool HasCurrentResource(ResourceId id);
  WrappedResourceType GetCurrentResource(ResourceId id);
  void ReleaseCurrentResource(ResourceId id);

  // insert the chunks for the resources referenced in the frame
  void InsertReferencedChunks(WriteSerialiser &ser);

  // mark resource records as unwritten, ready to be written to a new logfile.
  void MarkUnwrittenResources();

  // clear the list of frame-referenced resources - e.g. if you're about to recapture a frame
  void ClearReferencedResources();

  // indicates this resource could have been modified by the GPU,
  // so it's now suspect and the data we have on it might well be out of date
  // and to be correct its contents should be serialised out at the start
  // of the frame.
  inline void MarkDirtyResource(ResourceId res);

  // returns if the resource has been marked as dirty
  bool IsResourceDirty(ResourceId res);

  // call callbacks to prepare initial contents for dirty resources
  void PrepareInitialContents();

  InitialContentData GetInitialContents(ResourceId id);
  void SetInitialContents(ResourceId id, InitialContentData contents);
  void SetInitialChunk(ResourceId id, Chunk *chunk);

  // generate chunks for initial contents and insert.
  void InsertInitialContentsChunks(WriteSerialiser &ser);

  // for initial contents that don't need a chunk - apply them here. This allows any patching to
  // creation-time chunks to happen before they're written to disk.
  void ApplyInitialContentsNonChunks(WriteSerialiser &ser);

  // Serialise out which resources need initial contents, along with whether their
  // initial contents are in the serialised stream (e.g. RTs might still want to be
  // cleared on frame init).
  void Serialise_InitialContentsNeeded(WriteSerialiser &ser);

  // mark resource referenced somewhere in the main frame-affecting calls.
  // That means this resource should be included in the final serialise out
  template <typename Compose>
  void MarkResourceFrameReferenced(ResourceId id, FrameRefType refType, Compose comp);

  inline void MarkResourceFrameReferenced(ResourceId id, FrameRefType refType);

  ///////////////////////////////////////////
  // Replay-side methods

  // Live resources to replace serialised IDs
  void AddLiveResource(ResourceId origid, WrappedResourceType livePtr);
  bool HasLiveResource(ResourceId origid);
  WrappedResourceType GetLiveResource(ResourceId origid);
  void EraseLiveResource(ResourceId origid);

  // when asked for a given id, return the resource for a replacement id
  void ReplaceResource(ResourceId from, ResourceId to);
  bool HasReplacement(ResourceId from);
  void RemoveReplacement(ResourceId id);

  // fetch original ID for a real ID or vice-versa.
  ResourceId GetOriginalID(ResourceId id);
  ResourceId GetLiveID(ResourceId id);

  // Serialise in which resources need initial contents and set them up.
  void CreateInitialContents(ReadSerialiser &ser);

  // Free any initial contents that are prepared (for after capture is complete)
  void FreeInitialContents();

  // Apply the initial contents for the resources that need them, used at the start of a frame
  void ApplyInitialContents();

  // Resource wrapping, allows for querying and adding/removing of wrapper layers around resources
  bool AddWrapper(WrappedResourceType wrap, RealResourceType real);
  bool HasWrapper(RealResourceType real);
  WrappedResourceType GetWrapper(RealResourceType real);
  void RemoveWrapper(RealResourceType real);

protected:
  friend InitialContentData;
  // 'interface' to implement by derived classes
  virtual ResourceId GetID(WrappedResourceType res) = 0;

  virtual bool ResourceTypeRelease(WrappedResourceType res) = 0;

  virtual bool Need_InitialStateChunk(ResourceId id, const InitialContentData &initial)
  {
    return true;
  }
  virtual bool Prepare_InitialState(WrappedResourceType res) = 0;
  virtual uint64_t GetSize_InitialState(ResourceId id, const InitialContentData &initial) = 0;
  virtual bool Serialise_InitialState(WriteSerialiser &ser, ResourceId id, RecordType *record,
                                      const InitialContentData *initialData) = 0;
  virtual void Create_InitialState(ResourceId id, WrappedResourceType live, bool hasData) = 0;
  virtual void Apply_InitialState(WrappedResourceType live, const InitialContentData &initial) = 0;
  virtual std::vector<ResourceId> InitialContentResources();

  // very coarse lock, protects EVERYTHING. This could certainly be improved and it may be a
  // bottleneck
  // for performance. Given that the main use cases are write-rarely read-often the lock should be
  // optimised
  // for that as we only want to make sure we're not modifying the objects together, by far the most
  // common
  // operation is looking up data.
  Threading::CriticalSection m_Lock;

  // easy optimisation win - don't use maps everywhere. It's convenient but not optimal, and
  // profiling will
  // likely prove that some or all of these could be a problem

  // used during capture - map from real resource to its wrapper (other way can be done just with an
  // Unwrap)
  std::map<RealResourceType, WrappedResourceType> m_WrapperMap;

  // used during capture - holds resources referenced in current frame (and how they're referenced)
  std::map<ResourceId, FrameRefType> m_FrameReferencedResources;

  // used during capture - holds resources marked as dirty, needing initial contents
  std::set<ResourceId> m_DirtyResources;

  struct InitialContentDataOrChunk
  {
    Chunk *chunk = NULL;
    InitialContentData data;

    void Free(ResourceManager *mgr)
    {
      if(chunk)
      {
        delete chunk;
        chunk = NULL;
      }

      data.Free(mgr);
    }
  };

  // used during capture or replay - holds initial contents
  std::map<ResourceId, InitialContentDataOrChunk> m_InitialContents;

  // used during capture or replay - map of resources currently alive with their real IDs, used in
  // capture and replay.
  std::map<ResourceId, WrappedResourceType> m_CurrentResourceMap;

  // used during replay - maps back and forth from original id to live id and vice-versa
  std::map<ResourceId, ResourceId> m_OriginalIDs, m_LiveIDs;

  // used during replay - holds resources allocated and the original id that they represent
  std::map<ResourceId, WrappedResourceType> m_LiveResourceMap;

  // used during capture - holds resource records by id.
  std::map<ResourceId, RecordType *> m_ResourceRecords;

  // used during replay - holds current resource replacements
  std::map<ResourceId, ResourceId> m_Replacements;
};

template <typename Configuration>
ResourceManager<Configuration>::ResourceManager()
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(ResourceManager));
}

template <typename Configuration>
void ResourceManager<Configuration>::Shutdown()
{
  FreeInitialContents();

  while(!m_LiveResourceMap.empty())
  {
    auto it = m_LiveResourceMap.begin();
    ResourceId id = it->first;
    ResourceTypeRelease(it->second);

    auto removeit = m_LiveResourceMap.find(id);
    if(removeit != m_LiveResourceMap.end())
      m_LiveResourceMap.erase(removeit);
  }

  RDCASSERT(m_ResourceRecords.empty());
}

template <typename Configuration>
ResourceManager<Configuration>::~ResourceManager()
{
  RDCASSERT(m_LiveResourceMap.empty());
  RDCASSERT(m_InitialContents.empty());
  RDCASSERT(m_ResourceRecords.empty());

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

template <typename Configuration>
template <typename Compose>
void ResourceManager<Configuration>::MarkResourceFrameReferenced(ResourceId id,
                                                                 FrameRefType refType, Compose comp)
{
  SCOPED_LOCK(m_Lock);

  if(id == ResourceId())
    return;

  bool newRef = MarkReferenced(m_FrameReferencedResources, id, refType, comp);

  if(newRef)
  {
    RecordType *record = GetResourceRecord(id);

    if(record)
      record->AddRef();
  }
}

template <typename Configuration>
void ResourceManager<Configuration>::MarkResourceFrameReferenced(ResourceId id, FrameRefType refType)
{
  return MarkResourceFrameReferenced(id, refType, ComposeFrameRefs);
}

template <typename Configuration>
void ResourceManager<Configuration>::MarkDirtyResource(ResourceId res)
{
  SCOPED_LOCK(m_Lock);

  if(res == ResourceId())
    return;

  m_DirtyResources.insert(res);
}

template <typename Configuration>
bool ResourceManager<Configuration>::IsResourceDirty(ResourceId res)
{
  SCOPED_LOCK(m_Lock);

  if(res == ResourceId())
    return false;

  return m_DirtyResources.find(res) != m_DirtyResources.end();
}

template <typename Configuration>
void ResourceManager<Configuration>::SetInitialContents(ResourceId id, InitialContentData contents)
{
  SCOPED_LOCK(m_Lock);

  RDCASSERT(id != ResourceId());

  auto it = m_InitialContents.find(id);

  if(it != m_InitialContents.end())
    it->second.Free(this);

  m_InitialContents[id].data = contents;
}

template <typename Configuration>
void ResourceManager<Configuration>::SetInitialChunk(ResourceId id, Chunk *chunk)
{
  SCOPED_LOCK(m_Lock);

  RDCASSERT(id != ResourceId());
  RDCASSERT(chunk->GetChunkType<SystemChunk>() == SystemChunk::InitialContents);

  InitialContentDataOrChunk &data = m_InitialContents[id];

  if(data.chunk)
    delete data.chunk;

  data.chunk = chunk;
}

template <typename Configuration>
typename Configuration::InitialContentData ResourceManager<Configuration>::GetInitialContents(
    ResourceId id)
{
  SCOPED_LOCK(m_Lock);

  if(id == ResourceId())
    return InitialContentData();

  if(m_InitialContents.find(id) != m_InitialContents.end())
    return m_InitialContents[id].data;

  return InitialContentData();
}

// use a namespace so this doesn't pollute the global namesapce
namespace ResourceManagerInternal
{
struct WrittenRecord
{
  ResourceId id;
  bool written;
};
};

DECLARE_REFLECTION_STRUCT(ResourceManagerInternal::WrittenRecord);

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, ResourceManagerInternal::WrittenRecord &el)
{
  SERIALISE_MEMBER(id);
  SERIALISE_MEMBER(written);
}

template <typename Configuration>
void ResourceManager<Configuration>::Serialise_InitialContentsNeeded(WriteSerialiser &ser)
{
  using namespace ResourceManagerInternal;

  SCOPED_LOCK(m_Lock);

  std::vector<WrittenRecord> WrittenRecords;

  // reasonable estimate, and these records are small
  WrittenRecords.reserve(m_FrameReferencedResources.size());

  // all resources that were recorded as being modified should be included in the list of those
  // needing initial contents
  for(auto it = m_FrameReferencedResources.begin(); it != m_FrameReferencedResources.end(); ++it)
  {
    RecordType *record = GetResourceRecord(it->first);
    if(IsDirtyFrameRef(it->second))
    {
      WrittenRecord wr = {it->first, record ? record->DataInSerialiser : true};

      WrittenRecords.push_back(wr);
    }
  }

  // any resources that had initial contents generated should also be included
  for(auto it = m_InitialContents.begin(); it != m_InitialContents.end(); ++it)
  {
    ResourceId id = it->first;
    auto ref = m_FrameReferencedResources.find(id);
    if(ref == m_FrameReferencedResources.end() || !IsDirtyFrameRef(ref->second))
    {
      WrittenRecord wr = {id, true};

      WrittenRecords.push_back(wr);
    }
  }

  uint64_t chunkSize = uint64_t(WrittenRecords.size() * sizeof(WrittenRecord) + 16);

  SCOPED_SERIALISE_CHUNK(SystemChunk::InitialContentsList, chunkSize);
  SERIALISE_ELEMENT(WrittenRecords);
}

template <typename Configuration>
void ResourceManager<Configuration>::FreeInitialContents()
{
  while(!m_InitialContents.empty())
  {
    auto it = m_InitialContents.begin();
    it->second.Free(this);
    if(!m_InitialContents.empty())
      m_InitialContents.erase(m_InitialContents.begin());
  }
}

template <typename Configuration>
void ResourceManager<Configuration>::CreateInitialContents(ReadSerialiser &ser)
{
  using namespace ResourceManagerInternal;

  std::set<ResourceId> neededInitials;

  std::vector<WrittenRecord> WrittenRecords;
  SERIALISE_ELEMENT(WrittenRecords);

  for(const WrittenRecord &wr : WrittenRecords)
  {
    ResourceId id = wr.id;

    neededInitials.insert(id);

    if(HasLiveResource(id) && m_InitialContents.find(id) == m_InitialContents.end())
      Create_InitialState(id, GetLiveResource(id), wr.written);
  }

  for(auto it = m_InitialContents.begin(); it != m_InitialContents.end();)
  {
    ResourceId id = it->first;

    if(neededInitials.find(id) == neededInitials.end())
    {
      it->second.Free(this);
      ++it;
      m_InitialContents.erase(id);
    }
    else
    {
      ++it;
    }
  }
}

template <typename Configuration>
void ResourceManager<Configuration>::ApplyInitialContents()
{
  RDCDEBUG("Applying initial contents");
  std::vector<ResourceId> resources = InitialContentResources();
  for(auto it = resources.begin(); it != resources.end(); ++it)
  {
    ResourceId id = *it;
    const InitialContentDataOrChunk &data = m_InitialContents[id];
    WrappedResourceType live = GetLiveResource(id);
    Apply_InitialState(live, data.data);
  }
  RDCDEBUG("Applied %d", (uint32_t)resources.size());
}

template <typename Configuration>
std::vector<ResourceId> ResourceManager<Configuration>::InitialContentResources()
{
  std::vector<ResourceId> resources;
  for(auto it = m_InitialContents.begin(); it != m_InitialContents.end(); ++it)
  {
    ResourceId id = it->first;

    if(HasLiveResource(id))
    {
      resources.push_back(id);
    }
  }
  return resources;
}

template <typename Configuration>
void ResourceManager<Configuration>::MarkUnwrittenResources()
{
  SCOPED_LOCK(m_Lock);

  for(auto it = m_ResourceRecords.begin(); it != m_ResourceRecords.end(); ++it)
  {
    it->second->MarkDataUnwritten();
  }
}

template <typename Configuration>
void ResourceManager<Configuration>::InsertReferencedChunks(WriteSerialiser &ser)
{
  std::map<int32_t, Chunk *> sortedChunks;

  SCOPED_LOCK(m_Lock);

  RDCDEBUG("%u frame resource records", (uint32_t)m_FrameReferencedResources.size());

  if(RenderDoc::Inst().GetCaptureOptions().refAllResources)
  {
    float num = float(m_ResourceRecords.size());
    float idx = 0.0f;

    for(auto it = m_ResourceRecords.begin(); it != m_ResourceRecords.end(); ++it)
    {
      RenderDoc::Inst().SetProgress(CaptureProgress::AddReferencedResources, idx / num);
      idx += 1.0f;

      if(m_FrameReferencedResources.find(it->first) == m_FrameReferencedResources.end() &&
         it->second->InternalResource)
        continue;

      it->second->Insert(sortedChunks);
    }
  }
  else
  {
    float num = float(m_FrameReferencedResources.size());
    float idx = 0.0f;

    for(auto it = m_FrameReferencedResources.begin(); it != m_FrameReferencedResources.end(); ++it)
    {
      RenderDoc::Inst().SetProgress(CaptureProgress::AddReferencedResources, idx / num);
      idx += 1.0f;

      RecordType *record = GetResourceRecord(it->first);
      if(record)
        record->Insert(sortedChunks);
    }
  }

  RDCDEBUG("%u frame resource chunks", (uint32_t)sortedChunks.size());

  for(auto it = sortedChunks.begin(); it != sortedChunks.end(); it++)
    it->second->Write(ser);

  RDCDEBUG("inserted to serialiser");
}

template <typename Configuration>
void ResourceManager<Configuration>::PrepareInitialContents()
{
  SCOPED_LOCK(m_Lock);

  RDCDEBUG("Preparing up to %u potentially dirty resources", (uint32_t)m_DirtyResources.size());
  uint32_t prepared = 0;

  float num = float(m_DirtyResources.size());
  float idx = 0.0f;

  for(auto it = m_DirtyResources.begin(); it != m_DirtyResources.end(); ++it)
  {
    ResourceId id = *it;

    RenderDoc::Inst().SetProgress(CaptureProgress::PrepareInitialStates, idx / num);
    idx += 1.0f;

    // if somehow this resource has been deleted but is still dirty, we can't prepare it. Resources
    // deleted prior to beginning the frame capture cannot linger and be needed - we only need to
    // care about resources deleted after this point (mid-capture)
    if(!HasCurrentResource(id))
      continue;

    RecordType *record = GetResourceRecord(id);
    WrappedResourceType res = GetCurrentResource(id);

    // don't prepare internal resources, or those without a record
    if(record == NULL || record->InternalResource)
      continue;

    prepared++;

#if ENABLED(VERBOSE_DIRTY_RESOURCES)
    RDCDEBUG("Prepare Resource %llu", id);
#endif

    Prepare_InitialState(res);
  }

  RDCDEBUG("Prepared %u dirty resources", prepared);
}

template <typename Configuration>
void ResourceManager<Configuration>::InsertInitialContentsChunks(WriteSerialiser &ser)
{
  SCOPED_LOCK(m_Lock);

  uint32_t dirty = 0;
  uint32_t skipped = 0;

  RDCDEBUG("Checking %u resources with initial contents", (uint32_t)m_InitialContents.size());

  float num = float(m_InitialContents.size());
  float idx = 0.0f;

  for(auto it = m_InitialContents.begin(); it != m_InitialContents.end(); ++it)
  {
    ResourceId id = it->first;

    RenderDoc::Inst().SetProgress(CaptureProgress::SerialiseInitialStates, idx / num);
    idx += 1.0f;

    if(m_FrameReferencedResources.find(id) == m_FrameReferencedResources.end() &&
       !RenderDoc::Inst().GetCaptureOptions().refAllResources)
    {
#if ENABLED(VERBOSE_DIRTY_RESOURCES)
      RDCDEBUG("Dirty tesource %llu is GPU dirty but not referenced - skipping", id);
#endif
      skipped++;
      continue;
    }

    RecordType *record = GetResourceRecord(id);

    if(record == NULL)
    {
#if ENABLED(VERBOSE_DIRTY_RESOURCES)
      RDCDEBUG("Resource %llu has no resource record - skipping", id);
#endif
      continue;
    }

    if(record->InternalResource)
    {
#if ENABLED(VERBOSE_DIRTY_RESOURCES)
      RDCDEBUG("Resource %llu is special - skipping", id);
#endif
      continue;
    }

#if ENABLED(VERBOSE_DIRTY_RESOURCES)
    RDCDEBUG("Serialising dirty Resource %llu", id);
#endif

    dirty++;

    if(!Need_InitialStateChunk(id, it->second.data))
    {
      // this was handled in ApplyInitialContentsNonChunks(), do nothing as there's no point copying
      // the data again (it's already been serialised).
      continue;
    }

    if(it->second.chunk)
    {
      it->second.chunk->Write(ser);
    }
    else
    {
      uint64_t size = GetSize_InitialState(id, it->second.data);

      SCOPED_SERIALISE_CHUNK(SystemChunk::InitialContents, size);

      Serialise_InitialState(ser, id, record, &it->second.data);
    }
  }

  RDCDEBUG("Serialised %u resources, skipped %u unreferenced", dirty, skipped);
}

template <typename Configuration>
void ResourceManager<Configuration>::ApplyInitialContentsNonChunks(WriteSerialiser &ser)
{
  SCOPED_LOCK(m_Lock);

  for(auto it = m_InitialContents.begin(); it != m_InitialContents.end(); ++it)
  {
    ResourceId id = it->first;

    if(m_FrameReferencedResources.find(id) == m_FrameReferencedResources.end() &&
       !RenderDoc::Inst().GetCaptureOptions().refAllResources)
    {
      continue;
    }

    RecordType *record = GetResourceRecord(id);

    if(!record || record->InternalResource)
      continue;

    if(!Need_InitialStateChunk(id, it->second.data))
      Serialise_InitialState(ser, id, record, &it->second.data);
  }
}

template <typename Configuration>
void ResourceManager<Configuration>::ClearReferencedResources()
{
  SCOPED_LOCK(m_Lock);

  for(auto it = m_FrameReferencedResources.begin(); it != m_FrameReferencedResources.end(); ++it)
  {
    RecordType *record = GetResourceRecord(it->first);

    if(record)
      record->Delete(this);
  }

  m_FrameReferencedResources.clear();
}

template <typename Configuration>
void ResourceManager<Configuration>::ReplaceResource(ResourceId from, ResourceId to)
{
  SCOPED_LOCK(m_Lock);

  if(HasLiveResource(to))
    m_Replacements[from] = to;
}

template <typename Configuration>
bool ResourceManager<Configuration>::HasReplacement(ResourceId from)
{
  SCOPED_LOCK(m_Lock);

  return m_Replacements.find(from) != m_Replacements.end();
}

template <typename Configuration>
void ResourceManager<Configuration>::RemoveReplacement(ResourceId id)
{
  SCOPED_LOCK(m_Lock);

  auto it = m_Replacements.find(id);

  if(it == m_Replacements.end())
    return;

  m_Replacements.erase(it);
}

template <typename Configuration>
typename Configuration::RecordType *ResourceManager<Configuration>::GetResourceRecord(ResourceId id)
{
  SCOPED_LOCK(m_Lock);

  auto it = m_ResourceRecords.find(id);

  if(it == m_ResourceRecords.end())
    return NULL;

  return it->second;
}

template <typename Configuration>
bool ResourceManager<Configuration>::HasResourceRecord(ResourceId id)
{
  SCOPED_LOCK(m_Lock);

  auto it = m_ResourceRecords.find(id);

  if(it == m_ResourceRecords.end())
    return false;

  return true;
}

template <typename Configuration>
typename Configuration::RecordType *ResourceManager<Configuration>::AddResourceRecord(ResourceId id)
{
  SCOPED_LOCK(m_Lock);

  RDCASSERT(m_ResourceRecords.find(id) == m_ResourceRecords.end(), id);

  return (m_ResourceRecords[id] = new RecordType(id));
}

template <typename Configuration>
void ResourceManager<Configuration>::RemoveResourceRecord(ResourceId id)
{
  SCOPED_LOCK(m_Lock);

  RDCASSERT(m_ResourceRecords.find(id) != m_ResourceRecords.end(), id);

  m_ResourceRecords.erase(id);
}

template <typename Configuration>
void ResourceManager<Configuration>::DestroyResourceRecord(ResourceRecord *record)
{
  delete(RecordType *)record;
}

template <typename Configuration>
bool ResourceManager<Configuration>::AddWrapper(WrappedResourceType wrap, RealResourceType real)
{
  SCOPED_LOCK(m_Lock);

  bool ret = true;

  if(wrap == (WrappedResourceType)RecordType::NullResource ||
     real == (RealResourceType)RecordType::NullResource)
  {
    RDCERR("Invalid state creating resource wrapper - wrapped or real resource is NULL");
    ret = false;
  }

  if(m_WrapperMap[real] != (WrappedResourceType)RecordType::NullResource)
  {
    RDCERR("Overriding wrapper for resource");
    ret = false;
  }

  m_WrapperMap[real] = wrap;

  return ret;
}

template <typename Configuration>
void ResourceManager<Configuration>::RemoveWrapper(RealResourceType real)
{
  SCOPED_LOCK(m_Lock);

  if(real == (RealResourceType)RecordType::NullResource || !HasWrapper(real))
  {
    RDCERR(
        "Invalid state removing resource wrapper - real resource is NULL or doesn't have wrapper");
    return;
  }

  m_WrapperMap.erase(m_WrapperMap.find(real));
}

template <typename Configuration>
bool ResourceManager<Configuration>::HasWrapper(RealResourceType real)
{
  SCOPED_LOCK(m_Lock);

  if(real == (RealResourceType)RecordType::NullResource)
    return false;

  return (m_WrapperMap.find(real) != m_WrapperMap.end());
}

template <typename Configuration>
typename Configuration::WrappedResourceType ResourceManager<Configuration>::GetWrapper(
    RealResourceType real)
{
  SCOPED_LOCK(m_Lock);

  if(real == (RealResourceType)RecordType::NullResource)
    return (WrappedResourceType)RecordType::NullResource;

  if(real != (RealResourceType)RecordType::NullResource && !HasWrapper(real))
  {
    RDCERR(
        "Invalid state removing resource wrapper - real resource isn't NULL and doesn't have "
        "wrapper");
  }

  return m_WrapperMap[real];
}

template <typename Configuration>
void ResourceManager<Configuration>::AddLiveResource(ResourceId origid, WrappedResourceType livePtr)
{
  SCOPED_LOCK(m_Lock);

  if(origid == ResourceId() || livePtr == (WrappedResourceType)RecordType::NullResource)
  {
    RDCERR("Invalid state adding resource mapping - id is invalid or live pointer is NULL");
  }

  m_OriginalIDs[GetID(livePtr)] = origid;
  m_LiveIDs[origid] = GetID(livePtr);

  if(m_LiveResourceMap.find(origid) != m_LiveResourceMap.end())
  {
    RDCERR("Releasing live resource for duplicate creation: %llu", origid);
    ResourceTypeRelease(m_LiveResourceMap[origid]);
    m_LiveResourceMap.erase(origid);
  }

  m_LiveResourceMap[origid] = livePtr;
}

template <typename Configuration>
bool ResourceManager<Configuration>::HasLiveResource(ResourceId origid)
{
  SCOPED_LOCK(m_Lock);

  if(origid == ResourceId())
    return false;

  return (m_Replacements.find(origid) != m_Replacements.end() ||
          m_LiveResourceMap.find(origid) != m_LiveResourceMap.end());
}

template <typename Configuration>
typename Configuration::WrappedResourceType ResourceManager<Configuration>::GetLiveResource(
    ResourceId origid)
{
  SCOPED_LOCK(m_Lock);

  if(origid == ResourceId())
    return (WrappedResourceType)RecordType::NullResource;

  RDCASSERT(HasLiveResource(origid), origid);

  if(m_Replacements.find(origid) != m_Replacements.end())
    return GetLiveResource(m_Replacements[origid]);

  if(m_LiveResourceMap.find(origid) != m_LiveResourceMap.end())
    return m_LiveResourceMap[origid];

  return (WrappedResourceType)RecordType::NullResource;
}

template <typename Configuration>
void ResourceManager<Configuration>::EraseLiveResource(ResourceId origid)
{
  SCOPED_LOCK(m_Lock);

  RDCASSERT(HasLiveResource(origid), origid);

  m_LiveResourceMap.erase(origid);
}

template <typename Configuration>
void ResourceManager<Configuration>::AddCurrentResource(ResourceId id, WrappedResourceType res)
{
  SCOPED_LOCK(m_Lock);

  RDCASSERT(m_CurrentResourceMap.find(id) == m_CurrentResourceMap.end(), id);
  m_CurrentResourceMap[id] = res;
}

template <typename Configuration>
bool ResourceManager<Configuration>::HasCurrentResource(ResourceId id)
{
  SCOPED_LOCK(m_Lock);

  return m_CurrentResourceMap.find(id) != m_CurrentResourceMap.end();
}

template <typename Configuration>
typename Configuration::WrappedResourceType ResourceManager<Configuration>::GetCurrentResource(
    ResourceId id)
{
  SCOPED_LOCK(m_Lock);

  if(id == ResourceId())
    return (WrappedResourceType)RecordType::NullResource;

  if(m_Replacements.find(id) != m_Replacements.end())
    return GetCurrentResource(m_Replacements[id]);

  RDCASSERT(m_CurrentResourceMap.find(id) != m_CurrentResourceMap.end(), id);
  return m_CurrentResourceMap[id];
}

template <typename Configuration>
void ResourceManager<Configuration>::ReleaseCurrentResource(ResourceId id)
{
  SCOPED_LOCK(m_Lock);

  RDCASSERT(m_CurrentResourceMap.find(id) != m_CurrentResourceMap.end(), id);
  m_CurrentResourceMap.erase(id);
  m_DirtyResources.erase(id);
}

template <typename Configuration>
ResourceId ResourceManager<Configuration>::GetOriginalID(ResourceId id)
{
  if(id == ResourceId())
    return id;

  RDCASSERT(m_OriginalIDs.find(id) != m_OriginalIDs.end(), id);
  return m_OriginalIDs[id];
}

template <typename Configuration>
ResourceId ResourceManager<Configuration>::GetLiveID(ResourceId id)
{
  if(id == ResourceId())
    return id;

  RDCASSERT(m_LiveIDs.find(id) != m_LiveIDs.end(), id);
  return m_LiveIDs[id];
}
