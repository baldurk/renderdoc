/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "d3d11_resources.h"
#include "api/app/renderdoc_app.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "driver/shaders/dxbc/dxbc_reflect.h"
#include "d3d11_context.h"
#include "d3d11_renderstate.h"

WRAPPED_POOL_INST(WrappedID3D11Buffer);
WRAPPED_POOL_INST(WrappedID3D11Texture1D);
WRAPPED_POOL_INST(WrappedID3D11Texture2D1);
WRAPPED_POOL_INST(WrappedID3D11Texture3D1);
WRAPPED_POOL_INST(WrappedID3D11InputLayout);
WRAPPED_POOL_INST(WrappedID3D11SamplerState);
WRAPPED_POOL_INST(WrappedID3D11RasterizerState2);
WRAPPED_POOL_INST(WrappedID3D11DepthStencilState);
WRAPPED_POOL_INST(WrappedID3D11BlendState1);
WRAPPED_POOL_INST(WrappedID3D11ShaderResourceView1);
WRAPPED_POOL_INST(WrappedID3D11UnorderedAccessView1);
WRAPPED_POOL_INST(WrappedID3D11RenderTargetView1);
WRAPPED_POOL_INST(WrappedID3D11DepthStencilView);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11VertexShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11HullShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11DomainShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11GeometryShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11PixelShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11ComputeShader>);
WRAPPED_POOL_INST(WrappedID3D11Counter);
WRAPPED_POOL_INST(WrappedID3D11Query1);
WRAPPED_POOL_INST(WrappedID3D11Predicate);
WRAPPED_POOL_INST(WrappedID3D11ClassInstance);
WRAPPED_POOL_INST(WrappedID3D11ClassLinkage);
WRAPPED_POOL_INST(WrappedID3DDeviceContextState);
WRAPPED_POOL_INST(WrappedID3D11Fence);

std::map<ResourceId, WrappedID3D11Texture1D::TextureEntry>
    WrappedTexture<ID3D11Texture1D, D3D11_TEXTURE1D_DESC, ID3D11Texture1D>::m_TextureList;
std::map<ResourceId, WrappedID3D11Texture2D1::TextureEntry>
    WrappedTexture<ID3D11Texture2D, D3D11_TEXTURE2D_DESC, ID3D11Texture2D1>::m_TextureList;
std::map<ResourceId, WrappedID3D11Texture3D1::TextureEntry>
    WrappedTexture<ID3D11Texture3D, D3D11_TEXTURE3D_DESC, ID3D11Texture3D1>::m_TextureList;
std::map<ResourceId, WrappedID3D11Buffer::BufferEntry> WrappedID3D11Buffer::m_BufferList;
std::map<ResourceId, WrappedShader::ShaderEntry *> WrappedShader::m_ShaderList;
Threading::CriticalSection WrappedShader::m_ShaderListLock;
rdcarray<WrappedID3DDeviceContextState *> WrappedID3DDeviceContextState::m_List;
Threading::CriticalSection WrappedID3DDeviceContextState::m_Lock;

const GUID RENDERDOC_ID3D11ShaderGUID_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;
const GUID RENDERDOC_DeleteSelf = {
    0x1e4bf855,
    0xcc83,
    0x4b7a,
    {0x91, 0x8a, 0xd6, 0x64, 0x56, 0x7c, 0xdd, 0x40},
};

WrappedShader::ShaderEntry::ShaderEntry(WrappedID3D11Device *device, ResourceId id,
                                        const byte *code, size_t codeLen)
{
  m_ID = id;
  m_Bytecode.assign(code, codeLen);
  m_DXBCFile = NULL;
  m_Details = new ShaderReflection;
  m_DescriptorStore = device->GetImmediateContext()->GetDescriptorsID();
}

