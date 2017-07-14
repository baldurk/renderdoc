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

#include "TimelineBar.h"
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include "Code/Resources.h"

TimelineBar::TimelineBar(ICaptureContext &ctx, QWidget *parent)
    : QAbstractScrollArea(parent), m_Ctx(ctx)
{
  m_Ctx.AddLogViewer(this);

  setMouseTracking(true);

  setFrameShape(NoFrame);

  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  QObject::connect(horizontalScrollBar(), &QScrollBar::valueChanged,
                   [this](int value) { m_pan = -value; });

  setWindowTitle(tr("Timeline"));
}

TimelineBar::~TimelineBar()
{
  m_Ctx.BuiltinWindowClosed(this);

  m_Ctx.RemoveLogViewer(this);
}

void TimelineBar::HighlightResourceUsage(ResourceId id)
{
}

void TimelineBar::HighlightHistory(ResourceId id, const QList<PixelModification> &history)
{
}

void TimelineBar::OnLogfileClosed()
{
  setWindowTitle(tr("Timeline"));

  m_Events.clear();
}

void TimelineBar::OnLogfileLoaded()
{
  setWindowTitle(tr("Timeline - Frame #%1").arg(m_Ctx.FrameInfo().frameNumber));

  addEvents(m_Ctx.CurDrawcalls());

  m_zoom = 1.0;
  m_pan = 0.0;
  m_lastPos = QPointF();

  layout();
}

void TimelineBar::OnEventChanged(uint32_t eventID)
{
  viewport()->update();
}

QSize TimelineBar::minimumSizeHint() const
{
  return QSize(margin * 4 + borderWidth * 2 + 100, margin * 4 + borderWidth * 2 + 40);
}

void TimelineBar::resizeEvent(QResizeEvent *e)
{
  layout();
}

void TimelineBar::layout()
{
  QFontMetrics fm(Formatter::PreferredFont());

  // outer margin + border + inner margin + width of title + text margin
  m_leftCoord = margin + borderWidth + margin + fm.width(eidAxisTitle) + fm.height();

  m_totalSize = viewport()->width() - m_leftCoord - margin - borderWidth - margin;

  uint32_t maxEID = m_Events.isEmpty() ? 0 : m_Events.back();

  int stepSize = 1;
  int stepMagnitude = 1;

  m_eidAxisLabelTextWidth = fm.width(QString::number(maxEID));
  m_eidAxisLabelWidth = m_eidAxisLabelTextWidth + fm.height();
  m_eidAxisLabelStep = stepSize * stepMagnitude;

  qreal virtualSize = m_totalSize * m_zoom;

  while(virtualSize > 0 && (maxEID / m_eidAxisLabelStep) * m_eidAxisLabelWidth > virtualSize)
  {
    // increment 1, 2, 5, 10, 20, 50, 100, ...
    if(stepSize == 1)
    {
      stepSize = 2;
    }
    else if(stepSize == 2)
    {
      stepSize = 5;
    }
    else if(stepSize == 5)
    {
      stepSize = 1;
      stepMagnitude *= 10;
    }

    m_eidAxisLabelStep = stepSize * stepMagnitude;
  }

  int numLabels = maxEID / m_eidAxisLabelStep + 1;

  m_eidAxisLabelWidth = virtualSize / numLabels;

  horizontalScrollBar()->setRange(0, virtualSize - m_totalSize);
  horizontalScrollBar()->setSingleStep(m_eidAxisLabelWidth);
  horizontalScrollBar()->setPageStep(m_totalSize);
  horizontalScrollBar()->setValue(-m_pan);

  viewport()->update();
}

void TimelineBar::mousePressEvent(QMouseEvent *e)
{
  m_lastPos = e->localPos();

  qreal x = e->localPos().x();

  if((e->modifiers() & Qt::AltModifier) == 0 && !m_Events.isEmpty() && x >= m_leftCoord &&
     x <= m_leftCoord + m_totalSize)
  {
    uint32_t eid = eventAt(x);
    auto it = std::find_if(m_Events.begin(), m_Events.end(), [this, eid](uint32_t d) {
      if(d >= eid)
        return true;

      return false;
    });

    if(it == m_Events.end())
      m_Ctx.SetEventID({}, m_Events.back(), m_Events.back());
    else
      m_Ctx.SetEventID({}, *it, *it);
  }
}

void TimelineBar::mouseReleaseEvent(QMouseEvent *e)
{
}

