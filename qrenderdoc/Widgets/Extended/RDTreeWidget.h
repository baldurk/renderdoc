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

#include "RDTreeView.h"

class RDTreeWidget;
class RDTreeWidgetModel;

class RDTreeWidgetItem
{
public:
  RDTreeWidgetItem() = default;
  RDTreeWidgetItem(const QVariantList &values);
  RDTreeWidgetItem(const std::initializer_list<QVariant> &values);
  ~RDTreeWidgetItem();

  QVariant data(int column, int role) const;
  void setData(int column, int role, const QVariant &value);

  void addChild(RDTreeWidgetItem *item);
  void insertChild(int index, RDTreeWidgetItem *child);

  // the data above requires allocating a bunch of vectors since it's stored per-column. Where
  // possible, just use this single per-item tag
  inline QVariant tag() const { return m_tag; }
  inline void setTag(const QVariant &value) { m_tag = value; }
  // inline accessors to the private data
  inline void setIcon(int column, const QIcon &icon)
  {
    if(column >= m_icons.size())
    {
      m_text.resize(column + 1);
      m_icons.resize(m_text.size());
    }

    m_icons[column] = icon;
    dataChanged(column, Qt::DecorationRole);
  }
  inline RDTreeWidgetItem *child(int index) const { return m_children[index]; }
  inline int indexOfChild(RDTreeWidgetItem *child) const { return m_children.indexOf(child); }
  RDTreeWidgetItem *takeChild(int index);
  void removeChild(RDTreeWidgetItem *child);
  void clear();
  inline int dataCount() const { return m_text.count(); }
  inline int childCount() const { return m_children.count(); }
  inline RDTreeWidgetItem *parent() const { return m_parent; }
  inline RDTreeWidget *treeWidget() const { return m_widget; }
  inline void setBold(bool bold)
  {
    m_bold = bold;
    dataChanged(0, Qt::FontRole);
  }
  inline void setItalic(bool italic)
  {
    m_italic = italic;
    dataChanged(0, Qt::FontRole);
  }
  inline void setTreeColor(QColor col, float pixels)
  {
    m_treeCol = col;
    m_treeColWidth = pixels;
  }
  inline void setBackgroundColor(QColor background) { setBackground(QBrush(background)); }
  inline void setForegroundColor(QColor foreground) { setForeground(QBrush(foreground)); }
  inline void setBackground(QBrush background)
  {
    m_back = background;
    dataChanged(0, Qt::BackgroundRole);
  }
  inline void setForeground(QBrush foreground)
  {
    m_fore = foreground;
    dataChanged(0, Qt::ForegroundRole);
  }
  inline QBrush background() { return m_back; }
  inline QBrush foreground() { return m_fore; }
  inline QString text(int column) const { return m_text[column].toString(); }
  inline void setText(int column, const QVariant &value)
  {
    if(column >= m_text.size())
    {
      m_text.resize(column + 1);
      m_icons.resize(m_text.size());
    }

    m_text[column] = value;
    checkForResourceId(column);
    dataChanged(column, Qt::DisplayRole);
  }
  inline void setToolTip(const QString &value)
  {
    m_tooltip = value;
    dataChanged(0, Qt::ToolTipRole);
  }

  inline Qt::CheckState checkState(int column) const
  {
    return static_cast<Qt::CheckState>(data(column, Qt::CheckStateRole).toInt());
  }
  inline void setCheckState(int column, Qt::CheckState state)
  {
    setData(column, Qt::CheckStateRole, static_cast<int>(state));
    dataChanged(column, Qt::CheckStateRole);
  }

private:
  void checkForResourceId(int column);

  void sort(int column, Qt::SortOrder order);

  friend class RDTreeWidget;
  friend class RDTreeWidgetModel;
  friend class RDTreeWidgetDelegate;

  void setWidget(RDTreeWidget *widget);
  RDTreeWidget *m_widget = NULL;

  RDTreeWidgetItem *m_parent = NULL;
  QVector<RDTreeWidgetItem *> m_children;

  struct RoleData
  {
    RoleData() : role(0), data() {}
    RoleData(int r, const QVariant &d) : role(r), data(d) {}
    int role;
    QVariant data;
  };

  void dataChanged(int column, int role);

  // per-column properties
  QVector<QVariant> m_text;
  QVector<QIcon> m_icons;
  // each element, per-column, is a list of other data values
  // we allocate this lazily only if it's really needed
  QVector<QVector<RoleData>> *m_data = NULL;

  // per-item properties
  QString m_tooltip;
  bool m_bold = false;
  bool m_italic = false;
  QColor m_treeCol;
  float m_treeColWidth = 0.0f;
  QBrush m_back;
  QBrush m_fore;
  QVariant m_tag;
};

class RDTreeWidgetItemIterator
{
public:
  RDTreeWidgetItemIterator(RDTreeWidget *widget);

  RDTreeWidgetItemIterator &operator++();
  inline const RDTreeWidgetItemIterator operator++(int)
  {
    RDTreeWidgetItemIterator it = *this;
    ++(*this);
    return it;
  }

  // TODO implement operator-- if we need it.
  /*
  RDTreeWidgetItemIterator &operator--();
  inline const RDTreeWidgetItemIterator operator--(int)
  {
    RDTreeWidgetItemIterator it = *this;
    --(*this);
    return it;
  }
  */

