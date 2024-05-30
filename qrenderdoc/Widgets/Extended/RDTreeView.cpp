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

#include "RDTreeView.h"
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopWidget>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QProxyStyle>
#include <QScrollBar>
#include <QStack>
#include <QStylePainter>
#include <QWheelEvent>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"

static int GetDepth(const QAbstractItemModel *model, const QModelIndex &idx)
{
  if(idx == QModelIndex())
    return 0;

  return 1 + GetDepth(model, model->parent(idx));
}

static bool CompareModelIndex(const QModelIndex &a, const QModelIndex &b)
{
  if(a == b)
    return false;

  if(a == QModelIndex())
    return true;
  else if(b == QModelIndex())
    return false;

  if(a.model() != b.model())
    return false;

  QModelIndex ap = a.model()->parent(a);
  QModelIndex bp = b.model()->parent(b);
  if(ap == bp)
  {
    if(a.row() == b.row())
      return a.column() < b.column();
    return a.row() < b.row();
  }

  if(a == bp)
    return true;
  if(b == ap)
    return false;

  int ad = GetDepth(a.model(), a);
  int bd = GetDepth(b.model(), b);

  if(ad > bd)
    return CompareModelIndex(ap, b);
  else if(ad < bd)
    return CompareModelIndex(a, bp);
  return CompareModelIndex(ap, bp);
}

RDTreeViewDelegate::RDTreeViewDelegate(RDTreeView *view) : RichTextViewDelegate(view), m_View(view)
{
}

void RDTreeViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
  return RichTextViewDelegate::paint(painter, option, index);
}

QSize RDTreeViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QSize ret = RichTextViewDelegate::sizeHint(option, index);

  int minHeight = option.fontMetrics.height();
  if(!m_View->ignoreIconSize())
    minHeight = qMax(option.decorationSize.height(), minHeight);

  if(m_View->ignoreIconSize())
    ret.setHeight(qMax(qMax(option.decorationSize.height(), minHeight) + 2, ret.height()));

  // expand a pixel for the grid lines
  if(m_View->visibleGridLines())
    ret.setWidth(ret.width() + 1);

  // ensure we have at least the margin on top of font size. If the style applied more, don't add to
  // it.
  ret.setHeight(qMax(ret.height(), minHeight + m_View->verticalItemMargin()));

  return ret;
}

RDTipLabel::RDTipLabel(QWidget *listener) : QLabel(NULL), mouseListener(listener)
{
  int margin = style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, NULL, this);
  int opacity = style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, NULL, this);

  setWindowFlags(Qt::ToolTip);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setForegroundRole(QPalette::ToolTipText);
  setBackgroundRole(QPalette::ToolTipBase);
  setMargin(margin + 1);
  setFrameStyle(QFrame::NoFrame);
  setAlignment(Qt::AlignLeft);
  setIndent(1);
  setWindowOpacity(opacity / 255.0);
}

QSize RDTipLabel::configureTip(QWidget *, QModelIndex, QString text)
{
  setText(text);
  return minimumSizeHint();
}

void RDTipLabel::showTip(QPoint pos)
{
  move(pos);
  show();
}

bool RDTipLabel::forceTip(QWidget *widget, QModelIndex idx)
{
  return false;
}

void RDTipLabel::paintEvent(QPaintEvent *ev)
{
  QStylePainter p(this);
  QStyleOptionFrame opt;
  opt.init(this);
  p.drawPrimitive(QStyle::PE_PanelTipLabel, opt);
  p.end();

  QLabel::paintEvent(ev);
}

void RDTipLabel::mousePressEvent(QMouseEvent *e)
{
  if(mouseListener)
    sendListenerEvent(e);
}

void RDTipLabel::sendListenerEvent(QMouseEvent *e)
{
  QMouseEvent *duplicate =
      new QMouseEvent(e->type(), mouseListener->mapFromGlobal(e->globalPos()), e->windowPos(),
                      e->globalPos(), e->button(), e->buttons(), e->modifiers(), e->source());
  QCoreApplication::postEvent(mouseListener, duplicate);
}

