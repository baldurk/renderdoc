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

#include "CommonPipelineState.h"
#include "QRDUtils.h"

QString CommonPipelineState::GetImageLayout(ResourceId id)
{
  if(LogLoaded())
  {
    if(IsLogVK())
    {
      for(const VulkanPipelineState::ImageData &i : m_Vulkan->images)
      {
        if(i.image == id)
          return ToQStr(i.layouts[0].name);
      }
    }

    if(IsLogD3D12())
    {
      for(const D3D12PipelineState::ResourceData &r : m_D3D12->Resources)
      {
        if(r.id == id)
          return ToQStr(r.states[0].name);
      }
    }
  }

  return "Unknown";
}

QString CommonPipelineState::Abbrev(ShaderStageType stage)
{
  if(IsLogD3D11() || (!LogLoaded() && DefaultType == eGraphicsAPI_D3D11) || IsLogD3D12() ||
     (!LogLoaded() && DefaultType == eGraphicsAPI_D3D12))
  {
    switch(stage)
    {
      case eShaderStage_Vertex: return "VS";
      case eShaderStage_Hull: return "HS";
      case eShaderStage_Domain: return "DS";
      case eShaderStage_Geometry: return "GS";
      case eShaderStage_Pixel: return "PS";
      case eShaderStage_Compute: return "CS";
      default: break;
    }
  }
  else if(IsLogGL() || (!LogLoaded() && DefaultType == eGraphicsAPI_OpenGL) || IsLogVK() ||
          (!LogLoaded() && DefaultType == eGraphicsAPI_Vulkan))
  {
    switch(stage)
    {
      case eShaderStage_Vertex: return "VS";
      case eShaderStage_Tess_Control: return "TCS";
      case eShaderStage_Tess_Eval: return "TES";
      case eShaderStage_Geometry: return "GS";
      case eShaderStage_Fragment: return "FS";
      case eShaderStage_Compute: return "CS";
      default: break;
    }
  }

  return "?S";
}

QString CommonPipelineState::OutputAbbrev()
{
  if(IsLogGL() || (!LogLoaded() && DefaultType == eGraphicsAPI_OpenGL) || IsLogVK() ||
     (!LogLoaded() && DefaultType == eGraphicsAPI_Vulkan))
  {
    return "FB";
  }

  return "RT";
}

QString CommonPipelineState::GetShaderExtension()
{
  if(IsLogGL() || (!LogLoaded() && DefaultType == eGraphicsAPI_OpenGL) || IsLogVK() ||
     (!LogLoaded() && DefaultType == eGraphicsAPI_Vulkan))
  {
    return "glsl";
  }

  return "hlsl";
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
      ret.x = m_D3D11->m_RS.Viewports[index].TopLeft[0];
      ret.y = m_D3D11->m_RS.Viewports[index].TopLeft[1];
      ret.width = m_D3D11->m_RS.Viewports[index].Width;
      ret.height = m_D3D11->m_RS.Viewports[index].Height;
    }
    else if(IsLogD3D12() && index < m_D3D12->m_RS.Viewports.count)
    {
      ret.x = m_D3D12->m_RS.Viewports[index].TopLeft[0];
      ret.y = m_D3D12->m_RS.Viewports[index].TopLeft[1];
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

const ShaderBindpointMapping &CommonPipelineState::GetBindpointMapping(ShaderStageType stage)
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_D3D11->m_VS.BindpointMapping;
        case eShaderStage_Domain: return m_D3D11->m_DS.BindpointMapping;
        case eShaderStage_Hull: return m_D3D11->m_HS.BindpointMapping;
        case eShaderStage_Geometry: return m_D3D11->m_GS.BindpointMapping;
        case eShaderStage_Pixel: return m_D3D11->m_PS.BindpointMapping;
        case eShaderStage_Compute: return m_D3D11->m_CS.BindpointMapping;
        default: break;
      }
    }
    else if(IsLogD3D12())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_D3D12->m_VS.BindpointMapping;
        case eShaderStage_Domain: return m_D3D12->m_DS.BindpointMapping;
        case eShaderStage_Hull: return m_D3D12->m_HS.BindpointMapping;
        case eShaderStage_Geometry: return m_D3D12->m_GS.BindpointMapping;
        case eShaderStage_Pixel: return m_D3D12->m_PS.BindpointMapping;
        case eShaderStage_Compute: return m_D3D12->m_CS.BindpointMapping;
        default: break;
      }
    }
    else if(IsLogGL())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_GL->m_VS.BindpointMapping;
        case eShaderStage_Tess_Control: return m_GL->m_TCS.BindpointMapping;
        case eShaderStage_Tess_Eval: return m_GL->m_TES.BindpointMapping;
        case eShaderStage_Geometry: return m_GL->m_GS.BindpointMapping;
        case eShaderStage_Fragment: return m_GL->m_FS.BindpointMapping;
        case eShaderStage_Compute: return m_GL->m_CS.BindpointMapping;
        default: break;
      }
    }
    else if(IsLogVK())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_Vulkan->m_VS.BindpointMapping;
        case eShaderStage_Tess_Control: return m_Vulkan->m_TCS.BindpointMapping;
        case eShaderStage_Tess_Eval: return m_Vulkan->m_TES.BindpointMapping;
        case eShaderStage_Geometry: return m_Vulkan->m_GS.BindpointMapping;
        case eShaderStage_Fragment: return m_Vulkan->m_FS.BindpointMapping;
        case eShaderStage_Compute: return m_Vulkan->m_CS.BindpointMapping;
        default: break;
      }
    }
  }

  static ShaderBindpointMapping empty;

  return empty;
}

