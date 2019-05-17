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

#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_resources.h"

D3D11RenderState::D3D11RenderState(D3D11RenderState::EmptyInit)
{
  RDCEraseEl(IA);
  RDCEraseEl(VS);
  RDCEraseEl(HS);
  RDCEraseEl(DS);
  RDCEraseEl(GS);
  RDCEraseEl(SO);
  RDCEraseEl(RS);
  RDCEraseEl(PS);
  RDCEraseEl(OM);
  RDCEraseEl(CS);
  RDCEraseEl(CSUAVs);
  Predicate = NULL;
  PredicateValue = FALSE;
  Clear();

  m_ImmediatePipeline = false;
  m_pDevice = NULL;
}

D3D11RenderState::D3D11RenderState(const D3D11RenderState &other)
{
  RDCEraseEl(IA);
  RDCEraseEl(VS);
  RDCEraseEl(HS);
  RDCEraseEl(DS);
  RDCEraseEl(GS);
  RDCEraseEl(SO);
  RDCEraseEl(RS);
  RDCEraseEl(PS);
  RDCEraseEl(OM);
  RDCEraseEl(CS);
  RDCEraseEl(CSUAVs);
  Predicate = NULL;
  PredicateValue = FALSE;

  m_ImmediatePipeline = false;
  m_pDevice = NULL;

  CopyState(other);
}

void D3D11RenderState::CopyState(const D3D11RenderState &other)
{
  ReleaseRefs();

  memcpy(&IA, &other.IA, sizeof(IA));
  memcpy(&VS, &other.VS, sizeof(VS));
  memcpy(&HS, &other.HS, sizeof(HS));
  memcpy(&DS, &other.DS, sizeof(DS));
  memcpy(&GS, &other.GS, sizeof(GS));
  memcpy(&SO, &other.SO, sizeof(SO));
  memcpy(&RS, &other.RS, sizeof(RS));
  memcpy(&PS, &other.PS, sizeof(PS));
  memcpy(&OM, &other.OM, sizeof(OM));
  memcpy(&CS, &other.CS, sizeof(CS));
  memcpy(&CSUAVs, &other.CSUAVs, sizeof(CSUAVs));

  Predicate = other.Predicate;
  PredicateValue = other.PredicateValue;

  AddRefs();
}

D3D11RenderState::~D3D11RenderState()
{
  ReleaseRefs();
}