void RDTipLabel::mouseReleaseEvent(QMouseEvent *e)
{
  if(mouseListener)
    sendListenerEvent(e);
}

void RDTipLabel::mouseDoubleClickEvent(QMouseEvent *e)
{
  if(mouseListener)
    sendListenerEvent(e);
}

void RDTipLabel::resizeEvent(QResizeEvent *e)
{
  QStyleHintReturnMask frameMask;
  QStyleOption option;
  option.init(this);
  if(style()->styleHint(QStyle::SH_ToolTip_Mask, &option, this, &frameMask))
    setMask(frameMask.region);

  QLabel::resizeEvent(e);
}

RDTreeView::RDTreeView(QWidget *parent) : QTreeView(parent)
{
  setMouseTracking(true);

  m_delegate = new RDTreeViewDelegate(this);
  QTreeView::setItemDelegate(m_delegate);

  m_TooltipLabel = new RDTipLabel(viewport());
  m_TooltipLabel->hide();
  m_CurrentTooltipElided = false;

  m_Tooltip = m_TooltipLabel;
}

RDTreeView::~RDTreeView()
{
  setModel(NULL);

  delete m_TooltipLabel;
}

void RDTreeView::mouseMoveEvent(QMouseEvent *e)
{
  QModelIndex oldHoverIndex = m_currentHoverIndex;

  if(m_CurrentTooltipElided && m_TooltipLabel->isVisible() &&
     !m_TooltipLabel->geometry().contains(QCursor::pos()))
    m_Tooltip->hideTip();

  m_currentHoverIndex = indexAt(e->pos());

  if(m_delegate->linkHover(e, font(), m_currentHoverIndex))
  {
    if(cursor().shape() != Qt::PointingHandCursor)
    {
      viewport()->update(visualRect(m_currentHoverIndex));
      setCursor(QCursor(Qt::PointingHandCursor));
    }
  }
  else if(cursor().shape() == Qt::PointingHandCursor)
  {
    viewport()->update(visualRect(m_currentHoverIndex));
    unsetCursor();
  }

  if(oldHoverIndex != m_currentHoverIndex)
  {
    if(m_instantTooltips)
    {
      m_Tooltip->hideTip();

      if(m_currentHoverIndex.isValid())
      {
        QString tooltip = m_currentHoverIndex.data(Qt::ToolTipRole).toString();

        if(!tooltip.isEmpty() || m_Tooltip->forceTip(this, m_currentHoverIndex))
        {
          // We don't use QToolTip since we have a custom tooltip for showing elided results, and we
          // use that for consistency. This also makes it easier to slot in a custom tooltip widget
          // externally.
          QPoint p = QCursor::pos();

          // estimate, as this is not easily queryable
          const QPoint cursorSize(16, 16);
          const QRect screenAvailGeom = QApplication::desktop()->availableGeometry(p);

          // start with the tooltip placed bottom-right of the cursor, as the default
          QRect tooltipRect;
          tooltipRect.setTopLeft(p + cursorSize);
          tooltipRect.setSize(m_Tooltip->configureTip(this, m_currentHoverIndex, tooltip));

          // clip by the available geometry in x
          if(tooltipRect.right() > screenAvailGeom.right())
            tooltipRect.moveRight(screenAvailGeom.right());

          // if we'd go out of bounds in y, place the tooltip above the cursor. Don't just clip like
          // in x, because that could place the tooltip over the cursor.
          if(tooltipRect.bottom() > screenAvailGeom.bottom())
            tooltipRect.moveBottom(p.y() - cursorSize.y());

          m_Tooltip->showTip(tooltipRect.topLeft());
          m_CurrentTooltipElided = false;
        }
      }
    }
  }

  QTreeView::mouseMoveEvent(e);
}

