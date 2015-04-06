#ifndef CUSTOMPAINTWIDGET_H
#define CUSTOMPAINTWIDGET_H

#include <QWidget>

struct IReplayOutput;

class CustomPaintWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit CustomPaintWidget(QWidget *parent = 0);
    ~CustomPaintWidget();

    void SetOutput(IReplayOutput *out) { m_Output = out; }

  signals:

  public slots:

  protected:
    void paintEvent(QPaintEvent *e);
    QPaintEngine *paintEngine() const { return NULL; }

    IReplayOutput *m_Output;
};

#endif // CUSTOMPAINTWIDGET_H
