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
#include <QCommonStyle>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleOption>
#include <QtMath>
#include "Code/QRDUtils.h"

RDStyle::RDStyle(ColorScheme scheme) : RDTweakedNativeStyle()
{
  m_Scheme = scheme;
}

RDStyle::~RDStyle()
{
}

void RDStyle::polish(QPalette &pal)
{
  int h = 0, s = 0, v = 0;

  QColor windowText;
  QColor window;
  QColor base;
  QColor highlight;
  QColor tooltip;

  if(m_Scheme == Light)
  {
    window = QColor(225, 225, 225);
    windowText = QColor(Qt::black);
    base = QColor(Qt::white);
    highlight = QColor(80, 110, 160);
    tooltip = QColor(250, 245, 200);
  }
  else
  {
    window = QColor(45, 55, 60);
    windowText = QColor(225, 225, 225);
    base = QColor(22, 27, 30);
    highlight = QColor(100, 130, 200);
    tooltip = QColor(70, 70, 65);
  }

  QColor light = window.lighter(150);
  QColor mid = window.darker(150);
  QColor dark = mid.darker(150);

  QColor text = windowText;

  pal = QPalette(windowText, window, light, dark, mid, text, base);

  pal.setColor(QPalette::Shadow, Qt::black);

  if(m_Scheme == Light)
    pal.setColor(QPalette::AlternateBase, base.darker(110));
  else
    pal.setColor(QPalette::AlternateBase, base.lighter(110));

  pal.setColor(QPalette::ToolTipBase, tooltip);
  pal.setColor(QPalette::ToolTipText, text);

  pal.setColor(QPalette::Highlight, highlight);
  // inactive highlight is desaturated
  highlight.getHsv(&h, &s, &v);
  highlight.setHsv(h, int(s * 0.5), v);
  pal.setColor(QPalette::Inactive, QPalette::Highlight, highlight);

  pal.setColor(QPalette::HighlightedText, Qt::white);

  // links are based on the highlight colour
  QColor link = highlight.lighter(105);
  pal.setColor(QPalette::Link, link);

  // visited links are desaturated
  QColor linkVisited = link;
  linkVisited.getHsv(&h, &s, &v);
  linkVisited.setHsv(h, 0, v);
  pal.setColor(QPalette::LinkVisited, linkVisited);

  // for the 'text' type roles, make the disabled colour half as bright
  for(QPalette::ColorRole role :
      {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::Highlight,
       QPalette::HighlightedText, QPalette::Link, QPalette::LinkVisited})
  {
    QColor col = pal.color(QPalette::Inactive, role);

    col.getHsv(&h, &s, &v);

    // with the exception of link text, the disabled version is desaturated
    if(role != QPalette::Link)
      s = 0;

    // black is the only colour that gets brighter, any other colour gets darker
    if(s == 0 && v == 0)
    {
      pal.setColor(QPalette::Disabled, role, QColor(160, 160, 160));
    }
    else
    {
      col.setHsv(h, s, v / 2);
      pal.setColor(QPalette::Disabled, role, col);
    }
  }

  // the 'base' roles get every so slightly darker, but not as much as text
  for(QPalette::ColorRole role : {QPalette::Base, QPalette::Window, QPalette::Button})
  {
    QColor col = pal.color(QPalette::Inactive, role);

    col.getHsv(&h, &s, &v);
    col.setHsv(h, s, v * 0.9);
    pal.setColor(QPalette::Disabled, role, col);
  }
}

QRect RDStyle::subElementRect(SubElement element, const QStyleOption *opt, const QWidget *widget) const
{
  return RDTweakedNativeStyle::subElementRect(element, opt, widget);
}

QSize RDStyle::sizeFromContents(ContentsType type, const QStyleOption *opt, const QSize &size,
                                const QWidget *widget) const
{
  return RDTweakedNativeStyle::sizeFromContents(type, opt, size, widget);
}

