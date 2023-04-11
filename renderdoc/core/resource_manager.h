/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include "api/replay/rdcflatmap.h"
#include "api/replay/resourceid.h"
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
//         +------------------ NONE -----------------------------+
//         |                    |                                |
//        read            partialWrite                     completeWrite
//         |                    |                                |
//         V                    V                                V
//       READ             PARTIAL_WRITE --completeWrite--> COMPLETE_WRITE
//         |                    |
//         |                  read
//       write                  |
//         |                    V
//         |            WRITE_BEFORE_READ
//         V                    |
//  READ_BEFORE_WRITE <--write--+
//
// Note:
//  * All resources begin implicitly in the None state.
//  * The transitions labeled "write" correspond to either PartialWrite or
//    CompleteWrite (e.g. in the READ state, either a PartialWrite or a
//    CompleteWrite moves to the READ_BEFORE_WRITE state).
//  * The state transitions for ReadBeforeWrite are simply the composition of
//    the transition for read, followed by the transition for write (e.g.
//    ReadBeforeWrite moves from NONE state to READBEFOREWRITE state);
//    similarly, the state transitions for WriteBeforeRead are the composition
//    of the transition for write, followed by the transition for read.
//  * All other transitions (excluding ReadBeforeWrite and WriteBeforeRead)
//    that are not explicitly shown leave the state unchanged (e.g. a read in
//    the COMPLETE_WRITE state remains in the COMPLETE_WRITE state).
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

  // Partial write followed by read;
  // For the purpose of correct replay, this is equivalent to `Read`. However,
  // if this resource is inspected by the user before the write, the future
  // read could, incorrectly, be observed. This is because read-only resources
  // are not reset, so the write from the previous replay may still be present.
  eFrameRef_WriteBeforeRead = 5,

  // Similar to a CompleteWrite, but this is effectively an atomic "completely written, used, and
  // discarded". This encodes more information than CompleteWrite because it tells us that the
  // resource contents won't ever be used. For most purposes we treat this the same as
  // CompleteWrite, but below we use it to identify skippable resources.
  eFrameRef_CompleteWriteAndDiscard = 6,

  // No reference info is available;
  // This should only appear durring replay, and any (sub)resource with `Unknown`
  // reference type should be conservatively reset before each replay.
  eFrameRef_Unknown = 1000000000,
};

bool IncludesRead(FrameRefType refType);

bool IncludesWrite(FrameRefType refType);

const FrameRefType eFrameRef_Minimum = eFrameRef_None;
const FrameRefType eFrameRef_Maximum = eFrameRef_CompleteWriteAndDiscard;

// Threshold value for resource "age", i.e. how long it wasn't
// referred with the any write reference.
const double PERSISTENT_RESOURCE_AGE = 3000;
// how long since the first time it was skipped. If the skip state hasn't been invalidated in this
// long it can be skipped
const double SKIP_RESOURCE_AGE = 3000;

DECLARE_REFLECTION_ENUM(FrameRefType);

typedef FrameRefType (*FrameRefCompFunc)(FrameRefType, FrameRefType);

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

// Compose frame refs for disjoint subresources.
// This is used to compute the overall frame ref for images/memory from the
// frame refs of their subresources.
FrameRefType ComposeFrameRefsDisjoint(FrameRefType x, FrameRefType y);

// Returns whichever of `first` or `second` is valid.
FrameRefType ComposeFrameRefsFirstKnown(FrameRefType first, FrameRefType second);

// Dummy frame ref composition that always keeps the old ref.
FrameRefType KeepOldFrameRef(FrameRefType first, FrameRefType second);

bool IsDirtyFrameRef(FrameRefType refType);
bool IsCompleteWriteFrameRef(FrameRefType refType);

// Captures the possible initialization/reset requirements for resources.
// These requirements are entirely determined by the resource's FrameRefType,
// but this type improves the readability of the code that checks
// init/reset requirements.
enum InitReqType
{
  // No initialization required.
  eInitReq_None,

  // Initialize the resource by clearing.
  eInitReq_Clear,

  // Initialize the resource by copying initial data.
  eInitReq_Copy,
};

enum InitPolicy
{
  // Completely disable optimizations--copy initial data into every resource
  // before every replay.
  eInitPolicy_NoOpt,

  // CopyAll--conservative policy which ensures each subresource begins each
  // replay with the correct initial data.
  //
  // Initialization policy:
  //   Copy initial data into each subresource
  //
  // Reset policy:
  //   Copy initial data into each subresource which is written
  eInitPolicy_CopyAll,

  // ClearUnread--avoid copying initial data which is never read by the replay
  // commands. A user inspecting a resource before it is written may observe
  // cleared data, rather than the actual initial data.
  //
  // Initialization policy:
  //   Copy initial data into each subresource that is read.
  //   Clear each subresource that is not read.
  //
  // Reset policy:
  //   Copy initial data into each subresource where the initial data is read
  //   and then overwritten.
  //   Clear each subresource which is written, but whose initial data is not read.
  eInitPolicy_ClearUnread,

  // Fastest--Initialize/reset as little as possible for correct replay.
  // A user inspecting a resource before it is written may observe the data
  // from a future write (from the previous replay).
  //
  // Initialization policy:
  //   Copy initial data into each subresource that is read.
  //   Clear each subresource that is not read.
  //
  // Reset policy:
  //   Copy initial data into each subresource where the initial data is read
  //   and then overwritten.
  eInitPolicy_Fastest,
};

