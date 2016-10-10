#include "RDListView.h"
#include <QMouseEvent>

RDListView::RDListView(QWidget *parent) : QListView(parent)
{
  setMouseTracking(true);
}

void RDListView::mouseMoveEvent(QMouseEvent *e)
{
  emit mouseMove(e);

  if(indexAt(e->pos()).isValid() && m_Shape != Qt::ArrowCursor)
  {
    setCursor(QCursor(m_Shape));
  }
  else if(m_Shape != Qt::ArrowCursor)
  {
    unsetCursor();
  }

  QListView::mouseMoveEvent(e);
}
