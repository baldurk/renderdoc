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

#pragma once

#include "core/resource_manager.h"
#include "gl_initstate.h"
#include "gl_resources.h"

class WrappedOpenGL;

struct GLResourceManagerConfiguration
{
  typedef GLResource WrappedResourceType;
  typedef GLResource RealResourceType;
  typedef GLResourceRecord RecordType;
  typedef GLInitialContents InitialContentData;
};

class GLResourceManager : public ResourceManager<GLResourceManagerConfiguration>
{
public:
  GLResourceManager(CaptureState &state, WrappedOpenGL *driver);
  ~GLResourceManager() {}
  void Shutdown()
  {
    // there's a bit of a dependency issue here. We're essentially forcibly deleting/garbage
    // collecting
    // all the resource records. In some cases, we might have a parent->child type relationship
    // where
    // the only reference on the parent is from the child. If we delete the parent before the child,
    // when the child destructs and tries to delete the parent things will go badly.
    // We want to avoid keeping a reference ourselves on all records since that would cause
    // difficulties
    // during normal lifetime, so instead we just ask all records to release their references on
    // parents
    // before deleting them.

    // special care is taken since the act of freeing parents will by design potentially modify the
    // container. Since this is shutdown, we take the simple & naive approach of simply restarting
    // the
    // loop whenever we detect the size changing. FreeParents() is a safe operation to perform on
    // records
    // that have already freed their parents.
    {
      auto it = m_CurrentResources.begin();

      for(size_t i = 0; it != m_CurrentResources.end();)
      {
        size_t prevSize = m_CurrentResources.size();
        if(it->second.second)
          it->second.second->FreeParents(this);

        // collection modified, restart loop
        if(prevSize != m_CurrentResources.size())
        {
          i = 0;
          it = m_CurrentResources.begin();
          continue;
        }

        // collection not modified, continue
        i++;
        it++;
      }
    }

    while(!m_CurrentResources.empty())
    {
      auto it = m_CurrentResources.end();
      --it;
      ResourceId id = it->second.first;
      if(it->second.second)
        it->second.second->Delete(this);

      if(!m_CurrentResources.empty())
      {
        auto last = m_CurrentResources.end();
        last--;
        if(last->second.first == id)
          m_CurrentResources.erase(last);
      }
    }

    m_CurrentResources.clear();

    ResourceManager::Shutdown();
  }

  void DeleteContext(void *context)
  {
    size_t count = 0;
    for(auto it = m_CurrentResources.begin(); it != m_CurrentResources.end();)
    {
      if(it->first.ContextShareGroup == context && it->first.Namespace != eResSpecial)
      {
        ++count;
        if(it->second.second)
          it->second.second->Delete(this);
        ReleaseCurrentResource(it->second.first);
        m_CurrentResources.erase(it);
      }
      else
      {
        ++it;
      }
    }
    RDCDEBUG("Removed %zu/%zu resources belonging to context/sharegroup %p", count,
             m_CurrentResources.size(), context);
  }

  inline void RemoveResourceRecord(ResourceId id)
  {
    GLResourceRecord *record = ResourceManager::GetResourceRecord(id);

    if(record)
    {
      auto it = m_CurrentResources.find(record->Resource);
      if(it != m_CurrentResources.end() && it->second.first == id)
        it->second.second = NULL;
    }

    ResourceManager::RemoveResourceRecord(id);
  }

  ResourceId RegisterResource(GLResource res, ResourceId id = ResourceId())
  {
    if(id == ResourceId())
      id = ResourceIDGen::GetNewUniqueID();
    m_CurrentResources[res].first = id;
    AddCurrentResource(id, res);
    return id;
  }

  using ResourceManager::HasCurrentResource;

  bool HasCurrentResource(GLResource res)
  {
    auto it = m_CurrentResources.find(res);
    if(it != m_CurrentResources.end())
      return true;

    return false;
  }

