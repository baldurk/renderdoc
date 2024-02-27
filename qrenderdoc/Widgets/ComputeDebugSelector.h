/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class ComputeDebugSelector;
}

class ComputeDebugSelector : public QDialog
{
  Q_OBJECT

public:
  explicit ComputeDebugSelector(QWidget *parent = 0);
  ~ComputeDebugSelector();

  void SetThreadBounds(const rdcfixedarray<uint32_t, 3> &group,
                       const rdcfixedarray<uint32_t, 3> &thread);

public slots:

signals:
  void beginDebug(const rdcfixedarray<uint32_t, 3> &group, const rdcfixedarray<uint32_t, 3> &thread);

private slots:
  // automatic slots
  void on_groupX_valueChanged(int i) { SyncGroupThreadValue(); }
  void on_groupY_valueChanged(int i) { SyncGroupThreadValue(); }
  void on_groupZ_valueChanged(int i) { SyncGroupThreadValue(); }
  void on_threadX_valueChanged(int i) { SyncGroupThreadValue(); }
  void on_threadY_valueChanged(int i) { SyncGroupThreadValue(); }
  void on_threadZ_valueChanged(int i) { SyncGroupThreadValue(); }
  void on_dispatchX_valueChanged(int i) { SyncDispatchThreadValue(); }
  void on_dispatchY_valueChanged(int i) { SyncDispatchThreadValue(); }
  void on_dispatchZ_valueChanged(int i) { SyncDispatchThreadValue(); }
  void on_beginDebug_clicked();
  void on_cancelDebug_clicked();

private:
  void SyncGroupThreadValue();
  void SyncDispatchThreadValue();

  Ui::ComputeDebugSelector *ui;
  rdcfixedarray<uint32_t, 3> m_threadGroupSize;
};
