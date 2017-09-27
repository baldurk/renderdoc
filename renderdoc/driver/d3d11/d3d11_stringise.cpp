/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "common/common.h"
#include "d3d11_common.h"
#include "d3d11_resources.h"

template <>
std::string DoStringise(const ResourceType &el)
{
  BEGIN_ENUM_STRINGISE(ResourceType);
  {
    STRINGISE_ENUM(Resource_InputLayout)
    STRINGISE_ENUM(Resource_Buffer)
    STRINGISE_ENUM(Resource_Texture1D)
    STRINGISE_ENUM(Resource_Texture2D)
    STRINGISE_ENUM(Resource_Texture3D)
    STRINGISE_ENUM(Resource_RasterizerState)
    STRINGISE_ENUM(Resource_BlendState)
    STRINGISE_ENUM(Resource_DepthStencilState)
    STRINGISE_ENUM(Resource_SamplerState)
    STRINGISE_ENUM(Resource_RenderTargetView)
    STRINGISE_ENUM(Resource_ShaderResourceView)
    STRINGISE_ENUM(Resource_DepthStencilView)
    STRINGISE_ENUM(Resource_Shader)
    STRINGISE_ENUM(Resource_UnorderedAccessView)
    STRINGISE_ENUM(Resource_Counter)
    STRINGISE_ENUM(Resource_Query)
    STRINGISE_ENUM(Resource_Predicate)
    STRINGISE_ENUM(Resource_ClassInstance)
    STRINGISE_ENUM(Resource_ClassLinkage)
    STRINGISE_ENUM(Resource_DeviceContext)
    STRINGISE_ENUM(Resource_CommandList)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11Chunk &el)
{
  RDCCOMPILE_ASSERT((uint32_t)D3D11Chunk::Max == 1131, "Chunks changed without updating names");

  BEGIN_ENUM_STRINGISE(D3D11Chunk)
  {
    STRINGISE_ENUM_CLASS_NAMED(DeviceInitialisation, "Device Initialisation");
    STRINGISE_ENUM_CLASS_NAMED(SetResourceName, "ID3D11Resource::SetDebugName");
    STRINGISE_ENUM_CLASS_NAMED(ReleaseResource, "IUnknown::Release");
    STRINGISE_ENUM_CLASS_NAMED(CreateSwapBuffer, "IDXGISwapChain::GetBuffer");
    STRINGISE_ENUM_CLASS_NAMED(CreateTexture1D, "ID3D11Device::CreateTexture1D");
    STRINGISE_ENUM_CLASS_NAMED(CreateTexture2D, "ID3D11Device::CreateTexture2D");
    STRINGISE_ENUM_CLASS_NAMED(CreateTexture3D, "ID3D11Device::CreateTexture3D");
    STRINGISE_ENUM_CLASS_NAMED(CreateBuffer, "ID3D11Device::CreateBuffer");
    STRINGISE_ENUM_CLASS_NAMED(CreateVertexShader, "ID3D11Device::CreateVertexShader");
    STRINGISE_ENUM_CLASS_NAMED(CreateHullShader, "ID3D11Device::CreateHullShader");
    STRINGISE_ENUM_CLASS_NAMED(CreateDomainShader, "ID3D11Device::CreateDomainShader");
    STRINGISE_ENUM_CLASS_NAMED(CreateGeometryShader, "ID3D11Device::CreateGeometryShader");
    STRINGISE_ENUM_CLASS_NAMED(CreateGeometryShaderWithStreamOutput,
                               "ID3D11Device::CreateGeometryShaderWithStreamOutput");
    STRINGISE_ENUM_CLASS_NAMED(CreatePixelShader, "ID3D11Device::CreatePixelShader");
    STRINGISE_ENUM_CLASS_NAMED(CreateComputeShader, "ID3D11Device::CreateComputeShader");
    STRINGISE_ENUM_CLASS_NAMED(GetClassInstance, "ID3D11ClassLinkage::GetClassInstance");
    STRINGISE_ENUM_CLASS_NAMED(CreateClassInstance, "ID3D11ClassLinkage::CreateClassInstance");
    STRINGISE_ENUM_CLASS_NAMED(CreateClassLinkage, "ID3D11Device::CreateClassLinkage");
    STRINGISE_ENUM_CLASS_NAMED(CreateShaderResourceView, "ID3D11Device::CreateShaderResourceView");
    STRINGISE_ENUM_CLASS_NAMED(CreateRenderTargetView, "ID3D11Device::CreateRenderTargetView");
    STRINGISE_ENUM_CLASS_NAMED(CreateDepthStencilView, "ID3D11Device::CreateDepthStencilView");
    STRINGISE_ENUM_CLASS_NAMED(CreateUnorderedAccessView,
                               "ID3D11Device::CreateUnorderedAccessView");
    STRINGISE_ENUM_CLASS_NAMED(CreateInputLayout, "ID3D11Device::CreateInputLayout");
    STRINGISE_ENUM_CLASS_NAMED(CreateBlendState, "ID3D11Device::CreateBlendState");
    STRINGISE_ENUM_CLASS_NAMED(CreateDepthStencilState, "ID3D11Device::CreateDepthStencilState");
    STRINGISE_ENUM_CLASS_NAMED(CreateRasterizerState, "ID3D11Device::CreateRasterizerState");
    STRINGISE_ENUM_CLASS_NAMED(CreateSamplerState, "ID3D11Device::CreateSamplerState");
    STRINGISE_ENUM_CLASS_NAMED(CreateQuery, "ID3D11Device::CreateQuery");
    STRINGISE_ENUM_CLASS_NAMED(CreatePredicate, "ID3D11Device::CreatePredicate");
    STRINGISE_ENUM_CLASS_NAMED(CreateCounter, "ID3D11Device::CreateCounter");
    STRINGISE_ENUM_CLASS_NAMED(CreateDeferredContext, "ID3D11Device::CreateDeferredContext");
    STRINGISE_ENUM_CLASS_NAMED(SetExceptionMode, "ID3D11Device::SetExceptionMode");
    STRINGISE_ENUM_CLASS_NAMED(OpenSharedResource, "ID3D11Device::OpenSharedResource");
    STRINGISE_ENUM_CLASS_NAMED(CaptureScope, "Frame Capture Metadata");
    STRINGISE_ENUM_CLASS_NAMED(IASetInputLayout, "ID3D11DeviceContext::IASetInputLayout");
    STRINGISE_ENUM_CLASS_NAMED(IASetVertexBuffers, "ID3D11DeviceContext::IASetVertexBuffers");
    STRINGISE_ENUM_CLASS_NAMED(IASetIndexBuffer, "ID3D11DeviceContext::IASetIndexBuffer");
    STRINGISE_ENUM_CLASS_NAMED(IASetPrimitiveTopology,
                               "ID3D11DeviceContext::IASetPrimitiveTopology");
    STRINGISE_ENUM_CLASS_NAMED(VSSetConstantBuffers, "ID3D11DeviceContext::VSSetConstantBuffers");
    STRINGISE_ENUM_CLASS_NAMED(VSSetShaderResources, "ID3D11DeviceContext::VSSetShaderResources");
    STRINGISE_ENUM_CLASS_NAMED(VSSetSamplers, "ID3D11DeviceContext::VSSetSamplers");
    STRINGISE_ENUM_CLASS_NAMED(VSSetShader, "ID3D11DeviceContext::VSSetShader");
    STRINGISE_ENUM_CLASS_NAMED(HSSetConstantBuffers, "ID3D11DeviceContext::HSSetConstantBuffers");
    STRINGISE_ENUM_CLASS_NAMED(HSSetShaderResources, "ID3D11DeviceContext::HSSetShaderResources");
    STRINGISE_ENUM_CLASS_NAMED(HSSetSamplers, "ID3D11DeviceContext::HSSetSamplers");
    STRINGISE_ENUM_CLASS_NAMED(HSSetShader, "ID3D11DeviceContext::HSSetShader");
    STRINGISE_ENUM_CLASS_NAMED(DSSetConstantBuffers, "ID3D11DeviceContext::DSSetConstantBuffers");
    STRINGISE_ENUM_CLASS_NAMED(DSSetShaderResources, "ID3D11DeviceContext::DSSetShaderResources");
    STRINGISE_ENUM_CLASS_NAMED(DSSetSamplers, "ID3D11DeviceContext::DSSetSamplers");
    STRINGISE_ENUM_CLASS_NAMED(DSSetShader, "ID3D11DeviceContext::DSSetShader");
    STRINGISE_ENUM_CLASS_NAMED(GSSetConstantBuffers, "ID3D11DeviceContext::GSSetConstantBuffers");
    STRINGISE_ENUM_CLASS_NAMED(GSSetShaderResources, "ID3D11DeviceContext::GSSetShaderResources");
    STRINGISE_ENUM_CLASS_NAMED(GSSetSamplers, "ID3D11DeviceContext::GSSetSamplers");
    STRINGISE_ENUM_CLASS_NAMED(GSSetShader, "ID3D11DeviceContext::GSSetShader");
    STRINGISE_ENUM_CLASS_NAMED(SOSetTargets, "ID3D11DeviceContext::SOSetTargets");
    STRINGISE_ENUM_CLASS_NAMED(PSSetConstantBuffers, "ID3D11DeviceContext::PSSetConstantBuffers");
    STRINGISE_ENUM_CLASS_NAMED(PSSetShaderResources, "ID3D11DeviceContext::PSSetShaderResources");
    STRINGISE_ENUM_CLASS_NAMED(PSSetSamplers, "ID3D11DeviceContext::PSSetSamplers");
    STRINGISE_ENUM_CLASS_NAMED(PSSetShader, "ID3D11DeviceContext::PSSetShader");
    STRINGISE_ENUM_CLASS_NAMED(CSSetConstantBuffers, "ID3D11DeviceContext::CSSetConstantBuffers");
    STRINGISE_ENUM_CLASS_NAMED(CSSetShaderResources, "ID3D11DeviceContext::CSSetShaderResources");
    STRINGISE_ENUM_CLASS_NAMED(CSSetUnorderedAccessViews,
                               "ID3D11DeviceContext::CSSetUnorderedAccessViews");
    STRINGISE_ENUM_CLASS_NAMED(CSSetSamplers, "ID3D11DeviceContext::CSSetSamplers");
    STRINGISE_ENUM_CLASS_NAMED(CSSetShader, "ID3D11DeviceContext::CSSetShader");
    STRINGISE_ENUM_CLASS_NAMED(RSSetViewports, "ID3D11DeviceContext::RSSetViewports");
    STRINGISE_ENUM_CLASS_NAMED(RSSetScissorRects, "ID3D11DeviceContext::RSSetScissors");
    STRINGISE_ENUM_CLASS_NAMED(RSSetState, "ID3D11DeviceContext::RSSetState");
    STRINGISE_ENUM_CLASS_NAMED(OMSetRenderTargets, "ID3D11DeviceContext::OMSetRenderTargets");
    STRINGISE_ENUM_CLASS_NAMED(OMSetRenderTargetsAndUnorderedAccessViews,
                               "ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews");
    STRINGISE_ENUM_CLASS_NAMED(OMSetBlendState, "ID3D11DeviceContext::OMSetBlendState");
    STRINGISE_ENUM_CLASS_NAMED(OMSetDepthStencilState,
                               "ID3D11DeviceContext::OMSetDepthStencilState");
    STRINGISE_ENUM_CLASS_NAMED(DrawIndexedInstanced, "ID3D11DeviceContext::DrawIndexedInstanced");
    STRINGISE_ENUM_CLASS_NAMED(DrawInstanced, "ID3D11DeviceContext::DrawInstanced");
    STRINGISE_ENUM_CLASS_NAMED(DrawIndexed, "ID3D11DeviceContext::DrawIndexed");
    STRINGISE_ENUM_CLASS_NAMED(Draw, "ID3D11DeviceContext::Draw");
    STRINGISE_ENUM_CLASS_NAMED(DrawAuto, "ID3D11DeviceContext::DrawAuto");
    STRINGISE_ENUM_CLASS_NAMED(DrawIndexedInstancedIndirect,
                               "ID3D11DeviceContext::DrawIndexedInstancedIndirect");
    STRINGISE_ENUM_CLASS_NAMED(DrawInstancedIndirect, "ID3D11DeviceContext::DrawInstancedIndirect");
    STRINGISE_ENUM_CLASS_NAMED(Map, "ID3D11DeviceContext::Map");
    STRINGISE_ENUM_CLASS_NAMED(Unmap, "ID3D11DeviceContext::Unmap");
    STRINGISE_ENUM_CLASS_NAMED(CopySubresourceRegion, "ID3D11DeviceContext::CopySubresourceRegion");
    STRINGISE_ENUM_CLASS_NAMED(CopyResource, "ID3D11DeviceContext::CopyResource");
    STRINGISE_ENUM_CLASS_NAMED(UpdateSubresource, "ID3D11DeviceContext::UpdateSubresource");
    STRINGISE_ENUM_CLASS_NAMED(CopyStructureCount, "ID3D11DeviceContext::CopyStructureCount");
    STRINGISE_ENUM_CLASS_NAMED(ResolveSubresource, "ID3D11DeviceContext::ResolveSubresource");
    STRINGISE_ENUM_CLASS_NAMED(GenerateMips, "ID3D11DeviceContext::GenerateMips");
    STRINGISE_ENUM_CLASS_NAMED(ClearDepthStencilView, "ID3D11DeviceContext::ClearDepthStencilView");
    STRINGISE_ENUM_CLASS_NAMED(ClearRenderTargetView, "ID3D11DeviceContext::ClearRenderTargetView");
    STRINGISE_ENUM_CLASS_NAMED(ClearUnorderedAccessViewUint,
                               "ID3D11DeviceContext::ClearUnorderedAccessViewInt");
    STRINGISE_ENUM_CLASS_NAMED(ClearUnorderedAccessViewFloat,
                               "ID3D11DeviceContext::ClearUnorderedAccessViewFloat");
    STRINGISE_ENUM_CLASS_NAMED(ClearState, "ID3D11DeviceContext::ClearState");
    STRINGISE_ENUM_CLASS_NAMED(ExecuteCommandList, "ID3D11DeviceContext::ExecuteCommandList");
    STRINGISE_ENUM_CLASS_NAMED(Dispatch, "ID3D11DeviceContext::Dispatch");
    STRINGISE_ENUM_CLASS_NAMED(DispatchIndirect, "ID3D11DeviceContext::DispatchIndirect");
    STRINGISE_ENUM_CLASS_NAMED(FinishCommandList, "ID3D11DeviceContext::FinishCommandlist");
    STRINGISE_ENUM_CLASS_NAMED(Flush, "ID3D11DeviceContext::Flush");
    STRINGISE_ENUM_CLASS_NAMED(SetPredication, "ID3D11DeviceContext::SetPredication");
    STRINGISE_ENUM_CLASS_NAMED(SetResourceMinLOD, "ID3D11DeviceContext::SetResourceMinLOD");
    STRINGISE_ENUM_CLASS_NAMED(Begin, "ID3D11DeviceContext::Begin");
    STRINGISE_ENUM_CLASS_NAMED(End, "ID3D11DeviceContext::End");
    STRINGISE_ENUM_CLASS_NAMED(CreateRasterizerState1, "ID3D11Device2::CreateRasterizerState1");
    STRINGISE_ENUM_CLASS_NAMED(CreateBlendState1, "ID3D11Device2::CreateBlendState1");
    STRINGISE_ENUM_CLASS_NAMED(CopySubresourceRegion1,
                               "ID3D11DeviceContext1::CopySubresourceRegion1");
    STRINGISE_ENUM_CLASS_NAMED(UpdateSubresource1, "ID3D11DeviceContext1::UpdateSubresource1");
    STRINGISE_ENUM_CLASS_NAMED(ClearView, "ID3D11DeviceContext1::ClearView");
    STRINGISE_ENUM_CLASS_NAMED(VSSetConstantBuffers1,
                               "ID3D11DeviceContext1::VSSetConstantBuffers1");
    STRINGISE_ENUM_CLASS_NAMED(HSSetConstantBuffers1,
                               "ID3D11DeviceContext1::HSSetConstantBuffers1");
    STRINGISE_ENUM_CLASS_NAMED(DSSetConstantBuffers1,
                               "ID3D11DeviceContext1::DSSetConstantBuffers1");
    STRINGISE_ENUM_CLASS_NAMED(GSSetConstantBuffers1,
                               "ID3D11DeviceContext1::GSSetConstantBuffers1");
    STRINGISE_ENUM_CLASS_NAMED(PSSetConstantBuffers1,
                               "ID3D11DeviceContext1::PSSetConstantBuffers1");
    STRINGISE_ENUM_CLASS_NAMED(CSSetConstantBuffers1,
                               "ID3D11DeviceContext1::CSSetConstantBuffers1");
    STRINGISE_ENUM_CLASS_NAMED(PushMarker, "Push Debug Region");
    STRINGISE_ENUM_CLASS_NAMED(SetMarker, "Set Marker");
    STRINGISE_ENUM_CLASS_NAMED(PopMarker, "Pop Debug Region");
    STRINGISE_ENUM_CLASS_NAMED(CaptureBegin, "Beginning of Capture");
    STRINGISE_ENUM_CLASS_NAMED(CaptureEnd, "End of Capture");
    STRINGISE_ENUM_CLASS_NAMED(SetShaderDebugPath, "SetShaderDebugPath");
    STRINGISE_ENUM_CLASS_NAMED(DiscardResource, "ID3D11DeviceContext1::DiscardResource");
    STRINGISE_ENUM_CLASS_NAMED(DiscardView, "ID3D11DeviceContext1::DiscardView");
    STRINGISE_ENUM_CLASS_NAMED(DiscardView1, "ID3D11DeviceContext1::DiscardView1");
    STRINGISE_ENUM_CLASS_NAMED(CreateRasterizerState2, "ID3D11Device3::CreateRasterizerState2");
    STRINGISE_ENUM_CLASS_NAMED(CreateQuery1, "ID3D11Device3::CreateQuery1");
    STRINGISE_ENUM_CLASS_NAMED(CreateTexture2D1, "ID3D11Device3::CreateTexture2D1");
    STRINGISE_ENUM_CLASS_NAMED(CreateTexture3D1, "ID3D11Device3::CreateTexture3D1");
    STRINGISE_ENUM_CLASS_NAMED(CreateShaderResourceView1,
                               "ID3D11Device3::CreateShaderResourceView1");
    STRINGISE_ENUM_CLASS_NAMED(CreateRenderTargetView1, "ID3D11Device3::CreateRenderTargetView1");
    STRINGISE_ENUM_CLASS_NAMED(CreateUnorderedAccessView1,
                               "ID3D11Device3::CreateUnorderedAccessView1");
    STRINGISE_ENUM_CLASS_NAMED(SwapchainPresent, "IDXGISwapChain::Present");
    STRINGISE_ENUM_CLASS_NAMED(PostExecuteCommandListRestore,
                               "ID3D11DeviceContext::ExecuteCommandList");
    STRINGISE_ENUM_CLASS_NAMED(PostFinishCommandListSet, "ID3D11DeviceContext::FinishCommandList");
    STRINGISE_ENUM_CLASS_NAMED(SwapDeviceContextState,
                               "ID3D11DeviceContext1::SwapDeviceContextState");
    STRINGISE_ENUM_CLASS_NAMED(Max, "Max Chunk");
  }
  END_ENUM_STRINGISE()
}

template <>
std::string DoStringise(const D3D11_BIND_FLAG &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_BIND_FLAG);
  {
    STRINGISE_BITFIELD_VALUE(0)

    STRINGISE_BITFIELD_BIT(D3D11_BIND_VERTEX_BUFFER)
    STRINGISE_BITFIELD_BIT(D3D11_BIND_INDEX_BUFFER)
    STRINGISE_BITFIELD_BIT(D3D11_BIND_CONSTANT_BUFFER)
    STRINGISE_BITFIELD_BIT(D3D11_BIND_SHADER_RESOURCE)
    STRINGISE_BITFIELD_BIT(D3D11_BIND_STREAM_OUTPUT)
    STRINGISE_BITFIELD_BIT(D3D11_BIND_RENDER_TARGET)
    STRINGISE_BITFIELD_BIT(D3D11_BIND_DEPTH_STENCIL)
    STRINGISE_BITFIELD_BIT(D3D11_BIND_UNORDERED_ACCESS)
    STRINGISE_BITFIELD_BIT(D3D11_BIND_DECODER)
    STRINGISE_BITFIELD_BIT(D3D11_BIND_VIDEO_ENCODER)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_CPU_ACCESS_FLAG &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_CPU_ACCESS_FLAG);
  {
    STRINGISE_BITFIELD_VALUE(0)

    STRINGISE_BITFIELD_BIT(D3D11_CPU_ACCESS_READ)
    STRINGISE_BITFIELD_BIT(D3D11_CPU_ACCESS_WRITE)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_RESOURCE_MISC_FLAG &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_RESOURCE_MISC_FLAG);
  {
    STRINGISE_BITFIELD_VALUE(0)

    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_GENERATE_MIPS)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_SHARED)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_TEXTURECUBE)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_RESOURCE_CLAMP)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_GDI_COMPATIBLE)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_RESTRICTED_CONTENT)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE_DRIVER)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_GUARDED)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_TILE_POOL)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_TILED)
    STRINGISE_BITFIELD_BIT(D3D11_RESOURCE_MISC_HW_PROTECTED)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_COLOR_WRITE_ENABLE &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_COLOR_WRITE_ENABLE);
  {
    STRINGISE_BITFIELD_VALUE(0)
    STRINGISE_BITFIELD_VALUE(D3D11_COLOR_WRITE_ENABLE_ALL);

    STRINGISE_BITFIELD_BIT(D3D11_COLOR_WRITE_ENABLE_RED)
    STRINGISE_BITFIELD_BIT(D3D11_COLOR_WRITE_ENABLE_GREEN)
    STRINGISE_BITFIELD_BIT(D3D11_COLOR_WRITE_ENABLE_BLUE)
    STRINGISE_BITFIELD_BIT(D3D11_COLOR_WRITE_ENABLE_ALPHA)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_BUFFER_UAV_FLAG &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_BUFFER_UAV_FLAG);
  {
    STRINGISE_BITFIELD_VALUE(0)

    STRINGISE_BITFIELD_BIT(D3D11_BUFFER_UAV_FLAG_RAW)
    STRINGISE_BITFIELD_BIT(D3D11_BUFFER_UAV_FLAG_APPEND)
    STRINGISE_BITFIELD_BIT(D3D11_BUFFER_UAV_FLAG_COUNTER)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_DSV_FLAG &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_DSV_FLAG);
  {
    STRINGISE_BITFIELD_VALUE(0)

    STRINGISE_BITFIELD_BIT(D3D11_DSV_READ_ONLY_DEPTH)
    STRINGISE_BITFIELD_BIT(D3D11_DSV_READ_ONLY_STENCIL)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_COPY_FLAGS &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_COPY_FLAGS);
  {
    STRINGISE_BITFIELD_VALUE(0)

    STRINGISE_BITFIELD_BIT(D3D11_COPY_NO_OVERWRITE)
    STRINGISE_BITFIELD_BIT(D3D11_COPY_DISCARD)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_MAP_FLAG &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_MAP_FLAG);
  {
    STRINGISE_BITFIELD_VALUE(0)

    STRINGISE_BITFIELD_BIT(D3D11_MAP_FLAG_DO_NOT_WAIT)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_CLEAR_FLAG &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_CLEAR_FLAG);
  {
    STRINGISE_BITFIELD_VALUE(0)

    STRINGISE_BITFIELD_BIT(D3D11_CLEAR_DEPTH)
    STRINGISE_BITFIELD_BIT(D3D11_CLEAR_STENCIL)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_BUFFEREX_SRV_FLAG &el)
{
  BEGIN_BITFIELD_STRINGISE(D3D11_BUFFEREX_SRV_FLAG);
  {
    STRINGISE_BITFIELD_VALUE(0)

    STRINGISE_BITFIELD_BIT(D3D11_BUFFEREX_SRV_FLAG_RAW)
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_TEXTURE_LAYOUT &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_TEXTURE_LAYOUT);
  {
    STRINGISE_ENUM(D3D11_TEXTURE_LAYOUT_UNDEFINED)
    STRINGISE_ENUM(D3D11_TEXTURE_LAYOUT_ROW_MAJOR)
    STRINGISE_ENUM(D3D11_TEXTURE_LAYOUT_64K_STANDARD_SWIZZLE)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_DEPTH_WRITE_MASK &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_DEPTH_WRITE_MASK);
  {
    STRINGISE_ENUM(D3D11_DEPTH_WRITE_MASK_ZERO)
    STRINGISE_ENUM(D3D11_DEPTH_WRITE_MASK_ALL)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_COMPARISON_FUNC &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_COMPARISON_FUNC);
  {
    STRINGISE_ENUM(D3D11_COMPARISON_NEVER);
    STRINGISE_ENUM(D3D11_COMPARISON_LESS);
    STRINGISE_ENUM(D3D11_COMPARISON_EQUAL);
    STRINGISE_ENUM(D3D11_COMPARISON_LESS_EQUAL);
    STRINGISE_ENUM(D3D11_COMPARISON_GREATER);
    STRINGISE_ENUM(D3D11_COMPARISON_NOT_EQUAL);
    STRINGISE_ENUM(D3D11_COMPARISON_GREATER_EQUAL);
    STRINGISE_ENUM(D3D11_COMPARISON_ALWAYS);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_STENCIL_OP &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_STENCIL_OP);
  {
    STRINGISE_ENUM(D3D11_STENCIL_OP_KEEP);
    STRINGISE_ENUM(D3D11_STENCIL_OP_ZERO);
    STRINGISE_ENUM(D3D11_STENCIL_OP_REPLACE);
    STRINGISE_ENUM(D3D11_STENCIL_OP_INCR_SAT);
    STRINGISE_ENUM(D3D11_STENCIL_OP_DECR_SAT);
    STRINGISE_ENUM(D3D11_STENCIL_OP_INVERT);
    STRINGISE_ENUM(D3D11_STENCIL_OP_INCR);
    STRINGISE_ENUM(D3D11_STENCIL_OP_DECR);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_BLEND &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_BLEND);
  {
    STRINGISE_ENUM(D3D11_BLEND_ZERO);
    STRINGISE_ENUM(D3D11_BLEND_ONE);
    STRINGISE_ENUM(D3D11_BLEND_SRC_COLOR);
    STRINGISE_ENUM(D3D11_BLEND_INV_SRC_COLOR);
    STRINGISE_ENUM(D3D11_BLEND_SRC_ALPHA);
    STRINGISE_ENUM(D3D11_BLEND_INV_SRC_ALPHA);
    STRINGISE_ENUM(D3D11_BLEND_DEST_ALPHA);
    STRINGISE_ENUM(D3D11_BLEND_INV_DEST_ALPHA);
    STRINGISE_ENUM(D3D11_BLEND_DEST_COLOR);
    STRINGISE_ENUM(D3D11_BLEND_INV_DEST_COLOR);
    STRINGISE_ENUM(D3D11_BLEND_SRC_ALPHA_SAT);
    STRINGISE_ENUM(D3D11_BLEND_BLEND_FACTOR);
    STRINGISE_ENUM(D3D11_BLEND_INV_BLEND_FACTOR);
    STRINGISE_ENUM(D3D11_BLEND_SRC1_COLOR);
    STRINGISE_ENUM(D3D11_BLEND_INV_SRC1_COLOR);
    STRINGISE_ENUM(D3D11_BLEND_SRC1_ALPHA);
    STRINGISE_ENUM(D3D11_BLEND_INV_SRC1_ALPHA);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_BLEND_OP &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_BLEND_OP);
  {
    STRINGISE_ENUM(D3D11_BLEND_OP_ADD);
    STRINGISE_ENUM(D3D11_BLEND_OP_SUBTRACT);
    STRINGISE_ENUM(D3D11_BLEND_OP_REV_SUBTRACT);
    STRINGISE_ENUM(D3D11_BLEND_OP_MIN);
    STRINGISE_ENUM(D3D11_BLEND_OP_MAX);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_CULL_MODE &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_CULL_MODE);
  {
    STRINGISE_ENUM(D3D11_CULL_NONE);
    STRINGISE_ENUM(D3D11_CULL_FRONT);
    STRINGISE_ENUM(D3D11_CULL_BACK);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_FILL_MODE &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_FILL_MODE);
  {
    STRINGISE_ENUM(D3D11_FILL_WIREFRAME);
    STRINGISE_ENUM(D3D11_FILL_SOLID);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_CONSERVATIVE_RASTERIZATION_MODE &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_CONSERVATIVE_RASTERIZATION_MODE);
  {
    STRINGISE_ENUM(D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON);
    STRINGISE_ENUM(D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_TEXTURE_ADDRESS_MODE &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_TEXTURE_ADDRESS_MODE);
  {
    STRINGISE_ENUM(D3D11_TEXTURE_ADDRESS_WRAP);
    STRINGISE_ENUM(D3D11_TEXTURE_ADDRESS_MIRROR);
    STRINGISE_ENUM(D3D11_TEXTURE_ADDRESS_CLAMP);
    STRINGISE_ENUM(D3D11_TEXTURE_ADDRESS_BORDER);
    STRINGISE_ENUM(D3D11_TEXTURE_ADDRESS_MIRROR_ONCE);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_FILTER &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_FILTER);
  {
    STRINGISE_ENUM(D3D11_FILTER_MIN_MAG_MIP_POINT);
    STRINGISE_ENUM(D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR);
    STRINGISE_ENUM(D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT);
    STRINGISE_ENUM(D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR);
    STRINGISE_ENUM(D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT);
    STRINGISE_ENUM(D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
    STRINGISE_ENUM(D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT);
    STRINGISE_ENUM(D3D11_FILTER_MIN_MAG_MIP_LINEAR);
    STRINGISE_ENUM(D3D11_FILTER_ANISOTROPIC);
    STRINGISE_ENUM(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT);
    STRINGISE_ENUM(D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR);
    STRINGISE_ENUM(D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT);
    STRINGISE_ENUM(D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR);
    STRINGISE_ENUM(D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT);
    STRINGISE_ENUM(D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
    STRINGISE_ENUM(D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT);
    STRINGISE_ENUM(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);
    STRINGISE_ENUM(D3D11_FILTER_COMPARISON_ANISOTROPIC);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_SRV_DIMENSION &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_SRV_DIMENSION);
  {
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_BUFFER)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_TEXTURE1D)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_TEXTURE2D)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_TEXTURE2DMS)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_TEXTURE3D)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_TEXTURECUBE)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_TEXTURECUBEARRAY)
    STRINGISE_ENUM(D3D11_SRV_DIMENSION_BUFFEREX)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_RTV_DIMENSION &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_RTV_DIMENSION);
  {
    STRINGISE_ENUM(D3D11_RTV_DIMENSION_BUFFER)
    STRINGISE_ENUM(D3D11_RTV_DIMENSION_TEXTURE1D)
    STRINGISE_ENUM(D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
    STRINGISE_ENUM(D3D11_RTV_DIMENSION_TEXTURE2D)
    STRINGISE_ENUM(D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
    STRINGISE_ENUM(D3D11_RTV_DIMENSION_TEXTURE2DMS)
    STRINGISE_ENUM(D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
    STRINGISE_ENUM(D3D11_RTV_DIMENSION_TEXTURE3D)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_UAV_DIMENSION &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_UAV_DIMENSION);
  {
    STRINGISE_ENUM(D3D11_UAV_DIMENSION_BUFFER)
    STRINGISE_ENUM(D3D11_UAV_DIMENSION_TEXTURE1D)
    STRINGISE_ENUM(D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
    STRINGISE_ENUM(D3D11_UAV_DIMENSION_TEXTURE2D)
    STRINGISE_ENUM(D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
    STRINGISE_ENUM(D3D11_UAV_DIMENSION_TEXTURE3D)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_DSV_DIMENSION &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_DSV_DIMENSION);
  {
    STRINGISE_ENUM(D3D11_DSV_DIMENSION_TEXTURE1D)
    STRINGISE_ENUM(D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
    STRINGISE_ENUM(D3D11_DSV_DIMENSION_TEXTURE2D)
    STRINGISE_ENUM(D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
    STRINGISE_ENUM(D3D11_DSV_DIMENSION_TEXTURE2DMS)
    STRINGISE_ENUM(D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_CONTEXT_TYPE &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_CONTEXT_TYPE);
  {
    STRINGISE_ENUM(D3D11_CONTEXT_TYPE_ALL)
    STRINGISE_ENUM(D3D11_CONTEXT_TYPE_3D)
    STRINGISE_ENUM(D3D11_CONTEXT_TYPE_COMPUTE)
    STRINGISE_ENUM(D3D11_CONTEXT_TYPE_COPY)
    STRINGISE_ENUM(D3D11_CONTEXT_TYPE_VIDEO)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_QUERY &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_QUERY);
  {
    STRINGISE_ENUM(D3D11_QUERY_EVENT)
    STRINGISE_ENUM(D3D11_QUERY_OCCLUSION)
    STRINGISE_ENUM(D3D11_QUERY_TIMESTAMP)
    STRINGISE_ENUM(D3D11_QUERY_TIMESTAMP_DISJOINT)
    STRINGISE_ENUM(D3D11_QUERY_PIPELINE_STATISTICS)
    STRINGISE_ENUM(D3D11_QUERY_OCCLUSION_PREDICATE)
    STRINGISE_ENUM(D3D11_QUERY_SO_STATISTICS)
    STRINGISE_ENUM(D3D11_QUERY_SO_OVERFLOW_PREDICATE)
    STRINGISE_ENUM(D3D11_QUERY_SO_STATISTICS_STREAM0)
    STRINGISE_ENUM(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0)
    STRINGISE_ENUM(D3D11_QUERY_SO_STATISTICS_STREAM1)
    STRINGISE_ENUM(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1)
    STRINGISE_ENUM(D3D11_QUERY_SO_STATISTICS_STREAM2)
    STRINGISE_ENUM(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2)
    STRINGISE_ENUM(D3D11_QUERY_SO_STATISTICS_STREAM3)
    STRINGISE_ENUM(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_COUNTER &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_COUNTER);
  {
    STRINGISE_ENUM(D3D11_COUNTER_DEVICE_DEPENDENT_0)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_MAP &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_MAP);
  {
    STRINGISE_ENUM(D3D11_MAP_READ)
    STRINGISE_ENUM(D3D11_MAP_WRITE)
    STRINGISE_ENUM(D3D11_MAP_READ_WRITE)
    STRINGISE_ENUM(D3D11_MAP_WRITE_DISCARD)
    STRINGISE_ENUM(D3D11_MAP_WRITE_NO_OVERWRITE)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_PRIMITIVE_TOPOLOGY &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_PRIMITIVE_TOPOLOGY);
  {
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST);
    STRINGISE_ENUM(D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST);
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_USAGE &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_USAGE);
  {
    STRINGISE_ENUM(D3D11_USAGE_DEFAULT)
    STRINGISE_ENUM(D3D11_USAGE_IMMUTABLE)
    STRINGISE_ENUM(D3D11_USAGE_DYNAMIC)
    STRINGISE_ENUM(D3D11_USAGE_STAGING)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_INPUT_CLASSIFICATION &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_INPUT_CLASSIFICATION);
  {
    STRINGISE_ENUM(D3D11_INPUT_PER_VERTEX_DATA)
    STRINGISE_ENUM(D3D11_INPUT_PER_INSTANCE_DATA)
  }
  END_ENUM_STRINGISE();
}

template <>
std::string DoStringise(const D3D11_LOGIC_OP &el)
{
  BEGIN_ENUM_STRINGISE(D3D11_LOGIC_OP);
  {
    STRINGISE_ENUM(D3D11_LOGIC_OP_CLEAR);
    STRINGISE_ENUM(D3D11_LOGIC_OP_SET);
    STRINGISE_ENUM(D3D11_LOGIC_OP_COPY);
    STRINGISE_ENUM(D3D11_LOGIC_OP_COPY_INVERTED);
    STRINGISE_ENUM(D3D11_LOGIC_OP_NOOP);
    STRINGISE_ENUM(D3D11_LOGIC_OP_INVERT);
    STRINGISE_ENUM(D3D11_LOGIC_OP_AND);
    STRINGISE_ENUM(D3D11_LOGIC_OP_NAND);
    STRINGISE_ENUM(D3D11_LOGIC_OP_OR);
    STRINGISE_ENUM(D3D11_LOGIC_OP_NOR);
    STRINGISE_ENUM(D3D11_LOGIC_OP_XOR);
    STRINGISE_ENUM(D3D11_LOGIC_OP_EQUIV);
    STRINGISE_ENUM(D3D11_LOGIC_OP_AND_REVERSE);
    STRINGISE_ENUM(D3D11_LOGIC_OP_AND_INVERTED);
    STRINGISE_ENUM(D3D11_LOGIC_OP_OR_REVERSE);
    STRINGISE_ENUM(D3D11_LOGIC_OP_OR_INVERTED);
  }
  END_ENUM_STRINGISE();
}

#if ENABLED(ENABLE_UNIT_TESTS)
#include "3rdparty/catch/catch.hpp"

TEST_CASE("D3D11 ToStr", "[tostr][d3d]")
{
  CHECK(ToStr(D3D11_LOGIC_OP_SET) == "D3D11_LOGIC_OP_SET");
  CHECK(ToStr(D3D11_LOGIC_OP(9999)) == "D3D11_LOGIC_OP<9999>");

  CHECK(ToStr(D3D11_BIND_FLAG(D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER)) ==
        "D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER");
  CHECK(ToStr(D3D11_BIND_FLAG(D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER | 0x9b000)) ==
        "D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER | D3D11_BIND_FLAG(634880)");
}

#endif