void WrappedShader::ShaderEntry::BuildReflection()
{
  RDCCOMPILE_ASSERT(
      D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT == D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
      "Mismatched vertex input count");

  MakeShaderReflection(m_DXBCFile, {}, m_Details);
  m_Details->resourceId = m_ID;

  DescriptorAccess access;
  access.descriptorStore = m_DescriptorStore;
  access.stage = m_Details->stage;
  access.byteSize = 1;

  m_Access.reserve(m_Details->constantBlocks.size() + m_Details->samplers.size() +
                   m_Details->readOnlyResources.size() + m_Details->readWriteResources.size());

  RDCASSERT(m_Details->constantBlocks.size() < 0xffff, m_Details->constantBlocks.size());
  for(uint16_t i = 0; i < m_Details->constantBlocks.size(); i++)
  {
    access.type = DescriptorType::ConstantBuffer;
    access.index = i;
    access.byteOffset = EncodeD3D11DescriptorIndex(
        {access.stage, D3D11DescriptorMapping::CBs, m_Details->constantBlocks[i].fixedBindNumber});
    m_Access.push_back(access);
  }

  RDCASSERT(m_Details->samplers.size() < 0xffff, m_Details->samplers.size());
  for(uint16_t i = 0; i < m_Details->samplers.size(); i++)
  {
    access.type = DescriptorType::Sampler;
    access.index = i;
    access.byteOffset = EncodeD3D11DescriptorIndex(
        {access.stage, D3D11DescriptorMapping::Samplers, m_Details->samplers[i].fixedBindNumber});
    m_Access.push_back(access);
  }

  RDCASSERT(m_Details->readOnlyResources.size() < 0xffff, m_Details->readOnlyResources.size());
  for(uint16_t i = 0; i < m_Details->readOnlyResources.size(); i++)
  {
    access.type = m_Details->readOnlyResources[i].descriptorType;
    access.index = i;
    access.byteOffset = EncodeD3D11DescriptorIndex({access.stage, D3D11DescriptorMapping::SRVs,
                                                    m_Details->readOnlyResources[i].fixedBindNumber});
    m_Access.push_back(access);
  }

  RDCASSERT(m_Details->readWriteResources.size() < 0xffff, m_Details->readWriteResources.size());
  for(uint16_t i = 0; i < m_Details->readWriteResources.size(); i++)
  {
    access.type = m_Details->readWriteResources[i].descriptorType;
    access.index = i;
    access.byteOffset =
        EncodeD3D11DescriptorIndex({access.stage, D3D11DescriptorMapping::UAVs,
                                    m_Details->readWriteResources[i].fixedBindNumber});
    m_Access.push_back(access);
  }
}

UINT GetSubresourceCount(ID3D11Resource *res)
{
  D3D11_RESOURCE_DIMENSION dim;
  res->GetType(&dim);

  if(dim == D3D11_RESOURCE_DIMENSION_BUFFER)
    return 1;

  ID3D11Texture1D *tex1 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D) ? (ID3D11Texture1D *)res : NULL;
  ID3D11Texture2D *tex2 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) ? (ID3D11Texture2D *)res : NULL;
  ID3D11Texture3D *tex3 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) ? (ID3D11Texture3D *)res : NULL;

  if(tex1)
  {
    D3D11_TEXTURE1D_DESC desc;
    tex1->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, 1, 1);

    return desc.ArraySize * mipLevels;
  }
  else if(tex2)
  {
    D3D11_TEXTURE2D_DESC desc;
    tex2->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, desc.Height, 1);

    return desc.ArraySize * mipLevels;
  }
  else if(tex3)
  {
    D3D11_TEXTURE3D_DESC desc;
    tex3->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, desc.Height, desc.Depth);

    return mipLevels;
  }

  return 1;
}

