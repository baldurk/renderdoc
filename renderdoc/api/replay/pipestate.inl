/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2024 Baldur Karlsson
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

#include <ctype.h>

rdcstr PipeState::GetResourceLayout(ResourceId id) const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureVK())
    {
      for(const VKPipe::ImageData &i : m_Vulkan->images)
      {
        if(i.resourceId == id)
          return i.layouts[0].name;
      }
    }

    if(IsCaptureD3D12())
    {
      for(const D3D12Pipe::ResourceData &r : m_D3D12->resourceStates)
      {
        if(r.resourceId == id)
          return r.states[0].name;
      }
    }
  }

  return "Unknown";
}

rdcstr PipeState::Abbrev(ShaderStage stage) const
{
  if(IsCaptureGL() || IsCaptureVK())
  {
    switch(stage)
    {
      case ShaderStage::Vertex: return "VS";
      case ShaderStage::Tess_Control: return "TCS";
      case ShaderStage::Tess_Eval: return "TES";
      case ShaderStage::Geometry: return "GS";
      case ShaderStage::Fragment: return "FS";
      case ShaderStage::Compute: return "CS";
      case ShaderStage::Task: return "TS";
      case ShaderStage::Mesh: return "MS";
      default: break;
    }
  }
  else
  {
    switch(stage)
    {
      case ShaderStage::Vertex: return "VS";
      case ShaderStage::Hull: return "HS";
      case ShaderStage::Domain: return "DS";
      case ShaderStage::Geometry: return "GS";
      case ShaderStage::Pixel: return "PS";
      case ShaderStage::Compute: return "CS";
      case ShaderStage::Amplification: return "AS";
      case ShaderStage::Mesh: return "MS";
      default: break;
    }
  }

  return "?S";
}

rdcstr PipeState::OutputAbbrev() const
{
  if(IsCaptureGL() || IsCaptureVK())
  {
    return "FB";
  }

  return "RT";
}

bool PipeState::IsD3D11Stage(ShaderStage stage) const
{
  switch(stage)
  {
    case ShaderStage::Vertex:
    case ShaderStage::Domain:
    case ShaderStage::Hull:
    case ShaderStage::Geometry:
    case ShaderStage::Pixel:
    case ShaderStage::Compute: return true;
    default: return false;
  }
}

bool PipeState::IsD3D12Stage(ShaderStage stage) const
{
  switch(stage)
  {
    case ShaderStage::Vertex:
    case ShaderStage::Domain:
    case ShaderStage::Hull:
    case ShaderStage::Geometry:
    case ShaderStage::Pixel:
    case ShaderStage::Compute:
    case ShaderStage::Amplification:
    case ShaderStage::Mesh: return true;
    default: return false;
  }
}

bool PipeState::IsGLStage(ShaderStage stage) const
{
  switch(stage)
  {
    case ShaderStage::Vertex:
    case ShaderStage::Domain:
    case ShaderStage::Hull:
    case ShaderStage::Geometry:
    case ShaderStage::Pixel:
    case ShaderStage::Compute: return true;
    default: return false;
  }
}

bool PipeState::IsVulkanStage(ShaderStage stage) const
{
  switch(stage)
  {
    case ShaderStage::Vertex:
    case ShaderStage::Domain:
    case ShaderStage::Hull:
    case ShaderStage::Geometry:
    case ShaderStage::Pixel:
    case ShaderStage::Compute:
    case ShaderStage::Task:
    case ShaderStage::Mesh: return true;
    default: return false;
  }
}

const D3D11Pipe::Shader &PipeState::GetD3D11Stage(ShaderStage stage) const
{
  if(stage == ShaderStage::Vertex)
    return m_D3D11->vertexShader;
  if(stage == ShaderStage::Domain)
    return m_D3D11->domainShader;
  if(stage == ShaderStage::Hull)
    return m_D3D11->hullShader;
  if(stage == ShaderStage::Geometry)
    return m_D3D11->geometryShader;
  if(stage == ShaderStage::Pixel)
    return m_D3D11->pixelShader;
  if(stage == ShaderStage::Compute)
    return m_D3D11->computeShader;

  RENDERDOC_LogMessage(LogType::Error, "PIPE", __FILE__, __LINE__, "Error - invalid stage");
  return m_D3D11->computeShader;
}

const D3D12Pipe::Shader &PipeState::GetD3D12Stage(ShaderStage stage) const
{
  if(stage == ShaderStage::Vertex)
    return m_D3D12->vertexShader;
  if(stage == ShaderStage::Domain)
    return m_D3D12->domainShader;
  if(stage == ShaderStage::Hull)
    return m_D3D12->hullShader;
  if(stage == ShaderStage::Geometry)
    return m_D3D12->geometryShader;
  if(stage == ShaderStage::Pixel)
    return m_D3D12->pixelShader;
  if(stage == ShaderStage::Compute)
    return m_D3D12->computeShader;
  if(stage == ShaderStage::Amplification)
    return m_D3D12->ampShader;
  if(stage == ShaderStage::Mesh)
    return m_D3D12->meshShader;

  RENDERDOC_LogMessage(LogType::Error, "PIPE", __FILE__, __LINE__, "Error - invalid stage");
  return m_D3D12->computeShader;
}

