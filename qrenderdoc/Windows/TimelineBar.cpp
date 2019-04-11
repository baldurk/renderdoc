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

#include "TimelineBar.h"
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"

QPointF aliasAlign(QPointF pt)
{
  pt.setX(int(pt.x()) + 0.5);
  pt.setY(int(pt.y()) + 0.5);
  return pt;
}

QMarginsF uniformMargins(qreal m)
{
  return QMarginsF(m, m, m, m);
}

QMargins uniformMargins(int m)
{
  return QMargins(m, m, m, m);
}

class PipRanges
{
public:
  void push(qreal pos, const int triRadius)
  {
    if(ranges.isEmpty())
    {
      ranges.push_back({pos, pos});
      return;
    }

    QPair<qreal, qreal> &range = ranges.back();

    if(range.second + triRadius >= pos)
      range.second = pos;
    else
      ranges.push_back({pos, pos});
  }

  QPainterPath makePath(const int triRadius, const int triHeight, qreal y)
  {
    QPainterPath path;

    for(const QPair<qreal, qreal> &range : ranges)
    {
      if(range.first == range.second)
      {
        QPointF pos = aliasAlign(QPointF(range.first, y));

        QPainterPath triangle;
        triangle.addPolygon(
            QPolygonF({pos + QPoint(0, triHeight), pos + QPoint(triRadius * 2, triHeight),
                       pos + QPoint(triRadius, 0)}));
        triangle.closeSubpath();

        path = path.united(triangle);
      }
      else
      {
        QPointF left = aliasAlign(QPointF(range.first, y));
        QPointF right = aliasAlign(QPointF(range.second, y));

        QPainterPath trapezoid;
        trapezoid.addPolygon(
            QPolygonF({left + QPoint(0, triHeight), right + QPoint(triRadius * 2, triHeight),
                       right + QPoint(triRadius, 0), left + QPoint(triRadius, 0)}));
        trapezoid.closeSubpath();

        path = path.united(trapezoid);
      }
    }

    return path;
  }

private:
  QVector<QPair<qreal, qreal>> ranges;
};

TimelineBar::TimelineBar(ICaptureContext &ctx, QWidget *parent)
    : QAbstractScrollArea(parent), m_Ctx(ctx)
{
  m_Ctx.AddCaptureViewer(this);

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

  m_Ctx.RemoveCaptureViewer(this);
}

void TimelineBar::HighlightResourceUsage(ResourceId id)
{
  m_ID = id;
  m_UsageEvents.clear();
  m_UsageTarget = m_Ctx.GetResourceName(id);

  m_Ctx.Replay().AsyncInvoke([this, id](IReplayController *r) {
    rdcarray<EventUsage> usage = r->GetUsage(id);

    GUIInvoke::call(this, [this, usage]() {
      for(const EventUsage &u : usage)
        m_UsageEvents << u;
      qSort(m_UsageEvents);
      viewport()->update();
    });
  });

  viewport()->update();
}

void TimelineBar::HighlightHistory(ResourceId id, const rdcarray<PixelModification> &history)
{
  m_ID = id;
  m_HistoryTarget = QString();
  m_HistoryEvents.clear();

  if(id != ResourceId())
  {
    m_HistoryTarget = m_Ctx.GetResourceName(id);

    for(const PixelModification &mod : history)
      m_HistoryEvents << mod;
    qSort(m_HistoryEvents);
  }

  viewport()->update();
}

void TimelineBar::OnCaptureClosed()
{
  setWindowTitle(tr("Timeline"));

  m_ID = ResourceId();
  m_HistoryTarget = m_UsageTarget = QString();
  m_HistoryEvents.clear();
  m_UsageEvents.clear();

  m_Draws.clear();
  m_RootDraws.clear();
  m_RootMarkers.clear();

  layout();
}

