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
#include "ToolWindowManagerArea.h"
#include "ToolWindowManagerWrapper.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QEvent>
#include <QApplication>
#include <QDrag>
#include <QMetaMethod>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QDesktopWidget>
#include <QScreen>

template<class T>
T findClosestParent(QWidget* widget) {
  while(widget) {
    if (qobject_cast<T>(widget)) {
      return static_cast<T>(widget);
    }
    widget = widget->parentWidget();
  }
  return 0;
}

ToolWindowManager::ToolWindowManager(QWidget *parent) :
  QWidget(parent)
{
  m_borderSensitivity = 12;
  QSplitter* testSplitter = new QSplitter();
  m_rubberBandLineWidth = testSplitter->handleWidth();
  delete testSplitter;
  m_dragIndicator = new QLabel(0, Qt::ToolTip );
  m_dragIndicator->setAttribute(Qt::WA_ShowWithoutActivating);
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  ToolWindowManagerWrapper* wrapper = new ToolWindowManagerWrapper(this);
  wrapper->setWindowFlags(wrapper->windowFlags() & ~Qt::Tool);
  mainLayout->addWidget(wrapper);
  connect(&m_dropSuggestionSwitchTimer, SIGNAL(timeout()),
          this, SLOT(showNextDropSuggestion()));
  m_dropSuggestionSwitchTimer.setInterval(1000);
  m_dropCurrentSuggestionIndex = 0;
  m_allowFloatingWindow = true;
  m_createCallback = NULL;
  m_lastUsedArea = NULL;

  m_rectRubberBand = new QRubberBand(QRubberBand::Rectangle, this);
  m_lineRubberBand = new QRubberBand(QRubberBand::Line, this);
}

ToolWindowManager::~ToolWindowManager() {
  while(!m_areas.isEmpty()) {
    delete m_areas.first();
  }
  while(!m_wrappers.isEmpty()) {
    delete m_wrappers.first();
  }
}

void ToolWindowManager::setToolWindowProperties(QWidget* toolWindow, ToolWindowManager::ToolWindowProperty properties) {
  m_toolWindowProperties[toolWindow] = properties;
  ToolWindowManagerArea *area = areaOf(toolWindow);
  if(area)
    area->updateToolWindow(toolWindow);
}

ToolWindowManager::ToolWindowProperty ToolWindowManager::toolWindowProperties(QWidget* toolWindow) {
  return m_toolWindowProperties[toolWindow];
}

void ToolWindowManager::addToolWindow(QWidget *toolWindow, const AreaReference &area) {
  addToolWindows(QList<QWidget*>() << toolWindow, area);
}

void ToolWindowManager::addToolWindows(QList<QWidget *> toolWindows, const ToolWindowManager::AreaReference &area) {
  foreach(QWidget* toolWindow, toolWindows) {
    if (!toolWindow) {
      qWarning("cannot add null widget");
      continue;
    }
    if (m_toolWindows.contains(toolWindow)) {
      qWarning("this tool window has already been added");
      continue;
    }
    toolWindow->hide();
    toolWindow->setParent(0);
    m_toolWindows << toolWindow;
    m_toolWindowProperties[toolWindow] = ToolWindowProperty(0);
    QObject::connect(toolWindow, &QWidget::windowTitleChanged, this, &ToolWindowManager::windowTitleChanged);
  }
  moveToolWindows(toolWindows, area);
}

ToolWindowManagerArea *ToolWindowManager::areaOf(QWidget *toolWindow) {
  return findClosestParent<ToolWindowManagerArea*>(toolWindow);
}

void ToolWindowManager::moveToolWindow(QWidget *toolWindow, AreaReference area) {
  moveToolWindows(QList<QWidget*>() << toolWindow, area);
}

