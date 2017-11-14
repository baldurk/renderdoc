/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "RDTreeWidget.h"
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDebug>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QStack>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolTip>
#include "Code/Interface/QRDInterface.h"
#include "Code/Resources.h"

// this is used often, so cache it
QRegularExpression &ResourceIdRegexp()
{
  static QRegularExpression re(lit("(resourceid::)([0-9]*)"));
  return re;
}

struct ResourceIdLinkedText
{
  QVector<QVariant> fragments;

  // cached formatted document. We use cacheId to check if it needs to be updated
  QTextDocument doc;
  int cacheId = 0;

  // a plain-text version of the document, suitable for e.g. copy-paste
  QString text;

  void cacheDocument(QWidget *widget)
  {
    ICaptureContext *ctxptr = getCaptureContext(widget);

    if(!ctxptr)
      return;

    ICaptureContext &ctx = *(ICaptureContext *)ctxptr;

    int refCache = ctx.ResourceNameCacheID();

    if(cacheId == refCache)
      return;

    cacheId = refCache;

    // use a table to ensure images don't screw up the baseline for text. DON'T JUDGE ME.
    QString html = lit("<table><tr>");

    int i = 0;

    QVector<int> fragmentIndexFromBlockIndex;

    // there's an empty block at the start.
    fragmentIndexFromBlockIndex.push_back(-1);

    for(const QVariant &v : fragments)
    {
      if(v.userType() == qMetaTypeId<ResourceId>())
      {
        html += lit("<td><b>%1</b></td><td><img src=':/link.png'></td>")
                    .arg(ctx.GetResourceName(v.value<ResourceId>()));

        // these generate two blocks (one for each cell)
        fragmentIndexFromBlockIndex.push_back(i);
        fragmentIndexFromBlockIndex.push_back(i);
      }
      else
      {
        html += lit("<td>%1</td>").arg(v.toString());

        // this only generates one block
        fragmentIndexFromBlockIndex.push_back(i);
      }

      i++;
    }

    // there's another empty block at the end
    fragmentIndexFromBlockIndex.push_back(-1);

    html += lit("</tr></table>");

    doc.setDocumentMargin(0);
    doc.setHtml(html);

    if(doc.blockCount() != fragmentIndexFromBlockIndex.count())
    {
      qCritical() << "Block count is not what's expected!" << doc.blockCount()
                  << fragmentIndexFromBlockIndex.count();

      for(i = 0; i < doc.blockCount(); i++)
        doc.findBlockByNumber(i).setUserState(-1);

      return;
    }

    for(i = 0; i < doc.blockCount(); i++)
      doc.findBlockByNumber(i).setUserState(fragmentIndexFromBlockIndex[i]);

    text = doc.toPlainText();
    // "Embedded objects, such as images, are represented by a Unicode value U+FFFC (OBJECT
    // REPLACEMENT CHARACTER)."
    text.remove(QChar(0xfffc));
    text = text.trimmed();
  }
};

// using QSharedPointer here since the lifetime management of these objects would get quite
// complicated, and there is no obvious QObject parent to assign to. The alternative would be to
// have the RDTreeWidgetItem track pointers in and out of the QVariant text
typedef QSharedPointer<ResourceIdLinkedText> ResourceIdLinkedTextPtr;

Q_DECLARE_METATYPE(ResourceIdLinkedTextPtr);

class RDTreeWidgetModel : public QAbstractItemModel
{
public:
  RDTreeWidgetModel(RDTreeWidget *parent) : QAbstractItemModel(parent), widget(parent) {}
  QModelIndex indexForItem(RDTreeWidgetItem *item, int column) const
  {
    if(item->m_parent == NULL)
      return QModelIndex();

    int row = item->m_parent->indexOfChild(item);

    return createIndex(row, column, item);
  }

  RDTreeWidgetItem *itemForIndex(QModelIndex idx) const
  {
    if(!idx.isValid())
      return widget->m_root;

    return (RDTreeWidgetItem *)idx.internalPointer();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
      return QModelIndex();

    RDTreeWidgetItem *par = itemForIndex(parent);

    if(par == NULL)
      par = widget->m_root;

    return createIndex(row, column, par->m_children[row]);
  }

  void beginAddChild(RDTreeWidgetItem *item)
  {
    QModelIndex index = indexForItem(item, 0);
    beginInsertRows(index, item->childCount(), item->childCount());
  }

  void endAddChild(RDTreeWidgetItem *item) { endInsertRows(); }
  void beginRemoveChildren(RDTreeWidgetItem *parent, int first, int last)
  {
    beginRemoveRows(indexForItem(parent, 0), first, last);
  }

  void endRemoveChildren() { endRemoveRows(); }
  void itemChanged(RDTreeWidgetItem *item, const QVector<int> &roles)
  {
    QModelIndex topLeft = indexForItem(item, 0);
    QModelIndex bottomRight = indexForItem(item, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight, roles);
  }

