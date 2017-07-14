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

#pragma once

#include <QAbstractScrollArea>
#include "Code/CaptureContext.h"

class TimelineBar : public QAbstractScrollArea, public ITimelineBar, public ILogViewer
{
  Q_OBJECT

public:
  explicit TimelineBar(ICaptureContext &ctx, QWidget *parent = 0);
  ~TimelineBar();

  QSize minimumSizeHint() const override;

  // IStatisticsViewer
  QWidget *Widget() override { return this; }
  void HighlightResourceUsage(ResourceId id) override;
  void HighlightHistory(ResourceId id, const QList<PixelModification> &history) override;
  // ILogViewerForm
  void OnLogfileLoaded() override;
  void OnLogfileClosed() override;
  void OnSelectedEventChanged(uint32_t eventID) override {}
  void OnEventChanged(uint32_t eventID) override;

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void paintEvent(QPaintEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

private:
  ICaptureContext &m_Ctx;

  QVector<uint32_t> m_Events;

  const qreal margin = 2.0;
  const qreal borderWidth = 1.0;
  const QString eidAxisTitle = lit("EID:");
  const int eidAxisHeight = 18;

  qreal m_leftCoord = 0;
  qreal m_totalSize = 0;

  int m_eidAxisLabelStep = 0;
  qreal m_eidAxisLabelTextWidth = 0;
  qreal m_eidAxisLabelWidth = 0;

  qreal m_zoom = 1.0;
  qreal m_pan = 0.0;
  QPointF m_lastPos;

  void layout();

  uint32_t eventAt(qreal x);
  qreal offsetOf(uint32_t eid);
  void addEvents(const rdctype::array<DrawcallDescription> &curDraws);
};