void RDTreeView::wheelEvent(QWheelEvent *e)
{
  QTreeView::wheelEvent(e);
  m_currentHoverIndex = indexAt(e->pos());
}

void RDTreeView::leaveEvent(QEvent *e)
{
  if(m_CurrentTooltipElided)
  {
    if(m_TooltipLabel->isVisible() && !m_TooltipLabel->geometry().contains(QCursor::pos()))
      m_Tooltip->hideTip();
  }
  else
  {
    m_Tooltip->hideTip();
  }

  m_currentHoverIndex = QModelIndex();

  emit leave(e);

  QTreeView::leaveEvent(e);
}

void RDTreeView::keyPressEvent(QKeyEvent *e)
{
  if(e->matches(QKeySequence::Copy))
  {
    copySelection();
  }
  else
  {
    QTreeView::keyPressEvent(e);
  }
  emit(keyPress(e));
}

void RDTreeView::contextMenuEvent(QContextMenuEvent *event)
{
  QPoint pos = event->pos();

  QModelIndex index = indexAt(pos);

  QMenu contextMenu(this);

  QAction expandAllAction(tr("&Expand All"), this);
  QAction collapseAllAction(tr("&Collapse All"), this);
  QAction copy(tr("&Copy"), this);

  if(rootIsDecorated())
  {
    contextMenu.addAction(&expandAllAction);
    contextMenu.addAction(&collapseAllAction);
    contextMenu.addSeparator();
  }
  contextMenu.addAction(&copy);

  expandAllAction.setIcon(Icons::arrow_out());
  collapseAllAction.setIcon(Icons::arrow_in());

  copy.setIcon(Icons::copy());

  expandAllAction.setEnabled(index.isValid() && model()->rowCount(index) > 0);
  collapseAllAction.setEnabled(index.isValid() && model()->rowCount(index) > 0);

  QObject::connect(&expandAllAction, &QAction::triggered, [this, index]() { expandAll(index); });

  QObject::connect(&collapseAllAction, &QAction::triggered, [this, index]() { collapseAll(index); });

  QObject::connect(&copy, &QAction::triggered, [this, index, pos]() { copyIndex(pos, index); });

  RDDialog::show(&contextMenu, viewport()->mapToGlobal(pos));
}

void RDTreeView::copyIndex(QPoint pos, QModelIndex index)
{
  bool clearsel = false;
  if(selectionModel()->selectedRows().empty())
  {
    setSelection(QRect(pos, QSize(1, 1)), selectionCommand(index));
    clearsel = true;
  }
  copySelection();
  if(clearsel)
    selectionModel()->clear();
}

void RDTreeView::expandAllInternal(QModelIndex index)
{
  int rows = model()->rowCount(index);

  if(rows == 0)
    return;

  expand(index);

  for(int r = 0; r < rows; r++)
    expandAll(model()->index(r, 0, index));
}

void RDTreeView::collapseAllInternal(QModelIndex index)
{
  int rows = model()->rowCount(index);

  if(rows == 0)
    return;

  collapse(index);

  for(int r = 0; r < rows; r++)
    collapseAll(model()->index(r, 0, index));
}

void RDTreeView::expandAll(QModelIndex index)
{
  setUpdatesEnabled(false);
  expandAllInternal(index);
  setUpdatesEnabled(true);
}

void RDTreeView::collapseAll(QModelIndex index)
{
  setUpdatesEnabled(false);
  collapseAllInternal(index);
  setUpdatesEnabled(true);
}

