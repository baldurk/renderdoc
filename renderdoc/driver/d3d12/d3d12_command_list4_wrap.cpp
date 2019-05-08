/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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
bool WrappedID3D12GraphicsCommandList::Serialise_BeginRenderPass(
    SerialiserType &ser, UINT NumRenderTargets,
    const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets,
    const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags)
{
  ID3D12GraphicsCommandList4 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumRenderTargets);
  SERIALISE_ELEMENT_ARRAY(pRenderTargets, NumRenderTargets);
  SERIALISE_ELEMENT_OPT(pDepthStencil);
  SERIALISE_ELEMENT(Flags);

  // since CPU handles are consumed in the call, we need to read out and serialise the contents
  // here.
  std::vector<D3D12Descriptor> RTVs;
  D3D12Descriptor DSV;

  {
    if(ser.IsWriting())
    {
      for(UINT i = 0; i < NumRenderTargets; i++)
        RTVs.push_back(*GetWrapped(pRenderTargets[i].cpuDescriptor));
    }

    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately
    SERIALISE_ELEMENT(RTVs).Named("RenderTargetDescriptors"_lit);
  }

  {
    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately.
    const D3D12Descriptor *pDSV = NULL;

    if(ser.IsWriting())
      pDSV = pDepthStencil ? GetWrapped(pDepthStencil->cpuDescriptor) : NULL;

    SERIALISE_ELEMENT_OPT(pDSV).Named("DepthStencilDescriptor"_lit);

    if(pDSV)
      DSV = *pDSV;
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal4() == NULL)
    {
      RDCERR("Can't replay ID3D12GraphicsCommandList4 command");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    // patch the parameters so that we point into our local CPU descriptor handles that are up
    // to date
    {
      D3D12_RENDER_PASS_RENDER_TARGET_DESC *rts =
          (D3D12_RENDER_PASS_RENDER_TARGET_DESC *)pRenderTargets;
      D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *ds =
          (D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *)pDepthStencil;

      for(UINT i = 0; i < NumRenderTargets; i++)
        rts[i].cpuDescriptor =
            Unwrap(m_pDevice->GetReplay()->GetDebugManager()->GetTempDescriptor(RTVs[i], i));

      if(ds)
        ds->cpuDescriptor = Unwrap(m_pDevice->GetReplay()->GetDebugManager()->GetTempDescriptor(DSV));
    }

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        // patch the parameters further so that this renderpass always loads and saves on replay. We
        // do any clears as necessary manually here
        {
          D3D12_RENDER_PASS_RENDER_TARGET_DESC *rts =
              (D3D12_RENDER_PASS_RENDER_TARGET_DESC *)pRenderTargets;
          D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *ds =
              (D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *)pDepthStencil;

          for(UINT i = 0; i < NumRenderTargets; i++)
          {
            if(rts[i].BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
            {
              Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
                  ->ClearRenderTargetView(rts[i].cpuDescriptor,
                                          rts[i].BeginningAccess.Clear.ClearValue.Color, 0, NULL);
            }

            rts[i].BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
            rts[i].EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
          }

          if(ds)
          {
            D3D12_CLEAR_FLAGS flags = {};

            if(ds->DepthBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
              flags |= D3D12_CLEAR_FLAG_DEPTH;
            if(ds->StencilBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
              flags |= D3D12_CLEAR_FLAG_STENCIL;

            if(flags)
            {
              // we can safely read from either depth/stencil clear values because if the access
              // type isn't clear the corresponding flag will be unset - so whatever garbage value
              // we have isn't used.
              Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
                  ->ClearDepthStencilView(
                      ds->cpuDescriptor, flags,
                      ds->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth,
                      ds->StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil, 0, NULL);
            }

            ds->DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
            ds->StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
            ds->DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
            ds->StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
          }
        }

        Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->BeginRenderPass(NumRenderTargets, pRenderTargets, pDepthStencil, Flags);

        m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].renderPassActive = true;

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          m_Cmd->m_RenderState.rts = RTVs;
          m_Cmd->m_RenderState.dsv = DSV;
        }
      }
    }
    else
    {
      Unwrap4(pCommandList)->BeginRenderPass(NumRenderTargets, pRenderTargets, pDepthStencil, Flags);
      GetCrackedList4()->BeginRenderPass(NumRenderTargets, pRenderTargets, pDepthStencil, Flags);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.rts = RTVs;
      state.dsv = DSV;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::BeginRenderPass(
    UINT NumRenderTargets, const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets,
    const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags)
{
  SERIALISE_TIME_CALL(
      m_pList4->BeginRenderPass(NumRenderTargets, pRenderTargets, pDepthStencil, Flags));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_BeginRenderPass);
    Serialise_BeginRenderPass(ser, NumRenderTargets, pRenderTargets, pDepthStencil, Flags);

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < NumRenderTargets; i++)
    {
      D3D12Descriptor *desc = GetWrapped(pRenderTargets[i].cpuDescriptor);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);

      if(pRenderTargets[i].EndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
      {
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pRenderTargets[i].EndingAccess.Resolve.pSrcResource), eFrameRef_PartialWrite);
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pRenderTargets[i].EndingAccess.Resolve.pDstResource), eFrameRef_PartialWrite);
      }
    }

    if(pDepthStencil)
    {
      D3D12Descriptor *desc = GetWrapped(pDepthStencil->cpuDescriptor);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);

      if(pDepthStencil->DepthEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
      {
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pDepthStencil->DepthEndingAccess.Resolve.pSrcResource), eFrameRef_PartialWrite);
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pDepthStencil->DepthEndingAccess.Resolve.pDstResource), eFrameRef_PartialWrite);
      }

      if(pDepthStencil->StencilEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
      {
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pDepthStencil->StencilEndingAccess.Resolve.pSrcResource),
            eFrameRef_PartialWrite);
        m_ListRecord->MarkResourceFrameReferenced(
            GetResID(pDepthStencil->StencilEndingAccess.Resolve.pDstResource),
            eFrameRef_PartialWrite);
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_EndRenderPass(SerialiserType &ser)
{
  ID3D12GraphicsCommandList4 *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(GetWrapped(pCommandList)->GetReal4() == NULL)
    {
      RDCERR("Can't replay ID3D12GraphicsCommandList4 command");
      return false;
    }

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap4(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->EndRenderPass();

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          m_Cmd->m_RenderState.rts.clear();
          m_Cmd->m_RenderState.dsv = D3D12Descriptor();
        }
      }
    }
    else
    {
      Unwrap4(pCommandList)->EndRenderPass();
      GetCrackedList4()->EndRenderPass();

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.rts.clear();
      state.dsv = D3D12Descriptor();
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::EndRenderPass()
{
  SERIALISE_TIME_CALL(m_pList4->EndRenderPass());

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_EndRenderPass);
    Serialise_EndRenderPass(ser);

    m_ListRecord->AddChunk(scope.Get());
  }
}

