/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "CustomPaintWidget.h"
#include <QPainter>
#include "Code/CaptureContext.h"
#include "renderdoc_replay.h"

CustomPaintWidget::CustomPaintWidget(QWidget *parent) : QWidget(parent)
{
  m_Output = NULL;
  setAttribute(Qt::WA_PaintOnScreen);
  setMouseTracking(true);
}

CustomPaintWidget::~CustomPaintWidget()
{
}

void CustomPaintWidget::mousePressEvent(QMouseEvent *e)
{
  emit clicked(e);
}

void CustomPaintWidget::mouseMoveEvent(QMouseEvent *e)
{
  emit mouseMove(e);
}

void CustomPaintWidget::wheelEvent(QWheelEvent *e)
{
  emit mouseWheel(e);
}

void CustomPaintWidget::resizeEvent(QResizeEvent *e)
{
  emit resize(e);
}

void CustomPaintWidget::keyPressEvent(QKeyEvent *e)
{
  emit keyPress(e);
}

void CustomPaintWidget::paintEvent(QPaintEvent *e)
{
  if(m_Output)
  {
    m_Ctx->Renderer()->AsyncInvoke([this](IReplayRenderer *r) { m_Output->Display(); });
  }
  else
  {
    QPainter p(this);
    p.setBrush(QBrush(Qt::black));
    p.drawRect(rect());
  }
}
