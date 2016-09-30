#include "CustomPaintWidget.h"
#include <QPainter>
#include "Code/Core.h"
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

void CustomPaintWidget::paintEvent(QPaintEvent *e)
{
  if(m_Output)
  {
    m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *r) { m_Output->Display(); });
  }
  else
  {
    QPainter p(this);
    p.setBrush(QBrush(Qt::black));
    p.drawRect(rect());
  }
}
