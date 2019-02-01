/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "UpdateDialog.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include "Code/QRDUtils.h"
#include "ui_UpdateDialog.h"
#include "version.h"

UpdateDialog::UpdateDialog(QString updateResponse, QWidget *parent)
    : QDialog(parent), ui(new Ui::UpdateDialog)
{
  ui->setupUi(this);

  ui->updateText->setBackgroundRole(QPalette::Base);
  ui->updateText->setForegroundRole(QPalette::Text);

  m_NetManager = new QNetworkAccessManager(this);

  setWindowFlags((windowFlags() | Qt::MSWindowsFixedSizeDialogHint) &
                 ~Qt::WindowContextHelpButtonHint);

  QStringList lines = updateResponse.split(QLatin1Char('\n'), QString::SkipEmptyParts);

  m_NewVer = lines[0];
  m_URL = lines[1];
  m_Size = lines[2].toUInt();

  QString notes;

  for(int i = 3; i < lines.count(); i++)
    notes += lines[i];

  ui->progressText->setVisible(false);
  ui->progressBar->setVisible(false);

  QString text = tr("Update Available - v%1").arg(m_NewVer);
  ui->updateVer->setText(text);
  setWindowTitle(text);

  ui->updateText->setText(notes);

  ui->currentVersion->setText(lit(FULL_VERSION_STRING));
  ui->newVersion->setText(QFormatStr("v%1").arg(m_NewVer));
  ui->downloadSize->setText(QFormatStr("%1 MB").arg(double(m_Size) / 1000000.0, 0, 'f', 2));

  adjustSize();
}

UpdateDialog::~UpdateDialog()
{
  delete m_DownloadTimer;

  delete ui;
}

void UpdateDialog::keyPressEvent(QKeyEvent *e)
{
  if(e->key() == Qt::Key_Escape)
    return;

  QDialog::keyPressEvent(e);
}

void UpdateDialog::closeEvent(QCloseEvent *e)
{
  if(ui->close->isEnabled())
  {
    QDialog::closeEvent(e);
    return;
  }

  e->ignore();
  return;
}

void UpdateDialog::on_releaseNotes_clicked()
{
  QDesktopServices::openUrl(
      QUrl(lit("https://github.com/baldurk/renderdoc/releases/tag/v%1").arg(m_NewVer)));
}

void UpdateDialog::on_close_clicked()
{
  reject();
}

void UpdateDialog::on_update_clicked()
{
  QMessageBox::StandardButton res = RDDialog::question(
      this, tr("RenderDoc Update"), tr("This will close RenderDoc immediately - if you have any "
                                       "unsaved work, save it first!\n"
                                       "Continue?"),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

  if(res == QMessageBox::Yes)
  {
    QString runningPrograms;
    int running = 0;

    uint32_t nextIdent = 0;

    QString localhost = lit("localhost");

    for(;;)
    {
      // just a sanity check to make sure we don't hit some unexpected case and infinite loop
      uint32_t prevIdent = nextIdent;

      nextIdent = RENDERDOC_EnumerateRemoteTargets("localhost", nextIdent);

      if(nextIdent == 0 || prevIdent >= nextIdent)
        break;

      running++;

      ITargetControl *conn = RENDERDOC_CreateTargetControl("localhost", nextIdent, "updater", false);

      if(conn)
      {
        if(!runningPrograms.isEmpty())
          runningPrograms += lit("\n");

        QString target =
            conn->GetTarget() ? QString::fromUtf8(conn->GetTarget()) : lit("<unknown>");
        if(conn->GetAPI())
          runningPrograms += tr("%1 running %2").arg(target).arg(QString::fromUtf8(conn->GetAPI()));
        else
          runningPrograms += target;

        conn->Shutdown();
      }
    }

    if(running > 0)
    {
      RDDialog::critical(
          this, tr("RenderDoc in use"),
          tr("RenderDoc is currently capturing, cannot update until the program%1 closed:\n\n")
                  .arg(running > 1 ? lit("s are") : lit(" is")) +
              runningPrograms);
      return;
    }

    ui->metadataFrame->setVisible(false);
    ui->progressBar->setVisible(true);
    ui->progressText->setVisible(true);

    ui->progressBar->setMaximum(10000);
    ui->progressBar->setValue(0);
    ui->progressText->setText(tr("Preparing Download"));

    ui->close->setEnabled(false);
    ui->update->setEnabled(false);

    delete m_DownloadTimer;
    m_DownloadTimer = new QElapsedTimer();

    m_DownloadTimer->start();

    QNetworkReply *req = m_NetManager->get(QNetworkRequest(QUrl(m_URL)));

    QObject::connect(req, &QNetworkReply::downloadProgress, [this](qint64 recvd, qint64 total) {
      UpdateTransferProgress(recvd, total, m_DownloadTimer, ui->progressBar, ui->progressText,
                             tr("Downloading update..."));
    });

    QObject::connect(req, OverloadedSlot<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
                     [this, req](QNetworkReply::NetworkError err) {
                       ui->progressBar->setValue(0);
                       ui->progressText->setText(tr("Network error:\n%1").arg(req->errorString()));
                       ui->update->setEnabled(true);
                       ui->close->setEnabled(true);
                       ui->update->setText(tr("Retry Update"));
                     });

    QObject::connect(req, &QNetworkReply::finished, [this, req]() {

      // don't do anything if we're finished after an error
      if(ui->update->isEnabled())
        return;

      QDir dir(QDir::tempPath());

      dir.mkdir(lit("RenderDocUpdate"));
      dir.cd(lit("RenderDocUpdate"));

      QString path = dir.absoluteFilePath(lit("update.zip"));

      {
        QFile file(path);
        if(file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
          file.write(req->readAll());
        }
        else
        {
          RDDialog::critical(this, tr("Error saving file"),
                             tr("Couldn't save update file to: %1").arg(path));
          reject();
        }
      }

      QDir appDir = QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir();

      bool success = true;

      QString dll = lit("renderdoc.dll");
      QString cmd = lit("renderdoccmd.exe");

      QFile::remove(dir.absoluteFilePath(dll));
      QFile::remove(dir.absoluteFilePath(cmd));

      success &= QFile::copy(appDir.absoluteFilePath(dll), dir.absoluteFilePath(dll));
      success &= QFile::copy(appDir.absoluteFilePath(cmd), dir.absoluteFilePath(cmd));

      if(!success)
      {
        RDDialog::critical(this, tr("Error running updated"),
                           tr("Couldn't copy updater files to temporary path"));
        reject();
      }

      QDir::setCurrent(dir.absolutePath());

      success = RunProcessAsAdmin(
          dir.absoluteFilePath(cmd),
          QStringList() << lit("upgrade") << lit("--path") << appDir.absolutePath(), NULL, true);

      exit(0);

    });
  }
}