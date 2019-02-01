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

#pragma once

#include "common/common.h"
#include "core/core.h"
#include "driver/d3d11/d3d11_manager.h"

class WrappedID3D11Device;
class WrappedID3D11DeviceContext;
class D3D11ResourceManager;

struct D3D11RenderState
{
  enum EmptyInit
  {
    Empty
  };
  D3D11RenderState(EmptyInit);
  D3D11RenderState(WrappedID3D11DeviceContext *context);
  D3D11RenderState(const D3D11RenderState &other);
  ~D3D11RenderState();

  // we don't allow operator = since we want to preserve some properties.
  // Instead use CopyState() which copies all of the state contained without
  // modifying the device pointer or immediate pipeline flag.
  D3D11RenderState &operator=(const D3D11RenderState &other) = delete;

  void CopyState(const D3D11RenderState &other);

  void ApplyState(WrappedID3D11DeviceContext *context);
  void Clear();

  ///////////////////////////////////////////////////////////////////////////////
  // pipeline-auto NULL. When binding a resource for write, it will be
  // unbound anywhere that it is bound for read.
  //
  // if binding a resource for READ, and it's still bound for write, the
  // bind will be disallowed and NULL will be bound instead
  //
  // need to be aware of depth-stencil as a special case, DSV can be flagged read-only
  // of depth, stencil or both to allow read binds of that component at the same time.

  // the below functions are only called with ResourceRange, which is only constructable for
  // views (where it takes the resource and visible subresources from the view parameters)
  // or directly for a resource. Thus, they are never called with a view directly, or any other
  // type.

  // is any part of this range bound for a writing part of the pipeline? if so, read binds will bind
  // NULL
  bool IsRangeBoundForWrite(const ResourceRange &range);

  // this range was bound for writing - find any overlapping read binds and set them to NULL
  void UnbindRangeForRead(const ResourceRange &range);

  // just for utility, not used below
  void UnbindRangeForWrite(const ResourceRange &range);

  // define template but only implement for specific types, so we can more easily reason
  // about what types are passing through these functions
  template <typename T>
  bool IsBoundForWrite(T *resource);

  // same as IsBoundForWrite above
  template <typename T>
  void UnbindForRead(T *resource);

  template <typename T>
  void UnbindForWrite(T *resource);

  /////////////////////////////////////////////////////////////////////////
  // Utility functions to swap resources around, removing and adding refs

  void TakeRef(ID3D11DeviceChild *p);
  void ReleaseRef(ID3D11DeviceChild *p);

  template <typename T>
  void ChangeRefRead(T *&stateItem, T *newItem)
  {
    // don't do anything for redundant changes. This prevents the object from bouncing off refcount
    // 0 during the changeover if it's only bound once, has no external refcount.
    if(stateItem == newItem)
      return;

    // release the old item, which may destroy it but we won't use it again as we know is not the
    // same as the new item.
    ReleaseRef(stateItem);

    // assign the new item, but don't ref it yet
    stateItem = newItem;

    // if the item is bound for writing anywhere, we instead bind NULL
    if(IsBoundForWrite(newItem))
    {
      // RDCDEBUG("Resource was bound for write, forcing to NULL");
      stateItem = NULL;
    }

    // finally we take the ref on the new item
    TakeRef(stateItem);
  }

  template <typename T>
  void ChangeRefRead(T **stateArray, T *const *newArray, size_t offset, size_t num)
  {
    // addref the whole array so none of it can be destroyed during processing
    for(size_t i = 0; i < num; i++)
      if(newArray[i])
        newArray[i]->AddRef();

    for(size_t i = 0; i < num; i++)
      ChangeRefRead(stateArray[offset + i], newArray[i]);

    // release the ref we added above
    for(size_t i = 0; i < num; i++)
      if(newArray[i])
        newArray[i]->Release();
  }

  template <typename T>
  void ChangeRefWrite(T *&stateItem, T *newItem)
  {
    // don't do anything for redundant changes. This prevents the object from bouncing off refcount
    // 0 during the changeover if it's only bound once, has no external refcount.
    if(stateItem == newItem)
      return;

    // release the old item, which may destroy it but we won't use it again as we know is not the
    // same as the new item. We NULL it out so that it doesn't get unbound again below
    ReleaseRef(stateItem);
    stateItem = NULL;

    // if we're not binding NULL, then unbind any other conflicting uses
    if(newItem)
    {
      UnbindForRead(newItem);
      // when binding something for write, all other write slots are NULL'd too
      UnbindForWrite(newItem);
    }

    // now bind the new item and ref it
    stateItem = newItem;
    TakeRef(stateItem);
  }

  template <typename T>
  void ChangeRefWrite(T **stateArray, T *const *newArray, size_t offset, size_t num)
  {
    // addref the whole array so none of it can be destroyed during processing
    for(size_t i = 0; i < num; i++)
      if(newArray[i])
        newArray[i]->AddRef();

    for(size_t i = 0; i < num; i++)
      ChangeRefWrite(stateArray[offset + i], newArray[i]);

    // release the ref we added above
    for(size_t i = 0; i < num; i++)
      if(newArray[i])
        newArray[i]->Release();
  }

