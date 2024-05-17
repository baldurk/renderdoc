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

#include "RDTweakedNativeStyle.h"
#include <QDebug>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleOption>

namespace Constants
{
static const int MenuBarItemHPadding = 4;
static const int MenuBarItemVPadding = 2;
static const int MenuBarItemSpacing = 4;
static const int ToolButtonIconSpacing = 4;
};

static QWindow *widgetWindow(const QWidget *widget)
{
  return widget ? widget->window()->windowHandle() : NULL;
}

RDTweakedNativeStyle::RDTweakedNativeStyle(QStyle *parent) : QProxyStyle(parent)
{
}

RDTweakedNativeStyle::~RDTweakedNativeStyle()
{
}

QRect RDTweakedNativeStyle::subControlRect(ComplexControl cc, const QStyleOptionComplex *opt,
                                           SubControl sc, const QWidget *widget) const
{
  if(cc == QStyle::CC_ToolButton)
  {
    int indicatorWidth = proxy()->pixelMetric(PM_MenuButtonIndicator, opt, widget);

    QRect ret = opt->rect;

    const QStyleOptionToolButton *toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(opt);

    // return the normal rect if there's no menu
    if(!shouldDrawToolButtonMenuArrow(toolbutton))
    {
      return ret;
    }

    if(sc == QStyle::SC_ToolButton)
    {
      ret.setRight(ret.right() - indicatorWidth);
    }
    else if(sc == QStyle::SC_ToolButtonMenu)
    {
      ret.setLeft(ret.right() - indicatorWidth);
    }

    return ret;
  }

  return QProxyStyle::subControlRect(cc, opt, sc, widget);
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

  if(type == QStyle::CT_ToolButton)
  {
    const QStyleOptionToolButton *toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(opt);
    if(toolbutton)
      sz = adjustToolButtonSize(toolbutton, sz, widget);
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
    bool hasMenu = false;
    bool hasSeparateMenu = false;

    // draw the menu arrow, if there is one
    if(shouldDrawToolButtonMenuArrow(toolbutton))
    {
      menu = *toolbutton;
      menu.rect = subControlRect(control, opt, SC_ToolButtonMenu, widget);
      hasMenu = true;
      // We always draw an arrow if a menu is present (normally Qt only does it for MenuButtonPopup,
      // where there is both a button with a default action and a menu triggered by a small arrow,
      // and not InstantPopup where there is only a button). If the button uses MenuButtonPopup,
      // we want to draw a line to distinguish the menu part of the button and the main part,
      // but we don't need that line if the arrow is decorative only.
      if(toolbutton->features & QStyleOptionToolButton::MenuButtonPopup)
      {
        hasSeparateMenu = true;
      }
    }

    QStyle::State masked = opt->state & (State_On | State_MouseOver);

    if(!(opt->state & State_Enabled))
      masked &= ~State_MouseOver;

    if(masked)
    {
      QRect rect = opt->rect.adjusted(0, 0, -1, -1);
      p->setPen(opt->palette.color(QPalette::Shadow));
      p->drawRect(rect);
      if(hasSeparateMenu)
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

    if(hasMenu)
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

      // turbo hack to pass desired colour through QTreeView::drawBranches when it can't customise
      // the colour and doesn't set the model index to let us look up this data ourselves :(
      if(oldPen.widthF() == 1234.5f)
        p->setPen(QPen(oldPen.color(), 2.0));
      else
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
    if(!proxy()->styleHint(SH_UnderlineShortcut, opt, widget))
      flags |= Qt::TextHideMnemonic;

    rect.adjust(Constants::MenuBarItemHPadding, Constants::MenuBarItemVPadding,
                -Constants::MenuBarItemHPadding, -Constants::MenuBarItemVPadding);

    int iconSize = pixelMetric(QStyle::PM_SmallIconSize, opt, widget);

    QPixmap pix =
        menuopt->icon.pixmap(widgetWindow(widget), QSize(iconSize, iconSize),
                             (menuopt->state & State_Enabled) ? QIcon::Normal : QIcon::Disabled);

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
  else if(control == QStyle::CE_ToolButtonLabel)
  {
    // unfortunately Qt made a 'fix' at some point to some unalterable magic numbers which reduces
    // the spacing around the icon and ends up being too small at least in cases we care about.
    // So we instead render the label ourselves

    const QStyleOptionToolButton *toolopt = qstyleoption_cast<const QStyleOptionToolButton *>(opt);

    if((toolopt->features & QStyleOptionToolButton::Arrow) && toolopt->arrowType != Qt::NoArrow)
    {
      return QProxyStyle::drawControl(control, opt, p, widget);
    }

    QRect rect = toolopt->rect;

    // even though our style doesn't shift the button contents, this is the tweaked native style so
    // we need to check for that
    if(toolopt->state & (State_Sunken | State_On))
    {
      rect.translate(proxy()->pixelMetric(PM_ButtonShiftHorizontal, toolopt, widget),
                     proxy()->pixelMetric(PM_ButtonShiftVertical, toolopt, widget));
    }

    int textFlags = Qt::TextShowMnemonic;
    if(!proxy()->styleHint(SH_UnderlineShortcut, opt, widget))
      textFlags |= Qt::TextHideMnemonic;

    // fetch the icon if we're not text-only and there's a valid icon
    QPixmap pixmap;
    QSize iconSize = toolopt->iconSize;
    if(!toolopt->icon.isNull() && toolopt->toolButtonStyle != Qt::ToolButtonTextOnly)
    {
      QIcon::Mode mode = QIcon::Normal;

      if((toolopt->state & State_Enabled) == 0)
        mode = QIcon::Disabled;
      else if((opt->state & (State_AutoRaise | State_MouseOver)) ==
              (State_AutoRaise | State_MouseOver))
        mode = QIcon::Active;

      iconSize.setWidth(qMin(toolopt->iconSize.width(), toolopt->rect.width()));
      iconSize.setHeight(qMin(toolopt->iconSize.height(), toolopt->rect.height()));

      pixmap = toolopt->icon.pixmap(widget->window()->windowHandle(), iconSize, mode,
                                    toolopt->state & State_On ? QIcon::On : QIcon::Off);
      double d = widget->devicePixelRatioF();
      iconSize = pixmap.size();
      iconSize /= pixmap.devicePixelRatio();
    }

    // if we're only rendering the icon, render it now centred
    if(toolopt->toolButtonStyle == Qt::ToolButtonIconOnly)
    {
      drawItemPixmap(p, rect, Qt::AlignCenter, pixmap);
    }
    else
    {
      // otherwise we're expecting to render text, set the font
      p->setFont(toolopt->font);

      QRect iconRect = rect, textRect = rect;

      if(toolopt->toolButtonStyle == Qt::ToolButtonTextOnly)
      {
        textFlags |= Qt::AlignCenter;
        iconRect = QRect();
      }
      else if(toolopt->toolButtonStyle == Qt::ToolButtonTextUnderIcon)
      {
        // take spacing above and below for the icon
        iconRect.setHeight(iconSize.height() + Constants::ToolButtonIconSpacing * 2);
        // place the text below the icon
        textRect.setTop(textRect.top() + iconRect.height());
        // center the text below the icon
        textFlags |= Qt::AlignCenter;
      }
      else
      {
        // take spacing left and right for the icon and remove it from the text rect
        iconRect.setWidth(iconSize.width() + Constants::ToolButtonIconSpacing * 2);
        textRect.setLeft(textRect.left() + iconRect.width());

        // left align the text horizontally next to the icon, but still vertically center it.
        textFlags |= Qt::AlignLeft | Qt::AlignVCenter;
      }

      if(iconRect.isValid())
        proxy()->drawItemPixmap(p, QStyle::visualRect(opt->direction, rect, iconRect),
                                Qt::AlignCenter, pixmap);

      // elide text from the right if there's not enough space
      QFontMetrics metrics(toolopt->font);

      int space = metrics.width(QLatin1Char(' '));
      textRect = QStyle::visualRect(opt->direction, rect, textRect);

      if(toolopt->toolButtonStyle == Qt::ToolButtonTextOnly)
      {
        textRect.adjust(3 + space, 0, -3 - space, 0);
      }

      const QString elidedText = metrics.elidedText(toolopt->text, Qt::ElideRight, textRect.width());

      // if we elided, align left now
      if(elidedText.length() < toolopt->text.length())
      {
        textFlags &= ~Qt::AlignCenter;
        textFlags |= Qt::AlignLeft | Qt::AlignVCenter;
      }

      proxy()->drawItemText(p, textRect, textFlags, toolopt->palette,
                            toolopt->state & State_Enabled, elidedText, QPalette::ButtonText);
    }

    return;
  }
// https://bugreports.qt.io/browse/QTBUG-14949
// work around itemview rendering bug - the first line in a multi-line text that is elided stops
// all subsequent text from rendering. Should be fixed in 5.11, but for all other versions we need
// to manually step in. We manually elide the text before calling down to the style
//
// However in 5.11.1 at least on macOS it still seems to be broken
#if 1    //(QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
  else if(control == QStyle::CE_ItemViewItem)
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

bool RDTweakedNativeStyle::shouldDrawToolButtonMenuArrow(const QStyleOptionToolButton *toolbutton) const
{
  // Qt normally only draws the arrow for MenuButtonPopup; we want it for all tools button with
  // menus (including InstantPopup).
  return (toolbutton->subControls & SC_ToolButtonMenu) ||
         (toolbutton->features & QStyleOptionToolButton::HasMenu);
}

QSize RDTweakedNativeStyle::adjustToolButtonSize(const QStyleOptionToolButton *toolbutton,
                                                 const QSize &size, const QWidget *widget) const
{
  QSize sz = size;

  // Toolbuttons are always at least icon sized, for consistency.
  sz = sz.expandedTo(toolbutton->iconSize);

  if(shouldDrawToolButtonMenuArrow(toolbutton))
  {
    // QToolButton::sizeHint automatically increases the width for MenuButtonPopup separate from
    // calling sizeFromContents. But we want to draw the arrow for all tool buttons with menus,
    // not just those using MenuButtonPopup. Check for MenuButtonPopup to avoid increasing the
    // size twice.
    if(!(toolbutton->features & QStyleOptionToolButton::MenuButtonPopup))
    {
      sz.setWidth(sz.width() + proxy()->pixelMetric(PM_MenuButtonIndicator, toolbutton, widget));
    }
  }

  return sz;
}
