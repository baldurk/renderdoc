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

#include "RDStyle.h"
#include <QDebug>
#include <QPainter>
#include <QPen>
#include <QStyleOption>

RDStyle::RDStyle(ColorScheme scheme) : QProxyStyle()
{
}

RDStyle::~RDStyle()
{
}

QSize RDStyle::sizeFromContents(ContentsType type, const QStyleOption *opt, const QSize &size,
                                const QWidget *widget) const
{
  QSize sz = size;

  // Toolbuttons are always at least icon sized, for consistency.
  if(type == QStyle::CT_ToolButton)
  {
    const QStyleOptionToolButton *toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(opt);
    if(toolbutton)
      sz = sz.expandedTo(toolbutton->iconSize);
  }

  return QProxyStyle::sizeFromContents(type, opt, sz, widget);
}

int RDStyle::pixelMetric(PixelMetric metric, const QStyleOption *opt, const QWidget *widget) const
{
  // toolbuttons don't shift their text when clicked.
  if(metric == QStyle::PM_ButtonShiftHorizontal || metric == QStyle::PM_ButtonShiftVertical)
  {
    if(opt && (opt->state & State_AutoRaise))
      return 0;
  }

  return QProxyStyle::pixelMetric(metric, opt, widget);
}

void RDStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *opt,
                                 QPainter *p, const QWidget *widget) const
{
  // autoraise toolbuttons are rendered flat with a semi-transparent highlight to show their state.
  if(control == QStyle::CC_ToolButton && (opt->state & State_AutoRaise))
  {
    QRect dropdown = subControlRect(control, opt, SC_ToolButtonMenu, widget);

    QPen oldPen = p->pen();
    QColor backCol = opt->palette.color(QPalette::Normal, QPalette::Highlight);

    backCol.setAlphaF(0.2);
    QStyle::State masked = opt->state & (State_On | State_MouseOver);

    // when the mouse is over, make it a little stronger
    if(masked && (masked & State_MouseOver))
      backCol.setAlphaF(0.4);

    if(masked)
    {
      QRect rect = opt->rect.adjusted(0, 0, -1, -1);
      p->setPen(opt->palette.color(QPalette::Shadow));
      p->drawRect(rect);
      p->fillRect(rect, QBrush(backCol));
    }

    p->setPen(oldPen);

    const QStyleOptionToolButton *toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(opt);

    QStyleOptionToolButton labelTextIcon = *toolbutton;
    labelTextIcon.rect = subControlRect(control, opt, SC_ToolButton, widget);

    // draw the label text/icon
    drawControl(CE_ToolButtonLabel, &labelTextIcon, p, widget);

    // draw the menu arrow, if there is one
    if((toolbutton->subControls & SC_ToolButtonMenu) ||
       (toolbutton->features & QStyleOptionToolButton::HasMenu))
    {
      QStyleOptionToolButton menu = *toolbutton;
      menu.rect = subControlRect(control, opt, SC_ToolButtonMenu, widget);
      drawPrimitive(PE_IndicatorArrowDown, &menu, p, widget);
    }

    return;
  }

  return QProxyStyle::drawComplexControl(control, opt, p, widget);
}

void RDStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *opt, QPainter *p,
                            const QWidget *widget) const
{
  QProxyStyle::drawPrimitive(element, opt, p, widget);
}

void RDStyle::drawControl(ControlElement control, const QStyleOption *opt, QPainter *p,
                          const QWidget *widget) const
{
  QProxyStyle::drawControl(control, opt, p, widget);
}
