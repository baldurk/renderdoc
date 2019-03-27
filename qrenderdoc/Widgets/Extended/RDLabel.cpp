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

#include "RDLabel.h"
#include <QMouseEvent>
#include <QPainter>
#include "Code/QRDUtils.h"

RDLabel::RDLabel(QWidget *parent) : QLabel(parent)
{
}

RDLabel::~RDLabel()
{
}

void RDLabel::modifySizeHint(QSize &sz) const
{
  if(m_preserveRatio)
    sz.setWidth(sz.width() - contentsMargins().left() - contentsMargins().right());

  if(m_variant.isValid())
    sz.setWidth(qMax(RichResourceTextWidthHint(this, font(), m_variant) + contentsMargins().left() +
                         contentsMargins().right() + margin() * 2,
                     sz.width()));
}

QSize RDLabel::sizeHint() const
{
  QSize sz = QLabel::sizeHint();
  modifySizeHint(sz);
  return sz;
}

QSize RDLabel::minimumSizeHint() const
{
  QSize sz = QLabel::minimumSizeHint();
  modifySizeHint(sz);
  if(m_minSizeHint.isValid())
    sz = sz.expandedTo(m_minSizeHint);
  return sz;
}

void RDLabel::setText(const QString &text)
{
  m_variant = text;
  RichResourceTextInitialise(m_variant);
  if(RichResourceTextCheck(m_variant))
  {
    setMouseTracking(true);
    m_hover = false;
    QLabel::setText(QString());
    updateGeometry();
    repaint();
  }
  else
  {
    m_variant = QVariant();
    QLabel::setText(text);
  }
}

QString RDLabel::text() const
{
  if(m_variant.isValid())
    return m_variant.toString();

  return QLabel::text();
}

void RDLabel::setMinimumSizeHint(const QSize &sz)
{
  m_minSizeHint = sz;
  updateGeometry();
}

void RDLabel::mousePressEvent(QMouseEvent *event)
{
  emit(clicked(event));

  QLabel::mousePressEvent(event);
}

void RDLabel::mouseReleaseEvent(QMouseEvent *event)
{
  if(m_variant.isValid())
  {
    RichResourceTextMouseEvent(this, m_variant, rect(), font(), event);
    return;
  }

  QLabel::mouseReleaseEvent(event);
}

void RDLabel::mouseMoveEvent(QMouseEvent *event)
{
  emit(mouseMoved(event));

  if(m_variant.isValid())
  {
    bool hover = RichResourceTextMouseEvent(this, m_variant, rect(), font(), event);
    if(hover)
      setCursor(QCursor(Qt::PointingHandCursor));
    else
      unsetCursor();

    if(m_hover != hover)
      update();
    m_hover = hover;

    return;
  }

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

  if(m_variant.isValid())
  {
    unsetCursor();
    repaint();
    m_hover = false;
  }

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

  QLabel::resizeEvent(event);
}

void RDLabel::changeEvent(QEvent *event)
{
  if(event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
    emit styleChanged(event);

  QLabel::changeEvent(event);
}

void RDLabel::paintEvent(QPaintEvent *event)
{
  QLabel::paintEvent(event);

  if(m_variant.isValid())
  {
    QPainter painter(this);

    QPoint pos = mapFromGlobal(QCursor::pos());

    QRect r = rect();
    r.setLeft(r.left() + contentsMargins().left() + margin());
    r.setRight(r.right() - contentsMargins().right() - margin());

    RichResourceTextPaint(this, &painter, r, font(), palette(), r.contains(pos), pos, m_variant);
  }
}