int RDStyle::pixelMetric(PixelMetric metric, const QStyleOption *opt, const QWidget *widget) const
{
  if(metric == QStyle::PM_ButtonShiftHorizontal || metric == QStyle::PM_ButtonShiftVertical)
  {
    if(opt && (opt->state & State_AutoRaise) == 0)
      return 1;
  }
  return RDTweakedNativeStyle::pixelMetric(metric, opt, widget);
}

int RDStyle::styleHint(StyleHint stylehint, const QStyleOption *opt, const QWidget *widget,
                       QStyleHintReturn *returnData) const
{
  if(stylehint == QStyle::SH_EtchDisabledText || stylehint == QStyle::SH_DitherDisabledText)
    return 0;

  return RDTweakedNativeStyle::styleHint(stylehint, opt, widget, returnData);
}

QIcon RDStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption *opt,
                            const QWidget *widget) const
{
  return RDTweakedNativeStyle::standardIcon(standardIcon, opt, widget);
}

void RDStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *opt,
                                 QPainter *p, const QWidget *widget) const
{
  // let the tweaked native style render autoraise tool buttons
  if(control == QStyle::CC_ToolButton && (opt->state & State_AutoRaise) == 0)
  {
    drawRoundedRectBorder(opt, p, widget, QPalette::Button, true);

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

  return RDTweakedNativeStyle::drawComplexControl(control, opt, p, widget);
}

void RDStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *opt, QPainter *p,
                            const QWidget *widget) const
{
  if(element == QStyle::PE_PanelLineEdit)
  {
    const QStyleOptionFrame *frame = qstyleoption_cast<const QStyleOptionFrame *>(opt);

    if(frame && frame->lineWidth > 0)
    {
      QStyleOption o = *opt;
      o.state &= ~State_Sunken;
      drawRoundedRectBorder(&o, p, widget, QPalette::Base, false);
    }

    return;
  }

  RDTweakedNativeStyle::drawPrimitive(element, opt, p, widget);
}

const QBrush &RDStyle::outlineBrush(const QPalette &pal) const
{
  return m_Scheme == Light ? pal.brush(QPalette::WindowText) : pal.brush(QPalette::Light);
}

