#pragma once
#include <QLineEdit>

class RDLineEdit : public QLineEdit
{
private:
  Q_OBJECT
public:
  explicit RDLineEdit(QWidget *parent = 0);
  ~RDLineEdit();

signals:
  void enter();
  void leave();

public slots:

protected:
  void focusInEvent(QFocusEvent *e);
  void focusOutEvent(QFocusEvent *e);
};