void ToolWindowManager::moveToolWindows(QList<QWidget *> toolWindows,
                                        ToolWindowManager::AreaReference area) {
  foreach(QWidget* toolWindow, toolWindows) {
    if (!m_toolWindows.contains(toolWindow)) {
      qWarning("unknown tool window");
      return;
    }
    if (toolWindow->parentWidget() != 0) {
      releaseToolWindow(toolWindow);
    }
  }
  if (area.type() == LastUsedArea && !m_lastUsedArea) {
    ToolWindowManagerArea* foundArea = findChild<ToolWindowManagerArea*>();
    if (foundArea) {
      area = AreaReference(AddTo, foundArea);
    } else {
      area = EmptySpace;
    }
  }

  if (area.type() == NoArea) {
    //do nothing
  } else if (area.type() == NewFloatingArea) {
    ToolWindowManagerArea* area = createArea();
    area->addToolWindows(toolWindows);
    ToolWindowManagerWrapper* wrapper = new ToolWindowManagerWrapper(this);
    wrapper->layout()->addWidget(area);
    wrapper->move(QCursor::pos());
    wrapper->show();
  } else if (area.type() == AddTo) {
    area.area()->addToolWindows(toolWindows);
  } else if (area.type() == LeftOf || area.type() == RightOf ||
             area.type() == TopOf || area.type() == BottomOf) {
    QSplitter* parentSplitter = qobject_cast<QSplitter*>(area.widget()->parentWidget());
    ToolWindowManagerWrapper* wrapper = qobject_cast<ToolWindowManagerWrapper*>(area.widget()->parentWidget());
    if (!parentSplitter && !wrapper) {
      qWarning("unknown parent type");
      return;
    }
    bool useParentSplitter = false;
    int indexInParentSplitter = 0;
    QList<QRect> parentSplitterGeometries;
    QList<int> parentSplitterSizes;
    if (parentSplitter) {
      indexInParentSplitter = parentSplitter->indexOf(area.widget());
      parentSplitterSizes = parentSplitter->sizes();
      for(int i = 0; i < parentSplitter->count(); i++) {
        parentSplitterGeometries.push_back(parentSplitter->widget(i)->geometry());
      }
      if (parentSplitter->orientation() == Qt::Vertical) {
        useParentSplitter = area.type() == TopOf || area.type() == BottomOf;
      } else {
        useParentSplitter = area.type() == LeftOf || area.type() == RightOf;
      }
    }
    if (useParentSplitter) {
      if (area.type() == BottomOf || area.type() == RightOf) {
        indexInParentSplitter++;
      }
      ToolWindowManagerArea* newArea = createArea();
      newArea->addToolWindows(toolWindows);
      parentSplitter->insertWidget(indexInParentSplitter, newArea);

      for(int i = 0; i < qMin(parentSplitter->count(), parentSplitterGeometries.count()); i++) {
        parentSplitter->widget(i)->setGeometry(parentSplitterGeometries[i]);
      }
    } else {
      area.widget()->hide();
      area.widget()->setParent(0);
      QSplitter* splitter = createSplitter();
      if (area.type() == TopOf || area.type() == BottomOf) {
        splitter->setOrientation(Qt::Vertical);
      } else {
        splitter->setOrientation(Qt::Horizontal);
      }

      ToolWindowManagerArea* newArea = createArea();

      // inherit the size policy from the widget we are wrapping
      splitter->setSizePolicy(area.widget()->sizePolicy());

      // store old geometries so we can restore them
      QRect areaGeometry = area.widget()->geometry();
      QRect newGeometry = newArea->geometry();

      splitter->addWidget(area.widget());
      area.widget()->show();

      int indexInSplitter = 0;

      if (area.type() == TopOf || area.type() == LeftOf) {
        splitter->insertWidget(0, newArea);
        indexInSplitter = 0;
      } else {
        splitter->addWidget(newArea);
        indexInSplitter = 1;
      }

      // Convert area percentage desired to a stretch factor.
      const int totalStretch = 100;
      int pct = int(totalStretch*area.percentage());
      splitter->setStretchFactor(indexInSplitter, pct);
      splitter->setStretchFactor(1-indexInSplitter, totalStretch-pct);

      if (parentSplitter) {
        parentSplitter->insertWidget(indexInParentSplitter, splitter);

        for (int i = 0; i < qMin(parentSplitter->count(), parentSplitterGeometries.count()); i++) {
          parentSplitter->widget(i)->setGeometry(parentSplitterGeometries[i]);
        }

        if (parentSplitterSizes.count() > 0 && parentSplitterSizes[0] != 0) {
          parentSplitter->setSizes(parentSplitterSizes);
        }

      } else {
        wrapper->layout()->addWidget(splitter);
      }

      newArea->addToolWindows(toolWindows);

      area.widget()->setGeometry(areaGeometry);
      newArea->setGeometry(newGeometry);
    }
  } else if (area.type() == EmptySpace) {
    ToolWindowManagerArea* newArea = createArea();
    findChild<ToolWindowManagerWrapper*>()->layout()->addWidget(newArea);
    newArea->addToolWindows(toolWindows);
  } else if (area.type() == LastUsedArea) {
    m_lastUsedArea->addToolWindows(toolWindows);
  } else {
    qWarning("invalid type");
  }
  simplifyLayout();
  foreach(QWidget* toolWindow, toolWindows) {
    emit toolWindowVisibilityChanged(toolWindow, toolWindow->parent() != 0);
  }
}

