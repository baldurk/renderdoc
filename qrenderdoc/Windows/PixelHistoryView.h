/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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
#include "Code/CaptureContext.h"

namespace Ui
{
class PixelHistoryView;
}

class CaptureContext;
class PixelHistoryItemModel;
struct EventTag;

class PixelHistoryView : public QFrame, public ILogViewerForm
{
  Q_OBJECT

public:
  explicit PixelHistoryView(CaptureContext &ctx, ResourceId id, QPoint point,
                            const TextureDisplay &display, QWidget *parent = 0);
  ~PixelHistoryView();

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnSelectedEventChanged(uint32_t eventID) {}
  void OnEventChanged(uint32_t eventID) {}
  void setHistory(const rdctype::array<PixelModification> &history);

private slots:
  // automatic slots
  void on_events_customContextMenuRequested(const QPoint &pos);
  void on_events_doubleClicked(const QModelIndex &index);

private:
  Ui::PixelHistoryView *ui;
  CaptureContext &m_Ctx;

  TextureDisplay m_Display;
  QPoint m_Pixel;
  PixelHistoryItemModel *m_Model;
  bool m_ShowFailures = true;
  void startDebug(EventTag tag);
  void jumpToPrimitive(EventTag tag);
};
