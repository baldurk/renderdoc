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

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QSemaphore>
#include <QSortFilterProxyModel>
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
    return QString::number(el.id).toStdString();
  }

  static std::string Get(const ReplayCreateStatus &el)
  {
    switch(el)
    {
      case eReplayCreate_Success: return "Success";
      case eReplayCreate_UnknownError: return "Unknown error";
      case eReplayCreate_InternalError: return "Internal error";
      case eReplayCreate_FileNotFound: return "File not found";
      case eReplayCreate_InjectionFailed: return "RenderDoc injection failed";
      case eReplayCreate_IncompatibleProcess: return "Process is incompatible";
      case eReplayCreate_NetworkIOFailed: return "Network I/O operation failed";
      case eReplayCreate_NetworkRemoteBusy: return "Remote side of network connection is busy";
      case eReplayCreate_NetworkVersionMismatch: return "Version mismatch between network clients";
      case eReplayCreate_FileIOFailed: return "File I/O failed";
      case eReplayCreate_FileIncompatibleVersion: return "File of incompatible version";
      case eReplayCreate_FileCorrupted: return "File corrupted";
      case eReplayCreate_APIUnsupported: return "API unsupported";
      case eReplayCreate_APIInitFailed: return "API initialisation failed";
      case eReplayCreate_APIIncompatibleVersion: return "API incompatible version";
      case eReplayCreate_APIHardwareUnsupported: return "API hardware unsupported";
      default: break;
    }
    return "Invalid error code";
  }

  static std::string Get(const FormatComponentType &el)
  {
    switch(el)
    {
      case eCompType_None: return "Typeless";
      case eCompType_Float: return "Float";
      case eCompType_UNorm: return "UNorm";
      case eCompType_SNorm: return "SNorm";
      case eCompType_UInt: return "UInt";
      case eCompType_SInt: return "SInt";
      case eCompType_UScaled: return "UScaled";
      case eCompType_SScaled: return "SScaled";
      case eCompType_Depth: return "Depth/Stencil";
      case eCompType_Double: return "Double";
      default: break;
    }
    return "Invalid component type";
  }

  static std::string Get(const FileType &el)
  {
    switch(el)
    {
      case eFileType_DDS: return "DDS";
      case eFileType_PNG: return "PNG";
      case eFileType_JPG: return "JPG";
      case eFileType_BMP: return "BMP";
      case eFileType_TGA: return "TGA";
      case eFileType_HDR: return "HDR";
      case eFileType_EXR: return "EXR";
      default: break;
    }
    return "Invalid file type";
  }

  static std::string Get(const AlphaMapping &el)
  {
    switch(el)
    {
      case eAlphaMap_Discard: return "Discard";
      case eAlphaMap_BlendToColour: return "Blend to Colour";
      case eAlphaMap_BlendToCheckerboard: return "Blend to Checkerboard";
      case eAlphaMap_Preserve: return "Preserve";
      default: break;
    }
    return "Invalid mapping";
  }

  static std::string Get(const EnvironmentModificationType &el)
  {
    switch(el)
    {
      case eEnvMod_Set: return "Set";
      case eEnvMod_Append: return "Append";
      case eEnvMod_Prepend: return "Prepend";
      default: break;
    }
    return "Invalid modification";
  }

  static std::string Get(const EnvironmentSeparator &el)
  {
    switch(el)
    {
      case eEnvSep_Platform: return "Platform style";
      case eEnvSep_SemiColon: return "Semi-colon (;)";
      case eEnvSep_Colon: return "Colon (:)";
      case eEnvSep_None: return "No Separator";
      default: break;
    }
    return "Invalid separator";
  }

  static std::string Get(const PrimitiveTopology &el)
  {
    switch(el)
    {
      case eTopology_Unknown: return "Unknown";
      case eTopology_PointList: return "Point List";
      case eTopology_LineList: return "Line List";
      case eTopology_LineStrip: return "Line Strip";
      case eTopology_LineLoop: return "Line Loop";
      case eTopology_TriangleList: return "Triangle List";
      case eTopology_TriangleStrip: return "Triangle Strip";
      case eTopology_TriangleFan: return "Triangle Fan";
      case eTopology_LineList_Adj: return "Line List with Adjacency";
      case eTopology_LineStrip_Adj: return "Line Strip with Adjacency";
      case eTopology_TriangleList_Adj: return "Triangle List with Adjacency";
      case eTopology_TriangleStrip_Adj: return "Triangle Strip with Adjacency";
      case eTopology_PatchList_1CPs:
      case eTopology_PatchList_2CPs:
      case eTopology_PatchList_3CPs:
      case eTopology_PatchList_4CPs:
      case eTopology_PatchList_5CPs:
      case eTopology_PatchList_6CPs:
      case eTopology_PatchList_7CPs:
      case eTopology_PatchList_8CPs:
      case eTopology_PatchList_9CPs:
      case eTopology_PatchList_10CPs:
      case eTopology_PatchList_11CPs:
      case eTopology_PatchList_12CPs:
      case eTopology_PatchList_13CPs:
      case eTopology_PatchList_14CPs:
      case eTopology_PatchList_15CPs:
      case eTopology_PatchList_16CPs:
      case eTopology_PatchList_17CPs:
      case eTopology_PatchList_18CPs:
      case eTopology_PatchList_19CPs:
      case eTopology_PatchList_20CPs:
      case eTopology_PatchList_21CPs:
      case eTopology_PatchList_22CPs:
      case eTopology_PatchList_23CPs:
      case eTopology_PatchList_24CPs:
      case eTopology_PatchList_25CPs:
      case eTopology_PatchList_26CPs:
      case eTopology_PatchList_27CPs:
      case eTopology_PatchList_28CPs:
      case eTopology_PatchList_29CPs:
      case eTopology_PatchList_30CPs:
      case eTopology_PatchList_31CPs:
      case eTopology_PatchList_32CPs: return "Patch List";
      default: break;
    }
    return "Unknown topology";
  }

  static std::string Get(const TriangleFillMode &el)
  {
    switch(el)
    {
      case eFill_Solid: return "Solid";
      case eFill_Wireframe: return "Wireframe";
      case eFill_Point: return "Point";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const TriangleCullMode &el)
  {
    switch(el)
    {
      case eCull_None: return "None";
      case eCull_Front: return "Front";
      case eCull_Back: return "Back";
      case eCull_FrontAndBack: return "Front & Back";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const ShaderResourceType &el)
  {
    switch(el)
    {
      case eResType_None: return "None";
      case eResType_Buffer: return "Buffer";
      case eResType_Texture1D: return "Texture 1D";
      case eResType_Texture1DArray: return "Texture 1D Array";
      case eResType_Texture2D: return "Texture 2D";
      case eResType_TextureRect: return "Texture Rect";
      case eResType_Texture2DArray: return "Texture 2D Array";
      case eResType_Texture2DMS: return "Texture 2D MS";
      case eResType_Texture2DMSArray: return "Texture 2D MS Array";
      case eResType_Texture3D: return "Texture 3D";
      case eResType_TextureCube: return "Texture Cube";
      case eResType_TextureCubeArray: return "Texture Cube Array";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const SystemAttribute &el)
  {
    switch(el)
    {
      case eAttr_None: return "None";
      case eAttr_Position: return "Position";
      case eAttr_PointSize: return "Point Size";
      case eAttr_ClipDistance: return "Clip Distance";
      case eAttr_CullDistance: return "Cull Distance";
      case eAttr_RTIndex: return "RT Index";
      case eAttr_ViewportIndex: return "Viewport Index";
      case eAttr_VertexIndex: return "Vertex Index";
      case eAttr_PrimitiveIndex: return "Primitive Index";
      case eAttr_InstanceIndex: return "Instance Index";
      case eAttr_InvocationIndex: return "Invocation Index";
      case eAttr_DispatchSize: return "Dispatch Size";
      case eAttr_DispatchThreadIndex: return "Dispatch Thread Index";
      case eAttr_GroupIndex: return "Group Index";
      case eAttr_GroupFlatIndex: return "Group Flat Index";
      case eAttr_GroupThreadIndex: return "Group Thread Index";
      case eAttr_GSInstanceIndex: return "GS Instance Index";
      case eAttr_OutputControlPointIndex: return "Output Control Point Index";
      case eAttr_DomainLocation: return "Domain Location";
      case eAttr_IsFrontFace: return "Is FrontFace";
      case eAttr_MSAACoverage: return "MSAA Coverage";
      case eAttr_MSAASamplePosition: return "MSAA Sample Position";
      case eAttr_MSAASampleIndex: return "MSAA Sample Index";
      case eAttr_PatchNumVertices: return "Patch NumVertices";
      case eAttr_OuterTessFactor: return "Outer TessFactor";
      case eAttr_InsideTessFactor: return "Inside TessFactor";
      case eAttr_ColourOutput: return "Colour Output";
      case eAttr_DepthOutput: return "Depth Output";
      case eAttr_DepthOutputGreaterEqual: return "Depth Output (GEqual)";
      case eAttr_DepthOutputLessEqual: return "Depth Output (LEqual)";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const ShaderBindType &el)
  {
    switch(el)
    {
      case eBindType_ConstantBuffer: return "Constants";
      case eBindType_Sampler: return "Sampler";
      case eBindType_ImageSampler: return "Image&Sampler";
      case eBindType_ReadOnlyImage: return "Image";
      case eBindType_ReadWriteImage: return "RW Image";
      case eBindType_ReadOnlyTBuffer: return "TexBuffer";
      case eBindType_ReadWriteTBuffer: return "RW TexBuffer";
      case eBindType_ReadOnlyBuffer: return "Buffer";
      case eBindType_ReadWriteBuffer: return "RW Buffer";
      case eBindType_InputAttachment: return "Input";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const DebugMessageSource &el)
  {
    switch(el)
    {
      case eDbgSource_API: return "API";
      case eDbgSource_RedundantAPIUse: return "Redundant API Use";
      case eDbgSource_IncorrectAPIUse: return "Incorrect API Use";
      case eDbgSource_GeneralPerformance: return "General Performance";
      case eDbgSource_GCNPerformance: return "GCN Performance";
      case eDbgSource_RuntimeWarning: return "Runtime Warning";
      case eDbgSoruce_UnsupportedConfiguration: return "Unsupported Configuration";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const DebugMessageSeverity &el)
  {
    switch(el)
    {
      case eDbgSeverity_High: return "High";
      case eDbgSeverity_Medium: return "Medium";
      case eDbgSeverity_Low: return "Low";
      case eDbgSeverity_Info: return "Info";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const DebugMessageCategory &el)
  {
    switch(el)
    {
      case eDbgCategory_Application_Defined: return "Application Defined";
      case eDbgCategory_Miscellaneous: return "Miscellaneous";
      case eDbgCategory_Initialization: return "Initialization";
      case eDbgCategory_Cleanup: return "Cleanup";
      case eDbgCategory_Compilation: return "Compilation";
      case eDbgCategory_State_Creation: return "State Creation";
      case eDbgCategory_State_Setting: return "State Setting";
      case eDbgCategory_State_Getting: return "State Getting";
      case eDbgCategory_Resource_Manipulation: return "Resource Manipulation";
      case eDbgCategory_Execution: return "Execution";
      case eDbgCategory_Shaders: return "Shaders";
      case eDbgCategory_Deprecated: return "Deprecated";
      case eDbgCategory_Undefined: return "Undefined";
      case eDbgCategory_Portability: return "Portability";
      case eDbgCategory_Performance: return "Performance";
      default: break;
    }
    return "Unknown";
  }

  static std::string Get(const TextureSwizzle &el)
  {
    switch(el)
    {
      case eSwizzle_Red: return "R";
      case eSwizzle_Green: return "G";
      case eSwizzle_Blue: return "B";
      case eSwizzle_Alpha: return "A";
      case eSwizzle_Zero: return "0";
      case eSwizzle_One: return "1";
    }
    return "Unknown";
  }

  static std::string Get(const VarType &el)
  {
    switch(el)
    {
      case eVar_Float: return "float";
      case eVar_Int: return "int";
      case eVar_UInt: return "uint";
      case eVar_Double: return "double";
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
QString ToQStr(const ShaderStageType stage, const GraphicsAPI apitype);

struct FormatElement
{
  FormatElement();
  FormatElement(const QString &Name, int buf, uint offs, bool perInst, int instRate, bool rowMat,
                uint matDim, ResourceFormat f, bool hexDisplay);

  static QList<FormatElement> ParseFormatString(const QString &formatString, uint64_t maxLen,
                                                bool tightPacking, QString &errors);

  QVariantList GetVariants(const byte *&data, const byte *end) const;
  ShaderVariable GetShaderVar(const byte *&data, const byte *end) const;

  QString ElementString(const QVariant &var);

  uint32_t byteSize() const;

  QString name;
  int buffer;
  uint32_t offset;
  bool perinstance;
  int instancerate;
  bool rowmajor;
  uint32_t matrixdim;
  ResourceFormat format;
  bool hex;
  SystemAttribute systemValue;
};

QString TypeString(const ShaderVariable &v);
QString RowString(const ShaderVariable &v, uint32_t row);
QString VarString(const ShaderVariable &v);
QString RowTypeString(const ShaderVariable &v);

QString TypeString(const SigParameter &sig);
QString D3DSemanticString(const SigParameter &sig);
QString GetComponentString(byte mask);

struct Formatter
{
  static void setParams(int minFigures, int maxFigures, int expNegCutoff, int expPosCutoff);

  static QString Format(double f, bool hex = false);
  static QString Format(uint64_t u, bool hex = false)
  {
    return QString("%1").arg(u, hex ? 16 : 0, hex ? 16 : 10, QChar('0'));
  }
  static QString Format(uint32_t u, bool hex = false)
  {
    return QString("%1").arg(u, hex ? 8 : 0, hex ? 16 : 10, QChar('0'));
  }
  static QString Format(uint16_t u, bool hex = false)
  {
    return QString("%1").arg(u, hex ? 4 : 0, hex ? 16 : 10, QChar('0'));
  }
  static QString Format(int32_t i, bool hex = false) { return QString::number(i); }
private:
  static int m_minFigures, m_maxFigures, m_expNegCutoff, m_expPosCutoff;
  static double m_expNegValue, m_expPosValue;
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
    m_Thread->deleteLater();
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
  }

  void start(QThread::Priority prio = QThread::InheritPriority) { m_Thread->start(prio); }
  bool isRunning() { return completed.available(); }
  bool wait(unsigned long time = ULONG_MAX)
  {
    if(m_Thread)
      return m_Thread->wait(time);
    return true;
  }
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

// useful delegate for enforcing a given size
#include <QItemDelegate>

class SizeDelegate : public QItemDelegate
{
private:
  Q_OBJECT

  QSize m_Size;

public:
  SizeDelegate(QSize size) : m_Size(size) {}
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
  {
    return m_Size;
  }
};

class QGridLayout;

void addGridLines(QGridLayout *grid);

class QTreeWidgetItem;

QTreeWidgetItem *makeTreeNode(const std::initializer_list<QVariant> &values);
QTreeWidgetItem *makeTreeNode(const QVariantList &values);
