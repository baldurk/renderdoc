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
#include <QAbstractItemView>
#include <QComboBox>
#include <QCommonStyle>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleOption>
#include <QtMath>
#include "Code/QRDUtils.h"

namespace Constants
{
static const int ButtonMargin = 6;
static const int ButtonBorder = 1;

static const int HighlightBorder = 2;

static const int CheckWidth = 14;
static const int CheckHeight = 14;
static const int CheckMargin = 3;

static const int GroupMargin = 4;

static const int ScrollButtonDim = 12;
static const int ScrollBarMargin = 2;
static const int ScrollBarMin = ScrollButtonDim;
static const qreal ScrollBarRadius = 4.0;

static const int SeparatorMargin = 2;

static const int ComboMargin = 2;
static const int ComboArrowDim = 12;

static const int SpinButtonDim = 12;
static const int SpinMargin = 1;
};

RDStyle::RDStyle(ColorScheme scheme) : RDTweakedNativeStyle(new QCommonStyle())
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

void RDStyle::polish(QWidget *widget)
{
  if(qobject_cast<QAbstractSlider *>(widget))
    widget->setAttribute(Qt::WA_Hover);
}

QRect RDStyle::subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc,
                              const QWidget *widget) const
{
  if(cc == QStyle::CC_ToolButton)
  {
    int indicatorWidth = proxy()->pixelMetric(PM_MenuButtonIndicator, opt, widget);

    QRect ret = opt->rect;

    const QStyleOptionToolButton *toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(opt);

    // return the normal rect if there's no menu
    if(!(toolbutton->subControls & SC_ToolButtonMenu) &&
       !(toolbutton->features & QStyleOptionToolButton::HasMenu))
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
  else if(cc == QStyle::CC_GroupBox)
  {
    QRect ret = opt->rect;

    if(sc == SC_GroupBoxFrame)
      return ret;

    const QStyleOptionGroupBox *group = qstyleoption_cast<const QStyleOptionGroupBox *>(opt);

    const int border = Constants::ButtonBorder;
    const int lineHeight = group->fontMetrics.height();

    ret.adjust(border, border, -2 * border, -2 * border);

    const int labelHeight = lineHeight + border * 2;

    if(sc == SC_GroupBoxLabel)
    {
      ret.setHeight(labelHeight);
      return ret;
    }

    if(sc == SC_GroupBoxCheckBox)
    {
      return QRect();
    }

    if(sc == QStyle::SC_GroupBoxContents)
    {
      ret.setTop(ret.top() + labelHeight + Constants::GroupMargin);

      return ret;
    }

    return opt->rect;
  }
  else if(cc == QStyle::CC_ScrollBar)
  {
    QRect ret = opt->rect;

    // shrink by the border
    ret.adjust(1, 1, -1, -1);

    // don't have first/last buttons
    if(sc == QStyle::SC_ScrollBarFirst || sc == QStyle::SC_ScrollBarLast)
      return QRect();

    const QStyleOptionSlider *scroll = qstyleoption_cast<const QStyleOptionSlider *>(opt);
    const int range = scroll->maximum - scroll->minimum;

    if(scroll->orientation == Qt::Horizontal)
    {
      if(sc == QStyle::SC_ScrollBarSubLine)
        return ret.adjusted(0, 0, -ret.width() + Constants::ScrollButtonDim, 0);
      if(sc == QStyle::SC_ScrollBarAddLine)
        return ret.adjusted(ret.width() - Constants::ScrollButtonDim, 0, 0, 0);

      const int buttonAdjust = Constants::ScrollButtonDim + Constants::ScrollBarMargin;
      ret.adjust(buttonAdjust, 0, -buttonAdjust, 0);

      if(sc == QStyle::SC_ScrollBarGroove)
        return ret;

      QRect slider = ret;

      if(scroll->maximum > scroll->minimum)
      {
        int sliderSize = qMax(Constants::ScrollBarMin,
                              (scroll->pageStep * ret.width()) / (range + scroll->pageStep));

        slider.setWidth(qMin(slider.width(), sliderSize));
        slider.moveLeft(ret.left() +
                        (scroll->sliderPosition * (ret.width() - slider.width())) / range);
      }
      else
      {
        return QRect();
      }

      if(sc == QStyle::SC_ScrollBarSlider)
        return slider;

      if(sc == QStyle::SC_ScrollBarSubPage)
        return ret.adjusted(0, 0, slider.left() - ret.right(), 0);
      if(sc == QStyle::SC_ScrollBarAddPage)
        return ret.adjusted(slider.right() - ret.left(), 0, 0, 0);
    }
    else
    {
      if(sc == QStyle::SC_ScrollBarSubLine)
        return ret.adjusted(0, 0, 0, -ret.height() + Constants::ScrollButtonDim);
      if(sc == QStyle::SC_ScrollBarAddLine)
        return ret.adjusted(0, ret.height() - Constants::ScrollButtonDim, 0, 0);

      const int buttonAdjust = Constants::ScrollButtonDim + Constants::ScrollBarMargin;
      ret.adjust(0, buttonAdjust, 0, -buttonAdjust);

      if(sc == QStyle::SC_ScrollBarGroove)
        return ret;

      QRect slider = ret;

      if(scroll->maximum > scroll->minimum)
      {
        int sliderSize = qMax(Constants::ScrollBarMin,
                              (scroll->pageStep * ret.height()) / (range + scroll->pageStep));

        slider.setHeight(qMin(slider.height(), sliderSize));
        slider.moveTop(ret.top() +
                       (scroll->sliderPosition * (ret.height() - slider.height())) / range);
      }
      else
      {
        return QRect();
      }

      if(sc == QStyle::SC_ScrollBarSlider)
        return slider;

      if(sc == QStyle::SC_ScrollBarSubPage)
        return ret.adjusted(0, 0, 0, slider.top() - ret.bottom());
      if(sc == QStyle::SC_ScrollBarAddPage)
        return ret.adjusted(0, slider.bottom() - ret.top(), 0, 0);
    }

    return opt->rect;
  }
  else if(cc == QStyle::CC_ComboBox)
  {
    QRect rect = opt->rect;

    if(sc == QStyle::SC_ComboBoxFrame || sc == QStyle::SC_ComboBoxListBoxPopup)
      return rect;

    rect.adjust(Constants::ComboMargin, Constants::ComboMargin, -Constants::ComboMargin,
                -Constants::ComboMargin);

    if(sc == QStyle::SC_ComboBoxEditField)
      return rect.adjusted(0, 0, -Constants::ComboArrowDim, 0);

    if(sc == QStyle::SC_ComboBoxArrow)
      return rect.adjusted(rect.width() - Constants::ComboArrowDim, 0, 0, 0);
  }
  else if(cc == QStyle::CC_SpinBox)
  {
    QRect rect = opt->rect;

    if(sc == QStyle::SC_SpinBoxFrame)
      return rect;

    rect.adjust(Constants::ButtonBorder, Constants::ButtonBorder, -Constants::ButtonBorder,
                -Constants::ButtonBorder);

    if(sc == QStyle::SC_SpinBoxEditField)
      return rect.adjusted(Constants::SpinMargin, Constants::SpinMargin,
                           -Constants::SpinButtonDim - Constants::SpinMargin, -Constants::SpinMargin);

    rect.adjust(rect.width() - Constants::SpinButtonDim, 0, 0, 0);

    int buttonHeight = rect.height() / 2;

    if(sc == QStyle::SC_SpinBoxUp)
      return rect.adjusted(0, 0, 0, -(rect.height() - buttonHeight));

    if(sc == QStyle::SC_SpinBoxDown)
      return rect.adjusted(0, rect.height() - buttonHeight, 0, 0);

    return opt->rect;
  }

  return RDTweakedNativeStyle::subControlRect(cc, opt, sc, widget);
}

QRect RDStyle::subElementRect(SubElement element, const QStyleOption *opt, const QWidget *widget) const
{
  if(element == QStyle::SE_PushButtonContents || element == QStyle::SE_PushButtonFocusRect)
  {
    const int border = Constants::ButtonBorder;
    return opt->rect.adjusted(border, border, -2 * border, -2 * border);
  }
  else if(element == QStyle::SE_RadioButtonFocusRect || element == QStyle::SE_CheckBoxFocusRect)
  {
    return opt->rect;
  }
  else if(element == QStyle::SE_RadioButtonIndicator || element == QStyle::SE_CheckBoxIndicator)
  {
    QRect ret = opt->rect;

    ret.setWidth(Constants::CheckWidth);

    int extra = ret.height() - Constants::CheckHeight;

    ret.setTop((ret.height() - Constants::CheckHeight) / 2);
    ret.setHeight(Constants::CheckHeight);

    return ret;
  }
  else if(element == QStyle::SE_RadioButtonContents || element == QStyle::SE_CheckBoxContents)
  {
    QRect ret = opt->rect;

    ret.setLeft(ret.left() + Constants::CheckWidth + Constants::CheckMargin);

    return ret;
  }

  return RDTweakedNativeStyle::subElementRect(element, opt, widget);
}

QSize RDStyle::sizeFromContents(ContentsType type, const QStyleOption *opt, const QSize &size,
                                const QWidget *widget) const
{
  if(type == CT_PushButton || type == CT_ToolButton)
  {
    const QStyleOptionButton *button = qstyleoption_cast<const QStyleOptionButton *>(opt);

    QSize ret = size;

    // only for pushbuttons with text, ensure a minimum size
    if(type == CT_PushButton && button && !button->text.isEmpty())
    {
      ret.setWidth(qMax(50, ret.width()));
      ret.setHeight(qMax(15, ret.height()));
    }

    // add margin and border
    ret.setHeight(ret.height() + Constants::ButtonMargin + Constants::ButtonBorder * 2);
    ret.setWidth(ret.width() + Constants::ButtonMargin + Constants::ButtonBorder * 2);

    return ret;
  }
  else if(type == CT_CheckBox || type == CT_RadioButton)
  {
    const QStyleOptionButton *button = qstyleoption_cast<const QStyleOptionButton *>(opt);

    QSize ret = size;

    // set minimum height for check/radio
    ret.setHeight(qMax(ret.height(), Constants::CheckHeight) + Constants::HighlightBorder);

    // add width for the check/radio and a gap before the text/icon
    ret.setWidth(Constants::CheckWidth + Constants::CheckMargin + ret.width());

    return ret;
  }
  else if(type == CT_LineEdit)
  {
    QSize ret = size;

    ret.setWidth(Constants::ButtonBorder * 2 + ret.width());
    ret.setHeight(Constants::ButtonBorder * 2 + ret.height());

    return ret;
  }
  else if(type == CT_GroupBox || type == CT_ScrollBar)
  {
    return size;
  }
  else if(type == CT_ComboBox)
  {
    QSize ret = size;

    // make room for both the down arrow button and a potential scrollbar
    ret.setWidth(Constants::ButtonBorder * 2 + Constants::ComboMargin * 2 +
                 Constants::ComboArrowDim + Constants::ScrollButtonDim + ret.width());
    ret.setHeight(Constants::ButtonBorder * 2 + Constants::ComboMargin * 2 + ret.height());

    return ret;
  }
  else if(type == CT_SpinBox)
  {
    QSize ret = size;

    const int margin = Constants::SpinMargin + Constants::ButtonBorder;
    ret.setWidth(margin * 2 + Constants::SpinButtonDim + ret.width());
    ret.setHeight(margin * 2 + ret.height());

    return ret;
  }

  return RDTweakedNativeStyle::sizeFromContents(type, opt, size, widget);
}

int RDStyle::pixelMetric(PixelMetric metric, const QStyleOption *opt, const QWidget *widget) const
{
  if(metric == QStyle::PM_ButtonShiftHorizontal || metric == QStyle::PM_ButtonShiftVertical)
  {
    if(opt && (opt->state & State_AutoRaise) == 0)
      return 1;
  }

  if(metric == PM_ScrollBarExtent)
    return Constants::ScrollButtonDim + 2;

  return RDTweakedNativeStyle::pixelMetric(metric, opt, widget);
}

int RDStyle::styleHint(StyleHint stylehint, const QStyleOption *opt, const QWidget *widget,
                       QStyleHintReturn *returnData) const
{
  if(stylehint == QStyle::SH_EtchDisabledText || stylehint == QStyle::SH_DitherDisabledText)
    return 0;

  if(stylehint == QStyle::SH_ComboBox_PopupFrameStyle)
    return QFrame::StyledPanel | QFrame::Plain;

  if(stylehint == QStyle::SH_ComboBox_Popup)
    return false;

  if(stylehint == SH_ToolTipLabel_Opacity)
    return 255;

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
  else if(control == CC_GroupBox)
  {
    drawRoundedRectBorder(opt, p, widget, QPalette::Window, false);

    const QStyleOptionGroupBox *group = qstyleoption_cast<const QStyleOptionGroupBox *>(opt);

    QRect labelRect = subControlRect(CC_GroupBox, opt, QStyle::SC_GroupBoxLabel, widget);

    labelRect.adjust(Constants::ButtonMargin + Constants::GroupMargin, Constants::ButtonBorder,
                     -Constants::ButtonMargin, 0);

    QColor textColor = group->textColor;
    QPalette::ColorRole penRole = QPalette::WindowText;

    if(textColor.isValid())
    {
      p->setPen(textColor);
      penRole = QPalette::NoRole;
    }

    drawItemText(p, labelRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextHideMnemonic, group->palette,
                 group->state & State_Enabled, group->text, penRole);

    labelRect.adjust(-Constants::GroupMargin, 0, 0, Constants::GroupMargin / 2);

    p->setPen(QPen(opt->palette.brush(m_Scheme == Light ? QPalette::Mid : QPalette::Midlight), 1.0));
    p->drawLine(labelRect.bottomLeft(), labelRect.bottomRight());

    return;
  }
  else if(control == QStyle::CC_ScrollBar)
  {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    p->fillRect(opt->rect, opt->palette.brush(QPalette::Window));

    QBrush hoverBrush;
    QBrush sliderBrush;

    if(m_Scheme == Light)
    {
      sliderBrush = opt->palette.brush(QPalette::Dark);
      hoverBrush = opt->palette.brush(QPalette::Midlight);
    }
    else
    {
      sliderBrush = opt->palette.brush(QPalette::Text);
      hoverBrush = opt->palette.brush(QPalette::Light);
    }

    const QStyleOptionSlider *scroll = qstyleoption_cast<const QStyleOptionSlider *>(opt);

    if(scroll)
    {
      const int margin = Constants::ScrollBarMargin;

      {
        p->setPen(QPen(sliderBrush, 2.5));

        QRectF rect = subControlRect(CC_ScrollBar, opt, QStyle::SC_ScrollBarSubLine, widget);

        rect = rect.adjusted(margin, margin, -margin, -margin);

        if(scroll->orientation == Qt::Vertical)
          rect.moveTop(rect.top() + rect.height() / 4);
        else
          rect.moveLeft(rect.left() + rect.width() / 4);

        QPainterPath path;
        QPolygonF poly;

        if(scroll->orientation == Qt::Vertical)
        {
          QPointF pt = rect.center();
          pt.setX(rect.left());
          poly << pt;

          pt = rect.center();
          pt.setY(rect.top());
          poly << pt;

          pt = rect.center();
          pt.setX(rect.right());
          poly << pt;
        }
        else
        {
          QPointF pt = rect.center();
          pt.setY(rect.top());
          poly << pt;

          pt = rect.center();
          pt.setX(rect.left());
          poly << pt;

          pt = rect.center();
          pt.setY(rect.bottom());
          poly << pt;
        }

        path.addPolygon(poly);

        p->drawPath(path);
      }

      {
        p->setPen(QPen(sliderBrush, 2.5));

        QRectF rect = subControlRect(CC_ScrollBar, opt, QStyle::SC_ScrollBarAddLine, widget);

        rect = rect.adjusted(margin, margin, -margin, -margin);

        if(scroll->orientation == Qt::Vertical)
          rect.moveBottom(rect.bottom() - rect.height() / 4);
        else
          rect.moveRight(rect.right() - rect.width() / 4);

        QPainterPath path;
        QPolygonF poly;

        if(scroll->orientation == Qt::Vertical)
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
        }

        path.addPolygon(poly);

        p->drawPath(path);
      }
    }

    int activeHover = State_MouseOver | State_Active | State_Enabled;
    if((opt->state & activeHover) == activeHover)
    {
      QRect hoverRect =
          subControlRect(CC_ScrollBar, opt, QStyle::SC_ScrollBarAddPage, widget)
              .united(subControlRect(CC_ScrollBar, opt, QStyle::SC_ScrollBarSubPage, widget));

      QPainterPath path;
      path.addRoundedRect(hoverRect, Constants::ScrollBarRadius, Constants::ScrollBarRadius);

      p->fillPath(path, hoverBrush);
    }

    QRect slider = subControlRect(CC_ScrollBar, opt, QStyle::SC_ScrollBarSlider, widget);

    if(slider.isValid() && (opt->state & State_Enabled))
    {
      QPainterPath path;
      path.addRoundedRect(slider, Constants::ScrollBarRadius, Constants::ScrollBarRadius);

      if(opt->state & State_Sunken)
        p->fillPath(path, opt->palette.brush(QPalette::Highlight));
      else
        p->fillPath(path, sliderBrush);
    }

    p->restore();

    return;
  }
  else if(control == QStyle::CC_ComboBox)
  {
    drawRoundedRectBorder(opt, p, widget, QPalette::Base, false);

    QRectF rect = subControlRect(control, opt, QStyle::SC_ComboBoxArrow, widget);

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    rect.setTop(rect.top() + rect.height() / 2.0 - rect.width() / 2.0);
    rect.setHeight(rect.width());

    {
      qreal penWidth = 1.5;
      p->setPen(QPen(outlineBrush(opt->palette), penWidth));

      QPainterPath path;
      QPolygonF poly;

      QPointF pt = rect.center();
      pt.setX(rect.left() + penWidth);
      poly << pt;

      pt = rect.center();
      pt.setY(rect.bottom() - penWidth);
      poly << pt;

      pt = rect.center();
      pt.setX(rect.right() - penWidth);
      poly << pt;

      path.addPolygon(poly);

      p->drawPath(path);
    }

    p->restore();

    return;
  }
  else if(control == QStyle::CC_SpinBox)
  {
    {
      QStyleOption o = *opt;
      o.state &= ~State_Sunken;
      drawRoundedRectBorder(&o, p, widget, QPalette::Base, false);
    }

    QRect rect = opt->rect;
    rect.adjust(Constants::ButtonBorder, Constants::ButtonBorder, -Constants::ButtonBorder,
                -Constants::ButtonBorder);

    rect.adjust(0, 0, -Constants::SpinButtonDim, 0);

    p->save();

    p->setPen(QPen(outlineBrush(opt->palette), 1.0));

    p->drawLine(rect.topRight(), rect.bottomRight());

    rect = subControlRect(control, opt, QStyle::SC_SpinBoxUp, widget);

    p->setClipRect(rect);

    const QStyleOptionSpinBox *spinbox = qstyleoption_cast<const QStyleOptionSpinBox *>(opt);

    {
      QPainterPath path;
      path.addRoundedRect(rect, 1.0, 1.0);

      if((opt->state & State_Sunken) && (spinbox->activeSubControls & QStyle::SC_SpinBoxUp))
        p->fillPath(path, opt->palette.brush(QPalette::Midlight));
      else
        p->fillPath(path, opt->palette.brush(QPalette::Button));
    }

    p->drawLine(rect.bottomLeft(), rect.bottomRight());

    p->setRenderHint(QPainter::Antialiasing);

    QPalette::ColorGroup group = QPalette::Disabled;
    if(spinbox->stepEnabled & QAbstractSpinBox::StepUpEnabled)
      group = QPalette::Normal;

    qreal penWidth = 1.5;
    p->setPen(QPen(opt->palette.brush(group, QPalette::WindowText), penWidth));

    {
      QRectF arrowRect = QRectF(rect);
      arrowRect.adjust(0.5, 0.5, -0.5, 0.5);

      QPainterPath path;
      QPolygonF poly;

      QPointF pt = arrowRect.center();
      pt.setX(arrowRect.left() + penWidth);
      poly << pt;

      pt = arrowRect.center();
      pt.setY(arrowRect.top() + penWidth);
      poly << pt;

      pt = arrowRect.center();
      pt.setX(arrowRect.right() - penWidth);
      poly << pt;

      path.addPolygon(poly);

      p->drawPath(path);
    }

    rect = subControlRect(control, opt, QStyle::SC_SpinBoxDown, widget);

    p->setClipRect(rect);

    {
      QPainterPath path;
      path.addRoundedRect(rect, 1.0, 1.0);

      if((opt->state & State_Sunken) && (spinbox->activeSubControls & QStyle::SC_SpinBoxDown))
        p->fillPath(path, opt->palette.brush(QPalette::Midlight));
      else
        p->fillPath(path, opt->palette.brush(QPalette::Button));
    }

    group = QPalette::Disabled;
    if(spinbox->stepEnabled & QAbstractSpinBox::StepDownEnabled)
      group = QPalette::Normal;

    p->setPen(QPen(opt->palette.brush(group, QPalette::WindowText), penWidth));

    {
      QRectF arrowRect = QRectF(rect);
      arrowRect.adjust(0.5, -0.5, -0.5, -0.5);

      QPainterPath path;
      QPolygonF poly;

      QPointF pt = arrowRect.center();
      pt.setX(arrowRect.left() + penWidth);
      poly << pt;

      pt = arrowRect.center();
      pt.setY(arrowRect.bottom() - penWidth);
      poly << pt;

      pt = arrowRect.center();
      pt.setX(arrowRect.right() - penWidth);
      poly << pt;

      path.addPolygon(poly);

      p->drawPath(path);
    }

    p->restore();

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
  else if(element == QStyle::PE_Frame)
  {
    const QStyleOptionFrame *frame = qstyleoption_cast<const QStyleOptionFrame *>(opt);

    QStyleOptionFrame frameOpt = *frame;
    frameOpt.frameShape = QFrame::Panel;
    drawControl(CE_ShapedFrame, &frameOpt, p, widget);
    return;
  }
  else if(element == QStyle::PE_FrameFocusRect)
  {
    // don't draw focus rects
    return;
  }
  else if(element == QStyle::PE_PanelTipLabel)
  {
    QPen oldPen = p->pen();

    p->fillRect(opt->rect, opt->palette.brush(QPalette::ToolTipBase));

    p->setPen(QPen(outlineBrush(opt->palette), 0));
    p->drawRect(opt->rect.adjusted(0, 0, -1, -1));

    p->setPen(oldPen);

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
  else if(control == CE_CheckBoxLabel || control == QStyle::CE_RadioButtonLabel)
  {
    const QStyleOptionButton *checkbox = qstyleoption_cast<const QStyleOptionButton *>(opt);
    if(checkbox)
    {
      QRect rect = checkbox->rect;

      if(!checkbox->icon.isNull())
      {
        drawItemPixmap(p, rect, Qt::AlignLeft | Qt::AlignVCenter,
                       checkbox->icon.pixmap(
                           checkbox->iconSize.width(), checkbox->iconSize.height(),
                           checkbox->state & State_Enabled ? QIcon::Normal : QIcon::Disabled));

        rect.setLeft(rect.left() + checkbox->iconSize.width() + Constants::CheckMargin);
      }

      if(!checkbox->text.isEmpty())
      {
        drawItemText(p, rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextHideMnemonic,
                     checkbox->palette, checkbox->state & State_Enabled, checkbox->text,
                     QPalette::WindowText);
      }
    }

    return;
  }
  else if(control == CE_SizeGrip)
  {
    // don't draw size grips
    return;
  }
  else if(control == CE_ShapedFrame)
  {
    const QStyleOptionFrame *frame = qstyleoption_cast<const QStyleOptionFrame *>(opt);

    p->save();
    p->setPen(QPen(outlineBrush(opt->palette), 1.0));

    QRectF rect = QRectF(opt->rect).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath path;
    path.addRoundedRect(rect, 1.0, 1.0);

    if(frame->frameShape == QFrame::NoFrame)
    {
      // draw nothing
    }
    else if(frame->frameShape == QFrame::Box)
    {
      p->drawRect(rect);
    }
    else if(frame->frameShape == QFrame::Panel || frame->frameShape == QFrame::WinPanel ||
            frame->frameShape == QFrame::StyledPanel)
    {
      p->setRenderHint(QPainter::Antialiasing);
      p->drawPath(path);
    }
    else if(frame->frameShape == QFrame::HLine)
    {
      rect.adjust(Constants::SeparatorMargin, 0, -Constants::SeparatorMargin, 0);
      QPoint offs(0, opt->rect.height() / 2);
      p->drawLine(opt->rect.topLeft() + offs, opt->rect.topRight() + offs);
    }
    else if(frame->frameShape == QFrame::VLine)
    {
      rect.adjust(0, Constants::SeparatorMargin, 0, -Constants::SeparatorMargin);
      QPoint offs(opt->rect.width() / 2, 0);
      p->drawLine(opt->rect.topLeft() + offs, opt->rect.bottomLeft() + offs);
    }

    p->restore();

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