  void refresh()
  {
    emit beginResetModel();
    emit endResetModel();
  }

  void headerRefresh() { emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1); }
  void itemsChanged(RDTreeWidgetItem *p, QPair<int, int> minRowColumn, QPair<int, int> maxRowColumn,
                    const QVector<int> &roles)
  {
    QModelIndex topLeft = createIndex(minRowColumn.first, minRowColumn.second, p);
    QModelIndex bottomRight = createIndex(maxRowColumn.first, maxRowColumn.second, p);
    emit dataChanged(topLeft, bottomRight, roles);
  }

  QModelIndex parent(const QModelIndex &index) const override
  {
    if(index.internalPointer() == NULL)
      return QModelIndex();

    RDTreeWidgetItem *item = itemForIndex(index);

    if(item)
      return indexForItem(item->m_parent, 0);

    return QModelIndex();
  }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    if(!parent.isValid())
      return widget->m_root->childCount();

    RDTreeWidgetItem *parentItem = itemForIndex(parent);

    if(parentItem)
      return parentItem->childCount();
    return 0;
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return widget->m_headers.count();
  }

  bool hasChildren(const QModelIndex &parent) const override
  {
    if(!parent.isValid())
      return widget->m_root->childCount() > 0;

    RDTreeWidgetItem *parentItem = itemForIndex(parent);

    if(parentItem)
      return parentItem->childCount() > 0;
    return false;
  }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index) | Qt::ItemIsUserCheckable;
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole && section < widget->m_headers.count())
      return widget->m_headers[section];

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    RDTreeWidgetItem *item = itemForIndex(index);

    // invisible root element has no data
    if(!item->m_parent || index.column() >= item->m_text.count())
      return QVariant();

    if(role == Qt::DisplayRole)
    {
      return item->m_text[index.column()];
    }
    else if(role == Qt::DecorationRole)
    {
      if(widget->m_hoverColumn == index.column())
      {
        if(itemForIndex(widget->m_currentHoverIndex) == item)
          return widget->m_activeHoverIcon;
        else
          return widget->m_normalHoverIcon;
      }

      return item->m_icons[index.column()];
    }
    else if(role == Qt::BackgroundRole)
    {
      // item's background color takes priority but only if not selected
      if(item->m_back != QBrush() && !widget->selectionModel()->isSelected(index))
        return item->m_back;

      // otherwise if we're hover-highlighting, use the highlight color at 20% opacity
      if(itemForIndex(widget->m_currentHoverIndex) == item && widget->m_hoverColumn >= 0)
      {
        QColor col = widget->palette().color(QPalette::Highlight);
        col.setAlphaF(0.2f);
        return QBrush(col);
      }

      // otherwise, no special background
      return QVariant();
    }
    else if(role == Qt::ForegroundRole)
    {
      // same priority order as background role above
      if(item->m_fore != QBrush())
        return item->m_fore;

      return QVariant();
    }
    else if(role == Qt::ToolTipRole && !widget->m_instantTooltips)
    {
      return item->m_tooltip;
    }
    else if(role == Qt::FontRole)
    {
      QFont font;    // TODO should this come from some default?
      font.setItalic(item->m_italic);
      font.setBold(item->m_bold);
      return font;
    }
    else if(role == Qt::TextAlignmentRole)
    {
      if(index.column() < widget->m_alignments.count())
      {
        Qt::Alignment align = widget->m_alignments[index.column()];

        if(align != 0)
          return QVariant(align);
      }
    }
    else if(role < 64 && item->m_customData & 1ULL << role)
    {
      return item->data(index.column(), role);
    }
    else if(role >= Qt::UserRole)
    {
      return item->data(index.column(), role);
    }

    return QVariant();
  }

  bool setData(const QModelIndex &index, const QVariant &value, int role) override
  {
    RDTreeWidgetItem *item = itemForIndex(index);

    // invisible root element has no data
    if(!item->m_parent)
      return false;

    bool ret = false;

    if(role == Qt::DisplayRole)
    {
      if(index.column() < item->m_text.count())
      {
        item->m_text[index.column()] = value;
        ret = true;
      }
    }
    else if(role == Qt::DecorationRole)
    {
      if(index.column() < item->m_icons.count())
      {
        item->m_icons[index.column()] = value.value<QIcon>();
        ret = true;
      }
    }
    else if(role == Qt::BackgroundRole)
    {
      item->m_back = value.value<QBrush>();
      ret = true;
    }
    else if(role == Qt::ForegroundRole)
    {
      item->m_fore = value.value<QBrush>();
      ret = true;
    }
    else if(role == Qt::ToolTipRole && !widget->m_instantTooltips)
    {
      item->m_tooltip = value.toString();
      ret = true;
    }
    else if(role == Qt::FontRole)
    {
      ret = false;
    }
    else
    {
      item->setData(index.column(), role, value);
      ret = true;
    }

    if(ret)
      widget->itemDataChanged(item, index.column(), role);

    return ret;
  }

