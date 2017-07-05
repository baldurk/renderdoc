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
using System.Runtime.InteropServices;

namespace renderdoc
{
    [StructLayout(LayoutKind.Sequential)]
    public class D3D11PipelineState
    {
        [StructLayout(LayoutKind.Sequential)]
        public class InputAssembler
        {
            private void PostMarshal()
            {
                if (_ptr_Bytecode != IntPtr.Zero)
                {
                    Bytecode = (ShaderReflection)CustomMarshal.PtrToStructure(_ptr_Bytecode, typeof(ShaderReflection), false);
                    Bytecode.origPtr = _ptr_Bytecode;
                }
                else
                {
                    Bytecode = null;
                }

                _ptr_Bytecode = IntPtr.Zero;
            }

            [StructLayout(LayoutKind.Sequential)]
            public class LayoutInput
            {
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string SemanticName;
                public UInt32 SemanticIndex;
                [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                public ResourceFormat Format;
                public UInt32 InputSlot;
                public UInt32 ByteOffset;
                public bool PerInstance;
                public UInt32 InstanceDataStepRate;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public LayoutInput[] layouts;
            public ResourceId layout;
            private IntPtr _ptr_Bytecode;
            [CustomMarshalAs(CustomUnmanagedType.Skip)]
            public ShaderReflection Bytecode;
            public bool customName;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string LayoutName;

            [StructLayout(LayoutKind.Sequential)]
            public class VertexBuffer
            {
                public ResourceId Buffer;
                public UInt32 Stride;
                public UInt32 Offset;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public VertexBuffer[] vbuffers;

            [StructLayout(LayoutKind.Sequential)]
            public class IndexBuffer
            {
                public ResourceId Buffer;
                public UInt32 Offset;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public IndexBuffer ibuffer;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public InputAssembler m_IA;

        [StructLayout(LayoutKind.Sequential)]
        public class ShaderStage
        {
            private void PostMarshal()
            {
                if (_ptr_ShaderDetails != IntPtr.Zero)
                {
                    ShaderDetails = (ShaderReflection)CustomMarshal.PtrToStructure(_ptr_ShaderDetails, typeof(ShaderReflection), false);
                    ShaderDetails.origPtr = _ptr_ShaderDetails;
                }
                else
                {
                    ShaderDetails = null;
                }

                _ptr_ShaderDetails = IntPtr.Zero;
            }

            public ResourceId Shader;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string ShaderName;
            public bool customName;
            private IntPtr _ptr_ShaderDetails;
            [CustomMarshalAs(CustomUnmanagedType.Skip)]
            public ShaderReflection ShaderDetails;
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public ShaderBindpointMapping BindpointMapping;

            public ShaderStageType stage;

            [StructLayout(LayoutKind.Sequential)]
            public class ResourceView
            {
                public ResourceId View;
                public ResourceId Resource;
                public ShaderResourceType Type;
                [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                public ResourceFormat Format;

                public bool Structured;
                public UInt32 BufferStructCount;
                public UInt32 ElementSize;

                // Buffer
                public UInt32 FirstElement;
                public UInt32 NumElements;

                // BufferEx
                public D3DBufferViewFlags Flags;

                // Texture
                public UInt32 HighestMip;
                public UInt32 NumMipLevels;

                // Texture Array
                public UInt32 ArraySize;
                public UInt32 FirstArraySlice;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ResourceView[] SRVs;
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ResourceView[] UAVs;

            [StructLayout(LayoutKind.Sequential)]
            public class Sampler
            {
                public ResourceId Samp;

                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string SamplerName;
                public bool customSamplerName;

                public AddressMode AddressU, AddressV, AddressW;
                [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
                public float[] BorderColor;
                public CompareFunc Comparison;
                [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                public TextureFilter Filter;
                public UInt32 MaxAniso;
                public float MaxLOD;
                public float MinLOD;
                public float MipLODBias;

                public bool UseBorder()
                {
                    return AddressU == AddressMode.ClampBorder ||
                           AddressV == AddressMode.ClampBorder ||
                           AddressW == AddressMode.ClampBorder;
                }
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public Sampler[] Samplers;

            [StructLayout(LayoutKind.Sequential)]
            public class CBuffer
            {
                public ResourceId Buffer;
                public UInt32 VecOffset;
                public UInt32 VecCount;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public CBuffer[] ConstantBuffers;

            [StructLayout(LayoutKind.Sequential)]
            public class ClassInstance
            {
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                string name;
            };

            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ClassInstance[] ClassInstances;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderStage m_VS, m_HS, m_DS, m_GS, m_PS, m_CS;

        [StructLayout(LayoutKind.Sequential)]
        public class Streamout
        {
            [StructLayout(LayoutKind.Sequential)]
            public class Output
            {
                public ResourceId Buffer;
                public UInt32 Offset;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public Output[] Outputs;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Streamout m_SO;

        [StructLayout(LayoutKind.Sequential)]
        public class Rasterizer
        {
            [StructLayout(LayoutKind.Sequential)]
            public class Viewport
            {
                [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 2)]
                public float[] TopLeft;
                public float Width, Height;
                public float MinDepth, MaxDepth;
                public bool Enabled;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public Viewport[] Viewports;

            [StructLayout(LayoutKind.Sequential)]
            public class Scissor
            {
                public Int32 left, top, right, bottom;
                public bool Enabled;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public Scissor[] Scissors;

            [StructLayout(LayoutKind.Sequential)]
            public class RasterizerState
            {
                public ResourceId State;
                public TriangleFillMode FillMode;
                public TriangleCullMode CullMode;
                public bool FrontCCW;
                public Int32 DepthBias;
                public float DepthBiasClamp;
                public float SlopeScaledDepthBias;
                public bool DepthClip;
                public bool ScissorEnable;
                public bool MultisampleEnable;
                public bool AntialiasedLineEnable;
                public UInt32 ForcedSampleCount;
                public bool ConservativeRasterization;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public RasterizerState m_State;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Rasterizer m_RS;

        [StructLayout(LayoutKind.Sequential)]
        public class OutputMerger
        {
            [StructLayout(LayoutKind.Sequential)]
            public class DepthStencilState
            {
                public ResourceId State;
                public bool DepthEnable;
                public CompareFunc DepthFunc;
                public bool DepthWrites;
                public bool StencilEnable;
                public byte StencilReadMask;
                public byte StencilWriteMask;

                [StructLayout(LayoutKind.Sequential)]
                public class StencilFace
                {
                    public StencilOp FailOp;
                    public StencilOp DepthFailOp;
                    public StencilOp PassOp;
                    public CompareFunc Func;
                };
                [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                public StencilFace m_FrontFace, m_BackFace;

                public UInt32 StencilRef;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public DepthStencilState m_State;

            [StructLayout(LayoutKind.Sequential)]
            public class BlendState
            {
                public ResourceId State;

                public bool AlphaToCoverage;
                public bool IndependentBlend;

                [StructLayout(LayoutKind.Sequential)]
                public class RTBlend
                {
                    [StructLayout(LayoutKind.Sequential)]
                    public class BlendEquation
                    {
                        public BlendMultiplier Source;
                        public BlendMultiplier Destination;
                        public BlendOp Operation;
                    };
                    [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                    public BlendEquation m_Blend, m_AlphaBlend;

                    public LogicOp Logic;

                    public bool Enabled;
                    public bool LogicEnabled;
                    public byte WriteMask;
                };
                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public RTBlend[] Blends;

                [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
                public float[] BlendFactor;
                public UInt32 SampleMask;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public BlendState m_BlendState;

            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ShaderStage.ResourceView[] RenderTargets;

            public UInt32 UAVStartSlot;
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ShaderStage.ResourceView[] UAVs;

            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public ShaderStage.ResourceView DepthTarget;
            public bool DepthReadOnly;
            public bool StencilReadOnly;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public OutputMerger m_OM;
    };
}