// Return the initialization/reset requirements for a FrameRefType
inline InitReqType InitReq(FrameRefType refType, InitPolicy policy, bool initialized)
{
  if(eFrameRef_Minimum > refType || refType > eFrameRef_Maximum)
    return eInitReq_Copy;
#define COPY_ONCE (initialized ? eInitReq_None : eInitReq_Copy)
#define CLEAR_ONCE (initialized ? eInitReq_None : eInitReq_Clear)
  switch(policy)
  {
    case eInitPolicy_NoOpt: return eInitReq_Copy;
    case eInitPolicy_CopyAll:
      switch(refType)
      {
        case eFrameRef_None: return COPY_ONCE;
        case eFrameRef_Read: return COPY_ONCE;
        default: return eInitReq_Copy;
      }
    case eInitPolicy_ClearUnread:
      switch(refType)
      {
        case eFrameRef_None: return CLEAR_ONCE;
        case eFrameRef_Read: return COPY_ONCE;
        case eFrameRef_ReadBeforeWrite: return eInitReq_Copy;
        case eFrameRef_WriteBeforeRead: return eInitReq_Copy;
        default: return eInitReq_Clear;
      }
    case eInitPolicy_Fastest:
      switch(refType)
      {
        case eFrameRef_None: return CLEAR_ONCE;
        case eFrameRef_Read: return COPY_ONCE;
        case eFrameRef_ReadBeforeWrite: return eInitReq_Copy;
        case eFrameRef_WriteBeforeRead: return COPY_ONCE;
        default: return CLEAR_ONCE;
      }
    default: RDCERR("Unknown initialization policy (%d).", policy); return eInitReq_Copy;
  }
#undef COPY_ONCE
#undef CLEAR_ONCE
}

// handle marking a resource referenced for read or write and storing RAW access etc.
template <typename Compose>
bool MarkReferenced(std::unordered_map<ResourceId, FrameRefType> &refs, ResourceId id,
                    FrameRefType refType, Compose comp)
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

