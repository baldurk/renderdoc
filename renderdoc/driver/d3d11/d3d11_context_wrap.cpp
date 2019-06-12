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

#include "d3d11_context.h"
#include "3rdparty/tinyfiledialogs/tinyfiledialogs.h"
#include "driver/dx/official/dxgi1_3.h"
#include "strings/string_utils.h"
#include "d3d11_debug.h"
#include "d3d11_renderstate.h"
#include "d3d11_resources.h"

#ifndef DXGI_ERROR_INVALID_CALL
#define DXGI_ERROR_INVALID_CALL MAKE_DXGI_HRESULT(1)
#endif

uint32_t NullCBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
uint32_t NullCBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

#pragma region D3DPERF

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_SetMarker(SerialiserType &ser, uint32_t Color,
                                                     const wchar_t *MarkerNameW)
{
  SERIALISE_ELEMENT(Color);
  SERIALISE_ELEMENT_LOCAL(MarkerName, StringFormat::Wide2UTF8(MarkerNameW ? MarkerNameW : L""));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D11MarkerRegion::Set(MarkerName);

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = MarkerName;
      draw.flags |= DrawFlags::SetMarker;

      byte alpha = (Color >> 24) & 0xff;
      byte red = (Color >> 16) & 0xff;
      byte green = (Color >> 8) & 0xff;
      byte blue = (Color >> 0) & 0xff;

      draw.markerColor[0] = float(red) / 255.0f;
      draw.markerColor[1] = float(green) / 255.0f;
      draw.markerColor[2] = float(blue) / 255.0f;
      draw.markerColor[3] = float(alpha) / 255.0f;

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_PushMarker(SerialiserType &ser, uint32_t Color,
                                                      const wchar_t *MarkerNameW)
{
  SERIALISE_ELEMENT(Color);
  SERIALISE_ELEMENT_LOCAL(MarkerName, StringFormat::Wide2UTF8(MarkerNameW ? MarkerNameW : L""));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D11MarkerRegion::Begin(MarkerName);
    m_pDevice->ReplayPushEvent();

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = MarkerName;
      draw.flags |= DrawFlags::PushMarker;

      byte alpha = (Color >> 24) & 0xff;
      byte red = (Color >> 16) & 0xff;
      byte green = (Color >> 8) & 0xff;
      byte blue = (Color >> 0) & 0xff;

      draw.markerColor[0] = float(red) / 255.0f;
      draw.markerColor[1] = float(green) / 255.0f;
      draw.markerColor[2] = float(blue) / 255.0f;
      draw.markerColor[3] = float(alpha) / 255.0f;

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_PopMarker(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    D3D11MarkerRegion::End();
    m_pDevice->ReplayPopEvent();

    if(IsLoading(m_State) && HasNonMarkerEvents())
    {
      DrawcallDescription draw;
      draw.name = "API Calls";
      draw.flags |= DrawFlags::APICalls;

      AddEvent();
      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::SetMarker(uint32_t Color, const wchar_t *MarkerName)
{
  SERIALISE_TIME_CALL();

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::SetMarker);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_SetMarker(GET_SERIALISER, Color, MarkerName);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

int WrappedID3D11DeviceContext::PushMarker(uint32_t Color, const wchar_t *MarkerName)
{
  SERIALISE_TIME_CALL();

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::PushMarker);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_PushMarker(GET_SERIALISER, Color, MarkerName);

    m_ContextRecord->AddChunk(scope.Get());
  }

  return m_MarkerIndentLevel++;
}

int WrappedID3D11DeviceContext::PopMarker()
{
  SERIALISE_TIME_CALL();

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::PopMarker);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_PopMarker(GET_SERIALISER);

    m_ContextRecord->AddChunk(scope.Get());
  }

  return --m_MarkerIndentLevel;
}

void WrappedID3D11DeviceContext::ThreadSafe_SetMarker(uint32_t col, const wchar_t *name)
{
  Annotation annot;
  annot.m_Type = Annotation::ANNOT_SETMARKER;
  annot.m_Col = col;
  annot.m_Name = name;

  {
    SCOPED_LOCK(m_AnnotLock);
    m_AnnotationQueue.push_back(annot);
  }
}

int WrappedID3D11DeviceContext::ThreadSafe_BeginEvent(uint32_t col, const wchar_t *name)
{
  Annotation annot;
  annot.m_Type = Annotation::ANNOT_BEGINEVENT;
  annot.m_Col = col;
  annot.m_Name = name;

  {
    SCOPED_LOCK(m_AnnotLock);
    m_AnnotationQueue.push_back(annot);
  }

  // not thread safe but I don't want to lock over access to this - if people use D3DPERF + MT
  // they shouldn't rely on this return value anyway :).
  return m_MarkerIndentLevel;
}

int WrappedID3D11DeviceContext::ThreadSafe_EndEvent()
{
  Annotation annot;
  annot.m_Type = Annotation::ANNOT_ENDEVENT;

  {
    SCOPED_LOCK(m_AnnotLock);
    m_AnnotationQueue.push_back(annot);
  }

  // not thread safe but I don't want to lock over access to this - if people use D3DPERF + MT
  // they shouldn't rely on this return value anyway :).
  return m_MarkerIndentLevel - 1;
}

void WrappedID3D11DeviceContext::DrainAnnotationQueue()
{
  if(!IsActiveCapturing(m_State))
    return;

  m_AnnotLock.Lock();

  // fastest possible early-out
  if(m_AnnotationQueue.empty())
  {
    m_AnnotLock.Unlock();
    return;
  }

  std::vector<Annotation> annotations;
  annotations.swap(m_AnnotationQueue);

  m_AnnotLock.Unlock();

  for(size_t i = 0; i < annotations.size(); i++)
  {
    const Annotation &a = annotations[i];

    switch(a.m_Type)
    {
      case Annotation::ANNOT_SETMARKER: SetMarker(a.m_Col, a.m_Name.c_str()); break;
      case Annotation::ANNOT_BEGINEVENT: PushMarker(a.m_Col, a.m_Name.c_str()); break;
      case Annotation::ANNOT_ENDEVENT: PopMarker(); break;
    }
  }
}

#pragma endregion D3DPERF

#pragma region Input Assembly

void WrappedID3D11DeviceContext::IAGetInputLayout(ID3D11InputLayout **ppInputLayout)
{
  if(ppInputLayout)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11InputLayout *real = NULL;
    m_pRealContext->IAGetInputLayout(&real);

    SAFE_RELEASE_NOCLEAR(real);
    *ppInputLayout = (ID3D11InputLayout *)m_pDevice->GetResourceManager()->GetWrapper(real);
    SAFE_ADDREF(*ppInputLayout);

    RDCASSERT(*ppInputLayout == m_CurrentPipelineState->IA.Layout);
  }
}

void WrappedID3D11DeviceContext::IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                                                    ID3D11Buffer **ppVertexBuffers, UINT *pStrides,
                                                    UINT *pOffsets)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11Buffer *real[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {0};
  m_pRealContext->IAGetVertexBuffers(StartSlot, NumBuffers, real, pStrides, pOffsets);

  for(UINT i = 0; i < NumBuffers; i++)
  {
    SAFE_RELEASE_NOCLEAR(real[i]);
    if(ppVertexBuffers)
    {
      ppVertexBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppVertexBuffers[i]);

      RDCASSERT(ppVertexBuffers[i] == m_CurrentPipelineState->IA.VBs[i + StartSlot]);
    }

    // D3D11 really inconsistently tracks these.
    // RDCASSERT(pStrides[i] == m_CurrentPipelineState->IA.Strides[i+StartSlot]);
    // RDCASSERT(pOffsets[i] == m_CurrentPipelineState->IA.Offsets[i+StartSlot]);
  }
}

void WrappedID3D11DeviceContext::IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format,
                                                  UINT *Offset)
{
  if(pIndexBuffer)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11Buffer *real = NULL;
    m_pRealContext->IAGetIndexBuffer(&real, Format, Offset);

    SAFE_RELEASE_NOCLEAR(real);
    *pIndexBuffer = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real);
    SAFE_ADDREF(*pIndexBuffer);

    RDCASSERT(*pIndexBuffer == m_CurrentPipelineState->IA.IndexBuffer);

    if(Format)
      RDCASSERT(*Format == m_CurrentPipelineState->IA.IndexFormat);
    if(Offset)
      RDCASSERT(*Offset == m_CurrentPipelineState->IA.IndexOffset);
  }
}

void WrappedID3D11DeviceContext::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_pRealContext->IAGetPrimitiveTopology(pTopology);
  if(pTopology)
    RDCASSERT(*pTopology == m_CurrentPipelineState->IA.Topo);
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_IASetPrimitiveTopology(SerialiserType &ser,
                                                                  D3D11_PRIMITIVE_TOPOLOGY Topology)
{
  SERIALISE_ELEMENT(Topology);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Topo, Topology);
    m_pRealContext->IASetPrimitiveTopology(Topology);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->IASetPrimitiveTopology(Topology));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::IASetPrimitiveTopology);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_IASetPrimitiveTopology(GET_SERIALISER, Topology);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Topo, Topology);
  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_IASetInputLayout(SerialiserType &ser,
                                                            ID3D11InputLayout *pInputLayout)
{
  SERIALISE_ELEMENT(pInputLayout);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordLayoutBindStats(pInputLayout);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.Layout, pInputLayout);
    m_pRealContext->IASetInputLayout(UNWRAP(WrappedID3D11InputLayout, pInputLayout));
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::IASetInputLayout(ID3D11InputLayout *pInputLayout)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(
      m_pRealContext->IASetInputLayout(UNWRAP(WrappedID3D11InputLayout, pInputLayout)));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::IASetInputLayout);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_IASetInputLayout(GET_SERIALISER, pInputLayout);

    MarkResourceReferenced(GetIDForResource(pInputLayout), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.Layout, pInputLayout);
  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_IASetVertexBuffers(SerialiserType &ser, UINT StartSlot,
                                                              UINT NumBuffers,
                                                              ID3D11Buffer *const *ppVertexBuffers,
                                                              const UINT *pStrides,
                                                              const UINT *pOffsets)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppVertexBuffers, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pStrides, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pOffsets, NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordVertexBindStats(NumBuffers, ppVertexBuffers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.VBs, ppVertexBuffers,
                                          StartSlot, NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Strides, pStrides, StartSlot,
                                   NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Offsets, pOffsets, StartSlot,
                                   NumBuffers);

    ID3D11Buffer *bufs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    for(UINT i = 0; i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppVertexBuffers[i]);

    m_pRealContext->IASetVertexBuffers(StartSlot, NumBuffers, bufs, pStrides, pOffsets);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::IASetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                                                    ID3D11Buffer *const *ppVertexBuffers,
                                                    const UINT *pStrides, const UINT *pOffsets)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11Buffer *bufs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppVertexBuffers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppVertexBuffers[i]), eFrameRef_Read);
    bufs[i] = UNWRAP(WrappedID3D11Buffer, ppVertexBuffers[i]);
  }

  SERIALISE_TIME_CALL(
      m_pRealContext->IASetVertexBuffers(StartSlot, NumBuffers, bufs, pStrides, pOffsets));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::IASetVertexBuffers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_IASetVertexBuffers(GET_SERIALISER, StartSlot, NumBuffers, ppVertexBuffers, pStrides,
                                 pOffsets);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.VBs, ppVertexBuffers, StartSlot,
                                        NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Strides, pStrides, StartSlot, NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Offsets, pOffsets, StartSlot, NumBuffers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_IASetIndexBuffer(SerialiserType &ser,
                                                            ID3D11Buffer *pIndexBuffer,
                                                            DXGI_FORMAT Format, UINT Offset)
{
  SERIALISE_ELEMENT(pIndexBuffer);
  SERIALISE_ELEMENT(Format);
  SERIALISE_ELEMENT(Offset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordIndexBindStats(pIndexBuffer);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.IndexBuffer, pIndexBuffer);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.IndexFormat, Format);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.IndexOffset, Offset);
    m_pRealContext->IASetIndexBuffer(UNWRAP(WrappedID3D11Buffer, pIndexBuffer), Format, Offset);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format,
                                                  UINT Offset)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(
      m_pRealContext->IASetIndexBuffer(UNWRAP(WrappedID3D11Buffer, pIndexBuffer), Format, Offset));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::IASetIndexBuffer);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_IASetIndexBuffer(GET_SERIALISER, pIndexBuffer, Format, Offset);

    m_ContextRecord->AddChunk(scope.Get());
  }

  if(pIndexBuffer && IsActiveCapturing(m_State))
    MarkResourceReferenced(GetIDForResource(pIndexBuffer), eFrameRef_Read);

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.IndexBuffer, pIndexBuffer);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.IndexFormat, Format);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.IndexOffset, Offset);
  VerifyState();
}

#pragma endregion Input Assembly

#pragma region Vertex Shader

