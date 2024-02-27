/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include "MarkerBreadcrumbs.h"
#include <QAction>
#include <QMenu>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDLabel.h"
#include "Widgets/Extended/RDToolButton.h"

BreadcrumbsLayout::BreadcrumbsLayout(QWidget *parent, RDToolButton *elidedItems) : QLayout(parent)
{
  m_ElidedItems = elidedItems;
}

BreadcrumbsLayout::~BreadcrumbsLayout()
{
}

void BreadcrumbsLayout::clear()
{
  while(!m_Items.isEmpty())
  {
    QLayoutItem *item = m_Items.takeAt(0);
    delete item->widget();
    delete item;
  }
}

void BreadcrumbsLayout::addItem(QLayoutItem *item)
{
  m_Items.push_back(item);
}

int BreadcrumbsLayout::count() const
{
  return m_Items.count();
}

Qt::Orientations BreadcrumbsLayout::expandingDirections() const
{
  return 0;
}

QSize BreadcrumbsLayout::minimumSize() const
{
  QSize ret(0, 16);
  if(!m_Items.isEmpty())
    ret = m_Items[0]->minimumSize();
  ret.setWidth(100);
  return ret + QSize(contentsMargins().left() + contentsMargins().right(),
                     contentsMargins().top() + contentsMargins().bottom());
}

void BreadcrumbsLayout::setGeometry(const QRect &rect)
{
  bool needUpdate = (rect != m_PrevRect);

  QLayout::setGeometry(rect);

  {
    QRect avail = QRect(rect.topLeft(), rect.marginsRemoved(contentsMargins()).size());

    QList<QLayoutItem *> items = m_Items;

    avail.setWidth(qMin(avail.width(), sizeHint().width()));

    // alternate between taking the last and first, starting with the last
    bool takeLast = true;
    bool elided = false;
    QLayoutItem *prevItem = NULL;
    while(!items.isEmpty() && avail.width() > 0)
    {
      QLayoutItem *item = items.takeAt(takeLast ? items.count() - 1 : 0);

      QSize sz = item->sizeHint();
      QSize s = sz;
      s.setWidth(qMin(s.width(), avail.width()));
      s.setHeight(avail.height());

      // if we're eliding, this is the last item we'll add. Leave space for the elide label
      if(s.width() < sz.width())
        s.setWidth(s.width() - s.height());

      QPoint p;
      if(takeLast)
      {
        p = avail.topRight();
        p.setX(p.x() - s.width());
      }
      else
      {
        p = avail.topLeft();
      }

      QRect itemRect(p, s);

      if((itemRect.width() < 40 && itemRect.width() < sz.width()) ||
         (itemRect.width() < sz.width() / 2 && itemRect.width() < itemRect.height() * 3))
      {
        item->setGeometry(QRect(0, 0, 0, 0));

        elided = true;
        break;
      }

      item->setGeometry(itemRect);

      if(takeLast)
        avail.setRight(itemRect.left());
      else
        avail.setLeft(itemRect.right());

      prevItem = item;

      takeLast = !takeLast;
    }

    if(elided || !items.isEmpty())
    {
      // if there's not enough room for the elide label, steal from the last item
      if(avail.width() < avail.height() && prevItem)
      {
        int neededSpace = avail.height() - avail.width();

        QRect itemRect = prevItem->geometry();

        // if we're now taking last, the previous item was on the first side, so shrink it from
        // the right
        if(takeLast)
        {
          itemRect.adjust(0, 0, -neededSpace, 0);
          avail.setLeft(avail.left() - neededSpace);
        }
        else
        {
          itemRect.adjust(neededSpace, 0, 0, 0);
          avail.setWidth(avail.width() + neededSpace);
        }

        prevItem->setGeometry(itemRect);
      }
    }
    else
    {
      avail.setWidth(0);
      avail.setHeight(0);
    }
    m_ElidedItems->setGeometry(avail);

    for(QLayoutItem *item : items)
      item->setGeometry(QRect(0, 0, 0, 0));
  }

  if(needUpdate)
    update();

  m_PrevRect = rect;
}

QSize BreadcrumbsLayout::sizeHint() const
{
  QSize ret(0, 16);
  for(QLayoutItem *item : m_Items)
  {
    QSize s = item->sizeHint();

    ret.setWidth(ret.width() + s.width());
    ret.setHeight(qMax(ret.height(), s.height()));
  }
  return ret;
}

QLayoutItem *BreadcrumbsLayout::itemAt(int index) const
{
  return index >= 0 && index < m_Items.count() ? m_Items[index] : NULL;
}

QLayoutItem *BreadcrumbsLayout::takeAt(int index)
{
  if(index >= 0 && index < m_Items.count())
    return m_Items.takeAt(index);

  return NULL;
}