inline bool MarkReferenced(std::unordered_map<ResourceId, FrameRefType> &refs, ResourceId id,
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

  void DisableChunkLocking() { SAFE_DELETE(m_ChunkLock); }
  ~ResourceRecord() { SAFE_DELETE(m_ChunkLock); }
  void AddParent(ResourceRecord *r)
  {
    if(r == this)
      return;

    if(Parents.indexOf(r) < 0)
    {
      r->AddRef();
      Parents.push_back(r);
    }
  }
  bool HasParent(ResourceRecord *r) const { return Parents.indexOf(r) >= 0; }
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
  void Insert(std::map<int64_t, Chunk *> &recordlist)
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
        recordlist[it->id] = it->chunk;
    }
  }

  void AddRef() { Atomic::Inc32(&RefCount); }
  int GetRefCount() const { return RefCount; }
  void Delete(ResourceRecordHandler *mgr);

  ResourceId GetResourceID() const { return ResID; }
  void AddChunk(Chunk *chunk, int64_t ID = 0)
  {
    if(ID == 0)
      ID = GetID();
    LockChunks();
    m_Chunks.push_back(StoredChunk(ID, chunk));
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
      AddChunk(it->chunk->Duplicate());

    for(auto it = other->Parents.begin(); it != other->Parents.end(); ++it)
      AddParent(*it);

    other->UnlockChunks();
    UnlockChunks();
  }

  void DeleteChunks()
  {
    LockChunks();
    for(auto it = m_Chunks.begin(); it != m_Chunks.end(); ++it)
      it->chunk->Delete(it->fromAllocator != 0);
    m_Chunks.clear();
    UnlockChunks();
  }

  Chunk *GetLastChunk() const
  {
    RDCASSERT(HasChunks());
    return m_Chunks.back().chunk;
  }

  int64_t GetLastChunkID() const
  {
    RDCASSERT(HasChunks());
    return m_Chunks.back().id;
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
  void AddReferencedIDs(std::unordered_set<ResourceId> &ids)
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
  int32_t RefCount;

  byte *DataPtr;
  uint64_t DataOffset;

  ResourceId ResID;

  rdcarray<ResourceRecord *> Parents;

  int64_t GetID()
  {
    static int64_t globalIDCounter = 10;

    return Atomic::Inc64(&globalIDCounter);
  }

  struct StoredChunk
  {
    StoredChunk(int64_t i, Chunk *c)
    {
      id = i;
      // we store this here because by the time it comes to delete the chunks the allocator may have
      // already been reset and the contents trashed.
      fromAllocator = c->IsFromAllocator() ? 1 : 0;
      chunk = c;
    }
    int64_t id : 63;
    int64_t fromAllocator : 1;
    Chunk *chunk;
  };

  rdcarray<StoredChunk> m_Chunks;
  Threading::CriticalSection *m_ChunkLock;

  std::unordered_map<ResourceId, FrameRefType> m_FrameRefs;
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

  ResourceManager(CaptureState &state);
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
  void SetInitialFileStore(ResourceId id, const rdcstr &filename, uint64_t start, uint64_t end);

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
  void MarkBackgroundFrameReferenced(const rdcflatmap<ResourceId, FrameRefType> &refs);
  void CleanBackgroundFrameReferences();

  ///////////////////////////////////////////
  // Replay-side methods

  // Live resources to replace serialised IDs
  void AddLiveResource(ResourceId origid, WrappedResourceType livePtr);
  bool HasLiveResource(ResourceId origid);
  WrappedResourceType GetLiveResource(ResourceId origid, bool optional = false);
  void EraseLiveResource(ResourceId origid);

  // when asked for a given id, return the resource for a replacement id
  void ReplaceResource(ResourceId from, ResourceId to);
  bool HasReplacement(ResourceId from);
  void RemoveReplacement(ResourceId id);

  // get the original ID for a real ID that may be a replacement. i.e. if ID 123 is ID 10000005
  // live, and 10000005 live is replaced with 10000839, then calling this function with either ID
  // 10000005 or ID 10000839 will return ID 123.
  ResourceId GetUnreplacedOriginalID(ResourceId id);

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

  void ResetLastWriteTimes();
  void ResetCaptureStartTime();

  bool HasPersistentAge(ResourceId id);
  bool HasSkippableAge(ResourceId id);
  bool IsResourcePostponed(ResourceId id);
  bool IsResourceSkipped(ResourceId id);
  bool ShouldPostpone(ResourceId id);
  bool ShouldSkip(ResourceId id);

  virtual bool IsResourceTrackedForPersistency(const WrappedResourceType &res) { return false; }
protected:
  friend InitialContentData;
  // 'interface' to implement by derived classes
  virtual ResourceId GetID(WrappedResourceType res) = 0;

  virtual bool ResourceTypeRelease(WrappedResourceType res) = 0;

  virtual bool Need_InitialStateChunk(ResourceId id, const InitialContentData &initial)
  {
    return true;
  }
  virtual void Begin_PrepareInitialBatch() {}
  virtual bool Prepare_InitialState(WrappedResourceType res) = 0;
  virtual void End_PrepareInitialBatch() {}
  virtual uint64_t GetSize_InitialState(ResourceId id, const InitialContentData &initial) = 0;
  virtual bool Serialise_InitialState(WriteSerialiser &ser, ResourceId id, RecordType *record,
                                      const InitialContentData *initialData) = 0;
  virtual void Create_InitialState(ResourceId id, WrappedResourceType live, bool hasData) = 0;
  virtual void Apply_InitialState(WrappedResourceType live, const InitialContentData &initial) = 0;
  virtual rdcarray<ResourceId> InitialContentResources();

  void UpdateLastWriteTime(ResourceId id, FrameRefType refType);

  void Prepare_InitialStateIfPostponed(ResourceId id, bool midframe);
  void SkipOrPostponeOrPrepare_InitialState(ResourceId id, FrameRefType refType);

  // very coarse lock, protects EVERYTHING. This could certainly be improved and it may be a
  // bottleneck for performance. Given that the main use cases are write-rarely read-often the lock
  // should be optimised for that as we only want to make sure we're not modifying the objects
  // together, by far the most common operation is looking up data.
  Threading::CriticalSection m_Lock;

  // we only need to lock during capturing, on replay we have single threaded access.
  bool m_Capturing;

  // easy optimisation win - don't use maps everywhere. It's convenient but not optimal, and
  // profiling will likely prove that some or all of these could be a problem

  // used during capture - map from real resource to its wrapper (other way can be done just with an
  // Unwrap)
  std::map<RealResourceType, WrappedResourceType> m_WrapperMap;

  // used during capture - holds resources referenced in current frame (and how they're referenced)
  std::unordered_map<ResourceId, FrameRefType> m_FrameReferencedResources;

  // used during capture - holds resources marked as dirty, needing initial contents
  std::set<ResourceId> m_DirtyResources;

  struct InitialContentStorage
  {
    Chunk *chunk = NULL;
    InitialContentData data;
    rdcstr filename;
    uint64_t fileStart = 0, fileEnd = 0;

    void Free(ResourceManager *mgr)
    {
      if(chunk)
      {
        chunk->Delete();
        chunk = NULL;
      }

      data.Free(mgr);
    }
  };

  // used during capture or replay - holds initial contents
  std::unordered_map<ResourceId, InitialContentStorage> m_InitialContents;

  // used during capture or replay - map of resources currently alive with their real IDs, used in
  // capture and replay.
  std::unordered_map<ResourceId, WrappedResourceType> m_CurrentResourceMap;

  // used during replay - maps back and forth from original id to live id and vice-versa
  std::unordered_map<ResourceId, ResourceId> m_OriginalIDs, m_LiveIDs;

  // used during replay - holds resources allocated and the original id that they represent
  std::unordered_map<ResourceId, WrappedResourceType> m_LiveResourceMap;

  // used during capture - holds resource records by id.
  std::unordered_map<ResourceId, RecordType *> m_ResourceRecords;
  Threading::RWLock m_ResourceRecordLock;

  // used during replay - holds current resource replacements
  // replaced -> replacement
  std::unordered_map<ResourceId, ResourceId> m_Replacements;
  // replacement -> replaced (for looking up original IDs)
  std::unordered_map<ResourceId, ResourceId> m_Replaced;

  // During initial resources preparation, persistent resources are
  // postponed until serializing to RDC file.
  std::unordered_set<ResourceId> m_PostponedResourceIDs;
  // During initial resources preparation, resources that are completely written
  // over are skipped
  std::unordered_set<ResourceId> m_SkippedResourceIDs;

  struct ResourceRefTimes
  {
    ResourceId id;

    // On marking resource write-referenced in frame, its last write time is reset. The time is used
    // to determine persistent resources, and is checked against the `PERSISTENT_RESOURCE_AGE`.
    double writeTime;

    // firstSkipTime is referring to the first time we saw a resource get marked as skippable.
    // Any write will reset this time to 0.0 (not skippable), so if the firstSkipTime is longer than
    // `SKIP_RESOURCE_AGE` ago then we know the resource has only ever been skipped (or not written)
    // for that long. If the time is 0.0 then it's been written in a more visible way recently or
    // has never been marked skippable.
    double firstSkipTime;

    bool operator<(const ResourceId &o) const { return id < o; }
  };

  // all resources that are written in some way end up in this list. We then check the last time
  // they were written, and the last time they were ever partially used (not completely overwritten
  // in one atomic chunk).
  rdcarray<ResourceRefTimes> m_ResourceRefTimes;

  // Timestamp at the beginning of the frame capture. Used to determine which
  // resources to refresh for their last write or partial use time (see `ResourceRefTimes`).
  double m_captureStartTime;

  PerformanceTimer m_ResourcesUpdateTimer;

  // The capture state is propagated by a specific driver.
  CaptureState &m_State;
};

