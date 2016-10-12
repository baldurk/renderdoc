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

#include <stdint.h>
#include <QMainWindow>
#include "Code/CaptureContext.h"

namespace Ui
{
class MainWindow;
}

class QLabel;
class QProgressBar;

class MainWindow : public QMainWindow, public ILogViewerForm
{
private:
  Q_OBJECT

public:
  explicit MainWindow(CaptureContext *ctx);
  ~MainWindow();

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnEventSelected(uint32_t eventID);

  void setProgress(float val);
  bool ownTemporaryLog() { return m_OwnTempLog; }
  void LoadFromFilename(const QString &filename);

private slots:
  // automatic slots
  void on_action_Exit_triggered();
  void on_action_About_triggered();
  void on_action_Open_Log_triggered();
  void on_action_Close_Log_triggered();
  void on_action_Mesh_Output_triggered();
  void on_action_Event_Viewer_triggered();
  void on_action_Texture_Viewer_triggered();

  // manual slots
  void saveLayout_triggered();
  void loadLayout_triggered();

private:
  void closeEvent(QCloseEvent *event) override;

  Ui::MainWindow *ui;
  CaptureContext *m_Ctx;

  QLabel *statusIcon;
  QLabel *statusText;
  QProgressBar *statusProgress;

  bool m_OwnTempLog = false;
  bool m_SavedTempLog = false;

  QString m_LastSaveCapturePath = "";

  void SetTitle(const QString &filename);
  void SetTitle();

  void PopulateRecentFiles();
  void PopulateRecentCaptures();

  void recentLog(const QString &filename);
  void recentCapture(const QString &filename);

  bool PromptCloseLog();
  bool PromptSaveLog();
  void LoadLogfile(const QString &filename, bool temporary, bool local);
  QString GetSavePath();
  void CloseLogfile();

  QVariantMap saveState();
  bool restoreState(QVariantMap &state);

  QString GetLayoutPath(int layout);
  void LoadSaveLayout(QAction *action, bool save);
  bool LoadLayout(int layout);
  bool SaveLayout(int layout);
};
