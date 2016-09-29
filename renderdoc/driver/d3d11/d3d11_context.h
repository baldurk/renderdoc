/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
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

#include <list>
#include <map>
#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "d3d11_common.h"
#include "d3d11_manager.h"

using std::map;
using std::list;

struct MapIntercept
{
  MapIntercept()
  {
    RDCEraseEl(app);
    RDCEraseEl(d3d);
    numRows = numSlices = 1;
    MapType = D3D11_MAP_WRITE_DISCARD;
    MapFlags = 0;
    verifyWrite = false;
  }

  void SetAppMemory(void *appMemory);
  void SetD3D(D3D11_MAPPED_SUBRESOURCE d3dMap);
  void SetD3D(D3D11_SUBRESOURCE_DATA d3dMap);

  void InitWrappedResource(ID3D11Resource *res, UINT sub, void *appMemory);

  void Init(ID3D11Buffer *buf, void *appMemory);
  void Init(ID3D11Texture1D *tex, UINT sub, void *appMemory);
  void Init(ID3D11Texture2D *tex, UINT sub, void *appMemory);
  void Init(ID3D11Texture3D *tex, UINT sub, void *appMemory);

  D3D11_MAPPED_SUBRESOURCE app, d3d;
  int numRows, numSlices;

  D3D11_MAP MapType;
  UINT MapFlags;

  bool verifyWrite;

  void CopyFromD3D();
  void CopyToD3D(size_t RangeStart = 0, size_t RangeEnd = 0);
};

class WrappedID3D11DeviceContext;

// ID3DUserDefinedAnnotation
class WrappedID3DUserDefinedAnnotation : public RefCounter, public ID3DUserDefinedAnnotation
{
public:
  WrappedID3DUserDefinedAnnotation() : RefCounter(NULL), m_Context(NULL) {}
  void SetContext(WrappedID3D11DeviceContext *ctx) { m_Context = ctx; }
  // doesn't need to soft-ref the device, for once!
  IMPLEMENT_IUNKNOWN_WITH_REFCOUNTER_CUSTOMQUERY;

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  virtual INT STDMETHODCALLTYPE BeginEvent(LPCWSTR Name);
  virtual INT STDMETHODCALLTYPE EndEvent();
  virtual void STDMETHODCALLTYPE SetMarker(LPCWSTR Name);
  virtual BOOL STDMETHODCALLTYPE GetStatus() { return TRUE; }
private:
  WrappedID3D11DeviceContext *m_Context;
};

enum CaptureFailReason
{
  CaptureSucceeded = 0,
  CaptureFailed_UncappedUnmap,
  CaptureFailed_UncappedCmdlist,
};

struct DrawcallTreeNode
{
  DrawcallTreeNode() {}
  explicit DrawcallTreeNode(const FetchDrawcall &d) : draw(d) {}
  FetchDrawcall draw;
  vector<DrawcallTreeNode> children;

  vector<FetchDrawcall> Bake()
  {
    vector<FetchDrawcall> ret;
    if(children.empty())
      return ret;

    ret.resize(children.size());
    for(size_t i = 0; i < children.size(); i++)
    {
      ret[i] = children[i].draw;
      ret[i].children = children[i].Bake();
    }

    return ret;
  }
};

template <typename T>
size_t BucketForRecordLinear(size_t value)
{
  RDCCOMPILE_ASSERT(T::BUCKET_TYPE == BUCKET_RECORD_TYPE_LINEAR,
                    "Incorrect bucket type for record query.");
  const size_t size = T::BUCKET_SIZE;
  const size_t count = T::BUCKET_COUNT;
  const size_t maximum = size * count;
  const size_t index = (value < maximum) ? (value / size) : (count - 1);
  return index;
}

template <typename T>
size_t BucketForRecordPow2(size_t value)
{
  RDCCOMPILE_ASSERT(T::BUCKET_TYPE == BUCKET_RECORD_TYPE_POW2,
                    "Incorrect bucket type for record query.");
  const size_t count = T::BUCKET_COUNT;
  RDCCOMPILE_ASSERT(count <= (sizeof(size_t) * 8),
                    "Unexpected correspondence between bucket size and sizeof(size_t)");
  const size_t maximum = (size_t)1 << count;
  const size_t index = (value < maximum) ? (size_t)(Log2Floor(value)) : (count - 1);
  return index;
}

class WrappedID3D11DeviceContext : public RefCounter, public ID3D11DeviceContext2
{
private:
  friend class WrappedID3D11DeviceContext;
  friend class WrappedID3DUserDefinedAnnotation;
  friend struct D3D11RenderState;

