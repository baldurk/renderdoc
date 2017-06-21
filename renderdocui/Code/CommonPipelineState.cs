/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2017 Baldur Karlsson
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

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using renderdoc;

namespace renderdocui.Code
{
    public class BoundResource
    {
        public BoundResource()
        { Id = ResourceId.Null; HighestMip = -1; FirstSlice = -1; typeHint = FormatComponentType.None; }
        public BoundResource(ResourceId id)
        { Id = id; HighestMip = -1; FirstSlice = -1; typeHint = FormatComponentType.None; }

        public ResourceId Id;
        public int HighestMip;
        public int FirstSlice;
        public FormatComponentType typeHint;
    };

    public struct BoundVBuffer
    {
        public ResourceId Buffer;
        public ulong ByteOffset;
        public uint ByteStride;
    };

    public struct VertexInputAttribute
    {
        public string Name;
        public int VertexBuffer;
        public uint RelativeByteOffset;
        public bool PerInstance;
        public int InstanceRate;
        public ResourceFormat Format;
        public object[] GenericValue;
        public bool Used;
    };

    public struct Viewport
    {
        public float x, y, width, height;
    };

    public class CommonPipelineState
    {
        private D3D11PipelineState m_D3D11 = null;
        private D3D12PipelineState m_D3D12 = null;
        private GLPipelineState m_GL = null;
        private VulkanPipelineState m_Vulkan = null;
        private APIProperties m_APIProps = null;

        public CommonPipelineState()
        {
        }

        public void SetStates(APIProperties props, D3D11PipelineState d3d11, D3D12PipelineState d3d12, GLPipelineState gl, VulkanPipelineState vk)
        {
            m_APIProps = props;
            m_D3D11 = d3d11;
            m_D3D12 = d3d12;
            m_GL = gl;
            m_Vulkan = vk;
        }

        public GraphicsAPI DefaultType = GraphicsAPI.D3D11;

        private bool LogLoaded
        {
            get
            {
                return m_D3D11 != null || m_D3D12 != null || m_GL != null || m_Vulkan != null;
            }
        }

        private bool IsLogD3D11
        {
            get
            {
                return LogLoaded && m_APIProps.pipelineType == GraphicsAPI.D3D11 && m_D3D11 != null;
            }
        }

        private bool IsLogD3D12
        {
            get
            {
                return LogLoaded && m_APIProps.pipelineType == GraphicsAPI.D3D12 && m_D3D12 != null;
            }
        }

        private bool IsLogGL
        {
            get
            {
                return LogLoaded && m_APIProps.pipelineType == GraphicsAPI.OpenGL && m_GL != null;
            }
        }

        private bool IsLogVK
        {
            get
            {
                return LogLoaded && m_APIProps.pipelineType == GraphicsAPI.Vulkan && m_Vulkan != null;
            }
        }

        // add a bunch of generic properties that people can check to save having to see which pipeline state
        // is valid and look at the appropriate part of it
        public bool IsTessellationEnabled
        {
            get
            {
                if (LogLoaded)
                {
                    if (IsLogD3D11)
                        return m_D3D11 != null && m_D3D11.m_HS.Shader != ResourceId.Null;

                    if (IsLogD3D12)
                        return m_D3D12 != null && m_D3D12.m_HS.Shader != ResourceId.Null;

                    if (IsLogGL)
                        return m_GL != null && m_GL.m_TES.Shader != ResourceId.Null;

                    if (IsLogVK)
                        return m_Vulkan != null && m_Vulkan.m_TES.Shader != ResourceId.Null;
                }

                return false;
            }
        }

        public bool SupportsResourceArrays
        {
            get
            {
                return LogLoaded && IsLogVK;
            }
        }

        public bool SupportsBarriers
        {
            get
            {
                return LogLoaded && (IsLogVK || IsLogD3D12);
            }
        }

        // whether or not the PostVS data is aligned in the typical fashion
        // ie. vectors not crossing float4 boundaries). APIs that use stream-out
        // or transform feedback have tightly packed data, but APIs that rewrite
        // shaders to dump data might have these alignment requirements
        public bool HasAlignedPostVSData
        {
            get
            {
                return LogLoaded && IsLogVK;
            }
        }

        public string GetImageLayout(ResourceId id)
        {
            if (LogLoaded)
            {
                if (IsLogVK && m_Vulkan.Images.ContainsKey(id))
                    return m_Vulkan.Images[id].layouts[0].name;

                if (IsLogD3D12 && m_D3D12.Resources.ContainsKey(id))
                    return m_D3D12.Resources[id].states[0].name;
            }

            return "Unknown";
        }

        public string Abbrev(ShaderStageType stage)
        {
            if (IsLogD3D11 || (!LogLoaded && DefaultType == GraphicsAPI.D3D11) ||
                IsLogD3D12 || (!LogLoaded && DefaultType == GraphicsAPI.D3D12))
            {
                switch (stage)
                {
                    case ShaderStageType.Vertex: return "VS";
                    case ShaderStageType.Hull: return "HS";
                    case ShaderStageType.Domain: return "DS";
                    case ShaderStageType.Geometry: return "GS";
                    case ShaderStageType.Pixel: return "PS";
                    case ShaderStageType.Compute: return "CS";
                }
            }
            else if (IsLogGL || (!LogLoaded && DefaultType == GraphicsAPI.OpenGL) ||
                     IsLogVK || (!LogLoaded && DefaultType == GraphicsAPI.Vulkan))
            {
                switch (stage)
                {
                    case ShaderStageType.Vertex: return "VS";
                    case ShaderStageType.Tess_Control: return "TCS";
                    case ShaderStageType.Tess_Eval: return "TES";
                    case ShaderStageType.Geometry: return "GS";
                    case ShaderStageType.Fragment: return "FS";
                    case ShaderStageType.Compute: return "CS";
                }
            }

            return "?S";
        }

        public string OutputAbbrev()
        {
            if (IsLogGL || (!LogLoaded && DefaultType == GraphicsAPI.OpenGL) ||
                IsLogVK || (!LogLoaded && DefaultType == GraphicsAPI.Vulkan))
            {
                return "FB";
            }

            return "RT";
        }

        // there's a lot of redundancy in these functions

