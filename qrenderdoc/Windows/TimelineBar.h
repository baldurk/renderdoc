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

#pragma once

#include <QAbstractScrollArea>
#include "Code/Interface/QRDInterface.h"

class TimelineBar : public QAbstractScrollArea, public ITimelineBar, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit TimelineBar(ICaptureContext &ctx, QWidget *parent = 0);
  ~TimelineBar();

  QSize minimumSizeHint() const override;

  // IStatisticsViewer
  QWidget *Widget() override { return this; }
  void HighlightResourceUsage(ResourceId id) override;
  void HighlightHistory(ResourceId id, const rdcarray<PixelModification> &history) override;
  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void paintEvent(QPaintEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

private:
  ICaptureContext &m_Ctx;

  struct Marker
  {
    uint32_t eidStart = 0, eidEnd = 0;

    QString name;
    QColor color;
    bool expanded = false;

    QVector<Marker> children;
    QVector<uint32_t> draws;
  };

  QVector<Marker> m_RootMarkers;
  QVector<uint32_t> m_RootDraws;
  QVector<uint32_t> m_Draws;

  ResourceId m_ID;
  QString m_HistoryTarget;
  QList<PixelModification> m_HistoryEvents;

  QString m_UsageTarget;
  QList<EventUsage> m_UsageEvents;

  const qreal margin = 2.0;
  const qreal borderWidth = 1.0;
  const QString eidAxisTitle = lit("EID:");
  const int dataBarHeight = 18;
  const int highlightingExtra = 12;

  int m_eidAxisLabelStep = 0;
  qreal m_eidAxisLabelTextWidth = 0;
  qreal m_eidAxisLabelWidth = 0;
  qreal m_eidWidth = 0;

  QRectF m_area;
  QRectF m_dataArea;
  QRectF m_eidAxisRect;
  QRectF m_markerRect;
  QRectF m_highlightingRect;
  qreal m_titleWidth = 0;

  qreal m_zoom = 1.0;
  qreal m_pan = 0.0;
  QPointF m_lastPos;

  void layout();

  uint32_t eventAt(qreal x);
  qreal offsetOf(uint32_t eid);
  uint32_t processDraws(QVector<Marker> &markers, QVector<uint32_t> &draws,
                        const rdcarray<DrawcallDescription> &curDraws);
  void paintMarkers(QPainter &p, const QVector<Marker> &markers, const QVector<uint32_t> &draws,
                    QRectF markerRect);
  Marker *findMarker(QVector<Marker> &markers, QRectF markerRect, QPointF pos);
};
