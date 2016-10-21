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

#pragma once

enum DirectoryFileProperty
{
  eFileProp_Directory = 0x1,
  eFileProp_Hidden = 0x2,
  eFileProp_Executable = 0x4,

  eFileProp_ErrorUnknown = 0x2000,
  eFileProp_ErrorAccessDenied = 0x4000,
  eFileProp_ErrorInvalidPath = 0x8000,
};

// replay_shader.h

enum VarType
{
  eVar_Float = 0,
  eVar_Int,
  eVar_UInt,
  eVar_Double,
};

enum FormatComponentType
{
  eCompType_None = 0,
  eCompType_Float,
  eCompType_UNorm,
  eCompType_SNorm,
  eCompType_UInt,
  eCompType_SInt,
  eCompType_UScaled,
  eCompType_SScaled,
  eCompType_Depth,
  eCompType_Double,
};

enum TextureSwizzle
{
  eSwizzle_Red,
  eSwizzle_Green,
  eSwizzle_Blue,
  eSwizzle_Alpha,
  eSwizzle_Zero,
  eSwizzle_One,
};

enum ShaderResourceType
{
  eResType_None,
  eResType_Buffer,
  eResType_Texture1D,
  eResType_Texture1DArray,
  eResType_Texture2D,
  eResType_TextureRect,
  eResType_Texture2DArray,
  eResType_Texture2DMS,
  eResType_Texture2DMSArray,
  eResType_Texture3D,
  eResType_TextureCube,
  eResType_TextureCubeArray,
  eResType_Count,
};

enum ShaderBindType
{
  eBindType_Unknown = 0,
  eBindType_ConstantBuffer,
  eBindType_Sampler,
  eBindType_ImageSampler,
  eBindType_ReadOnlyImage,
  eBindType_ReadWriteImage,
  eBindType_ReadOnlyTBuffer,
  eBindType_ReadWriteTBuffer,
  eBindType_ReadOnlyBuffer,
  eBindType_ReadWriteBuffer,
  eBindType_InputAttachment,
};

enum SystemAttribute
{
  eAttr_None = 0,
  eAttr_Position,
  eAttr_PointSize,
  eAttr_ClipDistance,
  eAttr_CullDistance,
  eAttr_RTIndex,
  eAttr_ViewportIndex,
  eAttr_VertexIndex,
  eAttr_PrimitiveIndex,
  eAttr_InstanceIndex,
  eAttr_InvocationIndex,
  eAttr_DispatchSize,
  eAttr_DispatchThreadIndex,
  eAttr_GroupIndex,
  eAttr_GroupFlatIndex,
  eAttr_GroupThreadIndex,
  eAttr_GSInstanceIndex,
  eAttr_OutputControlPointIndex,
  eAttr_DomainLocation,
  eAttr_IsFrontFace,
  eAttr_MSAACoverage,
  eAttr_MSAASamplePosition,
  eAttr_MSAASampleIndex,
  eAttr_PatchNumVertices,
  eAttr_OuterTessFactor,
  eAttr_InsideTessFactor,
  eAttr_ColourOutput,
  eAttr_DepthOutput,
  eAttr_DepthOutputGreaterEqual,
  eAttr_DepthOutputLessEqual,
};

// replay_render.h

enum OutputType
{
  eOutputType_None = 0,
  eOutputType_TexDisplay,
  eOutputType_MeshDisplay,
};

enum MeshDataStage
{
  eMeshDataStage_Unknown = 0,
  eMeshDataStage_VSIn,
  eMeshDataStage_VSOut,
  eMeshDataStage_GSOut,
};

enum TextureDisplayOverlay
{
  eTexOverlay_None = 0,
  eTexOverlay_Drawcall,
  eTexOverlay_Wireframe,
  eTexOverlay_Depth,
  eTexOverlay_Stencil,
  eTexOverlay_BackfaceCull,
  eTexOverlay_ViewportScissor,
  eTexOverlay_NaN,
  eTexOverlay_Clipping,
  eTexOverlay_ClearBeforePass,
  eTexOverlay_ClearBeforeDraw,
  eTexOverlay_QuadOverdrawPass,
  eTexOverlay_QuadOverdrawDraw,
  eTexOverlay_TriangleSizePass,
  eTexOverlay_TriangleSizeDraw,
};