bool RDTreeView::viewportEvent(QEvent *event)
{
  if(event->type() == QEvent::ToolTip)
  {
    // if we're doing instant tooltips this is all handled in the mousemove handler, don't do
    // anything here
    if(m_instantTooltips)
      return true;

    if(m_TooltipElidedItems)
    {
      QHelpEvent *he = (QHelpEvent *)event;
      QModelIndex index = indexAt(he->pos());

      QAbstractItemDelegate *delegate = m_userDelegate;

      if(!delegate)
        delegate = QTreeView::itemDelegate(index);

      if(delegate)
      {
        QStyleOptionViewItem option;
        option.initFrom(this);
        option.rect = visualRect(index);

        // delegates get first dibs at processing the event
        bool ret = delegate->helpEvent(he, this, option, index);

        if(ret)
          return true;

        QSize desiredSize = delegate->sizeHint(option, index);

        if(desiredSize.width() > option.rect.width())
        {
          const QString fullText = index.data(Qt::DisplayRole).toString();
          if(!fullText.isEmpty())
          {
            // need to use a custom label tooltip since the QToolTip freaks out as we're placing it
            // underneath the cursor instead of next to it (so that the tooltip lines up over the
            // row)
            m_Tooltip->configureTip(this, index, fullText);
            m_Tooltip->showTip(viewport()->mapToGlobal(option.rect.topLeft()));
            m_CurrentTooltipElided = true;
          }
        }
      }
    }
  }

  return QTreeView::viewportEvent(event);
}

void RDTreeView::setItemDelegate(QAbstractItemDelegate *delegate)
{
  m_userDelegate = delegate;
  m_delegate->setForwardDelegate(m_userDelegate);
}

QAbstractItemDelegate *RDTreeView::itemDelegate() const
{
  return m_userDelegate;
}

void RDTreeView::setModel(QAbstractItemModel *model)
{
  QAbstractItemModel *old = this->model();

  if(old)
  {
    QObject::disconnect(old, &QAbstractItemModel::modelAboutToBeReset, this,
                        &RDTreeView::modelAboutToBeReset);
    QObject::disconnect(old, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                        &RDTreeView::rowsAboutToBeRemoved);
    QObject::disconnect(old, &QAbstractItemModel::columnsAboutToBeRemoved, this,
                        &RDTreeView::columnsAboutToBeRemoved);
    QObject::disconnect(old, &QAbstractItemModel::rowsAboutToBeMoved, this,
                        &RDTreeView::rowsAboutToBeMoved);
    QObject::disconnect(old, &QAbstractItemModel::columnsAboutToBeMoved, this,
                        &RDTreeView::columnsAboutToBeMoved);
  }

  QTreeView::setModel(model);

  if(model)
  {
    QObject::connect(model, &QAbstractItemModel::modelAboutToBeReset, this,
                     &RDTreeView::modelAboutToBeReset);
    QObject::connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                     &RDTreeView::rowsAboutToBeRemoved);
    QObject::connect(model, &QAbstractItemModel::columnsAboutToBeRemoved, this,
                     &RDTreeView::columnsAboutToBeRemoved);
    QObject::connect(model, &QAbstractItemModel::rowsAboutToBeMoved, this,
                     &RDTreeView::rowsAboutToBeMoved);
    QObject::connect(model, &QAbstractItemModel::columnsAboutToBeMoved, this,
                     &RDTreeView::columnsAboutToBeMoved);
  }
}

void RDTreeView::modelAboutToBeReset()
{
  m_currentHoverIndex = QModelIndex();
}

void RDTreeView::rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
  m_currentHoverIndex = QModelIndex();
  QTreeView::rowsAboutToBeRemoved(parent, first, last);
}

void RDTreeView::columnsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
  m_currentHoverIndex = QModelIndex();
}

void RDTreeView::rowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
                                    const QModelIndex &destinationParent, int destinationRow)
{
  m_currentHoverIndex = QModelIndex();
}

void RDTreeView::columnsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart,
                                       int sourceEnd, const QModelIndex &destinationParent,
                                       int destinationColumn)
{
  m_currentHoverIndex = QModelIndex();
}