const GLPipe::Shader &PipeState::GetGLStage(ShaderStage stage) const
{
  if(stage == ShaderStage::Vertex)
    return m_GL->vertexShader;
  if(stage == ShaderStage::Tess_Control)
    return m_GL->tessControlShader;
  if(stage == ShaderStage::Tess_Eval)
    return m_GL->tessEvalShader;
  if(stage == ShaderStage::Geometry)
    return m_GL->geometryShader;
  if(stage == ShaderStage::Fragment)
    return m_GL->fragmentShader;
  if(stage == ShaderStage::Compute)
    return m_GL->computeShader;

  RENDERDOC_LogMessage(LogType::Error, "PIPE", __FILE__, __LINE__, "Error - invalid stage");
  return m_GL->computeShader;
}

const VKPipe::Shader &PipeState::GetVulkanStage(ShaderStage stage) const
{
  if(stage == ShaderStage::Vertex)
    return m_Vulkan->vertexShader;
  if(stage == ShaderStage::Tess_Control)
    return m_Vulkan->tessControlShader;
  if(stage == ShaderStage::Tess_Eval)
    return m_Vulkan->tessEvalShader;
  if(stage == ShaderStage::Geometry)
    return m_Vulkan->geometryShader;
  if(stage == ShaderStage::Fragment)
    return m_Vulkan->fragmentShader;
  if(stage == ShaderStage::Compute)
    return m_Vulkan->computeShader;
  if(stage == ShaderStage::Task)
    return m_Vulkan->taskShader;
  if(stage == ShaderStage::Mesh)
    return m_Vulkan->meshShader;

  RENDERDOC_LogMessage(LogType::Error, "PIPE", __FILE__, __LINE__, "Error - invalid stage");
  return m_Vulkan->computeShader;
}

Viewport PipeState::GetViewport(uint32_t index) const
{
  Viewport ret = {};

  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11() && index < m_D3D11->rasterizer.viewports.size())
    {
      return m_D3D11->rasterizer.viewports[index];
    }
    else if(IsCaptureD3D12() && index < m_D3D12->rasterizer.viewports.size())
    {
      return m_D3D12->rasterizer.viewports[index];
    }
    else if(IsCaptureGL() && index < m_GL->rasterizer.viewports.size())
    {
      return m_GL->rasterizer.viewports[index];
    }
    else if(IsCaptureVK() && index < m_Vulkan->viewportScissor.viewportScissors.size())
    {
      return m_Vulkan->viewportScissor.viewportScissors[index].vp;
    }
  }

  return ret;
}

Scissor PipeState::GetScissor(uint32_t index) const
{
  Scissor ret = {};

  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11() && index < m_D3D11->rasterizer.viewports.size())
    {
      return m_D3D11->rasterizer.scissors[index];
    }
    else if(IsCaptureD3D12() && index < m_D3D12->rasterizer.viewports.size())
    {
      return m_D3D12->rasterizer.scissors[index];
    }
    else if(IsCaptureGL() && index < m_GL->rasterizer.viewports.size())
    {
      return m_GL->rasterizer.scissors[index];
    }
    else if(IsCaptureVK() && index < m_Vulkan->viewportScissor.viewportScissors.size())
    {
      return m_Vulkan->viewportScissor.viewportScissors[index].scissor;
    }
  }

  return ret;
}

const ShaderReflection *PipeState::GetShaderReflection(ShaderStage stage) const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D11->vertexShader.reflection;
        case ShaderStage::Domain: return m_D3D11->domainShader.reflection;
        case ShaderStage::Hull: return m_D3D11->hullShader.reflection;
        case ShaderStage::Geometry: return m_D3D11->geometryShader.reflection;
        case ShaderStage::Pixel: return m_D3D11->pixelShader.reflection;
        case ShaderStage::Compute: return m_D3D11->computeShader.reflection;
        default: break;
      }
    }
    else if(IsCaptureD3D12())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D12->vertexShader.reflection;
        case ShaderStage::Domain: return m_D3D12->domainShader.reflection;
        case ShaderStage::Hull: return m_D3D12->hullShader.reflection;
        case ShaderStage::Geometry: return m_D3D12->geometryShader.reflection;
        case ShaderStage::Pixel: return m_D3D12->pixelShader.reflection;
        case ShaderStage::Compute: return m_D3D12->computeShader.reflection;
        case ShaderStage::Amplification: return m_D3D12->ampShader.reflection;
        case ShaderStage::Mesh: return m_D3D12->meshShader.reflection;
        default: break;
      }
    }
    else if(IsCaptureGL())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_GL->vertexShader.reflection;
        case ShaderStage::Tess_Control: return m_GL->tessControlShader.reflection;
        case ShaderStage::Tess_Eval: return m_GL->tessEvalShader.reflection;
        case ShaderStage::Geometry: return m_GL->geometryShader.reflection;
        case ShaderStage::Fragment: return m_GL->fragmentShader.reflection;
        case ShaderStage::Compute: return m_GL->computeShader.reflection;
        default: break;
      }
    }
    else if(IsCaptureVK())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_Vulkan->vertexShader.reflection;
        case ShaderStage::Tess_Control: return m_Vulkan->tessControlShader.reflection;
        case ShaderStage::Tess_Eval: return m_Vulkan->tessEvalShader.reflection;
        case ShaderStage::Geometry: return m_Vulkan->geometryShader.reflection;
        case ShaderStage::Fragment: return m_Vulkan->fragmentShader.reflection;
        case ShaderStage::Compute: return m_Vulkan->computeShader.reflection;
        case ShaderStage::Task: return m_Vulkan->taskShader.reflection;
        case ShaderStage::Mesh: return m_Vulkan->meshShader.reflection;
        default: break;
      }
    }
  }

  return NULL;
}

