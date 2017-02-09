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

#include "RDTreeWidget.h"
#include <QColor>
#include <QDebug>
#include <QMouseEvent>

RDTreeWidget::RDTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
  setMouseTracking(true);
}

void RDTreeWidget::setHoverIcons(QTreeWidgetItem *item, QIcon normal, QIcon hover)
{
  item->setData(m_hoverColumn, Qt::DecorationRole, normal);
  item->setData(m_hoverColumn, hoverIconRole, hover);
}

void RDTreeWidget::setHoverColour(QTreeWidgetItem *item, QColor col)
{
  item->setData(m_hoverColumn, hoverBackColourRole, col);
}

void RDTreeWidget::mouseMoveEvent(QMouseEvent *e)
{
  QTreeWidgetItem *item = itemAt(e->pos());

  clearHovers(invisibleRootItem(), item);

  if(item)
  {
    QVariant normalIcon = item->data(m_hoverColumn, Qt::DecorationRole);
    QVariant hoverIcon = item->data(m_hoverColumn, hoverIconRole);

    if(normalIcon.isValid() && hoverIcon.isValid())
    {
      item->setData(m_hoverColumn, hoverIconRole, QVariant());
      item->setData(m_hoverColumn, Qt::DecorationRole, hoverIcon);
      item->setData(m_hoverColumn, backupNormalIconRole, normalIcon);
    }

    if(!item->data(m_hoverColumn, backupBackColourRole).isValid())
    {
      QVariant backHoverColor = item->data(m_hoverColumn, hoverBackColourRole);

      QColor col = item->data(m_hoverColumn, hoverBackColourRole).value<QColor>();
      if(!col.isValid())
        col = m_defaultHoverColour;

      if(col.isValid())
      {
        item->setData(m_hoverColumn, backupBackColourRole, item->background(m_hoverColumn));
        for(int c = 0; c < item->columnCount(); c++)
          item->setData(c, Qt::BackgroundRole, QBrush(col));
      }
    }

    QModelIndex idx = indexAt(e->pos());

    if(idx.column() == m_hoverColumn && item->data(m_hoverColumn, Qt::DecorationRole).isValid() &&
       m_hoverHandCursor)
    {
      setCursor(QCursor(Qt::PointingHandCursor));
    }
    else
    {
      unsetCursor();
    }
  }

  emit mouseMove(e);

  QTreeWidget::mouseMoveEvent(e);
}

void RDTreeWidget::mouseReleaseEvent(QMouseEvent *e)
{
  QModelIndex idx = indexAt(e->pos());

  if(idx.isValid() && idx.column() == m_hoverColumn && m_activateOnClick)
  {
    emit itemActivated(itemAt(e->pos()), idx.column());
  }

  QTreeWidget::mouseReleaseEvent(e);
}

void RDTreeWidget::leaveEvent(QEvent *e)
{
  unsetCursor();

  clearHovers(invisibleRootItem(), NULL);

  emit leave(e);

  QTreeWidget::leaveEvent(e);
}

void RDTreeWidget::focusOutEvent(QFocusEvent *event)
{
  if(m_clearSelectionOnFocusLoss)
    clearSelection();

  QTreeWidget::focusOutEvent(event);
}

void RDTreeWidget::clearHovers(QTreeWidgetItem *root, QTreeWidgetItem *exception)
{
  for(int i = 0; i < root->childCount(); i++)
  {
    QTreeWidgetItem *n = root->child(i);

    if(n == exception)
      continue;

    clearHovers(n, exception);

    QVariant original = n->data(m_hoverColumn, backupNormalIconRole);

    if(original.isValid())
    {
      QVariant icon = n->data(m_hoverColumn, Qt::DecorationRole);

      n->setData(m_hoverColumn, hoverIconRole, icon);
      n->setData(m_hoverColumn, Qt::DecorationRole, original);
      n->setData(m_hoverColumn, backupNormalIconRole, QVariant());
    }

    original = n->data(m_hoverColumn, backupBackColourRole);

    if(original.isValid())
    {
      QBrush orig = original.value<QBrush>();
      for(int c = 0; c < n->columnCount(); c++)
        n->setBackground(c, orig);
      n->setData(m_hoverColumn, backupBackColourRole, QVariant());
    }
  }
}
