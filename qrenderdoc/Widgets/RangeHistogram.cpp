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

#include "RangeHistogram.h"
#include <float.h>
#include <math.h>
#include <QMouseEvent>
#include <QPainter>

RangeHistogram::RangeHistogram(QWidget *parent) : QWidget(parent)
{
  setMouseTracking(true);
}

RangeHistogram::~RangeHistogram()
{
}

// sets the range of data where the histogram data was calculated.
void RangeHistogram::setHistogramRange(float min, float max)
{
  m_HistogramMin = min;
  m_HistogramMax = max;
}

// sets the minimum and maximum as well as the black and white points
void RangeHistogram::setRange(float min, float max)
{
  m_RangeMin = min;
  if(min < 0.0f)
    m_RangeMax = qMax((min - FLT_EPSILON) * (1.0f - m_MinRangeSize), max);
  else
    m_RangeMax = qMax((min + FLT_EPSILON) * (1.0f + m_MinRangeSize), max);

  m_BlackPoint = m_RangeMin;
  m_WhitePoint = m_RangeMax;

  update();
  emit rangeUpdated();
}

bool RangeHistogram::ValidRange() const
{
  if(qIsInf(m_WhitePoint) || qIsNaN(m_WhitePoint) || qIsInf(m_BlackPoint) || qIsNaN(m_BlackPoint) ||
     qIsInf(m_RangeMax) || qIsNaN(m_RangeMax) || qIsInf(m_RangeMin) || qIsNaN(m_RangeMin) ||
     qIsInf(m_RangeMax - m_RangeMin) || qIsNaN(m_RangeMax - m_RangeMin) ||
     qIsInf(m_WhitePoint - m_BlackPoint) || qIsNaN(m_WhitePoint - m_BlackPoint))
  {
    return false;
  }

  return true;
}

void RangeHistogram::setHistogramData(const QVector<uint32_t> &histogram)
{
  m_HistogramData = histogram;

  update();
}

void RangeHistogram::setBlackPoint(float val)
{
  if(val <= m_RangeMin)
    m_BlackPoint = m_RangeMin = val;
  else
    m_BlackPoint = val;

  update();
  emit rangeUpdated();
}

void RangeHistogram::setWhitePoint(float val)
{
  if(val >= m_RangeMax)
    m_WhitePoint = m_RangeMax = val;
  else
    m_WhitePoint = val;

  update();
  emit rangeUpdated();
}

float RangeHistogram::blackDelta()
{
  if(!ValidRange())
    return 0.0f;

  return delta(m_BlackPoint);
}

void RangeHistogram::setBlackDelta(float value)
{
  setBlackPoint(qMin(m_WhitePoint - m_MinRangeSize, value * (m_RangeMax - m_RangeMin) + m_RangeMin));
}

float RangeHistogram::whiteDelta()
{
  if(!ValidRange())
    return 0.0f;

  return delta(m_WhitePoint);
}

void RangeHistogram::setWhiteDelta(float value)
{
  setWhitePoint(qMax(m_BlackPoint + m_MinRangeSize, value * (m_RangeMax - m_RangeMin) + m_RangeMin));
}

float RangeHistogram::delta(float val) const
{
  return (val - m_RangeMin) / (m_RangeMax - m_RangeMin);
}

void RangeHistogram::mousePressEvent(QMouseEvent *e)
{
  if(e->button() != Qt::LeftButton || !ValidRange())
    return;

  QRect r = rect();

  int whiteX = (int)(whiteDelta() * r.width());
  int blackX = (int)(blackDelta() * r.width());

  QPointF whiteVec(whiteX - e->pos().x(), rect().height() - e->pos().y());
  QPointF blackVec(blackX - e->pos().x(), e->pos().y());

  float whitedist = (float)sqrt(whiteVec.x() * whiteVec.x() + whiteVec.y() * whiteVec.y());
  float blackdist = (float)sqrt(blackVec.x() * blackVec.x() + blackVec.y() * blackVec.y());

  if(whitedist < blackdist && whitedist < 18.0f)
    m_DragMode = DraggingMode::White;
  else if(blackdist < whitedist && blackdist < 18.0f)
    m_DragMode = DraggingMode::Black;
  else if(e->pos().x() > whiteX)
    m_DragMode = DraggingMode::White;
  else if(e->pos().x() < blackX)
    m_DragMode = DraggingMode::Black;

  if(m_DragMode == DraggingMode::White)
  {
    float newWhite = (float)(e->pos().x() - totalSpace()) / (float)regionWidth();

    setWhiteDelta(qBound(blackDelta() + m_MinRangeSize, newWhite, 1.0f));
  }
  else if(m_DragMode == DraggingMode::Black)
  {
    float newBlack = (float)(e->pos().x() - totalSpace()) / (float)regionWidth();

    setBlackDelta(qBound(0.0f, newBlack, whiteDelta() - m_MinRangeSize));
  }

  emit rangeUpdated();

  if(m_DragMode != DraggingMode::None)
    update();

  m_MousePrev = e->pos();
}

void RangeHistogram::mouseReleaseEvent(QMouseEvent *e)
{
  m_DragMode = DraggingMode::None;

  m_MousePrev = QPoint(-1, -1);
}

