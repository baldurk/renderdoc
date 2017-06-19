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

#include "OrderedListEditor.h"
#include <QKeyEvent>
#include <QToolButton>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "ui_OrderedListEditor.h"

OrderedListEditor::OrderedListEditor(const QString &windowName, const QString &itemName,
                                     BrowseMode browse, QWidget *parent)
    : QDialog(parent), ui(new Ui::OrderedListEditor)
{
  ui->setupUi(this);

  ui->list->setFont(Formatter::PreferredFont());

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_BrowseMode = browse;

  setWindowTitle(windowName);

  if(m_BrowseMode == BrowseMode::None)
  {
    ui->list->setColumnCount(1);
    ui->list->setHorizontalHeaderLabels({itemName});
    ui->list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  }
  else
  {
    QStringList labels;
    labels << itemName << tr("Browse");
    ui->list->setColumnCount(2);
    ui->list->setHorizontalHeaderLabels(labels);

    ui->list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  }

  QObject::connect(ui->list, &RDTableWidget::keyPress, this, &OrderedListEditor::list_keyPress);
}

OrderedListEditor::~OrderedListEditor()
{
  delete ui;
}

QToolButton *OrderedListEditor::makeBrowseButton()
{
  QToolButton *ret = new QToolButton(this);

  ret->setIcon(Icons::folder_page_white());
  ret->setAutoRaise(true);

  QObject::connect(ret, &QToolButton::clicked, this, &OrderedListEditor::browse_clicked);

  return ret;
}

void OrderedListEditor::setItems(const QStringList &strings)
{
  ui->list->setUpdatesEnabled(false);
  ui->list->clearContents();

  ui->list->setRowCount(strings.count());

  for(int i = 0; i < strings.count(); i++)
  {
    ui->list->setItem(i, 0, new QTableWidgetItem(strings[i]));

    if(m_BrowseMode != BrowseMode::None)
      ui->list->setCellWidget(i, 1, makeBrowseButton());
  }

  // if we added any strings above the new item row was automatically
  // appended. If not, add it explicitly here
  if(strings.count() == 0)
    addNewItemRow();

  ui->list->resizeColumnToContents(0);
  if(m_BrowseMode != BrowseMode::None)
    ui->list->resizeColumnToContents(1);

  ui->list->setUpdatesEnabled(true);
}

void OrderedListEditor::addNewItemRow()
{
  ui->list->insertRow(ui->list->rowCount());

  QTableWidgetItem *item = new QTableWidgetItem(QString());
  item->setFlags(item->flags() & ~(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
  ui->list->setItem(ui->list->rowCount() - 1, 0, item);

  if(m_BrowseMode != BrowseMode::None)
  {
    item = new QTableWidgetItem(QString());
    item->setFlags(item->flags() & ~(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
    ui->list->setItem(ui->list->rowCount() - 1, 1, item);

    ui->list->setCellWidget(ui->list->rowCount() - 1, 1, makeBrowseButton());
  }
}

QStringList OrderedListEditor::getItems()
{
  QStringList ret;

  // don't include the last 'new item' entry
  for(int i = 0; i < ui->list->rowCount() - 1; i++)
    ret << ui->list->item(i, 0)->text();

  return ret;
}

void OrderedListEditor::on_list_cellChanged(int row, int column)
{
  // hack :(. Assume this will only be hit on single UI thread.
  static bool recurse = false;

  if(recurse)
    return;

  recurse = true;

  // if the last row has something added to it, make a new final row
  if(row == ui->list->rowCount() - 1)
  {
    if(!ui->list->item(row, column)->data(Qt::DisplayRole).toString().trimmed().isEmpty())
    {
      // enable dragging
      ui->list->item(row, 0)->setFlags(ui->list->item(row, 0)->flags() |
                                       (Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
      if(m_BrowseMode != BrowseMode::None)
        delete ui->list->takeItem(row, 1);

      addNewItemRow();
    }
  }

  if(row > 0 && column == 0 &&
     ui->list->item(row, column)->data(Qt::DisplayRole).toString().trimmed().isEmpty())
  {
    ui->list->removeRow(row);
  }

  recurse = false;
}

void OrderedListEditor::browse_clicked()
{
  QWidget *tool = qobject_cast<QWidget *>(QObject::sender());

  if(tool)
  {
    for(int i = 0; i < ui->list->rowCount(); i++)
    {
      QWidget *rowButton = ui->list->cellWidget(i, 1);
      if(rowButton == tool)
      {
        QString sel;
        if(m_BrowseMode == BrowseMode::Folder)
          sel = RDDialog::getExistingDirectory(this, tr("Browse for a folder"));
        else
          sel = RDDialog::getOpenFileName(this, tr("Browse for a file"));

        if(!sel.isEmpty())
          ui->list->item(i, 0)->setText(sel);
      }
    }
  }
}

void OrderedListEditor::list_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Delete)
  {
    int row = -1;
    if(ui->list->selectionModel()->selectedIndexes().count() > 0)
      row = ui->list->selectionModel()->selectedIndexes()[0].row();

    if(row >= 0)
      ui->list->removeRow(row);
  }
}
