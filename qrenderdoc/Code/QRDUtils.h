/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

// this will be here to lighten the burden of converting from std::string to
// QString everywhere.

template <typename T>
inline QString ToQStr(const T &el)
{
  return QString::fromStdString(ToStr(el));
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

struct FormatElement
{
  Q_DECLARE_TR_FUNCTIONS(FormatElement);

public:
  FormatElement();
  FormatElement(const QString &Name, int buf, uint offs, bool perInst, int instRate, bool rowMat,
                uint matDim, ResourceFormat f, bool hexDisplay, bool rgbDisplay);

  static QList<FormatElement> ParseFormatString(const QString &formatString, uint64_t maxLen,
                                                bool tightPacking, QString &errors);

  static QString GenerateTextureBufferFormat(const TextureDescription &tex);

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

void CombineUsageEvents(
    ICaptureContext &ctx, const rdcarray<EventUsage> &usage,
    std::function<void(uint32_t startEID, uint32_t endEID, ResourceUsage use)> callback);

class RDTreeWidgetItem;

void addStructuredObjects(RDTreeWidgetItem *parent, const StructuredObjectList &objs,
                          bool parentIsArray);

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
  void setForwardDelegate(QAbstractItemDelegate *real) { m_delegate = real; }
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

QStringList ParseArgsList(const QString &args);
bool IsRunningAsAdmin();
bool RunProcessAsAdmin(const QString &fullExecutablePath, const QStringList &params,
                       QWidget *parent = NULL, bool hidden = false,
                       std::function<void()> finishedCallback = std::function<void()>());

void RevealFilenameInExternalFileBrowser(const QString &filePath);

void ShowProgressDialog(QWidget *window, const QString &labelText, ProgressFinishedMethod finished,
                        ProgressUpdateMethod update = ProgressUpdateMethod());

void UpdateTransferProgress(qint64 xfer, qint64 total, QElapsedTimer *timer,
                            QProgressBar *progressBar, QLabel *progressLabel, QString progressText);

void setEnabledMultiple(const QList<QWidget *> &widgets, bool enabled);

QString GetSystemUsername();

void BringToForeground(QWidget *window);

bool IsDarkTheme();

float getLuminance(const QColor &col);
QColor contrastingColor(const QColor &col, const QColor &defaultCol);
