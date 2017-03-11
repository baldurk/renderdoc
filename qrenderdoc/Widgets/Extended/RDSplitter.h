#ifndef RDSPLITTER_H
#define RDSPLITTER_H

#include <QEvent>
#include <QSplitter>
#include <QString>

// A custom event class the is posted by a QSplitter handle
// when double clicking on it
// it contains the index of the handle or -1 of the handle
// does not know its index
class RDSplitterDoubleClickEvent : public QEvent
{
public:
  RDSplitterDoubleClickEvent(int index);

  // returns the index of the handle that posted the event
  // or -1 if the handle does not know its index
  int index() const;

  // registers the event in the Qt Event system
  static QEvent::Type staticType();

protected:
  int m_index;
};

// a splitter handle.
// it draws a text as the title and "..." if it's collapsed
// You need to set a title and its index.
class RDSplitterHandle : public QSplitterHandle
{
  Q_OBJECT

public:
  RDSplitterHandle(Qt::Orientation orientation, QSplitter *parent);

  void setIndex(int index);
  int index() const;

  void setTitle(const QString &title);
  const QString &title() const;

  void setCollapsed(bool collapsed);
  bool collapsed() const;

protected:
  virtual void paintEvent(QPaintEvent *event);
  virtual void mouseDoubleClickEvent(QMouseEvent *event);

  QString m_title;
  int m_index;
  bool m_isCollapsed;
};

// A Splitter that contains RDSplitterHandles
// when setting up, you need to get the handles for every index
// and set their title as well as their indexes
class RDSplitter : public QSplitter
{
  Q_OBJECT

public:
  RDSplitter(QWidget *parent = 0);
  RDSplitter(Qt::Orientation orientation, QWidget *parent = 0);

protected slots:
  void setHandleCollapsed(int pos, int index);

protected:
  void initialize();
  virtual QSplitterHandle *createHandle();

  virtual bool event(QEvent *event);
};

#endif    // RDSPLITTER_H