template <typename Configuration>
ResourceManager<Configuration>::ResourceManager(CaptureState &state) : m_State(state)
{
  m_Capturing = IsCaptureMode(state);
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(ResourceManager));
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

  RenderDoc::Inst().UnregisterMemoryRegion(this);
}

template <typename Configuration>
void ResourceManager<Configuration>::MarkBackgroundFrameReferenced(
    const rdcflatmap<ResourceId, FrameRefType> &refs)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(IsBackgroundCapturing(m_State))
  {
    if(refs.size() <= m_ResourceRefTimes.size())
    {
      for(auto it = refs.begin(); it != refs.end(); ++it)
        UpdateLastWriteTime(it->first, it->second);
    }
    else
    {
      for(const ResourceRefTimes &res : m_ResourceRefTimes)
      {
        auto it = refs.find(res.id);

        if(it != refs.end())
          UpdateLastWriteTime(it->first, it->second);
      }
    }
  }
}

template <typename Configuration>
void ResourceManager<Configuration>::CleanBackgroundFrameReferences()
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(IsBackgroundCapturing(m_State))
  {
    double now = m_ResourcesUpdateTimer.GetMilliseconds();

    // retire any old entries, if they were written once they shouldn't be tracked forever. This
    // means the list only keeps track of recently written resources, not all resources that have
    // ever been written.
    //
    // walk the array. If we find an item we want to remove we increment src otherwise we copy src
    // to dst (if they are different). Thus next time dst will point at the item we want to skip, at
    // each stage copying src to dst (if they are different). If we find an item we want to remove
    // we increment src and continue (thus it will get copied ove
    size_t dst = 0, src = 0;
    for(dst = 0, src = 0; src < m_ResourceRefTimes.size();)
    {
      ResourceRefTimes &check = m_ResourceRefTimes[src];

      // if this isn't skippable, and the write time was a long time ago then we can delete it.
      // Resources not in the list are treated as if they were written an infinite time ago and so
      // are postponable.
      if(now - check.writeTime > PERSISTENT_RESOURCE_AGE && check.firstSkipTime == 0.0)
      {
        // skip src, check the next one
        src++;
        continue;
      }

      // we want to keep src. If dst == src we can just continue on to the next iteration, if not
      // then we need to copy src into dst (where dst is the leftovers from previously remove
      // entries)
      if(dst != src)
        m_ResourceRefTimes[dst] = m_ResourceRefTimes[src];

      dst++;
      src++;
    }

    m_ResourceRefTimes.resize(dst);
  }
}

template <typename Configuration>
template <typename Compose>
void ResourceManager<Configuration>::MarkResourceFrameReferenced(ResourceId id,
                                                                 FrameRefType refType, Compose comp)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(id == ResourceId())
    return;

  if(IsActiveCapturing(m_State))
  {
    SkipOrPostponeOrPrepare_InitialState(id, refType);

    if(IsDirtyFrameRef(refType))
      Prepare_InitialStateIfPostponed(id, true);
  }

  UpdateLastWriteTime(id, refType);

  if(IsBackgroundCapturing(m_State))
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
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(res == ResourceId())
    return;

  m_DirtyResources.insert(res);
}

template <typename Configuration>
bool ResourceManager<Configuration>::IsResourceDirty(ResourceId res)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(res == ResourceId())
    return false;

  return m_DirtyResources.find(res) != m_DirtyResources.end();
}

template <typename Configuration>
void ResourceManager<Configuration>::SetInitialContents(ResourceId id, InitialContentData contents)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  RDCASSERT(id != ResourceId());

  auto it = m_InitialContents.find(id);

  if(it != m_InitialContents.end())
    it->second.Free(this);

  m_InitialContents[id].data = contents;
}

template <typename Configuration>
void ResourceManager<Configuration>::SetInitialChunk(ResourceId id, Chunk *chunk)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  RDCASSERT(id != ResourceId());
  RDCASSERT(chunk->GetChunkType<SystemChunk>() == SystemChunk::InitialContents);

  InitialContentStorage &data = m_InitialContents[id];

  if(data.chunk)
    data.chunk->Delete();

  data.chunk = chunk;
}

template <typename Configuration>
void ResourceManager<Configuration>::SetInitialFileStore(ResourceId id, const rdcstr &filename,
                                                         uint64_t start, uint64_t end)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  RDCASSERT(id != ResourceId());
  RDCASSERT(!filename.empty());
  RDCASSERT(end > start);

  InitialContentStorage &data = m_InitialContents[id];

  data.filename = filename;
  data.fileStart = start;
  data.fileEnd = end;
}

