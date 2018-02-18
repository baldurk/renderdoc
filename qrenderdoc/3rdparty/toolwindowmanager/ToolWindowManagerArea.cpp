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
#include <QApplication>
#include <QMouseEvent>
#include <algorithm>
#include "ToolWindowManager.h"
#include "ToolWindowManagerTabBar.h"
#include "ToolWindowManagerWrapper.h"

static void showCloseButton(QTabBar *bar, int index, bool show)
{
  QWidget *button = bar->tabButton(index, QTabBar::RightSide);
  if(button == NULL)
    button = bar->tabButton(index, QTabBar::LeftSide);

  if(button)
  {
    button->resize(show ? QSize(16, 16) : QSize(1, 1));
    button->setVisible(show);
  }
}

ToolWindowManagerArea::ToolWindowManagerArea(ToolWindowManager *manager, QWidget *parent)
    : QTabWidget(parent), m_manager(manager)
{
  m_tabBar = new ToolWindowManagerTabBar(this);
  setTabBar(m_tabBar);

  m_tabBar->setTabsClosable(true);

  m_dragCanStart = false;
  m_tabDragCanStart = false;
  m_inTabMoved = false;
  m_userCanDrop = true;
  setMovable(true);
  setDocumentMode(true);
  tabBar()->installEventFilter(this);
  m_manager->m_areas << this;

  QObject::connect(tabBar(), &QTabBar::tabMoved, this, &ToolWindowManagerArea::tabMoved);
  QObject::connect(tabBar(), &QTabBar::tabCloseRequested, this, &ToolWindowManagerArea::tabClosing);
  QObject::connect(tabBar(), &QTabBar::tabCloseRequested, this, &QTabWidget::tabCloseRequested);
  QObject::connect(this, &QTabWidget::currentChanged, this, &ToolWindowManagerArea::tabSelected);
}

ToolWindowManagerArea::~ToolWindowManagerArea()
{
  m_manager->m_areas.removeOne(this);
}

void ToolWindowManagerArea::addToolWindow(QWidget *toolWindow, int insertIndex)
{
  addToolWindows(QList<QWidget *>() << toolWindow, insertIndex);
}

void ToolWindowManagerArea::addToolWindows(const QList<QWidget *> &toolWindows, int insertIndex)
{
  int index = 0;
  foreach(QWidget *toolWindow, toolWindows)
  {
    index = insertTab(insertIndex, toolWindow, toolWindow->windowIcon(), toolWindow->windowTitle());
    insertIndex = index + 1;
  }
  setCurrentIndex(index);
  for(int i = 0; i < count(); i++)
  {
    updateToolWindow(widget(i));
  }
  m_manager->m_lastUsedArea = this;
}

QList<QWidget *> ToolWindowManagerArea::toolWindows()
{
  QList<QWidget *> result;
  for(int i = 0; i < count(); i++)
  {
    result << widget(i);
  }
  return result;
}

void ToolWindowManagerArea::updateToolWindow(QWidget *toolWindow)
{
  int index = indexOf(toolWindow);
  if(index >= 0)
  {
    ToolWindowManagerTabBar *tb = static_cast<ToolWindowManagerTabBar *>(tabBar());
    if(m_manager->toolWindowProperties(toolWindow) & ToolWindowManager::HideCloseButton)
      showCloseButton(tabBar(), index, false);
    else
      showCloseButton(tabBar(), index, true);
    tabBar()->setTabText(index, toolWindow->windowTitle());
  }
}

void ToolWindowManagerArea::mouseMoveEvent(QMouseEvent *)
{
  check_mouse_move();
}

