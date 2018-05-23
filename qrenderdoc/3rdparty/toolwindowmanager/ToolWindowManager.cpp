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
#include "ToolWindowManager.h"
#include <QApplication>
#include <QDebug>
#include <QDesktopWidget>
#include <QDrag>
#include <QEvent>
#include <QMetaMethod>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QSplitter>
#include <QTabBar>
#include <QVBoxLayout>
#include "ToolWindowManagerArea.h"
#include "ToolWindowManagerSplitter.h"
#include "ToolWindowManagerWrapper.h"

template <class T>
T findClosestParent(QWidget *widget)
{
  while(widget)
  {
    if(qobject_cast<T>(widget))
    {
      return static_cast<T>(widget);
    }
    widget = widget->parentWidget();
  }
  return 0;
}

ToolWindowManager::ToolWindowManager(QWidget *parent) : QWidget(parent)
{
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  ToolWindowManagerWrapper *wrapper = new ToolWindowManagerWrapper(this, false);
  wrapper->setWindowFlags(wrapper->windowFlags() & ~Qt::Tool);
  mainLayout->addWidget(wrapper);
  m_allowFloatingWindow = true;
  m_createCallback = NULL;
  m_lastUsedArea = NULL;

  m_draggedWrapper = NULL;
  m_hoverArea = NULL;

  QPalette pal = palette();
  pal.setColor(QPalette::Background, pal.color(QPalette::Highlight));

  m_previewOverlay = new QWidget(NULL);
  m_previewOverlay->setAutoFillBackground(true);
  m_previewOverlay->setPalette(pal);
  m_previewOverlay->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                                   Qt::X11BypassWindowManagerHint);
  m_previewOverlay->setWindowOpacity(0.3);
  m_previewOverlay->setAttribute(Qt::WA_ShowWithoutActivating);
  m_previewOverlay->setAttribute(Qt::WA_AlwaysStackOnTop);
  m_previewOverlay->hide();

  m_previewTabOverlay = new QWidget(NULL);
  m_previewTabOverlay->setAutoFillBackground(true);
  m_previewTabOverlay->setPalette(pal);
  m_previewTabOverlay->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                                      Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
  m_previewTabOverlay->setWindowOpacity(0.3);
  m_previewTabOverlay->setAttribute(Qt::WA_ShowWithoutActivating);
  m_previewTabOverlay->setAttribute(Qt::WA_AlwaysStackOnTop);
  m_previewTabOverlay->hide();

  for(int i = 0; i < NumReferenceTypes; i++)
    m_dropHotspots[i] = NULL;

  m_dropHotspotDimension = 32;
  m_dropHotspotMargin = 4;

  drawHotspotPixmaps();

  for(AreaReferenceType type : {AddTo, TopOf, LeftOf, RightOf, BottomOf, TopWindowSide,
                                LeftWindowSide, RightWindowSide, BottomWindowSide})
  {
    m_dropHotspots[type] = new QLabel(NULL);
    m_dropHotspots[type]->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                                         Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    m_dropHotspots[type]->setAttribute(Qt::WA_ShowWithoutActivating);
    m_dropHotspots[type]->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_dropHotspots[type]->setPixmap(m_pixmaps[type]);
    m_dropHotspots[type]->setFixedSize(m_dropHotspotDimension, m_dropHotspotDimension);
  }
}

ToolWindowManager::~ToolWindowManager()
{
  delete m_previewOverlay;
  delete m_previewTabOverlay;
  for(QWidget *hotspot : m_dropHotspots)
    delete hotspot;
  while(!m_areas.isEmpty())
  {
    delete m_areas.first();
  }
  while(!m_wrappers.isEmpty())
  {
    delete m_wrappers.first();
  }
}

void ToolWindowManager::setToolWindowProperties(QWidget *toolWindow,
                                                ToolWindowManager::ToolWindowProperty properties)
{
  m_toolWindowProperties[toolWindow] = properties;
  ToolWindowManagerArea *area = areaOf(toolWindow);
  if(area)
    area->updateToolWindow(toolWindow);
}

ToolWindowManager::ToolWindowProperty ToolWindowManager::toolWindowProperties(QWidget *toolWindow)
{
  return m_toolWindowProperties[toolWindow];
}

void ToolWindowManager::addToolWindow(QWidget *toolWindow, const AreaReference &area,
                                      ToolWindowManager::ToolWindowProperty properties)
{
  addToolWindows(QList<QWidget *>() << toolWindow, area, properties);
}

void ToolWindowManager::addToolWindows(QList<QWidget *> toolWindows,
                                       const ToolWindowManager::AreaReference &area,
                                       ToolWindowManager::ToolWindowProperty properties)
{
  foreach(QWidget *toolWindow, toolWindows)
  {
    if(!toolWindow)
    {
      qWarning("cannot add null widget");
      continue;
    }
    if(m_toolWindows.contains(toolWindow))
    {
      qWarning("this tool window has already been added");
      continue;
    }
    toolWindow->hide();
    toolWindow->setParent(0);
    m_toolWindows << toolWindow;
    m_toolWindowProperties[toolWindow] = properties;
    QObject::connect(toolWindow, &QWidget::windowTitleChanged, this,
                     &ToolWindowManager::windowTitleChanged);
  }
  moveToolWindows(toolWindows, area);
}

ToolWindowManagerArea *ToolWindowManager::areaOf(QWidget *toolWindow)
{
  return findClosestParent<ToolWindowManagerArea *>(toolWindow);
}

ToolWindowManagerWrapper *ToolWindowManager::wrapperOf(QWidget *toolWindow)
{
  return findClosestParent<ToolWindowManagerWrapper *>(toolWindow);
}

void ToolWindowManager::moveToolWindow(QWidget *toolWindow, AreaReference area)
{
  moveToolWindows(QList<QWidget *>() << toolWindow, area);
}