void ToolWindowManager::removeToolWindow(QWidget *toolWindow) {
  if (!m_toolWindows.contains(toolWindow)) {
    qWarning("unknown tool window");
    return;
  }
  moveToolWindow(toolWindow, NoArea);
  m_toolWindows.removeOne(toolWindow);
  m_toolWindowProperties.remove(toolWindow);
  delete toolWindow;
}

ToolWindowManager* ToolWindowManager::managerOf(QWidget* toolWindow) {
  if (!toolWindow) {
    qWarning("NULL tool window");
    return NULL;
  }

  return findClosestParent<ToolWindowManager*>(toolWindow);
}

void ToolWindowManager::closeToolWindow(QWidget *toolWindow) {
  if (!toolWindow) {
    qWarning("NULL tool window");
    return;
  }

  // search up to find the first parent manager
  ToolWindowManager *manager = findClosestParent<ToolWindowManager*>(toolWindow);

  if(manager) {
    manager->removeToolWindow(toolWindow);
    return;
  }

  qWarning("window not child of any tool window");
}

void ToolWindowManager::raiseToolWindow(QWidget *toolWindow) {
  if (!toolWindow) {
    qWarning("NULL tool window");
    return;
  }

  // if the parent is a ToolWindowManagerArea, switch tabs
  QWidget *parent = toolWindow->parentWidget();
  ToolWindowManagerArea *area = qobject_cast<ToolWindowManagerArea*>(parent);
  if(area == NULL)
    parent = parent->parentWidget();

  area = qobject_cast<ToolWindowManagerArea*>(parent);

  if(area)
    area->setCurrentWidget(toolWindow);
  else
    qWarning("parent is not a tool window area");
}

QWidget* ToolWindowManager::createToolWindow(const QString& objectName)
{
  if (m_createCallback) {
    QWidget *toolWindow = m_createCallback(objectName);
    if(toolWindow) {
      m_toolWindows << toolWindow;
      m_toolWindowProperties[toolWindow] = ToolWindowProperty(0);
      QObject::connect(toolWindow, &QWidget::windowTitleChanged, this, &ToolWindowManager::windowTitleChanged);
      return toolWindow;
    }
  }

  return NULL;
}

void ToolWindowManager::setSuggestionSwitchInterval(int msec) {
  m_dropSuggestionSwitchTimer.setInterval(msec);
}

int ToolWindowManager::suggestionSwitchInterval() {
  return m_dropSuggestionSwitchTimer.interval();
}

void ToolWindowManager::setBorderSensitivity(int pixels) {
  m_borderSensitivity = pixels;
}

void ToolWindowManager::setRubberBandLineWidth(int pixels) {
  m_rubberBandLineWidth = pixels;
}

void ToolWindowManager::setAllowFloatingWindow(bool allow) {
  m_allowFloatingWindow = allow;
}

QVariantMap ToolWindowManager::saveState() {
  QVariantMap result;
  result["toolWindowManagerStateFormat"] = 1;
  ToolWindowManagerWrapper* mainWrapper = findChild<ToolWindowManagerWrapper*>();
  if (!mainWrapper) {
    qWarning("can't find main wrapper");
    return QVariantMap();
  }
  result["mainWrapper"] = mainWrapper->saveState();
  QVariantList floatingWindowsData;
  foreach(ToolWindowManagerWrapper* wrapper, m_wrappers) {
    if (!wrapper->isWindow()) { continue; }
    floatingWindowsData << wrapper->saveState();
  }
  result["floatingWindows"] = floatingWindowsData;
  return result;
}

