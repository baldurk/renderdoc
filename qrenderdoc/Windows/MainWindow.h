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

#include <stdint.h>
#include <QMainWindow>
#include <QTimer>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/CaptureContext.h"

namespace Ui
{
class MainWindow;
}

class RDLabel;
class QMimeData;
class QProgressBar;
class QToolButton;
class CaptureDialog;
class LiveCapture;

class MainWindow : public QMainWindow, public ILogViewerForm
{
private:
  Q_OBJECT

public:
  explicit MainWindow(CaptureContext &ctx);
  ~MainWindow();

  void OnLogfileLoaded() override;
  void OnLogfileClosed() override;
  void OnSelectedEventChanged(uint32_t eventID) override {}
  void OnEventChanged(uint32_t eventID) override;

  void setProgress(float val);
  void takeLogOwnership() { m_OwnTempLog = true; }
  void LoadFromFilename(const QString &filename);
  void LoadLogfile(const QString &filename, bool temporary, bool local);
  void CloseLogfile();
  QString GetSavePath();

  void OnCaptureTrigger(const QString &exe, const QString &workingDir, const QString &cmdLine,
                        const QList<EnvironmentModification> &env, CaptureOptions opts,
                        std::function<void(LiveCapture *)> callback);
  void OnInjectTrigger(uint32_t PID, const QList<EnvironmentModification> &env, const QString &name,
                       CaptureOptions opts, std::function<void(LiveCapture *)> callback);

  void PopulateRecentFiles();

  void ShowLiveCapture(LiveCapture *live);
  void LiveCaptureClosed(LiveCapture *live);

  void showEventBrowser() { on_action_Event_Browser_triggered(); }
  void showAPIInspector() { on_action_API_Inspector_triggered(); }
  void showMeshPreview() { on_action_Mesh_Output_triggered(); }
  void showTextureViewer() { on_action_Texture_Viewer_triggered(); }
  void showPipelineViewer() { on_action_Pipeline_State_triggered(); }
  void showCaptureDialog() { on_action_Capture_Log_triggered(); }
  void showDebugMessageView() { on_action_Errors_and_Warnings_triggered(); }
  void showStatisticsViewer() { on_action_Statistics_Viewer_triggered(); }
private slots:
  // automatic slots
  void on_action_Exit_triggered();
  void on_action_About_triggered();
  void on_action_Open_Log_triggered();
  void on_action_Save_Log_triggered();
  void on_action_Close_Log_triggered();
  void on_action_Mesh_Output_triggered();
  void on_action_API_Inspector_triggered();
  void on_action_Event_Browser_triggered();
  void on_action_Texture_Viewer_triggered();
  void on_action_Pipeline_State_triggered();
  void on_action_Capture_Log_triggered();
  void on_action_Errors_and_Warnings_triggered();
  void on_action_Statistics_Viewer_triggered();
  void on_action_Inject_into_Process_triggered();
  void on_action_Resolve_Symbols_triggered();
  void on_action_Attach_to_Running_Instance_triggered();
  void on_action_Manage_Remote_Servers_triggered();
  void on_action_Start_Android_Remote_Server_triggered();
  void on_action_Settings_triggered();
  void on_action_View_Documentation_triggered();
  void on_action_View_Diagnostic_Log_File_triggered();
  void on_action_Source_on_github_triggered();
  void on_action_Build_Release_downloads_triggered();

  // manual slots
  void saveLayout_triggered();
  void loadLayout_triggered();
  void messageCheck();
  void remoteProbe();
  void statusDoubleClicked();
  void switchContext();
  void contextChooser_menuShowing();

private:
  void closeEvent(QCloseEvent *event) override;

  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

  QString dragFilename(const QMimeData *mimeData);

  ToolWindowManager::AreaReference mainToolArea();
  ToolWindowManager::AreaReference leftToolArea();

  Ui::MainWindow *ui;
  CaptureContext &m_Ctx;

  QList<LiveCapture *> m_LiveCaptures;

  RDLabel *statusIcon;
  RDLabel *statusText;
  QProgressBar *statusProgress;
  QMenu *contextChooserMenu;
  QToolButton *contextChooser;

  QTimer m_MessageTick;
  QSemaphore m_RemoteProbeSemaphore;
  LambdaThread *m_RemoteProbe;

  bool m_messageAlternate = false;

  bool m_OwnTempLog = false;
  bool m_SavedTempLog = false;

  QString m_LastSaveCapturePath = "";

  void setLogHasErrors(bool errors);

  void SetTitle(const QString &filename);
  void SetTitle();

  void PopulateRecentCaptures();

  void recentLog(const QString &filename);
  void recentCapture(const QString &filename);

  bool PromptCloseLog();
  bool PromptSaveLog();
  void OpenCaptureConfigFile(const QString &filename, bool exe);

  QVariantMap saveState();
  bool restoreState(QVariantMap &state);

  QString GetLayoutPath(int layout);
  void LoadSaveLayout(QAction *action, bool save);
  bool LoadLayout(int layout);
  bool SaveLayout(int layout);

  void FillRemotesMenu(QMenu *menu, bool includeLocalhost);
};