void ToolWindowManager::moveToolWindows(QList<QWidget *> toolWindows,
                                        ToolWindowManager::AreaReference area)
{
  QList<ToolWindowManagerWrapper *> wrappersToUpdate;
  foreach(QWidget *toolWindow, toolWindows)
  {
    if(!m_toolWindows.contains(toolWindow))
    {
      qWarning("unknown tool window");
      return;
    }
    ToolWindowManagerWrapper *oldWrapper = wrapperOf(toolWindow);
    if(toolWindow->parentWidget() != 0)
    {
      releaseToolWindow(toolWindow);
    }
    if(oldWrapper && !wrappersToUpdate.contains(oldWrapper))
      wrappersToUpdate.push_back(oldWrapper);
  }
  if(area.type() == LastUsedArea && !m_lastUsedArea)
  {
    ToolWindowManagerArea *foundArea = findChild<ToolWindowManagerArea *>();
    if(foundArea)
    {
      area = AreaReference(AddTo, foundArea);
    }
    else
    {
      area = EmptySpace;
    }
  }

  if(area.type() == NoArea)
  {
    // do nothing
  }
  else if(area.type() == NewFloatingArea)
  {
    ToolWindowManagerArea *floatArea = createArea();
    floatArea->addToolWindows(toolWindows);
    ToolWindowManagerWrapper *wrapper = new ToolWindowManagerWrapper(this, true);
    wrapper->layout()->addWidget(floatArea);
    wrapper->move(QCursor::pos());
    wrapper->updateTitle();
    wrapper->show();
  }
  else if(area.type() == AddTo)
  {
    int idx = -1;
    if(area.dragResult)
    {
      idx = area.area()->tabBar()->tabAt(area.area()->tabBar()->mapFromGlobal(QCursor::pos()));
    }
    area.area()->addToolWindows(toolWindows, idx);
  }
  else if(area.type() == LeftWindowSide || area.type() == RightWindowSide ||
          area.type() == TopWindowSide || area.type() == BottomWindowSide)
  {
    ToolWindowManagerWrapper *wrapper = findClosestParent<ToolWindowManagerWrapper *>(area.area());
    if(!wrapper)
    {
      qWarning("couldn't find wrapper");
      return;
    }

    if(wrapper->layout()->count() > 1)
    {
      qWarning("wrapper has multiple direct children");
      return;
    }

    QLayoutItem *item = wrapper->layout()->takeAt(0);

    QSplitter *splitter = createSplitter();
    if(area.type() == TopWindowSide || area.type() == BottomWindowSide)
    {
      splitter->setOrientation(Qt::Vertical);
    }
    else
    {
      splitter->setOrientation(Qt::Horizontal);
    }

    splitter->addWidget(item->widget());
    area.widget()->show();

    delete item;

    ToolWindowManagerArea *newArea = createArea();
    newArea->addToolWindows(toolWindows);

    if(area.type() == TopWindowSide || area.type() == LeftWindowSide)
    {
      splitter->insertWidget(0, newArea);
    }
    else
    {
      splitter->addWidget(newArea);
    }

    wrapper->layout()->addWidget(splitter);

    QRect areaGeometry = area.widget()->geometry();

    // Convert area percentage desired to relative sizes.
    const int totalStretch = (area.type() == TopWindowSide || area.type() == BottomWindowSide)
                                 ? areaGeometry.height()
                                 : areaGeometry.width();
    int pct = int(totalStretch * area.percentage());

    int a = pct;
    int b = totalStretch - pct;

    if(area.type() == BottomWindowSide || area.type() == RightWindowSide)
      std::swap(a, b);

    splitter->setSizes({a, b});
  }
  else if(area.type() == LeftOf || area.type() == RightOf || area.type() == TopOf ||
          area.type() == BottomOf)
  {
    QSplitter *parentSplitter = qobject_cast<QSplitter *>(area.widget()->parentWidget());
    ToolWindowManagerWrapper *wrapper =
        qobject_cast<ToolWindowManagerWrapper *>(area.widget()->parentWidget());
    if(!parentSplitter && !wrapper)
    {
      qWarning("unknown parent type");
      return;
    }
    bool useParentSplitter = false;
    int indexInParentSplitter = 0;
    QList<int> parentSplitterSizes;
    if(parentSplitter)
    {
      indexInParentSplitter = parentSplitter->indexOf(area.widget());
      parentSplitterSizes = parentSplitter->sizes();
      if(parentSplitter->orientation() == Qt::Vertical)
      {
        useParentSplitter = area.type() == TopOf || area.type() == BottomOf;
      }
      else
      {
        useParentSplitter = area.type() == LeftOf || area.type() == RightOf;
      }
    }
    if(useParentSplitter)
    {
      int insertIndex = indexInParentSplitter;
      if(area.type() == BottomOf || area.type() == RightOf)
      {
        insertIndex++;
      }
      ToolWindowManagerArea *newArea = createArea();
      newArea->addToolWindows(toolWindows);
      parentSplitter->insertWidget(insertIndex, newArea);

      if(parentSplitterSizes.count() > indexInParentSplitter && parentSplitterSizes[0] != 0)
      {
        int availSize = parentSplitterSizes[indexInParentSplitter];

        parentSplitterSizes[indexInParentSplitter] = int(availSize * (1.0f - area.percentage()));
        parentSplitterSizes.insert(insertIndex, int(availSize * area.percentage()));

        parentSplitter->setSizes(parentSplitterSizes);
      }
    }
    else
    {
      area.widget()->hide();
      area.widget()->setParent(0);
      QSplitter *splitter = createSplitter();
      if(area.type() == TopOf || area.type() == BottomOf)
      {
        splitter->setOrientation(Qt::Vertical);
      }
      else
      {
        splitter->setOrientation(Qt::Horizontal);
      }

      ToolWindowManagerArea *newArea = createArea();

      // inherit the size policy from the widget we are wrapping
      splitter->setSizePolicy(area.widget()->sizePolicy());

      // store old geometries so we can restore them
      QRect areaGeometry = area.widget()->geometry();
      QRect newGeometry = newArea->geometry();

      splitter->addWidget(area.widget());
      area.widget()->show();

      if(area.type() == TopOf || area.type() == LeftOf)
      {
        splitter->insertWidget(0, newArea);
      }
      else
      {
        splitter->addWidget(newArea);
      }

      if(parentSplitter)
      {
        parentSplitter->insertWidget(indexInParentSplitter, splitter);

        if(parentSplitterSizes.count() > 0 && parentSplitterSizes[0] != 0)
        {
          parentSplitter->setSizes(parentSplitterSizes);
        }
      }
      else
      {
        wrapper->layout()->addWidget(splitter);
      }

      newArea->addToolWindows(toolWindows);

      area.widget()->setGeometry(areaGeometry);
      newArea->setGeometry(newGeometry);

      // Convert area percentage desired to relative sizes.
      const int totalStretch = (area.type() == TopOf || area.type() == BottomOf)
                                   ? areaGeometry.height()
                                   : areaGeometry.width();
      int pct = int(totalStretch * area.percentage());

      int a = pct;
      int b = totalStretch - pct;

      if(area.type() == BottomOf || area.type() == RightOf)
        std::swap(a, b);

      splitter->setSizes({a, b});
    }
  }
  else if(area.type() == EmptySpace)
  {
    ToolWindowManagerArea *newArea = createArea();
    findChild<ToolWindowManagerWrapper *>()->layout()->addWidget(newArea);
    newArea->addToolWindows(toolWindows);
  }
  else if(area.type() == LastUsedArea)
  {
    m_lastUsedArea->addToolWindows(toolWindows);
  }
  else
  {
    qWarning("invalid type");
  }
  simplifyLayout();
  foreach(QWidget *toolWindow, toolWindows)
  {
    emit toolWindowVisibilityChanged(toolWindow, toolWindow->parent() != 0);
    ToolWindowManagerWrapper *wrapper = wrapperOf(toolWindow);
    if(wrapper && !wrappersToUpdate.contains(wrapper))
      wrappersToUpdate.push_back(wrapper);
  }
  foreach(ToolWindowManagerWrapper *wrapper, wrappersToUpdate)
  {
    wrapper->updateTitle();
  }
}