ResourceId PipeState::GetComputePipelineObject() const
{
  if(IsCaptureLoaded() && IsCaptureVK())
  {
    return m_Vulkan->compute.pipelineResourceId;
  }
  else if(IsCaptureLoaded() && IsCaptureD3D12())
  {
    return m_D3D12->pipelineResourceId;
  }

  return ResourceId();
}

ResourceId PipeState::GetGraphicsPipelineObject() const
{
  if(IsCaptureLoaded() && IsCaptureVK())
  {
    return m_Vulkan->graphics.pipelineResourceId;
  }
  else if(IsCaptureLoaded() && IsCaptureD3D12())
  {
    return m_D3D12->pipelineResourceId;
  }

  return ResourceId();
}

uint32_t PipeState::MultiviewBroadcastCount() const
{
  if(IsCaptureLoaded() && IsCaptureVK())
  {
    return std::max((uint32_t)m_Vulkan->currentPass.renderpass.multiviews.size(), 1U);
  }

  return 1;
}

rdcstr PipeState::GetShaderEntryPoint(ShaderStage stage) const
{
  if(IsCaptureLoaded() && IsCaptureVK())
  {
    switch(stage)
    {
      case ShaderStage::Vertex: return m_Vulkan->vertexShader.entryPoint;
      case ShaderStage::Tess_Control: return m_Vulkan->tessControlShader.entryPoint;
      case ShaderStage::Tess_Eval: return m_Vulkan->tessEvalShader.entryPoint;
      case ShaderStage::Geometry: return m_Vulkan->geometryShader.entryPoint;
      case ShaderStage::Fragment: return m_Vulkan->fragmentShader.entryPoint;
      case ShaderStage::Compute: return m_Vulkan->computeShader.entryPoint;
      case ShaderStage::Task: return m_Vulkan->taskShader.entryPoint;
      case ShaderStage::Mesh: return m_Vulkan->meshShader.entryPoint;
      default: break;
    }
  }

  return "main";
}

ResourceId PipeState::GetShader(ShaderStage stage) const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D11->vertexShader.resourceId;
        case ShaderStage::Domain: return m_D3D11->domainShader.resourceId;
        case ShaderStage::Hull: return m_D3D11->hullShader.resourceId;
        case ShaderStage::Geometry: return m_D3D11->geometryShader.resourceId;
        case ShaderStage::Pixel: return m_D3D11->pixelShader.resourceId;
        case ShaderStage::Compute: return m_D3D11->computeShader.resourceId;
        default: break;
      }
    }
    else if(IsCaptureD3D12())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D12->vertexShader.resourceId;
        case ShaderStage::Domain: return m_D3D12->domainShader.resourceId;
        case ShaderStage::Hull: return m_D3D12->hullShader.resourceId;
        case ShaderStage::Geometry: return m_D3D12->geometryShader.resourceId;
        case ShaderStage::Pixel: return m_D3D12->pixelShader.resourceId;
        case ShaderStage::Compute: return m_D3D12->computeShader.resourceId;
        case ShaderStage::Amplification: return m_D3D12->ampShader.resourceId;
        case ShaderStage::Mesh: return m_D3D12->meshShader.resourceId;
        default: break;
      }
    }
    else if(IsCaptureGL())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_GL->vertexShader.shaderResourceId;
        case ShaderStage::Tess_Control: return m_GL->tessControlShader.shaderResourceId;
        case ShaderStage::Tess_Eval: return m_GL->tessEvalShader.shaderResourceId;
        case ShaderStage::Geometry: return m_GL->geometryShader.shaderResourceId;
        case ShaderStage::Fragment: return m_GL->fragmentShader.shaderResourceId;
        case ShaderStage::Compute: return m_GL->computeShader.shaderResourceId;
        default: break;
      }
    }
    else if(IsCaptureVK())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_Vulkan->vertexShader.resourceId;
        case ShaderStage::Tess_Control: return m_Vulkan->tessControlShader.resourceId;
        case ShaderStage::Tess_Eval: return m_Vulkan->tessEvalShader.resourceId;
        case ShaderStage::Geometry: return m_Vulkan->geometryShader.resourceId;
        case ShaderStage::Fragment: return m_Vulkan->fragmentShader.resourceId;
        case ShaderStage::Compute: return m_Vulkan->computeShader.resourceId;
        case ShaderStage::Task: return m_Vulkan->taskShader.resourceId;
        case ShaderStage::Mesh: return m_Vulkan->meshShader.resourceId;
        default: break;
      }
    }
  }

  return ResourceId();
}

