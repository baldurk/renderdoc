/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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
#include "renderdoc_replay.h"

// total hack, expose the same basic interface as on renderdoc side.
// Eventually we want to move the code in the main project into header-only
// and .inl implementations for at least the public API, so it can be compiled
// directly without duplication

struct ToStr
{
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
};

// this will be here to lighten the burden of converting from std::string to
// QString everywhere.

template <typename T>
QString ToQStr(const T &el)
{
  return QString::fromStdString(ToStr::Get(el));
}

// overload for a couple of things that need to know the pipeline type when converting
QString ToQStr(const ResourceUsage usage, const GraphicsAPI apitype);

// overload for a couple of things that need to know the pipeline type when converting
QString ToQStr(const ShaderStageType stage, const GraphicsAPI apitype);

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
