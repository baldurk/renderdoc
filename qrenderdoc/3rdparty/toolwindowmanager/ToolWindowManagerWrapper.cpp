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
#include "ToolWindowManagerWrapper.h"
#include <QApplication>
#include <QDebug>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QSplitter>
#include <QStyleOption>
#include <QStylePainter>
#include <QTimer>
#include <QVBoxLayout>
#include "ToolWindowManager.h"
#include "ToolWindowManagerArea.h"

ToolWindowManagerWrapper::ToolWindowManagerWrapper(ToolWindowManager *manager, bool floating)
    : QWidget(manager), m_manager(manager)
{
  Qt::WindowFlags flags = Qt::Tool;

#if defined(Q_OS_WIN32)
  flags = Qt::Dialog;
  flags |= Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint |
           Qt::WindowMaximizeButtonHint;
#else
  flags |= Qt::FramelessWindowHint;
#endif

  setMouseTracking(true);

  setWindowFlags(flags);
  setWindowTitle(QStringLiteral(" "));

  m_dragReady = false;
  m_dragActive = false;
  m_dragDirection = ResizeDirection::Count;

  m_floating = floating;

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  m_manager->m_wrappers << this;

  m_moveTimeout = new QTimer(this);
  m_moveTimeout->setInterval(100);
  m_moveTimeout->stop();
  QObject::connect(m_moveTimeout, &QTimer::timeout, this, &ToolWindowManagerWrapper::moveTimeout);

  m_closeButtonSize = 0;
  m_frameWidth = 0;
  m_titleHeight = 0;

  if(floating && (flags & Qt::FramelessWindowHint))
  {
    m_closeButtonSize = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);

    QFontMetrics titleFontMetrics = fontMetrics();
    int mw = style()->pixelMetric(QStyle::PM_DockWidgetTitleMargin, 0, this);

    m_titleHeight = qMax(m_closeButtonSize + 2, titleFontMetrics.height() + 2 * mw);

    m_frameWidth = style()->pixelMetric(QStyle::PM_DockWidgetFrameWidth, 0, this);

    mainLayout->setContentsMargins(QMargins(m_frameWidth + 4, m_frameWidth + 4 + m_titleHeight,
                                            m_frameWidth + 4, m_frameWidth + 4));
  }

  if(floating)
  {
    installEventFilter(this);
    updateTitle();
  }
}

ToolWindowManagerWrapper::~ToolWindowManagerWrapper()
{
  m_manager->m_wrappers.removeOne(this);
}

void ToolWindowManagerWrapper::updateTitle()
{
  if(!m_floating)
    return;

  // find the best candidate for a 'title' for this floating window.
  if(layout()->count() > 0)
  {
    QWidget *child = layout()->itemAt(0)->widget();

    while(child)
    {
      // if we've found an area, use its currently selected tab's text
      if(ToolWindowManagerArea *area = qobject_cast<ToolWindowManagerArea *>(child))
      {
        setWindowTitle(area->tabText(area->currentIndex()));
        return;
      }
      // otherwise we should have a splitter
      if(QSplitter *splitter = qobject_cast<QSplitter *>(child))
      {
        // if it's empty, just bail
        if(splitter->count() == 0)
          break;

        // if it's vertical, we pick the first child and recurse
        if(splitter->orientation() == Qt::Vertical)
        {
          child = splitter->widget(0);
          continue;
        }

        // if it's horizontal there's ambiguity so we just pick the biggest one by size, with a
        // tie-break for the leftmost one
        QList<int> sizes = splitter->sizes();
        int maxIdx = 0;
        int maxSize = sizes[0];
        for(int i = 1; i < sizes.count(); i++)
        {
          if(sizes[i] > maxSize)
          {
            maxSize = sizes[i];
            maxIdx = i;
          }
        }

        child = splitter->widget(maxIdx);
        continue;
      }

      // if not, use this object's window title
      setWindowTitle(child->windowTitle());
      return;
    }
  }

  setWindowTitle(QStringLiteral("Tool Window"));
}

void ToolWindowManagerWrapper::closeEvent(QCloseEvent *event)
{
  // abort dragging caused by QEvent::NonClientAreaMouseButtonPress in eventFilter function
  m_manager->abortDrag();

  QList<QWidget *> toolWindows;
  foreach(ToolWindowManagerArea *tabWidget, findChildren<ToolWindowManagerArea *>())
  {
    if(ToolWindowManager::managerOf(tabWidget) == m_manager)
    {
      toolWindows << tabWidget->toolWindows();
    }
  }

  foreach(QWidget *toolWindow, toolWindows)
  {
    if(!m_manager->allowClose(toolWindow))
    {
      event->ignore();
      return;
    }
  }

  foreach(QWidget *toolWindow, toolWindows)
  {
    if(m_manager->toolWindowProperties(toolWindow) & ToolWindowManager::HideOnClose)
      m_manager->hideToolWindow(toolWindow);
    else
      m_manager->removeToolWindow(toolWindow, true);
  }
}

bool ToolWindowManagerWrapper::eventFilter(QObject *object, QEvent *event)
{
  const Qt::CursorShape shapes[(int)ResizeDirection::Count] = {
      Qt::SizeFDiagCursor, Qt::SizeBDiagCursor, Qt::SizeBDiagCursor, Qt::SizeFDiagCursor,
      Qt::SizeVerCursor,   Qt::SizeHorCursor,   Qt::SizeVerCursor,   Qt::SizeHorCursor,
  };

  if(object == this)
  {
    if(event->type() == QEvent::MouseButtonRelease ||
       event->type() == QEvent::NonClientAreaMouseButtonRelease)
    {
      m_dragReady = false;
      m_dragDirection = ResizeDirection::Count;
      if(!m_dragActive && m_closeRect.contains(mapFromGlobal(QCursor::pos())))
      {
        // catch clicks on the close button
        close();
      }
      else
      {
        // if the mouse button is released, let the manager finish the drag and don't call any more
        // updates for any further move events
        m_dragActive = false;
        m_manager->updateDragPosition();
      }
    }
    else if(event->type() == QEvent::MouseMove || event->type() == QEvent::NonClientAreaMouseMove)
    {
      // if we're ready to start a drag, check how far we've moved and start the drag if past a
      // certain pixel threshold.
      if(m_dragReady)
      {
        if((QCursor::pos() - m_dragStartCursor).manhattanLength() > 10)
        {
          m_dragActive = true;
          m_dragReady = false;
          QList<QWidget *> toolWindows;
          foreach(ToolWindowManagerArea *tabWidget, findChildren<ToolWindowManagerArea *>())
          {
            if(ToolWindowManager::managerOf(tabWidget) == m_manager)
            {
              toolWindows << tabWidget->toolWindows();
            }
          }
          m_manager->startDrag(toolWindows, this);
        }
      }
      // if the drag is active, update it in the manager.
      if(m_dragActive)
      {
        m_manager->updateDragPosition();

// on non-windows we have no native title bar, so we need to move the window ourselves
#if !defined(Q_OS_WIN32)
        move(QCursor::pos() - (m_dragStartCursor - m_dragStartGeometry.topLeft()));
#endif
      }
      if(titleRect().contains(mapFromGlobal(QCursor::pos())))
      {
        // if we're in the title bar, repaint to pick up motion over the close button
        update();
      }

      ResizeDirection dir = checkResize();

      if(m_dragDirection != ResizeDirection::Count)
      {
        dir = m_dragDirection;

        QRect g = geometry();

        switch(dir)
        {
          case ResizeDirection::NW: g.setTopLeft(QCursor::pos()); break;
          case ResizeDirection::NE: g.setTopRight(QCursor::pos()); break;
          case ResizeDirection::SW: g.setBottomLeft(QCursor::pos()); break;
          case ResizeDirection::SE: g.setBottomRight(QCursor::pos()); break;
          case ResizeDirection::N: g.setTop(QCursor::pos().y()); break;
          case ResizeDirection::E: g.setRight(QCursor::pos().x()); break;
          case ResizeDirection::S: g.setBottom(QCursor::pos().y()); break;
          case ResizeDirection::W: g.setLeft(QCursor::pos().x()); break;
          case ResizeDirection::Count: break;
        }

        setGeometry(g);
      }

      if(dir != ResizeDirection::Count)
      {
        setCursor(shapes[(int)dir]);

        QObjectList children = this->children();
        for(int i = 0; i < children.size(); ++i)
        {
          if(QWidget *w = qobject_cast<QWidget *>(children.at(i)))
          {
            if(!w->testAttribute(Qt::WA_SetCursor))
            {
              w->setCursor(Qt::ArrowCursor);
            }
          }
        }
      }
      else
      {
        unsetCursor();
      }
    }
    else if(event->type() == QEvent::MouseButtonPress)
    {
      ResizeDirection dir = checkResize();
      m_dragStartCursor = QCursor::pos();
      m_dragStartGeometry = geometry();
      if(dir == ResizeDirection::Count)
        m_dragReady = true;
      else
        m_dragDirection = dir;
    }
    else if(event->type() == QEvent::NonClientAreaMouseButtonPress)
    {
      m_dragActive = true;
      m_dragReady = false;
      m_dragStartCursor = QCursor::pos();
      m_dragStartGeometry = geometry();
      QList<QWidget *> toolWindows;
      foreach(ToolWindowManagerArea *tabWidget, findChildren<ToolWindowManagerArea *>())
      {
        if(ToolWindowManager::managerOf(tabWidget) == m_manager)
        {
          toolWindows << tabWidget->toolWindows();
        }
      }
      m_manager->startDrag(toolWindows, this);
    }
    else if(event->type() == QEvent::Move && m_dragActive)
    {
      m_manager->updateDragPosition();
      m_moveTimeout->start();
    }
    else if(event->type() == QEvent::Leave)
    {
      unsetCursor();
    }
    else if(event->type() == QEvent::MouseButtonDblClick &&
            titleRect().contains(mapFromGlobal(QCursor::pos())))
    {
      if(isMaximized())
      {
        showNormal();
      }
      else
      {
        showMaximized();
      }
    }
    else if(event->type() == QEvent::NonClientAreaMouseButtonDblClick)
    {
      if(isMaximized())
      {
        showNormal();
      }
      else
      {
        showMaximized();
      }
    }
  }
  return QWidget::eventFilter(object, event);
}

