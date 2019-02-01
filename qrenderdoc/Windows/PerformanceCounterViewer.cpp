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

#include "PerformanceCounterViewer.h"
#include "Code/QRDUtils.h"
#include "Windows/Dialogs/PerformanceCounterSelection.h"
#include "ui_PerformanceCounterViewer.h"

struct SortValue
{
  enum
  {
    Integer,
    Float
  } type;
  union
  {
    uint64_t u;
    double d;
  } val;

  SortValue(uint32_t eventId)
  {
    type = Integer;
    val.u = eventId;
  }

  SortValue(const CounterResult &result, const CounterDescription &description)
  {
    switch(description.resultType)
    {
      case CompType::Float:
        type = Float;
        val.d = result.value.f;
        break;
      case CompType::Double:
        type = Float;
        val.d = result.value.d;
        break;

      case CompType::UInt:
        type = Integer;
        if(description.resultByteWidth == 8)
          val.u = result.value.u64;
        else
          val.u = result.value.u32;
        break;

      default:
        qCritical() << "Unexpected component type" << ToQStr(description.resultType);
        type = Float;
        val.d = -1.0;
        break;
    }
  }
};

struct CustomSortedTableItem : public QTableWidgetItem
{
  explicit CustomSortedTableItem(const QString &text, SortValue v)
      : QTableWidgetItem(text), sortVal(v)
  {
  }
  bool operator<(const QTableWidgetItem &other) const
  {
    const CustomSortedTableItem &customother = (const CustomSortedTableItem &)other;

    if(sortVal.type == SortValue::Integer)
      return sortVal.val.u < customother.sortVal.val.u;
    return sortVal.val.d < customother.sortVal.val.d;
  }

  virtual QVariant data(int role) const
  {
    if(role == Qt::TextAlignmentRole && column() > 0)
      return QVariant(Qt::AlignRight | Qt::AlignCenter);

    return QTableWidgetItem::data(role);
  }
  SortValue sortVal;
};

PerformanceCounterViewer::PerformanceCounterViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PerformanceCounterViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_Ctx.AddCaptureViewer(this);

  connect(ui->captureCounters, &QToolButton::clicked, this,
          &PerformanceCounterViewer::CaptureCounters);

  ui->captureCounters->setEnabled(m_Ctx.IsCaptureLoaded());
  ui->saveCSV->setEnabled(m_Ctx.IsCaptureLoaded());

  ui->counterResults->horizontalHeader()->setSectionsMovable(true);
}