BoundVBuffer PipeState::GetIBuffer() const
{
  BoundVBuffer ret;

  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      ret.resourceId = m_D3D11->inputAssembly.indexBuffer.resourceId;
      ret.byteOffset = m_D3D11->inputAssembly.indexBuffer.byteOffset;
      ret.byteStride = m_D3D11->inputAssembly.indexBuffer.byteStride;
      ret.byteSize = ~0ULL;
    }
    else if(IsCaptureD3D12())
    {
      ret.resourceId = m_D3D12->inputAssembly.indexBuffer.resourceId;
      ret.byteOffset = m_D3D12->inputAssembly.indexBuffer.byteOffset;
      ret.byteStride = m_D3D12->inputAssembly.indexBuffer.byteStride;
      ret.byteSize = m_D3D12->inputAssembly.indexBuffer.byteSize;
    }
    else if(IsCaptureGL())
    {
      ret.resourceId = m_GL->vertexInput.indexBuffer;
      ret.byteOffset = 0;    // GL only has per-draw index offset
      ret.byteStride = m_GL->vertexInput.indexByteStride;
      ret.byteSize = ~0ULL;
    }
    else if(IsCaptureVK())
    {
      ret.resourceId = m_Vulkan->inputAssembly.indexBuffer.resourceId;
      ret.byteOffset = m_Vulkan->inputAssembly.indexBuffer.byteOffset;
      ret.byteStride = m_Vulkan->inputAssembly.indexBuffer.byteStride;
      ret.byteSize = ~0ULL;
    }
  }

  return ret;
}

bool PipeState::IsRestartEnabled() const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      // D3D11 this is always enabled for strips
      const Topology topology = m_D3D11->inputAssembly.topology;
      return topology == Topology::LineStrip || topology == Topology::TriangleStrip ||
             topology == Topology::LineStrip_Adj || topology == Topology::TriangleStrip_Adj ||
             topology == Topology::TriangleFan;
    }
    else if(IsCaptureD3D12())
    {
      return m_D3D12->inputAssembly.indexStripCutValue != 0;
    }
    else if(IsCaptureGL())
    {
      return m_GL->vertexInput.primitiveRestart;
    }
    else if(IsCaptureVK())
    {
      return m_Vulkan->inputAssembly.primitiveRestartEnable;
    }
  }

  return false;
}

uint32_t PipeState::GetRestartIndex() const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11() || IsCaptureVK())
    {
      // D3D11 or Vulkan this is always '-1'
      return UINT32_MAX;
    }
    else if(IsCaptureD3D12())
    {
      return m_D3D12->inputAssembly.indexStripCutValue;
    }
    else if(IsCaptureGL())
    {
      return std::min(UINT32_MAX, m_GL->vertexInput.restartIndex);
    }
  }

  return UINT32_MAX;
}

rdcarray<BoundVBuffer> PipeState::GetVBuffers() const
{
  rdcarray<BoundVBuffer> ret;

  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      ret.resize(m_D3D11->inputAssembly.vertexBuffers.count());
      for(int i = 0; i < m_D3D11->inputAssembly.vertexBuffers.count(); i++)
      {
        ret[i].resourceId = m_D3D11->inputAssembly.vertexBuffers[i].resourceId;
        ret[i].byteOffset = m_D3D11->inputAssembly.vertexBuffers[i].byteOffset;
        ret[i].byteStride = m_D3D11->inputAssembly.vertexBuffers[i].byteStride;
        ret[i].byteSize = ~0ULL;
      }
    }
    else if(IsCaptureD3D12())
    {
      ret.resize(m_D3D12->inputAssembly.vertexBuffers.count());
      for(int i = 0; i < m_D3D12->inputAssembly.vertexBuffers.count(); i++)
      {
        ret[i].resourceId = m_D3D12->inputAssembly.vertexBuffers[i].resourceId;
        ret[i].byteOffset = m_D3D12->inputAssembly.vertexBuffers[i].byteOffset;
        ret[i].byteStride = m_D3D12->inputAssembly.vertexBuffers[i].byteStride;
        ret[i].byteSize = m_D3D12->inputAssembly.vertexBuffers[i].byteSize;
      }
    }
    else if(IsCaptureGL())
    {
      ret.resize(m_GL->vertexInput.vertexBuffers.count());
      for(int i = 0; i < m_GL->vertexInput.vertexBuffers.count(); i++)
      {
        ret[i].resourceId = m_GL->vertexInput.vertexBuffers[i].resourceId;
        ret[i].byteOffset = m_GL->vertexInput.vertexBuffers[i].byteOffset;
        ret[i].byteStride = m_GL->vertexInput.vertexBuffers[i].byteStride;
        ret[i].byteSize = ~0ULL;
      }
    }
    else if(IsCaptureVK())
    {
      ret.resize(m_Vulkan->vertexInput.vertexBuffers.count());
      for(int i = 0; i < m_Vulkan->vertexInput.vertexBuffers.count(); i++)
      {
        ret[i].resourceId = m_Vulkan->vertexInput.vertexBuffers[i].resourceId;
        ret[i].byteOffset = m_Vulkan->vertexInput.vertexBuffers[i].byteOffset;
        ret[i].byteStride = m_Vulkan->vertexInput.vertexBuffers[i].byteStride;
        ret[i].byteSize = m_Vulkan->vertexInput.vertexBuffers[i].byteSize;
      }
    }
  }

  return ret;
}

