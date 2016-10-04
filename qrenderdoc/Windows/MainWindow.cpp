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

#include "MainWindow.h"
#include <QFileDialog>
#include <QFileInfo>
#include "Code/CaptureContext.h"
#include "Windows/AboutDialog.h"
#include "EventBrowser.h"
#include "TextureViewer.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(CaptureContext *ctx) : QMainWindow(NULL), ui(new Ui::MainWindow), m_Ctx(ctx)
{
  ui->setupUi(this);

  EventBrowser *eventbrowser = new EventBrowser(ctx);

  ui->toolWindowManager->addToolWindow(eventbrowser, ToolWindowManager::EmptySpace);

  TextureViewer *textureviewer = new TextureViewer(ctx);

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
        new LambdaThread([filename, this]() { m_Ctx->LoadLogfile(filename, false); });
    thread->start();
  }
}

void MainWindow::on_action_About_triggered()
{
  AboutDialog about(this);
  about.exec();
}