void TimelineBar::OnCaptureLoaded()
{
  setWindowTitle(tr("Timeline - Frame #%1").arg(m_Ctx.FrameInfo().frameNumber));

  processDraws(m_RootMarkers, m_RootDraws, m_Ctx.CurDrawcalls());

  m_zoom = 1.0;
  m_pan = 0.0;
  m_lastPos = QPointF();

  layout();
}

void TimelineBar::OnEventChanged(uint32_t eventId)
{
  if(!m_HistoryTarget.isEmpty())
    m_HistoryTarget = m_Ctx.GetResourceName(m_ID);
  if(!m_UsageTarget.isEmpty())
    m_UsageTarget = m_Ctx.GetResourceName(m_ID);

  viewport()->update();
}

QSize TimelineBar::minimumSizeHint() const
{
  return QSize(margin * 4 + borderWidth * 2 + 100,
               margin * 4 + borderWidth * 2 + m_eidAxisRect.height() * 2 +
                   m_highlightingRect.height() + horizontalScrollBar()->sizeHint().height());
}

void TimelineBar::resizeEvent(QResizeEvent *e)
{
  layout();
}

void TimelineBar::layout()
{
  QFontMetrics fm(Formatter::PreferredFont());

  // the area of everything
  m_area = QRectF(viewport()->rect()).marginsRemoved(uniformMargins(borderWidth + margin));

  m_titleWidth = fm.width(eidAxisTitle) + fm.height();

  m_dataArea = m_area;
  m_dataArea.setLeft(m_dataArea.left() + m_titleWidth);

  m_eidAxisRect = m_dataArea.marginsRemoved(uniformMargins(margin));
  m_eidAxisRect.setHeight(qMax(fm.height(), dataBarHeight));

  m_markerRect = m_dataArea.marginsRemoved(uniformMargins(margin));
  m_markerRect.setTop(m_eidAxisRect.bottom() + margin);

  m_highlightingRect = m_area;
  m_highlightingRect.setHeight(qMax(fm.height(), dataBarHeight) + highlightingExtra);
  m_highlightingRect.moveTop(m_markerRect.bottom() - m_highlightingRect.height());

  m_markerRect.setBottom(m_highlightingRect.top());

  uint32_t maxEID = m_Draws.isEmpty() ? 0 : m_Draws.back();

  int stepSize = 1;
  int stepMagnitude = 1;

  m_eidAxisLabelTextWidth = fm.width(QString::number(maxEID));
  m_eidAxisLabelWidth = m_eidAxisLabelTextWidth + fm.height();
  m_eidAxisLabelStep = stepSize * stepMagnitude;

  qreal virtualSize = m_dataArea.width() * m_zoom;

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

  m_eidWidth = virtualSize / (maxEID + 1);

  int savedPan = m_pan;

  horizontalScrollBar()->setRange(0, virtualSize - m_dataArea.width());
  horizontalScrollBar()->setSingleStep(m_eidAxisLabelWidth);
  horizontalScrollBar()->setPageStep(m_dataArea.width());
  horizontalScrollBar()->setValue(-savedPan);

  viewport()->update();
}