Topology PipeState::GetPrimitiveTopology() const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      return m_D3D11->inputAssembly.topology;
    }
    else if(IsCaptureD3D12())
    {
      return m_D3D12->inputAssembly.topology;
    }
    else if(IsCaptureVK())
    {
      return m_Vulkan->inputAssembly.topology;
    }
    else if(IsCaptureGL())
    {
      return m_GL->vertexInput.topology;
    }
  }

  return Topology::Unknown;
}

rdcarray<VertexInputAttribute> PipeState::GetVertexInputs() const
{
  auto striequal = [](const rdcstr &a, const rdcstr &b) {
    if(a.length() != b.length())
      return false;

    for(size_t i = 0; i < a.length(); i++)
      if(toupper(a[i]) != toupper(b[i]))
        return false;

    return true;
  };

  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      uint32_t byteOffs[128] = {};

      const rdcarray<D3D11Pipe::Layout> &layouts = m_D3D11->inputAssembly.layouts;

      rdcarray<VertexInputAttribute> ret;
      ret.resize(layouts.size());
      for(int i = 0; i < layouts.count(); i++)
      {
        const rdcstr &semName = layouts[i].semanticName;

        bool needsSemanticIdx = false;
        for(int j = 0; j < layouts.count(); j++)
        {
          if(i != j && striequal(semName, layouts[j].semanticName))
          {
            needsSemanticIdx = true;
            break;
          }
        }

        uint32_t offs = layouts[i].byteOffset;
        if(offs == UINT32_MAX)    // APPEND_ALIGNED
          offs = byteOffs[layouts[i].inputSlot];
        else
          byteOffs[layouts[i].inputSlot] = offs = layouts[i].byteOffset;

        byteOffs[layouts[i].inputSlot] +=
            layouts[i].format.compByteWidth * layouts[i].format.compCount;

        ret[i].name = semName + (needsSemanticIdx ? ToStr(layouts[i].semanticIndex) : "");
        ret[i].vertexBuffer = (int)layouts[i].inputSlot;
        ret[i].byteOffset = offs;
        ret[i].perInstance = layouts[i].perInstance;
        ret[i].instanceRate = (int)layouts[i].instanceDataStepRate;
        ret[i].format = layouts[i].format;
        memset(&ret[i].genericValue, 0, sizeof(PixelValue));
        ret[i].used = false;
        ret[i].genericEnabled = false;

        if(m_D3D11->inputAssembly.bytecode != NULL)
        {
          rdcarray<SigParameter> &sig = m_D3D11->inputAssembly.bytecode->inputSignature;
          for(int ia = 0; ia < sig.count(); ia++)
          {
            if(striequal(semName, sig[ia].semanticName) &&
               sig[ia].semanticIndex == layouts[i].semanticIndex)
            {
              ret[i].used = true;
              break;
            }
          }
        }
      }

      return ret;
    }
    else if(IsCaptureD3D12())
    {
      uint32_t byteOffs[128] = {};

      const rdcarray<D3D12Pipe::Layout> &layouts = m_D3D12->inputAssembly.layouts;

      rdcarray<VertexInputAttribute> ret;
      ret.resize(layouts.size());
      for(int i = 0; i < layouts.count(); i++)
      {
        const rdcstr &semName = layouts[i].semanticName;

        bool needsSemanticIdx = false;
        for(int j = 0; j < layouts.count(); j++)
        {
          if(i != j && striequal(semName, rdcstr(layouts[j].semanticName)))
          {
            needsSemanticIdx = true;
            break;
          }
        }

        uint32_t offs = layouts[i].byteOffset;
        if(offs == UINT32_MAX)    // APPEND_ALIGNED
          offs = byteOffs[layouts[i].inputSlot];
        else
          byteOffs[layouts[i].inputSlot] = offs = layouts[i].byteOffset;

        byteOffs[layouts[i].inputSlot] +=
            layouts[i].format.compByteWidth * layouts[i].format.compCount;

        ret[i].name = semName + (needsSemanticIdx ? ToStr(layouts[i].semanticIndex) : "");
        ret[i].vertexBuffer = (int)layouts[i].inputSlot;
        ret[i].byteOffset = offs;
        ret[i].perInstance = layouts[i].perInstance;
        ret[i].instanceRate = (int)layouts[i].instanceDataStepRate;
        ret[i].format = layouts[i].format;
        memset(&ret[i].genericValue, 0, sizeof(PixelValue));
        ret[i].used = false;
        ret[i].genericEnabled = false;

        if(m_D3D12->vertexShader.reflection != NULL)
        {
          rdcarray<SigParameter> &sig = m_D3D12->vertexShader.reflection->inputSignature;
          for(int ia = 0; ia < sig.count(); ia++)
          {
            if(striequal(semName, sig[ia].semanticName) &&
               sig[ia].semanticIndex == layouts[i].semanticIndex)
            {
              ret[i].used = true;
              break;
            }
          }
        }
      }

      return ret;
    }
    else if(IsCaptureGL())
    {
      const rdcarray<GLPipe::VertexAttribute> &attrs = m_GL->vertexInput.attributes;

      int num = 0;
      for(int i = 0; i < attrs.count(); i++)
      {
        if(attrs[i].boundShaderInput >= 0)
          num++;
      }

      int a = 0;
      rdcarray<VertexInputAttribute> ret;
      ret.resize(attrs.count());
      for(int i = 0; i < attrs.count() && a < num; i++)
      {
        ret[a].name = "attr" + ToStr((uint32_t)i);
        memset(&ret[a].genericValue, 0, sizeof(PixelValue));
        ret[a].vertexBuffer = (int)attrs[i].vertexBufferSlot;
        ret[a].byteOffset = attrs[i].byteOffset;
        ret[a].perInstance =
            m_GL->vertexInput.vertexBuffers[attrs[i].vertexBufferSlot].instanceDivisor > 0;
        ret[a].instanceRate =
            (int)m_GL->vertexInput.vertexBuffers[attrs[i].vertexBufferSlot].instanceDivisor;
        ret[a].format = attrs[i].format;
        ret[a].used = true;
        ret[a].genericEnabled = false;

        if(m_GL->vertexShader.reflection != NULL)
        {
          int attrib = attrs[i].boundShaderInput;

          if(attrib == -1 || attrib >= m_GL->vertexShader.reflection->inputSignature.count())
            continue;

          const SigParameter &sigParam = m_GL->vertexShader.reflection->inputSignature[attrib];

          ret[a].name = sigParam.varName;

          VarType varType = sigParam.varType;

          if(attrs[i].floatCast && (VarTypeCompType(sigParam.varType) == CompType::UInt ||
                                    VarTypeCompType(sigParam.varType) == CompType::SInt))
            ret[a].floatCastWrong = true;

          if(!attrs[i].enabled)
          {
            uint32_t compCount = sigParam.compCount;

            for(uint32_t c = 0; c < compCount; c++)
            {
              if(varType == VarType::Float || varType == VarType::Double)
                ret[a].genericValue.floatValue[c] = attrs[i].genericValue.floatValue[c];
              else if(varType == VarType::UInt || varType == VarType::Bool)
                ret[a].genericValue.uintValue[c] = attrs[i].genericValue.uintValue[c];
              else if(varType == VarType::SInt)
                ret[a].genericValue.intValue[c] = attrs[i].genericValue.intValue[c];
            }

            ret[a].genericEnabled = true;

            ret[a].perInstance = false;
            ret[a].instanceRate = 0;
            ret[a].format.compByteWidth = 4;
            ret[a].format.compCount = (uint8_t)compCount;
            ret[a].format.compType = VarTypeCompType(varType);
            ret[a].format.type = ResourceFormatType::Regular;
          }
        }

        a++;
      }

      return ret;
    }
    else if(IsCaptureVK())
    {
      const rdcarray<VKPipe::VertexAttribute> &attrs = m_Vulkan->vertexInput.attributes;

      rdcarray<VertexInputAttribute> ret;
      ret.resize(attrs.size());
      for(size_t i = 0; i < attrs.size(); i++)
      {
        ret[i].name = "attr" + ToStr((uint32_t)i);
        memset(&ret[i].genericValue, 0, sizeof(PixelValue));
        ret[i].vertexBuffer = (int)attrs[i].binding;
        ret[i].byteOffset = attrs[i].byteOffset;
        ret[i].perInstance = false;
        ret[i].instanceRate = 1;
        if(attrs[i].binding < m_Vulkan->vertexInput.bindings.size())
        {
          ret[i].perInstance = m_Vulkan->vertexInput.bindings[attrs[i].binding].perInstance;
          ret[i].instanceRate = m_Vulkan->vertexInput.bindings[attrs[i].binding].instanceDivisor;
        }
        ret[i].format = attrs[i].format;
        ret[i].used = true;
        ret[i].genericEnabled = false;

        if(m_Vulkan->vertexShader.reflection != NULL)
        {
          const rdcarray<SigParameter> &sig = m_Vulkan->vertexShader.reflection->inputSignature;
          for(const SigParameter &attr : sig)
          {
            if(attr.regIndex == attrs[i].location && attr.systemValue == ShaderBuiltin::Undefined)
            {
              ret[i].name = attr.varName;
              break;
            }
          }
        }
      }

      return ret;
    }
  }

  return rdcarray<VertexInputAttribute>();
}