void ToolWindowManager::removeToolWindow(QWidget *toolWindow, bool allowCloseAlreadyChecked)
{
  if(!m_toolWindows.contains(toolWindow))
  {
    qWarning("unknown tool window");
    return;
  }

  // search up to find the first parent manager
  ToolWindowManager *manager = findClosestParent<ToolWindowManager *>(toolWindow);

  if(!manager)
  {
    qWarning("unknown tool window");
    return;
  }

  if(!allowCloseAlreadyChecked)
  {
    if(!manager->allowClose(toolWindow))
      return;
  }

  moveToolWindow(toolWindow, NoArea);
  m_toolWindows.removeOne(toolWindow);
  m_toolWindowProperties.remove(toolWindow);
  delete toolWindow;
}

bool ToolWindowManager::isFloating(QWidget *toolWindow)
{
  ToolWindowManagerWrapper *wrapper = wrapperOf(toolWindow);
  if(wrapper)
  {
    return wrapper->floating();
  }
  return false;
}

ToolWindowManager *ToolWindowManager::managerOf(QWidget *toolWindow)
{
  if(!toolWindow)
  {
    qWarning("NULL tool window");
    return NULL;
  }

  return findClosestParent<ToolWindowManager *>(toolWindow);
}

void ToolWindowManager::closeToolWindow(QWidget *toolWindow)
{
  if(!toolWindow)
  {
    qWarning("NULL tool window");
    return;
  }

  // search up to find the first parent manager
  ToolWindowManager *manager = findClosestParent<ToolWindowManager *>(toolWindow);

  if(manager)
  {
    manager->removeToolWindow(toolWindow);
    return;
  }

  qWarning("window not child of any tool window");
}

void ToolWindowManager::raiseToolWindow(QWidget *toolWindow)
{
  if(!toolWindow)
  {
    qWarning("NULL tool window");
    return;
  }

  // if the parent is a ToolWindowManagerArea, switch tabs
  QWidget *parent = toolWindow->parentWidget();
  ToolWindowManagerArea *area = qobject_cast<ToolWindowManagerArea *>(parent);
  if(area == NULL && parent)
    parent = parent->parentWidget();

  area = qobject_cast<ToolWindowManagerArea *>(parent);

  if(area)
    area->setCurrentWidget(toolWindow);
  else
    qWarning("parent is not a tool window area");
}

QWidget *ToolWindowManager::createToolWindow(const QString &objectName)
{
  if(m_createCallback)
  {
    QWidget *toolWindow = m_createCallback(objectName);
    if(toolWindow)
    {
      m_toolWindows << toolWindow;
      m_toolWindowProperties[toolWindow] = ToolWindowProperty(0);
      QObject::connect(toolWindow, &QWidget::windowTitleChanged, this,
                       &ToolWindowManager::windowTitleChanged);
      return toolWindow;
    }
  }

  return NULL;
}

void ToolWindowManager::setDropHotspotMargin(int pixels)
{
  m_dropHotspotMargin = pixels;
  drawHotspotPixmaps();
}

void ToolWindowManager::setDropHotspotDimension(int pixels)
{
  m_dropHotspotDimension = pixels;

  for(QLabel *hotspot : m_dropHotspots)
  {
    if(hotspot)
      hotspot->setFixedSize(m_dropHotspotDimension, m_dropHotspotDimension);
  }
}

void ToolWindowManager::setAllowFloatingWindow(bool allow)
{
  m_allowFloatingWindow = allow;
}

