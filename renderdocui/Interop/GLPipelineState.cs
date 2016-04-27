/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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
    public class GLPipelineState
    {
        [StructLayout(LayoutKind.Sequential)]
        public class VertexInputs
        {
            [StructLayout(LayoutKind.Sequential)]
            public class VertexAttribute
            {
                public bool Enabled;
                [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                public ResourceFormat Format;

                [StructLayout(LayoutKind.Sequential)]
                public struct GenericValueUnion
                {
                    [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4, FixedType = CustomFixedType.Float)]
                    public float[] f;

                    [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4, FixedType = CustomFixedType.UInt32)]
                    public UInt32[] u;

                    [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4, FixedType = CustomFixedType.Int32)]
                    public Int32[] i;
                };

                [CustomMarshalAs(CustomUnmanagedType.Union)]
                public GenericValueUnion GenericValue;

                public UInt32 BufferSlot;
                public UInt32 RelativeOffset;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public VertexAttribute[] attributes;

            [StructLayout(LayoutKind.Sequential)]
            public class VertexBuffer
            {
                public ResourceId Buffer;
                public UInt32 Stride;
                public UInt32 Offset;
                public UInt32 Divisor;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public VertexBuffer[] vbuffers;

            public ResourceId ibuffer;
            public bool primitiveRestart;
            public UInt32 restartIndex;

            public bool provokingVertexLast;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public VertexInputs m_VtxIn;

        [StructLayout(LayoutKind.Sequential)]
        public class ShaderStage
        {
            private void PostMarshal()
            {
                if (_ptr_ShaderDetails != IntPtr.Zero)
                    ShaderDetails = (ShaderReflection)CustomMarshal.PtrToStructure(_ptr_ShaderDetails, typeof(ShaderReflection), false);
                else
                    ShaderDetails = null;

                _ptr_ShaderDetails = IntPtr.Zero;
            }

            public ResourceId Shader;

            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string ShaderName;
            public bool customShaderName;

            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string ProgramName;
            public bool customProgramName;

            public bool PipelineActive;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string PipelineName;
            public bool customPipelineName;

            private IntPtr _ptr_ShaderDetails;
            [CustomMarshalAs(CustomUnmanagedType.Skip)]
            public ShaderReflection ShaderDetails;
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public ShaderBindpointMapping BindpointMapping;

            public ShaderStageType stage;

            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public UInt32[] Subroutines;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderStage m_VS, m_TCS, m_TES, m_GS, m_FS, m_CS;

        [StructLayout(LayoutKind.Sequential)]
        public class FixedVertexProcessing
        {
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 2)]
            public float[] defaultInnerLevel;
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
            public float[] defaultOuterLevel;
            public bool discard;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 8)]
            public bool[] clipPlanes;
            public bool clipOriginLowerLeft;
            public bool clipNegativeOneToOne;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public FixedVertexProcessing m_VtxProcess;

        [StructLayout(LayoutKind.Sequential)]
        public class Texture
        {
            public ResourceId Resource;
            public UInt32 FirstSlice;
            public UInt32 HighestMip;
            public ShaderResourceType ResType;
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
            public TextureSwizzle[] Swizzle;
            public Int32 DepthReadChannel;
        };
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public Texture[] Textures;

        [StructLayout(LayoutKind.Sequential)]
        public class Sampler
        {
            public ResourceId Samp;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string AddressS, AddressT, AddressR;
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
            public float[] BorderColor;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string Comparison;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string MinFilter;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string MagFilter;
            public bool UseBorder;
            public bool UseComparison;
            public bool SeamlessCube;
            public float MaxAniso;
            public float MaxLOD;
            public float MinLOD;
            public float MipLODBias;
        };
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public Sampler[] Samplers;

        [StructLayout(LayoutKind.Sequential)]
        public class Buffer
        {
            public ResourceId Resource;

            public UInt64 Offset;
            public UInt64 Size;
        };
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public Buffer[] AtomicBuffers;
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public Buffer[] UniformBuffers;
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public Buffer[] ShaderStorageBuffers;

        [StructLayout(LayoutKind.Sequential)]
        public class ImageLoadStore
        {
            public ResourceId Resource;
            public UInt32 Level;
            public bool Layered;
            public UInt32 Layer;
            public ShaderResourceType ResType;
            public bool readAllowed;
            public bool writeAllowed;
            public ResourceFormat Format;
        };
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ImageLoadStore[] Images;

        [StructLayout(LayoutKind.Sequential)]
        public class Feedback
        {
            public ResourceId Obj;
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
            public ResourceId[] BufferBinding;
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
            public UInt64[] Offset;
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
            public UInt64[] Size;
            public bool Active;
            public bool Paused;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Feedback m_Feedback;

        [StructLayout(LayoutKind.Sequential)]
        public class Rasterizer
        {
            [StructLayout(LayoutKind.Sequential)]
            public class Viewport
            {
                public float Left, Bottom;
                public float Width, Height;
                public double MinDepth, MaxDepth;
                public bool Enabled;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public Viewport[] Viewports;

            [StructLayout(LayoutKind.Sequential)]
            public class Scissor
            {
                public Int32 Left, Bottom;
                public Int32 Width, Height;
                public bool Enabled;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public Scissor[] Scissors;

            [StructLayout(LayoutKind.Sequential)]
            public class RasterizerState
            {
                public TriangleFillMode FillMode;
                public TriangleCullMode CullMode;
                public bool FrontCCW;
                public float DepthBias;
                public float SlopeScaledDepthBias;
                public float OffsetClamp;
                public bool DepthClamp;

                public bool MultisampleEnable;
                public bool SampleShading;
                public bool SampleMask;
                public UInt32 SampleMaskValue;
                public bool SampleCoverage;
                public bool SampleCoverageInvert;
                public float SampleCoverageValue;
                public bool SampleAlphaToCoverage;
                public bool SampleAlphaToOne;
                public float MinSampleShadingRate;

                public bool ProgrammablePointSize;
                public float PointSize;
                public float LineWidth;
                public float PointFadeThreshold;
                public bool PointOriginUpperLeft;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public RasterizerState m_State;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Rasterizer m_RS;

        [StructLayout(LayoutKind.Sequential)]
        public class DepthState
        {
            public bool DepthEnable;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string DepthFunc;
            public bool DepthWrites;
            public bool DepthBounds;
            public double NearBound;
            public double FarBound;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public DepthState m_DepthState;

        [StructLayout(LayoutKind.Sequential)]
        public class StencilState
        {
            public bool StencilEnable;

            [StructLayout(LayoutKind.Sequential)]
            public class StencilOp
            {
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string FailOp;
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string DepthFailOp;
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string PassOp;
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string Func;
                public UInt32 Ref;
                public UInt32 ValueMask;
                public UInt32 WriteMask;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public StencilOp m_FrontFace, m_BackFace;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public StencilState m_StencilState;

        [StructLayout(LayoutKind.Sequential)]
        public class FrameBuffer
        {
            public bool FramebufferSRGB;
            
            [StructLayout(LayoutKind.Sequential)]
            public class Attachment
            {
                public ResourceId Obj;
                public UInt32 Layer;
                public UInt32 Mip;
            };

            [StructLayout(LayoutKind.Sequential)]
            public class FBO
            {
                public ResourceId Obj;

                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public Attachment[] Color;
                public Attachment Depth;
                public Attachment Stencil;

                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public Int32[] DrawBuffers;
                public Int32 ReadBuffer;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public FBO m_DrawFBO, m_ReadFBO;

            [StructLayout(LayoutKind.Sequential)]
            public class BlendState
            {
                [StructLayout(LayoutKind.Sequential)]
                public class RTBlend
                {
                    [StructLayout(LayoutKind.Sequential)]
                    public class BlendOp
                    {
                        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                        public string Source;
                        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                        public string Destination;
                        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                        public string Operation;
                    };
                    [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                    public BlendOp m_Blend, m_AlphaBlend;

                    [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                    public string LogicOp;

                    public bool Enabled;
                    public byte WriteMask;
                };
                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public RTBlend[] Blends;

                [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
                public float[] BlendFactor;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public BlendState m_BlendState;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public FrameBuffer m_FB;

        [StructLayout(LayoutKind.Sequential)]
        public class Hints
        {
            public Int32 Derivatives;
            public Int32 LineSmooth;
            public Int32 PolySmooth;
            public Int32 TexCompression;
            public bool LineSmoothEnabled;
            public bool PolySmoothEnabled;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Hints m_Hints;
    }
}