int32_t PipeState::GetRasterizedStream() const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureGL())
    {
      return 0;
    }
    else if(IsCaptureVK())
    {
      return (int32_t)m_Vulkan->transformFeedback.rasterizedStream;
    }
    else if(IsCaptureD3D11())
    {
      if(m_D3D11->streamOut.rasterizedStream == D3D11Pipe::StreamOut::NoRasterization)
        return -1;

      return (int32_t)m_D3D11->streamOut.rasterizedStream;
    }
    else if(IsCaptureD3D12())
    {
      if(m_D3D12->streamOut.rasterizedStream == D3D12Pipe::StreamOut::NoRasterization)
        return -1;

      return (int32_t)m_D3D12->streamOut.rasterizedStream;
    }
  }

  return 0;
}

rdcarray<UsedDescriptor> PipeState::GetAllUsedDescriptors(bool onlyUsed) const
{
  rdcarray<UsedDescriptor> ret;
  ret.reserve(m_Access.size());

  for(size_t i = 0; i < m_Access.size(); i++)
  {
    if(onlyUsed == false || !m_Access[i].staticallyUnused)
    {
      if(i < m_Descriptors.size())
        ret.push_back({m_Access[i], m_Descriptors[i], m_SamplerDescriptors[i]});
    }
  }

  return ret;
}

