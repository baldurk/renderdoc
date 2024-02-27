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

#include "PerformanceCounterViewer.h"
#include "Code/QRDUtils.h"
#include "Windows/Dialogs/PerformanceCounterSelection.h"
#include "ui_PerformanceCounterViewer.h"

static const int EIDRole = Qt::UserRole + 1;
static const int SortDataRole = Qt::UserRole + 2;

class PerformanceCounterItemModel : public QAbstractItemModel
{
public:
  PerformanceCounterItemModel(ICaptureContext &ctx, QObject *parent)
      : QAbstractItemModel(parent), m_Ctx(ctx)
  {
    m_TimeUnit = m_Ctx.Config().EventBrowser_TimeUnit;
    m_NumRows = 0;
  }

  bool UpdateDurationColumn()
  {
    if(m_TimeUnit == m_Ctx.Config().EventBrowser_TimeUnit)
      return false;

    m_TimeUnit = m_Ctx.Config().EventBrowser_TimeUnit;
    emit headerDataChanged(Qt::Horizontal, 1, columnCount());

    return true;
  }

  void refresh(const rdcarray<CounterDescription> &counterDescriptions,
               const rdcarray<CounterResult> &results)
  {
    emit beginResetModel();

    m_Descriptions = counterDescriptions;

    QMap<uint32_t, int> eventIdToRow;
    for(const CounterResult &result : results)
    {
      if(eventIdToRow.contains(result.eventId))
        continue;
      eventIdToRow.insert(result.eventId, eventIdToRow.size());
    }
    QMap<GPUCounter, int> counterToCol;
    for(int i = 0; i < counterDescriptions.count(); i++)
    {
      counterToCol[counterDescriptions[i].counter] = i;
    }

    m_NumRows = eventIdToRow.size();
    m_Data.resize(m_NumRows * (m_Descriptions.size() + 1));

    for(int i = 0; i < (int)results.size(); ++i)
    {
      int row = eventIdToRow[results[i].eventId];

      getData(row, 0).u = results[i].eventId;

      int col = counterToCol[results[i].counter];

      const CounterDescription &desc = counterDescriptions[col];

      col++;

      if(desc.resultType == CompType::UInt)
      {
        if(desc.resultByteWidth == 4)
          getData(row, col).u = results[i].value.u32;
        else
          getData(row, col).u = results[i].value.u64;
      }
      else
      {
        if(desc.resultByteWidth == 4)
          getData(row, col).f = results[i].value.f;
        else
          getData(row, col).f = results[i].value.d;
      }
    }

    emit endResetModel();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || row >= rowCount())
      return QModelIndex();

    return createIndex(row, column);
  }

  QModelIndex parent(const QModelIndex &index) const override { return QModelIndex(); }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_NumRows; }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return m_Descriptions.count() + 1;
  }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
      if(section == 0)
        return lit("EID");

      const CounterDescription &cd = m_Descriptions[section - 1];

      QString unit = QString::null;
      switch(cd.unit)
      {
        case CounterUnit::Bytes: unit = lit("bytes"); break;

        case CounterUnit::Cycles: unit = lit("cycles"); break;

        case CounterUnit::Percentage: unit = lit("%"); break;

        case CounterUnit::Seconds: unit = UnitSuffix(m_TimeUnit); break;

        case CounterUnit::Absolute:
        case CounterUnit::Ratio: break;

        case CounterUnit::Hertz: unit = lit("Hz"); break;
        case CounterUnit::Volt: unit = lit("V"); break;
        case CounterUnit::Celsius: unit = lit("°C"); break;
      }

      if(unit.isNull())
        return cd.name;
      else
        return QFormatStr("%1 (%2)").arg(cd.name, unit);
    }

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid())
    {
      int row = index.row();
      int col = index.column();

      if(role == Qt::TextAlignmentRole)
      {
        if(col == 0)
          return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        else
          return QVariant(Qt::AlignRight | Qt::AlignVCenter);
      }

      if((role == Qt::DisplayRole && col == 0) || role == EIDRole)
        return (qulonglong)getData(row, 0).u;

      const CounterDescription &desc = m_Descriptions[qMax(1, col) - 1];

      if(role == SortDataRole)
      {
        if(col == 0 || desc.resultType == CompType::UInt)
          return (qulonglong)getData(row, col).u;
        else
          return getData(row, col).f;
      }

      if(role == Qt::DisplayRole)
      {
        if(col == 0 || desc.resultType == CompType::UInt)
          return (qulonglong)getData(row, col).u;

        double val = getData(row, col).f;

        if(desc.unit == CounterUnit::Seconds)
        {
          if(m_TimeUnit == TimeUnit::Milliseconds)
            val *= 1000.0;
          else if(m_TimeUnit == TimeUnit::Microseconds)
            val *= 1000000.0;
          else if(m_TimeUnit == TimeUnit::Nanoseconds)
            val *= 1000000000.0;
        }

        return Formatter::Format(val);
      }
    }

    return QVariant();
  }