template <typename Configuration>
typename Configuration::InitialContentData ResourceManager<Configuration>::GetInitialContents(
    ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

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

  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  // which resources need initial contents? either those which we didn't have proper initialisation
  // for (because they were dirty one way or another) or resources that are written mid-frame and so
  // need to be reset.
  rdcarray<WrittenRecord> NeededInitials;

  // reasonable estimate, and these records are small
  NeededInitials.reserve(m_FrameReferencedResources.size() + m_InitialContents.size());

  // all resources that were recorded as being modified should be included in the list of those
  // needing initial contents
  for(auto it = m_FrameReferencedResources.begin(); it != m_FrameReferencedResources.end(); ++it)
  {
    RecordType *record = GetResourceRecord(it->first);
    if(IsDirtyFrameRef(it->second))
    {
      WrittenRecord wr = {it->first, record ? record->DataInSerialiser : true};

      NeededInitials.push_back(wr);
    }
  }

  // any resources that had initial contents generated should also be included, even if they're only
  // referenced read-only, as anything not in this list will have its initial contents freed on
  // replay (see CreateInitialContents). However we only need to keep resources that are referenced
  // (unless we have ref all resources on)
  for(auto it = m_InitialContents.begin(); it != m_InitialContents.end(); ++it)
  {
    bool include = RenderDoc::Inst().GetCaptureOptions().refAllResources;

    ResourceId id = it->first;
    if(m_FrameReferencedResources.find(id) != m_FrameReferencedResources.end())
      include = true;

    if(include)
    {
      WrittenRecord wr = {id, true};

      NeededInitials.push_back(wr);
    }
  }

  uint64_t chunkSize = uint64_t(NeededInitials.size() * sizeof(WrittenRecord) + 16);

  SCOPED_SERIALISE_CHUNK(SystemChunk::InitialContentsList, chunkSize);
  SERIALISE_ELEMENT(NeededInitials);
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
  m_PostponedResourceIDs.clear();
  m_SkippedResourceIDs.clear();
}

template <typename Configuration>
void ResourceManager<Configuration>::Prepare_InitialStateIfPostponed(ResourceId id, bool midframe)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(!IsResourcePostponed(id))
    return;

  if(midframe)
  {
    RDCLOG("Preparing resource %s after it has been postponed.", ToStr(id).c_str());
    Begin_PrepareInitialBatch();
  }

  WrappedResourceType res = GetCurrentResource(id);
  Prepare_InitialState(res);

  if(midframe)
    End_PrepareInitialBatch();

  m_PostponedResourceIDs.erase(id);
}

template <typename Configuration>
void ResourceManager<Configuration>::SkipOrPostponeOrPrepare_InitialState(ResourceId id,
                                                                          FrameRefType refType)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(!IsResourceSkipped(id))
    return;

  // the first time we encounter a skipped resource, we can choose to
  // skip this resource forever, convert it to be postponed, or prepare
  // it immediately. We can't retrieve its initial state once it has
  // been written over.
  m_SkippedResourceIDs.erase(id);

  // skip this forever if the first encounter is a complete write
  if(IsCompleteWriteFrameRef(refType))
  {
    RDCDEBUG("Resource %s skipped forever on refType of %s", ToStr(id).c_str(),
             ToStr(refType).c_str());
    return;
  }

  // If this resource is only being read, we might as well try to
  // postpone it to conserve memory consumption.
  if(!IsDirtyFrameRef(refType) && IsResourceTrackedForPersistency(GetCurrentResource(id)))
  {
    m_PostponedResourceIDs.insert(id);
    RDCDEBUG("Resource %s converted from skipped to postponed on refType of %s", ToStr(id).c_str(),
             ToStr(refType).c_str());
    SetInitialContents(id, InitialContentData());
  }
  else
  {
    WrappedResourceType res = GetCurrentResource(id);
    RDCDEBUG("Preparing resource %s after it has been skipped on refType of %s", ToStr(id).c_str(),
             ToStr(refType).c_str());
    Begin_PrepareInitialBatch();
    Prepare_InitialState(res);
    End_PrepareInitialBatch();
  }
}

template <typename Configuration>
inline void ResourceManager<Configuration>::ResetCaptureStartTime()
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);
  // This time is used to analyze which resources to refresh
  // for their last write time.
  m_captureStartTime = m_ResourcesUpdateTimer.GetMilliseconds();
}

template <typename Configuration>
inline void ResourceManager<Configuration>::ResetLastWriteTimes()
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);
  for(auto it = m_ResourceRefTimes.begin(); it != m_ResourceRefTimes.end(); ++it)
  {
    // Reset only those resources which were below the threshold on
    // capture start. Other resource are already above the threshold.
    if(m_captureStartTime - it->writeTime <= PERSISTENT_RESOURCE_AGE)
      it->writeTime = m_ResourcesUpdateTimer.GetMilliseconds();
  }
}