enum FileType
{
  eFileType_DDS,
  eFileType_PNG,
  eFileType_JPG,
  eFileType_BMP,
  eFileType_TGA,
  eFileType_HDR,
  eFileType_EXR,
};

enum AlphaMapping
{
  eAlphaMap_Discard,
  eAlphaMap_BlendToColour,
  eAlphaMap_BlendToCheckerboard,
};

enum SpecialFormat
{
  eSpecial_Unknown = 0,
  eSpecial_BC1,
  eSpecial_BC2,
  eSpecial_BC3,
  eSpecial_BC4,
  eSpecial_BC5,
  eSpecial_BC6,
  eSpecial_BC7,
  eSpecial_ETC2,
  eSpecial_EAC,
  eSpecial_ASTC,
  eSpecial_R10G10B10A2,
  eSpecial_R11G11B10,
  eSpecial_R5G6B5,
  eSpecial_R5G5B5A1,
  eSpecial_R9G9B9E5,
  eSpecial_R4G4B4A4,
  eSpecial_R4G4,
  eSpecial_D16S8,
  eSpecial_D24S8,
  eSpecial_D32S8,
  eSpecial_S8,
  eSpecial_YUV,
};

enum QualityHint
{
  eQuality_DontCare,
  eQuality_Nicest,
  eQuality_Fastest,
};

enum GraphicsAPI
{
  eGraphicsAPI_D3D11,
  eGraphicsAPI_D3D12,
  eGraphicsAPI_OpenGL,
  eGraphicsAPI_Vulkan,
};

enum PrimitiveTopology
{
  eTopology_Unknown,
  eTopology_PointList,
  eTopology_LineList,
  eTopology_LineStrip,
  eTopology_LineLoop,
  eTopology_TriangleList,
  eTopology_TriangleStrip,
  eTopology_TriangleFan,
  eTopology_LineList_Adj,
  eTopology_LineStrip_Adj,
  eTopology_TriangleList_Adj,
  eTopology_TriangleStrip_Adj,
  eTopology_PatchList,
  eTopology_PatchList_1CPs = eTopology_PatchList,
  eTopology_PatchList_2CPs,
  eTopology_PatchList_3CPs,
  eTopology_PatchList_4CPs,
  eTopology_PatchList_5CPs,
  eTopology_PatchList_6CPs,
  eTopology_PatchList_7CPs,
  eTopology_PatchList_8CPs,
  eTopology_PatchList_9CPs,
  eTopology_PatchList_10CPs,
  eTopology_PatchList_11CPs,
  eTopology_PatchList_12CPs,
  eTopology_PatchList_13CPs,
  eTopology_PatchList_14CPs,
  eTopology_PatchList_15CPs,
  eTopology_PatchList_16CPs,
  eTopology_PatchList_17CPs,
  eTopology_PatchList_18CPs,
  eTopology_PatchList_19CPs,
  eTopology_PatchList_20CPs,
  eTopology_PatchList_21CPs,
  eTopology_PatchList_22CPs,
  eTopology_PatchList_23CPs,
  eTopology_PatchList_24CPs,
  eTopology_PatchList_25CPs,
  eTopology_PatchList_26CPs,
  eTopology_PatchList_27CPs,
  eTopology_PatchList_28CPs,
  eTopology_PatchList_29CPs,
  eTopology_PatchList_30CPs,
  eTopology_PatchList_31CPs,
  eTopology_PatchList_32CPs,
};

enum BufferCreationFlags
{
  eBufferCreate_VB = 0x1,
  eBufferCreate_IB = 0x2,
  eBufferCreate_CB = 0x4,
  eBufferCreate_UAV = 0x8,
  eBufferCreate_Indirect = 0x10,
};

enum TextureCreationFlags
{
  eTextureCreate_SRV = 0x1,
  eTextureCreate_RTV = 0x2,
  eTextureCreate_DSV = 0x4,
  eTextureCreate_UAV = 0x8,
  eTextureCreate_SwapBuffer = 0x10,
};

