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

#include "RDLabel.h"
#include <QMouseEvent>

RDLabel::RDLabel(QWidget *parent) : QLabel(parent)
{
}

RDLabel::~RDLabel()
{
}

QSize RDLabel::sizeHint() const
{
  QSize sz = QLabel::sizeHint();

  if(m_preserveRatio)
    sz.setWidth(sz.width() - contentsMargins().left() - contentsMargins().right());

  return sz;
}

QSize RDLabel::minimumSizeHint() const
{
  QSize sz = QLabel::minimumSizeHint();

  if(m_preserveRatio)
    sz.setWidth(sz.width() - contentsMargins().left() - contentsMargins().right());

  return sz;
}

void RDLabel::mousePressEvent(QMouseEvent *event)
{
  emit(clicked(event));

  QLabel::mousePressEvent(event);
}

void RDLabel::mouseMoveEvent(QMouseEvent *event)
{
  emit(mouseMoved(event));

  QLabel::mouseMoveEvent(event);
}

void RDLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
  emit(doubleClicked(event));

  QLabel::mouseDoubleClickEvent(event);
}

void RDLabel::leaveEvent(QEvent *event)
{
  emit(leave());

  QLabel::leaveEvent(event);
}

void RDLabel::resizeEvent(QResizeEvent *event)
{
  const QPixmap *p = pixmap();
  if(m_preserveRatio && p)
  {
    QRect r = rect();

    float pratio = float(p->width()) / float(p->height());
    float rratio = float(r.width()) / float(r.height());

    if(pratio > rratio)
    {
      int correctHeight = int(r.width() / pratio);

      int margin = (r.height() - correctHeight) / 2;

      setContentsMargins(0, margin, 0, margin);
    }
    else
    {
      int correctWidth = int(r.height() * pratio);

      int margin = (r.width() - correctWidth) / 2;

      setContentsMargins(margin, 0, margin, 0);
    }
  }
}