QVariantMap ToolWindowManager::saveState()
{
  QVariantMap result;
  result[QStringLiteral("toolWindowManagerStateFormat")] = 1;
  ToolWindowManagerWrapper *mainWrapper = findChild<ToolWindowManagerWrapper *>();
  if(!mainWrapper)
  {
    qWarning("can't find main wrapper");
    return QVariantMap();
  }
  result[QStringLiteral("mainWrapper")] = mainWrapper->saveState();
  QVariantList floatingWindowsData;
  foreach(ToolWindowManagerWrapper *wrapper, m_wrappers)
  {
    if(!wrapper->isWindow())
    {
      continue;
    }
    floatingWindowsData << wrapper->saveState();
  }
  result[QStringLiteral("floatingWindows")] = floatingWindowsData;
  return result;
}

void ToolWindowManager::restoreState(const QVariantMap &dataMap)
{
  if(dataMap.isEmpty())
  {
    return;
  }
  if(dataMap[QStringLiteral("toolWindowManagerStateFormat")].toInt() != 1)
  {
    qWarning("state format is not recognized");
    return;
  }
  moveToolWindows(m_toolWindows, NoArea);
  ToolWindowManagerWrapper *mainWrapper = findChild<ToolWindowManagerWrapper *>();
  if(!mainWrapper)
  {
    qWarning("can't find main wrapper");
    return;
  }
  mainWrapper->restoreState(dataMap[QStringLiteral("mainWrapper")].toMap());
  QVariantList floatWins = dataMap[QStringLiteral("floatingWindows")].toList();
  foreach(QVariant windowData, floatWins)
  {
    ToolWindowManagerWrapper *wrapper = new ToolWindowManagerWrapper(this, true);
    wrapper->restoreState(windowData.toMap());
    wrapper->updateTitle();
    wrapper->show();
    if(wrapper->windowState() & Qt::WindowMaximized)
    {
      wrapper->setWindowState(0);
      wrapper->setWindowState(Qt::WindowMaximized);
    }
  }
  simplifyLayout();
  foreach(QWidget *toolWindow, m_toolWindows)
  {
    emit toolWindowVisibilityChanged(toolWindow, toolWindow->parentWidget() != 0);
  }
}

ToolWindowManagerArea *ToolWindowManager::createArea()
{
  ToolWindowManagerArea *area = new ToolWindowManagerArea(this, 0);
  connect(area, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequested(int)));
  return area;
}

void ToolWindowManager::releaseToolWindow(QWidget *toolWindow)
{
  ToolWindowManagerArea *previousTabWidget = findClosestParent<ToolWindowManagerArea *>(toolWindow);
  if(!previousTabWidget)
  {
    qWarning("cannot find tab widget for tool window");
    return;
  }
  previousTabWidget->removeTab(previousTabWidget->indexOf(toolWindow));
  toolWindow->hide();
  toolWindow->setParent(0);
}

void ToolWindowManager::simplifyLayout()
{
  foreach(ToolWindowManagerArea *area, m_areas)
  {
    if(area->parentWidget() == 0)
    {
      if(area->count() == 0)
      {
        if(area == m_lastUsedArea)
        {
          m_lastUsedArea = 0;
        }
        // QTimer::singleShot(1000, area, SLOT(deleteLater()));
        area->deleteLater();
      }
      continue;
    }
    QSplitter *splitter = qobject_cast<QSplitter *>(area->parentWidget());
    QSplitter *validSplitter = 0;      // least top level splitter that should remain
    QSplitter *invalidSplitter = 0;    // most top level splitter that should be deleted
    while(splitter)
    {
      if(splitter->count() > 1)
      {
        validSplitter = splitter;
        break;
      }
      else
      {
        invalidSplitter = splitter;
        splitter = qobject_cast<QSplitter *>(splitter->parentWidget());
      }
    }
    if(!validSplitter)
    {
      ToolWindowManagerWrapper *wrapper = findClosestParent<ToolWindowManagerWrapper *>(area);
      if(!wrapper)
      {
        qWarning("can't find wrapper");
        return;
      }
      if(area->count() == 0 && wrapper->isWindow())
      {
        wrapper->hide();
        // can't deleteLater immediately (strange MacOS bug)
        // QTimer::singleShot(1000, wrapper, SLOT(deleteLater()));
        wrapper->deleteLater();
      }
      else if(area->parent() != wrapper)
      {
        wrapper->layout()->addWidget(area);
      }
    }
    else
    {
      if(area->count() > 0)
      {
        if(validSplitter && area->parent() != validSplitter)
        {
          int index = validSplitter->indexOf(invalidSplitter);
          validSplitter->insertWidget(index, area);
        }
      }
    }
    if(invalidSplitter)
    {
      invalidSplitter->hide();
      invalidSplitter->setParent(0);
      // QTimer::singleShot(1000, invalidSplitter, SLOT(deleteLater()));
      invalidSplitter->deleteLater();
    }
    if(area->count() == 0)
    {
      area->hide();
      area->setParent(0);
      if(area == m_lastUsedArea)
      {
        m_lastUsedArea = 0;
      }
      // QTimer::singleShot(1000, area, SLOT(deleteLater()));
      area->deleteLater();
    }
  }
}

void ToolWindowManager::startDrag(const QList<QWidget *> &toolWindows,
                                  ToolWindowManagerWrapper *wrapper)
{
  if(dragInProgress())
  {
    qWarning("ToolWindowManager::execDrag: drag is already in progress");
    return;
  }
  foreach(QWidget *toolWindow, toolWindows)
  {
    if(toolWindowProperties(toolWindow) & DisallowUserDocking)
    {
      return;
    }
  }
  if(toolWindows.isEmpty())
  {
    return;
  }

  m_draggedWrapper = wrapper;
  m_draggedToolWindows = toolWindows;
  qApp->installEventFilter(this);
}

