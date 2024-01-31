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

#include "ComputeDebugSelector.h"
#include "Code/QRDUtils.h"
#include "ui_ComputeDebugSelector.h"

ComputeDebugSelector::ComputeDebugSelector(QWidget *parent)
    : QDialog(parent), ui(new Ui::ComputeDebugSelector)
{
  ui->setupUi(this);

  m_threadGroupSize[0] = m_threadGroupSize[1] = m_threadGroupSize[2] = 1;

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ui->groupX->setFont(Formatter::PreferredFont());
  ui->groupY->setFont(Formatter::PreferredFont());
  ui->groupZ->setFont(Formatter::PreferredFont());
  ui->threadX->setFont(Formatter::PreferredFont());
  ui->threadY->setFont(Formatter::PreferredFont());
  ui->threadZ->setFont(Formatter::PreferredFont());
  ui->dispatchX->setFont(Formatter::PreferredFont());
  ui->dispatchY->setFont(Formatter::PreferredFont());
  ui->dispatchZ->setFont(Formatter::PreferredFont());

  // A threadgroup's size in any dimension can be up to 1024, but a dispatch can be 65535
  // threadgroups for a dimension. Use that upper bound to fix the min size of all fields.
  ui->groupX->setMaximum(65535);
  int sizeHint = ui->groupX->minimumSizeHint().width();
  ui->groupX->setMinimumWidth(sizeHint);
  ui->groupY->setMinimumWidth(sizeHint);
  ui->groupZ->setMinimumWidth(sizeHint);
  ui->threadX->setMinimumWidth(sizeHint);
  ui->threadY->setMinimumWidth(sizeHint);
  ui->threadZ->setMinimumWidth(sizeHint);
  ui->dispatchX->setMinimumWidth(sizeHint);
  ui->dispatchY->setMinimumWidth(sizeHint);
  ui->dispatchZ->setMinimumWidth(sizeHint);
}

ComputeDebugSelector::~ComputeDebugSelector()
{
  delete ui;
}

void ComputeDebugSelector::SetThreadBounds(const rdcfixedarray<uint32_t, 3> &group,
                                           const rdcfixedarray<uint32_t, 3> &thread)
{
  // Set maximums for CS debugging
  ui->groupX->setMaximum(group[0] - 1);
  ui->groupY->setMaximum(group[1] - 1);
  ui->groupZ->setMaximum(group[2] - 1);

  ui->threadX->setMaximum(thread[0] - 1);
  ui->threadY->setMaximum(thread[1] - 1);
  ui->threadZ->setMaximum(thread[2] - 1);

  ui->dispatchX->setMaximum(group[0] * thread[0] - 1);
  ui->dispatchY->setMaximum(group[1] * thread[1] - 1);
  ui->dispatchZ->setMaximum(group[2] * thread[2] - 1);

  m_threadGroupSize = thread;
}

void ComputeDebugSelector::SyncGroupThreadValue()
{
  QSignalBlocker blockers[3] = {QSignalBlocker(ui->dispatchX), QSignalBlocker(ui->dispatchY),
                                QSignalBlocker(ui->dispatchZ)};

  ui->dispatchX->setValue(ui->groupX->value() * m_threadGroupSize[0] + ui->threadX->value());
  ui->dispatchY->setValue(ui->groupY->value() * m_threadGroupSize[1] + ui->threadY->value());
  ui->dispatchZ->setValue(ui->groupZ->value() * m_threadGroupSize[2] + ui->threadZ->value());
}

void ComputeDebugSelector::SyncDispatchThreadValue()
{
  uint32_t group[3] = {ui->dispatchX->value() / m_threadGroupSize[0],
                       ui->dispatchY->value() / m_threadGroupSize[1],
                       ui->dispatchZ->value() / m_threadGroupSize[2]};
  uint32_t thread[3] = {ui->dispatchX->value() % m_threadGroupSize[0],
                        ui->dispatchY->value() % m_threadGroupSize[1],
                        ui->dispatchZ->value() % m_threadGroupSize[2]};

  QSignalBlocker blockers[6] = {QSignalBlocker(ui->groupX),  QSignalBlocker(ui->groupY),
                                QSignalBlocker(ui->groupZ),  QSignalBlocker(ui->threadX),
                                QSignalBlocker(ui->threadY), QSignalBlocker(ui->threadZ)};

  ui->groupX->setValue(group[0]);
  ui->groupY->setValue(group[1]);
  ui->groupZ->setValue(group[2]);
  ui->threadX->setValue(thread[0]);
  ui->threadY->setValue(thread[1]);
  ui->threadZ->setValue(thread[2]);
}

void ComputeDebugSelector::on_beginDebug_clicked()
{
  // The dispatch thread IDs and the group/thread IDs are synced on editing either set, so we can
  // choose either one to begin debugging.
  uint32_t group[3] = {(uint32_t)ui->groupX->value(), (uint32_t)ui->groupY->value(),
                       (uint32_t)ui->groupZ->value()};
  uint32_t thread[3] = {(uint32_t)ui->threadX->value(), (uint32_t)ui->threadY->value(),
                        (uint32_t)ui->threadZ->value()};
  emit beginDebug(group, thread);
  close();
}

void ComputeDebugSelector::on_cancelDebug_clicked()
{
  close();
}
