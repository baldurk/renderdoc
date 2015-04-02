#ifndef CUSTOMPAINTWIDGET_H
#define CUSTOMPAINTWIDGET_H

#include <QWidget>

class CustomPaintWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit CustomPaintWidget(QWidget *parent = 0);
    ~CustomPaintWidget();

  signals:

  public slots:

  protected:
    void paintEvent(QPaintEvent *e);
    QPaintEngine *paintEngine() const { return NULL; }
};

#endif // CUSTOMPAINTWIDGET_H
