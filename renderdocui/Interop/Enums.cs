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

// from replay_enums.h

namespace renderdoc
{
    [Flags]
    public enum DirectoryFileProperty
    {
        Directory = 0x1,
        Hidden = 0x2,
        Executable = 0x4,

        ErrorUnknown = 0x2000,
        ErrorAccessDenied = 0x4000,
        ErrorInvalidPath = 0x8000,
    };

    public enum VarType
    {
        Float = 0,
        Int,
        UInt,
        Double,
    };

    public enum FormatComponentType
    {
        None = 0,
        Float,
        UNorm,
        SNorm,
        UInt,
        SInt,
        UScaled,
        SScaled,
        Depth,
        Double,
    };

    public enum TextureSwizzle
    {
        Red,
        Green,
        Blue,
        Alpha,
        Zero,
        One,
    };

    public enum AddressMode
    {
        Wrap,
        Mirror,
        MirrorOnce,
        ClampEdge,
        ClampBorder,
    };

    public enum ShaderResourceType
    {
        None,
        Buffer,
        Texture1D,
        Texture1DArray,
        Texture2D,
        TextureRect,
        Texture2DArray,
        Texture2DMS,
        Texture2DMSArray,
        Texture3D,
        TextureCube,
        TextureCubeArray,
    };

    public enum ShaderBindType
    {
        Unknown = 0,
        ConstantBuffer,
        Sampler,
        ImageSampler,
        ReadOnlyImage,
        ReadWriteImage,
        ReadOnlyTBuffer,
        ReadWriteTBuffer,
        ReadOnlyBuffer,
        ReadWriteBuffer,
        InputAttachment,
    };

    public enum SystemAttribute
    {
        None = 0,
        Position,
        PointSize,
        ClipDistance,
        CullDistance,
        RTIndex,
        ViewportIndex,
        VertexIndex,
        PrimitiveIndex,
        InstanceIndex,
        DispatchSize,
        DispatchThreadIndex,
        GroupIndex,
        GroupFlatIndex,
        GroupThreadIndex,
        GSInstanceIndex,
        OutputControlPointIndex,
        DomainLocation,
        IsFrontFace,
        MSAACoverage,
        MSAASamplePosition,
        MSAASampleIndex,
        PatchNumVertices,
        OuterTessFactor,
        InsideTessFactor,
        ColourOutput,
        DepthOutput,
        DepthOutputGreaterEqual,
        DepthOutputLessEqual,
    };

    // replay_render.h

    public enum OutputType
    {
        None = 0,
        TexDisplay,
        MeshDisplay,
    };

    public enum MeshDataStage
    {
        Unknown = 0,
        VSIn,
        VSOut,
        GSOut,
    };

    public enum TextureDisplayOverlay
    {
        None = 0,
        Drawcall,
        Wireframe,
        Depth,
        Stencil,
        BackfaceCull,
        ViewportScissor,
        NaN,
        Clipping,
        ClearBeforePass,
        ClearBeforeDraw,
        QuadOverdrawPass,
        QuadOverdrawDraw,
        TriangleSizePass,
        TriangleSizeDraw,
    };

    public enum FileType
    {
        DDS,
        PNG,
        JPG,
        BMP,
        TGA,
        HDR,
        EXR,
    };

    public enum AlphaMapping
    {
        Discard,
        BlendToColour,
        BlendToCheckerboard,
        Preserve,
    };

    public enum SpecialFormat
    {
        Unknown = 0,
        BC1,
        BC2,
        BC3,
        BC4,
        BC5,
        BC6,
        BC7,
        ETC2,
        EAC,
        ASTC,
        R10G10B10A2,
        R11G11B10,
        R5G6B5,
        R5G5B5A1,
        R9G9B9E5,
        R4G4B4A4,
        R4G4,
        D16S8,
        D24S8,
        D32S8,
        S8,
        YUV,
    };

    public enum QualityHint
    {
      DontCare,
      Nicest,
      Fastest,
    };

    public enum GraphicsAPI
    {
        D3D11,
        D3D12,
        OpenGL,
        Vulkan,
    };