void D3D11RenderState::ReleaseRefs()
{
  ReleaseRef(IA.IndexBuffer);
  ReleaseRef(IA.Layout);

  for(UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
    ReleaseRef(IA.VBs[i]);

  Shader *stages[] = {&VS, &HS, &DS, &GS, &PS, &CS};
  for(int s = 0; s < 6; s++)
  {
    Shader *sh = stages[s];

    ReleaseRef(sh->Object);

    for(UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
      ReleaseRef(sh->ConstantBuffers[i]);

    for(UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++)
      ReleaseRef(sh->Samplers[i]);

    for(UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
      ReleaseRef(sh->SRVs[i]);

    for(UINT i = 0; i < D3D11_SHADER_MAX_INTERFACES; i++)
      ReleaseRef(sh->Instances[i]);

    sh++;
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
    ReleaseRef(CSUAVs[i]);

  for(UINT i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
    ReleaseRef(SO.Buffers[i]);

  ReleaseRef(RS.State);

  ReleaseRef(OM.BlendState);
  ReleaseRef(OM.DepthStencilState);

  for(UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    ReleaseRef(OM.RenderTargets[i]);

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
    ReleaseRef(OM.UAVs[i]);

  ReleaseRef(OM.DepthView);

  ReleaseRef(Predicate);

  RDCEraseEl(IA);
  RDCEraseEl(VS);
  RDCEraseEl(HS);
  RDCEraseEl(DS);
  RDCEraseEl(GS);
  RDCEraseEl(SO);
  RDCEraseEl(RS);
  RDCEraseEl(PS);
  RDCEraseEl(OM);
  RDCEraseEl(CS);
  RDCEraseEl(CSUAVs);
  Predicate = NULL;
}

void D3D11RenderState::MarkReferenced(WrappedID3D11DeviceContext *ctx, bool initial) const
{
  ctx->MarkResourceReferenced(GetIDForResource(IA.Layout), initial ? eFrameRef_None : eFrameRef_Read);

  ctx->MarkResourceReferenced(GetIDForResource(IA.IndexBuffer),
                              initial ? eFrameRef_None : eFrameRef_Read);

  for(UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
    ctx->MarkResourceReferenced(GetIDForResource(IA.VBs[i]),
                                initial ? eFrameRef_None : eFrameRef_Read);

  const Shader *stages[] = {&VS, &HS, &DS, &GS, &PS, &CS};
  for(int s = 0; s < 6; s++)
  {
    const Shader *sh = stages[s];

    ctx->MarkResourceReferenced(GetIDForResource(sh->Object),
                                initial ? eFrameRef_None : eFrameRef_Read);

    for(UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
      ctx->MarkResourceReferenced(GetIDForResource(sh->ConstantBuffers[i]),
                                  initial ? eFrameRef_None : eFrameRef_Read);

    for(UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++)
      ctx->MarkResourceReferenced(GetIDForResource(sh->Samplers[i]),
                                  initial ? eFrameRef_None : eFrameRef_Read);

    for(UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
      if(sh->SRVs[i])
      {
        ctx->MarkResourceReferenced(GetIDForResource(sh->SRVs[i]),
                                    initial ? eFrameRef_None : eFrameRef_Read);
        ctx->MarkResourceReferenced(GetViewResourceResID(sh->SRVs[i]),
                                    initial ? eFrameRef_None : eFrameRef_Read);
      }
    }

    sh++;
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(CSUAVs[i])
    {
      // UAVs we always assume to be partial updates
      ctx->MarkResourceReferenced(GetIDForResource(CSUAVs[i]),
                                  initial ? eFrameRef_None : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetIDForResource(CSUAVs[i]),
                                  initial ? eFrameRef_None : eFrameRef_PartialWrite);
      ctx->MarkResourceReferenced(GetViewResourceResID(CSUAVs[i]),
                                  initial ? eFrameRef_None : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetViewResourceResID(CSUAVs[i]),
                                  initial ? eFrameRef_None : eFrameRef_PartialWrite);
    }
  }

  for(UINT i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
    ctx->MarkResourceReferenced(GetIDForResource(SO.Buffers[i]),
                                initial ? eFrameRef_None : eFrameRef_PartialWrite);

  ctx->MarkResourceReferenced(GetIDForResource(RS.State), initial ? eFrameRef_None : eFrameRef_Read);

  ctx->MarkResourceReferenced(GetIDForResource(OM.BlendState),
                              initial ? eFrameRef_None : eFrameRef_Read);

  ctx->MarkResourceReferenced(GetIDForResource(OM.DepthStencilState),
                              initial ? eFrameRef_None : eFrameRef_Read);

  for(UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(OM.RenderTargets[i])
    {
      ctx->MarkResourceReferenced(GetIDForResource(OM.RenderTargets[i]),
                                  initial ? eFrameRef_None : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetViewResourceResID(OM.RenderTargets[i]),
                                  initial ? eFrameRef_None : eFrameRef_PartialWrite);
    }
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(OM.UAVs[i])
    {
      // UAVs we always assume to be partial updates
      ctx->MarkResourceReferenced(GetIDForResource(OM.UAVs[i]),
                                  initial ? eFrameRef_None : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetIDForResource(OM.UAVs[i]),
                                  initial ? eFrameRef_None : eFrameRef_PartialWrite);
      ctx->MarkResourceReferenced(GetViewResourceResID(OM.UAVs[i]),
                                  initial ? eFrameRef_None : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetViewResourceResID(OM.UAVs[i]),
                                  initial ? eFrameRef_None : eFrameRef_PartialWrite);
    }
  }

  if(OM.DepthView)
  {
    ctx->MarkResourceReferenced(GetIDForResource(OM.DepthView),
                                initial ? eFrameRef_None : eFrameRef_Read);
    ctx->MarkResourceReferenced(GetViewResourceResID(OM.DepthView),
                                initial ? eFrameRef_None : eFrameRef_PartialWrite);
  }

  if(Predicate)
  {
    ctx->MarkResourceReferenced(GetIDForResource(Predicate),
                                initial ? eFrameRef_None : eFrameRef_Read);
  }
}

void D3D11RenderState::AddRefs()
{
  TakeRef(IA.IndexBuffer);
  TakeRef(IA.Layout);

  for(UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
    TakeRef(IA.VBs[i]);

  Shader *stages[] = {&VS, &HS, &DS, &GS, &PS, &CS};
  for(int s = 0; s < 6; s++)
  {
    Shader *sh = stages[s];

    TakeRef(sh->Object);

    for(UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
      TakeRef(sh->ConstantBuffers[i]);

    for(UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++)
      TakeRef(sh->Samplers[i]);

    for(UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
      TakeRef(sh->SRVs[i]);

    for(UINT i = 0; i < D3D11_SHADER_MAX_INTERFACES; i++)
      TakeRef(sh->Instances[i]);

    sh++;
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
    TakeRef(CSUAVs[i]);

  for(UINT i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
    TakeRef(SO.Buffers[i]);

  TakeRef(RS.State);

  TakeRef(OM.BlendState);
  TakeRef(OM.DepthStencilState);

  for(UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    TakeRef(OM.RenderTargets[i]);

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
    TakeRef(OM.UAVs[i]);

  TakeRef(OM.DepthView);

  TakeRef(Predicate);
}

D3D11RenderState::D3D11RenderState(WrappedID3D11DeviceContext *context)
{
  RDCEraseMem(this, sizeof(D3D11RenderState));

  // IA
  context->IAGetInputLayout(&IA.Layout);
  context->IAGetPrimitiveTopology(&IA.Topo);
  context->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, IA.VBs,
                              (UINT *)IA.Strides, (UINT *)IA.Offsets);
  context->IAGetIndexBuffer(&IA.IndexBuffer, &IA.IndexFormat, &IA.IndexOffset);

  // VS
  context->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, VS.SRVs);
  context->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, VS.Samplers);
  context->VSGetShader((ID3D11VertexShader **)&VS.Object, VS.Instances, &VS.NumInstances);

  // DS
  context->DSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, DS.SRVs);
  context->DSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, DS.Samplers);
  context->DSGetShader((ID3D11DomainShader **)&DS.Object, DS.Instances, &DS.NumInstances);

  // HS
  context->HSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, HS.SRVs);
  context->HSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, HS.Samplers);
  context->HSGetShader((ID3D11HullShader **)&HS.Object, HS.Instances, &HS.NumInstances);

  // GS
  context->GSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, GS.SRVs);
  context->GSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, GS.Samplers);
  context->GSGetShader((ID3D11GeometryShader **)&GS.Object, GS.Instances, &GS.NumInstances);

  context->SOGetTargets(D3D11_SO_BUFFER_SLOT_COUNT, SO.Buffers);

  // RS
  context->RSGetState(&RS.State);
  RDCEraseEl(RS.Viewports);
  RS.NumViews = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  context->RSGetViewports(&RS.NumViews, RS.Viewports);
  RDCEraseEl(RS.Scissors);
  RS.NumScissors = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  context->RSGetScissorRects(&RS.NumScissors, RS.Scissors);

  // CS
  context->CSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, CS.SRVs);
  if(context->IsFL11_1())
    context->CSGetUnorderedAccessViews(0, D3D11_1_UAV_SLOT_COUNT, CSUAVs);
  else
    context->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, CSUAVs);
  context->CSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, CS.Samplers);
  context->CSGetShader((ID3D11ComputeShader **)&CS.Object, CS.Instances, &CS.NumInstances);

  // PS
  context->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, PS.SRVs);
  context->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, PS.Samplers);
  context->PSGetShader((ID3D11PixelShader **)&PS.Object, PS.Instances, &PS.NumInstances);

  context->VSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 VS.ConstantBuffers, VS.CBOffsets, VS.CBCounts);
  context->DSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 DS.ConstantBuffers, DS.CBOffsets, DS.CBCounts);
  context->HSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 HS.ConstantBuffers, HS.CBOffsets, HS.CBCounts);
  context->GSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 GS.ConstantBuffers, GS.CBOffsets, GS.CBCounts);
  context->CSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 CS.ConstantBuffers, CS.CBOffsets, CS.CBCounts);
  context->PSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 PS.ConstantBuffers, PS.CBOffsets, PS.CBCounts);

  // OM
  context->OMGetBlendState(&OM.BlendState, OM.BlendFactor, &OM.SampleMask);
  context->OMGetDepthStencilState(&OM.DepthStencilState, &OM.StencRef);

  ID3D11RenderTargetView *tmpViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
  context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpViews, NULL);

  OM.UAVStartSlot = 0;
  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(tmpViews[i] != NULL)
    {
      OM.UAVStartSlot = i + 1;
      SAFE_RELEASE(tmpViews[i]);
    }
  }

  if(context->IsFL11_1())
    context->OMGetRenderTargetsAndUnorderedAccessViews(
        OM.UAVStartSlot, OM.RenderTargets, &OM.DepthView, OM.UAVStartSlot,
        D3D11_1_UAV_SLOT_COUNT - OM.UAVStartSlot, OM.UAVs);
  else
    context->OMGetRenderTargetsAndUnorderedAccessViews(
        OM.UAVStartSlot, OM.RenderTargets, &OM.DepthView, OM.UAVStartSlot,
        D3D11_PS_CS_UAV_REGISTER_COUNT - OM.UAVStartSlot, OM.UAVs);

  context->GetPredication(&Predicate, &PredicateValue);
}

