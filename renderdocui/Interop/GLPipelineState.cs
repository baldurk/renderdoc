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
                public FloatVector GenericValue;
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
                public bool PerInstance;
                public UInt32 Divisor;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public VertexBuffer[] vbuffers;

            public ResourceId ibuffer;
            public bool primitiveRestart;
            public UInt32 restartIndex;
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
            private IntPtr _ptr_ShaderDetails;
            [CustomMarshalAs(CustomUnmanagedType.Skip)]
            public ShaderReflection ShaderDetails;
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public ShaderBindpointMapping BindpointMapping;

            public ShaderStageType stage;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderStage m_VS, m_TCS, m_TES, m_GS, m_FS, m_CS;

        [StructLayout(LayoutKind.Sequential)]
        public class Texture
        {
            public ResourceId Resource;

            public UInt32 FirstSlice;
        };
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public Texture[] Textures;

        [StructLayout(LayoutKind.Sequential)]
        public class Buffer
        {
            public ResourceId Resource;

            public UInt64 Offset;
            public UInt64 Size;
        };
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public Buffer[] UniformBuffers;

        [StructLayout(LayoutKind.Sequential)]
        public class Rasterizer
        {
            [StructLayout(LayoutKind.Sequential)]
            public class Viewport
            {
                public float Left, Bottom;
                public float Width, Height;
                public double MinDepth, MaxDepth;
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
                public bool DepthClamp;
                public bool MultisampleEnable;
                public bool AntialiasedLineEnable;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public RasterizerState m_State;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Rasterizer m_RS;

        [StructLayout(LayoutKind.Sequential)]
        public class FrameBuffer
        {
            public ResourceId FBO;

            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ResourceId[] Color;
            public ResourceId Depth;
            public ResourceId Stencil;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public FrameBuffer m_FB;
    }
}