private:
  RDTreeWidget *widget;
};

RDTreeWidgetItem::RDTreeWidgetItem(const QVariantList &values)
{
  m_text.reserve(values.size());
  for(const QVariant &v : values)
    m_text.push_back(v);
  m_icons.resize(m_text.size());

  for(int i = 0; i < m_text.count(); i++)
    checkForResourceId(i);
}

RDTreeWidgetItem::RDTreeWidgetItem(const std::initializer_list<QVariant> &values)
{
  m_text = values;
  m_icons.resize(m_text.size());

  for(int i = 0; i < m_text.count(); i++)
    checkForResourceId(i);
}

void RDTreeWidgetItem::checkForResourceId(int col)
{
  QRegularExpression &re = ResourceIdRegexp();

  QVariant &v = m_text[col];

  if(v.userType() == qMetaTypeId<ResourceId>() || re.match(v.toString()).hasMatch())
  {
    QString text;
    if(v.userType() == qMetaTypeId<ResourceId>())
      text = ToQStr(v.value<ResourceId>());
    else
      text = v.toString();

    ResourceIdLinkedTextPtr linkedText(new ResourceIdLinkedText);

    // use regexp to split up into fragments of text and resourceid. The resourceid is then
    // formatted on the fly in ResourceIdLinkedText::cacheDocument
    QRegularExpressionMatch match = re.match(text);
    while(match.hasMatch())
    {
      qulonglong idnum = match.captured(2).toULongLong();
      ResourceId id;
      memcpy(&id, &idnum, sizeof(id));

      // push any text that preceeded the ResourceId.
      if(match.capturedStart(1) > 0)
        linkedText->fragments.push_back(text.left(match.capturedStart(1)));

      text.remove(0, match.capturedEnd(2));

      linkedText->fragments.push_back(id);

      match = re.match(text);
    }

    if(!text.isEmpty())
      linkedText->fragments.push_back(text);

    linkedText->doc.setHtml(text);

    v = QVariant::fromValue(linkedText);
  }
}

RDTreeWidgetItem::~RDTreeWidgetItem()
{
  if(m_parent)
    m_parent->removeChild(this);

  clear();

  delete m_data;
}

QVariant RDTreeWidgetItem::data(int column, int role) const
{
  if(m_data == NULL || column >= m_data->count())
    return QVariant();

  const QVector<RoleData> &dataVec = (*m_data)[column];

  for(const RoleData &d : dataVec)
  {
    if(d.role == role)
      return d.data;
  }

  return QVariant();
}

void RDTreeWidgetItem::setData(int column, int role, const QVariant &value)
{
  // we lazy allocate
  if(!m_data)
  {
    m_data = new QVector<QVector<RoleData>>;
    m_data->resize(qMax(m_text.count(), column + 1));
  }

  if(role < Qt::UserRole)
  {
    Q_ASSERT(role < 64);
    m_customData |= 1ULL << role;
  }

  // data is allowed to resize above the column count in the widget
  if(m_data->count() <= column)
    m_data->resize(column + 1);

  QVector<RoleData> &dataVec = (*m_data)[column];

  for(RoleData &d : dataVec)
  {
    if(d.role == role)
    {
      bool different = (d.data != value);

      d.data = value;

      if(different && role < Qt::UserRole)
        m_widget->m_model->itemChanged(this, {role});

      return;
    }
  }

  dataVec.push_back(RoleData(role, value));

  if(role < Qt::UserRole)
    m_widget->m_model->itemChanged(this, {role});
}

void RDTreeWidgetItem::addChild(RDTreeWidgetItem *item)
{
  int colCount = item->m_text.count();

  // remove it from any previous parent
  if(item->m_parent)
    item->m_parent->removeChild(this);

  // set up its new parent to us
  item->m_parent = this;

  // set the widget in case this changed
  item->setWidget(m_widget);

  // resize per-column vectors to column count
  item->m_text.resize(colCount);
  item->m_icons.resize(colCount);

  // data can resize up, but we don't resize it down.
  if(item->m_data)
    item->m_data->resize(qMax(item->m_data->count(), colCount));

  if(m_widget)
    m_widget->beginAddChild(this);

  // add to our list of children
  m_children.push_back(item);

  if(m_widget)
    m_widget->endAddChild(this);
}

void RDTreeWidgetItem::setWidget(RDTreeWidget *widget)
{
  if(widget == m_widget)
    return;

  QWidget *parent = widget;
  while(parent)
    parent = parent->parentWidget();

  // if the widget is different, we need to recurse to children
  m_widget = widget;
  for(RDTreeWidgetItem *item : m_children)
    item->setWidget(widget);
}

