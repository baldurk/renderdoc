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

#ifndef QDYNAMICMAINWINDOWLAYOUT_P_H
#define QDYNAMICMAINWINDOWLAYOUT_P_H

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
#include "qmainwindow.h"

#include "QtWidgets/qlayout.h"
#if QT_CONFIG(tabbar)
#include "QtWidgets/qtabbar.h"
#endif
#include "QtCore/qvector.h"
#include "QtCore/qset.h"
#include "QtCore/qbasictimer.h"
#include "private/qlayoutengine_p.h"
#include "private/qwidgetanimator_p.h"

#if QT_CONFIG(dockwidget)
#include "qdockarealayout_p.h"
#endif
#include "qtoolbararealayout_p.h"

QT_REQUIRE_CONFIG(mainwindow);

QT_BEGIN_NAMESPACE

class QToolBar;
class QRubberBand;

#if QT_CONFIG(dockwidget)
class QDockWidgetGroupWindow : public QWidget
{
    Q_OBJECT
public:
    explicit QDockWidgetGroupWindow(QWidget* parent = 0, Qt::WindowFlags f = 0)
        : QWidget(parent, f) {}
    QDockAreaLayoutInfo *layoutInfo() const;
    QDockWidget *topDockWidget() const;
    void destroyOrHideIfEmpty();
    void adjustFlags();
    bool hasNativeDecos() const;

protected:
    bool event(QEvent *) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;

private:
    QSize m_removedFrameSize;
};

// This item will be used in the layout for the gap item. We cannot use QWidgetItem directly
// because QWidgetItem functions return an empty size for widgets that are are floating.
class QDockWidgetGroupWindowItem : public QWidgetItem
{
public:
    explicit QDockWidgetGroupWindowItem(QDockWidgetGroupWindow *parent) : QWidgetItem(parent) {}
    QSize minimumSize() const Q_DECL_OVERRIDE { return lay()->minimumSize(); }
    QSize maximumSize() const Q_DECL_OVERRIDE { return lay()->maximumSize(); }
    QSize sizeHint() const Q_DECL_OVERRIDE { return lay()->sizeHint(); }

private:
    QLayout *lay() const { return const_cast<QDockWidgetGroupWindowItem *>(this)->widget()->layout(); }
};
#endif

/* This data structure represents the state of all the tool-bars and dock-widgets. It's value based
   so it can be easilly copied into a temporary variable. All operations are performed without moving
   any widgets. Only when we are sure we have the desired state, we call apply(), which moves the
   widgets.
*/

class QMainWindowLayoutState
{
public:
    QRect rect;
    QMainWindow *mainWindow;

    QMainWindowLayoutState(QMainWindow *win);

#ifndef QT_NO_TOOLBAR
    QToolBarAreaLayout toolBarAreaLayout;
#endif

#if QT_CONFIG(dockwidget)
    QDockAreaLayout dockAreaLayout;
#else
    QLayoutItem *centralWidgetItem;
    QRect centralWidgetRect;
#endif

    void apply(bool animated);
    void deleteAllLayoutItems();
    void deleteCentralWidgetItem();

    QSize sizeHint() const;
    QSize minimumSize() const;
    void fitLayout();

    QLayoutItem *itemAt(int index, int *x) const;
    QLayoutItem *takeAt(int index, int *x);
    QList<int> indexOf(QWidget *widget) const;
    QLayoutItem *item(const QList<int> &path);
    QRect itemRect(const QList<int> &path) const;
    QRect gapRect(const QList<int> &path) const; // ### get rid of this, use itemRect() instead

    bool contains(QWidget *widget) const;

    void setCentralWidget(QWidget *widget);
    QWidget *centralWidget() const;

    QList<int> gapIndex(QWidget *widget, const QPoint &pos) const;
    bool insertGap(const QList<int> &path, QLayoutItem *item);
    void remove(const QList<int> &path);
    void remove(QLayoutItem *item);
    void clear();
    bool isValid() const;

    QLayoutItem *plug(const QList<int> &path);
    QLayoutItem *unplug(const QList<int> &path, QMainWindowLayoutState *savedState = 0);

    void saveState(QDataStream &stream) const;
    bool checkFormat(QDataStream &stream);
    bool restoreState(QDataStream &stream, const QMainWindowLayoutState &oldState);
};

