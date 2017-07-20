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

  for(int i = 0; i < QPalette::NColorRoles; i++)
  {
    QPalette::ColorRole role = (QPalette::ColorRole)i;

    // skip tooltip roles entirely
    if(role == QPalette::ToolTipBase || role == QPalette::ToolTipText)
      continue;

    // with the exception of link text, the disabled version is desaturated
    QColor col = pal.color(QPalette::Inactive, role);

    if(role != QPalette::Link)
    {
      col.getHsv(&h, &s, &v);
      col.setHsv(h, 0, v);
    }

    // the disabled version is closer to mid grey than inactive
    if(getLuminance(col) > 0.5)
      pal.setColor(QPalette::Disabled, role, col.darker(125));
    else
      pal.setColor(QPalette::Disabled, role, col.lighter(125));
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
  return RDTweakedNativeStyle::pixelMetric(metric, opt, widget);
}

QIcon RDStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption *opt,
                            const QWidget *widget) const
{
  return RDTweakedNativeStyle::standardIcon(standardIcon, opt, widget);
}

void RDStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *opt,
                                 QPainter *p, const QWidget *widget) const
{
  return RDTweakedNativeStyle::drawComplexControl(control, opt, p, widget);
}

void RDStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *opt, QPainter *p,
                            const QWidget *widget) const
{
  RDTweakedNativeStyle::drawPrimitive(element, opt, p, widget);
}

void RDStyle::drawControl(ControlElement control, const QStyleOption *opt, QPainter *p,
                          const QWidget *widget) const
{
  RDTweakedNativeStyle::drawControl(control, opt, p, widget);
}