void RangeHistogram::mouseMoveEvent(QMouseEvent *e)
{
  QPoint pos = e->pos();
  if(ValidRange() && (e->buttons() & Qt::LeftButton) && pos != m_MousePrev)
  {
    if(m_DragMode == DraggingMode::White)
    {
      float newWhite = (float)(pos.x() - totalSpace()) / (float)regionWidth();

      setWhiteDelta(qBound(blackDelta() + m_MinRangeSize, newWhite, 1.0f));
    }
    else if(m_DragMode == DraggingMode::Black)
    {
      float newBlack = (float)(pos.x() - totalSpace()) / (float)regionWidth();

      setBlackDelta(qBound(0.0f, newBlack, whiteDelta() - m_MinRangeSize));
    }

    emit rangeUpdated();

    if(m_DragMode != DraggingMode::None)
      update();

    m_MousePrev = pos;
  }
}

void RangeHistogram::paintEvent(QPaintEvent *e)
{
  QPainter p(this);

  p.setRenderHint(QPainter::Antialiasing);

  const QBrush blackBrush(QColor(0, 0, 0));
  const QBrush redBrush(QColor(60, 0, 0));
  const QBrush greenBrush(QColor(0, 128, 0));
  const QBrush whiteBrush(QColor(255, 255, 255));

  QRectF r = rect();

  p.eraseRect(r);

  r = r.marginsRemoved(QMarginsF(m_Margin, m_Margin, m_Margin, m_Margin));

  p.fillRect(r, palette().brush(QPalette::Shadow));

  QMarginsF border(m_Border, m_Border, m_Border, m_Border);
  border /= devicePixelRatioF();

  r = r.marginsRemoved(border);

  p.fillRect(r, ValidRange() ? palette().brush(QPalette::Inactive, QPalette::Highlight) : redBrush);

  int whiteX = (int)(whiteDelta() * r.width());
  int blackX = (int)(blackDelta() * r.width() + 0.5);

  QRectF blackPoint(r.topLeft(), QSize(blackX, r.height()));
  QRectF whitePoint(r.left() + whiteX, r.top(), r.width() - whiteX, r.height());

  if(ValidRange())
  {
    p.setPen(QPen(palette().color(QPalette::Dark)));
    p.drawLine(blackPoint.topRight(), blackPoint.bottomRight());
    p.drawLine(whitePoint.topLeft(), whitePoint.bottomLeft());
  }

  p.fillRect(whitePoint, whiteBrush);
  p.fillRect(blackPoint, blackBrush);

  if(!ValidRange())
    return;

  if(!m_HistogramData.isEmpty())
  {
    float minx = delta(m_HistogramMin);
    float maxx = delta(m_HistogramMax);

    uint32_t maxval = 0;
    for(int i = 0; i < m_HistogramData.count(); i++)
    {
      float x = (float)i / (float)m_HistogramData.count();

      float xdelta = minx + x * (maxx - minx);

      if(xdelta >= 0.0f && xdelta <= 1.0f)
      {
        maxval = qMax(maxval, m_HistogramData[i]);
      }
    }

    if(maxval == 0)
      maxval = 1;

    for(int i = 0; i < m_HistogramData.count(); i++)
    {
      float x = (float)i / (float)m_HistogramData.count();
      float y = (float)m_HistogramData[i] / (float)maxval;

      float xdelta = minx + x * (maxx - minx);

      if(xdelta >= 0.0f && xdelta <= 1.0f)
      {
        float segwidth = qMax(r.width() * (maxx - minx) / (float)m_HistogramData.count(), 1.0);

        QRectF barRect(QPointF(r.left() + r.width() * (minx + x * (maxx - minx)),
                               r.bottom() - r.height() * y + 1),
                       QSizeF(segwidth, r.height() * y));

        p.fillRect(barRect, greenBrush);
      }
    }
  }

  QVector<QPointF> blackTriangle = {QPoint(blackPoint.right(), m_MarkerSize * 2),
                                    QPoint(blackPoint.right() + m_MarkerSize, 0),
                                    QPoint(blackPoint.right() - m_MarkerSize, 0)};

  QPainterPath blackPath;
  blackPath.addPolygon(QPolygonF(blackTriangle));
  p.fillPath(blackPath, palette().brush(QPalette::Dark));

  QVector<QPointF> whiteTriangle = {
      QPoint(whitePoint.left(), whitePoint.bottom() - m_MarkerSize * 2 + m_Margin),
      QPoint(whitePoint.left() + m_MarkerSize, whitePoint.bottom() + m_Margin),
      QPoint(whitePoint.left() - m_MarkerSize, whitePoint.bottom() + m_Margin)};

  QPainterPath whitePath;
  whitePath.addPolygon(QPolygonF(whiteTriangle));
  p.fillPath(whitePath, palette().brush(QPalette::Dark));

  blackTriangle[0] -= QPointF(0.0, 2.0) / devicePixelRatioF();
  blackTriangle[1] += QPointF(-2.0, 1.0) / devicePixelRatioF();
  blackTriangle[2] += QPointF(2.0, 1.0) / devicePixelRatioF();

  blackPath = QPainterPath();
  blackPath.addPolygon(QPolygonF(blackTriangle));

  whiteTriangle[0] += QPointF(0.0, 2.0) / devicePixelRatioF();
  whiteTriangle[1] -= QPointF(2.0, 1.0) / devicePixelRatioF();
  whiteTriangle[2] += QPointF(2.0, -1.0) / devicePixelRatioF();

  whitePath = QPainterPath();
  whitePath.addPolygon(QPolygonF(whiteTriangle));

  p.fillPath(blackPath, blackBrush);
  p.fillPath(whitePath, whiteBrush);
}