enum ShaderStageType
{
  eShaderStage_Vertex = 0,
  eShaderStage_First = eShaderStage_Vertex,

  eShaderStage_Hull,
  eShaderStage_Tess_Control = eShaderStage_Hull,

  eShaderStage_Domain,
  eShaderStage_Tess_Eval = eShaderStage_Domain,

  eShaderStage_Geometry,

  eShaderStage_Pixel,
  eShaderStage_Fragment = eShaderStage_Pixel,

  eShaderStage_Compute,

  eShaderStage_Count,
};

enum ShaderStageBits
{
  eStageBits_None = 0,
  eStageBits_Vertex = 1 << eShaderStage_Vertex,
  eStageBits_Hull = 1 << eShaderStage_Hull,
  eStageBits_Tess_Control = 1 << eShaderStage_Tess_Control,
  eStageBits_Domain = 1 << eShaderStage_Domain,
  eStageBits_Tess_Eval = 1 << eShaderStage_Tess_Eval,
  eStageBits_Geometry = 1 << eShaderStage_Geometry,
  eStageBits_Pixel = 1 << eShaderStage_Pixel,
  eStageBits_Fragment = 1 << eShaderStage_Fragment,
  eStageBits_Compute = 1 << eShaderStage_Compute,
  eStageBits_All = eStageBits_Vertex | eStageBits_Hull | eStageBits_Domain | eStageBits_Geometry |
                   eStageBits_Pixel |
                   eStageBits_Compute,
};

enum DebugMessageCategory
{
  eDbgCategory_Application_Defined = 0,
  eDbgCategory_Miscellaneous,
  eDbgCategory_Initialization,
  eDbgCategory_Cleanup,
  eDbgCategory_Compilation,
  eDbgCategory_State_Creation,
  eDbgCategory_State_Setting,
  eDbgCategory_State_Getting,
  eDbgCategory_Resource_Manipulation,
  eDbgCategory_Execution,
  eDbgCategory_Shaders,
  eDbgCategory_Deprecated,
  eDbgCategory_Undefined,
  eDbgCategory_Portability,
  eDbgCategory_Performance,
};

enum DebugMessageSeverity
{
  eDbgSeverity_High = 0,
  eDbgSeverity_Medium,
  eDbgSeverity_Low,
  eDbgSeverity_Info,
};

enum DebugMessageSource
{
  eDbgSource_API = 0,
  eDbgSource_RedundantAPIUse,
  eDbgSource_IncorrectAPIUse,
  eDbgSource_GeneralPerformance,
  eDbgSource_GCNPerformance,
  eDbgSource_RuntimeWarning,
  eDbgSoruce_UnsupportedConfiguration,
};

enum ResourceUsage
{
  eUsage_None,

  eUsage_VertexBuffer,
  eUsage_IndexBuffer,

  eUsage_VS_Constants,
  eUsage_HS_Constants,
  eUsage_DS_Constants,
  eUsage_GS_Constants,
  eUsage_PS_Constants,
  eUsage_CS_Constants,

  eUsage_All_Constants,

  eUsage_SO,

  eUsage_VS_Resource,
  eUsage_HS_Resource,
  eUsage_DS_Resource,
  eUsage_GS_Resource,
  eUsage_PS_Resource,
  eUsage_CS_Resource,

  eUsage_All_Resource,

  eUsage_VS_RWResource,
  eUsage_HS_RWResource,
  eUsage_DS_RWResource,
  eUsage_GS_RWResource,
  eUsage_PS_RWResource,
  eUsage_CS_RWResource,

  eUsage_All_RWResource,

  eUsage_InputTarget,
  eUsage_ColourTarget,
  eUsage_DepthStencilTarget,

  eUsage_Indirect,

  eUsage_Clear,

  eUsage_GenMips,
  eUsage_Resolve,
  eUsage_ResolveSrc,
  eUsage_ResolveDst,
  eUsage_Copy,
  eUsage_CopySrc,
  eUsage_CopyDst,

  eUsage_Barrier,
};

