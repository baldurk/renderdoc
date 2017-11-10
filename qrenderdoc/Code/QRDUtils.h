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

#include <QCheckBox>
#include <QCoreApplication>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QSemaphore>
#include <QSortFilterProxyModel>
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

  // same as call() above, but it doesn't check for an instant call on the UI thread
  static void defer(const std::function<void()> &f);

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

bool IsDarkTheme();

float getLuminance(const QColor &col);
QColor contrastingColor(const QColor &col, const QColor &defaultCol);