QVariantMap ToolWindowManager::saveSplitterState(QSplitter *splitter)
{
  QVariantMap result;
  result[QStringLiteral("state")] = splitter->saveState().toBase64();
  result[QStringLiteral("type")] = QStringLiteral("splitter");
  QVariantList items;
  for(int i = 0; i < splitter->count(); i++)
  {
    QWidget *item = splitter->widget(i);
    QVariantMap itemValue;
    ToolWindowManagerArea *area = qobject_cast<ToolWindowManagerArea *>(item);
    if(area)
    {
      itemValue = area->saveState();
    }
    else
    {
      QSplitter *childSplitter = qobject_cast<QSplitter *>(item);
      if(childSplitter)
      {
        itemValue = saveSplitterState(childSplitter);
      }
      else
      {
        qWarning("unknown splitter item");
      }
    }
    items << itemValue;
  }
  result[QStringLiteral("items")] = items;
  return result;
}

QSplitter *ToolWindowManager::restoreSplitterState(const QVariantMap &savedData)
{
  if(savedData[QStringLiteral("items")].toList().count() < 2)
  {
    qWarning("invalid splitter encountered");
  }
  QSplitter *splitter = createSplitter();

  QVariantList itemList = savedData[QStringLiteral("items")].toList();
  foreach(QVariant itemData, itemList)
  {
    QVariantMap itemValue = itemData.toMap();
    QString itemType = itemValue[QStringLiteral("type")].toString();
    if(itemType == QStringLiteral("splitter"))
    {
      splitter->addWidget(restoreSplitterState(itemValue));
    }
    else if(itemType == QStringLiteral("area"))
    {
      ToolWindowManagerArea *area = createArea();
      area->restoreState(itemValue);
      splitter->addWidget(area);
    }
    else
    {
      qWarning("unknown item type");
    }
  }
  splitter->restoreState(QByteArray::fromBase64(savedData[QStringLiteral("state")].toByteArray()));
  return splitter;
}