void D3D11RenderState::Clear()
{
  ReleaseRefs();
  OM.BlendFactor[0] = OM.BlendFactor[1] = OM.BlendFactor[2] = OM.BlendFactor[3] = 1.0f;
  OM.SampleMask = 0xffffffff;

  Predicate = NULL;
  PredicateValue = FALSE;

  for(size_t i = 0; i < ARRAY_COUNT(VS.CBCounts); i++)
    VS.CBCounts[i] = HS.CBCounts[i] = DS.CBCounts[i] = GS.CBCounts[i] = PS.CBCounts[i] =
        CS.CBCounts[i] = 4096;
}

bool D3D11RenderState::PredicationWouldPass()
{
  if(Predicate == NULL)
    return true;

  BOOL data = TRUE;

  HRESULT hr = S_FALSE;

  do
  {
    hr = m_pDevice->GetImmediateContext()->GetData(Predicate, &data, sizeof(BOOL), 0);
  } while(hr == S_FALSE);
  RDCASSERTEQUAL(hr, S_OK);

  // From SetPredication for PredicateValue:
  //
  // "If TRUE, rendering will be affected by when the predicate's conditions are met. If FALSE,
  // rendering will be affected when the conditions are not met."
  //
  // Which is really confusingly worded. 'rendering will be affected' means 'no rendering will
  // happen', and 'conditions are met' for e.g. an occlusion query means that it passed.
  // Thus a passing occlusion query has value TRUE and is 'condition is met', so for a typical "skip
  // when occlusion query fails" the value will be FALSE.

  return PredicateValue != data;
}

