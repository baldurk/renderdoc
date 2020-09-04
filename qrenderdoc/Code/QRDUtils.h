/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include <QCheckBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QSemaphore>
#include <QSharedPointer>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include "Code/Interface/QRDInterface.h"

template <typename T>
inline T AlignUp(T x, T a)
{
  return (x + (a - 1)) & (~(a - 1));
}

#ifndef ARRAY_COUNT
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

// this will be here to lighten the burden of converting from rdcstr to
// QString everywhere.

template <typename T>
inline QString ToQStr(const T &el)
{
  return QString(ToStr(el));
}

// overload for a couple of things that need to know the pipeline type when converting
QString ToQStr(const ResourceUsage usage, const GraphicsAPI apitype);

// overload for a couple of things that need to know the pipeline type when converting
QString ToQStr(const ShaderStage stage, const GraphicsAPI apitype);

// overload for a couple of things that need to know the pipeline type when converting
QString ToQStr(const AddressMode addr, const GraphicsAPI apitype);

inline QMetaType::Type GetVariantMetatype(const QVariant &v)
{
  // this is explicitly called out by the documentation as the recommended process:
  // "Although this function is declared as returning QVariant::Type, the return value should be
  // interpreted as QMetaType::Type."
  // Suppress static analysis complaints about the enums mismatching:
  // coverity[mixed_enums]
  return (QMetaType::Type)v.type();
}

struct BufferFormatter
{
  Q_DECLARE_TR_FUNCTIONS(BufferFormatter);

  static GraphicsAPI m_API;

  static QString DeclareStruct(QList<QString> &declaredStructs, const QString &name,
                               const rdcarray<ShaderConstant> &members, uint32_t requiredByteStride);

  static uint32_t GetVarSize(const ShaderConstant &var);

public:
  BufferFormatter() = default;

  static void Init(GraphicsAPI api) { m_API = api; }
  static ShaderConstant ParseFormatString(const QString &formatString, uint64_t maxLen,
                                          bool tightPacking, QString &errors);

  static QString GetTextureFormatString(const TextureDescription &tex);
  static QString GetBufferFormatString(const ShaderResource &res, const ResourceFormat &viewFormat,
                                       uint64_t &baseByteOffset);

  static QString DeclareStruct(const QString &name, const rdcarray<ShaderConstant> &members,
                               uint32_t requiredByteStride);
  static QString DeclarePaddingBytes(uint32_t bytes);
};

QVariantList GetVariants(ResourceFormat format, const ShaderVariableDescriptor &varDesc,
                         const byte *&data, const byte *end);
ResourceFormat GetInterpretedResourceFormat(const ShaderConstant &elem);
void SetInterpretedResourceFormat(ShaderConstant &elem, ResourceFormatType interpretType,
                                  CompType interpretCompType);
ShaderVariable InterpretShaderVar(const ShaderConstant &elem, const byte *data, const byte *end);

QString TypeString(const ShaderVariable &v);
QString RowString(const ShaderVariable &v, uint32_t row, VarType type = VarType::Unknown);
QString VarString(const ShaderVariable &v);
QString RowTypeString(const ShaderVariable &v);

QString TypeString(const SigParameter &sig);
QString D3DSemanticString(const SigParameter &sig);
QString GetComponentString(byte mask);

void CombineUsageEvents(
    ICaptureContext &ctx, const rdcarray<EventUsage> &usage,
    std::function<void(uint32_t startEID, uint32_t endEID, ResourceUsage use)> callback);

class RDTreeWidgetItem;

void addStructuredObjects(RDTreeWidgetItem *parent, const StructuredObjectList &objs,
                          bool parentIsArray);

struct PointerTypeRegistry
{
public:
  static void Init();

  static void CacheShader(const ShaderReflection *reflection);

  static uint32_t GetTypeID(ResourceId shader, uint32_t pointerTypeId);
  static uint32_t GetTypeID(PointerVal val) { return GetTypeID(val.shader, val.pointerTypeID); }
  static uint32_t GetTypeID(const ShaderVariableType &structDef);

