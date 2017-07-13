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

  // outer margin + border + inner margin + width of title + scale bar text advance
  m_leftCoord = margin + borderWidth + margin + fm.width(scaleTitle) + fm.height();

  m_totalSize = viewport()->width() - m_leftCoord - margin - borderWidth - margin;

  uint32_t maxEID = m_Events.isEmpty() ? 0 : m_Events.back();

  int stepSize = 1;
  int stepMagnitude = 1;

  m_scaleLabelWidth = fm.width(QString::number(maxEID)) + fm.height();
  m_scaleLabelStep = stepSize * stepMagnitude;

  qreal virtualSize = m_totalSize * m_zoom;

  while(virtualSize > 0 && (maxEID / m_scaleLabelStep) * m_scaleLabelWidth > virtualSize)
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

    m_scaleLabelStep = stepSize * stepMagnitude;
  }

  int numLabels = maxEID / m_scaleLabelStep + 1;

  m_scaleLabelWidth = virtualSize / numLabels;

  horizontalScrollBar()->setRange(0, virtualSize - m_totalSize);
  horizontalScrollBar()->setSingleStep(m_scaleLabelWidth);
  horizontalScrollBar()->setPageStep(m_totalSize);
  horizontalScrollBar()->setValue(-m_pan);

  viewport()->update();
}

void TimelineBar::mousePressEvent(QMouseEvent *e)
{
  m_lastPos = e->localPos();
}

void TimelineBar::mouseReleaseEvent(QMouseEvent *e)
{
}

void TimelineBar::mouseMoveEvent(QMouseEvent *e)
{
  if(e->buttons() == Qt::LeftButton)
  {
    qreal delta = e->localPos().x() - m_lastPos.x();
    m_pan += delta;

    m_pan = qBound(-m_totalSize * (m_zoom - 1.0), m_pan, 0.0);

    m_lastPos = e->localPos();

    layout();
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

void TimelineBar::paintEvent(QPaintEvent *e)
{
  QPainter p(viewport());

  p.setFont(font());
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::HighQualityAntialiasing);

  QRectF r = viewport()->rect();

  p.fillRect(r, palette().brush(QPalette::Window));

  r = r.marginsRemoved(QMarginsF(margin, margin, margin, margin));

  p.fillRect(r, palette().brush(QPalette::Text));

  r = r.marginsRemoved(QMarginsF(borderWidth, borderWidth, borderWidth, borderWidth));

  p.fillRect(r, palette().brush(QPalette::Base));

  QTextOption to;

  to.setWrapMode(QTextOption::NoWrap);
  to.setAlignment(Qt::AlignLeft | Qt::AlignTop);

  QRectF scaleRect = r;
  scaleRect.setHeight(qMin(scaleRect.height(), p.fontMetrics().height() + margin * 2));

  p.setPen(QPen(palette().brush(QPalette::Text), 0.0));
  p.drawLine(scaleRect.bottomLeft() + QPointF(0, 0.5), scaleRect.bottomRight() + QPointF(0, 0.5));

  scaleRect = scaleRect.marginsRemoved(QMargins(margin, margin, margin, margin));
  QString text = scaleTitle;
  p.drawText(scaleRect, text, to);

  scaleRect.setLeft(scaleRect.left() + p.fontMetrics().width(text) + scaleRect.height());

  if(!m_Ctx.LogLoaded())
    return;

  p.setClipRect(scaleRect);

  scaleRect.setLeft(scaleRect.left() + m_pan);

  uint32_t maxEID = m_Events.isEmpty() ? 0 : m_Events.back();

  to.setAlignment(Qt::AlignCenter | Qt::AlignTop);

  p.setFont(Formatter::PreferredFont());

  for(uint32_t i = 0; i <= maxEID; i += m_scaleLabelStep)
  {
    if(scaleRect.left() + m_scaleLabelWidth >= 0)
    {
      QRectF labelRect = scaleRect;
      labelRect.setWidth(m_scaleLabelWidth);
      p.drawText(labelRect, QString::number(i), to);
    }

    scaleRect.setLeft(scaleRect.left() + m_scaleLabelWidth);

    if(scaleRect.width() <= 0)
      break;
  }

  p.setClipRect(viewport()->rect());
}

uint32_t TimelineBar::eventAt(qreal x)
{
  if(m_Events.isEmpty())
    return 0;

  // pan x
  x -= m_pan;

  // normalise x between 0 and 1, left to right of bar area.
  x -= m_leftCoord;
  x /= m_totalSize;

  // apply zoom factor
  x /= m_zoom;

  // x = 0 is the left side of EID 0, x = 1 is the right side of the last EID
  return uint32_t(x * (m_Events.back() + 1));
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
