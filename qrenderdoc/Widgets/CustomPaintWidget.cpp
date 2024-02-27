/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include <QEvent>
#include <QPainter>
#include <QPointer>
#include <QVBoxLayout>
#include "Code/Interface/QRDInterface.h"
#include "Code/QRDUtils.h"

CustomPaintWidgetInternal::CustomPaintWidgetInternal(CustomPaintWidget &parentCustom, bool rendering)
    : m_Custom(parentCustom), m_Rendering(rendering)
{
  setAttribute(Qt::WA_OpaquePaintEvent);
  setMouseTracking(true);
  if(m_Rendering)
    setAttribute(Qt::WA_PaintOnScreen);
}

CustomPaintWidgetInternal::~CustomPaintWidgetInternal()
{
}

CustomPaintWidget::CustomPaintWidget(QWidget *parent) : QWidget(parent)
{
  m_Tag = QFormatStr("custompaint%1").arg((uintptr_t)this);

  setAttribute(Qt::WA_OpaquePaintEvent);

  m_Dark = Formatter::DarkCheckerColor();
  m_Light = Formatter::LightCheckerColor();

  QVBoxLayout *l = new QVBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(0);
  setLayout(l);

  RecreateInternalWidget();
}

CustomPaintWidget::~CustomPaintWidget()
{
  if(m_Ctx)
    m_Ctx->RemoveCaptureViewer(this);
}

void CustomPaintWidget::SetContext(ICaptureContext &ctx)
{
  if(m_Ctx)
    m_Ctx->RemoveCaptureViewer(this);

  m_Ctx = &ctx;
  m_Ctx->AddCaptureViewer(this);

  RecreateInternalWidget();
}

void CustomPaintWidget::OnCaptureLoaded()
{
  RecreateInternalWidget();
}

void CustomPaintWidget::OnCaptureClosed()
{
  // forget any output we used to have
  SetOutput(NULL);
}

void CustomPaintWidget::OnSelectedEventChanged(uint32_t eventId)
{
  // nothing, we only care about capture loaded/closed events
}

void CustomPaintWidget::OnEventChanged(uint32_t eventId)
{
  // if we've encountered a fatal error recreate the widget and take over painting
  if(m_Rendering && m_Ctx && !m_Ctx->GetFatalError().OK())
  {
    RecreateInternalWidget();
    update();
  }
}

void CustomPaintWidget::update()
{
  m_Internal->update();
  QWidget::update();
}

WindowingData CustomPaintWidget::GetWidgetWindowingData()
{
  // switch to rendering here and recreate the widget, so we have an updated winId for the windowing
  // data
  m_Rendering = true;
  RecreateInternalWidget();
  return m_Ctx->CreateWindowingData(m_Internal);
}

void CustomPaintWidget::SetOutput(IReplayOutput *out)
{
  m_Output = out;
  m_Rendering = (out != NULL);

  RecreateInternalWidget();
}

void CustomPaintWidget::RecreateInternalWidget()
{
  if(!GUIInvoke::onUIThread())
  {
    GUIInvoke::call(this, [this]() { RecreateInternalWidget(); });
    return;
  }

  // if no capture is loaded, or we've encountered a fatal error, we're not rendering anymore.
  m_Rendering = m_Rendering && m_Ctx && m_Ctx->IsCaptureLoaded() && m_Ctx->GetFatalError().OK();

  // we need to recreate the widget if it's not matching out rendering state.
  if(m_Internal == NULL || m_Rendering != m_Internal->IsRendering())
  {
    delete m_Internal;
    m_Internal = new CustomPaintWidgetInternal(*this, m_Rendering);

    layout()->addWidget(m_Internal);
  }
}

void CustomPaintWidget::changeEvent(QEvent *event)
{
  if(event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
  {
    m_Dark = Formatter::DarkCheckerColor();
    m_Light = Formatter::LightCheckerColor();
    update();
  }
}

void CustomPaintWidget::renderInternal(QPaintEvent *e)
{
  if(m_Ctx && m_Output && m_Ctx->IsCaptureLoaded())
  {
    QPointer<CustomPaintWidget> me(this);
    m_Ctx->Replay().AsyncInvoke(m_Tag, [me](IReplayController *r) {
      if(me && me->m_Output && me->m_Ctx->IsCaptureLoaded())
        me->m_Output->Display();
    });
  }
}

void CustomPaintWidget::paintInternal(QPaintEvent *e)
{
  if(m_BackCol.isValid())
  {
    QPainter p(m_Internal);
    p.fillRect(rect(), m_BackCol);
  }
  else
  {
    int numX = (int)ceil((float)rect().width() / 64.0f);
    int numY = (int)ceil((float)rect().height() / 64.0f);

    QPainter p(m_Internal);
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

void CustomPaintWidgetInternal::mousePressEvent(QMouseEvent *e)
{
  emit m_Custom.clicked(e);
}

void CustomPaintWidgetInternal::mouseDoubleClickEvent(QMouseEvent *event)
{
  emit m_Custom.doubleClicked(event);
}

void CustomPaintWidgetInternal::mouseMoveEvent(QMouseEvent *e)
{
  emit m_Custom.mouseMove(e);
}

void CustomPaintWidgetInternal::wheelEvent(QWheelEvent *e)
{
  emit m_Custom.mouseWheel(e);
}

void CustomPaintWidgetInternal::resizeEvent(QResizeEvent *e)
{
  emit m_Custom.resize(e);
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
  // don't paint this widget
}

void CustomPaintWidgetInternal::paintEvent(QPaintEvent *e)
{
  if(m_Rendering)
    m_Custom.renderInternal(e);
  else
    m_Custom.paintInternal(e);
}

#if defined(RENDERDOC_PLATFORM_APPLE)
bool CustomPaintWidgetInternal::event(QEvent *e)
{
  if(m_Rendering && e->type() == QEvent::UpdateRequest)
    paintEvent(NULL);
  return QWidget::event(e);
}
#endif
