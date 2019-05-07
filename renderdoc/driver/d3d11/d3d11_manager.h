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

#include "api/replay/renderdoc_replay.h"
#include "common/wrapped_pool.h"
#include "core/core.h"
#include "core/resource_manager.h"
#include "serialise/serialiser.h"
#include "d3d11_common.h"

enum D3D11ResourceType
{
  Resource_Unknown = 0,
  Resource_InputLayout,
  Resource_Buffer,
  Resource_Texture1D,
  Resource_Texture2D,
  Resource_Texture3D,
  Resource_RasterizerState,
  Resource_BlendState,
  Resource_DepthStencilState,
  Resource_SamplerState,
  Resource_RenderTargetView,
  Resource_ShaderResourceView,
  Resource_DepthStencilView,
  Resource_UnorderedAccessView,
  Resource_Shader,
  Resource_Counter,
  Resource_Query,
  Resource_Predicate,
  Resource_ClassInstance,
  Resource_ClassLinkage,

  Resource_DeviceContext,
  Resource_CommandList,
  Resource_DeviceState,
  Resource_Fence,
};

DECLARE_REFLECTION_ENUM(D3D11ResourceType);

struct D3D11ResourceRecord : public ResourceRecord
{
  enum
  {
    NullResource = NULL
  };

  static byte markerValue[32];

  D3D11ResourceRecord(ResourceId id)
      : ResourceRecord(id, true), NumSubResources(0), SubResources(NULL)
  {
    RDCEraseEl(ShadowPtr);
    RDCEraseEl(contexts);
  }

  ~D3D11ResourceRecord()
  {
    for(int i = 0; i < NumSubResources; i++)
    {
      SubResources[i]->DeleteChunks();
      SAFE_DELETE(SubResources[i]);
    }

    SAFE_DELETE_ARRAY(SubResources);

    FreeShadowStorage();
  }

  void AllocShadowStorage(int ctx, size_t size)
  {
    if(ShadowPtr[ctx][0] == NULL)
    {
      ShadowPtr[ctx][0] = AllocAlignedBuffer(size + sizeof(markerValue));
      ShadowPtr[ctx][1] = AllocAlignedBuffer(size + sizeof(markerValue));

      memcpy(ShadowPtr[ctx][0] + size, markerValue, sizeof(markerValue));
      memcpy(ShadowPtr[ctx][1] + size, markerValue, sizeof(markerValue));

      ShadowSize[ctx] = size;
    }
  }

  bool VerifyShadowStorage(int ctx)
  {
    if(ShadowPtr[ctx][0] &&
       memcmp(ShadowPtr[ctx][0] + ShadowSize[ctx], markerValue, sizeof(markerValue)))
      return false;

    if(ShadowPtr[ctx][1] &&
       memcmp(ShadowPtr[ctx][1] + ShadowSize[ctx], markerValue, sizeof(markerValue)))
      return false;

    return true;
  }

  void FreeShadowStorage()
  {
    for(int i = 0; i < 32; i++)
    {
      if(ShadowPtr[i][0] != NULL)
      {
        FreeAlignedBuffer(ShadowPtr[i][0]);
        FreeAlignedBuffer(ShadowPtr[i][1]);
      }
      ShadowPtr[i][0] = ShadowPtr[i][1] = NULL;
    }
  }

  byte *GetShadowPtr(int ctx, int p) { return ShadowPtr[ctx][p]; }
  int GetContextID()
  {
    // 0 is reserved for the immediate context
    for(int i = 1; i < 32; i++)
    {
      if(contexts[i] == false)
      {
        contexts[i] = true;
        return i;
      }
    }

    RDCERR(
        "More than 32 deferred contexts wanted an ID! Either a leak, or many many contexts mapping "
        "the same buffer");

    return 0;
  }

