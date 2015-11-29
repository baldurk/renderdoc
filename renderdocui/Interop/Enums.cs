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

// from replay_enums.h

namespace renderdoc
{
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
        InvocationIndex,
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
        D16S8,
        D24S8,
        D32S8,
        S8,
        YUV,
    };

    public enum APIPipelineStateType
    {
        D3D11,
        OpenGL,
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

    public enum ShaderStageType
    {
        Vertex = 0,

        Hull,
        Tess_Control = Hull,

        Domain,
        Tess_Eval = Domain,

        Geometry,

        Pixel,
        Fragment = Pixel,

        Compute,
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

    	SO,

    	VS_Resource,
    	HS_Resource,
    	DS_Resource,
    	GS_Resource,
    	PS_Resource,
    	CS_Resource,

        VS_RWResource,
        HS_RWResource,
        DS_RWResource,
        GS_RWResource,
        PS_RWResource,
        CS_RWResource,

    	ColourTarget,
    	DepthStencilTarget,

        Clear,

        GenMips,
        Resolve,
        ResolveSrc,
        ResolveDst,
        Copy,
        CopySrc,
        CopyDst,
    };

    [Flags]
    public enum DrawcallFlags
    {
        // types
        Clear       = 0x01,
        Drawcall    = 0x02,
        Dispatch    = 0x04,
        CmdList     = 0x08,
        SetMarker   = 0x10,
        PushMarker  = 0x20,
        Present     = 0x40,
        MultiDraw   = 0x80,
        Copy        = 0x100,
        Resolve     = 0x200,
        GenMips     = 0x400,

        // flags
        UseIBuffer  = 0x01000,
        Instanced   = 0x02000,
        Auto        = 0x04000,
        Indirect    = 0x08000,
        ClearColour = 0x10000,
        ClearDepth  = 0x20000,
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

    public enum GPUCounters
    {
        FirstGeneric = 1,
        EventGPUDuration = FirstGeneric,
        InputVerticesRead,
        VSInvocations,
        PSInvocations,
        RasterizedPrimitives,
        SamplesWritten,

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

    public enum ReplayCreateStatus
    {
        Success = 0,
        UnknownError,
        InternalError,
        NetworkIOFailed,
        FileIOFailed,
        FileIncompatibleVersion,
        FileCorrupted,
        APIUnsupported,
        APIInitFailed,
        APIIncompatibleVersion,
        APIHardwareUnsupported,
    };

    public enum RemoteMessageType
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

    public static class EnumString
    {
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
                case ReplayCreateStatus.NetworkIOFailed: return "Network I/O operation failed";
                case ReplayCreateStatus.FileIOFailed: return "File I/O operation failed";
                case ReplayCreateStatus.FileIncompatibleVersion: return "Logfile is of an incompatible version";
                case ReplayCreateStatus.FileCorrupted: return "Logfile data is corrupted";
                case ReplayCreateStatus.APIUnsupported: return "API used in logfile is not supported";
                case ReplayCreateStatus.APIInitFailed: return "Replay API failed to initialise";
                case ReplayCreateStatus.APIIncompatibleVersion: return "API-specific data used in logfile is of an incompatible version";
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

        public static string Str(this ResourceUsage usage, APIPipelineStateType apitype)
        {
            if (apitype == APIPipelineStateType.D3D11)
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

                    case ResourceUsage.SO: return "Stream Out";

                    case ResourceUsage.VS_Resource: return "VS - Resource";
                    case ResourceUsage.GS_Resource: return "GS - Resource";
                    case ResourceUsage.HS_Resource: return "HS - Resource";
                    case ResourceUsage.DS_Resource: return "DS - Resource";
                    case ResourceUsage.CS_Resource: return "CS - Resource";
                    case ResourceUsage.PS_Resource: return "PS - Resource";

                    case ResourceUsage.VS_RWResource: return "VS - UAV";
                    case ResourceUsage.HS_RWResource: return "HS - UAV";
                    case ResourceUsage.DS_RWResource: return "DS - UAV";
                    case ResourceUsage.GS_RWResource: return "GS - UAV";
                    case ResourceUsage.PS_RWResource: return "PS - UAV";
                    case ResourceUsage.CS_RWResource: return "CS - UAV";

                    case ResourceUsage.ColourTarget: return "Rendertarget";
                    case ResourceUsage.DepthStencilTarget: return "Depthstencil";

                    case ResourceUsage.Clear: return "Clear";

                    case ResourceUsage.GenMips: return "Generate Mips";
                    case ResourceUsage.Resolve: return "Resolve";
                    case ResourceUsage.ResolveSrc: return "Resolve - Source";
                    case ResourceUsage.ResolveDst: return "Resolve - Dest";
                    case ResourceUsage.Copy: return "Copy";
                    case ResourceUsage.CopySrc: return "Copy - Source";
                    case ResourceUsage.CopyDst: return "Copy - Dest";
                }
            }
            else if (apitype == APIPipelineStateType.OpenGL)
            {
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

                    case ResourceUsage.SO: return "Transform Feedback";

                    case ResourceUsage.VS_Resource: return "VS - Texture";
                    case ResourceUsage.GS_Resource: return "GS - Texture";
                    case ResourceUsage.HS_Resource: return "HS - Texture";
                    case ResourceUsage.DS_Resource: return "DS - Texture";
                    case ResourceUsage.CS_Resource: return "CS - Texture";
                    case ResourceUsage.PS_Resource: return "PS - Texture";

                    case ResourceUsage.VS_RWResource: return "VS - Image/SSBO";
                    case ResourceUsage.HS_RWResource: return "HS - Image/SSBO";
                    case ResourceUsage.DS_RWResource: return "DS - Image/SSBO";
                    case ResourceUsage.GS_RWResource: return "GS - Image/SSBO";
                    case ResourceUsage.PS_RWResource: return "PS - Image/SSBO";
                    case ResourceUsage.CS_RWResource: return "CS - Image/SSBO";

                    case ResourceUsage.ColourTarget: return "FBO Colour";
                    case ResourceUsage.DepthStencilTarget: return "FBO Depthstencil";

                    case ResourceUsage.Clear: return "Clear";

                    case ResourceUsage.GenMips: return "Generate Mips";
                    case ResourceUsage.Resolve: return "Framebuffer blit";
                    case ResourceUsage.ResolveSrc: return "Framebuffer blit - Source";
                    case ResourceUsage.ResolveDst: return "Framebuffer blit - Dest";
                    case ResourceUsage.Copy: return "Copy";
                    case ResourceUsage.CopySrc: return "Copy - Source";
                    case ResourceUsage.CopyDst: return "Copy - Dest";
                }
            }

            return "Unknown Usage String";
        }

        public static string Str(this ShaderStageType stage, APIPipelineStateType apitype)
        {
            if (apitype == APIPipelineStateType.D3D11)
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
            else if(apitype == APIPipelineStateType.OpenGL)
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

        public static string Abbrev(this ShaderStageType stage, APIPipelineStateType apitype)
        {
            if (apitype == APIPipelineStateType.D3D11)
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
            else if (apitype == APIPipelineStateType.OpenGL)
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
    }
}