  struct MappedResource
  {
    MappedResource(ResourceId res = ResourceId(), UINT sub = 0) : resource(res), subresource(sub) {}
    ResourceId resource;
    UINT subresource;

    bool operator<(const MappedResource &o) const
    {
      if(resource != o.resource)
        return resource < o.resource;

      return subresource < o.subresource;
    }
  };

  set<ResourceId> m_DeferredDirty;

  set<ResourceId> m_HighTrafficResources;
  map<MappedResource, MapIntercept> m_OpenMaps;

  struct StreamOutData
  {
    StreamOutData() : query(NULL), running(false), numPrims(0) {}
    ID3D11Query *query;
    bool running;
    uint64_t numPrims;
  };

  map<ResourceId, StreamOutData> m_StreamOutCounters;

  map<ResourceId, vector<EventUsage> > m_ResourceUses;

  WrappedID3D11Device *m_pDevice;
  ID3D11DeviceContext *m_pRealContext;
  ID3D11DeviceContext1 *m_pRealContext1;
  bool m_SetCBuffer1;

  ID3D11DeviceContext2 *m_pRealContext2;
  ID3D11DeviceContext3 *m_pRealContext3;

  bool m_NeedUpdateSubWorkaround;

  map<ResourceId, int> m_MapResourceRecordAllocs;

  set<ResourceId> m_MissingTracks;

  ResourceId m_ResourceID;
  D3D11ResourceRecord *m_ContextRecord;

  bool m_OwnSerialiser;
  Serialiser *m_pSerialiser;
  LogState m_State;
  CaptureFailReason m_FailureReason;
  bool m_SuccessfulCapture;
  bool m_EmptyCommandList;

  bool m_PresentChunk;

  ResourceId m_FakeContext;

  bool m_DoStateVerify;
  D3D11RenderState *m_CurrentPipelineState;

  D3D11RenderState *m_DeferredSavedState;

  vector<FetchAPIEvent> m_CurEvents, m_Events;
  bool m_AddedDrawcall;

  WrappedID3DUserDefinedAnnotation m_UserAnnotation;
  int32_t m_MarkerIndentLevel;

  struct Annotation
  {
    enum
    {
      ANNOT_SETMARKER,
      ANNOT_BEGINEVENT,
      ANNOT_ENDEVENT
    } m_Type;
    uint32_t m_Col;
    std::wstring m_Name;
  };
  vector<Annotation> m_AnnotationQueue;
  Threading::CriticalSection m_AnnotLock;

  uint64_t m_CurChunkOffset;
  uint32_t m_CurEventID, m_CurDrawcallID;

  DrawcallTreeNode m_ParentDrawcall;
  map<ResourceId, DrawcallTreeNode> m_CmdLists;

  list<DrawcallTreeNode *> m_DrawcallStack;

  void FlattenLog();

  const char *GetChunkName(D3D11ChunkType idx);

  void Serialise_DebugMessages();

  void DrainAnnotationQueue();

  void AddUsage(const FetchDrawcall &d);

  void AddEvent(D3D11ChunkType type, string description);
  void AddDrawcall(const FetchDrawcall &d, bool hasEvents);

  void RecordIndexBindStats(ID3D11Buffer *Buffer);
  void RecordVertexBindStats(UINT NumBuffers, ID3D11Buffer *Buffers[]);
  void RecordLayoutBindStats(ID3D11InputLayout *Layout);
  void RecordConstantStats(ShaderStageType stage, UINT NumBuffers, ID3D11Buffer *Buffers[]);
  void RecordResourceStats(ShaderStageType stage, UINT NumResources,
                           ID3D11ShaderResourceView *Resources[]);
  void RecordSamplerStats(ShaderStageType stage, UINT NumSamplers, ID3D11SamplerState *Samplers[]);
  void RecordUpdateStats(ID3D11Resource *res, uint32_t Size, bool Server);
  void RecordDrawStats(bool instanced, bool indirect, UINT InstanceCount);
  void RecordDispatchStats(bool indirect);
  void RecordShaderStats(ShaderStageType stage, ID3D11DeviceChild *Current,
                         ID3D11DeviceChild *Shader);
  void RecordBlendStats(ID3D11BlendState *Blend, FLOAT BlendFactor[4], UINT SampleMask);
  void RecordDepthStencilStats(ID3D11DepthStencilState *DepthStencil, UINT StencilRef);
  void RecordRasterizationStats(ID3D11RasterizerState *Rasterizer);
  void RecordViewportStats(UINT NumViewports, const D3D11_VIEWPORT *viewports);
  void RecordScissorStats(UINT NumRects, const D3D11_RECT *rects);
  void RecordOutputMergerStats(UINT NumRTVs, ID3D11RenderTargetView *RTVs[],
                               ID3D11DepthStencilView *DSV, UINT UAVStartSlot, UINT NumUAVs,
                               ID3D11UnorderedAccessView *UAVs[]);

