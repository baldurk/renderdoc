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
class DebugMessageView;
}

class QAction;
class QMenu;
class DebugMessageItemModel;
class DebugMessageFilterModel;

class DebugMessageView : public QFrame, public IDebugMessageView, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit DebugMessageView(ICaptureContext &ctx, QWidget *parent = 0);
  ~DebugMessageView();

  // IDebugMessageView
  QWidget *Widget() override { return this; }
  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override {}
  void RefreshMessageList();

private slots:
  // automatic slots
  void on_messages_doubleClicked(const QModelIndex &index);

  // manual slots
  void messages_contextMenu(const QPoint &pos);
  void messages_toggled();

private:
  void paintEvent(QPaintEvent *e) override;
  Ui::DebugMessageView *ui;
  ICaptureContext &m_Ctx;

  QVector<DebugMessage> m_Messages;
  DebugMessageItemModel *m_ItemModel;
  DebugMessageFilterModel *m_FilterModel;

  DebugMessage m_ContextMessage;
  QMenu *m_ContextMenu;
  QAction *m_ShowHidden;
  QAction *m_ToggleSource;
  QAction *m_ToggleSeverity;
  QAction *m_ToggleCategory;
  QAction *m_ToggleMessageType;
};