void ToolWindowManager::updateDragPosition()
{
  if(!dragInProgress())
  {
    return;
  }
  if(!(qApp->mouseButtons() & Qt::LeftButton))
  {
    finishDrag();
    return;
  }

  QPoint pos = QCursor::pos();
  m_hoverArea = NULL;
  ToolWindowManagerWrapper *hoverWrapper = NULL;

  foreach(ToolWindowManagerArea *area, m_areas)
  {
    // don't allow dragging a whole wrapper into a subset of itself
    if(m_draggedWrapper && area->window() == m_draggedWrapper->window())
    {
      continue;
    }
    if(area->rect().contains(area->mapFromGlobal(pos)))
    {
      m_hoverArea = area;
      break;
    }
  }

  if(m_hoverArea == NULL)
  {
    foreach(ToolWindowManagerWrapper *wrapper, m_wrappers)
    {
      // don't allow dragging a whole wrapper into a subset of itself
      if(wrapper == m_draggedWrapper)
      {
        continue;
      }
      if(wrapper->rect().contains(wrapper->mapFromGlobal(pos)))
      {
        hoverWrapper = wrapper;
        break;
      }
    }

    // if we found a wrapper and it's not empty, then we fill into a gap between two areas in a
    // splitter. Search down the hierarchy until we find a splitter whose handle intersects the
    // cursor and pick an area to map to.
    if(hoverWrapper)
    {
      QLayout *layout = hoverWrapper->layout();
      QLayoutItem *layoutitem = layout ? layout->itemAt(0) : NULL;
      QWidget *layoutwidget = layoutitem ? layoutitem->widget() : NULL;
      QSplitter *splitter = qobject_cast<QSplitter *>(layoutwidget);

      while(splitter)
      {
        QSplitter *previous = splitter;

        for(int h = 1; h < splitter->count(); h++)
        {
          QSplitterHandle *handle = splitter->handle(h);

          if(handle->rect().contains(handle->mapFromGlobal(pos)))
          {
            QWidget *a = splitter->widget(h);
            QWidget *b = splitter->widget(h + 1);

            // try the first widget, if it's an area stop
            m_hoverArea = qobject_cast<ToolWindowManagerArea *>(a);
            if(m_hoverArea)
              break;

            // then the second widget
            m_hoverArea = qobject_cast<ToolWindowManagerArea *>(b);
            if(m_hoverArea)
              break;

            // neither widget is an area - let's search for a splitter to recurse to
            splitter = qobject_cast<QSplitter *>(a);
            if(splitter)
              break;

            splitter = qobject_cast<QSplitter *>(b);
            if(splitter)
              break;

            // neither side is an area or a splitter - should be impossible, but stop recursing
            // and treat this like a floating window
            qWarning("Couldn't find splitter or area at terminal side of splitter");
            splitter = NULL;
            hoverWrapper = NULL;
            break;
          }
        }

        // if we still have a splitter, and didn't find an area, find which widget contains the
        // cursor and recurse to that splitter
        if(previous == splitter && !m_hoverArea)
        {
          for(int w = 0; w < splitter->count(); w++)
          {
            QWidget *widget = splitter->widget(w);

            if(widget->rect().contains(widget->mapFromGlobal(pos)))
            {
              splitter = qobject_cast<QSplitter *>(widget);
              if(splitter)
                break;

              // if this isn't a splitter, and it's not an area (since that would have been found
              // before any of this started) then bail out
              qWarning("cursor inside unknown child widget that isn't a splitter or area");
              splitter = NULL;
              hoverWrapper = NULL;
              break;
            }
          }
        }

        // we found an area to use! stop now
        if(m_hoverArea)
          break;

        // if we still haven't found anything, bail out
        if(previous == splitter)
        {
          qWarning("Couldn't find cursor inside any child of wrapper");
          splitter = NULL;
          hoverWrapper = NULL;
          break;
        }
      }
    }
  }

  if(m_hoverArea || hoverWrapper)
  {
    ToolWindowManagerWrapper *wrapper = hoverWrapper;
    if(m_hoverArea)
      wrapper = findClosestParent<ToolWindowManagerWrapper *>(m_hoverArea);
    QRect wrapperGeometry;
    wrapperGeometry.setSize(wrapper->rect().size());
    wrapperGeometry.moveTo(wrapper->mapToGlobal(QPoint(0, 0)));

    const int margin = m_dropHotspotMargin;

    const int size = m_dropHotspotDimension;
    const int hsize = size / 2;

    if(m_hoverArea)
    {
      QRect areaClientRect;

      // calculate the rect of the area
      areaClientRect.setTopLeft(m_hoverArea->mapToGlobal(QPoint(0, 0)));
      areaClientRect.setSize(m_hoverArea->rect().size());

      // subtract the rect for the tab bar.
      areaClientRect.adjust(0, m_hoverArea->tabBar()->rect().height(), 0, 0);

      QPoint c = areaClientRect.center();

      m_dropHotspots[AddTo]->move(c + QPoint(-hsize, -hsize));
      m_dropHotspots[AddTo]->show();

      m_dropHotspots[TopOf]->move(c + QPoint(-hsize, -hsize - margin - size));
      m_dropHotspots[TopOf]->show();

      m_dropHotspots[LeftOf]->move(c + QPoint(-hsize - margin - size, -hsize));
      m_dropHotspots[LeftOf]->show();

      m_dropHotspots[RightOf]->move(c + QPoint(hsize + margin, -hsize));
      m_dropHotspots[RightOf]->show();

      m_dropHotspots[BottomOf]->move(c + QPoint(-hsize, hsize + margin));
      m_dropHotspots[BottomOf]->show();

      c = wrapperGeometry.center();

      m_dropHotspots[TopWindowSide]->move(QPoint(c.x() - hsize, wrapperGeometry.y() + margin * 2));
      m_dropHotspots[TopWindowSide]->show();

      m_dropHotspots[LeftWindowSide]->move(QPoint(wrapperGeometry.x() + margin * 2, c.y() - hsize));
      m_dropHotspots[LeftWindowSide]->show();

      m_dropHotspots[RightWindowSide]->move(
          QPoint(wrapperGeometry.right() - size - margin * 2, c.y() - hsize));
      m_dropHotspots[RightWindowSide]->show();

      m_dropHotspots[BottomWindowSide]->move(
          QPoint(c.x() - hsize, wrapperGeometry.bottom() - size - margin * 2));
      m_dropHotspots[BottomWindowSide]->show();
    }
    else
    {
      m_dropHotspots[AddTo]->move(wrapperGeometry.center() + QPoint(-hsize, -hsize));
      m_dropHotspots[AddTo]->show();

      m_dropHotspots[TopOf]->hide();
      m_dropHotspots[LeftOf]->hide();
      m_dropHotspots[RightOf]->hide();
      m_dropHotspots[BottomOf]->hide();

      m_dropHotspots[TopWindowSide]->hide();
      m_dropHotspots[LeftWindowSide]->hide();
      m_dropHotspots[RightWindowSide]->hide();
      m_dropHotspots[BottomWindowSide]->hide();
    }

    for(QWidget *hotspot : m_dropHotspots)
      if(hotspot)
        hotspot->show();
  }
  else
  {
    for(QWidget *hotspot : m_dropHotspots)
      if(hotspot)
        hotspot->hide();
  }

  AreaReferenceType hotspot = currentHotspot();
  if((m_hoverArea || hoverWrapper) && (hotspot == AddTo || hotspot == LeftOf || hotspot == RightOf ||
                                       hotspot == TopOf || hotspot == BottomOf))
  {
    QWidget *parent = m_hoverArea;
    if(parent == NULL)
      parent = hoverWrapper;

    QRect g = parent->geometry();
    g.moveTopLeft(parent->parentWidget()->mapToGlobal(g.topLeft()));

    if(hotspot == LeftOf)
      g.adjust(0, 0, -g.width() / 2, 0);
    else if(hotspot == RightOf)
      g.adjust(g.width() / 2, 0, 0, 0);
    else if(hotspot == TopOf)
      g.adjust(0, 0, 0, -g.height() / 2);
    else if(hotspot == BottomOf)
      g.adjust(0, g.height() / 2, 0, 0);

    QRect tabGeom;

    if(hotspot == AddTo && m_hoverArea && m_hoverArea->count() > 1)
    {
      QTabBar *tb = m_hoverArea->tabBar();
      g.adjust(0, tb->rect().height(), 0, 0);

      int idx = tb->tabAt(tb->mapFromGlobal(pos));

      if(idx == -1)
      {
        tabGeom = tb->tabRect(m_hoverArea->count() - 1);
        tabGeom.moveTo(tb->mapToGlobal(QPoint(0, 0)) + tabGeom.topLeft());

        // move the tab one to the right, to indicate the tab is being added after the last one.
        tabGeom.moveLeft(tabGeom.left() + tabGeom.width());

        // clamp from the right, to ensure we don't display any tab off the end of the range
        if(tabGeom.right() > g.right())
          tabGeom.moveLeft(g.right() - tabGeom.width());
      }
      else
      {
        tabGeom = tb->tabRect(idx);
        tabGeom.moveTo(tb->mapToGlobal(QPoint(0, 0)) + tabGeom.topLeft());
      }
    }

    m_previewOverlay->setGeometry(g);

    m_previewTabOverlay->setGeometry(tabGeom);
  }
  else if((m_hoverArea || hoverWrapper) && (hotspot == LeftWindowSide || hotspot == RightWindowSide ||
                                            hotspot == TopWindowSide || hotspot == BottomWindowSide))
  {
    ToolWindowManagerWrapper *wrapper = hoverWrapper;
    if(m_hoverArea)
      wrapper = findClosestParent<ToolWindowManagerWrapper *>(m_hoverArea);

    QRect g;
    g.moveTopLeft(wrapper->mapToGlobal(QPoint()));
    g.setSize(wrapper->rect().size());

    if(hotspot == LeftWindowSide)
      g.adjust(0, 0, -(g.width() * 5) / 6, 0);
    else if(hotspot == RightWindowSide)
      g.adjust((g.width() * 5) / 6, 0, 0, 0);
    else if(hotspot == TopWindowSide)
      g.adjust(0, 0, 0, -(g.height() * 3) / 4);
    else if(hotspot == BottomWindowSide)
      g.adjust(0, (g.height() * 3) / 4, 0, 0);

    m_previewOverlay->setGeometry(g);
    m_previewTabOverlay->setGeometry(QRect());
  }
  else
  {
    bool allowFloat = m_allowFloatingWindow;

    for(QWidget *w : m_draggedToolWindows)
      allowFloat &= !(toolWindowProperties(w) & DisallowFloatWindow);

    // no hotspot highlighted, draw geometry for a float window if previewing a tear-off, or draw
    // nothing if we're dragging a float window as it moves itself.
    // we also don't render any preview tear-off when floating windows are disallowed
    if(m_draggedWrapper || !allowFloat)
    {
      m_previewOverlay->setGeometry(QRect());
    }
    else
    {
      QRect r;
      for(QWidget *w : m_draggedToolWindows)
      {
        if(w->isVisible())
          r = r.united(w->rect());
      }
      m_previewOverlay->setGeometry(pos.x(), pos.y(), r.width(), r.height());
    }
    m_previewTabOverlay->setGeometry(QRect());
  }

  m_previewOverlay->show();
  m_previewTabOverlay->show();
  for(QWidget *h : m_dropHotspots)
    if(h && h->isVisible())
      h->raise();
}