bool ToolWindowManagerArea::eventFilter(QObject *object, QEvent *event)
{
  if(object == tabBar())
  {
    if(event->type() == QEvent::MouseButtonPress && qApp->mouseButtons() == Qt::LeftButton)
    {
      QPoint pos = static_cast<QMouseEvent *>(event)->pos();

      int tabIndex = tabBar()->tabAt(pos);

      // can start tab drag only if mouse is at some tab, not at empty tabbar space
      if(tabIndex >= 0)
      {
        m_tabDragCanStart = true;

        if(m_manager->toolWindowProperties(widget(tabIndex)) & ToolWindowManager::DisableDraggableTab)
        {
          setMovable(false);
        }
        else
        {
          setMovable(true);
        }
      }
      else if(m_tabBar == NULL || !m_tabBar->inButton(pos))
      {
        m_dragCanStart = true;
        m_dragCanStartPos = QCursor::pos();
      }
    }
    else if(event->type() == QEvent::MouseButtonPress && qApp->mouseButtons() == Qt::MiddleButton)
    {
      int tabIndex = tabBar()->tabAt(static_cast<QMouseEvent *>(event)->pos());

      if(tabIndex >= 0)
      {
        QWidget *w = widget(tabIndex);

        if(!(m_manager->toolWindowProperties(w) & ToolWindowManager::HideCloseButton))
        {
          emit tabCloseRequested(tabIndex);
        }
      }
    }
    else if(event->type() == QEvent::MouseButtonRelease)
    {
      m_tabDragCanStart = false;
      m_dragCanStart = false;
      m_manager->updateDragPosition();
    }
    else if(event->type() == QEvent::MouseMove)
    {
      m_manager->updateDragPosition();
      if(m_tabDragCanStart)
      {
        if(tabBar()->rect().contains(static_cast<QMouseEvent *>(event)->pos()))
        {
          return false;
        }
        if(qApp->mouseButtons() != Qt::LeftButton)
        {
          return false;
        }
        QWidget *toolWindow = currentWidget();
        if(!toolWindow || !m_manager->m_toolWindows.contains(toolWindow))
        {
          return false;
        }
        m_tabDragCanStart = false;
        // stop internal tab drag in QTabBar
        QMouseEvent *releaseEvent =
            new QMouseEvent(QEvent::MouseButtonRelease, static_cast<QMouseEvent *>(event)->pos(),
                            Qt::LeftButton, Qt::LeftButton, 0);
        qApp->sendEvent(tabBar(), releaseEvent);
        m_manager->startDrag(QList<QWidget *>() << toolWindow, NULL);
      }
      else if(m_dragCanStart)
      {
        check_mouse_move();
      }
    }
  }
  return QTabWidget::eventFilter(object, event);
}

void ToolWindowManagerArea::tabInserted(int index)
{
  // update the select order. Increment any existing index after the insertion point to keep the
  // indices in the list up to date.
  for(int &idx : m_tabSelectOrder)
  {
    if(idx >= index)
      idx++;
  }

  // if the tab inserted is the current index (most likely) then add it at the end, otherwise
  // add it next-to-end (to keep the most recent tab the same).
  if(currentIndex() == index || m_tabSelectOrder.isEmpty())
    m_tabSelectOrder.append(index);
  else
    m_tabSelectOrder.insert(m_tabSelectOrder.count() - 1, index);

  QTabWidget::tabInserted(index);
}

void ToolWindowManagerArea::tabRemoved(int index)
{
  // update the select order. Remove the index that just got deleted, and decrement any index
  // greater than it to remap to their new indices
  m_tabSelectOrder.removeOne(index);

  for(int &idx : m_tabSelectOrder)
  {
    if(idx > index)
      idx--;
  }

  QTabWidget::tabRemoved(index);
}

void ToolWindowManagerArea::tabSelected(int index)
{
  // move this tab to the end of the select order, as long as we have it - if it's a new index then
  // ignore and leave it to be handled in tabInserted()
  if(m_tabSelectOrder.contains(index))
  {
    m_tabSelectOrder.removeOne(index);
    m_tabSelectOrder.append(index);
  }

  ToolWindowManagerWrapper *wrapper = m_manager->wrapperOf(this);
  if(wrapper)
    wrapper->updateTitle();
}

void ToolWindowManagerArea::tabClosing(int index)
{
  // before closing this index, switch the current index to the next tab in succession.

  // should never get here but let's check this
  if(m_tabSelectOrder.isEmpty())
    return;

  // when closing the last tab there's nothing to do
  if(m_tabSelectOrder.count() == 1)
    return;

  // if the last in the select order is being closed, switch to the next most selected tab
  if(m_tabSelectOrder.last() == index)
    setCurrentIndex(m_tabSelectOrder.at(m_tabSelectOrder.count() - 2));
}

QVariantMap ToolWindowManagerArea::saveState()
{
  QVariantMap result;
  result[QStringLiteral("type")] = QStringLiteral("area");
  result[QStringLiteral("currentIndex")] = currentIndex();
  QVariantList objects;
  objects.reserve(count());
  for(int i = 0; i < count(); i++)
  {
    QWidget *w = widget(i);
    QString name = w->objectName();
    if(name.isEmpty())
    {
      qWarning("cannot save state of tool window without object name");
    }
    else
    {
      QVariantMap objectData;
      objectData[QStringLiteral("name")] = name;
      objectData[QStringLiteral("data")] = w->property("persistData");
      objects.push_back(objectData);
    }
  }
  result[QStringLiteral("objects")] = objects;
  return result;
}

void ToolWindowManagerArea::restoreState(const QVariantMap &savedData)
{
  for(QVariant object : savedData[QStringLiteral("objects")].toList())
  {
    QVariantMap objectData = object.toMap();
    if(objectData.isEmpty())
    {
      continue;
    }
    QString objectName = objectData[QStringLiteral("name")].toString();
    if(objectName.isEmpty())
    {
      continue;
    }
    QWidget *t = NULL;
    for(QWidget *toolWindow : m_manager->m_toolWindows)
    {
      if(toolWindow->objectName() == objectName)
      {
        t = toolWindow;
        break;
      }
    }
    if(t == NULL)
      t = m_manager->createToolWindow(objectName);
    if(t)
    {
      t->setProperty("persistData", objectData[QStringLiteral("data")]);
      addToolWindow(t);
    }
    else
    {
      qWarning("tool window with name '%s' not found or created",
               objectName.toLocal8Bit().constData());
    }
  }
  setCurrentIndex(savedData[QStringLiteral("currentIndex")].toInt());
}

void ToolWindowManagerArea::check_mouse_move()
{
  if(qApp->mouseButtons() != Qt::LeftButton && m_dragCanStart)
  {
    m_dragCanStart = false;
  }
  m_manager->updateDragPosition();
  if(m_dragCanStart && (QCursor::pos() - m_dragCanStartPos).manhattanLength() > 10)
  {
    m_dragCanStart = false;
    QList<QWidget *> toolWindows;
    for(int i = 0; i < count(); i++)
    {
      QWidget *toolWindow = widget(i);
      if(!m_manager->m_toolWindows.contains(toolWindow))
      {
        qWarning("tab widget contains unmanaged widget");
      }
      else
      {
        toolWindows << toolWindow;
      }
    }
    m_manager->startDrag(toolWindows, NULL);
  }
}

bool ToolWindowManagerArea::useMinimalTabBar()
{
  QWidget *w = widget(0);
  if(w == NULL)
    return false;

  return (m_manager->toolWindowProperties(w) & ToolWindowManager::AlwaysDisplayFullTabs) == 0;
}

void ToolWindowManagerArea::tabMoved(int from, int to)
{
  if(m_inTabMoved)
    return;

  // update the select order.
  // This amounts to just a swap - any indices other than the pair in question are unaffected since
  // one tab is removed (above/below) and added (below/above) so the indices themselves remain the
  // same.
  for(int &idx : m_tabSelectOrder)
  {
    if(idx == from)
      idx = to;
    else if(idx == to)
      idx = from;
  }

  QWidget *a = widget(from);
  QWidget *b = widget(to);

  if(!a || !b)
    return;

  if(m_manager->toolWindowProperties(a) & ToolWindowManager::DisableDraggableTab ||
     m_manager->toolWindowProperties(b) & ToolWindowManager::DisableDraggableTab)
  {
    m_inTabMoved = true;
    tabBar()->moveTab(to, from);
    m_inTabMoved = false;
  }
}
