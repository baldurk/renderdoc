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

#include <QFrame>

class PipelineFlowChart : public QFrame
{
  Q_OBJECT

public:
  explicit PipelineFlowChart(QWidget *parent = 0);
  ~PipelineFlowChart();

  const QStringList &stageAbbreviations() { return m_StageAbbrevs; }
  const QStringList &stageNames() { return m_StageNames; }
  void setStages(const QStringList &abbrevs, const QStringList &names);
  void setStageName(int index, const QString &abbrev, const QString &name);
  void setIsolatedStage(int index);

  void setStagesEnabled(const QList<bool> &enabled);

  void setSelectedStage(int idx);
  int selectedStage();

  QSize minimumSizeHint() const override;
  QSize sizeHint() const override;

signals:
  void stageSelected(int idx);

private:
  void paintEvent(QPaintEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void focusOutEvent(QFocusEvent *event) override;

  QList<bool> m_StagesEnabled;
  QList<bool> m_StageFlows;
  QStringList m_StageAbbrevs;
  QStringList m_StageNames;

  int m_HoverStage = -1;
  int m_SelectedStage = 0;

  bool stageEnabled(int index);

  const qreal BoxBorderWidth = 2.5;
  const qreal MinBoxDimension = 25.0;
  const qreal MaxBoxCornerRadius = 20.0;
  const qreal BoxCornerRadiusFraction = 1.0 / 6.0;
  const qreal ArrowHeadSize = 6.0;
  const qreal MinBoxMargin = 4.0;
  const qreal BoxLabelMargin = 8.0;
  const qreal BoxMarginFraction = 0.02;

  int numGaps();
  int numItems();

  QRectF totalAreaRect();
  qreal boxMargin();
  QRectF boxRect(int i);

  void drawArrow(QPainter &p, const QPen &pen, float headsize, float y, float left, float right);
};
