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

#include "AboutDialog.h"
#include <QApplication>
#include <QLabel>
#include <QString>
#include "Code/QRDUtils.h"
#include "ui_AboutDialog.h"
#include "version.h"

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent), ui(new Ui::AboutDialog)
{
  ui->setupUi(this);

  QString hash = QString::fromLatin1(RENDERDOC_GetCommitHash());

  if(hash[0] == QLatin1Char('N') && hash[1] == QLatin1Char('O'))
  {
    ui->version->setText(
        QFormatStr("Version %1 (built from unknown source)").arg(lit(FULL_VERSION_STRING)));
  }
  else
  {
    ui->version->setText(QFormatStr("Version %1 (built from <a href='%2'>%3</a>)")
                             .arg(lit(FULL_VERSION_STRING))
                             .arg(lit("https://github.com/baldurk/renderdoc/commit/%1").arg(hash))
                             .arg(hash.left(8)));
  }

#if defined(DISTRIBUTION_VERSION)
  ui->owner->setText(QFormatStr("Baldur Karlsson - Packaged for %1").arg(lit(DISTRIBUTION_NAME)));
  ui->contact->setText(QFormatStr("<a href='%1'>%1</a>").arg(lit(DISTRIBUTION_CONTACT)));
#endif

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

AboutDialog::~AboutDialog()
{
  delete ui;
}