void WrappedID3D11DeviceContext::VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers)
{
  if(ppConstantBuffers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    m_pRealContext->VSGetConstantBuffers(StartSlot, NumBuffers, real);

    for(UINT i = 0; i < NumBuffers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->VS.ConstantBuffers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::VSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                      ID3D11ShaderResourceView **ppShaderResourceViews)
{
  if(ppShaderResourceViews)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
    m_pRealContext->VSGetShaderResources(StartSlot, NumViews, real);

    for(UINT i = 0; i < NumViews; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppShaderResourceViews[i] =
          (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppShaderResourceViews[i]);

      RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->VS.SRVs[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::VSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState **ppSamplers)
{
  if(ppSamplers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
    m_pRealContext->VSGetSamplers(StartSlot, NumSamplers, real);

    for(UINT i = 0; i < NumSamplers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppSamplers[i]);

      RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->VS.Samplers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::VSGetShader(ID3D11VertexShader **ppVertexShader,
                                             ID3D11ClassInstance **ppClassInstances,
                                             UINT *pNumClassInstances)
{
  if(ppVertexShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
  UINT numInsts = 0;
  ID3D11VertexShader *realShader = NULL;
  m_pRealContext->VSGetShader(&realShader, realInsts, &numInsts);

  SAFE_RELEASE_NOCLEAR(realShader);
  for(UINT i = 0; i < numInsts; i++)
    SAFE_RELEASE_NOCLEAR(realInsts[i]);

  if(ppVertexShader)
  {
    *ppVertexShader = (ID3D11VertexShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
    SAFE_ADDREF(*ppVertexShader);

    RDCASSERT(*ppVertexShader == m_CurrentPipelineState->VS.Object);
  }

  if(ppClassInstances)
  {
    for(UINT i = 0; i < numInsts; i++)
    {
      ppClassInstances[i] =
          (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
      SAFE_ADDREF(ppClassInstances[i]);

      RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->VS.Instances[i]);
    }
  }

  if(pNumClassInstances)
  {
    *pNumClassInstances = numInsts;
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_VSSetConstantBuffers(SerialiserType &ser, UINT StartSlot,
                                                                UINT NumBuffers,
                                                                ID3D11Buffer *const *ppConstantBuffers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Vertex, NumBuffers, ppConstantBuffers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBOffsets, NullCBOffsets, StartSlot,
                                   NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBCounts, NullCBCounts, StartSlot,
                                   NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    for(UINT i = 0; i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    m_pRealContext->VSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

    bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->VSSetConstantBuffers(StartSlot, NumBuffers, bufs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::VSSetConstantBuffers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_VSSetConstantBuffers(GET_SERIALISER, StartSlot, NumBuffers, ppConstantBuffers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.ConstantBuffers,
                                        ppConstantBuffers, StartSlot, NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBOffsets, NullCBOffsets, StartSlot,
                                 NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBCounts, NullCBCounts, StartSlot,
                                 NumBuffers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_VSSetShaderResources(
    SerialiserType &ser, UINT StartSlot, UINT NumViews,
    ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(ppShaderResourceViews, NumViews);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordResourceStats(ShaderStage::Vertex, NumViews, ppShaderResourceViews);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.SRVs, ppShaderResourceViews,
                                          StartSlot, NumViews);

    ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    for(UINT i = 0; i < NumViews; i++)
      SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);

    m_pRealContext->VSSetShaderResources(StartSlot, NumViews, SRVs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::VSSetShaderResources(
    UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  for(UINT i = 0; i < NumViews; i++)
  {
    if(ppShaderResourceViews[i] && IsActiveCapturing(m_State))
    {
      MarkResourceReferenced(GetIDForResource(ppShaderResourceViews[i]), eFrameRef_Read);
      MarkResourceReferenced(GetViewResourceResID(ppShaderResourceViews[i]), eFrameRef_Read);
    }

    SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->VSSetShaderResources(StartSlot, NumViews, SRVs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::VSSetShaderResources);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_VSSetShaderResources(GET_SERIALISER, StartSlot, NumViews, ppShaderResourceViews);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.SRVs, ppShaderResourceViews,
                                        StartSlot, NumViews);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_VSSetSamplers(SerialiserType &ser, UINT StartSlot,
                                                         UINT NumSamplers,
                                                         ID3D11SamplerState *const *ppSamplers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumSamplers);
  SERIALISE_ELEMENT_ARRAY(ppSamplers, NumSamplers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordSamplerStats(ShaderStage::Vertex, NumSamplers, ppSamplers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Samplers, ppSamplers,
                                          StartSlot, NumSamplers);

    ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    for(UINT i = 0; i < NumSamplers; i++)
      samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

    m_pRealContext->VSSetSamplers(StartSlot, NumSamplers, samps);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::VSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState *const *ppSamplers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  for(UINT i = 0; i < NumSamplers; i++)
  {
    if(ppSamplers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppSamplers[i]), eFrameRef_Read);

    samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->VSSetSamplers(StartSlot, NumSamplers, samps));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::VSSetSamplers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_VSSetSamplers(GET_SERIALISER, StartSlot, NumSamplers, ppSamplers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Samplers, ppSamplers, StartSlot,
                                        NumSamplers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_VSSetShader(SerialiserType &ser,
                                                       ID3D11VertexShader *pShader,
                                                       ID3D11ClassInstance *const *ppClassInstances,
                                                       UINT NumClassInstances)
{
  SERIALISE_ELEMENT(pShader);
  SERIALISE_ELEMENT_ARRAY(ppClassInstances, NumClassInstances);
  SERIALISE_ELEMENT(NumClassInstances);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordShaderStats(ShaderStage::Vertex, m_CurrentPipelineState->VS.Object, pShader);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Instances, ppClassInstances, 0,
                                          NumClassInstances);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.NumInstances, NumClassInstances);
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Object,
                                          (ID3D11DeviceChild *)pShader);

    ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

    m_pRealContext->VSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11VertexShader>, pShader), insts,
                                NumClassInstances);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::VSSetShader(ID3D11VertexShader *pVertexShader,
                                             ID3D11ClassInstance *const *ppClassInstances,
                                             UINT NumClassInstances)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
  if(ppClassInstances && NumClassInstances > 0)
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

  SERIALISE_TIME_CALL(m_pRealContext->VSSetShader(
      UNWRAP(WrappedID3D11Shader<ID3D11VertexShader>, pVertexShader), insts, NumClassInstances));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::VSSetShader);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_VSSetShader(GET_SERIALISER, pVertexShader, ppClassInstances, NumClassInstances);

    MarkResourceReferenced(GetIDForResource(pVertexShader), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Object,
                                        (ID3D11DeviceChild *)pVertexShader);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.NumInstances, NumClassInstances);
  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Instances, ppClassInstances, 0,
                                        NumClassInstances);

  VerifyState();
}

#pragma endregion Vertex Shader

#pragma region Hull Shader

void WrappedID3D11DeviceContext::HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers)
{
  if(ppConstantBuffers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    m_pRealContext->HSGetConstantBuffers(StartSlot, NumBuffers, real);

    for(UINT i = 0; i < NumBuffers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->HS.ConstantBuffers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::HSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                      ID3D11ShaderResourceView **ppShaderResourceViews)
{
  if(ppShaderResourceViews)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
    m_pRealContext->HSGetShaderResources(StartSlot, NumViews, real);

    for(UINT i = 0; i < NumViews; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppShaderResourceViews[i] =
          (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppShaderResourceViews[i]);

      RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->HS.SRVs[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::HSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState **ppSamplers)
{
  if(ppSamplers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
    m_pRealContext->HSGetSamplers(StartSlot, NumSamplers, real);

    for(UINT i = 0; i < NumSamplers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppSamplers[i]);

      RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->HS.Samplers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::HSGetShader(ID3D11HullShader **ppHullShader,
                                             ID3D11ClassInstance **ppClassInstances,
                                             UINT *pNumClassInstances)
{
  if(ppHullShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
  UINT numInsts = 0;
  ID3D11HullShader *realShader = NULL;
  m_pRealContext->HSGetShader(&realShader, realInsts, &numInsts);

  SAFE_RELEASE_NOCLEAR(realShader);
  for(UINT i = 0; i < numInsts; i++)
    SAFE_RELEASE_NOCLEAR(realInsts[i]);

  if(ppHullShader)
  {
    *ppHullShader = (ID3D11HullShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
    SAFE_ADDREF(*ppHullShader);

    RDCASSERT(*ppHullShader == m_CurrentPipelineState->HS.Object);
  }

  if(ppClassInstances)
  {
    for(UINT i = 0; i < numInsts; i++)
    {
      ppClassInstances[i] =
          (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
      SAFE_ADDREF(ppClassInstances[i]);

      RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->HS.Instances[i]);
    }
  }

  if(pNumClassInstances)
  {
    *pNumClassInstances = numInsts;
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_HSSetConstantBuffers(SerialiserType &ser, UINT StartSlot,
                                                                UINT NumBuffers,
                                                                ID3D11Buffer *const *ppConstantBuffers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Hull, NumBuffers, ppConstantBuffers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBOffsets, NullCBOffsets, StartSlot,
                                   NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBCounts, NullCBCounts, StartSlot,
                                   NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    for(UINT i = 0; i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    m_pRealContext->HSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

    bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->HSSetConstantBuffers(StartSlot, NumBuffers, bufs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::HSSetConstantBuffers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_HSSetConstantBuffers(GET_SERIALISER, StartSlot, NumBuffers, ppConstantBuffers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.ConstantBuffers,
                                        ppConstantBuffers, StartSlot, NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBOffsets, NullCBOffsets, StartSlot,
                                 NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBCounts, NullCBCounts, StartSlot,
                                 NumBuffers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_HSSetShaderResources(
    SerialiserType &ser, UINT StartSlot, UINT NumViews,
    ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(ppShaderResourceViews, NumViews);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordResourceStats(ShaderStage::Hull, NumViews, ppShaderResourceViews);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.SRVs, ppShaderResourceViews,
                                          StartSlot, NumViews);

    ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    for(UINT i = 0; i < NumViews; i++)
      SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);

    m_pRealContext->HSSetShaderResources(StartSlot, NumViews, SRVs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::HSSetShaderResources(
    UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  for(UINT i = 0; i < NumViews; i++)
  {
    if(ppShaderResourceViews[i] && IsActiveCapturing(m_State))
    {
      MarkResourceReferenced(GetIDForResource(ppShaderResourceViews[i]), eFrameRef_Read);
      MarkResourceReferenced(GetViewResourceResID(ppShaderResourceViews[i]), eFrameRef_Read);
    }

    SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->HSSetShaderResources(StartSlot, NumViews, SRVs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::HSSetShaderResources);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_HSSetShaderResources(GET_SERIALISER, StartSlot, NumViews, ppShaderResourceViews);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.SRVs, ppShaderResourceViews,
                                        StartSlot, NumViews);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_HSSetSamplers(SerialiserType &ser, UINT StartSlot,
                                                         UINT NumSamplers,
                                                         ID3D11SamplerState *const *ppSamplers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumSamplers);
  SERIALISE_ELEMENT_ARRAY(ppSamplers, NumSamplers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordSamplerStats(ShaderStage::Hull, NumSamplers, ppSamplers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Samplers, ppSamplers,
                                          StartSlot, NumSamplers);

    ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    for(UINT i = 0; i < NumSamplers; i++)
      samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

    m_pRealContext->HSSetSamplers(StartSlot, NumSamplers, samps);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::HSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState *const *ppSamplers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  for(UINT i = 0; i < NumSamplers; i++)
  {
    if(ppSamplers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppSamplers[i]), eFrameRef_Read);

    samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->HSSetSamplers(StartSlot, NumSamplers, samps));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::HSSetSamplers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_HSSetSamplers(GET_SERIALISER, StartSlot, NumSamplers, ppSamplers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Samplers, ppSamplers, StartSlot,
                                        NumSamplers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_HSSetShader(SerialiserType &ser, ID3D11HullShader *pShader,
                                                       ID3D11ClassInstance *const *ppClassInstances,
                                                       UINT NumClassInstances)
{
  SERIALISE_ELEMENT(pShader);
  SERIALISE_ELEMENT_ARRAY(ppClassInstances, NumClassInstances);
  SERIALISE_ELEMENT(NumClassInstances);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordShaderStats(ShaderStage::Hull, m_CurrentPipelineState->HS.Object, pShader);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Instances, ppClassInstances, 0,
                                          NumClassInstances);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.NumInstances, NumClassInstances);
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Object,
                                          (ID3D11DeviceChild *)pShader);

    ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

    m_pRealContext->HSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11HullShader>, pShader), insts,
                                NumClassInstances);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::HSSetShader(ID3D11HullShader *pHullShader,
                                             ID3D11ClassInstance *const *ppClassInstances,
                                             UINT NumClassInstances)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
  if(ppClassInstances && NumClassInstances > 0)
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

  SERIALISE_TIME_CALL(m_pRealContext->HSSetShader(
      UNWRAP(WrappedID3D11Shader<ID3D11HullShader>, pHullShader), insts, NumClassInstances));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::HSSetShader);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_HSSetShader(GET_SERIALISER, pHullShader, ppClassInstances, NumClassInstances);

    MarkResourceReferenced(GetIDForResource(pHullShader), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Object,
                                        (ID3D11DeviceChild *)pHullShader);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.NumInstances, NumClassInstances);
  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Instances, ppClassInstances, 0,
                                        NumClassInstances);

  VerifyState();
}

#pragma endregion Hull Shader

#pragma region Domain Shader

void WrappedID3D11DeviceContext::DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers)
{
  if(ppConstantBuffers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    m_pRealContext->DSGetConstantBuffers(StartSlot, NumBuffers, real);

    for(UINT i = 0; i < NumBuffers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->DS.ConstantBuffers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::DSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                      ID3D11ShaderResourceView **ppShaderResourceViews)
{
  if(ppShaderResourceViews)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
    m_pRealContext->DSGetShaderResources(StartSlot, NumViews, real);

    for(UINT i = 0; i < NumViews; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppShaderResourceViews[i] =
          (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppShaderResourceViews[i]);

      RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->DS.SRVs[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::DSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState **ppSamplers)
{
  if(ppSamplers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
    m_pRealContext->DSGetSamplers(StartSlot, NumSamplers, real);

    for(UINT i = 0; i < NumSamplers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppSamplers[i]);

      RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->DS.Samplers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::DSGetShader(ID3D11DomainShader **ppDomainShader,
                                             ID3D11ClassInstance **ppClassInstances,
                                             UINT *pNumClassInstances)
{
  if(ppDomainShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
  UINT numInsts = 0;
  ID3D11DomainShader *realShader = NULL;
  m_pRealContext->DSGetShader(&realShader, realInsts, &numInsts);

  SAFE_RELEASE_NOCLEAR(realShader);
  for(UINT i = 0; i < numInsts; i++)
    SAFE_RELEASE_NOCLEAR(realInsts[i]);

  if(ppDomainShader)
  {
    *ppDomainShader = (ID3D11DomainShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
    SAFE_ADDREF(*ppDomainShader);

    RDCASSERT(*ppDomainShader == m_CurrentPipelineState->DS.Object);
  }

  if(ppClassInstances)
  {
    for(UINT i = 0; i < numInsts; i++)
    {
      ppClassInstances[i] =
          (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
      SAFE_ADDREF(ppClassInstances[i]);

      RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->DS.Instances[i]);
    }
  }

  if(pNumClassInstances)
  {
    *pNumClassInstances = numInsts;
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DSSetConstantBuffers(SerialiserType &ser, UINT StartSlot,
                                                                UINT NumBuffers,
                                                                ID3D11Buffer *const *ppConstantBuffers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Domain, NumBuffers, ppConstantBuffers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBOffsets, NullCBOffsets, StartSlot,
                                   NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBCounts, NullCBCounts, StartSlot,
                                   NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    for(UINT i = 0; i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    m_pRealContext->DSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

    bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->DSSetConstantBuffers(StartSlot, NumBuffers, bufs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DSSetConstantBuffers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DSSetConstantBuffers(GET_SERIALISER, StartSlot, NumBuffers, ppConstantBuffers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.ConstantBuffers,
                                        ppConstantBuffers, StartSlot, NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBOffsets, NullCBOffsets, StartSlot,
                                 NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBCounts, NullCBCounts, StartSlot,
                                 NumBuffers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DSSetShaderResources(
    SerialiserType &ser, UINT StartSlot, UINT NumViews,
    ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(ppShaderResourceViews, NumViews);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordResourceStats(ShaderStage::Domain, NumViews, ppShaderResourceViews);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.SRVs, ppShaderResourceViews,
                                          StartSlot, NumViews);

    ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    for(UINT i = 0; i < NumViews; i++)
      SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);

    m_pRealContext->DSSetShaderResources(StartSlot, NumViews, SRVs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::DSSetShaderResources(
    UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  for(UINT i = 0; i < NumViews; i++)
  {
    if(ppShaderResourceViews[i] && IsActiveCapturing(m_State))
    {
      MarkResourceReferenced(GetIDForResource(ppShaderResourceViews[i]), eFrameRef_Read);
      MarkResourceReferenced(GetViewResourceResID(ppShaderResourceViews[i]), eFrameRef_Read);
    }

    SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->DSSetShaderResources(StartSlot, NumViews, SRVs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DSSetShaderResources);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DSSetShaderResources(GET_SERIALISER, StartSlot, NumViews, ppShaderResourceViews);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.SRVs, ppShaderResourceViews,
                                        StartSlot, NumViews);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DSSetSamplers(SerialiserType &ser, UINT StartSlot,
                                                         UINT NumSamplers,
                                                         ID3D11SamplerState *const *ppSamplers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumSamplers);
  SERIALISE_ELEMENT_ARRAY(ppSamplers, NumSamplers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordSamplerStats(ShaderStage::Domain, NumSamplers, ppSamplers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Samplers, ppSamplers,
                                          StartSlot, NumSamplers);

    ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    for(UINT i = 0; i < NumSamplers; i++)
      samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

    m_pRealContext->DSSetSamplers(StartSlot, NumSamplers, samps);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::DSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState *const *ppSamplers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  for(UINT i = 0; i < NumSamplers; i++)
  {
    if(ppSamplers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppSamplers[i]), eFrameRef_Read);

    samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->DSSetSamplers(StartSlot, NumSamplers, samps));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DSSetSamplers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DSSetSamplers(GET_SERIALISER, StartSlot, NumSamplers, ppSamplers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Samplers, ppSamplers, StartSlot,
                                        NumSamplers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DSSetShader(SerialiserType &ser,
                                                       ID3D11DomainShader *pShader,
                                                       ID3D11ClassInstance *const *ppClassInstances,
                                                       UINT NumClassInstances)
{
  SERIALISE_ELEMENT(pShader);
  SERIALISE_ELEMENT_ARRAY(ppClassInstances, NumClassInstances);
  SERIALISE_ELEMENT(NumClassInstances);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordShaderStats(ShaderStage::Domain, m_CurrentPipelineState->DS.Object, pShader);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Instances, ppClassInstances, 0,
                                          NumClassInstances);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.NumInstances, NumClassInstances);
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Object,
                                          (ID3D11DeviceChild *)pShader);

    ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

    m_pRealContext->DSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11DomainShader>, pShader), insts,
                                NumClassInstances);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::DSSetShader(ID3D11DomainShader *pDomainShader,
                                             ID3D11ClassInstance *const *ppClassInstances,
                                             UINT NumClassInstances)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
  if(ppClassInstances && NumClassInstances > 0)
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

  SERIALISE_TIME_CALL(m_pRealContext->DSSetShader(
      UNWRAP(WrappedID3D11Shader<ID3D11DomainShader>, pDomainShader), insts, NumClassInstances));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DSSetShader);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DSSetShader(GET_SERIALISER, pDomainShader, ppClassInstances, NumClassInstances);

    MarkResourceReferenced(GetIDForResource(pDomainShader), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Object,
                                        (ID3D11DeviceChild *)pDomainShader);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.NumInstances, NumClassInstances);
  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Instances, ppClassInstances, 0,
                                        NumClassInstances);

  VerifyState();
}

#pragma endregion Domain Shader

#pragma region Geometry Shader

void WrappedID3D11DeviceContext::GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers)
{
  if(ppConstantBuffers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    m_pRealContext->GSGetConstantBuffers(StartSlot, NumBuffers, real);

    for(UINT i = 0; i < NumBuffers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->GS.ConstantBuffers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::GSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                      ID3D11ShaderResourceView **ppShaderResourceViews)
{
  if(ppShaderResourceViews)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
    m_pRealContext->GSGetShaderResources(StartSlot, NumViews, real);

    for(UINT i = 0; i < NumViews; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppShaderResourceViews[i] =
          (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppShaderResourceViews[i]);

      RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->GS.SRVs[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::GSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState **ppSamplers)
{
  if(ppSamplers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
    m_pRealContext->GSGetSamplers(StartSlot, NumSamplers, real);

    for(UINT i = 0; i < NumSamplers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppSamplers[i]);

      RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->GS.Samplers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::GSGetShader(ID3D11GeometryShader **ppGeometryShader,
                                             ID3D11ClassInstance **ppClassInstances,
                                             UINT *pNumClassInstances)
{
  if(ppGeometryShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
  UINT numInsts = 0;
  ID3D11GeometryShader *realShader = NULL;
  m_pRealContext->GSGetShader(&realShader, realInsts, &numInsts);

  SAFE_RELEASE_NOCLEAR(realShader);
  for(UINT i = 0; i < numInsts; i++)
    SAFE_RELEASE_NOCLEAR(realInsts[i]);

  if(ppGeometryShader)
  {
    *ppGeometryShader =
        (ID3D11GeometryShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
    SAFE_ADDREF(*ppGeometryShader);

    RDCASSERT(*ppGeometryShader == m_CurrentPipelineState->GS.Object);
  }

  if(ppClassInstances)
  {
    for(UINT i = 0; i < numInsts; i++)
    {
      ppClassInstances[i] =
          (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
      SAFE_ADDREF(ppClassInstances[i]);

      RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->GS.Instances[i]);
    }
  }

  if(pNumClassInstances)
  {
    *pNumClassInstances = numInsts;
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_GSSetConstantBuffers(SerialiserType &ser, UINT StartSlot,
                                                                UINT NumBuffers,
                                                                ID3D11Buffer *const *ppConstantBuffers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Geometry, NumBuffers, ppConstantBuffers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBOffsets, NullCBOffsets, StartSlot,
                                   NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBCounts, NullCBCounts, StartSlot,
                                   NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    for(UINT i = 0; i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    m_pRealContext->GSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

    bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->GSSetConstantBuffers(StartSlot, NumBuffers, bufs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::GSSetConstantBuffers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_GSSetConstantBuffers(GET_SERIALISER, StartSlot, NumBuffers, ppConstantBuffers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.ConstantBuffers,
                                        ppConstantBuffers, StartSlot, NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBOffsets, NullCBOffsets, StartSlot,
                                 NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBCounts, NullCBCounts, StartSlot,
                                 NumBuffers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_GSSetShaderResources(
    SerialiserType &ser, UINT StartSlot, UINT NumViews,
    ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(ppShaderResourceViews, NumViews);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordResourceStats(ShaderStage::Geometry, NumViews, ppShaderResourceViews);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.SRVs, ppShaderResourceViews,
                                          StartSlot, NumViews);

    ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    for(UINT i = 0; i < NumViews; i++)
      SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);

    m_pRealContext->GSSetShaderResources(StartSlot, NumViews, SRVs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::GSSetShaderResources(
    UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  for(UINT i = 0; i < NumViews; i++)
  {
    if(ppShaderResourceViews[i] && IsActiveCapturing(m_State))
    {
      MarkResourceReferenced(GetIDForResource(ppShaderResourceViews[i]), eFrameRef_Read);
      MarkResourceReferenced(GetViewResourceResID(ppShaderResourceViews[i]), eFrameRef_Read);
    }

    SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->GSSetShaderResources(StartSlot, NumViews, SRVs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::GSSetShaderResources);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_GSSetShaderResources(GET_SERIALISER, StartSlot, NumViews, ppShaderResourceViews);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.SRVs, ppShaderResourceViews,
                                        StartSlot, NumViews);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_GSSetSamplers(SerialiserType &ser, UINT StartSlot,
                                                         UINT NumSamplers,
                                                         ID3D11SamplerState *const *ppSamplers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumSamplers);
  SERIALISE_ELEMENT_ARRAY(ppSamplers, NumSamplers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordSamplerStats(ShaderStage::Geometry, NumSamplers, ppSamplers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Samplers, ppSamplers,
                                          StartSlot, NumSamplers);

    ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    for(UINT i = 0; i < NumSamplers; i++)
      samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

    m_pRealContext->GSSetSamplers(StartSlot, NumSamplers, samps);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::GSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState *const *ppSamplers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  for(UINT i = 0; i < NumSamplers; i++)
  {
    if(ppSamplers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppSamplers[i]), eFrameRef_Read);

    samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->GSSetSamplers(StartSlot, NumSamplers, samps));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::GSSetSamplers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_GSSetSamplers(GET_SERIALISER, StartSlot, NumSamplers, ppSamplers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Samplers, ppSamplers, StartSlot,
                                        NumSamplers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_GSSetShader(SerialiserType &ser,
                                                       ID3D11GeometryShader *pShader,
                                                       ID3D11ClassInstance *const *ppClassInstances,
                                                       UINT NumClassInstances)
{
  SERIALISE_ELEMENT(pShader);
  SERIALISE_ELEMENT_ARRAY(ppClassInstances, NumClassInstances);
  SERIALISE_ELEMENT(NumClassInstances);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordShaderStats(ShaderStage::Geometry, m_CurrentPipelineState->GS.Object, pShader);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Instances, ppClassInstances, 0,
                                          NumClassInstances);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.NumInstances, NumClassInstances);
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Object,
                                          (ID3D11DeviceChild *)pShader);

    ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

    m_pRealContext->GSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11GeometryShader>, pShader), insts,
                                NumClassInstances);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::GSSetShader(ID3D11GeometryShader *pShader,
                                             ID3D11ClassInstance *const *ppClassInstances,
                                             UINT NumClassInstances)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
  if(ppClassInstances && NumClassInstances > 0)
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

  SERIALISE_TIME_CALL(m_pRealContext->GSSetShader(
      UNWRAP(WrappedID3D11Shader<ID3D11GeometryShader>, pShader), insts, NumClassInstances));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::GSSetShader);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_GSSetShader(GET_SERIALISER, pShader, ppClassInstances, NumClassInstances);

    MarkResourceReferenced(GetIDForResource(pShader), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Object,
                                        (ID3D11DeviceChild *)pShader);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.NumInstances, NumClassInstances);
  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Instances, ppClassInstances, 0,
                                        NumClassInstances);

  VerifyState();
}

#pragma endregion Geometry Shader

#pragma region Stream Out

void WrappedID3D11DeviceContext::SOGetTargets(UINT NumBuffers, ID3D11Buffer **ppSOTargets)
{
  if(ppSOTargets)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11Buffer *real[D3D11_SO_BUFFER_SLOT_COUNT] = {0};
    m_pRealContext->SOGetTargets(NumBuffers, real);

    for(UINT i = 0; i < NumBuffers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppSOTargets[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppSOTargets[i]);

      RDCASSERT(ppSOTargets[i] == m_CurrentPipelineState->SO.Buffers[i]);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_SOSetTargets(SerialiserType &ser, UINT NumBuffers,
                                                        ID3D11Buffer *const *ppSOTargets,
                                                        const UINT *pOffsets)
{
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppSOTargets, NumBuffers);
  SERIALISE_ELEMENT_ARRAY(pOffsets, NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // "If less than four [buffers] are defined by the call, the remaining buffer slots are set to
    // NULL."
    ID3D11Buffer *setbufs[D3D11_SO_STREAM_COUNT] = {0};
    UINT setoffs[D3D11_SO_STREAM_COUNT] = {0};

    for(UINT b = 0; b < NumBuffers; b++)
    {
      setbufs[b] = ppSOTargets ? ppSOTargets[b] : NULL;
      // passing NULL for pOffsets seems to act like -1 => append
      setoffs[b] = pOffsets ? pOffsets[b] : ~0U;
    }

    // end stream-out queries for outgoing targets
    for(UINT b = 0; b < D3D11_SO_STREAM_COUNT; b++)
    {
      ID3D11Buffer *buf = m_CurrentPipelineState->SO.Buffers[b];

      if(buf)
      {
        ResourceId id = GetIDForResource(buf);

        m_pRealContext->End(m_StreamOutCounters[id].query);
        m_StreamOutCounters[id].running = false;
      }
    }

    // start new queries for incoming targets
    for(UINT b = 0; b < D3D11_SO_STREAM_COUNT; b++)
    {
      ID3D11Buffer *buf = setbufs[b];

      if(buf)
      {
        ResourceId id = GetIDForResource(buf);

        // release any previous query as the hidden counter is overwritten
        SAFE_RELEASE(m_StreamOutCounters[id].query);

        D3D11_QUERY queryTypes[] = {
            D3D11_QUERY_SO_STATISTICS_STREAM0, D3D11_QUERY_SO_STATISTICS_STREAM1,
            D3D11_QUERY_SO_STATISTICS_STREAM2, D3D11_QUERY_SO_STATISTICS_STREAM3,
        };

        D3D11_QUERY_DESC qdesc;
        qdesc.MiscFlags = 0;
        qdesc.Query = queryTypes[b];

        m_pDevice->GetReal()->CreateQuery(&qdesc, &m_StreamOutCounters[id].query);

        m_pRealContext->Begin(m_StreamOutCounters[id].query);
        m_StreamOutCounters[id].running = true;
      }
    }

    m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->SO.Buffers, setbufs, 0,
                                           D3D11_SO_STREAM_COUNT);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->SO.Offsets, setoffs, 0,
                                   D3D11_SO_STREAM_COUNT);

    for(UINT i = 0; i < NumBuffers; i++)
      setbufs[i] = UNWRAP(WrappedID3D11Buffer, setbufs[i]);

    m_pRealContext->SOSetTargets(NumBuffers, setbufs, setoffs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::SOSetTargets(UINT NumBuffers, ID3D11Buffer *const *ppSOTargets,
                                              const UINT *pOffsets)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11Buffer *bufs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {0};
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppSOTargets && ppSOTargets[i])
    {
      // technically this isn't dirty until the draw call, but let's be conservative
      // to avoid having to track "possibly" dirty resources.
      // Besides, it's unlikely an application will set an output then not draw to it
      if(IsActiveCapturing(m_State))
        MarkResourceReferenced(GetIDForResource(ppSOTargets[i]), eFrameRef_PartialWrite);

      MarkDirtyResource(GetIDForResource(ppSOTargets[i]));
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppSOTargets[i]);
    }
  }

  SERIALISE_TIME_CALL(m_pRealContext->SOSetTargets(NumBuffers, bufs, pOffsets));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::SOSetTargets);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_SOSetTargets(GET_SERIALISER, NumBuffers, ppSOTargets, pOffsets);

    m_ContextRecord->AddChunk(scope.Get());
  }

  // "If less than four [buffers] are defined by the call, the remaining buffer slots are set to
  // NULL."
  ID3D11Buffer *setbufs[D3D11_SO_STREAM_COUNT] = {0};
  UINT setoffs[D3D11_SO_STREAM_COUNT] = {0};

  for(UINT b = 0; b < NumBuffers; b++)
  {
    setbufs[b] = ppSOTargets ? ppSOTargets[b] : NULL;
    // passing NULL for pOffsets seems to act like -1 => append
    setoffs[b] = pOffsets ? pOffsets[b] : ~0U;
  }

  // end stream-out queries for outgoing targets
  for(UINT b = 0; b < D3D11_SO_STREAM_COUNT; b++)
  {
    ID3D11Buffer *buf = m_CurrentPipelineState->SO.Buffers[b];

    if(buf)
    {
      ResourceId id = GetIDForResource(buf);

      m_pRealContext->End(m_StreamOutCounters[id].query);
      m_StreamOutCounters[id].running = false;
    }
  }

  // start new queries for incoming targets
  for(UINT b = 0; b < D3D11_SO_STREAM_COUNT; b++)
  {
    ID3D11Buffer *buf = setbufs[b];

    if(buf)
    {
      ResourceId id = GetIDForResource(buf);

      // release any previous query as the hidden counter is overwritten
      SAFE_RELEASE(m_StreamOutCounters[id].query);

      D3D11_QUERY queryTypes[] = {
          D3D11_QUERY_SO_STATISTICS_STREAM0, D3D11_QUERY_SO_STATISTICS_STREAM1,
          D3D11_QUERY_SO_STATISTICS_STREAM2, D3D11_QUERY_SO_STATISTICS_STREAM3,
      };

      D3D11_QUERY_DESC qdesc;
      qdesc.MiscFlags = 0;
      qdesc.Query = queryTypes[b];

      m_pDevice->GetReal()->CreateQuery(&qdesc, &m_StreamOutCounters[id].query);

      m_pRealContext->Begin(m_StreamOutCounters[id].query);
      m_StreamOutCounters[id].running = true;
    }
  }

  m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->SO.Buffers, setbufs, 0,
                                         D3D11_SO_STREAM_COUNT);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->SO.Offsets, setoffs, 0,
                                 D3D11_SO_STREAM_COUNT);

  VerifyState();
}

#pragma endregion Stream Out

#pragma region Rasterizer

void WrappedID3D11DeviceContext::RSGetViewports(UINT *pNumViewports, D3D11_VIEWPORT *pViewports)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_pRealContext->RSGetViewports(pNumViewports, pViewports);

  if(pViewports)
    RDCASSERT(memcmp(pViewports, m_CurrentPipelineState->RS.Viewports,
                     sizeof(D3D11_VIEWPORT) * (*pNumViewports)) == 0);
}

void WrappedID3D11DeviceContext::RSGetScissorRects(UINT *pNumRects, D3D11_RECT *pRects)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_pRealContext->RSGetScissorRects(pNumRects, pRects);

  if(pRects)
    RDCASSERT(memcmp(pRects, m_CurrentPipelineState->RS.Scissors,
                     sizeof(D3D11_RECT) * (*pNumRects)) == 0);
}

void WrappedID3D11DeviceContext::RSGetState(ID3D11RasterizerState **ppRasterizerState)
{
  if(ppRasterizerState)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11RasterizerState *real = NULL;
    m_pRealContext->RSGetState(&real);

    if(real != NULL)
    {
      real->Release();
      ID3D11DeviceChild *state = m_pDevice->GetResourceManager()->GetWrapper(real);
      *ppRasterizerState = (ID3D11RasterizerState *)state;
      (*ppRasterizerState)->AddRef();
    }
    else
    {
      *ppRasterizerState = NULL;
    }

    RDCASSERT(*ppRasterizerState == m_CurrentPipelineState->RS.State);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_RSSetViewports(SerialiserType &ser, UINT NumViewports,
                                                          const D3D11_VIEWPORT *pViewports)
{
  SERIALISE_ELEMENT(NumViewports);
  SERIALISE_ELEMENT_ARRAY(pViewports, NumViewports);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordViewportStats(NumViewports, pViewports);

    m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.Viewports, pViewports, 0, NumViewports);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.NumViews, NumViewports);
    m_pRealContext->RSSetViewports(NumViewports, pViewports);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT *pViewports)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->RSSetViewports(NumViewports, pViewports));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::RSSetViewports);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_RSSetViewports(GET_SERIALISER, NumViewports, pViewports);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.Viewports, pViewports, 0, NumViewports);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.NumViews, NumViewports);
  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_RSSetScissorRects(SerialiserType &ser, UINT NumRects,
                                                             const D3D11_RECT *pRects)
{
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordScissorStats(NumRects, pRects);

    m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.Scissors, pRects, 0, NumRects);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.NumScissors, NumRects);
    m_pRealContext->RSSetScissorRects(NumRects, pRects);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->RSSetScissorRects(NumRects, pRects));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::RSSetScissorRects);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_RSSetScissorRects(GET_SERIALISER, NumRects, pRects);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.Scissors, pRects, 0, NumRects);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.NumScissors, NumRects);
  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_RSSetState(SerialiserType &ser,
                                                      ID3D11RasterizerState *pRasterizerState)
{
  SERIALISE_ELEMENT(pRasterizerState);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordRasterizationStats(pRasterizerState);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->RS.State, pRasterizerState);
    m_pRealContext->RSSetState(UNWRAP(WrappedID3D11RasterizerState2, pRasterizerState));
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::RSSetState(ID3D11RasterizerState *pRasterizerState)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->RSSetState(
      (ID3D11RasterizerState *)UNWRAP(WrappedID3D11RasterizerState2, pRasterizerState)));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::RSSetState);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_RSSetState(GET_SERIALISER, pRasterizerState);

    MarkResourceReferenced(GetIDForResource(pRasterizerState), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->RS.State, pRasterizerState);

  VerifyState();
}

#pragma endregion Rasterizer

#pragma region Pixel Shader

void WrappedID3D11DeviceContext::PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers)
{
  if(ppConstantBuffers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    m_pRealContext->PSGetConstantBuffers(StartSlot, NumBuffers, real);

    for(UINT i = 0; i < NumBuffers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->PS.ConstantBuffers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::PSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                      ID3D11ShaderResourceView **ppShaderResourceViews)
{
  if(ppShaderResourceViews)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
    m_pRealContext->PSGetShaderResources(StartSlot, NumViews, real);

    for(UINT i = 0; i < NumViews; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppShaderResourceViews[i] =
          (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppShaderResourceViews[i]);

      RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->PS.SRVs[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::PSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState **ppSamplers)
{
  if(ppSamplers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
    m_pRealContext->PSGetSamplers(StartSlot, NumSamplers, real);

    for(UINT i = 0; i < NumSamplers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppSamplers[i]);

      RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->PS.Samplers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::PSGetShader(ID3D11PixelShader **ppPixelShader,
                                             ID3D11ClassInstance **ppClassInstances,
                                             UINT *pNumClassInstances)
{
  if(ppPixelShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
  UINT numInsts = 0;
  ID3D11PixelShader *realShader = NULL;
  m_pRealContext->PSGetShader(&realShader, realInsts, &numInsts);

  SAFE_RELEASE_NOCLEAR(realShader);
  for(UINT i = 0; i < numInsts; i++)
    SAFE_RELEASE_NOCLEAR(realInsts[i]);

  if(ppPixelShader)
  {
    *ppPixelShader = (ID3D11PixelShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
    SAFE_ADDREF(*ppPixelShader);

    RDCASSERT(*ppPixelShader == m_CurrentPipelineState->PS.Object);
  }

  if(ppClassInstances)
  {
    for(UINT i = 0; i < numInsts; i++)
    {
      ppClassInstances[i] =
          (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
      SAFE_ADDREF(ppClassInstances[i]);

      RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->PS.Instances[i]);
    }
  }

  if(pNumClassInstances)
  {
    *pNumClassInstances = numInsts;
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_PSSetConstantBuffers(SerialiserType &ser, UINT StartSlot,
                                                                UINT NumBuffers,
                                                                ID3D11Buffer *const *ppConstantBuffers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Pixel, NumBuffers, ppConstantBuffers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBOffsets, NullCBOffsets, StartSlot,
                                   NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBCounts, NullCBCounts, StartSlot,
                                   NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    for(UINT i = 0; i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    m_pRealContext->PSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

    bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->PSSetConstantBuffers(StartSlot, NumBuffers, bufs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::PSSetConstantBuffers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_PSSetConstantBuffers(GET_SERIALISER, StartSlot, NumBuffers, ppConstantBuffers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.ConstantBuffers,
                                        ppConstantBuffers, StartSlot, NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBOffsets, NullCBOffsets, StartSlot,
                                 NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBCounts, NullCBCounts, StartSlot,
                                 NumBuffers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_PSSetShaderResources(
    SerialiserType &ser, UINT StartSlot, UINT NumViews,
    ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(ppShaderResourceViews, NumViews);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordResourceStats(ShaderStage::Pixel, NumViews, ppShaderResourceViews);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.SRVs, ppShaderResourceViews,
                                          StartSlot, NumViews);

    ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    for(UINT i = 0; i < NumViews; i++)
      SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);

    m_pRealContext->PSSetShaderResources(StartSlot, NumViews, SRVs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::PSSetShaderResources(
    UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  for(UINT i = 0; i < NumViews; i++)
  {
    if(ppShaderResourceViews[i] && IsActiveCapturing(m_State))
    {
      MarkResourceReferenced(GetIDForResource(ppShaderResourceViews[i]), eFrameRef_Read);
      MarkResourceReferenced(GetViewResourceResID(ppShaderResourceViews[i]), eFrameRef_Read);
    }

    SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->PSSetShaderResources(StartSlot, NumViews, SRVs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::PSSetShaderResources);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_PSSetShaderResources(GET_SERIALISER, StartSlot, NumViews, ppShaderResourceViews);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.SRVs, ppShaderResourceViews,
                                        StartSlot, NumViews);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_PSSetSamplers(SerialiserType &ser, UINT StartSlot,
                                                         UINT NumSamplers,
                                                         ID3D11SamplerState *const *ppSamplers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumSamplers);
  SERIALISE_ELEMENT_ARRAY(ppSamplers, NumSamplers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordSamplerStats(ShaderStage::Pixel, NumSamplers, ppSamplers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Samplers, ppSamplers,
                                          StartSlot, NumSamplers);

    ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    for(UINT i = 0; i < NumSamplers; i++)
      samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

    m_pRealContext->PSSetSamplers(StartSlot, NumSamplers, samps);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::PSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState *const *ppSamplers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  for(UINT i = 0; i < NumSamplers; i++)
  {
    if(ppSamplers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppSamplers[i]), eFrameRef_Read);

    samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->PSSetSamplers(StartSlot, NumSamplers, samps));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::PSSetSamplers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_PSSetSamplers(GET_SERIALISER, StartSlot, NumSamplers, ppSamplers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Samplers, ppSamplers, StartSlot,
                                        NumSamplers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_PSSetShader(SerialiserType &ser,
                                                       ID3D11PixelShader *pShader,
                                                       ID3D11ClassInstance *const *ppClassInstances,
                                                       UINT NumClassInstances)
{
  SERIALISE_ELEMENT(pShader);
  SERIALISE_ELEMENT_ARRAY(ppClassInstances, NumClassInstances);
  SERIALISE_ELEMENT(NumClassInstances);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordShaderStats(ShaderStage::Pixel, m_CurrentPipelineState->PS.Object, pShader);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Instances, ppClassInstances, 0,
                                          NumClassInstances);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.NumInstances, NumClassInstances);
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Object,
                                          (ID3D11DeviceChild *)pShader);

    ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

    m_pRealContext->PSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, pShader), insts,
                                NumClassInstances);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::PSSetShader(ID3D11PixelShader *pPixelShader,
                                             ID3D11ClassInstance *const *ppClassInstances,
                                             UINT NumClassInstances)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
  if(ppClassInstances && NumClassInstances > 0)
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

  SERIALISE_TIME_CALL(m_pRealContext->PSSetShader(
      UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, pPixelShader), insts, NumClassInstances));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::PSSetShader);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_PSSetShader(GET_SERIALISER, pPixelShader, ppClassInstances, NumClassInstances);

    MarkResourceReferenced(GetIDForResource(pPixelShader), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Object,
                                        (ID3D11DeviceChild *)pPixelShader);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.NumInstances, NumClassInstances);
  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Instances, ppClassInstances, 0,
                                        NumClassInstances);

  VerifyState();
}

#pragma endregion Pixel Shader

#pragma region Output Merger

void WrappedID3D11DeviceContext::OMGetRenderTargets(UINT NumViews,
                                                    ID3D11RenderTargetView **ppRenderTargetViews,
                                                    ID3D11DepthStencilView **ppDepthStencilView)
{
  if(ppRenderTargetViews == NULL && ppDepthStencilView == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11RenderTargetView *rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
  ID3D11DepthStencilView *dsv = NULL;
  m_pRealContext->OMGetRenderTargets(NumViews, rtv, &dsv);

  for(UINT i = 0; i < NumViews; i++)
    SAFE_RELEASE_NOCLEAR(rtv[i]);

  SAFE_RELEASE_NOCLEAR(dsv);

  if(ppRenderTargetViews)
  {
    for(UINT i = 0; i < NumViews; i++)
    {
      ppRenderTargetViews[i] =
          (ID3D11RenderTargetView *)m_pDevice->GetResourceManager()->GetWrapper(rtv[i]);
      SAFE_ADDREF(ppRenderTargetViews[i]);

      RDCASSERT(ppRenderTargetViews[i] == m_CurrentPipelineState->OM.RenderTargets[i]);
    }
  }

  if(ppDepthStencilView)
  {
    *ppDepthStencilView = (ID3D11DepthStencilView *)m_pDevice->GetResourceManager()->GetWrapper(dsv);
    SAFE_ADDREF(*ppDepthStencilView);

    RDCASSERT(*ppDepthStencilView == m_CurrentPipelineState->OM.DepthView);
  }
}

void WrappedID3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(
    UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews,
    ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs,
    ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
  if(ppRenderTargetViews == NULL && ppDepthStencilView == NULL && ppUnorderedAccessViews == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11RenderTargetView *rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
  ID3D11UnorderedAccessView *uav[D3D11_1_UAV_SLOT_COUNT] = {0};
  ID3D11DepthStencilView *dsv = NULL;
  m_pRealContext->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, rtv, &dsv, UAVStartSlot,
                                                            NumUAVs, uav);

  for(UINT i = 0; i < NumRTVs; i++)
    SAFE_RELEASE_NOCLEAR(rtv[i]);

  SAFE_RELEASE_NOCLEAR(dsv);

  for(UINT i = 0; i < NumUAVs; i++)
    SAFE_RELEASE_NOCLEAR(uav[i]);

  if(ppRenderTargetViews)
  {
    for(UINT i = 0; i < NumRTVs; i++)
    {
      ppRenderTargetViews[i] =
          (ID3D11RenderTargetView *)m_pDevice->GetResourceManager()->GetWrapper(rtv[i]);
      SAFE_ADDREF(ppRenderTargetViews[i]);

      RDCASSERT(ppRenderTargetViews[i] == m_CurrentPipelineState->OM.RenderTargets[i]);
    }
  }

  if(ppDepthStencilView)
  {
    *ppDepthStencilView = (ID3D11DepthStencilView *)m_pDevice->GetResourceManager()->GetWrapper(dsv);
    SAFE_ADDREF(*ppDepthStencilView);

    RDCASSERT(*ppDepthStencilView == m_CurrentPipelineState->OM.DepthView);
  }

  if(ppUnorderedAccessViews)
  {
    for(UINT i = 0; i < NumUAVs; i++)
    {
      ppUnorderedAccessViews[i] =
          (ID3D11UnorderedAccessView *)m_pDevice->GetResourceManager()->GetWrapper(uav[i]);
      SAFE_ADDREF(ppUnorderedAccessViews[i]);

      RDCASSERT(ppUnorderedAccessViews[i] == m_CurrentPipelineState->OM.UAVs[i]);
    }
  }
}

void WrappedID3D11DeviceContext::OMGetBlendState(ID3D11BlendState **ppBlendState,
                                                 FLOAT BlendFactor[4], UINT *pSampleMask)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11BlendState *real = NULL;
  m_pRealContext->OMGetBlendState(&real, BlendFactor, pSampleMask);

  SAFE_RELEASE_NOCLEAR(real);

  if(ppBlendState)
  {
    if(real != NULL)
    {
      ID3D11DeviceChild *state = m_pDevice->GetResourceManager()->GetWrapper(real);
      *ppBlendState = (ID3D11BlendState *)state;
      (*ppBlendState)->AddRef();
    }
    else
    {
      *ppBlendState = NULL;
    }

    RDCASSERT(*ppBlendState == m_CurrentPipelineState->OM.BlendState);
  }
  if(BlendFactor)
    RDCASSERT(memcmp(BlendFactor, m_CurrentPipelineState->OM.BlendFactor, sizeof(float) * 4) == 0);
  if(pSampleMask)
    RDCASSERT(*pSampleMask == m_CurrentPipelineState->OM.SampleMask);
}

void WrappedID3D11DeviceContext::OMGetDepthStencilState(ID3D11DepthStencilState **ppDepthStencilState,
                                                        UINT *pStencilRef)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11DepthStencilState *real = NULL;
  m_pRealContext->OMGetDepthStencilState(&real, pStencilRef);

  SAFE_RELEASE_NOCLEAR(real);

  if(ppDepthStencilState)
  {
    if(real != NULL)
    {
      *ppDepthStencilState =
          (ID3D11DepthStencilState *)m_pDevice->GetResourceManager()->GetWrapper(real);
      SAFE_ADDREF(*ppDepthStencilState);
    }
    else
    {
      *ppDepthStencilState = NULL;
    }

    RDCASSERT(*ppDepthStencilState == m_CurrentPipelineState->OM.DepthStencilState);
  }
  if(pStencilRef)
    RDCASSERT(*pStencilRef == m_CurrentPipelineState->OM.StencRef);
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_OMSetRenderTargets(
    SerialiserType &ser, UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews,
    ID3D11DepthStencilView *pDepthStencilView)
{
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(ppRenderTargetViews, NumViews);
  SERIALISE_ELEMENT(pDepthStencilView);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11RenderTargetView *RTs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
    for(UINT i = 0; i < NumViews; i++)
      RTs[i] = ppRenderTargetViews[i];

    if(m_CurrentPipelineState->ValidOutputMerger(ppRenderTargetViews, NumViews, pDepthStencilView,
                                                 NULL, 0))
    {
      m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets, RTs, 0,
                                             D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
      m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.DepthView, pDepthStencilView);
    }

    ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
    m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.UAVs, UAVs, 0,
                                           D3D11_1_UAV_SLOT_COUNT);

    m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.UAVStartSlot, NumViews);

    for(UINT i = 0; i < NumViews; i++)
      RTs[i] = UNWRAP(WrappedID3D11RenderTargetView1, RTs[i]);

    if(IsLoading(m_State))
      RecordOutputMergerStats(NumViews, RTs, pDepthStencilView, 0, 0, NULL);

    m_pRealContext->OMSetRenderTargets(NumViews, RTs,
                                       UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView));
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::OMSetRenderTargets(UINT NumViews,
                                                    ID3D11RenderTargetView *const *ppRenderTargetViews,
                                                    ID3D11DepthStencilView *pDepthStencilView)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11RenderTargetView *RTs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
  for(UINT i = 0; i < NumViews && ppRenderTargetViews; i++)
    RTs[i] = UNWRAP(WrappedID3D11RenderTargetView1, ppRenderTargetViews[i]);

  SERIALISE_TIME_CALL(m_pRealContext->OMSetRenderTargets(
      NumViews, RTs, UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView)));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::OMSetRenderTargets);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_OMSetRenderTargets(GET_SERIALISER, NumViews, ppRenderTargetViews, pDepthStencilView);

    m_ContextRecord->AddChunk(scope.Get());
  }

  for(UINT i = 0; i < NumViews && ppRenderTargetViews; i++)
    RTs[i] = ppRenderTargetViews[i];

  // this function always sets all render targets
  if(m_CurrentPipelineState->ValidOutputMerger(RTs, NumViews, pDepthStencilView, NULL, 0))
  {
    m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets, RTs, 0,
                                           D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
    m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.DepthView, pDepthStencilView);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.UAVStartSlot, NumViews);
  }

  ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
  m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.UAVs, UAVs, 0,
                                         D3D11_1_UAV_SLOT_COUNT);

  for(UINT i = 0; i < NumViews; i++)
  {
    if(ppRenderTargetViews && ppRenderTargetViews[i])
    {
      if(IsCaptureMode(m_State))
      {
        // technically this isn't dirty until the draw call, but let's be conservative
        // to avoid having to track "possibly" dirty resources.
        // Besides, it's unlikely an application will set an output then not draw to it
        MarkDirtyResource(GetViewResourceResID(ppRenderTargetViews[i]));
      }

      RTs[i] = UNWRAP(WrappedID3D11RenderTargetView1, ppRenderTargetViews[i]);
    }
  }

  if(pDepthStencilView && IsCaptureMode(m_State))
  {
    MarkDirtyResource(GetViewResourceResID(pDepthStencilView));
  }

  if(IsActiveCapturing(m_State))
  {
    // make sure to mark resources referenced even if the OM state is invalid, so they aren't
    // eliminated from the capture (which might make this combination valid on replay without some
    // of the targets!)
    for(UINT i = 0; i < NumViews; i++)
    {
      if(ppRenderTargetViews && ppRenderTargetViews[i])
      {
        MarkResourceReferenced(GetIDForResource(ppRenderTargetViews[i]), eFrameRef_Read);
        MarkResourceReferenced(GetViewResourceResID(ppRenderTargetViews[i]), eFrameRef_PartialWrite);
      }
    }

    if(pDepthStencilView)
    {
      MarkResourceReferenced(GetIDForResource(pDepthStencilView), eFrameRef_Read);
      MarkResourceReferenced(GetViewResourceResID(pDepthStencilView), eFrameRef_PartialWrite);
    }
  }

  VerifyState();
}

// some helper enums with custom stringise to handle special cases
enum D3D11RTVCount
{
};

DECLARE_REFLECTION_ENUM(D3D11RTVCount);

template <>
rdcstr DoStringise(const D3D11RTVCount &el)
{
  RDCCOMPILE_ASSERT(sizeof(D3D11RTVCount) == sizeof(uint32_t), "Enum isn't uint sized");

  if(el == D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
    return "D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL";

  return ToStr(uint32_t(el));
}

enum D3D11UAVCount
{
};

DECLARE_REFLECTION_ENUM(D3D11UAVCount);

template <>
rdcstr DoStringise(const D3D11UAVCount &el)
{
  RDCCOMPILE_ASSERT(sizeof(D3D11UAVCount) == sizeof(uint32_t), "Enum isn't uint sized");

  if(el == D3D11_KEEP_UNORDERED_ACCESS_VIEWS)
    return "D3D11_KEEP_UNORDERED_ACCESS_VIEWS";

  return ToStr(uint32_t(el));
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_OMSetRenderTargetsAndUnorderedAccessViews(
    SerialiserType &ser, UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews,
    ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs,
    ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts)
{
  // cast to a special enum so we print the two 'keep' constants nicely
  SERIALISE_ELEMENT_TYPED(D3D11RTVCount, NumRTVs);

  // handle special values for counts
  const bool KeepRTVs = (NumRTVs == D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL);
  const bool ModifyRTVs = !KeepRTVs;
  if(!ModifyRTVs)
    NumRTVs = 0;

  SERIALISE_ELEMENT_ARRAY(ppRenderTargetViews, NumRTVs);

  SERIALISE_ELEMENT(pDepthStencilView);
  SERIALISE_ELEMENT(UAVStartSlot);

  SERIALISE_ELEMENT_TYPED(D3D11UAVCount, NumUAVs);

  const bool KeepUAVs = (NumUAVs == D3D11_KEEP_UNORDERED_ACCESS_VIEWS);
  const bool ModifyUAVs = !KeepUAVs;
  if(!ModifyUAVs)
    NumUAVs = 0;

  SERIALISE_ELEMENT_ARRAY(ppUnorderedAccessViews, NumUAVs);
  SERIALISE_ELEMENT_ARRAY(pUAVInitialCounts, NumUAVs);

  RDCASSERT(ModifyRTVs || ModifyUAVs);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // any bind of RTVs or UAVs implicitly NULLs out the remaining slots, so we don't want to just
    // change the bindings we were passed, we want to always change ALL bindings, and just set the
    // subset that we were passed
    ID3D11RenderTargetView *RTs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
    ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT] = {0};

    for(UINT i = 0; i < NumRTVs && ModifyRTVs; i++)
      RTs[i] = ppRenderTargetViews[i];

    for(UINT i = 0; i < NumUAVs && ModifyUAVs; i++)
      UAVs[i] = ppUnorderedAccessViews[i];

    if(ModifyRTVs)
    {
      ID3D11UnorderedAccessView *const *srcUAVs = ppUnorderedAccessViews;
      UINT srcUAVcount = NumUAVs;

      if(!ModifyUAVs)
      {
        srcUAVs = m_CurrentPipelineState->OM.UAVs;
        srcUAVcount = D3D11_1_UAV_SLOT_COUNT;

        // if we're not modifying the UAVs but NumRTVs > oldUAVStartSlot then we unbind any
        // overlapped UAVs.
        if(NumRTVs > m_CurrentPipelineState->OM.UAVStartSlot)
        {
          UINT diff = NumRTVs - m_CurrentPipelineState->OM.UAVStartSlot;
          srcUAVcount -= diff;
          srcUAVs += diff;
        }
      }

      if(m_CurrentPipelineState->ValidOutputMerger(ppRenderTargetViews, NumRTVs, pDepthStencilView,
                                                   srcUAVs, srcUAVcount))
      {
        m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets, RTs, 0,
                                               D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
        m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.DepthView,
                                               pDepthStencilView);

        if(!ModifyUAVs)
        {
          // set UAVStartSlot to NumRTVs
          m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.UAVStartSlot, NumRTVs);

          if(NumRTVs > m_CurrentPipelineState->OM.UAVStartSlot)
          {
            UINT diff = NumRTVs - m_CurrentPipelineState->OM.UAVStartSlot;

            // release and unbind any UAVs that were unbound
            for(UINT i = 0; i < diff; i++)
              m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.UAVs[i],
                                                     (ID3D11UnorderedAccessView *)NULL);

            // move the array down *without* changing any refs

            for(UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
            {
              if(i < D3D11_1_UAV_SLOT_COUNT - diff)
              {
                m_CurrentPipelineState->OM.UAVs[i] = m_CurrentPipelineState->OM.UAVs[i + diff];
              }
              else
              {
                // NULL without ref'ing, since we just moved this down lower in the array
                m_CurrentPipelineState->OM.UAVs[i] = NULL;
              }
            }
          }
        }
      }
    }

    if(ModifyUAVs)
    {
      bool valid = false;
      if(ModifyRTVs)
      {
        valid = m_CurrentPipelineState->ValidOutputMerger(
            ppRenderTargetViews, NumRTVs, pDepthStencilView, ppUnorderedAccessViews, NumUAVs);
      }
      else
      {
        // if we're not modifying RTVs, any that are < UAVStartSlot get unbound so don't consider
        // for validity
        valid = m_CurrentPipelineState->ValidOutputMerger(
            m_CurrentPipelineState->OM.RenderTargets, UAVStartSlot,
            m_CurrentPipelineState->OM.DepthView, ppUnorderedAccessViews, NumUAVs);
      }

      if(valid)
      {
        m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.UAVs, UAVs, 0,
                                               D3D11_1_UAV_SLOT_COUNT);
        m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.UAVStartSlot, UAVStartSlot);

        // unbind any conflicting RTVS
        for(UINT i = UAVStartSlot; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
          m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets[i],
                                                 (ID3D11RenderTargetView *)NULL);
      }
    }

    for(UINT i = 0; i < NumRTVs && ModifyRTVs; i++)
      RTs[i] = UNWRAP(WrappedID3D11RenderTargetView1, RTs[i]);

    for(UINT i = 0; i < NumUAVs && ModifyUAVs; i++)
      UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView1, UAVs[i]);

    if(ModifyRTVs)
      pDepthStencilView = UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView);
    else
      pDepthStencilView = NULL;

    if(IsLoading(m_State))
      RecordOutputMergerStats(NumRTVs, RTs, pDepthStencilView, UAVStartSlot, NumUAVs, UAVs);

    m_pRealContext->OMSetRenderTargetsAndUnorderedAccessViews(
        ModifyRTVs ? NumRTVs : D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, RTs, pDepthStencilView,
        UAVStartSlot, ModifyUAVs ? NumUAVs : D3D11_KEEP_UNORDERED_ACCESS_VIEWS, UAVs,
        pUAVInitialCounts);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(
    UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews,
    ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs,
    ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  UINT StartSlot = UAVStartSlot;

  // D3D11 doesn't seem to complain about this case, but it messes our render state tracking so
  // ensure we don't blat over any RTs with 'empty' UAVs.
  if(NumUAVs == 0)
  {
    if(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
      UAVStartSlot = RDCMAX(NumRTVs, UAVStartSlot);
    else
      UAVStartSlot = RDCMAX(m_CurrentPipelineState->OM.UAVStartSlot, UAVStartSlot);
  }

  ID3D11RenderTargetView *RTs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
  ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT] = {0};

  for(UINT i = 0;
      ppRenderTargetViews && NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL && i < NumRTVs;
      i++)
    RTs[i] = UNWRAP(WrappedID3D11RenderTargetView1, ppRenderTargetViews[i]);

  for(UINT i = 0;
      ppUnorderedAccessViews && NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS && i < NumUAVs; i++)
    UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView1, ppUnorderedAccessViews[i]);

  SERIALISE_TIME_CALL(m_pRealContext->OMSetRenderTargetsAndUnorderedAccessViews(
      NumRTVs, ppRenderTargetViews ? RTs : NULL,
      UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView), UAVStartSlot, NumUAVs,
      ppUnorderedAccessViews ? UAVs : NULL, pUAVInitialCounts));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::OMSetRenderTargetsAndUnorderedAccessViews);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_OMSetRenderTargetsAndUnorderedAccessViews(GET_SERIALISER, NumRTVs, ppRenderTargetViews,
                                                        pDepthStencilView, UAVStartSlot, NumUAVs,
                                                        ppUnorderedAccessViews, pUAVInitialCounts);

    m_ContextRecord->AddChunk(scope.Get());
  }

  for(UINT i = 0;
      ppRenderTargetViews && NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL && i < NumRTVs;
      i++)
    RTs[i] = ppRenderTargetViews[i];

  for(UINT i = 0;
      ppUnorderedAccessViews && NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS && i < NumUAVs; i++)
    UAVs[i] = ppUnorderedAccessViews[i];

  const bool ModifyRTVs = NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL;
  const bool ModifyUAVs = NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS;

  if(ModifyRTVs)
  {
    ID3D11UnorderedAccessView **srcUAVs = ModifyUAVs ? UAVs : m_CurrentPipelineState->OM.UAVs;
    UINT srcUAVcount = ModifyUAVs ? NumUAVs : D3D11_1_UAV_SLOT_COUNT;

    if(m_CurrentPipelineState->ValidOutputMerger(ppRenderTargetViews, NumRTVs, pDepthStencilView,
                                                 srcUAVs, srcUAVcount))
    {
      m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets, RTs, 0,
                                             D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
      m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.DepthView, pDepthStencilView);
    }

    if(IsActiveCapturing(m_State))
    {
      // make sure to mark resources referenced if the OM state is invalid, so they aren't
      // eliminated from the
      // log (which might make this combination valid on replay without some of the targets!)
      for(UINT i = 0; i < NumRTVs; i++)
      {
        if(ppRenderTargetViews && ppRenderTargetViews[i])
        {
          MarkResourceReferenced(GetIDForResource(ppRenderTargetViews[i]), eFrameRef_Read);
          MarkResourceReferenced(GetViewResourceResID(ppRenderTargetViews[i]), eFrameRef_Read);
        }
      }

      if(pDepthStencilView)
      {
        MarkResourceReferenced(GetIDForResource(pDepthStencilView), eFrameRef_Read);
        MarkResourceReferenced(GetViewResourceResID(pDepthStencilView), eFrameRef_Read);
      }
    }
  }

  if(ModifyUAVs)
  {
    bool valid = false;
    if(ModifyRTVs)
      valid =
          m_CurrentPipelineState->ValidOutputMerger(RTs, NumRTVs, pDepthStencilView, UAVs, NumUAVs);
    else
      valid = m_CurrentPipelineState->ValidOutputMerger(
          m_CurrentPipelineState->OM.RenderTargets, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
          m_CurrentPipelineState->OM.DepthView, UAVs, NumUAVs);

    if(valid)
    {
      m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.UAVs, UAVs, 0,
                                             D3D11_1_UAV_SLOT_COUNT);
      m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.UAVStartSlot, UAVStartSlot);
    }

    if(IsActiveCapturing(m_State))
    {
      // make sure to mark resources referenced if the OM state is invalid, so they aren't
      // eliminated from the
      // log (which might make this combination valid on replay without some of the targets!)
      for(UINT i = 0; i < NumUAVs; i++)
      {
        if(UAVs[i])
        {
          MarkResourceReferenced(GetIDForResource(UAVs[i]), eFrameRef_Read);
          MarkResourceReferenced(GetViewResourceResID(UAVs[i]), eFrameRef_Read);
        }
      }
    }
  }

  // invalid case where UAV/RTV overlap, UAV seems to take precedence
  bool UAVOverlap = (NumUAVs > 0 && NumUAVs <= D3D11_1_UAV_SLOT_COUNT && NumRTVs > 0 &&
                     NumRTVs <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT && StartSlot < NumRTVs);

  if(UAVOverlap)
  {
    ID3D11RenderTargetView *NullRTs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};

    // unset any RTs overlapping with the UAV range
    m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets, NullRTs,
                                           StartSlot,
                                           RDCMIN(NumUAVs, D3D11_1_UAV_SLOT_COUNT - StartSlot));
  }

  for(UINT i = 0;
      ppRenderTargetViews && NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL && i < NumRTVs;
      i++)
  {
    if(ppRenderTargetViews[i] && IsCaptureMode(m_State))
    {
      // technically this isn't dirty until the draw call, but let's be conservative
      // to avoid having to track "possibly" dirty resources.
      // Besides, it's unlikely an application will set an output then not draw to it
      MarkDirtyResource(GetViewResourceResID(ppRenderTargetViews[i]));
    }

    RTs[i] = UNWRAP(WrappedID3D11RenderTargetView1, ppRenderTargetViews[i]);
  }

  for(UINT i = 0;
      ppUnorderedAccessViews && NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS && i < NumUAVs; i++)
  {
    if(ppUnorderedAccessViews[i] && IsCaptureMode(m_State))
    {
      MarkDirtyResource(GetViewResourceResID(ppUnorderedAccessViews[i]));
    }

    UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView1, ppUnorderedAccessViews[i]);
  }

  if(pDepthStencilView && IsCaptureMode(m_State))
  {
    MarkDirtyResource(GetViewResourceResID(pDepthStencilView));
  }

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_OMSetBlendState(SerialiserType &ser,
                                                           ID3D11BlendState *pBlendState,
                                                           const FLOAT BlendFactor[4],
                                                           UINT SampleMask)
{
  SERIALISE_ELEMENT(pBlendState);
  SERIALISE_ELEMENT_ARRAY(BlendFactor, 4);
  SERIALISE_ELEMENT(SampleMask);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    FLOAT DefaultBlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const FLOAT *SafeBlendFactor = BlendFactor ? BlendFactor : DefaultBlendFactor;

    if(IsLoading(m_State))
      RecordBlendStats(pBlendState, SafeBlendFactor, SampleMask);

    {
      m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->OM.BlendState, pBlendState);
      m_pRealContext->OMSetBlendState(UNWRAP(WrappedID3D11BlendState1, pBlendState), BlendFactor,
                                      SampleMask);
    }
    m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.BlendFactor, SafeBlendFactor, 0, 4);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.SampleMask, SampleMask);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::OMSetBlendState(ID3D11BlendState *pBlendState,
                                                 const FLOAT BlendFactor[4], UINT SampleMask)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->OMSetBlendState(
      (ID3D11BlendState *)UNWRAP(WrappedID3D11BlendState1, pBlendState), BlendFactor, SampleMask));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::OMSetBlendState);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_OMSetBlendState(GET_SERIALISER, pBlendState, BlendFactor, SampleMask);

    MarkResourceReferenced(GetIDForResource(pBlendState), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  FLOAT DefaultBlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->OM.BlendState, pBlendState);
  if(BlendFactor != NULL)
    m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.BlendFactor, BlendFactor, 0, 4);
  else
    m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.BlendFactor, DefaultBlendFactor, 0, 4);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.SampleMask, SampleMask);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_OMSetDepthStencilState(
    SerialiserType &ser, ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef)
{
  SERIALISE_ELEMENT(pDepthStencilState);
  SERIALISE_ELEMENT(StencilRef);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordDepthStencilStats(pDepthStencilState, StencilRef);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->OM.DepthStencilState,
                                          pDepthStencilState);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.StencRef, StencilRef & 0xff);
    m_pRealContext->OMSetDepthStencilState(
        UNWRAP(WrappedID3D11DepthStencilState, pDepthStencilState), StencilRef);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState,
                                                        UINT StencilRef)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->OMSetDepthStencilState(
      UNWRAP(WrappedID3D11DepthStencilState, pDepthStencilState), StencilRef));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::OMSetDepthStencilState);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_OMSetDepthStencilState(GET_SERIALISER, pDepthStencilState, StencilRef);

    MarkResourceReferenced(GetIDForResource(pDepthStencilState), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->OM.DepthStencilState,
                                        pDepthStencilState);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.StencRef, StencilRef & 0xff);
  VerifyState();
}

