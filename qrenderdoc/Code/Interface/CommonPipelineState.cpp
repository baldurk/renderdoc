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

#include <QDebug>
#include "Code/QRDUtils.h"
#include "QRDInterface.h"

QString CommonPipelineState::GetResourceLayout(ResourceId id)
{
  if(LogLoaded())
  {
    if(IsLogVK())
    {
      for(const VKPipe::ImageData &i : m_Vulkan->images)
      {
        if(i.image == id)
          return ToQStr(i.layouts[0].name);
      }
    }

    if(IsLogD3D12())
    {
      for(const D3D12Pipe::ResourceData &r : m_D3D12->Resources)
      {
        if(r.id == id)
          return ToQStr(r.states[0].name);
      }
    }
  }

  return lit("Unknown");
}

QString CommonPipelineState::Abbrev(ShaderStage stage)
{
  if(IsLogD3D11() || (!LogLoaded() && DefaultType == GraphicsAPI::D3D11) || IsLogD3D12() ||
     (!LogLoaded() && DefaultType == GraphicsAPI::D3D12))
  {
    switch(stage)
    {
      case ShaderStage::Vertex: return lit("VS");
      case ShaderStage::Hull: return lit("HS");
      case ShaderStage::Domain: return lit("DS");
      case ShaderStage::Geometry: return lit("GS");
      case ShaderStage::Pixel: return lit("PS");
      case ShaderStage::Compute: return lit("CS");
      default: break;
    }
  }
  else if(IsLogGL() || (!LogLoaded() && DefaultType == GraphicsAPI::OpenGL) || IsLogVK() ||
          (!LogLoaded() && DefaultType == GraphicsAPI::Vulkan))
  {
    switch(stage)
    {
      case ShaderStage::Vertex: return lit("VS");
      case ShaderStage::Tess_Control: return lit("TCS");
      case ShaderStage::Tess_Eval: return lit("TES");
      case ShaderStage::Geometry: return lit("GS");
      case ShaderStage::Fragment: return lit("FS");
      case ShaderStage::Compute: return lit("CS");
      default: break;
    }
  }

  return lit("?S");
}

QString CommonPipelineState::OutputAbbrev()
{
  if(IsLogGL() || (!LogLoaded() && DefaultType == GraphicsAPI::OpenGL) || IsLogVK() ||
     (!LogLoaded() && DefaultType == GraphicsAPI::Vulkan))
  {
    return lit("FB");
  }

  return lit("RT");
}

const D3D11Pipe::Shader &CommonPipelineState::GetD3D11Stage(ShaderStage stage)
{
  if(stage == ShaderStage::Vertex)
    return m_D3D11->m_VS;
  if(stage == ShaderStage::Domain)
    return m_D3D11->m_DS;
  if(stage == ShaderStage::Hull)
    return m_D3D11->m_HS;
  if(stage == ShaderStage::Geometry)
    return m_D3D11->m_GS;
  if(stage == ShaderStage::Pixel)
    return m_D3D11->m_PS;
  if(stage == ShaderStage::Compute)
    return m_D3D11->m_CS;

  qCritical() << "Error - invalid stage " << (int)stage;
  return m_D3D11->m_CS;
}

const D3D12Pipe::Shader &CommonPipelineState::GetD3D12Stage(ShaderStage stage)
{
  if(stage == ShaderStage::Vertex)
    return m_D3D12->m_VS;
  if(stage == ShaderStage::Domain)
    return m_D3D12->m_DS;
  if(stage == ShaderStage::Hull)
    return m_D3D12->m_HS;
  if(stage == ShaderStage::Geometry)
    return m_D3D12->m_GS;
  if(stage == ShaderStage::Pixel)
    return m_D3D12->m_PS;
  if(stage == ShaderStage::Compute)
    return m_D3D12->m_CS;

  qCritical() << "Error - invalid stage " << (int)stage;
  return m_D3D12->m_CS;
}

const GLPipe::Shader &CommonPipelineState::GetGLStage(ShaderStage stage)
{
  if(stage == ShaderStage::Vertex)
    return m_GL->m_VS;
  if(stage == ShaderStage::Tess_Control)
    return m_GL->m_TCS;
  if(stage == ShaderStage::Tess_Eval)
    return m_GL->m_TES;
  if(stage == ShaderStage::Geometry)
    return m_GL->m_GS;
  if(stage == ShaderStage::Fragment)
    return m_GL->m_FS;
  if(stage == ShaderStage::Compute)
    return m_GL->m_CS;

  qCritical() << "Error - invalid stage " << (int)stage;
  return m_GL->m_CS;
}

const VKPipe::Shader &CommonPipelineState::GetVulkanStage(ShaderStage stage)
{
  if(stage == ShaderStage::Vertex)
    return m_Vulkan->m_VS;
  if(stage == ShaderStage::Tess_Control)
    return m_Vulkan->m_TCS;
  if(stage == ShaderStage::Tess_Eval)
    return m_Vulkan->m_TES;
  if(stage == ShaderStage::Geometry)
    return m_Vulkan->m_GS;
  if(stage == ShaderStage::Fragment)
    return m_Vulkan->m_FS;
  if(stage == ShaderStage::Compute)
    return m_Vulkan->m_CS;

  qCritical() << "Error - invalid stage " << (int)stage;
  return m_Vulkan->m_CS;
}

QString CommonPipelineState::GetShaderExtension()
{
  if(IsLogGL() || (!LogLoaded() && DefaultType == GraphicsAPI::OpenGL) || IsLogVK() ||
     (!LogLoaded() && DefaultType == GraphicsAPI::Vulkan))
  {
    return lit("glsl");
  }

  return lit("hlsl");
}

