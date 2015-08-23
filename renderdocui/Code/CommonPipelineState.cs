/******************************************************************************
 * The MIT License (MIT)
 * 
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
    public class CommonPipelineState
    {
        private D3D11PipelineState m_D3D11 = null;
        private GLPipelineState m_GL = null;
        private APIProperties m_APIProps = null;

        public CommonPipelineState()
        {
        }

        public void SetStates(APIProperties props, D3D11PipelineState d3d11, GLPipelineState gl)
        {
            m_APIProps = props;
            m_D3D11 = d3d11;
            m_GL = gl;
        }

        private bool LogLoaded
        {
            get
            {
                return m_D3D11 != null || m_GL != null;
            }
        }

        private bool IsLogD3D11
        {
            get
            {
                return LogLoaded && m_APIProps.pipelineType == APIPipelineStateType.D3D11 && m_D3D11 != null;
            }
        }

        private bool IsLogGL
        {
            get
            {
                return LogLoaded && m_APIProps.pipelineType == APIPipelineStateType.OpenGL && m_GL != null;
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

                    if (IsLogGL)
                        return m_GL != null && m_GL.m_TES.Shader != ResourceId.Null;
                }

                return false;
            }
        }

        // there's a lot of redundancy in these functions

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
            }

            return null;
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
                else if (IsLogGL)
                {
                    switch (stage)
                    {
                        case ShaderStageType.Vertex: return String.Format("Shader {0}", m_GL.m_VS.Shader);
                        case ShaderStageType.Tess_Control: return String.Format("Shader {0}", m_GL.m_TCS.Shader);
                        case ShaderStageType.Tess_Eval: return String.Format("Shader {0}", m_GL.m_TES.Shader);
                        case ShaderStageType.Geometry: return String.Format("Shader {0}", m_GL.m_GS.Shader);
                        case ShaderStageType.Fragment: return String.Format("Shader {0}", m_GL.m_FS.Shader);
                        case ShaderStageType.Compute: return String.Format("Shader {0}", m_GL.m_CS.Shader);
                    }
                }
            }

            return "";
        }

        public void GetIBuffer(out ResourceId buf, out uint ByteOffset)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    buf = m_D3D11.m_IA.ibuffer.Buffer;
                    ByteOffset = m_D3D11.m_IA.ibuffer.Offset;

                    return;
                }
                else if (IsLogGL)
                {
                    buf = m_GL.m_VtxIn.ibuffer;
                    ByteOffset = 0; // GL only has per-draw index offset

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
                else if (IsLogGL)
                {
                    return m_GL.m_VtxIn.primitiveRestart;
                }
            }

            return false;
        }

        public uint GetStripRestartIndex(uint indexByteWidth)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    // D3D11 this is always '-1' in whichever size of index we're using
                    return indexByteWidth == 2 ? ushort.MaxValue : uint.MaxValue;
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

        public struct VBuffer
        {
            public ResourceId Buffer;
            public uint ByteOffset;
            public uint ByteStride;
        };

        public VBuffer[] GetVBuffers()
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    VBuffer[] ret = new VBuffer[m_D3D11.m_IA.vbuffers.Length];
                    for (int i = 0; i < m_D3D11.m_IA.vbuffers.Length; i++)
                    {
                        ret[i].Buffer = m_D3D11.m_IA.vbuffers[i].Buffer;
                        ret[i].ByteOffset = m_D3D11.m_IA.vbuffers[i].Offset;
                        ret[i].ByteStride = m_D3D11.m_IA.vbuffers[i].Stride;
                    }

                    return ret;
                }
                else if (IsLogGL)
                {
                    VBuffer[] ret = new VBuffer[m_GL.m_VtxIn.vbuffers.Length];
                    for (int i = 0; i < m_GL.m_VtxIn.vbuffers.Length; i++)
                    {
                        ret[i].Buffer = m_GL.m_VtxIn.vbuffers[i].Buffer;
                        ret[i].ByteOffset = m_GL.m_VtxIn.vbuffers[i].Offset;
                        ret[i].ByteStride = m_GL.m_VtxIn.vbuffers[i].Stride;
                    }

                    return ret;
                }
            }

            return null;
        }

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
            }

            return null;
        }

        public void GetConstantBuffer(ShaderStageType stage, uint BufIdx, out ResourceId buf, out uint ByteOffset, out uint ByteSize)
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

                    if(BufIdx < s.ConstantBuffers.Length)
                    {
                        buf = s.ConstantBuffers[BufIdx].Buffer;
                        ByteOffset = s.ConstantBuffers[BufIdx].VecOffset * 4 * sizeof(float);
                        ByteSize = s.ConstantBuffers[BufIdx].VecCount * 4 * sizeof(float);

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
                                ByteOffset = (uint)b.Offset;
                                ByteSize = (uint)b.Size;

                                return;
                            }
                        }
                    }
                }
            }

            buf = ResourceId.Null;
            ByteOffset = 0;
            ByteSize = 0;
        }

        public ResourceId[] GetResources(ShaderStageType stage)
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

                    ResourceId[] ret = new ResourceId[s.SRVs.Length];
                    for (int i = 0; i < s.SRVs.Length; i++)
                        ret[i] = s.SRVs[i].Resource;

                    return ret;
                }
                else if (IsLogGL)
                {
                    ResourceId[] ret = new ResourceId[m_GL.Textures.Length];
                    for (int i = 0; i < m_GL.Textures.Length; i++)
                        ret[i] = m_GL.Textures[i].Resource;

                    return ret;
                }
            }

            return new ResourceId[0];
        }

        public ResourceId[] GetReadWriteResources(ShaderStageType stage)
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    if (stage == ShaderStageType.Compute)
                    {
                        ResourceId[] ret = new ResourceId[m_D3D11.m_CS.UAVs.Length];
                        for (int i = 0; i < m_D3D11.m_CS.UAVs.Length; i++)
                            ret[i] = m_D3D11.m_CS.UAVs[i].Resource;

                        return ret;
                    }
                    else
                    {
                        ResourceId[] ret = new ResourceId[m_D3D11.m_OM.UAVs.Length];
                        for (int i = 0; i < m_D3D11.m_OM.UAVs.Length; i++)
                            ret[i] = m_D3D11.m_OM.UAVs[i].Resource;

                        return ret;
                    }
                }
                else if (IsLogGL)
                {
                    ResourceId[] ret = new ResourceId[m_GL.Images.Length];
                    for (int i = 0; i < m_GL.Images.Length; i++)
                        ret[i] = m_GL.Images[i].Resource;

                    return ret;
                }
            }

            return new ResourceId[0];
        }

        public ResourceId GetDepthTarget()
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                    return m_D3D11.m_OM.DepthTarget.Resource;
                else if (IsLogGL)
                    return m_GL.m_FB.m_DrawFBO.Depth.Obj;
            }

            return ResourceId.Null;
        }

        public ResourceId GetStencilTarget()
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                    return m_D3D11.m_OM.DepthTarget.Resource;
                else if (IsLogGL)
                    return m_GL.m_FB.m_DrawFBO.Stencil.Obj;
            }

            return ResourceId.Null;
        }

        public ResourceId[] GetOutputTargets()
        {
            if (LogLoaded)
            {
                if (IsLogD3D11)
                {
                    ResourceId[] ret = new ResourceId[m_D3D11.m_OM.RenderTargets.Length];
                    for (int i = 0; i < m_D3D11.m_OM.RenderTargets.Length; i++)
                        ret[i] = m_D3D11.m_OM.RenderTargets[i].Resource;

                    return ret;
                }
                else if (IsLogGL)
                {
                    ResourceId[] ret = new ResourceId[m_GL.m_FB.m_DrawFBO.DrawBuffers.Length];
                    for(int i=0; i < m_GL.m_FB.m_DrawFBO.DrawBuffers.Length; i++)
                    {
                        int db = m_GL.m_FB.m_DrawFBO.DrawBuffers[i];
                        if(db >= 0)
                            ret[i] = m_GL.m_FB.m_DrawFBO.Color[db].Obj;
                    }

                    return ret;
                }
            }

            return new ResourceId[0];
        }

        public ResourceId OutputDepth
        {
            get
            {
                if (LogLoaded)
                {
                    if (IsLogD3D11)
                        return m_D3D11.m_OM.DepthTarget.Resource;

                    if (IsLogGL)
                        return m_GL.m_FB.m_DrawFBO.Depth.Obj;
                }

                return ResourceId.Null;
            }
        }

        public ResourceId OutputStencil
        {
            get
            {
                if (LogLoaded)
                {
                    if (IsLogD3D11)
                        return m_D3D11.m_OM.DepthTarget.Resource;

                    if (IsLogGL)
                        return m_GL.m_FB.m_DrawFBO.Stencil.Obj;
                }

                return ResourceId.Null;
            }
        }

        // Still to add:
        // [ShaderViewer]   * {FetchTexture,FetchBuffer} GetFetchBufferOrFetchTexture(ShaderResource) 
    }
}