void ToolWindowManager::abortDrag()
{
  if(!dragInProgress())
    return;

  m_previewOverlay->hide();
  m_previewTabOverlay->hide();
  for(QWidget *hotspot : m_dropHotspots)
    if(hotspot)
      hotspot->hide();
  m_draggedToolWindows.clear();
  m_draggedWrapper = NULL;
  qApp->removeEventFilter(this);
}

void ToolWindowManager::finishDrag()
{
  if(!dragInProgress())
  {
    qWarning("unexpected finishDrag");
    return;
  }
  qApp->removeEventFilter(this);

  // move these locally to prevent re-entrancy
  QList<QWidget *> draggedToolWindows = m_draggedToolWindows;
  ToolWindowManagerWrapper *draggedWrapper = m_draggedWrapper;

  m_draggedToolWindows.clear();
  m_draggedWrapper = NULL;

  AreaReferenceType hotspot = currentHotspot();

  m_previewOverlay->hide();
  m_previewTabOverlay->hide();
  for(QWidget *h : m_dropHotspots)
    if(h)
      h->hide();

  if(hotspot == NewFloatingArea)
  {
    // check if we're dragging a whole float window, if so we don't do anything as it's already
    // moved
    if(!draggedWrapper)
    {
      bool allowFloat = m_allowFloatingWindow;

      for(QWidget *w : draggedToolWindows)
        allowFloat &= !(toolWindowProperties(w) & DisallowFloatWindow);

      if(allowFloat)
      {
        QRect r;
        for(QWidget *w : draggedToolWindows)
        {
          if(w->isVisible())
            r = r.united(w->rect());
        }

        moveToolWindows(draggedToolWindows, NewFloatingArea);

        ToolWindowManagerArea *area = areaOf(draggedToolWindows[0]);

        area->parentWidget()->resize(r.size());
      }
    }
  }
  else
  {
    if(m_hoverArea)
    {
      AreaReference ref(hotspot, m_hoverArea);
      ref.dragResult = true;
      moveToolWindows(draggedToolWindows, ref);
    }
    else
    {
      moveToolWindows(draggedToolWindows, AreaReference(EmptySpace));
    }
  }
}

void ToolWindowManager::drawHotspotPixmaps()
{
  for(AreaReferenceType ref : {AddTo, LeftOf, TopOf, RightOf, BottomOf})
  {
    m_pixmaps[ref] = QPixmap(m_dropHotspotDimension * devicePixelRatio(),
                             m_dropHotspotDimension * devicePixelRatio());
    m_pixmaps[ref].setDevicePixelRatio(devicePixelRatioF());

    QPainter p(&m_pixmaps[ref]);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::HighQualityAntialiasing);

    QRectF rect(0, 0, m_dropHotspotDimension, m_dropHotspotDimension);

    p.fillRect(rect, Qt::transparent);

    rect = rect.marginsAdded(QMarginsF(-1, -1, -1, -1));

    p.setPen(QPen(QBrush(Qt::darkGray), 1.5));
    p.setBrush(QBrush(Qt::lightGray));
    p.drawRoundedRect(rect, 1.5, 1.5, Qt::AbsoluteSize);

    rect = rect.marginsAdded(QMarginsF(-4, -4, -4, -4));

    QRectF fullRect = rect;

    if(ref == LeftOf)
      rect = rect.marginsAdded(QMarginsF(0, 0, -12, 0));
    else if(ref == TopOf)
      rect = rect.marginsAdded(QMarginsF(0, 0, 0, -12));
    else if(ref == RightOf)
      rect = rect.marginsAdded(QMarginsF(-12, 0, 0, 0));
    else if(ref == BottomOf)
      rect = rect.marginsAdded(QMarginsF(0, -12, 0, 0));

    p.setPen(QPen(QBrush(Qt::black), 1.0));
    p.setBrush(QBrush(Qt::white));
    p.drawRect(rect);

    // add a little title bar
    rect.setHeight(3);
    p.fillRect(rect, Qt::SolidPattern);

    // for the sides, add an arrow.
    if(ref != AddTo)
    {
      QPainterPath path;

      if(ref == LeftOf)
      {
        QPointF tip = fullRect.center() + QPointF(4, 0);

        path.addPolygon(QPolygonF({
            tip, tip + QPoint(3, 3), tip + QPoint(3, -3),
        }));
      }
      else if(ref == TopOf)
      {
        QPointF tip = fullRect.center() + QPointF(0, 4);

        path.addPolygon(QPolygonF({
            tip, tip + QPointF(-3, 3), tip + QPointF(3, 3),
        }));
      }
      else if(ref == RightOf)
      {
        QPointF tip = fullRect.center() + QPointF(-4, 0);

        path.addPolygon(QPolygonF({
            tip, tip + QPointF(-3, 3), tip + QPointF(-3, -3),
        }));
      }
      else if(ref == BottomOf)
      {
        QPointF tip = fullRect.center() + QPointF(0, -4);

        path.addPolygon(QPolygonF({
            tip, tip + QPointF(-3, -3), tip + QPointF(3, -3),
        }));
      }

      p.fillPath(path, QBrush(Qt::black));
    }
  }

  // duplicate these pixmaps by default
  m_pixmaps[LeftWindowSide] = m_pixmaps[LeftOf];
  m_pixmaps[RightWindowSide] = m_pixmaps[RightOf];
  m_pixmaps[TopWindowSide] = m_pixmaps[TopOf];
  m_pixmaps[BottomWindowSide] = m_pixmaps[BottomOf];
}

