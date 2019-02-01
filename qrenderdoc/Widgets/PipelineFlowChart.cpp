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

#include "PipelineFlowChart.h"
#include <QMouseEvent>
#include <QPainter>

PipelineFlowChart::PipelineFlowChart(QWidget *parent) : QFrame(parent)
{
  setMouseTracking(true);
}

PipelineFlowChart::~PipelineFlowChart()
{
}

void PipelineFlowChart::setStages(const QStringList &abbrevs, const QStringList &names)
{
  m_StageNames = names;
  m_StageAbbrevs = abbrevs;
  m_StageFlows.reserve(m_StageNames.count());
  m_StagesEnabled.reserve(m_StageNames.count());
  for(int i = 0; i < m_StageNames.count(); i++)
  {
    m_StageFlows.push_back(true);
    m_StagesEnabled.push_back(true);
  }

  update();
}

void PipelineFlowChart::setStageName(int index, const QString &abbrev, const QString &name)
{
  if(index >= 0 && index < m_StageNames.count())
  {
    m_StageAbbrevs[index] = abbrev;
    m_StageNames[index] = name;
  }

  update();
}

void PipelineFlowChart::setIsolatedStage(int index)
{
  if(index >= 0 && index < m_StageFlows.count())
    m_StageFlows[index] = false;
}

void PipelineFlowChart::setSelectedStage(int idx)
{
  if(idx >= 0 && idx < m_StageNames.length())
  {
    m_SelectedStage = idx;
    update();

    emit stageSelected(idx);
  }
}

int PipelineFlowChart::selectedStage()
{
  return m_SelectedStage;
}

QSize PipelineFlowChart::minimumSizeHint() const
{
  return QSize(0, 30);
}

QSize PipelineFlowChart::sizeHint() const
{
  return QSize(0, 60);
}

void PipelineFlowChart::setStagesEnabled(const QList<bool> &enabled)
{
  for(int i = 0; i < enabled.count() && i < m_StagesEnabled.count(); i++)
    m_StagesEnabled[i] = enabled[i];

  update();
}

bool PipelineFlowChart::stageEnabled(int index)
{
  return index >= 0 && index < m_StagesEnabled.count() && m_StagesEnabled[index];
}

int PipelineFlowChart::numGaps()
{
  if(!m_StageNames.isEmpty())
    return m_StageNames.count() - 1;

  return 1;
}

int PipelineFlowChart::numItems()
{
  if(!m_StageNames.isEmpty())
    return m_StageNames.count();

  return 2;
}

QRectF PipelineFlowChart::totalAreaRect()
{
  QRectF rect = this->rect();

  qreal m = 6 + BoxBorderWidth;

  return rect.marginsRemoved(QMarginsF(m, m, m, m));
}

qreal PipelineFlowChart::boxMargin()
{
  qreal margin = qMax(totalAreaRect().width(), totalAreaRect().height()) * BoxMarginFraction;

  margin = qMax(MinBoxMargin, margin);

  return margin;
}

QRectF PipelineFlowChart::boxRect(int i)
{
  QRectF totalRect = totalAreaRect();

  qreal boxeswidth = totalRect.width() - numGaps() * boxMargin();

  qreal boxdim = qMin(totalRect.height(), boxeswidth / numItems());

  boxdim = qMax(MinBoxDimension, boxdim);

  qreal oblongwidth = qMax(0.0, (boxeswidth - boxdim * numItems()) / numItems());

  QSizeF boxSize(boxdim + oblongwidth, boxdim);

  QRectF ret(totalRect.x() + i * (boxSize.width() + boxMargin()),
             totalRect.y() + totalRect.height() / 2 - boxSize.height() / 2, boxSize.width(),
             boxSize.height());

  return ret;
}

