#include "MainWindow.h"
#include "EventBrowser.h"
#include "TextureViewer.h"
#include "ui_MainWindow.h"
#include "renderdoc_replay.h"

ReplayRenderer *renderer = NULL;
QWidget *texviewer = NULL;

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);

  float progress = 0.0f;
  RENDERDOC_CreateReplayRenderer("T:\\renderdoc\\archive_renderdoc_captures\\deferred_plusplus.rdc", &progress, &renderer);

  EventBrowser *eventbrowser = new EventBrowser();

  ui->toolWindowManager->addToolWindow(eventbrowser, ToolWindowManager::EmptySpace);

  TextureViewer *textureviewer = new TextureViewer();

  texviewer = textureviewer->renderSurf();

  ui->toolWindowManager->addToolWindow(textureviewer, ToolWindowManager::AreaReference(ToolWindowManager::RightOf, ui->toolWindowManager->areaOf(eventbrowser)));

  ui->toolWindowManager->setRubberBandLineWidth(50);
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::on_action_Exit_triggered()
{
  this->close();
}
