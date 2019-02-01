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

#include "AnalyticsPromptDialog.h"
#include <QDesktopServices>
#include <QUrl>
#include "Code/Interface/QRDInterface.h"
#include "ui_AnalyticsPromptDialog.h"

AnalyticsPromptDialog::AnalyticsPromptDialog(PersistantConfig &cfg, QWidget *parent)
    : QDialog(parent), ui(new Ui::AnalyticsPromptDialog), m_Config(cfg)
{
  ui->setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

AnalyticsPromptDialog::~AnalyticsPromptDialog()
{
  delete ui;
}

void AnalyticsPromptDialog::on_autoSubmit_toggled(bool checked)
{
  if(checked)
  {
    m_Config.Analytics_ManualCheck = false;
    m_Config.Analytics_TotalOptOut = false;
  }
}

void AnalyticsPromptDialog::on_manualCheck_toggled(bool checked)
{
  if(checked)
  {
    m_Config.Analytics_ManualCheck = true;
    m_Config.Analytics_TotalOptOut = false;
  }
}

void AnalyticsPromptDialog::on_optOut_toggled(bool checked)
{
  if(checked)
  {
    m_Config.Analytics_ManualCheck = false;
    m_Config.Analytics_TotalOptOut = true;
  }
}

void AnalyticsPromptDialog::on_label_linkActivated(const QString &link)
{
  if(link == lit("#documentreport"))
  {
    Analytics::DocumentReport();
  }
  else
  {
    QDesktopServices::openUrl(QUrl(link));
  }
}
