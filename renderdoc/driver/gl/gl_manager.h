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

#pragma once

#include "core/resource_manager.h"
#include "driver/gl/gl_resources.h"

class WrappedOpenGL;

class GLResourceManager : public ResourceManager<GLResource, GLResource, GLResourceRecord>
{
public:
  GLResourceManager(LogState state, Serialiser *ser, WrappedOpenGL *gl)
      : ResourceManager(state, ser), m_GL(gl), m_SyncName(1)
  {
  }
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
      auto it = m_GLResourceRecords.begin();

      for(size_t i = 0; it != m_GLResourceRecords.end();)
      {
        size_t prevSize = m_GLResourceRecords.size();
        it->second->FreeParents(this);

        // collection modified, restart loop
        if(prevSize != m_GLResourceRecords.size())
        {
          i = 0;
          it = m_GLResourceRecords.begin();
          continue;
        }

        // collection not modified, continue
        i++;
        it++;
      }
    }

    while(!m_GLResourceRecords.empty())
    {
      auto it = m_GLResourceRecords.begin();
      ResourceId id = it->second->GetResourceID();
      it->second->Delete(this);

      if(!m_GLResourceRecords.empty() && m_GLResourceRecords.begin()->second->GetResourceID() == id)
        m_GLResourceRecords.erase(m_GLResourceRecords.begin());
    }

    m_CurrentResourceIds.clear();

    ResourceManager::Shutdown();
  }

  inline void RemoveResourceRecord(ResourceId id)
  {
    for(auto it = m_GLResourceRecords.begin(); it != m_GLResourceRecords.end(); it++)
    {
      if(it->second->GetResourceID() == id)
      {
        m_GLResourceRecords.erase(it);
        break;
      }
    }

    ResourceManager::RemoveResourceRecord(id);
  }

  ResourceId RegisterResource(GLResource res)
  {
    ResourceId id = ResourceIDGen::GetNewUniqueID();
    m_CurrentResourceIds[res] = id;
    AddCurrentResource(id, res);
    return id;
  }

  using ResourceManager::HasCurrentResource;

  bool HasCurrentResource(GLResource res)
  {
    auto it = m_CurrentResourceIds.find(res);
    if(it != m_CurrentResourceIds.end())
      return true;

    return false;
  }

  void UnregisterResource(GLResource res)
  {
    auto it = m_CurrentResourceIds.find(res);
    if(it != m_CurrentResourceIds.end())
    {
      ReleaseCurrentResource(it->second);
      m_CurrentResourceIds.erase(res);
    }
  }

  ResourceId GetID(GLResource res)
  {
    auto it = m_CurrentResourceIds.find(res);
    if(it != m_CurrentResourceIds.end())
      return it->second;
    return ResourceId();
  }

  GLResourceRecord *AddResourceRecord(ResourceId id)
  {
    GLResourceRecord *ret = ResourceManager::AddResourceRecord(id);
    GLResource res = GetCurrentResource(id);

    m_GLResourceRecords[res] = ret;
    ret->Resource = res;

    return ret;
  }

  using ResourceManager::HasResourceRecord;

  bool HasResourceRecord(GLResource res) { return ResourceManager::HasResourceRecord(GetID(res)); }
  using ResourceManager::GetResourceRecord;

  GLResourceRecord *GetResourceRecord(GLResource res)
  {
    auto it = m_GLResourceRecords.find(res);
    if(it != m_GLResourceRecords.end())
      return it->second;

    return ResourceManager::GetResourceRecord(GetID(res));
  }

  using ResourceManager::MarkResourceFrameReferenced;

  void MarkResourceFrameReferenced(GLResource res, FrameRefType refType)
  {
    // we allow VAO 0 as a special case
    if(res.name == 0 && res.Namespace != eResVertexArray)
      return;
    ResourceManager::MarkResourceFrameReferenced(GetID(res), refType);
  }

  using ResourceManager::MarkDirtyResource;

  void MarkDirtyResource(GLResource res) { return ResourceManager::MarkDirtyResource(GetID(res)); }
  using ResourceManager::MarkCleanResource;

  void MarkCleanResource(GLResource res) { return ResourceManager::MarkCleanResource(GetID(res)); }
  void RegisterSync(void *ctx, GLsync sync, GLuint &name, ResourceId &id)
  {
    name = (GLuint)Atomic::Inc64(&m_SyncName);
    id = RegisterResource(SyncRes(ctx, name));

    m_SyncIDs[sync] = id;
    m_CurrentSyncs[name] = sync;
  }

  GLsync GetSync(GLuint name) { return m_CurrentSyncs[name]; }
  ResourceId GetSyncID(GLsync sync) { return m_SyncIDs[sync]; }
  // KHR_debug storage on replay
  const std::string &GetName(ResourceId id) { return m_Names[id]; }
  void SetName(ResourceId id, const std::string &name) { m_Names[id] = name; }
  // we need to find all the children bound to VAOs/FBOs and mark them referenced. The reason for
  // this is that say a VAO became high traffic and we stopped serialising buffer binds, but then it
  // is never modified in a frame and none of the buffers are ever referenced. They would be
  // eliminated from the log and the VAO initial state that tries to bind them would fail. Normally
  // this would be handled by record parenting, but that would be a nightmare to track.
  void MarkVAOReferenced(GLResource res, FrameRefType ref, bool allowFake0 = false);
  void MarkFBOReferenced(GLResource res, FrameRefType ref);

  bool Prepare_InitialState(GLResource res, byte *blob);
  bool Serialise_InitialState(ResourceId resid, GLResource res);

private:
  bool SerialisableResource(ResourceId id, GLResourceRecord *record);

  bool ResourceTypeRelease(GLResource res) { return true; }
  bool Force_InitialState(GLResource res, bool prepare);
  bool Need_InitialStateChunk(GLResource res);
  bool Prepare_InitialState(GLResource res);

  void CreateTextureImage(GLuint tex, GLenum internalFormat, GLenum textype, GLint dim, GLint width,
                          GLint height, GLint depth, GLint samples, int mips);
  void PrepareTextureInitialContents(ResourceId liveid, ResourceId origid, GLResource res);

  void Create_InitialState(ResourceId id, GLResource live, bool hasData);
  void Apply_InitialState(GLResource live, InitialContentData initial);

  map<GLResource, GLResourceRecord *> m_GLResourceRecords;

  map<GLResource, ResourceId> m_CurrentResourceIds;

  // sync objects must be treated differently as they're not GLuint names, but pointer sized.
  // We manually give them GLuint names so they're otherwise namespaced as (eResSync, GLuint)
  map<GLsync, ResourceId> m_SyncIDs;
  map<GLuint, GLsync> m_CurrentSyncs;
  map<ResourceId, std::string> m_Names;
  volatile int64_t m_SyncName;

  WrappedOpenGL *m_GL;
};