Viewport CommonPipelineState::GetViewport(int index)
{
  Viewport ret;

  // default to a 1x1 viewport just to avoid having to check for 0s all over
  ret.x = ret.y = 0.0f;
  ret.width = ret.height = 1.0f;

  if(LogLoaded())
  {
    if(IsLogD3D11() && index < m_D3D11->m_RS.Viewports.count)
    {
      ret.x = m_D3D11->m_RS.Viewports[index].X;
      ret.y = m_D3D11->m_RS.Viewports[index].Y;
      ret.width = m_D3D11->m_RS.Viewports[index].Width;
      ret.height = m_D3D11->m_RS.Viewports[index].Height;
    }
    else if(IsLogD3D12() && index < m_D3D12->m_RS.Viewports.count)
    {
      ret.x = m_D3D12->m_RS.Viewports[index].X;
      ret.y = m_D3D12->m_RS.Viewports[index].Y;
      ret.width = m_D3D12->m_RS.Viewports[index].Width;
      ret.height = m_D3D12->m_RS.Viewports[index].Height;
    }
    else if(IsLogGL() && index < m_GL->m_Rasterizer.Viewports.count)
    {
      ret.x = m_GL->m_Rasterizer.Viewports[index].Left;
      ret.y = m_GL->m_Rasterizer.Viewports[index].Bottom;
      ret.width = m_GL->m_Rasterizer.Viewports[index].Width;
      ret.height = m_GL->m_Rasterizer.Viewports[index].Height;
    }
    else if(IsLogVK() && index < m_Vulkan->VP.viewportScissors.count)
    {
      ret.x = m_Vulkan->VP.viewportScissors[index].vp.x;
      ret.y = m_Vulkan->VP.viewportScissors[index].vp.y;
      ret.width = m_Vulkan->VP.viewportScissors[index].vp.width;
      ret.height = m_Vulkan->VP.viewportScissors[index].vp.height;
    }
  }

  return ret;
}

const ShaderBindpointMapping &CommonPipelineState::GetBindpointMapping(ShaderStage stage)
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D11->m_VS.BindpointMapping;
        case ShaderStage::Domain: return m_D3D11->m_DS.BindpointMapping;
        case ShaderStage::Hull: return m_D3D11->m_HS.BindpointMapping;
        case ShaderStage::Geometry: return m_D3D11->m_GS.BindpointMapping;
        case ShaderStage::Pixel: return m_D3D11->m_PS.BindpointMapping;
        case ShaderStage::Compute: return m_D3D11->m_CS.BindpointMapping;
        default: break;
      }
    }
    else if(IsLogD3D12())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D12->m_VS.BindpointMapping;
        case ShaderStage::Domain: return m_D3D12->m_DS.BindpointMapping;
        case ShaderStage::Hull: return m_D3D12->m_HS.BindpointMapping;
        case ShaderStage::Geometry: return m_D3D12->m_GS.BindpointMapping;
        case ShaderStage::Pixel: return m_D3D12->m_PS.BindpointMapping;
        case ShaderStage::Compute: return m_D3D12->m_CS.BindpointMapping;
        default: break;
      }
    }
    else if(IsLogGL())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_GL->m_VS.BindpointMapping;
        case ShaderStage::Tess_Control: return m_GL->m_TCS.BindpointMapping;
        case ShaderStage::Tess_Eval: return m_GL->m_TES.BindpointMapping;
        case ShaderStage::Geometry: return m_GL->m_GS.BindpointMapping;
        case ShaderStage::Fragment: return m_GL->m_FS.BindpointMapping;
        case ShaderStage::Compute: return m_GL->m_CS.BindpointMapping;
        default: break;
      }
    }
    else if(IsLogVK())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_Vulkan->m_VS.BindpointMapping;
        case ShaderStage::Tess_Control: return m_Vulkan->m_TCS.BindpointMapping;
        case ShaderStage::Tess_Eval: return m_Vulkan->m_TES.BindpointMapping;
        case ShaderStage::Geometry: return m_Vulkan->m_GS.BindpointMapping;
        case ShaderStage::Fragment: return m_Vulkan->m_FS.BindpointMapping;
        case ShaderStage::Compute: return m_Vulkan->m_CS.BindpointMapping;
        default: break;
      }
    }
  }

  static ShaderBindpointMapping empty;

  return empty;
}

const ShaderReflection *CommonPipelineState::GetShaderReflection(ShaderStage stage)
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D11->m_VS.ShaderDetails;
        case ShaderStage::Domain: return m_D3D11->m_DS.ShaderDetails;
        case ShaderStage::Hull: return m_D3D11->m_HS.ShaderDetails;
        case ShaderStage::Geometry: return m_D3D11->m_GS.ShaderDetails;
        case ShaderStage::Pixel: return m_D3D11->m_PS.ShaderDetails;
        case ShaderStage::Compute: return m_D3D11->m_CS.ShaderDetails;
        default: break;
      }
    }
    else if(IsLogD3D12())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D12->m_VS.ShaderDetails;
        case ShaderStage::Domain: return m_D3D12->m_DS.ShaderDetails;
        case ShaderStage::Hull: return m_D3D12->m_HS.ShaderDetails;
        case ShaderStage::Geometry: return m_D3D12->m_GS.ShaderDetails;
        case ShaderStage::Pixel: return m_D3D12->m_PS.ShaderDetails;
        case ShaderStage::Compute: return m_D3D12->m_CS.ShaderDetails;
        default: break;
      }
    }
    else if(IsLogGL())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_GL->m_VS.ShaderDetails;
        case ShaderStage::Tess_Control: return m_GL->m_TCS.ShaderDetails;
        case ShaderStage::Tess_Eval: return m_GL->m_TES.ShaderDetails;
        case ShaderStage::Geometry: return m_GL->m_GS.ShaderDetails;
        case ShaderStage::Fragment: return m_GL->m_FS.ShaderDetails;
        case ShaderStage::Compute: return m_GL->m_CS.ShaderDetails;
        default: break;
      }
    }
    else if(IsLogVK())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_Vulkan->m_VS.ShaderDetails;
        case ShaderStage::Tess_Control: return m_Vulkan->m_TCS.ShaderDetails;
        case ShaderStage::Tess_Eval: return m_Vulkan->m_TES.ShaderDetails;
        case ShaderStage::Geometry: return m_Vulkan->m_GS.ShaderDetails;
        case ShaderStage::Fragment: return m_Vulkan->m_FS.ShaderDetails;
        case ShaderStage::Compute: return m_Vulkan->m_CS.ShaderDetails;
        default: break;
      }
    }
  }

  return NULL;
}

