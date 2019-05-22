/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "d3d11_pipestate.h"
#include "d3d12_pipestate.h"
#include "gl_pipestate.h"
#include "vk_pipestate.h"

DOCUMENT(R"(An API-agnostic view of the common aspects of the pipeline state. This allows simple
access to e.g. find out the bound resources or vertex buffers, or certain pipeline state which is
available on all APIs.

For more detailed or precise information without abstraction, access the specific pipeline state
for the capture that's open.
)");
struct PipeState
{
public:
  PipeState() = default;
  // disallow copy of this object
  PipeState(const PipeState &) = delete;

#if defined(RENDERDOC_EXPORTS)
  // we initialise this internally only
  void SetStates(APIProperties props, const D3D11Pipe::State *d3d11, const D3D12Pipe::State *d3d12,
                 const GLPipe::State *gl, const VKPipe::State *vk)
  {
    m_PipelineType = props.pipelineType;
    m_D3D11 = d3d11;
    m_D3D12 = d3d12;
    m_GL = gl;
    m_Vulkan = vk;
  }
#endif

  DOCUMENT(R"(Determines whether or not a capture is currently loaded.

:return: A boolean indicating if a capture is currently loaded.
:rtype: ``bool``
)");
  bool IsCaptureLoaded() const
  {
    return m_D3D11 != NULL || m_D3D12 != NULL || m_GL != NULL || m_Vulkan != NULL;
  }

  DOCUMENT(R"(Determines whether or not a D3D11 capture is currently loaded.

:return: A boolean indicating if a D3D11 capture is currently loaded.
:rtype: ``bool``
)");
  bool IsCaptureD3D11() const
  {
    return IsCaptureLoaded() && m_PipelineType == GraphicsAPI::D3D11 && m_D3D11 != NULL;
  }

  DOCUMENT(R"(Determines whether or not a D3D12 capture is currently loaded.

:return: A boolean indicating if a D3D12 capture is currently loaded.
:rtype: ``bool``
)");
  bool IsCaptureD3D12() const
  {
    return IsCaptureLoaded() && m_PipelineType == GraphicsAPI::D3D12 && m_D3D12 != NULL;
  }

  DOCUMENT(R"(Determines whether or not an OpenGL capture is currently loaded.

:return: A boolean indicating if an OpenGL capture is currently loaded.
:rtype: ``bool``
)");
  bool IsCaptureGL() const
  {
    return IsCaptureLoaded() && m_PipelineType == GraphicsAPI::OpenGL && m_GL != NULL;
  }

  DOCUMENT(R"(Determines whether or not a Vulkan capture is currently loaded.

:return: A boolean indicating if a Vulkan capture is currently loaded.
:rtype: ``bool``
)");
  bool IsCaptureVK() const
  {
    return IsCaptureLoaded() && m_PipelineType == GraphicsAPI::Vulkan && m_Vulkan != NULL;
  }

  // add a bunch of generic properties that people can check to save having to see which pipeline
  // state is valid and look at the appropriate part of it
  DOCUMENT(R"(Determines whether or not tessellation is currently enabled.

:return: A boolean indicating if tessellation is currently enabled.
:rtype: ``bool``
)");
  bool IsTessellationEnabled() const
  {
    if(IsCaptureLoaded())
    {
      if(IsCaptureD3D11())
        return m_D3D11 != NULL && m_D3D11->hullShader.resourceId != ResourceId();

      if(IsCaptureD3D12())
        return m_D3D12 != NULL && m_D3D12->hullShader.resourceId != ResourceId();

      if(IsCaptureGL())
        return m_GL != NULL && m_GL->tessEvalShader.shaderResourceId != ResourceId();

      if(IsCaptureVK())
        return m_Vulkan != NULL && m_Vulkan->tessEvalShader.resourceId != ResourceId();
    }

    return false;
  }

  DOCUMENT("Returns the number of views being broadcast to simultaneously during rendering.");
  uint32_t MultiviewBroadcastCount() const;