    public enum PrimitiveTopology
    {
        Unknown,
        PointList,
        LineList,
        LineStrip,
        LineLoop,
        TriangleList,
        TriangleStrip,
        TriangleFan,
        LineList_Adj,
        LineStrip_Adj,
        TriangleList_Adj,
        TriangleStrip_Adj,
        PatchList,
        PatchList_1CPs = PatchList,
        PatchList_2CPs,
        PatchList_3CPs,
        PatchList_4CPs,
        PatchList_5CPs,
        PatchList_6CPs,
        PatchList_7CPs,
        PatchList_8CPs,
        PatchList_9CPs,
        PatchList_10CPs,
        PatchList_11CPs,
        PatchList_12CPs,
        PatchList_13CPs,
        PatchList_14CPs,
        PatchList_15CPs,
        PatchList_16CPs,
        PatchList_17CPs,
        PatchList_18CPs,
        PatchList_19CPs,
        PatchList_20CPs,
        PatchList_21CPs,
        PatchList_22CPs,
        PatchList_23CPs,
        PatchList_24CPs,
        PatchList_25CPs,
        PatchList_26CPs,
        PatchList_27CPs,
        PatchList_28CPs,
        PatchList_29CPs,
        PatchList_30CPs,
        PatchList_31CPs,
        PatchList_32CPs,
    };

    [Flags]
    public enum BufferCreationFlags
    {
        VB = 0x1,
        IB = 0x2,
        CB = 0x4,
        UAV = 0x8,
        Indirect = 0x10,
    };

    [Flags]
    public enum TextureCreationFlags
    {
        SRV = 0x1,
        RTV = 0x2,
        DSV = 0x4,
        UAV = 0x8,
        SwapBuffer = 0x10,
    };

    [Flags]
    public enum D3DBufferViewFlags
    {
        Raw = 0x1,
        Append = 0x2,
        Counter = 0x4,
    };

    public enum ShaderStageType
    {
        Vertex = 0,
        First = Vertex,

        Hull,
        Tess_Control = Hull,

        Domain,
        Tess_Eval = Domain,

        Geometry,

        Pixel,
        Fragment = Pixel,

        Compute,

        Count,
    };

    [Flags]
    public enum ShaderStageBits
    {
        None         = 0,
        Vertex       = (1 << ShaderStageType.Vertex),
        Hull         = (1 << ShaderStageType.Hull),
        Tess_Control = (1 << ShaderStageType.Tess_Control),
        Domain       = (1 << ShaderStageType.Domain),
        Tess_Eval    = (1 << ShaderStageType.Tess_Eval),
        Geometry     = (1 << ShaderStageType.Geometry),
        Pixel        = (1 << ShaderStageType.Pixel),
        Fragment     = (1 << ShaderStageType.Fragment),
        Compute      = (1 << ShaderStageType.Compute),
        All          = (Vertex | Hull | Domain | Geometry | Pixel | Fragment | Compute),
    };

    [Flags]
    public enum ShaderDebugStateFlags
    {
        SampleLoadGather = 0x1,
        GeneratedNanOrInf = 0x2,
    };

    public enum DebugMessageSource
    {
        API = 0,
        RedundantAPIUse,
        IncorrectAPIUse,
        GeneralPerformance,
        GCNPerformance,
        RuntimeWarning,
        UnsupportedConfiguration,
    };

    public enum DebugMessageCategory
    {
        Defined = 0,
        Miscellaneous,
        Initialization,
        Cleanup,
        Compilation,
        Creation,
        Setting,
        Getting,
        Manipulation,
        Execution,
        Shaders,
        Deprecated,
        Undefined,
        Portability,
        Performance,
    };

    public enum DebugMessageSeverity
    {
        High = 0,
        Medium,
        Low,
        Info,
    };

    public enum ResourceUsage
    {
    	None,

    	VertexBuffer,
    	IndexBuffer,

    	VS_Constants,
    	HS_Constants,
    	DS_Constants,
    	GS_Constants,
    	PS_Constants,
    	CS_Constants,
        All_Constants,

    	SO,

    	VS_Resource,
    	HS_Resource,
    	DS_Resource,
    	GS_Resource,
    	PS_Resource,
    	CS_Resource,
        All_Resource,

        VS_RWResource,
        HS_RWResource,
        DS_RWResource,
        GS_RWResource,
        PS_RWResource,
        CS_RWResource,
        All_RWResource,

        InputTarget,
    	ColourTarget,
    	DepthStencilTarget,