void RDTreeWidgetItem::dataChanged(int column, int role)
{
  if(m_widget)
    m_widget->itemDataChanged(this, column, role);
}

RDTreeWidgetItem *RDTreeWidgetItem::takeChild(int index)
{
  if(m_widget && !m_widget->m_clearing)
    m_widget->m_model->beginRemoveChildren(this, index, index);

  m_children[index]->m_parent = NULL;
  RDTreeWidgetItem *ret = m_children.takeAt(index);

  if(m_widget && !m_widget->m_clearing)
    m_widget->m_model->endRemoveChildren();

  return ret;
}

void RDTreeWidgetItem::removeChild(RDTreeWidgetItem *child)
{
  if(m_widget)
  {
    int row = m_children.indexOf(child);
    m_widget->m_model->beginRemoveChildren(this, row, row);
  }

  m_children.removeOne(child);

  if(m_widget)
    m_widget->m_model->endRemoveChildren();
}

void RDTreeWidgetItem::clear()
{
  if(!childCount())
    return;

  if(m_widget && !m_widget->m_clearing)
    m_widget->m_model->beginRemoveChildren(this, 0, childCount() - 1);

  while(childCount() > 0)
  {
    RDTreeWidgetItem *child = takeChild(0);
    child->clear();
    delete child;
  }

  if(m_widget && !m_widget->m_clearing)
    m_widget->m_model->endRemoveChildren();
}

RDTreeWidgetItemIterator::RDTreeWidgetItemIterator(RDTreeWidget *widget)
{
  if(widget->topLevelItemCount() == 0)
    m_Current = NULL;
  else
    m_Current = widget->topLevelItem(0);
}

RDTreeWidgetItemIterator &RDTreeWidgetItemIterator::operator++()
{
  // depth first
  if(m_Current->childCount() > 0)
  {
    m_Current = m_Current->child(0);
    return *this;
  }

  // otherwise check if we have siblings, recursively up
  RDTreeWidgetItem *parent = m_Current->parent();
  RDTreeWidgetItem *child = m_Current;
  while(parent)
  {
    // if there's a sibling at this level, move to it
    int idx = parent->indexOfChild(child);
    if(idx + 1 < parent->childCount())
    {
      m_Current = parent->child(idx + 1);
      return *this;
    }

    // if there are no more siblings at this level, move up
    child = parent;
    parent = parent->parent();

    // if we just exhausted siblings at the top-level, parent will now be NULL, so we abort.
  }

  // no more siblings, stop.
  m_Current = NULL;
  return *this;
}

RDTreeWidgetDelegate::RDTreeWidgetDelegate(RDTreeWidget *parent)
    : m_widget(parent), ForwardingDelegate(parent)
{
}

RDTreeWidgetDelegate::~RDTreeWidgetDelegate()
{
}

void RDTreeWidgetDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
  RDTreeWidgetModel *model = m_widget->m_model;
  if(model == index.model())
  {
    RDTreeWidgetItem *item = model->itemForIndex(index);

    if(item->m_text[index.column()].userType() == qMetaTypeId<ResourceIdLinkedTextPtr>())
    {
      {
        // draw the item without text, so we get the proper background/selection/etc.
        QStyleOptionViewItem opt = option;
        opt.text.clear();

        ForwardingDelegate::paint(painter, opt, index);
      }

      ResourceIdLinkedTextPtr linkedText =
          item->m_text[index.column()].value<ResourceIdLinkedTextPtr>();

      painter->save();

      painter->translate(option.rect.left(), option.rect.top());

      linkedText->cacheDocument(m_widget);
      linkedText->doc.setTextWidth(option.rect.width());
      linkedText->doc.drawContents(painter);

      if(option.state & QStyle::State_MouseOver)
      {
        painter->setPen(QPen(option.palette.brush(QPalette::WindowText), 1.0));

        QAbstractTextDocumentLayout *layout = linkedText->doc.documentLayout();

        QPoint p = m_widget->viewport()->mapFromGlobal(QCursor::pos());
        p -= option.rect.topLeft();

        int pos = layout->hitTest(p, Qt::FuzzyHit);

        if(pos >= 0)
        {
          QTextBlock block = linkedText->doc.findBlock(pos);

          int frag = block.userState();
          if(frag >= 0)
          {
            QVariant v = linkedText->fragments[frag];
            if(v.userType() == qMetaTypeId<ResourceId>() && v.value<ResourceId>() != ResourceId())
            {
              layout->blockBoundingRect(block);
              QRectF rect = layout->blockBoundingRect(block);

              if(block.previous().userState() == frag)
              {
                rect = rect.united(layout->blockBoundingRect(block.previous()));
              }

              if(block.next().userState() == frag)
              {
                rect = rect.united(layout->blockBoundingRect(block.next()));
              }

              rect.translate(0.0, -2.0);

              painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            }
          }
        }
      }

      painter->restore();
      return;
    }
  }
  else
  {
    qCritical() << "Unexpected model being passed to RDTreeWidgetDelegate!";
  }

  return ForwardingDelegate::paint(painter, option, index);
}

