#pragma once

#include <QWidget>

struct IReplayOutput;
class Core;

class CustomPaintWidget : public QWidget
{
private:
  Q_OBJECT
public:
  explicit CustomPaintWidget(QWidget *parent = 0);
  ~CustomPaintWidget();

  void SetOutput(Core *c, IReplayOutput *out)
  {
    m_Core = c;
    m_Output = out;
  }
signals:
  void clicked(QMouseEvent *e);
  void mouseMove(QMouseEvent *e);
  void resize(QResizeEvent *e);
  void mouseWheel(QWheelEvent *e);

private slots:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

public slots:

protected:
  void paintEvent(QPaintEvent *e);
  QPaintEngine *paintEngine() const { return NULL; }
  Core *m_Core;
  IReplayOutput *m_Output;
};
