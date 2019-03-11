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

#include <QFrame>
#include <functional>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class CaptureDialog;
}

class QStandardItemModel;
class LiveCapture;
class MainWindow;
class RDLabel;

class CaptureDialog : public QFrame, public ICaptureDialog
{
  Q_OBJECT

public:
  typedef std::function<void(const QString &exe, const QString &workingDir, const QString &cmdLine,
                             const rdcarray<EnvironmentModification> &env, CaptureOptions opts,
                             std::function<void(LiveCapture *)> callback)>
      OnCaptureMethod;
  typedef std::function<void(uint32_t PID, const rdcarray<EnvironmentModification> &env, const QString &name,
                             CaptureOptions opts, std::function<void(LiveCapture *)> callback)>
      OnInjectMethod;

  explicit CaptureDialog(ICaptureContext &ctx, OnCaptureMethod captureCallback,
                         OnInjectMethod injectCallback, MainWindow *main, QWidget *parent = 0);
  ~CaptureDialog();

  // ICaptureDialog
  QWidget *Widget() override { return this; }
  bool IsInjectMode() override { return m_Inject; }
  void SetInjectMode(bool inject) override;

  void SetExecutableFilename(const rdcstr &filename) override;
  void SetWorkingDirectory(const rdcstr &dir) override;
  void SetCommandLine(const rdcstr &cmd) override;
  void SetEnvironmentModifications(const rdcarray<EnvironmentModification> &modifications) override;

  void SetSettings(CaptureSettings settings) override;
  CaptureSettings Settings() override;

  void TriggerCapture() override;

  void LoadSettings(const rdcstr &filename) override;
  void SaveSettings(const rdcstr &filename) override;
  void UpdateGlobalHook() override;
  void UpdateRemoteHost() override;

public slots:
  bool checkAllowClose();

private slots:
  // automatic slots
  void on_exePathBrowse_clicked();
  void on_exePath_textChanged(const QString &arg1);
  void on_workDirBrowse_clicked();
  void on_envVarEdit_clicked();

  void on_processFilter_textChanged(const QString &arg1);
  void on_processRefesh_clicked();

  void on_processList_activated(const QModelIndex &index);

  void on_saveSettings_clicked();
  void on_loadSettings_clicked();
  void on_loadLastCapture_clicked();

  void on_launch_clicked();

  void on_toggleGlobal_clicked();

  void on_CaptureCallstacks_toggled(bool checked);

  // manual slots
  void vulkanLayerWarn_mouseClick();
  void androidWarn_mouseClick();

private:
  Ui::CaptureDialog *ui;
  ICaptureContext &m_Ctx;
  MainWindow *m_Main;

  QStandardItemModel *m_ProcessModel;

  OnCaptureMethod m_CaptureCallback;
  OnInjectMethod m_InjectCallback;

  rdcarray<EnvironmentModification> m_EnvModifications;
  bool m_Inject;
  void fillProcessList();
  void initWarning(RDLabel *label);

  QString mostRecentFilename();

  void PopulateMostRecent();
  CaptureSettings LoadSettingsFromDisk(const rdcstr &filename);

  void CheckAndroidSetup(QString &filename);
  AndroidFlags m_AndroidFlags;
};
