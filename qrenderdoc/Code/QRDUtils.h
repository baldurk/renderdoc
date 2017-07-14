/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include <QCoreApplication>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QSemaphore>
#include <QSortFilterProxyModel>
#include "Code/Interface/QRDInterface.h"
#include "renderdoc_replay.h"

template <typename T>
inline T AlignUp(T x, T a)
{
  return (x + (a - 1)) & (~(a - 1));
}

#ifndef ARRAY_COUNT
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

// total hack, expose the same basic interface as on renderdoc side.
// Eventually we want to move the code in the main project into header-only
// and .inl implementations for at least the public API, so it can be compiled
// directly without duplication

struct ToStr
{
  static std::string Get(const ResourceId &el)
  {
    // super inefficient to convert to qstr then std::string then back to qstr
    // but this is just a temporary measure
    uint64_t num = 0;
    memcpy(&num, &el, sizeof(num));
    return QString::number(num).toStdString();
  }

  static std::string Get(const ReplayStatus &el)
  {
    switch(el)
    {
      case ReplayStatus::Succeeded: return "Success";
      case ReplayStatus::UnknownError: return "Unknown error";
      case ReplayStatus::InternalError: return "Internal error";
      case ReplayStatus::FileNotFound: return "File not found";
      case ReplayStatus::InjectionFailed: return "RenderDoc injection failed";
      case ReplayStatus::IncompatibleProcess: return "Process is incompatible";
      case ReplayStatus::NetworkIOFailed: return "Network I/O operation failed";
      case ReplayStatus::NetworkRemoteBusy: return "Remote side of network connection is busy";
      case ReplayStatus::NetworkVersionMismatch: return "Version mismatch between network clients";
      case ReplayStatus::FileIOFailed: return "File I/O failed";
      case ReplayStatus::FileIncompatibleVersion: return "File of incompatible version";
      case ReplayStatus::FileCorrupted: return "File corrupted";
      case ReplayStatus::APIUnsupported: return "API unsupported";
      case ReplayStatus::APIInitFailed: return "API initialisation failed";
      case ReplayStatus::APIIncompatibleVersion: return "API incompatible version";
      case ReplayStatus::APIHardwareUnsupported: return "API hardware unsupported";
      default: break;
    }
    return "Invalid error code";
  }

  static std::string Get(const CompType &el)
  {
    switch(el)
    {
      case CompType::Typeless: return "Typeless";
      case CompType::Float: return "Float";
      case CompType::UNorm: return "UNorm";
      case CompType::SNorm: return "SNorm";
      case CompType::UInt: return "UInt";
      case CompType::SInt: return "SInt";
      case CompType::UScaled: return "UScaled";
      case CompType::SScaled: return "SScaled";
      case CompType::Depth: return "Depth/Stencil";
      case CompType::Double: return "Double";
      default: break;
    }
    return "Invalid component type";
  }

  static std::string Get(const FileType &el)
  {
    switch(el)
    {
      case FileType::DDS: return "DDS";
      case FileType::PNG: return "PNG";
      case FileType::JPG: return "JPG";
      case FileType::BMP: return "BMP";
      case FileType::TGA: return "TGA";
      case FileType::HDR: return "HDR";
      case FileType::EXR: return "EXR";
      default: break;
    }
    return "Invalid file type";
  }

  static std::string Get(const AlphaMapping &el)
  {
    switch(el)
    {
      case AlphaMapping::Discard: return "Discard";
      case AlphaMapping::BlendToColor: return "Blend to Color";
      case AlphaMapping::BlendToCheckerboard: return "Blend to Checkerboard";
      case AlphaMapping::Preserve: return "Preserve";
      default: break;
    }
    return "Invalid mapping";
  }

  static std::string Get(const EnvMod &el)
  {
    switch(el)
    {
      case EnvMod::Set: return "Set";
      case EnvMod::Append: return "Append";
      case EnvMod::Prepend: return "Prepend";
      default: break;
    }
    return "Invalid modification";
  }

  static std::string Get(const EnvSep &el)
  {
    switch(el)
    {
      case EnvSep::Platform: return "Platform style";
      case EnvSep::SemiColon: return "Semi-colon (;)";
      case EnvSep::Colon: return "Colon (:)";
      case EnvSep::NoSep: return "No Separator";
      default: break;
    }
    return "Invalid separator";
  }