void PipeState::ApplyVulkanDynamicOffsets(UsedDescriptor &used) const
{
  if(IsCaptureVK())
  {
    const rdcarray<VKPipe::DescriptorSet> &sets = used.access.stage == ShaderStage::Compute
                                                      ? m_Vulkan->compute.descriptorSets
                                                      : m_Vulkan->graphics.descriptorSets;
    for(const VKPipe::DescriptorSet &set : sets)
    {
      for(const VKPipe::DynamicOffset &offs : set.dynamicOffsets)
      {
        if(set.descriptorSetResourceId == used.access.descriptorStore &&
           offs.descriptorByteOffset == used.access.byteOffset)
        {
          used.descriptor.byteOffset += offs.dynamicBufferByteOffset;
        }
      }
    }
  }
}

UsedDescriptor PipeState::GetConstantBlock(ShaderStage stage, uint32_t index, uint32_t arrayIdx) const
{
  for(size_t i = 0; i < m_Access.size(); i++)
  {
    if(m_Access[i].stage == stage && IsConstantBlockDescriptor(m_Access[i].type) &&
       m_Access[i].index == index && m_Access[i].arrayElement == arrayIdx)
    {
      if(i < m_Descriptors.size())
      {
        UsedDescriptor ret = {m_Access[i], m_Descriptors[i], SamplerDescriptor()};
        ApplyVulkanDynamicOffsets(ret);
        return ret;
      }

      break;
    }
  }

  return UsedDescriptor();
}

rdcarray<UsedDescriptor> PipeState::GetConstantBlocks(ShaderStage stage, bool onlyUsed) const
{
  rdcarray<UsedDescriptor> ret;

  for(size_t i = 0; i < m_Access.size(); i++)
  {
    if(m_Access[i].stage == stage && IsConstantBlockDescriptor(m_Access[i].type) &&
       (onlyUsed == false || !m_Access[i].staticallyUnused))
    {
      if(i < m_Descriptors.size())
      {
        ret.push_back({m_Access[i], m_Descriptors[i], SamplerDescriptor()});
        ApplyVulkanDynamicOffsets(ret.back());
      }
    }
  }

  return ret;
}

rdcarray<UsedDescriptor> PipeState::GetReadOnlyResources(ShaderStage stage, bool onlyUsed) const
{
  rdcarray<UsedDescriptor> ret;

  for(size_t i = 0; i < m_Access.size(); i++)
  {
    if(m_Access[i].stage == stage && IsReadOnlyDescriptor(m_Access[i].type) &&
       (onlyUsed == false || !m_Access[i].staticallyUnused))
    {
      if(i < m_Descriptors.size() && i < m_SamplerDescriptors.size())
      {
        ret.push_back({m_Access[i], m_Descriptors[i], m_SamplerDescriptors[i]});
        ApplyVulkanDynamicOffsets(ret.back());
      }
    }
  }

  return ret;
}

rdcarray<UsedDescriptor> PipeState::GetSamplers(ShaderStage stage, bool onlyUsed) const
{
  rdcarray<UsedDescriptor> ret;

  for(size_t i = 0; i < m_Access.size(); i++)
  {
    if(m_Access[i].stage == stage && IsSamplerDescriptor(m_Access[i].type) &&
       (onlyUsed == false || !m_Access[i].staticallyUnused))
    {
      if(i < m_Descriptors.size())
      {
        ret.push_back({m_Access[i], Descriptor(), m_SamplerDescriptors[i]});
        ApplyVulkanDynamicOffsets(ret.back());
      }
    }
  }

  return ret;
}

rdcarray<UsedDescriptor> PipeState::GetReadWriteResources(ShaderStage stage, bool onlyUsed) const
{
  rdcarray<UsedDescriptor> ret;

  for(size_t i = 0; i < m_Access.size(); i++)
  {
    if(m_Access[i].stage == stage && IsReadWriteDescriptor(m_Access[i].type) &&
       (onlyUsed == false || !m_Access[i].staticallyUnused))
    {
      if(i < m_SamplerDescriptors.size())
        ret.push_back({m_Access[i], m_Descriptors[i], SamplerDescriptor()});
    }
  }

  return ret;
}

Descriptor PipeState::GetDepthTarget() const
{
  Descriptor ret;
  ret.type = DescriptorType::ReadWriteImage;

  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      return m_D3D11->outputMerger.depthTarget;
    }
    else if(IsCaptureD3D12())
    {
      return m_D3D12->outputMerger.depthTarget;
    }
    else if(IsCaptureGL())
    {
      return m_GL->framebuffer.drawFBO.depthAttachment;
    }
    else if(IsCaptureVK())
    {
      const VKPipe::RenderPass &rp = m_Vulkan->currentPass.renderpass;
      const VKPipe::Framebuffer &fb = m_Vulkan->currentPass.framebuffer;

      if(rp.depthstencilAttachment >= 0 && rp.depthstencilAttachment < fb.attachments.count())
      {
        return fb.attachments[rp.depthstencilAttachment];
      }
    }
  }

  return ret;
}

