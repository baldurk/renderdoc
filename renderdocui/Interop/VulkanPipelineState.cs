/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2017 Baldur Karlsson
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
using System.Collections.Generic;

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
                    public UInt32 descriptorCount;
                    public ShaderBindType type;
                    public ShaderStageBits stageFlags;

                    [StructLayout(LayoutKind.Sequential)]
                    public class BindingElement
                    {
                        public ResourceId view;
                        public ResourceId res;
                        public ResourceId sampler;
                        public bool immutableSampler;

                        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                        public string SamplerName;
                        public bool customSamplerName;

                        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                        public ResourceFormat viewfmt;

                        [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
                        public TextureSwizzle[] swizzle;

                        public UInt32 baseMip;
                        public UInt32 baseLayer;
                        public UInt32 numMip;
                        public UInt32 numLayer;

                        public UInt64 offset;
                        public UInt64 size;

                        public TextureFilter Filter;

                        public AddressMode addrU;
                        public AddressMode addrV;
                        public AddressMode addrW;

                        public float mipBias;
                        public float maxAniso;
                        public CompareFunc comparison;
                        public float minlod, maxlod;
                        [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
                        public float[] BorderColor;
                        public bool unnormalized;

                        public bool UseBorder()
                        {
                            return addrU == AddressMode.ClampBorder ||
                                   addrV == AddressMode.ClampBorder ||
                                   addrW == AddressMode.ClampBorder;
                        }
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
            public string entryPoint;

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
            struct SpecInfo
            {
                public UInt32 specID;
                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public byte[] data;
            };
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            SpecInfo[] specialization;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderStage m_VS, m_TCS, m_TES, m_GS, m_FS, m_CS;

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
                    public Int32 x, y, width, height;
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
            public bool depthClampEnable;
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
            public bool alphaToOneEnable;
            public bool logicOpEnable;

            public LogicOp Logic;

            [StructLayout(LayoutKind.Sequential)]
            public class Attachment
            {
                public bool blendEnable;

                [StructLayout(LayoutKind.Sequential)]
                public class BlendEquation
                {
                    public BlendMultiplier Source;
                    public BlendMultiplier Destination;
                    public BlendOp Operation;
                };
                [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                public BlendEquation m_Blend, m_AlphaBlend;

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
            public CompareFunc depthCompareOp;

            public bool stencilTestEnable;
            [StructLayout(LayoutKind.Sequential)]
            public class StencilFace
            {
                public StencilOp FailOp;
                public StencilOp DepthFailOp;
                public StencilOp PassOp;
                public CompareFunc Func;

                public UInt32 stencilref, compareMask, writeMask;
            };
            [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
            public StencilFace front, back;

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
                [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
                public UInt32[] resolveAttachments;
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

                    [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
                    public ResourceFormat viewfmt;

                    [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4)]
                    public TextureSwizzle[] swizzle;

                    public UInt32 baseMip;
                    public UInt32 baseLayer;
                    public UInt32 numMip;
                    public UInt32 numLayer;
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

        [StructLayout(LayoutKind.Sequential)]
        public class ImageData
        {
            public ResourceId image;

            [StructLayout(LayoutKind.Sequential)]
            public class ImageLayout
            {
                public UInt32 baseMip;
                public UInt32 baseLayer;
                public UInt32 numMip;
                public UInt32 numLayer;

                [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
                public string name;
            };

            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ImageLayout[] layouts;
        };

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        private ImageData[] images_;

        // add to dictionary for convenience
        private void PostMarshal()
        {
            Images = new Dictionary<ResourceId, ImageData>();

            foreach (ImageData i in images_)
                Images.Add(i.image, i);
        }

        [CustomMarshalAs(CustomUnmanagedType.Skip)]
        public Dictionary<ResourceId, ImageData> Images;
    };
}
