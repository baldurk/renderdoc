/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "CollapseGroupBox.h"
#include <QMouseEvent>
#include <QStyleOptionGroupBox>
#include <QStylePainter>

CollapseGroupBox::CollapseGroupBox(QWidget *parent) : QGroupBox(parent)
{
}

CollapseGroupBox::~CollapseGroupBox()
{
}

void CollapseGroupBox::setMaximumSize(const QSize &size)
{
  // if collapsed, silently update the 'original' max height for when we un-collapse. Otherwise
  // forward the call
  if(m_collapsed)
  {
    m_prevMaxHeight = size.height();
    QGroupBox::setMaximumSize(QSize(size.width(), maximumHeight()));
  }
  else
  {
    QGroupBox::setMaximumSize(size);
  }
}

void CollapseGroupBox::setMaximumHeight(int maxh)
{
  // if collapsed, silently update the 'original' max height for when we un-collapse. Otherwise
  // forward the call
  if(m_collapsed)
    m_prevMaxHeight = maxh;
  else
    QGroupBox::setMaximumHeight(maxh);
}

void CollapseGroupBox::setCollapsed(bool coll)
{
  // redundant call
  if(m_collapsed == coll)
    return;

  // if not collapsed, save the current maximum height so we can restore it again and set the
  // 'collapsed' maximum height. If we *are* collapsed, then restore the maximum height
  if(m_collapsed)
  {
    QGroupBox::setMaximumHeight(m_prevMaxHeight);
  }
  else
  {
    m_prevMaxHeight = maximumHeight();

    QStyleOptionGroupBox option;
    initStyleOption(&option);

    option.subControls |= QStyle::SC_GroupBoxCheckBox;

    QRect contentsRect =
        style()->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxContents, this);

    QGroupBox::setMaximumHeight(height() - contentsRect.height());
  }

  m_collapsed = coll;

  update();
}

void CollapseGroupBox::paintEvent(QPaintEvent *)
{
  QStylePainter paint(this);
  QStyleOptionGroupBox option;
  initStyleOption(&option);

  // pretend we have a groupbox so the painting allocates space for it
  option.subControls |= QStyle::SC_GroupBoxCheckBox;

  paint.drawComplexControl(QStyle::CC_GroupBox, option);

  // now paint over the checkbox

  QRect checkBoxRect =
      style()->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxCheckBox, this);

  paint.fillRect(checkBoxRect, palette().brush(QPalette::Window));

  QStyleOption arrowOpt = option;
  arrowOpt.rect = checkBoxRect;
  paint.drawPrimitive(m_collapsed ? QStyle::PE_IndicatorArrowRight : QStyle::PE_IndicatorArrowDown,
                      arrowOpt);
}

void CollapseGroupBox::mouseReleaseEvent(QMouseEvent *event)
{
  if(event->button() != Qt::LeftButton)
  {
    event->ignore();
    return;
  }

  QStyleOptionGroupBox option;
  initStyleOption(&option);

  // pretend we have a groupbox so we can hit-test for it
  option.subControls |= QStyle::SC_GroupBoxCheckBox;

  if(style()->hitTestComplexControl(QStyle::CC_GroupBox, &option, event->pos(), this) &
     (QStyle::SC_GroupBoxCheckBox | QStyle::SC_GroupBoxLabel))
  {
    setCollapsed(!m_collapsed);
  }
}
