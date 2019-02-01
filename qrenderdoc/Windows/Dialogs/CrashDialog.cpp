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

#include "CrashDialog.h"
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslSocket>
#include <QString>
#include <QUrlQuery>
#include "Code/QRDUtils.h"
#include "ui_CrashDialog.h"

CrashDialog::CrashDialog(PersistantConfig &cfg, QVariantMap crashReportJSON, QWidget *parent)
    : QDialog(parent), ui(new Ui::CrashDialog), m_Config(cfg)
{
  ui->setupUi(this);

  m_NetManager = new QNetworkAccessManager(this);

  m_ReportPath = crashReportJSON[lit("report")].toString();
  m_ReportMetadata = crashReportJSON;

  bool replayCrash = crashReportJSON[lit("replaycrash")].toUInt() != 0;

  // remove metadata we don't send directly
  m_ReportMetadata.remove(lit("report"));
  m_ReportMetadata.remove(lit("replaycrash"));

  setStage(ReportStage::FillingDetails);

  m_CaptureFilename = m_Config.CrashReport_LastOpenedCapture;

  ui->rememberEmail->setChecked(m_Config.CrashReport_ShouldRememberEmail);
  ui->email->setText(m_Config.CrashReport_EmailAddress);

  QFileInfo capInfo(m_CaptureFilename);

  if(replayCrash && capInfo.exists())
  {
    // if we have a previous capture, fill out the capture group
    ui->captureFilename->setTextFormat(Qt::RichText);
    ui->captureFilename->setText(lit("<a href=\"file://%1\">%2</a>")
                                     .arg(QUrl::fromLocalFile(capInfo.absoluteFilePath()).toString())
                                     .arg(capInfo.fileName()));

    // hide the preview until we have a successful thumbnail
    ui->capturePreviewFrame->hide();

    ICaptureFile *cap = RENDERDOC_OpenCaptureFile();

    ReplayStatus status = cap->OpenFile(capInfo.absoluteFilePath().toUtf8().data(), "", NULL);

    if(status == ReplayStatus::Succeeded)
    {
      Thumbnail thumb = cap->GetThumbnail(FileType::Raw, 320);
      QImage i = QImage(thumb.data.data(), (int)thumb.width, (int)thumb.height, QImage::Format_RGB888)
                     .copy(0, 0, (int)thumb.width, (int)thumb.height);
      if(!i.isNull())
      {
        ui->capturePreview->setPixmap(QPixmap::fromImage(i));
        ui->capturePreview->setPreserveAspectRatio(true);
        ui->capturePreviewFrame->show();

        m_Thumbnail = new Thumbnail(cap->GetThumbnail(FileType::JPG, 0));
      }
    }

    cap->Shutdown();

    if(capInfo.size() > 2250LL * 1024LL * 1024LL)
    {
      // capture is too large to upload :(
      ui->captureFilename->setText(
          tr("%1 is too large for upload (%2 MB).").arg(capInfo.fileName()).arg(capInfo.size() >> 20));
      ui->captureUpload->setChecked(false);
      ui->captureUpload->setEnabled(false);

      ui->capturePreviewFrame->hide();
    }
  }
  else
  {
    m_CaptureFilename = QString();

    // otherwise hide it entirely - this is probably a crash in the injected application or
    // something along those lines where a capture isn't directly associated.
    ui->captureLabel->hide();
    ui->captureUpload->hide();
    ui->captureFilename->hide();
    ui->capturePreviewFrame->hide();
  }

  QString text =
      tr("<p>RenderDoc encountered a serious problem. Please take a moment to look over this "
         "form and send it off so that RenderDoc can get better!</p>");

  if(m_Config.CheckUpdate_UpdateAvailable)
  {
    text +=
        tr("<p><b><a href=\"https://renderdoc.org/builds\">An updated version of RenderDoc</a> is "
           "available</b>. This bug may be fixed in a newer version, it's advised that you "
           "update to see if the bug is fixed.</p>");
  }

  text += tr("<p>The contents of the report can be found <a href=\"%1\">in this zip</a> which "
             "you can edit/censor if you wish.</p>")
              .arg(QUrl::fromLocalFile(m_ReportPath).toString());

  text += tr("<p>More information about <a href=\"" BUGREPORT_URL
             "\">the bug "
             "reporter</a> and <a href=\"" BUGREPORT_URL
             "/privacy\">privacy statement</a> "
             "for submissions.");

  if(!QSslSocket::supportsSsl())
  {
    ui->send->setEnabled(false);
    ui->description->setEnabled(false);
    ui->captureUpload->setEnabled(false);
    ui->rememberEmail->setEnabled(false);
    ui->email->setEnabled(false);

    text = tr(
        "<p>RenderDoc encountered a serious problem. "
        "Unfortunately something went wrong while initialising the bug reporter as Qt was unable "
        "to load SSL support at runtime.</p>");

    text +=
        tr("<p>Due to legal reasons only official builds can be distributed with the OpenSSL "
           "libraries needed for SSL support. "
           "If you are building locally, check that ");

#if defined(Q_OS_WIN32)
    text += tr("you have libeay32.dll and ssleay32.dll available next to qrenderdoc.exe.");
#else
    text += tr("you have the runtime libopenssl library >= 1.0.0 available in your system.");
#endif

    text += lit("</p>");

    text += tr("<p>There is no non-secure bug reporting system available so unfortunately we can't "
               "proceed. If you'd like to send in the capture directly you can "
               "<a href=\"mailto:baldurk@baldurk.org\">email it to me</a> attaching "
               "<a href=\"%1\">this report</a> ")
                .arg(QUrl::fromLocalFile(m_ReportPath).toString());

    if(ui->captureFilename->isVisible())
      text += tr(" and if you'd like, the capture linked below.");

    text += lit("</p>");
  }

  ui->reportText->setTextFormat(Qt::RichText);
  ui->reportText->setText(text);

  setWindowFlags((windowFlags() | Qt::MSWindowsFixedSizeDialogHint) &
                 ~Qt::WindowContextHelpButtonHint);

  adjustSize();
}