#pragma endregion Output Merger

#pragma region Draw

template <typename SerialiserType>
void WrappedID3D11DeviceContext::Serialise_DebugMessages(SerialiserType &ser)
{
  std::vector<DebugMessage> DebugMessages;

  m_EmptyCommandList = false;

  // only grab debug messages for the immediate context, without serialising all
  // API use there's no way to find out which messages come from which context :(.
  if(ser.IsWriting() && IsActiveCapturing(m_State) && GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
  {
    DebugMessages = m_pDevice->GetDebugMessages();
  }

  SERIALISE_ELEMENT(DebugMessages);

  // hide empty sets of messages.
  if(ser.IsReading() && DebugMessages.empty())
    ser.Hidden();

  if(ser.IsReading() && IsLoading(m_State))
  {
    for(DebugMessage &msg : DebugMessages)
    {
      msg.eventId = m_CurEventID;
      m_pDevice->AddDebugMessage(msg);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DrawIndexedInstanced(
    SerialiserType &ser, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
    INT BaseVertexLocation, UINT StartInstanceLocation)
{
  SERIALISE_ELEMENT(IndexCountPerInstance);
  SERIALISE_ELEMENT(InstanceCount);
  SERIALISE_ELEMENT(StartIndexLocation);
  SERIALISE_ELEMENT(BaseVertexLocation);
  SERIALISE_ELEMENT(StartInstanceLocation);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_pRealContext->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                         BaseVertexLocation, StartInstanceLocation);

    if(IsLoading(m_State))
    {
      RecordDrawStats(true, false, InstanceCount);

      AddEvent();

      DrawcallDescription draw;
      draw.name =
          StringFormat::Fmt("DrawIndexedInstanced(%u, %u)", IndexCountPerInstance, InstanceCount);
      draw.numIndices = IndexCountPerInstance;
      draw.numInstances = InstanceCount;
      draw.indexOffset = StartIndexLocation;
      draw.baseVertex = BaseVertexLocation;
      draw.instanceOffset = StartInstanceLocation;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indexed;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
                                                      UINT StartIndexLocation, INT BaseVertexLocation,
                                                      UINT StartInstanceLocation)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  MarkAPIActive();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount,
                                                           StartIndexLocation, BaseVertexLocation,
                                                           StartInstanceLocation));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DrawIndexedInstanced);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DrawIndexedInstanced(GET_SERIALISER, IndexCountPerInstance, InstanceCount,
                                   StartIndexLocation, BaseVertexLocation, StartInstanceLocation);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DrawInstanced(SerialiserType &ser,
                                                         UINT VertexCountPerInstance,
                                                         UINT InstanceCount, UINT StartVertexLocation,
                                                         UINT StartInstanceLocation)
{
  SERIALISE_ELEMENT(VertexCountPerInstance);
  SERIALISE_ELEMENT(InstanceCount);
  SERIALISE_ELEMENT(StartVertexLocation);
  SERIALISE_ELEMENT(StartInstanceLocation);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_pRealContext->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                                  StartInstanceLocation);

    if(IsLoading(m_State))
    {
      RecordDrawStats(true, false, InstanceCount);

      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("DrawInstanced(%u, %u)", VertexCountPerInstance, InstanceCount);
      draw.numIndices = VertexCountPerInstance;
      draw.numInstances = InstanceCount;
      draw.vertexOffset = StartVertexLocation;
      draw.instanceOffset = StartInstanceLocation;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
                                               UINT StartVertexLocation, UINT StartInstanceLocation)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  MarkAPIActive();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->DrawInstanced(VertexCountPerInstance, InstanceCount,
                                                    StartVertexLocation, StartInstanceLocation));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DrawInstanced);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DrawInstanced(GET_SERIALISER, VertexCountPerInstance, InstanceCount,
                            StartVertexLocation, StartInstanceLocation);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DrawIndexed(SerialiserType &ser, UINT IndexCount,
                                                       UINT StartIndexLocation,
                                                       INT BaseVertexLocation)
{
  SERIALISE_ELEMENT(IndexCount);
  SERIALISE_ELEMENT(StartIndexLocation);
  SERIALISE_ELEMENT(BaseVertexLocation);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_pRealContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);

    if(IsLoading(m_State))
    {
      RecordDrawStats(false, false, 1);

      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("DrawIndexed(%u)", IndexCount);
      draw.numIndices = IndexCount;
      draw.baseVertex = BaseVertexLocation;
      draw.indexOffset = StartIndexLocation;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Indexed;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation,
                                             INT BaseVertexLocation)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  MarkAPIActive();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DrawIndexed);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DrawIndexed(GET_SERIALISER, IndexCount, StartIndexLocation, BaseVertexLocation);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_Draw(SerialiserType &ser, UINT VertexCount,
                                                UINT StartVertexLocation)
{
  SERIALISE_ELEMENT(VertexCount);
  SERIALISE_ELEMENT(StartVertexLocation);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_pRealContext->Draw(VertexCount, StartVertexLocation);

    if(IsLoading(m_State))
    {
      RecordDrawStats(false, false, 1);

      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("Draw(%u)", VertexCount);
      draw.numIndices = VertexCount;
      draw.vertexOffset = StartVertexLocation;

      draw.flags |= DrawFlags::Drawcall;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::Draw(UINT VertexCount, UINT StartVertexLocation)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  MarkAPIActive();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->Draw(VertexCount, StartVertexLocation));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::Draw);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_Draw(GET_SERIALISER, VertexCount, StartVertexLocation);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DrawAuto(SerialiserType &ser)
{
  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  uint64_t numVerts = 0;

  if(IsReplayingAndReading())
  {
    // spec says that only the first vertex buffer is used
    if(m_CurrentPipelineState->IA.VBs[0] == NULL)
    {
      m_pDevice->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                 MessageSource::IncorrectAPIUse,
                                 "DrawAuto() with VB 0 set to NULL!");
    }
    else
    {
      ResourceId id = GetIDForResource(m_CurrentPipelineState->IA.VBs[0]);

      StreamOutData &data = m_StreamOutCounters[id];

      // if we have a query, the stream-out data for this DrawAuto was generated
      // in the captured frame, so we can do a legitimate DrawAuto()
      if(data.query)
      {
        // shouldn't still be bound on output
        RDCASSERT(!data.running);

        D3D11_QUERY_DATA_SO_STATISTICS numPrims;

        HRESULT hr = S_FALSE;

        do
        {
          hr = m_pRealContext->GetData(data.query, &numPrims,
                                       sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
        } while(hr == S_FALSE);

        if(m_CurrentPipelineState->IA.Topo == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST)
          numVerts = numPrims.NumPrimitivesWritten;
        else if(m_CurrentPipelineState->IA.Topo == D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
          numVerts = numPrims.NumPrimitivesWritten * 2;
        else
          numVerts = numPrims.NumPrimitivesWritten * 3;

        m_pRealContext->DrawAuto();
      }
      else
      {
        // otherwise use the cached value from the previous frame.

        if(m_CurrentPipelineState->IA.Topo == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST)
          numVerts = data.numPrims;
        else if(m_CurrentPipelineState->IA.Topo == D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
          numVerts = data.numPrims * 2;
        else
          numVerts = data.numPrims * 3;

        m_pRealContext->Draw((UINT)numVerts, 0);
      }
    }

    if(IsLoading(m_State))
    {
      RecordDrawStats(false, false, 1);

      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("DrawAuto(<%u>)", numVerts);
      draw.flags |= DrawFlags::Drawcall | DrawFlags::Auto;
      draw.numIndices = (uint32_t)numVerts;
      draw.vertexOffset = 0;
      draw.indexOffset = 0;
      draw.instanceOffset = 0;
      draw.numInstances = 1;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DrawAuto()
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  MarkAPIActive();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->DrawAuto());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DrawAuto);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DrawAuto(GET_SERIALISER);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DrawIndexedInstancedIndirect(SerialiserType &ser,
                                                                        ID3D11Buffer *pBufferForArgs,
                                                                        UINT AlignedByteOffsetForArgs)
{
  SERIALISE_ELEMENT(pBufferForArgs);
  SERIALISE_ELEMENT(AlignedByteOffsetForArgs);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pBufferForArgs)
    {
      m_pRealContext->DrawIndexedInstancedIndirect(UNWRAP(WrappedID3D11Buffer, pBufferForArgs),
                                                   AlignedByteOffsetForArgs);
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;

      std::string name = "DrawIndexedInstancedIndirect(-, -)";

      if(pBufferForArgs)
      {
        struct DrawArgs
        {
          uint32_t IndexCountPerInstance;
          uint32_t InstanceCount;
          uint32_t StartIndexLocation;
          int32_t BaseVertexLocation;
          uint32_t StartInstanceLocation;
        };

        bytebuf argarray;
        m_pDevice->GetDebugManager()->GetBufferData(pBufferForArgs, AlignedByteOffsetForArgs,
                                                    sizeof(DrawArgs), argarray);

        if(argarray.size() >= sizeof(DrawArgs))
        {
          DrawArgs *args = (DrawArgs *)&argarray[0];

          draw.numIndices = args->IndexCountPerInstance;
          draw.numInstances = args->InstanceCount;
          draw.indexOffset = args->StartIndexLocation;
          draw.baseVertex = args->BaseVertexLocation;
          draw.instanceOffset = args->StartInstanceLocation;

          RecordDrawStats(true, true, draw.numInstances);

          name = StringFormat::Fmt("DrawIndexedInstancedIndirect(<%u, %u>)", draw.numIndices,
                                   draw.numInstances);
        }
        else
        {
          name = "DrawIndexedInstancedIndirect(<!, !>)";
          D3D11_BUFFER_DESC bufDesc;

          pBufferForArgs->GetDesc(&bufDesc);

          if(AlignedByteOffsetForArgs >= bufDesc.ByteWidth)
          {
            m_pDevice->AddDebugMessage(
                MessageCategory::Execution, MessageSeverity::High, MessageSource::IncorrectAPIUse,
                StringFormat::Fmt("Call to DrawIndexedInstancedIndirect with buffer of %u "
                                  "bytes, but byte offset specified is %u bytes.",
                                  bufDesc.ByteWidth, AlignedByteOffsetForArgs));
          }
          else if(AlignedByteOffsetForArgs + sizeof(DrawArgs) >= bufDesc.ByteWidth)
          {
            m_pDevice->AddDebugMessage(
                MessageCategory::Execution, MessageSeverity::High, MessageSource::IncorrectAPIUse,
                StringFormat::Fmt("Call to DrawIndexedInstancedIndirect with buffer of %u "
                                  "bytes at offset of %u bytes, which leaves less than %u bytes "
                                  "for the arguments.",
                                  bufDesc.ByteWidth, AlignedByteOffsetForArgs, sizeof(DrawArgs)));
          }
        }

        m_ResourceUses[GetIDForResource(pBufferForArgs)].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Indirect));
      }

      draw.name = name;

      draw.flags |=
          DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indexed | DrawFlags::Indirect;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs,
                                                              UINT AlignedByteOffsetForArgs)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  MarkAPIActive();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->DrawIndexedInstancedIndirect(
      UNWRAP(WrappedID3D11Buffer, pBufferForArgs), AlignedByteOffsetForArgs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DrawIndexedInstancedIndirect);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DrawIndexedInstancedIndirect(GET_SERIALISER, pBufferForArgs, AlignedByteOffsetForArgs);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }

  if(pBufferForArgs && IsActiveCapturing(m_State))
    MarkResourceReferenced(GetIDForResource(pBufferForArgs), eFrameRef_Read);
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DrawInstancedIndirect(SerialiserType &ser,
                                                                 ID3D11Buffer *pBufferForArgs,
                                                                 UINT AlignedByteOffsetForArgs)
{
  SERIALISE_ELEMENT(pBufferForArgs);
  SERIALISE_ELEMENT(AlignedByteOffsetForArgs);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pBufferForArgs)
    {
      m_pRealContext->DrawInstancedIndirect(UNWRAP(WrappedID3D11Buffer, pBufferForArgs),
                                            AlignedByteOffsetForArgs);
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;

      std::string name = "DrawInstancedIndirect(-, -)";

      if(pBufferForArgs)
      {
        struct DrawArgs
        {
          uint32_t VertexCountPerInstance;
          uint32_t InstanceCount;
          uint32_t StartVertexLocation;
          uint32_t StartInstanceLocation;
        };

        bytebuf argarray;
        m_pDevice->GetDebugManager()->GetBufferData(pBufferForArgs, AlignedByteOffsetForArgs,
                                                    sizeof(DrawArgs), argarray);

        if(argarray.size() >= sizeof(DrawArgs))
        {
          DrawArgs *args = (DrawArgs *)&argarray[0];

          draw.numIndices = args->VertexCountPerInstance;
          draw.numInstances = args->InstanceCount;
          draw.vertexOffset = args->StartVertexLocation;
          draw.instanceOffset = args->StartInstanceLocation;

          RecordDrawStats(true, true, draw.numInstances);

          name = StringFormat::Fmt("DrawInstancedIndirect(<%u, %u>)", draw.numIndices,
                                   draw.numInstances);
        }
        else
        {
          name = "DrawInstancedIndirect(<!, !>)";
          D3D11_BUFFER_DESC bufDesc;

          pBufferForArgs->GetDesc(&bufDesc);

          if(AlignedByteOffsetForArgs >= bufDesc.ByteWidth)
          {
            m_pDevice->AddDebugMessage(
                MessageCategory::Execution, MessageSeverity::High, MessageSource::IncorrectAPIUse,
                StringFormat::Fmt("Call to DrawIndexedInstancedIndirect with buffer of %u "
                                  "bytes, but byte offset specified is %u bytes.",
                                  bufDesc.ByteWidth, AlignedByteOffsetForArgs));
          }
          else if(AlignedByteOffsetForArgs + sizeof(DrawArgs) >= bufDesc.ByteWidth)
          {
            m_pDevice->AddDebugMessage(
                MessageCategory::Execution, MessageSeverity::High, MessageSource::IncorrectAPIUse,
                StringFormat::Fmt("Call to DrawIndexedInstancedIndirect with buffer of %u "
                                  "bytes at offset of %u bytes, which leaves less than %u bytes "
                                  "for the arguments.",
                                  bufDesc.ByteWidth, AlignedByteOffsetForArgs, sizeof(DrawArgs)));
          }
        }

        m_ResourceUses[GetIDForResource(pBufferForArgs)].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Indirect));
      }

      draw.name = name;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs,
                                                       UINT AlignedByteOffsetForArgs)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  MarkAPIActive();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->DrawInstancedIndirect(
      UNWRAP(WrappedID3D11Buffer, pBufferForArgs), AlignedByteOffsetForArgs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DrawInstancedIndirect);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DrawInstancedIndirect(GET_SERIALISER, pBufferForArgs, AlignedByteOffsetForArgs);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }

  if(pBufferForArgs && IsActiveCapturing(m_State))
    MarkResourceReferenced(GetIDForResource(pBufferForArgs), eFrameRef_Read);
}

