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

#include "PerformanceCounterViewer.h"
#include "Windows/Dialogs/PerformanceCounterSelection.h"
#include "ui_PerformanceCounterViewer.h"

static QString FormatCounterResult(const CounterResult &result, const CounterDescription &description)
{
  QString returnValue;

  switch(description.resultType)
  {
    case CompType::Float: returnValue += QString::number(result.value.f); break;

    case CompType::Double: returnValue += QString::number(result.value.d); break;

    case CompType::UInt:
      if(description.resultByteWidth == 8)
      {
        returnValue += QString::number(result.value.u64);
      }
      else
      {
        returnValue += QString::number(result.value.u32);
      }

    default:
      // assert (false)
      break;
  }

  switch(description.unit)
  {
    case CounterUnit::Bytes: returnValue += lit(" bytes"); break;

    case CounterUnit::Cycles: returnValue += lit(" cycles"); break;

    case CounterUnit::Percentage: returnValue += lit(" %"); break;

    case CounterUnit::Seconds: returnValue += lit(" s"); break;

    case CounterUnit::Absolute:
    case CounterUnit::Ratio: break;
  }

  return returnValue;
}

PerformanceCounterViewer::PerformanceCounterViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PerformanceCounterViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_Ctx.AddLogViewer(this);

  connect(ui->captureCounters, &QToolButton::pressed, this,
          &PerformanceCounterViewer::CaptureCounters);
}

void PerformanceCounterViewer::CaptureCounters()
{
  PerformanceCounterSelection pcs(m_Ctx, this);
  if(RDDialog::show(&pcs) != QDialog::Accepted)
    return;
  const QList<GPUCounter> selectedCounters = pcs.GetSelectedCounters();

  bool done = false;
  m_Ctx.Replay().AsyncInvoke([this, selectedCounters, &done](IReplayController *controller) -> void {
    rdctype::array<GPUCounter> counters;
    counters.create(selectedCounters.size());

    QMap<GPUCounter, CounterDescription> counterDescriptions;

    for(int i = 0; i < selectedCounters.size(); ++i)
    {
      counters[i] = (GPUCounter)selectedCounters[i];
      counterDescriptions.insert(counters[i], controller->DescribeCounter(counters[i]));
    }

    QMap<GPUCounter, int> counterIndex;
    for(int i = 0; i < selectedCounters.size(); ++i)
    {
      counterIndex.insert((GPUCounter)selectedCounters[i], i);
    }

    const rdctype::array<CounterResult> results = controller->FetchCounters(counters);

    GUIInvoke::call([this, results, counterDescriptions, counterIndex]() -> void {
      ui->counterResults->clear();

      QStringList headers;
      headers << lit("EID");
      for(const CounterDescription &cd : counterDescriptions)
      {
        headers << cd.name;
      }

      QMap<uint32_t, int> eventIdToRow;
      for(const CounterResult &result : results)
      {
        if(eventIdToRow.contains(result.eventID))
          continue;
        eventIdToRow[result.eventID] = eventIdToRow.size();
      }

      ui->counterResults->setColumnCount(headers.size());
      ui->counterResults->setHorizontalHeaderLabels(headers);
      ui->counterResults->setRowCount(eventIdToRow.size());

      for(int i = 0; i < (int)results.size(); ++i)
      {
        int row = eventIdToRow[results[i].eventID];
        ui->counterResults->setItem(row, 0,
                                    new QTableWidgetItem(QString::number(results[i].eventID)));
        ui->counterResults->setItem(row, counterIndex[results[i].counterID] + 1,
                                    new QTableWidgetItem(FormatCounterResult(
                                        results[i], counterDescriptions[results[i].counterID])));
      }
    });

    done = true;
  });

  ShowProgressDialog(this, tr("Capturing counters"), [&done]() -> bool { return done; });
}

PerformanceCounterViewer::~PerformanceCounterViewer()
{
  m_Ctx.BuiltinWindowClosed(this);

  m_Ctx.RemoveLogViewer(this);
  delete ui;
}

void PerformanceCounterViewer::OnLogfileClosed()
{
}

void PerformanceCounterViewer::OnLogfileLoaded()
{
}