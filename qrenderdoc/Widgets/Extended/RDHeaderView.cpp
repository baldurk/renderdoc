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

#include "RDHeaderView.h"
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

RDHeaderView::RDHeaderView(Qt::Orientation orient, QWidget *parent) : QHeaderView(orient, parent)
{
  m_sectionPreview = new QLabel(this);
}

RDHeaderView::~RDHeaderView()
{
}

void RDHeaderView::mousePressEvent(QMouseEvent *event)
{
  int mousePos = event->x();
  int idx = logicalIndexAt(mousePos);

  if(idx >= 0 && event->buttons() == Qt::LeftButton)
  {
    int secSize = sectionSize(idx);
    int secPos = sectionViewportPosition(idx);

    int handleWidth = style()->pixelMetric(QStyle::PM_HeaderGripMargin, 0, this);

    if(secPos >= 0 && secSize > 0 && mousePos >= secPos + handleWidth &&
       mousePos <= secPos + secSize - handleWidth)
    {
      m_movingSection = idx;

      m_sectionPreview->resize(secSize, height());

      QPixmap preview(m_sectionPreview->size());
      preview.fill(QColor::fromRgba(qRgba(0, 0, 0, 100)));

      QPainter painter(&preview);
      painter.setOpacity(0.75f);
      paintSection(&painter, QRect(QPoint(0, 0), m_sectionPreview->size()), idx);
      painter.end();

      m_sectionPreview->setPixmap(preview);

      m_sectionPreviewOffset = mousePos - secPos;

      m_sectionPreview->move(mousePos - m_sectionPreviewOffset, 0);
      m_sectionPreview->show();

      return;
    }
  }

  QHeaderView::mousePressEvent(event);
}

void RDHeaderView::mouseMoveEvent(QMouseEvent *event)
{
  if(m_movingSection >= 0)
  {
    m_sectionPreview->move(event->x() - m_sectionPreviewOffset, 0);
    return;
  }

  QHeaderView::mouseMoveEvent(event);
}

void RDHeaderView::mouseReleaseEvent(QMouseEvent *event)
{
  if(m_movingSection >= 0)
  {
    int mousePos = event->x();
    int idx = logicalIndexAt(mousePos);

    if(idx >= 0)
    {
      int secSize = sectionSize(idx);
      int secPos = sectionPosition(idx);

      int srcSection = visualIndex(m_movingSection);
      int dstSection = visualIndex(idx);

      if(srcSection >= 0 && dstSection >= 0 && srcSection != dstSection)
      {
        // the half-way point of the section decides whether we're dropping to the left
        // or the right of it.
        if(mousePos < secPos + secSize / 2)
        {
          // if we're moving from the left, place it to the left of dstSection
          if(srcSection < dstSection)
            moveSection(srcSection, dstSection - 1);
          else
            moveSection(srcSection, dstSection);
        }
        else
        {
          // if we're moving it from the right, place it to the right of dstSection
          if(srcSection > dstSection)
            moveSection(srcSection, dstSection + 1);
          else
            moveSection(srcSection, dstSection);
        }
      }
    }

    m_sectionPreview->hide();
  }

  m_movingSection = -1;

  QHeaderView::mouseReleaseEvent(event);
}