        Indirect,

        Clear,

        GenMips,
        Resolve,
        ResolveSrc,
        ResolveDst,
        Copy,
        CopySrc,
        CopyDst,

        Barrier,
    };

    [Flags]
    public enum DrawcallFlags
    {
        // types
        Clear        = 0x0001,
        Drawcall     = 0x0002,
        Dispatch     = 0x0004,
        CmdList      = 0x0008,
        SetMarker    = 0x0010,
        PushMarker   = 0x0020,
        PopMarker    = 0x0040, // this is only for internal tracking use
        Present      = 0x0080,
        MultiDraw    = 0x0100,
        Copy         = 0x0200,
        Resolve      = 0x0400,
        GenMips      = 0x0800,
        PassBoundary = 0x1000,

        // flags
        UseIBuffer        = 0x010000,
        Instanced         = 0x020000,
        Auto              = 0x040000,
        Indirect          = 0x080000,
        ClearColour       = 0x100000,
        ClearDepthStencil = 0x200000,
        BeginPass         = 0x400000,
        EndPass           = 0x800000,
        APICalls          = 0x1000000,
    };

    public enum SolidShadeMode
    {
        None = 0,
        Solid,
        Lit,
        Secondary,
    };

    public enum TriangleFillMode
    {
        Solid = 0,
        Wireframe,
        Point
    };

    public enum TriangleCullMode
    {
        None = 0,
        Front,
        Back,
        FrontAndBack,
    };

    public enum FilterMode
    {
        NoFilter,
        Point,
        Linear,
        Cubic,
        Anisotropic,
    };

    public enum FilterFunc
    {
        Normal,
        Comparison,
        Minimum,
        Maximum,
    };

    public enum CompareFunc
    {
        Never,
        AlwaysTrue,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        Equal,
        NotEqual,
    };

    public enum StencilOp
    {
        Keep,
        Zero,
        Replace,
        IncSat,
        DecSat,
        IncWrap,
        DecWrap,
        Invert,
    };

    public enum BlendMultiplier
    {
        Zero,
        One,
        SrcCol,
        InvSrcCol,
        DstCol,
        InvDstCol,
        SrcAlpha,
        InvSrcAlpha,
        DstAlpha,
        InvDstAlpha,
        SrcAlphaSat,
        FactorRGB,
        InvFactorRGB,
        FactorAlpha,
        InvFactorAlpha,
        Src1Col,
        InvSrc1Col,
        Src1Alpha,
        InvSrc1Alpha,
    };

    public enum BlendOp
    {
        Add,
        Subtract,
        ReversedSubtract,
        Minimum,
        Maximum,
    };

    public enum LogicOp
    {
        NoOp,
        Clear,
        Set,
        Copy,
        CopyInverted,
        Invert,
        And,
        Nand,
        Or,
        Xor,
        Nor,
        Equivalent,
        AndReverse,
        AndInverted,
        OrReverse,
        OrInverted,
    };

    public enum GPUCounters : uint
    {
        FirstGeneric = 1,
        EventGPUDuration = FirstGeneric,
        InputVerticesRead,
        IAPrimitives,
        GSPrimitives,
        RasterizerInvocations,
        RasterizedPrimitives,
        SamplesWritten,
        VSInvocations,
        HSInvocations,
        DSInvocations,
        TESInvocations = DSInvocations,
        GSInvocations,
        PSInvocations,
        CSInvocations,

        FirstAMD = 1000000,

        FirstIntel = 2000000,

        FirstNvidia = 3000000,
    };

    public enum CounterUnits
    {
        Absolute,
        Seconds,
        Percentage,
    };

    public enum ReplaySupport
    {
        Unsupported,
        Supported,
        SuggestRemote,
    };

    public enum ReplayCreateStatus
    {
        Success = 0,
        UnknownError,
        InternalError,
        FileNotFound,
        InjectionFailed,
        IncompatibleProcess,
        NetworkIOFailed,
        NetworkRemoteBusy,
        NetworkVersionMismatch,
        FileIOFailed,
        FileIncompatibleVersion,
        FileCorrupted,
        ImageUnsupported,
        APIUnsupported,
        APIInitFailed,
        APIIncompatibleVersion,
        APIHardwareUnsupported,
    };