enum DrawcallFlags
{
  // types
  eDraw_Clear = 0x0001,
  eDraw_Drawcall = 0x0002,
  eDraw_Dispatch = 0x0004,
  eDraw_CmdList = 0x0008,
  eDraw_SetMarker = 0x0010,
  eDraw_PushMarker = 0x0020,
  eDraw_PopMarker = 0x0040,    // this is only for internal tracking use
  eDraw_Present = 0x0080,
  eDraw_MultiDraw = 0x0100,
  eDraw_Copy = 0x0200,
  eDraw_Resolve = 0x0400,
  eDraw_GenMips = 0x0800,
  eDraw_PassBoundary = 0x1000,

  // flags
  eDraw_UseIBuffer = 0x010000,
  eDraw_Instanced = 0x020000,
  eDraw_Auto = 0x040000,
  eDraw_Indirect = 0x080000,
  eDraw_ClearColour = 0x100000,
  eDraw_ClearDepthStencil = 0x200000,
  eDraw_BeginPass = 0x400000,
  eDraw_EndPass = 0x800000,
};

enum SolidShadeMode
{
  eShade_None = 0,
  eShade_Solid,
  eShade_Lit,
  eShade_Secondary,
  eShade_Count,
};

enum TriangleFillMode
{
  eFill_Solid = 0,
  eFill_Wireframe,
  eFill_Point,
};

enum TriangleCullMode
{
  eCull_None = 0,
  eCull_Front,
  eCull_Back,
  eCull_FrontAndBack,
};

enum GPUCounters
{
  eCounter_FirstGeneric = 1,
  eCounter_EventGPUDuration = eCounter_FirstGeneric,
  eCounter_InputVerticesRead,
  eCounter_VSInvocations,
  eCounter_PSInvocations,
  eCounter_RasterizedPrimitives,
  eCounter_SamplesWritten,

  // IHV specific counters can be set above this point
  // with ranges reserved for each IHV
  eCounter_FirstAMD = 1000000,

  eCounter_FirstIntel = 2000000,

  eCounter_FirstNvidia = 3000000,
};

enum CounterUnits
{
  eUnits_Absolute,
  eUnits_Seconds,
  eUnits_Percentage,
};

enum ReplaySupport
{
  eReplaySupport_Unsupported,
  eReplaySupport_Supported,
  eReplaySupport_SuggestRemote,
};

enum ReplayCreateStatus
{
  eReplayCreate_Success = 0,
  eReplayCreate_UnknownError,
  eReplayCreate_InternalError,
  eReplayCreate_FileNotFound,
  eReplayCreate_InjectionFailed,
  eReplayCreate_IncompatibleProcess,
  eReplayCreate_NetworkIOFailed,
  eReplayCreate_NetworkRemoteBusy,
  eReplayCreate_NetworkVersionMismatch,
  eReplayCreate_FileIOFailed,
  eReplayCreate_FileIncompatibleVersion,
  eReplayCreate_FileCorrupted,
  eReplayCreate_ImageUnsupported,
  eReplayCreate_APIUnsupported,
  eReplayCreate_APIInitFailed,
  eReplayCreate_APIIncompatibleVersion,
  eReplayCreate_APIHardwareUnsupported,
};

enum TargetControlMessageType
{
  eTargetControlMsg_Unknown = 0,
  eTargetControlMsg_Disconnected,
  eTargetControlMsg_Busy,
  eTargetControlMsg_Noop,
  eTargetControlMsg_NewCapture,
  eTargetControlMsg_CaptureCopied,
  eTargetControlMsg_RegisterAPI,
  eTargetControlMsg_NewChild,
};

enum EnvironmentModificationType
{
  eEnvMod_Set,
  eEnvMod_Append,
  eEnvMod_Prepend,
};

enum EnvironmentSeparator
{
  eEnvSep_Platform,
  eEnvSep_SemiColon,
  eEnvSep_Colon,
  eEnvSep_None,
};

// matches enum in common.h
enum LogMessageType
{
  eLogType_First = -1,
  eLogType_Debug,
  eLogType_Comment,
  eLogType_Warning,
  eLogType_Error,
  eLogType_Fatal,
  eLogType_NumTypes,
};