  static const ShaderVariableType &GetTypeDescriptor(uint32_t typeId);
  static const ShaderVariableType &GetTypeDescriptor(ResourceId shader, uint32_t pointerTypeId)
  {
    return GetTypeDescriptor(GetTypeID(shader, pointerTypeId));
  }
  static const ShaderVariableType &GetTypeDescriptor(PointerVal val)
  {
    return GetTypeDescriptor(GetTypeID(val));
  }

private:
  static void CacheSubTypes(const ShaderReflection *reflection, ShaderVariableType &structDef);

  static QMap<QPair<ResourceId, uint32_t>, uint32_t> typeMapping;
  static rdcarray<ShaderVariableType> typeDescriptions;
};

struct GPUAddress
{
  GPUAddress() = default;
  GPUAddress(const PointerVal &v) : val(v) {}
  PointerVal val;

  // cached data
  ResourceId base;
  uint64_t offset = 0;

  // cache the context once we've obtained it.
  ICaptureContext *ctxptr = NULL;

  void cacheAddress(const QWidget *widget);
};

// this will check the variant, and if it contains a ResourceId directly or text with ResourceId
// identifiers then it will be converted into a RichResourceTextPtr or ResourceId in-place. The new
// QVariant will still convert to QString so it doesn't have to be special-cased. However it must be
// processed through one of the functions below (generally painting) to cache the rendered text. If
// the variant doesn't match the above conditions, it's unchanged. So it's safe to apply this
// reasonably liberally.
//
// In this case the variant may not actually be a complex RichResourceText object, that's only used
// when there is text with ResourceId(s) inside it. If the text is just a ResourceId on its own
// (which is quite common) it will be stored as just a ResourceId but will still be painted & mouse
// handled the same way, and all of the below functions can be used on the variant either way.
//
// NOTE: It is not possible to move a RichResourceText instance from one ICaptureContext to another
// as the pointer is cached internally. Instead you should delete the old and re-initialise from
// scratch.
void RichResourceTextInitialise(QVariant &var);

// Checks if a variant is rich resource text and should be treated specially
// Particularly meaning we need mouse tracking on the widget to handle the on-hover highlighting
// and mouse clicks
bool RichResourceTextCheck(const QVariant &var);

// Paint the given variant containing rich text with the given parameters.
void RichResourceTextPaint(const QWidget *owner, QPainter *painter, QRect rect, QFont font,
                           QPalette palette, bool mouseOver, QPoint mousePos, const QVariant &var);

// Gives the width for a size hint for the rich text (since it might be larger than the original
// text)
int RichResourceTextWidthHint(const QWidget *owner, const QFont &font, const QVariant &var);

// Handle a mouse event on some rich resource text.
// Returns true if the event is processed - for mouse move events, this means that the mouse is over
// a resource link (which can be used to change the cursor to a pointing hand, for example).
bool RichResourceTextMouseEvent(const QWidget *owner, const QVariant &var, QRect rect,
                                const QFont &font, QMouseEvent *event);

// immediately format a variant that may contain rich resource text. For use in places where we
// can't paint rich resource text but we still want to display the string nicely
QString RichResourceTextFormat(ICaptureContext &ctx, QVariant var);

// Register runtime conversions for custom Qt metatypes
void RegisterMetatypeConversions();

struct Formatter
{
  static void setParams(const PersistantConfig &config);
  static void setPalette(QPalette palette);
  static void shutdown();

  static QString Format(double f, bool hex = false);
  static QString HumanFormat(uint64_t u);
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
  static QString Format(bool b, bool hex = false) { return b ? lit("true") : lit("false"); }
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
  static QString Format(int64_t i, bool hex = false) { return QString::number(i); }
  static const QFont &PreferredFont() { return *m_Font; }
  static const QColor DarkCheckerColor() { return m_DarkChecker; }
  static const QColor LightCheckerColor() { return m_LightChecker; }
private:
  static int m_minFigures, m_maxFigures, m_expNegCutoff, m_expPosCutoff;
  static double m_expNegValue, m_expPosValue;
  static QFont *m_Font;
  static float m_FontBaseSize;
  static QColor m_DarkChecker, m_LightChecker;
};

