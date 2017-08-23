/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include <QDialog>
#include <QMap>

#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class PerformanceCounterSelection;
}

struct ICaptureContext;
class QListWidgetItem;

class PerformanceCounterSelection : public QDialog
{
  Q_OBJECT

public:
  explicit PerformanceCounterSelection(ICaptureContext &ctx, QWidget *parent = 0);
  ~PerformanceCounterSelection();

  QList<GPUCounter> GetSelectedCounters() const;

public Q_SLOTS:
  void Save();
  void Load();

private:
  void SetCounters(const QVector<CounterDescription> &descriptions);

  Ui::PerformanceCounterSelection *ui;

  ICaptureContext &m_Ctx;
  QMap<GPUCounter, QListWidgetItem *> m_SelectedCounters;
  QMap<GPUCounter, Uuid> m_CounterToUuid;
  QMap<Uuid, GPUCounter> m_UuidToCounter;

  static const int CounterDescriptionRole;
  static const int CounterIdRole;
};