UINT GetMipForSubresource(ID3D11Resource *res, int Subresource)
{
  D3D11_RESOURCE_DIMENSION dim;
  res->GetType(&dim);

  if(dim == D3D11_RESOURCE_DIMENSION_BUFFER)
    return 0;

  ID3D11Texture1D *tex1 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D) ? (ID3D11Texture1D *)res : NULL;
  ID3D11Texture2D *tex2 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) ? (ID3D11Texture2D *)res : NULL;
  ID3D11Texture3D *tex3 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) ? (ID3D11Texture3D *)res : NULL;

  RDCASSERT(tex1 || tex2 || tex3);

  UINT mipLevel = Subresource;

  if(tex1)
  {
    D3D11_TEXTURE1D_DESC desc;
    tex1->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, 1, 1);

    mipLevel %= mipLevels;
  }
  else if(tex2)
  {
    D3D11_TEXTURE2D_DESC desc;
    tex2->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, desc.Height, 1);

    mipLevel %= mipLevels;
  }
  else if(tex3)
  {
    D3D11_TEXTURE3D_DESC desc;
    tex3->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, desc.Height, desc.Depth);

    mipLevel %= mipLevels;
  }

  return mipLevel;
}

UINT GetSliceForSubresource(ID3D11Resource *res, int Subresource)
{
  D3D11_RESOURCE_DIMENSION dim;
  res->GetType(&dim);

  if(dim == D3D11_RESOURCE_DIMENSION_BUFFER)
    return 0;

  ID3D11Texture1D *tex1 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D) ? (ID3D11Texture1D *)res : NULL;
  ID3D11Texture2D *tex2 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) ? (ID3D11Texture2D *)res : NULL;
  ID3D11Texture3D *tex3 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) ? (ID3D11Texture3D *)res : NULL;

  RDCASSERT(tex1 || tex2 || tex3);

  UINT slice = Subresource;

  if(tex1)
  {
    D3D11_TEXTURE1D_DESC desc;
    tex1->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, 1, 1);

    slice = (Subresource / mipLevels) % desc.ArraySize;
  }
  else if(tex2)
  {
    D3D11_TEXTURE2D_DESC desc;
    tex2->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, desc.Height, 1);

    slice = (Subresource / mipLevels) % desc.ArraySize;
  }
  else if(tex3)
  {
    D3D11_TEXTURE3D_DESC desc;
    tex3->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, desc.Height, desc.Depth);

    slice = (Subresource / mipLevels) % desc.Depth;
  }

  return slice;
}

UINT GetMipForDsv(const D3D11_DEPTH_STENCIL_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_DSV_DIMENSION_TEXTURE1D: return view.Texture1D.MipSlice;
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.MipSlice;
    case D3D11_DSV_DIMENSION_TEXTURE2D: return view.Texture2D.MipSlice;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.MipSlice;
    default: return 0;
  }
}

UINT GetSliceForDsv(const D3D11_DEPTH_STENCIL_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.FirstArraySlice;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.FirstArraySlice;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY: return view.Texture2DMSArray.FirstArraySlice;
    default: return 0;
  }
}

UINT GetSliceCountForDsv(const D3D11_DEPTH_STENCIL_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.ArraySize;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.ArraySize;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY: return view.Texture2DMSArray.ArraySize;
    default: return 0;
  }
}

UINT GetMipForRtv(const D3D11_RENDER_TARGET_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_RTV_DIMENSION_TEXTURE1D: return view.Texture1D.MipSlice;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.MipSlice;
    case D3D11_RTV_DIMENSION_TEXTURE2D: return view.Texture2D.MipSlice;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.MipSlice;
    case D3D11_RTV_DIMENSION_TEXTURE3D: return view.Texture3D.MipSlice;
    default: return 0;
  }
}

UINT GetSliceForRtv(const D3D11_RENDER_TARGET_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.FirstArraySlice;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.FirstArraySlice;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: return view.Texture2DMSArray.FirstArraySlice;
    default: return 0;
  }
}

UINT GetSliceCountForRtv(const D3D11_RENDER_TARGET_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.ArraySize;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.ArraySize;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: return view.Texture2DMSArray.ArraySize;
    default: return 0;
  }
}