void RDTreeView::updateExpansion(RDTreeViewExpansionState &state, const ExpansionKeyGen &keygen)
{
  for(int i = 0; i < model()->rowCount(); i++)
    updateExpansionFromRow(state, model()->index(i, 0), 0, keygen);
}

void RDTreeView::applyExpansion(const RDTreeViewExpansionState &state, const ExpansionKeyGen &keygen)
{
  for(int i = 0; i < model()->rowCount(); i++)
    applyExpansionToRow(state, model()->index(i, 0), 0, keygen);
}

void RDTreeView::copySelection()
{
  QModelIndexList sel = selectionModel()->selectedRows();

  std::sort(sel.begin(), sel.end(), CompareModelIndex);

  QVector<int> widths;

  ICaptureContext *ctx = getCaptureContext(this);

  int minDepth = INT_MAX;
  int maxDepth = 0;

  // align the copied data so that each column is the same width
  for(QModelIndex idx : sel)
  {
    int colCount = model()->columnCount(idx);

    widths.resize(qMax(widths.size(), colCount));

    for(int i = 0; i < colCount; i++)
    {
      QVariant var = model()->data(model()->index(idx.row(), i, idx.parent()));
      QString text = ctx ? RichResourceTextFormat(*ctx, var) : var.toString();
      widths[i] = qMax(widths[i], text.count());
    }

    int depth = GetDepth(model(), idx);

    minDepth = qMin(minDepth, depth);
    maxDepth = qMax(maxDepth, depth);
  }

  // add on two characters for every depth, for indent
  for(int &i : widths)
    i += 2 * (maxDepth - minDepth - 1);

  // only align up to 50 characters so one really long item doesn't mess up the whole thing
  for(int &i : widths)
    i = qMin(50, i);

  QString clipData;
  for(QModelIndex idx : sel)
  {
    int colCount = model()->columnCount(idx);

    int depth = GetDepth(model(), idx);

    QString line;

    for(int i = 0; i < colCount; i++)
    {
      QString format = i == 0 ? QFormatStr("%1") : QFormatStr(" %1");

      QVariant var = model()->data(model()->index(idx.row(), i, idx.parent()));
      QString text = ctx ? RichResourceTextFormat(*ctx, var) : var.toString();

      if(i == 0)
        text.prepend(QString((depth - minDepth) * 2, QLatin1Char(' ')));

      line += format.arg(text, -widths[i]);
    }

    clipData += line.trimmed() + lit("\n");
  }

  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(clipData);
}

void RDTreeView::updateExpansionFromRow(RDTreeViewExpansionState &state, QModelIndex idx, uint seed,
                                        const ExpansionKeyGen &keygen)
{
  if(!idx.isValid())
    return;

  int rowcount = model()->rowCount(idx);

  if(rowcount == 0)
    return;

  uint key = keygen(idx, seed);
  if(isExpanded(idx))
  {
    state.insert(key);

    // only recurse to children if this one is expanded - forget expansion state under collapsed
    // branches. Technically we're losing information here but it allows us to skip a full expensive
    // search
    for(int i = 0; i < rowcount; i++)
      updateExpansionFromRow(state, model()->index(i, 0, idx), seed, keygen);
  }
  else
  {
    state.remove(key);
  }
}

void RDTreeView::applyExpansionToRow(const RDTreeViewExpansionState &state, QModelIndex idx,
                                     uint seed, const ExpansionKeyGen &keygen)
{
  if(!idx.isValid())
    return;

  uint key = keygen(idx, seed);
  if(state.contains(key))
  {
    expand(idx);

    // same as above - only recurse when we have a parent that's expanded.
    for(int i = 0; i < model()->rowCount(idx); i++)
      applyExpansionToRow(state, model()->index(i, 0, idx), seed, keygen);
  }
}

