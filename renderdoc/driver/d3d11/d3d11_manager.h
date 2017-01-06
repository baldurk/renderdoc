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

#include "api/replay/renderdoc_replay.h"
#include "common/wrapped_pool.h"
#include "core/core.h"
#include "core/resource_manager.h"
#include "driver/d3d11/d3d11_common.h"
#include "serialise/serialiser.h"

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
    ignoreSerialise = false;
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
      ShadowPtr[ctx][0] = Serialiser::AllocAlignedBuffer(size + sizeof(markerValue));
      ShadowPtr[ctx][1] = Serialiser::AllocAlignedBuffer(size + sizeof(markerValue));

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
        Serialiser::FreeAlignedBuffer(ShadowPtr[i][0]);
        Serialiser::FreeAlignedBuffer(ShadowPtr[i][1]);
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

  bool ignoreSerialise;

  int NumSubResources;
  ResourceRecord **SubResources;

private:
  byte *ShadowPtr[32][2];
  size_t ShadowSize[32];

  bool contexts[32];
};

class D3D11ResourceManager
    : public ResourceManager<ID3D11DeviceChild *, ID3D11DeviceChild *, D3D11ResourceRecord>
{
public:
  D3D11ResourceManager(LogState state, Serialiser *ser, WrappedID3D11Device *dev)
      : ResourceManager(state, ser), m_Device(dev)
  {
  }

  ID3D11DeviceChild *UnwrapResource(ID3D11DeviceChild *res);
  ID3D11Resource *UnwrapResource(ID3D11Resource *res)
  {
    return (ID3D11Resource *)UnwrapResource((ID3D11DeviceChild *)res);
  }

private:
  bool SerialisableResource(ResourceId id, D3D11ResourceRecord *record);
  ResourceId GetID(ID3D11DeviceChild *res);

  bool ResourceTypeRelease(ID3D11DeviceChild *res);

  bool Force_InitialState(ID3D11DeviceChild *res, bool prepare);
  bool Need_InitialStateChunk(ID3D11DeviceChild *res);
  bool Prepare_InitialState(ID3D11DeviceChild *res);
  bool Serialise_InitialState(ResourceId resid, ID3D11DeviceChild *res);
  void Create_InitialState(ResourceId id, ID3D11DeviceChild *live, bool hasData);
  void Apply_InitialState(ID3D11DeviceChild *live, InitialContentData data);

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
