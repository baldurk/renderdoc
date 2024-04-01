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

#include <QTableView>
#include "RDHeaderView.h"

class RichTextViewDelegate;

class RDTableView : public QTableView
{
  Q_OBJECT
public:
  explicit RDTableView(QWidget *parent = 0);

  // these aren't virtual so we can't override them properly, but it's convenient for internal use
  // and any external calls that go to this type directly to use the correct version
  RDHeaderView *horizontalHeader() const { return m_horizontalHeader; }
  int columnViewportPosition(int column) const;
  int columnAt(int x) const;
  int columnWidth(int column) const;
  void setColumnWidth(int column, int width);
  void setColumnWidths(const QList<int> &widths);
  void resizeColumnsToContents();

  void copyIndices(const QModelIndexList &sel);
  void copySelectedIndices();

  void setAllowKeyboardSearches(bool allow) { m_allowKeyboardSearches = allow; }
  bool allowKeyboardSearches() const { return m_allowKeyboardSearches; }
  void keyboardSearch(const QString &search) override;

  void setCustomHeaderSizing(bool sizing) { m_horizontalHeader->setCustomSizing(sizing); }
  void setItemDelegate(QAbstractItemDelegate *delegate);
  QAbstractItemDelegate *itemDelegate() const;

  // these ones we CAN override, so even though the implementation is identical to QTableView we
  // reimplement so it can pick up the above functions
  QRect visualRect(const QModelIndex &index) const override;
  QRegion visualRegionForSelection(const QItemSelection &selection) const override;
  QModelIndex indexAt(const QPoint &p) const override;
  void scrollTo(const QModelIndex &index, ScrollHint hint = QAbstractItemView::EnsureVisible) override;

  void setColumnGroupRole(int role);
  int columnGroupRole() const { return m_columnGroupRole; }
  QStyleOptionViewItem viewOptions() const override { return QTableView::viewOptions(); }
  void setPinnedColumns(int numColumns);
  int pinnedColumns() const { return m_pinnedColumns; }
protected:
  void mouseMoveEvent(QMouseEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void paintEvent(QPaintEvent *e) override;
  void updateGeometries() override;
  void scrollContentsBy(int dx, int dy) override;

  void paintCell(QPainter *painter, const QModelIndex &index, const QStyleOptionViewItem &opt);

private:
  int m_pinnedColumns = 0;
  int m_columnGroupRole = 0;

  bool m_allowKeyboardSearches = true;

  RDHeaderView *m_horizontalHeader;

  QModelIndex m_currentHoverIndex;

  QAbstractItemDelegate *m_userDelegate = NULL;
  RichTextViewDelegate *m_delegate;

  friend class RichTextViewDelegate;
};
