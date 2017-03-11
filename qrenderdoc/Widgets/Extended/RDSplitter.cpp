#include "RDSplitter.h"

#include <QApplication>
#include <QPaintEvent>
#include <QPainter>

RDSplitterDoubleClickEvent::RDSplitterDoubleClickEvent(int index)
    : QEvent(staticType()), m_index(index)
{
}

int RDSplitterDoubleClickEvent::index() const
{
  return m_index;
}

QEvent::Type RDSplitterDoubleClickEvent::staticType()
{
  static int type = registerEventType();
  return (QEvent::Type)type;
}

RDSplitterHandle::RDSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent), m_index(-1), m_isCollapsed(false)
{
}

void RDSplitterHandle::setIndex(int index)
{
  m_index = index;
}

int RDSplitterHandle::index() const
{
  return m_index;
}

void RDSplitterHandle::setTitle(const QString &title)
{
  m_title = title;
}

const QString &RDSplitterHandle::title() const
{
  return m_title;
}

void RDSplitterHandle::setCollapsed(bool collapsed)
{
  m_isCollapsed = collapsed;
}

bool RDSplitterHandle::collapsed() const
{
  return m_isCollapsed;
}

void RDSplitterHandle::paintEvent(QPaintEvent *event)
{
  QPainter painter(this);
  painter.setPen(Qt::black);

  QString text(m_title);
  if(m_isCollapsed)
    text.append(" ...");

  painter.drawText(event->rect(), Qt::AlignCenter | Qt::TextWordWrap, text);
}

void RDSplitterHandle::mouseDoubleClickEvent(QMouseEvent *event)
{
  // post the event to the parent
  RDSplitterDoubleClickEvent *rdEvent = new RDSplitterDoubleClickEvent(m_index);
  QApplication::postEvent(parent(), rdEvent);
}

RDSplitter::RDSplitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent)
{
  initialize();
}

RDSplitter::RDSplitter(QWidget *parent) : QSplitter(parent)
{
  initialize();
}

void RDSplitter::setHandleCollapsed(int pos, int index)
{
  QList<int> totalSizes = sizes();
  RDSplitterHandle *rdHandle = static_cast<RDSplitterHandle *>(handle(index));
  if(totalSizes[index] == 0)
    rdHandle->setCollapsed(true);
  else
    rdHandle->setCollapsed(false);
}

void RDSplitter::initialize()
{
  connect(this, SIGNAL(splitterMoved(int, int)), this, SLOT(setHandleCollapsed(int, int)));
}

QSplitterHandle *RDSplitter::createHandle()
{
  return new RDSplitterHandle(orientation(), this);
}

bool RDSplitter::event(QEvent *event)
{
  if(event->type() == RDSplitterDoubleClickEvent::staticType())
  {
    RDSplitterDoubleClickEvent *rdEvent = static_cast<RDSplitterDoubleClickEvent *>(event);
    int index = rdEvent->index();
    RDSplitterHandle *rdHandle = static_cast<RDSplitterHandle *>(handle(index));
    QList<int> totalSizes = sizes();
    if(totalSizes[index] > 0)
    {
      // add to the previous handle the size of the current one
      totalSizes[index - 1] += totalSizes[index];
      // set the current handle's size to 0
      totalSizes[index] = 0;
      rdHandle->setCollapsed(true);
    }
    else
    {
      // split the sizes in half
      int s = totalSizes[index - 1] / 2;
      totalSizes[index] = totalSizes[index - 1] = s;
      rdHandle->setCollapsed(false);
    }
    setSizes(totalSizes);
    return true;
  }

  return QSplitter::event(event);
}
