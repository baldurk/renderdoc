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
#include <QJsonDocument>
#include "Code/CaptureContext.h"
#include "Windows/AboutDialog.h"
#include "EventBrowser.h"
#include "TextureViewer.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(CaptureContext *ctx) : QMainWindow(NULL), ui(new Ui::MainWindow), m_Ctx(ctx)
{
  ui->setupUi(this);

  QObject::connect(ui->action_Load_Default_Layout, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_1, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_2, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_3, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_4, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_5, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_6, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);

  QObject::connect(ui->action_Save_Default_Layout, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_1, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_2, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_3, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_4, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_5, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_6, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);

  ui->toolWindowManager->setRubberBandLineWidth(50);
  ui->toolWindowManager->setToolWindowCreateCallback([this](const QString &objectName) -> QWidget * {
    if(objectName == "textureViewer")
    {
      TextureViewer *textureViewer = new TextureViewer(m_Ctx);
      textureViewer->setObjectName("textureViewer");
      return textureViewer;
    }
    else if(objectName == "eventBrowser")
    {
      EventBrowser *eventBrowser = new EventBrowser(m_Ctx);
      eventBrowser->setObjectName("eventBrowser");
      return eventBrowser;
    }

    return NULL;
  });

  bool loaded = LoadLayout(0);

  // create default layout if layout failed to load
  if(!loaded)
  {
    EventBrowser *eventBrowser = new EventBrowser(m_Ctx);
    eventBrowser->setObjectName("eventBrowser");

    ui->toolWindowManager->addToolWindow(eventBrowser, ToolWindowManager::EmptySpace);

    TextureViewer *textureViewer = new TextureViewer(m_Ctx);
    textureViewer->setObjectName("textureViewer");

    ui->toolWindowManager->addToolWindow(
        textureViewer,
        ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                         ui->toolWindowManager->areaOf(eventBrowser), 0.75f));
  }
}

MainWindow::~MainWindow()
{
  delete ui;
}

QString MainWindow::GetLayoutPath(int layout)
{
  QString filename = "DefaultLayout.config";

  if(layout > 0)
    filename = QString("Layout%1.config").arg(layout);

  return m_Ctx->ConfigFile(filename);
}

void MainWindow::on_action_Exit_triggered()
{
  this->close();
}

void MainWindow::on_action_Open_Log_triggered()
{
  QString filename =
      RDDialog::getOpenFileName(this, "Select Logfile to open", "",
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
  RDDialog::show(&about);
}

void MainWindow::on_action_Mesh_Output_triggered()
{
}

void MainWindow::on_action_Event_Viewer_triggered()
{
  EventBrowser *eventBrowser = new EventBrowser(m_Ctx);
  eventBrowser->setObjectName("eventBrowser");

  ui->toolWindowManager->addToolWindow(eventBrowser, ToolWindowManager::EmptySpace);
}

void MainWindow::on_action_Texture_Viewer_triggered()
{
  TextureViewer *textureViewer = new TextureViewer(m_Ctx);
  textureViewer->setObjectName("textureViewer");

  ui->toolWindowManager->addToolWindow(textureViewer, ToolWindowManager::EmptySpace);
}

void MainWindow::saveLayout_triggered()
{
  LoadSaveLayout(qobject_cast<QAction *>(QObject::sender()), true);
}

void MainWindow::loadLayout_triggered()
{
  LoadSaveLayout(qobject_cast<QAction *>(QObject::sender()), false);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  SaveLayout(0);
}

void MainWindow::LoadSaveLayout(QAction *action, bool save)
{
  if(action == NULL)
  {
    qWarning() << "NULL action passed to LoadSaveLayout - bad signal?";
    return;
  }

  bool success = false;

  if(action == ui->action_Save_Default_Layout)
  {
    success = SaveLayout(0);
  }
  else if(action == ui->action_Load_Default_Layout)
  {
    success = LoadLayout(0);
  }
  else
  {
    QString name = action->objectName();
    name.remove(0, name.size() - 1);
    int idx = name.toInt();

    if(idx > 0)
    {
      if(save)
        success = SaveLayout(idx);
      else
        success = LoadLayout(idx);
    }
  }

  if(!success)
  {
    if(save)
      RDDialog::critical(this, "Error saving layout", "Couldn't save layout");
    else
      RDDialog::critical(this, "Error loading layout", "Couldn't load layout");
  }
}

QVariantMap MainWindow::saveState()
{
  QVariantMap state = ui->toolWindowManager->saveState();

  state["mainWindowGeometry"] = saveGeometry().toBase64();

  return state;
}

bool MainWindow::restoreState(QVariantMap &state)
{
  restoreGeometry(QByteArray::fromBase64(state["mainWindowGeometry"].toByteArray()));

  ui->toolWindowManager->restoreState(state);

  return true;
}

bool MainWindow::SaveLayout(int layout)
{
  QString path = GetLayoutPath(layout);

  QVariantMap state = saveState();

  // marker that this is indeed a valid state to load from
  state["renderdocLayoutData"] = 1;

  QFile f(path);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
  {
    QJsonDocument doc = QJsonDocument::fromVariant(state);

    if(doc.isEmpty() || doc.isNull())
    {
      qCritical() << "Failed to convert state data to JSON document";
      return false;
    }

    QByteArray jsontext = doc.toJson(QJsonDocument::Indented);

    qint64 ret = f.write(jsontext);

    if(ret != jsontext.size())
    {
      qCritical() << "Failed to write JSON data to file: " << ret << " " << f.errorString();
      return false;
    }

    return true;
  }

  qWarning() << "Couldn't write to " << path << " " << f.errorString();

  return false;
}

bool MainWindow::LoadLayout(int layout)
{
  QString path = GetLayoutPath(layout);

  QFile f(path);
  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QByteArray json = f.readAll();

    if(json.isEmpty())
    {
      qCritical() << "Read invalid empty JSON data from file " << f.errorString();
      return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(json);

    if(doc.isEmpty() || doc.isNull())
    {
      qCritical() << "Failed to convert file to JSON document";
      return false;
    }

    QVariantMap state = doc.toVariant().toMap();

    if(state.isEmpty() || !state.contains("renderdocLayoutData"))
    {
      qCritical() << "Converted state data is invalid or unrecognised";
      return false;
    }

    return restoreState(state);
  }

  qInfo() << "Couldn't load layout from " << path << " " << f.errorString();

  return false;
}
