#include "MainWindow.h"
#include <QFileDialog>
#include <QFileInfo>
#include "Code/Core.h"
#include "Windows/AboutDialog.h"
#include "EventBrowser.h"
#include "TextureViewer.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(Core *core) : QMainWindow(NULL), ui(new Ui::MainWindow), m_Core(core)
{
  ui->setupUi(this);

  EventBrowser *eventbrowser = new EventBrowser(core);

  ui->toolWindowManager->addToolWindow(eventbrowser, ToolWindowManager::EmptySpace);

  TextureViewer *textureviewer = new TextureViewer(core);

  ui->toolWindowManager->addToolWindow(
      textureviewer,
      ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                       ui->toolWindowManager->areaOf(eventbrowser), 0.75f));

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

void MainWindow::on_action_Open_Log_triggered()
{
  QString filename =
      QFileDialog::getOpenFileName(this, "Select Logfile to open", "",
                                   "Log Files (*.rdc);;Image Files (*.dds *.hdr *.exr *.bmp *.jpg "
                                   "*.jpeg *.png *.tga *.gif *.psd;;All Files (*.*)");

  QFileInfo checkFile(filename);
  if(filename != "" && checkFile.exists() && checkFile.isFile())
  {
    LambdaThread *thread =
        new LambdaThread([filename, this]() { m_Core->LoadLogfile(filename, false); });
    thread->start();
  }
}

void MainWindow::on_action_About_triggered()
{
  AboutDialog about(this);
  about.exec();
}