QString CommonPipelineState::GetShaderEntryPoint(ShaderStage stage)
{
  QString ret;

  if(LogLoaded() && IsLogVK())
  {
    switch(stage)
    {
      case ShaderStage::Vertex: ret = ToQStr(m_Vulkan->m_VS.entryPoint); break;
      case ShaderStage::Tess_Control: ret = ToQStr(m_Vulkan->m_TCS.entryPoint); break;
      case ShaderStage::Tess_Eval: ret = ToQStr(m_Vulkan->m_TES.entryPoint); break;
      case ShaderStage::Geometry: ret = ToQStr(m_Vulkan->m_GS.entryPoint); break;
      case ShaderStage::Fragment: ret = ToQStr(m_Vulkan->m_FS.entryPoint); break;
      case ShaderStage::Compute: ret = ToQStr(m_Vulkan->m_CS.entryPoint); break;
      default: break;
    }
  }

  return ret;
}

ResourceId CommonPipelineState::GetShader(ShaderStage stage)
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D11->m_VS.Object;
        case ShaderStage::Domain: return m_D3D11->m_DS.Object;
        case ShaderStage::Hull: return m_D3D11->m_HS.Object;
        case ShaderStage::Geometry: return m_D3D11->m_GS.Object;
        case ShaderStage::Pixel: return m_D3D11->m_PS.Object;
        case ShaderStage::Compute: return m_D3D11->m_CS.Object;
        default: break;
      }
    }
    else if(IsLogD3D12())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_D3D12->m_VS.Object;
        case ShaderStage::Domain: return m_D3D12->m_DS.Object;
        case ShaderStage::Hull: return m_D3D12->m_HS.Object;
        case ShaderStage::Geometry: return m_D3D12->m_GS.Object;
        case ShaderStage::Pixel: return m_D3D12->m_PS.Object;
        case ShaderStage::Compute: return m_D3D12->m_CS.Object;
        default: break;
      }
    }
    else if(IsLogGL())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_GL->m_VS.Object;
        case ShaderStage::Tess_Control: return m_GL->m_TCS.Object;
        case ShaderStage::Tess_Eval: return m_GL->m_TES.Object;
        case ShaderStage::Geometry: return m_GL->m_GS.Object;
        case ShaderStage::Fragment: return m_GL->m_FS.Object;
        case ShaderStage::Compute: return m_GL->m_CS.Object;
        default: break;
      }
    }
    else if(IsLogVK())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: return m_Vulkan->m_VS.Object;
        case ShaderStage::Tess_Control: return m_Vulkan->m_TCS.Object;
        case ShaderStage::Tess_Eval: return m_Vulkan->m_TES.Object;
        case ShaderStage::Geometry: return m_Vulkan->m_GS.Object;
        case ShaderStage::Fragment: return m_Vulkan->m_FS.Object;
        case ShaderStage::Compute: return m_Vulkan->m_CS.Object;
        default: break;
      }
    }
  }

  return ResourceId();
}

QString CommonPipelineState::GetShaderName(ShaderStage stage)
{
  QString ret;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: ret = ToQStr(m_D3D11->m_VS.name); break;
        case ShaderStage::Domain: ret = ToQStr(m_D3D11->m_DS.name); break;
        case ShaderStage::Hull: ret = ToQStr(m_D3D11->m_HS.name); break;
        case ShaderStage::Geometry: ret = ToQStr(m_D3D11->m_GS.name); break;
        case ShaderStage::Pixel: ret = ToQStr(m_D3D11->m_PS.name); break;
        case ShaderStage::Compute: ret = ToQStr(m_D3D11->m_CS.name); break;
        default: break;
      }
    }
    else if(IsLogD3D12())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: ret = ToQStr(m_D3D12->name) + lit(" VS"); break;
        case ShaderStage::Domain: ret = ToQStr(m_D3D12->name) + lit(" DS"); break;
        case ShaderStage::Hull: ret = ToQStr(m_D3D12->name) + lit(" HS"); break;
        case ShaderStage::Geometry: ret = ToQStr(m_D3D12->name) + lit(" GS"); break;
        case ShaderStage::Pixel: ret = ToQStr(m_D3D12->name) + lit(" PS"); break;
        case ShaderStage::Compute: ret = ToQStr(m_D3D12->name) + lit(" CS"); break;
        default: break;
      }
    }
    else if(IsLogGL())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: ret = ToQStr(m_GL->m_VS.ShaderName); break;
        case ShaderStage::Tess_Control: ret = ToQStr(m_GL->m_TCS.ShaderName); break;
        case ShaderStage::Tess_Eval: ret = ToQStr(m_GL->m_TES.ShaderName); break;
        case ShaderStage::Geometry: ret = ToQStr(m_GL->m_GS.ShaderName); break;
        case ShaderStage::Fragment: ret = ToQStr(m_GL->m_FS.ShaderName); break;
        case ShaderStage::Compute: ret = ToQStr(m_GL->m_CS.ShaderName); break;
        default: break;
      }
    }
    else if(IsLogVK())
    {
      switch(stage)
      {
        case ShaderStage::Vertex: ret = ToQStr(m_Vulkan->m_VS.name); break;
        case ShaderStage::Domain: ret = ToQStr(m_Vulkan->m_TCS.name); break;
        case ShaderStage::Hull: ret = ToQStr(m_Vulkan->m_TES.name); break;
        case ShaderStage::Geometry: ret = ToQStr(m_Vulkan->m_GS.name); break;
        case ShaderStage::Pixel: ret = ToQStr(m_Vulkan->m_FS.name); break;
        case ShaderStage::Compute: ret = ToQStr(m_Vulkan->m_CS.name); break;
        default: break;
      }
    }
  }

  return ret;
}