MarkerBreadcrumbs::MarkerBreadcrumbs(ICaptureContext &ctx, IEventBrowser *browser, QWidget *parent)
    : QFrame(parent), m_Ctx(ctx), m_Browser(browser)
{
  setFont(Formatter::PreferredFont());

  m_ElidedItems = new RDToolButton(this);
  m_ElidedItems->setAutoRaise(true);
  m_ElidedItems->setText(lit("..."));

  m_Layout = new BreadcrumbsLayout(this, m_ElidedItems);
  m_Layout->setContentsMargins(QMargins(0, 2, 0, 2));
  m_Layout->setMargin(0);
  setLayout(m_Layout);

  m_ElidedMenu = new QMenu(this);

  QObject::connect(m_ElidedItems, &RDToolButton::clicked, this,
                   &MarkerBreadcrumbs::elidedItemsClicked);
}

MarkerBreadcrumbs::~MarkerBreadcrumbs()
{
}

void MarkerBreadcrumbs::OnEventChanged(uint32_t eventId)
{
  const ActionDescription *parent = m_Browser->GetActionForEID(m_Ctx.CurEvent());

  if(parent != NULL && !(parent->flags & ActionFlags::PushMarker))
    parent = parent->parent;

  if(m_CurParent == parent && m_Layout->count() != 0)
    return;

  m_CurParent = parent;
  m_Layout->clear();

  QVector<const ActionDescription *> path;

  while(parent)
  {
    path.push_back(parent);
    parent = parent->parent;
  }

  AddPathButton(NULL);

  m_Path.clear();
  for(int i = path.count() - 1; i >= 0; i--)
  {
    m_Path.push_back(path[i]);
    AddPathButton(path[i]);
  }
}

void MarkerBreadcrumbs::ForceRefresh()
{
  m_CurParent = NULL;
  m_Layout->clear();

  OnEventChanged(m_Ctx.CurEvent());
}

void MarkerBreadcrumbs::ConfigurePathMenu(QMenu *menu, const ActionDescription *action)
{
  const rdcarray<ActionDescription> &actions = action ? action->children : m_Ctx.CurRootActions();

  menu->clear();
  for(const ActionDescription &child : actions)
  {
    if((child.flags & ActionFlags::PushMarker) && m_Browser->IsAPIEventVisible(child.eventId))
    {
      QAction *menuAction = new QAction(child.customName, menu);

      uint32_t eid = child.eventId;

      if(child.IsFakeMarker())
        eid = child.children[0].eventId;

      QObject::connect(menuAction, &QAction::triggered,
                       [this, eid]() { m_Ctx.SetEventID({}, eid, eid); });

      menu->addAction(menuAction);
    }
  }
}

void MarkerBreadcrumbs::elidedItemsClicked()
{
  m_ElidedMenu->clear();

  for(int i = 0; i < m_Layout->count(); i++)
  {
    RDToolButton *button = qobject_cast<RDToolButton *>(m_Layout->itemAt(i)->widget());

    if(button && button->size().width() == 0)
    {
      QAction *action = new QAction(button->text(), m_ElidedMenu);

      QObject::connect(action, &QAction::triggered, [button]() { button->click(); });

      m_ElidedMenu->addAction(action);
    }
  }

  m_ElidedMenu->move(mapToGlobal(m_ElidedItems->geometry().bottomLeft()));
  m_ElidedMenu->show();
}

void MarkerBreadcrumbs::AddPathButton(const ActionDescription *action)
{
  RDToolButton *b = new RDToolButton();
  b->setText(action ? QString(action->customName) : QString());
  b->setToolTip(b->text());
  if(!action)
  {
    b->setIcon(Icons::house());
    b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    b->setToolTip(tr("Capture Root"));
  }

  bool hasChildMarkers = false;

  for(const ActionDescription &child : action ? action->children : m_Ctx.CurRootActions())
  {
    if((child.flags & ActionFlags::PushMarker) && m_Browser->IsAPIEventVisible(child.eventId))
    {
      hasChildMarkers = true;
      break;
    }
  }

  if(hasChildMarkers)
  {
    QMenu *menu = new QMenu(b);

    b->setPopupMode(QToolButton::MenuButtonPopup);
    b->setMenu(menu);

    QObject::connect(menu, &QMenu::aboutToShow,
                     [this, menu, action]() { ConfigurePathMenu(menu, action); });
  }

  uint32_t eid = action ? action->eventId : 0;

  if(action && action->IsFakeMarker())
    eid = action->children[0].eventId;

  b->setAutoRaise(true);
  QObject::connect(b, &RDToolButton::clicked, [this, eid]() { m_Ctx.SetEventID({}, eid, eid); });
  m_Layout->addWidget(b);
}