void D3D11RenderState::ApplyState(WrappedID3D11DeviceContext *context)
{
  context->ClearState();

  // IA
  context->IASetInputLayout(IA.Layout);
  context->IASetPrimitiveTopology(IA.Topo);
  context->IASetIndexBuffer(IA.IndexBuffer, IA.IndexFormat, IA.IndexOffset);
  context->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, IA.VBs, IA.Strides,
                              IA.Offsets);

  // VS
  context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, VS.SRVs);
  context->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, VS.Samplers);
  context->VSSetShader((ID3D11VertexShader *)VS.Object, VS.Instances, VS.NumInstances);

  // DS
  context->DSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, DS.SRVs);
  context->DSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, DS.Samplers);
  context->DSSetShader((ID3D11DomainShader *)DS.Object, DS.Instances, DS.NumInstances);

  // HS
  context->HSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, HS.SRVs);
  context->HSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, HS.Samplers);
  context->HSSetShader((ID3D11HullShader *)HS.Object, HS.Instances, HS.NumInstances);

  // GS
  context->GSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, GS.SRVs);
  context->GSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, GS.Samplers);
  context->GSSetShader((ID3D11GeometryShader *)GS.Object, GS.Instances, GS.NumInstances);

  context->SOSetTargets(D3D11_SO_BUFFER_SLOT_COUNT, SO.Buffers, SO.Offsets);

  // RS
  context->RSSetState(RS.State);
  context->RSSetViewports(RS.NumViews, RS.Viewports);
  context->RSSetScissorRects(RS.NumScissors, RS.Scissors);

  UINT UAV_keepcounts[D3D11_1_UAV_SLOT_COUNT] = {(UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1,
                                                 (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1};

  // CS
  context->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, CS.SRVs);
  context->CSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, CS.Samplers);
  if(context->IsFL11_1())
    context->CSSetUnorderedAccessViews(0, D3D11_1_UAV_SLOT_COUNT, CSUAVs, UAV_keepcounts);
  else
    context->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, CSUAVs, UAV_keepcounts);
  context->CSSetShader((ID3D11ComputeShader *)CS.Object, CS.Instances, CS.NumInstances);

  // PS
  context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, PS.SRVs);
  context->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, PS.Samplers);
  context->PSSetShader((ID3D11PixelShader *)PS.Object, PS.Instances, PS.NumInstances);

  context->VSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 VS.ConstantBuffers, VS.CBOffsets, VS.CBCounts);
  context->DSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 DS.ConstantBuffers, DS.CBOffsets, DS.CBCounts);
  context->HSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 HS.ConstantBuffers, HS.CBOffsets, HS.CBCounts);
  context->GSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 GS.ConstantBuffers, GS.CBOffsets, GS.CBCounts);
  context->CSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 CS.ConstantBuffers, CS.CBOffsets, CS.CBCounts);
  context->PSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                                 PS.ConstantBuffers, PS.CBOffsets, PS.CBCounts);

  // OM
  context->OMSetBlendState(OM.BlendState, OM.BlendFactor, OM.SampleMask);
  context->OMSetDepthStencilState(OM.DepthStencilState, OM.StencRef);

  if(context->IsFL11_1())
    context->OMSetRenderTargetsAndUnorderedAccessViews(
        RDCMIN(OM.UAVStartSlot, (UINT)D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT), OM.RenderTargets,
        OM.DepthView, OM.UAVStartSlot, D3D11_1_UAV_SLOT_COUNT - OM.UAVStartSlot, OM.UAVs,
        UAV_keepcounts);
  else
    context->OMSetRenderTargetsAndUnorderedAccessViews(
        RDCMIN(OM.UAVStartSlot, (UINT)D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT), OM.RenderTargets,
        OM.DepthView, OM.UAVStartSlot, D3D11_PS_CS_UAV_REGISTER_COUNT - OM.UAVStartSlot, OM.UAVs,
        UAV_keepcounts);

  context->SetPredication(Predicate, PredicateValue);
}

void D3D11RenderState::TakeRef(ID3D11DeviceChild *p)
{
  if(p)
  {
    p->AddRef();
    if(m_ImmediatePipeline)
    {
      if(WrappedID3D11RenderTargetView1::IsAlloc(p) || WrappedID3D11ShaderResourceView1::IsAlloc(p) ||
         WrappedID3D11DepthStencilView::IsAlloc(p) || WrappedID3D11UnorderedAccessView1::IsAlloc(p))
        m_pDevice->InternalRef();

      m_pDevice->InternalRef();

      // we can use any specialisation of device child here, as all that is templated
      // is the nested pointer type. Saves having another class in the inheritance
      // heirarchy :(
      ((WrappedDeviceChild11<ID3D11Buffer> *)p)->PipelineAddRef();
    }
  }
}

void D3D11RenderState::ReleaseRef(ID3D11DeviceChild *p)
{
  if(p)
  {
    p->Release();
    if(m_ImmediatePipeline)
    {
      if(WrappedID3D11RenderTargetView1::IsAlloc(p) || WrappedID3D11ShaderResourceView1::IsAlloc(p) ||
         WrappedID3D11DepthStencilView::IsAlloc(p) || WrappedID3D11UnorderedAccessView1::IsAlloc(p))
        m_pDevice->InternalRelease();

      m_pDevice->InternalRelease();

      // see above
      ((WrappedDeviceChild11<ID3D11Buffer> *)p)->PipelineRelease();
    }
  }
}

bool D3D11RenderState::IsRangeBoundForWrite(const ResourceRange &range)
{
  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(CSUAVs[i] && range.Intersects(GetResourceRange(CSUAVs[i])))
    {
      // RDCDEBUG("Resource was bound on CS UAV %u", i);
      return true;
    }
  }

  for(UINT i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
  {
    if(SO.Buffers[i] && range.Intersects(ResourceRange(SO.Buffers[i])))
    {
      // RDCDEBUG("Resource was bound on SO buffer %u", i);
      return true;
    }
  }

  for(UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(OM.RenderTargets[i] && range.Intersects(GetResourceRange(OM.RenderTargets[i])))
    {
      // RDCDEBUG("Resource was bound on RTV %u", i);
      return true;
    }
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(OM.UAVs[i] && range.Intersects(GetResourceRange(OM.UAVs[i])))
    {
      // RDCDEBUG("Resource was bound on OM UAV %d", i);
      return true;
    }
  }

  if(OM.DepthView)
  {
    const ResourceRange &depthRange = GetResourceRange(OM.DepthView);

    if(range.Intersects(depthRange))
    {
      // RDCDEBUG("Resource was bound on OM DSV");

      if(depthRange.IsDepthReadOnly() && depthRange.IsStencilReadOnly())
      {
        // RDCDEBUG("but it's a readonly DSV, so that's fine");
      }
      else if(depthRange.IsDepthReadOnly() && range.IsDepthReadOnly())
      {
        // RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
      }
      else if(depthRange.IsStencilReadOnly() && range.IsStencilReadOnly())
      {
        // RDCDEBUG("but it's a stencil readonly DSV and we're only reading stencil, so that's OK");
      }
      else
      {
        return true;
      }
    }
  }

  return false;
}