QPair<ResourceId, uint64_t> CommonPipelineState::GetIBuffer()
{
  ResourceId buf;
  uint64_t ByteOffset = 0;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      buf = m_D3D11->m_IA.ibuffer.Buffer;
      ByteOffset = m_D3D11->m_IA.ibuffer.Offset;
    }
    else if(IsLogD3D12())
    {
      buf = m_D3D12->m_IA.ibuffer.Buffer;
      ByteOffset = m_D3D12->m_IA.ibuffer.Offset;
    }
    else if(IsLogGL())
    {
      buf = m_GL->m_VtxIn.ibuffer;
      ByteOffset = 0;    // GL only has per-draw index offset
    }
    else if(IsLogVK())
    {
      buf = m_Vulkan->IA.ibuffer.buf;
      ByteOffset = m_Vulkan->IA.ibuffer.offs;
    }
  }

  return qMakePair(buf, ByteOffset);
}

bool CommonPipelineState::IsStripRestartEnabled()
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      // D3D11 this is always enabled
      return true;
    }
    else if(IsLogD3D12())
    {
      return m_D3D12->m_IA.indexStripCutValue != 0;
    }
    else if(IsLogGL())
    {
      return m_GL->m_VtxIn.primitiveRestart;
    }
    else if(IsLogVK())
    {
      return m_Vulkan->IA.primitiveRestartEnable;
    }
  }

  return false;
}

uint32_t CommonPipelineState::GetStripRestartIndex()
{
  if(LogLoaded())
  {
    if(IsLogD3D11() || IsLogVK())
    {
      // D3D11 or Vulkan this is always '-1'
      return UINT32_MAX;
    }
    else if(IsLogD3D12())
    {
      return m_D3D12->m_IA.indexStripCutValue;
    }
    else if(IsLogGL())
    {
      return qMin(UINT32_MAX, m_GL->m_VtxIn.restartIndex);
    }
  }

  return UINT32_MAX;
}

QVector<BoundVBuffer> CommonPipelineState::GetVBuffers()
{
  QVector<BoundVBuffer> ret;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      ret.resize(m_D3D11->m_IA.vbuffers.count);
      for(int i = 0; i < m_D3D11->m_IA.vbuffers.count; i++)
      {
        ret[i].Buffer = m_D3D11->m_IA.vbuffers[i].Buffer;
        ret[i].ByteOffset = m_D3D11->m_IA.vbuffers[i].Offset;
        ret[i].ByteStride = m_D3D11->m_IA.vbuffers[i].Stride;
      }
    }
    else if(IsLogD3D12())
    {
      ret.resize(m_D3D12->m_IA.vbuffers.count);
      for(int i = 0; i < m_D3D12->m_IA.vbuffers.count; i++)
      {
        ret[i].Buffer = m_D3D12->m_IA.vbuffers[i].Buffer;
        ret[i].ByteOffset = m_D3D12->m_IA.vbuffers[i].Offset;
        ret[i].ByteStride = m_D3D12->m_IA.vbuffers[i].Stride;
      }
    }
    else if(IsLogGL())
    {
      ret.resize(m_GL->m_VtxIn.vbuffers.count);
      for(int i = 0; i < m_GL->m_VtxIn.vbuffers.count; i++)
      {
        ret[i].Buffer = m_GL->m_VtxIn.vbuffers[i].Buffer;
        ret[i].ByteOffset = m_GL->m_VtxIn.vbuffers[i].Offset;
        ret[i].ByteStride = m_GL->m_VtxIn.vbuffers[i].Stride;
      }
    }
    else if(IsLogVK())
    {
      ret.resize(m_Vulkan->VI.binds.count);
      for(int i = 0; i < m_Vulkan->VI.binds.count; i++)
      {
        ret[i].Buffer =
            i < m_Vulkan->VI.vbuffers.count ? m_Vulkan->VI.vbuffers[i].buffer : ResourceId();
        ret[i].ByteOffset = i < m_Vulkan->VI.vbuffers.count ? m_Vulkan->VI.vbuffers[i].offset : 0;
        ret[i].ByteStride = m_Vulkan->VI.binds[i].bytestride;
      }
    }
  }

  return ret;
}

