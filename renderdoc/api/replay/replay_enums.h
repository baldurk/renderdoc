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

#pragma once

#ifdef NO_ENUM_CLASS_OPERATORS

#define BITMASK_OPERATORS(a)
#define ITERABLE_OPERATORS(a)

#else

#include <type_traits>

// helper template that allows the result of & to be cast back to the enum or explicitly cast to
// bool for use in if() or ?: or so on or compared against 0.
//
// If you get an error about missing operator then you're probably doing something like
// (bitfield & value) == 0 or (bitfield & value) != 0 or similar. Instead prefer:
// !(bitfield & value)     or (bitfield & value) to make use of the bool cast directly
template <typename enum_name>
struct EnumCastHelper
{
public:
  constexpr EnumCastHelper(enum_name v) : val(v) {}
  constexpr operator enum_name() const { return val; }
  constexpr explicit operator bool() const
  {
    typedef typename std::underlying_type<enum_name>::type etype;
    return etype(val) != 0;
  }

private:
  const enum_name val;
};

// helper templates for iterating over all values in an enum that has sequential values and is
// to be used for array indices or something like that.
template <typename enum_name>
struct ValueIterContainer
{
  struct ValueIter
  {
    ValueIter(enum_name v) : val(v) {}
    enum_name val;
    enum_name operator*() const { return val; }
    bool operator!=(const ValueIter &it) const { return !(val == *it); }
    const inline enum_name operator++()
    {
      ++val;
      return val;
    }
  };

  ValueIter begin() { return ValueIter(enum_name::First); }
  ValueIter end() { return ValueIter(enum_name::Count); }
};

template <typename enum_name>
struct IndexIterContainer
{
  typedef typename std::underlying_type<enum_name>::type etype;

  struct IndexIter
  {
    IndexIter(enum_name v) : val(v) {}
    enum_name val;
    etype operator*() const { return etype(val); }
    bool operator!=(const IndexIter &it) const { return !(val == it.val); }
    const inline enum_name operator++()
    {
      ++val;
      return val;
    }
  };

  IndexIter begin() { return IndexIter(enum_name::First); }
  IndexIter end() { return IndexIter(enum_name::Count); }
};

template <typename enum_name>
constexpr inline ValueIterContainer<enum_name> values()
{
  return ValueIterContainer<enum_name>();
};

template <typename enum_name>
constexpr inline IndexIterContainer<enum_name> indices()
{
  return IndexIterContainer<enum_name>();
};

template <typename enum_name>
constexpr inline size_t arraydim()
{
  typedef typename std::underlying_type<enum_name>::type etype;
  return (size_t)etype(enum_name::Count);
};

// clang-format makes a even more of a mess of this multi-line macro than it usually does, for some
// reason. So we just disable it since it's still readable and this isn't really the intended case
// we are using clang-format for.

// clang-format off
#define BITMASK_OPERATORS(enum_name)                                           \
                                                                               \
constexpr inline enum_name operator|(enum_name a, enum_name b)                 \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return enum_name(etype(a) | etype(b));                                       \
}                                                                              \
                                                                               \
constexpr inline EnumCastHelper<enum_name> operator&(enum_name a, enum_name b) \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return EnumCastHelper<enum_name>(enum_name(etype(a) & etype(b)));            \
}                                                                              \
                                                                               \
constexpr inline enum_name operator~(enum_name a)                              \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return enum_name(~etype(a));                                                 \
}                                                                              \
                                                                               \
inline enum_name &operator|=(enum_name &a, enum_name b)                        \
{ return a = a | b; }                                                          \
                                                                               \
inline enum_name &operator&=(enum_name &a, enum_name b)                        \
{ return a = a & b; }

#define ITERABLE_OPERATORS(enum_name)                                          \
                                                                               \
inline enum_name operator++(enum_name &a)                                      \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return a = enum_name(etype(a)+1);                                            \
}
// clang-format on

#endif

#define ENUM_ARRAY_SIZE(enum_name) size_t(enum_name::Count)

enum class FileProperty : uint32_t
{
  NoFlags = 0x0,
  Directory = 0x1,
  Hidden = 0x2,
  Executable = 0x4,

  ErrorUnknown = 0x2000,
  ErrorAccessDenied = 0x4000,
  ErrorInvalidPath = 0x8000,
};

BITMASK_OPERATORS(FileProperty);

// replay_shader.h

enum class VarType : uint32_t
{
  Float = 0,
  Int,
  UInt,
  Double,
  Unknown = ~0U,
};

