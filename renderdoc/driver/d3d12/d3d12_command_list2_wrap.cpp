/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_WriteBufferImmediate(
    SerialiserType &ser, UINT Count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *pParams,
    const D3D12_WRITEBUFFERIMMEDIATE_MODE *pModes)
{
  ID3D12GraphicsCommandList2 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(Count);
  SERIALISE_ELEMENT_ARRAY(pParams, Count);
  SERIALISE_ELEMENT_ARRAY(pModes, Count);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal2() == NULL)
    {
      RDCERR("Can't replay ID3D12GraphicsCommandList2 command");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap2(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->WriteBufferImmediate(Count, pParams, pModes);
      }
    }
    else
    {
      Unwrap2(pCommandList)->WriteBufferImmediate(Count, pParams, pModes);
      GetCrackedList2()->WriteBufferImmediate(Count, pParams, pModes);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::WriteBufferImmediate(
    UINT Count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *pParams,
    const D3D12_WRITEBUFFERIMMEDIATE_MODE *pModes)
{
  SERIALISE_TIME_CALL(m_pList2->WriteBufferImmediate(Count, pParams, pModes));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_WriteBufferImmediate);
    Serialise_WriteBufferImmediate(ser, Count, pParams, pModes);

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < Count; i++)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource1::GetResIDFromAddr(pParams[i].Dest), eFrameRef_PartialWrite);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, WriteBufferImmediate,
                                UINT Count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *pParams,
                                const D3D12_WRITEBUFFERIMMEDIATE_MODE *pModes);
