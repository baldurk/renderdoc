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

#include <QLabel>
#include <QStyledItemDelegate>
#include <QTreeView>
#include "Code/QRDUtils.h"

class RDTreeView;

typedef QSet<uint> RDTreeViewExpansionState;

class RDTreeViewDelegate : public RichTextViewDelegate
{
private:
  Q_OBJECT

  RDTreeView *m_View;

public:
  RDTreeViewDelegate(RDTreeView *view);
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

struct ITreeViewTipDisplay
{
public:
  virtual void hideTip() = 0;
  virtual QSize configureTip(QWidget *widget, QModelIndex idx, QString text) = 0;
  virtual void showTip(QPoint pos) = 0;
  virtual bool forceTip(QWidget *widget, QModelIndex idx) = 0;
};

class RDTipLabel : public QLabel, public ITreeViewTipDisplay
{
private:
  Q_OBJECT

  QWidget *mouseListener;

public:
  explicit RDTipLabel(QWidget *listener = NULL);

  void hideTip() { hide(); }
  QSize configureTip(QWidget *widget, QModelIndex idx, QString text);
  void showTip(QPoint pos);
  bool forceTip(QWidget *widget, QModelIndex idx);

protected:
  void paintEvent(QPaintEvent *);
  void mousePressEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);
  void mouseDoubleClickEvent(QMouseEvent *);
  void resizeEvent(QResizeEvent *);

  void sendListenerEvent(QMouseEvent *e);
};

typedef std::function<uint(QModelIndex, uint)> ExpansionKeyGen;

class RDTreeView : public QTreeView
{
  Q_OBJECT

  Q_PROPERTY(bool customCopyPasteHandler READ customCopyPasteHandler WRITE setCustomCopyPasteHandler)
  Q_PROPERTY(bool instantTooltips READ instantTooltips WRITE setInstantTooltips)
public:
  explicit RDTreeView(QWidget *parent = 0);
  virtual ~RDTreeView();

  static const int TreeLineColorRole = Qt::UserRole + 1000000;

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
  void setIgnoreBackgroundColors(bool ignore) { m_IgnoreBackgroundColors = ignore; }
  void setColoredTreeLineWidth(float w) { m_treeColorLineWidth = w; }
  bool ignoreIconSize() { return m_IgnoreIconSize; }
  bool customCopyPasteHandler() { return m_customCopyPaste; }
  void setCustomCopyPasteHandler(bool custom) { m_customCopyPaste = custom; }
  bool instantTooltips() { return m_instantTooltips; }
  void setInstantTooltips(bool instant) { m_instantTooltips = instant; }
  QModelIndex currentHoverIndex() const { return m_currentHoverIndex; }
  void setItemDelegate(QAbstractItemDelegate *delegate);
  QAbstractItemDelegate *itemDelegate() const;

  void setCustomTooltip(ITreeViewTipDisplay *tip)
  {
    m_Tooltip = tip;
    m_TooltipElidedItems = false;
  }
  void setModel(QAbstractItemModel *model) override;

  // state is the storage to save the expansion state into. The state will be preserved but any rows
  // which are not processed (either because they don't currently exist in the model or because they
  // are below a currently collapsed node) will be preserved. Nodes which are known and collapsed
  // will be removed from the set. This can be useful for preserving expansion across filtering that
  // may temporarily remove nodes.
  // keygen is a function that will take the index of a row and a previous hash, and return the hash
  //   for that row.
  void updateExpansion(RDTreeViewExpansionState &state, const ExpansionKeyGen &keygen);

  // similar to updateExpansion but always starts from a clean slate so no previous data is
  // preserved
  void saveExpansion(RDTreeViewExpansionState &state, const ExpansionKeyGen &keygen)
  {
    state.clear();
    updateExpansion(state, keygen);
  }
  void applyExpansion(const RDTreeViewExpansionState &state, const ExpansionKeyGen &keygen);

  // convenience overloads for the simple case of using a single column's data as hash
  void updateExpansion(RDTreeViewExpansionState &state, int keyColumn, int keyRole = Qt::DisplayRole)
  {
    updateExpansion(state, [keyColumn, keyRole](QModelIndex idx, uint seed) {
      return qHash(idx.sibling(idx.row(), keyColumn).data(keyRole).toString(), seed);
    });
  }
  void saveExpansion(RDTreeViewExpansionState &state, int keyColumn, int keyRole = Qt::DisplayRole)
  {
    saveExpansion(state, [keyColumn, keyRole](QModelIndex idx, uint seed) {
      return qHash(idx.sibling(idx.row(), keyColumn).data(keyRole).toString(), seed);
    });
  }
  void applyExpansion(const RDTreeViewExpansionState &state, int keyColumn,
                      int keyRole = Qt::DisplayRole)
  {
    applyExpansion(state, [keyColumn, keyRole](QModelIndex idx, uint seed) {
      return qHash(idx.sibling(idx.row(), keyColumn).data(keyRole).toString(), seed);
    });
  }

  // Internally tracked expansions
  RDTreeViewExpansionState &getInternalExpansion(uint expansionID)
  {
    return m_Expansions[expansionID];
  }
  bool hasInternalExpansion(uint expansionID) { return m_Expansions.contains(expansionID); }
  void clearInternalExpansions() { m_Expansions.clear(); }
  virtual void copySelection();

  void copyIndex(QPoint pos, QModelIndex index);

  void expandAll(QModelIndex index);
  void collapseAll(QModelIndex index);
  using QTreeView::expandAll;
  using QTreeView::collapseAll;

signals:
  void leave(QEvent *e);
  void keyPress(QKeyEvent *e);

protected:
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  bool viewportEvent(QEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

  void drawRow(QPainter *painter, const QStyleOptionViewItem &options,
               const QModelIndex &index) const override;
  void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;

  QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;

  QModelIndex m_currentHoverIndex;

private slots:
  void modelAboutToBeReset();
  void rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last) override;
  void columnsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
  void rowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
                          const QModelIndex &destinationParent, int destinationRow);
  void columnsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
                             const QModelIndex &destinationParent, int destinationColumn);

private:
  bool m_VisibleBranches = true;
  bool m_VisibleGridLines = true;
  bool m_TooltipElidedItems = true;
  bool m_customCopyPaste = false;
  bool m_instantTooltips = false;

  void expandAllInternal(QModelIndex index);
  void collapseAllInternal(QModelIndex index);

  QMap<uint, RDTreeViewExpansionState> m_Expansions;

  void updateExpansionFromRow(RDTreeViewExpansionState &state, QModelIndex idx, uint seed,
                              const ExpansionKeyGen &keygen);
  void applyExpansionToRow(const RDTreeViewExpansionState &state, QModelIndex idx, uint seed,
                           const ExpansionKeyGen &keygen);

  QAbstractItemDelegate *m_userDelegate = NULL;
  RDTreeViewDelegate *m_delegate;

  RDTipLabel *m_TooltipLabel;
  ITreeViewTipDisplay *m_Tooltip;
  bool m_CurrentTooltipElided = false;

  int m_VertMargin = 6;
  bool m_IgnoreIconSize = false;

  bool m_IgnoreBackgroundColors = false;

  float m_treeColorLineWidth = 1.0f;
};