enum class CompType : uint32_t
{
  Typeless = 0,
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

enum class TextureSwizzle : uint32_t
{
  Red,
  Green,
  Blue,
  Alpha,
  Zero,
  One,
};

enum class TextureDim : uint32_t
{
  Unknown,
  First = Unknown,
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
  Count,
};

ITERABLE_OPERATORS(TextureDim);

enum class BindType : uint32_t
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

enum class ShaderBuiltin : uint32_t
{
  Undefined = 0,
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

enum class ReplayOutputType : uint32_t
{
  Headless = 0,
  Texture,
  Mesh,
};

enum class MeshDataStage : uint32_t
{
  Unknown = 0,
  VSIn,
  VSOut,
  GSOut,
};

enum class DebugOverlay : uint32_t
{
  NoOverlay = 0,
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

enum class FileType : uint32_t
{
  DDS,
  First = DDS,
  PNG,
  JPG,
  BMP,
  TGA,
  HDR,
  EXR,
  Count,
};

ITERABLE_OPERATORS(FileType);

enum class AlphaMapping : uint32_t
{
  Discard,
  First = Discard,
  BlendToColour,
  BlendToCheckerboard,
  Preserve,
  Count,
};

ITERABLE_OPERATORS(AlphaMapping);

enum class SpecialFormat : uint32_t
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

enum class QualityHint : uint32_t
{
  DontCare,
  Nicest,
  Fastest,
};

enum class GraphicsAPI : uint32_t
{
  D3D11,
  D3D12,
  OpenGL,
  Vulkan,
};

constexpr inline bool IsD3D(GraphicsAPI api)
{
  return api == GraphicsAPI::D3D11 || api == GraphicsAPI::D3D12;
}

enum class Topology : uint32_t
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

constexpr inline Topology PatchList_Topology(uint32_t N)
{
  return (N < 1 || N > 32) ? Topology::PatchList_1CPs
                           : Topology(uint32_t(Topology::PatchList_1CPs) + N - 1);
}

constexpr inline uint32_t PatchList_Count(Topology t)
{
  return uint32_t(t) < uint32_t(Topology::PatchList_1CPs)
             ? 0
             : uint32_t(t) - uint32_t(Topology::PatchList_1CPs);
}

enum class BufferCategory : uint32_t
{
  NoFlags = 0x0,
  Vertex = 0x1,
  Index = 0x2,
  Constants = 0x4,
  ReadWrite = 0x8,
  Indirect = 0x10,
};

BITMASK_OPERATORS(BufferCategory);

enum class D3DBufferViewFlags : uint32_t
{
  NoFlags = 0x0,
  Raw = 0x1,
  Append = 0x2,
  Counter = 0x4,
};

BITMASK_OPERATORS(D3DBufferViewFlags);

enum class TextureCategory : uint32_t
{
  NoFlags = 0x0,
  ShaderRead = 0x1,
  ColorTarget = 0x2,
  DepthTarget = 0x4,
  ShaderReadWrite = 0x8,
  SwapBuffer = 0x10,
};

BITMASK_OPERATORS(TextureCategory);

enum class ShaderStage : uint32_t
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

ITERABLE_OPERATORS(ShaderStage);

template <typename integer>
constexpr inline ShaderStage StageFromIndex(integer stage)
{
  return ShaderStage(stage);
}

enum class ShaderStageMask : uint32_t
{
  Unknown = 0,
  Vertex = 1 << uint32_t(ShaderStage::Vertex),
  Hull = 1 << uint32_t(ShaderStage::Hull),
  Tess_Control = Hull,
  Domain = 1 << uint32_t(ShaderStage::Domain),
  Tess_Eval = Domain,
  Geometry = 1 << uint32_t(ShaderStage::Geometry),
  Pixel = 1 << uint32_t(ShaderStage::Pixel),
  Fragment = Pixel,
  Compute = 1 << uint32_t(ShaderStage::Compute),
  All = Vertex | Hull | Domain | Geometry | Pixel | Compute,
};

BITMASK_OPERATORS(ShaderStageMask);

constexpr inline ShaderStageMask MaskForStage(ShaderStage stage)
{
  return ShaderStageMask(1 << uint32_t(stage));
}

enum class ShaderEvents : uint32_t
{
  NoEvent = 0,
  SampleLoadGather = 0x1,
  GeneratedNanOrInf = 0x2,
};

BITMASK_OPERATORS(ShaderEvents);

enum class MessageCategory : uint32_t
{
  Application_Defined = 0,
  Miscellaneous,
  Initialization,
  Cleanup,
  Compilation,
  State_Creation,
  State_Setting,
  State_Getting,
  Resource_Manipulation,
  Execution,
  Shaders,
  Deprecated,
  Undefined,
  Portability,
  Performance,
};

enum class MessageSeverity : uint32_t
{
  High = 0,
  Medium,
  Low,
  Info,
};

enum class MessageSource : uint32_t
{
  API = 0,
  RedundantAPIUse,
  IncorrectAPIUse,
  GeneralPerformance,
  GCNPerformance,
  RuntimeWarning,
  UnsupportedConfiguration,
};

enum class ResourceUsage : uint32_t
{
  Unused,