void TimelineBar::mouseMoveEvent(QMouseEvent *e)
{
  if(e->buttons() == Qt::LeftButton)
  {
    qreal x = e->localPos().x();

    if(e->modifiers() & Qt::AltModifier)
    {
      qreal delta = x - m_lastPos.x();
      m_pan += delta;

      m_pan = qBound(-m_totalSize * (m_zoom - 1.0), m_pan, 0.0);

      m_lastPos = e->localPos();

      layout();
    }
    else if(!m_Events.isEmpty() && x >= m_leftCoord && x <= m_leftCoord + m_totalSize)
    {
      uint32_t eid = eventAt(x);
      if(m_Events.contains(eid) && eid != m_Ctx.CurEvent())
        m_Ctx.SetEventID({}, eid, eid);
    }
  }
  else
  {
    viewport()->update();
  }
}

void TimelineBar::wheelEvent(QWheelEvent *e)
{
  float mod = (1.0 + e->delta() / 2500.0f);

  qreal prevZoom = m_zoom;

  m_zoom = qMax(1.0, m_zoom * mod);

  qreal zoomDelta = (m_zoom / prevZoom);

  // adjust the pan so that it's still in bounds, and so the zoom acts centred on the mouse
  qreal newPan = m_pan;

  newPan -= (e->x() - m_leftCoord);
  newPan = newPan * zoomDelta;
  newPan += (e->x() - m_leftCoord);

  m_pan = qBound(-m_totalSize * (m_zoom - 1.0), newPan, 0.0);

  e->accept();

  layout();
}

void TimelineBar::leaveEvent(QEvent *e)
{
  viewport()->update();
}