const ShaderReflection *CommonPipelineState::GetShaderReflection(ShaderStageType stage)
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_D3D11->m_VS.ShaderDetails;
        case eShaderStage_Domain: return m_D3D11->m_DS.ShaderDetails;
        case eShaderStage_Hull: return m_D3D11->m_HS.ShaderDetails;
        case eShaderStage_Geometry: return m_D3D11->m_GS.ShaderDetails;
        case eShaderStage_Pixel: return m_D3D11->m_PS.ShaderDetails;
        case eShaderStage_Compute: return m_D3D11->m_CS.ShaderDetails;
        default: break;
      }
    }
    else if(IsLogD3D12())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_D3D12->m_VS.ShaderDetails;
        case eShaderStage_Domain: return m_D3D12->m_DS.ShaderDetails;
        case eShaderStage_Hull: return m_D3D12->m_HS.ShaderDetails;
        case eShaderStage_Geometry: return m_D3D12->m_GS.ShaderDetails;
        case eShaderStage_Pixel: return m_D3D12->m_PS.ShaderDetails;
        case eShaderStage_Compute: return m_D3D12->m_CS.ShaderDetails;
        default: break;
      }
    }
    else if(IsLogGL())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_GL->m_VS.ShaderDetails;
        case eShaderStage_Tess_Control: return m_GL->m_TCS.ShaderDetails;
        case eShaderStage_Tess_Eval: return m_GL->m_TES.ShaderDetails;
        case eShaderStage_Geometry: return m_GL->m_GS.ShaderDetails;
        case eShaderStage_Fragment: return m_GL->m_FS.ShaderDetails;
        case eShaderStage_Compute: return m_GL->m_CS.ShaderDetails;
        default: break;
      }
    }
    else if(IsLogVK())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_Vulkan->m_VS.ShaderDetails;
        case eShaderStage_Tess_Control: return m_Vulkan->m_TCS.ShaderDetails;
        case eShaderStage_Tess_Eval: return m_Vulkan->m_TES.ShaderDetails;
        case eShaderStage_Geometry: return m_Vulkan->m_GS.ShaderDetails;
        case eShaderStage_Fragment: return m_Vulkan->m_FS.ShaderDetails;
        case eShaderStage_Compute: return m_Vulkan->m_CS.ShaderDetails;
        default: break;
      }
    }
  }

  return NULL;
}