    public enum TargetControlMessageType
    {
        Unknown = 0,
        Disconnected,
        Busy,
        Noop,
        NewCapture,
        CaptureCopied,
        RegisterAPI,
        NewChild,
    };

    public enum EnvironmentModificationType
    {
        Set,
        Append,
        Prepend,
    };

    public enum EnvironmentSeparator
    {
        Platform,
        SemiColon,
        Colon,
        None,
    };

    public static class EnumString
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_VertexOffset(PrimitiveTopology topology, UInt32 prim);

        public static UInt32 GetVertexOffset(this PrimitiveTopology topology, UInt32 primitiveIndex)
        {
            return RENDERDOC_VertexOffset(topology, primitiveIndex);
        }

        public static bool IsD3D(this GraphicsAPI apitype)
        {
            return (apitype == GraphicsAPI.D3D11 || apitype == GraphicsAPI.D3D12);
        }

        public static string Str(this DebugMessageSource source)
        {
            switch (source)
            {
                case DebugMessageSource.API: return "API's debug messages";
                case DebugMessageSource.RedundantAPIUse: return "Redundant use of API";
                case DebugMessageSource.IncorrectAPIUse: return "Incorrect use of API";
                case DebugMessageSource.GeneralPerformance: return "General Performance issues";
                case DebugMessageSource.GCNPerformance: return "GCN (AMD) Performance issues";
                case DebugMessageSource.RuntimeWarning: return "Issues raised while debugging";
                case DebugMessageSource.UnsupportedConfiguration: return "Unsupported Software or Hardware Configuration";
            }

            return "Unknown Source";
        }

        public static string Str(this VarType type)
        {
            switch (type)
            {
                case VarType.Double: return "double";
                case VarType.Float: return "float";
                case VarType.Int: return "int";
                case VarType.UInt: return "uint";
            }

            return "Unknown Type";
        }

        public static string Str(this TextureSwizzle swiz)
        {
            switch (swiz)
            {
                case TextureSwizzle.Red: return "R";
                case TextureSwizzle.Green: return "G";
                case TextureSwizzle.Blue: return "B";
                case TextureSwizzle.Alpha: return "A";
                case TextureSwizzle.Zero: return "0";
                case TextureSwizzle.One: return "1";
            }

            return "Unknown";
        }

        public static string Str(this ReplayCreateStatus status)
        {
            switch (status)
            {
                case ReplayCreateStatus.Success: return "Success";
                case ReplayCreateStatus.UnknownError: return "Unknown Error";
                case ReplayCreateStatus.InternalError: return "Internal Error";
                case ReplayCreateStatus.FileNotFound: return "File not found";
                case ReplayCreateStatus.InjectionFailed: return "RenderDoc injection failed";
                case ReplayCreateStatus.IncompatibleProcess: return "Process is incompatible (likely 64-bit/32-bit issue)";
                case ReplayCreateStatus.NetworkIOFailed: return "Network I/O operation failed";
                case ReplayCreateStatus.NetworkRemoteBusy: return "Remote side of network connection is busy";
                case ReplayCreateStatus.NetworkVersionMismatch: return "Version mismatch between network clients";
                case ReplayCreateStatus.FileIOFailed: return "File I/O operation failed";
                case ReplayCreateStatus.FileIncompatibleVersion: return "File is of an incompatible version";
                case ReplayCreateStatus.FileCorrupted: return "File is corrupted or unrecognisable format";
                case ReplayCreateStatus.ImageUnsupported: return "The contents or format of the image is not supported";
                case ReplayCreateStatus.APIUnsupported: return "API used is not supported";
                case ReplayCreateStatus.APIInitFailed: return "Replay API failed to initialise";
                case ReplayCreateStatus.APIIncompatibleVersion: return "API-specific data used is of an incompatible version";
                case ReplayCreateStatus.APIHardwareUnsupported: return "Your hardware or software configuration doesn't meet this API's minimum requirements";
            }

            return "Unknown Error Code";
        }

