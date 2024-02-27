/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
bool WrappedID3D12GraphicsCommandList::Serialise_RSSetShadingRate(
    SerialiserType &ser, D3D12_SHADING_RATE baseShadingRate,
    const D3D12_SHADING_RATE_COMBINER *combiners)
{
  ID3D12GraphicsCommandList5 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(baseShadingRate).Important();
  SERIALISE_ELEMENT_ARRAY(combiners, 2).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal5() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList5 which isn't available");
      return false;
    }

    if(m_pDevice->GetOpts6().VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED)
    {
      // if the shading rate is 1x1 and the combiners are NULL (implicitly passthrough) or
      // explicitly passthrough, we can skip this call
      if(baseShadingRate == D3D12_SHADING_RATE_1X1 &&
         (combiners == NULL ||
          (combiners[0] == combiners[1] && combiners[0] == D3D12_SHADING_RATE_COMBINER_PASSTHROUGH)))
      {
        RDCWARN(
            "VRS is not supported, but skipping no-op "
            "RSSetShadingRate(baseShadingRate=%s, passthrough combiners)",
            ToStr(baseShadingRate).c_str());
        return true;
      }

      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires variable rate shading support which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap5(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->RSSetShadingRate(baseShadingRate, combiners);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap5(pCommandList)->RSSetShadingRate(baseShadingRate, combiners);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.shadingRate = baseShadingRate;
      if(combiners)
      {
        state.shadingRateCombiners[0] = combiners[0];
        state.shadingRateCombiners[1] = combiners[1];
      }
      else
      {
        state.shadingRateCombiners[0] = D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
        state.shadingRateCombiners[1] = D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
      }
    }
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12GraphicsCommandList::RSSetShadingRate(
    D3D12_SHADING_RATE baseShadingRate, const D3D12_SHADING_RATE_COMBINER *combiners)
{
  SERIALISE_TIME_CALL(m_pList5->RSSetShadingRate(baseShadingRate, combiners));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_RSSetShadingRate);
    Serialise_RSSetShadingRate(ser, baseShadingRate, combiners);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_RSSetShadingRateImage(SerialiserType &ser,
                                                                       ID3D12Resource *shadingRateImage)
{
  ID3D12GraphicsCommandList5 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(shadingRateImage).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal5() == NULL)
    {
      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12GraphicsCommandList5 which isn't available");
      return false;
    }

    if(m_pDevice->GetOpts6().VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED)
    {
      // if the shading rate image is NULL, we can skip this call
      if(shadingRateImage == NULL)
      {
        RDCWARN("VRS is not supported, but skipping no-op RSSetShadingRateImage(NULL)");
        return true;
      }

      SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires variable rate shading support which isn't available");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap5(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->RSSetShadingRateImage(Unwrap(shadingRateImage));

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap5(pCommandList)->RSSetShadingRateImage(Unwrap(shadingRateImage));

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.shadingRateImage = GetResID(shadingRateImage);
    }
  }

  return true;
}

void STDMETHODCALLTYPE
WrappedID3D12GraphicsCommandList::RSSetShadingRateImage(ID3D12Resource *shadingRateImage)
{
  SERIALISE_TIME_CALL(m_pList5->RSSetShadingRateImage(Unwrap(shadingRateImage)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_RSSetShadingRateImage);
    Serialise_RSSetShadingRateImage(ser, shadingRateImage);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(shadingRateImage), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, RSSetShadingRate,
                                D3D12_SHADING_RATE baseShadingRate,
                                const D3D12_SHADING_RATE_COMBINER *combiners);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, RSSetShadingRateImage,
                                ID3D12Resource *shadingRateImage);
