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

#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_resources.h"

D3D11RenderState::D3D11RenderState(Serialiser *ser)
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
  Clear();
  m_pSerialiser = ser;

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

  shader *stages[] = {&VS, &HS, &DS, &GS, &PS, &CS};
  for(int s = 0; s < 6; s++)
  {
    shader *sh = stages[s];

    ReleaseRef(sh->Shader);

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
}

void D3D11RenderState::MarkDirty(WrappedID3D11DeviceContext *ctx) const
{
  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    ID3D11Resource *res = NULL;
    if(CSUAVs[i])
    {
      CSUAVs[i]->GetResource(&res);
      ctx->MarkDirtyResource(GetIDForResource(res));
      SAFE_RELEASE(res);
    }
  }

  for(UINT i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
    ctx->MarkDirtyResource(GetIDForResource(SO.Buffers[i]));

  for(UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    ID3D11Resource *res = NULL;
    if(OM.RenderTargets[i])
    {
      OM.RenderTargets[i]->GetResource(&res);
      ctx->MarkDirtyResource(GetIDForResource(res));
      SAFE_RELEASE(res);
    }
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    ID3D11Resource *res = NULL;
    if(OM.UAVs[i])
    {
      OM.UAVs[i]->GetResource(&res);
      ctx->MarkDirtyResource(GetIDForResource(res));
      SAFE_RELEASE(res);
    }
  }

  {
    ID3D11Resource *res = NULL;
    if(OM.DepthView)
    {
      OM.DepthView->GetResource(&res);
      ctx->MarkDirtyResource(GetIDForResource(res));
      SAFE_RELEASE(res);
    }
  }
}