        public Viewport GetViewport(int index)
        {
            Viewport ret = new Viewport();

            // default to a 1x1 viewport just to avoid having to check for 0s all over
            ret.x = ret.y = 0.0f;
            ret.width = ret.height = 1.0f;

            if (LogLoaded)
            {
                if (IsLogD3D11 && m_D3D11.m_RS.Viewports.Length > 0)
                {
                    ret.x = m_D3D11.m_RS.Viewports[0].TopLeft[0];
                    ret.y = m_D3D11.m_RS.Viewports[0].TopLeft[1];
                    ret.width = m_D3D11.m_RS.Viewports[0].Width;
                    ret.height = m_D3D11.m_RS.Viewports[0].Height;
                }
                else if (IsLogD3D12 && m_D3D12.m_RS.Viewports.Length > 0)
                {
                    ret.x = m_D3D12.m_RS.Viewports[0].TopLeft[0];
                    ret.y = m_D3D12.m_RS.Viewports[0].TopLeft[1];
                    ret.width = m_D3D12.m_RS.Viewports[0].Width;
                    ret.height = m_D3D12.m_RS.Viewports[0].Height;
                }
                else if (IsLogGL && m_GL.m_RS.Viewports.Length > 0)
                {
                    ret.x = m_GL.m_RS.Viewports[0].Left;
                    ret.y = m_GL.m_RS.Viewports[0].Bottom;
                    ret.width = m_GL.m_RS.Viewports[0].Width;
                    ret.height = m_GL.m_RS.Viewports[0].Height;
                }
                else if (IsLogVK && m_Vulkan.VP.viewportScissors.Length > 0)
                {
                    ret.x = m_Vulkan.VP.viewportScissors[0].vp.x;
                    ret.y = m_Vulkan.VP.viewportScissors[0].vp.y;
                    ret.width = m_Vulkan.VP.viewportScissors[0].vp.Width;
                    ret.height = m_Vulkan.VP.viewportScissors[0].vp.Height;
                }
            }

            return ret;
        }