        public static string Str(this PrimitiveTopology topo)
        {
            switch (topo)
            {
                case PrimitiveTopology.Unknown: return "Unknown";
                case PrimitiveTopology.PointList: return "Point List";
                case PrimitiveTopology.LineList: return "Line List";
                case PrimitiveTopology.LineStrip: return "Line Strip";
                case PrimitiveTopology.LineLoop: return "Line Loop";
                case PrimitiveTopology.TriangleList: return "Triangle List";
                case PrimitiveTopology.TriangleStrip: return "Triangle Strip";
                case PrimitiveTopology.TriangleFan: return "Triangle Fan";
                case PrimitiveTopology.LineList_Adj: return "Line List with Adjacency";
                case PrimitiveTopology.LineStrip_Adj: return "Line Strip with Adjacency";
                case PrimitiveTopology.TriangleList_Adj: return "Triangle List with Adjacency";
                case PrimitiveTopology.TriangleStrip_Adj: return "Triangle Strip with Adjacency";
                default: break;
            }

            if (topo >= PrimitiveTopology.PatchList)
                return String.Format("Patch List {0} Control Points", (int)topo - (int)PrimitiveTopology.PatchList_1CPs + 1);

            return "Unknown";
        }
        
        public static string Str(this ShaderResourceType type)
        {
            switch (type)
            {
                case ShaderResourceType.None: return "None";
                case ShaderResourceType.Buffer: return "Buffer";
                case ShaderResourceType.Texture1D: return "1D";
                case ShaderResourceType.Texture1DArray: return "1D Array";
                case ShaderResourceType.Texture2D: return "2D";
                case ShaderResourceType.TextureRect: return "Rect";
                case ShaderResourceType.Texture2DArray: return "2D Array";
                case ShaderResourceType.Texture2DMS: return "2D MS";
                case ShaderResourceType.Texture2DMSArray: return "2D MS Array";
                case ShaderResourceType.Texture3D: return "3D";
                case ShaderResourceType.TextureCube: return "Cube";
                case ShaderResourceType.TextureCubeArray: return "Cube Array";
            }

            return "Unknown resource type";
        }