void RDTreeView::drawRow(QPainter *painter, const QStyleOptionViewItem &options,
                         const QModelIndex &index) const
{
  QTreeView::drawRow(painter, options, index);

  if(m_VisibleGridLines)
  {
    QPen p = painter->pen();

    QColor back = options.palette.color(QPalette::Active, QPalette::Background);
    QColor fore = options.palette.color(QPalette::Active, QPalette::Foreground);

    // draw the grid lines with a colour half way between background and foreground
    painter->setPen(QPen(QColor::fromRgbF(back.redF() * 0.8 + fore.redF() * 0.2,
                                          back.greenF() * 0.8 + fore.greenF() * 0.2,
                                          back.blueF() * 0.8 + fore.blueF() * 0.2)));

    QRect intersectrect = options.rect.adjusted(0, 0, 1, 0);

    for(int i = 0, count = model()->columnCount(); i < count; i++)
    {
      QRect r = visualRect(model()->index(index.row(), i, index.parent()));

      if(r.width() <= 0)
        r.moveLeft(r.left() + r.width());
      if(r.height() <= 0)
        r.moveTop(r.top() + r.height());

      r = r.intersected(intersectrect);

      if(treePosition() == i)
      {
        int depth = 1;
        QModelIndex idx = index;
        while(idx.parent().isValid())
        {
          depth++;
          idx = idx.parent();
        }
        r.setLeft(r.left() - indentation() * depth);
      }

      // draw bottom and right of the rect
      painter->drawLine(r.bottomLeft(), r.bottomRight());
      painter->drawLine(r.topRight(), r.bottomRight());
    }

    painter->setPen(p);
  }
}

