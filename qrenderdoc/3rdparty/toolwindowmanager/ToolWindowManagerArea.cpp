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
#include "ToolWindowManagerArea.h"
#include "ToolWindowManager.h"
#include <QApplication>
#include <QMouseEvent>
#include <QDebug>

ToolWindowManagerArea::ToolWindowManagerArea(ToolWindowManager *manager, QWidget *parent) :
  QTabWidget(parent)
, m_manager(manager)
{
  m_dragCanStart = false;
  m_tabDragCanStart = false;
  setMovable(true);
  setTabsClosable(true);
  setDocumentMode(true);
  tabBar()->installEventFilter(this);
  m_manager->m_areas << this;
}

ToolWindowManagerArea::~ToolWindowManagerArea() {
  m_manager->m_areas.removeOne(this);
}

void ToolWindowManagerArea::addToolWindow(QWidget *toolWindow) {
  addToolWindows(QList<QWidget*>() << toolWindow);
}

void ToolWindowManagerArea::addToolWindows(const QList<QWidget *> &toolWindows) {
  int index = 0;
  foreach(QWidget* toolWindow, toolWindows) {
    index = addTab(toolWindow, toolWindow->windowIcon(), toolWindow->windowTitle());
  }
  setCurrentIndex(index);
  m_manager->m_lastUsedArea = this;
}

QList<QWidget *> ToolWindowManagerArea::toolWindows() {
  QList<QWidget *> result;
  for(int i = 0; i < count(); i++) {
    result << widget(i);
  }
  return result;
}

void ToolWindowManagerArea::mousePressEvent(QMouseEvent *) {
  if (qApp->mouseButtons() == Qt::LeftButton) {
    m_dragCanStart = true;
  }
}

void ToolWindowManagerArea::mouseReleaseEvent(QMouseEvent *) {
  m_dragCanStart = false;
  m_manager->updateDragPosition();
}

void ToolWindowManagerArea::mouseMoveEvent(QMouseEvent *) {
  check_mouse_move();
}

bool ToolWindowManagerArea::eventFilter(QObject *object, QEvent *event) {
  if (object == tabBar()) {
    if (event->type() == QEvent::MouseButtonPress &&
        qApp->mouseButtons() == Qt::LeftButton) {
      // can start tab drag only if mouse is at some tab, not at empty tabbar space
      if (tabBar()->tabAt(static_cast<QMouseEvent*>(event)->pos()) >= 0 ) {
        m_tabDragCanStart = true;
      } else {
        m_dragCanStart = true;
      }

    } else if (event->type() == QEvent::MouseButtonRelease) {
      m_tabDragCanStart = false;
      m_dragCanStart = false;
      m_manager->updateDragPosition();
    } else if (event->type() == QEvent::MouseMove) {
      m_manager->updateDragPosition();
      if (m_tabDragCanStart) {
        if (tabBar()->rect().contains(static_cast<QMouseEvent*>(event)->pos())) {
          return false;
        }
        if (qApp->mouseButtons() != Qt::LeftButton) {
          return false;
        }
        QWidget* toolWindow = currentWidget();
        if (!toolWindow || !m_manager->m_toolWindows.contains(toolWindow)) {
          return false;
        }
        m_tabDragCanStart = false;
        //stop internal tab drag in QTabBar
        QMouseEvent* releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease,
                                                    static_cast<QMouseEvent*>(event)->pos(),
                                                    Qt::LeftButton, Qt::LeftButton, 0);
        qApp->sendEvent(tabBar(), releaseEvent);
        m_manager->startDrag(QList<QWidget*>() << toolWindow);
      } else if (m_dragCanStart) {
        check_mouse_move();
      }
    }
  }
  return QTabWidget::eventFilter(object, event);
}

QVariantMap ToolWindowManagerArea::saveState() {
  QVariantMap result;
  result["type"] = "area";
  result["currentIndex"] = currentIndex();
  QStringList objectNames;
  for(int i = 0; i < count(); i++) {
    QString name = widget(i)->objectName();
    if (name.isEmpty()) {
      qWarning("cannot save state of tool window without object name");
    } else {
      objectNames << name;
    }
  }
  result["objectNames"] = objectNames;
  return result;
}

void ToolWindowManagerArea::restoreState(const QVariantMap &data) {
  foreach(QVariant objectNameValue, data["objectNames"].toList()) {
    QString objectName = objectNameValue.toString();
    if (objectName.isEmpty()) { continue; }
    bool found = false;
    foreach(QWidget* toolWindow, m_manager->m_toolWindows) {
      if (toolWindow->objectName() == objectName) {
        addToolWindow(toolWindow);
        found = true;
        break;
      }
    }
    if (!found) {
      qWarning("tool window with name '%s' not found", objectName.toLocal8Bit().constData());
    }
  }
  setCurrentIndex(data["currentIndex"].toInt());
}

void ToolWindowManagerArea::check_mouse_move() {
  m_manager->updateDragPosition();
  if (qApp->mouseButtons() == Qt::LeftButton &&
      !rect().contains(mapFromGlobal(QCursor::pos())) &&
      m_dragCanStart) {
    m_dragCanStart = false;
    QList<QWidget*> toolWindows;
    for(int i = 0; i < count(); i++) {
      QWidget* toolWindow = widget(i);
      if (!m_manager->m_toolWindows.contains(toolWindow)) {
        qWarning("tab widget contains unmanaged widget");
      } else {
        toolWindows << toolWindow;
      }
    }
    m_manager->startDrag(toolWindows);
  }
}