PerformanceCounterViewer::~PerformanceCounterViewer()
{
  m_Ctx.BuiltinWindowClosed(this);

  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

QTableWidgetItem *PerformanceCounterViewer::MakeCounterResultItem(const CounterResult &result,
                                                                  const CounterDescription &description)
{
  QString returnValue;

  double mul = 1.0;

  TimeUnit timeunit = m_Ctx.Config().EventBrowser_TimeUnit;

  if(description.unit == CounterUnit::Seconds)
  {
    if(timeunit == TimeUnit::Milliseconds)
      mul *= 1000.0;
    else if(timeunit == TimeUnit::Microseconds)
      mul *= 1000000.0;
    else if(timeunit == TimeUnit::Nanoseconds)
      mul *= 1000000000.0;
  }

  switch(description.resultType)
  {
    case CompType::Float: returnValue += Formatter::Format(mul * result.value.f); break;

    case CompType::Double: returnValue += Formatter::Format(mul * result.value.d); break;

    case CompType::UInt:
      if(description.resultByteWidth == 8)
      {
        returnValue += Formatter::Format(result.value.u64);
      }
      else
      {
        returnValue += Formatter::Format(result.value.u32);
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

    case CounterUnit::Seconds: returnValue += lit(" ") + UnitSuffix(timeunit); break;

    case CounterUnit::Absolute:
    case CounterUnit::Ratio: break;
  }

  return new CustomSortedTableItem(returnValue, SortValue(result, description));
}

void PerformanceCounterViewer::CaptureCounters()
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  PerformanceCounterSelection pcs(m_Ctx, m_SelectedCounters, this);
  if(RDDialog::show(&pcs) != QDialog::Accepted)
    return;
  m_SelectedCounters = pcs.GetSelectedCounters();

  ANALYTIC_SET(UIFeatures.PerformanceCounters, true);

  bool done = false;
  m_Ctx.Replay().AsyncInvoke([this, &done](IReplayController *controller) -> void {
    rdcarray<GPUCounter> counters;
    counters.resize(m_SelectedCounters.size());

    QMap<GPUCounter, CounterDescription> counterDescriptions;

    for(int i = 0; i < m_SelectedCounters.size(); ++i)
    {
      counters[i] = (GPUCounter)m_SelectedCounters[i];
      counterDescriptions.insert(counters[i], controller->DescribeCounter(counters[i]));
    }

    QMap<GPUCounter, int> counterIndex;
    for(int i = 0; i < m_SelectedCounters.size(); ++i)
    {
      counterIndex.insert((GPUCounter)m_SelectedCounters[i], i);
    }

    const rdcarray<CounterResult> results = controller->FetchCounters(counters);

    GUIInvoke::call(this, [this, results, counterDescriptions, counterIndex]() {
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
        if(eventIdToRow.contains(result.eventId))
          continue;
        eventIdToRow.insert(result.eventId, eventIdToRow.size());
      }

      ui->counterResults->setColumnCount(headers.size());
      ui->counterResults->setHorizontalHeaderLabels(headers);
      ui->counterResults->setRowCount(eventIdToRow.size());

      ui->counterResults->setSortingEnabled(false);

      for(int i = 0; i < (int)results.size(); ++i)
      {
        int row = eventIdToRow[results[i].eventId];

        ui->counterResults->setItem(row, 0,
                                    new CustomSortedTableItem(QString::number(results[i].eventId),
                                                              SortValue(results[i].eventId)));

        ui->counterResults->setItem(
            row, counterIndex[results[i].counter] + 1,
            MakeCounterResultItem(results[i], counterDescriptions[results[i].counter]));

        ui->counterResults->item(row, 0)->setData(Qt::UserRole, results[i].eventId);
      }

      ui->counterResults->setSortingEnabled(true);

      ui->counterResults->resizeColumnsToContents();
    });

    done = true;
  });

  ShowProgressDialog(this, tr("Capturing counters"), [&done]() -> bool { return done; });
}

void PerformanceCounterViewer::OnCaptureClosed()
{
  ui->captureCounters->setEnabled(false);
  ui->saveCSV->setEnabled(false);

  ui->counterResults->clearContents();
  ui->counterResults->setRowCount(0);
  ui->counterResults->setColumnCount(1);
}

void PerformanceCounterViewer::OnCaptureLoaded()
{
  ui->captureCounters->setEnabled(true);
  ui->saveCSV->setEnabled(true);
}

void PerformanceCounterViewer::on_counterResults_doubleClicked(const QModelIndex &index)
{
  QTableWidgetItem *item = ui->counterResults->item(index.row(), 0);

  if(item)
  {
    bool ok = false;
    uint32_t eid = item->data(Qt::UserRole).toUInt(&ok);

    if(ok)
      m_Ctx.SetEventID({}, eid, eid);
  }
}

void PerformanceCounterViewer::on_saveCSV_clicked()
{
  QString filename = RDDialog::getSaveFileName(this, tr("Export counter results as CSV"), QString(),
                                               tr("CSV Files (*.csv)"));

  if(!filename.isEmpty())
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile f(filename, this);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        QTextStream ts(&f);

        for(int col = 0; col < ui->counterResults->columnCount(); col++)
        {
          ts << ui->counterResults->horizontalHeaderItem(col)->text();

          if(col == ui->counterResults->columnCount() - 1)
            ts << lit("\n");
          else
            ts << lit(",");
        }

        for(int row = 0; row < ui->counterResults->rowCount(); row++)
        {
          for(int col = 0; col < ui->counterResults->columnCount(); col++)
          {
            QTableWidgetItem *item = ui->counterResults->item(row, col);

            if(item)
              ts << item->text();
            else
              ts << lit("-");

            if(col == ui->counterResults->columnCount() - 1)
              ts << lit("\n");
            else
              ts << lit(",");
          }
        }

        return;
      }

      RDDialog::critical(
          this, tr("Error exporting counter results"),
          tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f.errorString()));
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to"));
    }
  }
}
