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
#include <QString>
#include <QVector>

namespace Ui
{
class TipsDialog;
}

struct ICaptureContext;

class TipsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit TipsDialog(ICaptureContext &Ctx, QWidget *parent = 0);
  ~TipsDialog();

protected:
  struct Tip
  {
    Tip() {}
    Tip(const QString &tt, const QString &tx) : title(tt), tip(tx) {}
    QString title;
    QString tip;
  };

  void initialize();
  void showTip(int i);
  void showRandomTip();

  QVector<Tip> m_tips;
  int m_currentTip;
  ICaptureContext &m_Ctx;

private slots:
  void on_nextButton_clicked();

  void on_closeButton_clicked();

  void on_randomButton_clicked();

private:
  Ui::TipsDialog *ui;
};