void D3D11RenderState::UnbindRangeForWrite(const ResourceRange &range)
{
  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(CSUAVs[i] && range.Intersects(GetResourceRange(CSUAVs[i])))
    {
      ReleaseRef(CSUAVs[i]);
      CSUAVs[i] = NULL;
    }
  }

  for(UINT i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
  {
    if(SO.Buffers[i] && range.Intersects(ResourceRange(SO.Buffers[i])))
    {
      ReleaseRef(SO.Buffers[i]);
      SO.Buffers[i] = NULL;
    }
  }

  for(UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(OM.RenderTargets[i] && range.Intersects(GetResourceRange(OM.RenderTargets[i])))
    {
      ReleaseRef(OM.RenderTargets[i]);
      OM.RenderTargets[i] = NULL;
    }
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(OM.UAVs[i] && range.Intersects(GetResourceRange(OM.UAVs[i])))
    {
      ReleaseRef(OM.UAVs[i]);
      OM.UAVs[i] = NULL;
    }
  }

  if(OM.DepthView && range.Intersects(GetResourceRange(OM.DepthView)))
  {
    ReleaseRef(OM.DepthView);
    OM.DepthView = NULL;
  }
}

void D3D11RenderState::UnbindRangeForRead(const ResourceRange &range)
{
  for(int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
  {
    if(IA.VBs[i] && range.Intersects(ResourceRange(IA.VBs[i])))
    {
      // RDCDEBUG("Resource was bound on IA VB %u", i);
      ReleaseRef(IA.VBs[i]);
      IA.VBs[i] = NULL;
    }
  }

  if(IA.IndexBuffer && range.Intersects(ResourceRange(IA.IndexBuffer)))
  {
    // RDCDEBUG("Resource was bound on IA IB");
    ReleaseRef(IA.IndexBuffer);
    IA.IndexBuffer = NULL;
  }

  // const char *names[] = { "VS", "DS", "HS", "GS", "PS", "CS" };
  Shader *stages[] = {&VS, &HS, &DS, &GS, &PS, &CS};
  for(int s = 0; s < 6; s++)
  {
    Shader *sh = stages[s];

    for(UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    {
      if(sh->ConstantBuffers[i] && range.Intersects(ResourceRange(sh->ConstantBuffers[i])))
      {
        // RDCDEBUG("Resource was bound on %s CB %u", names[s], i);
        ReleaseRef(sh->ConstantBuffers[i]);
        sh->ConstantBuffers[i] = NULL;
      }
    }

    for(UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
      if(!sh->SRVs[i])
        continue;

      const ResourceRange &srvRange = GetResourceRange(sh->SRVs[i]);

      if(range.Intersects(srvRange))
      {
        // RDCDEBUG("Resource was bound on %s SRV %u", names[s], i);

        if(range.IsDepthReadOnly() && srvRange.IsDepthReadOnly())
        {
          // RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
        }
        else if(range.IsStencilReadOnly() && srvRange.IsStencilReadOnly())
        {
          // RDCDEBUG("but it's a stencil readonly DSV and we're only reading stenc, so that's OK");
        }
        else
        {
          // RDCDEBUG("Unbinding.");
          ReleaseRef(sh->SRVs[i]);
          sh->SRVs[i] = NULL;
        }
      }
    }

    sh++;
  }
}

bool D3D11RenderState::ValidOutputMerger(ID3D11RenderTargetView *const RTs[], UINT NumRTs,
                                         ID3D11DepthStencilView *depth,
                                         ID3D11UnorderedAccessView *const uavs[], UINT NumUAVs)
{
  D3D11_RENDER_TARGET_VIEW_DESC RTDescs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
  D3D11_DEPTH_STENCIL_VIEW_DESC DepthDesc;

  RDCEraseEl(RTDescs);
  RDCEraseEl(DepthDesc);

  ID3D11Resource *Resources[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
  ID3D11Resource *DepthResource = NULL;

  D3D11_RESOURCE_DIMENSION renderdim[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {
      D3D11_RESOURCE_DIMENSION_UNKNOWN};
  D3D11_RESOURCE_DIMENSION depthdim = D3D11_RESOURCE_DIMENSION_UNKNOWN;

  for(UINT i = 0; RTs && i < NumRTs; i++)
  {
    if(RTs[i])
    {
      RTs[i]->GetDesc(&RTDescs[i]);
      RTs[i]->GetResource(&Resources[i]);
      Resources[i]->GetType(&renderdim[i]);
    }
  }

  if(depth)
  {
    depth->GetDesc(&DepthDesc);
    depth->GetResource(&DepthResource);
    DepthResource->GetType(&depthdim);
  }

  bool valid = true;

  // check for duplicates and mark as invalid
  {
    ResourceRange rtvRanges[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
    };
    ResourceRange depthRange(depth);
    ResourceRange uavRanges[D3D11_1_UAV_SLOT_COUNT] = {
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
        ResourceRange::Null, ResourceRange::Null, ResourceRange::Null, ResourceRange::Null,
    };

    for(UINT i = 0; RTs && i < NumRTs; i++)
    {
      if(RTs[i])
        rtvRanges[i] = GetResourceRange(RTs[i]);
      else
        break;
    }

    if(depth)
      depthRange = GetResourceRange(depth);

    int numUAVs = 0;

    for(UINT i = 0; uavs && i < NumUAVs; i++)
    {
      if(uavs[i])
      {
        uavRanges[i] = GetResourceRange(uavs[i]);
        numUAVs = i + 1;
      }
    }

    // since constants are low, just do naive check for any intersecting ranges

    for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
      if(rtvRanges[i].IsNull())
        continue;

      // does it match any other RTV?
      for(int j = i + 1; j < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; j++)
      {
        if(rtvRanges[i].Intersects(rtvRanges[j]))
        {
          valid = false;
          m_pDevice->AddDebugMessage(
              MessageCategory::State_Setting, MessageSeverity::High, MessageSource::IncorrectAPIUse,
              StringFormat::Fmt("Invalid output merger - Render targets %d and %d overlap", i, j));
          break;
        }
      }

      // or depth?
      if(rtvRanges[i].Intersects(depthRange))
      {
        valid = false;
        m_pDevice->AddDebugMessage(
            MessageCategory::State_Setting, MessageSeverity::High, MessageSource::IncorrectAPIUse,
            StringFormat::Fmt("Invalid output merger - Render target %d and depth overlap", i));
        break;
      }

      // or a UAV?
      for(int j = 0; j < numUAVs; j++)
      {
        if(rtvRanges[i].Intersects(uavRanges[j]))
        {
          valid = false;
          m_pDevice->AddDebugMessage(
              MessageCategory::State_Setting, MessageSeverity::High, MessageSource::IncorrectAPIUse,
              StringFormat::Fmt("Invalid output merger - Render target %d and UAV %d overlap", i, j));
          break;
        }
      }
    }

    for(int i = 0; valid && i < numUAVs; i++)
    {
      if(uavRanges[i].IsNull())
        continue;

      // don't have to check RTVs, that's the reflection of the above check

      // does it match depth?
      if(uavRanges[i].Intersects(depthRange))
      {
        valid = false;
        m_pDevice->AddDebugMessage(
            MessageCategory::State_Setting, MessageSeverity::High, MessageSource::IncorrectAPIUse,
            StringFormat::Fmt("Invalid output merger - UAV %d and depth overlap", i));
        break;
      }

      // or another UAV?
      for(int j = i + 1; j < numUAVs; j++)
      {
        if(uavRanges[i].Intersects(uavRanges[j]))
        {
          valid = false;
          m_pDevice->AddDebugMessage(
              MessageCategory::State_Setting, MessageSeverity::High, MessageSource::IncorrectAPIUse,
              StringFormat::Fmt("Invalid output merger - UAVs %d and %d overlap", i, j));
          break;
        }
      }
    }

    // don't have to check depth - it was checked against all RTs and UAVs above
  }

  //////////////////////////////////////////////////////////////////////////
  // Resource dimensions of all views must be the same

  D3D11_RESOURCE_DIMENSION dim = D3D11_RESOURCE_DIMENSION_UNKNOWN;

  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(renderdim[i] == D3D11_RESOURCE_DIMENSION_UNKNOWN)
      continue;
    if(dim == D3D11_RESOURCE_DIMENSION_UNKNOWN)
      dim = renderdim[i];

    if(renderdim[i] != dim)
    {
      valid = false;
      m_pDevice->AddDebugMessage(MessageCategory::State_Setting, MessageSeverity::High,
                                 MessageSource::IncorrectAPIUse,
                                 "Invalid output merger - Render targets of different type");
      break;
    }
  }

  if(depthdim != D3D11_RESOURCE_DIMENSION_UNKNOWN && dim != D3D11_RESOURCE_DIMENSION_UNKNOWN &&
     depthdim != dim)
  {
    m_pDevice->AddDebugMessage(
        MessageCategory::State_Setting, MessageSeverity::High, MessageSource::IncorrectAPIUse,
        "Invalid output merger - Render target(s) and depth target of different type");
    valid = false;
  }

  if(!valid)
  {
    // RDCDEBUG("Resource dimensions don't match between render targets and/or depth stencil");
  }
  else
  {
    // pretend all resources are 3D descs just to make the code simpler
    // * put arraysize for 1D/2D into the depth for 3D
    // * use sampledesc from 2d as it will be identical for 1D/3D

    D3D11_TEXTURE3D_DESC desc = {0};
    D3D11_TEXTURE2D_DESC desc2 = {0};

    for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
      if(Resources[i] == NULL)
        continue;

      D3D11_TEXTURE1D_DESC d1 = {0};
      D3D11_TEXTURE2D_DESC d2 = {0};
      D3D11_TEXTURE3D_DESC d3 = {0};

      if(dim == D3D11_RESOURCE_DIMENSION_BUFFER)
      {
      }
      if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
      {
        ((ID3D11Texture1D *)Resources[i])->GetDesc(&d1);
        d3.Width = RDCMAX(1U, d1.Width >> RTDescs[i].Texture1D.MipSlice);

        if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D)
          d3.Depth = 1;
        else if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
          d3.Depth = RDCMIN(d1.ArraySize, RTDescs[i].Texture1DArray.ArraySize);
      }
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
      {
        ((ID3D11Texture2D *)Resources[i])->GetDesc(&d2);

        if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D)
        {
          d3.Width = RDCMAX(1U, d2.Width >> RTDescs[i].Texture2D.MipSlice);
          d3.Height = RDCMAX(1U, d2.Height >> RTDescs[i].Texture2D.MipSlice);
          d3.Depth = 1;
        }
        else if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS)
        {
          d3.Width = d2.Width;
          d3.Height = d2.Height;
          d3.Depth = 1;
        }
        else if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
        {
          d3.Width = RDCMAX(1U, d2.Width >> RTDescs[i].Texture2DArray.MipSlice);
          d3.Height = RDCMAX(1U, d2.Height >> RTDescs[i].Texture2DArray.MipSlice);
          d3.Depth = RDCMIN(d2.ArraySize, RTDescs[i].Texture2DArray.ArraySize);
        }
        else if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
        {
          d3.Width = d2.Width;
          d3.Height = d2.Height;
          d3.Depth = RDCMIN(d2.ArraySize, RTDescs[i].Texture2DMSArray.ArraySize);
        }
      }
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
      {
        ((ID3D11Texture3D *)Resources[i])->GetDesc(&d3);
        d3.Width = RDCMAX(1U, d3.Width >> RTDescs[i].Texture3D.MipSlice);
        d3.Height = RDCMAX(1U, d3.Height >> RTDescs[i].Texture3D.MipSlice);
        d3.Depth = RDCMAX(1U, d3.Depth >> RTDescs[i].Texture3D.MipSlice);
        d3.Depth = RDCMIN(d3.Depth, RTDescs[i].Texture3D.WSize);
      }

      if(desc.Width == 0)
      {
        desc = d3;
        desc2 = d2;
        continue;
      }

      if(desc.Width != d3.Width || desc.Height != d3.Height || desc.Depth != d3.Depth ||
         desc2.SampleDesc.Count != d2.SampleDesc.Count ||
         desc2.SampleDesc.Quality != d2.SampleDesc.Quality)
      {
        m_pDevice->AddDebugMessage(
            MessageCategory::State_Setting, MessageSeverity::High, MessageSource::IncorrectAPIUse,
            "Invalid output merger - Render targets are different dimensions");
        valid = false;
        break;
      }
    }

    if(DepthResource && valid)
    {
      D3D11_TEXTURE1D_DESC d1 = {0};
      D3D11_TEXTURE2D_DESC d2 = {0};
      D3D11_TEXTURE3D_DESC d3 = {0};

      if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
      {
        ((ID3D11Texture1D *)DepthResource)->GetDesc(&d1);
        d3.Width = RDCMAX(1U, d1.Width >> DepthDesc.Texture1D.MipSlice);

        if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1D)
          d3.Depth = 1;
        else if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
          d3.Depth = RDCMIN(d1.ArraySize, DepthDesc.Texture1DArray.ArraySize);
      }
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
      {
        ((ID3D11Texture2D *)DepthResource)->GetDesc(&d2);

        if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
        {
          d3.Width = RDCMAX(1U, d2.Width >> DepthDesc.Texture2D.MipSlice);
          d3.Height = RDCMAX(1U, d2.Height >> DepthDesc.Texture2D.MipSlice);
          d3.Depth = 1;
        }
        else if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
        {
          d3.Width = RDCMAX(1U, d2.Width >> DepthDesc.Texture2DArray.MipSlice);
          d3.Height = RDCMAX(1U, d2.Height >> DepthDesc.Texture2DArray.MipSlice);
          d3.Depth = RDCMIN(d2.ArraySize, DepthDesc.Texture2DArray.ArraySize);
        }
        else if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMS)
        {
          d3.Width = d2.Width;
          d3.Height = d2.Height;
          d3.Depth = 1;
        }
        else if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY)
        {
          d3.Width = d2.Width;
          d3.Height = d2.Height;
          d3.Depth = RDCMIN(d2.ArraySize, DepthDesc.Texture2DMSArray.ArraySize);
        }
      }
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D || dim == D3D11_RESOURCE_DIMENSION_BUFFER)
      {
        m_pDevice->AddDebugMessage(MessageCategory::State_Setting, MessageSeverity::High,
                                   MessageSource::IncorrectAPIUse,
                                   "Invalid output merger - Depth target is Texture3D or Buffer "
                                   "(shouldn't be possible! How did you create this view?!)");
        valid = false;
      }

      if(desc.Width != 0 && valid)
      {
        if(desc.Width != d3.Width || desc.Height != d3.Height || desc.Depth != d3.Depth ||
           desc2.SampleDesc.Count != d2.SampleDesc.Count ||
           desc2.SampleDesc.Quality != d2.SampleDesc.Quality)
        {
          valid = false;

          // explicitly allow over-sized depth targets
          if(desc.Width <= d3.Width && desc.Height <= d3.Height && desc.Depth <= d3.Depth &&
             desc2.SampleDesc.Count == d2.SampleDesc.Count &&
             desc2.SampleDesc.Quality == d2.SampleDesc.Quality)
          {
            valid = true;
            m_pDevice->AddDebugMessage(
                MessageCategory::State_Setting, MessageSeverity::High, MessageSource::IncorrectAPIUse,
                "Valid but unusual output merger - Depth target is larger than render target(s)");
          }
          else
          {
            m_pDevice->AddDebugMessage(MessageCategory::State_Setting, MessageSeverity::High,
                                       MessageSource::IncorrectAPIUse,
                                       "Invalid output merger - Depth target is different size or "
                                       "MS count to render target(s)");
          }
        }
      }
    }
  }

  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    SAFE_RELEASE(Resources[i]);

  SAFE_RELEASE(DepthResource);

  return valid;
}