QVector<VertexInputAttribute> CommonPipelineState::GetVertexInputs()
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      uint32_t byteOffs[128] = {};

      auto &layouts = m_D3D11->m_IA.layouts;

      QVector<VertexInputAttribute> ret(layouts.count);
      for(int i = 0; i < layouts.count; i++)
      {
        QString semName = ToQStr(layouts[i].SemanticName);

        bool needsSemanticIdx = false;
        for(int j = 0; j < layouts.count; j++)
        {
          if(i != j && !semName.compare(ToQStr(layouts[j].SemanticName), Qt::CaseInsensitive))
          {
            needsSemanticIdx = true;
            break;
          }
        }

        uint32_t offs = layouts[i].ByteOffset;
        if(offs == UINT32_MAX)    // APPEND_ALIGNED
          offs = byteOffs[layouts[i].InputSlot];
        else
          byteOffs[layouts[i].InputSlot] = offs = layouts[i].ByteOffset;

        byteOffs[layouts[i].InputSlot] +=
            layouts[i].Format.compByteWidth * layouts[i].Format.compCount;

        ret[i].Name =
            semName + (needsSemanticIdx ? QString::number(layouts[i].SemanticIndex) : QString());
        ret[i].VertexBuffer = (int)layouts[i].InputSlot;
        ret[i].RelativeByteOffset = offs;
        ret[i].PerInstance = layouts[i].PerInstance;
        ret[i].InstanceRate = (int)layouts[i].InstanceDataStepRate;
        ret[i].Format = layouts[i].Format;
        memset(&ret[i].GenericValue, 0, sizeof(PixelValue));
        ret[i].Used = false;

        if(m_D3D11->m_IA.Bytecode != NULL)
        {
          rdctype::array<SigParameter> &sig = m_D3D11->m_IA.Bytecode->InputSig;
          for(int ia = 0; ia < sig.count; ia++)
          {
            if(!semName.compare(ToQStr(sig[ia].semanticName), Qt::CaseInsensitive) &&
               sig[ia].semanticIndex == layouts[i].SemanticIndex)
            {
              ret[i].Used = true;
              break;
            }
          }
        }
      }

      return ret;
    }
    else if(IsLogD3D12())
    {
      uint32_t byteOffs[128] = {};

      auto &layouts = m_D3D12->m_IA.layouts;

      QVector<VertexInputAttribute> ret(layouts.count);
      for(int i = 0; i < layouts.count; i++)
      {
        QString semName = ToQStr(layouts[i].SemanticName);

        bool needsSemanticIdx = false;
        for(int j = 0; j < layouts.count; j++)
        {
          if(i != j && !semName.compare(ToQStr(layouts[j].SemanticName), Qt::CaseInsensitive))
          {
            needsSemanticIdx = true;
            break;
          }
        }

        uint32_t offs = layouts[i].ByteOffset;
        if(offs == UINT32_MAX)    // APPEND_ALIGNED
          offs = byteOffs[layouts[i].InputSlot];
        else
          byteOffs[layouts[i].InputSlot] = offs = layouts[i].ByteOffset;

        byteOffs[layouts[i].InputSlot] +=
            layouts[i].Format.compByteWidth * layouts[i].Format.compCount;

        ret[i].Name =
            semName + (needsSemanticIdx ? QString::number(layouts[i].SemanticIndex) : QString());
        ret[i].VertexBuffer = (int)layouts[i].InputSlot;
        ret[i].RelativeByteOffset = offs;
        ret[i].PerInstance = layouts[i].PerInstance;
        ret[i].InstanceRate = (int)layouts[i].InstanceDataStepRate;
        ret[i].Format = layouts[i].Format;
        memset(&ret[i].GenericValue, 0, sizeof(PixelValue));
        ret[i].Used = false;

        if(m_D3D12->m_VS.ShaderDetails != NULL)
        {
          rdctype::array<SigParameter> &sig = m_D3D12->m_VS.ShaderDetails->InputSig;
          for(int ia = 0; ia < sig.count; ia++)
          {
            if(!semName.compare(ToQStr(sig[ia].semanticName), Qt::CaseInsensitive) &&
               sig[ia].semanticIndex == layouts[i].SemanticIndex)
            {
              ret[i].Used = true;
              break;
            }
          }
        }
      }

      return ret;
    }
    else if(IsLogGL())
    {
      auto &attrs = m_GL->m_VtxIn.attributes;

      int num = 0;
      for(int i = 0; i < attrs.count; i++)
      {
        int attrib = -1;
        if(m_GL->m_VS.ShaderDetails != NULL)
          attrib = m_GL->m_VS.BindpointMapping.InputAttributes[i];
        else
          attrib = i;

        if(attrib >= 0)
          num++;
      }

      int a = 0;
      QVector<VertexInputAttribute> ret(attrs.count);
      for(int i = 0; i < attrs.count && a < num; i++)
      {
        ret[a].Name = lit("attr%1").arg(i);
        memset(&ret[a].GenericValue, 0, sizeof(PixelValue));
        ret[a].VertexBuffer = (int)attrs[i].BufferSlot;
        ret[a].RelativeByteOffset = attrs[i].RelativeOffset;
        ret[a].PerInstance = m_GL->m_VtxIn.vbuffers[attrs[i].BufferSlot].Divisor > 0;
        ret[a].InstanceRate = (int)m_GL->m_VtxIn.vbuffers[attrs[i].BufferSlot].Divisor;
        ret[a].Format = attrs[i].Format;
        ret[a].Used = true;

        if(m_GL->m_VS.ShaderDetails != NULL)
        {
          int attrib = m_GL->m_VS.BindpointMapping.InputAttributes[i];

          if(attrib >= 0 && attrib < m_GL->m_VS.ShaderDetails->InputSig.count)
            ret[a].Name = ToQStr(m_GL->m_VS.ShaderDetails->InputSig[attrib].varName);

          if(attrib == -1)
            continue;

          if(!attrs[i].Enabled)
          {
            uint32_t compCount = m_GL->m_VS.ShaderDetails->InputSig[attrib].compCount;
            CompType compType = m_GL->m_VS.ShaderDetails->InputSig[attrib].compType;

            for(uint32_t c = 0; c < compCount; c++)
            {
              if(compType == CompType::Float)
                ret[a].GenericValue.value_f[c] = attrs[i].GenericValue.value_f[c];
              else if(compType == CompType::UInt)
                ret[a].GenericValue.value_u[c] = attrs[i].GenericValue.value_u[c];
              else if(compType == CompType::SInt)
                ret[a].GenericValue.value_i[c] = attrs[i].GenericValue.value_i[c];
              else if(compType == CompType::UScaled)
                ret[a].GenericValue.value_f[c] = (float)attrs[i].GenericValue.value_u[c];
              else if(compType == CompType::SScaled)
                ret[a].GenericValue.value_f[c] = (float)attrs[i].GenericValue.value_i[c];
            }

            ret[a].PerInstance = false;
            ret[a].InstanceRate = 0;
            ret[a].Format.compByteWidth = 4;
            ret[a].Format.compCount = compCount;
            ret[a].Format.compType = compType;
            ret[a].Format.special = false;
            ret[a].Format.srgbCorrected = false;
          }
        }

        a++;
      }

      return ret;
    }
    else if(IsLogVK())
    {
      auto &attrs = m_Vulkan->VI.attrs;

      int num = 0;
      for(int i = 0; i < attrs.count; i++)
      {
        int attrib = -1;
        if(m_Vulkan->m_VS.ShaderDetails != NULL)
        {
          if(attrs[i].location < (uint32_t)m_Vulkan->m_VS.BindpointMapping.InputAttributes.count)
            attrib = m_Vulkan->m_VS.BindpointMapping.InputAttributes[attrs[i].location];
        }
        else
          attrib = i;

        if(attrib >= 0)
          num++;
      }

      int a = 0;
      QVector<VertexInputAttribute> ret(num);
      for(int i = 0; i < attrs.count && a < num; i++)
      {
        ret[a].Name = lit("attr%1").arg(i);
        memset(&ret[a].GenericValue, 0, sizeof(PixelValue));
        ret[a].VertexBuffer = (int)attrs[i].binding;
        ret[a].RelativeByteOffset = attrs[i].byteoffset;
        ret[a].PerInstance = false;
        if(attrs[i].binding < (uint32_t)m_Vulkan->VI.binds.count)
          ret[a].PerInstance = m_Vulkan->VI.binds[attrs[i].binding].perInstance;
        ret[a].InstanceRate = 1;
        ret[a].Format = attrs[i].format;
        ret[a].Used = true;

        if(m_Vulkan->m_VS.ShaderDetails != NULL)
        {
          int attrib = -1;

          if(attrs[i].location < (uint32_t)m_Vulkan->m_VS.BindpointMapping.InputAttributes.count)
            attrib = m_Vulkan->m_VS.BindpointMapping.InputAttributes[attrs[i].location];

          if(attrib >= 0 && attrib < m_Vulkan->m_VS.ShaderDetails->InputSig.count)
            ret[a].Name = ToQStr(m_Vulkan->m_VS.ShaderDetails->InputSig[attrib].varName);

          if(attrib == -1)
            continue;
        }

        a++;
      }

      return ret;
    }
  }

  return QVector<VertexInputAttribute>();
}