void WrappedID3D12GraphicsCommandList::InitializeMetaCommand(
    _In_ ID3D12MetaCommand *pMetaCommand,
    _In_reads_bytes_opt_(InitializationParametersDataSizeInBytes)
        const void *pInitializationParametersData,
    _In_ SIZE_T InitializationParametersDataSizeInBytes)
{
  RDCERR("InitializeMetaCommand called but no meta commands reported!");
}

void WrappedID3D12GraphicsCommandList::ExecuteMetaCommand(
    _In_ ID3D12MetaCommand *pMetaCommand,
    _In_reads_bytes_opt_(ExecutionParametersDataSizeInBytes) const void *pExecutionParametersData,
    _In_ SIZE_T ExecutionParametersDataSizeInBytes)
{
  RDCERR("ExecuteMetaCommand called but no meta commands reported!");
}

void WrappedID3D12GraphicsCommandList::BuildRaytracingAccelerationStructure(
    _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *pDesc,
    _In_ UINT NumPostbuildInfoDescs,
    _In_reads_opt_(NumPostbuildInfoDescs)
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pPostbuildInfoDescs)
{
  RDCERR("BuildRaytracingAccelerationStructure called but raytracing is not supported!");
}

void WrappedID3D12GraphicsCommandList::EmitRaytracingAccelerationStructurePostbuildInfo(
    _In_ const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pDesc,
    _In_ UINT NumSourceAccelerationStructures,
    _In_reads_(NumSourceAccelerationStructures)
        const D3D12_GPU_VIRTUAL_ADDRESS *pSourceAccelerationStructureData)
{
  RDCERR(
      "EmitRaytracingAccelerationStructurePostbuildInfo called but raytracing is not supported!");
}

void WrappedID3D12GraphicsCommandList::CopyRaytracingAccelerationStructure(
    _In_ D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData,
    _In_ D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData,
    _In_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode)
{
  RDCERR("CopyRaytracingAccelerationStructure called but raytracing is not supported!");
}

void WrappedID3D12GraphicsCommandList::SetPipelineState1(_In_ ID3D12StateObject *pStateObject)
{
  RDCERR("SetPipelineState1 called but raytracing is not supported!");
}

void WrappedID3D12GraphicsCommandList::DispatchRays(_In_ const D3D12_DISPATCH_RAYS_DESC *pDesc)
{
  RDCERR("DispatchRays called but raytracing is not supported!");
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, BeginRenderPass,
                                UINT NumRenderTargets,
                                const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets,
                                const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil,
                                D3D12_RENDER_PASS_FLAGS Flags);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, EndRenderPass);
