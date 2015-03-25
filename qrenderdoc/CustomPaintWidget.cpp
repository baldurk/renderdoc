#include "CustomPaintWidget.h"
#include <QPainter>

#include "renderdoc_replay.h"

extern ReplayOutput *out;
extern TextureDisplay d;

CustomPaintWidget::CustomPaintWidget(QWidget *parent) : QWidget(parent)
{
  setAttribute(Qt::WA_PaintOnScreen);
}

CustomPaintWidget::~CustomPaintWidget()
{

}

void CustomPaintWidget::paintEvent(QPaintEvent *e)
{
  static float t = 0.0f;
  t += 0.01f;
  d.scale = 1.5f + sinf(t);
  ReplayOutput_SetTextureDisplay(out, d);
  ReplayOutput_Display(out);
}