  ////////////////////////////////////////////////////////////////
  // implement InterceptorSystem privately, since it is not thread safe (like all other context
  // functions)
  IMPLEMENT_FUNCTION_SERIALISED(void, SetMarker(uint32_t col, const wchar_t *name));
  IMPLEMENT_FUNCTION_SERIALISED(int, PushEvent(uint32_t col, const wchar_t *name));
  IMPLEMENT_FUNCTION_SERIALISED(int, PopEvent());

public:
  static const int AllocPoolCount = 2048;
  static const int AllocPoolMaxByteSize = 3 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11DeviceContext, AllocPoolCount, AllocPoolMaxByteSize);

  WrappedID3D11DeviceContext(WrappedID3D11Device *realDevice, ID3D11DeviceContext *context,
                             Serialiser *ser);
  void SetSerialiser(Serialiser *ser) { m_pSerialiser = ser; }
  virtual ~WrappedID3D11DeviceContext();

  void VerifyState();

  void BeginFrame();
  void EndFrame();

  bool Serialise_BeginCaptureFrame(bool applyInitialState);
  void BeginCaptureFrame();
  void EndCaptureFrame();

  void MarkDirtyResource(ResourceId id);

  // insert a fake chunk just to store these parameters
  void Present(UINT SyncInterval, UINT Flags);

  void CleanupCapture();
  void FreeCaptureData();

  bool HasSuccessfulCapture(CaptureFailReason &reason)
  {
    reason = m_FailureReason;
    return m_SuccessfulCapture && m_ContextRecord->NumChunks() > 0;
  }

  void AttemptCapture();
  void FinishCapture();

  D3D11RenderState *GetCurrentPipelineState() { return m_CurrentPipelineState; }
  Serialiser *GetSerialiser() { return m_pSerialiser; }
  ResourceId GetResourceID() { return m_ResourceID; }
  ID3D11DeviceContext *GetReal() { return m_pRealContext; }
  bool IsFL11_1();

  void ProcessChunk(uint64_t offset, D3D11ChunkType chunk, bool forceExecute);
  void ReplayFakeContext(ResourceId id);
  void ReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial);

  void MarkResourceReferenced(ResourceId id, FrameRefType refType);

  vector<EventUsage> GetUsage(ResourceId id) { return m_ResourceUses[id]; }
  void ClearMaps();

  uint32_t GetEventID() { return m_CurEventID; }
  FetchAPIEvent GetEvent(uint32_t eventID);

  const DrawcallTreeNode &GetRootDraw() { return m_ParentDrawcall; }
  void ThreadSafe_SetMarker(uint32_t col, const wchar_t *name);
  int ThreadSafe_BeginEvent(uint32_t col, const wchar_t *name);
  int ThreadSafe_EndEvent();

  //////////////////////////////
  // implement IUnknown
  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter::SoftRef(m_pDevice); }
  ULONG STDMETHODCALLTYPE Release() { return RefCounter::SoftRelease(m_pDevice); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement IDXGIDeviceChild

  virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
  {
    return m_pRealContext->SetPrivateData(Name, DataSize, pData);
  }

  virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
  {
    return m_pRealContext->SetPrivateDataInterface(Name, pUnknown);
  }

  virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
  {
    return m_pRealContext->GetPrivateData(Name, pDataSize, pData);
  }

  virtual void STDMETHODCALLTYPE GetDevice(ID3D11Device **ppDevice);

  //////////////////////////////
  // implement ID3D11DeviceContext

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer *const *ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      PSSetShaderResources(UINT StartSlot, UINT NumViews,
                           ID3D11ShaderResourceView *const *ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                PSSetShader(ID3D11PixelShader *pPixelShader,
                                            ID3D11ClassInstance *const *ppClassInstances,
                                            UINT NumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                PSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState *const *ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                VSSetShader(ID3D11VertexShader *pVertexShader,
                                            ID3D11ClassInstance *const *ppClassInstances,
                                            UINT NumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DrawIndexed(UINT IndexCount, UINT StartIndexLocation,
                                            INT BaseVertexLocation));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                Draw(UINT VertexCount, UINT StartVertexLocation));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType,
                                    UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                Unmap(ID3D11Resource *pResource, UINT Subresource));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer *const *ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IASetInputLayout(ID3D11InputLayout *pInputLayout));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IASetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                                                   ID3D11Buffer *const *ppVertexBuffers,
                                                   const UINT *pStrides, const UINT *pOffsets));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format,
                                                 UINT Offset));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
                                                     UINT StartIndexLocation, INT BaseVertexLocation,
                                                     UINT StartInstanceLocation));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
                                              UINT StartVertexLocation, UINT StartInstanceLocation));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer *const *ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GSSetShader(ID3D11GeometryShader *pShader,
                                            ID3D11ClassInstance *const *ppClassInstances,
                                            UINT NumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      VSSetShaderResources(UINT StartSlot, UINT NumViews,
                           ID3D11ShaderResourceView *const *ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                VSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState *const *ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, Begin(ID3D11Asynchronous *pAsync));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, End(ID3D11Asynchronous *pAsync));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize,
                                        UINT GetDataFlags));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetPredication(ID3D11Predicate *pPredicate, BOOL PredicateValue));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      GSSetShaderResources(UINT StartSlot, UINT NumViews,
                           ID3D11ShaderResourceView *const *ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState *const *ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                OMSetRenderTargets(UINT NumViews,
                                                   ID3D11RenderTargetView *const *ppRenderTargetViews,
                                                   ID3D11DepthStencilView *pDepthStencilView));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                OMSetRenderTargetsAndUnorderedAccessViews(
                                    UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews,
                                    ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot,
                                    UINT NumUAVs,
                                    ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
                                    const UINT *pUAVInitialCounts));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                OMSetBlendState(ID3D11BlendState *pBlendState,
                                                const FLOAT BlendFactor[4], UINT SampleMask));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState,
                                                       UINT StencilRef));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SOSetTargets(UINT NumBuffers, ID3D11Buffer *const *ppSOTargets,
                                             const UINT *pOffsets));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DrawAuto(void));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs,
                                                             UINT AlignedByteOffsetForArgs));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs,
                                                      UINT AlignedByteOffsetForArgs));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                                         UINT ThreadGroupCountZ));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DispatchIndirect(ID3D11Buffer *pBufferForArgs,
                                                 UINT AlignedByteOffsetForArgs));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                RSSetState(ID3D11RasterizerState *pRasterizerState));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT *pViewports));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CopySubresourceRegion(ID3D11Resource *pDstResource,
                                                      UINT DstSubresource, UINT DstX, UINT DstY,
                                                      UINT DstZ, ID3D11Resource *pSrcResource,
                                                      UINT SrcSubresource, const D3D11_BOX *pSrcBox));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CopyResource(ID3D11Resource *pDstResource,
                                             ID3D11Resource *pSrcResource));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                UpdateSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                                                  const D3D11_BOX *pDstBox, const void *pSrcData,
                                                  UINT SrcRowPitch, UINT SrcDepthPitch));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset,
                                                   ID3D11UnorderedAccessView *pSrcView));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView,
                                                      const FLOAT ColorRGBA[4]));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView,
                                   const UINT Values[4]));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView,
                                    const FLOAT Values[4]));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView,
                                                      UINT ClearFlags, FLOAT Depth, UINT8 Stencil));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GenerateMips(ID3D11ShaderResourceView *pShaderResourceView));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD));

  IMPLEMENT_FUNCTION_SERIALISED(virtual FLOAT STDMETHODCALLTYPE,
                                GetResourceMinLOD(ID3D11Resource *pResource));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ResolveSubresource(ID3D11Resource *pDstResource,
                                                   UINT DstSubresource, ID3D11Resource *pSrcResource,
                                                   UINT SrcSubresource, DXGI_FORMAT Format));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ExecuteCommandList(ID3D11CommandList *pCommandList,
                                                   BOOL RestoreContextState));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      HSSetShaderResources(UINT StartSlot, UINT NumViews,
                           ID3D11ShaderResourceView *const *ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                HSSetShader(ID3D11HullShader *pHullShader,
                                            ID3D11ClassInstance *const *ppClassInstances,
                                            UINT NumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                HSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState *const *ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer *const *ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      DSSetShaderResources(UINT StartSlot, UINT NumViews,
                           ID3D11ShaderResourceView *const *ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DSSetShader(ID3D11DomainShader *pDomainShader,
                                            ID3D11ClassInstance *const *ppClassInstances,
                                            UINT NumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState *const *ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer *const *ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CSSetShaderResources(UINT StartSlot, UINT NumViews,
                           ID3D11ShaderResourceView *const *ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs,
                                ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
                                const UINT *pUAVInitialCounts));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CSSetShader(ID3D11ComputeShader *pComputeShader,
                                            ID3D11ClassInstance *const *ppClassInstances,
                                            UINT NumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState *const *ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer *const *ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer **ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                PSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                     ID3D11ShaderResourceView **ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                PSGetShader(ID3D11PixelShader **ppPixelShader,
                                            ID3D11ClassInstance **ppClassInstances,
                                            UINT *pNumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                PSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState **ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                VSGetShader(ID3D11VertexShader **ppVertexShader,
                                            ID3D11ClassInstance **ppClassInstances,
                                            UINT *pNumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer **ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IAGetInputLayout(ID3D11InputLayout **ppInputLayout));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                                                   ID3D11Buffer **ppVertexBuffers, UINT *pStrides,
                                                   UINT *pOffsets));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format,
                                                 UINT *Offset));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer **ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GSGetShader(ID3D11GeometryShader **ppGeometryShader,
                                            ID3D11ClassInstance **ppClassInstances,
                                            UINT *pNumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                VSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                     ID3D11ShaderResourceView **ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                VSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState **ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GetPredication(ID3D11Predicate **ppPredicate, BOOL *pPredicateValue));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                     ID3D11ShaderResourceView **ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState **ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                OMGetRenderTargets(UINT NumViews,
                                                   ID3D11RenderTargetView **ppRenderTargetViews,
                                                   ID3D11DepthStencilView **ppDepthStencilView));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                OMGetRenderTargetsAndUnorderedAccessViews(
                                    UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews,
                                    ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot,
                                    UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                OMGetBlendState(ID3D11BlendState **ppBlendState,
                                                FLOAT BlendFactor[4], UINT *pSampleMask));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                OMGetDepthStencilState(ID3D11DepthStencilState **ppDepthStencilState,
                                                       UINT *pStencilRef));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SOGetTargets(UINT NumBuffers, ID3D11Buffer **ppSOTargets));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                RSGetState(ID3D11RasterizerState **ppRasterizerState));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                RSGetViewports(UINT *pNumViewports, D3D11_VIEWPORT *pViewports));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                RSGetScissorRects(UINT *pNumRects, D3D11_RECT *pRects));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                HSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                     ID3D11ShaderResourceView **ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                HSGetShader(ID3D11HullShader **ppHullShader,
                                            ID3D11ClassInstance **ppClassInstances,
                                            UINT *pNumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                HSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState **ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer **ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                     ID3D11ShaderResourceView **ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DSGetShader(ID3D11DomainShader **ppDomainShader,
                                            ID3D11ClassInstance **ppClassInstances,
                                            UINT *pNumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState **ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer **ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CSGetShaderResources(UINT StartSlot, UINT NumViews,
                                                     ID3D11ShaderResourceView **ppShaderResourceViews));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs,
                                ID3D11UnorderedAccessView **ppUnorderedAccessViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CSGetShader(ID3D11ComputeShader **ppComputeShader,
                                            ID3D11ClassInstance **ppClassInstances,
                                            UINT *pNumClassInstances));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                              ID3D11SamplerState **ppSamplers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                                                     ID3D11Buffer **ppConstantBuffers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearState(void));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, Flush(void));

  IMPLEMENT_FUNCTION_SERIALISED(virtual D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE, GetType(void));

  IMPLEMENT_FUNCTION_SERIALISED(virtual UINT STDMETHODCALLTYPE, GetContextFlags(void));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                FinishCommandList(BOOL RestoreDeferredContextState,
                                                  ID3D11CommandList **ppCommandList));

  //////////////////////////////
  // implement ID3D11DeviceContext1

  // outside the define as it doesn't depend on any 11_1 definitions, and it's just an unused
  // virtual function. We re-use the Serialise_UpdateSubresource1 function for
  // Serialise_UpdateSubresource
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                UpdateSubresource1(ID3D11Resource *pDstResource,
                                                   UINT DstSubresource, const D3D11_BOX *pDstBox,
                                                   const void *pSrcData, UINT SrcRowPitch,
                                                   UINT SrcDepthPitch, UINT CopyFlags));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CopySubresourceRegion1(ID3D11Resource *pDstResource,
                                                       UINT DstSubresource, UINT DstX, UINT DstY,
                                                       UINT DstZ, ID3D11Resource *pSrcResource,
                                                       UINT SrcSubresource,
                                                       const D3D11_BOX *pSrcBox, UINT CopyFlags));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DiscardResource(ID3D11Resource *pResource));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DiscardView(ID3D11View *pResourceView));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers,
                                                      const UINT *pFirstConstant,
                                                      const UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers,
                                                      const UINT *pFirstConstant,
                                                      const UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers,
                                                      const UINT *pFirstConstant,
                                                      const UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers,
                                                      const UINT *pFirstConstant,
                                                      const UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers,
                                                      const UINT *pFirstConstant,
                                                      const UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer *const *ppConstantBuffers,
                                                      const UINT *pFirstConstant,
                                                      const UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers,
                                                      UINT *pFirstConstant, UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers,
                                                      UINT *pFirstConstant, UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers,
                                                      UINT *pFirstConstant, UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers,
                                                      UINT *pFirstConstant, UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers,
                                                      UINT *pFirstConstant, UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                                      ID3D11Buffer **ppConstantBuffers,
                                                      UINT *pFirstConstant, UINT *pNumConstants));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SwapDeviceContextState(ID3DDeviceContextState *pState,
                                                       ID3DDeviceContextState **ppPreviousState));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ClearView(ID3D11View *pView, const FLOAT Color[4],
                                          const D3D11_RECT *pRect, UINT NumRects));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DiscardView1(ID3D11View *pResourceView, const D3D11_RECT *pRects,
                                             UINT NumRects));

  //////////////////////////////
  // implement ID3D11DeviceContext2

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual HRESULT STDMETHODCALLTYPE,
      UpdateTileMappings(ID3D11Resource *pTiledResource, UINT NumTiledResourceRegions,
                         const D3D11_TILED_RESOURCE_COORDINATE *pTiledResourceRegionStartCoordinates,
                         const D3D11_TILE_REGION_SIZE *pTiledResourceRegionSizes,
                         ID3D11Buffer *pTilePool, UINT NumRanges, const UINT *pRangeFlags,
                         const UINT *pTilePoolStartOffsets, const UINT *pRangeTileCounts, UINT Flags));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual HRESULT STDMETHODCALLTYPE,
      CopyTileMappings(ID3D11Resource *pDestTiledResource,
                       const D3D11_TILED_RESOURCE_COORDINATE *pDestRegionStartCoordinate,
                       ID3D11Resource *pSourceTiledResource,
                       const D3D11_TILED_RESOURCE_COORDINATE *pSourceRegionStartCoordinate,
                       const D3D11_TILE_REGION_SIZE *pTileRegionSize, UINT Flags));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CopyTiles(ID3D11Resource *pTiledResource,
                const D3D11_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
                const D3D11_TILE_REGION_SIZE *pTileRegionSize, ID3D11Buffer *pBuffer,
                UINT64 BufferStartOffsetInBytes, UINT Flags));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      UpdateTiles(ID3D11Resource *pDestTiledResource,
                  const D3D11_TILED_RESOURCE_COORDINATE *pDestTileRegionStartCoordinate,
                  const D3D11_TILE_REGION_SIZE *pDestTileRegionSize, const void *pSourceTileData,
                  UINT Flags));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                ResizeTilePool(ID3D11Buffer *pTilePool, UINT64 NewSizeInBytes));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      TiledResourceBarrier(ID3D11DeviceChild *pTiledResourceOrViewAccessBeforeBarrier,
                           ID3D11DeviceChild *pTiledResourceOrViewAccessAfterBarrier));

  virtual BOOL STDMETHODCALLTYPE IsAnnotationEnabled();

  virtual void STDMETHODCALLTYPE SetMarkerInt(LPCWSTR pLabel, INT Data);

  virtual void STDMETHODCALLTYPE BeginEventInt(LPCWSTR pLabel, INT Data);

  virtual void STDMETHODCALLTYPE EndEvent();

  //////////////////////////////
  // implement ID3D11DeviceContext3

  virtual void STDMETHODCALLTYPE Flush1(D3D11_CONTEXT_TYPE ContextType, HANDLE hEvent);

  virtual void STDMETHODCALLTYPE SetHardwareProtectionState(BOOL HwProtectionEnable);

  virtual void STDMETHODCALLTYPE GetHardwareProtectionState(BOOL *pHwProtectionEnable);
};
