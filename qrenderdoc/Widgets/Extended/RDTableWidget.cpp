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

#include "RDTableWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QDropEvent>
#include "Code/Interface/QRDInterface.h"

RDTableWidget::RDTableWidget(QWidget *parent) : QTableWidget(parent)
{
}

void RDTableWidget::dropEvent(QDropEvent *event)
{
  if(event->source() == this && model()->supportedDropActions() & event->dropAction())
  {
    QModelIndex index;
    if(viewport()->rect().contains(event->pos()))
      index = indexAt(event->pos());

    // ignore no-op drops (same source and dest row)
    if(selectedIndexes().contains(index))
      return;

    QRect rect = visualRect(index);

    int halfway = rect.top() + rect.height() / 2;

    // only allow below or above dropping
    int row = index.row();
    if(event->pos().y() > halfway)
      row++;

    // verify we can drop past this row (bit of a hack)
    {
      QTableWidgetItem *src = item(qMin(row, rowCount() - 1), 0);
      if(src && !(src->flags() & Qt::ItemIsDropEnabled))
        return;
    }

    insertRow(row);

    int srcRow = selectedIndexes()[0].row();

    // copy data across
    for(int i = 0; i < columnCount(); i++)
    {
      QTableWidgetItem *src = item(srcRow, i);
      if(src)
        setItem(row, i, new QTableWidgetItem(*item(srcRow, i)));
      else
        setCellWidget(row, i, cellWidget(srcRow, i));
    }

    removeRow(srcRow);

    return;
  }

  return QTableWidget::dropEvent(event);
}

void RDTableWidget::keyPressEvent(QKeyEvent *e)
{
  if(!m_customCopyPaste && e->matches(QKeySequence::Copy))
  {
    copySelection();
    return;
  }
  else
  {
    QTableView::keyPressEvent(e);
  }

  emit(keyPress(e));
}

void RDTableWidget::copySelection()
{
  QList<QTableWidgetItem *> items = selectedItems();

  std::sort(items.begin(), items.end(), [this](QTableWidgetItem *a, QTableWidgetItem *b) {
    if(row(a) != row(b))
      return row(a) < row(b);
    return column(a) < column(b);
  });

  int prevRow = row(items[0]);

  QString clipboardText;
  for(QTableWidgetItem *i : items)
  {
    clipboardText += i->text();

    if(prevRow != row(i))
      clipboardText += lit("\n");
    else
      clipboardText += lit(" | ");
  }

  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(clipboardText.trimmed());
}
