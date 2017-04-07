/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

// do not include any headers here, they must all be in QRDInterface.h
#include "QRDInterface.h"

struct BoundResource
{
  BoundResource()
  {
    Id = ResourceId();
    HighestMip = -1;
    FirstSlice = -1;
    typeHint = CompType::Typeless;
  }
  BoundResource(ResourceId id)
  {
    Id = id;
    HighestMip = -1;
    FirstSlice = -1;
    typeHint = CompType::Typeless;
  }

  ResourceId Id;
  int HighestMip;
  int FirstSlice;
  CompType typeHint;
};

DECLARE_REFLECTION_STRUCT(BoundResource);

struct BoundVBuffer
{
  ResourceId Buffer;
  uint64_t ByteOffset;
  uint32_t ByteStride;
};

DECLARE_REFLECTION_STRUCT(BoundVBuffer);

struct BoundCBuffer
{
  ResourceId Buffer;
  uint64_t ByteOffset = 0;
  uint32_t ByteSize = 0;
};

DECLARE_REFLECTION_STRUCT(BoundCBuffer);

struct VertexInputAttribute
{
  QString Name;
  int VertexBuffer;
  uint32_t RelativeByteOffset;
  bool PerInstance;
  int InstanceRate;
  ResourceFormat Format;
  PixelValue GenericValue;
  bool Used;
};

DECLARE_REFLECTION_STRUCT(VertexInputAttribute);

struct Viewport
{
  float x, y, width, height;
};

DECLARE_REFLECTION_STRUCT(Viewport);

class CommonPipelineState
{
public:
  CommonPipelineState() {}
  void SetStates(APIProperties props, D3D11Pipe::State *d3d11, D3D12Pipe::State *d3d12,
                 GLPipe::State *gl, VKPipe::State *vk)
  {
    m_APIProps = props;
    m_D3D11 = d3d11;
    m_D3D12 = d3d12;
    m_GL = gl;
    m_Vulkan = vk;
  }

  GraphicsAPI DefaultType = GraphicsAPI::D3D11;

  bool LogLoaded()
  {
    return m_D3D11 != NULL || m_D3D12 != NULL || m_GL != NULL || m_Vulkan != NULL;
  }

  bool IsLogD3D11()
  {
    return LogLoaded() && m_APIProps.pipelineType == GraphicsAPI::D3D11 && m_D3D11 != NULL;
  }

  bool IsLogD3D12()
  {
    return LogLoaded() && m_APIProps.pipelineType == GraphicsAPI::D3D12 && m_D3D12 != NULL;
  }

  bool IsLogGL()
  {
    return LogLoaded() && m_APIProps.pipelineType == GraphicsAPI::OpenGL && m_GL != NULL;
  }

  bool IsLogVK()
  {
    return LogLoaded() && m_APIProps.pipelineType == GraphicsAPI::Vulkan && m_Vulkan != NULL;
  }

  // add a bunch of generic properties that people can check to save having to see which pipeline
  // state
  // is valid and look at the appropriate part of it
  bool IsTessellationEnabled()
  {
    if(LogLoaded())
    {
      if(IsLogD3D11())
        return m_D3D11 != NULL && m_D3D11->m_HS.Object != ResourceId();

      if(IsLogD3D12())
        return m_D3D12 != NULL && m_D3D12->m_HS.Object != ResourceId();

      if(IsLogGL())
        return m_GL != NULL && m_GL->m_TES.Object != ResourceId();

      if(IsLogVK())
        return m_Vulkan != NULL && m_Vulkan->m_TES.Object != ResourceId();
    }

    return false;
  }

  bool SupportsResourceArrays() { return LogLoaded() && IsLogVK(); }
  bool SupportsBarriers() { return LogLoaded() && (IsLogVK() || IsLogD3D12()); }
  // whether or not the PostVS data is aligned in the typical fashion
  // ie. vectors not crossing float4 boundaries). APIs that use stream-out
  // or transform feedback have tightly packed data, but APIs that rewrite
  // shaders to dump data might have these alignment requirements
  bool HasAlignedPostVSData() { return LogLoaded() && IsLogVK(); }
  QString GetResourceLayout(ResourceId id);
  QString Abbrev(ShaderStage stage);
  QString OutputAbbrev();

  Viewport GetViewport(int index);
  const ShaderBindpointMapping &GetBindpointMapping(ShaderStage stage);
  const ShaderReflection *GetShaderReflection(ShaderStage stage);
  QString GetShaderEntryPoint(ShaderStage stage);
  ResourceId GetShader(ShaderStage stage);
  QString GetShaderName(ShaderStage stage);
  QString GetShaderExtension();
  QPair<ResourceId, uint64_t> GetIBuffer();
  bool IsStripRestartEnabled();
  uint32_t GetStripRestartIndex(uint32_t indexByteWidth);
  QVector<BoundVBuffer> GetVBuffers();
  QVector<VertexInputAttribute> GetVertexInputs();
  BoundCBuffer GetConstantBuffer(ShaderStage stage, uint32_t BufIdx, uint32_t ArrayIdx);
  QMap<BindpointMap, QVector<BoundResource>> GetReadOnlyResources(ShaderStage stage);
  QMap<BindpointMap, QVector<BoundResource>> GetReadWriteResources(ShaderStage stage);
  BoundResource GetDepthTarget();
  QVector<BoundResource> GetOutputTargets();

private:
  D3D11Pipe::State *m_D3D11 = NULL;
  D3D12Pipe::State *m_D3D12 = NULL;
  GLPipe::State *m_GL = NULL;
  VKPipe::State *m_Vulkan = NULL;
  APIProperties m_APIProps;

  const D3D11Pipe::Shader &GetD3D11Stage(ShaderStage stage);
  const D3D12Pipe::Shader &GetD3D12Stage(ShaderStage stage);
  const GLPipe::Shader &GetGLStage(ShaderStage stage);
  const VKPipe::Shader &GetVulkanStage(ShaderStage stage);
};