void TimelineBar::paintEvent(QPaintEvent *e)
{
  QPainter p(viewport());

  p.setFont(font());
  p.setRenderHint(QPainter::TextAntialiasing);

  QRectF r = viewport()->rect();

  p.fillRect(r, palette().brush(QPalette::Window));

  r = r.marginsRemoved(QMarginsF(margin, margin, margin, margin));

  p.fillRect(r, palette().brush(QPalette::Text));

  r = r.marginsRemoved(QMarginsF(borderWidth, borderWidth, borderWidth, borderWidth));

  p.fillRect(r, palette().brush(QPalette::Base));

  QTextOption to;

  to.setWrapMode(QTextOption::NoWrap);
  to.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  QFontMetrics fm = p.fontMetrics();

  QRectF eidAxisRect = r;
  eidAxisRect.setHeight(qMin(eidAxisRect.height(), qMax(fm.height(), eidAxisHeight) + margin * 2));

  p.setPen(QPen(palette().brush(QPalette::Text), 1.0));
  p.drawLine(eidAxisRect.bottomLeft(), eidAxisRect.bottomRight());

  eidAxisRect = eidAxisRect.marginsRemoved(QMargins(margin, margin, margin, margin));
  QString text = eidAxisTitle;
  p.drawText(eidAxisRect, text, to);

  eidAxisRect.setLeft(eidAxisRect.left() + p.fontMetrics().width(text) + fm.height());

  if(!m_Ctx.LogLoaded())
    return;

  QRectF clipRect = eidAxisRect;

  eidAxisRect.setLeft(eidAxisRect.left() + m_pan);

  uint32_t maxEID = m_Events.isEmpty() ? 0 : m_Events.back();

  to.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

  p.setFont(Formatter::PreferredFont());

  QRectF hoverRect = eidAxisRect;

  // draw where we're hovering
  {
    QPoint pos = viewport()->mapFromGlobal(QCursor::pos());

    if(r.contains(pos) && pos.x() >= m_leftCoord)
    {
      uint32_t hoverEID = eventAt(pos.x());

      hoverRect.setLeft(offsetOf(hoverEID));
      hoverRect.setWidth(m_eidAxisLabelWidth);

      p.drawText(hoverRect, QString::number(hoverEID), to);

      // re-add the vertical margins so the lines match up with the border around the EID axis
      hoverRect = hoverRect.marginsAdded(QMargins(0, margin, 0, margin));

      p.drawLine(hoverRect.topLeft(), hoverRect.bottomLeft());
      p.drawLine(hoverRect.topRight(), hoverRect.bottomRight());

      // shrink the rect a bit for clipping against labels below
      hoverRect.setX(qRound(hoverRect.x() + 0.5));
      hoverRect.setWidth(int(hoverRect.width()));
    }
    else
    {
      hoverRect = QRectF();
    }
  }

  // clip labels to the visible section
  p.setClipRect(clipRect);

  QRectF labelRect = eidAxisRect;

  // iterate through the EIDs from 0, starting from possible a negative offset if the user has
  // panned to the right.
  for(uint32_t i = 0; i <= maxEID; i += m_eidAxisLabelStep)
  {
    // check if this label is visible at all
    if(labelRect.left() + m_eidAxisLabelWidth >= 0)
    {
      QRectF textRect = labelRect;
      textRect.setWidth(m_eidAxisLabelWidth);

      // don't draw labels that intersect with the hovered number
      if(!textRect.intersects(hoverRect))
        p.drawText(textRect, QString::number(i), to);
    }

    labelRect.setLeft(labelRect.left() + m_eidAxisLabelWidth);

    // labelRect's right-hand side is the edge of the screen, so when width collapses to 0 no more
    // labels will be visible
    if(labelRect.width() <= 0)
      break;
  }

  // stop clipping
  p.setClipRect(viewport()->rect());

  QRectF currentRect = eidAxisRect;

  // draw the current label
  {
    uint32_t curEID = m_Ctx.CurEvent();

    // this is the centre of the label we want
    currentRect.setLeft(qRound(offsetOf(curEID)) + m_eidAxisLabelWidth / 2);

    // set the left based on the new width we want
    currentRect.setWidth(m_eidAxisLabelTextWidth + eidAxisHeight + margin * 2);
    currentRect.moveLeft(currentRect.left() - currentRect.width() / 2);

    // set the height a little lower to allow for the shadow
    currentRect.setHeight(currentRect.height() - 1);

    // remember where the middle would have been, without clamping
    qreal realMiddle = currentRect.center().x();

    // clamp the position from the left or right side
    if(currentRect.left() < m_leftCoord)
      currentRect.moveLeft(m_leftCoord);
    else if(currentRect.right() > eidAxisRect.right())
      currentRect.moveRight(eidAxisRect.right());

    // draw a shadow that's slightly offsetted, then the label itself.
    QRectF shadowRect = currentRect;
    shadowRect.adjust(1, 2, 1, 2);
    p.fillRect(shadowRect, palette().brush(QPalette::Shadow));
    p.fillRect(currentRect, palette().brush(QPalette::Base));
    p.drawRect(currentRect);

    // draw the 'current marker' pixmap
    const QPixmap &px = Pixmaps::flag_green(devicePixelRatio());
    p.drawPixmap(currentRect.topLeft() + QPointF(margin, 1), px, px.rect());

    // move to where the text should be and draw it
    currentRect.setLeft(currentRect.left() + margin * 2 + eidAxisHeight);
    p.drawText(currentRect, QString::number(curEID), to);

    // draw a line from the bottom of the shadow downwards
    QPointF currentTop = shadowRect.center();
    currentTop.setX(int(qBound(m_leftCoord, realMiddle, eidAxisRect.right() - 2.0)) + 0.5);
    currentTop.setY(shadowRect.bottom());

    QPointF currentBottom = currentTop;
    currentBottom.setY(r.bottom());

    p.drawLine(currentTop, currentBottom);
  }
}

uint32_t TimelineBar::eventAt(qreal x)
{
  if(m_Events.isEmpty())
    return 0;

  x = qBound(m_leftCoord, x, m_leftCoord + m_totalSize);

  // pan x
  x -= m_pan;

  // normalise x between 0 and 1, left to right of bar area.
  x -= m_leftCoord;
  x /= m_totalSize;

  // apply zoom factor
  x /= m_zoom;

  // x = 0 is the left side of EID 0, x = 1 is the right side of the last EID
  uint32_t maxEID = m_Events.back();
  return qMin(maxEID, uint32_t(x * (maxEID + 1)));
}

qreal TimelineBar::offsetOf(uint32_t eid)
{
  int steps = eid / m_eidAxisLabelStep;

  qreal fractionalPart = qreal(eid % m_eidAxisLabelStep) / qreal(m_eidAxisLabelStep);

  return m_leftCoord + m_pan + steps * m_eidAxisLabelWidth + fractionalPart * m_eidAxisLabelWidth;
}

void TimelineBar::addEvents(const rdctype::array<DrawcallDescription> &curDraws)
{
  for(const DrawcallDescription &d : curDraws)
  {
    addEvents(d.children);

    if(!(d.flags & (DrawFlags::SetMarker | DrawFlags::PushMarker)) || (d.flags & DrawFlags::APICalls))
      m_Events.push_back(d.eventID);
  }
}