  void UnregisterResource(GLResource res)
  {
    auto it = m_CurrentResources.find(res);
    if(it != m_CurrentResources.end())
    {
      ResourceId id = it->second.first;
      m_Names.erase(id);

      if(IsReplayMode(m_State) && HasLiveResource(id))
        EraseLiveResource(id);
      ReleaseCurrentResource(id);
      m_CurrentResources.erase(res);

      auto fboit = m_FBOAttachmentsCache.find(id);
      if(fboit != m_FBOAttachmentsCache.end())
      {
        delete fboit->second;
        m_FBOAttachmentsCache.erase(fboit);
      }
    }
  }

  ResourceId GetResID(GLResource res)
  {
    auto it = m_CurrentResources.find(res);
    if(it != m_CurrentResources.end())
      return it->second.first;
    return ResourceId();
  }

  GLResourceRecord *AddResourceRecord(ResourceId id)
  {
    GLResourceRecord *ret = ResourceManager::AddResourceRecord(id);
    GLResource res = GetCurrentResource(id);

    m_CurrentResources[res].second = ret;
    ret->Resource = res;

    return ret;
  }

  using ResourceManager::HasResourceRecord;

  bool HasResourceRecord(GLResource res) { return ResourceManager::HasResourceRecord(GetID(res)); }
  using ResourceManager::GetResourceRecord;

  GLResourceRecord *GetResourceRecord(GLResource res)
  {
    auto it = m_CurrentResources.find(res);
    if(it != m_CurrentResources.end())
      return it->second.second;

    return ResourceManager::GetResourceRecord(GetID(res));
  }

  using ResourceManager::MarkResourceFrameReferenced;

  void MarkResourceFrameReferenced(ResourceId id, FrameRefType refType)
  {
    GLResourceRecord *record = GetResourceRecord(id);
    if(record && record->viewSource != ResourceId())
      ResourceManager::MarkResourceFrameReferenced(record->viewSource, refType);

    ResourceManager::MarkResourceFrameReferenced(id, refType);
  }

  void MarkResourceFrameReferenced(GLResourceRecord *record, FrameRefType refType)
  {
    if(record && record->viewSource != ResourceId())
      ResourceManager::MarkResourceFrameReferenced(record->viewSource, refType);

    ResourceManager::MarkResourceFrameReferenced(record->GetResourceID(), refType);
  }

  void MarkResourceFrameReferenced(GLResource res, FrameRefType refType)
  {
    // we allow VAO 0 as a special case
    if(res.name == 0 && res.Namespace != eResVertexArray)
      return;

    rdcpair<ResourceId, GLResourceRecord *> &it = m_CurrentResources[res];

    if(it.second && it.second->viewSource != ResourceId())
      ResourceManager::MarkResourceFrameReferenced(it.second->viewSource, refType);

    ResourceManager::MarkResourceFrameReferenced(it.first, refType);
  }

  void MarkDirtyResource(ResourceId id)
  {
    GLResourceRecord *record = GetResourceRecord(id);
    if(record && record->viewSource != ResourceId())
      ResourceManager::MarkDirtyResource(record->viewSource);

    return ResourceManager::MarkDirtyResource(id);
  }
  void MarkDirtyResource(GLResource res)
  {
    rdcpair<ResourceId, GLResourceRecord *> &it = m_CurrentResources[res];

    if(it.second && it.second->viewSource != ResourceId())
      ResourceManager::MarkDirtyResource(it.second->viewSource);

    return ResourceManager::MarkDirtyResource(it.first);
  }
  // Mark resource as dirty and write-referenced.
  // Write-referenced resources are used to track resource "age".
  void MarkDirtyWithWriteReference(GLResource res)
  {
    rdcpair<ResourceId, GLResourceRecord *> &it = m_CurrentResources[res];

    if(it.second && it.second->viewSource != ResourceId())
    {
      ResourceManager::MarkResourceFrameReferenced(it.second->viewSource, eFrameRef_ReadBeforeWrite);
      ResourceManager::MarkDirtyResource(it.second->viewSource);
    }

    ResourceManager::MarkResourceFrameReferenced(it.first, eFrameRef_ReadBeforeWrite);
    ResourceManager::MarkDirtyResource(it.first);
  }