class Q_AUTOTEST_EXPORT QMainWindowLayout : public QLayout
{
    Q_OBJECT

public:
    QMainWindowLayoutState layoutState, savedState;

    QMainWindowLayout(QMainWindow *mainwindow, QLayout *parentLayout);
    ~QMainWindowLayout();

    QMainWindow::DockOptions dockOptions;
    void setDockOptions(QMainWindow::DockOptions opts);
    bool usesHIToolBar(QToolBar *toolbar) const;

    void timerEvent(QTimerEvent *e) Q_DECL_OVERRIDE;

    // status bar

    QLayoutItem *statusbar;

#if QT_CONFIG(statusbar)
    QStatusBar *statusBar() const;
    void setStatusBar(QStatusBar *sb);
#endif

    // central widget

    QWidget *centralWidget() const;
    void setCentralWidget(QWidget *cw);

    // toolbars

#ifndef QT_NO_TOOLBAR
    void addToolBarBreak(Qt::ToolBarArea area);
    void insertToolBarBreak(QToolBar *before);
    void removeToolBarBreak(QToolBar *before);

    void addToolBar(Qt::ToolBarArea area, QToolBar *toolbar, bool needAddChildWidget = true);
    void insertToolBar(QToolBar *before, QToolBar *toolbar);
    Qt::ToolBarArea toolBarArea(QToolBar *toolbar) const;
    bool toolBarBreak(QToolBar *toolBar) const;
    void getStyleOptionInfo(QStyleOptionToolBar *option, QToolBar *toolBar) const;
    void removeToolBar(QToolBar *toolbar);
    void toggleToolBarsVisible();
    void moveToolBar(QToolBar *toolbar, int pos);
#endif

    // dock widgets

#if QT_CONFIG(dockwidget)
    void setCorner(Qt::Corner corner, Qt::DockWidgetArea area);
    Qt::DockWidgetArea corner(Qt::Corner corner) const;
    void addDockWidget(Qt::DockWidgetArea area,
                       QDockWidget *dockwidget,
                       Qt::Orientation orientation);
    void splitDockWidget(QDockWidget *after,
                         QDockWidget *dockwidget,
                         Qt::Orientation orientation);
    void tabifyDockWidget(QDockWidget *first, QDockWidget *second);
    Qt::DockWidgetArea dockWidgetArea(QWidget* widget) const;
    void raise(QDockWidget *widget);
    void setVerticalTabsEnabled(bool enabled);
    bool restoreDockWidget(QDockWidget *dockwidget);

#if QT_CONFIG(tabbar)
    QDockAreaLayoutInfo *dockInfo(QWidget *w);
    bool _documentMode;
    bool documentMode() const;
    void setDocumentMode(bool enabled);

    QTabBar *getTabBar();
    QSet<QTabBar*> usedTabBars;
    QList<QTabBar*> unusedTabBars;
    bool verticalTabsEnabled;

    QWidget *getSeparatorWidget();
    QSet<QWidget*> usedSeparatorWidgets;
    QList<QWidget*> unusedSeparatorWidgets;
    int sep; // separator extent

#if QT_CONFIG(tabwidget)
    QTabWidget::TabPosition tabPositions[4];
    QTabWidget::TabShape _tabShape;

    QTabWidget::TabShape tabShape() const;
    void setTabShape(QTabWidget::TabShape tabShape);
    QTabWidget::TabPosition tabPosition(Qt::DockWidgetArea area) const;
    void setTabPosition(Qt::DockWidgetAreas areas, QTabWidget::TabPosition tabPosition);

    QDockWidgetGroupWindow *createTabbedDockWindow();
#endif // QT_CONFIG(tabwidget)
#endif // QT_CONFIG(tabbar)

    // separators

    QList<int> movingSeparator;
    QPoint movingSeparatorOrigin, movingSeparatorPos;
    QBasicTimer separatorMoveTimer;

    bool startSeparatorMove(const QPoint &pos);
    bool separatorMove(const QPoint &pos);
    bool endSeparatorMove(const QPoint &pos);
    void keepSize(QDockWidget *w);
#endif // QT_CONFIG(dockwidget)

    // save/restore

    enum VersionMarkers { // sentinel values used to validate state data
        VersionMarker = 0xff
    };
    void saveState(QDataStream &stream) const;
    bool restoreState(QDataStream &stream);

