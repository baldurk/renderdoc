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

#include "RDTreeView.h"
#include <QPainter>

RDTreeView::RDTreeView(QWidget *parent) : QTreeView(NULL)
{
}

void RDTreeView::drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const
{
  if(m_DrawBranches)
  {
    QTreeView::drawBranches(painter, rect, index);
  }
  else
  {
    // draw only the expand item, not the branches
    QRect primitive(0, rect.top(), indentation(), rect.height());

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