#pragma endregion Draw

#pragma region Compute Shader

void WrappedID3D11DeviceContext::CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers)
{
  if(ppConstantBuffers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    m_pRealContext->CSGetConstantBuffers(StartSlot, NumBuffers, real);

    for(UINT i = 0; i < NumBuffers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppConstantBuffers[i]);

      RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->CS.ConstantBuffers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::CSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                      ID3D11ShaderResourceView **ppShaderResourceViews)
{
  if(ppShaderResourceViews)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
    m_pRealContext->CSGetShaderResources(StartSlot, NumViews, real);

    for(UINT i = 0; i < NumViews; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppShaderResourceViews[i] =
          (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppShaderResourceViews[i]);

      RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->CS.SRVs[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::CSGetUnorderedAccessViews(
    UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
  if(ppUnorderedAccessViews)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11UnorderedAccessView *real[D3D11_1_UAV_SLOT_COUNT] = {0};
    m_pRealContext->CSGetUnorderedAccessViews(StartSlot, NumUAVs, real);

    for(UINT i = 0; i < NumUAVs; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppUnorderedAccessViews[i] =
          (ID3D11UnorderedAccessView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppUnorderedAccessViews[i]);

      RDCASSERT(ppUnorderedAccessViews[i] == m_CurrentPipelineState->CSUAVs[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::CSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState **ppSamplers)
{
  if(ppSamplers)
  {
    SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

    ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
    m_pRealContext->CSGetSamplers(StartSlot, NumSamplers, real);

    for(UINT i = 0; i < NumSamplers; i++)
    {
      SAFE_RELEASE_NOCLEAR(real[i]);
      ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
      SAFE_ADDREF(ppSamplers[i]);

      RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->CS.Samplers[i + StartSlot]);
    }
  }
}

void WrappedID3D11DeviceContext::CSGetShader(ID3D11ComputeShader **ppComputeShader,
                                             ID3D11ClassInstance **ppClassInstances,
                                             UINT *pNumClassInstances)
{
  if(ppComputeShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
  UINT numInsts = 0;
  ID3D11ComputeShader *realShader = NULL;
  m_pRealContext->CSGetShader(&realShader, realInsts, &numInsts);

  SAFE_RELEASE_NOCLEAR(realShader);
  for(UINT i = 0; i < numInsts; i++)
    SAFE_RELEASE_NOCLEAR(realInsts[i]);

  if(ppComputeShader)
  {
    *ppComputeShader = (ID3D11ComputeShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
    SAFE_ADDREF(*ppComputeShader);

    RDCASSERT(*ppComputeShader == m_CurrentPipelineState->CS.Object);
  }

  if(ppClassInstances)
  {
    for(UINT i = 0; i < numInsts; i++)
    {
      ppClassInstances[i] =
          (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
      SAFE_ADDREF(ppClassInstances[i]);

      RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->CS.Instances[i]);
    }
  }

  if(pNumClassInstances)
  {
    *pNumClassInstances = numInsts;
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CSSetConstantBuffers(SerialiserType &ser, UINT StartSlot,
                                                                UINT NumBuffers,
                                                                ID3D11Buffer *const *ppConstantBuffers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumBuffers);
  SERIALISE_ELEMENT_ARRAY(ppConstantBuffers, NumBuffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordConstantStats(ShaderStage::Compute, NumBuffers, ppConstantBuffers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.ConstantBuffers,
                                          ppConstantBuffers, StartSlot, NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBOffsets, NullCBOffsets, StartSlot,
                                   NumBuffers);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBCounts, NullCBCounts, StartSlot,
                                   NumBuffers);

    ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
    for(UINT i = 0; i < NumBuffers; i++)
      bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);

    m_pRealContext->CSSetConstantBuffers(StartSlot, NumBuffers, bufs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(ppConstantBuffers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

    bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->CSSetConstantBuffers(StartSlot, NumBuffers, bufs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CSSetConstantBuffers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_CSSetConstantBuffers(GET_SERIALISER, StartSlot, NumBuffers, ppConstantBuffers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.ConstantBuffers,
                                        ppConstantBuffers, StartSlot, NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBOffsets, NullCBOffsets, StartSlot,
                                 NumBuffers);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBCounts, NullCBCounts, StartSlot,
                                 NumBuffers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CSSetShaderResources(
    SerialiserType &ser, UINT StartSlot, UINT NumViews,
    ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(ppShaderResourceViews, NumViews);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordResourceStats(ShaderStage::Compute, NumViews, ppShaderResourceViews);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.SRVs, ppShaderResourceViews,
                                          StartSlot, NumViews);

    ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    for(UINT i = 0; i < NumViews; i++)
      SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);

    m_pRealContext->CSSetShaderResources(StartSlot, NumViews, SRVs);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::CSSetShaderResources(
    UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  for(UINT i = 0; i < NumViews; i++)
  {
    if(ppShaderResourceViews[i] && IsActiveCapturing(m_State))
    {
      MarkResourceReferenced(GetIDForResource(ppShaderResourceViews[i]), eFrameRef_Read);
      MarkResourceReferenced(GetViewResourceResID(ppShaderResourceViews[i]), eFrameRef_Read);
    }

    SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, ppShaderResourceViews[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->CSSetShaderResources(StartSlot, NumViews, SRVs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CSSetShaderResources);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_CSSetShaderResources(GET_SERIALISER, StartSlot, NumViews, ppShaderResourceViews);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.SRVs, ppShaderResourceViews,
                                        StartSlot, NumViews);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CSSetUnorderedAccessViews(
    SerialiserType &ser, UINT StartSlot, UINT NumUAVs,
    ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumUAVs);
  SERIALISE_ELEMENT_ARRAY(ppUnorderedAccessViews, NumUAVs);
  SERIALISE_ELEMENT_ARRAY(pUAVInitialCounts, NumUAVs);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->CSUAVs, ppUnorderedAccessViews,
                                           StartSlot, NumUAVs);

    ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT];
    for(UINT i = 0; i < NumUAVs; i++)
      UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView1, ppUnorderedAccessViews[i]);

    // #mivance this isn't strictly correct...
    if(IsLoading(m_State))
      RecordOutputMergerStats(0, NULL, NULL, StartSlot, NumUAVs, UAVs);

    m_pRealContext->CSSetUnorderedAccessViews(StartSlot, NumUAVs, UAVs, pUAVInitialCounts);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::CSSetUnorderedAccessViews(
    UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
    const UINT *pUAVInitialCounts)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT];
  for(UINT i = 0; i < NumUAVs; i++)
  {
    if(ppUnorderedAccessViews[i] && IsCaptureMode(m_State))
    {
      if(IsActiveCapturing(m_State))
      {
        MarkResourceReferenced(GetIDForResource(ppUnorderedAccessViews[i]), eFrameRef_Read);
        MarkResourceReferenced(GetViewResourceResID(ppUnorderedAccessViews[i]), eFrameRef_Read);
      }

      MarkDirtyResource(GetViewResourceResID(ppUnorderedAccessViews[i]));
    }

    UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView1, ppUnorderedAccessViews[i]);
  }

  SERIALISE_TIME_CALL(
      m_pRealContext->CSSetUnorderedAccessViews(StartSlot, NumUAVs, UAVs, pUAVInitialCounts));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CSSetUnorderedAccessViews);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_CSSetUnorderedAccessViews(GET_SERIALISER, StartSlot, NumUAVs, ppUnorderedAccessViews,
                                        pUAVInitialCounts);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->CSUAVs, ppUnorderedAccessViews,
                                         StartSlot, NumUAVs);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CSSetSamplers(SerialiserType &ser, UINT StartSlot,
                                                         UINT NumSamplers,
                                                         ID3D11SamplerState *const *ppSamplers)
{
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumSamplers);
  SERIALISE_ELEMENT_ARRAY(ppSamplers, NumSamplers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordSamplerStats(ShaderStage::Compute, NumSamplers, ppSamplers);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Samplers, ppSamplers,
                                          StartSlot, NumSamplers);

    ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    for(UINT i = 0; i < NumSamplers; i++)
      samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

    m_pRealContext->CSSetSamplers(StartSlot, NumSamplers, samps);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::CSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                               ID3D11SamplerState *const *ppSamplers)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  for(UINT i = 0; i < NumSamplers; i++)
  {
    if(ppSamplers[i] && IsActiveCapturing(m_State))
      MarkResourceReferenced(GetIDForResource(ppSamplers[i]), eFrameRef_Read);

    samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);
  }

  SERIALISE_TIME_CALL(m_pRealContext->CSSetSamplers(StartSlot, NumSamplers, samps));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CSSetSamplers);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_CSSetSamplers(GET_SERIALISER, StartSlot, NumSamplers, ppSamplers);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Samplers, ppSamplers, StartSlot,
                                        NumSamplers);

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CSSetShader(SerialiserType &ser,
                                                       ID3D11ComputeShader *pShader,
                                                       ID3D11ClassInstance *const *ppClassInstances,
                                                       UINT NumClassInstances)
{
  SERIALISE_ELEMENT(pShader);
  SERIALISE_ELEMENT_ARRAY(ppClassInstances, NumClassInstances);
  SERIALISE_ELEMENT(NumClassInstances);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      RecordShaderStats(ShaderStage::Compute, m_CurrentPipelineState->CS.Object, pShader);

    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Instances, ppClassInstances, 0,
                                          NumClassInstances);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.NumInstances, NumClassInstances);
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Object,
                                          (ID3D11DeviceChild *)pShader);

    ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

    m_pRealContext->CSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11ComputeShader>, pShader), insts,
                                NumClassInstances);
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::CSSetShader(ID3D11ComputeShader *pComputeShader,
                                             ID3D11ClassInstance *const *ppClassInstances,
                                             UINT NumClassInstances)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
  if(ppClassInstances && NumClassInstances > 0)
    for(UINT i = 0; i < NumClassInstances; i++)
      insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);

  SERIALISE_TIME_CALL(m_pRealContext->CSSetShader(
      UNWRAP(WrappedID3D11Shader<ID3D11ComputeShader>, pComputeShader), insts, NumClassInstances));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CSSetShader);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_CSSetShader(GET_SERIALISER, pComputeShader, ppClassInstances, NumClassInstances);

    MarkResourceReferenced(GetIDForResource(pComputeShader), eFrameRef_Read);

    m_ContextRecord->AddChunk(scope.Get());
  }

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Object,
                                        (ID3D11DeviceChild *)pComputeShader);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.NumInstances, NumClassInstances);
  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Instances, ppClassInstances, 0,
                                        NumClassInstances);

  VerifyState();
}

