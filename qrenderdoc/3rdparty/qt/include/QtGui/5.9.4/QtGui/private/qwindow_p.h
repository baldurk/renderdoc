/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QWINDOW_P_H
#define QWINDOW_P_H

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

#include <QtGui/private/qtguiglobal_p.h>
#include <QtGui/qscreen.h>
#include <QtGui/qwindow.h>
#include <qpa/qplatformwindow.h>

#include <QtCore/private/qobject_p.h>
#include <QtCore/qelapsedtimer.h>
#include <QtGui/QIcon>

QT_BEGIN_NAMESPACE

#define QWINDOWSIZE_MAX ((1<<24)-1)

class Q_GUI_EXPORT QWindowPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QWindow)

public:
    enum PositionPolicy
    {
        WindowFrameInclusive,
        WindowFrameExclusive
    };

    QWindowPrivate()
        : QObjectPrivate()
        , surfaceType(QWindow::RasterSurface)
        , windowFlags(Qt::Window)
        , parentWindow(0)
        , platformWindow(0)
        , visible(false)
        , visibilityOnDestroy(false)
        , exposed(false)
        , windowState(Qt::WindowNoState)
        , visibility(QWindow::Hidden)
        , resizeEventPending(true)
        , receivedExpose(false)
        , positionPolicy(WindowFrameExclusive)
        , positionAutomatic(true)
        , contentOrientation(Qt::PrimaryOrientation)
        , opacity(qreal(1.0))
        , minimumSize(0, 0)
        , maximumSize(QWINDOWSIZE_MAX, QWINDOWSIZE_MAX)
        , modality(Qt::NonModal)
        , blockedByModalWindow(false)
        , updateRequestPending(false)
        , updateTimer(0)
        , transientParent(0)
        , topLevelScreen(0)
#ifndef QT_NO_CURSOR
        , cursor(Qt::ArrowCursor)
        , hasCursor(false)
#endif
        , compositing(false)
    {
        isWindow = true;
    }

    ~QWindowPrivate()
    {
    }

    void init(QScreen *targetScreen = nullptr);

    void maybeQuitOnLastWindowClosed();
#ifndef QT_NO_CURSOR
    void setCursor(const QCursor *c = 0);
    bool applyCursor();
#endif

    void deliverUpdateRequest();

    QPoint globalPosition() const;

    QWindow *topLevelWindow() const;

    virtual QWindow *eventReceiver() { Q_Q(QWindow); return q; }

    void updateVisibility();
    void _q_clearAlert();

    enum SiblingPosition { PositionTop, PositionBottom };
    void updateSiblingPosition(SiblingPosition);

    bool windowRecreationRequired(QScreen *newScreen) const;
    void create(bool recursive, WId nativeHandle = 0);
    void destroy();
    void setTopLevelScreen(QScreen *newScreen, bool recreate);
    void connectToScreen(QScreen *topLevelScreen);
    void disconnectFromScreen();
    void emitScreenChangedRecursion(QScreen *newScreen);
    QScreen *screenForGeometry(const QRect &rect);

    virtual void clearFocusObject();
    virtual QRectF closestAcceptableGeometry(const QRectF &rect) const;

    virtual void processSafeAreaMarginsChanged() {};

    bool isPopup() const { return (windowFlags & Qt::WindowType_Mask) == Qt::Popup; }

    static QWindowPrivate *get(QWindow *window) { return window->d_func(); }

    QWindow::SurfaceType surfaceType;
    Qt::WindowFlags windowFlags;
    QWindow *parentWindow;
    QPlatformWindow *platformWindow;
    bool visible;
    bool visibilityOnDestroy;
    bool exposed;
    QSurfaceFormat requestedFormat;
    QString windowTitle;
    QString windowFilePath;
    QIcon windowIcon;
    QRect geometry;
    Qt::WindowState windowState;
    QWindow::Visibility visibility;
    bool resizeEventPending;
    bool receivedExpose;
    PositionPolicy positionPolicy;
    bool positionAutomatic;
    Qt::ScreenOrientation contentOrientation;
    qreal opacity;
    QRegion mask;

    QSize minimumSize;
    QSize maximumSize;
    QSize baseSize;
    QSize sizeIncrement;

    Qt::WindowModality modality;
    bool blockedByModalWindow;

    bool updateRequestPending;
    int updateTimer;

    QPointer<QWindow> transientParent;
    QPointer<QScreen> topLevelScreen;

#ifndef QT_NO_CURSOR
    QCursor cursor;
    bool hasCursor;
#endif

    bool compositing;
    QElapsedTimer lastComposeTime;
};


QT_END_NAMESPACE

#endif // QWINDOW_P_H
