/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QTABBAR_P_H
#define QTABBAR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include "qtabbar.h"
#include "private/qwidget_p.h"

#include <qicon.h>
#include <qtoolbutton.h>
#include <qdebug.h>
#include <qvariantanimation.h>

#define ANIMATION_DURATION 250

#include <qstyleoption.h>

QT_REQUIRE_CONFIG(tabbar);

QT_BEGIN_NAMESPACE

class QMovableTabWidget : public QWidget
{
public:
    explicit QMovableTabWidget(QWidget *parent = Q_NULLPTR);
    void setPixmap(const QPixmap &pixmap);

protected:
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;

private:
    QPixmap m_pixmap;
};

class QTabBarPrivate  : public QWidgetPrivate
{
    Q_DECLARE_PUBLIC(QTabBar)
public:
    QTabBarPrivate()
        :currentIndex(-1), pressedIndex(-1), shape(QTabBar::RoundedNorth), layoutDirty(false),
        drawBase(true), scrollOffset(0), hoverIndex(-1), elideModeSetByUser(false), useScrollButtonsSetByUser(false), expanding(true), closeButtonOnTabs(false),
        selectionBehaviorOnRemove(QTabBar::SelectRightTab), paintWithOffsets(true), movable(false),
        dragInProgress(false), documentMode(false), autoHide(false), changeCurrentOnDrag(false),
        switchTabCurrentIndex(-1), switchTabTimerId(0), movingTab(0)
#if 0 // Used to be included in Qt4 for Q_WS_MAC
        , previousPressedIndex(-1)
#endif
        {}

    int currentIndex;
    int pressedIndex;
    QTabBar::Shape shape;
    bool layoutDirty;
    bool drawBase;
    int scrollOffset;

    struct Tab {
        inline Tab(const QIcon &ico, const QString &txt)
            : enabled(true) , shortcutId(0), text(txt), icon(ico),
            leftWidget(0), rightWidget(0), lastTab(-1), dragOffset(0)
#ifndef QT_NO_ANIMATION
            , animation(0)
#endif //QT_NO_ANIMATION
        {}
        bool operator==(const Tab &other) const { return &other == this; }
        bool enabled;
        int shortcutId;
        QString text;
#ifndef QT_NO_TOOLTIP
        QString toolTip;
#endif
#if QT_CONFIG(whatsthis)
        QString whatsThis;
#endif
        QIcon icon;
        QRect rect;
        QRect minRect;
        QRect maxRect;

        QColor textColor;
        QVariant data;
        QWidget *leftWidget;
        QWidget *rightWidget;
        int lastTab;
        int dragOffset;
#ifndef QT_NO_ACCESSIBILITY
        QString accessibleName;
#endif

#ifndef QT_NO_ANIMATION
        ~Tab() { delete animation; }
        struct TabBarAnimation : public QVariantAnimation {
            TabBarAnimation(Tab *t, QTabBarPrivate *_priv) : tab(t), priv(_priv)
            { setEasingCurve(QEasingCurve::InOutQuad); }

            void updateCurrentValue(const QVariant &current) Q_DECL_OVERRIDE;

            void updateState(State, State newState) Q_DECL_OVERRIDE;
        private:
            //these are needed for the callbacks
            Tab *tab;
            QTabBarPrivate *priv;
        } *animation;

        void startAnimation(QTabBarPrivate *priv, int duration) {
            if (!priv->isAnimated()) {
                priv->moveTabFinished(priv->tabList.indexOf(*this));
                return;
            }
            if (!animation)
                animation = new TabBarAnimation(this, priv);
            animation->setStartValue(dragOffset);
            animation->setEndValue(0);
            animation->setDuration(duration);
            animation->start();
        }
#else
        void startAnimation(QTabBarPrivate *priv, int duration)
        { Q_UNUSED(duration); priv->moveTabFinished(priv->tabList.indexOf(*this)); }
#endif //QT_NO_ANIMATION
    };
    QList<Tab> tabList;
    mutable QHash<QString, QSize> textSizes;

