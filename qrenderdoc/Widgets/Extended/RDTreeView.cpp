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

#include "RDTreeView.h"
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QProxyStyle>
#include <QStylePainter>
#include <QToolTip>
#include <QWheelEvent>

RDTreeViewDelegate::RDTreeViewDelegate(RDTreeView *view) : ForwardingDelegate(view), m_View(view)
{
}

QSize RDTreeViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QSize ret = ForwardingDelegate::sizeHint(option, index);

  if(m_View->ignoreIconSize())
    ret.setHeight(qMax(option.decorationSize.height() + 2, ret.height()));

  // expand a pixel for the grid lines
  if(m_View->visibleGridLines())
    ret.setWidth(ret.width() + 1);

  // ensure we have at least the margin on top of font size. If the style applied more, don't add to
  // it.
  int minHeight = option.fontMetrics.height();
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

  m_ElidedTooltip = new RDTipLabel(viewport());
  m_ElidedTooltip->hide();
}

RDTreeView::~RDTreeView()
{
  setModel(NULL);

  delete m_ElidedTooltip;
}

void RDTreeView::mouseMoveEvent(QMouseEvent *e)
{
  if(m_ElidedTooltip->isVisible() && !m_ElidedTooltip->geometry().contains(QCursor::pos()))
    m_ElidedTooltip->hide();

  m_currentHoverIndex = indexAt(e->pos());
  QTreeView::mouseMoveEvent(e);
}

void RDTreeView::wheelEvent(QWheelEvent *e)
{
  QTreeView::wheelEvent(e);
  m_currentHoverIndex = indexAt(e->pos());
}

void RDTreeView::leaveEvent(QEvent *e)
{
  if(m_ElidedTooltip->isVisible() && !m_ElidedTooltip->geometry().contains(QCursor::pos()))
    m_ElidedTooltip->hide();

  m_currentHoverIndex = QModelIndex();

  emit leave(e);

  QTreeView::leaveEvent(e);
}

void RDTreeView::keyPressEvent(QKeyEvent *e)
{
  emit(keyPress(e));

  QTreeView::keyPressEvent(e);
}

bool RDTreeView::viewportEvent(QEvent *event)
{
  if(m_TooltipElidedItems && event->type() == QEvent::ToolTip)
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
          // underneath the cursor instead of next to it (so that the tooltip lines up over the row)
          m_ElidedTooltip->move(viewport()->mapToGlobal(option.rect.topLeft()));
          m_ElidedTooltip->setText(fullText);
          m_ElidedTooltip->show();
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

void RDTreeView::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
  m_currentHoverIndex = QModelIndex();
}

void RDTreeView::saveExpansion(RDTreeViewExpansionState &state, const ExpansionKeyGen &keygen)
{
  state.clear();

  for(int i = 0; i < model()->rowCount(); i++)
    saveExpansionFromRow(state, model()->index(i, 0), 0, keygen);
}

void RDTreeView::applyExpansion(const RDTreeViewExpansionState &state, const ExpansionKeyGen &keygen)
{
  for(int i = 0; i < model()->rowCount(); i++)
    applyExpansionToRow(state, model()->index(i, 0), 0, keygen);
}

void RDTreeView::saveExpansionFromRow(RDTreeViewExpansionState &state, QModelIndex idx, uint seed,
                                      const ExpansionKeyGen &keygen)
{
  if(!idx.isValid())
    return;

  uint key = keygen(idx, seed);
  if(isExpanded(idx))
  {
    state.insert(key);

    // only recurse to children if this one is expanded - forget expansion state under collapsed
    // branches. Technically we're losing information here but it allows us to skip a full expensive
    // search
    for(int i = 0; i < model()->rowCount(idx); i++)
      saveExpansionFromRow(state, model()->index(i, 0, idx), seed, keygen);
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

void RDTreeView::fillBranchesRect(QPainter *painter, const QRect &rect, const QModelIndex &index) const
{
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

  opt.rect = rect;
  opt.rect.setWidth(depth * indentation());
  opt.showDecorationSelected = true;
  style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, this);
}

void RDTreeView::drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const
{
  if(m_fillBranchRect)
    fillBranchesRect(painter, rect, index);

  if(m_VisibleBranches)
  {
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

    QStyleOptionViewItem opt = viewOptions();

    opt.rect = primitive;

    // unfortunately QStyle::State_Children doesn't render ONLY the
    // open-toggle-button, but the vertical line upwards to a previous sibling.
    // For consistency, draw one downwards too.
    opt.state = QStyle::State_Children | QStyle::State_Sibling;
    if(isExpanded(index))
      opt.state |= QStyle::State_Open;

    style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, painter, this);
  }
}
