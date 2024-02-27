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

#pragma once

#include <QFrame>
#include <QLayout>

struct ICaptureContext;
struct IEventBrowser;
class RDToolButton;
class RDLabel;

struct ActionDescription;

class QMenu;

class BreadcrumbsLayout : public QLayout
{
public:
  explicit BreadcrumbsLayout(QWidget *parent, RDToolButton *elidedItems);
  ~BreadcrumbsLayout();

  void clear();
  void addItem(QLayoutItem *item) override;
  int count() const override;
  QLayoutItem *itemAt(int index) const override;
  QLayoutItem *takeAt(int index) override;

  Qt::Orientations expandingDirections() const override;
  QSize minimumSize() const override;
  void setGeometry(const QRect &rect) override;
  QSize sizeHint() const override;

private:
  QList<QLayoutItem *> m_Items;
  QRect m_PrevRect;
  RDToolButton *m_ElidedItems;
};

class MarkerBreadcrumbs : public QFrame
{
  Q_OBJECT

public:
  explicit MarkerBreadcrumbs(ICaptureContext &ctx, IEventBrowser *browser, QWidget *parent = 0);
  ~MarkerBreadcrumbs();

  // when a new event is selected
  void OnEventChanged(uint32_t eventId);
  // forcibly refresh even if the event hasn't changed
  void ForceRefresh();

  QVector<const ActionDescription *> getPath() { return m_Path; }
private slots:
  // manual slots
  void elidedItemsClicked();

private:
  void AddPathButton(const ActionDescription *);
  void ConfigurePathMenu(QMenu *, const ActionDescription *);

  ICaptureContext &m_Ctx;
  IEventBrowser *m_Browser;

  QVector<const ActionDescription *> m_Path;

  const ActionDescription *m_CurParent = NULL;

  BreadcrumbsLayout *m_Layout;
  RDToolButton *m_ElidedItems;
  QMenu *m_ElidedMenu;
};
