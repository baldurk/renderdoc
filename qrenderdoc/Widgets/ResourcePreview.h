/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

namespace Ui
{
class ResourcePreview;
}

struct IReplayOutput;
struct ICaptureContext;

class ResourcePreview : public QFrame
{
  Q_OBJECT

public:
  explicit ResourcePreview(ICaptureContext &c, IReplayOutput *output, QWidget *parent = 0);
  ~ResourcePreview();

signals:
  void clicked(QMouseEvent *e);
  void doubleClicked(QMouseEvent *e);

public:
  void setSlotName(const QString &n);
  void setResourceName(const QString &n);

  void clickEvent(QMouseEvent *e);
  void doubleClickEvent(QMouseEvent *e);

  QWidget *thumbWidget();

  void setActive(bool b)
  {
    m_Active = b;
    if(b)
      show();
    else
      hide();
  }
  bool isActive() { return m_Active; }
  void setSize(QSize s);

  void setSelected(bool sel);

protected:
  void changeEvent(QEvent *event) override;

private:
  Ui::ResourcePreview *ui;

  bool m_Active;
  bool m_Selected = false;
};