#pragma endregion Compute Shader

#pragma region Dispatch

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_Dispatch(SerialiserType &ser, UINT ThreadGroupCountX,
                                                    UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
  SERIALISE_ELEMENT(ThreadGroupCountX);
  SERIALISE_ELEMENT(ThreadGroupCountY);
  SERIALISE_ELEMENT(ThreadGroupCountZ);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_pRealContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

    if(IsLoading(m_State))
    {
      RecordDispatchStats(false);

      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("Dispatch(%u, %u, %u)", ThreadGroupCountX, ThreadGroupCountY,
                                    ThreadGroupCountZ);
      draw.flags |= DrawFlags::Dispatch;

      draw.dispatchDimension[0] = ThreadGroupCountX;
      draw.dispatchDimension[1] = ThreadGroupCountY;
      draw.dispatchDimension[2] = ThreadGroupCountZ;

      if(ThreadGroupCountX == 0)
        m_pDevice->AddDebugMessage(
            MessageCategory::Execution, MessageSeverity::Medium, MessageSource::IncorrectAPIUse,
            "Dispatch call has ThreadGroup count X=0. This will do nothing, "
            "which is unusual for a non-indirect Dispatch. Did you mean X=1?");
      if(ThreadGroupCountY == 0)
        m_pDevice->AddDebugMessage(
            MessageCategory::Execution, MessageSeverity::Medium, MessageSource::IncorrectAPIUse,
            "Dispatch call has ThreadGroup count Y=0. This will do nothing, "
            "which is unusual for a non-indirect Dispatch. Did you mean Y=1?");
      if(ThreadGroupCountZ == 0)
        m_pDevice->AddDebugMessage(
            MessageCategory::Execution, MessageSeverity::Medium, MessageSource::IncorrectAPIUse,
            "Dispatch call has ThreadGroup count Z=0. This will do nothing, "
            "which is unusual for a non-indirect Dispatch. Did you mean Z=1?");

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                                          UINT ThreadGroupCountZ)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  MarkAPIActive();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(
      m_pRealContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::Dispatch);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_Dispatch(GET_SERIALISER, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_DispatchIndirect(SerialiserType &ser,
                                                            ID3D11Buffer *pBufferForArgs,
                                                            UINT AlignedByteOffsetForArgs)
{
  SERIALISE_ELEMENT(pBufferForArgs);
  SERIALISE_ELEMENT(AlignedByteOffsetForArgs);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pBufferForArgs)
    {
      m_pRealContext->DispatchIndirect(UNWRAP(WrappedID3D11Buffer, pBufferForArgs),
                                       AlignedByteOffsetForArgs);
    }

    if(IsLoading(m_State))
    {
      RecordDispatchStats(true);

      AddEvent();

      DrawcallDescription draw;

      std::string name = "DispatchIndirect(-, -, -)";
      if(pBufferForArgs)
      {
        struct DispatchArgs
        {
          UINT ThreadGroupCountX;
          UINT ThreadGroupCountY;
          UINT ThreadGroupCountZ;
        };

        bytebuf argarray;
        m_pDevice->GetDebugManager()->GetBufferData(pBufferForArgs, AlignedByteOffsetForArgs,
                                                    sizeof(DispatchArgs), argarray);

        if(argarray.size() >= sizeof(DispatchArgs))
        {
          DispatchArgs *args = (DispatchArgs *)&argarray[0];

          draw.dispatchDimension[0] = args->ThreadGroupCountX;
          draw.dispatchDimension[1] = args->ThreadGroupCountY;
          draw.dispatchDimension[2] = args->ThreadGroupCountZ;

          name = StringFormat::Fmt("DispatchIndirect(<%u, %u, %u>)", args->ThreadGroupCountX,
                                   args->ThreadGroupCountY, args->ThreadGroupCountZ);
        }
        else
        {
          name = "DispatchIndirect(<!, !, !>)";
          D3D11_BUFFER_DESC bufDesc;

          pBufferForArgs->GetDesc(&bufDesc);

          if(AlignedByteOffsetForArgs >= bufDesc.ByteWidth)
          {
            m_pDevice->AddDebugMessage(
                MessageCategory::Execution, MessageSeverity::High, MessageSource::IncorrectAPIUse,
                StringFormat::Fmt("Call to DispatchIndirect with buffer of %u "
                                  "bytes, but byte offset specified is %u bytes.",
                                  bufDesc.ByteWidth, AlignedByteOffsetForArgs));
          }
          else if(AlignedByteOffsetForArgs + sizeof(DispatchArgs) >= bufDesc.ByteWidth)
          {
            m_pDevice->AddDebugMessage(
                MessageCategory::Execution, MessageSeverity::High, MessageSource::IncorrectAPIUse,
                StringFormat::Fmt("Call to DispatchIndirect with buffer of %u "
                                  "bytes at offset of %u bytes, which leaves less than %u bytes "
                                  "for the arguments.",
                                  bufDesc.ByteWidth, AlignedByteOffsetForArgs, sizeof(DispatchArgs)));
          }
        }

        m_ResourceUses[GetIDForResource(pBufferForArgs)].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Indirect));
      }

      draw.name = name;
      draw.flags |= DrawFlags::Dispatch | DrawFlags::Indirect;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::DispatchIndirect(ID3D11Buffer *pBufferForArgs,
                                                  UINT AlignedByteOffsetForArgs)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  MarkAPIActive();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->DispatchIndirect(UNWRAP(WrappedID3D11Buffer, pBufferForArgs),
                                                       AlignedByteOffsetForArgs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::DispatchIndirect);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_DispatchIndirect(GET_SERIALISER, pBufferForArgs, AlignedByteOffsetForArgs);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }

  if(pBufferForArgs && IsActiveCapturing(m_State))
    MarkResourceReferenced(GetIDForResource(pBufferForArgs), eFrameRef_Read);
}

#pragma endregion Dispatch

#pragma region Execute

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_ExecuteCommandList(SerialiserType &ser,
                                                              ID3D11CommandList *pCommandList,
                                                              BOOL RestoreContextState_)
{
  SERIALISE_ELEMENT_LOCAL(CommandList, GetIDForResource(pCommandList))
      .TypedAs("ID3D11CommandList *"_lit);
  SERIALISE_ELEMENT_LOCAL(RestoreContextState, bool(RestoreContextState_ == TRUE));

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // The serialise of exec happens at the start of the inserted linearised commands from the
    // deferred command list. So if we're going to restore context state afterwards, save off our
    // current state. This will be restored in the Serialise_PostExecuteCommandList chunk.
    if(RestoreContextState)
    {
      SAFE_DELETE(m_DeferredSavedState);
      m_DeferredSavedState = new D3D11RenderState(this);
    }

    // From the Docs:
    // "Immediate context state is cleared before and after a command list is executed. A command
    // list has no concept of inheritance."
    ClearState();

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("ExecuteCommandList(List %s)", ToStr(CommandList).c_str());
      draw.flags |= DrawFlags::CmdList;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_PostExecuteCommandList(SerialiserType &ser,
                                                                  ID3D11CommandList *pCommandList,
                                                                  BOOL RestoreContextState_)
{
  SERIALISE_ELEMENT_LOCAL(CommandList, GetIDForResource(pCommandList))
      .TypedAs("ID3D11CommandList *"_lit);
  SERIALISE_ELEMENT_LOCAL(RestoreContextState, bool(RestoreContextState_ == TRUE));

  // this is a 'fake' call we insert after executing, to give us a chance to restore the state.
  if(IsReplayingAndReading())
  {
    if(RestoreContextState)
    {
      if(m_DeferredSavedState)
      {
        m_DeferredSavedState->ApplyState(this);
        SAFE_DELETE(m_DeferredSavedState);
      }
      else
      {
        RDCERR("Expected to have saved state from before execute saved, but didn't find one.");
      }
    }
    else
    {
      // if we don't restore the state, then it's cleared. There's no inheritance down from the
      // deferred context's state to the immediate context.
      ClearState();
    }
  }
  return true;
}

void WrappedID3D11DeviceContext::ExecuteCommandList(ID3D11CommandList *pCommandList,
                                                    BOOL RestoreContextState)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  RDCASSERT(GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

  SERIALISE_TIME_CALL(m_pRealContext->ExecuteCommandList(
      UNWRAP(WrappedID3D11CommandList, pCommandList), RestoreContextState));

  if(IsActiveCapturing(m_State))
  {
    {
      USE_SCRATCH_SERIALISER();
      GET_SERIALISER.SetDrawChunk();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::ExecuteCommandList);
      SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
      Serialise_ExecuteCommandList(GET_SERIALISER, pCommandList, RestoreContextState);
      m_ContextRecord->AddChunk(scope.Get());
    }

    WrappedID3D11CommandList *wrapped = (WrappedID3D11CommandList *)pCommandList;

    if(!wrapped->IsCaptured())
    {
      // we don't have this command list captured. This frame is no longer successful
      RDCWARN("Don't have command list %llu captured! This frame is unsuccessful.",
              wrapped->GetResourceID());
      m_SuccessfulCapture = false;
      m_FailureReason = CaptureFailed_UncappedCmdlist;
    }
    else
    {
      RDCDEBUG("Executed successful command list %llu", wrapped->GetResourceID());
      ResourceId contextId = wrapped->GetResourceID();

      D3D11ResourceRecord *cmdListRecord =
          m_pDevice->GetResourceManager()->GetResourceRecord(contextId);

      RDCASSERT(cmdListRecord);

      // insert all the deferred chunks immediately following the execute chunk.
      m_ContextRecord->AppendFrom(cmdListRecord);
      cmdListRecord->AddResourceReferences(m_pDevice->GetResourceManager());
    }

    {
      // insert a chunk to let us know on replay that we finished the command list's
      // chunks and we can restore the state
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::PostExecuteCommandList);
      SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
      Serialise_PostExecuteCommandList(GET_SERIALISER, pCommandList, RestoreContextState);
      m_ContextRecord->AddChunk(scope.Get());
    }

    m_CurrentPipelineState->MarkReferenced(this, false);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    WrappedID3D11CommandList *wrapped = (WrappedID3D11CommandList *)pCommandList;

    wrapped->MarkDirtyResources(m_pDevice->GetResourceManager());
  }

  if(!RestoreContextState)
    m_CurrentPipelineState->Clear();

  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_FinishCommandList(SerialiserType &ser,
                                                             BOOL RestoreDeferredContextState_,
                                                             ID3D11CommandList **ppCommandList)
{
  SERIALISE_ELEMENT_LOCAL(RestoreDeferredContextState, bool(RestoreDeferredContextState_ == TRUE));
  SERIALISE_ELEMENT_LOCAL(pCommandList, GetIDForResource(*ppCommandList))
      .TypedAs("ID3D11CommandList *"_lit);

  Serialise_DebugMessages(GET_SERIALISER);

  if(IsReplayingAndReading() && IsLoading(m_State))
  {
    AddEvent();

    DrawcallDescription draw;
    draw.name = StringFormat::Fmt("FinishCommandList(List %s)", ToStr(pCommandList).c_str());
    draw.flags |= DrawFlags::CmdList;

    AddDrawcall(draw, true);

    m_pDevice->AddResource(pCommandList, ResourceType::CommandBuffer, "Command List");

    // add the current deferred context ID as a parent
    m_pDevice->GetReplay()->GetResourceDesc(m_CurContextId).derivedResources.push_back(pCommandList);
    m_pDevice->GetReplay()->GetResourceDesc(pCommandList).parentResources.push_back(m_CurContextId);

    // don't include this as an 'initialisation chunk'
    m_pDevice->GetReplay()->GetResourceDesc(pCommandList).initialisationChunks.clear();
  }

  return true;
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_PostFinishCommandListSet(SerialiserType &ser,
                                                                    ID3D11CommandList *pCommandList)
{
  SERIALISE_ELEMENT_LOCAL(CommandList, GetIDForResource(pCommandList))
      .TypedAs("ID3D11CommandList *"_lit);

  D3D11RenderState RenderState(*m_CurrentPipelineState);

  SERIALISE_ELEMENT(RenderState).Named("Initial Pipeline State"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  // this is a 'fake' call we insert after finishing in a fresh deferred context, to allow us to
  // preserve the state after a previous finish stole all the serialised chunks.
  if(IsReplayingAndReading())
  {
    RenderState.ApplyState(this);
  }
  return true;
}

HRESULT WrappedID3D11DeviceContext::FinishCommandList(BOOL RestoreDeferredContextState,
                                                      ID3D11CommandList **ppCommandList)
{
  if(GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
  {
    m_pDevice->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                               MessageSource::IncorrectAPIUse,
                               "It is invalid to call FinishCommandList on an immediate context. "
                               "The call has been dropped from the capture.");
    return DXGI_ERROR_INVALID_CALL;
  }

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  ID3D11CommandList *real = NULL;
  HRESULT hr;
  SERIALISE_TIME_CALL(hr = m_pRealContext->FinishCommandList(RestoreDeferredContextState, &real));

  RDCASSERT(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED);

  bool cmdListSuccessful = m_SuccessfulCapture;

  if(!IsActiveCapturing(m_State) && !m_EmptyCommandList)
    cmdListSuccessful = false;

  WrappedID3D11CommandList *wrapped =
      new WrappedID3D11CommandList(real, m_pDevice, this, cmdListSuccessful);

  if(IsCaptureMode(m_State))
  {
    RDCASSERT(m_pDevice->GetResourceManager()->GetResourceRecord(wrapped->GetResourceID()) == NULL);

    D3D11ResourceRecord *record =
        m_pDevice->GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
    record->ResType = Resource_CommandList;
    record->Length = 0;
    record->InternalResource = true;

    if(IsActiveCapturing(m_State))
    {
      // if we got here and m_SuccessfulCapture is on, we have captured everything in this command
      // list
      if(m_SuccessfulCapture)
      {
        RDCDEBUG("Deferred Context %llu Finish()'d successfully! Got successful command list %llu",
                 GetResourceID(), wrapped->GetResourceID());

        RDCASSERT(wrapped->IsCaptured());

        ID3D11CommandList *w = wrapped;

        // serialise the finish marker
        {
          USE_SCRATCH_SERIALISER();
          GET_SERIALISER.SetDrawChunk();
          SCOPED_SERIALISE_CHUNK(D3D11Chunk::FinishCommandList);
          SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
          Serialise_FinishCommandList(GET_SERIALISER, RestoreDeferredContextState, &w);
          m_ContextRecord->AddChunk(scope.Get());
        }

        D3D11ResourceRecord *r =
            m_pDevice->GetResourceManager()->GetResourceRecord(wrapped->GetResourceID());
        RDCASSERT(r);

        // swap our chunks, references, and dirty resources with the 'baked' command list.
        m_ContextRecord->SwapChunks(r);
        wrapped->SwapReferences(m_DeferredReferences);
        wrapped->SwapDirtyResources(m_DeferredDirty);
      }
      else    // !m_SuccessfulCapture
      {
        RDCDEBUG(
            "Deferred Context %llu wasn't successful, but now we've Finish()'d so it is! Produced "
            "unsuccessful command list %llu.",
            GetResourceID(), wrapped->GetResourceID());

        RDCASSERT(!wrapped->IsCaptured());

        // need to clear out anything we had serialised before
        m_ContextRecord->LockChunks();
        while(m_ContextRecord->HasChunks())
        {
          Chunk *chunk = m_ContextRecord->GetLastChunk();
          SAFE_DELETE(chunk);
          m_ContextRecord->PopChunk();
        }
        m_ContextRecord->UnlockChunks();

        // clear the references, and delete the resource records so they aren't kept around forever
        for(ResourceId id : m_DeferredReferences)
        {
          D3D11ResourceRecord *deferredRecord =
              m_pDevice->GetResourceManager()->GetResourceRecord(id);
          if(deferredRecord)
            deferredRecord->Delete(m_pDevice->GetResourceManager());
        }

        m_DeferredReferences.clear();

        // clear the dirty marks
        m_DeferredDirty.clear();

        // It's now 'successful' again, being empty
        m_SuccessfulCapture = true;
      }

      // if we're supposed to restore, save the state to restore to now. This is because the next
      // recording to this deferred context is expected to have the same state, but we don't have
      // that state serialised right now. So we blat out the whole serialisation
      if(RestoreDeferredContextState)
      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::PostFinishCommandListSet);
        SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
        Serialise_PostFinishCommandListSet(GET_SERIALISER, wrapped);
        m_ContextRecord->AddChunk(scope.Get());
      }
    }
    else    // IsIdleCapturing(m_State)
    {
      // mark that this command list is empty so that if we immediately try and capture
      // we pick up on that.
      m_EmptyCommandList = true;

      // still need to propagate up dirty resources to the immediate context
      wrapped->SwapDirtyResources(m_DeferredDirty);

      // clear the references and decref the resource records
      for(ResourceId id : m_DeferredReferences)
      {
        D3D11ResourceRecord *deferredRecord = m_pDevice->GetResourceManager()->GetResourceRecord(id);
        if(deferredRecord)
          deferredRecord->Delete(m_pDevice->GetResourceManager());
      }

      m_DeferredReferences.clear();

      RDCDEBUG(
          "Deferred Context %llu not capturing at the moment, Produced unsuccessful command list "
          "%llu.",
          GetResourceID(), wrapped->GetResourceID());
    }
  }

  if(!RestoreDeferredContextState)
    m_CurrentPipelineState->Clear();

  VerifyState();

  if(ppCommandList)
    *ppCommandList = wrapped;

  return hr;
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_Flush(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    m_pRealContext->Flush();
  }

  return true;
}

