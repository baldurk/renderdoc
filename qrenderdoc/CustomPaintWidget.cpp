#include "CustomPaintWidget.h"
#include <QPainter>

#include "renderdoc_replay.h"

extern ReplayOutput *out;

CustomPaintWidget::CustomPaintWidget(QWidget *parent) : QWidget(parent)
{
  setAttribute(Qt::WA_PaintOnScreen);
}

CustomPaintWidget::~CustomPaintWidget()
{

}

void CustomPaintWidget::paintEvent(QPaintEvent *e)
{
  ReplayOutput_Display(out);
}

