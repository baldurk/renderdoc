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

#ifndef QMDISUBWINDOW_H
#define QMDISUBWINDOW_H

#include <QtWidgets/qtwidgetsglobal.h>
#include <QtWidgets/qwidget.h>

QT_REQUIRE_CONFIG(mdiarea);

QT_BEGIN_NAMESPACE

class QMenu;
class QMdiArea;

namespace QMdi { class ControlContainer; }
class QMdiSubWindowPrivate;
class Q_WIDGETS_EXPORT QMdiSubWindow : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int keyboardSingleStep READ keyboardSingleStep WRITE setKeyboardSingleStep)
    Q_PROPERTY(int keyboardPageStep READ keyboardPageStep WRITE setKeyboardPageStep)
public:
    enum SubWindowOption {
        AllowOutsideAreaHorizontally = 0x1, // internal
        AllowOutsideAreaVertically = 0x2, // internal
        RubberBandResize = 0x4,
        RubberBandMove = 0x8
    };
    Q_DECLARE_FLAGS(SubWindowOptions, SubWindowOption)

    QMdiSubWindow(QWidget *parent = Q_NULLPTR, Qt::WindowFlags flags = Qt::WindowFlags());
    ~QMdiSubWindow();

    QSize sizeHint() const Q_DECL_OVERRIDE;
    QSize minimumSizeHint() const Q_DECL_OVERRIDE;

    void setWidget(QWidget *widget);
    QWidget *widget() const;

    QWidget *maximizedButtonsWidget() const; // internal
    QWidget *maximizedSystemMenuIconWidget() const; // internal

    bool isShaded() const;

    void setOption(SubWindowOption option, bool on = true);
    bool testOption(SubWindowOption) const;

    void setKeyboardSingleStep(int step);
    int keyboardSingleStep() const;

    void setKeyboardPageStep(int step);
    int keyboardPageStep() const;

#if QT_CONFIG(menu)
    void setSystemMenu(QMenu *systemMenu);
    QMenu *systemMenu() const;
#endif

    QMdiArea *mdiArea() const;

Q_SIGNALS:
    void windowStateChanged(Qt::WindowStates oldState, Qt::WindowStates newState);
    void aboutToActivate();

public Q_SLOTS:
#if QT_CONFIG(menu)
    void showSystemMenu();
#endif
    void showShaded();

protected:
    bool eventFilter(QObject *object, QEvent *event) Q_DECL_OVERRIDE;
    bool event(QEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *showEvent) Q_DECL_OVERRIDE;
    void hideEvent(QHideEvent *hideEvent) Q_DECL_OVERRIDE;
    void changeEvent(QEvent *changeEvent) Q_DECL_OVERRIDE;
    void closeEvent(QCloseEvent *closeEvent) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *leaveEvent) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *resizeEvent) Q_DECL_OVERRIDE;
    void timerEvent(QTimerEvent *timerEvent) Q_DECL_OVERRIDE;
    void moveEvent(QMoveEvent *moveEvent) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *paintEvent) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *mouseEvent) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *mouseEvent) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *mouseEvent) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *mouseEvent) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *keyEvent) Q_DECL_OVERRIDE;
#ifndef QT_NO_CONTEXTMENU
    void contextMenuEvent(QContextMenuEvent *contextMenuEvent) Q_DECL_OVERRIDE;
#endif
    void focusInEvent(QFocusEvent *focusInEvent) Q_DECL_OVERRIDE;
    void focusOutEvent(QFocusEvent *focusOutEvent) Q_DECL_OVERRIDE;
    void childEvent(QChildEvent *childEvent) Q_DECL_OVERRIDE;

private:
    Q_DISABLE_COPY(QMdiSubWindow)
    Q_DECLARE_PRIVATE(QMdiSubWindow)
    Q_PRIVATE_SLOT(d_func(), void _q_updateStaysOnTopHint())
    Q_PRIVATE_SLOT(d_func(), void _q_enterInteractiveMode())
    Q_PRIVATE_SLOT(d_func(), void _q_processFocusChanged(QWidget *, QWidget *))
    friend class QMdiAreaPrivate;
#if QT_CONFIG(tabbar)
    friend class QMdiAreaTabBar;
#endif
    friend class QMdi::ControlContainer;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QMdiSubWindow::SubWindowOptions)

QT_END_NAMESPACE

#endif // QMDISUBWINDOW_H