QString CommonPipelineState::GetShaderEntryPoint(ShaderStageType stage)
{
  QString ret;

  if(LogLoaded() && IsLogVK())
  {
    switch(stage)
    {
      case eShaderStage_Vertex: ret = m_Vulkan->m_VS.entryPoint; break;
      case eShaderStage_Tess_Control: ret = m_Vulkan->m_TCS.entryPoint; break;
      case eShaderStage_Tess_Eval: ret = m_Vulkan->m_TES.entryPoint; break;
      case eShaderStage_Geometry: ret = m_Vulkan->m_GS.entryPoint; break;
      case eShaderStage_Fragment: ret = m_Vulkan->m_FS.entryPoint; break;
      case eShaderStage_Compute: ret = m_Vulkan->m_CS.entryPoint; break;
      default: break;
    }
  }

  return ret;
}

ResourceId CommonPipelineState::GetShader(ShaderStageType stage)
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_D3D11->m_VS.Shader;
        case eShaderStage_Domain: return m_D3D11->m_DS.Shader;
        case eShaderStage_Hull: return m_D3D11->m_HS.Shader;
        case eShaderStage_Geometry: return m_D3D11->m_GS.Shader;
        case eShaderStage_Pixel: return m_D3D11->m_PS.Shader;
        case eShaderStage_Compute: return m_D3D11->m_CS.Shader;
        default: break;
      }
    }
    else if(IsLogD3D12())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_D3D12->m_VS.Shader;
        case eShaderStage_Domain: return m_D3D12->m_DS.Shader;
        case eShaderStage_Hull: return m_D3D12->m_HS.Shader;
        case eShaderStage_Geometry: return m_D3D12->m_GS.Shader;
        case eShaderStage_Pixel: return m_D3D12->m_PS.Shader;
        case eShaderStage_Compute: return m_D3D12->m_CS.Shader;
        default: break;
      }
    }
    else if(IsLogGL())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_GL->m_VS.Shader;
        case eShaderStage_Tess_Control: return m_GL->m_TCS.Shader;
        case eShaderStage_Tess_Eval: return m_GL->m_TES.Shader;
        case eShaderStage_Geometry: return m_GL->m_GS.Shader;
        case eShaderStage_Fragment: return m_GL->m_FS.Shader;
        case eShaderStage_Compute: return m_GL->m_CS.Shader;
        default: break;
      }
    }
    else if(IsLogVK())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: return m_Vulkan->m_VS.Shader;
        case eShaderStage_Tess_Control: return m_Vulkan->m_TCS.Shader;
        case eShaderStage_Tess_Eval: return m_Vulkan->m_TES.Shader;
        case eShaderStage_Geometry: return m_Vulkan->m_GS.Shader;
        case eShaderStage_Fragment: return m_Vulkan->m_FS.Shader;
        case eShaderStage_Compute: return m_Vulkan->m_CS.Shader;
        default: break;
      }
    }
  }

  return ResourceId();
}

QString CommonPipelineState::GetShaderName(ShaderStageType stage)
{
  QString ret;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: ret = m_D3D11->m_VS.ShaderName; break;
        case eShaderStage_Domain: ret = m_D3D11->m_DS.ShaderName; break;
        case eShaderStage_Hull: ret = m_D3D11->m_HS.ShaderName; break;
        case eShaderStage_Geometry: ret = m_D3D11->m_GS.ShaderName; break;
        case eShaderStage_Pixel: ret = m_D3D11->m_PS.ShaderName; break;
        case eShaderStage_Compute: ret = m_D3D11->m_CS.ShaderName; break;
        default: break;
      }
    }
    else if(IsLogD3D12())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: ret = QString(m_D3D12->PipelineName) + " VS"; break;
        case eShaderStage_Domain: ret = QString(m_D3D12->PipelineName) + " DS"; break;
        case eShaderStage_Hull: ret = QString(m_D3D12->PipelineName) + " HS"; break;
        case eShaderStage_Geometry: ret = QString(m_D3D12->PipelineName) + " GS"; break;
        case eShaderStage_Pixel: ret = QString(m_D3D12->PipelineName) + " PS"; break;
        case eShaderStage_Compute: ret = QString(m_D3D12->PipelineName) + " CS"; break;
        default: break;
      }
    }
    else if(IsLogGL())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: ret = m_GL->m_VS.ShaderName; break;
        case eShaderStage_Tess_Control: ret = m_GL->m_TCS.ShaderName; break;
        case eShaderStage_Tess_Eval: ret = m_GL->m_TES.ShaderName; break;
        case eShaderStage_Geometry: ret = m_GL->m_GS.ShaderName; break;
        case eShaderStage_Fragment: ret = m_GL->m_FS.ShaderName; break;
        case eShaderStage_Compute: ret = m_GL->m_CS.ShaderName; break;
        default: break;
      }
    }
    else if(IsLogVK())
    {
      switch(stage)
      {
        case eShaderStage_Vertex: ret = m_Vulkan->m_VS.ShaderName; break;
        case eShaderStage_Domain: ret = m_Vulkan->m_TCS.ShaderName; break;
        case eShaderStage_Hull: ret = m_Vulkan->m_TES.ShaderName; break;
        case eShaderStage_Geometry: ret = m_Vulkan->m_GS.ShaderName; break;
        case eShaderStage_Pixel: ret = m_Vulkan->m_FS.ShaderName; break;
        case eShaderStage_Compute: ret = m_Vulkan->m_CS.ShaderName; break;
        default: break;
      }
    }
  }

  return ret;
}