void ToolWindowManager::restoreState(const QVariantMap &dataMap) {
  if (dataMap.isEmpty()) { return; }
  if (dataMap["toolWindowManagerStateFormat"].toInt() != 1) {
    qWarning("state format is not recognized");
    return;
  }
  moveToolWindows(m_toolWindows, NoArea);
  ToolWindowManagerWrapper* mainWrapper = findChild<ToolWindowManagerWrapper*>();
  if (!mainWrapper) {
    qWarning("can't find main wrapper");
    return;
  }
  mainWrapper->restoreState(dataMap["mainWrapper"].toMap());
  foreach(QVariant windowData, dataMap["floatingWindows"].toList()) {
    ToolWindowManagerWrapper* wrapper = new ToolWindowManagerWrapper(this);
    wrapper->restoreState(windowData.toMap());
    wrapper->show();
    if(wrapper->windowState() && Qt::WindowMaximized)
    {
      wrapper->setWindowState(0);
      wrapper->setWindowState(Qt::WindowMaximized);
    }
  }
  simplifyLayout();
  foreach(QWidget* toolWindow, m_toolWindows) {
    emit toolWindowVisibilityChanged(toolWindow, toolWindow->parentWidget() != 0);
  }
}

ToolWindowManagerArea *ToolWindowManager::createArea() {
  ToolWindowManagerArea* area = new ToolWindowManagerArea(this, 0);
  connect(area, SIGNAL(tabCloseRequested(int)),
          this, SLOT(tabCloseRequested(int)));
  return area;
}


void ToolWindowManager::handleNoSuggestions() {
  m_rectRubberBand->hide();
  m_lineRubberBand->hide();
  m_lineRubberBand->setParent(this);
  m_rectRubberBand->setParent(this);
  m_suggestions.clear();
  m_dropCurrentSuggestionIndex = 0;
  if (m_dropSuggestionSwitchTimer.isActive()) {
    m_dropSuggestionSwitchTimer.stop();
  }
}

void ToolWindowManager::releaseToolWindow(QWidget *toolWindow) {
  ToolWindowManagerArea* previousTabWidget = findClosestParent<ToolWindowManagerArea*>(toolWindow);
  if (!previousTabWidget) {
    qWarning("cannot find tab widget for tool window");
    return;
  }
  previousTabWidget->removeTab(previousTabWidget->indexOf(toolWindow));
  toolWindow->hide();
  toolWindow->setParent(0);

}

void ToolWindowManager::simplifyLayout() {
  foreach(ToolWindowManagerArea* area, m_areas) {
    if (area->parentWidget() == 0) {
      if (area->count() == 0) {
        if (area == m_lastUsedArea) { m_lastUsedArea = 0; }
        //QTimer::singleShot(1000, area, SLOT(deleteLater()));
        area->deleteLater();
      }
      continue;
    }
    QSplitter* splitter = qobject_cast<QSplitter*>(area->parentWidget());
    QSplitter* validSplitter = 0; // least top level splitter that should remain
    QSplitter* invalidSplitter = 0; //most top level splitter that should be deleted
    while(splitter) {
      if (splitter->count() > 1) {
        validSplitter = splitter;
        break;
      } else {
        invalidSplitter = splitter;
        splitter = qobject_cast<QSplitter*>(splitter->parentWidget());
      }
    }
    if (!validSplitter) {
      ToolWindowManagerWrapper* wrapper = findClosestParent<ToolWindowManagerWrapper*>(area);
      if (!wrapper) {
        qWarning("can't find wrapper");
        return;
      }
      if (area->count() == 0 && wrapper->isWindow()) {
        wrapper->hide();
        // can't deleteLater immediately (strange MacOS bug)
        //QTimer::singleShot(1000, wrapper, SLOT(deleteLater()));
        wrapper->deleteLater();
      } else if (area->parent() != wrapper) {
        wrapper->layout()->addWidget(area);
      }
    } else {
      if (area->count() > 0) {
        if (validSplitter && area->parent() != validSplitter) {
          int index = validSplitter->indexOf(invalidSplitter);
          validSplitter->insertWidget(index, area);
        }
      }
    }
    if (invalidSplitter) {
      invalidSplitter->hide();
      invalidSplitter->setParent(0);
      //QTimer::singleShot(1000, invalidSplitter, SLOT(deleteLater()));
      invalidSplitter->deleteLater();
    }
    if (area->count() == 0) {
      area->hide();
      area->setParent(0);
      if (area == m_lastUsedArea) { m_lastUsedArea = 0; }
      //QTimer::singleShot(1000, area, SLOT(deleteLater()));
      area->deleteLater();
    }
  }
}