    int calculateNewPosition(int from, int to, int index) const;
    void slide(int from, int to);
    void init();

    Tab *at(int index);
    const Tab *at(int index) const;

    int indexAtPos(const QPoint &p) const;

    inline bool isAnimated() const { Q_Q(const QTabBar); return q->style()->styleHint(QStyle::SH_Widget_Animate, 0, q); }
    inline bool validIndex(int index) const { return index >= 0 && index < tabList.count(); }
    void setCurrentNextEnabledIndex(int offset);

    QToolButton* rightB; // right or bottom
    QToolButton* leftB; // left or top

    void _q_scrollTabs();
    void _q_closeTab();
    void moveTab(int index, int offset);
    void moveTabFinished(int index);
    QRect hoverRect;
    int hoverIndex;

    void refresh();
    void layoutTabs();
    void layoutWidgets(int start = 0);
    void layoutTab(int index);
    void updateMacBorderMetrics();
    bool isTabInMacUnifiedToolbarArea() const;
    void setupMovableTab();
    void autoHideTabs();
    QRect normalizedScrollRect(int index = -1);
    int hoveredTabIndex() const;

    void initBasicStyleOption(QStyleOptionTab *option, int tabIndex) const;

    void makeVisible(int index);
    QSize iconSize;
    Qt::TextElideMode elideMode;
    bool elideModeSetByUser;
    bool useScrollButtons;
    bool useScrollButtonsSetByUser;

    bool expanding;
    bool closeButtonOnTabs;
    QTabBar::SelectionBehavior selectionBehaviorOnRemove;

    QPoint dragStartPosition;
    bool paintWithOffsets;
    bool movable;
    bool dragInProgress;
    bool documentMode;
    bool autoHide;
    bool changeCurrentOnDrag;

    int switchTabCurrentIndex;
    int switchTabTimerId;

    QMovableTabWidget *movingTab;
#if 0 // Used to be included in Qt4 for Q_WS_MAC
    int previousPressedIndex;
#endif
    // shared by tabwidget and qtabbar
    static void initStyleBaseOption(QStyleOptionTabBarBase *optTabBase, QTabBar *tabbar, QSize size)
    {
        QStyleOptionTab tabOverlap;
        tabOverlap.shape = tabbar->shape();
        int overlap = tabbar->style()->pixelMetric(QStyle::PM_TabBarBaseOverlap, &tabOverlap, tabbar);
        QWidget *theParent = tabbar->parentWidget();
        optTabBase->init(tabbar);
        optTabBase->shape = tabbar->shape();
        optTabBase->documentMode = tabbar->documentMode();
        if (theParent && overlap > 0) {
            QRect rect;
            switch (tabOverlap.shape) {
            case QTabBar::RoundedNorth:
            case QTabBar::TriangularNorth:
                rect.setRect(0, size.height()-overlap, size.width(), overlap);
                break;
            case QTabBar::RoundedSouth:
            case QTabBar::TriangularSouth:
                rect.setRect(0, 0, size.width(), overlap);
                break;
            case QTabBar::RoundedEast:
            case QTabBar::TriangularEast:
                rect.setRect(0, 0, overlap, size.height());
                break;
            case QTabBar::RoundedWest:
            case QTabBar::TriangularWest:
                rect.setRect(size.width() - overlap, 0, overlap, size.height());
                break;
            }
            optTabBase->rect = rect;
        }
    }

    void killSwitchTabTimer();

};

class CloseButton : public QAbstractButton
{
    Q_OBJECT

public:
    explicit CloseButton(QWidget *parent = 0);

    QSize sizeHint() const Q_DECL_OVERRIDE;
    QSize minimumSizeHint() const Q_DECL_OVERRIDE
        { return sizeHint(); }
    void enterEvent(QEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif
