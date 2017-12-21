/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Pavel Strakhov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#ifndef TOOLWINDOWMANAGERWRAPPER_H
#define TOOLWINDOWMANAGERWRAPPER_H

#include <QIcon>
#include <QVariantMap>
#include <QWidget>

class ToolWindowManager;
class QLabel;

/*!
 * \brief The ToolWindowManagerWrapper class is used by ToolWindowManager to wrap its content.
 * One wrapper is a direct child of the manager and contains tool windows that are inside its
 * window.
 * All other wrappers are top level floating windows that contain detached tool windows.
 *
 */
class ToolWindowManagerWrapper : public QWidget
{
  Q_OBJECT
public:
  //! Creates new wrapper.
  explicit ToolWindowManagerWrapper(ToolWindowManager *manager, bool floating);
  //! Removes the wrapper.
  virtual ~ToolWindowManagerWrapper();

  ToolWindowManager *manager() { return m_manager; }
  bool floating() { return m_floating; }
  void updateTitle();

protected:
  //! Reimplemented to register hiding of contained tool windows when user closes the floating
  //! window.
  virtual void closeEvent(QCloseEvent *) Q_DECL_OVERRIDE;

  //! Event filter for grabbing and processing mouse drags as toolwindow drags.
  virtual bool eventFilter(QObject *object, QEvent *event) Q_DECL_OVERRIDE;

  //! Painting and resizing for custom-rendered widget frames
  virtual void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE;
  virtual void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;

private:
  ToolWindowManager *m_manager;

  enum class ResizeDirection
  {
    NW,
    NE,
    SW,
    SE,
    N,
    E,
    S,
    W,
    Count,
  };

  QRect titleRect();
  ResizeDirection checkResize();

  QRect m_closeRect;
  QIcon m_closeIcon;
  int m_closeButtonSize;
  int m_titleHeight;
  int m_frameWidth;
  bool m_floating;

  QTimer *m_moveTimeout;

  bool m_dragReady;             // we've clicked and started moving but haven't moved enough yet
  QPoint m_dragStartCursor;     // cursor at the click to start a drag
  QRect m_dragStartGeometry;    // window geometry at the click to start a drag
  bool m_dragActive;            // whether a drag currently on-going
  ResizeDirection m_dragDirection;    // the current direction being dragged

  // dump content's layout to variable
  QVariantMap saveState();

  // construct layout based on given dump
  void restoreState(const QVariantMap &data);

  friend class ToolWindowManager;

private slots:
  void moveTimeout();
};

#endif    // TOOLWINDOWMANAGERWRAPPER_H