void WrappedID3D11DeviceContext::Flush()
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->Flush());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::Flush);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_Flush(GET_SERIALISER);

    m_ContextRecord->AddChunk(scope.Get());

    m_CurrentPipelineState->MarkReferenced(this, false);
  }
}

#pragma endregion Execute

#pragma region Copy

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CopySubresourceRegion(
    SerialiserType &ser, ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY,
    UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox)
{
  SERIALISE_ELEMENT(pDstResource);
  SERIALISE_ELEMENT(DstSubresource);
  SERIALISE_ELEMENT(DstX);
  SERIALISE_ELEMENT(DstY);
  SERIALISE_ELEMENT(DstZ);
  SERIALISE_ELEMENT(pSrcResource);
  SERIALISE_ELEMENT(SrcSubresource);
  SERIALISE_ELEMENT_OPT(pSrcBox);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pDstResource && pSrcResource)
    {
      m_pRealContext->CopySubresourceRegion(
          GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, DstX, DstY, DstZ,
          GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, pSrcBox);
    }

    if(IsLoading(m_State))
    {
      ResourceId dstLiveID = GetIDForResource(pDstResource);
      ResourceId srcLiveID = GetIDForResource(pSrcResource);
      ResourceId dstOrigID = GetResourceManager()->GetOriginalID(dstLiveID);
      ResourceId srcOrigID = GetResourceManager()->GetOriginalID(srcLiveID);

      AddEvent();

      DrawcallDescription draw;
      draw.name = "CopySubresourceRegion(" + ToStr(dstOrigID) + ", " + ToStr(srcOrigID) + ")";
      draw.flags |= DrawFlags::Copy;

      if(pDstResource && pSrcResource)
      {
        draw.copySource = srcOrigID;
        draw.copyDestination = dstOrigID;

        if(m_CurEventID)
        {
          if(dstLiveID == srcLiveID)
          {
            m_ResourceUses[dstLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::Copy));
          }
          else
          {
            m_ResourceUses[dstLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::CopyDst));
            m_ResourceUses[srcLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::CopySrc));
          }
        }
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::CopySubresourceRegion(ID3D11Resource *pDstResource,
                                                       UINT DstSubresource, UINT DstX, UINT DstY,
                                                       UINT DstZ, ID3D11Resource *pSrcResource,
                                                       UINT SrcSubresource, const D3D11_BOX *pSrcBox)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->CopySubresourceRegion(
      m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, DstX, DstY,
      DstZ, m_pDevice->GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, pSrcBox));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CopySubresourceRegion);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_CopySubresourceRegion(GET_SERIALISER, pDstResource, DstSubresource, DstX, DstY, DstZ,
                                    pSrcResource, SrcSubresource, pSrcBox);

    D3D11ResourceRecord *record =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
    RDCASSERT(record);
    D3D11ResourceRecord *srcRecord =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcResource));
    RDCASSERT(srcRecord);
    record->AddParent(srcRecord);

    m_ContextRecord->AddChunk(scope.Get());

    MarkDirtyResource(GetIDForResource(pDstResource));

    // assume partial update
    MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_Read);
    MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_PartialWrite);
    MarkResourceReferenced(GetIDForResource(pSrcResource), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    D3D11ResourceRecord *record =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
    RDCASSERT(record);

    D3D11ResourceRecord *srcRecord =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcResource));
    RDCASSERT(srcRecord);

    if(m_pDevice->GetResourceManager()->IsResourceDirty(GetIDForResource(pSrcResource)))
    {
      MarkDirtyResource(GetIDForResource(pDstResource));
    }
    else if(WrappedID3D11Buffer::IsAlloc(pDstResource) && WrappedID3D11Buffer::IsAlloc(pSrcResource))
    {
      // perform copy manually (since we have buffer contents locally)

      RDCASSERT(record->DataInSerialiser);
      RDCASSERT(srcRecord->DataInSerialiser);

      byte *from = srcRecord->GetDataPtr();
      byte *to = record->GetDataPtr();

      to += DstX;

      size_t length = (size_t)srcRecord->Length;

      if(pSrcBox)
      {
        from += pSrcBox->left;
        length = pSrcBox->right - pSrcBox->left;
      }

      if(length > 0)
      {
        memcpy(to, from, length);
      }
    }
    else
    {
      // GPU dirty. Just let initial state handle this.

      MarkDirtyResource(GetIDForResource(pDstResource));
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CopyResource(SerialiserType &ser,
                                                        ID3D11Resource *pDstResource,
                                                        ID3D11Resource *pSrcResource)
{
  SERIALISE_ELEMENT(pDstResource);
  SERIALISE_ELEMENT(pSrcResource);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pDstResource && pSrcResource)
    {
      m_pRealContext->CopyResource(m_pDevice->GetResourceManager()->UnwrapResource(pDstResource),
                                   m_pDevice->GetResourceManager()->UnwrapResource(pSrcResource));
    }

    if(IsLoading(m_State))
    {
      ResourceId dstLiveID = GetIDForResource(pDstResource);
      ResourceId srcLiveID = GetIDForResource(pSrcResource);
      ResourceId dstOrigID = GetResourceManager()->GetOriginalID(dstLiveID);
      ResourceId srcOrigID = GetResourceManager()->GetOriginalID(srcLiveID);

      AddEvent();

      DrawcallDescription draw;
      draw.name = "CopyResource(" + ToStr(dstOrigID) + ", " + ToStr(srcOrigID) + ")";
      draw.flags |= DrawFlags::Copy;

      if(pDstResource && pSrcResource)
      {
        draw.copySource = srcOrigID;
        draw.copyDestination = dstOrigID;

        if(m_CurEventID)
        {
          if(dstLiveID == srcLiveID)
          {
            m_ResourceUses[dstLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::Copy));
          }
          else
          {
            m_ResourceUses[dstLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::CopyDst));
            m_ResourceUses[srcLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::CopySrc));
          }
        }
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::CopyResource(ID3D11Resource *pDstResource,
                                              ID3D11Resource *pSrcResource)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(
      m_pRealContext->CopyResource(m_pDevice->GetResourceManager()->UnwrapResource(pDstResource),
                                   m_pDevice->GetResourceManager()->UnwrapResource(pSrcResource)));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CopyResource);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_CopyResource(GET_SERIALISER, pDstResource, pSrcResource);

    D3D11ResourceRecord *record =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
    RDCASSERT(record);
    D3D11ResourceRecord *srcRecord =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcResource));
    RDCASSERT(srcRecord);
    record->AddParent(srcRecord);

    m_ContextRecord->AddChunk(scope.Get());

    MarkDirtyResource(GetIDForResource(pDstResource));

    MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_PartialWrite);
    MarkResourceReferenced(GetIDForResource(pSrcResource), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    D3D11ResourceRecord *record =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
    RDCASSERT(record);

    D3D11ResourceRecord *srcRecord =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcResource));
    RDCASSERT(srcRecord);

    record->UpdateCount++;

    if(record->UpdateCount > 5 ||
       m_pDevice->GetResourceManager()->IsResourceDirty(GetIDForResource(pSrcResource)))
    {
      MarkDirtyResource(GetIDForResource(pDstResource));
    }
    else if(WrappedID3D11Buffer::IsAlloc(pDstResource) && WrappedID3D11Buffer::IsAlloc(pSrcResource))
    {
      // perform copy manually (since we have buffer contents locally)

      RDCASSERT(record->DataInSerialiser);
      RDCASSERT(srcRecord->DataInSerialiser);

      byte *from = srcRecord->GetDataPtr();
      byte *to = record->GetDataPtr();

      memcpy(to, from, (size_t)record->Length);
    }
    else if((WrappedID3D11Texture1D::IsAlloc(pDstResource) &&
             WrappedID3D11Texture1D::IsAlloc(pSrcResource)) ||
            (WrappedID3D11Texture2D1::IsAlloc(pDstResource) &&
             WrappedID3D11Texture2D1::IsAlloc(pSrcResource)) ||
            (WrappedID3D11Texture3D1::IsAlloc(pDstResource) &&
             WrappedID3D11Texture3D1::IsAlloc(pSrcResource)))
    {
      if(record->DataInSerialiser && srcRecord->DataInSerialiser)
      {
        RDCASSERT(record->NumSubResources == srcRecord->NumSubResources);

        for(int i = 0; i < record->NumSubResources; i++)
        {
          byte *from = srcRecord->SubResources[i]->GetDataPtr();
          byte *to = record->SubResources[i]->GetDataPtr();

          memcpy(to, from, (size_t)record->SubResources[i]->Length);
        }
      }
      else
      {
        // can't copy without data allocated
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::CopyResource);
        SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
        Serialise_CopyResource(GET_SERIALISER, pDstResource, pSrcResource);

        record->AddChunk(scope.Get());
        record->AddParent(srcRecord);
      }
    }
    else
    {
      RDCERR("Unexpected resource type");
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_UpdateSubresource(
    SerialiserType &ser, ID3D11Resource *pDstResource, UINT DstSubresource,
    const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
  // pass ~0U as the flags to indicate this came for UpdateSubresource, so we know to apply
  // deferred context workarounds or not
  return Serialise_UpdateSubresource1(ser, pDstResource, DstSubresource, pDstBox, pSrcData,
                                      SrcRowPitch, SrcDepthPitch, ~0U);
}

void WrappedID3D11DeviceContext::UpdateSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                                                   const D3D11_BOX *pDstBox, const void *pSrcData,
                                                   UINT SrcRowPitch, UINT SrcDepthPitch)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->UpdateSubresource(
      m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, pDstBox,
      pSrcData, SrcRowPitch, SrcDepthPitch));

  if(pDstBox && m_NeedUpdateSubWorkaround && GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
  {
    // we need to apply the *inverse* of the workaround, which matches the broken D3D behaviour
    // so that we end up pointing at the expected data.

    D3D11_BOX alignedBox = *pDstBox;

    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

    if(WrappedID3D11Texture1D::IsAlloc(pDstResource))
    {
      D3D11_TEXTURE1D_DESC desc;
      ((WrappedID3D11Texture1D *)pDstResource)->GetDesc(&desc);
      fmt = desc.Format;
    }
    else if(WrappedID3D11Texture2D1::IsAlloc(pDstResource))
    {
      D3D11_TEXTURE2D_DESC desc;
      ((WrappedID3D11Texture2D1 *)pDstResource)->GetDesc(&desc);
      fmt = desc.Format;
    }
    else if(WrappedID3D11Texture3D1::IsAlloc(pDstResource))
    {
      D3D11_TEXTURE3D_DESC desc;
      ((WrappedID3D11Texture3D1 *)pDstResource)->GetDesc(&desc);
      fmt = desc.Format;
    }
    else
    {
      RDCASSERT(WrappedID3D11Buffer::IsAlloc(pDstResource));
    }

    // convert from pixels to blocks
    if(IsBlockFormat(fmt))
    {
      alignedBox.left /= 4;
      alignedBox.right /= 4;
      alignedBox.top /= 4;
      alignedBox.bottom /= 4;
    }

    // if we couldn't get a format it's a buffer, so work in bytes
    if(fmt != DXGI_FORMAT_UNKNOWN)
      pSrcData = ((const BYTE *)pSrcData) + (alignedBox.front * SrcDepthPitch) +
                 (alignedBox.top * SrcRowPitch) + (alignedBox.left * GetByteSize(1, 1, 1, fmt, 0));
    else
      pSrcData = ((const BYTE *)pSrcData) + (alignedBox.left);
  }

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::UpdateSubresource);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_UpdateSubresource(GET_SERIALISER, pDstResource, DstSubresource, pDstBox, pSrcData,
                                SrcRowPitch, SrcDepthPitch);

    MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_PartialWrite);

    MarkDirtyResource(GetIDForResource(pDstResource));

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsBackgroundCapturing(m_State))
  {
    ResourceId idx = GetIDForResource(pDstResource);
    D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(idx);
    RDCASSERT(record);

    // buffers MUST update the whole resource, and don't have any subresources,
    // so this effectively becomes just a map/unmap pair.
    if(WrappedID3D11Buffer::IsAlloc(pDstResource))
    {
      RDCASSERT(record->NumSubResources == 0);

      size_t offs = 0;
      size_t length = (size_t)record->Length;
      if(pDstBox)
      {
        offs += pDstBox->left;
        length = RDCMIN((uint32_t)length, pDstBox->right - pDstBox->left);
      }

      RDCASSERT(record->DataInSerialiser);

      void *ptr = record->GetDataPtr() + offs;

      memcpy(ptr, pSrcData, length);
    }
    else if(WrappedID3D11Texture1D::IsAlloc(pDstResource) ||
            WrappedID3D11Texture2D1::IsAlloc(pDstResource) ||
            WrappedID3D11Texture3D1::IsAlloc(pDstResource))
    {
      RDCASSERT(record->Length == 1 && record->NumSubResources > 0);

      if(DstSubresource >= (UINT)record->NumSubResources)
      {
        RDCERR("DstSubresource %u >= %u (num subresources)", DstSubresource, record->NumSubResources);
        return;
      }

      // this record isn't in the log already, write out a chunk that we can update after.
      if(!record->SubResources[DstSubresource]->DataInSerialiser)
      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::UpdateSubresource);
        SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);

        Serialise_UpdateSubresource(GET_SERIALISER, pDstResource, DstSubresource, pDstBox, pSrcData,
                                    SrcRowPitch, SrcDepthPitch);

        Chunk *chunk = scope.Get();

        record->AddChunk(chunk);
        record->SubResources[DstSubresource]->SetDataPtr(chunk->GetData());

        record->SubResources[DstSubresource]->DataInSerialiser = true;
      }

      {
        RDCASSERT(record->SubResources[DstSubresource]->DataInSerialiser);

        void *ptr = record->SubResources[DstSubresource]->GetDataPtr();

        // if the box is empty, we don't have to do anything! hooray!
        if(pDstBox && (pDstBox->back == pDstBox->front || pDstBox->left == pDstBox->right ||
                       pDstBox->top == pDstBox->bottom))
        {
          // empty, do nothing.
        }
        else
        {
          WrappedID3D11Texture1D *tex1 = WrappedID3D11Texture1D::IsAlloc(pDstResource)
                                             ? (WrappedID3D11Texture1D *)pDstResource
                                             : NULL;
          WrappedID3D11Texture2D1 *tex2 = WrappedID3D11Texture2D1::IsAlloc(pDstResource)
                                              ? (WrappedID3D11Texture2D1 *)pDstResource
                                              : NULL;
          WrappedID3D11Texture3D1 *tex3 = WrappedID3D11Texture3D1::IsAlloc(pDstResource)
                                              ? (WrappedID3D11Texture3D1 *)pDstResource
                                              : NULL;

          RDCASSERT(tex1 || tex2 || tex3);

          DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
          UINT subWidth = 1;
          UINT subHeight = 1;
          UINT subDepth = 1;

          UINT mipLevel = GetMipForSubresource(pDstResource, DstSubresource);

          if(tex1)
          {
            D3D11_TEXTURE1D_DESC desc = {0};
            tex1->GetDesc(&desc);
            fmt = desc.Format;
            subWidth = RDCMAX(1U, desc.Width >> mipLevel);
          }
          else if(tex2)
          {
            D3D11_TEXTURE2D_DESC desc = {0};
            tex2->GetDesc(&desc);
            fmt = desc.Format;
            subWidth = RDCMAX(1U, desc.Width >> mipLevel);
            subHeight = RDCMAX(1U, desc.Height >> mipLevel);
          }
          else if(tex3)
          {
            D3D11_TEXTURE3D_DESC desc = {0};
            tex3->GetDesc(&desc);
            fmt = desc.Format;
            subWidth = RDCMAX(1U, desc.Width >> mipLevel);
            subHeight = RDCMAX(1U, desc.Height >> mipLevel);
            subDepth = RDCMAX(1U, desc.Depth >> mipLevel);
          }

          UINT boxWidth = pDstBox ? pDstBox->right - pDstBox->left : subWidth;
          UINT boxHeight = pDstBox ? pDstBox->bottom - pDstBox->top : subHeight;
          UINT boxDepth = pDstBox ? pDstBox->back - pDstBox->front : subDepth;

          UINT boxTop = pDstBox ? pDstBox->top : 0;

          UINT DstRowPitch = GetRowPitch(subWidth, fmt, 0);
          UINT DstBoxRowPitch = GetRowPitch(boxWidth, fmt, 0);
          UINT DstSlicePitch = GetByteSize(subWidth, subHeight, 1, fmt, 0);

          // for block formats, rows are in blocks (so height is squished essentially)
          if(IsBlockFormat(fmt))
          {
            subWidth = AlignUp4(subWidth);
            subHeight = AlignUp4(RDCMAX(1U, subHeight / 4));
            boxHeight = RDCMAX(1U, boxHeight / 4);
            boxTop = RDCMAX(0U, boxTop / 4);
          }

          RDCASSERT(boxWidth <= subWidth && boxHeight <= subHeight && boxDepth <= subDepth);

          bool totalUpdate = false;

          // if there is no box, it's a totalUpdate (boxwidth/height are equal by inspection from
          // the initialisation above)
          // if the box describes the whole subresource, it's a totalUpdate
          if(boxWidth == subWidth && boxHeight == subHeight && boxDepth == subDepth)
            totalUpdate = true;

          // fast path for a total update from a source of the same size
          if(totalUpdate &&
             ((tex1 && (UINT)record->SubResources[DstSubresource]->Length == SrcRowPitch) ||
              (tex2 && (UINT)record->SubResources[DstSubresource]->Length == SrcRowPitch * subHeight) ||
              (tex3 && (UINT)record->SubResources[DstSubresource]->Length == SrcDepthPitch * subDepth)))
          {
            memcpy(ptr, pSrcData, (size_t)record->SubResources[DstSubresource]->Length);
          }
          else
          {
            // need to fall back to copying row by row from the source
            byte *dstBuf = (byte *)ptr;
            byte *src = (byte *)pSrcData;

            // if we have a box, skip to the front of it
            if(pDstBox)
              dstBuf += DstSlicePitch * pDstBox->front;

            for(UINT slice = 0; slice < boxDepth; slice++)
            {
              byte *slicedst = dstBuf;
              byte *slicesrc = src;

              // if we have a box, skip to the top of it
              if(pDstBox)
                slicedst += DstRowPitch * boxTop;

              for(UINT row = 0; row < boxHeight; row++)
              {
                byte *rowdst = slicedst;

                // if we have a box, skip to the left of it
                if(pDstBox && pDstBox->left > 0)
                  rowdst += GetByteSize(pDstBox->left, 1, 1, fmt, 0);

                memcpy(rowdst, slicesrc, DstBoxRowPitch);

                slicedst += DstRowPitch;
                slicesrc += SrcRowPitch;
              }

              dstBuf += DstSlicePitch;
              src += SrcDepthPitch;
            }
          }
        }
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_CopyStructureCount(SerialiserType &ser,
                                                              ID3D11Buffer *pDstBuffer,
                                                              UINT DstAlignedByteOffset,
                                                              ID3D11UnorderedAccessView *pSrcView)
{
  SERIALISE_ELEMENT(pDstBuffer);
  SERIALISE_ELEMENT(DstAlignedByteOffset);
  SERIALISE_ELEMENT(pSrcView);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pDstBuffer && pSrcView)
  {
    m_pRealContext->CopyStructureCount(UNWRAP(WrappedID3D11Buffer, pDstBuffer), DstAlignedByteOffset,
                                       UNWRAP(WrappedID3D11UnorderedAccessView1, pSrcView));

    if(IsLoading(m_State))
    {
      WrappedID3D11UnorderedAccessView1 *view = (WrappedID3D11UnorderedAccessView1 *)pSrcView;

      ResourceId dstLiveID = GetIDForResource(pDstBuffer);
      ResourceId srcLiveID = view->GetResourceResID();
      ResourceId dstOrigID = GetResourceManager()->GetOriginalID(dstLiveID);
      ResourceId srcOrigID = GetResourceManager()->GetOriginalID(srcLiveID);

      AddEvent();

      DrawcallDescription draw;
      draw.name = "CopyStructureCount(" + ToStr(dstOrigID) + ", " + ToStr(srcOrigID) + ")";
      draw.flags |= DrawFlags::Copy;

      if(pDstBuffer && pSrcView)
      {
        draw.copySource = srcOrigID;
        draw.copyDestination = dstOrigID;

        if(m_CurEventID)
        {
          if(dstLiveID == srcLiveID)
          {
            m_ResourceUses[dstLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::Copy));
          }
          else
          {
            m_ResourceUses[dstLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::CopyDst));
            m_ResourceUses[srcLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::CopySrc));
          }
        }
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::CopyStructureCount(ID3D11Buffer *pDstBuffer,
                                                    UINT DstAlignedByteOffset,
                                                    ID3D11UnorderedAccessView *pSrcView)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->CopyStructureCount(
      UNWRAP(WrappedID3D11Buffer, pDstBuffer), DstAlignedByteOffset,
      UNWRAP(WrappedID3D11UnorderedAccessView1, pSrcView)));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CopyStructureCount);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_CopyStructureCount(GET_SERIALISER, pDstBuffer, DstAlignedByteOffset, pSrcView);

    m_ContextRecord->AddChunk(scope.Get());

    MarkDirtyResource(GetIDForResource(pDstBuffer));

    MarkResourceReferenced(GetIDForResource(pDstBuffer), eFrameRef_Read);
    MarkResourceReferenced(GetIDForResource(pDstBuffer), eFrameRef_PartialWrite);

    MarkResourceReferenced(GetIDForResource(pSrcView), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    // needs to go into device serialiser

    D3D11ResourceRecord *record =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstBuffer));
    RDCASSERT(record);

    D3D11ResourceRecord *srcRecord =
        m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcView));
    RDCASSERT(srcRecord);

    record->AddParent(srcRecord);

    MarkDirtyResource(GetIDForResource(pDstBuffer));
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_ResolveSubresource(SerialiserType &ser,
                                                              ID3D11Resource *pDstResource,
                                                              UINT DstSubresource,
                                                              ID3D11Resource *pSrcResource,
                                                              UINT SrcSubresource, DXGI_FORMAT Format)
{
  SERIALISE_ELEMENT(pDstResource);
  SERIALISE_ELEMENT(DstSubresource);
  SERIALISE_ELEMENT(pSrcResource);
  SERIALISE_ELEMENT(SrcSubresource);
  SERIALISE_ELEMENT(Format);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pDstResource && pSrcResource)
    {
      m_pRealContext->ResolveSubresource(
          m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource,
          m_pDevice->GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, Format);
    }

    if(IsLoading(m_State))
    {
      ResourceId dstLiveID = GetIDForResource(pDstResource);
      ResourceId srcLiveID = GetIDForResource(pSrcResource);
      ResourceId dstOrigID = GetResourceManager()->GetOriginalID(dstLiveID);
      ResourceId srcOrigID = GetResourceManager()->GetOriginalID(srcLiveID);

      AddEvent();

      DrawcallDescription draw;
      draw.name = "ResolveSubresource(" + ToStr(dstOrigID) + ", " + ToStr(srcOrigID) + ")";
      draw.flags |= DrawFlags::Resolve;

      if(pDstResource && pSrcResource)
      {
        draw.copySource = srcOrigID;
        draw.copyDestination = dstOrigID;

        if(m_CurEventID)
        {
          if(dstLiveID == srcLiveID)
          {
            m_ResourceUses[dstLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::Resolve));
          }
          else
          {
            m_ResourceUses[dstLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::ResolveDst));
            m_ResourceUses[srcLiveID].push_back(EventUsage(m_CurEventID, ResourceUsage::ResolveSrc));
          }
        }
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::ResolveSubresource(ID3D11Resource *pDstResource,
                                                    UINT DstSubresource, ID3D11Resource *pSrcResource,
                                                    UINT SrcSubresource, DXGI_FORMAT Format)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->ResolveSubresource(
      m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource,
      m_pDevice->GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, Format));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::ResolveSubresource);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_ResolveSubresource(GET_SERIALISER, pDstResource, DstSubresource, pSrcResource,
                                 SrcSubresource, Format);

    m_ContextRecord->AddChunk(scope.Get());

    MarkDirtyResource(GetIDForResource(pDstResource));
    MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_Read);
    MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_PartialWrite);
    MarkResourceReferenced(GetIDForResource(pSrcResource), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    MarkDirtyResource(GetIDForResource(pDstResource));
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_GenerateMips(SerialiserType &ser,
                                                        ID3D11ShaderResourceView *pShaderResourceView)
{
  SERIALISE_ELEMENT(pShaderResourceView);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pShaderResourceView)
    {
      m_pRealContext->GenerateMips(UNWRAP(WrappedID3D11ShaderResourceView1, pShaderResourceView));
    }

    if(IsLoading(m_State))
    {
      WrappedID3D11ShaderResourceView1 *view =
          (WrappedID3D11ShaderResourceView1 *)pShaderResourceView;

      if(view)
      {
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::GenMips, view->GetResourceID()));
      }

      AddEvent();

      DrawcallDescription draw;
      draw.name =
          "GenerateMips(" +
          ToStr(GetResourceManager()->GetOriginalID(GetIDForResource(pShaderResourceView))) + ")";
      draw.flags |= DrawFlags::GenMips;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::GenerateMips(ID3D11ShaderResourceView *pShaderResourceView)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  if(!pShaderResourceView)
    return;

  SERIALISE_TIME_CALL(
      m_pRealContext->GenerateMips(UNWRAP(WrappedID3D11ShaderResourceView1, pShaderResourceView)));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::GenerateMips);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_GenerateMips(GET_SERIALISER, pShaderResourceView);

    m_ContextRecord->AddChunk(scope.Get());

    ResourceId id = GetViewResourceResID(pShaderResourceView);

    MarkDirtyResource(id);

    MarkResourceReferenced(id, eFrameRef_Read);
    MarkResourceReferenced(id, eFrameRef_PartialWrite);
    MarkResourceReferenced(GetIDForResource(pShaderResourceView), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    MarkDirtyResource(GetViewResourceResID(pShaderResourceView));
  }
}

