/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include <QWidget>

struct IReplayOutput;
struct ICaptureContext;

class CustomPaintWidget : public QWidget
{
private:
  Q_OBJECT
public:
  explicit CustomPaintWidget(QWidget *parent = 0);
  explicit CustomPaintWidget(ICaptureContext *c, QWidget *parent = 0);
  ~CustomPaintWidget();

  // this is needed to solve a chicken-and-egg problem. We need to recreate the widget
  // whenever we go from custom rendering to painting (e.g. capture loaded or closed). But
  // we need the widget to have been recreated before we create the output, so we can
  // pass in the winId.
  // So we go by whether or not we have a CaptureContext * and go on faith that the
  // output will be set before any painting work has to happen.
  void setOutput(IReplayOutput *out) { m_Output = out; }
  void setColours(QColor dark, QColor light)
  {
    m_Dark = dark;
    m_Light = light;
  }

signals:
  void clicked(QMouseEvent *e);
  void doubleClicked(QMouseEvent *e);
  void mouseMove(QMouseEvent *e);
  void resize(QResizeEvent *e);
  void mouseWheel(QWheelEvent *e);
  void keyPress(QKeyEvent *e);
  void keyRelease(QKeyEvent *e);

private:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;

public slots:

protected:
#if defined(RENDERDOC_PLATFORM_APPLE)
  bool event(QEvent *event) override;
#endif

  void paintEvent(QPaintEvent *e) override;
  QPaintEngine *paintEngine() const override { return m_Ctx ? NULL : QWidget::paintEngine(); }
  ICaptureContext *m_Ctx;
  IReplayOutput *m_Output;
  QString m_Tag;
  QColor m_Dark;
  QColor m_Light;
};
