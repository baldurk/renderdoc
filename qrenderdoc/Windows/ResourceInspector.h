/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include <QFrame>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class ResourceInspector;
}

class QCollatorSortFilterProxyModel;

class RDTreeWidgetItem;
class ResourceListItemModel;

class ResourceInspector : public QFrame, public IResourceInspector, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit ResourceInspector(ICaptureContext &ctx, QWidget *parent = 0);
  ~ResourceInspector();

  // IResourceInspector
  QWidget *Widget() override { return this; }
  void Inspect(ResourceId id) override;
  ResourceId CurrentResource() override { return m_Resource; }
  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;
public slots:
  // automatic slots
  void on_renameResource_clicked();
  void on_resourceNameEdit_keyPress(QKeyEvent *event);
  void on_resetName_clicked();

  void on_cancelResourceListFilter_clicked();
  void on_resourceListFilter_textChanged(const QString &text);

  // manual slots
  void resource_doubleClicked(const QModelIndex &index);

private slots:
  void on_viewContents_clicked();
  void on_resourceUsage_doubleClicked(const QModelIndex &index);

protected:
  void enterEvent(QEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void HighlightUsage();

  Ui::ResourceInspector *ui;
  ICaptureContext &m_Ctx;

  rdcarray<ShaderEntryPoint> m_Entries;

  ResourceId m_Resource;
  ResourceListItemModel *m_ResourceModel;
  int m_ResourceCacheID = -1;
  QCollatorSortFilterProxyModel *m_FilterModel;
};
