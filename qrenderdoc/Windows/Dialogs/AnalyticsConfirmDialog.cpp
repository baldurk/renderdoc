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

#include "AnalyticsConfirmDialog.h"
#include <QDesktopServices>
#include <QFontDatabase>
#include <QPushButton>
#include <QUrl>
#include "Code/Interface/QRDInterface.h"
#include "ui_AnalyticsConfirmDialog.h"

AnalyticsConfirmDialog::AnalyticsConfirmDialog(QString report, QWidget *parent)
    : QDialog(parent), ui(new Ui::AnalyticsConfirmDialog)
{
  ui->setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ui->analyticsReport->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->analyticsReport->setText(report);

  QObject::connect(ui->buttonBox->button(QDialogButtonBox::Discard), &QPushButton::clicked, this,
                   &AnalyticsConfirmDialog::reject);
}

AnalyticsConfirmDialog::~AnalyticsConfirmDialog()
{
  delete ui;
}

void AnalyticsConfirmDialog::on_label_linkActivated(const QString &link)
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