void D3D11RenderState::MarkReferenced(WrappedID3D11DeviceContext *ctx, bool initial) const
{
  ctx->MarkResourceReferenced(GetIDForResource(IA.Layout),
                              initial ? eFrameRef_Unknown : eFrameRef_Read);

  ctx->MarkResourceReferenced(GetIDForResource(IA.IndexBuffer),
                              initial ? eFrameRef_Unknown : eFrameRef_Read);

  for(UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
    ctx->MarkResourceReferenced(GetIDForResource(IA.VBs[i]),
                                initial ? eFrameRef_Unknown : eFrameRef_Read);

  const shader *stages[] = {&VS, &HS, &DS, &GS, &PS, &CS};
  for(int s = 0; s < 6; s++)
  {
    const shader *sh = stages[s];

    ctx->MarkResourceReferenced(GetIDForResource(sh->Shader),
                                initial ? eFrameRef_Unknown : eFrameRef_Read);

    for(UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
      ctx->MarkResourceReferenced(GetIDForResource(sh->ConstantBuffers[i]),
                                  initial ? eFrameRef_Unknown : eFrameRef_Read);

    for(UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
      ID3D11Resource *res = NULL;
      if(sh->SRVs[i])
      {
        sh->SRVs[i]->GetResource(&res);
        ctx->MarkResourceReferenced(GetIDForResource(sh->SRVs[i]),
                                    initial ? eFrameRef_Unknown : eFrameRef_Read);
        ctx->MarkResourceReferenced(GetIDForResource(res),
                                    initial ? eFrameRef_Unknown : eFrameRef_Read);
        SAFE_RELEASE(res);
      }
    }

    sh++;
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    ID3D11Resource *res = NULL;
    if(CSUAVs[i])
    {
      CSUAVs[i]->GetResource(&res);
      ctx->m_MissingTracks.insert(GetIDForResource(res));
      // UAVs we always assume to be partial updates
      ctx->MarkResourceReferenced(GetIDForResource(CSUAVs[i]),
                                  initial ? eFrameRef_Unknown : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetIDForResource(CSUAVs[i]),
                                  initial ? eFrameRef_Unknown : eFrameRef_Write);
      ctx->MarkResourceReferenced(GetIDForResource(res),
                                  initial ? eFrameRef_Unknown : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetIDForResource(res),
                                  initial ? eFrameRef_Unknown : eFrameRef_Write);
      SAFE_RELEASE(res);
    }
  }

  for(UINT i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
    ctx->MarkResourceReferenced(GetIDForResource(SO.Buffers[i]),
                                initial ? eFrameRef_Unknown : eFrameRef_Write);

  // tracks the min region of the enabled viewports plus scissors, to see if we could potentially
  // partially-update a render target (ie. we know for sure that we are only
  // writing to a region in one of the viewports). In this case we mark the
  // RT/DSV as read-write instead of just write, for initial state tracking.
  D3D11_RECT viewportScissorMin = {0, 0, 0xfffffff, 0xfffffff};

  D3D11_RASTERIZER_DESC rsdesc;
  RDCEraseEl(rsdesc);
  rsdesc.ScissorEnable = FALSE;
  if(RS.State)
    RS.State->GetDesc(&rsdesc);

  for(UINT v = 0; v < RS.NumViews; v++)
  {
    D3D11_RECT scissor = {(LONG)RS.Viewports[v].TopLeftX, (LONG)RS.Viewports[v].TopLeftY,
                          (LONG)RS.Viewports[v].Width, (LONG)RS.Viewports[v].Height};

    // scissor (if set) is relative to matching viewport)
    if(v < RS.NumScissors && rsdesc.ScissorEnable)
    {
      scissor.left += RS.Scissors[v].left;
      scissor.top += RS.Scissors[v].top;
      scissor.right = RDCMIN(scissor.right, RS.Scissors[v].right - RS.Scissors[v].left);
      scissor.bottom = RDCMIN(scissor.bottom, RS.Scissors[v].bottom - RS.Scissors[v].top);
    }

    viewportScissorMin.left = RDCMAX(viewportScissorMin.left, scissor.left);
    viewportScissorMin.top = RDCMAX(viewportScissorMin.top, scissor.top);
    viewportScissorMin.right = RDCMIN(viewportScissorMin.right, scissor.right);
    viewportScissorMin.bottom = RDCMIN(viewportScissorMin.bottom, scissor.bottom);
  }

  bool viewportScissorPartial = false;

  if(viewportScissorMin.left > 0 || viewportScissorMin.top > 0)
  {
    viewportScissorPartial = true;
  }
  else
  {
    ID3D11Resource *res = NULL;
    if(OM.RenderTargets[0])
      OM.RenderTargets[0]->GetResource(&res);
    else if(OM.DepthView)
      OM.DepthView->GetResource(&res);

    if(res)
    {
      D3D11_RESOURCE_DIMENSION dim;
      res->GetType(&dim);

      if(dim == D3D11_RESOURCE_DIMENSION_BUFFER)
      {
        // assume partial
        viewportScissorPartial = true;
      }
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
      {
        D3D11_TEXTURE1D_DESC desc;
        ((ID3D11Texture1D *)res)->GetDesc(&desc);

        if(viewportScissorMin.right < (LONG)desc.Width)
          viewportScissorPartial = true;
      }
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
      {
        D3D11_TEXTURE2D_DESC desc;
        ((ID3D11Texture2D *)res)->GetDesc(&desc);

        if(viewportScissorMin.right < (LONG)desc.Width ||
           viewportScissorMin.bottom < (LONG)desc.Height)
          viewportScissorPartial = true;
      }
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
      {
        D3D11_TEXTURE3D_DESC desc;
        ((ID3D11Texture3D *)res)->GetDesc(&desc);

        if(viewportScissorMin.right < (LONG)desc.Width ||
           viewportScissorMin.bottom < (LONG)desc.Height)
          viewportScissorPartial = true;
      }
    }

    SAFE_RELEASE(res);
  }

  for(UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    ID3D11Resource *res = NULL;
    if(OM.RenderTargets[i])
    {
      OM.RenderTargets[i]->GetResource(&res);
      ctx->m_MissingTracks.insert(GetIDForResource(res));
      ctx->MarkResourceReferenced(GetIDForResource(OM.RenderTargets[i]),
                                  initial ? eFrameRef_Unknown : eFrameRef_Read);
      if(viewportScissorPartial)
        ctx->MarkResourceReferenced(GetIDForResource(res),
                                    initial ? eFrameRef_Unknown : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetIDForResource(res),
                                  initial ? eFrameRef_Unknown : eFrameRef_Write);
      SAFE_RELEASE(res);
    }
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    ID3D11Resource *res = NULL;
    if(OM.UAVs[i])
    {
      OM.UAVs[i]->GetResource(&res);
      ctx->m_MissingTracks.insert(GetIDForResource(res));
      // UAVs we always assume to be partial updates
      ctx->MarkResourceReferenced(GetIDForResource(OM.UAVs[i]),
                                  initial ? eFrameRef_Unknown : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetIDForResource(OM.UAVs[i]),
                                  initial ? eFrameRef_Unknown : eFrameRef_Write);
      ctx->MarkResourceReferenced(GetIDForResource(res),
                                  initial ? eFrameRef_Unknown : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetIDForResource(res),
                                  initial ? eFrameRef_Unknown : eFrameRef_Write);
      SAFE_RELEASE(res);
    }
  }

  {
    ID3D11Resource *res = NULL;
    if(OM.DepthView)
    {
      OM.DepthView->GetResource(&res);
      ctx->m_MissingTracks.insert(GetIDForResource(res));
      ctx->MarkResourceReferenced(GetIDForResource(OM.DepthView),
                                  initial ? eFrameRef_Unknown : eFrameRef_Read);
      if(viewportScissorPartial)
        ctx->MarkResourceReferenced(GetIDForResource(res),
                                    initial ? eFrameRef_Unknown : eFrameRef_Read);
      ctx->MarkResourceReferenced(GetIDForResource(res),
                                  initial ? eFrameRef_Unknown : eFrameRef_Write);
      SAFE_RELEASE(res);
    }
  }
}

void D3D11RenderState::AddRefs()
{
  TakeRef(IA.IndexBuffer);
  TakeRef(IA.Layout);

  for(UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
    TakeRef(IA.VBs[i]);

  shader *stages[] = {&VS, &HS, &DS, &GS, &PS, &CS};
  for(int s = 0; s < 6; s++)
  {
    shader *sh = stages[s];

    TakeRef(sh->Shader);

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
}

void D3D11RenderState::Serialise(LogState m_State, WrappedID3D11Device *device)
{
  SERIALISE_ELEMENT(ResourceId, IALayout, GetIDForResource(IA.Layout));
  if(m_State < WRITING)
  {
    if(device->GetResourceManager()->HasLiveResource(IALayout))
      IA.Layout = (ID3D11InputLayout *)device->GetResourceManager()->GetLiveResource(IALayout);
    else
      IA.Layout = NULL;
  }

  m_pSerialiser->Serialise("IA.Topo", IA.Topo);

  SERIALISE_ELEMENT(ResourceId, IAIndexBuffer, GetIDForResource(IA.IndexBuffer));
  if(m_State < WRITING)
  {
    if(device->GetResourceManager()->HasLiveResource(IAIndexBuffer))
      IA.IndexBuffer = (ID3D11Buffer *)device->GetResourceManager()->GetLiveResource(IAIndexBuffer);
    else
      IA.IndexBuffer = NULL;
  }

  for(int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
  {
    ResourceId VB;
    if(m_State >= WRITING)
      VB = GetIDForResource(IA.VBs[i]);
    m_pSerialiser->Serialise("IA.VBs", VB);
    if(m_State < WRITING)
    {
      if(device->GetResourceManager()->HasLiveResource(VB))
        IA.VBs[i] = (ID3D11Buffer *)device->GetResourceManager()->GetLiveResource(VB);
      else
        IA.VBs[i] = NULL;
    }
  }

  m_pSerialiser->SerialisePODArray<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT>("IA.Strides",
                                                                              IA.Strides);
  m_pSerialiser->SerialisePODArray<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT>("IA.Offsets",
                                                                              IA.Offsets);
  m_pSerialiser->Serialise("IA.indexFormat", IA.IndexFormat);
  m_pSerialiser->Serialise("IA.indexOffset", IA.IndexOffset);

#define MAKE_NAMES(suffix)                                                                  \
  const char *CONCAT(suffix, _names)[] = {"VS." STRINGIZE(suffix), "HS." STRINGIZE(suffix), \
                                          "DS." STRINGIZE(suffix), "GS." STRINGIZE(suffix), \
                                          "PS." STRINGIZE(suffix), "CS." STRINGIZE(suffix)};

  MAKE_NAMES(ConstantBuffers);
  MAKE_NAMES(CBOffsets);
  MAKE_NAMES(CBCounts);
  MAKE_NAMES(Samplers);
  MAKE_NAMES(SRVs);
  MAKE_NAMES(Instances);

#undef MAKE_NAMES

  shader *stages[] = {&VS, &HS, &DS, &GS, &PS, &CS};
  for(int s = 0; s < 6; s++)
  {
    shader *sh = stages[s];

    SERIALISE_ELEMENT(ResourceId, Shader, GetIDForResource(sh->Shader));
    if(m_State < WRITING)
    {
      if(device->GetResourceManager()->HasLiveResource(Shader))
        sh->Shader = (ID3D11DeviceChild *)device->GetResourceManager()->GetLiveResource(Shader);
      else
        sh->Shader = NULL;
    }

    for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    {
      ResourceId id;
      if(m_State >= WRITING)
        id = GetIDForResource(sh->ConstantBuffers[i]);
      m_pSerialiser->Serialise(ConstantBuffers_names[s], id);
      if(m_State < WRITING)
      {
        if(device->GetResourceManager()->HasLiveResource(id))
          sh->ConstantBuffers[i] = (ID3D11Buffer *)device->GetResourceManager()->GetLiveResource(id);
        else
          sh->ConstantBuffers[i] = NULL;
      }

      m_pSerialiser->Serialise(CBOffsets_names[s], sh->CBOffsets[i]);
      m_pSerialiser->Serialise(CBCounts_names[s], sh->CBCounts[i]);
    }

    for(int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++)
    {
      ResourceId id;
      if(m_State >= WRITING)
        id = GetIDForResource(sh->Samplers[i]);
      m_pSerialiser->Serialise(Samplers_names[s], id);
      if(m_State < WRITING)
      {
        if(device->GetResourceManager()->HasLiveResource(id))
          sh->Samplers[i] = (ID3D11SamplerState *)device->GetResourceManager()->GetLiveResource(id);
        else
          sh->Samplers[i] = NULL;
      }
    }

    for(int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
      ResourceId id;
      if(m_State >= WRITING)
        id = GetIDForResource(sh->SRVs[i]);
      m_pSerialiser->Serialise(SRVs_names[s], id);
      if(m_State < WRITING)
      {
        if(device->GetResourceManager()->HasLiveResource(id))
          sh->SRVs[i] = (ID3D11ShaderResourceView *)device->GetResourceManager()->GetLiveResource(id);
        else
          sh->SRVs[i] = NULL;
      }
    }

    // Before 0x000008 the UAVs were serialised per-shader (even though it was only for compute)
    // here
    if(device->GetLogVersion() < 0x000008)
    {
      for(int i = 0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
      {
        ResourceId id;
        m_pSerialiser->Serialise("CSUAVs", id);

        if(s == 5)
        {
          if(device->GetResourceManager()->HasLiveResource(id))
            CSUAVs[i] =
                (ID3D11UnorderedAccessView *)device->GetResourceManager()->GetLiveResource(id);
          else
            CSUAVs[i] = NULL;
        }
      }
    }

    for(int i = 0; i < D3D11_SHADER_MAX_INTERFACES; i++)
    {
      ResourceId id;
      if(m_State >= WRITING)
        id = GetIDForResource(sh->Instances[i]);
      m_pSerialiser->Serialise(Instances_names[s], id);
      if(m_State < WRITING)
      {
        if(device->GetResourceManager()->HasLiveResource(id))
          sh->Instances[i] = (ID3D11ClassInstance *)device->GetResourceManager()->GetLiveResource(id);
        else
          sh->Instances[i] = NULL;
      }
    }

    sh++;
  }

  if(device->GetLogVersion() >= 0x000008)
  {
    for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
    {
      ResourceId id;
      if(m_State >= WRITING)
        id = GetIDForResource(CSUAVs[i]);
      m_pSerialiser->Serialise("CSUAVs", id);
      if(m_State < WRITING)
      {
        if(device->GetResourceManager()->HasLiveResource(id))
          CSUAVs[i] = (ID3D11UnorderedAccessView *)device->GetResourceManager()->GetLiveResource(id);
        else
          CSUAVs[i] = NULL;
      }
    }
  }

  for(int i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
  {
    ResourceId id;
    if(m_State >= WRITING)
      id = GetIDForResource(SO.Buffers[i]);
    m_pSerialiser->Serialise("SO.Buffers", id);
    if(m_State < WRITING)
    {
      if(device->GetResourceManager()->HasLiveResource(id))
        SO.Buffers[i] = (ID3D11Buffer *)device->GetResourceManager()->GetLiveResource(id);
      else
        SO.Buffers[i] = NULL;
    }

    m_pSerialiser->Serialise("SO.Offsets", SO.Offsets[i]);
  }

  SERIALISE_ELEMENT(ResourceId, RSState, GetIDForResource(RS.State));
  if(m_State < WRITING)
  {
    if(device->GetResourceManager()->HasLiveResource(RSState))
      RS.State = (ID3D11RasterizerState *)device->GetResourceManager()->GetLiveResource(RSState);
    else
      RS.State = NULL;
  }

  m_pSerialiser->Serialise("RS.NumViews", RS.NumViews);
  m_pSerialiser->Serialise("RS.NumScissors", RS.NumScissors);
  m_pSerialiser->SerialisePODArray<D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>(
      "RS.Viewports", RS.Viewports);
  m_pSerialiser->SerialisePODArray<D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>(
      "RS.Scissors", RS.Scissors);

  SERIALISE_ELEMENT(ResourceId, OMDepthStencilState, GetIDForResource(OM.DepthStencilState));
  if(m_State < WRITING)
  {
    if(device->GetResourceManager()->HasLiveResource(OMDepthStencilState))
      OM.DepthStencilState = (ID3D11DepthStencilState *)device->GetResourceManager()->GetLiveResource(
          OMDepthStencilState);
    else
      OM.DepthStencilState = NULL;
  }

  m_pSerialiser->Serialise("OM.StencRef", OM.StencRef);

  SERIALISE_ELEMENT(ResourceId, OMBlendState, GetIDForResource(OM.BlendState));
  if(m_State < WRITING)
  {
    if(device->GetResourceManager()->HasLiveResource(OMBlendState))
      OM.BlendState = (ID3D11BlendState *)device->GetResourceManager()->GetLiveResource(OMBlendState);
    else
      OM.BlendState = NULL;
  }

  m_pSerialiser->SerialisePODArray<4>("OM.BlendFactor", OM.BlendFactor);
  m_pSerialiser->Serialise("OM.SampleMask", OM.SampleMask);

  SERIALISE_ELEMENT(ResourceId, OMDepthView, GetIDForResource(OM.DepthView));
  if(m_State < WRITING)
  {
    if(device->GetResourceManager()->HasLiveResource(OMDepthView))
      OM.DepthView =
          (ID3D11DepthStencilView *)device->GetResourceManager()->GetLiveResource(OMDepthView);
    else
      OM.DepthView = NULL;
  }

  m_pSerialiser->Serialise("OM.UAVStartSlot", OM.UAVStartSlot);

  const int numUAVs =
      device->GetLogVersion() >= 0x000008 ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;

  for(int i = 0; i < numUAVs; i++)
  {
    ResourceId UAV;
    if(m_State >= WRITING)
      UAV = GetIDForResource(OM.UAVs[i]);
    m_pSerialiser->Serialise("OM.UAVs", UAV);
    if(m_State < WRITING)
    {
      if(device->GetResourceManager()->HasLiveResource(UAV))
        OM.UAVs[i] = (ID3D11UnorderedAccessView *)device->GetResourceManager()->GetLiveResource(UAV);
      else
        OM.UAVs[i] = NULL;
    }
  }

  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    ResourceId RTV;
    if(m_State >= WRITING)
      RTV = GetIDForResource(OM.RenderTargets[i]);
    m_pSerialiser->Serialise("OM.RenderTargets", RTV);
    if(m_State < WRITING)
    {
      if(device->GetResourceManager()->HasLiveResource(RTV))
        OM.RenderTargets[i] =
            (ID3D11RenderTargetView *)device->GetResourceManager()->GetLiveResource(RTV);
      else
        OM.RenderTargets[i] = NULL;
    }
  }

  if(m_State < WRITING)
    AddRefs();
}

D3D11RenderState::D3D11RenderState(WrappedID3D11DeviceContext *context)
{
  RDCEraseMem(this, sizeof(D3D11RenderState));
  m_pSerialiser = context->GetSerialiser();

  // IA
  context->IAGetInputLayout(&IA.Layout);
  context->IAGetPrimitiveTopology(&IA.Topo);
  context->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, IA.VBs,
                              (UINT *)IA.Strides, (UINT *)IA.Offsets);
  context->IAGetIndexBuffer(&IA.IndexBuffer, &IA.IndexFormat, &IA.IndexOffset);

  // VS
  context->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, VS.SRVs);
  context->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, VS.Samplers);
  context->VSGetShader((ID3D11VertexShader **)&VS.Shader, VS.Instances, &VS.NumInstances);

  // DS
  context->DSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, DS.SRVs);
  context->DSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, DS.Samplers);
  context->DSGetShader((ID3D11DomainShader **)&DS.Shader, DS.Instances, &DS.NumInstances);

  // HS
  context->HSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, HS.SRVs);
  context->HSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, HS.Samplers);
  context->HSGetShader((ID3D11HullShader **)&HS.Shader, HS.Instances, &HS.NumInstances);

  // GS
  context->GSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, GS.SRVs);
  context->GSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, GS.Samplers);
  context->GSGetShader((ID3D11GeometryShader **)&GS.Shader, GS.Instances, &GS.NumInstances);

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
  context->CSGetShader((ID3D11ComputeShader **)&CS.Shader, CS.Instances, &CS.NumInstances);

  // PS
  context->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, PS.SRVs);
  context->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, PS.Samplers);
  context->PSGetShader((ID3D11PixelShader **)&PS.Shader, PS.Instances, &PS.NumInstances);

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
}