CrashDialog::~CrashDialog()
{
  delete m_UploadTimer;
  delete m_Thumbnail;

  delete ui;
}

void CrashDialog::showEvent(QShowEvent *)
{
  adjustSize();
  recentre();
}

void CrashDialog::resizeEvent(QResizeEvent *)
{
  recentre();
}
void CrashDialog::recentre()
{
  QRect scr = QApplication::desktop()->screenGeometry();
  move(scr.center() - rect().center());

  // when we're first shown, on this stage, move the cursor
  if(m_Stage == ReportStage::FillingDetails)
    QCursor::setPos(geometry().center());
}

void CrashDialog::setStage(ReportStage stage)
{
  m_Stage = stage;

  switch(stage)
  {
    case ReportStage::FillingDetails:
      ui->reportGroup->show();
      ui->uploadingGroup->hide();
      ui->reportedGroup->hide();
      break;
    case ReportStage::Uploading:
      ui->reportGroup->hide();
      ui->uploadingGroup->show();
      ui->reportedGroup->hide();
      break;
    case ReportStage::Reported:
      ui->reportGroup->hide();
      ui->uploadingGroup->hide();
      ui->reportedGroup->show();
      break;
  }

  adjustSize();
}

void CrashDialog::on_send_clicked()
{
  // confirm if the user REALLY wants to upload their capture
  if(ui->captureUpload->isChecked())
  {
    QMessageBox::StandardButton result = RDDialog::question(
        this, tr("Are you sure?"), tr("Uploading your capture file will send it privately to the "
                                      "RenderDoc server where I can "
                                      "use it to reproduce your problem.\n\nAre you sure you are "
                                      "OK with sending the capture "
                                      "securely to RenderDoc's website?"));

    if(result != QMessageBox::Yes)
    {
      // uncheck and return back so they can confirm
      ui->captureUpload->setChecked(false);
      return;
    }
  }

  // if we haven't nagged the user before about entering their email address, do so now.
  if(!m_Config.CrashReport_EmailNagged && ui->email->text().isEmpty())
  {
    // don't prompt about this again
    m_Config.CrashReport_EmailNagged = true;
    m_Config.Save();

    QMessageBox::StandardButton result =
        RDDialog::question(this, tr("Please consider leaving your email"),
                           tr("Most bug reports without an email address for contact can't be "
                              "resolved. Would you like to enter your email address?\n\n"
                              "You won't be asked about this again."));

    if(result == QMessageBox::Yes)
    {
      // focus the email field and return so the user can enter something
      ui->email->setFocus(Qt::OtherFocusReason);
      return;
    }
  }

  // save the email configuration for next time so the user can click-through.
  m_Config.CrashReport_ShouldRememberEmail = ui->rememberEmail->isChecked();
  if(ui->rememberEmail->isChecked() && !ui->email->text().isEmpty())
    m_Config.CrashReport_EmailAddress = ui->email->text();
  m_Config.Save();

  sendReport();

  setStage(ReportStage::Uploading);
}