  inline RDTreeWidgetItem *operator*() const { return m_Current; }
private:
  RDTreeWidgetItem *m_Current;
};

class RichTextViewDelegate;

class RDTreeWidget : public RDTreeView
{
  Q_OBJECT

  Q_PROPERTY(bool instantTooltips READ instantTooltips WRITE setInstantTooltips)
  Q_PROPERTY(bool customCopyPasteHandler READ customCopyPasteHandler WRITE setCustomCopyPasteHandler)
public:
  explicit RDTreeWidget(QWidget *parent = 0);
  ~RDTreeWidget();

  void setHoverIconColumn(int column, QIcon normal, QIcon hover)
  {
    m_hoverColumn = column;
    m_normalHoverIcon = normal;
    m_activeHoverIcon = hover;
    m_hoverHandCursor = true;
    m_activateOnClick = true;
  }
  void setHoverHandCursor(bool hand) { m_hoverHandCursor = hand; }
  void setHoverClickActivate(bool click) { m_activateOnClick = click; }
  void setClearSelectionOnFocusLoss(bool clear) { m_clearSelectionOnFocusLoss = clear; }
  bool instantTooltips() { return m_instantTooltips; }
  void setInstantTooltips(bool instant) { m_instantTooltips = instant; }
  bool customCopyPasteHandler() { return m_customCopyPaste; }
  void setCustomCopyPasteHandler(bool custom) { m_customCopyPaste = custom; }
  RDTreeWidgetItem *invisibleRootItem() { return m_root; }
  void addTopLevelItem(RDTreeWidgetItem *item) { m_root->addChild(item); }
  RDTreeWidgetItem *topLevelItem(int index) const { return m_root->child(index); }
  int indexOfTopLevelItem(RDTreeWidgetItem *item) const { return m_root->indexOfChild(item); }
  RDTreeWidgetItem *takeTopLevelItem(int index) { return m_root->takeChild(index); }
  int topLevelItemCount() const { return m_root->childCount(); }
  void beginUpdate();
  void endUpdate();
  void setColumnAlignment(int column, Qt::Alignment align);

  void setItemDelegate(QAbstractItemDelegate *delegate);
  QAbstractItemDelegate *itemDelegate() const;

  void setColumns(const QStringList &columns);
  QString headerText(int column) const { return m_headers[column]; }
  void setHeaderText(int column, const QString &text);
  RDTreeWidgetItem *selectedItem() const;
  RDTreeWidgetItem *currentItem() const;
  void setSelectedItem(RDTreeWidgetItem *node);
  void setCurrentItem(RDTreeWidgetItem *node);

  RDTreeWidgetItem *itemAt(const QPoint &p) const;
  RDTreeWidgetItem *itemAt(int x, int y) const { return itemAt(QPoint(x, y)); }
  void expandItem(RDTreeWidgetItem *item);
  void expandAllItems(RDTreeWidgetItem *item);
  void collapseItem(RDTreeWidgetItem *item);
  void collapseAllItems(RDTreeWidgetItem *item);
  void scrollToItem(RDTreeWidgetItem *node);

  void copySelection();

  void clear();

signals:
  void mouseMove(QMouseEvent *e);
  void itemClicked(RDTreeWidgetItem *item, int column);
  void itemChanged(RDTreeWidgetItem *item, int column);
  void itemDoubleClicked(RDTreeWidgetItem *item, int column);
  void itemActivated(RDTreeWidgetItem *item, int column);
  void currentItemChanged(RDTreeWidgetItem *current, RDTreeWidgetItem *previous);
  void itemSelectionChanged();

public slots:

private:
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void focusOutEvent(QFocusEvent *event) override;
  void keyPressEvent(QKeyEvent *e) override;
  void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;

  void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
  void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

  void setModel(QAbstractItemModel *model) override {}
  void itemDataChanged(RDTreeWidgetItem *item, int column, int role);
  void beginInsertChild(RDTreeWidgetItem *item, int index);
  void endInsertChild(RDTreeWidgetItem *item, int index);

  friend class RDTreeWidgetModel;
  friend class RDTreeWidgetItem;
  friend class RDTreeWidgetDelegate;

  // invisible root item, used to simplify recursion by even top-level items having a parent
  RDTreeWidgetItem *m_root;

  RDTreeWidgetModel *m_model;

  QAbstractItemDelegate *m_userDelegate = NULL;
  RichTextViewDelegate *m_delegate;

  bool m_clearing = false;

  QStringList m_headers;

  bool m_queueUpdates = false;

  RDTreeWidgetItem *m_queuedItem;
  QPair<int, int> m_lowestIndex;
  QPair<int, int> m_highestIndex;
  uint64_t m_queuedRoles = 0;
  bool m_queuedChildren = false;

  QVector<Qt::Alignment> m_alignments;

  bool m_instantTooltips = false;
  bool m_customCopyPaste = false;
  int m_hoverColumn = -1;
  QIcon m_normalHoverIcon;
  QIcon m_activeHoverIcon;
  bool m_hoverHandCursor = false;
  bool m_clearSelectionOnFocusLoss = false;
  bool m_activateOnClick = false;
};