template <typename Configuration>
inline void ResourceManager<Configuration>::UpdateLastWriteTime(ResourceId id, FrameRefType refType)
{
  // parent must hold m_Lock for us

  // only care about write refs. A read ref would invalidate skippable state, however a skippable
  // resource is left in an undefined state where reads are not valid so we don't. We need to see
  // another write first before a read could be a problem, so we just pay attention for that write.
  if(!IsDirtyFrameRef(refType))
    return;

  ResourceRefTimes *it = std::lower_bound(m_ResourceRefTimes.begin(), m_ResourceRefTimes.end(), id);

  if(it == m_ResourceRefTimes.end() || it->id != id)
  {
    // if it's not pointing to the end, figure out where we need to insert it there
    size_t idx = it - m_ResourceRefTimes.begin();
    m_ResourceRefTimes.insert(idx, {id, 0.0, 0.0});
    it = m_ResourceRefTimes.begin() + idx;
  }

  double now = m_ResourcesUpdateTimer.GetMilliseconds();

  it->writeTime = now;

  if(refType == eFrameRef_CompleteWriteAndDiscard)
  {
    // don't continually update it. We want to know that this resource *was* completely written and
    // discarded, and hasn't been written in any other way since then.
    if(it->firstSkipTime == 0.0)
      it->firstSkipTime = now;
  }
  else
  {
    it->firstSkipTime = 0.0;
  }
}

template <typename Configuration>
inline bool ResourceManager<Configuration>::HasPersistentAge(ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  ResourceRefTimes *it = std::lower_bound(m_ResourceRefTimes.begin(), m_ResourceRefTimes.end(), id);

  if(it == m_ResourceRefTimes.end() || it->id != id)
    return true;

  return m_ResourcesUpdateTimer.GetMilliseconds() - it->writeTime >= PERSISTENT_RESOURCE_AGE;
}

template <typename Configuration>
inline bool ResourceManager<Configuration>::HasSkippableAge(ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  ResourceRefTimes *it = std::lower_bound(m_ResourceRefTimes.begin(), m_ResourceRefTimes.end(), id);

  // if it doesn't have a write time, it can't be skippable
  if(it == m_ResourceRefTimes.end() || it->id != id)
    return false;

  // if it's never been skipped or it was reset, it's also not skippable
  if(it->firstSkipTime == 0.0)
    return false;

  return m_ResourcesUpdateTimer.GetMilliseconds() - it->firstSkipTime >= SKIP_RESOURCE_AGE;
}

template <typename Configuration>
inline bool ResourceManager<Configuration>::IsResourcePostponed(ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);
  return m_PostponedResourceIDs.find(id) != m_PostponedResourceIDs.end();
}

template <typename Configuration>
inline bool ResourceManager<Configuration>::IsResourceSkipped(ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);
  return m_SkippedResourceIDs.find(id) != m_SkippedResourceIDs.end();
}

template <typename Configuration>
inline bool ResourceManager<Configuration>::ShouldPostpone(ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  WrappedResourceType res = GetCurrentResource(id);

  if(!IsResourceTrackedForPersistency(res))
    return false;

  return HasPersistentAge(id);
}

template <typename Configuration>
inline bool ResourceManager<Configuration>::ShouldSkip(ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  WrappedResourceType res = GetCurrentResource(id);

  if(!IsResourceTrackedForPersistency(res))
    return false;

  return HasSkippableAge(id);
}