void TimelineBar::mousePressEvent(QMouseEvent *e)
{
  m_lastPos = e->localPos();

  qreal x = e->localPos().x();

  if((e->modifiers() & Qt::AltModifier) == 0)
  {
    Marker *marker = findMarker(m_RootMarkers, m_markerRect, m_lastPos);
    if(marker)
    {
      marker->expanded = !marker->expanded;
      m_lastPos = QPointF();
      viewport()->update();
      return;
    }

    if(m_highlightingRect.contains(m_lastPos))
    {
      uint32_t eid = eventAt(x);

      m_lastPos = QPointF();

      // history events get first crack at any selection, if they exist
      if(!m_HistoryEvents.isEmpty())
      {
        PixelModification search = {eid};
        auto it = std::lower_bound(m_HistoryEvents.begin(), m_HistoryEvents.end(), search);

        if(it != m_HistoryEvents.end())
        {
          // lower_bound will have returned the next highest hit. Check if there is one below, and
          // if it's closer.
          if(it != m_HistoryEvents.begin() && (eid - (it - 1)->eventId) < (it->eventId - eid))
            it--;

          m_Ctx.SetEventID({}, it->eventId, it->eventId);
        }

        return;
      }

      if(!m_UsageEvents.isEmpty())
      {
        EventUsage search(eid, ResourceUsage::Unused);
        auto it = std::lower_bound(m_UsageEvents.begin(), m_UsageEvents.end(), search);

        if(it != m_UsageEvents.end())
        {
          // lower_bound will have returned the next highest hit. Check if there is one below, and
          // if it's closer.
          if(it != m_UsageEvents.begin() && (eid - (it - 1)->eventId) < (it->eventId - eid))
            it--;

          m_Ctx.SetEventID({}, it->eventId, it->eventId);
        }
      }

      return;
    }

    if(!m_Draws.isEmpty() && m_dataArea.contains(m_lastPos))
    {
      uint32_t eid = eventAt(x);
      auto it = std::find_if(m_Draws.begin(), m_Draws.end(), [eid](uint32_t d) {
        if(d >= eid)
          return true;

        return false;
      });

      if(it == m_Draws.end())
        m_Ctx.SetEventID({}, m_Draws.back(), m_Draws.back());
      else
        m_Ctx.SetEventID({}, *it, *it);
    }
  }
}

void TimelineBar::mouseReleaseEvent(QMouseEvent *e)
{
}

void TimelineBar::mouseMoveEvent(QMouseEvent *e)
{
  if(e->buttons() == Qt::LeftButton && m_lastPos != QPointF())
  {
    qreal x = e->localPos().x();

    if(e->modifiers() & Qt::AltModifier)
    {
      qreal delta = x - m_lastPos.x();
      m_pan += delta;

      m_pan = qBound(-m_eidAxisRect.width() * (m_zoom - 1.0), m_pan, 0.0);

      layout();
    }
    else if(!m_Draws.isEmpty() && m_dataArea.contains(e->localPos()) &&
            !m_highlightingRect.contains(e->localPos()))
    {
      uint32_t eid = eventAt(x);
      auto it = std::find_if(m_Draws.begin(), m_Draws.end(), [eid](uint32_t d) {
        if(d >= eid)
          return true;

        return false;
      });

      if(it != m_Draws.end())
        m_Ctx.SetEventID({}, *it, *it);
    }
  }
  else
  {
    viewport()->update();
  }

  m_lastPos = e->localPos();

  Marker *marker = findMarker(m_RootMarkers, m_markerRect, m_lastPos);
  if(marker)
    setCursor(Qt::PointingHandCursor);
  else
    unsetCursor();
}

void TimelineBar::wheelEvent(QWheelEvent *e)
{
  float mod = (1.0 + e->delta() / 2500.0f);

  qreal prevZoom = m_zoom;

  m_zoom = qMax(1.0, m_zoom * mod);

  qreal zoomDelta = (m_zoom / prevZoom);

  // adjust the pan so that it's still in bounds, and so the zoom acts centred on the mouse
  qreal newPan = m_pan;

  newPan -= (e->x() - m_eidAxisRect.left());
  newPan = newPan * zoomDelta;
  newPan += (e->x() - m_eidAxisRect.left());

  m_pan = qBound(-m_dataArea.width() * (m_zoom - 1.0), newPan, 0.0);

  e->accept();

  layout();
}

void TimelineBar::leaveEvent(QEvent *e)
{
  unsetCursor();
  viewport()->update();
}

