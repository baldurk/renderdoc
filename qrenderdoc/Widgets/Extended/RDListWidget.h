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
#include <QListWidget>

class RDListWidget : public QListWidget
{
private:
  Q_OBJECT
  Q_PROPERTY(bool customCopyPasteHandler READ customCopyPasteHandler WRITE setCustomCopyPasteHandler)
public:
  explicit RDListWidget(QWidget *parent = 0);
  ~RDListWidget();

  bool customCopyPasteHandler() { return m_customCopyPaste; }
  void setCustomCopyPasteHandler(bool custom) { m_customCopyPaste = custom; }
signals:
  void mouseClicked(QMouseEvent *event);
  void mouseDoubleClicked(QMouseEvent *event);
  void keyPress(QKeyEvent *event);

public slots:

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

  bool m_customCopyPaste = false;
};
