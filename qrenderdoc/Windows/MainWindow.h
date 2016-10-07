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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <stdint.h>
#include <QMainWindow>

namespace Ui
{
class MainWindow;
}

class CaptureContext;

class MainWindow : public QMainWindow
{
private:
  Q_OBJECT

public:
  explicit MainWindow(CaptureContext *ctx);
  ~MainWindow();

private slots:
  // automatic slots
  void on_action_Exit_triggered();
  void on_action_About_triggered();
  void on_action_Open_Log_triggered();
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

  QVariantMap saveState();
  bool restoreState(QVariantMap &state);

  QString GetLayoutPath(int layout);
  void LoadSaveLayout(QAction *action, bool save);
  bool LoadLayout(int layout);
  bool SaveLayout(int layout);
};

#endif    // MAINWINDOW_H