void CrashDialog::sendReport()
{
  delete m_Request;
  m_Request = NULL;

  QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

  // from the QHttpMultiPart example
  for(QString key : m_ReportMetadata.keys())
  {
    QHttpPart param;
    param.setHeader(QNetworkRequest::ContentDispositionHeader,
                    lit("form-data; name=\"%1\"").arg(key));
    param.setBody(m_ReportMetadata[key].toString().toUtf8());

    multiPart->append(param);
  }

  QString email = ui->email->text();
  QString description = ui->description->toPlainText();

  if(!email.isEmpty())
  {
    QHttpPart param;
    param.setHeader(QNetworkRequest::ContentDispositionHeader, lit("form-data; name=\"email\""));
    param.setBody(email.toUtf8());

    multiPart->append(param);
  }

  if(!description.isEmpty())
  {
    QHttpPart param;
    param.setHeader(QNetworkRequest::ContentDispositionHeader,
                    lit("form-data; name=\"description\""));
    param.setBody(description.toUtf8());

    multiPart->append(param);
  }

  if(!m_CaptureFilename.isEmpty() && ui->captureUpload->isChecked())
  {
    {
      QHttpPart capture;

      QFile *file = new QFile(m_CaptureFilename);
      if(file->open(QIODevice::ReadOnly))
      {
        file->setParent(multiPart);

        capture.setHeader(QNetworkRequest::ContentTypeHeader,
                          lit("application/x-renderdoc-capture"));
        capture.setHeader(QNetworkRequest::ContentDispositionHeader,
                          lit("form-data; name=\"capture\"; filename=\"capture.rdc\""));
        capture.setBodyDevice(file);

        multiPart->append(capture);
      }
    }

    if(m_Thumbnail)
    {
      QHttpPart capture;

      QByteArray thumb;

      thumb.insert(0, (const char *)m_Thumbnail->data.data(), m_Thumbnail->data.count());

      capture.setHeader(QNetworkRequest::ContentTypeHeader, lit("image/jpeg"));
      capture.setHeader(QNetworkRequest::ContentDispositionHeader,
                        lit("form-data; name=\"thumb\"; filename=\"thumb.jpg\""));
      capture.setBody(thumb);

      multiPart->append(capture);
    }
  }

  {
    QHttpPart report;

    QFile *file = new QFile(m_ReportPath);
    if(file->open(QIODevice::ReadOnly))
    {
      file->setParent(multiPart);

      report.setHeader(QNetworkRequest::ContentTypeHeader, lit("application/zip"));
      report.setHeader(QNetworkRequest::ContentDispositionHeader,
                       lit("form-data; name=\"report\"; filename=\"report.zip\""));
      report.setBodyDevice(file);

      multiPart->append(report);
    }
    else
    {
      ui->progressText->setText(tr("Error preparing crash report"));
      // can't send report without report.zip
      return;
    }
  }

  QNetworkRequest request(QUrl(lit(BUGREPORT_URL)));

  m_Request = m_NetManager->post(request, multiPart);
  multiPart->setParent(m_Request);

  QObject::connect(
      m_Request, OverloadedSlot<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
      [this](QNetworkReply::NetworkError err) {
        ui->progressBar->setValue(0);
        ui->progressText->setText(tr("Network error uploading:\n%1").arg(m_Request->errorString()));
        ui->uploadRetry->setEnabled(true);
      });

  ui->progressBar->setValue(0);
  ui->progressText->setText(tr("Uploading report...\nCalculating time remaining"));

  delete m_UploadTimer;
  m_UploadTimer = new QElapsedTimer();

  m_UploadTimer->start();

  QObject::connect(m_Request, &QNetworkReply::uploadProgress, [this](qint64 sent, qint64 total) {
    UpdateTransferProgress(sent, total, m_UploadTimer, ui->progressBar, ui->progressText,
                           tr("Uploading report..."));
  });

  QObject::connect(m_Request, &QNetworkReply::finished, [this]() {

    // don't do anything if we're finished after an error
    if(ui->uploadRetry->isEnabled())
      return;

    QString text = tr("<p>Your report has been uploaded, thank you for your help!</p>");

    m_ReportID = QString::fromUtf8(m_Request->readAll());

    if(!m_ReportID.isEmpty())
    {
      BugReport bug;
      bug.reportId = m_ReportID;
      QString url = bug.URL();

      text +=
          tr("<p>The unique anonymous URL for your report is <a href=\"%1\">%1</a>.</p>").arg(url);
    }

    ui->finishedText->setTextFormat(Qt::RichText);
    ui->finishedText->setText(text);
    setStage(ReportStage::Reported);
  });
}

void CrashDialog::on_cancel_clicked()
{
  // don't nag the user, just close.
  reject();
}

void CrashDialog::on_uploadCancel_clicked()
{
  // check that it wasn't an accident
  QMessageBox::StandardButton result = RDDialog::question(
      this, tr("Cancel upload?"), tr("Are you sure you want to cancel the bug report upload?"));

  if(result == QMessageBox::Yes)
  {
    // cancel the request in flight
    m_Request->abort();
    delete m_Request;

    // then close the window
    reject();
  }
}

void CrashDialog::on_uploadRetry_clicked()
{
  // restart the request
  sendReport();
  ui->uploadRetry->setEnabled(false);
}

void CrashDialog::on_buttonBox_accepted()
{
  if(!m_ReportID.isEmpty() && ui->checkUpdates->isChecked())
  {
    // add to list of bug reports to check for updates.
    BugReport bug;
    bug.reportId = m_ReportID;
    bug.submitDate = QDateTime::currentDateTimeUtc();
    bug.checkDate = QDateTime::currentDateTimeUtc();
    m_Config.CrashReport_ReportedBugs.push_back(bug);

    if(m_Config.CrashReport_ReportedBugs.count() > 20)
      m_Config.CrashReport_ReportedBugs.erase(0);

    m_Config.Save();
  }

  accept();
}

void CrashDialog::on_captureFilename_linkActivated(const QString &link)
{
  if(QFileInfo::exists(m_CaptureFilename))
    RevealFilenameInExternalFileBrowser(m_CaptureFilename);
}