ToolWindowManager::AreaReferenceType ToolWindowManager::currentHotspot()
{
  QPoint pos = QCursor::pos();

  for(int i = 0; i < NumReferenceTypes; i++)
  {
    if(m_dropHotspots[i] && m_dropHotspots[i]->isVisible() &&
       m_dropHotspots[i]->geometry().contains(pos))
    {
      return (ToolWindowManager::AreaReferenceType)i;
    }
  }

  if(m_hoverArea)
  {
    QTabBar *tb = m_hoverArea->tabBar();
    if(tb->rect().contains(tb->mapFromGlobal(QCursor::pos())))
      return AddTo;
  }

  return NewFloatingArea;
}

bool ToolWindowManager::eventFilter(QObject *object, QEvent *event)
{
  if(event->type() == QEvent::MouseButtonRelease)
  {
    // right clicking aborts any drag in progress
    if(static_cast<QMouseEvent *>(event)->button() == Qt::RightButton)
      abortDrag();
  }
  else if(event->type() == QEvent::KeyPress)
  {
    // pressing escape any drag in progress
    QKeyEvent *ke = (QKeyEvent *)event;
    if(ke->key() == Qt::Key_Escape)
    {
      abortDrag();
    }
  }
  return QWidget::eventFilter(object, event);
}

bool ToolWindowManager::allowClose(QWidget *toolWindow)
{
  if(!m_toolWindows.contains(toolWindow))
  {
    qWarning("unknown tool window");
    return true;
  }
  int methodIndex = toolWindow->metaObject()->indexOfMethod(
      QMetaObject::normalizedSignature("checkAllowClose()"));

  if(methodIndex >= 0)
  {
    bool ret = true;
    toolWindow->metaObject()
        ->method(methodIndex)
        .invoke(toolWindow, Qt::DirectConnection, Q_RETURN_ARG(bool, ret));

    return ret;
  }

  return true;
}

void ToolWindowManager::tabCloseRequested(int index)
{
  ToolWindowManagerArea *tabWidget = qobject_cast<ToolWindowManagerArea *>(sender());
  if(!tabWidget)
  {
    qWarning("sender is not a ToolWindowManagerArea");
    return;
  }
  QWidget *toolWindow = tabWidget->widget(index);
  if(!m_toolWindows.contains(toolWindow))
  {
    qWarning("unknown tab in tab widget");
    return;
  }

  if(!allowClose(toolWindow))
    return;

  if(toolWindowProperties(toolWindow) & ToolWindowManager::HideOnClose)
    hideToolWindow(toolWindow);
  else
    removeToolWindow(toolWindow, true);
}

void ToolWindowManager::windowTitleChanged(const QString &)
{
  QWidget *toolWindow = qobject_cast<QWidget *>(sender());
  if(!toolWindow)
  {
    return;
  }
  ToolWindowManagerArea *area = areaOf(toolWindow);
  if(area)
  {
    area->updateToolWindow(toolWindow);
  }
}

QSplitter *ToolWindowManager::createSplitter()
{
  QSplitter *splitter = new ToolWindowManagerSplitter();
  splitter->setChildrenCollapsible(false);
  return splitter;
}

ToolWindowManager::AreaReference::AreaReference(ToolWindowManager::AreaReferenceType type,
                                                ToolWindowManagerArea *area, float percentage)
{
  m_type = type;
  m_percentage = percentage;
  dragResult = false;
  setWidget(area);
}

void ToolWindowManager::AreaReference::setWidget(QWidget *widget)
{
  if(m_type == LastUsedArea || m_type == NewFloatingArea || m_type == NoArea || m_type == EmptySpace)
  {
    if(widget != 0)
    {
      qWarning("area parameter ignored for this type");
    }
    m_widget = 0;
  }
  else if(m_type == AddTo)
  {
    m_widget = qobject_cast<ToolWindowManagerArea *>(widget);
    if(!m_widget)
    {
      qWarning("only ToolWindowManagerArea can be used with this type");
    }
  }
  else
  {
    if(!qobject_cast<ToolWindowManagerArea *>(widget) && !qobject_cast<QSplitter *>(widget))
    {
      qWarning("only ToolWindowManagerArea or splitter can be used with this type");
      m_widget = 0;
    }
    else
    {
      m_widget = widget;
    }
  }
}

ToolWindowManagerArea *ToolWindowManager::AreaReference::area() const
{
  return qobject_cast<ToolWindowManagerArea *>(m_widget);
}

ToolWindowManager::AreaReference::AreaReference(ToolWindowManager::AreaReferenceType type,
                                                QWidget *widget)
{
  m_type = type;
  dragResult = false;
  setWidget(widget);
}
