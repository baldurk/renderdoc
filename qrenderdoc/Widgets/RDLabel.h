#pragma once
#include <QLabel>

class RDLabel : public QLabel
{
private:
  Q_OBJECT
public:
  explicit RDLabel(QWidget *parent = 0);
  ~RDLabel();

signals:
  void clicked();

public slots:

protected:
  void mousePressEvent(QMouseEvent *event);
};