  template <typename T>
  void Change(T *stateArray, const T *newArray, size_t offset, size_t num)
  {
    for(size_t i = 0; i < num; i++)
      stateArray[i + offset] = newArray[i];
  }

  template <typename T>
  void Change(T &stateItem, const T &newItem)
  {
    stateItem = newItem;
  }

  /////////////////////////////////////////////////////////////////////////
  // Implement any checks that D3D does that will change the state in ways
  // that might not be obvious/intended.

  // validate an output merger combination of render targets and depth view
  bool ValidOutputMerger(ID3D11RenderTargetView *const RTs[], UINT NumRTs,
                         ID3D11DepthStencilView *depth, ID3D11UnorderedAccessView *const uavs[],
                         UINT NumUAVs);

  struct InputAssembler
  {
    ID3D11InputLayout *Layout;
    D3D11_PRIMITIVE_TOPOLOGY Topo;
    ID3D11Buffer *VBs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    UINT Strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    UINT Offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11Buffer *IndexBuffer;
    DXGI_FORMAT IndexFormat;
    UINT IndexOffset;

    bool Used_VB(WrappedID3D11Device *device, uint32_t slot) const;
  } IA;

  struct Shader
  {
    ID3D11DeviceChild *Object;
    ID3D11Buffer *ConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    UINT CBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    UINT CBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11SamplerState *Samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    ID3D11ClassInstance *Instances[D3D11_SHADER_MAX_INTERFACES];
    UINT NumInstances;

    bool Used_CB(uint32_t slot) const;
    bool Used_SRV(uint32_t slot) const;
    bool Used_UAV(uint32_t slot) const;
  } VS, HS, DS, GS, PS, CS;

  ID3D11UnorderedAccessView *CSUAVs[D3D11_1_UAV_SLOT_COUNT];

  struct StreamOut
  {
    ID3D11Buffer *Buffers[D3D11_SO_BUFFER_SLOT_COUNT];
    UINT Offsets[D3D11_SO_BUFFER_SLOT_COUNT];
  } SO;

  struct Rasterizer
  {
    UINT NumViews, NumScissors;
    D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    D3D11_RECT Scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    ID3D11RasterizerState *State;
  } RS;

  struct OutputMerger
  {
    ID3D11DepthStencilState *DepthStencilState;
    UINT StencRef;

    ID3D11BlendState *BlendState;
    FLOAT BlendFactor[4];
    UINT SampleMask;

    ID3D11DepthStencilView *DepthView;

    ID3D11RenderTargetView *RenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];

    UINT UAVStartSlot;
    ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT];
  } OM;

  ID3D11Predicate *Predicate;
  BOOL PredicateValue;

  bool PredicationWouldPass();

  void SetImmediatePipeline(WrappedID3D11Device *device)
  {
    m_ImmediatePipeline = true;
    m_pDevice = device;
  }
  void SetDevice(WrappedID3D11Device *device) { m_pDevice = device; }
  void MarkReferenced(WrappedID3D11DeviceContext *ctx, bool initial) const;

private:
  template <class SerialiserType>
  friend void DoSerialise(SerialiserType &ser, D3D11RenderState &el);

  void AddRefs();
  void ReleaseRefs();

  bool m_ImmediatePipeline;
  WrappedID3D11Device *m_pDevice;
};

DECLARE_REFLECTION_STRUCT(D3D11RenderState::InputAssembler);
DECLARE_REFLECTION_STRUCT(D3D11RenderState::Shader);
DECLARE_REFLECTION_STRUCT(D3D11RenderState::StreamOut);
DECLARE_REFLECTION_STRUCT(D3D11RenderState::Rasterizer);
DECLARE_REFLECTION_STRUCT(D3D11RenderState::OutputMerger);
DECLARE_REFLECTION_STRUCT(D3D11RenderState);

struct D3D11RenderStateTracker
{
public:
  D3D11RenderStateTracker(WrappedID3D11DeviceContext *ctx);
  ~D3D11RenderStateTracker();

  const D3D11RenderState &State() { return m_RS; }
private:
  D3D11RenderState m_RS;
  WrappedID3D11DeviceContext *m_pContext;
};

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11InputLayout *resource);

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11Predicate *resource);

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11ClassInstance *resource);

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11DeviceChild *shader);

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11SamplerState *resource);

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11BlendState *state);

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11RasterizerState *state);

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11DepthStencilState *state);

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11Buffer *buffer);

template <>
bool D3D11RenderState::IsBoundForWrite(ID3D11ShaderResourceView *srv);

template <>
void D3D11RenderState::UnbindForRead(ID3D11Buffer *buffer);

template <>
void D3D11RenderState::UnbindForRead(ID3D11RenderTargetView *rtv);

template <>
void D3D11RenderState::UnbindForRead(ID3D11DepthStencilView *dsv);

template <>
void D3D11RenderState::UnbindForRead(ID3D11UnorderedAccessView *uav);

template <>
void D3D11RenderState::UnbindForWrite(ID3D11Buffer *buffer);

template <>
void D3D11RenderState::UnbindForWrite(ID3D11RenderTargetView *rtv);

template <>
void D3D11RenderState::UnbindForWrite(ID3D11DepthStencilView *dsv);

template <>
void D3D11RenderState::UnbindForWrite(ID3D11UnorderedAccessView *uav);
