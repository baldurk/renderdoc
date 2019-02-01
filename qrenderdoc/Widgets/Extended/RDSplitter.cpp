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

#include "RDSplitter.h"

#include <QPaintEvent>
#include <QPainter>

RDSplitterHandle::RDSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent), m_index(-1), m_isCollapsed(false)
{
}

void RDSplitterHandle::setIndex(int index)
{
  m_index = index;
}

int RDSplitterHandle::index() const
{
  return m_index;
}

void RDSplitterHandle::setTitle(const QString &title)
{
  m_title = title;
}

const QString &RDSplitterHandle::title() const
{
  return m_title;
}

void RDSplitterHandle::setCollapsed(bool collapsed)
{
  m_isCollapsed = collapsed;
}

bool RDSplitterHandle::collapsed() const
{
  return m_isCollapsed;
}

void RDSplitterHandle::paintEvent(QPaintEvent *event)
{
  QPainter painter(this);
  QColor col = palette().color(QPalette::WindowText);
  painter.setPen(col);
  painter.setBrush(QBrush(col));

  int w = width();
  int h = height();

  if(orientation() == Qt::Vertical)
  {
    painter.drawText(QRect(0, 0, w, 25), Qt::AlignHCenter, m_title);
  }
  else
  {
    painter.drawText(QRect(0, h / 2 - 12, w, 25), Qt::AlignHCenter, m_title);
  }

  painter.setRenderHint(QPainter::Antialiasing, true);

  // draw the arrow
  if(orientation() == Qt::Vertical)
  {
    if(m_isCollapsed)
    {
      m_arrowPoints[0] = QPoint(w / 2, h - 9);
      m_arrowPoints[1] = QPoint(w / 2 - 10, h - 1);
      m_arrowPoints[2] = QPoint(w / 2 + 10, h - 1);
    }
    else
    {
      m_arrowPoints[0] = QPoint(w / 2, h - 1);
      m_arrowPoints[1] = QPoint(w / 2 - 10, h - 9);
      m_arrowPoints[2] = QPoint(w / 2 + 10, h - 9);
    }
  }
  else
  {
    if(m_isCollapsed)
    {
      m_arrowPoints[0] = QPoint(w - 9, h / 2 + 15);
      m_arrowPoints[1] = QPoint(w - 1, h / 2 + 5);
      m_arrowPoints[2] = QPoint(w - 1, h / 2 + 20);
    }
    else
    {
      m_arrowPoints[0] = QPoint(w - 1, h / 2 + 15);
      m_arrowPoints[1] = QPoint(w - 9, h / 2 + 5);
      m_arrowPoints[2] = QPoint(w - 9, h / 2 + 25);
    }
  }

  painter.drawPolygon(m_arrowPoints, 3);

  // draw the bullets
  if(orientation() == Qt::Vertical)
  {
    painter.drawEllipse(QPoint(w / 4 - 10, h - 10), 3, 3);
    painter.drawEllipse(QPoint(w / 4, h - 10), 3, 3);
    painter.drawEllipse(QPoint(w / 4 + 10, h - 10), 3, 3);

    painter.drawEllipse(QPoint(3 * w / 4 - 10, h - 10), 3, 3);
    painter.drawEllipse(QPoint(3 * w / 4, h - 10), 3, 3);
    painter.drawEllipse(QPoint(3 * w / 4 + 10, h - 10), 3, 3);
  }
  else
  {
    painter.drawEllipse(QPoint(w - 10, h / 4 - 10), 3, 3);
    painter.drawEllipse(QPoint(w - 10, h / 4), 3, 3);
    painter.drawEllipse(QPoint(w - 10, h / 4 + 10), 3, 3);

    painter.drawEllipse(QPoint(w - 10, 3 * h / 4 - 10), 3, 3);
    painter.drawEllipse(QPoint(w - 10, 3 * h / 4), 3, 3);
    painter.drawEllipse(QPoint(w - 10, 3 * h / 4 + 10), 3, 3);
  }
}

void RDSplitterHandle::mouseDoubleClickEvent(QMouseEvent *event)
{
  RDSplitter *par = (RDSplitter *)splitter();
  par->handleDoubleClicked(m_index);
}

RDSplitter::RDSplitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent)
{
  initialize();
}

RDSplitter::RDSplitter(QWidget *parent) : QSplitter(parent)
{
  initialize();
}

void RDSplitter::handleDoubleClicked(int index)
{
  if(index < 0 || index >= count())
    return;

  RDSplitterHandle *rdHandle = (RDSplitterHandle *)handle(index);
  QList<int> totalSizes = sizes();
  if(totalSizes[index] > 0)
  {
    // add to the previous handle the size of the current one
    totalSizes[index - 1] += totalSizes[index];
    // set the current handle's size to 0
    totalSizes[index] = 0;
    rdHandle->setCollapsed(true);
  }
  else
  {
    // split the sizes in half
    int s = totalSizes[index - 1] / 2;
    totalSizes[index] = totalSizes[index - 1] = s;
    rdHandle->setCollapsed(false);
  }
  setSizes(totalSizes);
}

void RDSplitter::setHandleCollapsed(int pos, int index)
{
  QList<int> totalSizes = sizes();
  RDSplitterHandle *rdHandle = (RDSplitterHandle *)handle(index);
  if(totalSizes[index] == 0)
    rdHandle->setCollapsed(true);
  else
    rdHandle->setCollapsed(false);
}

void RDSplitter::initialize()
{
  connect(this, &RDSplitter::splitterMoved, this, &RDSplitter::setHandleCollapsed);
}

QSplitterHandle *RDSplitter::createHandle()
{
  return new RDSplitterHandle(orientation(), this);
}