void PipelineFlowChart::drawArrow(QPainter &p, const QPen &pen, float headsize, float y, float left,
                                  float right)
{
  p.setPen(pen);

  p.drawPolygon(QPolygonF({QPointF(right, y), QPointF(right - headsize, y - headsize),
                           QPointF(right - headsize, y + headsize)}));

  QPen linePen(pen);
  linePen.setWidthF(BoxBorderWidth);
  p.setPen(linePen);

  p.drawLine(QPointF(left, y), QPointF(right, y));
}

void PipelineFlowChart::paintEvent(QPaintEvent *e)
{
  if(m_StageNames.empty())
    return;

  QPainter p(this);

  p.fillRect(rect(), Qt::transparent);

  p.setRenderHint(QPainter::Antialiasing, true);

  const QRectF totalRect = totalAreaRect();
  const QRectF box0Rect = boxRect(0);

  const qreal radius = qMin(MaxBoxCornerRadius, box0Rect.height() * BoxCornerRadiusFraction);

  const qreal arrowY = totalRect.y() + totalRect.height() / 2;

  QColor enabledBase = palette().color(QPalette::Base);
  QColor enabledText = palette().color(QPalette::Text);
  QColor disabledBase = palette().color(QPalette::Disabled, QPalette::Base);
  QColor disabledText = palette().color(QPalette::Disabled, QPalette::Text);
  QColor tooltip = palette().color(QPalette::ToolTipBase);
  QColor tooltipText = palette().color(QPalette::ToolTipText);

  QPen pen(enabledText);
  QPen selectedPen(Qt::red);

  int num = numGaps();
  for(int i = 0; i < num; i++)
  {
    if(!m_StageFlows[i] || !m_StageFlows[i + 1])
      continue;

    float right = totalRect.x() + (i + 1) * (box0Rect.width() + boxMargin());
    float left = right - boxMargin();

    p.setBrush(enabledText);
    drawArrow(p, pen, ArrowHeadSize, arrowY, left, right);
  }

  num = numItems();
  for(int i = 0; i < num; i++)
  {
    QRectF boxrect = boxRect(i);

    QBrush backBrush(enabledBase);
    QPen textPen(enabledText);
    QPen outlinePen(enabledText);

    if(!stageEnabled(i))
    {
      backBrush.setColor(disabledBase);
      outlinePen.setColor(disabledText);
    }

    if(i == m_HoverStage)
    {
      backBrush.setColor(tooltip);
      textPen.setColor(tooltipText);
    }

    if(i == m_SelectedStage)
    {
      outlinePen = selectedPen;
    }

    outlinePen.setWidthF(BoxBorderWidth);

    p.setPen(outlinePen);
    p.setBrush(backBrush);
    p.drawRoundedRect(boxrect, radius, radius);

    QTextOption opts(Qt::AlignCenter);
    opts.setWrapMode(QTextOption::NoWrap);

    QString s = m_StageNames[i];

    QRectF reqBox = p.boundingRect(QRectF(0, 0, 1, 1), m_StageNames[i], opts);

    if(reqBox.width() + BoxLabelMargin > (float)boxrect.width())
      s = m_StageAbbrevs[i];

    p.setPen(textPen);
    p.drawText(boxrect, s, opts);
  }
}

void PipelineFlowChart::mouseMoveEvent(QMouseEvent *e)
{
  int old = m_HoverStage;
  m_HoverStage = -1;

  const int num = numItems();
  for(int i = 0; i < num; i++)
  {
    if(boxRect(i).contains(e->pos()))
    {
      m_HoverStage = i;
      break;
    }
  }

  if(m_HoverStage != old)
    update();
}

void PipelineFlowChart::mousePressEvent(QMouseEvent *e)
{
  if(e->button() == Qt::LeftButton)
  {
    const int num = numItems();
    for(int i = 0; i < numItems(); i++)
    {
      if(boxRect(i).contains(e->pos()))
      {
        setSelectedStage(i);
        break;
      }
    }
  }
}

void PipelineFlowChart::leaveEvent(QEvent *e)
{
  m_HoverStage = -1;

  update();
}

void PipelineFlowChart::focusOutEvent(QFocusEvent *event)
{
  m_HoverStage = -1;

  update();
}