void ToolWindowManager::startDrag(const QList<QWidget *> &toolWindows) {
  if (dragInProgress()) {
    qWarning("ToolWindowManager::execDrag: drag is already in progress");
    return;
  }
  foreach(QWidget* toolWindow, toolWindows) {
    if(toolWindowProperties(toolWindow) & DisallowUserDocking) { return; }
  }
  if (toolWindows.isEmpty()) { return; }
  m_draggedToolWindows = toolWindows;
  m_dragIndicator->setPixmap(generateDragPixmap(toolWindows));
  updateDragPosition();
  m_dragIndicator->show();
}

QVariantMap ToolWindowManager::saveSplitterState(QSplitter *splitter) {
  QVariantMap result;
  result["state"] = splitter->saveState().toBase64();
  result["type"] = "splitter";
  QVariantList items;
  for(int i = 0; i < splitter->count(); i++) {
    QWidget* item = splitter->widget(i);
    QVariantMap itemValue;
    ToolWindowManagerArea* area = qobject_cast<ToolWindowManagerArea*>(item);
    if (area) {
      itemValue = area->saveState();
    } else {
      QSplitter* childSplitter = qobject_cast<QSplitter*>(item);
      if (childSplitter) {
        itemValue = saveSplitterState(childSplitter);
      } else {
        qWarning("unknown splitter item");
      }
    }
    items << itemValue;
  }
  result["items"] = items;
  return result;
}

QSplitter *ToolWindowManager::restoreSplitterState(const QVariantMap &data) {
  if (data["items"].toList().count() < 2) {
    qWarning("invalid splitter encountered");
  }
  QSplitter* splitter = createSplitter();

  foreach(QVariant itemData, data["items"].toList()) {
    QVariantMap itemValue = itemData.toMap();
    QString itemType = itemValue["type"].toString();
    if (itemType == "splitter") {
      splitter->addWidget(restoreSplitterState(itemValue));
    } else if (itemType == "area") {
      ToolWindowManagerArea* area = createArea();
      area->restoreState(itemValue);
      splitter->addWidget(area);
    } else {
      qWarning("unknown item type");
    }
  }
  splitter->restoreState(QByteArray::fromBase64(data["state"].toByteArray()));
  return splitter;
}

QPixmap ToolWindowManager::generateDragPixmap(const QList<QWidget *> &toolWindows) {
  QTabBar widget;
  widget.setDocumentMode(true);
  foreach(QWidget* toolWindow, toolWindows) {
    widget.addTab(toolWindow->windowIcon(), toolWindow->windowTitle());
  }
#if QT_VERSION >= 0x050000 // Qt5
  return widget.grab();
#else //Qt4
  return QPixmap::grabWidget(&widget);
#endif
}