  DOCUMENT(R"(Determines whether or not the current capture supports binding arrays of resources.

:return: A boolean indicating if binding arrays of resources is supported.
:rtype: ``bool``
)");
  bool SupportsResourceArrays() const { return IsCaptureLoaded() && IsCaptureVK(); }
  DOCUMENT(R"(Determines whether or not the current capture uses explicit barriers.

:return: A boolean indicating if explicit barriers are used.
:rtype: ``bool``
)");
  bool SupportsBarriers() const { return IsCaptureLoaded() && (IsCaptureVK() || IsCaptureD3D12()); }
  DOCUMENT(R"(Determines whether or not the PostVS data is aligned in the typical fashion (ie.
vectors not crossing ``float4`` boundaries). APIs that use stream-out or transform feedback have
tightly packed data, but APIs that rewrite shaders to dump data might have these alignment
requirements.

:param MeshDataStage stage: The mesh data stage for the output data.
:return: A boolean indicating if post-VS data is aligned.
:rtype: ``bool``
)");
  bool HasAlignedPostVSData(MeshDataStage stage) const
  {
    return IsCaptureLoaded() && IsCaptureVK() && stage == MeshDataStage::VSOut;
  }
  DOCUMENT(R"(For APIs that have explicit barriers, retrieves the current layout of a resource.

:return: The name of the current resource layout.
:rtype: ``str``
)");
  rdcstr GetResourceLayout(ResourceId id) const;

  DOCUMENT(R"(Retrieves a suitable two or three letter abbreviation of the given shader stage.

:param ShaderStage stage: The shader stage to abbreviate.
:return: The abbreviation of the stage.
:rtype: ``str``
)");
  rdcstr Abbrev(ShaderStage stage) const;

  DOCUMENT(R"(Retrieves a suitable two or three letter abbreviation of the output stage. Typically
'OM' or 'FBO'.

:return: The abbreviation of the output stage.
:rtype: ``str``
)");
  rdcstr OutputAbbrev() const;

  DOCUMENT(R"(Retrieves the viewport for a given index.

:param int index: The index to retrieve.
:return: The viewport for the given index.
:rtype: Viewport
)");
  Viewport GetViewport(int index) const;

  DOCUMENT(R"(Retrieves the scissor region for a given index.

:param int index: The index to retrieve.
:return: The scissor region for the given index.
:rtype: Scissor
)");
  Scissor GetScissor(int index) const;

  DOCUMENT(R"(Retrieves the current bindpoint mapping for a shader stage.

This returns an empty bindpoint mapping if no shader is bound.

:param ShaderStage stage: The shader stage to fetch.
:return: The bindpoint mapping for the given shader.
:rtype: ShaderBindpointMapping
)");
  const ShaderBindpointMapping &GetBindpointMapping(ShaderStage stage) const;

  DOCUMENT(R"(Retrieves the shader reflection information for a shader stage.

This returns ``None`` if no shader is bound.

:param ShaderStage stage: The shader stage to fetch.
:return: The reflection data for the given shader.
:rtype: :class:`ShaderBindpointMapping` or ``None``
)");
  const ShaderReflection *GetShaderReflection(ShaderStage stage) const;

  DOCUMENT(R"(Retrieves the the compute pipeline state object, if applicable.

:return: The object ID for the given pipeline object.
:rtype: ResourceId
)");
  ResourceId GetComputePipelineObject() const;

  DOCUMENT(R"(Retrieves the the graphics pipeline state object, if applicable.

:return: The object ID for the given pipeline object.
:rtype: ResourceId
)");
  ResourceId GetGraphicsPipelineObject() const;

  DOCUMENT(R"(Retrieves the name of the entry point function for a shader stage.

For some APIs that don't distinguish by entry point, this may be empty.

:param ShaderStage stage: The shader stage to fetch.
:return: The entry point name for the given shader.
:rtype: ``str``
)");
  rdcstr GetShaderEntryPoint(ShaderStage stage) const;

  DOCUMENT(R"(Retrieves the object ID of the shader bound at a shader stage.

:param ShaderStage stage: The shader stage to fetch.
:return: The object ID for the given shader.
:rtype: ResourceId
)");
  ResourceId GetShader(ShaderStage stage) const;

  DOCUMENT(R"(Retrieves the current index buffer binding.

:return: A :class:`BoundVBuffer` with the index buffer details. The stride is always 0.
:rtype: ``BoundVBuffer``
)");
  BoundVBuffer GetIBuffer() const;

  DOCUMENT(R"(Determines whether or not primitive restart is enabled.

:return: A boolean indicating if primitive restart is enabled.
:rtype: ``bool``
)");
  bool IsStripRestartEnabled() const;

  DOCUMENT(R"(Retrieves the primitive restart index.

:param int indexByteWidth: The width in bytes of the indices.
:return: The index value that represents a strip restart not a real index.
:rtype: ``int``
)");
  uint32_t GetStripRestartIndex() const;

  DOCUMENT(R"(Retrieves the currently bound vertex buffers.

:return: The list of bound vertex buffers.
:rtype: ``list`` of :class:`BoundVBuffer`.
)");
  rdcarray<BoundVBuffer> GetVBuffers() const;

  DOCUMENT(R"(Retrieves the currently specified vertex attributes.

:return: The list of current vertex attributes.
:rtype: ``list`` of :class:`VertexInputAttribute`.
)");
  rdcarray<VertexInputAttribute> GetVertexInputs() const;

  DOCUMENT(R"(Retrieves the constant buffer at a given binding.

:param ShaderStage stage: The shader stage to fetch from.
:param int BufIdx: The index in the shader's ConstantBlocks array to look up.
:param int ArrayIdx: For APIs that support arrays of constant buffers in a single binding, the index
  in that array to look up.
:return: The constant buffer at the specified binding.
:rtype: BoundCBuffer
)");
  BoundCBuffer GetConstantBuffer(ShaderStage stage, uint32_t BufIdx, uint32_t ArrayIdx) const;

  DOCUMENT(R"(Retrieves the read-only resources bound to a particular shader stage.

:param ShaderStage stage: The shader stage to fetch from.
:return: The currently bound read-only resoruces.
:rtype: ``list`` of :class:`BoundResourceArray` entries
)");
  rdcarray<BoundResourceArray> GetReadOnlyResources(ShaderStage stage) const;

  DOCUMENT(R"(Retrieves the read/write resources bound to a particular shader stage.

:param ShaderStage stage: The shader stage to fetch from.
:return: The currently bound read/write resoruces.
:rtype: ``list`` of :class:`BoundResourceArray` entries
)");
  rdcarray<BoundResourceArray> GetReadWriteResources(ShaderStage stage) const;

  DOCUMENT(R"(Retrieves the read/write resources bound to the depth-stencil output.

:return: The currently bound depth-stencil resource.
:rtype: BoundResource
)");
  BoundResource GetDepthTarget() const;

  DOCUMENT(R"(Retrieves the resources bound to the color outputs.

:return: The currently bound output targets.
:rtype: ``list`` of :class:`BoundResource`.
)");
  rdcarray<BoundResource> GetOutputTargets() const;

private:
  const D3D11Pipe::State *m_D3D11 = NULL;
  const D3D12Pipe::State *m_D3D12 = NULL;
  const GLPipe::State *m_GL = NULL;
  const VKPipe::State *m_Vulkan = NULL;
  GraphicsAPI m_PipelineType = GraphicsAPI::D3D11;

  // helper functions
  const D3D11Pipe::Shader &GetD3D11Stage(ShaderStage stage) const;
  const D3D12Pipe::Shader &GetD3D12Stage(ShaderStage stage) const;
  const GLPipe::Shader &GetGLStage(ShaderStage stage) const;
  const VKPipe::Shader &GetVulkanStage(ShaderStage stage) const;
};