  static std::string Get(const Topology &el)
  {
    switch(el)
    {
      case Topology::Unknown: return "Unknown";
      case Topology::PointList: return "Point List";
      case Topology::LineList: return "Line List";
      case Topology::LineStrip: return "Line Strip";
      case Topology::LineLoop: return "Line Loop";
      case Topology::TriangleList: return "Triangle List";
      case Topology::TriangleStrip: return "Triangle Strip";
      case Topology::TriangleFan: return "Triangle Fan";
      case Topology::LineList_Adj: return "Line List with Adjacency";
      case Topology::LineStrip_Adj: return "Line Strip with Adjacency";
      case Topology::TriangleList_Adj: return "Triangle List with Adjacency";
      case Topology::TriangleStrip_Adj: return "Triangle Strip with Adjacency";
      case Topology::PatchList_1CPs:
      case Topology::PatchList_2CPs:
      case Topology::PatchList_3CPs:
      case Topology::PatchList_4CPs:
      case Topology::PatchList_5CPs:
      case Topology::PatchList_6CPs:
      case Topology::PatchList_7CPs:
      case Topology::PatchList_8CPs:
      case Topology::PatchList_9CPs:
      case Topology::PatchList_10CPs:
      case Topology::PatchList_11CPs:
      case Topology::PatchList_12CPs:
      case Topology::PatchList_13CPs:
      case Topology::PatchList_14CPs:
      case Topology::PatchList_15CPs:
      case Topology::PatchList_16CPs:
      case Topology::PatchList_17CPs:
      case Topology::PatchList_18CPs:
      case Topology::PatchList_19CPs:
      case Topology::PatchList_20CPs:
      case Topology::PatchList_21CPs:
      case Topology::PatchList_22CPs:
      case Topology::PatchList_23CPs:
      case Topology::PatchList_24CPs:
      case Topology::PatchList_25CPs:
      case Topology::PatchList_26CPs:
      case Topology::PatchList_27CPs:
      case Topology::PatchList_28CPs:
      case Topology::PatchList_29CPs:
      case Topology::PatchList_30CPs:
      case Topology::PatchList_31CPs:
      case Topology::PatchList_32CPs: return "Patch List";
      default: break;
    }
    return "Unknown topology";
  }