QSize RDTreeWidgetDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  if(index.isValid())
  {
    RDTreeWidgetModel *model = m_widget->m_model;
    if(model == index.model())
    {
      RDTreeWidgetItem *item = model->itemForIndex(index);

      if(item->m_text[index.column()].userType() == qMetaTypeId<ResourceIdLinkedTextPtr>())
      {
        ResourceIdLinkedTextPtr linkedText =
            item->m_text[index.column()].value<ResourceIdLinkedTextPtr>();

        linkedText->cacheDocument(m_widget);
        return QSize(linkedText->doc.idealWidth(), option.fontMetrics.height());
      }
    }
    else
    {
      qCritical() << "Unexpected model being passed to RDTreeWidgetDelegate!";
    }
  }

  return ForwardingDelegate::sizeHint(option, index);
}

bool RDTreeWidgetDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                       const QStyleOptionViewItem &option, const QModelIndex &index)
{
  if(event->type() == QEvent::MouseButtonRelease && index.isValid())
  {
    RDTreeWidgetModel *rdmodel = m_widget->m_model;
    if(rdmodel == model)
    {
      RDTreeWidgetItem *item = rdmodel->itemForIndex(index);

      if(item->m_text[index.column()].userType() == qMetaTypeId<ResourceIdLinkedTextPtr>())
      {
        ResourceIdLinkedTextPtr linkedText =
            item->m_text[index.column()].value<ResourceIdLinkedTextPtr>();

        linkedText->cacheDocument(m_widget);

        QAbstractTextDocumentLayout *layout = linkedText->doc.documentLayout();

        QPoint p = ((QMouseEvent *)event)->pos();
        p -= option.rect.topLeft();

        int pos = layout->hitTest(p, Qt::FuzzyHit);

        if(pos >= 0)
        {
          QTextBlock block = linkedText->doc.findBlock(pos);

          int frag = block.userState();
          if(frag >= 0)
          {
            QVariant v = linkedText->fragments[frag];
            if(v.userType() == qMetaTypeId<ResourceId>() && v.value<ResourceId>() != ResourceId())
            {
              ICaptureContext *ctxptr = getCaptureContext(m_widget);

              if(ctxptr)
              {
                ICaptureContext &ctx = *(ICaptureContext *)ctxptr;

                if(!ctx.HasResourceInspector())
                  ctx.ShowResourceInspector();

                ctx.GetResourceInspector()->Inspect(v.value<ResourceId>());

                ctx.RaiseDockWindow(ctx.GetResourceInspector()->Widget());
              }
            }
          }
        }

        return true;
      }
    }
    else
    {
      qCritical() << "Unexpected model being passed to RDTreeWidgetDelegate!";
    }
  }

  return ForwardingDelegate::editorEvent(event, model, option, index);
}

bool RDTreeWidgetDelegate::linkHover(QMouseEvent *e, const QModelIndex &index)
{
  if(index.isValid())
  {
    RDTreeWidgetItem *item = m_widget->m_model->itemForIndex(index);

    if(item->m_text[index.column()].userType() == qMetaTypeId<ResourceIdLinkedTextPtr>())
    {
      ResourceIdLinkedTextPtr linkedText =
          item->m_text[index.column()].value<ResourceIdLinkedTextPtr>();

      linkedText->cacheDocument(m_widget);

      QAbstractTextDocumentLayout *layout = linkedText->doc.documentLayout();

      QPoint p = e->pos();
      p -= m_widget->visualRect(index).topLeft();

      int pos = layout->hitTest(p, Qt::FuzzyHit);

      if(pos >= 0)
      {
        QTextBlock block = linkedText->doc.findBlock(pos);

        int frag = block.userState();
        if(frag >= 0)
        {
          QVariant v = linkedText->fragments[frag];
          if(v.userType() == qMetaTypeId<ResourceId>() && v.value<ResourceId>() != ResourceId())
          {
            return true;
          }
        }
      }
    }
  }

  return false;
}

RDTreeWidget::RDTreeWidget(QWidget *parent) : RDTreeView(parent)
{
  // we'll call this ourselves in drawBranches()
  RDTreeView::enableBranchRectFill(false);

  m_delegate = new RDTreeWidgetDelegate(this);
  RDTreeView::setItemDelegate(m_delegate);

  header()->setSectionsMovable(false);

  m_root = new RDTreeWidgetItem;
  m_root->m_widget = this;

  m_model = new RDTreeWidgetModel(this);
  RDTreeView::setModel(m_model);

  QObject::connect(this, &RDTreeWidget::activated, [this](const QModelIndex &idx) {
    emit itemActivated(m_model->itemForIndex(idx), idx.column());
  });
  QObject::connect(this, &RDTreeWidget::clicked, [this](const QModelIndex &idx) {
    emit itemClicked(m_model->itemForIndex(idx), idx.column());
  });
  QObject::connect(this, &RDTreeWidget::doubleClicked, [this](const QModelIndex &idx) {
    emit itemDoubleClicked(m_model->itemForIndex(idx), idx.column());
  });
}

