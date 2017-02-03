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

#include <QFrame>
#include <functional>
#include "Code/CaptureContext.h"

namespace Ui
{
class CaptureDialog;
}

class QStandardItemModel;
class LiveCapture;

struct CaptureSettings
{
  CaptureSettings();

  CaptureOptions Options;
  bool Inject;
  bool AutoStart;
  QString Executable;
  QString WorkingDir;
  QString CmdLine;
  QList<EnvironmentModification> Environment;

  QVariantMap toJSON() const;
  void fromJSON(const QVariantMap &data);
};

class CaptureDialog : public QFrame
{
  Q_OBJECT

public:
  typedef std::function<LiveCapture *(const QString &exe, const QString &workingDir, const QString &cmdLine,
                                      const QList<EnvironmentModification> &env, CaptureOptions opts)>
      OnCaptureMethod;
  typedef std::function<LiveCapture *(uint32_t PID, const QList<EnvironmentModification> &env,
                                      const QString &name, CaptureOptions opts)>
      OnInjectMethod;

  explicit CaptureDialog(CaptureContext *ctx, OnCaptureMethod captureCallback,
                         OnInjectMethod injectCallback, QWidget *parent = 0);
  ~CaptureDialog();

  bool injectMode() { return m_Inject; }
  void setInjectMode(bool inject);

  void setExecutableFilename(QString filename);
  void loadSettings(QString filename);

  void updateGlobalHook();

private slots:
  // automatic slots
  void on_exePathBrowse_clicked();
  void on_exePath_textChanged(const QString &arg1);
  void on_workDirBrowse_clicked();
  void on_envVarEdit_clicked();

  void on_processFilter_textChanged(const QString &arg1);
  void on_processRefesh_clicked();

  void on_saveSettings_clicked();
  void on_loadSettings_clicked();

  void on_launch_clicked();
  void on_close_clicked();

  void on_toggleGlobal_clicked();

  void on_vulkanLayerWarn_clicked();

  void on_CaptureCallstacks_toggled(bool checked);

private:
  Ui::CaptureDialog *ui;
  CaptureContext *m_Ctx;

  QStandardItemModel *m_ProcessModel;

  OnCaptureMethod m_CaptureCallback;
  OnInjectMethod m_InjectCallback;

  void setEnvironmentModifications(const QList<EnvironmentModification> &modifications);
  void triggerCapture();

  void setSettings(CaptureSettings settings);
  CaptureSettings settings();

  void saveSettings(QString filename);

  QList<EnvironmentModification> m_EnvModifications;
  bool m_Inject;
  void fillProcessList();
};
