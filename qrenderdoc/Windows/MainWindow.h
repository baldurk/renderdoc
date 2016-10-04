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
  void on_action_Exit_triggered();
  void on_action_About_triggered();
  void on_action_Open_Log_triggered();

private:
  Ui::MainWindow *ui;
  CaptureContext *m_Ctx;
};

#endif    // MAINWINDOW_H