  void RegisterSync(ContextPair &ctx, GLsync sync, GLuint &name, ResourceId &id)
  {
    name = (GLuint)Atomic::Inc64(&m_SyncName);
    id = RegisterResource(SyncRes(ctx, name));

    m_SyncIDs[sync] = id;
    m_CurrentSyncs[name] = sync;
  }

  GLsync GetSync(GLuint name) { return m_CurrentSyncs[name]; }
  ResourceId GetSyncID(GLsync sync) { return m_SyncIDs[sync]; }
  // KHR_debug storage
  const rdcstr &GetName(ResourceId id) { return m_Names[id]; }
  void SetName(ResourceId id, const rdcstr &name) { m_Names[id] = name; }
  void SetName(GLResource res, const rdcstr &name) { SetName(GetID(res), name); }
  rdcstr GetName(GLResource res) { return GetName(GetID(res)); }
  // we need to find all the children bound to VAOs/FBOs and mark them referenced. The reason for
  // this is that say a VAO became high traffic and we stopped serialising buffer binds, but then it
  // is never modified in a frame and none of the buffers are ever referenced. They would be
  // eliminated from the log and the VAO initial state that tries to bind them would fail. Normally
  // this would be handled by record parenting, but that would be a nightmare to track.
  void MarkVAOReferenced(GLResource res, FrameRefType ref, bool allowFake0 = false);
  void MarkFBOReferenced(GLResource res, FrameRefType ref);
  void MarkFBODirtyWithWriteReference(GLResourceRecord *record);

  bool IsResourceTrackedForPersistency(const GLResource &res);

  template <typename SerialiserType>
  bool Serialise_InitialState(SerialiserType &ser, ResourceId id, GLResourceRecord *record,
                              const GLInitialContents *initial);
  bool Serialise_InitialState(WriteSerialiser &ser, ResourceId id, GLResourceRecord *record,
                              const GLInitialContents *initial);

  void ContextPrepare_InitialState(GLResource res);

  void SetInternalResource(GLResource res);

private:
  // forward this on. We de-alias it so that uses of GetID() within the GL driver aren't virtual
  ResourceId GetID(GLResource res) { return GetResID(res); }
  bool ResourceTypeRelease(GLResource res);
  bool Prepare_InitialState(GLResource res);
  uint64_t GetSize_InitialState(ResourceId resid, const GLInitialContents &initial);

  void PrepareTextureInitialContents(ResourceId liveid, ResourceId origid, GLResource res);

  void Create_InitialState(ResourceId id, GLResource live, bool hasData);
  void Apply_InitialState(GLResource live, const GLInitialContents &initial);

  void MarkFBOAttachmentsReferenced(ResourceId id, GLResourceRecord *record, FrameRefType ref,
                                    bool markDirty);

  // unfortunately not all resources have a record even at capture time (certain special resources
  // do not) so we store a pair to ensure we can always lookup the resource ID
  rdcflatmap<GLResource, rdcpair<ResourceId, GLResourceRecord *>> m_CurrentResources;

  // sync objects must be treated differently as they're not GLuint names, but pointer sized.
  // We manually give them GLuint names so they're otherwise namespaced as (eResSync, GLuint)
  std::map<GLsync, ResourceId> m_SyncIDs;
  std::map<GLuint, GLsync> m_CurrentSyncs;
  std::map<ResourceId, rdcstr> m_Names;
  int64_t m_SyncName;

  struct FBOCache
  {
    uint32_t age = 0;
    rdcarray<ResourceId> attachments;
  };

  rdcflatmap<ResourceId, FBOCache *> m_FBOAttachmentsCache;

  WrappedOpenGL *m_Driver;
};