template <typename Configuration>
void ResourceManager<Configuration>::CreateInitialContents(ReadSerialiser &ser)
{
  using namespace ResourceManagerInternal;

  std::unordered_set<ResourceId> ids;

  rdcarray<WrittenRecord> NeededInitials;
  SERIALISE_ELEMENT(NeededInitials);

  for(const WrittenRecord &wr : NeededInitials)
  {
    ResourceId id = wr.id;

    ids.insert(id);

    // if this resource exists and we don't have initial contents for it serialised, create some for
    // reset purposes.
    if(HasLiveResource(id) && m_InitialContents.find(id) == m_InitialContents.end())
      Create_InitialState(id, GetLiveResource(id), wr.written);
  }

  // any initial contents that we ended up with which we don't need can be freed now
  for(auto it = m_InitialContents.begin(); it != m_InitialContents.end();)
  {
    ResourceId id = it->first;

    if(ids.find(id) == ids.end())
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
  rdcarray<ResourceId> resources = InitialContentResources();
  for(auto it = resources.begin(); it != resources.end(); ++it)
  {
    ResourceId id = *it;
    const InitialContentStorage &data = m_InitialContents[id];
    WrappedResourceType live = GetLiveResource(id);
    Apply_InitialState(live, data.data);
  }
  RDCDEBUG("Applied %d", (uint32_t)resources.size());
}

template <typename Configuration>
rdcarray<ResourceId> ResourceManager<Configuration>::InitialContentResources()
{
  rdcarray<ResourceId> resources;
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
  SCOPED_READLOCK(m_ResourceRecordLock);

  for(auto it = m_ResourceRecords.begin(); it != m_ResourceRecords.end(); ++it)
    it->second->MarkDataUnwritten();
}

template <typename Configuration>
void ResourceManager<Configuration>::InsertReferencedChunks(WriteSerialiser &ser)
{
  std::map<int64_t, Chunk *> sortedChunks;

  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  RDCDEBUG("%u frame resource records", (uint32_t)m_FrameReferencedResources.size());

  if(RenderDoc::Inst().GetCaptureOptions().refAllResources)
  {
    SCOPED_READLOCK(m_ResourceRecordLock);

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
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  RDCLOG("Preparing up to %u potentially dirty resources", (uint32_t)m_DirtyResources.size());
  uint32_t prepared = 0;
  uint32_t postponed = 0;
  uint32_t skipped = 0;

  float num = float(m_DirtyResources.size());
  float idx = 0.0f;

  Begin_PrepareInitialBatch();

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

    if(ShouldSkip(id))
    {
      m_SkippedResourceIDs.insert(id);
      skipped++;
      continue;
    }

    if(ShouldPostpone(id))
    {
      m_PostponedResourceIDs.insert(id);
      // Set empty contents here, it'll be prepared on serialization.
      SetInitialContents(id, InitialContentData());
      postponed++;
      continue;
    }

    prepared++;

#if ENABLED(VERBOSE_DIRTY_RESOURCES)
    RDCDEBUG("Prepare Resource %s", ToStr(id).c_str());
#endif

    Prepare_InitialState(res);
  }

  End_PrepareInitialBatch();

  RDCLOG("Prepared %u dirty resources, postponed %u, skipped %u", prepared, postponed, skipped);
}

template <typename Configuration>
void ResourceManager<Configuration>::InsertInitialContentsChunks(WriteSerialiser &ser)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  uint32_t dirty = 0;
  uint32_t skipped = 0;

  RDCLOG("Checking %u resources with initial contents against %u referenced resources",
         (uint32_t)m_InitialContents.size(), (uint32_t)m_FrameReferencedResources.size());

  float num = float(m_InitialContents.size());
  float idx = 0.0f;

  Begin_PrepareInitialBatch();

  for(auto it = m_InitialContents.begin(); it != m_InitialContents.end(); ++it)
  {
    ResourceId id = it->first;

    if(m_FrameReferencedResources.find(id) == m_FrameReferencedResources.end() &&
       !RenderDoc::Inst().GetCaptureOptions().refAllResources)
    {
      continue;
    }

    RecordType *record = GetResourceRecord(id);

    if(record == NULL)
      continue;

    if(record->InternalResource)
      continue;

    // Load postponed resource if needed.
    Prepare_InitialStateIfPostponed(id, false);
  }

  End_PrepareInitialBatch();

  for(auto it = m_InitialContents.begin(); it != m_InitialContents.end(); ++it)
  {
    ResourceId id = it->first;

    RenderDoc::Inst().SetProgress(CaptureProgress::SerialiseInitialStates, idx / num);
    idx += 1.0f;

    if(m_FrameReferencedResources.find(id) == m_FrameReferencedResources.end() &&
       !RenderDoc::Inst().GetCaptureOptions().refAllResources)
    {
#if ENABLED(VERBOSE_DIRTY_RESOURCES)
      RDCDEBUG("Dirty resource %s is GPU dirty but not referenced - skipping", ToStr(id).c_str());
#endif
      skipped++;
      continue;
    }

    RecordType *record = GetResourceRecord(id);

    if(record == NULL)
    {
#if ENABLED(VERBOSE_DIRTY_RESOURCES)
      RDCDEBUG("Resource %s has no resource record - skipping", ToStr(id).c_str());
#endif
      continue;
    }

    if(record->InternalResource)
    {
#if ENABLED(VERBOSE_DIRTY_RESOURCES)
      RDCDEBUG("Resource %s is special - skipping", ToStr(id).c_str());
#endif
      continue;
    }

#if ENABLED(VERBOSE_DIRTY_RESOURCES)
    RDCDEBUG("Serialising dirty Resource %s", ToStr(id).c_str());
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
    else if(!it->second.filename.empty())
    {
      FILE *f = FileIO::fopen(it->second.filename, FileIO::ReadBinary);
      FileIO::fseek64(f, it->second.fileStart, SEEK_SET);
      StreamReader reader(f, it->second.fileEnd - it->second.fileStart, Ownership::Stream);

      StreamTransfer(ser.GetWriter(), &reader, NULL);
    }
    else
    {
      uint64_t size = GetSize_InitialState(id, it->second.data);

      SCOPED_SERIALISE_CHUNK(SystemChunk::InitialContents, size);

      Serialise_InitialState(ser, id, record, &it->second.data);
    }

    // Reset back to empty contents, unloading the actual resource.
    SetInitialContents(id, InitialContentData());
  }

  RDCLOG("Serialised %u resources, skipped %u unreferenced", dirty, skipped);
}

template <typename Configuration>
void ResourceManager<Configuration>::ApplyInitialContentsNonChunks(WriteSerialiser &ser)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

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
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  for(auto it = m_FrameReferencedResources.begin(); it != m_FrameReferencedResources.end(); ++it)
  {
    RecordType *record = GetResourceRecord(it->first);

    if(record)
    {
      if(IncludesWrite(it->second))
        MarkDirtyResource(it->first);
      record->Delete(this);
    }
  }

  m_FrameReferencedResources.clear();
}

template <typename Configuration>
void ResourceManager<Configuration>::ReplaceResource(ResourceId from, ResourceId to)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(HasLiveResource(to))
  {
    m_Replacements[from] = to;
    m_Replaced[to] = from;
  }
}

template <typename Configuration>
bool ResourceManager<Configuration>::HasReplacement(ResourceId from)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  return m_Replacements.find(from) != m_Replacements.end();
}

template <typename Configuration>
void ResourceManager<Configuration>::RemoveReplacement(ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  auto it = m_Replacements.find(id);

  if(it == m_Replacements.end())
    return;

  m_Replaced.erase(it->second);
  m_Replacements.erase(it);
}

template <typename Configuration>
typename Configuration::RecordType *ResourceManager<Configuration>::GetResourceRecord(ResourceId id)
{
  SCOPED_READLOCK(m_ResourceRecordLock);

  auto it = m_ResourceRecords.find(id);

  if(it == m_ResourceRecords.end())
    return NULL;

  return it->second;
}

