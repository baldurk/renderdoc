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

#include "PerformanceCounterSelection.h"
#include <QMenu>
#include <QSet>
#include "Code/Interface/QRDInterface.h"
#include "Code/Resources.h"
#include "ui_PerformanceCounterSelection.h"

#include <unordered_map>

#define JSON_ID "rdocPerformanceCounterSettings"
#define JSON_VER 1

// we can't specialise the template, but creating an overload works. This lets us use
// QSet<GPUCounter>
inline uint qHash(const GPUCounter &t)
{
  return qHash(uint32_t(t));
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
const int PerformanceCounterSelection::PreviousCheckStateRole = Qt::UserRole + 3;

void PerformanceCounterSelection::uncheckAllChildren(RDTreeWidgetItem *item)
{
  for(int i = 0; i < item->childCount(); i++)
  {
    uncheckAllChildren(item->child(i));

    item->child(i)->setCheckState(0, Qt::Unchecked);
    item->child(i)->setData(0, PreviousCheckStateRole, Qt::Unchecked);
  }
}

void PerformanceCounterSelection::checkAllChildren(RDTreeWidgetItem *item)
{
  for(int i = 0; i < item->childCount(); i++)
  {
    checkAllChildren(item->child(i));

    item->child(i)->setCheckState(0, Qt::Checked);
    item->child(i)->setData(0, PreviousCheckStateRole, Qt::Checked);
  }
}

void PerformanceCounterSelection::updateParentCheckState(RDTreeWidgetItem *item)
{
  if(!item)
    return;

  int numChecked = 0;
  int numPartial = 0;

  for(int i = 0; i < item->childCount(); i++)
  {
    Qt::CheckState state = item->child(i)->checkState(0);
    if(state == Qt::PartiallyChecked)
      numPartial++;
    else if(state == Qt::Checked)
      numChecked++;
  }

  if(numChecked == item->childCount())
    item->setCheckState(0, Qt::Checked);
  else if(numChecked > 0 || numPartial > 0)
    item->setCheckState(0, Qt::PartiallyChecked);
  else
    item->setCheckState(0, Qt::Unchecked);

  item->setData(0, PreviousCheckStateRole, item->checkState(0));

  updateParentCheckState(item->parent());
}

void PerformanceCounterSelection::expandToNode(RDTreeWidgetItem *node)
{
  RDTreeWidgetItem *n = node;
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

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ui->counterTree->setColumns({QString()});
  ui->counterTree->setHeaderHidden(true);

  connect(ui->counterTree, &RDTreeWidget::currentItemChanged,
          [this](RDTreeWidgetItem *item, RDTreeWidgetItem *) -> void {
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

  connect(ui->counterTree, &RDTreeWidget::itemChanged, [this](RDTreeWidgetItem *item, int) -> void {
    const QVariant d = item->data(0, CounterIdRole);

    static bool recurse = false;

    if(d.isValid())
    {
      if(item->checkState(0) == Qt::Checked)
      {
        // Add
        if(!m_SelectedCounters.contains((GPUCounter)d.toUInt()))
        {
          QListWidgetItem *listItem = new QListWidgetItem(ui->enabledCounters);
          listItem->setText(item->text(0));
          listItem->setData(CounterIdRole, d);
          m_SelectedCounters.insert((GPUCounter)d.toUInt(), listItem);
        }
      }
      else
      {
        // Remove
        QListWidgetItem *listItem = m_SelectedCounters.take((GPUCounter)d.toUInt());
        delete listItem;
      }

      if(!recurse)
      {
        recurse = true;
        updateParentCheckState(item->parent());
        recurse = false;
      }
    }
    else if(!recurse)
    {
      Qt::CheckState prev = item->data(0, PreviousCheckStateRole).value<Qt::CheckState>();

      if(item->checkState(0) != prev)
      {
        recurse = true;

        if(item->checkState(0) == Qt::Checked)
        {
          checkAllChildren(item);
        }
        else
        {
          uncheckAllChildren(item);
        }

        item->setData(0, PreviousCheckStateRole, item->checkState(0));

        updateParentCheckState(item);

        recurse = false;
      }
    }
  });

  ui->counterTree->setMouseTracking(true);

  ctx.Replay().AsyncInvoke([this, selectedCounters](IReplayController *controller) {
    QVector<CounterDescription> counterDescriptions;
    for(const GPUCounter counter : controller->EnumerateCounters())
    {
      counterDescriptions.append(controller->DescribeCounter(counter));
    }

    GUIInvoke::call(this, [counterDescriptions, selectedCounters, this]() {
      SetCounters(counterDescriptions);
      SetSelectedCounters(selectedCounters);
    });
  });

  ui->counterTree->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui->counterTree, &RDTreeWidget::customContextMenuRequested, this,
                   &PerformanceCounterSelection::counterTree_contextMenu);
}

PerformanceCounterSelection::~PerformanceCounterSelection()
{
  delete ui;
}

void PerformanceCounterSelection::SetCounters(const QVector<CounterDescription> &descriptions)
{
  ui->counterTree->clear();
  ui->enabledCounters->clear();

  RDTreeWidgetItem *currentRoot = NULL;
  CounterFamily currentFamily = CounterFamily::Unknown;

  std::unordered_map<std::string, RDTreeWidgetItem *> categories;

  for(const CounterDescription &desc : descriptions)
  {
    m_CounterToUuid[desc.counter] = desc.uuid;
    m_UuidToCounter[desc.uuid] = desc.counter;

    const CounterFamily family = GetCounterFamily(desc.counter);
    if(family != currentFamily)
    {
      currentRoot = new RDTreeWidgetItem();
      currentRoot->setText(0, ToString(family));
      currentRoot->setCheckState(0, Qt::Unchecked);
      currentRoot->setData(0, PreviousCheckStateRole, Qt::Unchecked);
      ui->counterTree->addTopLevelItem(currentRoot);

      categories.clear();

      currentFamily = family;
    }

    RDTreeWidgetItem *categoryItem = NULL;

    const std::string category = desc.category;
    auto categoryIterator = categories.find(category);

    if(categoryIterator == categories.end())
    {
      RDTreeWidgetItem *item = new RDTreeWidgetItem();
      item->setText(0, desc.category);
      item->setCheckState(0, Qt::Unchecked);
      item->setData(0, PreviousCheckStateRole, Qt::Unchecked);
      currentRoot->addChild(item);

      categories[category] = item;
      categoryItem = item;
    }
    else
    {
      categoryItem = categoryIterator->second;
    }

    RDTreeWidgetItem *counterItem = new RDTreeWidgetItem();
    counterItem->setText(0, desc.name);
    counterItem->setData(0, CounterDescriptionRole, desc.description);
    counterItem->setData(0, CounterIdRole, (uint32_t)desc.counter);
    counterItem->setCheckState(0, Qt::Unchecked);
    counterItem->setData(0, PreviousCheckStateRole, Qt::Unchecked);
    categoryItem->addChild(counterItem);

    m_CounterToTreeItem[desc.counter] = counterItem;
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
  RDTreeWidgetItemIterator it(ui->counterTree);
  while(*it)
  {
    const QVariant id = (*it)->data(0, CounterIdRole);
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

    for(const uint32_t b : uuid.words)
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
    bool success = LoadFromJSON(doc, f, JSON_ID, JSON_VER);

    if(success)
    {
      QSet<GPUCounter> selectedCounters;

      QVariantList counters = doc[lit("counters")].toList();

      for(const QVariant &counter : counters)
      {
        QVariantList bytes = counter.toList();
        Uuid uuid;

        if(bytes.size() != 4)
        {
          qWarning() << "Counter ID doesn't count 4 words";
          continue;
        }

        for(int i = 0; i < 4; ++i)
        {
          uuid.words[i] = bytes[i].toUInt();
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
                         tr("Couldn't interpret settings in %1.").arg(filename));
    }
  }
  else
  {
    RDDialog::critical(this, tr("Error loading config"),
                       tr("Couldn't open path %1 for reading.").arg(filename));
  }
}
void PerformanceCounterSelection::counterTree_contextMenu(const QPoint &pos)
{
  RDTreeWidgetItem *item = ui->counterTree->itemAt(pos);

  QMenu contextMenu(this);

  QAction expandAll(tr("&Expand All"), this);
  QAction collapseAll(tr("&Collapse All"), this);

  contextMenu.addAction(&expandAll);
  contextMenu.addAction(&collapseAll);

  expandAll.setIcon(Icons::arrow_out());
  collapseAll.setIcon(Icons::arrow_in());

  expandAll.setEnabled(item && item->childCount() > 0);
  collapseAll.setEnabled(item && item->childCount() > 0);

  QObject::connect(&expandAll, &QAction::triggered,
                   [this, item]() { ui->counterTree->expandAllItems(item); });

  QObject::connect(&collapseAll, &QAction::triggered,
                   [this, item]() { ui->counterTree->collapseAllItems(item); });

  RDDialog::show(&contextMenu, ui->counterTree->viewport()->mapToGlobal(pos));
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
    ui->counterTree->setSelectedItem(it.value());

    expandToNode(it.value());
  }
}