        public ShaderBindpointMapping GetBindpointMapping(ShaderStageType stage)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_D3D11.m_VS.BindpointMapping;
                        case ShaderStageType.Domain: return m_D3D11.m_DS.BindpointMapping;
                        case ShaderStageType.Hull: return m_D3D11.m_HS.BindpointMapping;
                        case ShaderStageType.Geometry: return m_D3D11.m_GS.BindpointMapping;
                        case ShaderStageType.Pixel: return m_D3D11.m_PS.BindpointMapping;
                        case ShaderStageType.Compute: return m_D3D11.m_CS.BindpointMapping;
                    }
                }
                else if (IsLogD3D12)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_D3D12.m_VS.BindpointMapping;
                        case ShaderStageType.Domain: return m_D3D12.m_DS.BindpointMapping;
                        case ShaderStageType.Hull: return m_D3D12.m_HS.BindpointMapping;
                        case ShaderStageType.Geometry: return m_D3D12.m_GS.BindpointMapping;
                        case ShaderStageType.Pixel: return m_D3D12.m_PS.BindpointMapping;
                        case ShaderStageType.Compute: return m_D3D12.m_CS.BindpointMapping;
                    }
                }
                else if (IsLogGL)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_GL.m_VS.BindpointMapping;
                        case ShaderStageType.Tess_Control: return m_GL.m_TCS.BindpointMapping;
                        case ShaderStageType.Tess_Eval: return m_GL.m_TES.BindpointMapping;
                        case ShaderStageType.Geometry: return m_GL.m_GS.BindpointMapping;
                        case ShaderStageType.Fragment: return m_GL.m_FS.BindpointMapping;
                        case ShaderStageType.Compute: return m_GL.m_CS.BindpointMapping;
                    }
                }
                else if (IsLogVK)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_Vulkan.m_VS.BindpointMapping;
                        case ShaderStageType.Tess_Control: return m_Vulkan.m_TCS.BindpointMapping;
                        case ShaderStageType.Tess_Eval: return m_Vulkan.m_TES.BindpointMapping;
                        case ShaderStageType.Geometry: return m_Vulkan.m_GS.BindpointMapping;
                        case ShaderStageType.Fragment: return m_Vulkan.m_FS.BindpointMapping;
                        case ShaderStageType.Compute: return m_Vulkan.m_CS.BindpointMapping;
                    }
                }
            }

            return null;
        }

        public ShaderReflection GetShaderReflection(ShaderStageType stage)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_D3D11.m_VS.ShaderDetails;
                        case ShaderStageType.Domain: return m_D3D11.m_DS.ShaderDetails;
                        case ShaderStageType.Hull: return m_D3D11.m_HS.ShaderDetails;
                        case ShaderStageType.Geometry: return m_D3D11.m_GS.ShaderDetails;
                        case ShaderStageType.Pixel: return m_D3D11.m_PS.ShaderDetails;
                        case ShaderStageType.Compute: return m_D3D11.m_CS.ShaderDetails;
                    }
                }
                else if (IsLogD3D12)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_D3D12.m_VS.ShaderDetails;
                        case ShaderStageType.Domain: return m_D3D12.m_DS.ShaderDetails;
                        case ShaderStageType.Hull: return m_D3D12.m_HS.ShaderDetails;
                        case ShaderStageType.Geometry: return m_D3D12.m_GS.ShaderDetails;
                        case ShaderStageType.Pixel: return m_D3D12.m_PS.ShaderDetails;
                        case ShaderStageType.Compute: return m_D3D12.m_CS.ShaderDetails;
                    }
                }
                else if (IsLogGL)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_GL.m_VS.ShaderDetails;
                        case ShaderStageType.Tess_Control: return m_GL.m_TCS.ShaderDetails;
                        case ShaderStageType.Tess_Eval: return m_GL.m_TES.ShaderDetails;
                        case ShaderStageType.Geometry: return m_GL.m_GS.ShaderDetails;
                        case ShaderStageType.Fragment: return m_GL.m_FS.ShaderDetails;
                        case ShaderStageType.Compute: return m_GL.m_CS.ShaderDetails;
                    }
                }
                else if (IsLogVK)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_Vulkan.m_VS.ShaderDetails;
                        case ShaderStageType.Tess_Control: return m_Vulkan.m_TCS.ShaderDetails;
                        case ShaderStageType.Tess_Eval: return m_Vulkan.m_TES.ShaderDetails;
                        case ShaderStageType.Geometry: return m_Vulkan.m_GS.ShaderDetails;
                        case ShaderStageType.Fragment: return m_Vulkan.m_FS.ShaderDetails;
                        case ShaderStageType.Compute: return m_Vulkan.m_CS.ShaderDetails;
                    }
                }
            }

            return null;
        }

        public String GetShaderEntryPoint(ShaderStageType stage)
        {
            if (LogLoaded && IsLogVK)
            {
                switch (stage)
                {
                    case ShaderStageType.Vertex: return m_Vulkan.m_VS.entryPoint;
                    case ShaderStageType.Tess_Control: return m_Vulkan.m_TCS.entryPoint;
                    case ShaderStageType.Tess_Eval: return m_Vulkan.m_TES.entryPoint;
                    case ShaderStageType.Geometry: return m_Vulkan.m_GS.entryPoint;
                    case ShaderStageType.Fragment: return m_Vulkan.m_FS.entryPoint;
                    case ShaderStageType.Compute: return m_Vulkan.m_CS.entryPoint;
                }
            }

            return "";
        }

        public ResourceId GetShader(ShaderStageType stage)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_D3D11.m_VS.Shader;
                        case ShaderStageType.Domain: return m_D3D11.m_DS.Shader;
                        case ShaderStageType.Hull: return m_D3D11.m_HS.Shader;
                        case ShaderStageType.Geometry: return m_D3D11.m_GS.Shader;
                        case ShaderStageType.Pixel: return m_D3D11.m_PS.Shader;
                        case ShaderStageType.Compute: return m_D3D11.m_CS.Shader;
                    }
                }
                else if (IsLogD3D12)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_D3D12.m_VS.Shader;
                        case ShaderStageType.Domain: return m_D3D12.m_DS.Shader;
                        case ShaderStageType.Hull: return m_D3D12.m_HS.Shader;
                        case ShaderStageType.Geometry: return m_D3D12.m_GS.Shader;
                        case ShaderStageType.Pixel: return m_D3D12.m_PS.Shader;
                        case ShaderStageType.Compute: return m_D3D12.m_CS.Shader;
                    }
                }
                else if (IsLogGL)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_GL.m_VS.Shader;
                        case ShaderStageType.Tess_Control: return m_GL.m_TCS.Shader;
                        case ShaderStageType.Tess_Eval: return m_GL.m_TES.Shader;
                        case ShaderStageType.Geometry: return m_GL.m_GS.Shader;
                        case ShaderStageType.Fragment: return m_GL.m_FS.Shader;
                        case ShaderStageType.Compute: return m_GL.m_CS.Shader;
                    }
                }
                else if (IsLogVK)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_Vulkan.m_VS.Shader;
                        case ShaderStageType.Tess_Control: return m_Vulkan.m_TCS.Shader;
                        case ShaderStageType.Tess_Eval: return m_Vulkan.m_TES.Shader;
                        case ShaderStageType.Geometry: return m_Vulkan.m_GS.Shader;
                        case ShaderStageType.Fragment: return m_Vulkan.m_FS.Shader;
                        case ShaderStageType.Compute: return m_Vulkan.m_CS.Shader;
                    }
                }
            }

            return ResourceId.Null;
        }

        public string GetShaderName(ShaderStageType stage)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_D3D11.m_VS.ShaderName;
                        case ShaderStageType.Domain: return m_D3D11.m_DS.ShaderName;
                        case ShaderStageType.Hull: return m_D3D11.m_HS.ShaderName;
                        case ShaderStageType.Geometry: return m_D3D11.m_GS.ShaderName;
                        case ShaderStageType.Pixel: return m_D3D11.m_PS.ShaderName;
                        case ShaderStageType.Compute: return m_D3D11.m_CS.ShaderName;
                    }
                }
                else if (IsLogD3D12)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_D3D12.PipelineName + " VS";
                        case ShaderStageType.Domain: return m_D3D12.PipelineName + " DS";
                        case ShaderStageType.Hull: return m_D3D12.PipelineName + " HS";
                        case ShaderStageType.Geometry: return m_D3D12.PipelineName + " GS";
                        case ShaderStageType.Pixel: return m_D3D12.PipelineName + " PS";
                        case ShaderStageType.Compute: return m_D3D12.PipelineName + " CS";
                    }
                }
                else if (IsLogGL)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_GL.m_VS.ShaderName;
                        case ShaderStageType.Tess_Control: return m_GL.m_TCS.ShaderName;
                        case ShaderStageType.Tess_Eval: return m_GL.m_TES.ShaderName;
                        case ShaderStageType.Geometry: return m_GL.m_GS.ShaderName;
                        case ShaderStageType.Fragment: return m_GL.m_FS.ShaderName;
                        case ShaderStageType.Compute: return m_GL.m_CS.ShaderName;
                    }
                }
                else if (IsLogVK)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return m_Vulkan.m_VS.ShaderName;
                        case ShaderStageType.Domain: return m_Vulkan.m_TCS.ShaderName;
                        case ShaderStageType.Hull: return m_Vulkan.m_TES.ShaderName;
                        case ShaderStageType.Geometry: return m_Vulkan.m_GS.ShaderName;
                        case ShaderStageType.Pixel: return m_Vulkan.m_FS.ShaderName;
                        case ShaderStageType.Compute: return m_Vulkan.m_CS.ShaderName;
                    }
                }
            }

            return "";
        }

        public void GetIBuffer(out ResourceId buf, out ulong ByteOffset)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    buf = m_D3D11.m_IA.ibuffer.Buffer;
                    ByteOffset = m_D3D11.m_IA.ibuffer.Offset;

                    return;
                }
                else if (IsLogD3D12)
                {
                    buf = m_D3D12.m_IA.ibuffer.Buffer;
                    ByteOffset = m_D3D12.m_IA.ibuffer.Offset;

                    return;
                }
                else if (IsLogGL)
                {
                    buf = m_GL.m_VtxIn.ibuffer;
                    ByteOffset = 0; // GL only has per-draw index offset

                    return;
                }
                else if (IsLogVK)
                {
                    buf = m_Vulkan.IA.ibuffer.buf;
                    ByteOffset = m_Vulkan.IA.ibuffer.offs;

                    return;
                }
            }

            buf = ResourceId.Null;
            ByteOffset = 0;
        }

        public bool IsStripRestartEnabled()
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    // D3D11 this is always enabled
                    return true;
                }
                else if (IsLogD3D12)
                {
                    return m_D3D12.m_IA.indexStripCutValue != 0;
                }
                else if (IsLogGL)
                {
                    return m_GL.m_VtxIn.primitiveRestart;
                }
                else if (IsLogVK)
                {
                    return m_Vulkan.IA.primitiveRestartEnable;
                }
            }

            return false;
        }

        public uint GetStripRestartIndex(uint indexByteWidth)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11 || IsLogVK)
                {
                    // D3D11 or Vulkan this is always '-1' in whichever size of index we're using
                    return indexByteWidth == 2 ? ushort.MaxValue : uint.MaxValue;
                }
                else if (IsLogD3D12)
                {
                    return m_D3D12.m_IA.indexStripCutValue;
                }
                else if (IsLogGL)
                {
                    uint maxval = uint.MaxValue;
                    if (indexByteWidth == 2)
                        maxval = ushort.MaxValue;
                    else if (indexByteWidth == 1)
                        maxval = 0xff;
                    return Math.Min(maxval, m_GL.m_VtxIn.restartIndex);
                }
            }

            return uint.MaxValue;
        }

        public BoundVBuffer[] GetVBuffers()
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    BoundVBuffer[] ret = new BoundVBuffer[m_D3D11.m_IA.vbuffers.Length];
                    for (int i = 0; i < m_D3D11.m_IA.vbuffers.Length; i++)
                    {
                        ret[i].Buffer = m_D3D11.m_IA.vbuffers[i].Buffer;
                        ret[i].ByteOffset = m_D3D11.m_IA.vbuffers[i].Offset;
                        ret[i].ByteStride = m_D3D11.m_IA.vbuffers[i].Stride;
                    }

                    return ret;
                }
                else if (IsLogD3D12)
                {
                    BoundVBuffer[] ret = new BoundVBuffer[m_D3D12.m_IA.vbuffers.Length];
                    for (int i = 0; i < m_D3D12.m_IA.vbuffers.Length; i++)
                    {
                        ret[i].Buffer = m_D3D12.m_IA.vbuffers[i].Buffer;
                        ret[i].ByteOffset = m_D3D12.m_IA.vbuffers[i].Offset;
                        ret[i].ByteStride = m_D3D12.m_IA.vbuffers[i].Stride;
                    }

                    return ret;
                }
                else if (IsLogGL)
                {
                    BoundVBuffer[] ret = new BoundVBuffer[m_GL.m_VtxIn.vbuffers.Length];
                    for (int i = 0; i < m_GL.m_VtxIn.vbuffers.Length; i++)
                    {
                        ret[i].Buffer = m_GL.m_VtxIn.vbuffers[i].Buffer;
                        ret[i].ByteOffset = m_GL.m_VtxIn.vbuffers[i].Offset;
                        ret[i].ByteStride = m_GL.m_VtxIn.vbuffers[i].Stride;
                    }

                    return ret;
                }
                else if (IsLogVK)
                {
                    BoundVBuffer[] ret = new BoundVBuffer[m_Vulkan.VI.binds.Length];
                    for (int i = 0; i < m_Vulkan.VI.binds.Length; i++)
                    {
                        ret[i].Buffer = i < m_Vulkan.VI.vbuffers.Length ? m_Vulkan.VI.vbuffers[i].buffer : ResourceId.Null;
                        ret[i].ByteOffset = i < m_Vulkan.VI.vbuffers.Length ? m_Vulkan.VI.vbuffers[i].offset : 0;
                        ret[i].ByteStride = m_Vulkan.VI.binds[i].bytestride;
                    }

                    return ret;
                }
            }

            return null;
        }

        public VertexInputAttribute[] GetVertexInputs()
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    uint[] byteOffs = new uint[128];
                    for (int i = 0; i < 128; i++)
                        byteOffs[i] = 0;

                    var layouts = m_D3D11.m_IA.layouts;

                    VertexInputAttribute[] ret = new VertexInputAttribute[layouts.Length];
                    for (int i = 0; i < layouts.Length; i++)
                    {
                        bool needsSemanticIdx = false;
                        for (int j = 0; j < layouts.Length; j++)
                        {
                            if (i != j && layouts[i].SemanticName == layouts[j].SemanticName)
                            {
                                needsSemanticIdx = true;
                                break;
                            }
                        }

                        uint offs = layouts[i].ByteOffset;
                        if (offs == uint.MaxValue) // APPEND_ALIGNED
                            offs = byteOffs[layouts[i].InputSlot];
                        else
                            byteOffs[layouts[i].InputSlot] = offs = layouts[i].ByteOffset;

                        byteOffs[layouts[i].InputSlot] += layouts[i].Format.compByteWidth * layouts[i].Format.compCount;

                        ret[i].Name = layouts[i].SemanticName + (needsSemanticIdx ? layouts[i].SemanticIndex.ToString() : "");
                        ret[i].VertexBuffer = (int)layouts[i].InputSlot;
                        ret[i].RelativeByteOffset = offs;
                        ret[i].PerInstance = layouts[i].PerInstance;
                        ret[i].InstanceRate = (int)layouts[i].InstanceDataStepRate;
                        ret[i].Format = layouts[i].Format;
                        ret[i].GenericValue = null;
                        ret[i].Used = false;

                        if (m_D3D11.m_IA.Bytecode != null)
                        {
                            for (int ia = 0; ia < m_D3D11.m_IA.Bytecode.InputSig.Length; ia++)
                            {
                                if (m_D3D11.m_IA.Bytecode.InputSig[ia].semanticName.ToUpperInvariant() == layouts[i].SemanticName.ToUpperInvariant() &&
                                    m_D3D11.m_IA.Bytecode.InputSig[ia].semanticIndex == layouts[i].SemanticIndex)
                                {
                                    ret[i].Used = true;
                                    break;
                                }
                            }
                        }
                    }

                    return ret;
                }
                else if (IsLogD3D12)
                {
                    uint[] byteOffs = new uint[128];
                    for (int i = 0; i < 128; i++)
                        byteOffs[i] = 0;

                    var layouts = m_D3D12.m_IA.layouts;

                    VertexInputAttribute[] ret = new VertexInputAttribute[layouts.Length];
                    for (int i = 0; i < layouts.Length; i++)
                    {
                        bool needsSemanticIdx = false;
                        for (int j = 0; j < layouts.Length; j++)
                        {
                            if (i != j && layouts[i].SemanticName == layouts[j].SemanticName)
                            {
                                needsSemanticIdx = true;
                                break;
                            }
                        }

                        uint offs = layouts[i].ByteOffset;
                        if (offs == uint.MaxValue) // APPEND_ALIGNED
                            offs = byteOffs[layouts[i].InputSlot];
                        else
                            byteOffs[layouts[i].InputSlot] = offs = layouts[i].ByteOffset;

                        byteOffs[layouts[i].InputSlot] += layouts[i].Format.compByteWidth * layouts[i].Format.compCount;

                        ret[i].Name = layouts[i].SemanticName + (needsSemanticIdx ? layouts[i].SemanticIndex.ToString() : "");
                        ret[i].VertexBuffer = (int)layouts[i].InputSlot;
                        ret[i].RelativeByteOffset = offs;
                        ret[i].PerInstance = layouts[i].PerInstance;
                        ret[i].InstanceRate = (int)layouts[i].InstanceDataStepRate;
                        ret[i].Format = layouts[i].Format;
                        ret[i].GenericValue = null;
                        ret[i].Used = false;

                        if (m_D3D12.m_VS.ShaderDetails != null)
                        {
                            for (int ia = 0; ia < m_D3D12.m_VS.ShaderDetails.InputSig.Length; ia++)
                            {
                                if (m_D3D12.m_VS.ShaderDetails.InputSig[ia].semanticName.ToUpperInvariant() == layouts[i].SemanticName.ToUpperInvariant() &&
                                    m_D3D12.m_VS.ShaderDetails.InputSig[ia].semanticIndex == layouts[i].SemanticIndex)
                                {
                                    ret[i].Used = true;
                                    break;
                                }
                            }
                        }
                    }

                    return ret;
                }
                else if (IsLogGL)
                {
                    var attrs = m_GL.m_VtxIn.attributes;

                    int num = 0;
                    for (int i = 0; i < attrs.Length; i++)
                    {
                        int attrib = -1;
                        if (m_GL.m_VS.BindpointMapping != null && m_GL.m_VS.ShaderDetails != null)
                            attrib = m_GL.m_VS.BindpointMapping.InputAttributes[i];
                        else
                            attrib = i;

                        if (attrib >= 0)
                            num++;
                    }

                    int a = 0;
                    VertexInputAttribute[] ret = new VertexInputAttribute[num];
                    for (int i = 0; i < attrs.Length && a < num; i++)
                    {
                        ret[a].Name = String.Format("attr{0}", i);
                        ret[a].GenericValue = null;
                        ret[a].VertexBuffer = (int)attrs[i].BufferSlot;
                        ret[a].RelativeByteOffset = attrs[i].RelativeOffset;
                        ret[a].PerInstance = m_GL.m_VtxIn.vbuffers[attrs[i].BufferSlot].Divisor > 0;
                        ret[a].InstanceRate = (int)m_GL.m_VtxIn.vbuffers[attrs[i].BufferSlot].Divisor;
                        ret[a].Format = attrs[i].Format;
                        ret[a].Used = true;

                        if (m_GL.m_VS.BindpointMapping != null && m_GL.m_VS.ShaderDetails != null)
                        {
                            int attrib = m_GL.m_VS.BindpointMapping.InputAttributes[i];

                            if (attrib >= 0 && attrib < m_GL.m_VS.ShaderDetails.InputSig.Length)
                                ret[a].Name = m_GL.m_VS.ShaderDetails.InputSig[attrib].varName;

                            if (attrib == -1) continue;

                            if (!attrs[i].Enabled)
                            {
                                uint compCount = m_GL.m_VS.ShaderDetails.InputSig[attrib].compCount;
                                FormatComponentType compType = m_GL.m_VS.ShaderDetails.InputSig[attrib].compType;

                                ret[a].GenericValue = new object[compCount];

                                for (uint c = 0; c < compCount; c++)
                                {
                                    if (compType == FormatComponentType.Float)
                                        ret[a].GenericValue[c] = attrs[i].GenericValue.f[c];
                                    else if (compType == FormatComponentType.UInt)
                                        ret[a].GenericValue[c] = attrs[i].GenericValue.u[c];
                                    else if (compType == FormatComponentType.SInt)
                                        ret[a].GenericValue[c] = attrs[i].GenericValue.i[c];
                                    else if (compType == FormatComponentType.UScaled)
                                        ret[a].GenericValue[c] = (float)attrs[i].GenericValue.u[c];
                                    else if (compType == FormatComponentType.SScaled)
                                        ret[a].GenericValue[c] = (float)attrs[i].GenericValue.i[c];
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
                else if (IsLogVK)
                {
                    var attrs = m_Vulkan.VI.attrs;

                    int num = 0;
                    for (int i = 0; i < attrs.Length; i++)
                    {
                        int attrib = -1;
                        if (m_Vulkan.m_VS.BindpointMapping != null && m_Vulkan.m_VS.ShaderDetails != null)
                        {
                            if(attrs[i].location < m_Vulkan.m_VS.BindpointMapping.InputAttributes.Length)
                                attrib = m_Vulkan.m_VS.BindpointMapping.InputAttributes[attrs[i].location];
                        }
                        else
                            attrib = i;

                        if (attrib >= 0)
                            num++;
                    }

                    int a = 0;
                    VertexInputAttribute[] ret = new VertexInputAttribute[num];
                    for (int i = 0; i < attrs.Length && a < num; i++)
                    {
                        ret[a].Name = String.Format("attr{0}", i);
                        ret[a].GenericValue = null;
                        ret[a].VertexBuffer = (int)attrs[i].binding;
                        ret[a].RelativeByteOffset = attrs[i].byteoffset;
                        ret[a].PerInstance = false;
                        if(attrs[i].binding < m_Vulkan.VI.binds.Length)
                            ret[a].PerInstance = m_Vulkan.VI.binds[attrs[i].binding].perInstance;
                        ret[a].InstanceRate = 1;
                        ret[a].Format = attrs[i].format;
                        ret[a].Used = true;

                        if (m_Vulkan.m_VS.BindpointMapping != null && m_Vulkan.m_VS.ShaderDetails != null)
                        {
                            int attrib = -1;

                            if (attrs[i].location < m_Vulkan.m_VS.BindpointMapping.InputAttributes.Length)
                                attrib = m_Vulkan.m_VS.BindpointMapping.InputAttributes[attrs[i].location];

                            if (attrib >= 0 && attrib < m_Vulkan.m_VS.ShaderDetails.InputSig.Length)
                                ret[a].Name = m_Vulkan.m_VS.ShaderDetails.InputSig[attrib].varName;

                            if (attrib == -1) continue;
                        }

                        a++;
                    }

                    return ret;
                }
            }

            return null;
        }

        public void GetConstantBuffer(ShaderStageType stage, uint BufIdx, uint ArrayIdx, out ResourceId buf, out ulong ByteOffset, out ulong ByteSize)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    D3D11PipelineState.ShaderStage s = null;

                    switch (stage)
                    {
                        case ShaderStageType.Vertex: s = m_D3D11.m_VS; break;
                        case ShaderStageType.Domain: s = m_D3D11.m_DS; break;
                        case ShaderStageType.Hull: s = m_D3D11.m_HS; break;
                        case ShaderStageType.Geometry: s = m_D3D11.m_GS; break;
                        case ShaderStageType.Pixel: s = m_D3D11.m_PS; break;
                        case ShaderStageType.Compute: s = m_D3D11.m_CS; break;
                    }

                    if (s.ShaderDetails != null && BufIdx < s.ShaderDetails.ConstantBlocks.Length)
                    {
                        int bind = s.ShaderDetails.ConstantBlocks[BufIdx].bindPoint;

                        if (bind < s.ConstantBuffers.Length)
                        {
                            buf = s.ConstantBuffers[bind].Buffer;
                            ByteOffset = (ulong)(s.ConstantBuffers[bind].VecOffset * 4 * sizeof(float));
                            ByteSize = (ulong)(s.ConstantBuffers[bind].VecCount * 4 * sizeof(float));

                            return;
                        }
                    }
                }
                else if (IsLogD3D12)
                {
                    D3D12PipelineState.ShaderStage s = null;

                    switch (stage)
                    {
                        case ShaderStageType.Vertex: s = m_D3D12.m_VS; break;
                        case ShaderStageType.Domain: s = m_D3D12.m_DS; break;
                        case ShaderStageType.Hull: s = m_D3D12.m_HS; break;
                        case ShaderStageType.Geometry: s = m_D3D12.m_GS; break;
                        case ShaderStageType.Pixel: s = m_D3D12.m_PS; break;
                        case ShaderStageType.Compute: s = m_D3D12.m_CS; break;
                    }

                    if (s.ShaderDetails != null && BufIdx < s.ShaderDetails.ConstantBlocks.Length)
                    {
                        var bind = s.BindpointMapping.ConstantBlocks[s.ShaderDetails.ConstantBlocks[BufIdx].bindPoint];

                        if (bind.bindset >= s.Spaces.Length ||
                           bind.bind >= s.Spaces[bind.bindset].ConstantBuffers.Length)
                        {
                            buf = ResourceId.Null;
                            ByteOffset = 0;
                            ByteSize = 0;
                            return;
                        }

                        var descriptor = s.Spaces[bind.bindset].ConstantBuffers[bind.bind];

                        buf = descriptor.Buffer;
                        ByteOffset = descriptor.Offset;
                        ByteSize = descriptor.ByteSize;

                        return;
                    }
                }
                else if (IsLogGL)
                {
                    GLPipelineState.ShaderStage s = null;

                    switch (stage)
                    {
                        case ShaderStageType.Vertex: s = m_GL.m_VS; break;
                        case ShaderStageType.Tess_Control: s = m_GL.m_TCS; break;
                        case ShaderStageType.Tess_Eval: s = m_GL.m_TES; break;
                        case ShaderStageType.Geometry: s = m_GL.m_GS; break;
                        case ShaderStageType.Fragment: s = m_GL.m_FS; break;
                        case ShaderStageType.Compute: s = m_GL.m_CS; break;
                    }

                    if(s.ShaderDetails != null && BufIdx < s.ShaderDetails.ConstantBlocks.Length)
                    {
                        if (s.ShaderDetails.ConstantBlocks[BufIdx].bindPoint >= 0)
                        {
                            int uboIdx = s.BindpointMapping.ConstantBlocks[s.ShaderDetails.ConstantBlocks[BufIdx].bindPoint].bind;
                            if (uboIdx >= 0 && uboIdx < m_GL.UniformBuffers.Length)
                            {
                                var b = m_GL.UniformBuffers[uboIdx];

                                buf = b.Resource;
                                ByteOffset = b.Offset;
                                ByteSize = b.Size;

                                return;
                            }
                        }
                    }
                }
                else if (IsLogVK)
                {
                    VulkanPipelineState.Pipeline pipe = m_Vulkan.graphics;
                    if (stage == ShaderStageType.Compute)
                        pipe = m_Vulkan.compute;

                    VulkanPipelineState.ShaderStage s = null;

                    switch (stage)
                    {
                        case ShaderStageType.Vertex: s = m_Vulkan.m_VS; break;
                        case ShaderStageType.Tess_Control: s = m_Vulkan.m_TCS; break;
                        case ShaderStageType.Tess_Eval: s = m_Vulkan.m_TES; break;
                        case ShaderStageType.Geometry: s = m_Vulkan.m_GS; break;
                        case ShaderStageType.Fragment: s = m_Vulkan.m_FS; break;
                        case ShaderStageType.Compute: s = m_Vulkan.m_CS; break;
                    }

                    if (s.ShaderDetails != null && BufIdx < s.ShaderDetails.ConstantBlocks.Length)
                    {
                        var bind = s.BindpointMapping.ConstantBlocks[s.ShaderDetails.ConstantBlocks[BufIdx].bindPoint];

                        if (s.ShaderDetails.ConstantBlocks[BufIdx].bufferBacked == false)
                        {
                            // dummy values, it would be nice to fetch these properly
                            buf = ResourceId.Null;
                            ByteOffset = 0;
                            ByteSize = 1024;
                            return;
                        }

                        var descriptorBind = pipe.DescSets[bind.bindset].bindings[bind.bind].binds[ArrayIdx];

                        buf = descriptorBind.res;
                        ByteOffset = descriptorBind.offset;
                        ByteSize = descriptorBind.size;

                        return;
                    }
                }
            }

            buf = ResourceId.Null;
            ByteOffset = 0;
            ByteSize = 0;
        }

        public Dictionary<BindpointMap, BoundResource[]> GetReadOnlyResources(ShaderStageType stage)
        {
            var ret = new Dictionary<BindpointMap, BoundResource[]>();

            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    D3D11PipelineState.ShaderStage s = null;

                    switch (stage)
                    {
                        case ShaderStageType.Vertex: s = m_D3D11.m_VS; break;
                        case ShaderStageType.Domain: s = m_D3D11.m_DS; break;
                        case ShaderStageType.Hull: s = m_D3D11.m_HS; break;
                        case ShaderStageType.Geometry: s = m_D3D11.m_GS; break;
                        case ShaderStageType.Pixel: s = m_D3D11.m_PS; break;
                        case ShaderStageType.Compute: s = m_D3D11.m_CS; break;
                    }

                    for (int i = 0; i < s.SRVs.Length; i++)
                    {
                        var key = new BindpointMap(0, i);
                        var val = new BoundResource();

                        val.Id = s.SRVs[i].Resource;
                        val.HighestMip = (int)s.SRVs[i].HighestMip;
                        val.FirstSlice = (int)s.SRVs[i].FirstArraySlice;
                        val.typeHint = s.SRVs[i].Format.compType;

                        ret.Add(key, new BoundResource[] { val });
                    }

                    return ret;
                }
                else if (IsLogD3D12)
                {
                    D3D12PipelineState.ShaderStage s = null;

                    switch (stage)
                    {
                        case ShaderStageType.Vertex: s = m_D3D12.m_VS; break;
                        case ShaderStageType.Domain: s = m_D3D12.m_DS; break;
                        case ShaderStageType.Hull: s = m_D3D12.m_HS; break;
                        case ShaderStageType.Geometry: s = m_D3D12.m_GS; break;
                        case ShaderStageType.Pixel: s = m_D3D12.m_PS; break;
                        case ShaderStageType.Compute: s = m_D3D12.m_CS; break;
                    }

                    for (int space = 0; space < s.Spaces.Length; space++)
                    {
                        for (int reg = 0; reg < s.Spaces[space].SRVs.Length; reg++)
                        {
                            var bind = s.Spaces[space].SRVs[reg];
                            var key = new BindpointMap(space, reg);
                            var val = new BoundResource();

                            // consider this register to not exist - it's in a gap defined by sparse root signature elements
                            if (bind.RootElement == uint.MaxValue)
                                continue;

                            val = new BoundResource();
                            val.Id = bind.Resource;
                            val.HighestMip = (int)bind.HighestMip;
                            val.FirstSlice = (int)bind.FirstArraySlice;
                            val.typeHint = bind.Format.compType;

                            ret.Add(key, new BoundResource[] { val });
                        }
                    }

                    return ret;
                }
                else if (IsLogGL)
                {
                    for (int i = 0; i < m_GL.Textures.Length; i++)
                    {
                        var key = new BindpointMap(0, i);
                        var val = new BoundResource();

                        val.Id = m_GL.Textures[i].Resource;
                        val.HighestMip = (int)m_GL.Textures[i].HighestMip;
                        val.FirstSlice = (int)m_GL.Textures[i].FirstSlice;
                        val.typeHint = FormatComponentType.None;

                        ret.Add(key, new BoundResource[] { val });
                    }

                    return ret;
                }
                else if (IsLogVK)
                {
                    VulkanPipelineState.Pipeline.DescriptorSet[] descsets = m_Vulkan.graphics.DescSets;

                    if (stage == ShaderStageType.Compute)
                        descsets = m_Vulkan.compute.DescSets;

                    ShaderStageBits mask = (ShaderStageBits)(1 << (int)stage);

                    for (int set = 0; set < descsets.Length; set++)
                    {
                        var descset = descsets[set];
                        for (int slot = 0; slot < descset.bindings.Length; slot++)
                        {
                            var bind = descset.bindings[slot];
                            if ((bind.type == ShaderBindType.ImageSampler ||
                                bind.type == ShaderBindType.InputAttachment ||
                                bind.type == ShaderBindType.ReadOnlyImage ||
                                bind.type == ShaderBindType.ReadOnlyTBuffer
                               ) && (bind.stageFlags & mask) == mask)
                            {
                                var key = new BindpointMap(set, slot);
                                var val = new BoundResource[bind.descriptorCount];

                                for (UInt32 i = 0; i < bind.descriptorCount; i++)
                                {
                                    val[i] = new BoundResource();
                                    val[i].Id = bind.binds[i].res;
                                    val[i].HighestMip = (int)bind.binds[i].baseMip;
                                    val[i].FirstSlice = (int)bind.binds[i].baseLayer;
                                    val[i].typeHint = bind.binds[i].viewfmt.compType;
                                }

                                ret.Add(key, val);
                            }
                        }
                    }

                    return ret;
                }
            }

            return ret;
        }

        public Dictionary<BindpointMap, BoundResource[]> GetReadWriteResources(ShaderStageType stage)
        {
            var ret = new Dictionary<BindpointMap, BoundResource[]>();

            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    if (stage == ShaderStageType.Compute)
                    {
                        for (int i = 0; i < m_D3D11.m_CS.UAVs.Length; i++)
                        {
                            var key = new BindpointMap(0, i);
                            var val = new BoundResource();

                            val.Id = m_D3D11.m_CS.UAVs[i].Resource;
                            val.HighestMip = (int)m_D3D11.m_CS.UAVs[i].HighestMip;
                            val.FirstSlice = (int)m_D3D11.m_CS.UAVs[i].FirstArraySlice;
                            val.typeHint = m_D3D11.m_CS.UAVs[i].Format.compType;

                            ret.Add(key, new BoundResource[] { val });
                        }
                    }
                    else
                    {
                        int uavstart = (int)m_D3D11.m_OM.UAVStartSlot;

                        // up to UAVStartSlot treat these bindings as empty.
                        for (int i = 0; i < uavstart; i++)
                        {
                            var key = new BindpointMap(0, i);
                            var val = new BoundResource();

                            ret.Add(key, new BoundResource[] { val });
                        }

                        for (int i = 0; i < m_D3D11.m_OM.UAVs.Length - uavstart; i++)
                        {
                            // the actual UAV bindings start at the given slot
                            var key = new BindpointMap(0, i + uavstart);
                            var val = new BoundResource();

                            val.Id = m_D3D11.m_OM.UAVs[i].Resource;
                            val.HighestMip = (int)m_D3D11.m_OM.UAVs[i].HighestMip;
                            val.FirstSlice = (int)m_D3D11.m_OM.UAVs[i].FirstArraySlice;
                            val.typeHint = FormatComponentType.None;

                            ret.Add(key, new BoundResource[] { val });
                        }
                    }

                    return ret;
                }
                else if (IsLogD3D12)
                {
                    D3D12PipelineState.ShaderStage s = null;

                    switch (stage)
                    {
                        case ShaderStageType.Vertex: s = m_D3D12.m_VS; break;
                        case ShaderStageType.Domain: s = m_D3D12.m_DS; break;
                        case ShaderStageType.Hull: s = m_D3D12.m_HS; break;
                        case ShaderStageType.Geometry: s = m_D3D12.m_GS; break;
                        case ShaderStageType.Pixel: s = m_D3D12.m_PS; break;
                        case ShaderStageType.Compute: s = m_D3D12.m_CS; break;
                    }

                    for (int space = 0; space < s.Spaces.Length; space++)
                    {
                        for (int reg = 0; reg < s.Spaces[space].UAVs.Length; reg++)
                        {
                            var bind = s.Spaces[space].UAVs[reg];
                            var key = new BindpointMap(space, reg);
                            var val = new BoundResource();

                            // consider this register to not exist - it's in a gap defined by sparse root signature elements
                            if (bind.RootElement == uint.MaxValue)
                                continue;

                            val = new BoundResource();
                            val.Id = bind.Resource;
                            val.HighestMip = (int)bind.HighestMip;
                            val.FirstSlice = (int)bind.FirstArraySlice;
                            val.typeHint = bind.Format.compType;

                            ret.Add(key, new BoundResource[] { val });
                        }
                    }

                    return ret;
                }
                else if (IsLogGL)
                {
                    for (int i = 0; i < m_GL.Images.Length; i++)
                    {
                        var key = new BindpointMap(0, i);
                        var val = new BoundResource();

                        val.Id = m_GL.Images[i].Resource;
                        val.HighestMip = (int)m_GL.Images[i].Level;
                        val.FirstSlice = (int)m_GL.Images[i].Layer;
                        val.typeHint = m_GL.Images[i].Format.compType;

                        ret.Add(key, new BoundResource[] { val });
                    }

                    return ret;
                }
                else if (IsLogVK)
                {
                    VulkanPipelineState.Pipeline.DescriptorSet[] descsets = m_Vulkan.graphics.DescSets;

                    if (stage == ShaderStageType.Compute)
                        descsets = m_Vulkan.compute.DescSets;

                    ShaderStageBits mask = (ShaderStageBits)(1 << (int)stage);
                    for (int set = 0; set < descsets.Length; set++)
                    {
                        var descset = descsets[set];
                        for (int slot = 0; slot < descset.bindings.Length; slot++)
                        {
                            var bind = descset.bindings[slot];

                            if ((bind.type == ShaderBindType.ReadWriteBuffer ||
                                bind.type == ShaderBindType.ReadWriteImage ||
                                bind.type == ShaderBindType.ReadWriteTBuffer
                               ) && (bind.stageFlags & mask) == mask)
                            {
                                var key = new BindpointMap(set, slot);
                                var val = new BoundResource[bind.descriptorCount];

                                for (UInt32 i = 0; i < bind.descriptorCount; i++)
                                {
                                    val[i] = new BoundResource();
                                    val[i].Id = bind.binds[i].res;
                                    val[i].HighestMip = (int)bind.binds[i].baseMip;
                                    val[i].FirstSlice = (int)bind.binds[i].baseLayer;
                                    val[i].typeHint = bind.binds[i].viewfmt.compType;
                                }

                                ret.Add(key, val);
                            }
                        }
                    }

                    return ret;
                }
            }

            return ret;
        }

        public BoundResource GetDepthTarget()
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    var ret = new BoundResource();
                    ret.Id = m_D3D11.m_OM.DepthTarget.Resource;
                    ret.HighestMip = (int)m_D3D11.m_OM.DepthTarget.HighestMip;
                    ret.FirstSlice = (int)m_D3D11.m_OM.DepthTarget.FirstArraySlice;
                    ret.typeHint = m_D3D11.m_OM.DepthTarget.Format.compType;
                    return ret;
                }
                else if (IsLogD3D12)
                {
                    var ret = new BoundResource();
                    ret.Id = m_D3D12.m_OM.DepthTarget.Resource;
                    ret.HighestMip = (int)m_D3D12.m_OM.DepthTarget.HighestMip;
                    ret.FirstSlice = (int)m_D3D12.m_OM.DepthTarget.FirstArraySlice;
                    ret.typeHint = m_D3D12.m_OM.DepthTarget.Format.compType;
                    return ret;
                }
                else if (IsLogGL)
                {
                    var ret = new BoundResource();
                    ret.Id = m_GL.m_FB.m_DrawFBO.Depth.Obj;
                    ret.HighestMip = (int)m_GL.m_FB.m_DrawFBO.Depth.Mip;
                    ret.FirstSlice = (int)m_GL.m_FB.m_DrawFBO.Depth.Layer;
                    ret.typeHint = FormatComponentType.None;
                    return ret;
                }
                else if (IsLogVK)
                {
                    var rp = m_Vulkan.Pass.renderpass;
                    var fb = m_Vulkan.Pass.framebuffer;

                    if (rp.depthstencilAttachment >= 0 && rp.depthstencilAttachment < fb.attachments.Length)
                    {
                        var ret = new BoundResource();
                        ret.Id = fb.attachments[rp.depthstencilAttachment].img;
                        ret.HighestMip = (int)fb.attachments[rp.depthstencilAttachment].baseMip;
                        ret.FirstSlice = (int)fb.attachments[rp.depthstencilAttachment].baseLayer;
                        ret.typeHint = fb.attachments[rp.depthstencilAttachment].viewfmt.compType;
                        return ret;
                    }

                    return new BoundResource();
                }
            }

            return new BoundResource();
        }

        public BoundResource[] GetOutputTargets()
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    BoundResource[] ret = new BoundResource[m_D3D11.m_OM.RenderTargets.Length];
                    for (int i = 0; i < m_D3D11.m_OM.RenderTargets.Length; i++)
                    {
                        ret[i] = new BoundResource();
                        ret[i].Id = m_D3D11.m_OM.RenderTargets[i].Resource;
                        ret[i].HighestMip = (int)m_D3D11.m_OM.RenderTargets[i].HighestMip;
                        ret[i].FirstSlice = (int)m_D3D11.m_OM.RenderTargets[i].FirstArraySlice;
                        ret[i].typeHint = m_D3D11.m_OM.RenderTargets[i].Format.compType;
                    }

                    return ret;
                }
                else if (IsLogD3D12)
                {
                    BoundResource[] ret = new BoundResource[m_D3D12.m_OM.RenderTargets.Length];
                    for (int i = 0; i < m_D3D12.m_OM.RenderTargets.Length; i++)
                    {
                        ret[i] = new BoundResource();
                        ret[i].Id = m_D3D12.m_OM.RenderTargets[i].Resource;
                        ret[i].HighestMip = (int)m_D3D12.m_OM.RenderTargets[i].HighestMip;
                        ret[i].FirstSlice = (int)m_D3D12.m_OM.RenderTargets[i].FirstArraySlice;
                        ret[i].typeHint = m_D3D12.m_OM.RenderTargets[i].Format.compType;
                    }

                    return ret;
                }
                else if (IsLogGL)
                {
                    BoundResource[] ret = new BoundResource[m_GL.m_FB.m_DrawFBO.DrawBuffers.Length];
                    for (int i = 0; i < m_GL.m_FB.m_DrawFBO.DrawBuffers.Length; i++)
                    {
                        ret[i] = new BoundResource();

                        int db = m_GL.m_FB.m_DrawFBO.DrawBuffers[i];

                        if (db >= 0)
                        {
                            ret[i].Id = m_GL.m_FB.m_DrawFBO.Color[db].Obj;
                            ret[i].HighestMip = (int)m_GL.m_FB.m_DrawFBO.Color[db].Mip;
                            ret[i].FirstSlice = (int)m_GL.m_FB.m_DrawFBO.Color[db].Layer;
                            ret[i].typeHint = FormatComponentType.None;
                        }
                    }

                    return ret;
                }
                else if (IsLogVK)
                {
                    var rp = m_Vulkan.Pass.renderpass;
                    var fb = m_Vulkan.Pass.framebuffer;

                    int idx = 0;

                    BoundResource[] ret = new BoundResource[rp.colorAttachments.Length*2];
                    for (int i = 0; i < rp.colorAttachments.Length; i++)
                    {
                        ret[idx] = new BoundResource();

                        if(rp.colorAttachments[i] < fb.attachments.Length)
                        {
                            ret[idx].Id = fb.attachments[rp.colorAttachments[i]].img;
                            ret[idx].HighestMip = (int)fb.attachments[rp.colorAttachments[i]].baseMip;
                            ret[idx].FirstSlice = (int)fb.attachments[rp.colorAttachments[i]].baseLayer;
                            ret[idx].typeHint = fb.attachments[rp.colorAttachments[i]].viewfmt.compType;
                        }

                        idx++;
                    }
                    for (int i = 0; i < rp.resolveAttachments.Length; i++)
                    {
                        ret[idx] = new BoundResource();

                        if (rp.resolveAttachments[i] < fb.attachments.Length)
                        {
                            ret[idx].Id = fb.attachments[rp.resolveAttachments[i]].img;
                            ret[idx].HighestMip = (int)fb.attachments[rp.resolveAttachments[i]].baseMip;
                            ret[idx].FirstSlice = (int)fb.attachments[rp.resolveAttachments[i]].baseLayer;
                            ret[idx].typeHint = fb.attachments[rp.resolveAttachments[i]].viewfmt.compType;
                        }

                        idx++;
                    }

                    return ret;
                }
            }

            return new BoundResource[0];
        }

        // Still to add:
        // [ShaderViewer]   * {FetchTexture,FetchBuffer} GetFetchBufferOrFetchTexture(ShaderResource) 
    }
}