void RDStyle::drawControl(ControlElement control, const QStyleOption *opt, QPainter *p,
                          const QWidget *widget) const
{
  if(control == CE_PushButton)
  {
    drawRoundedRectBorder(opt, p, widget, QPalette::Button, true);

    QCommonStyle::drawControl(CE_PushButtonLabel, opt, p, widget);
    return;
  }
  else if(control == CE_PushButtonBevel)
  {
    drawRoundedRectBorder(opt, p, widget, QPalette::Button, true);
    return;
  }
  else if(control == CE_RadioButton)
  {
    const QStyleOptionButton *radiobutton = qstyleoption_cast<const QStyleOptionButton *>(opt);
    if(radiobutton)
    {
      QRectF rect = subElementRect(SE_CheckBoxIndicator, opt, widget);

      rect = rect.adjusted(1.5, 1.5, -1, -1);

      p->save();
      p->setRenderHint(QPainter::Antialiasing);

      if(opt->state & State_HasFocus)
      {
        QPainterPath highlight;
        highlight.addEllipse(rect.center(), rect.width() / 2.0 + 1.25, rect.height() / 2.0 + 1.25);

        p->fillPath(highlight, opt->palette.brush(QPalette::Highlight));
      }

      QPainterPath path;
      path.addEllipse(rect.center(), rect.width() / 2.0, rect.height() / 2.0);

      p->fillPath(path, outlineBrush(opt->palette));

      rect = rect.adjusted(1, 1, -1, -1);

      path = QPainterPath();
      path.addEllipse(rect.center(), rect.width() / 2.0, rect.height() / 2.0);

      if(opt->state & State_Sunken)
        p->fillPath(path, opt->palette.brush(QPalette::Midlight));
      else
        p->fillPath(path, opt->palette.brush(QPalette::Button));

      if(opt->state & State_On)
      {
        rect = rect.adjusted(1.5, 1.5, -1.5, -1.5);

        path = QPainterPath();
        path.addEllipse(rect.center(), rect.width() / 2.0, rect.height() / 2.0);

        p->fillPath(path, opt->palette.brush(QPalette::ButtonText));
      }

      p->restore();

      QStyleOptionButton labelText = *radiobutton;
      labelText.rect = subElementRect(SE_RadioButtonContents, &labelText, widget);
      drawControl(CE_RadioButtonLabel, &labelText, p, widget);
    }

    return;
  }
  else if(control == CE_CheckBox)
  {
    const QStyleOptionButton *checkbox = qstyleoption_cast<const QStyleOptionButton *>(opt);
    if(checkbox)
    {
      QRectF rect = subElementRect(SE_CheckBoxIndicator, opt, widget).adjusted(1, 1, -1, -1);

      QPen outlinePen(outlineBrush(opt->palette), 1.0);

      p->save();
      p->setRenderHint(QPainter::Antialiasing);

      if(opt->state & State_HasFocus)
      {
        QPainterPath highlight;
        highlight.addRoundedRect(rect.adjusted(-0.5, -0.5, 0.5, 0.5), 1.0, 1.0);

        p->strokePath(highlight.translated(QPointF(0.5, 0.5)),
                      QPen(opt->palette.brush(QPalette::Highlight), 1.5));
      }

      QPainterPath path;
      path.addRoundedRect(rect, 1.0, 1.0);

      p->setPen(outlinePen);
      p->drawPath(path.translated(QPointF(0.5, 0.5)));

      rect = rect.adjusted(2, 2, -1, -1);

      if(opt->state & State_On)
        p->fillRect(rect, opt->palette.brush(QPalette::ButtonText));

      p->restore();

      QStyleOptionButton labelText = *checkbox;
      labelText.rect = subElementRect(SE_CheckBoxContents, &labelText, widget);
      drawControl(CE_CheckBoxLabel, &labelText, p, widget);
    }

    return;
  }

  RDTweakedNativeStyle::drawControl(control, opt, p, widget);
}

void RDStyle::drawRoundedRectBorder(const QStyleOption *opt, QPainter *p, const QWidget *widget,
                                    QPalette::ColorRole fillRole, bool shadow) const
{
  QPen outlinePen(outlineBrush(opt->palette), 1.0);

  if(opt->state & State_HasFocus)
    outlinePen = QPen(opt->palette.brush(QPalette::Highlight), 1.5);

  p->save();

  p->setRenderHint(QPainter::Antialiasing);

  int xshift = pixelMetric(PM_ButtonShiftHorizontal, opt, widget);
  int yshift = pixelMetric(PM_ButtonShiftVertical, opt, widget);

  QRect rect = opt->rect.adjusted(0, 0, -1, -1);

  if(opt->state & State_Sunken)
  {
    rect.setLeft(rect.left() + xshift);
    rect.setTop(rect.top() + yshift);

    QPainterPath path;
    path.addRoundedRect(rect, 1.0, 1.0);

    p->fillPath(path, opt->palette.brush(QPalette::Midlight));

    p->setPen(outlinePen);
    p->drawPath(path.translated(QPointF(0.5, 0.5)));
  }
  else
  {
    if(shadow)
    {
      rect.setRight(rect.right() - xshift);
      rect.setBottom(rect.bottom() - yshift);
    }

    QPainterPath path;
    path.addRoundedRect(rect, 1.0, 1.0);

    if(shadow)
    {
      p->setPen(QPen(opt->palette.brush(QPalette::Shadow), 1.0));
      p->drawPath(path.translated(QPointF(1.0, 1.0)));
    }

    p->fillPath(path, opt->palette.brush(fillRole));

    p->setPen(outlinePen);
    p->drawPath(path.translated(QPointF(0.5, 0.5)));
  }

  p->restore();
}
