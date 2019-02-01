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

#pragma once

#include <QDialog>
#include <QVariantMap>

namespace Ui
{
class CrashDialog;
}

class PersistantConfig;
class QNetworkAccessManager;
class QNetworkReply;
class QElapsedTimer;

struct Thumbnail;

class CrashDialog : public QDialog
{
  Q_OBJECT
public:
  explicit CrashDialog(PersistantConfig &cfg, QVariantMap crashReportJSON, QWidget *parent = 0);
  ~CrashDialog();

private slots:
  // automatic slots
  void on_send_clicked();
  void on_cancel_clicked();
  void on_uploadCancel_clicked();
  void on_uploadRetry_clicked();
  void on_buttonBox_accepted();
  void on_captureFilename_linkActivated(const QString &link);

private:
  void showEvent(QShowEvent *) override;
  void resizeEvent(QResizeEvent *) override;

  enum class ReportStage
  {
    FillingDetails,
    Uploading,
    Reported,
  };

  void recentre();
  void setStage(ReportStage stage);
  void sendReport();

  Ui::CrashDialog *ui;

  ReportStage m_Stage;
  QString m_CaptureFilename;
  QString m_ReportPath;
  QString m_ReportID;
  QVariantMap m_ReportMetadata;

  QElapsedTimer *m_UploadTimer = NULL;

  QNetworkAccessManager *m_NetManager;
  QNetworkReply *m_Request = NULL;

  Thumbnail *m_Thumbnail = NULL;

  PersistantConfig &m_Config;
};