void ToolWindowManager::showNextDropSuggestion() {
  if (m_suggestions.isEmpty()) {
    qWarning("showNextDropSuggestion called but no suggestions");
    return;
  }
  m_dropCurrentSuggestionIndex++;
  if (m_dropCurrentSuggestionIndex >= m_suggestions.count()) {
    m_dropCurrentSuggestionIndex = 0;
  }
  const AreaReference& suggestion = m_suggestions[m_dropCurrentSuggestionIndex];
  if (suggestion.type() == AddTo || suggestion.type() == EmptySpace) {
    QWidget* widget;
    if (suggestion.type() == EmptySpace) {
      widget = findChild<ToolWindowManagerWrapper*>();
    } else {
      widget = suggestion.widget();
    }
    QWidget* placeHolderParent;
    if (widget->topLevelWidget() == topLevelWidget()) {
      placeHolderParent = this;
    } else {
      placeHolderParent = widget->topLevelWidget();
    }
    QRect placeHolderGeometry = widget->rect();
    placeHolderGeometry.moveTopLeft(widget->mapTo(placeHolderParent,
                                                             placeHolderGeometry.topLeft()));
    m_rectRubberBand->setGeometry(placeHolderGeometry);
    m_rectRubberBand->setParent(placeHolderParent);
    m_rectRubberBand->show();
    m_lineRubberBand->hide();
  } else if (suggestion.type() == LeftOf || suggestion.type() == RightOf ||
             suggestion.type() == TopOf || suggestion.type() == BottomOf) {
    QWidget* placeHolderParent;
    if (suggestion.widget()->topLevelWidget() == topLevelWidget()) {
      placeHolderParent = this;
    } else {
      placeHolderParent = suggestion.widget()->topLevelWidget();
    }
    QRect placeHolderGeometry = sidePlaceHolderRect(suggestion.widget(), suggestion.type());
    placeHolderGeometry.moveTopLeft(suggestion.widget()->mapTo(placeHolderParent,
                                                             placeHolderGeometry.topLeft()));

    m_lineRubberBand->setGeometry(placeHolderGeometry);
    m_lineRubberBand->setParent(placeHolderParent);
    m_lineRubberBand->show();
    m_rectRubberBand->hide();
  } else {
    qWarning("unsupported suggestion type");
  }
}

void ToolWindowManager::findSuggestions(ToolWindowManagerWrapper* wrapper) {
  m_suggestions.clear();
  m_dropCurrentSuggestionIndex = -1;
  QPoint globalPos = QCursor::pos();
  QList<QWidget*> candidates;
  foreach(QSplitter* splitter, wrapper->findChildren<QSplitter*>()) {
    // make sure this is one of our layout splitters, not a proper widget or a splitter
    // from another manager. We walk the parents, expecting either a QSplitter or QTabWidget
    // at each time until we reach an area.

    QWidget *w = splitter;

    bool valid = false;

    while(w)
    {
      QWidget *parent = w->parentWidget();

      QSplitter *parentSplitter = qobject_cast<QSplitter*>(parent);
      QTabWidget *parentTab = qobject_cast<QTabWidget*>(parent);

      // keep recursing up what looks like our hierarchy
      if(parentSplitter || parentTab)
      {
        w = parent;
        continue;
      }

      ToolWindowManagerArea* area = qobject_cast<ToolWindowManagerArea*>(parent);
      ToolWindowManagerWrapper* wrapper = qobject_cast<ToolWindowManagerWrapper*>(parent);

      // if it's an area or wrapper, check if it's ours
      if(area)
        valid = area->manager() == this;
      else if(wrapper)
        valid = wrapper->manager() == this;

      // we're done now, whether we checked for validity, or if we
      // found something that's none of the above
      break;
    }

    if(valid)
      candidates << splitter;
  }
  foreach(ToolWindowManagerArea* area, m_areas) {
    if (area->topLevelWidget() == wrapper->topLevelWidget()) {
      candidates << area;
    }
  }
  foreach(QWidget* widget, candidates) {
    QSplitter* splitter = qobject_cast<QSplitter*>(widget);
    ToolWindowManagerArea* area = qobject_cast<ToolWindowManagerArea*>(widget);
    if (!splitter && !area) {
      qWarning("unexpected widget type");
      continue;
    }
    QSplitter* parentSplitter = qobject_cast<QSplitter*>(widget->parentWidget());
    bool lastInSplitter = parentSplitter &&
        parentSplitter->indexOf(widget) == parentSplitter->count() - 1;

    QList<AreaReferenceType> allowedSides;
    if (!splitter || splitter->orientation() == Qt::Vertical) {
      allowedSides << LeftOf;
    }
    if (!splitter || splitter->orientation() == Qt::Horizontal) {
      allowedSides << TopOf;
    }
    if (!parentSplitter || parentSplitter->orientation() == Qt::Vertical || lastInSplitter) {
      if (!splitter || splitter->orientation() == Qt::Vertical) {
        allowedSides << RightOf;
      }
    }
    if (!parentSplitter || parentSplitter->orientation() == Qt::Horizontal || lastInSplitter) {
      if (!splitter || splitter->orientation() == Qt::Horizontal) {
        allowedSides << BottomOf;
      }
    }
    foreach(AreaReferenceType side, allowedSides) {
      if (sideSensitiveArea(widget, side).contains(widget->mapFromGlobal(globalPos))) {
        m_suggestions << AreaReference(side, widget);
      }
    }
    if (area && area->allowUserDrop() && area->rect().contains(area->mapFromGlobal(globalPos))) {
      m_suggestions << AreaReference(AddTo, area);
    }
  }
  if (candidates.isEmpty()) {
    m_suggestions << EmptySpace;
  }

  if (m_suggestions.isEmpty()) {
    handleNoSuggestions();
  } else {
    showNextDropSuggestion();
  }
}

