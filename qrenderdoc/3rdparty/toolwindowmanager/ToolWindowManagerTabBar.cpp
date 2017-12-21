/*
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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include "ToolWindowManager.h"
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <QStylePainter>
#include "ToolWindowManagerArea.h"
#include "ToolWindowManagerTabBar.h"
#include "ToolWindowManagerWrapper.h"

ToolWindowManagerTabBar::ToolWindowManagerTabBar(QWidget *parent) : QTabBar(parent)
{
  m_tabsClosable = false;

  setMouseTracking(true);

  m_area = qobject_cast<ToolWindowManagerArea *>(parent);

  // Workaround for extremely dodgy KDE behaviour - by default the KDE theme will install event
  // filters on various widgets such as QTabBar and any descendents, and if a click is detected on
  // them that isn't on a tab it will immediately start moving the window, interfering with our own
  // click-to-drag behaviour.
  setProperty("_kde_no_window_grab", true);

  QStyleOptionToolButton buttonOpt;

  int size = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);

  buttonOpt.initFrom(parentWidget());
  buttonOpt.iconSize = QSize(size, size);
  buttonOpt.subControls = 0;
  buttonOpt.activeSubControls = 0;
  buttonOpt.features = QStyleOptionToolButton::None;
  buttonOpt.arrowType = Qt::NoArrow;
  buttonOpt.state |= QStyle::State_AutoRaise;

  // TODO make our own pin icon, that is pinned/unpinned
  m_pin.icon = style()->standardIcon(QStyle::SP_TitleBarNormalButton, &buttonOpt, this);
  m_close.icon = style()->standardIcon(QStyle::SP_TitleBarCloseButton, &buttonOpt, this);

  m_pin.hover = m_pin.clicked = false;
  m_close.hover = m_close.clicked = false;
}

ToolWindowManagerTabBar::~ToolWindowManagerTabBar()
{
}

bool ToolWindowManagerTabBar::useMinimalBar() const
{
  if(count() > 1)
    return false;

  if(m_area)
  {
    return m_area->useMinimalTabBar();
  }
  return true;
}

QSize ToolWindowManagerTabBar::sizeHint() const
{
  if(useMinimalBar())
  {
    if(floatingWindowChild())
      return QSize(0, 0);

    QFontMetrics fm = fontMetrics();

    int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);
    int mw = style()->pixelMetric(QStyle::PM_DockWidgetTitleMargin, 0, this);

    int h = qMax(fm.height(), iconSize) + 2 * mw;

    return QSize(m_area->width(), h);
  }

  return QTabBar::sizeHint();
}

QSize ToolWindowManagerTabBar::minimumSizeHint() const
{
  if(useMinimalBar())
  {
    if(floatingWindowChild())
      return QSize(0, 0);

    QFontMetrics fm = fontMetrics();

    int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);
    int mw = style()->pixelMetric(QStyle::PM_DockWidgetTitleMargin, 0, this);

    int h = qMax(fm.height(), iconSize) + 2 * mw;

    return QSize(h, h);
  }

  return QTabBar::minimumSizeHint();
}

bool ToolWindowManagerTabBar::inButton(QPoint pos)
{
  return m_pin.rect.contains(pos) || m_close.rect.contains(pos);
}

void ToolWindowManagerTabBar::paintEvent(QPaintEvent *event)
{
  if(useMinimalBar())
  {
    if(floatingWindowChild())
      return;

    QStylePainter p(this);

    QStyleOptionDockWidget option;

    option.initFrom(parentWidget());
    option.rect = m_titleRect;
    option.title = tabText(0);
    option.closable = m_tabsClosable;
    option.movable = false;
    // we only set floatable true so we can hijack the float button for our own pin/auto-hide button
    option.floatable = true;

    Shape s = shape();
    option.verticalTitleBar =
        s == RoundedEast || s == TriangularEast || s == RoundedWest || s == TriangularWest;

    p.drawControl(QStyle::CE_DockWidgetTitle, option);

    int size = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);

    QStyleOptionToolButton buttonOpt;

    buttonOpt.initFrom(parentWidget());
    buttonOpt.iconSize = QSize(size, size);
    buttonOpt.subControls = 0;
    buttonOpt.activeSubControls = 0;
    buttonOpt.features = QStyleOptionToolButton::None;
    buttonOpt.arrowType = Qt::NoArrow;
    buttonOpt.state = QStyle::State_Active | QStyle::State_Enabled | QStyle::State_AutoRaise;

    buttonOpt.rect = m_pin.rect;
    buttonOpt.icon = m_pin.icon;

    ToolWindowManager::ToolWindowProperty props =
        m_area->m_manager->toolWindowProperties(m_area->widget(0));

    bool tabClosable = (props & ToolWindowManager::HideCloseButton) == 0;

    if(!tabClosable && !m_pin.rect.isEmpty())
      buttonOpt.rect = m_close.rect;

    QStyle::State prevState = buttonOpt.state;

    if(m_pin.clicked)
      buttonOpt.state |= QStyle::State_Sunken;
    else if(m_pin.hover)
      buttonOpt.state |= QStyle::State_Raised | QStyle::State_MouseOver;

    if(style()->styleHint(QStyle::SH_DockWidget_ButtonsHaveFrame, 0, this))
    {
      style()->drawPrimitive(QStyle::PE_PanelButtonTool, &buttonOpt, &p, this);
    }

    style()->drawComplexControl(QStyle::CC_ToolButton, &buttonOpt, &p, this);

    if(m_tabsClosable && tabClosable)
    {
      buttonOpt.rect = m_close.rect;
      buttonOpt.icon = m_close.icon;

      buttonOpt.state = prevState;

      if(m_close.clicked)
        buttonOpt.state |= QStyle::State_Sunken;
      else if(m_close.hover)
        buttonOpt.state |= QStyle::State_Raised | QStyle::State_MouseOver;

      style()->drawPrimitive(QStyle::PE_IndicatorTabClose, &buttonOpt, &p, this);
    }
    return;
  }

  QTabBar::paintEvent(event);
}

void ToolWindowManagerTabBar::resizeEvent(QResizeEvent *event)
{
  QTabBar::resizeEvent(event);

  if(count() > 1 || floatingWindowChild())
    return;

  m_titleRect = QRect(0, 0, size().width(), sizeHint().height());

  QStyleOptionDockWidget option;

  option.initFrom(parentWidget());
  option.rect = m_titleRect;
  option.closable = m_tabsClosable;
  option.movable = false;
  // we only set floatable true so we can hijack the float button for our own pin/auto-hide button
  option.floatable = true;

  m_pin.rect = style()->subElementRect(QStyle::SE_DockWidgetFloatButton, &option, this);
  m_close.rect = style()->subElementRect(QStyle::SE_DockWidgetCloseButton, &option, this);

  // TODO - temporarily until this is implemented, hide the pin button.
  m_pin.rect = QRect();
}

void ToolWindowManagerTabBar::mousePressEvent(QMouseEvent *event)
{
  QTabBar::mousePressEvent(event);

  if(count() > 1 || floatingWindowChild())
    return;

  ButtonData prevPin = m_pin;
  ButtonData prevClose = m_close;

  ToolWindowManager::ToolWindowProperty props =
      m_area->m_manager->toolWindowProperties(m_area->widget(0));

  bool tabClosable = (props & ToolWindowManager::HideCloseButton) == 0;

  QRect pinRect = m_pin.rect;
  QRect closeRect = m_close.rect;

  if(!tabClosable)
  {
    if(!pinRect.isEmpty())
      pinRect = closeRect;
    closeRect = QRect();
  }

  if(pinRect.contains(mapFromGlobal(QCursor::pos())) && event->buttons() & Qt::LeftButton)
  {
    m_pin.clicked = true;
  }
  else
  {
    m_pin.clicked = false;
  }

  if(closeRect.contains(mapFromGlobal(QCursor::pos())) && event->buttons() & Qt::LeftButton)
  {
    m_close.clicked = true;
  }
  else
  {
    m_close.clicked = false;
  }

  if(prevPin != m_pin || prevClose != m_close)
    update();

  event->accept();
}

void ToolWindowManagerTabBar::mouseMoveEvent(QMouseEvent *event)
{
  QTabBar::mouseMoveEvent(event);

  if(count() > 1 || floatingWindowChild())
    return;

  ButtonData prevPin = m_pin;
  ButtonData prevClose = m_close;

  ToolWindowManager::ToolWindowProperty props =
      m_area->m_manager->toolWindowProperties(m_area->widget(0));

  bool tabClosable = (props & ToolWindowManager::HideCloseButton) == 0;

  QRect pinRect = m_pin.rect;
  QRect closeRect = m_close.rect;

  if(!tabClosable)
  {
    if(!pinRect.isEmpty())
      pinRect = closeRect;
    closeRect = QRect();
  }

  if(pinRect.contains(mapFromGlobal(QCursor::pos())))
  {
    m_pin.hover = true;
    if(event->buttons() & Qt::LeftButton)
      m_pin.clicked = true;
  }
  else
  {
    m_pin.hover = false;
    m_pin.clicked = false;
  }

  if(closeRect.contains(mapFromGlobal(QCursor::pos())))
  {
    m_close.hover = true;
    if(event->buttons() & Qt::LeftButton)
      m_close.clicked = true;
  }
  else
  {
    m_close.hover = false;
    m_close.clicked = false;
  }

  if(prevPin != m_pin || prevClose != m_close)
    update();
}

void ToolWindowManagerTabBar::leaveEvent(QEvent *)
{
  m_pin.hover = false;
  m_pin.clicked = false;

  m_close.hover = false;
  m_close.clicked = false;

  update();
}

void ToolWindowManagerTabBar::mouseReleaseEvent(QMouseEvent *event)
{
  QTabBar::mouseReleaseEvent(event);

  if(count() > 1 || floatingWindowChild())
    return;

  ToolWindowManager::ToolWindowProperty props =
      m_area->m_manager->toolWindowProperties(m_area->widget(0));

  bool tabClosable = (props & ToolWindowManager::HideCloseButton) == 0;

  QRect pinRect = m_pin.rect;
  QRect closeRect = m_close.rect;

  if(!tabClosable)
  {
    if(!pinRect.isEmpty())
      pinRect = closeRect;
    closeRect = QRect();
  }

  if(pinRect.contains(mapFromGlobal(QCursor::pos())))
  {
    // process a pin of these tabs

    m_pin.clicked = false;

    update();

    event->accept();
  }

  if(closeRect.contains(mapFromGlobal(QCursor::pos())))
  {
    if(m_area)
      m_area->tabCloseRequested(0);

    m_close.clicked = false;

    update();

    event->accept();
  }
}

void ToolWindowManagerTabBar::tabInserted(int)
{
  updateClosable();
}

void ToolWindowManagerTabBar::tabRemoved(int)
{
  updateClosable();
}

void ToolWindowManagerTabBar::updateClosable()
{
  QTabBar::setTabsClosable(m_tabsClosable && !useMinimalBar());
}

bool ToolWindowManagerTabBar::floatingWindowChild() const
{
  ToolWindowManagerArea *area = qobject_cast<ToolWindowManagerArea *>(parentWidget());

  if(area)
  {
    ToolWindowManagerWrapper *wrapper =
        qobject_cast<ToolWindowManagerWrapper *>(area->parentWidget());

    if(wrapper && wrapper->floating())
      return true;
  }

  return false;
}