bool D3D11RenderState::InputAssembler::Used_VB(WrappedID3D11Device *device, uint32_t slot) const
{
  if(Layout == NULL)
    return false;

  const std::vector<D3D11_INPUT_ELEMENT_DESC> &vec = device->GetLayoutDesc(Layout);

  for(size_t i = 0; i < vec.size(); i++)
    if(vec[i].InputSlot == slot)
      return true;

  return false;
}

bool D3D11RenderState::Shader::Used_CB(uint32_t slot) const
{
  if(ConstantBuffers[slot] == NULL)
    return false;

  WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)Object;

  if(shad == NULL)
    return false;

  DXBC::DXBCFile *dxbc = shad->GetDXBC();

  // have to assume it's used if there's no DXBC
  if(dxbc == NULL)
    return true;

  for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
    if(dxbc->m_CBuffers[i].reg == slot)
      return true;

  return false;
}

bool D3D11RenderState::Shader::Used_SRV(uint32_t slot) const
{
  if(SRVs[slot] == NULL)
    return false;

  WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)Object;

  if(shad == NULL)
    return false;

  DXBC::DXBCFile *dxbc = shad->GetDXBC();

  // have to assume it's used if there's no DXBC
  if(dxbc == NULL)
    return true;

  for(const DXBC::ShaderInputBind &bind : dxbc->m_SRVs)
    if(bind.reg == slot)
      return true;

  return false;
}