QRect ToolWindowManager::sideSensitiveArea(QWidget *widget, ToolWindowManager::AreaReferenceType side) {
  QRect widgetRect = widget->rect();
  if (side == TopOf) {
    return QRect(QPoint(widgetRect.left(), widgetRect.top() - m_borderSensitivity),
                 QSize(widgetRect.width(), m_borderSensitivity * 2));
  } else if (side == LeftOf) {
    return QRect(QPoint(widgetRect.left() - m_borderSensitivity, widgetRect.top()),
                 QSize(m_borderSensitivity * 2, widgetRect.height()));

  } else if (side == BottomOf) {
    return QRect(QPoint(widgetRect.left(), widgetRect.top() + widgetRect.height() - m_borderSensitivity),
                 QSize(widgetRect.width(), m_borderSensitivity * 2));
  } else if (side == RightOf) {
    return QRect(QPoint(widgetRect.left() + widgetRect.width() - m_borderSensitivity, widgetRect.top()),
                 QSize(m_borderSensitivity * 2, widgetRect.height()));
  } else {
    qWarning("invalid side");
    return QRect();
  }
}

QRect ToolWindowManager::sidePlaceHolderRect(QWidget *widget, ToolWindowManager::AreaReferenceType side) {
  QRect widgetRect = widget->rect();
  QSplitter* parentSplitter = qobject_cast<QSplitter*>(widget->parentWidget());
  if (parentSplitter && parentSplitter->indexOf(widget) > 0) {
    int delta = parentSplitter->handleWidth() / 2 + m_rubberBandLineWidth / 2;
    if (side == TopOf && parentSplitter->orientation() == Qt::Vertical) {
      return QRect(QPoint(widgetRect.left(), widgetRect.top() - delta),
                   QSize(widgetRect.width(), m_rubberBandLineWidth));
    } else if (side == LeftOf && parentSplitter->orientation() == Qt::Horizontal) {
      return QRect(QPoint(widgetRect.left() - delta, widgetRect.top()),
                   QSize(m_rubberBandLineWidth, widgetRect.height()));
    }
  }
  if (side == TopOf) {
    return QRect(QPoint(widgetRect.left(), widgetRect.top()),
                 QSize(widgetRect.width(), m_rubberBandLineWidth));
  } else if (side == LeftOf) {
    return QRect(QPoint(widgetRect.left(), widgetRect.top()),
                 QSize(m_rubberBandLineWidth, widgetRect.height()));
  } else if (side == BottomOf) {
    return QRect(QPoint(widgetRect.left(), widgetRect.top() + widgetRect.height() - m_rubberBandLineWidth),
                 QSize(widgetRect.width(), m_rubberBandLineWidth));
  } else if (side == RightOf) {
    return QRect(QPoint(widgetRect.left() + widgetRect.width() - m_rubberBandLineWidth, widgetRect.top()),
                 QSize(m_rubberBandLineWidth, widgetRect.height()));
  } else {
    qWarning("invalid side");
    return QRect();
  }
}

void ToolWindowManager::updateDragPosition() {
  if (!dragInProgress()) { return; }
  if (!(qApp->mouseButtons() & Qt::LeftButton)) {
    finishDrag();
    return;
  }

  QPoint pos = QCursor::pos();
  m_dragIndicator->move(pos + QPoint(1, 1));
  bool foundWrapper = false;

  QWidget* window = qApp->topLevelAt(pos);
  foreach(ToolWindowManagerWrapper* wrapper, m_wrappers) {
    if (wrapper->window() == window) {
      if (wrapper->rect().contains(wrapper->mapFromGlobal(pos))) {
        findSuggestions(wrapper);
        if (!m_suggestions.isEmpty()) {
          //starting or restarting timer
          if (m_dropSuggestionSwitchTimer.isActive()) {
            m_dropSuggestionSwitchTimer.stop();
          }
          m_dropSuggestionSwitchTimer.start();
          foundWrapper = true;
        }
      }
      break;
    }
  }
  if (!foundWrapper) {
    handleNoSuggestions();
  }
}