bool SaveToJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion);
bool LoadFromJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier,
                  uint32_t magicVersion);

QString VariantToJSON(const QVariantMap &data);
QVariantMap JSONToVariant(const QString &json);

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

#include <QPointer>

class GUIInvoke : public QObject
{
private:
  Q_OBJECT
  GUIInvoke(QObject *obj, const std::function<void()> &f) : ptr(obj), func(f) {}
  QPointer<QObject> ptr;
  std::function<void()> func;

  static int methodIndex;

public:
  static void init();
  static void call(QObject *obj, const std::function<void()> &f);
  static void blockcall(QObject *obj, const std::function<void()> &f);
  static bool onUIThread();

  // same as call() above, but it doesn't check for an instant call on the UI thread
  static void defer(QObject *obj, const std::function<void()> &f);

protected slots:
  void doInvoke()
  {
    if(ptr)
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
  QString m_Name;

  void windowsSetName();

public slots:
  void process()
  {
    if(!m_Name.isEmpty())
      windowsSetName();
    m_func();
    m_Thread->quit();
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

  ~LambdaThread() { m_Thread->deleteLater(); }
  void setName(QString name)
  {
    m_Name = name;
    m_Thread->setObjectName(name);
  }
  void start(QThread::Priority prio = QThread::InheritPriority) { m_Thread->start(prio); }
  bool isRunning() { return completed.available(); }
  bool wait(unsigned long time = ULONG_MAX)
  {
    if(m_Thread)
      return m_Thread->wait(time);
    return true;
  }

  void moveObjectToThread(QObject *o) { o->moveToThread(m_Thread); }
  bool isCurrentThread() { return QThread::currentThread() == m_Thread; }
};

class RDProcess : public QProcess
{
public:
  RDProcess(QObject *parent = NULL) : QProcess(parent) {}
  void detach() { setProcessState(QProcess::NotRunning); }
};

class RegisteredMenuItem : public QObject
{
private:
  Q_OBJECT
public:
  ContextMenu context;
  PanelMenu panel;
  rdcarray<rdcstr> submenus;
  IExtensionManager::ExtensionCallback callback;
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

class QCollator;

class QCollatorSortFilterProxyModel : public QSortFilterProxyModel
{
  Q_OBJECT

public:
  explicit QCollatorSortFilterProxyModel(QObject *parent = Q_NULLPTR);
  ~QCollatorSortFilterProxyModel();

  QCollator *collator() { return m_collator; }
protected:
  virtual bool lessThan(const QModelIndex &source_left,
                        const QModelIndex &source_right) const override;

private:
  QCollator *m_collator;
};

// Simple QStyledItemDelegate child that will either forward to an external delegate (allowing
// chaining) or to the base implementation. Delegates can derive from this and specialise a couple
// of functions to still be able to chain
class ForwardingDelegate : public QStyledItemDelegate
{
  Q_OBJECT
public:
  explicit ForwardingDelegate(QObject *parent = NULL) : QStyledItemDelegate(parent) {}
  ~ForwardingDelegate() {}
  void setForwardDelegate(QAbstractItemDelegate *real)
  {
    if(m_delegate)
    {
      QObject::disconnect(m_delegate, &QAbstractItemDelegate::commitData, this,
                          &QAbstractItemDelegate::commitData);
      QObject::disconnect(m_delegate, &QAbstractItemDelegate::closeEditor, this,
                          &QAbstractItemDelegate::closeEditor);
      QObject::disconnect(m_delegate, &QAbstractItemDelegate::sizeHintChanged, this,
                          &QAbstractItemDelegate::sizeHintChanged);
    }
    m_delegate = real;
    if(m_delegate)
    {
      QObject::connect(m_delegate, &QAbstractItemDelegate::commitData, this,
                       &QAbstractItemDelegate::commitData);
      QObject::connect(m_delegate, &QAbstractItemDelegate::closeEditor, this,
                       &QAbstractItemDelegate::closeEditor);
      QObject::connect(m_delegate, &QAbstractItemDelegate::sizeHintChanged, this,
                       &QAbstractItemDelegate::sizeHintChanged);
    }
  }
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override
  {
    if(m_delegate)
      return m_delegate->paint(painter, option, index);

    return QStyledItemDelegate::paint(painter, option, index);
  }

  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
  {
    if(m_delegate)
      return m_delegate->sizeHint(option, index);

    return QStyledItemDelegate::sizeHint(option, index);
  }

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override
  {
    if(m_delegate)
      return m_delegate->createEditor(parent, option, index);

    return QStyledItemDelegate::createEditor(parent, option, index);
  }

  void destroyEditor(QWidget *editor, const QModelIndex &index) const override
  {
    if(m_delegate)
      return m_delegate->destroyEditor(editor, index);

    return QStyledItemDelegate::destroyEditor(editor, index);
  }

  void setEditorData(QWidget *editor, const QModelIndex &index) const override
  {
    if(m_delegate)
      return m_delegate->setEditorData(editor, index);

    return QStyledItemDelegate::setEditorData(editor, index);
  }

  void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
  {
    if(m_delegate)
      return m_delegate->setModelData(editor, model, index);

    return QStyledItemDelegate::setModelData(editor, model, index);
  }

  void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const override
  {
    if(m_delegate)
      return m_delegate->updateEditorGeometry(editor, option, index);

    return QStyledItemDelegate::updateEditorGeometry(editor, option, index);
  }

  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if(m_delegate)
      return m_delegate->eventFilter(watched, event);

    return QStyledItemDelegate::eventFilter(watched, event);
  }

  bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override
  {
    if(m_delegate)
      return m_delegate->editorEvent(event, model, option, index);

    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }

  bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                 const QModelIndex &index) override
  {
    if(m_delegate)
      return m_delegate->helpEvent(event, view, option, index);

    return QStyledItemDelegate::helpEvent(event, view, option, index);
  }

  QVector<int> paintingRoles() const override
  {
    if(m_delegate)
      return m_delegate->paintingRoles();

    return QStyledItemDelegate::paintingRoles();
  }

private:
  QAbstractItemDelegate *m_delegate = NULL;
};

// delegate that will handle painting, hovering and clicking on rich text items.
// owning view needs to call linkHover, and adjust its cursor and repaint as necessary.
class RichTextViewDelegate : public ForwardingDelegate
{
  Q_OBJECT
public:
  explicit RichTextViewDelegate(QAbstractItemView *parent);
  ~RichTextViewDelegate();

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

  bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

  bool linkHover(QMouseEvent *e, const QFont &font, const QModelIndex &index);

private:
  QAbstractItemView *m_widget;
};

// helper functions for using a double spinbox for 64-bit integers. We do this because it's
// infeasible in Qt to actually derive and create a real 64-bit integer spinbox because critical
// functionality depends on deriving QAbstractSpinBoxPrivate which is unavailable. So instead we use
// a QDoubleSpinBox and restrict ourselves to the 53 bits of mantissa. This struct only has inline
// helpers so we can cast a QDoubleSpinBox to one of these and use it as-is.
class RDSpinBox64 : public QDoubleSpinBox
{
private:
  static const qlonglong mask = (1ULL << 53U) - 1;

public:
  void configure() { QDoubleSpinBox::setDecimals(0); }
  void setSingleStep(qlonglong val) { QDoubleSpinBox::setSingleStep(makeValue(val)); }
  void setMinimum(qlonglong min) { QDoubleSpinBox::setMinimum(makeValue(min)); }
  void setMaximum(qlonglong max) { QDoubleSpinBox::setMaximum(makeValue(max)); }
  void setRange(qlonglong min, qlonglong max)
  {
    RDSpinBox64::setMinimum(min);
    RDSpinBox64::setMaximum(max);
  }