BoundCBuffer CommonPipelineState::GetConstantBuffer(ShaderStage stage, uint32_t BufIdx,
                                                    uint32_t ArrayIdx)
{
  ResourceId buf;
  uint64_t ByteOffset = 0;
  uint64_t ByteSize = 0;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      const D3D11Pipe::Shader &s = GetD3D11Stage(stage);

      if(BufIdx < (uint32_t)s.ConstantBuffers.count)
      {
        buf = s.ConstantBuffers[BufIdx].Buffer;
        ByteOffset = s.ConstantBuffers[BufIdx].VecOffset * 4 * sizeof(float);
        ByteSize = s.ConstantBuffers[BufIdx].VecCount * 4 * sizeof(float);
      }
    }
    else if(IsLogD3D12())
    {
      const D3D12Pipe::Shader &s = GetD3D12Stage(stage);

      if(s.ShaderDetails != NULL && BufIdx < (uint32_t)s.ShaderDetails->ConstantBlocks.count)
      {
        const BindpointMap &bind =
            s.BindpointMapping.ConstantBlocks[s.ShaderDetails->ConstantBlocks[BufIdx].bindPoint];

        if(bind.bindset >= s.Spaces.count || bind.bind >= s.Spaces[bind.bindset].ConstantBuffers.count)
          return BoundCBuffer();

        const D3D12Pipe::CBuffer &descriptor = s.Spaces[bind.bindset].ConstantBuffers[bind.bind];

        buf = descriptor.Buffer;
        ByteOffset = descriptor.Offset;
        ByteSize = descriptor.ByteSize;
      }
    }
    else if(IsLogGL())
    {
      const GLPipe::Shader &s = GetGLStage(stage);

      if(s.ShaderDetails != NULL && BufIdx < (uint32_t)s.ShaderDetails->ConstantBlocks.count)
      {
        if(s.ShaderDetails->ConstantBlocks[BufIdx].bindPoint >= 0)
        {
          int uboIdx =
              s.BindpointMapping.ConstantBlocks[s.ShaderDetails->ConstantBlocks[BufIdx].bindPoint].bind;
          if(uboIdx >= 0 && uboIdx < m_GL->UniformBuffers.count)
          {
            GLPipe::Buffer &b = m_GL->UniformBuffers[uboIdx];

            buf = b.Resource;
            ByteOffset = b.Offset;
            ByteSize = b.Size;
          }
        }
      }
    }
    else if(IsLogVK())
    {
      VKPipe::Pipeline &pipe = stage == ShaderStage::Compute ? m_Vulkan->compute : m_Vulkan->graphics;
      const VKPipe::Shader &s = GetVulkanStage(stage);

      if(s.ShaderDetails != NULL && BufIdx < (uint32_t)s.ShaderDetails->ConstantBlocks.count)
      {
        const BindpointMap &bind =
            s.BindpointMapping.ConstantBlocks[s.ShaderDetails->ConstantBlocks[BufIdx].bindPoint];

        if(s.ShaderDetails->ConstantBlocks[BufIdx].bufferBacked == false)
        {
          BoundCBuffer ret;
          // dummy value, it would be nice to fetch this properly
          ret.ByteSize = 1024;
          return ret;
        }

        auto &descriptorBind = pipe.DescSets[bind.bindset].bindings[bind.bind].binds[ArrayIdx];

        buf = descriptorBind.res;
        ByteOffset = descriptorBind.offset;
        ByteSize = descriptorBind.size;
      }
    }
  }

  BoundCBuffer ret;

  ret.Buffer = buf;
  ret.ByteOffset = ByteOffset;
  ret.ByteSize = ByteSize;

  return ret;
}