        public static string Str(this ResourceUsage usage, GraphicsAPI apitype)
        {
            if (apitype.IsD3D())
            {
                switch (usage)
                {
                    case ResourceUsage.VertexBuffer: return "Vertex Buffer";
                    case ResourceUsage.IndexBuffer: return "Index Buffer";

                    case ResourceUsage.VS_Constants: return "VS - Constant Buffer";
                    case ResourceUsage.GS_Constants: return "GS - Constant Buffer";
                    case ResourceUsage.HS_Constants: return "HS - Constant Buffer";
                    case ResourceUsage.DS_Constants: return "DS - Constant Buffer";
                    case ResourceUsage.CS_Constants: return "CS - Constant Buffer";
                    case ResourceUsage.PS_Constants: return "PS - Constant Buffer";
                    case ResourceUsage.All_Constants: return "All - Constant Buffer";

                    case ResourceUsage.SO: return "Stream Out";

                    case ResourceUsage.VS_Resource: return "VS - Resource";
                    case ResourceUsage.GS_Resource: return "GS - Resource";
                    case ResourceUsage.HS_Resource: return "HS - Resource";
                    case ResourceUsage.DS_Resource: return "DS - Resource";
                    case ResourceUsage.CS_Resource: return "CS - Resource";
                    case ResourceUsage.PS_Resource: return "PS - Resource";
                    case ResourceUsage.All_Resource: return "All - Resource";

                    case ResourceUsage.VS_RWResource: return "VS - UAV";
                    case ResourceUsage.HS_RWResource: return "HS - UAV";
                    case ResourceUsage.DS_RWResource: return "DS - UAV";
                    case ResourceUsage.GS_RWResource: return "GS - UAV";
                    case ResourceUsage.PS_RWResource: return "PS - UAV";
                    case ResourceUsage.CS_RWResource: return "CS - UAV";
                    case ResourceUsage.All_RWResource: return "All - UAV";

                    case ResourceUsage.InputTarget: return "Colour Input";
                    case ResourceUsage.ColourTarget: return "Rendertarget";
                    case ResourceUsage.DepthStencilTarget: return "Depthstencil";

                    case ResourceUsage.Indirect: return "Indirect argument";

                    case ResourceUsage.Clear: return "Clear";

                    case ResourceUsage.GenMips: return "Generate Mips";
                    case ResourceUsage.Resolve: return "Resolve";
                    case ResourceUsage.ResolveSrc: return "Resolve - Source";
                    case ResourceUsage.ResolveDst: return "Resolve - Dest";
                    case ResourceUsage.Copy: return "Copy";
                    case ResourceUsage.CopySrc: return "Copy - Source";
                    case ResourceUsage.CopyDst: return "Copy - Dest";

                    case ResourceUsage.Barrier: return "Barrier";
                }
            }
            else if (apitype == GraphicsAPI.OpenGL || apitype == GraphicsAPI.Vulkan)
            {
                bool vk = (apitype == GraphicsAPI.Vulkan);

                switch (usage)
                {
                    case ResourceUsage.VertexBuffer: return "Vertex Buffer";
                    case ResourceUsage.IndexBuffer: return "Index Buffer";

                    case ResourceUsage.VS_Constants: return "VS - Uniform Buffer";
                    case ResourceUsage.GS_Constants: return "GS - Uniform Buffer";
                    case ResourceUsage.HS_Constants: return "HS - Uniform Buffer";
                    case ResourceUsage.DS_Constants: return "DS - Uniform Buffer";
                    case ResourceUsage.CS_Constants: return "CS - Uniform Buffer";
                    case ResourceUsage.PS_Constants: return "PS - Uniform Buffer";
                    case ResourceUsage.All_Constants: return "All - Uniform Buffer";

                    case ResourceUsage.SO: return "Transform Feedback";

                    case ResourceUsage.VS_Resource: return "VS - Texture";
                    case ResourceUsage.GS_Resource: return "GS - Texture";
                    case ResourceUsage.HS_Resource: return "HS - Texture";
                    case ResourceUsage.DS_Resource: return "DS - Texture";
                    case ResourceUsage.CS_Resource: return "CS - Texture";
                    case ResourceUsage.PS_Resource: return "PS - Texture";
                    case ResourceUsage.All_Resource: return "All - Texture";

                    case ResourceUsage.VS_RWResource: return "VS - Image/SSBO";
                    case ResourceUsage.HS_RWResource: return "HS - Image/SSBO";
                    case ResourceUsage.DS_RWResource: return "DS - Image/SSBO";
                    case ResourceUsage.GS_RWResource: return "GS - Image/SSBO";
                    case ResourceUsage.PS_RWResource: return "PS - Image/SSBO";
                    case ResourceUsage.CS_RWResource: return "CS - Image/SSBO";
                    case ResourceUsage.All_RWResource: return "All - Image/SSBO";

                    case ResourceUsage.InputTarget: return "FBO Input";
                    case ResourceUsage.ColourTarget: return "FBO Colour";
                    case ResourceUsage.DepthStencilTarget: return "FBO Depthstencil";

                    case ResourceUsage.Indirect: return "Indirect argument";

                    case ResourceUsage.Clear: return "Clear";

                    case ResourceUsage.GenMips: return "Generate Mips";
                    case ResourceUsage.Resolve: return vk ? "Resolve" : "Framebuffer blit";
                    case ResourceUsage.ResolveSrc: return vk ? "Resolve - Source" : "Framebuffer blit - Source";
                    case ResourceUsage.ResolveDst: return vk ? "Resolve - Dest" : "Framebuffer blit - Dest";
                    case ResourceUsage.Copy: return "Copy";
                    case ResourceUsage.CopySrc: return "Copy - Source";
                    case ResourceUsage.CopyDst: return "Copy - Dest";

                    case ResourceUsage.Barrier: return "Barrier";
                }
            }

            return "Unknown Usage String";
        }

        public static string Str(this ShaderStageType stage, GraphicsAPI apitype)
        {
            if (apitype.IsD3D())
            {
                switch (stage)
                {
                    case ShaderStageType.Vertex: return "Vertex";
                    case ShaderStageType.Hull: return "Hull";
                    case ShaderStageType.Domain: return "Domain";
                    case ShaderStageType.Geometry: return "Geometry";
                    case ShaderStageType.Pixel: return "Pixel";
                    case ShaderStageType.Compute: return "Compute";
                }
            }
            else if (apitype == GraphicsAPI.OpenGL || apitype == GraphicsAPI.Vulkan)
            {
                switch (stage)
                {
                    case ShaderStageType.Vertex: return "Vertex";
                    case ShaderStageType.Tess_Control: return "Tess. Control";
                    case ShaderStageType.Tess_Eval: return "Tess. Eval";
                    case ShaderStageType.Geometry: return "Geometry";
                    case ShaderStageType.Fragment: return "Fragment";
                    case ShaderStageType.Compute: return "Compute";
                }
            }

            return stage.ToString();
        }