UINT GetMipForSrv(const D3D11_SHADER_RESOURCE_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_SRV_DIMENSION_TEXTURE1D: return view.Texture1D.MostDetailedMip;
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.MostDetailedMip;
    case D3D11_SRV_DIMENSION_TEXTURE2D: return view.Texture2D.MostDetailedMip;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.MostDetailedMip;
    case D3D11_SRV_DIMENSION_TEXTURECUBE: return view.TextureCube.MostDetailedMip;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY: return view.TextureCubeArray.MostDetailedMip;
    case D3D11_SRV_DIMENSION_TEXTURE3D: return view.Texture3D.MostDetailedMip;
    default: return 0;
  }
}
UINT GetSliceForSrv(const D3D11_SHADER_RESOURCE_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.FirstArraySlice;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.FirstArraySlice;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY: return view.TextureCubeArray.First2DArrayFace;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY: return view.Texture2DMSArray.FirstArraySlice;
    default: return 0;
  }
}

UINT GetSliceCountForSrv(const D3D11_SHADER_RESOURCE_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.ArraySize;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.ArraySize;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY: return view.TextureCubeArray.NumCubes * 6;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY: return view.Texture2DMSArray.ArraySize;
    default: return 0;
  }
}

UINT GetMipForUav(const D3D11_UNORDERED_ACCESS_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_UAV_DIMENSION_TEXTURE1D: return view.Texture1D.MipSlice;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.MipSlice;
    case D3D11_UAV_DIMENSION_TEXTURE2D: return view.Texture2D.MipSlice;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.MipSlice;
    case D3D11_UAV_DIMENSION_TEXTURE3D: return view.Texture3D.MipSlice;
    default: return 0;
  }
}
UINT GetSliceForUav(const D3D11_UNORDERED_ACCESS_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.FirstArraySlice;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.FirstArraySlice;
    case D3D11_UAV_DIMENSION_TEXTURE3D: return view.Texture3D.FirstWSlice;
    default: return 0;
  }
}

UINT GetSliceCountForUav(const D3D11_UNORDERED_ACCESS_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.ArraySize;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.ArraySize;
    case D3D11_UAV_DIMENSION_TEXTURE3D: return view.Texture3D.WSize;
    default: return 0;
  }
}

ResourcePitch GetResourcePitchForSubresource(ID3D11DeviceContext *ctx, ID3D11Resource *res,
                                             int Subresource)
{
  ResourcePitch pitch = {};
  D3D11_MAPPED_SUBRESOURCE mapped = {};

  HRESULT hr = ctx->Map(res, Subresource, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to map while getting resource pitch HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    pitch.m_RowPitch = mapped.RowPitch;
    pitch.m_DepthPitch = mapped.DepthPitch;
    ctx->Unmap(res, Subresource);
  }

  return pitch;
}

UINT GetByteSize(ID3D11Texture1D *tex, int SubResource)
{
  D3D11_TEXTURE1D_DESC desc;
  tex->GetDesc(&desc);

  return GetByteSize(desc.Width, 1, 1, desc.Format, SubResource % desc.MipLevels);
}

UINT GetByteSize(ID3D11Texture2D *tex, int SubResource)
{
  D3D11_TEXTURE2D_DESC desc;
  tex->GetDesc(&desc);

  return GetByteSize(desc.Width, desc.Height, 1, desc.Format, SubResource % desc.MipLevels);
}

UINT GetByteSize(ID3D11Texture3D *tex, int SubResource)
{
  D3D11_TEXTURE3D_DESC desc;
  tex->GetDesc(&desc);

  return GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, SubResource);
}

