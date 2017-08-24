/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "PerformanceCounterSelection.h"
#include <QSet>
#include "Code/CaptureContext.h"
#include "Code/Interface/QRDInterface.h"
#include "ui_PerformanceCounterSelection.h"

#include <unordered_map>

#define JSON_ID "rdocPerformanceCounterSettings"
#define JSON_VER 1

// specialise this template so we can use QSet<GPUCounter>
template <>
inline uint qHash(const GPUCounter &t, uint seed)
{
  return qHash(uint32_t(t), seed);
}

namespace
{
enum class CounterFamily
{
  Unknown,
  Generic,
  AMD,
  Intel,
  NVIDIA,
};

CounterFamily GetCounterFamily(GPUCounter counter)
{
  if(IsAMDCounter(counter))
  {
    return CounterFamily::AMD;
  }
  else if(IsIntelCounter(counter))
  {
    return CounterFamily::Intel;
  }
  else if(IsNvidiaCounter(counter))
  {
    return CounterFamily::NVIDIA;
  }

  return CounterFamily::Generic;
}

QString ToString(CounterFamily family)
{
  switch(family)
  {
    case CounterFamily::AMD: return lit("AMD");
    case CounterFamily::Generic: return lit("Generic");
    case CounterFamily::Intel: return lit("Intel");
    case CounterFamily::NVIDIA: return lit("NVIDIA");
    case CounterFamily::Unknown: return lit("Unknown");
  }

  return QString();
}
}

const int PerformanceCounterSelection::CounterDescriptionRole = Qt::UserRole + 1;
const int PerformanceCounterSelection::CounterIdRole = Qt::UserRole + 2;

void PerformanceCounterSelection::expandToNode(QTreeWidgetItem *node)
{
  QTreeWidgetItem *n = node;
  while(node != NULL)
  {
    ui->counterTree->expandItem(node);
    node = node->parent();
  }

  if(n)
    ui->counterTree->scrollToItem(n);
}

PerformanceCounterSelection::PerformanceCounterSelection(ICaptureContext &ctx,
                                                         const QList<GPUCounter> &selectedCounters,
                                                         QWidget *parent)
    : QDialog(parent), ui(new Ui::PerformanceCounterSelection), m_Ctx(ctx)
{
  ui->setupUi(this);

  connect(ui->counterTree, &QTreeWidget::currentItemChanged,
          [this](QTreeWidgetItem *item, QTreeWidgetItem *) -> void {
            const QVariant d = item->data(0, CounterDescriptionRole);

            if(d.isValid())
            {
              ui->counterDescription->setText(
                  QString(lit("<b>%1</b><hr>%2")).arg(item->text(0)).arg(d.toString()));
            }
          });

  connect(ui->save, &QPushButton::pressed, this, &PerformanceCounterSelection::Save);
  connect(ui->load, &QPushButton::pressed, this, &PerformanceCounterSelection::Load);
  connect(ui->sampleCounters, &QPushButton::pressed, this, &PerformanceCounterSelection::accept);
  connect(ui->cancel, &QPushButton::pressed, this, &PerformanceCounterSelection::reject);

  connect(ui->counterTree, &QTreeWidget::itemChanged, [this](QTreeWidgetItem *item, int) -> void {
    const QVariant d = item->data(0, CounterIdRole);

    if(d.isValid())
    {
      if(item->checkState(0) == Qt::Checked)
      {
        // Add
        QListWidgetItem *listItem = new QListWidgetItem(ui->enabledCounters);
        listItem->setText(item->text(0));
        listItem->setData(CounterIdRole, d);
        m_SelectedCounters.insert((GPUCounter)d.toUInt(), listItem);
      }
      else
      {
        // Remove
        QListWidgetItem *listItem = m_SelectedCounters.take((GPUCounter)d.toUInt());
        delete listItem;
      }
    }
  });

  ui->counterTree->setMouseTracking(true);

  ctx.Replay().AsyncInvoke([this, selectedCounters](IReplayController *controller) -> void {
    QVector<CounterDescription> counterDescriptions;
    for(const GPUCounter counter : controller->EnumerateCounters())
    {
      counterDescriptions.append(controller->DescribeCounter(counter));
    }

    GUIInvoke::call([counterDescriptions, selectedCounters, this]() -> void {
      SetCounters(counterDescriptions);
      SetSelectedCounters(selectedCounters);
    });
  });
}

PerformanceCounterSelection::~PerformanceCounterSelection()
{
  delete ui;
}

