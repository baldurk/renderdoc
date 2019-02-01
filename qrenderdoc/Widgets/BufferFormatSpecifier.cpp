/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

#include "BufferFormatSpecifier.h"
#include <QFontDatabase>
#include "Code/QRDUtils.h"
#include "ui_BufferFormatSpecifier.h"

BufferFormatSpecifier::BufferFormatSpecifier(QWidget *parent)
    : QWidget(parent), ui(new Ui::BufferFormatSpecifier)
{
  ui->setupUi(this);

  QObject::connect(ui->toggleHelp, &QPushButton::clicked, this, &BufferFormatSpecifier::toggleHelp);

  setErrors(QString());

  ui->formatText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

BufferFormatSpecifier::~BufferFormatSpecifier()
{
  delete ui;
}

void BufferFormatSpecifier::toggleHelp()
{
  ui->helpText->setVisible(!ui->helpText->isVisible());

  if(ui->helpText->isVisible())
    showHelp(true);
  else
    showHelp(false);
}

void BufferFormatSpecifier::setFormat(const QString &format)
{
  ui->formatText->setText(format);
}

void BufferFormatSpecifier::setErrors(const QString &errors)
{
  ui->errors->setText(errors);
  if(errors.isEmpty())
    ui->errors->setVisible(false);
  else
    ui->errors->setVisible(true);
}

void BufferFormatSpecifier::showHelp(bool help)
{
  ui->helpText->setVisible(help);

  if(help)
  {
    ui->gridLayout->removeWidget(ui->formatGroup);
    ui->gridLayout->addWidget(ui->formatGroup, 1, 0, 1, 3);
  }
  else
  {
    ui->gridLayout->removeWidget(ui->formatGroup);
    ui->gridLayout->addWidget(ui->formatGroup, 0, 0, 2, 3);
  }
}

void BufferFormatSpecifier::on_apply_clicked()
{
  setErrors(QString());
  emit processFormat(ui->formatText->toPlainText());
}
