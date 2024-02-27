/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "d3d12_command_list.h"
#include "d3d12_debug.h"

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_Barrier(SerialiserType &ser, UINT32 NumBarrierGroups,
                                                         const D3D12_BARRIER_GROUP *pBarrierGroups)
{
  ID3D12GraphicsCommandList7 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumBarrierGroups);
  SERIALISE_ELEMENT_ARRAY(pBarrierGroups, NumBarrierGroups).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal7() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList7 which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    rdcarray<D3D12_BUFFER_BARRIER> filteredUnwrappedBuf;
    rdcarray<D3D12_TEXTURE_BARRIER> filteredUnwrappedTex;
    rdcarray<D3D12_BARRIER_GROUP> filteredUnwrapped;

    // resize so we can take pointers
    for(UINT i = 0; i < NumBarrierGroups; i++)
    {
      if(pBarrierGroups[i].Type == D3D12_BARRIER_TYPE_BUFFER)
        filteredUnwrappedBuf.resize(filteredUnwrappedBuf.size() + pBarrierGroups[i].NumBarriers);
      else if(pBarrierGroups[i].Type == D3D12_BARRIER_TYPE_TEXTURE)
        filteredUnwrappedTex.resize(filteredUnwrappedTex.size() + pBarrierGroups[i].NumBarriers);
    }

    D3D12_TEXTURE_BARRIER *tex = filteredUnwrappedTex.data();
    D3D12_BUFFER_BARRIER *buf = filteredUnwrappedBuf.data();

    BakedCmdListInfo &cmdinfo = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID];

    // filter out any barriers that reference a NULL resource - this means the resource wasn't used
    // elsewhere so was discarded from the capture
    for(UINT i = 0; i < NumBarrierGroups; i++)
    {
      D3D12_BARRIER_GROUP group = pBarrierGroups[i];

      if(pBarrierGroups[i].Type == D3D12_BARRIER_TYPE_BUFFER)
      {
        const UINT num = group.NumBarriers;

        group.NumBarriers = 0;

        for(UINT b = 0; b < num; b++)
        {
          ID3D12Resource *res = group.pBufferBarriers[b].pResource;
          if(res)
          {
            buf[group.NumBarriers] = group.pBufferBarriers[b];
            buf[group.NumBarriers].pResource = Unwrap(res);
            group.NumBarriers++;

            cmdinfo.resourceUsage.push_back(make_rdcpair(
                GetResID(res), EventUsage(cmdinfo.curEventID, ResourceUsage::Barrier)));
          }
        }

        group.pBufferBarriers = buf;
        buf += group.NumBarriers;

        if(group.NumBarriers > 0)
          filteredUnwrapped.push_back(group);
      }
      else if(pBarrierGroups[i].Type == D3D12_BARRIER_TYPE_TEXTURE)
      {
        const UINT num = group.NumBarriers;

        group.NumBarriers = 0;

        for(UINT b = 0; b < num; b++)
        {
          ID3D12Resource *res = group.pTextureBarriers[b].pResource;
          if(res)
          {
            tex[group.NumBarriers] = group.pTextureBarriers[b];
            tex[group.NumBarriers].pResource = Unwrap(res);
            group.NumBarriers++;

            cmdinfo.resourceUsage.push_back(make_rdcpair(
                GetResID(res), EventUsage(cmdinfo.curEventID, ResourceUsage::Barrier)));
          }
        }

        group.pTextureBarriers = tex;
        tex += group.NumBarriers;

        if(group.NumBarriers > 0)
          filteredUnwrapped.push_back(group);
      }
      else
      {
        filteredUnwrapped.push_back(group);
      }
    }

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *rerecord = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);
        pCommandList = rerecord;

        if(!filteredUnwrapped.empty())
        {
          Unwrap7(pCommandList)->Barrier((UINT)filteredUnwrapped.size(), filteredUnwrapped.data());

          if(m_pDevice->GetReplayOptions().optimisation != ReplayOptimisationLevel::Fastest)
          {
            for(UINT i = 0; i < NumBarrierGroups; i++)
            {
              if(pBarrierGroups[i].Type == D3D12_BARRIER_TYPE_TEXTURE)
              {
                for(UINT b = 0; b < pBarrierGroups[i].NumBarriers; b++)
                {
                  const D3D12_TEXTURE_BARRIER &barrier = pBarrierGroups[i].pTextureBarriers[b];
                  if(barrier.pResource && (barrier.Flags & D3D12_TEXTURE_BARRIER_FLAG_DISCARD))
                  {
                    m_pDevice->GetDebugManager()->FillWithDiscardPattern(
                        rerecord, m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state,
                        DiscardType::UndefinedTransition, barrier.pResource, NULL,
                        barrier.LayoutAfter);
                  }
                }
              }
            }
          }
        }
      }
      else
      {
        pCommandList = NULL;
      }
    }
    else
    {
      if(!filteredUnwrapped.empty())
      {
        Unwrap7(pCommandList)->Barrier((UINT)filteredUnwrapped.size(), filteredUnwrapped.data());
      }
    }

    if(pCommandList)
    {
      ResourceId cmd = GetResID(pCommandList);

      for(UINT i = 0; i < NumBarrierGroups; i++)
      {
        if(pBarrierGroups[i].Type == D3D12_BARRIER_TYPE_TEXTURE)
        {
          for(UINT b = 0; b < pBarrierGroups[i].NumBarriers; b++)
          {
            if(pBarrierGroups[i].pTextureBarriers[b].pResource)
            {
              m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].barriers.newBarriers.push_back(
                  pBarrierGroups[i].pTextureBarriers[b]);
              m_Cmd->m_BakedCmdListInfo[cmd].barriers.newBarriers.push_back(
                  pBarrierGroups[i].pTextureBarriers[b]);
            }
          }
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::Barrier(UINT32 NumBarrierGroups,
                                               const D3D12_BARRIER_GROUP *pBarrierGroups)
{
  size_t memSize = sizeof(D3D12_BARRIER_GROUP) * NumBarrierGroups;
  for(UINT i = 0; i < NumBarrierGroups; i++)
  {
    if(pBarrierGroups[i].Type == D3D12_BARRIER_TYPE_BUFFER)
      memSize += pBarrierGroups[i].NumBarriers * sizeof(D3D12_BUFFER_BARRIER);
    else if(pBarrierGroups[i].Type == D3D12_BARRIER_TYPE_TEXTURE)
      memSize += pBarrierGroups[i].NumBarriers * sizeof(D3D12_TEXTURE_BARRIER);
  }

  byte *mem = m_pDevice->GetTempMemory(memSize);
  D3D12_BARRIER_GROUP *barriers = (D3D12_BARRIER_GROUP *)mem;
  mem += sizeof(D3D12_BARRIER_GROUP) * NumBarrierGroups;

  for(UINT i = 0; i < NumBarrierGroups; i++)
  {
    barriers[i] = pBarrierGroups[i];

    if(barriers[i].Type == D3D12_BARRIER_TYPE_BUFFER)
    {
      D3D12_BUFFER_BARRIER *buf = (D3D12_BUFFER_BARRIER *)mem;
      mem += sizeof(D3D12_BUFFER_BARRIER) * barriers[i].NumBarriers;
      for(UINT b = 0; b < barriers[i].NumBarriers; b++)
      {
        buf[b] = barriers[i].pBufferBarriers[b];
        buf[b].pResource = Unwrap(buf[b].pResource);
      }
      barriers[i].pBufferBarriers = buf;
    }
    else if(barriers[i].Type == D3D12_BARRIER_TYPE_TEXTURE)
    {
      D3D12_TEXTURE_BARRIER *tex = (D3D12_TEXTURE_BARRIER *)mem;
      mem += sizeof(D3D12_TEXTURE_BARRIER) * barriers[i].NumBarriers;
      for(UINT b = 0; b < barriers[i].NumBarriers; b++)
      {
        tex[b] = barriers[i].pTextureBarriers[b];
        tex[b].pResource = Unwrap(tex[b].pResource);
      }
      barriers[i].pTextureBarriers = tex;
    }
  }

  SERIALISE_TIME_CALL(m_pList7->Barrier(NumBarrierGroups, barriers));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_Barrier);
    Serialise_Barrier(ser, NumBarrierGroups, pBarrierGroups);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    for(UINT i = 0; i < NumBarrierGroups; i++)
      if(pBarrierGroups[i].Type == D3D12_BARRIER_TYPE_TEXTURE)
        m_ListRecord->cmdInfo->barriers.newBarriers.append(pBarrierGroups[i].pTextureBarriers,
                                                           pBarrierGroups[i].NumBarriers);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, Barrier,
                                UINT32 NumBarrierGroups, const D3D12_BARRIER_GROUP *pBarrierGroups);