void PerformanceCounterSelection::SetCounters(const QVector<CounterDescription> &descriptions)
{
  ui->counterTree->clear();
  ui->enabledCounters->clear();

  QTreeWidgetItem *currentRoot = NULL;
  CounterFamily currentFamily = CounterFamily::Unknown;

  std::unordered_map<std::string, QTreeWidgetItem *> categories;

  for(const CounterDescription &desc : descriptions)
  {
    m_CounterToUuid[desc.counterID] = desc.uuid;
    m_UuidToCounter[desc.uuid] = desc.counterID;

    const CounterFamily family = GetCounterFamily(desc.counterID);
    if(family != currentFamily)
    {
      currentRoot = new QTreeWidgetItem(ui->counterTree);
      currentRoot->setText(0, ToString(family));

      categories.clear();

      currentFamily = family;
    }

    QTreeWidgetItem *categoryItem = NULL;

    const std::string category = desc.category;
    auto categoryIterator = categories.find(category);

    if(categoryIterator == categories.end())
    {
      QTreeWidgetItem *item = new QTreeWidgetItem(currentRoot);
      item->setText(0, desc.category);
      categories[category] = item;
      categoryItem = item;
    }
    else
    {
      categoryItem = categoryIterator->second;
    }

    QTreeWidgetItem *counterItem = new QTreeWidgetItem(categoryItem);
    counterItem->setText(0, desc.name);
    counterItem->setData(0, CounterDescriptionRole, desc.description);
    counterItem->setData(0, CounterIdRole, (uint32_t)desc.counterID);
    counterItem->setCheckState(0, Qt::Unchecked);

    m_CounterToTreeItem[desc.counterID] = counterItem;
  }
}

QList<GPUCounter> PerformanceCounterSelection::GetSelectedCounters() const
{
  return m_SelectedCounters.keys();
}

void PerformanceCounterSelection::SetSelectedCounters(const QList<GPUCounter> &counters)
{
  // We we walk over the complete tree, and toggle everything so it
  // matches the settings
  QTreeWidgetItemIterator it(ui->counterTree);
  while(*it)
  {
    const QVariant id = (*it)->data(0, Qt::UserRole + 2);
    if(id.isValid())
    {
      const GPUCounter counter = (GPUCounter)id.toUInt();

      (*it)->setCheckState(0, counters.contains(counter) ? Qt::Checked : Qt::Unchecked);
    }

    // The loop above will uncheck all unknown counters, and not crash if some counter is no
    // longer present, or unknown

    ++it;
  }
}

void PerformanceCounterSelection::Save()
{
  QString filename = RDDialog::getSaveFileName(this, tr("Save File"), QDir::homePath(),
                                               tr("Performance Counter Settings (*.json)"));

  if(filename.isEmpty())
    return;

  QVariantList counterIds;
  for(const GPUCounter v : m_SelectedCounters.keys())
  {
    const Uuid uuid = m_CounterToUuid[v];
    QVariantList e;

    for(const byte b : uuid.bytes)
    {
      e.append(b);
    }

    counterIds.append(QVariant(e));
  }

  QVariantMap doc;
  doc[lit("counters")] = counterIds;

  QFile f(filename);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
  {
    SaveToJSON(doc, f, JSON_ID, JSON_VER);
  }
  else
  {
    RDDialog::critical(this, tr("Error saving config"),
                       tr("Couldn't open path %1 for write.").arg(filename));
  }
}

void PerformanceCounterSelection::Load()
{
  QString filename = RDDialog::getOpenFileName(this, tr("Load file"), QDir::homePath(),
                                               tr("Performance Counter Settings (*.json)"));

  if(filename.isEmpty())
    return;

  QVariantMap doc;
  QFile f(filename);
  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    LoadFromJSON(doc, f, JSON_ID, JSON_VER);

    QSet<GPUCounter> selectedCounters;

    QVariantList counters = doc[lit("counters")].toList();

    for(const QVariant &counter : counters)
    {
      QVariantList bytes = counter.toList();
      Uuid uuid;

      /// TODO assert counter.size () == 4

      for(int i = 0; i < 4; ++i)
      {
        uuid.bytes[i] = bytes[i].toUInt();
      }

      if(!m_UuidToCounter.contains(uuid))
        continue;

      selectedCounters.insert(m_UuidToCounter[uuid]);
    }

    SetSelectedCounters(selectedCounters.toList());
  }
  else
  {
    RDDialog::critical(this, tr("Error loading config"),
                       tr("Couldn't open path %1 for reading.").arg(filename));
  }
}

void PerformanceCounterSelection::on_enabledCounters_activated(const QModelIndex &index)
{
  QListWidgetItem *item = ui->enabledCounters->item(index.row());

  if(!item)
    return;

  QVariant counterID = item->data(CounterIdRole);

  // locate the item in the description tree
  auto it = m_CounterToTreeItem.find((GPUCounter)counterID.toUInt());

  if(it != m_CounterToTreeItem.end())
  {
    ui->counterTree->setCurrentItem(it.value());
    it.value()->setSelected(true);

    expandToNode(it.value());
  }
}