RDTreeWidget::~RDTreeWidget()
{
  delete m_root;
  delete m_model;
}

void RDTreeWidget::beginUpdate()
{
  m_queueUpdates = true;

  m_queuedItem = NULL;
  m_lowestIndex = m_highestIndex = qMakePair<int, int>(-1, -1);
  m_queuedChildren = false;
  m_queuedRoles = 0;

  setUpdatesEnabled(false);
}

void RDTreeWidget::endUpdate()
{
  m_queueUpdates = false;

  if(m_queuedRoles || m_queuedChildren)
  {
    // if we updated multiple different trees we can't issue a single dataChanged for everything
    // under a parent. Refresh the whole model.
    if(m_queuedItem == NULL)
    {
      m_model->refresh();
    }
    else
    {
      QVector<int> roles;
      for(int r = 0; r < 64; r++)
      {
        if(m_queuedRoles & (1ULL << r))
          roles.push_back(r);
      }

      if(m_queuedChildren)
      {
        m_model->beginAddChild(m_queuedItem);
        m_model->endAddChild(m_queuedItem);
      }

      if(!roles.isEmpty())
        m_model->itemsChanged(m_queuedItem, m_lowestIndex, m_highestIndex, roles);
    }
  }

  setUpdatesEnabled(true);
}

void RDTreeWidget::setColumnAlignment(int column, Qt::Alignment align)
{
  if(m_alignments.count() <= column)
    m_alignments.resize(column + 1);

  m_alignments[column] = align;
}

void RDTreeWidget::setItemDelegate(QAbstractItemDelegate *delegate)
{
  m_userDelegate = delegate;
  m_delegate->setForwardDelegate(m_userDelegate);
}

QAbstractItemDelegate *RDTreeWidget::itemDelegate() const
{
  return m_userDelegate;
}

void RDTreeWidget::setColumns(const QStringList &columns)
{
  m_headers = columns;
  m_model->refresh();
}

void RDTreeWidget::setHeaderText(int column, const QString &text)
{
  m_headers[column] = text;
  m_model->headerRefresh();
}

RDTreeWidgetItem *RDTreeWidget::selectedItem() const
{
  QModelIndexList sel = selectionModel()->selectedIndexes();

  if(sel.isEmpty())
    return NULL;

  return m_model->itemForIndex(sel[0]);
}

RDTreeWidgetItem *RDTreeWidget::currentItem() const
{
  return m_model->itemForIndex(currentIndex());
}