#pragma endregion Copy

#pragma region Clear

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_ClearState(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    // end stream-out queries for outgoing targets
    for(UINT b = 0; b < D3D11_SO_STREAM_COUNT; b++)
    {
      ID3D11Buffer *buf = m_CurrentPipelineState->SO.Buffers[b];

      if(buf)
      {
        ResourceId id = GetIDForResource(buf);

        m_pRealContext->End(m_StreamOutCounters[id].query);
        m_StreamOutCounters[id].running = false;
      }
    }

    m_CurrentPipelineState->Clear();
    m_pRealContext->ClearState();
    VerifyState();
  }

  return true;
}

void WrappedID3D11DeviceContext::ClearState()
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->ClearState());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::ClearState);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_ClearState(GET_SERIALISER);

    m_ContextRecord->AddChunk(scope.Get());
  }

  // end stream-out queries for outgoing targets
  for(UINT b = 0; b < D3D11_SO_STREAM_COUNT; b++)
  {
    ID3D11Buffer *buf = m_CurrentPipelineState->SO.Buffers[b];

    if(buf)
    {
      ResourceId id = GetIDForResource(buf);

      m_pRealContext->End(m_StreamOutCounters[id].query);
      m_StreamOutCounters[id].running = false;
    }
  }

  m_CurrentPipelineState->Clear();
  VerifyState();
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_ClearRenderTargetView(
    SerialiserType &ser, ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4])
{
  SERIALISE_ELEMENT(pRenderTargetView);
  SERIALISE_ELEMENT_ARRAY(ColorRGBA, 4);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pRenderTargetView)
    {
      m_pRealContext->ClearRenderTargetView(
          UNWRAP(WrappedID3D11RenderTargetView1, pRenderTargetView), ColorRGBA);
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      WrappedID3D11RenderTargetView1 *view = (WrappedID3D11RenderTargetView1 *)pRenderTargetView;

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("ClearRenderTargetView(%f, %f, %f, %f)", ColorRGBA[0],
                                    ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);
      draw.flags |= DrawFlags::Clear | DrawFlags::ClearColor;

      if(view)
      {
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView,
                                                       const FLOAT ColorRGBA[4])
{
  DrainAnnotationQueue();

  if(pRenderTargetView == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->ClearRenderTargetView(
      UNWRAP(WrappedID3D11RenderTargetView1, pRenderTargetView), ColorRGBA));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::ClearRenderTargetView);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_ClearRenderTargetView(GET_SERIALISER, pRenderTargetView, ColorRGBA);

    if(pRenderTargetView)
    {
      MarkResourceReferenced(GetViewResourceResID(pRenderTargetView), eFrameRef_PartialWrite);
      MarkResourceReferenced(GetIDForResource(pRenderTargetView), eFrameRef_Read);
    }

    MarkDirtyResource(GetViewResourceResID(pRenderTargetView));

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsBackgroundCapturing(m_State))
  {
    MarkDirtyResource(GetViewResourceResID(pRenderTargetView));
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_ClearUnorderedAccessViewUint(
    SerialiserType &ser, ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values[4])
{
  SERIALISE_ELEMENT(pUnorderedAccessView);
  SERIALISE_ELEMENT_ARRAY(Values, 4);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pUnorderedAccessView)
    {
      m_pRealContext->ClearUnorderedAccessViewUint(
          UNWRAP(WrappedID3D11UnorderedAccessView1, pUnorderedAccessView), Values);
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      WrappedID3D11UnorderedAccessView1 *view =
          (WrappedID3D11UnorderedAccessView1 *)pUnorderedAccessView;

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("ClearUnorderedAccessViewUint(%u, %u, %u, %u)", Values[0],
                                    Values[1], Values[2], Values[3]);
      draw.flags |= DrawFlags::Clear;

      if(view)
      {
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::ClearUnorderedAccessViewUint(
    ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values[4])
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->ClearUnorderedAccessViewUint(
      UNWRAP(WrappedID3D11UnorderedAccessView1, pUnorderedAccessView), Values));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::ClearUnorderedAccessViewUint);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_ClearUnorderedAccessViewUint(GET_SERIALISER, pUnorderedAccessView, Values);

    if(pUnorderedAccessView)
    {
      MarkResourceReferenced(GetViewResourceResID(pUnorderedAccessView), eFrameRef_PartialWrite);
      MarkResourceReferenced(GetIDForResource(pUnorderedAccessView), eFrameRef_Read);
    }

    MarkDirtyResource(GetViewResourceResID(pUnorderedAccessView));

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsBackgroundCapturing(m_State))
  {
    MarkDirtyResource(GetViewResourceResID(pUnorderedAccessView));
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_ClearUnorderedAccessViewFloat(
    SerialiserType &ser, ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values[4])
{
  SERIALISE_ELEMENT(pUnorderedAccessView);
  SERIALISE_ELEMENT_ARRAY(Values, 4);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pUnorderedAccessView)
    {
      m_pRealContext->ClearUnorderedAccessViewFloat(
          UNWRAP(WrappedID3D11UnorderedAccessView1, pUnorderedAccessView), Values);
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      WrappedID3D11UnorderedAccessView1 *view =
          (WrappedID3D11UnorderedAccessView1 *)pUnorderedAccessView;

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("ClearUnorderedAccessViewFloat(%f, %f, %f, %f)", Values[0],
                                    Values[1], Values[2], Values[3]);
      draw.flags |= DrawFlags::Clear;

      if(view)
      {
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::ClearUnorderedAccessViewFloat(
    ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values[4])
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->ClearUnorderedAccessViewFloat(
      UNWRAP(WrappedID3D11UnorderedAccessView1, pUnorderedAccessView), Values));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::ClearUnorderedAccessViewFloat);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_ClearUnorderedAccessViewFloat(GET_SERIALISER, pUnorderedAccessView, Values);

    if(pUnorderedAccessView)
    {
      MarkResourceReferenced(GetViewResourceResID(pUnorderedAccessView), eFrameRef_PartialWrite);
      MarkResourceReferenced(GetIDForResource(pUnorderedAccessView), eFrameRef_Read);
    }

    MarkDirtyResource(GetViewResourceResID(pUnorderedAccessView));

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsBackgroundCapturing(m_State))
  {
    MarkDirtyResource(GetViewResourceResID(pUnorderedAccessView));
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_ClearDepthStencilView(
    SerialiserType &ser, ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth,
    UINT8 Stencil)
{
  SERIALISE_ELEMENT(pDepthStencilView);
  SERIALISE_ELEMENT_TYPED(D3D11_CLEAR_FLAG, ClearFlags);
  SERIALISE_ELEMENT(Depth);
  SERIALISE_ELEMENT(Stencil);

  Serialise_DebugMessages(GET_SERIALISER);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pDepthStencilView)
    {
      m_pRealContext->ClearDepthStencilView(
          UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView), ClearFlags, Depth, Stencil);
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      WrappedID3D11DepthStencilView *view = (WrappedID3D11DepthStencilView *)pDepthStencilView;

      DrawcallDescription draw;
      if(ClearFlags == (D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL))
        draw.name = StringFormat::Fmt("ClearDepthStencilView(D=%f, S=%02hhx)", Depth, Stencil);
      else if(ClearFlags == D3D11_CLEAR_DEPTH)
        draw.name = StringFormat::Fmt("ClearDepthStencilView(D=%f)", Depth);
      else if(ClearFlags == D3D11_CLEAR_STENCIL)
        draw.name = StringFormat::Fmt("ClearDepthStencilView(S=%02hhx)", Stencil);
      else
        draw.name = "ClearDepthStencilView(None)";
      draw.flags |= DrawFlags::Clear | DrawFlags::ClearDepthStencil;

      if(view)
      {
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Clear, view->GetResourceID()));
        draw.copyDestination =
            m_pDevice->GetResourceManager()->GetOriginalID(view->GetResourceResID());
      }

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView,
                                                       UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
  DrainAnnotationQueue();

  if(pDepthStencilView == NULL)
    return;

  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->ClearDepthStencilView(
      UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView), ClearFlags, Depth, Stencil));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    GET_SERIALISER.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::ClearDepthStencilView);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_ClearDepthStencilView(GET_SERIALISER, pDepthStencilView, ClearFlags, Depth, Stencil);

    if(pDepthStencilView)
    {
      MarkResourceReferenced(GetViewResourceResID(pDepthStencilView), eFrameRef_PartialWrite);
      MarkResourceReferenced(GetIDForResource(pDepthStencilView), eFrameRef_Read);
    }

    MarkDirtyResource(GetViewResourceResID(pDepthStencilView));

    m_ContextRecord->AddChunk(scope.Get());
  }
  else if(IsBackgroundCapturing(m_State))
  {
    MarkDirtyResource(GetViewResourceResID(pDepthStencilView));
  }
}

#pragma endregion Clear

#pragma region Misc

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_Begin(SerialiserType &ser, ID3D11Asynchronous *pAsync)
{
  SERIALISE_ELEMENT(pAsync);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pAsync)
  {
    ID3D11Asynchronous *unwrapped = NULL;

    // only replay predicates which can affect rendering, don't re-submit queries or counters (that
    // might even interfere with queries we want to run)
    if(WrappedID3D11Predicate::IsAlloc(pAsync))
      unwrapped = UNWRAP(WrappedID3D11Predicate, pAsync);

    // if you change this to replay other types, check with Serialise_CreateCounter which creates
    // dummy queries to ensure it always succeeds.

    if(unwrapped)
      m_pRealContext->Begin(unwrapped);
  }

  return true;
}

void WrappedID3D11DeviceContext::Begin(ID3D11Asynchronous *pAsync)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11Asynchronous *unwrapped = NULL;
  ResourceId id;

  if(WrappedID3D11Query1::IsAlloc(pAsync))
  {
    unwrapped = UNWRAP(WrappedID3D11Query1, pAsync);
    id = ((WrappedID3D11Query1 *)pAsync)->GetResourceID();
  }
  else if(WrappedID3D11Predicate::IsAlloc(pAsync))
  {
    unwrapped = UNWRAP(WrappedID3D11Predicate, pAsync);
    id = ((WrappedID3D11Predicate *)pAsync)->GetResourceID();
  }
  else if(WrappedID3D11Counter::IsAlloc(pAsync))
  {
    unwrapped = UNWRAP(WrappedID3D11Counter, pAsync);
    id = ((WrappedID3D11Counter *)pAsync)->GetResourceID();
  }
  else
  {
    RDCERR("Unexpected ID3D11Asynchronous");
  }

  SERIALISE_TIME_CALL(m_pRealContext->Begin(unwrapped));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::Begin);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_Begin(GET_SERIALISER, pAsync);

    m_ContextRecord->AddChunk(scope.Get());

    MarkResourceReferenced(id, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_End(SerialiserType &ser, ID3D11Asynchronous *pAsync)
{
  SERIALISE_ELEMENT(pAsync);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pAsync)
  {
    ID3D11Asynchronous *unwrapped = NULL;

    // only replay predicates which can affect rendering, don't re-submit queries or counters (that
    // might even interfere with queries we want to run)
    if(WrappedID3D11Predicate::IsAlloc(pAsync))
      unwrapped = UNWRAP(WrappedID3D11Predicate, pAsync);

    // if you change this to replay other types, check with Serialise_CreateCounter which creates
    // dummy queries to ensure it always succeeds.

    if(unwrapped)
      m_pRealContext->End(unwrapped);
  }

  return true;
}

void WrappedID3D11DeviceContext::End(ID3D11Asynchronous *pAsync)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11Asynchronous *unwrapped = NULL;
  ResourceId id;

  if(WrappedID3D11Query1::IsAlloc(pAsync))
  {
    unwrapped = UNWRAP(WrappedID3D11Query1, pAsync);
    id = ((WrappedID3D11Query1 *)pAsync)->GetResourceID();
  }
  else if(WrappedID3D11Predicate::IsAlloc(pAsync))
  {
    unwrapped = UNWRAP(WrappedID3D11Predicate, pAsync);
    id = ((WrappedID3D11Predicate *)pAsync)->GetResourceID();
  }
  else if(WrappedID3D11Counter::IsAlloc(pAsync))
  {
    unwrapped = UNWRAP(WrappedID3D11Counter, pAsync);
    id = ((WrappedID3D11Counter *)pAsync)->GetResourceID();
  }
  else
  {
    RDCERR("Unexpected ID3D11Asynchronous");
  }

  SERIALISE_TIME_CALL(m_pRealContext->End(unwrapped));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::End);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_End(GET_SERIALISER, pAsync);

    m_ContextRecord->AddChunk(scope.Get());

    MarkResourceReferenced(id, eFrameRef_Read);
  }
}

HRESULT WrappedID3D11DeviceContext::GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize,
                                            UINT GetDataFlags)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  ID3D11Asynchronous *unwrapped = NULL;

  if(WrappedID3D11Query1::IsAlloc(pAsync))
    unwrapped = UNWRAP(WrappedID3D11Query1, pAsync);
  else if(WrappedID3D11Predicate::IsAlloc(pAsync))
    unwrapped = UNWRAP(WrappedID3D11Predicate, pAsync);
  else if(WrappedID3D11Counter::IsAlloc(pAsync))
    unwrapped = UNWRAP(WrappedID3D11Counter, pAsync);
  else
    RDCERR("Unexpected ID3D11Asynchronous");

  return m_pRealContext->GetData(unwrapped, pData, DataSize, GetDataFlags);
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_SetPredication(SerialiserType &ser,
                                                          ID3D11Predicate *pPredicate,
                                                          BOOL PredicateValue_)
{
  SERIALISE_ELEMENT(pPredicate);
  SERIALISE_ELEMENT_LOCAL(PredicateValue, PredicateValue_ == TRUE);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // we don't replay predication as it can be confusing and inconsistent. We just store the state
    // so that we can manually check whether it *would* have passed or failed.
    m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->Predicate, pPredicate);
    m_CurrentPipelineState->Change(m_CurrentPipelineState->PredicateValue,
                                   PredicateValue ? TRUE : FALSE);

    /*
    m_pRealContext->SetPredication(UNWRAP(WrappedID3D11Predicate, pPredicate),
                                   PredicateValue ? TRUE : FALSE);
                                   */
  }

  return true;
}

void WrappedID3D11DeviceContext::SetPredication(ID3D11Predicate *pPredicate, BOOL PredicateValue)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_EmptyCommandList = false;

  m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->Predicate, pPredicate);
  m_CurrentPipelineState->Change(m_CurrentPipelineState->PredicateValue, PredicateValue);

  // on replay don't actually apply any predication. Just update the state and bail
  if(IsReplayMode(m_State))
    return;

  SERIALISE_TIME_CALL(
      m_pRealContext->SetPredication(UNWRAP(WrappedID3D11Predicate, pPredicate), PredicateValue));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::SetPredication);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_SetPredication(GET_SERIALISER, pPredicate, PredicateValue);

    m_ContextRecord->AddChunk(scope.Get());

    if(pPredicate)
    {
      ResourceId id = ((WrappedID3D11Predicate *)pPredicate)->GetResourceID();
      MarkResourceReferenced(id, eFrameRef_Read);
    }
  }
}

FLOAT WrappedID3D11DeviceContext::GetResourceMinLOD(ID3D11Resource *pResource)
{
  return m_pRealContext->GetResourceMinLOD(m_pDevice->GetResourceManager()->UnwrapResource(pResource));
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_SetResourceMinLOD(SerialiserType &ser,
                                                             ID3D11Resource *pResource, FLOAT MinLOD)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT(MinLOD);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(pResource)
      m_pRealContext->SetResourceMinLOD(m_pDevice->GetResourceManager()->UnwrapResource(pResource),
                                        MinLOD);
  }

  return true;
}

void WrappedID3D11DeviceContext::SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  m_EmptyCommandList = false;

  SERIALISE_TIME_CALL(m_pRealContext->SetResourceMinLOD(
      m_pDevice->GetResourceManager()->UnwrapResource(pResource), MinLOD));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::SetResourceMinLOD);
    SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
    Serialise_SetResourceMinLOD(GET_SERIALISER, pResource, MinLOD);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedID3D11DeviceContext::GetPredication(ID3D11Predicate **ppPredicate, BOOL *pPredicateValue)
{
  ID3D11Predicate *real = NULL;
  m_pRealContext->GetPredication(&real, pPredicateValue);
  SAFE_RELEASE_NOCLEAR(real);

  if(ppPredicate)
  {
    if(real)
      *ppPredicate = UNWRAP(WrappedID3D11Predicate, real);
    else
      *ppPredicate = NULL;
  }
}

D3D11_DEVICE_CONTEXT_TYPE WrappedID3D11DeviceContext::GetType()
{
  return m_pRealContext->GetType();
}

UINT WrappedID3D11DeviceContext::GetContextFlags()
{
  return m_pRealContext->GetContextFlags();
}

#pragma endregion Misc

#pragma region Map

void MapIntercept::SetAppMemory(void *appMemory)
{
  app.pData = appMemory;
}

void MapIntercept::SetD3D(D3D11_SUBRESOURCE_DATA d3dData)
{
  D3D11_MAPPED_SUBRESOURCE d3dMap;
  d3dMap.pData = (void *)d3dData.pSysMem;
  d3dMap.RowPitch = d3dData.SysMemPitch;
  d3dMap.DepthPitch = d3dData.SysMemSlicePitch;

  d3d = d3dMap;

  RDCASSERT(d3d.pData);
}

void MapIntercept::SetD3D(D3D11_MAPPED_SUBRESOURCE d3dMap)
{
  d3d = d3dMap;

  RDCASSERT(d3d.pData);
}

void MapIntercept::InitWrappedResource(ID3D11Resource *res, UINT sub, void *appMemory)
{
  if(WrappedID3D11Buffer::IsAlloc(res))
    Init((ID3D11Buffer *)res, appMemory);
  else if(WrappedID3D11Texture1D::IsAlloc(res))
    Init((ID3D11Texture1D *)res, sub, appMemory);
  else if(WrappedID3D11Texture2D1::IsAlloc(res))
    Init((ID3D11Texture2D *)res, sub, appMemory);
  else if(WrappedID3D11Texture3D1::IsAlloc(res))
    Init((ID3D11Texture3D *)res, sub, appMemory);
  else
    RDCERR("Unexpected resource type");
}

void MapIntercept::Init(ID3D11Buffer *buf, void *appMemory)
{
  app.pData = appMemory;

  if(buf == NULL)
    return;

  D3D11_BUFFER_DESC desc;
  buf->GetDesc(&desc);

  app.RowPitch = app.DepthPitch = desc.ByteWidth;

  if(d3d.RowPitch == 0)
    d3d.RowPitch = desc.ByteWidth;
  if(d3d.DepthPitch == 0)
    d3d.DepthPitch = desc.ByteWidth;
}

void MapIntercept::Init(ID3D11Texture1D *tex, UINT sub, void *appMemory)
{
  app.pData = appMemory;

  if(tex == NULL)
    return;

  D3D11_TEXTURE1D_DESC desc;
  tex->GetDesc(&desc);

  int width = desc.Width;
  int height = 1;
  DXGI_FORMAT fmt = desc.Format;

  int mip = GetMipForSubresource(tex, sub);

  // a row in block formats is a row of 4x4 blocks.
  if(IsBlockFormat(fmt))
    numRows /= 4;

  numRows = RDCMAX(1, numRows >> mip);
  numSlices = RDCMAX(1, numSlices >> mip);

  if(IsYUVPlanarFormat(fmt))
    numRows = GetYUVNumRows(fmt, numRows);

  app.RowPitch = GetRowPitch(width, fmt, mip);
  app.DepthPitch = GetByteSize(width, height, 1, fmt, mip);

  if(d3d.DepthPitch == 0)
    d3d.DepthPitch = app.RowPitch;
  if(d3d.DepthPitch == 0)
    d3d.DepthPitch = app.DepthPitch;
}

void MapIntercept::Init(ID3D11Texture2D *tex, UINT sub, void *appMemory)
{
  app.pData = appMemory;

  if(tex == NULL)
    return;

  D3D11_TEXTURE2D_DESC desc;
  tex->GetDesc(&desc);

  int width = desc.Width;
  int height = numRows = desc.Height;
  DXGI_FORMAT fmt = desc.Format;

  int mip = GetMipForSubresource(tex, sub);

  // a row in block formats is a row of 4x4 blocks.
  if(IsBlockFormat(fmt))
    numRows /= 4;

  numRows = RDCMAX(1, numRows >> mip);
  numSlices = RDCMAX(1, numSlices >> mip);

  if(IsYUVPlanarFormat(fmt))
    numRows = GetYUVNumRows(fmt, numRows);

  app.RowPitch = GetRowPitch(width, fmt, mip);
  app.DepthPitch = GetByteSize(width, height, 1, fmt, mip);

  if(d3d.DepthPitch == 0)
    d3d.DepthPitch = app.DepthPitch;
}

void MapIntercept::Init(ID3D11Texture3D *tex, UINT sub, void *appMemory)
{
  app.pData = appMemory;

  if(tex == NULL)
    return;

  D3D11_TEXTURE3D_DESC desc;
  tex->GetDesc(&desc);

  int width = desc.Width;
  int height = numRows = desc.Height;
  numSlices = desc.Depth;
  DXGI_FORMAT fmt = desc.Format;

  int mip = GetMipForSubresource(tex, sub);

  // a row in block formats is a row of 4x4 blocks.
  if(IsBlockFormat(fmt))
    numRows /= 4;

  numRows = RDCMAX(1, numRows >> mip);
  numSlices = RDCMAX(1, numSlices >> mip);

  if(IsYUVPlanarFormat(fmt))
    numRows = GetYUVNumRows(fmt, numRows);

  app.RowPitch = GetRowPitch(width, fmt, mip);
  app.DepthPitch = GetByteSize(width, height, 1, fmt, mip);
}