void ToolWindowManagerWrapper::paintEvent(QPaintEvent *)
{
  if(!m_floating || m_titleHeight == 0)
    return;

  {
    QStylePainter p(this);

    QStyleOptionFrame frameOptions;
    frameOptions.init(this);
    p.drawPrimitive(QStyle::PE_FrameDockWidget, frameOptions);

    // Title must be painted after the frame, since the areas overlap, and
    // the title may wish to extend out to all sides (eg. XP style)
    QStyleOptionDockWidget titlebarOptions;

    titlebarOptions.initFrom(this);
    titlebarOptions.rect = titleRect();
    titlebarOptions.title = windowTitle();
    titlebarOptions.closable = true;
    titlebarOptions.movable = true;
    titlebarOptions.floatable = false;
    titlebarOptions.verticalTitleBar = false;

    p.drawControl(QStyle::CE_DockWidgetTitle, titlebarOptions);

    QStyleOptionToolButton buttonOpt;

    buttonOpt.initFrom(this);
    buttonOpt.iconSize = QSize(m_closeButtonSize, m_closeButtonSize);
    buttonOpt.subControls = 0;
    buttonOpt.activeSubControls = 0;
    buttonOpt.features = QStyleOptionToolButton::None;
    buttonOpt.arrowType = Qt::NoArrow;
    buttonOpt.state = QStyle::State_Active | QStyle::State_Enabled | QStyle::State_AutoRaise;

    if(m_closeRect.contains(mapFromGlobal(QCursor::pos())))
    {
      buttonOpt.state |= QStyle::State_MouseOver | QStyle::State_Raised;
    }

    buttonOpt.rect = m_closeRect;
    buttonOpt.icon = m_closeIcon;

    if(style()->styleHint(QStyle::SH_DockWidget_ButtonsHaveFrame, 0, this))
    {
      style()->drawPrimitive(QStyle::PE_PanelButtonTool, &buttonOpt, &p, this);
    }

    style()->drawComplexControl(QStyle::CC_ToolButton, &buttonOpt, &p, this);
  }
}