  VertexBuffer,
  IndexBuffer,

  VS_Constants,
  HS_Constants,
  DS_Constants,
  GS_Constants,
  PS_Constants,
  CS_Constants,

  All_Constants,

  StreamOut,

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

template <typename integer>
constexpr inline ResourceUsage CBUsage(integer stage)
{
  return ResourceUsage(uint32_t(ResourceUsage::VS_Constants) + stage);
}

constexpr inline ResourceUsage CBUsage(ShaderStage stage)
{
  return CBUsage(uint32_t(stage));
}

template <typename integer>
constexpr inline ResourceUsage ResUsage(integer stage)
{
  return ResourceUsage(uint32_t(ResourceUsage::VS_Resource) + stage);
}

constexpr inline ResourceUsage ResUsage(ShaderStage stage)
{
  return ResUsage(uint32_t(stage));
}

template <typename integer>
constexpr inline ResourceUsage RWResUsage(integer stage)
{
  return ResourceUsage(uint32_t(ResourceUsage::VS_RWResource) + stage);
}

constexpr inline ResourceUsage RWResUsage(ShaderStage stage)
{
  return RWResUsage(uint32_t(stage));
}

enum class DrawFlags : uint32_t
{
  NoFlags = 0x0000,

  // types
  Clear = 0x0001,
  Drawcall = 0x0002,
  Dispatch = 0x0004,
  CmdList = 0x0008,
  SetMarker = 0x0010,
  PushMarker = 0x0020,
  PopMarker = 0x0040,    // this is only for internal tracking use
  Present = 0x0080,
  MultiDraw = 0x0100,
  Copy = 0x0200,
  Resolve = 0x0400,
  GenMips = 0x0800,
  PassBoundary = 0x1000,

  // flags
  UseIBuffer = 0x010000,
  Instanced = 0x020000,
  Auto = 0x040000,
  Indirect = 0x080000,
  ClearColour = 0x100000,
  ClearDepthStencil = 0x200000,
  BeginPass = 0x400000,
  EndPass = 0x800000,
  APICalls = 0x1000000,
};

BITMASK_OPERATORS(DrawFlags);

enum class SolidShade : uint32_t
{
  NoSolid = 0,
  Solid,
  Lit,
  Secondary,
  Count,
};

enum class FillMode : uint32_t
{
  Solid = 0,
  Wireframe,
  Point,
};

enum class CullMode : uint32_t
{
  NoCull = 0,
  Front,
  Back,
  FrontAndBack,
};

enum class GPUCounter : uint32_t
{
  EventGPUDuration = 1,
  First = EventGPUDuration,
  InputVerticesRead,
  IAPrimitives,
  GSPrimitives,
  RasterizerInvocations,
  RasterizedPrimitives,
  SamplesWritten,
  VSInvocations,
  HSInvocations,
  TCSInvocations = HSInvocations,
  DSInvocations,
  TESInvocations = DSInvocations,
  GSInvocations,
  PSInvocations,
  FSInvocations = PSInvocations,
  CSInvocations,
  Count,

  // IHV specific counters can be set above this point
  // with ranges reserved for each IHV
  FirstAMD = 1000000,

  FirstIntel = 2000000,

  FirstNvidia = 3000000,
};

ITERABLE_OPERATORS(GPUCounter);

enum class CounterUnit : uint32_t
{
  Absolute,
  Seconds,
  Percentage,
};

enum class ReplaySupport : uint32_t
{
  Unsupported,
  Supported,
  SuggestRemote,
};

enum class ReplayStatus : uint32_t
{
  Succeeded = 0,
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

enum class TargetControlMessageType : uint32_t
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

enum class EnvMod : uint32_t
{
  Set,
  Append,
  Prepend,
};

enum class EnvSep : uint32_t
{
  Platform,
  SemiColon,
  Colon,
  NoSep,
};

enum class LogType : int32_t
{
  Debug,
  Comment,
  Warning,
  Error,
  Fatal,
  Count,
};

enum class VulkanLayerFlags : uint32_t
{
  NoFlags = 0x0,
  OtherInstallsRegistered = 0x1,
  ThisInstallRegistered = 0x2,
  NeedElevation = 0x4,
  CouldElevate = 0x8,
  RegisterAll = 0x10,
  UpdateAllowed = 0x20,
  Unfixable = 0x40,
};

BITMASK_OPERATORS(VulkanLayerFlags);