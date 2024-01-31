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

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_OMSetFrontAndBackStencilRef(SerialiserType &ser,
                                                                             UINT FrontStencilRef,
                                                                             UINT BackStencilRef)
{
  ID3D12GraphicsCommandList8 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(FrontStencilRef).Important();
  SERIALISE_ELEMENT(BackStencilRef).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal8() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList8 which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap8(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->OMSetFrontAndBackStencilRef(FrontStencilRef, BackStencilRef);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap8(pCommandList)->OMSetFrontAndBackStencilRef(FrontStencilRef, BackStencilRef);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &rs = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;
      rs.stencilRefFront = FrontStencilRef;
      rs.stencilRefBack = BackStencilRef;
    }
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12GraphicsCommandList::OMSetFrontAndBackStencilRef(
    UINT FrontStencilRef, UINT BackStencilRef)
{
  SERIALISE_TIME_CALL(m_pList8->OMSetFrontAndBackStencilRef(FrontStencilRef, BackStencilRef));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_OMSetFrontAndBackStencilRef);
    Serialise_OMSetFrontAndBackStencilRef(ser, FrontStencilRef, BackStencilRef);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, OMSetFrontAndBackStencilRef,
                                UINT FrontStencilRef, UINT BackStencilRef);