  void FreeContextID(int ctx) { contexts[ctx] = false; }
  void SetDataPtr(byte *ptr)
  {
    DataPtr = ptr;

    for(int i = 0; i < NumSubResources; i++)
      SubResources[i]->SetDataPtr(ptr);
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

      for(int i = 0; i < NumSubResources; i++)
        SubResources[i]->Insert(recordlist);
    }
  }

  int NumSubResources;
  D3D11ResourceRecord **SubResources;

private:
  byte *ShadowPtr[32][2];
  size_t ShadowSize[32];

  bool contexts[32];
};

struct D3D11InitialContents
{
  enum Tag
  {
    Copy,
    ClearRTV,
    ClearDSV,
    UAVCount,
  };
  D3D11InitialContents(D3D11ResourceType t, ID3D11Resource *r)
      : resourceType(t), tag(Copy), resource(r), resource2(NULL), uavCount(0)
  {
  }
  D3D11InitialContents(D3D11ResourceType t, ID3D11RenderTargetView *r,
                       ID3D11RenderTargetView *r2 = NULL)
      : resourceType(t), tag(ClearRTV), resource(r), resource2(r2), uavCount(0)
  {
  }
  D3D11InitialContents(D3D11ResourceType t, ID3D11DepthStencilView *r)
      : resourceType(t), tag(ClearDSV), resource(r), resource2(NULL), uavCount(0)
  {
  }
  D3D11InitialContents(D3D11ResourceType t, uint32_t c)
      : resourceType(t), tag(UAVCount), resource(NULL), resource2(NULL), uavCount(c)
  {
  }
  D3D11InitialContents() : resourceType(Resource_Unknown), tag(Copy), resource(NULL), uavCount(0) {}
  template <typename Configuration>
  void Free(ResourceManager<Configuration> *rm)
  {
    SAFE_RELEASE(resource);
    SAFE_RELEASE(resource2);
  }

  D3D11ResourceType resourceType;
  Tag tag;
  ID3D11DeviceChild *resource, *resource2;
  uint32_t uavCount;
};

struct D3D11ResourceManagerConfiguration
{
  typedef ID3D11DeviceChild *WrappedResourceType;
  typedef ID3D11DeviceChild *RealResourceType;
  typedef D3D11ResourceRecord RecordType;
  typedef D3D11InitialContents InitialContentData;
};

class D3D11ResourceManager : public ResourceManager<D3D11ResourceManagerConfiguration>
{
public:
  D3D11ResourceManager(WrappedID3D11Device *dev) : m_Device(dev) {}
  ID3D11DeviceChild *UnwrapResource(ID3D11DeviceChild *res);
  ID3D11Resource *UnwrapResource(ID3D11Resource *res)
  {
    return (ID3D11Resource *)UnwrapResource((ID3D11DeviceChild *)res);
  }

  void SetInternalResource(ID3D11DeviceChild *res);

private:
  ResourceId GetID(ID3D11DeviceChild *res);

  bool ResourceTypeRelease(ID3D11DeviceChild *res);

  bool Force_InitialState(ID3D11DeviceChild *res, bool prepare);
  bool Need_InitialStateChunk(ID3D11DeviceChild *res);
  bool Prepare_InitialState(ID3D11DeviceChild *res);
  uint64_t GetSize_InitialState(ResourceId id, ID3D11DeviceChild *res);
  bool Serialise_InitialState(WriteSerialiser &ser, ResourceId resid, ID3D11DeviceChild *res);
  void Create_InitialState(ResourceId id, ID3D11DeviceChild *live, bool hasData);
  void Apply_InitialState(ID3D11DeviceChild *live, D3D11InitialContents data);

  WrappedID3D11Device *m_Device;
};

template <typename Dest>
typename Dest::InnerType *Unwrap(typename Dest::InnerType *obj)
{
#if ENABLED(RDOC_DEVEL)
  if(obj && !Dest::IsAlloc(obj))
  {
    RDCERR("Trying to unwrap invalid type");
    return NULL;
  }
#endif
  return obj == NULL ? NULL : (Dest::InnerType *)((Dest *)obj)->GetReal();
}

#define UNWRAP(type, obj) Unwrap<type>((type::InnerType *)obj)
