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

#include "OrderedListEditor.h"
#include <QHeaderView>
#include <QKeyEvent>
#include <QToolButton>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"

OrderedListEditor::OrderedListEditor(const QString &itemName, ItemButton mode, QWidget *parent)
    : RDTableWidget(parent)
{
  setFont(Formatter::PreferredFont());

  m_ButtonMode = mode;

  setDragEnabled(true);
  setDragDropOverwriteMode(false);
  setDragDropMode(QAbstractItemView::InternalMove);
  setDefaultDropAction(Qt::MoveAction);
  setAlternatingRowColors(true);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setCornerButtonEnabled(false);

  horizontalHeader()->setHighlightSections(false);
  horizontalHeader()->setMinimumSectionSize(50);
  verticalHeader()->setHighlightSections(false);

  if(m_ButtonMode == ItemButton::None)
  {
    setColumnCount(1);
    setHorizontalHeaderLabels({itemName});
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  }
  else
  {
    QStringList labels;
    labels << itemName;
    switch(m_ButtonMode)
    {
      case ItemButton::None: labels << lit("????"); break;
      case ItemButton::BrowseFile:
      case ItemButton::BrowseFolder: labels << tr("Browse"); break;
      case ItemButton::Delete: labels << tr("Delete"); break;
    }
    setColumnCount(2);
    setHorizontalHeaderLabels(labels);

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  }

  QObject::connect(this, &RDTableWidget::cellChanged, this, &OrderedListEditor::cellChanged);
}

OrderedListEditor::~OrderedListEditor()
{
}

QToolButton *OrderedListEditor::makeButton()
{
  QToolButton *ret = new QToolButton(this);

  if(m_ButtonMode == ItemButton::Delete)
    ret->setIcon(Icons::del());
  else
    ret->setIcon(Icons::folder_page_white());
  ret->setAutoRaise(true);

  QObject::connect(ret, &QToolButton::clicked, this, &OrderedListEditor::buttonActivate);

  return ret;
}

void OrderedListEditor::setItems(const QStringList &strings)
{
  setUpdatesEnabled(false);
  clearContents();

  setRowCount(strings.count());

  for(int i = 0; i < strings.count(); i++)
  {
    setItem(i, 0, new QTableWidgetItem(strings[i]));

    if(m_ButtonMode != ItemButton::None)
      setCellWidget(i, 1, makeButton());
  }

  // if we added any strings above the new item row was automatically
  // appended. If not, add it explicitly here
  if(strings.count() == 0)
    addNewItemRow();

  resizeColumnToContents(0);
  if(m_ButtonMode != ItemButton::None)
    resizeColumnToContents(1);

  setUpdatesEnabled(true);
}

void OrderedListEditor::addNewItemRow()
{
  if(!allowAddition())
    return;

  insertRow(rowCount());

  QTableWidgetItem *item = new QTableWidgetItem(QString());
  item->setFlags(item->flags() & ~(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
  setItem(rowCount() - 1, 0, item);

  if(m_ButtonMode != ItemButton::None)
  {
    item = new QTableWidgetItem(QString());
    item->setFlags(item->flags() & ~(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
    setItem(rowCount() - 1, 1, item);

    setCellWidget(rowCount() - 1, 1, makeButton());
  }
}

QStringList OrderedListEditor::getItems()
{
  QStringList ret;

  int count = rowCount();
  // don't include the last 'new item' entry
  if(allowAddition())
    count--;
  for(int i = 0; i < count; i++)
    ret << item(i, 0)->text();

  return ret;
}

void OrderedListEditor::cellChanged(int row, int column)
{
  // hack :(. Assume this will only be hit on single UI thread.
  static bool recurse = false;

  if(recurse)
    return;

  recurse = true;

  // if the last row has something added to it, make a new final row
  if(row == rowCount() - 1)
  {
    if(!item(row, column)->data(Qt::DisplayRole).toString().trimmed().isEmpty())
    {
      // enable dragging
      item(row, 0)->setFlags(item(row, 0)->flags() | (Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
      if(m_ButtonMode != ItemButton::None)
        delete takeItem(row, 1);

      addNewItemRow();
    }
  }

  if(row > 0 && column == 0 && item(row, column)->data(Qt::DisplayRole).toString().trimmed().isEmpty())
  {
    removeRow(row);
  }

  recurse = false;
}

void OrderedListEditor::buttonActivate()
{
  QWidget *tool = qobject_cast<QWidget *>(QObject::sender());

  if(tool)
  {
    for(int i = 0; i < rowCount(); i++)
    {
      QWidget *rowButton = cellWidget(i, 1);
      if(rowButton == tool)
      {
        if(m_ButtonMode == ItemButton::Delete)
        {
          this->removeRow(i);
          return;
        }

        QString sel;
        if(m_ButtonMode == ItemButton::BrowseFolder)
          sel = RDDialog::getExistingDirectory(this, tr("Browse for a folder"));
        else if(m_ButtonMode == ItemButton::BrowseFile)
          sel = RDDialog::getOpenFileName(this, tr("Browse for a file"));

        if(!sel.isEmpty())
          item(i, 0)->setText(sel);
      }
    }
  }
}

void OrderedListEditor::keyPressEvent(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Delete)
  {
    int row = -1;
    if(selectionModel()->selectedIndexes().count() > 0)
      row = selectionModel()->selectedIndexes()[0].row();

    if(row >= 0)
      removeRow(row);
  }

  RDTableWidget::keyPress(event);
}