private:
  ICaptureContext &m_Ctx;

  union CounterDataVal
  {
    uint64_t u;
    double f;
  };

  const CounterDataVal &getData(int row, int col) const
  {
    return m_Data[row * (m_Descriptions.size() + 1) + col];
  }

  CounterDataVal &getData(int row, int col)
  {
    return m_Data[row * (m_Descriptions.size() + 1) + col];
  }

  TimeUnit m_TimeUnit;

  rdcarray<CounterDataVal> m_Data;
  rdcarray<CounterDescription> m_Descriptions;
  int m_NumRows;
};

class PerformanceCounterFilterModel : public QSortFilterProxyModel
{
public:
  PerformanceCounterFilterModel(ICaptureContext &ctx, QObject *parent)
      : QSortFilterProxyModel(parent), m_Ctx(ctx)
  {
  }

  void refresh(bool sync)
  {
    if(m_Sync || sync)
    {
      m_Sync = sync;
      invalidateFilter();
    }
  }

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
  {
    if(!m_Sync)
      return true;

    QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);

    return m_Ctx.GetEventBrowser()->IsAPIEventVisible(idx.data(EIDRole).toUInt());
  }

  bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
  {
    return sourceModel()->data(left, SortDataRole) < sourceModel()->data(right, SortDataRole);
  }

private:
  ICaptureContext &m_Ctx;
  bool m_Sync = false;
};

PerformanceCounterViewer::PerformanceCounterViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PerformanceCounterViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  connect(ui->captureCounters, &QToolButton::clicked, this,
          &PerformanceCounterViewer::CaptureCounters);

  ui->captureCounters->setEnabled(m_Ctx.IsCaptureLoaded());
  ui->saveCSV->setEnabled(m_Ctx.IsCaptureLoaded());

  m_ItemModel = new PerformanceCounterItemModel(m_Ctx, this);
  m_FilterModel = new PerformanceCounterFilterModel(m_Ctx, this);

  m_FilterModel->setSourceModel(m_ItemModel);
  ui->counterResults->setModel(m_FilterModel);

  ui->counterResults->horizontalHeader()->setSectionsMovable(true);
  ui->counterResults->horizontalHeader()->setStretchLastSection(false);

  ui->counterResults->setFont(Formatter::PreferredFont());

  ui->counterResults->setSortingEnabled(true);
  ui->counterResults->sortByColumn(0, Qt::AscendingOrder);

  m_Ctx.AddCaptureViewer(this);
}

PerformanceCounterViewer::~PerformanceCounterViewer()
{
  m_Ctx.BuiltinWindowClosed(this);

  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
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

    rdcarray<CounterDescription> counterDescriptions;

    for(int i = 0; i < m_SelectedCounters.size(); ++i)
    {
      counters[i] = (GPUCounter)m_SelectedCounters[i];
      counterDescriptions.push_back(controller->DescribeCounter(counters[i]));
    }

    rdcarray<CounterResult> results = controller->FetchCounters(counters);

    GUIInvoke::call(this, [this, results, counterDescriptions]() {
      m_ItemModel->refresh(counterDescriptions, results);

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

  m_ItemModel->refresh({}, {});
}

void PerformanceCounterViewer::UpdateDurationColumn()
{
  if(m_ItemModel->UpdateDurationColumn())
    ui->counterResults->viewport()->update();
}

void PerformanceCounterViewer::OnCaptureLoaded()
{
  ui->captureCounters->setEnabled(true);
  ui->saveCSV->setEnabled(true);
}

void PerformanceCounterViewer::OnEventChanged(uint32_t eventId)
{
  m_FilterModel->refresh(ui->syncViews->isChecked());
  if(ui->syncViews->isChecked())
  {
    const int numItems = (int)ui->counterResults->model()->rowCount();
    for(int i = 0; i < numItems; ++i)
    {
      QModelIndex index = ui->counterResults->model()->index(i, 0);
      if(index.data(EIDRole).toUInt() == eventId)
      {
        ui->counterResults->setCurrentIndex(index);
        ui->counterResults->scrollTo(index);
        break;
      }
    }
  }
}

void PerformanceCounterViewer::on_counterResults_doubleClicked(const QModelIndex &index)
{
  uint32_t eid = index.data(EIDRole).toUInt();

  m_Ctx.SetEventID({}, eid, eid);
}

void PerformanceCounterViewer::on_syncViews_toggled(bool checked)
{
  OnEventChanged(m_Ctx.CurEvent());
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

        QAbstractItemModel *model = ui->counterResults->model();

        for(int col = 0; col < model->columnCount(); col++)
        {
          ts << model->headerData(col, Qt::Horizontal).toString();

          if(col == model->columnCount() - 1)
            ts << lit("\n");
          else
            ts << lit(",");
        }

        for(int row = 0; row < model->rowCount(); row++)
        {
          for(int col = 0; col < model->columnCount(); col++)
          {
            ts << model->index(row, col).data().toString();

            if(col == model->columnCount() - 1)
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
