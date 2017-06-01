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

#include <QStyledItemDelegate>
#include <QTreeView>

class RDTreeView;

class RDTreeViewDelegate : public QStyledItemDelegate
{
private:
  Q_OBJECT

  RDTreeView *m_View;

public:
  RDTreeViewDelegate(RDTreeView *view);
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class RDTreeView : public QTreeView
{
  Q_OBJECT
public:
  explicit RDTreeView(QWidget *parent = 0);

  void showBranches() { m_VisibleBranches = true; }
  void hideBranches() { m_VisibleBranches = false; }
  void showGridLines() { m_VisibleGridLines = true; }
  void hideGridLines() { m_VisibleGridLines = false; }
  void setItemMargins(int horizontal, int vertical)
  {
    m_HorizMargin = horizontal;
    m_VertMargin = vertical;
  }
  int horizontalItemMargin() { return m_HorizMargin; }
  int verticalItemMargin() { return m_VertMargin; }
protected:
  void drawRow(QPainter *painter, const QStyleOptionViewItem &options,
               const QModelIndex &index) const override;
  void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;

private:
  bool m_VisibleBranches = true;
  bool m_VisibleGridLines = true;

  int m_HorizMargin = 3, m_VertMargin = 3;

  friend class RDTreeViewDelegate;
};
