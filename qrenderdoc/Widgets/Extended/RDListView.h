#ifndef RDLISTVIEW_H
#define RDLISTVIEW_H

#include <QListView>

class RDListView : public QListView
{
  Q_OBJECT
public:
  explicit RDListView(QWidget *parent = 0);

  void setHoverCursor(Qt::CursorShape shape) { m_Shape = shape; }
signals:
  void mouseMove(QMouseEvent *e);

public slots:

private:
  void mouseMoveEvent(QMouseEvent *e) override;

  Qt::CursorShape m_Shape = Qt::ArrowCursor;
};

#endif    // RDLISTVIEW_H
