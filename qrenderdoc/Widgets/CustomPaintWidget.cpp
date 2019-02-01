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

#include "CustomPaintWidget.h"
#include <math.h>
#include <QPainter>
#include "Code/Interface/QRDInterface.h"

CustomPaintWidget::CustomPaintWidget(QWidget *parent) : QWidget(parent)
{
  m_Ctx = NULL;
  m_Output = NULL;
  setAttribute(Qt::WA_OpaquePaintEvent);
  setMouseTracking(true);
  m_Tag = QFormatStr("custompaint%1").arg((uintptr_t) this);
}

CustomPaintWidget::CustomPaintWidget(ICaptureContext *c, QWidget *parent) : QWidget(parent)
{
  m_Ctx = c;
  m_Output = NULL;
  setAttribute(Qt::WA_OpaquePaintEvent);
  if(c)
    setAttribute(Qt::WA_PaintOnScreen);
  setMouseTracking(true);
  m_Tag = QFormatStr("custompaint%1").arg((uintptr_t) this);
}

CustomPaintWidget::~CustomPaintWidget()
{
}

void CustomPaintWidget::mousePressEvent(QMouseEvent *e)
{
  emit clicked(e);
}

void CustomPaintWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  emit(doubleClicked(event));
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

void CustomPaintWidget::keyReleaseEvent(QKeyEvent *e)
{
  emit keyRelease(e);
}

void CustomPaintWidget::paintEvent(QPaintEvent *e)
{
  if(m_Ctx)
  {
    if(m_Output != NULL)
      m_Ctx->Replay().AsyncInvoke(m_Tag, [this](IReplayController *r) { m_Output->Display(); });
  }
  else if(m_Dark == m_Light)
  {
    QPainter p(this);
    p.fillRect(rect(), m_Dark);
  }
  else
  {
    int numX = (int)ceil((float)rect().width() / 64.0f);
    int numY = (int)ceil((float)rect().height() / 64.0f);

    QPainter p(this);
    for(int x = 0; x < numX; x++)
    {
      for(int y = 0; y < numY; y++)
      {
        QColor &col = ((x % 2) == (y % 2)) ? m_Dark : m_Light;

        p.fillRect(QRect(x * 64, y * 64, 64, 64), col);
      }
    }
  }
}

#if defined(RENDERDOC_PLATFORM_APPLE)
bool CustomPaintWidget::event(QEvent *e)
{
  if(m_Ctx && e->type() == QEvent::UpdateRequest)
    paintEvent(NULL);
  return QWidget::event(e);
}
#endif