D3D11ResourceType IdentifyTypeByPtr(IUnknown *ptr)
{
  if(WrappedID3D11InputLayout::IsAlloc(ptr))
    return Resource_InputLayout;

  if(WrappedID3D11Shader<ID3D11VertexShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11PixelShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11GeometryShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11HullShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11DomainShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11ComputeShader>::IsAlloc(ptr))
    return Resource_Shader;

  if(WrappedID3D11Buffer::IsAlloc(ptr))
    return Resource_Buffer;

  if(WrappedID3D11Texture1D::IsAlloc(ptr))
    return Resource_Texture1D;
  if(WrappedID3D11Texture2D1::IsAlloc(ptr))
    return Resource_Texture2D;
  if(WrappedID3D11Texture3D1::IsAlloc(ptr))
    return Resource_Texture3D;

  if(WrappedID3D11RasterizerState2::IsAlloc(ptr))
    return Resource_RasterizerState;
  if(WrappedID3D11BlendState1::IsAlloc(ptr))
    return Resource_BlendState;
  if(WrappedID3D11DepthStencilState::IsAlloc(ptr))
    return Resource_DepthStencilState;
  if(WrappedID3D11SamplerState::IsAlloc(ptr))
    return Resource_SamplerState;

  if(WrappedID3D11RenderTargetView1::IsAlloc(ptr))
    return Resource_RenderTargetView;
  if(WrappedID3D11ShaderResourceView1::IsAlloc(ptr))
    return Resource_ShaderResourceView;
  if(WrappedID3D11DepthStencilView::IsAlloc(ptr))
    return Resource_DepthStencilView;
  if(WrappedID3D11UnorderedAccessView1::IsAlloc(ptr))
    return Resource_UnorderedAccessView;

  if(WrappedID3D11Counter::IsAlloc(ptr))
    return Resource_Counter;
  if(WrappedID3D11Query1::IsAlloc(ptr))
    return Resource_Query;
  if(WrappedID3D11Predicate::IsAlloc(ptr))
    return Resource_Predicate;

  if(WrappedID3D11ClassInstance::IsAlloc(ptr))
    return Resource_ClassInstance;
  if(WrappedID3D11ClassLinkage::IsAlloc(ptr))
    return Resource_ClassLinkage;

  if(WrappedID3D11DeviceContext::IsAlloc(ptr))
    return Resource_DeviceContext;
  if(WrappedID3D11CommandList::IsAlloc(ptr))
    return Resource_CommandList;

  if(WrappedID3DDeviceContextState::IsAlloc(ptr))
    return Resource_DeviceState;

  if(WrappedID3D11Fence::IsAlloc(ptr))
    return Resource_Fence;

  RDCERR("Unknown type for ptr 0x%p", ptr);

  return Resource_Unknown;
}

void *UnwrapDXDevice(void *dxDevice)
{
  if(WrappedID3D11Device::IsAlloc(dxDevice))
    return ((WrappedID3D11Device *)(ID3D11Device *)dxDevice)->GetReal();

  return NULL;
}

ID3D11Resource *UnwrapDXResource(void *dxObject)
{
  if(WrappedID3D11Buffer::IsAlloc(dxObject))
  {
    WrappedID3D11Buffer *w = (WrappedID3D11Buffer *)(ID3D11Buffer *)dxObject;
    return w->GetReal();
  }
  else if(WrappedID3D11Texture1D::IsAlloc(dxObject))
  {
    WrappedID3D11Texture1D *w = (WrappedID3D11Texture1D *)(ID3D11Texture1D *)dxObject;
    return w->GetReal();
  }
  else if(WrappedID3D11Texture2D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture2D1 *w = (WrappedID3D11Texture2D1 *)(ID3D11Texture2D1 *)dxObject;
    return w->GetReal();
  }
  else if(WrappedID3D11Texture3D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture3D1 *w = (WrappedID3D11Texture3D1 *)(ID3D11Texture3D1 *)dxObject;
    return w->GetReal();
  }

  return NULL;
}