void RDTreeWidget::setSelectedItem(RDTreeWidgetItem *node)
{
  selectionModel()->select(m_model->indexForItem(node, 0),
                           QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void RDTreeWidget::setCurrentItem(RDTreeWidgetItem *node)
{
  setCurrentIndex(m_model->indexForItem(node, 0));
}

RDTreeWidgetItem *RDTreeWidget::itemAt(const QPoint &p) const
{
  return m_model->itemForIndex(indexAt(p));
}

void RDTreeWidget::expandItem(RDTreeWidgetItem *item)
{
  expand(m_model->indexForItem(item, 0));
}

void RDTreeWidget::expandAllItems(RDTreeWidgetItem *item)
{
  expandItem(item);

  for(int c = 0; c < item->childCount(); c++)
  {
    RDTreeWidgetItem *child = item->child(c);
    expandAllItems(child);
  }
}

void RDTreeWidget::collapseItem(RDTreeWidgetItem *item)
{
  collapse(m_model->indexForItem(item, 0));
}

void RDTreeWidget::collapseAllItems(RDTreeWidgetItem *item)
{
  collapseItem(item);

  for(int c = 0; c < item->childCount(); c++)
  {
    RDTreeWidgetItem *child = item->child(c);
    collapseAllItems(child);
  }
}

void RDTreeWidget::scrollToItem(RDTreeWidgetItem *node)
{
  scrollTo(m_model->indexForItem(node, 0));
}

void RDTreeWidget::clear()
{
  m_clearing = true;
  m_root->clear();
  m_clearing = false;

  m_currentHoverIndex = QModelIndex();

  m_model->refresh();
}

void RDTreeWidget::mouseMoveEvent(QMouseEvent *e)
{
  RDTreeWidgetItem *oldHover = m_model->itemForIndex(m_currentHoverIndex);

  RDTreeView::mouseMoveEvent(e);

  RDTreeWidgetItem *newHover = m_model->itemForIndex(m_currentHoverIndex);

  if(m_currentHoverIndex.column() == m_hoverColumn && m_hoverHandCursor)
  {
    setCursor(QCursor(Qt::PointingHandCursor));
  }
  else if(m_delegate->linkHover(e, m_currentHoverIndex))
  {
    m_model->itemChanged(m_model->itemForIndex(m_currentHoverIndex), {Qt::DecorationRole});
    setCursor(QCursor(Qt::PointingHandCursor));
  }
  else
  {
    unsetCursor();
  }

  if(oldHover == newHover)
    return;

  // it's only two items, don't try and make a range but just change them both
  QVector<int> roles = {Qt::DecorationRole, Qt::BackgroundRole, Qt::ForegroundRole};
  if(oldHover)
    m_model->itemChanged(oldHover, roles);
  m_model->itemChanged(newHover, roles);

  if(m_instantTooltips)
  {
    QToolTip::hideText();

    if(newHover && !newHover->m_tooltip.isEmpty())
    {
      // the documentation says:
      //
      // "If text is empty the tool tip is hidden. If the text is the same as the currently shown
      // tooltip, the tip will not move. You can force moving by first hiding the tip with an empty
      // text, and then showing the new tip at the new position."
      //
      // However the actual implementation has some kind of 'fading' check, so if you hide then
      // immediately show, it will try to reuse the tooltip and end up not moving it at all if the
      // text hasn't changed.
      QToolTip::showText(QCursor::pos(), lit(" "), this);
      QToolTip::showText(QCursor::pos(), newHover->m_tooltip, this);
    }
  }

  emit mouseMove(e);

  RDTreeView::mouseMoveEvent(e);
}

void RDTreeWidget::mouseReleaseEvent(QMouseEvent *e)
{
  QModelIndex idx = indexAt(e->pos());

  if(idx.isValid() && idx.column() == m_hoverColumn && m_activateOnClick)
  {
    emit itemActivated(itemAt(e->pos()), idx.column());
  }

  RDTreeView::mouseReleaseEvent(e);
}

void RDTreeWidget::leaveEvent(QEvent *e)
{
  unsetCursor();

  if(m_currentHoverIndex.isValid())
  {
    RDTreeWidgetItem *item = m_model->itemForIndex(m_currentHoverIndex);
    if(!item->m_tooltip.isEmpty() && m_instantTooltips)
      QToolTip::hideText();
    m_model->itemChanged(item, {Qt::DecorationRole, Qt::BackgroundRole, Qt::ForegroundRole});
  }

  emit leave(e);

  RDTreeView::leaveEvent(e);
}

void RDTreeWidget::focusOutEvent(QFocusEvent *event)
{
  if(m_clearSelectionOnFocusLoss)
    clearSelection();

  RDTreeView::focusOutEvent(event);
}

void RDTreeWidget::keyPressEvent(QKeyEvent *e)
{
  if(!m_customCopyPaste && e->matches(QKeySequence::Copy))
  {
    QModelIndexList sel = selectionModel()->selectedRows();

    int stackWidths[16];
    int *heapWidths = NULL;

    int colCount = m_model->columnCount();

    if(colCount >= 16)
      heapWidths = new int[colCount];

    int *widths = heapWidths ? heapWidths : stackWidths;

    for(int i = 0; i < colCount; i++)
      widths[i] = 0;

    // align the copied data so that each column is the same width
    for(QModelIndex idx : sel)
    {
      RDTreeWidgetItem *item = m_model->itemForIndex(idx);

      for(int i = 0; i < qMin(colCount, item->m_text.count()); i++)
      {
        QString text = item->m_text[i].toString();
        if(item->m_text[i].userType() == qMetaTypeId<ResourceIdLinkedTextPtr>())
        {
          ResourceIdLinkedTextPtr linkedText = item->m_text[i].value<ResourceIdLinkedTextPtr>();
          linkedText->cacheDocument(this);
          text = linkedText->text;
        }

        widths[i] = qMax(widths[i], text.count());
      }
    }

    // only align up to 50 characters so one really long item doesn't mess up the whole thing
    for(int i = 0; i < colCount; i++)
      widths[i] = qMin(50, widths[i]);

    QString clipData;
    for(QModelIndex idx : sel)
    {
      RDTreeWidgetItem *item = m_model->itemForIndex(idx);

      for(int i = 0; i < qMin(colCount, item->m_text.count()); i++)
      {
        QString format = i == 0 ? QFormatStr("%1") : QFormatStr(" %1");

        QString text = item->m_text[i].toString();
        if(item->m_text[i].userType() == qMetaTypeId<ResourceIdLinkedTextPtr>())
        {
          ResourceIdLinkedTextPtr linkedText = item->m_text[i].value<ResourceIdLinkedTextPtr>();
          linkedText->cacheDocument(this);
          text = linkedText->text;
        }

        clipData += format.arg(text, -widths[i]);
      }

      clipData += lit("\n");
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(clipData.trimmed());

    delete[] heapWidths;
  }
  else
  {
    RDTreeView::keyPressEvent(e);
  }

  emit(keyPress(e));
}

void RDTreeWidget::drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const
{
  // we do our own custom branch rendering to ensure the backgrounds for the +/- markers are filled
  // (as otherwise they don't show up well over selection or background fills) as well as to draw
  // any vertical branch colors.

  // start at the left-most side of the rect
  QRect branchRect(rect.left(), rect.top(), indentation(), rect.height());

  RDTreeWidgetItem *item = m_model->itemForIndex(index);

  // first draw the coloured lines - we're only interested in parents for this, so push all the
  // parents onto a stack
  QStack<RDTreeWidgetItem *> parents;

  RDTreeWidgetItem *parent = item->parent();

  while(parent && parent != m_root)
  {
    parents.push(parent);
    parent = parent->parent();
  }

  // fill in the background behind the lines for the whole row, since by default it doesn't show up
  // behind the tree lines.

  QRect allLinesRect(rect.left(), rect.top(), (parents.count() + 1) * indentation(), rect.height());

  // calling this manually here means it won't be called later in RDTreeView::drawBranches, and
  // allows us to overwrite it with our background filling if we want to.
  RDTreeWidget::fillBranchesRect(painter, rect, index);

  if(!selectionModel()->isSelected(index) && item->m_back != QBrush())
  {
    painter->fillRect(allLinesRect, item->m_back);
  }

  RDTreeView::drawBranches(painter, rect, index);

  // we now iterate from the top-most parent down, moving in from the left
  // we draw this after calling into drawBranches() so we paint on top of the built-in lines
  QPen oldPen = painter->pen();
  while(!parents.isEmpty())
  {
    parent = parents.pop();

    if(parent->m_treeCol.isValid())
    {
      // draw a centred pen vertically down the middle of branchRect

      painter->setPen(QPen(QBrush(parent->m_treeCol), parent->m_treeColWidth));

      QPoint topCentre = QRect(branchRect).center();
      QPoint bottomCentre = topCentre;

      topCentre.setY(branchRect.top());
      bottomCentre.setY(branchRect.bottom());

      painter->drawLine(topCentre, bottomCentre);
    }

    branchRect.moveLeft(branchRect.left() + indentation());
  }
  painter->setPen(oldPen);
}

void RDTreeWidget::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
  emit itemSelectionChanged();

  RDTreeView::selectionChanged(selected, deselected);
}

void RDTreeWidget::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
  emit currentItemChanged(m_model->itemForIndex(current), m_model->itemForIndex(previous));

  RDTreeView::currentChanged(current, previous);
}

void RDTreeWidget::itemDataChanged(RDTreeWidgetItem *item, int column, int role)
{
  if(m_queueUpdates)
  {
    m_queuedRoles |= (1ULL << role);

    // for now we only support updating the whole row, with all columns, even if only one column
    // changed.
    int row = item->m_parent->indexOfChild(item);

    // no queued updates yet, set up this one
    if(m_lowestIndex.first == -1)
    {
      m_queuedItem = item;
      m_lowestIndex = qMakePair<int, int>(row, 0);
      m_highestIndex = qMakePair<int, int>(m_lowestIndex.first, m_headers.count() - 1);
    }
    else
    {
      // there's already an update. Check if we can expand it
      if(m_queuedItem == item)
      {
        m_lowestIndex.first = qMin(m_lowestIndex.first, row);
        m_highestIndex.first = qMax(m_highestIndex.first, row);
      }
      else
      {
        // can't batch updates across multiple parents, so we just fallback to full model refresh
        m_queuedItem = NULL;
      }
    }
  }
  else
  {
    m_model->itemChanged(item, {role});
  }

  emit itemChanged(item, column);
}

void RDTreeWidget::beginAddChild(RDTreeWidgetItem *item)
{
  if(m_queueUpdates)
  {
    m_queuedChildren = true;

    if(m_lowestIndex.first == -1)
    {
      m_queuedItem = item;
      // make an update of row 0. This will be a bit pessimistic if there are later data changes
      // in a later row, but we're generally only changing data *or* adding children, not both, and
      // in either case this is primarily about batching updates not providing a minimal update set
      m_lowestIndex = qMakePair<int, int>(0, 0);
      m_highestIndex = qMakePair<int, int>(0, m_headers.count() - 1);
    }
    else
    {
      if(m_queuedItem == item)
      {
        // there's already an update. don't need to expand it, the m_queuedChildren is enough
      }
      else
      {
        // can't batch updates across multiple parents, so we just fallback to full model refresh
        m_queuedItem = NULL;
      }
    }
  }
  else
  {
    m_model->beginAddChild(item);
  }
}

void RDTreeWidget::endAddChild(RDTreeWidgetItem *item)
{
  // work is all done in beginAddChild
  if(!m_queueUpdates)
    m_model->endAddChild(item);
}