bool D3D11RenderState::Shader::Used_UAV(uint32_t slot) const
{
  WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)Object;

  if(shad == NULL)
    return false;

  DXBC::DXBCFile *dxbc = shad->GetDXBC();

  // have to assume it's used if there's no DXBC
  if(dxbc == NULL)
    return true;

  for(const DXBC::ShaderInputBind &bind : dxbc->m_UAVs)
    if(bind.reg == slot)
      return true;

  return false;
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11InputLayout *resource)
{
  return false;
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11Predicate *resource)
{
  return false;
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11ClassInstance *resource)
{
  return false;
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11DeviceChild *shader)
{
  return false;
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11SamplerState *resource)
{
  return false;
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11BlendState *state)
{
  return false;
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11RasterizerState *state)
{
  return false;
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11DepthStencilState *state)
{
  return false;
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11Buffer *buffer)
{
  if(buffer == NULL)
    return false;

  return IsRangeBoundForWrite(ResourceRange(buffer));
}

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11ShaderResourceView *srv)
{
  if(srv == NULL)
    return false;

  return IsRangeBoundForWrite(GetResourceRange(srv));
}

template <>
void D3D11RenderState::UnbindForRead(ID3D11Buffer *buffer)
{
  if(buffer == NULL)
    return;
  UnbindRangeForRead(ResourceRange(buffer));
}