IDXGIResource *UnwrapDXGIResource(void *dxObject)
{
  ID3D11Resource *dx = NULL;

  if(WrappedID3D11Buffer::IsAlloc(dxObject))
  {
    WrappedID3D11Buffer *w = (WrappedID3D11Buffer *)dxObject;
    dx = w->GetReal();
  }
  else if(WrappedID3D11Texture1D::IsAlloc(dxObject))
  {
    WrappedID3D11Texture1D *w = (WrappedID3D11Texture1D *)dxObject;
    dx = w->GetReal();
  }
  else if(WrappedID3D11Texture2D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture2D1 *w = (WrappedID3D11Texture2D1 *)dxObject;
    dx = w->GetReal();
  }
  else if(WrappedID3D11Texture3D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture3D1 *w = (WrappedID3D11Texture3D1 *)dxObject;
    dx = w->GetReal();
  }

  if(dx)
  {
    IDXGIResource *ret = NULL;
    dx->QueryInterface(__uuidof(IDXGIResource), (void **)&ret);
    if(ret)
    {
      ret->Release();
      return ret;
    }
  }

  return NULL;
}

void GetDXTextureProperties(void *dxObject, ResourceFormat &fmt, uint32_t &width, uint32_t &height,
                            uint32_t &depth, uint32_t &mips, uint32_t &layers, uint32_t &samples)
{
  if(WrappedID3D11Buffer::IsAlloc(dxObject))
  {
    WrappedID3D11Buffer *w = (WrappedID3D11Buffer *)(ID3D11Buffer *)dxObject;

    D3D11_BUFFER_DESC desc;
    w->GetDesc(&desc);

    fmt = ResourceFormat();
    width = desc.ByteWidth;
    height = 1;
    depth = 1;
    mips = 1;
    layers = 1;
    samples = 1;

    return;
  }
  else if(WrappedID3D11Texture1D::IsAlloc(dxObject))
  {
    WrappedID3D11Texture1D *w = (WrappedID3D11Texture1D *)(ID3D11Texture1D *)dxObject;

    D3D11_TEXTURE1D_DESC desc;
    w->GetDesc(&desc);

    fmt = MakeResourceFormat(desc.Format);
    width = desc.Width;
    height = 1;
    depth = 1;
    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, 1, 1);
    layers = desc.ArraySize;
    samples = 1;

    return;
  }
  else if(WrappedID3D11Texture2D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture2D1 *w = (WrappedID3D11Texture2D1 *)(ID3D11Texture2D1 *)dxObject;

    D3D11_TEXTURE2D_DESC desc;
    w->GetDesc(&desc);

    fmt = MakeResourceFormat(desc.Format);
    width = desc.Width;
    height = desc.Height;
    depth = 1;
    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, 1);
    layers = desc.ArraySize;
    samples = desc.SampleDesc.Count;

    return;
  }
  else if(WrappedID3D11Texture3D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture3D1 *w = (WrappedID3D11Texture3D1 *)(ID3D11Texture3D1 *)dxObject;

    D3D11_TEXTURE3D_DESC desc;
    w->GetDesc(&desc);

    fmt = MakeResourceFormat(desc.Format);
    width = desc.Width;
    height = desc.Height;
    depth = desc.Depth;
    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, desc.Depth);
    layers = 1;
    samples = 1;

    return;
  }

  RDCERR("Getting DX texture properties for unknown/unhandled objects %p", dxObject);
}

WrappedID3DDeviceContextState::WrappedID3DDeviceContextState(ID3DDeviceContextState *real,
                                                             WrappedID3D11Device *device)
    : WrappedDeviceChild11(real, device)
{
  state = new D3D11RenderState(D3D11RenderState::Empty);

  {
    SCOPED_LOCK(WrappedID3DDeviceContextState::m_Lock);
    WrappedID3DDeviceContextState::m_List.push_back(this);
  }
}

WrappedID3DDeviceContextState::~WrappedID3DDeviceContextState()
{
  SAFE_DELETE(state);

  {
    SCOPED_LOCK(WrappedID3DDeviceContextState::m_Lock);
    WrappedID3DDeviceContextState::m_List.removeOne(this);
  }
}
