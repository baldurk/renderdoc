/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include <QDialog>
#include <QList>
#include <QSemaphore>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class ExtensionManager;
}

class RDTreeWidgetItem;
struct ICaptureContext;
class MainWindow;

class ExtensionManager : public QDialog
{
  Q_OBJECT

public:
  explicit ExtensionManager(ICaptureContext &ctx);
  ~ExtensionManager();

private slots:
  // automatic slots
  void on_reload_clicked();
  void on_openLocation_clicked();
  void on_alwaysLoad_toggled(bool checked);
  void on_extensions_currentItemChanged(RDTreeWidgetItem *item, RDTreeWidgetItem *);
  void on_extensions_itemChanged(RDTreeWidgetItem *item, int col);

private:
  void update_currentItem(RDTreeWidgetItem *item);

  Ui::ExtensionManager *ui;
  ICaptureContext &m_Ctx;

  rdcarray<ExtensionMetadata> m_Extensions;
};
