#include "CustomPaintWidget.h"
#include <QPainter>

#include "renderdoc_replay.h"

CustomPaintWidget::CustomPaintWidget(QWidget *parent) : QWidget(parent)
{
  m_Output = NULL;
  setAttribute(Qt::WA_PaintOnScreen);
}

CustomPaintWidget::~CustomPaintWidget()
{

}

void CustomPaintWidget::paintEvent(QPaintEvent *e)
{
  if(m_Output) m_Output->Display();
}

