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

#include "RDTweakedNativeStyle.h"
#include <QDebug>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QStyleOption>

namespace Constants
{
static const int MenuBarItemHPadding = 4;
static const int MenuBarItemVPadding = 2;
static const int MenuBarItemSpacing = 4;
};

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

  // menu bar items can be sized for both the icon *and* the text
  if(type == CT_MenuBarItem)
  {
    const QStyleOptionMenuItem *menuopt = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
    int iconSize = pixelMetric(QStyle::PM_SmallIconSize, opt, widget);
    sz = menuopt->fontMetrics.size(Qt::TextShowMnemonic, menuopt->text);

    if(!menuopt->icon.isNull())
    {
      sz.setWidth(sz.width() + Constants::MenuBarItemSpacing + iconSize);
      sz = sz.expandedTo(QSize(1, iconSize));
    }

    sz += QSize(Constants::MenuBarItemHPadding * 2 + Constants::MenuBarItemSpacing * 2,
                Constants::MenuBarItemVPadding * 2);

    return sz;
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
    const QStyleOptionToolButton *toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(opt);

    QPen oldPen = p->pen();
    QColor backCol = opt->palette.color(QPalette::Normal, QPalette::Highlight);
    backCol.setAlphaF(0.2);

    QStyleOptionToolButton menu;

    // draw the menu arrow, if there is one
    if((toolbutton->subControls & SC_ToolButtonMenu) ||
       (toolbutton->features & QStyleOptionToolButton::MenuButtonPopup))
    {
      menu = *toolbutton;
      menu.rect = subControlRect(control, opt, SC_ToolButtonMenu, widget);
    }

    QStyle::State masked = opt->state & (State_On | State_MouseOver);

    if(!(opt->state & State_Enabled))
      masked &= ~State_MouseOver;

    if(masked)
    {
      QRect rect = opt->rect.adjusted(0, 0, -1, -1);
      p->setPen(opt->palette.color(QPalette::Shadow));
      p->drawRect(rect);
      if(menu.rect.isValid())
        p->drawLine(menu.rect.topLeft(), menu.rect.bottomLeft());

      // when the mouse is over, make it a little stronger
      if(masked & State_MouseOver)
        backCol.setAlphaF(0.4);

      p->fillRect(rect, QBrush(backCol));
    }

    p->setPen(oldPen);

    QStyleOptionToolButton labelTextIcon = *toolbutton;
    labelTextIcon.rect = subControlRect(control, opt, SC_ToolButton, widget);

    // draw the label text/icon
    drawControl(CE_ToolButtonLabel, &labelTextIcon, p, widget);

    if(menu.rect.isValid())
    {
      menu.rect.adjust(2, 0, 0, 0);
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
  if(control == QStyle::CE_MenuBarItem)
  {
    // we can't take over control of just rendering the icon/text, so we call down to common style
    // to draw the background since then we know how to render matching text over the top.
    const QStyleOptionMenuItem *menuopt = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);

    QRect rect =
        menuopt->rect.adjusted(Constants::MenuBarItemSpacing, 0, -Constants::MenuBarItemSpacing, 0);

    const bool selected = menuopt->state & State_Selected;
    const bool hovered = menuopt->state & State_MouseOver;
    const bool enabled = menuopt->state & State_Enabled;

    QPalette::ColorRole textRole = QPalette::ButtonText;

    if(enabled && (selected || hovered))
    {
      p->fillRect(rect, opt->palette.brush(QPalette::Highlight));
      textRole = QPalette::HighlightedText;
    }

    int flags = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
    if(!styleHint(SH_UnderlineShortcut, opt, widget))
      flags |= Qt::TextHideMnemonic;

    rect.adjust(Constants::MenuBarItemHPadding, Constants::MenuBarItemVPadding,
                -Constants::MenuBarItemHPadding, -Constants::MenuBarItemVPadding);

    int iconSize = pixelMetric(QStyle::PM_SmallIconSize, opt, widget);

    QPixmap pix = menuopt->icon.pixmap(
        iconSize, iconSize, (menuopt->state & State_Enabled) ? QIcon::Normal : QIcon::Disabled);

    if(!pix.isNull())
    {
      QRect iconRect = rect;
      iconRect.setWidth(iconSize);
      drawItemPixmap(p, iconRect, flags, pix);
      rect.adjust(Constants::MenuBarItemSpacing + iconSize, 0, 0, 0);
    }

    drawItemText(p, rect, flags, menuopt->palette, enabled, menuopt->text, textRole);

    return;
  }

// https://bugreports.qt.io/browse/QTBUG-14949
// work around itemview rendering bug - the first line in a multi-line text that is elided stops
// all subsequent text from rendering. Should be fixed in 5.11, but for all other versions we need
// to manually step in. We manually elide the text before calling down to the style
//
// However in 5.11.1 at least on macOS it still seems to be broken
#if 1    //(QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
  if(control == QStyle::CE_ItemViewItem)
  {
    const QStyleOptionViewItem *viewopt = qstyleoption_cast<const QStyleOptionViewItem *>(opt);

    // only if we're eliding, not wrapping, and we have multiple lines
    if((viewopt->features & QStyleOptionViewItem::WrapText) == 0 &&
       viewopt->text.contains(QChar::LineSeparator))
    {
      const int hmargin = pixelMetric(QStyle::PM_FocusFrameHMargin, 0, widget) + 1;

      QRect textRect =
          subElementRect(SE_ItemViewItemText, viewopt, widget).adjusted(hmargin, 0, -hmargin, 0);

      QFontMetrics metrics(viewopt->font);

      QStringList lines = viewopt->text.split(QChar::LineSeparator);

      for(QString &line : lines)
        line = metrics.elidedText(line, viewopt->textElideMode, textRect.width(), 0);

      QStyleOptionViewItem elided = *viewopt;

      elided.text = lines.join(QChar::LineSeparator);

      QProxyStyle::drawControl(control, &elided, p, widget);
      return;
    }
  }
#endif

  QProxyStyle::drawControl(control, opt, p, widget);
}