void D3D11RenderState::Clear()
{
  ReleaseRefs();
  OM.BlendFactor[0] = OM.BlendFactor[1] = OM.BlendFactor[2] = OM.BlendFactor[3] = 1.0f;
  OM.SampleMask = 0xffffffff;

  for(size_t i = 0; i < ARRAY_COUNT(VS.CBCounts); i++)
    VS.CBCounts[i] = HS.CBCounts[i] = DS.CBCounts[i] = GS.CBCounts[i] = PS.CBCounts[i] =
        CS.CBCounts[i] = 4096;
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
  context->VSSetShader((ID3D11VertexShader *)VS.Shader, VS.Instances, VS.NumInstances);

  // DS
  context->DSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, DS.SRVs);
  context->DSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, DS.Samplers);
  context->DSSetShader((ID3D11DomainShader *)DS.Shader, DS.Instances, DS.NumInstances);

  // HS
  context->HSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, HS.SRVs);
  context->HSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, HS.Samplers);
  context->HSSetShader((ID3D11HullShader *)HS.Shader, HS.Instances, HS.NumInstances);

  // GS
  context->GSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, GS.SRVs);
  context->GSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, GS.Samplers);
  context->GSSetShader((ID3D11GeometryShader *)GS.Shader, GS.Instances, GS.NumInstances);

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
  context->CSSetShader((ID3D11ComputeShader *)CS.Shader, CS.Instances, CS.NumInstances);

  // PS
  context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, PS.SRVs);
  context->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, PS.Samplers);
  context->PSSetShader((ID3D11PixelShader *)PS.Shader, PS.Instances, PS.NumInstances);

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
        OM.UAVStartSlot, OM.RenderTargets, OM.DepthView, OM.UAVStartSlot,
        D3D11_1_UAV_SLOT_COUNT - OM.UAVStartSlot, OM.UAVs, UAV_keepcounts);
  else
    context->OMSetRenderTargetsAndUnorderedAccessViews(
        OM.UAVStartSlot, OM.RenderTargets, OM.DepthView, OM.UAVStartSlot,
        D3D11_PS_CS_UAV_REGISTER_COUNT - OM.UAVStartSlot, OM.UAVs, UAV_keepcounts);
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