QMap<BindpointMap, QVector<BoundResource>> CommonPipelineState::GetReadOnlyResources(ShaderStage stage)
{
  QMap<BindpointMap, QVector<BoundResource>> ret;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      const D3D11Pipe::Shader &s = GetD3D11Stage(stage);

      for(int i = 0; i < s.SRVs.count; i++)
      {
        BindpointMap key(0, i);
        BoundResource val;

        val.Id = s.SRVs[i].Resource;
        val.HighestMip = (int)s.SRVs[i].HighestMip;
        val.FirstSlice = (int)s.SRVs[i].FirstArraySlice;
        val.typeHint = s.SRVs[i].Format.compType;

        ret[key] = {val};
      }

      return ret;
    }
    else if(IsLogD3D12())
    {
      const D3D12Pipe::Shader &s = GetD3D12Stage(stage);

      for(int space = 0; space < s.Spaces.count; space++)
      {
        for(int reg = 0; reg < s.Spaces[space].SRVs.count; reg++)
        {
          const D3D12Pipe::View &bind = s.Spaces[space].SRVs[reg];
          BindpointMap key(space, reg);
          BoundResource val;

          // consider this register to not exist - it's in a gap defined by sparse root signature
          // elements
          if(bind.RootElement == ~0U)
            continue;

          val.Id = bind.Resource;
          val.HighestMip = (int)bind.HighestMip;
          val.FirstSlice = (int)bind.FirstArraySlice;
          val.typeHint = bind.Format.compType;

          ret[key] = {val};
        }
      }

      return ret;
    }
    else if(IsLogGL())
    {
      for(int i = 0; i < m_GL->Textures.count; i++)
      {
        BindpointMap key(0, i);
        BoundResource val;

        val.Id = m_GL->Textures[i].Resource;
        val.HighestMip = (int)m_GL->Textures[i].HighestMip;
        val.FirstSlice = (int)m_GL->Textures[i].FirstSlice;
        val.typeHint = CompType::Typeless;

        ret[key] = {val};
      }

      return ret;
    }
    else if(IsLogVK())
    {
      const auto &descsets =
          stage == ShaderStage::Compute ? m_Vulkan->compute.DescSets : m_Vulkan->graphics.DescSets;

      ShaderStageMask mask = MaskForStage(stage);

      for(int set = 0; set < descsets.count; set++)
      {
        const auto &descset = descsets[set];
        for(int slot = 0; slot < descset.bindings.count; slot++)
        {
          const auto &bind = descset.bindings[slot];
          if((bind.type == BindType::ImageSampler || bind.type == BindType::InputAttachment ||
              bind.type == BindType::ReadOnlyImage || bind.type == BindType::ReadOnlyTBuffer) &&
             (bind.stageFlags & mask) == mask)
          {
            BindpointMap key(set, slot);
            QVector<BoundResource> val(bind.descriptorCount);

            for(uint32_t i = 0; i < bind.descriptorCount; i++)
            {
              val[i].Id = bind.binds[i].res;
              val[i].HighestMip = (int)bind.binds[i].baseMip;
              val[i].FirstSlice = (int)bind.binds[i].baseLayer;
              val[i].typeHint = bind.binds[i].viewfmt.compType;
            }

            ret[key] = val;
          }
        }
      }

      return ret;
    }
  }

  return ret;
}

QMap<BindpointMap, QVector<BoundResource>> CommonPipelineState::GetReadWriteResources(ShaderStage stage)
{
  QMap<BindpointMap, QVector<BoundResource>> ret;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      if(stage == ShaderStage::Compute)
      {
        for(int i = 0; i < m_D3D11->m_CS.UAVs.count; i++)
        {
          BindpointMap key(0, i);
          BoundResource val;

          val.Id = m_D3D11->m_CS.UAVs[i].Resource;
          val.HighestMip = (int)m_D3D11->m_CS.UAVs[i].HighestMip;
          val.FirstSlice = (int)m_D3D11->m_CS.UAVs[i].FirstArraySlice;
          val.typeHint = m_D3D11->m_CS.UAVs[i].Format.compType;

          ret[key] = {val};
        }
      }
      else
      {
        int uavstart = (int)m_D3D11->m_OM.UAVStartSlot;

        // up to UAVStartSlot treat these bindings as empty.
        for(int i = 0; i < uavstart; i++)
        {
          BindpointMap key(0, i);
          BoundResource val;

          ret[key] = {val};
        }

        for(int i = 0; i < m_D3D11->m_OM.UAVs.count - uavstart; i++)
        {
          BindpointMap key(0, i + uavstart);
          BoundResource val;

          val.Id = m_D3D11->m_OM.UAVs[i].Resource;
          val.HighestMip = (int)m_D3D11->m_OM.UAVs[i].HighestMip;
          val.FirstSlice = (int)m_D3D11->m_OM.UAVs[i].FirstArraySlice;
          val.typeHint = m_D3D11->m_OM.UAVs[i].Format.compType;

          ret[key] = {val};
        }
      }
    }
    else if(IsLogD3D12())
    {
      const D3D12Pipe::Shader &s = GetD3D12Stage(stage);

      for(int space = 0; space < s.Spaces.count; space++)
      {
        for(int reg = 0; reg < s.Spaces[space].UAVs.count; reg++)
        {
          const D3D12Pipe::View &bind = s.Spaces[space].UAVs[reg];
          BindpointMap key(space, reg);
          BoundResource val;

          // consider this register to not exist - it's in a gap defined by sparse root signature
          // elements
          if(bind.RootElement == ~0U)
            continue;

          val.Id = bind.Resource;
          val.HighestMip = (int)bind.HighestMip;
          val.FirstSlice = (int)bind.FirstArraySlice;
          val.typeHint = bind.Format.compType;

          ret[key] = {val};
        }
      }
    }
    else if(IsLogGL())
    {
      for(int i = 0; i < m_GL->Images.count; i++)
      {
        BindpointMap key(0, i);
        BoundResource val;

        val.Id = m_GL->Images[i].Resource;
        val.HighestMip = (int)m_GL->Images[i].Level;
        val.FirstSlice = (int)m_GL->Images[i].Layer;
        val.typeHint = m_GL->Images[i].Format.compType;

        ret[key] = {val};
      }
    }
    else if(IsLogVK())
    {
      const auto &descsets =
          stage == ShaderStage::Compute ? m_Vulkan->compute.DescSets : m_Vulkan->graphics.DescSets;

      ShaderStageMask mask = MaskForStage(stage);

      for(int set = 0; set < descsets.count; set++)
      {
        const auto &descset = descsets[set];
        for(int slot = 0; slot < descset.bindings.count; slot++)
        {
          const auto &bind = descset.bindings[slot];
          if((bind.type == BindType::ReadWriteBuffer || bind.type == BindType::ReadWriteImage ||
              bind.type == BindType::ReadWriteTBuffer) &&
             (bind.stageFlags & mask) == mask)
          {
            BindpointMap key(set, slot);
            QVector<BoundResource> val(bind.descriptorCount);

            for(uint32_t i = 0; i < bind.descriptorCount; i++)
            {
              val[i].Id = bind.binds[i].res;
              val[i].HighestMip = (int)bind.binds[i].baseMip;
              val[i].FirstSlice = (int)bind.binds[i].baseLayer;
              val[i].typeHint = bind.binds[i].viewfmt.compType;
            }

            ret[key] = val;
          }
        }
      }
    }
  }

  return ret;
}

