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
#include "ToolWindowManager.h"
#include "ToolWindowManagerArea.h"
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDebug>
#include <QApplication>

ToolWindowManagerWrapper::ToolWindowManagerWrapper(ToolWindowManager *manager) :
  QWidget(manager)
, m_manager(manager)
{
  setWindowFlags(windowFlags() | Qt::Tool);
  setWindowTitle(QStringLiteral(" "));

  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  m_manager->m_wrappers << this;
}

ToolWindowManagerWrapper::~ToolWindowManagerWrapper() {
  m_manager->m_wrappers.removeOne(this);
}

void ToolWindowManagerWrapper::closeEvent(QCloseEvent *) {
  QList<QWidget*> toolWindows;
  foreach(ToolWindowManagerArea* tabWidget, findChildren<ToolWindowManagerArea*>()) {
    toolWindows << tabWidget->toolWindows();
  }
  m_manager->moveToolWindows(toolWindows, ToolWindowManager::NoArea);
}

QVariantMap ToolWindowManagerWrapper::saveState() {
  if (layout()->count() > 1) {
    qWarning("too many children for wrapper");
    return QVariantMap();
  }
  if (isWindow() && layout()->count() == 0) {
    qWarning("empty top level wrapper");
    return QVariantMap();
  }
  QVariantMap result;
  result[QStringLiteral("geometry")] = saveGeometry().toBase64();
  QSplitter* splitter = findChild<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);
  if (splitter) {
    result[QStringLiteral("splitter")] = m_manager->saveSplitterState(splitter);
  } else {
    ToolWindowManagerArea* area = findChild<ToolWindowManagerArea*>();
    if (area) {
      result[QStringLiteral("area")] = area->saveState();
    } else if (layout()->count() > 0) {
      qWarning("unknown child");
      return QVariantMap();
    }
  }
  return result;
}

void ToolWindowManagerWrapper::restoreState(const QVariantMap &savedData) {
  restoreGeometry(QByteArray::fromBase64(savedData[QStringLiteral("geometry")].toByteArray()));
  if (layout()->count() > 0) {
    qWarning("wrapper is not empty");
    return;
  }
  if (savedData.contains(QStringLiteral("splitter"))) {
    layout()->addWidget(m_manager->restoreSplitterState(savedData[QStringLiteral("splitter")].toMap()));
  } else if (savedData.contains(QStringLiteral("area"))) {
    ToolWindowManagerArea* area = m_manager->createArea();
    area->restoreState(savedData[QStringLiteral("area")].toMap());
    layout()->addWidget(area);
  }
}
