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
bool WrappedID3D12GraphicsCommandList::Serialise_RSSetDepthBias(SerialiserType &ser, FLOAT DepthBias,
                                                                FLOAT DepthBiasClamp,
                                                                FLOAT SlopeScaledDepthBias)
{
  ID3D12GraphicsCommandList9 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(DepthBias).Important();
  SERIALISE_ELEMENT(DepthBiasClamp).Important();
  SERIALISE_ELEMENT(SlopeScaledDepthBias).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal9() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList9 which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap9(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->RSSetDepthBias(DepthBias, DepthBiasClamp, SlopeScaledDepthBias);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap9(pCommandList)->RSSetDepthBias(DepthBias, DepthBiasClamp, SlopeScaledDepthBias);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &rs = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;
      rs.depthBias = DepthBias;
      rs.depthBiasClamp = DepthBiasClamp;
      rs.slopeScaledDepthBias = SlopeScaledDepthBias;
    }
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12GraphicsCommandList::RSSetDepthBias(FLOAT DepthBias,
                                                                        FLOAT DepthBiasClamp,
                                                                        FLOAT SlopeScaledDepthBias)
{
  SERIALISE_TIME_CALL(m_pList9->RSSetDepthBias(DepthBias, DepthBiasClamp, SlopeScaledDepthBias));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_RSSetDepthBias);
    Serialise_RSSetDepthBias(ser, DepthBias, DepthBiasClamp, SlopeScaledDepthBias);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_IASetIndexBufferStripCutValue(
    SerialiserType &ser, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue)
{
  ID3D12GraphicsCommandList9 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(IBStripCutValue).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal9() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList9 which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap9(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->IASetIndexBufferStripCutValue(IBStripCutValue);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap9(pCommandList)->IASetIndexBufferStripCutValue(IBStripCutValue);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &rs = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;
      rs.cutValue = IBStripCutValue;
    }
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12GraphicsCommandList::IASetIndexBufferStripCutValue(
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue)
{
  SERIALISE_TIME_CALL(m_pList9->IASetIndexBufferStripCutValue(IBStripCutValue));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_IASetIndexBufferStripCutValue);
    Serialise_IASetIndexBufferStripCutValue(ser, IBStripCutValue);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, RSSetDepthBias,
                                FLOAT DepthBias, FLOAT DepthBiasClamp, FLOAT SlopeScaledDepthBias);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, IASetIndexBufferStripCutValue,
                                D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue);
