#include "RDLabel.h"

RDLabel::RDLabel(QWidget *parent) : QLabel(parent)
{
}

RDLabel::~RDLabel()
{
}

void RDLabel::mousePressEvent(QMouseEvent *event)
{
  emit(clicked());
}