        public static string Str(this SystemAttribute systemValue)
        {
            switch (systemValue)
            {
                case SystemAttribute.None: return "";
                case SystemAttribute.Position: return "SV_Position";
                case SystemAttribute.ClipDistance: return "SV_ClipDistance";
                case SystemAttribute.CullDistance: return "SV_CullDistance";
                case SystemAttribute.RTIndex: return "SV_RenderTargetIndex";
                case SystemAttribute.ViewportIndex: return "SV_ViewportIndex";
                case SystemAttribute.VertexIndex: return "SV_VertexID";
                case SystemAttribute.PrimitiveIndex: return "SV_PrimitiveID";
                case SystemAttribute.InstanceIndex: return "SV_InstanceID";
                case SystemAttribute.DispatchThreadIndex: return "SV_DispatchThreadID";
                case SystemAttribute.GroupIndex: return "SV_GroupID";
                case SystemAttribute.GroupFlatIndex: return "SV_GroupIndex";
                case SystemAttribute.GroupThreadIndex: return "SV_GroupThreadID";
                case SystemAttribute.GSInstanceIndex: return "SV_GSInstanceID";
                case SystemAttribute.OutputControlPointIndex: return "SV_OutputControlPointID";
                case SystemAttribute.DomainLocation: return "SV_DomainLocation";
                case SystemAttribute.IsFrontFace: return "SV_IsFrontFace";
                case SystemAttribute.MSAACoverage: return "SV_Coverage";
                case SystemAttribute.MSAASampleIndex: return "SV_SampleIndex";
                case SystemAttribute.OuterTessFactor: return "SV_TessFactor";
                case SystemAttribute.InsideTessFactor: return "SV_InsideTessFactor";
                case SystemAttribute.ColourOutput: return "SV_Target";
                case SystemAttribute.DepthOutput: return "SV_Depth";
                case SystemAttribute.DepthOutputGreaterEqual: return "SV_DepthGreaterEqual";
                case SystemAttribute.DepthOutputLessEqual: return "SV_DepthLessEqual";
            }

            return "SV_Unknown";
        }

        public static string Str(this ShaderBindType bindType)
        {
            switch (bindType)
            {
                case ShaderBindType.ConstantBuffer:   return "Constants";
                case ShaderBindType.Sampler:          return "Sampler";
                case ShaderBindType.ImageSampler:     return "Image&Sampler";
                case ShaderBindType.ReadOnlyImage:    return "Image";
                case ShaderBindType.ReadWriteImage:   return "RW Image";
                case ShaderBindType.ReadOnlyTBuffer:  return "TBuffer";
                case ShaderBindType.ReadWriteTBuffer: return "RW TBuffer";
                case ShaderBindType.ReadOnlyBuffer:   return "Buffer";
                case ShaderBindType.ReadWriteBuffer:  return "RW Buffer";
                case ShaderBindType.InputAttachment:  return "Input";
                default: break;
            }

            return "Unknown";
        }

        public static string Str(this FormatComponentType compType)
        {
            switch (compType)
            {
                case FormatComponentType.None: return "Typeless";
                case FormatComponentType.Float: return "Float";
                case FormatComponentType.UNorm: return "UNorm";
                case FormatComponentType.SNorm: return "SNorm";
                case FormatComponentType.UInt: return "UInt";
                case FormatComponentType.SInt: return "SInt";
                case FormatComponentType.UScaled: return "UScaled";
                case FormatComponentType.SScaled: return "SScaled";
                case FormatComponentType.Depth: return "Depth/Stencil";
                case FormatComponentType.Double: return "Double";
                default: break;
            }

            return "Unknown";
        }

        public static string Str(this EnvironmentSeparator sep)
        {
            switch (sep)
            {
                case EnvironmentSeparator.Platform: return "Platform style";
                case EnvironmentSeparator.SemiColon: return "Semi-colon (;)";
                case EnvironmentSeparator.Colon: return "Colon (:)";
                case EnvironmentSeparator.None: return "No Separator";
                default: break;
            }

            return "Unknown";
        }
    }
}
