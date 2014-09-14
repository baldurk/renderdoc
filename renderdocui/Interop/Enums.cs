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

    public enum ShaderResourceType
    {
        None,
        Buffer,
        Texture1D,
        Texture1DArray,
        Texture2D,
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
        DepthBoth,
        StencilBoth,
        ViewportScissor,
        NaN,
        Clipping,
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
        R10G10B10A2,
        R11G11B10,
        B5G6R5,
        B5G5R5A1,
        R9G9B9E5,
        B8G8R8A8,
        B4G4R4A4,
        D24S8,
        D32S8,
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
        Corruption = 0,
        Error,
        Warning,
        Info,
    };

    public enum ResourceUsage
    {
    	None,

    	IA_VB,
    	IA_IB,

    	VS_CB,
    	HS_CB,
    	DS_CB,
    	GS_CB,
    	PS_CB,
    	CS_CB,

    	SO,

    	VS_SRV,
    	HS_SRV,
    	DS_SRV,
    	GS_SRV,
    	PS_SRV,
    	CS_SRV,

    	CS_UAV,
    	PS_UAV,

    	OM_RTV,
    	OM_DSV,

        Clear,
    };

    [Flags]
    public enum DrawcallFlags
    {
        // types
        Clear = 0x01,
        Drawcall = 0x02,
        Dispatch = 0x04,
        CmdList = 0x08,
        SetMarker = 0x10,
        PushMarker = 0x20,
        Present = 0x40,

        // flags
        UseIBuffer = 0x100,
        Instanced = 0x200,
        Auto = 0x400,
        Indirect = 0x800,
        ClearColour = 0x1000,
        ClearDepth = 0x2000,
    };

    public enum SolidShadeMode
    {
        None = 0,
        Solid,
        Lit,
        Tex,
        VertCol,
    };

    public enum TriangleFillMode
    {
        Solid = 0,
        Wireframe,
    };

    public enum TriangleCullMode
    {
        None = 0,
        Front,
        Back,
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
    };

    public static class EnumString
    {
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
                case ReplayCreateStatus.APIIncompatibleVersion: return "API-speciifc data used in logfile is of an incompatible version";
                case ReplayCreateStatus.APIHardwareUnsupported: return "Your hardware or software configuration doesn't meet this API's minimum requirements";
            }

            return "Unknown Error Code";
        }

        public static string Str(this ResourceUsage usage)
        {
            switch (usage)
            {
                case ResourceUsage.IA_VB: return "Vertex Buffer";
                case ResourceUsage.IA_IB: return "Index Buffer";

                case ResourceUsage.VS_CB: return "VS - Constant Buffer";
                case ResourceUsage.GS_CB: return "GS - Constant Buffer";
                case ResourceUsage.HS_CB: return "HS - Constant Buffer";
                case ResourceUsage.DS_CB: return "DS - Constant Buffer";
                case ResourceUsage.CS_CB: return "CS - Constant Buffer";
                case ResourceUsage.PS_CB: return "PS - Constant Buffer";

                case ResourceUsage.SO: return "Stream Out";

                case ResourceUsage.VS_SRV: return "VS - Resource";
                case ResourceUsage.GS_SRV: return "GS - Resource";
                case ResourceUsage.HS_SRV: return "HS - Resource";
                case ResourceUsage.DS_SRV: return "DS - Resource";
                case ResourceUsage.CS_SRV: return "CS - Resource";
                case ResourceUsage.PS_SRV: return "PS - Resource";

                case ResourceUsage.CS_UAV: return "CS - UAV";
                case ResourceUsage.PS_UAV: return "PS - UAV";

                case ResourceUsage.OM_RTV: return "Rendertarget";
                case ResourceUsage.OM_DSV: return "Depthstencil";

                case ResourceUsage.Clear: return "Clear";
            }

            return "Unknown Usage String";
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