  void setSingleStep(qulonglong val) { QDoubleSpinBox::setSingleStep(makeValue(val)); }
  void setMinimum(qulonglong min) { QDoubleSpinBox::setMinimum(makeValue(min)); }
  void setMaximum(qulonglong max) { QDoubleSpinBox::setMaximum(makeValue(max)); }
  void setRange(qulonglong min, qulonglong max)
  {
    RDSpinBox64::setMinimum(min);
    RDSpinBox64::setMaximum(max);
  }

  static qlonglong getValue(double d) { return qlonglong(d); }
  static qulonglong getUValue(double d) { return qulonglong(d); }
  static double makeValue(qlonglong l) { return l < 0 ? -double((-l) & mask) : double(l & mask); }
  static double makeValue(qulonglong l) { return double(l & mask); }
};

class QMenu;

// helper for doing a manual blocking invoke of a dialog
struct RDDialog
{
  static const QMessageBox::StandardButtons YesNoCancel;

  static QString DefaultBrowsePath;

  static void show(QMenu *menu, QPoint pos);
  static int show(QDialog *dialog);
  static QMessageBox::StandardButton messageBox(
      QMessageBox::Icon, QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
  static QMessageBox::StandardButton messageBoxChecked(
      QMessageBox::Icon, QWidget *parent, const QString &title, const QString &text,
      QCheckBox *checkBox, bool &checked, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
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

  static QMessageBox::StandardButton questionChecked(
      QWidget *parent, const QString &title, const QString &text, QCheckBox *checkBox, bool &checked,
      QMessageBox::StandardButtons buttons = QMessageBox::StandardButtons(QMessageBox::Yes |
                                                                          QMessageBox::No),
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
  {
    return messageBoxChecked(QMessageBox::Question, parent, title, text, checkBox, checked, buttons,
                             defaultButton);
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
                                       const QString &defaultExe = QString(),
                                       QFileDialog::Options options = QFileDialog::Options());

  static QString getSaveFileName(QWidget *parent = NULL, const QString &caption = QString(),
                                 const QString &dir = QString(), const QString &filter = QString(),
                                 QString *selectedFilter = NULL,
                                 QFileDialog::Options options = QFileDialog::Options());
};

class QGridLayout;

void addGridLines(QGridLayout *grid, QColor gridColor);

class QProgressDialog;
class QProgressBar;
class QElapsedTimer;

typedef std::function<float()> ProgressUpdateMethod;
typedef std::function<bool()> ProgressFinishedMethod;
typedef std::function<void()> ProgressCancelMethod;

QStringList ParseArgsList(const QString &args);
bool IsRunningAsAdmin();
bool RunProcessAsAdmin(const QString &fullExecutablePath, const QStringList &params,
                       QWidget *parent = NULL, bool hidden = false,
                       std::function<void()> finishedCallback = std::function<void()>());

void RevealFilenameInExternalFileBrowser(const QString &filePath);

void ShowProgressDialog(QWidget *window, const QString &labelText, ProgressFinishedMethod finished,
                        ProgressUpdateMethod update = ProgressUpdateMethod(),
                        ProgressCancelMethod cancel = ProgressCancelMethod());

void UpdateTransferProgress(qint64 xfer, qint64 total, QElapsedTimer *timer,
                            QProgressBar *progressBar, QLabel *progressLabel, QString progressText);

void setEnabledMultiple(const QList<QWidget *> &widgets, bool enabled);

QString GetSystemUsername();

void BringToForeground(QWidget *window);

bool IsDarkTheme();

float getLuminance(const QColor &col);
QColor contrastingColor(const QColor &col, const QColor &defaultCol);

void *AccessWaylandPlatformInterface(const QByteArray &resource, QWindow *window);
