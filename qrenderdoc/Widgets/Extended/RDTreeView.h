/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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

#include <QLabel>
#include <QStyledItemDelegate>
#include <QTreeView>
#include "Code/QRDUtils.h"

class RDTreeView;

typedef QSet<uint> RDTreeViewExpansionState;

class RDTreeViewDelegate : public ForwardingDelegate
{
private:
  Q_OBJECT

  // I hate the mutable keyword, but it's necessary to modify it inside sizeHint to access in
  // initStyleOption. There's no other way to only remove the icon while in sizeHint, and not in
  // other places that call initStyleOption. We don't want to try and manually init, since if
  // there's a forwarded delegate it might do something different.
  mutable bool m_suppressIcon = false;
  RDTreeView *m_View;

public:
  RDTreeViewDelegate(RDTreeView *view);
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected:
  void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
};

class RDTipLabel : public QLabel
{
private:
  Q_OBJECT

  int m_TooltipMargin = 0;
  QWidget *mouseListener;

public:
  explicit RDTipLabel(QWidget *listener = NULL);

  int tipMargin() { return m_TooltipMargin; }
protected:
  void paintEvent(QPaintEvent *);
  void mousePressEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);
  void mouseDoubleClickEvent(QMouseEvent *);
  void resizeEvent(QResizeEvent *);

  void sendListenerEvent(QMouseEvent *e);
};

class RDTreeView : public QTreeView
{
  Q_OBJECT
public:
  explicit RDTreeView(QWidget *parent = 0);
  virtual ~RDTreeView();

  void showBranches() { m_VisibleBranches = true; }
  void hideBranches() { m_VisibleBranches = false; }
  void showGridLines() { m_VisibleGridLines = true; }
  void hideGridLines() { m_VisibleGridLines = false; }
  bool visibleGridLines() { return m_VisibleGridLines; }
  void setTooltipElidedItems(bool tool) { m_TooltipElidedItems = tool; }
  bool tooltipElidedItems() { return m_TooltipElidedItems; }
  void setItemVerticalMargin(int vertical) { m_VertMargin = vertical; }
  int verticalItemMargin() { return m_VertMargin; }
  void setIgnoreIconSize(bool ignore) { m_IgnoreIconSize = ignore; }
  bool ignoreIconSize() { return m_IgnoreIconSize; }
  void setItemDelegate(QAbstractItemDelegate *delegate);
  QAbstractItemDelegate *itemDelegate() const;

  void saveInternalExpansion(uint key, int keyColumn, int role = Qt::DisplayRole);
  bool hasInternalExpansion(uint key);
  void applyInternalExpansion(uint key, int keyColumn, int role = Qt::DisplayRole);
  void clearInternalExpansions();

  void saveExpansionExternal(RDTreeViewExpansionState &state, uint key, int keyColumn,
                             int role = Qt::DisplayRole);
  void applyExternalExpansion(const RDTreeViewExpansionState &state, uint key, int keyColumn,
                              int role = Qt::DisplayRole);

  // convenience overloads taking a string as a key that just hashes the string
  void saveInternalExpansion(QString keystring, int keyColumn, int role = Qt::DisplayRole)
  {
    saveInternalExpansion(qHash(keystring), keyColumn, role);
  }

  bool hasInternalExpansion(QString keystring) { return hasInternalExpansion(qHash(keystring)); }
  void applyInternalExpansion(QString keystring, int keyColumn, int role = Qt::DisplayRole)
  {
    applyInternalExpansion(qHash(keystring), keyColumn, role);
  }

  void saveExpansionExternal(RDTreeViewExpansionState &state, QString keystring, int keyColumn,
                             int role = Qt::DisplayRole)
  {
    saveExpansionExternal(state, qHash(keystring), keyColumn, role);
  }
  void applyExternalExpansion(const RDTreeViewExpansionState &state, QString keystring,
                              int keyColumn, int role = Qt::DisplayRole)
  {
    applyExternalExpansion(state, qHash(keystring), keyColumn, role);
  }

signals:
  void leave(QEvent *e);
  void keyPress(QKeyEvent *e);

protected:
  void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  bool viewportEvent(QEvent *event) override;

  void drawRow(QPainter *painter, const QStyleOptionViewItem &options,
               const QModelIndex &index) const override;
  void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;

  void fillBranchesRect(QPainter *painter, const QRect &rect, const QModelIndex &index) const;
  void enableBranchRectFill(bool fill) { m_fillBranchRect = fill; }
  QModelIndex m_currentHoverIndex;

private:
  bool m_VisibleBranches = true;
  bool m_VisibleGridLines = true;
  bool m_TooltipElidedItems = true;

  QMap<uint, RDTreeViewExpansionState> m_Expansions;

  void saveExpansion(RDTreeViewExpansionState &state, QModelIndex idx, uint seed, int keyColumn,
                     int role);
  void applyExpansion(const RDTreeViewExpansionState &state, QModelIndex idx, uint seed,
                      int keyColumn, int role);

  QAbstractItemDelegate *m_userDelegate = NULL;
  RDTreeViewDelegate *m_delegate;

  RDTipLabel *m_ElidedTooltip;

  int m_VertMargin = 6;
  bool m_IgnoreIconSize = false;

  bool m_fillBranchRect = true;
};