  static std::string Get(const FillMode &el)
  {
    switch(el)
    {
      case FillMode::Solid: return "Solid";
      case FillMode::Wireframe: return "Wireframe";
      case FillMode::Point: return "Point";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const CullMode &el)
  {
    switch(el)
    {
      case CullMode::NoCull: return "None";
      case CullMode::Front: return "Front";
      case CullMode::Back: return "Back";
      case CullMode::FrontAndBack: return "Front & Back";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const FilterMode &el)
  {
    switch(el)
    {
      case FilterMode::NoFilter: return "None";
      case FilterMode::Point: return "Point";
      case FilterMode::Linear: return "Linear";
      case FilterMode::Cubic: return "Cubic";
      case FilterMode::Anisotropic: return "Anisotropic";
    }
    return "Unknown";
  }

  static std::string Get(const FilterFunc &el)
  {
    switch(el)
    {
      case FilterFunc::Normal: return "Normal";
      case FilterFunc::Comparison: return "Comparison";
      case FilterFunc::Minimum: return "Minimum";
      case FilterFunc::Maximum: return "Maximum";
    }
    return "Unknown";
  }

  static std::string Get(const CompareFunc &el)
  {
    switch(el)
    {
      case CompareFunc::Never: return "Never";
      case CompareFunc::AlwaysTrue: return "Always";
      case CompareFunc::Less: return "Less";
      case CompareFunc::LessEqual: return "Less Equal";
      case CompareFunc::Greater: return "Greater";
      case CompareFunc::GreaterEqual: return "Greater Equal";
      case CompareFunc::Equal: return "Equal";
      case CompareFunc::NotEqual: return "NotEqual";
    }
    return "Unknown";
  }

  static std::string Get(const AddressMode &el)
  {
    switch(el)
    {
      case AddressMode::Wrap: return "Wrap";
      case AddressMode::Mirror: return "Mirror";
      case AddressMode::MirrorOnce: return "Mirror Once";
      case AddressMode::ClampEdge: return "Clamp Edge";
      case AddressMode::ClampBorder: return "Clamp Border";
    }
    return "Unknown";
  }

  static std::string Get(const BlendMultiplier &el)
  {
    switch(el)
    {
      case BlendMultiplier::Zero: return "Zero";
      case BlendMultiplier::One: return "One";
      case BlendMultiplier::SrcCol: return "Src Col";
      case BlendMultiplier::InvSrcCol: return "1 - Src Col";
      case BlendMultiplier::DstCol: return "Dst Col";
      case BlendMultiplier::InvDstCol: return "1 - Dst Col";
      case BlendMultiplier::SrcAlpha: return "Src Alpha";
      case BlendMultiplier::InvSrcAlpha: return "1 - Src Alpha";
      case BlendMultiplier::DstAlpha: return "Dst Alpha";
      case BlendMultiplier::InvDstAlpha: return "1 - Dst Alpha";
      case BlendMultiplier::SrcAlphaSat: return "Src Alpha Sat";
      case BlendMultiplier::FactorRGB: return "Constant RGB";
      case BlendMultiplier::InvFactorRGB: return "1 - Constant RGB";
      case BlendMultiplier::FactorAlpha: return "Constant A";
      case BlendMultiplier::InvFactorAlpha: return "1 - Constant A";
      case BlendMultiplier::Src1Col: return "Src1 Col";
      case BlendMultiplier::InvSrc1Col: return "1 - Src1 Col";
      case BlendMultiplier::Src1Alpha: return "Src1 Alpha";
      case BlendMultiplier::InvSrc1Alpha: return "1 - Src1 Alpha";
    }
    return "Unknown";
  }

  static std::string Get(const BlendOp &el)
  {
    switch(el)
    {
      case BlendOp::Add: return "Add";
      case BlendOp::Subtract: return "Subtract";
      case BlendOp::ReversedSubtract: return "Rev. Subtract";
      case BlendOp::Minimum: return "Minimum";
      case BlendOp::Maximum: return "Maximum";
    }
    return "Unknown";
  }

  static std::string Get(const StencilOp &el)
  {
    switch(el)
    {
      case StencilOp::Keep: return "Keep";
      case StencilOp::Zero: return "Zero";
      case StencilOp::Replace: return "Replace";
      case StencilOp::IncSat: return "Inc Sat";
      case StencilOp::DecSat: return "Dec Sat";
      case StencilOp::IncWrap: return "Inc Wrap";
      case StencilOp::DecWrap: return "Dec Wrap";
      case StencilOp::Invert: return "Invert";
    }
    return "Unknown";
  }

  static std::string Get(const LogicOp &el)
  {
    switch(el)
    {
      case LogicOp::NoOp: return "No-Op";
      case LogicOp::Clear: return "Clear";
      case LogicOp::Set: return "Set";
      case LogicOp::Copy: return "Copy";
      case LogicOp::CopyInverted: return "Copy Inverted";
      case LogicOp::Invert: return "Invert";
      case LogicOp::And: return "And";
      case LogicOp::Nand: return "Nand";
      case LogicOp::Or: return "Or";
      case LogicOp::Xor: return "Xor";
      case LogicOp::Nor: return "Nor";
      case LogicOp::Equivalent: return "Equivalent";
      case LogicOp::AndReverse: return "And Reverse";
      case LogicOp::AndInverted: return "And Inverted";
      case LogicOp::OrReverse: return "Or Reverse";
      case LogicOp::OrInverted: return "Or Inverted";
    }
    return "Unknown";
  }

  static std::string Get(const QualityHint &el)
  {
    switch(el)
    {
      case QualityHint::DontCare: return "Don't Care";
      case QualityHint::Nicest: return "Nicest";
      case QualityHint::Fastest: return "Fastest";
    }
    return "Unknown";
  }

  static std::string Get(const TextureFilter &el)
  {
    std::string filter = "";
    std::string filtPrefix = "";
    std::string filtVal = "";

    std::string filters[] = {ToStr::Get(el.minify), ToStr::Get(el.magnify), ToStr::Get(el.mip)};
    std::string filterPrefixes[] = {"Min", "Mag", "Mip"};

    for(int a = 0; a < 3; a++)
    {
      if(a == 0 || filters[a] == filters[a - 1])
      {
        if(filtPrefix != "")
          filtPrefix += "/";
        filtPrefix += filterPrefixes[a];
      }
      else
      {
        filter += filtPrefix + ": " + filtVal + ", ";

        filtPrefix = filterPrefixes[a];
      }
      filtVal = filters[a];
    }

    filter += filtPrefix + ": " + filtVal;

    return filter;
  }

  static std::string Get(const D3DBufferViewFlags &el)
  {
    std::string ret;

    if(el == D3DBufferViewFlags::NoFlags)
      return "";

    if(el & D3DBufferViewFlags::Raw)
      ret += " | Raw";
    if(el & D3DBufferViewFlags::Append)
      ret += " | Append";
    if(el & D3DBufferViewFlags::Counter)
      ret += " | Counter";

    if(!ret.empty())
      ret = ret.substr(3);

    return ret;
  }

  static std::string Get(const TextureDim &el)
  {
    switch(el)
    {
      case TextureDim::Unknown: return "Unknown";
      case TextureDim::Buffer: return "Buffer";
      case TextureDim::Texture1D: return "Texture 1D";
      case TextureDim::Texture1DArray: return "Texture 1D Array";
      case TextureDim::Texture2D: return "Texture 2D";
      case TextureDim::TextureRect: return "Texture Rect";
      case TextureDim::Texture2DArray: return "Texture 2D Array";
      case TextureDim::Texture2DMS: return "Texture 2D MS";
      case TextureDim::Texture2DMSArray: return "Texture 2D MS Array";
      case TextureDim::Texture3D: return "Texture 3D";
      case TextureDim::TextureCube: return "Texture Cube";
      case TextureDim::TextureCubeArray: return "Texture Cube Array";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const ShaderBuiltin &el)
  {
    switch(el)
    {
      case ShaderBuiltin::Undefined: return "Undefined";
      case ShaderBuiltin::Position: return "Position";
      case ShaderBuiltin::PointSize: return "Point Size";
      case ShaderBuiltin::ClipDistance: return "Clip Distance";
      case ShaderBuiltin::CullDistance: return "Cull Distance";
      case ShaderBuiltin::RTIndex: return "RT Index";
      case ShaderBuiltin::ViewportIndex: return "Viewport Index";
      case ShaderBuiltin::VertexIndex: return "Vertex Index";
      case ShaderBuiltin::PrimitiveIndex: return "Primitive Index";
      case ShaderBuiltin::InstanceIndex: return "Instance Index";
      case ShaderBuiltin::DispatchSize: return "Dispatch Size";
      case ShaderBuiltin::DispatchThreadIndex: return "Dispatch Thread Index";
      case ShaderBuiltin::GroupIndex: return "Group Index";
      case ShaderBuiltin::GroupFlatIndex: return "Group Flat Index";
      case ShaderBuiltin::GroupThreadIndex: return "Group Thread Index";
      case ShaderBuiltin::GSInstanceIndex: return "GS Instance Index";
      case ShaderBuiltin::OutputControlPointIndex: return "Output Control Point Index";
      case ShaderBuiltin::DomainLocation: return "Domain Location";
      case ShaderBuiltin::IsFrontFace: return "Is FrontFace";
      case ShaderBuiltin::MSAACoverage: return "MSAA Coverage";
      case ShaderBuiltin::MSAASamplePosition: return "MSAA Sample Position";
      case ShaderBuiltin::MSAASampleIndex: return "MSAA Sample Index";
      case ShaderBuiltin::PatchNumVertices: return "Patch NumVertices";
      case ShaderBuiltin::OuterTessFactor: return "Outer TessFactor";
      case ShaderBuiltin::InsideTessFactor: return "Inside TessFactor";
      case ShaderBuiltin::ColorOutput: return "Color Output";
      case ShaderBuiltin::DepthOutput: return "Depth Output";
      case ShaderBuiltin::DepthOutputGreaterEqual: return "Depth Output (GEqual)";
      case ShaderBuiltin::DepthOutputLessEqual: return "Depth Output (LEqual)";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const BindType &el)
  {
    switch(el)
    {
      case BindType::ConstantBuffer: return "Constants";
      case BindType::Sampler: return "Sampler";
      case BindType::ImageSampler: return "Image&Sampler";
      case BindType::ReadOnlyImage: return "Image";
      case BindType::ReadWriteImage: return "RW Image";
      case BindType::ReadOnlyTBuffer: return "TexBuffer";
      case BindType::ReadWriteTBuffer: return "RW TexBuffer";
      case BindType::ReadOnlyBuffer: return "Buffer";
      case BindType::ReadWriteBuffer: return "RW Buffer";
      case BindType::InputAttachment: return "Input";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const MessageSource &el)
  {
    switch(el)
    {
      case MessageSource::API: return "API";
      case MessageSource::RedundantAPIUse: return "Redundant API Use";
      case MessageSource::IncorrectAPIUse: return "Incorrect API Use";
      case MessageSource::GeneralPerformance: return "General Performance";
      case MessageSource::GCNPerformance: return "GCN Performance";
      case MessageSource::RuntimeWarning: return "Runtime Warning";
      case MessageSource::UnsupportedConfiguration: return "Unsupported Configuration";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const MessageSeverity &el)
  {
    switch(el)
    {
      case MessageSeverity::High: return "High";
      case MessageSeverity::Medium: return "Medium";
      case MessageSeverity::Low: return "Low";
      case MessageSeverity::Info: return "Info";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const MessageCategory &el)
  {
    switch(el)
    {
      case MessageCategory::Application_Defined: return "Application Defined";
      case MessageCategory::Miscellaneous: return "Miscellaneous";
      case MessageCategory::Initialization: return "Initialization";
      case MessageCategory::Cleanup: return "Cleanup";
      case MessageCategory::Compilation: return "Compilation";
      case MessageCategory::State_Creation: return "State Creation";
      case MessageCategory::State_Setting: return "State Setting";
      case MessageCategory::State_Getting: return "State Getting";
      case MessageCategory::Resource_Manipulation: return "Resource Manipulation";
      case MessageCategory::Execution: return "Execution";
      case MessageCategory::Shaders: return "Shaders";
      case MessageCategory::Deprecated: return "Deprecated";
      case MessageCategory::Undefined: return "Undefined";
      case MessageCategory::Portability: return "Portability";
      case MessageCategory::Performance: return "Performance";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const TextureSwizzle &el)
  {
    switch(el)
    {
      case TextureSwizzle::Red: return "R";
      case TextureSwizzle::Green: return "G";
      case TextureSwizzle::Blue: return "B";
      case TextureSwizzle::Alpha: return "A";
      case TextureSwizzle::Zero: return "0";
      case TextureSwizzle::One: return "1";
    }
    return "Unknown";
  }

  static std::string Get(const VarType &el)
  {
    switch(el)
    {
      case VarType::Float: return "float";
      case VarType::Int: return "int";
      case VarType::UInt: return "uint";
      case VarType::Double: return "double";
      case VarType::Unknown: break;
    }
    return "Unknown";
  }
};

// this will be here to lighten the burden of converting from std::string to
// QString everywhere.

template <typename T>
inline QString ToQStr(const T &el)
{
  return QString::fromStdString(ToStr::Get(el));
}

// overload for rdctype::str
template <>
inline QString ToQStr(const rdctype::str &el)
{
  return QString::fromUtf8(el.elems, el.count);
}

// overload for a couple of things that need to know the pipeline type when converting
QString ToQStr(const ResourceUsage usage, const GraphicsAPI apitype);

// overload for a couple of things that need to know the pipeline type when converting
QString ToQStr(const ShaderStage stage, const GraphicsAPI apitype);

struct FormatElement
{
  Q_DECLARE_TR_FUNCTIONS(FormatElement);

public:
  FormatElement();
  FormatElement(const QString &Name, int buf, uint offs, bool perInst, int instRate, bool rowMat,
                uint matDim, ResourceFormat f, bool hexDisplay, bool rgbDisplay);

  static QList<FormatElement> ParseFormatString(const QString &formatString, uint64_t maxLen,
                                                bool tightPacking, QString &errors);

  QVariantList GetVariants(const byte *&data, const byte *end) const;
  ShaderVariable GetShaderVar(const byte *&data, const byte *end) const;

  uint32_t byteSize() const;

  QString name;
  ResourceFormat format;
  ShaderBuiltin systemValue;
  int buffer;
  uint32_t offset;
  int instancerate;
  uint32_t matrixdim;
  bool perinstance;
  bool rowmajor;
  bool hex, rgb;
};

QString TypeString(const ShaderVariable &v);
QString RowString(const ShaderVariable &v, uint32_t row, VarType type = VarType::Unknown);
QString VarString(const ShaderVariable &v);
QString RowTypeString(const ShaderVariable &v);

QString TypeString(const SigParameter &sig);
QString D3DSemanticString(const SigParameter &sig);
QString GetComponentString(byte mask);

struct Formatter
{
  static void setParams(const PersistantConfig &config);
  static void shutdown();

  static QString Format(double f, bool hex = false);
  static QString Format(uint64_t u, bool hex = false)
  {
    return QFormatStr("%1").arg(u, hex ? 16 : 0, hex ? 16 : 10, QLatin1Char('0')).toUpper();
  }
  static QString Format(uint32_t u, bool hex = false)
  {
    return QFormatStr("%1").arg(u, hex ? 8 : 0, hex ? 16 : 10, QLatin1Char('0')).toUpper();
  }
  static QString Format(uint16_t u, bool hex = false)
  {
    return QFormatStr("%1").arg(u, hex ? 4 : 0, hex ? 16 : 10, QLatin1Char('0')).toUpper();
  }
  static QString Format(uint8_t u, bool hex = false)
  {
    return QFormatStr("%1").arg(u, hex ? 2 : 0, hex ? 16 : 10, QLatin1Char('0')).toUpper();
  }
  static QString HexFormat(uint32_t u, uint32_t byteSize)
  {
    if(byteSize == 1)
      return Format(uint8_t(u & 0xff), true);
    else if(byteSize == 2)
      return Format(uint16_t(u & 0xffff), true);
    else
      return Format(u, true);
  }
  static QString Format(int32_t i, bool hex = false) { return QString::number(i); }
  static const QFont &PreferredFont() { return *m_Font; }
private:
  static int m_minFigures, m_maxFigures, m_expNegCutoff, m_expPosCutoff;
  static double m_expNegValue, m_expPosValue;
  static QFont *m_Font;
};

bool SaveToJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion);
bool LoadFromJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier,
                  uint32_t magicVersion);

// implementation of QOverload, to avoid depending on 5.7.
// From: http://stackoverflow.com/a/16795664/4070143
template <typename... Args>
struct OverloadedSlot
{
  template <typename C, typename R>
  static constexpr auto of(R (C::*pmf)(Args...)) -> decltype(pmf)
  {
    return pmf;
  }
};

// Utility class for invoking a lambda on the GUI thread.
// This is supported by QTimer::singleShot on Qt 5.4 but it's probably
// wise not to require a higher version that necessary.
#include <functional>

class GUIInvoke : public QObject
{
private:
  Q_OBJECT
  GUIInvoke(const std::function<void()> &f) : func(f) {}
  GUIInvoke() {}
  std::function<void()> func;

  static int methodIndex;

public:
  static void init();
  static void call(const std::function<void()> &f);
  static void blockcall(const std::function<void()> &f);
  static bool onUIThread();

protected slots:
  void doInvoke()
  {
    func();
    deleteLater();
  }
};

// Utility class for calling a lambda on a new thread.
#include <QThread>

class LambdaThread : public QObject
{
private:
  Q_OBJECT

  std::function<void()> m_func;
  QThread *m_Thread;
  QSemaphore completed;
  bool m_SelfDelete = false;

public slots:
  void process()
  {
    m_func();
    m_Thread->quit();
    m_Thread = NULL;
    if(m_SelfDelete)
      deleteLater();
    completed.acquire();
  }

  void selfDelete(bool d) { m_SelfDelete = d; }
public:
  explicit LambdaThread(std::function<void()> f)
  {
    completed.release();
    m_Thread = new QThread();
    m_func = f;
    moveToThread(m_Thread);
    QObject::connect(m_Thread, &QThread::started, this, &LambdaThread::process);
    QObject::connect(m_Thread, &QThread::finished, m_Thread, &QThread::deleteLater);
  }

  void start(QThread::Priority prio = QThread::InheritPriority) { m_Thread->start(prio); }
  bool isRunning() { return completed.available(); }
  bool wait(unsigned long time = ULONG_MAX)
  {
    if(m_Thread)
      return m_Thread->wait(time);
    return true;
  }

  bool isCurrentThread() { return QThread::currentThread() == m_Thread; }
};

class RDProcess : public QProcess
{
public:
  RDProcess(QObject *parent = NULL) : QProcess(parent) {}
  void detach() { setProcessState(QProcess::NotRunning); }
};

class QFileFilterModel : public QSortFilterProxyModel
{
  Q_OBJECT

public:
  explicit QFileFilterModel(QObject *parent = Q_NULLPTR) : QSortFilterProxyModel(parent) {}
  void setRequirePermissions(QDir::Filters mask) { m_requireMask = mask; }
  void setExcludePermissions(QDir::Filters mask) { m_excludeMask = mask; }
protected:
  virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
  QDir::Filters m_requireMask, m_excludeMask;
};

class QMenu;

// helper for doing a manual blocking invoke of a dialog
struct RDDialog
{
  static const QMessageBox::StandardButtons YesNoCancel;

  static void show(QMenu *menu, QPoint pos);
  static int show(QDialog *dialog);
  static QMessageBox::StandardButton messageBox(
      QMessageBox::Icon, QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

  static QMessageBox::StandardButton information(
      QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
  {
    return messageBox(QMessageBox::Information, parent, title, text, buttons, defaultButton);
  }

  static QMessageBox::StandardButton question(
      QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::StandardButtons(QMessageBox::Yes |
                                                                          QMessageBox::No),
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
  {
    return messageBox(QMessageBox::Question, parent, title, text, buttons, defaultButton);
  }

  static QMessageBox::StandardButton warning(
      QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
  {
    return messageBox(QMessageBox::Warning, parent, title, text, buttons, defaultButton);
  }

  static QMessageBox::StandardButton critical(
      QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
  {
    return messageBox(QMessageBox::Critical, parent, title, text, buttons, defaultButton);
  }

  static QString getExistingDirectory(QWidget *parent = NULL, const QString &caption = QString(),
                                      const QString &dir = QString(),
                                      QFileDialog::Options options = QFileDialog::ShowDirsOnly);

  static QString getOpenFileName(QWidget *parent = NULL, const QString &caption = QString(),
                                 const QString &dir = QString(), const QString &filter = QString(),
                                 QString *selectedFilter = NULL,
                                 QFileDialog::Options options = QFileDialog::Options());

  static QString getExecutableFileName(QWidget *parent = NULL, const QString &caption = QString(),
                                       const QString &dir = QString(),
                                       QFileDialog::Options options = QFileDialog::Options());

  static QString getSaveFileName(QWidget *parent = NULL, const QString &caption = QString(),
                                 const QString &dir = QString(), const QString &filter = QString(),
                                 QString *selectedFilter = NULL,
                                 QFileDialog::Options options = QFileDialog::Options());
};

class QGridLayout;

void addGridLines(QGridLayout *grid, QColor gridColor);

class QProgressDialog;

typedef std::function<float()> ProgressUpdateMethod;
typedef std::function<bool()> ProgressFinishedMethod;

QStringList ParseArgsList(const QString &args);
bool IsRunningAsAdmin();
bool RunProcessAsAdmin(const QString &fullExecutablePath, const QStringList &params,
                       std::function<void()> finishedCallback = std::function<void()>());

void ShowProgressDialog(QWidget *window, const QString &labelText, ProgressFinishedMethod finished,
                        ProgressUpdateMethod update = ProgressUpdateMethod());

void setEnabledMultiple(const QList<QWidget *> &widgets, bool enabled);

QString GetSystemUsername();

float getLuminance(const QColor &col);
QColor contrastingColor(const QColor &col, const QColor &defaultCol);