void RDTreeView::drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const
{
  // we do our own custom branch rendering to ensure the backgrounds for the +/- markers are filled
  // (as otherwise they don't show up well over selection or background fills) as well as to draw
  // any vertical branch colors.

  // start at the left-most side of the rect
  QRect branchRect(rect.left(), rect.top(), indentation(), rect.height());

  // first draw the coloured lines - we're only interested in parents for this, so push all the
  // parents onto a stack
  QStack<QModelIndex> parents;

  QModelIndex parent = index.parent();

  while(parent.isValid())
  {
    parents.push(parent);
    parent = parent.parent();
  }

  // fill in the background behind the lines for the whole row, since by default it doesn't show up
  // behind the tree lines.

  QRect allLinesRect(rect.left(), rect.top(),
                     (parents.count() + (rootIsDecorated() ? 1 : 0)) * indentation(), rect.height());

  QStyleOptionViewItem opt;
  opt.initFrom(this);
  if(selectionModel()->isSelected(index))
    opt.state |= QStyle::State_Selected;
  if(m_currentHoverIndex.row() == index.row() && m_currentHoverIndex.parent() == index.parent())
    opt.state |= QStyle::State_MouseOver;
  else
    opt.state &= ~QStyle::State_MouseOver;

  if(hasFocus())
    opt.state |= (QStyle::State_Active | QStyle::State_HasFocus);
  else
    opt.state &= ~(QStyle::State_Active | QStyle::State_HasFocus);

  int depth = 1;
  QModelIndex idx = index.parent();
  while(idx.isValid())
  {
    depth++;
    idx = idx.parent();
  }

  opt.rect = allLinesRect;
  opt.showDecorationSelected = true;
  opt.backgroundBrush = index.data(Qt::BackgroundRole).value<QBrush>();
  QVariant foreColVar = index.data(Qt::ForegroundRole);
  QColor foreCol;

  if(foreColVar.isValid())
  {
    foreCol = foreColVar.value<QBrush>().color();
    opt.palette.setColor(QPalette::Foreground, foreCol);
    opt.palette.setColor(QPalette::Text, foreCol);
  }

  style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, this);

  QPen oldPen = painter->pen();

  if(m_VisibleBranches)
  {
    // set the desired colour for RDTweakedNativeStyle via a huge hack - see
    // RDTweakedNativeStyle::drawPrimitive for QStyle::PE_IndicatorBranch
    if(foreColVar.isValid())
      painter->setPen(QPen(foreCol, 1234.5));
    QTreeView::drawBranches(painter, rect, index);
  }
  else
  {
    // draw only the expand item, not the branches
    QRect primitive(0, rect.top(), qMin(rect.width(), indentation()), rect.height());

    // if root isn't decorated, skip
    if(!rootIsDecorated() && !index.parent().isValid())
      return;

    // if no children, nothing to render
    if(model()->rowCount(index) == 0)
      return;

    QStyleOptionViewItem branchopt = viewOptions();

    branchopt.rect = primitive;

    // unfortunately QStyle::State_Children doesn't render ONLY the
    // open-toggle-button, but the vertical line upwards to a previous sibling.
    // For consistency, draw one downwards too.
    branchopt.state = QStyle::State_Children | QStyle::State_Sibling;
    if(isExpanded(index))
      branchopt.state |= QStyle::State_Open;

    branchopt.palette = opt.palette;

    style()->drawPrimitive(QStyle::PE_IndicatorBranch, &branchopt, painter, this);
  }

  // we now iterate from the top-most parent down, moving in from the left
  // we draw this after calling into drawBranches() so we paint on top of the built-in lines
  while(!parents.isEmpty())
  {
    parent = parents.pop();

    QBrush line = parent.data(RDTreeView::TreeLineColorRole).value<QBrush>();

    if(line.style() != Qt::NoBrush)
    {
      // draw a centred pen vertically down the middle of branchRect
      painter->setPen(QPen(line, m_treeColorLineWidth));

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

QModelIndex RDTreeView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
  // Qt's handling for MoveLeft is a little broken when scrollbars are in use, so we customise it
  // do almost the same thing but with a fix
  if(cursorAction == QAbstractItemView::MoveLeft)
  {
    // The default MoveRight is fine. It does in order:
    // 1. if the current item is expandable but not expanded, it expands it.
    // 2. if SH_ItemView_ArrowKeysNavigateIntoChildren is enabled it moves to the first child of the
    //    current item if there is one.
    // 3. finally it tries to scroll right, either by selecting the next column or just moving the
    //    scrollbar.
    //
    // That's all good, but MoveLeft is not symmetric. Meaning it will do this:
    // 1. if the current item is expandable and expanded, collapse it, *but only if the scrollbar is
    //    all the way to the left*.
    // 2. if SH_ItemView_ArrowKeysNavigateIntoChildren is enabled it moves to the current item's
    //    parent.
    // 3. finally it tries to scroll left if it can't do that.
    //
    // The problem here is that because scrolling left is still the last-resort icon, pressing right
    // to expand an item and then perhaps scrolling right is not "undone" by pressing left, since
    // we've now scrolled so the collapse doesn't happen and instead we jump to the parent node.
    //
    // To fix this, we scroll first, then handle the other two cases

    QModelIndex current = currentIndex();

    if(selectionBehavior() == QAbstractItemView::SelectItems ||
       selectionBehavior() == QAbstractItemView::SelectColumns)
    {
      int col = header()->visualIndex(current.column());
      // move left one
      col--;

      // keep moving if the column is hiden
      while(col >= 0 && isColumnHidden(header()->logicalIndex(col)))
        col--;

      // if we landed on a valid column (we may have gone negative if we were already on the first
      // column) return it
      if(col >= 0)
      {
        QModelIndex sel = current.sibling(current.row(), header()->logicalIndex(col));
        if(sel.isValid())
          return sel;
      }
    }

    // if we didn't scroll left above by selecting an index, and the scrollbar is still not
    // minimised, scroll it left now.
    QScrollBar *scroll = horizontalScrollBar();
    if(scroll->value() > scroll->minimum())
    {
      scroll->setValue(scroll->value() - scroll->singleStep());
      return current;
    }

    // otherwise we can use the default behaviour
  }

  return QTreeView::moveCursor(cursorAction, modifiers);
}
