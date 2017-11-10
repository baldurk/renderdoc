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

#include "RDTweakedNativeStyle.h"
#include <QDebug>
#include <QPainter>
#include <QPen>
#include <QStyleOption>

RDTweakedNativeStyle::RDTweakedNativeStyle(QStyle *parent) : QProxyStyle(parent)
{
}

RDTweakedNativeStyle::~RDTweakedNativeStyle()
{
}

QRect RDTweakedNativeStyle::subElementRect(SubElement element, const QStyleOption *opt,
                                           const QWidget *widget) const
{
  QRect ret = QProxyStyle::subElementRect(element, opt, widget);

  if(element == QStyle::SE_DockWidgetCloseButton || element == QStyle::SE_DockWidgetFloatButton)
  {
    int width = pixelMetric(QStyle::PM_TabCloseIndicatorWidth, opt, widget);
    int height = pixelMetric(QStyle::PM_TabCloseIndicatorHeight, opt, widget);

    QPoint c = ret.center();
    ret.setSize(QSize(width, height));
    ret.moveCenter(c);
  }

  return ret;
}

QSize RDTweakedNativeStyle::sizeFromContents(ContentsType type, const QStyleOption *opt,
                                             const QSize &size, const QWidget *widget) const
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

int RDTweakedNativeStyle::pixelMetric(PixelMetric metric, const QStyleOption *opt,
                                      const QWidget *widget) const
{
  // toolbuttons don't shift their text when clicked.
  if(metric == QStyle::PM_ButtonShiftHorizontal || metric == QStyle::PM_ButtonShiftVertical)
  {
    if(opt && (opt->state & State_AutoRaise))
      return 0;
  }

  return QProxyStyle::pixelMetric(metric, opt, widget);
}

int RDTweakedNativeStyle::styleHint(StyleHint stylehint, const QStyleOption *opt,
                                    const QWidget *widget, QStyleHintReturn *returnData) const
{
  if(stylehint == QStyle::SH_Menu_Scrollable)
    return 1;

  return QProxyStyle::styleHint(stylehint, opt, widget, returnData);
}

QIcon RDTweakedNativeStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption *opt,
                                         const QWidget *widget) const
{
  if(standardIcon == QStyle::SP_TitleBarCloseButton)
  {
    int sz = pixelMetric(QStyle::PM_SmallIconSize);

    return QIcon(QPixmap(QSize(sz, sz)));
  }

  return QProxyStyle::standardIcon(standardIcon, opt, widget);
}

void RDTweakedNativeStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *opt,
                                              QPainter *p, const QWidget *widget) const
{
  // autoraise toolbuttons are rendered flat with a semi-transparent highlight to show their state.
  if(control == QStyle::CC_ToolButton && (opt->state & State_AutoRaise))
  {
    QPen oldPen = p->pen();
    QColor backCol = opt->palette.color(QPalette::Normal, QPalette::Highlight);

    backCol.setAlphaF(0.2);
    QStyle::State masked = opt->state & (State_On | State_MouseOver);

    if(!(opt->state & State_Enabled))
      masked &= ~State_MouseOver;

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
       (toolbutton->features & QStyleOptionToolButton::MenuButtonPopup))
    {
      QStyleOptionToolButton menu = *toolbutton;
      menu.rect = subControlRect(control, opt, SC_ToolButtonMenu, widget);
      drawPrimitive(PE_IndicatorArrowDown, &menu, p, widget);
    }

    return;
  }

  return QProxyStyle::drawComplexControl(control, opt, p, widget);
}

void RDTweakedNativeStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *opt,
                                         QPainter *p, const QWidget *widget) const
{
  if(element == QStyle::PE_IndicatorBranch)
  {
    QPen oldPen = p->pen();

    if(opt->state & State_Children)
    {
      bool aa = p->testRenderHint(QPainter::Antialiasing);
      p->setRenderHint(QPainter::Antialiasing);

      QColor col = opt->palette.color(QPalette::Text);

      if(opt->state & State_MouseOver)
      {
        QColor highlightCol = opt->palette.color(QPalette::Highlight);

        col.setRedF(col.redF() * 0.6 + highlightCol.redF() * 0.4);
        col.setGreenF(col.greenF() * 0.6 + highlightCol.greenF() * 0.4);
        col.setBlueF(col.blueF() * 0.6 + highlightCol.blueF() * 0.4);
      }

      p->setPen(QPen(col, 2.0));

      QPainterPath path;

      QPolygonF poly;

      QRectF rect = opt->rect;

      {
        qreal newdim = qMin(14.0, qMin(rect.height(), rect.width()));
        QPointF c = rect.center();
        rect.setTop(c.y() - newdim / 2);
        rect.setLeft(c.x() - newdim / 2);
        rect.setWidth(newdim);
        rect.setHeight(newdim);
      }

      rect = rect.adjusted(2, 2, -2, -2);

      if(opt->state & State_Open)
      {
        QPointF pt = rect.center();
        pt.setX(rect.left());
        poly << pt;

        pt = rect.center();
        pt.setY(rect.bottom());
        poly << pt;

        pt = rect.center();
        pt.setX(rect.right());
        poly << pt;

        path.addPolygon(poly);

        p->drawPath(path);
      }
      else
      {
        QPointF pt = rect.center();
        pt.setY(rect.top());
        poly << pt;

        pt = rect.center();
        pt.setX(rect.right());
        poly << pt;

        pt = rect.center();
        pt.setY(rect.bottom());
        poly << pt;

        path.addPolygon(poly);

        p->drawPath(path);
      }

      if(!aa)
        p->setRenderHint(QPainter::Antialiasing, false);
    }
    else if(opt->state & (State_Sibling | State_Item))
    {
      p->setPen(QPen(opt->palette.color(QPalette::Midlight), 1.0));

      int bottomY = opt->rect.center().y();

      if(opt->state & State_Sibling)
        bottomY = opt->rect.bottom();

      p->drawLine(QLine(opt->rect.center().x(), opt->rect.top(), opt->rect.center().x(), bottomY));

      if(opt->state & State_Item)
        p->drawLine(opt->rect.center(), QPoint(opt->rect.right(), opt->rect.center().y()));
    }
    p->setPen(oldPen);
    return;
  }
  else if(element == PE_IndicatorTabClose)
  {
    QPen oldPen = p->pen();
    bool aa = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing);

    QColor col = opt->palette.color(QPalette::Text);

    QRectF rect = opt->rect.adjusted(1, 1, -1, -1);

    if(opt->state & (QStyle::State_Raised | QStyle::State_Sunken | QStyle::State_MouseOver))
    {
      QPointF c = rect.center();
      qreal radius = rect.width() / 2.0;

      col = opt->palette.color(QPalette::Base);

      QPainterPath path;

      path.addEllipse(c, radius, radius);

      QColor fillCol = QColor(Qt::red).darker(120);

      if(opt->state & QStyle::State_Sunken)
        fillCol = fillCol.darker(120);

      p->fillPath(path, fillCol);
    }

    p->setPen(QPen(col, 1.5));

    QPointF c = rect.center();

    qreal crossrad = rect.width() / 4.0;

    p->drawLine(c + QPointF(-crossrad, -crossrad), c + QPointF(crossrad, crossrad));
    p->drawLine(c + QPointF(-crossrad, crossrad), c + QPointF(crossrad, -crossrad));

    p->setPen(oldPen);
    if(!aa)
      p->setRenderHint(QPainter::Antialiasing, false);

    return;
  }

  QProxyStyle::drawPrimitive(element, opt, p, widget);
}

void RDTweakedNativeStyle::drawControl(ControlElement control, const QStyleOption *opt, QPainter *p,
                                       const QWidget *widget) const
{
  QProxyStyle::drawControl(control, opt, p, widget);
}
