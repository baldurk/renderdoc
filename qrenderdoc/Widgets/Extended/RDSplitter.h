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

#include <QSplitter>
#include <QString>

// a splitter handle.
// it draws a text as the title and an arrow indicating if it's collapsed
// You need to set a title and its index.
class RDSplitterHandle : public QSplitterHandle
{
  Q_OBJECT

public:
  RDSplitterHandle(Qt::Orientation orientation, QSplitter *parent);

  void setIndex(int index);
  int index() const;

  void setTitle(const QString &title);
  const QString &title() const;

  void setCollapsed(bool collapsed);
  bool collapsed() const;

protected:
  virtual void paintEvent(QPaintEvent *event);
  virtual void mouseDoubleClickEvent(QMouseEvent *event);

  QString m_title;
  int m_index;
  bool m_isCollapsed;
  QPoint m_arrowPoints[3];
};

// A Splitter that contains RDSplitterHandles
// when setting up, you need to get the handles for every index
// and set their title as well as their indexes
class RDSplitter : public QSplitter
{
  Q_OBJECT

public:
  RDSplitter(QWidget *parent = 0);
  RDSplitter(Qt::Orientation orientation, QWidget *parent = 0);

  void handleDoubleClicked(int index);

protected slots:
  void setHandleCollapsed(int pos, int index);

protected:
  void initialize();
  virtual QSplitterHandle *createHandle();
};
