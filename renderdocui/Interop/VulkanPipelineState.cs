/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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
    public class VulkanPipelineState
    {
        [StructLayout(LayoutKind.Sequential)]
        public class Pipeline
        {
            public ResourceId obj;
            public UInt32 flags;

            [StructLayout(LayoutKind.Sequential)]
            public class DescriptorSet
            {
                public ResourceId layout;
                public ResourceId descset;

                [StructLayout(LayoutKind.Sequential)]
                public class DescriptorBinding
                {
                    public UInt32 arraySize;
                    public ShaderBindType type;
                    public ShaderStageBits stageFlags;

                    [StructLayout(LayoutKind.Sequential)]
                    public class BindingElement
                    {
                        public ResourceId view;
                        public ResourceId res;
                        public ResourceId sampler;
                        public UInt64 offset;
                        public UInt64 size;
                    };
                    [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                    public BindingElement[] binds;
                };
                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public DescriptorBinding[] bindings;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public DescriptorSet[] DescSets;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Pipeline compute;
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Pipeline graphics;

        [StructLayout(LayoutKind.Sequential)]
        public class InputAssembly
        {
            public bool primitiveRestartEnable;

            [StructLayout(LayoutKind.Sequential)]
            public class IndexBuffer
            {
                public ResourceId buf;
                public UInt64 offs;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public IndexBuffer ibuffer;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public InputAssembly IA;

        [StructLayout(LayoutKind.Sequential)]
        public class VertexInput
        {
            [StructLayout(LayoutKind.Sequential)]
            public class Attribute
            {
                public UInt32 location;
                public UInt32 binding;
                [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                public ResourceFormat format;
                public UInt32 byteoffset;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public Attribute[] attrs;

            [StructLayout(LayoutKind.Sequential)]
            public class Binding
            {
                public UInt32 vbufferBinding;
                public UInt32 bytestride;
                public bool perInstance;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public Binding[] binds;

            [StructLayout(LayoutKind.Sequential)]
            public class VertexBuffer
            {
                public ResourceId buffer;
                public UInt64 offset;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public VertexBuffer[] vbuffers;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public VertexInput VI;

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
            public bool customName;
            private IntPtr _ptr_ShaderDetails;
            [CustomMarshalAs(CustomUnmanagedType.Skip)]
            public ShaderReflection ShaderDetails;
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public ShaderBindpointMapping BindpointMapping;

            public ShaderStageType stage;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderStage VS, TCS, TES, GS, FS, CS;

        [StructLayout(LayoutKind.Sequential)]
        public class Tessellation
        {
            public UInt32 numControlPoints;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Tessellation Tess;
        
        [StructLayout(LayoutKind.Sequential)]
        public class ViewState
        {
            [StructLayout(LayoutKind.Sequential)]
            public class ViewportScissor
            {
                [StructLayout(LayoutKind.Sequential)]
                public class Viewport
                {
                    public float x, y;
                    public float Width, Height;
                    public float MinDepth, MaxDepth;
                };
                [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                public Viewport vp;

                [StructLayout(LayoutKind.Sequential)]
                public class Scissor
                {
                    public Int32 x, y, right, bottom;
                };
                [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                public Scissor scissor;
            };

            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ViewportScissor[] viewportScissors;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ViewState VP;

        [StructLayout(LayoutKind.Sequential)]
        public class Raster
        {
            public bool depthClipEnable;
            public bool rasterizerDiscardEnable;
            public bool FrontCCW;
            public TriangleFillMode FillMode;
            public TriangleCullMode CullMode;

            // from dynamic state
            public float depthBias;
            public float depthBiasClamp;
            public float slopeScaledDepthBias;
            public float lineWidth;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public Raster RS;

        [StructLayout(LayoutKind.Sequential)]
        public class MultiSample
        {
            public UInt32 rasterSamples;
            public bool sampleShadingEnable;
            public float minSampleShading;
            public UInt32 sampleMask;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public MultiSample MSAA;

        [StructLayout(LayoutKind.Sequential)]
        public class ColorBlend
        {
            public bool alphaToCoverageEnable;
            public bool logicOpEnable;

            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string LogicOp;

            [StructLayout(LayoutKind.Sequential)]
            public class Attachment
            {
                public bool blendEnable;

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

                public byte WriteMask;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public Attachment[] attachments;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
            public float[] blendConst;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ColorBlend CB;


        [StructLayout(LayoutKind.Sequential)]
        public class DepthStencil
        {
            public bool depthTestEnable;
            public bool depthWriteEnable;
            public bool depthBoundsEnable;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string depthCompareOp;

            public bool stencilTestEnable;
            [StructLayout(LayoutKind.Sequential)]
            public class StencilOp
            {
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string failOp;
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string depthFailOp;
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string passOp;
                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string func;

                public UInt32 stencilref, compareMask, writeMask;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public StencilOp front, back;

            public float minDepthBounds;
            public float maxDepthBounds;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public DepthStencil DS;
        
        [StructLayout(LayoutKind.Sequential)]
        public class CurrentPass
        {
            [StructLayout(LayoutKind.Sequential)]
            public class RenderPass
            {
                public ResourceId obj;

                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public UInt32[] inputAttachments;
                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public UInt32[] colorAttachments;
                public Int32 depthstencilAttachment;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public RenderPass renderpass;

            [StructLayout(LayoutKind.Sequential)]
            public class Framebuffer
            {
                public ResourceId obj;

                [StructLayout(LayoutKind.Sequential)]
                public class Attachment
                {
                    public ResourceId view;
                    public ResourceId img;
                };
                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public Attachment[] attachments;

                public UInt32 width, height, layers;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public Framebuffer framebuffer;

            [StructLayout(LayoutKind.Sequential)]
            public class RenderArea
            {
                public Int32 x, y, width, height;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public RenderArea renderArea;

        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public CurrentPass Pass;
    };
}