    // QLayout interface

    void addItem(QLayoutItem *item) Q_DECL_OVERRIDE;
    void setGeometry(const QRect &r) Q_DECL_OVERRIDE;
    QLayoutItem *itemAt(int index) const Q_DECL_OVERRIDE;
    QLayoutItem *takeAt(int index) Q_DECL_OVERRIDE;
    int count() const Q_DECL_OVERRIDE;

    QSize sizeHint() const Q_DECL_OVERRIDE;
    QSize minimumSize() const Q_DECL_OVERRIDE;
    mutable QSize szHint;
    mutable QSize minSize;
    void invalidate() Q_DECL_OVERRIDE;

    // animations

    QWidgetAnimator widgetAnimator;
    QList<int> currentGapPos;
    QRect currentGapRect;
    QWidget *pluggingWidget;
#if QT_CONFIG(rubberband)
    QPointer<QRubberBand> gapIndicator;
#endif
#if QT_CONFIG(dockwidget)
    QPointer<QWidget> currentHoveredFloat; // set when dragging over a floating dock widget
    void setCurrentHoveredFloat(QWidget *w);
#endif

    void hover(QLayoutItem *widgetItem, const QPoint &mousePos);
    bool plug(QLayoutItem *widgetItem);
    QLayoutItem *unplug(QWidget *widget, bool group = false);
    void revert(QLayoutItem *widgetItem);
    void paintDropIndicator(QPainter *p, QWidget *widget, const QRegion &clip);
    void applyState(QMainWindowLayoutState &newState, bool animate = true);
    void restore(bool keepSavedState = false);
    void updateHIToolBarStatus();
    void animationFinished(QWidget *widget);

private Q_SLOTS:
    void updateGapIndicator();
#if QT_CONFIG(dockwidget)
#if QT_CONFIG(tabbar)
    void tabChanged();
    void tabMoved(int from, int to);
#endif
#endif
private:
#if QT_CONFIG(tabbar)
    void updateTabBarShapes();
#endif
#if 0 // Used to be included in Qt4 for Q_WS_MAC
    static OSStatus qtmacToolbarDelegate(EventHandlerCallRef, EventRef , void *);
    static OSStatus qtoolbarInHIToolbarHandler(EventHandlerCallRef inCallRef, EventRef event,
                                               void *data);
    static void qtMacHIToolbarRegisterQToolBarInHIToolborItemClass();
    static HIToolbarItemRef CreateToolbarItemForIdentifier(CFStringRef identifier, CFTypeRef data);
    static HIToolbarItemRef createQToolBarInHIToolbarItem(QToolBar *toolbar,
                                                          QMainWindowLayout *layout);
public:
    struct ToolBarSaveState {
        ToolBarSaveState() : movable(false) { }
        ToolBarSaveState(bool newMovable, const QSize &newMax)
        : movable(newMovable), maximumSize(newMax) { }
        bool movable;
        QSize maximumSize;
    };
    QList<QToolBar *> qtoolbarsInUnifiedToolbarList;
    QList<void *> toolbarItemsCopy;
    QHash<void *, QToolBar *> unifiedToolbarHash;
    QHash<QToolBar *, ToolBarSaveState> toolbarSaveState;
    QHash<QString, QToolBar *> cocoaItemIDToToolbarHash;
    void insertIntoMacToolbar(QToolBar *before, QToolBar *after);
    void removeFromMacToolbar(QToolBar *toolbar);
    void cleanUpMacToolbarItems();
    void fixSizeInUnifiedToolbar(QToolBar *tb) const;
    bool useHIToolBar;
    bool activateUnifiedToolbarAfterFullScreen;
    void syncUnifiedToolbarVisibility();
    bool blockVisiblityCheck;

    QUnifiedToolbarSurface *unifiedSurface;
    void updateUnifiedToolbarOffset();

#endif
};

#if QT_CONFIG(dockwidget) && !defined(QT_NO_DEBUG_STREAM)
class QDebug;
QDebug operator<<(QDebug debug, const QDockAreaLayout &layout);
QDebug operator<<(QDebug debug, const QMainWindowLayout *layout);
#endif

QT_END_NAMESPACE

#endif // QDYNAMICMAINWINDOWLAYOUT_P_H