void MapIntercept::CopyFromD3D()
{
  byte *sliceSrc = (byte *)d3d.pData;
  byte *sliceDst = (byte *)app.pData;

  RDCASSERT(numSlices > 0 && numRows > 0 && (numRows == 1 || (app.RowPitch > 0 && d3d.RowPitch > 0)) &&
            (numSlices == 1 || (app.DepthPitch > 0 && d3d.DepthPitch > 0)));

  for(int slice = 0; slice < numSlices; slice++)
  {
    byte *rowSrc = sliceSrc;
    byte *rowDst = sliceDst;

    for(int row = 0; row < numRows; row++)
    {
      memcpy(rowDst, rowSrc, app.RowPitch);

      rowSrc += d3d.RowPitch;
      rowDst += app.RowPitch;
    }

    sliceSrc += d3d.DepthPitch;
    sliceDst += app.DepthPitch;
  }
}

void MapIntercept::CopyToD3D(size_t RangeStart, size_t RangeEnd)
{
  byte *sliceSrc = (byte *)app.pData;
  byte *sliceDst = (byte *)d3d.pData + RangeStart;

  RDCASSERT(numSlices > 0 && numRows > 0 && app.RowPitch > 0 && d3d.RowPitch > 0 &&
            app.DepthPitch > 0 && d3d.DepthPitch > 0);

  for(int slice = 0; slice < numSlices; slice++)
  {
    byte *rowSrc = sliceSrc;
    byte *rowDst = sliceDst;

    for(int row = 0; row < numRows; row++)
    {
      size_t len = app.RowPitch;

      if(RangeEnd > 0)
      {
        if(rowSrc + len > (byte *)app.pData + (RangeEnd - RangeStart))
          len = (byte *)app.pData + (RangeEnd - RangeStart) - rowSrc;
      }

      memcpy(rowDst, rowSrc, len);

      rowSrc += app.RowPitch;
      rowDst += d3d.RowPitch;

      if(RangeEnd > 0 && rowSrc > (byte *)app.pData + (RangeEnd - RangeStart))
        return;
    }

    sliceSrc += app.DepthPitch;
    sliceDst += d3d.DepthPitch;
  }
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_Map(SerialiserType &ser, ID3D11Resource *pResource,
                                               UINT Subresource, D3D11_MAP MapType, UINT MapFlags,
                                               D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
  // unused - just for the user's benefit
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT(Subresource);
  SERIALISE_ELEMENT(MapType);
  SERIALISE_ELEMENT_TYPED(D3D11_MAP_FLAG, MapFlags);

  SERIALISE_CHECK_READ_ERRORS();

  // nothing to do on replay, all the work happens in Unmap.
  if(ser.IsReading())
    return true;

  D3D11_MAPPED_SUBRESOURCE mappedResource = D3D11_MAPPED_SUBRESOURCE();

  if(pMappedResource)
    mappedResource = *pMappedResource;

  D3D11ResourceRecord *record =
      m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));

  RDCASSERT(record);

  if(record->NumSubResources > (int)Subresource)
    record = (D3D11ResourceRecord *)record->SubResources[Subresource];

  MapIntercept intercept;

  size_t mapLength = (size_t)record->Length;

  if(IsActiveCapturing(m_State) || !record->DataInSerialiser)
  {
    ResourceId Resource = GetIDForResource(pResource);

    RDCASSERT(m_OpenMaps.find(MappedResource(Resource, Subresource)) == m_OpenMaps.end());

    ID3D11Resource *resMap = pResource;

    RDCASSERT(resMap);

    size_t ctxMapID = 0;

    if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
    {
      if(m_MapResourceRecordAllocs[Resource] == 0)
        m_MapResourceRecordAllocs[Resource] = record->GetContextID();

      ctxMapID = m_MapResourceRecordAllocs[Resource];

      RDCASSERT(ctxMapID != 0);
    }

    void *appMem = record->GetShadowPtr(ctxMapID, 0);

    if(appMem == NULL)
    {
      record->AllocShadowStorage(ctxMapID, mapLength);
      appMem = record->GetShadowPtr(ctxMapID, 0);

      if(MapType != D3D11_MAP_WRITE_DISCARD)
      {
        if(m_pDevice->GetResourceManager()->IsResourceDirty(Resource))
        {
          ID3D11DeviceChild *initial =
              m_pDevice->GetResourceManager()->GetInitialContents(Resource).resource;

          if(WrappedID3D11Buffer::IsAlloc(pResource))
          {
            RDCASSERT(initial);

            ID3D11Buffer *stage = (ID3D11Buffer *)initial;

            D3D11_MAPPED_SUBRESOURCE mapped;
            HRESULT hr = m_pRealContext->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);

            if(FAILED(hr))
            {
              RDCERR("Failed to map while getting initial states HRESULT: %s", ToStr(hr).c_str());
            }
            else
            {
              intercept = MapIntercept();
              intercept.SetD3D(mapped);
              intercept.Init((ID3D11Buffer *)pResource, record->GetDataPtr());
              intercept.CopyFromD3D();

              RDCASSERT(mapLength == (size_t)record->Length);

              memcpy(appMem, record->GetDataPtr(), mapLength);

              m_pRealContext->Unmap(stage, 0);
            }
          }
          else
          {
            // need to get initial contents out
            RDCUNIMPLEMENTED("Not getting initial contents for non-buffer GPU dirty map");
            RDCERR(
                "CORRUPTION - Invalid/inaccurate initial data for Map() - non-buffer GPU dirty "
                "data mapped");
          }
        }
        else if(record->DataInSerialiser)
        {
          RDCASSERT(mapLength == (size_t)record->Length);
          memcpy(appMem, record->GetDataPtr(), mapLength);
        }
        else
        {
          memset(appMem, 0, mapLength);
        }
      }

      memcpy(record->GetShadowPtr(ctxMapID, 1), appMem, mapLength);
    }

    if(MapType == D3D11_MAP_WRITE_DISCARD)
    {
      if(RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess)
        memset(appMem, 0xcc, mapLength);
      memcpy(record->GetShadowPtr(ctxMapID, 1), appMem, mapLength);
    }

    intercept = MapIntercept();
    intercept.verifyWrite = RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess;
    intercept.SetD3D(mappedResource);
    intercept.InitWrappedResource(resMap, Subresource, appMem);
    intercept.MapType = MapType;
    intercept.MapFlags = MapFlags;

    RDCASSERT(pMappedResource);
    *pMappedResource = intercept.app;

    m_OpenMaps[MappedResource(Resource, Subresource)] = intercept;
  }
  else if(IsBackgroundCapturing(m_State))
  {
    RDCASSERT(record->DataInSerialiser);

    mapLength = (size_t)record->Length;

    intercept = MapIntercept();
    intercept.verifyWrite = RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess;
    intercept.SetD3D(mappedResource);
    intercept.MapType = MapType;
    intercept.MapFlags = MapFlags;

    if(intercept.verifyWrite)
    {
      size_t ctxMapID = 0;

      ResourceId Resource = GetIDForResource(pResource);

      if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
      {
        if(m_MapResourceRecordAllocs[Resource] == 0)
          m_MapResourceRecordAllocs[Resource] = record->GetContextID();

        ctxMapID = m_MapResourceRecordAllocs[Resource];

        RDCASSERT(ctxMapID != 0);
      }

      byte *appMem = record->GetShadowPtr(ctxMapID, 0);

      if(appMem == NULL)
      {
        record->AllocShadowStorage(ctxMapID, mapLength);
        appMem = record->GetShadowPtr(ctxMapID, 0);
      }

      memcpy(appMem, record->GetDataPtr(), mapLength);

      intercept.InitWrappedResource(pResource, Subresource, appMem);
    }
    else
    {
      intercept.InitWrappedResource(pResource, Subresource, record->GetDataPtr());
    }

    *pMappedResource = intercept.app;

    m_OpenMaps[MappedResource(GetIDForResource(pResource), Subresource)] = intercept;
  }
  else
  {
    RDCERR("Unexpected and unhandled case");
    RDCEraseEl(intercept);
  }

  // for read write fill out the buffer with what's on the mapped resource already
  if(MapType == D3D11_MAP_READ_WRITE || MapType == D3D11_MAP_READ)
  {
    intercept.CopyFromD3D();
  }
  else if(MapType == D3D11_MAP_WRITE_DISCARD)
  {
    // the easy case!
  }
  else if(MapType == D3D11_MAP_WRITE || MapType == D3D11_MAP_WRITE_NO_OVERWRITE)
  {
    // For now we'll just assume that the buffer contents are perfectly accurate
    // (which they are if no gpu writes to the buffer happens).

    // could take the performance hit and just copy anyway, spec doesn't see if the
    // data will be invalid but it will certainly be slow.
  }

  return true;
}

HRESULT WrappedID3D11DeviceContext::Map(ID3D11Resource *pResource, UINT Subresource,
                                        D3D11_MAP MapType, UINT MapFlags,
                                        D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ResourceId id = GetIDForResource(pResource);

  bool directMap = false;
  if(m_HighTrafficResources.find(id) != m_HighTrafficResources.end() && !IsActiveCapturing(m_State))
    directMap = true;

  if(m_pDevice->GetResourceManager()->IsResourceDirty(GetIDForResource(pResource)) &&
     !IsActiveCapturing(m_State))
    directMap = true;

  if((!directMap && MapType == D3D11_MAP_WRITE_NO_OVERWRITE && !IsActiveCapturing(m_State)) ||
     m_pRealContext->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
  {
    directMap = true;
    m_HighTrafficResources.insert(id);
    if(!IsActiveCapturing(m_State))
      MarkDirtyResource(id);
  }

  if(directMap && IsBackgroundCapturing(m_State))
  {
    return m_pRealContext->Map(m_pDevice->GetResourceManager()->UnwrapResource(pResource),
                               Subresource, MapType, MapFlags, pMappedResource);
  }

  // can't promise no-overwrite as we're going to blat the whole buffer!
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pRealContext->Map(
                          m_pDevice->GetResourceManager()->UnwrapResource(pResource), Subresource,
                          MapType == D3D11_MAP_WRITE_NO_OVERWRITE ? D3D11_MAP_WRITE_DISCARD : MapType,
                          MapFlags, pMappedResource));

  if(SUCCEEDED(ret) && IsCaptureMode(m_State))
  {
    if(IsActiveCapturing(m_State))
    {
      if(MapType == D3D11_MAP_READ)
      {
        MapIntercept intercept;
        intercept.MapType = MapType;
        intercept.MapFlags = MapFlags;

        m_OpenMaps[MappedResource(GetIDForResource(pResource), Subresource)] = intercept;
      }
      else
      {
        GetResourceManager()->MarkDirtyResource(GetIDForResource(pResource));

        // create a chunk purely for the user's benefit
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::Map);
        SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
        Serialise_Map(GET_SERIALISER, pResource, Subresource, MapType, MapFlags, pMappedResource);

        m_ContextRecord->AddChunk(scope.Get());
      }
    }
    else    // IsIdleCapturing(m_State)
    {
      RDCASSERT(WrappedID3D11Buffer::IsAlloc(pResource) ||
                WrappedID3D11Texture1D::IsAlloc(pResource) ||
                WrappedID3D11Texture2D1::IsAlloc(pResource) ||
                WrappedID3D11Texture3D1::IsAlloc(pResource));

      ResourceId Id = GetIDForResource(pResource);

      D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(Id);
      RDCASSERT(record);

      if(record->NumSubResources > (int)Subresource)
        record = (D3D11ResourceRecord *)record->SubResources[Subresource];

      record->UpdateCount++;

      if(record->UpdateCount > 60 && !RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess)
      {
        m_HighTrafficResources.insert(Id);
        MarkDirtyResource(Id);

        return ret;
      }

      // don't need to create a chunk but we do need to prepare the map.
      Serialise_Map(m_ScratchSerialiser, pResource, Subresource, MapType, MapFlags, pMappedResource);

      // throw away any serialised data
      m_ScratchSerialiser.GetWriter()->Rewind();
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_Unmap(SerialiserType &ser, ID3D11Resource *pResource,
                                                 UINT Subresource)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT(Subresource);

  MappedResource mapIdx(GetIDForResource(pResource), Subresource);
  MapIntercept intercept;
  size_t ctxMapID = 0;
  D3D11ResourceRecord *record = NULL;

  size_t len = 0;
  uint32_t diffStart = 0;
  uint32_t diffEnd = 0;

  byte *MapWrittenData = NULL;

  // handle book-keeping during capture mode
  if(ser.IsWriting())
  {
    record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));
    RDCASSERT(record);

    if((int)Subresource < record->NumSubResources)
      record = (D3D11ResourceRecord *)record->SubResources[Subresource];

    len = (size_t)record->Length;
    diffEnd = (uint32_t)len;

    // locate the intercept data and remove it from the open maps list
    auto it = m_OpenMaps.find(mapIdx);

    if(it != m_OpenMaps.end())
    {
      intercept = it->second;
      m_OpenMaps.erase(it);
    }
    else
    {
      RDCERR("Couldn't find map for %llu/%u in open maps list", mapIdx.resource, mapIdx.subresource);
    }

    MapWrittenData = (byte *)intercept.app.pData;

    // store the context ID we used for this map's shadow pointers
    if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED &&
       (IsActiveCapturing(m_State) || intercept.verifyWrite))
    {
      ctxMapID = m_MapResourceRecordAllocs[mapIdx.resource];

      RDCASSERT(ctxMapID != 0);
    }

    // if we were verifying this map, then make sure the shadow storage is still safe
    if(intercept.verifyWrite)
    {
      if(!record->VerifyShadowStorage(ctxMapID))
      {
        std::string msg = StringFormat::Fmt(
            "Overwrite of %llu byte Map()'d buffer detected\n"
            "Breakpoint now to see callstack,\nor click 'Yes' to debugbreak.",
            record->Length);
        int res = tinyfd_messageBox("Map() overwrite detected!", msg.c_str(), "yesno", "error", 1);
        if(res == 1)
        {
          OS_DEBUG_BREAK();
        }
      }

      if(IsBackgroundCapturing(m_State))
      {
        // if there was already backing store then normally the user wrote directly into it so we
        // don't even have to update anything, except in the case where we were verifying map
        // writes. Then we need to copy from the verification shadow buffer into the backing store.
        memcpy(record->GetDataPtr(), intercept.app.pData, len);
      }
    }

    // copy from the intercept buffer that the user wrote into, into D3D's real pointer
    intercept.CopyToD3D();

    // while actively capturing, on large buffers being updated, try to locate the range of data
    // being
    // updated and update the diffStart/diffEnd/len variables
    if(IsActiveCapturing(m_State) && len > 512 && intercept.MapType != D3D11_MAP_WRITE_DISCARD)
    {
      size_t s = diffStart;
      size_t e = diffEnd;
      bool found = FindDiffRange(MapWrittenData, record->GetShadowPtr(ctxMapID, 1), len, s, e);
      diffStart = (uint32_t)s;
      diffEnd = (uint32_t)e;

      // structured buffers must have copies aligned to their structure width, so we align down and
      // up the detected diff start/end region to match.
      if(WrappedID3D11Buffer::IsAlloc(pResource))
      {
        D3D11_BUFFER_DESC bufdesc = {};
        ((WrappedID3D11Buffer *)pResource)->GetDesc(&bufdesc);

        if(bufdesc.StructureByteStride)
        {
          diffStart -= (diffStart % bufdesc.StructureByteStride);

          if((diffEnd % bufdesc.StructureByteStride) != 0)
            diffEnd += bufdesc.StructureByteStride - (diffEnd % bufdesc.StructureByteStride);
        }
      }

      if(found)
      {
#if ENABLED(RDOC_DEVEL)
        static size_t saved = 0;

        saved += len - (diffEnd - diffStart);

        RDCDEBUG("Mapped resource size %u, difference: %u -> %u. Total bytes saved so far: %u",
                 (uint32_t)len, (uint32_t)diffStart, (uint32_t)diffEnd, (uint32_t)saved);
#endif

        len = diffEnd - diffStart;
      }
      else
      {
        diffStart = 0;
        diffEnd = 0;

        len = 0;
      }

      // update the data pointer to be rebased to the start of the diff data.
      MapWrittenData += diffStart;
    }

    // update shadow stores for future diff'ing
    if(IsActiveCapturing(m_State) && record->GetShadowPtr(ctxMapID, 1))
    {
      memcpy(record->GetShadowPtr(ctxMapID, 1) + diffStart, MapWrittenData, diffEnd - diffStart);
    }
  }    // if(ser.IsWriting())

  // if we're not actively capturing a frame, we'll skip serialising all this data for nothing as we
  // aren't going to save the chunk anywhere. The exception is if there's no backing store for this
  // resource - then we need to serialise into the chunk to allocate it
  bool SerialiseMap = ser.IsReading() || (ser.IsWriting() && IsActiveCapturing(m_State)) ||
                      (ser.IsWriting() && !record->DataInSerialiser);

  if(SerialiseMap)
  {
    SERIALISE_ELEMENT(intercept.MapType).Named("MapType"_lit);
    SERIALISE_ELEMENT_TYPED(D3D11_MAP_FLAG, intercept.MapFlags).Named("MapFlags"_lit);

    SERIALISE_ELEMENT(diffStart).Named("Byte offset to start of written data"_lit);
    SERIALISE_ELEMENT(diffEnd).Named("Byte offset to end of written data"_lit);

    SERIALISE_ELEMENT_ARRAY(MapWrittenData, len);

    if(ser.IsWriting() && IsBackgroundCapturing(m_State) && !record->DataInSerialiser)
    {
      record->DataInSerialiser = true;
      record->SetDataOffset(ser.GetWriter()->GetOffset() - (uint64_t)len);
    }
    else if(IsReplayingAndReading() && pResource)
    {
      // steal the allocated buffer, and manually release
      intercept.app.pData = MapWrittenData;
      MapWrittenData = NULL;
    }
  }

  if(IsReplayingAndReading() && ser.IsErrored())
  {
    FreeAlignedBuffer(MapWrittenData);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    MapWrittenData = (byte *)intercept.app.pData;

    if(IsLoading(m_State) && m_CurEventID > 0 && (diffStart < diffEnd))
      RecordUpdateStats(pResource, diffEnd - diffStart, false);

    if(diffStart >= diffEnd)
    {
      // do nothing
    }
    else if(intercept.MapType == D3D11_MAP_WRITE_NO_OVERWRITE)
    {
      RDCASSERT(WrappedID3D11Buffer::IsAlloc(pResource));
      ID3D11Buffer *mapContents = NULL;

      D3D11_BUFFER_DESC bdesc;
      bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      bdesc.ByteWidth = diffEnd - diffStart;
      bdesc.CPUAccessFlags = 0;
      bdesc.MiscFlags = 0;
      bdesc.StructureByteStride = 0;
      bdesc.Usage = D3D11_USAGE_IMMUTABLE;

      D3D11_SUBRESOURCE_DATA data;
      data.pSysMem = MapWrittenData;
      data.SysMemPitch = bdesc.ByteWidth;
      data.SysMemSlicePitch = bdesc.ByteWidth;

      HRESULT hr = m_pDevice->GetReal()->CreateBuffer(&bdesc, &data, &mapContents);

      if(FAILED(hr))
      {
        RDCERR("Failed to create temp Unmap() buffer HRESULT: %s", ToStr(hr).c_str());
      }
      else
      {
        m_pRealContext->CopySubresourceRegion(GetResourceManager()->UnwrapResource(pResource),
                                              mapIdx.subresource, diffStart, 0, 0, mapContents, 0,
                                              NULL);

        SAFE_RELEASE(mapContents);
      }
    }
    else
    {
      D3D11_MAPPED_SUBRESOURCE mappedResource;

      UINT flags = intercept.MapFlags & ~D3D11_MAP_FLAG_DO_NOT_WAIT;

      HRESULT hr = m_pRealContext->Map(GetResourceManager()->UnwrapResource(pResource),
                                       mapIdx.subresource, intercept.MapType, flags, &mappedResource);

      RDCASSERT(mappedResource.pData);

      if(FAILED(hr))
      {
        RDCERR("Failed to map resource, HRESULT: %s", ToStr(hr).c_str());
      }
      else
      {
        intercept.SetD3D(mappedResource);
        intercept.InitWrappedResource(pResource, mapIdx.subresource, MapWrittenData);

        intercept.CopyToD3D(diffStart, diffEnd);

        m_pRealContext->Unmap(m_pDevice->GetResourceManager()->UnwrapResource(pResource),
                              mapIdx.subresource);
      }
    }

    FreeAlignedBuffer(MapWrittenData);
  }

  return true;
}

void WrappedID3D11DeviceContext::Unmap(ID3D11Resource *pResource, UINT Subresource)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  DrainAnnotationQueue();

  m_EmptyCommandList = false;

  ResourceId id = GetIDForResource(pResource);

  auto it = m_OpenMaps.find(MappedResource(id, Subresource));

  if(IsBackgroundCapturing(m_State) && m_HighTrafficResources.find(id) != m_HighTrafficResources.end())
  {
    // we intercepted this, even though we now don't need to serialise it. Time to finish what we
    // started!
    if(it != m_OpenMaps.end() && it->second.MapType != D3D11_MAP_READ)
    {
      it->second.CopyToD3D();

      D3D11ResourceRecord *record =
          m_pDevice->GetResourceManager()->GetResourceRecord(it->first.resource);
      if(record)
        record->FreeShadowStorage();

      m_OpenMaps.erase(it);
    }
    else if(it != m_OpenMaps.end())
    {
      m_OpenMaps.erase(it);
    }
  }
  else if(IsCaptureMode(m_State))
  {
    if(it == m_OpenMaps.end() && IsActiveCapturing(m_State))
    {
      RDCWARN(
          "Saw an Unmap that we didn't capture the corresponding Map for - this frame is "
          "unsuccessful");
      m_SuccessfulCapture = false;
      m_FailureReason = CaptureFailed_UncappedUnmap;
    }

    if(it != m_OpenMaps.end())
    {
      if(it->second.MapType == D3D11_MAP_READ)
      {
        m_OpenMaps.erase(it);
      }
      else if(IsActiveCapturing(m_State))
      {
        MarkResourceReferenced(it->first.resource, eFrameRef_Read);
        MarkResourceReferenced(it->first.resource, eFrameRef_PartialWrite);

        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::Unmap);
        SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
        Serialise_Unmap(GET_SERIALISER, pResource, Subresource);

        m_ContextRecord->AddChunk(scope.Get());
      }
      else    // IsIdleCapturing(m_State)
      {
        RDCASSERT(WrappedID3D11Buffer::IsAlloc(pResource) ||
                  WrappedID3D11Texture1D::IsAlloc(pResource) ||
                  WrappedID3D11Texture2D1::IsAlloc(pResource) ||
                  WrappedID3D11Texture3D1::IsAlloc(pResource));

        D3D11ResourceRecord *record =
            m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));
        RDCASSERT(record);

        D3D11ResourceRecord *baserecord = record;

        if(record->NumSubResources > (int)Subresource)
          record = (D3D11ResourceRecord *)record->SubResources[Subresource];

        if(record->DataInSerialiser)
        {
          Serialise_Unmap(m_ScratchSerialiser, pResource, Subresource);

          // throw away any serialised data
          m_ScratchSerialiser.GetWriter()->Rewind();
        }
        else
        {
          USE_SCRATCH_SERIALISER();
          SCOPED_SERIALISE_CHUNK(D3D11Chunk::Unmap);
          SERIALISE_ELEMENT(m_ResourceID).Named("Context"_lit).TypedAs("ID3D11DeviceContext *"_lit);
          Serialise_Unmap(GET_SERIALISER, pResource, Subresource);

          Chunk *chunk = scope.Get();

          baserecord->AddChunk(chunk);
          record->SetDataPtr(chunk->GetData());

          record->DataInSerialiser = true;
        }

        record->FreeShadowStorage();
      }
    }
  }

  m_pRealContext->Unmap(m_pDevice->GetResourceManager()->UnwrapResource(pResource), Subresource);
}

#pragma endregion Map

#undef IMPLEMENT_FUNCTION_SERIALISED
#define IMPLEMENT_FUNCTION_SERIALISED(ret, func, ...)                                       \
  template bool WrappedID3D11DeviceContext::CONCAT(Serialise_,                              \
                                                   func(ReadSerialiser &ser, __VA_ARGS__)); \
  template bool WrappedID3D11DeviceContext::CONCAT(Serialise_,                              \
                                                   func(WriteSerialiser &ser, __VA_ARGS__));

SERIALISED_ID3D11CONTEXT_FUNCTIONS();
SERIALISED_ID3D11CONTEXT_MARKER_FUNCTIONS();
