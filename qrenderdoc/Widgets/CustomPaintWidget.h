/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "Code/Interface/QRDInterface.h"

class CustomPaintWidget;

// this is the internal widget that gets recreated
class CustomPaintWidgetInternal : public QWidget
{
private:
  Q_OBJECT

  CustomPaintWidget &m_Custom;
  bool m_Rendering = false;

public:
  explicit CustomPaintWidgetInternal(CustomPaintWidget &parentCustom, bool rendering);
  ~CustomPaintWidgetInternal();

  bool IsRendering() const { return m_Rendering; }
protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

#if defined(RENDERDOC_PLATFORM_APPLE)
  bool event(QEvent *event) override;
#endif

  void paintEvent(QPaintEvent *e) override;
  QPaintEngine *paintEngine() const override { return m_Rendering ? NULL : QWidget::paintEngine(); }
};

// this is the public-facing widget which is persistent and contains & recreates the internal widget
class CustomPaintWidget : public QWidget, ICaptureViewer
{
private:
  Q_OBJECT
public:
  explicit CustomPaintWidget(QWidget *parent = 0);
  ~CustomPaintWidget();

  void SetContext(ICaptureContext &ctx);

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override;
  void OnEventChanged(uint32_t eventId) override;

  void update();

  WindowingData GetWidgetWindowingData();
  void SetOutput(IReplayOutput *out);
  void SetBackCol(QColor col) { m_BackCol = col; }
signals:
  void clicked(QMouseEvent *e);
  void doubleClicked(QMouseEvent *e);
  void mouseMove(QMouseEvent *e);
  void resize(QResizeEvent *e);
  void mouseWheel(QWheelEvent *e);
  void keyPress(QKeyEvent *e);
  void keyRelease(QKeyEvent *e);

private:
  void changeEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;
  void paintEvent(QPaintEvent *e) override;

  QPaintEngine *paintEngine() const override { return NULL; }
  friend class CustomPaintWidgetInternal;

  CustomPaintWidgetInternal *m_Internal = NULL;

  bool m_Rendering = false;

  void RecreateInternalWidget();
  void renderInternal(QPaintEvent *e);
  void paintInternal(QPaintEvent *e);

  ICaptureContext *m_Ctx = NULL;
  IReplayOutput *m_Output = NULL;
  QString m_Tag;
  QColor m_Dark;
  QColor m_Light;
  QColor m_BackCol;
};
