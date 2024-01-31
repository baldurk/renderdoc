/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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
#include <QStyledItemDelegate>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class ShaderMessageViewer;
}

class ButtonDelegate : public QStyledItemDelegate
{
private:
  Q_OBJECT

  QModelIndex m_ClickedIndex;
  QIcon m_Icon;
  int m_EnableRole;

public:
  ButtonDelegate(const QIcon &icon, int enableRole, QWidget *parent);
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
  bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;
signals:
  void messageClicked(const QModelIndex &index);
};

class ShaderMessageViewer : public QFrame, public IShaderMessageViewer, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit ShaderMessageViewer(ICaptureContext &ctx, ShaderStageMask stages, QWidget *parent = 0);
  ~ShaderMessageViewer();

  // IShaderMessageViewer
  QWidget *Widget() override { return this; }
  uint32_t GetEvent() override { return m_EID; };
  rdcarray<ShaderMessage> GetShaderMessages() override { return m_Messages; };
  bool IsOutOfDate() override;

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;
private slots:

  void exportText();
  void exportCSV();

private:
  void refreshMessages();
  void exportData(bool csv);

  Ui::ShaderMessageViewer *ui;
  ICaptureContext &m_Ctx;

  ButtonDelegate *m_debugDelegate = NULL;
  ButtonDelegate *m_gotoDelegate = NULL;

  bool m_Multiview = false, m_Multisampled = false;

  GraphicsAPI m_API;
  uint32_t m_EID;
  const ActionDescription *m_Action;
  rdcarray<ShaderMessage> m_Messages;

  ShaderStage m_LayoutStage;
  ResourceId m_OrigShaders[NumShaderStages];
  ResourceId m_ReplacedShaders[NumShaderStages];
};
