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

static void showCloseButton(QTabBar *bar, int index, bool show) {
  QWidget *button = bar->tabButton(index, QTabBar::RightSide);
  if(button == NULL)
    button = bar->tabButton(index, QTabBar::LeftSide);

  if(button)
    button->resize(show ? QSize(16, 16) : QSize(0, 0));
}

ToolWindowManagerArea::ToolWindowManagerArea(ToolWindowManager *manager, QWidget *parent) :
  QTabWidget(parent)
, m_manager(manager)
{
  m_dragCanStart = false;
  m_tabDragCanStart = false;
  m_inTabMoved = false;
  m_userCanDrop = true;
  setMovable(true);
  setTabsClosable(true);
  setDocumentMode(true);
  tabBar()->installEventFilter(this);
  m_manager->m_areas << this;

  QObject::connect(tabBar(), &QTabBar::tabMoved, this, &ToolWindowManagerArea::tabMoved);
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
    if(m_manager->toolWindowProperties(toolWindow) & ToolWindowManager::HideCloseButton) {
      showCloseButton(tabBar(), index, false);
    }
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

void ToolWindowManagerArea::updateToolWindow(QWidget* toolWindow) {
  int index = indexOf(toolWindow);
  if(index >= 0) {
    if(m_manager->toolWindowProperties(toolWindow) & ToolWindowManager::HideCloseButton) {
      showCloseButton(tabBar(), index, false);
    } else {
      showCloseButton(tabBar(), index, true);
    }
    tabBar()->setTabText(index, toolWindow->windowTitle());
  }
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

      int tabIndex = tabBar()->tabAt(static_cast<QMouseEvent*>(event)->pos());

      // can start tab drag only if mouse is at some tab, not at empty tabbar space
      if (tabIndex >= 0) {
        m_tabDragCanStart = true;

        if (m_manager->toolWindowProperties(widget(tabIndex)) & ToolWindowManager::DisableDraggableTab) {
          setMovable(false);
        } else {
          setMovable(true);
        }
      } else {
        m_dragCanStart = true;
      }
    } else if (event->type() == QEvent::MouseButtonPress &&
        qApp->mouseButtons() == Qt::MiddleButton) {

      int tabIndex = tabBar()->tabAt(static_cast<QMouseEvent*>(event)->pos());

      if(tabIndex >= 0) {
        QWidget *w = widget(tabIndex);

        if(!(m_manager->toolWindowProperties(w) & ToolWindowManager::HideCloseButton)) {
          m_manager->removeToolWindow(w);
        }
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
  result[QStringLiteral("type")] = QStringLiteral("area");
  result[QStringLiteral("currentIndex")] = currentIndex();
  QVariantList objects;
  objects.reserve(count());
  for(int i = 0; i < count(); i++) {
    QWidget *w = widget(i);
    QString name = w->objectName();
    if (name.isEmpty()) {
      qWarning("cannot save state of tool window without object name");
    } else {
      QVariantMap objectData;
      objectData[QStringLiteral("name")] = name;
      objectData[QStringLiteral("data")] = w->property("persistData");
      objects.push_back(objectData);
    }
  }
  result[QStringLiteral("objects")] = objects;
  return result;
}

void ToolWindowManagerArea::restoreState(const QVariantMap &savedData) {
  for(QVariant object : savedData[QStringLiteral("objects")].toList()) {
    QVariantMap objectData = object.toMap();
    if (objectData.isEmpty()) { continue; }
    QString objectName = objectData[QStringLiteral("name")].toString();
    if (objectName.isEmpty()) { continue; }
    QWidget *t = NULL;
    for(QWidget* toolWindow : m_manager->m_toolWindows) {
      if (toolWindow->objectName() == objectName) {
        t = toolWindow;
        break;
      }
    }
    if (t == NULL) t = m_manager->createToolWindow(objectName);
    if (t) {
        t->setProperty("persistData", objectData[QStringLiteral("data")]);
        addToolWindow(t);
    } else {
      qWarning("tool window with name '%s' not found or created", objectName.toLocal8Bit().constData());
    }
  }
  setCurrentIndex(savedData[QStringLiteral("currentIndex")].toInt());
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

void ToolWindowManagerArea::tabMoved(int from, int to) {
  if(m_inTabMoved) return;

  QWidget *a = widget(from);
  QWidget *b = widget(to);

  if(!a || !b) return;

  if(m_manager->toolWindowProperties(a) & ToolWindowManager::DisableDraggableTab ||
     m_manager->toolWindowProperties(b) & ToolWindowManager::DisableDraggableTab)
  {
    m_inTabMoved = true;
    tabBar()->moveTab(to, from);
    m_inTabMoved = false;
  }
}