void TimelineBar::paintEvent(QPaintEvent *e)
{
  QPainter p(viewport());

  p.setFont(font());
  p.setRenderHint(QPainter::TextAntialiasing);

  // draw boundaries and background
  {
    QRectF r = viewport()->rect();

    p.fillRect(r, palette().brush(QPalette::Window));

    r = r.marginsRemoved(QMargins(borderWidth + margin, borderWidth + margin, borderWidth + margin,
                                  borderWidth + margin));

    p.fillRect(r, palette().brush(QPalette::Base));
    p.drawRect(r);
  }

  QTextOption to;

  to.setWrapMode(QTextOption::NoWrap);
  to.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  QFontMetrics fm = p.fontMetrics();

  {
    QRectF titleRect = m_eidAxisRect;
    titleRect.setLeft(titleRect.left() - m_titleWidth);
    titleRect.setWidth(m_titleWidth);

    p.setPen(QPen(palette().brush(QPalette::Text), 1.0));

    // add an extra margin for the text
    p.drawText(titleRect.marginsRemoved(QMarginsF(margin, 0, 0, 0)), eidAxisTitle, to);

    titleRect.setLeft(titleRect.left() - margin);
    titleRect.setTop(titleRect.top() - margin);
    p.drawLine(titleRect.bottomLeft(), titleRect.bottomRight());
    p.drawLine(titleRect.topRight(), titleRect.bottomRight());
  }

  QRectF eidAxisRect = m_eidAxisRect;

  p.drawLine(eidAxisRect.bottomLeft(), eidAxisRect.bottomRight() + QPointF(margin, 0));

  p.drawLine(m_highlightingRect.topLeft(), m_highlightingRect.topRight());

  if(m_Draws.isEmpty())
    return;

  eidAxisRect.setLeft(m_eidAxisRect.left() + m_pan);

  uint32_t maxEID = m_Draws.isEmpty() ? 0 : m_Draws.back();

  to.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

  p.setFont(Formatter::PreferredFont());

  QRectF hoverRect = eidAxisRect;

  // clip labels to the visible section
  p.setClipRect(m_eidAxisRect.marginsAdded(QMargins(0, margin, margin, 0)));

  // draw where we're hovering
  {
    QPoint pos = viewport()->mapFromGlobal(QCursor::pos());

    if(m_dataArea.contains(pos))
    {
      uint32_t hoverEID = eventAt(pos.x());

      hoverRect.setLeft(offsetOf(hoverEID));
      hoverRect.setWidth(m_eidAxisLabelWidth);

      // recentre
      hoverRect.moveLeft(hoverRect.left() - m_eidAxisLabelWidth / 2 + m_eidWidth / 2);

      QColor backCol = palette().color(QPalette::Base);

      if(getLuminance(backCol) < 0.2f)
        backCol = backCol.lighter(120);
      else
        backCol = backCol.darker(120);

      QRectF backRect = hoverRect.marginsAdded(QMargins(0, margin - borderWidth, 0, 0));

      backRect.setLeft(qMax(backRect.left(), m_eidAxisRect.left() + 1));

      p.fillRect(backRect, backCol);

      p.drawText(hoverRect, QString::number(hoverEID), to);

      // re-add the top margin so the lines match up with the border around the EID axis
      hoverRect = hoverRect.marginsAdded(QMargins(0, margin, 0, 0));

      if(hoverRect.left() >= m_eidAxisRect.left())
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

  QRectF labelRect = eidAxisRect;
  labelRect.setWidth(m_eidAxisLabelWidth);

  // iterate through the EIDs from 0, starting from possible a negative offset if the user has
  // panned to the right.
  for(uint32_t i = 0; i <= maxEID; i += m_eidAxisLabelStep)
  {
    labelRect.moveLeft(offsetOf(i) - labelRect.width() / 2 + m_eidWidth / 2);

    // check if this label is visible at all, but don't draw labels that intersect with the hovered
    // number
    if(labelRect.right() >= 0 && !labelRect.intersects(hoverRect))
      p.drawText(labelRect, QString::number(i), to);

    // check if labelRect is off the edge of the screen
    if(labelRect.left() >= m_eidAxisRect.right())
      break;
  }

  // stop clipping
  p.setClipRect(viewport()->rect());

  // clip the markers
  p.setClipRect(m_markerRect);

  {
    QPen pen = p.pen();
    paintMarkers(p, m_RootMarkers, m_RootDraws, m_markerRect);
    p.setPen(pen);
  }

  // stop clipping
  p.setClipRect(viewport()->rect());

  QRectF currentRect = eidAxisRect;

  // draw the current label and line
  {
    uint32_t curEID = m_Ctx.CurEvent();

    currentRect.setLeft(offsetOf(curEID));
    currentRect.setWidth(
        qMax(m_eidAxisLabelWidth, m_eidAxisLabelTextWidth + dataBarHeight + margin * 2));

    // recentre
    currentRect.moveLeft(currentRect.left() - currentRect.width() / 2 + m_eidWidth / 2);

    // remember where the middle would have been, without clamping
    qreal realMiddle = currentRect.center().x();

    // clamp the position from the left or right side
    if(currentRect.left() < eidAxisRect.left())
      currentRect.moveLeft(eidAxisRect.left());
    else if(currentRect.right() > eidAxisRect.right())
      currentRect.moveRight(eidAxisRect.right());

    // re-add the top margin so the lines match up with the border around the EID axis
    QRectF currentBackRect = currentRect.marginsAdded(QMargins(0, margin, 0, 0));

    p.fillRect(currentBackRect, palette().brush(QPalette::Base));
    p.drawRect(currentBackRect);

    // draw the 'current marker' pixmap
    const QPixmap &px = Pixmaps::flag_green(devicePixelRatio());
    p.drawPixmap(currentRect.topLeft() + QPointF(margin, 1), px, px.rect());

    // move to where the text should be and draw it
    currentRect.setLeft(currentRect.left() + margin * 2 + dataBarHeight);
    p.drawText(currentRect, QString::number(curEID), to);

    // draw a line from the bottom of the shadow downwards
    QPointF currentTop = currentRect.center();
    currentTop.setX(int(qBound(eidAxisRect.left(), realMiddle, eidAxisRect.right() - 2.0)) + 0.5);
    currentTop.setY(currentRect.bottom());

    QPointF currentBottom = currentTop;
    currentBottom.setY(m_markerRect.bottom());

    p.drawLine(currentTop, currentBottom);
  }

  to.setAlignment(Qt::AlignLeft | Qt::AlignTop);

  if(!m_UsageTarget.isEmpty() || !m_HistoryTarget.isEmpty())
  {
    p.setRenderHint(QPainter::Antialiasing);

    QRectF highlightLabel = m_highlightingRect.marginsRemoved(uniformMargins(margin));

    highlightLabel.setX(highlightLabel.x() + margin);

    QString text;

    if(!m_HistoryTarget.isEmpty())
      text = tr("Pixel history for %1").arg(m_HistoryTarget);
    else
      text = tr("Usage for %1:").arg(m_UsageTarget);

    p.drawText(highlightLabel, text, to);

    const int triRadius = fm.averageCharWidth();
    const int triHeight = fm.ascent();

    QPainterPath triangle;
    triangle.addPolygon(
        QPolygonF({QPoint(0, triHeight), QPoint(triRadius * 2, triHeight), QPoint(triRadius, 0)}));
    triangle.closeSubpath();

    enum
    {
      ReadUsage,
      WriteUsage,
      ReadWriteUsage,
      ClearUsage,
      BarrierUsage,

      HistoryPassed,
      HistoryFailed,

      UsageCount,
    };

    // colors taken from http://mkweb.bcgsc.ca/colorblind/ to be distinct for people with color
    // blindness

    const QColor colors[UsageCount] = {
        // read
        QColor::fromRgb(230, 159, 0),
        // write
        QColor::fromRgb(86, 180, 233),
        // read/write
        QColor::fromRgb(240, 228, 66),
        // clear
        QColor::fromRgb(0, 0, 0),
        // barrier
        QColor::fromRgb(204, 121, 167),

        // pass
        QColor::fromRgb(0, 158, 115),
        // fail
        QColor::fromRgb(213, 94, 0),
    };

    // draw the key
    if(m_HistoryTarget.isEmpty())
    {
      // advance past the first text to draw the key
      highlightLabel.setLeft(highlightLabel.left() + fm.width(text));

      text = lit(" Reads ( ");
      p.drawText(highlightLabel, text, to);
      highlightLabel.setLeft(highlightLabel.left() + fm.width(text));

      QPainterPath path = triangle.translated(aliasAlign(highlightLabel.topLeft()));
      p.fillPath(path, colors[ReadUsage]);
      p.drawPath(path);
      highlightLabel.setLeft(highlightLabel.left() + triRadius * 2);

      text = lit(" ), Writes ( ");
      p.drawText(highlightLabel, text, to);
      highlightLabel.setLeft(highlightLabel.left() + fm.width(text));

      path = triangle.translated(aliasAlign(highlightLabel.topLeft()));
      p.fillPath(path, colors[WriteUsage]);
      p.drawPath(path);
      highlightLabel.setLeft(highlightLabel.left() + triRadius * 2);

      text = lit(" ), Read/Write ( ");
      p.drawText(highlightLabel, text, to);
      highlightLabel.setLeft(highlightLabel.left() + fm.width(text));

      path = triangle.translated(aliasAlign(highlightLabel.topLeft()));
      p.fillPath(path, colors[ReadWriteUsage]);
      p.drawPath(path);
      highlightLabel.setLeft(highlightLabel.left() + triRadius * 2);

      if(m_Ctx.CurPipelineState().SupportsBarriers())
      {
        text = lit(" ) Barriers ( ");
        p.drawText(highlightLabel, text, to);
        highlightLabel.setLeft(highlightLabel.left() + fm.width(text));

        path = triangle.translated(aliasAlign(highlightLabel.topLeft()));
        p.fillPath(path, colors[BarrierUsage]);
        p.drawPath(path);
        highlightLabel.setLeft(highlightLabel.left() + triRadius * 2);
      }

      text = lit(" ), and Clears ( ");
      p.drawText(highlightLabel, text, to);
      highlightLabel.setLeft(highlightLabel.left() + fm.width(text));

      path = triangle.translated(aliasAlign(highlightLabel.topLeft()));
      p.fillPath(path, colors[ClearUsage]);
      p.drawPath(path);
      highlightLabel.setLeft(highlightLabel.left() + triRadius * 2);

      text = lit(" )");
      p.drawText(highlightLabel, text, to);
    }

    PipRanges pipranges[UsageCount];

    QRectF pipsRect = m_highlightingRect.marginsRemoved(uniformMargins(margin));

    pipsRect.setX(pipsRect.x() + margin + m_titleWidth);
    pipsRect.setHeight(triHeight + margin);
    pipsRect.moveBottom(m_highlightingRect.bottom());

    p.setClipRect(pipsRect);

    qreal leftClip = -triRadius * 2.0;
    qreal rightClip = pipsRect.width() + triRadius * 10.0;

    if(!m_HistoryEvents.isEmpty())
    {
      for(const PixelModification &mod : m_HistoryEvents)
      {
        qreal pos = offsetOf(mod.eventId) + m_eidWidth / 2 - triRadius;

        if(pos < leftClip || pos > rightClip)
          continue;

        if(mod.Passed())
          pipranges[HistoryPassed].push(pos, triRadius);
        else
          pipranges[HistoryFailed].push(pos, triRadius);
      }
    }
    else
    {
      for(const EventUsage &use : m_UsageEvents)
      {
        qreal pos = offsetOf(use.eventId) + m_eidWidth / 2 - triRadius;

        if(pos < leftClip || pos > rightClip)
          continue;

        if(((int)use.usage >= (int)ResourceUsage::VS_RWResource &&
            (int)use.usage <= (int)ResourceUsage::All_RWResource) ||
           use.usage == ResourceUsage::GenMips || use.usage == ResourceUsage::Copy ||
           use.usage == ResourceUsage::Resolve)
        {
          pipranges[ReadWriteUsage].push(pos, triRadius);
        }
        else if(use.usage == ResourceUsage::StreamOut || use.usage == ResourceUsage::ResolveDst ||
                use.usage == ResourceUsage::ColorTarget ||
                use.usage == ResourceUsage::DepthStencilTarget || use.usage == ResourceUsage::CopyDst)
        {
          pipranges[WriteUsage].push(pos, triRadius);
        }
        else if(use.usage == ResourceUsage::Clear)
        {
          pipranges[ClearUsage].push(pos, triRadius);
        }
        else if(use.usage == ResourceUsage::Barrier)
        {
          pipranges[BarrierUsage].push(pos, triRadius);
        }
        else
        {
          pipranges[ReadUsage].push(pos, triRadius);
        }
      }
    }

    for(int i = 0; i < UsageCount; i++)
    {
      QPainterPath path = pipranges[i].makePath(triRadius, triHeight, pipsRect.y());

      if(!path.isEmpty())
      {
        p.drawPath(path);
        p.fillPath(path, colors[i]);
      }
    }
  }
  else
  {
    QRectF highlightLabel = m_highlightingRect;
    highlightLabel = highlightLabel.marginsRemoved(uniformMargins(margin));

    highlightLabel.setX(highlightLabel.x() + margin);

    p.drawText(highlightLabel, tr("No resource selected for highlighting."), to);
  }
}

TimelineBar::Marker *TimelineBar::findMarker(QVector<Marker> &markers, QRectF markerRect, QPointF pos)
{
  QFontMetrics fm(Formatter::PreferredFont());

  for(Marker &m : markers)
  {
    QRectF r = markerRect;
    r.setLeft(qMax(m_markerRect.left() + borderWidth * 2, offsetOf(m.eidStart)));
    r.setRight(qMin(m_markerRect.right() - borderWidth, offsetOf(m.eidEnd + 1)));
    r.setHeight(fm.height() + borderWidth * 2);

    if(r.width() <= borderWidth * 2)
      continue;

    if(r.contains(pos))
    {
      return &m;
    }

    if(!m.children.isEmpty() && m.expanded)
    {
      QRectF childRect = r;
      childRect.setTop(r.bottom() + borderWidth * 2);
      childRect.setBottom(markerRect.bottom());

      Marker *res = findMarker(m.children, childRect, pos);

      if(res)
        return res;
    }
  }

  return NULL;
}

void TimelineBar::paintMarkers(QPainter &p, const QVector<Marker> &markers,
                               const QVector<uint32_t> &draws, QRectF markerRect)
{
  if(markers.isEmpty() && draws.isEmpty())
    return;

  QTextOption to;

  to.setWrapMode(QTextOption::NoWrap);
  to.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  QFontMetrics fm(Formatter::PreferredFont());

  // store a reference of what a completely elided string looks like
  QString tooshort = fm.elidedText(lit("asd"), Qt::ElideRight, fm.height());

  for(const Marker &m : markers)
  {
    QRectF r = markerRect;
    r.setLeft(qMax(m_dataArea.left() + borderWidth * 3, offsetOf(m.eidStart)));
    r.setRight(qMin(m_dataArea.right() - borderWidth, offsetOf(m.eidEnd + 1)));
    r.setHeight(fm.height() + borderWidth * 2);

    if(r.width() <= borderWidth * 2)
      continue;

    QColor backColor = m.color;
    if(r.contains(m_lastPos))
      backColor.setAlpha(150);

    p.setPen(QPen(palette().brush(QPalette::Text), 1.0));
    p.fillRect(r, QBrush(backColor));
    p.drawRect(r);

    p.setPen(QPen(QBrush(contrastingColor(backColor, palette().color(QPalette::Text))), 1.0));

    r.setLeft(r.left() + margin);

    int plusWidth = fm.width(QLatin1Char('+'));
    if(r.width() > plusWidth)
    {
      QRectF plusRect = r;
      plusRect.setWidth(plusWidth);

      QTextOption plusOption = to;
      plusOption.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

      p.drawText(plusRect, m.expanded ? lit("-") : lit("+"), plusOption);

      r.setLeft(r.left() + plusWidth + margin);
    }

    QString elided = fm.elidedText(m.name, Qt::ElideRight, r.width());

    // if everything was elided, just omit the title entirely
    if(elided == tooshort)
      elided = QString();

    r.setLeft(qRound(r.left() + margin));

    p.drawText(r, elided, to);

    if(m.expanded)
    {
      QRectF childRect = r;
      childRect.setTop(r.bottom() + borderWidth * 2);
      childRect.setBottom(markerRect.bottom());

      paintMarkers(p, m.children, m.draws, childRect);
    }
  }

  p.setRenderHint(QPainter::Antialiasing);

  for(uint32_t d : draws)
  {
    QRectF r = markerRect;
    r.setLeft(qMax(m_dataArea.left() + borderWidth * 3, offsetOf(d)));
    r.setRight(qMin(m_dataArea.right() - borderWidth, offsetOf(d + 1)));
    r.setHeight(fm.height() + borderWidth * 2);

    QPainterPath path;
    path.addRoundedRect(r, 5, 5);

    p.setPen(QPen(palette().brush(QPalette::Text), 1.0));
    p.fillPath(path, d == m_Ctx.CurEvent() ? Qt::green : Qt::blue);
    p.drawPath(path);
  }

  p.setRenderHint(QPainter::Antialiasing, false);
}

uint32_t TimelineBar::eventAt(qreal x)
{
  if(m_Draws.isEmpty())
    return 0;

  // clamp to the visible viewport
  x = qBound(m_eidAxisRect.left(), x, m_eidAxisRect.right());

  // do the reverse of offsetOf() - first make the x relative to the root
  x -= m_pan + m_eidAxisRect.left();

  // multiply up to get a floating point 'steps'
  qreal steps = x / m_eidAxisLabelWidth;

  // finally convert to EID and clamp
  uint32_t maxEID = m_Draws.back();
  return qMin(maxEID, uint32_t(steps * m_eidAxisLabelStep));
}

qreal TimelineBar::offsetOf(uint32_t eid)
{
  int steps = eid / m_eidAxisLabelStep;

  qreal fractionalPart = qreal(eid % m_eidAxisLabelStep) / qreal(m_eidAxisLabelStep);

  return m_eidAxisRect.left() + m_pan + steps * m_eidAxisLabelWidth +
         fractionalPart * m_eidAxisLabelWidth;
}

uint32_t TimelineBar::processDraws(QVector<Marker> &markers, QVector<uint32_t> &draws,
                                   const rdcarray<DrawcallDescription> &curDraws)
{
  uint32_t maxEID = 0;

  for(const DrawcallDescription &d : curDraws)
  {
    if(!d.children.isEmpty())
    {
      markers.push_back(Marker());
      Marker &m = markers.back();

      m.name = d.name;
      m.eidStart = d.eventId;
      m.eidEnd = processDraws(m.children, m.draws, d.children);

      maxEID = qMax(maxEID, m.eidEnd);

      if(d.markerColor[3] > 0.0f)
      {
        m.color = QColor::fromRgb(
            qRgb(d.markerColor[0] * 255.0f, d.markerColor[1] * 255.0f, d.markerColor[2] * 255.0f));
      }
      else
      {
        m.color = QColor(Qt::gray);
      }
    }
    else
    {
      if(!(d.flags & DrawFlags::SetMarker))
      {
        m_Draws.push_back(d.eventId);
        draws.push_back(d.eventId);
      }
    }

    maxEID = qMax(maxEID, d.eventId);
  }

  return maxEID;
}