void ToolWindowManager::finishDrag() {
  if (!dragInProgress()) {
    qWarning("unexpected finishDrag");
    return;
  }
  if (m_suggestions.isEmpty()) {
    if (m_allowFloatingWindow)
    {
      QRect r;
      for(QWidget *w : m_draggedToolWindows)
        r = r.united(w->rect());

      moveToolWindows(m_draggedToolWindows, NewFloatingArea);

      ToolWindowManagerArea *area = areaOf(m_draggedToolWindows[0]);

      area->parentWidget()->resize(r.size());
    }
  } else {
    if (m_dropCurrentSuggestionIndex >= m_suggestions.count()) {
      qWarning("invalid m_dropCurrentSuggestionIndex");
      return;
    }
    ToolWindowManager::AreaReference suggestion = m_suggestions[m_dropCurrentSuggestionIndex];
    handleNoSuggestions();
    moveToolWindows(m_draggedToolWindows, suggestion);
  }


  m_dragIndicator->hide();
  m_draggedToolWindows.clear();
}

void ToolWindowManager::tabCloseRequested(int index) {
  ToolWindowManagerArea* tabWidget = qobject_cast<ToolWindowManagerArea*>(sender());
  if (!tabWidget) {
    qWarning("sender is not a ToolWindowManagerArea");
    return;
  }
  QWidget* toolWindow = tabWidget->widget(index);
  if (!m_toolWindows.contains(toolWindow)) {
    qWarning("unknown tab in tab widget");
    return;
  }

  int methodIndex = toolWindow->metaObject()->indexOfMethod(QMetaObject::normalizedSignature("checkAllowClose()"));

  if(methodIndex >= 0) {
    bool ret = true;
    toolWindow->metaObject()->method(methodIndex).invoke(toolWindow, Qt::DirectConnection, Q_RETURN_ARG(bool, ret));

    if(!ret)
      return;
  }

  if(toolWindowProperties(toolWindow) & ToolWindowManager::HideOnClose)
    hideToolWindow(toolWindow);
  else
    removeToolWindow(toolWindow);
}

void ToolWindowManager::windowTitleChanged(const QString &title) {
  QWidget* toolWindow = qobject_cast<QWidget*>(sender());
  if(!toolWindow) {
    return;
  }
  ToolWindowManagerArea *area = areaOf(toolWindow);
  if(area) {
    area->updateToolWindow(toolWindow);
  }
}

QSplitter *ToolWindowManager::createSplitter() {
  QSplitter* splitter = new QSplitter();
  splitter->setChildrenCollapsible(false);
  return splitter;
}

ToolWindowManager::AreaReference::AreaReference(ToolWindowManager::AreaReferenceType type, ToolWindowManagerArea *area, float percentage) {
  m_type = type;
  m_percentage = percentage;
  setWidget(area);
}

void ToolWindowManager::AreaReference::setWidget(QWidget *widget) {
  if (m_type == LastUsedArea || m_type == NewFloatingArea || m_type == NoArea || m_type == EmptySpace) {
    if (widget != 0) {
      qWarning("area parameter ignored for this type");
    }
    m_widget = 0;
  } else if (m_type == AddTo) {
    m_widget = qobject_cast<ToolWindowManagerArea*>(widget);
    if (!m_widget) {
      qWarning("only ToolWindowManagerArea can be used with this type");
    }
  } else {
    if (!qobject_cast<ToolWindowManagerArea*>(widget) &&
        !qobject_cast<QSplitter*>(widget)) {
      qWarning("only ToolWindowManagerArea or splitter can be used with this type");
      m_widget = 0;
    } else {
      m_widget = widget;
    }
  }
}

ToolWindowManagerArea *ToolWindowManager::AreaReference::area() const {
  return qobject_cast<ToolWindowManagerArea*>(m_widget);
}

ToolWindowManager::AreaReference::AreaReference(ToolWindowManager::AreaReferenceType type, QWidget *widget) {
  m_type = type;
  setWidget(widget);
}
