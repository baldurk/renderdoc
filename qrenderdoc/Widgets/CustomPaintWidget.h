#pragma once

#include <QWidget>

struct IReplayOutput;
class CaptureContext;

class CustomPaintWidget : public QWidget
{
private:
  Q_OBJECT
public:
  explicit CustomPaintWidget(QWidget *parent = 0);
  ~CustomPaintWidget();

  void SetOutput(CaptureContext *c, IReplayOutput *out)
  {
    m_Ctx = c;
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
  CaptureContext *m_Ctx;
  IReplayOutput *m_Output;
};