template <typename Configuration>
bool ResourceManager<Configuration>::HasResourceRecord(ResourceId id)
{
  SCOPED_READLOCK(m_ResourceRecordLock);

  auto it = m_ResourceRecords.find(id);

  if(it == m_ResourceRecords.end())
    return false;

  return true;
}

template <typename Configuration>
typename Configuration::RecordType *ResourceManager<Configuration>::AddResourceRecord(ResourceId id)
{
  SCOPED_WRITELOCK(m_ResourceRecordLock);

  RDCASSERT(m_ResourceRecords.find(id) == m_ResourceRecords.end(), id);

  return (m_ResourceRecords[id] = new RecordType(id));
}

template <typename Configuration>
void ResourceManager<Configuration>::RemoveResourceRecord(ResourceId id)
{
  SCOPED_WRITELOCK(m_ResourceRecordLock);

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
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

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
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

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
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(real == (RealResourceType)RecordType::NullResource)
    return false;

  return (m_WrapperMap.find(real) != m_WrapperMap.end());
}

template <typename Configuration>
typename Configuration::WrappedResourceType ResourceManager<Configuration>::GetWrapper(
    RealResourceType real)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

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
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(origid == ResourceId() || livePtr == (WrappedResourceType)RecordType::NullResource)
  {
    RDCERR("Invalid state adding resource mapping - id is invalid or live pointer is NULL");
  }

  m_OriginalIDs[GetID(livePtr)] = origid;
  m_LiveIDs[origid] = GetID(livePtr);

  if(m_LiveResourceMap.find(origid) != m_LiveResourceMap.end())
  {
    RDCERR("Releasing live resource for duplicate creation: %s", ToStr(origid).c_str());
    ResourceTypeRelease(m_LiveResourceMap[origid]);
    m_LiveResourceMap.erase(origid);
  }

  m_LiveResourceMap[origid] = livePtr;
}

template <typename Configuration>
bool ResourceManager<Configuration>::HasLiveResource(ResourceId origid)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(origid == ResourceId())
    return false;

  return (m_Replacements.find(origid) != m_Replacements.end() ||
          m_LiveResourceMap.find(origid) != m_LiveResourceMap.end());
}

template <typename Configuration>
typename Configuration::WrappedResourceType ResourceManager<Configuration>::GetLiveResource(
    ResourceId origid, bool optional)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(origid == ResourceId())
    return (WrappedResourceType)RecordType::NullResource;

#if DISABLED(RDOC_RELEASE)
  if(!optional)
  {
    RDCASSERT(HasLiveResource(origid), origid);
  }
#endif

  {
    auto it = m_Replacements.find(origid);
    if(it != m_Replacements.end())
      return GetLiveResource(it->second);
  }

  {
    auto it = m_LiveResourceMap.find(origid);
    if(it != m_LiveResourceMap.end())
      return it->second;
  }

  return (WrappedResourceType)RecordType::NullResource;
}

template <typename Configuration>
void ResourceManager<Configuration>::EraseLiveResource(ResourceId origid)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  RDCASSERT(HasLiveResource(origid), origid);

  m_LiveResourceMap.erase(origid);
}

template <typename Configuration>
void ResourceManager<Configuration>::AddCurrentResource(ResourceId id, WrappedResourceType res)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  m_CurrentResourceMap[id] = res;
}

template <typename Configuration>
bool ResourceManager<Configuration>::HasCurrentResource(ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  return m_CurrentResourceMap.find(id) != m_CurrentResourceMap.end();
}

template <typename Configuration>
typename Configuration::WrappedResourceType ResourceManager<Configuration>::GetCurrentResource(
    ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(id == ResourceId())
    return (WrappedResourceType)RecordType::NullResource;

  if(m_Replacements.find(id) != m_Replacements.end())
    return GetCurrentResource(m_Replacements[id]);

  return m_CurrentResourceMap[id];
}

template <typename Configuration>
void ResourceManager<Configuration>::ReleaseCurrentResource(ResourceId id)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  // We potentially need to prepare this resource on Active Capture,
  // if it was postponed, but is about to go away.
  if(IsActiveCapturing(m_State))
    Prepare_InitialStateIfPostponed(id, true);

  m_CurrentResourceMap.erase(id);
  m_DirtyResources.erase(id);

  auto it = std::lower_bound(m_ResourceRefTimes.begin(), m_ResourceRefTimes.end(), id);
  if(it != m_ResourceRefTimes.end())
    m_ResourceRefTimes.erase(it - m_ResourceRefTimes.begin());
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
ResourceId ResourceManager<Configuration>::GetUnreplacedOriginalID(ResourceId id)
{
  if(id == ResourceId())
    return id;

  if(m_Replaced.find(id) != m_Replaced.end())
    return m_Replaced[id];

  RDCASSERT(m_OriginalIDs.find(id) != m_OriginalIDs.end(), id);
  return m_OriginalIDs[id];
}

template <typename Configuration>
ResourceId ResourceManager<Configuration>::GetLiveID(ResourceId id)
{
  if(id == ResourceId())
    return id;

  auto it = m_Replacements.find(id);
  if(it != m_Replacements.end())
    return it->second;

  RDCASSERT(m_LiveIDs.find(id) != m_LiveIDs.end(), id);
  return m_LiveIDs[id];
}
