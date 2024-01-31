/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include <QTimer>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class LogView;
}

class QStandardItem;
class QStandardItemModel;
class QAction;
class QMenu;
class LogItemModel;
class LogFilterModel;
class RichTextViewDelegate;

struct LogMessage
{
  QString Source;
  uint32_t PID;
  QTime Timestamp;
  QString Location;
  LogType Type;
  QString Message;
};

class LogView : public QFrame, public IDiagnosticLogView
{
  Q_OBJECT

public:
  explicit LogView(ICaptureContext &ctx, QWidget *parent = 0);
  ~LogView();

  // ILogView
  QWidget *Widget() override { return this; }
private slots:
  // automatic slots
  void on_openExternal_clicked();
  void on_save_clicked();
  void on_textFilter_textChanged(const QString &text);
  void on_textFilterMeaning_currentIndexChanged(int index);
  void on_regexpFilter_toggled();

  // manual slots
  void messages_refresh();
  void messages_keyPress(QKeyEvent *event);
  void pidFilter_changed(QStandardItem *item);
  void typeFilter_changed(QStandardItem *item);

private:
  bool eventFilter(QObject *watched, QEvent *event) override;

  Ui::LogView *ui;
  ICaptureContext &m_Ctx;

  uint64_t prevOffset = 0;

  QVector<LogMessage> m_Messages;

  QList<uint32_t> m_PIDs;

  RichTextViewDelegate *m_delegate = NULL;

  LogItemModel *m_ItemModel = NULL;
  LogFilterModel *m_FilterModel = NULL;

  friend class LogItemModel;
  friend class LogFilterModel;

  QStandardItemModel *m_PIDModel = NULL;
  QStandardItemModel *m_TypeModel = NULL;

  QTimer m_RefreshTimer;
};
