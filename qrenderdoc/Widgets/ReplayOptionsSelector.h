/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <QWidget>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class ReplayOptionsSelector;
}

class ReplayOptionsSelector : public QWidget
{
  Q_OBJECT

public:
  explicit ReplayOptionsSelector(ICaptureContext &ctx, bool actions, QWidget *parent = 0);
  ~ReplayOptionsSelector();

  QString filename();
  ReplayOptions options();

signals:
  void canceled();
  void opened();

private slots:
  // automatic slots
  void on_saveDefaults_clicked();
  void on_captureFileBrowse_clicked();

private:
  void keyPressEvent(QKeyEvent *e);
  Ui::ReplayOptionsSelector *ui;

  ICaptureContext &m_Ctx;
  rdcarray<GPUDevice> m_GPUs;
};