Descriptor PipeState::GetDepthResolveTarget() const
{
  Descriptor ret;
  ret.type = DescriptorType::ReadWriteImage;

  if(IsCaptureLoaded())
  {
    if(IsCaptureVK())
    {
      const VKPipe::RenderPass &rp = m_Vulkan->currentPass.renderpass;
      const VKPipe::Framebuffer &fb = m_Vulkan->currentPass.framebuffer;

      if(rp.depthstencilResolveAttachment >= 0 &&
         rp.depthstencilResolveAttachment < fb.attachments.count())
      {
        return fb.attachments[rp.depthstencilResolveAttachment];
      }
    }
  }

  return ret;
}

rdcarray<Descriptor> PipeState::GetOutputTargets() const
{
  rdcarray<Descriptor> ret;

  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      return m_D3D11->outputMerger.renderTargets;
    }
    else if(IsCaptureD3D12())
    {
      return m_D3D12->outputMerger.renderTargets;
    }
    else if(IsCaptureGL())
    {
      ret.resize(m_GL->framebuffer.drawFBO.drawBuffers.count());
      for(int i = 0; i < m_GL->framebuffer.drawFBO.drawBuffers.count(); i++)
      {
        int db = m_GL->framebuffer.drawFBO.drawBuffers[i];

        if(db >= 0)
        {
          ret[i] = m_GL->framebuffer.drawFBO.colorAttachments[db];
        }
      }
    }
    else if(IsCaptureVK())
    {
      const VKPipe::RenderPass &rp = m_Vulkan->currentPass.renderpass;
      const VKPipe::Framebuffer &fb = m_Vulkan->currentPass.framebuffer;

      int idx = 0;

      ret.resize(rp.colorAttachments.count() + rp.resolveAttachments.count());
      for(int i = 0; i < rp.colorAttachments.count(); i++)
      {
        if(rp.colorAttachments[i] < (uint32_t)fb.attachments.count())
        {
          ret[idx] = fb.attachments[rp.colorAttachments[i]];
        }

        idx++;
      }

      for(int i = 0; i < rp.resolveAttachments.count(); i++)
      {
        if(rp.resolveAttachments[i] < (uint32_t)fb.attachments.count())
        {
          ret[idx] = fb.attachments[rp.resolveAttachments[i]];
        }

        idx++;
      }
    }
  }

  return ret;
}

rdcarray<ColorBlend> PipeState::GetColorBlends() const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      if(m_D3D11->outputMerger.blendState.independentBlend)
        return m_D3D11->outputMerger.blendState.blends;

      rdcarray<ColorBlend> ret;
      ret.fill(m_D3D11->outputMerger.blendState.blends.count(),
               m_D3D11->outputMerger.blendState.blends[0]);
      return ret;
    }
    else if(IsCaptureD3D12())
    {
      if(m_D3D12->outputMerger.blendState.independentBlend)
        return m_D3D12->outputMerger.blendState.blends;

      rdcarray<ColorBlend> ret;
      ret.fill(m_D3D12->outputMerger.blendState.blends.count(),
               m_D3D12->outputMerger.blendState.blends[0]);
      return ret;
    }
    else if(IsCaptureGL())
    {
      return m_GL->framebuffer.blendState.blends;
    }
    else if(IsCaptureVK())
    {
      return m_Vulkan->colorBlend.blends;
    }
  }

  return {};
}

rdcpair<StencilFace, StencilFace> PipeState::GetStencilFaces() const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      return {m_D3D11->outputMerger.depthStencilState.frontFace,
              m_D3D11->outputMerger.depthStencilState.backFace};
    }
    else if(IsCaptureD3D12())
    {
      return {m_D3D12->outputMerger.depthStencilState.frontFace,
              m_D3D12->outputMerger.depthStencilState.backFace};
    }
    else if(IsCaptureGL())
    {
      return {m_GL->stencilState.frontFace, m_GL->stencilState.backFace};
    }
    else if(IsCaptureVK())
    {
      return {m_Vulkan->depthStencil.frontFace, m_Vulkan->depthStencil.backFace};
    }
  }

  return {StencilFace(), StencilFace()};
}

const rdcarray<ShaderMessage> &PipeState::GetShaderMessages() const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureVK())
    {
      return m_Vulkan->shaderMessages;
    }
  }

  static rdcarray<ShaderMessage> empty;

  return empty;
}

bool PipeState::IsIndependentBlendingEnabled() const
{
  if(IsCaptureLoaded())
  {
    if(IsCaptureD3D11())
    {
      return m_D3D11->outputMerger.blendState.independentBlend;
    }
    else if(IsCaptureD3D12())
    {
      return m_D3D12->outputMerger.blendState.independentBlend;
    }
    else if(IsCaptureGL())
    {
      // GL is always implicitly independent blending, just that if you set it in a non-independent
      // way it sets all states at once
      return true;
    }
    else if(IsCaptureVK())
    {
      // similarly for vulkan, there's a physical device feature but it just requires that all
      // states must be identical
      return true;
    }
  }

  return {};
}