BoundResource CommonPipelineState::GetDepthTarget()
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      BoundResource ret;
      ret.Id = m_D3D11->m_OM.DepthTarget.Resource;
      ret.HighestMip = (int)m_D3D11->m_OM.DepthTarget.HighestMip;
      ret.FirstSlice = (int)m_D3D11->m_OM.DepthTarget.FirstArraySlice;
      ret.typeHint = m_D3D11->m_OM.DepthTarget.Format.compType;
      return ret;
    }
    else if(IsLogD3D12())
    {
      BoundResource ret;
      ret.Id = m_D3D12->m_OM.DepthTarget.Resource;
      ret.HighestMip = (int)m_D3D12->m_OM.DepthTarget.HighestMip;
      ret.FirstSlice = (int)m_D3D12->m_OM.DepthTarget.FirstArraySlice;
      ret.typeHint = m_D3D12->m_OM.DepthTarget.Format.compType;
      return ret;
    }
    else if(IsLogGL())
    {
      BoundResource ret;
      ret.Id = m_GL->m_FB.m_DrawFBO.Depth.Obj;
      ret.HighestMip = (int)m_GL->m_FB.m_DrawFBO.Depth.Mip;
      ret.FirstSlice = (int)m_GL->m_FB.m_DrawFBO.Depth.Layer;
      ret.typeHint = CompType::Typeless;
      return ret;
    }
    else if(IsLogVK())
    {
      const auto &rp = m_Vulkan->Pass.renderpass;
      const auto &fb = m_Vulkan->Pass.framebuffer;

      if(rp.depthstencilAttachment >= 0 && rp.depthstencilAttachment < fb.attachments.count)
      {
        BoundResource ret;
        ret.Id = fb.attachments[rp.depthstencilAttachment].img;
        ret.HighestMip = (int)fb.attachments[rp.depthstencilAttachment].baseMip;
        ret.FirstSlice = (int)fb.attachments[rp.depthstencilAttachment].baseLayer;
        ret.typeHint = fb.attachments[rp.depthstencilAttachment].viewfmt.compType;
        return ret;
      }

      return BoundResource();
    }
  }

  return BoundResource();
}

QVector<BoundResource> CommonPipelineState::GetOutputTargets()
{
  QVector<BoundResource> ret;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      ret.resize(m_D3D11->m_OM.RenderTargets.count);
      for(int i = 0; i < m_D3D11->m_OM.RenderTargets.count; i++)
      {
        ret[i].Id = m_D3D11->m_OM.RenderTargets[i].Resource;
        ret[i].HighestMip = (int)m_D3D11->m_OM.RenderTargets[i].HighestMip;
        ret[i].FirstSlice = (int)m_D3D11->m_OM.RenderTargets[i].FirstArraySlice;
        ret[i].typeHint = m_D3D11->m_OM.RenderTargets[i].Format.compType;
      }
    }
    else if(IsLogD3D12())
    {
      ret.resize(m_D3D12->m_OM.RenderTargets.count);
      for(int i = 0; i < m_D3D12->m_OM.RenderTargets.count; i++)
      {
        ret[i].Id = m_D3D12->m_OM.RenderTargets[i].Resource;
        ret[i].HighestMip = (int)m_D3D12->m_OM.RenderTargets[i].HighestMip;
        ret[i].FirstSlice = (int)m_D3D12->m_OM.RenderTargets[i].FirstArraySlice;
        ret[i].typeHint = m_D3D12->m_OM.RenderTargets[i].Format.compType;
      }
    }
    else if(IsLogGL())
    {
      ret.resize(m_GL->m_FB.m_DrawFBO.DrawBuffers.count);
      for(int i = 0; i < m_GL->m_FB.m_DrawFBO.DrawBuffers.count; i++)
      {
        int db = m_GL->m_FB.m_DrawFBO.DrawBuffers[i];

        if(db >= 0)
        {
          ret[i].Id = m_GL->m_FB.m_DrawFBO.Color[db].Obj;
          ret[i].HighestMip = (int)m_GL->m_FB.m_DrawFBO.Color[db].Mip;
          ret[i].FirstSlice = (int)m_GL->m_FB.m_DrawFBO.Color[db].Layer;
          ret[i].typeHint = CompType::Typeless;
        }
      }
    }
    else if(IsLogVK())
    {
      const auto &rp = m_Vulkan->Pass.renderpass;
      const auto &fb = m_Vulkan->Pass.framebuffer;

      int idx = 0;

      ret.resize(rp.colorAttachments.count + rp.resolveAttachments.count);
      for(int i = 0; i < rp.colorAttachments.count; i++)
      {
        if(rp.colorAttachments[i] < (uint32_t)fb.attachments.count)
        {
          ret[idx].Id = fb.attachments[rp.colorAttachments[i]].img;
          ret[idx].HighestMip = (int)fb.attachments[rp.colorAttachments[i]].baseMip;
          ret[idx].FirstSlice = (int)fb.attachments[rp.colorAttachments[i]].baseLayer;
          ret[idx].typeHint = fb.attachments[rp.colorAttachments[i]].viewfmt.compType;
        }

        idx++;
      }

      for(int i = 0; i < rp.resolveAttachments.count; i++)
      {
        if(rp.resolveAttachments[i] < (uint32_t)fb.attachments.count)
        {
          ret[idx].Id = fb.attachments[rp.resolveAttachments[i]].img;
          ret[idx].HighestMip = (int)fb.attachments[rp.resolveAttachments[i]].baseMip;
          ret[idx].FirstSlice = (int)fb.attachments[rp.resolveAttachments[i]].baseLayer;
          ret[idx].typeHint = fb.attachments[rp.resolveAttachments[i]].viewfmt.compType;
        }

        idx++;
      }
    }
  }

  return ret;
}