bool D3D11RenderState::IsBoundIUnknownForWrite(const ResourceRange &range, bool readDepthOnly,
                                               bool readStencilOnly)
{
  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(range.Intersects(ResourceRange(CSUAVs[i])))
    {
      // RDCDEBUG("Resource was bound on CS UAV %u", i);
      return true;
    }
  }

  for(UINT i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
  {
    if(range.Intersects(ResourceRange(SO.Buffers[i])))
    {
      // RDCDEBUG("Resource was bound on SO buffer %u", i);
      return true;
    }
  }

  for(UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(range.Intersects(ResourceRange(OM.RenderTargets[i])))
    {
      // RDCDEBUG("Resource was bound on RTV %u", i);
      return true;
    }
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(range.Intersects(ResourceRange(OM.UAVs[i])))
    {
      // RDCDEBUG("Resource was bound on OM UAV %d", i);
      return true;
    }
  }

  {
    UINT depthFlags = 0;

    if(OM.DepthView)
    {
      D3D11_DEPTH_STENCIL_VIEW_DESC d;
      OM.DepthView->GetDesc(&d);

      depthFlags = d.Flags;
    }

    if(range.Intersects(ResourceRange(OM.DepthView)))
    {
      // RDCDEBUG("Resource was bound on OM DSV");

      if(depthFlags == (D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL))
      {
        // RDCDEBUG("but it's a readonly DSV, so that's fine");
      }
      else if(depthFlags == D3D11_DSV_READ_ONLY_DEPTH && readDepthOnly)
      {
        // RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
      }
      else if(depthFlags == D3D11_DSV_READ_ONLY_STENCIL && readStencilOnly)
      {
        // RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
      }
      else
      {
        return true;
      }
    }
  }

  return false;
}

