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
#include "renderdoc_replay.h"

namespace Ui
{
class ResourcePreview;
}

struct IReplayOutput;
struct ICaptureContext;
class RDLabel;

class ResourcePreview : public QFrame
{
  Q_OBJECT

public:
  explicit ResourcePreview(ICaptureContext &c, IReplayOutput *output, QWidget *parent = 0);
  // create a manually rendered preview
  explicit ResourcePreview(bool, QWidget *parent = 0);

  void Initialise();

  ~ResourcePreview();

signals:
  void clicked(QMouseEvent *e);
  void doubleClicked(QMouseEvent *e);
  void resized(ResourcePreview *prev);

public:
  void setSlotName(const QString &n);
  void setResourceName(const QString &n);

  void clickEvent(QMouseEvent *e);
  void doubleClickEvent(QMouseEvent *e);

  WindowingData GetWidgetWindowingData();
  QSize GetThumbSize();
  void UpdateThumb(QSize s, const bytebuf &imgData);

  void setActive(bool b)
  {
    m_Active = b;
    // we unconditionally hide the preview, the thumbnail strip will show it
    hide();
  }
  bool isActive() { return m_Active; }
  void setSize(QSize s);

  void setSelected(bool sel);

private:
  virtual void resizeEvent(QResizeEvent *event);

  Ui::ResourcePreview *ui;

  RDLabel *m_ManualThumbnail = NULL;

  bool m_Active;
  bool m_Selected = false;
};
