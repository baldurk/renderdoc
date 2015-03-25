#include "MainWindow.h"
#include "EventBrowser.h"
#include "TextureViewer.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);

  EventBrowser *eventbrowser = new EventBrowser();

  ui->toolWindowManager->addToolWindow(eventbrowser, ToolWindowManager::EmptySpace);

  TextureViewer *textureviewer = new TextureViewer();

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