template <>
void D3D11RenderState::UnbindForRead(ID3D11RenderTargetView *rtv)
{
  if(rtv == NULL)
    return;

  UnbindRangeForRead(GetResourceRange(rtv));
}

template <>
void D3D11RenderState::UnbindForRead(ID3D11DepthStencilView *dsv)
{
  if(dsv == NULL)
    return;

  const ResourceRange &dsvRange = GetResourceRange(dsv);

  if(dsvRange.IsDepthReadOnly() && dsvRange.IsStencilReadOnly())
  {
    // don't need to.
  }
  else
  {
    UnbindRangeForRead(dsvRange);
  }
}

template <>
void D3D11RenderState::UnbindForRead(ID3D11UnorderedAccessView *uav)
{
  if(uav == NULL)
    return;

  UnbindRangeForRead(GetResourceRange(uav));
}

template <>
void D3D11RenderState::UnbindForWrite(ID3D11Buffer *buffer)
{
  if(buffer == NULL)
    return;
  UnbindRangeForWrite(ResourceRange(buffer));
}

template <>
void D3D11RenderState::UnbindForWrite(ID3D11RenderTargetView *rtv)
{
  if(rtv == NULL)
    return;

  UnbindRangeForWrite(GetResourceRange(rtv));
}

template <>
void D3D11RenderState::UnbindForWrite(ID3D11DepthStencilView *dsv)
{
  if(dsv == NULL)
    return;

  UnbindRangeForWrite(GetResourceRange(dsv));
}

template <>
void D3D11RenderState::UnbindForWrite(ID3D11UnorderedAccessView *uav)
{
  if(uav == NULL)
    return;

  UnbindRangeForWrite(GetResourceRange(uav));
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11RenderState::InputAssembler &el)
{
  SERIALISE_MEMBER(Layout);
  SERIALISE_MEMBER(Topo);
  SERIALISE_MEMBER(VBs);
  SERIALISE_MEMBER(Strides);
  SERIALISE_MEMBER(Offsets);
  SERIALISE_MEMBER(IndexBuffer);
  SERIALISE_MEMBER(IndexFormat);
  SERIALISE_MEMBER(IndexOffset);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11RenderState::Shader &el)
{
  SERIALISE_MEMBER(Object);
  SERIALISE_MEMBER(ConstantBuffers);
  SERIALISE_MEMBER(CBOffsets);
  SERIALISE_MEMBER(CBCounts);
  SERIALISE_MEMBER(SRVs);
  SERIALISE_MEMBER(Samplers);
  SERIALISE_MEMBER(Instances);
  SERIALISE_MEMBER(NumInstances);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11RenderState::StreamOut &el)
{
  SERIALISE_MEMBER(Buffers);
  SERIALISE_MEMBER(Offsets);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11RenderState::Rasterizer &el)
{
  SERIALISE_MEMBER(NumViews);
  SERIALISE_MEMBER(NumScissors);
  SERIALISE_MEMBER(Viewports);
  SERIALISE_MEMBER(Scissors);
  SERIALISE_MEMBER(State);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11RenderState::OutputMerger &el)
{
  SERIALISE_MEMBER(DepthStencilState);
  SERIALISE_MEMBER(StencRef);
  SERIALISE_MEMBER(BlendState);
  SERIALISE_MEMBER(BlendFactor);
  SERIALISE_MEMBER(SampleMask);
  SERIALISE_MEMBER(DepthView);
  SERIALISE_MEMBER(RenderTargets);
  SERIALISE_MEMBER(UAVStartSlot);
  SERIALISE_MEMBER(UAVs);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11RenderState &el)
{
  SERIALISE_MEMBER(IA);
  SERIALISE_MEMBER(VS);
  SERIALISE_MEMBER(HS);
  SERIALISE_MEMBER(DS);
  SERIALISE_MEMBER(GS);
  SERIALISE_MEMBER(PS);
  SERIALISE_MEMBER(CS);
  SERIALISE_MEMBER(CSUAVs);
  SERIALISE_MEMBER(SO);
  SERIALISE_MEMBER(RS);
  SERIALISE_MEMBER(OM);
  SERIALISE_MEMBER(Predicate);
  SERIALISE_MEMBER(PredicateValue);

  if(ser.IsReading())
    el.AddRefs();
}

INSTANTIATE_SERIALISE_TYPE(D3D11RenderState);

D3D11RenderStateTracker::D3D11RenderStateTracker(WrappedID3D11DeviceContext *ctx)
    : m_RS(*ctx->GetCurrentPipelineState())
{
  m_pContext = ctx;
}

D3D11RenderStateTracker::~D3D11RenderStateTracker()
{
  m_RS.ApplyState(m_pContext);
}