void CommonPipelineState::GetIBuffer(ResourceId &buf, uint64_t &ByteOffset)
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      buf = m_D3D11->m_IA.ibuffer.Buffer;
      ByteOffset = m_D3D11->m_IA.ibuffer.Offset;

      return;
    }
    else if(IsLogD3D12())
    {
      buf = m_D3D12->m_IA.ibuffer.Buffer;
      ByteOffset = m_D3D12->m_IA.ibuffer.Offset;

      return;
    }
    else if(IsLogGL())
    {
      buf = m_GL->m_VtxIn.ibuffer;
      ByteOffset = 0;    // GL only has per-draw index offset

      return;
    }
    else if(IsLogVK())
    {
      buf = m_Vulkan->IA.ibuffer.buf;
      ByteOffset = m_Vulkan->IA.ibuffer.offs;

      return;
    }
  }

  buf = ResourceId();
  ByteOffset = 0;
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

uint32_t CommonPipelineState::GetStripRestartIndex(uint32_t indexByteWidth)
{
  if(LogLoaded())
  {
    if(IsLogD3D11() || IsLogVK())
    {
      // D3D11 or Vulkan this is always '-1' in whichever size of index we're using
      return indexByteWidth == 2 ? UINT16_MAX : UINT32_MAX;
    }
    else if(IsLogD3D12())
    {
      return m_D3D12->m_IA.indexStripCutValue;
    }
    else if(IsLogGL())
    {
      uint32_t maxval = UINT32_MAX;
      if(indexByteWidth == 2)
        maxval = UINT16_MAX;
      else if(indexByteWidth == 1)
        maxval = 0xff;
      return qMin(maxval, m_GL->m_VtxIn.restartIndex);
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
        QString semName(layouts[i].SemanticName);

        bool needsSemanticIdx = false;
        for(int j = 0; j < layouts.count; j++)
        {
          if(i != j && !semName.compare(QString(layouts[j].SemanticName), Qt::CaseInsensitive))
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

        ret[i].Name = semName + (needsSemanticIdx ? QString::number(layouts[i].SemanticIndex) : "");
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
            if(!semName.compare(QString(sig[ia].semanticName), Qt::CaseInsensitive) &&
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
        QString semName(layouts[i].SemanticName);

        bool needsSemanticIdx = false;
        for(int j = 0; j < layouts.count; j++)
        {
          if(i != j && !semName.compare(QString(layouts[j].SemanticName), Qt::CaseInsensitive))
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

        ret[i].Name = semName + (needsSemanticIdx ? QString::number(layouts[i].SemanticIndex) : "");
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
            if(!semName.compare(QString(sig[ia].semanticName), Qt::CaseInsensitive) &&
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
        ret[a].Name = QString("attr%1").arg(i);
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
            ret[a].Name = m_GL->m_VS.ShaderDetails->InputSig[attrib].varName;

          if(attrib == -1)
            continue;

          if(!attrs[i].Enabled)
          {
            uint32_t compCount = m_GL->m_VS.ShaderDetails->InputSig[attrib].compCount;
            FormatComponentType compType = m_GL->m_VS.ShaderDetails->InputSig[attrib].compType;

            for(uint32_t c = 0; c < compCount; c++)
            {
              if(compType == eCompType_Float)
                ret[a].GenericValue.value_f[c] = attrs[i].GenericValue.f[c];
              else if(compType == eCompType_UInt)
                ret[a].GenericValue.value_u[c] = attrs[i].GenericValue.u[c];
              else if(compType == eCompType_SInt)
                ret[a].GenericValue.value_i[c] = attrs[i].GenericValue.i[c];
              else if(compType == eCompType_UScaled)
                ret[a].GenericValue.value_f[c] = (float)attrs[i].GenericValue.u[c];
              else if(compType == eCompType_SScaled)
                ret[a].GenericValue.value_f[c] = (float)attrs[i].GenericValue.i[c];
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
        ret[a].Name = QString("attr%1").arg(i);
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
            ret[a].Name = m_Vulkan->m_VS.ShaderDetails->InputSig[attrib].varName;

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

void CommonPipelineState::GetConstantBuffer(ShaderStageType stage, uint32_t BufIdx,
                                            uint32_t ArrayIdx, ResourceId &buf,
                                            uint64_t &ByteOffset, uint64_t &ByteSize)
{
  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      const D3D11PipelineState::ShaderStage &s = GetD3D11Stage(stage);

      if(BufIdx < (uint32_t)s.ConstantBuffers.count)
      {
        buf = s.ConstantBuffers[BufIdx].Buffer;
        ByteOffset = s.ConstantBuffers[BufIdx].VecOffset * 4 * sizeof(float);
        ByteSize = s.ConstantBuffers[BufIdx].VecCount * 4 * sizeof(float);

        return;
      }
    }
    else if(IsLogD3D12())
    {
      const D3D12PipelineState::ShaderStage &s = GetD3D12Stage(stage);

      if(s.ShaderDetails != NULL && BufIdx < (uint32_t)s.ShaderDetails->ConstantBlocks.count)
      {
        const BindpointMap &bind =
            s.BindpointMapping.ConstantBlocks[s.ShaderDetails->ConstantBlocks[BufIdx].bindPoint];

        if(bind.bindset >= s.Spaces.count || bind.bind >= s.Spaces[bind.bindset].ConstantBuffers.count)
        {
          buf = ResourceId();
          ByteOffset = 0;
          ByteSize = 0;
          return;
        }

        const D3D12PipelineState::CBuffer &descriptor =
            s.Spaces[bind.bindset].ConstantBuffers[bind.bind];

        buf = descriptor.Buffer;
        ByteOffset = descriptor.Offset;
        ByteSize = descriptor.ByteSize;

        return;
      }
    }
    else if(IsLogGL())
    {
      const GLPipelineState::ShaderStage &s = GetGLStage(stage);

      if(s.ShaderDetails != NULL && BufIdx < (uint32_t)s.ShaderDetails->ConstantBlocks.count)
      {
        if(s.ShaderDetails->ConstantBlocks[BufIdx].bindPoint >= 0)
        {
          int uboIdx =
              s.BindpointMapping.ConstantBlocks[s.ShaderDetails->ConstantBlocks[BufIdx].bindPoint].bind;
          if(uboIdx >= 0 && uboIdx < m_GL->UniformBuffers.count)
          {
            GLPipelineState::Buffer &b = m_GL->UniformBuffers[uboIdx];

            buf = b.Resource;
            ByteOffset = b.Offset;
            ByteSize = b.Size;

            return;
          }
        }
      }
    }
    else if(IsLogVK())
    {
      VulkanPipelineState::Pipeline &pipe =
          stage == eShaderStage_Compute ? m_Vulkan->compute : m_Vulkan->graphics;
      const VulkanPipelineState::ShaderStage &s = GetVulkanStage(stage);

      if(s.ShaderDetails != NULL && BufIdx < (uint32_t)s.ShaderDetails->ConstantBlocks.count)
      {
        const BindpointMap &bind =
            s.BindpointMapping.ConstantBlocks[s.ShaderDetails->ConstantBlocks[BufIdx].bindPoint];

        if(s.ShaderDetails->ConstantBlocks[BufIdx].bufferBacked == false)
        {
          // dummy values, it would be nice to fetch these properly
          buf = ResourceId();
          ByteOffset = 0;
          ByteSize = 1024;
          return;
        }

        auto &descriptorBind = pipe.DescSets[bind.bindset].bindings[bind.bind].binds[ArrayIdx];

        buf = descriptorBind.res;
        ByteOffset = descriptorBind.offset;
        ByteSize = descriptorBind.size;

        return;
      }
    }
  }

  buf = ResourceId();
  ByteOffset = 0;
  ByteSize = 0;
}

QMap<BindpointMap, QVector<BoundResource>> CommonPipelineState::GetReadOnlyResources(ShaderStageType stage)
{
  QMap<BindpointMap, QVector<BoundResource>> ret;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      const D3D11PipelineState::ShaderStage &s = GetD3D11Stage(stage);

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
      const D3D12PipelineState::ShaderStage &s = GetD3D12Stage(stage);

      for(int space = 0; space < s.Spaces.count; space++)
      {
        for(int reg = 0; reg < s.Spaces[space].SRVs.count; reg++)
        {
          const D3D12PipelineState::ResourceView &bind = s.Spaces[space].SRVs[reg];
          BindpointMap key(space, reg);
          BoundResource val;

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
        val.typeHint = eCompType_None;

        ret[key] = {val};
      }

      return ret;
    }
    else if(IsLogVK())
    {
      const auto &descsets =
          stage == eShaderStage_Compute ? m_Vulkan->compute.DescSets : m_Vulkan->graphics.DescSets;

      ShaderStageBits mask = (ShaderStageBits)(1 << (int)stage);

      for(int set = 0; set < descsets.count; set++)
      {
        const auto &descset = descsets[set];
        for(int slot = 0; slot < descset.bindings.count; slot++)
        {
          const auto &bind = descset.bindings[slot];
          if((bind.type == eBindType_ImageSampler || bind.type == eBindType_InputAttachment ||
              bind.type == eBindType_ReadOnlyImage || bind.type == eBindType_ReadOnlyTBuffer) &&
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

QMap<BindpointMap, QVector<BoundResource>> CommonPipelineState::GetReadWriteResources(
    ShaderStageType stage)
{
  QMap<BindpointMap, QVector<BoundResource>> ret;

  if(LogLoaded())
  {
    if(IsLogD3D11())
    {
      if(stage == eShaderStage_Compute)
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
      const D3D12PipelineState::ShaderStage &s = GetD3D12Stage(stage);

      for(int space = 0; space < s.Spaces.count; space++)
      {
        for(int reg = 0; reg < s.Spaces[space].UAVs.count; reg++)
        {
          const D3D12PipelineState::ResourceView &bind = s.Spaces[space].UAVs[reg];
          BindpointMap key(space, reg);
          BoundResource val;

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
          stage == eShaderStage_Compute ? m_Vulkan->compute.DescSets : m_Vulkan->graphics.DescSets;

      ShaderStageBits mask = (ShaderStageBits)(1 << (int)stage);

      for(int set = 0; set < descsets.count; set++)
      {
        const auto &descset = descsets[set];
        for(int slot = 0; slot < descset.bindings.count; slot++)
        {
          const auto &bind = descset.bindings[slot];
          if((bind.type == eBindType_ReadWriteBuffer || bind.type == eBindType_ReadWriteImage ||
              bind.type == eBindType_ReadWriteTBuffer) &&
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
      ret.typeHint = eCompType_None;
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
          ret[i].typeHint = eCompType_None;
        }
      }
    }
    else if(IsLogVK())
    {
      const auto &rp = m_Vulkan->Pass.renderpass;
      const auto &fb = m_Vulkan->Pass.framebuffer;

      ret.resize(rp.colorAttachments.count);
      for(int i = 0; i < rp.colorAttachments.count; i++)
      {
        if(rp.colorAttachments[i] < (uint32_t)fb.attachments.count)
        {
          ret[i].Id = fb.attachments[rp.colorAttachments[i]].img;
          ret[i].HighestMip = (int)fb.attachments[rp.colorAttachments[i]].baseMip;
          ret[i].FirstSlice = (int)fb.attachments[rp.colorAttachments[i]].baseLayer;
          ret[i].typeHint = fb.attachments[rp.colorAttachments[i]].viewfmt.compType;
        }
      }
    }
  }

  return ret;
}