void ToolWindowManagerWrapper::resizeEvent(QResizeEvent *)
{
  // abort dragging caused by QEvent::NonClientAreaMouseButtonPress in eventFilter function
  m_manager->abortDrag();

  QStyleOptionDockWidget option;

  option.initFrom(this);
  option.rect = titleRect();
  option.closable = true;
  option.movable = true;
  option.floatable = true;

  m_closeRect = style()->subElementRect(QStyle::SE_DockWidgetCloseButton, &option, this);
  m_closeIcon = style()->standardIcon(QStyle::SP_TitleBarCloseButton, &option, this);
}

QRect ToolWindowManagerWrapper::titleRect()
{
  QRect ret;

  ret.setTopLeft(QPoint(m_frameWidth, m_frameWidth));
  ret.setSize(QSize(geometry().width() - (m_frameWidth * 2), m_titleHeight));

  return ret;
}

QVariantMap ToolWindowManagerWrapper::saveState()
{
  if(layout()->count() > 2)
  {
    qWarning("too many children for wrapper");
    return QVariantMap();
  }
  if(isWindow() && layout()->count() == 0)
  {
    qWarning("empty top level wrapper");
    return QVariantMap();
  }
  QVariantMap result;
  result[QStringLiteral("geometry")] = saveGeometry().toBase64();
  QSplitter *splitter = findChild<QSplitter *>(QString(), Qt::FindDirectChildrenOnly);
  if(splitter)
  {
    result[QStringLiteral("splitter")] = m_manager->saveSplitterState(splitter);
  }
  else
  {
    ToolWindowManagerArea *area = findChild<ToolWindowManagerArea *>();
    if(area)
    {
      result[QStringLiteral("area")] = area->saveState();
    }
    else if(layout()->count() > 0)
    {
      qWarning("unknown child");
      return QVariantMap();
    }
  }
  return result;
}

void ToolWindowManagerWrapper::restoreState(const QVariantMap &savedData)
{
  restoreGeometry(QByteArray::fromBase64(savedData[QStringLiteral("geometry")].toByteArray()));
  if(layout()->count() > 1)
  {
    qWarning("wrapper is not empty");
    return;
  }
  if(savedData.contains(QStringLiteral("splitter")))
  {
    layout()->addWidget(
        m_manager->restoreSplitterState(savedData[QStringLiteral("splitter")].toMap()));
  }
  else if(savedData.contains(QStringLiteral("area")))
  {
    ToolWindowManagerArea *area = m_manager->createArea();
    area->restoreState(savedData[QStringLiteral("area")].toMap());
    layout()->addWidget(area);
  }
}

void ToolWindowManagerWrapper::moveTimeout()
{
  m_manager->updateDragPosition();

  if(!m_manager->dragInProgress())
  {
    m_moveTimeout->stop();
  }
}

ToolWindowManagerWrapper::ResizeDirection ToolWindowManagerWrapper::checkResize()
{
  if(m_titleHeight == 0)
    return ResizeDirection::Count;

  // check if we should offer to resize
  QRect rect = this->rect();
  QPoint testPos = mapFromGlobal(QCursor::pos());

  if(m_closeRect.contains(testPos))
    return ResizeDirection::Count;

  const int resizeMargin = 4;

  if(rect.contains(testPos))
  {
    // check corners first, then horizontal/vertical
    if(testPos.x() < rect.x() + resizeMargin * 4 && testPos.y() < rect.y() + resizeMargin * 4)
    {
      return ResizeDirection::NW;
    }
    else if(testPos.x() > rect.width() - resizeMargin * 4 && testPos.y() < rect.y() + resizeMargin * 4)
    {
      return ResizeDirection::NE;
    }
    else if(testPos.x() < rect.x() + resizeMargin * 4 &&
            testPos.y() > rect.height() - resizeMargin * 4)
    {
      return ResizeDirection::SW;
    }
    else if(testPos.x() > rect.width() - resizeMargin * 4 &&
            testPos.y() > rect.height() - resizeMargin * 4)
    {
      return ResizeDirection::SE;
    }
    else if(testPos.x() < rect.x() + resizeMargin)
    {
      return ResizeDirection::W;
    }
    else if(testPos.x() > rect.width() - resizeMargin)
    {
      return ResizeDirection::E;
    }
    else if(testPos.y() < rect.y() + resizeMargin)
    {
      return ResizeDirection::N;
    }
    else if(testPos.y() > rect.height() - resizeMargin)
    {
      return ResizeDirection::S;
    }
  }

  return ResizeDirection::Count;
}