void D3D11RenderState::UnbindIUnknownForWrite(const ResourceRange &range)
{
  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(range.Intersects(ResourceRange(CSUAVs[i])))
    {
      ReleaseRef(CSUAVs[i]);
      CSUAVs[i] = NULL;
    }
  }

  for(UINT i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
  {
    if(range.Intersects(ResourceRange(SO.Buffers[i])))
    {
      ReleaseRef(SO.Buffers[i]);
      SO.Buffers[i] = NULL;
    }
  }

  for(UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(range.Intersects(ResourceRange(OM.RenderTargets[i])))
    {
      ReleaseRef(OM.RenderTargets[i]);
      OM.RenderTargets[i] = NULL;
    }
  }

  for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(range.Intersects(ResourceRange(OM.UAVs[i])))
    {
      ReleaseRef(OM.UAVs[i]);
      OM.UAVs[i] = NULL;
    }
  }

  if(range.Intersects(ResourceRange(OM.DepthView)))
  {
    ReleaseRef(OM.DepthView);
    OM.DepthView = NULL;
  }
}

void D3D11RenderState::UnbindIUnknownForRead(const ResourceRange &range, bool allowDepthOnly,
                                             bool allowStencilOnly)
{
  for(int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
  {
    if(range.Intersects(ResourceRange(IA.VBs[i])))
    {
      // RDCDEBUG("Resource was bound on IA VB %u", i);
      ReleaseRef(IA.VBs[i]);
      IA.VBs[i] = NULL;
    }
  }

  if(range.Intersects(ResourceRange(IA.IndexBuffer)))
  {
    // RDCDEBUG("Resource was bound on IA IB");
    ReleaseRef(IA.IndexBuffer);
    IA.IndexBuffer = NULL;
  }

  // const char *names[] = { "VS", "DS", "HS", "GS", "PS", "CS" };
  shader *stages[] = {&VS, &HS, &DS, &GS, &PS, &CS};
  for(int s = 0; s < 6; s++)
  {
    shader *sh = stages[s];

    for(UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    {
      if(range.Intersects(ResourceRange(sh->ConstantBuffers[i])))
      {
        // RDCDEBUG("Resource was bound on %s CB %u", names[s], i);
        ReleaseRef(sh->ConstantBuffers[i]);
        sh->ConstantBuffers[i] = NULL;
      }
    }

    for(UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
      bool readDepthOnly = false;
      bool readStencilOnly = false;

      D3D11_RESOURCE_DIMENSION dim;
      DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

      ID3D11Resource *res = NULL;
      // we only need to fetch the information about depth/stencil
      // read-only status if we're actually going to care about it.
      if(sh->SRVs[i] && (allowDepthOnly || allowStencilOnly))
      {
        sh->SRVs[i]->GetResource(&res);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        sh->SRVs[i]->GetDesc(&srvDesc);

        fmt = srvDesc.Format;

        res->GetType(&dim);

        if(fmt == DXGI_FORMAT_UNKNOWN)
        {
          if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
          {
            D3D11_TEXTURE1D_DESC d;
            ((ID3D11Texture1D *)res)->GetDesc(&d);

            fmt = d.Format;
          }
          else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
          {
            D3D11_TEXTURE2D_DESC d;
            ((ID3D11Texture2D *)res)->GetDesc(&d);

            fmt = d.Format;
          }
        }

        if(fmt == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT || fmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
        {
          readStencilOnly = true;
        }
        else if(fmt == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
                fmt == DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
        {
          readDepthOnly = true;
        }
        else
        {
          fmt = GetTypelessFormat(fmt);

          // any format that could be depth-only, treat it as reading depth only.
          // this only applies for conflicts detected with the depth target.
          if(fmt == DXGI_FORMAT_R32_TYPELESS || fmt == DXGI_FORMAT_R16_TYPELESS)
          {
            readDepthOnly = true;
          }
        }

        SAFE_RELEASE(res);
      }

      if(range.Intersects(ResourceRange(sh->SRVs[i])))
      {
        // RDCDEBUG("Resource was bound on %s SRV %u", names[s], i);

        if(allowDepthOnly && readDepthOnly)
        {
          // RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
        }
        else if(allowStencilOnly && readStencilOnly)
        {
          // RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
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

bool D3D11RenderState::ValidOutputMerger(ID3D11RenderTargetView **RTs, ID3D11DepthStencilView *depth,
                                         ID3D11UnorderedAccessView **uavs)
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

  for(int i = 0; RTs && i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
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

    for(int i = 0; RTs && i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
      if(RTs[i])
        rtvRanges[i] = ResourceRange(RTs[i]);
      else
        break;
    }

    if(depth)
      depthRange = ResourceRange(depth);

    int numUAVs = 0;

    for(int i = 0; uavs && i < D3D11_1_UAV_SLOT_COUNT; i++)
    {
      if(uavs[i])
      {
        uavRanges[i] = ResourceRange(uavs[i]);
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

bool D3D11RenderState::inputassembler::Used_VB(WrappedID3D11Device *device, uint32_t slot) const
{
  if(Layout == NULL)
    return false;

  const vector<D3D11_INPUT_ELEMENT_DESC> &vec = device->GetLayoutDesc(Layout);

  for(size_t i = 0; i < vec.size(); i++)
    if(vec[i].InputSlot == slot)
      return true;

  return false;
}

bool D3D11RenderState::shader::Used_CB(uint32_t slot) const
{
  if(ConstantBuffers[slot] == NULL)
    return false;

  WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)Shader;

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

bool D3D11RenderState::shader::Used_SRV(uint32_t slot) const
{
  if(SRVs[slot] == NULL)
    return false;

  WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)Shader;

  if(shad == NULL)
    return false;

  DXBC::DXBCFile *dxbc = shad->GetDXBC();

  // have to assume it's used if there's no DXBC
  if(dxbc == NULL)
    return true;

  for(size_t i = 0; i < dxbc->m_Resources.size(); i++)
  {
    if(dxbc->m_Resources[i].reg == slot &&
       (dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_TEXTURE ||
        dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_STRUCTURED ||
        dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_TBUFFER ||
        dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_BYTEADDRESS))
    {
      return true;
    }
  }

  return false;
}

bool D3D11RenderState::shader::Used_UAV(uint32_t slot) const
{
  WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)Shader;

  if(shad == NULL)
    return false;

  DXBC::DXBCFile *dxbc = shad->GetDXBC();

  // have to assume it's used if there's no DXBC
  if(dxbc == NULL)
    return true;

  for(size_t i = 0; i < dxbc->m_Resources.size(); i++)
  {
    if(dxbc->m_Resources[i].reg == slot &&
       (dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_APPEND_STRUCTURED ||
        dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_CONSUME_STRUCTURED ||
        dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_RWBYTEADDRESS ||
        dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED ||
        dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER ||
        dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_RWTYPED))
    {
      return true;
    }
  }

  return false;
}

D3D11RenderStateTracker::D3D11RenderStateTracker(WrappedID3D11DeviceContext *ctx)
    : m_RS(*ctx->GetCurrentPipelineState())
{
  m_pContext = ctx;
}

D3D11RenderStateTracker::~D3D11RenderStateTracker()
{
  m_RS.ApplyState(m_pContext);
}

D3D11RenderState::ResourceRange D3D11RenderState::ResourceRange::Null =
    D3D11RenderState::ResourceRange(NULL, 0, 0);

D3D11RenderState::ResourceRange::ResourceRange(ID3D11ShaderResourceView *srv)
{
  minMip = minSlice = 0;

  if(srv == NULL)
  {
    resource = NULL;
    maxMip = maxSlice = ~0U;
    fullRange = true;
    return;
  }

  ID3D11Resource *res = NULL;
  srv->GetResource(&res);
  res->Release();
  resource = (IUnknown *)res;

  UINT numMips = ~0U, numSlices = ~0U;

  D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
  srv->GetDesc(&srvd);

  switch(srvd.ViewDimension)
  {
    case D3D11_SRV_DIMENSION_TEXTURE1D:
      minMip = srvd.Texture1D.MostDetailedMip;
      numMips = srvd.Texture1D.MipLevels;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
      minMip = srvd.Texture1DArray.MostDetailedMip;
      numMips = srvd.Texture1DArray.MipLevels;
      minSlice = srvd.Texture1DArray.FirstArraySlice;
      numSlices = srvd.Texture1DArray.ArraySize;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2D:
      minMip = srvd.Texture2D.MostDetailedMip;
      numMips = srvd.Texture2D.MipLevels;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
      minMip = srvd.Texture2DArray.MostDetailedMip;
      numMips = srvd.Texture2DArray.MipLevels;
      minSlice = srvd.Texture2DArray.FirstArraySlice;
      numSlices = srvd.Texture2DArray.ArraySize;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMS: break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
      minSlice = srvd.Texture2DMSArray.FirstArraySlice;
      numSlices = srvd.Texture2DMSArray.ArraySize;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE3D:
      minMip = srvd.Texture3D.MostDetailedMip;
      numMips = srvd.Texture3D.MipLevels;
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBE:
      minMip = srvd.TextureCube.MostDetailedMip;
      numMips = srvd.TextureCube.MipLevels;
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
      minMip = srvd.TextureCubeArray.MostDetailedMip;
      numMips = srvd.TextureCubeArray.MipLevels;
      minSlice = srvd.TextureCubeArray.First2DArrayFace;
      numSlices = srvd.TextureCubeArray.NumCubes * 6;
      break;
    case D3D11_SRV_DIMENSION_UNKNOWN:
    case D3D11_SRV_DIMENSION_BUFFER:
    case D3D11_SRV_DIMENSION_BUFFEREX: break;
  }

  SetMaxes(numMips, numSlices);
}

D3D11RenderState::ResourceRange::ResourceRange(ID3D11UnorderedAccessView *uav)
{
  minMip = minSlice = 0;

  if(uav == NULL)
  {
    resource = NULL;
    maxMip = maxSlice = ~0U;
    fullRange = true;
    return;
  }

  ID3D11Resource *res = NULL;
  uav->GetResource(&res);
  res->Release();
  resource = (IUnknown *)res;

  UINT numMips = ~0U, numSlices = ~0U;

  D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
  uav->GetDesc(&desc);

  switch(desc.ViewDimension)
  {
    case D3D11_UAV_DIMENSION_TEXTURE1D:
      minMip = desc.Texture1D.MipSlice;
      numMips = 1;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
      minMip = desc.Texture1DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture1DArray.FirstArraySlice;
      numSlices = desc.Texture1DArray.ArraySize;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2D:
      minMip = desc.Texture2D.MipSlice;
      numMips = 1;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      minMip = desc.Texture2DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture2DArray.FirstArraySlice;
      numSlices = desc.Texture2DArray.ArraySize;
      break;
    case D3D11_UAV_DIMENSION_TEXTURE3D:
      minMip = desc.Texture3D.MipSlice;
      numMips = 1;
      minSlice = desc.Texture3D.FirstWSlice;
      numSlices = desc.Texture3D.WSize;
      break;
    case D3D11_UAV_DIMENSION_UNKNOWN:
    case D3D11_UAV_DIMENSION_BUFFER: break;
  }

  SetMaxes(numMips, numSlices);
}

D3D11RenderState::ResourceRange::ResourceRange(ID3D11RenderTargetView *rtv)
{
  minMip = minSlice = 0;

  if(rtv == NULL)
  {
    resource = NULL;
    maxMip = maxSlice = ~0U;
    fullRange = true;
    return;
  }

  ID3D11Resource *res = NULL;
  rtv->GetResource(&res);
  res->Release();
  resource = (IUnknown *)res;

  UINT numMips = ~0U, numSlices = ~0U;

  D3D11_RENDER_TARGET_VIEW_DESC desc;
  rtv->GetDesc(&desc);

  switch(desc.ViewDimension)
  {
    case D3D11_RTV_DIMENSION_TEXTURE1D:
      minMip = desc.Texture1D.MipSlice;
      numMips = 1;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
      minMip = desc.Texture1DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture1DArray.FirstArraySlice;
      numSlices = desc.Texture1DArray.ArraySize;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2D:
      minMip = desc.Texture2D.MipSlice;
      numMips = 1;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
      minMip = desc.Texture2DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture2DArray.FirstArraySlice;
      numSlices = desc.Texture2DArray.ArraySize;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMS: break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
      minSlice = desc.Texture2DMSArray.FirstArraySlice;
      numSlices = desc.Texture2DMSArray.ArraySize;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE3D:
      minMip = desc.Texture3D.MipSlice;
      numMips = 1;
      minSlice = desc.Texture3D.FirstWSlice;
      numSlices = desc.Texture3D.WSize;
      break;
    case D3D11_RTV_DIMENSION_UNKNOWN:
    case D3D11_RTV_DIMENSION_BUFFER: break;
  }

  SetMaxes(numMips, numSlices);
}

D3D11RenderState::ResourceRange::ResourceRange(ID3D11DepthStencilView *dsv)
{
  minMip = minSlice = 0;

  if(dsv == NULL)
  {
    resource = NULL;
    maxMip = maxSlice = ~0U;
    fullRange = true;
    return;
  }

  ID3D11Resource *res = NULL;
  dsv->GetResource(&res);
  res->Release();
  resource = (IUnknown *)res;

  UINT numMips = ~0U, numSlices = ~0U;

  D3D11_DEPTH_STENCIL_VIEW_DESC desc;
  dsv->GetDesc(&desc);

  switch(desc.ViewDimension)
  {
    case D3D11_DSV_DIMENSION_TEXTURE1D:
      minMip = desc.Texture1D.MipSlice;
      numMips = 1;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
      minMip = desc.Texture1DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture1DArray.FirstArraySlice;
      numSlices = desc.Texture1DArray.ArraySize;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2D:
      minMip = desc.Texture2D.MipSlice;
      numMips = 1;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
      minMip = desc.Texture2DArray.MipSlice;
      numMips = 1;
      minSlice = desc.Texture2DArray.FirstArraySlice;
      numSlices = desc.Texture2DArray.ArraySize;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMS: break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
      minSlice = desc.Texture2DMSArray.FirstArraySlice;
      numSlices = desc.Texture2DMSArray.ArraySize;
      break;
    case D3D11_DSV_DIMENSION_UNKNOWN: break;
  }

  SetMaxes(numMips, numSlices);
}
