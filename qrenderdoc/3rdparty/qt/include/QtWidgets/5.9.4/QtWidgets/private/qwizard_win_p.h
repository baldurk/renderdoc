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

#ifndef QWIZARD_WIN_P_H
#define QWIZARD_WIN_P_H

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

#if QT_CONFIG(style_windowsvista)

#include <qobject.h>
#include <qwidget.h>
#include <qabstractbutton.h>
#include <QtWidgets/private/qwidget_p.h>
#include <QtWidgets/private/qstylehelper_p.h>
#include <qt_windows.h>

QT_REQUIRE_CONFIG(wizard);

QT_BEGIN_NAMESPACE

class QVistaBackButton : public QAbstractButton
{
public:
    QVistaBackButton(QWidget *widget);

    QSize sizeHint() const;
    inline QSize minimumSizeHint() const
    { return sizeHint(); }

    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);
};

class QWizard;

class QVistaHelper : public QObject
{
public:
    QVistaHelper(QWizard *wizard);
    ~QVistaHelper();
    enum TitleBarChangeType { NormalTitleBar, ExtendedTitleBar };
    void updateCustomMargins(bool vistaMargins);
    bool setDWMTitleBar(TitleBarChangeType type);
    void setTitleBarIconAndCaptionVisible(bool visible);
    void mouseEvent(QEvent *event);
    bool handleWinEvent(MSG *message, long *result);
    void resizeEvent(QResizeEvent *event);
    void paintEvent(QPaintEvent *event);
    QVistaBackButton *backButton() const { return backButton_; }
    void disconnectBackButton();
    void hideBackButton() { if (backButton_) backButton_->hide(); }
    QColor basicWindowFrameColor();
    enum VistaState { VistaAero, VistaBasic, Classic, Dirty };
    static VistaState vistaState();
    static int titleBarSize() { return QVistaHelper::titleBarSizeDp() / QVistaHelper::m_devicePixelRatio; }
    static int titleBarSizeDp() { return QVistaHelper::frameSizeDp() + QVistaHelper::captionSizeDp(); }
    static int topPadding() { // padding under text
        return int(QStyleHelper::dpiScaled(
                QSysInfo::WindowsVersion >= QSysInfo::WV_WINDOWS7 ? 4 : 6));
    }
    static int topOffset();

    static HDC backingStoreDC(const QWidget *wizard, QPoint *offset);

private:
    HWND wizardHWND() const;
    bool drawTitleText(QPainter *painter, const QString &text, const QRect &rect, HDC hdc);
    static bool drawBlackRect(const QRect &rect, HDC hdc);

    static int frameSize() { return QVistaHelper::frameSizeDp() / QVistaHelper::m_devicePixelRatio; }
    static int frameSizeDp();
    static int captionSize() { return QVistaHelper::captionSizeDp() / QVistaHelper::m_devicePixelRatio; }
    static int captionSizeDp();

    static int backButtonSize() { return int(QStyleHelper::dpiScaled(30)); }
    static int iconSize();
    static int glowSize();
    int leftMargin() { return backButton_->isVisible() ? backButtonSize() + iconSpacing : 0; }

    int titleOffset();
    void drawTitleBar(QPainter *painter);
    void setMouseCursor(QPoint pos);
    void collapseTopFrameStrut();
    bool winEvent(MSG *message, long *result);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

    static int instanceCount;
    static VistaState cachedVistaState;
    static bool isCompositionEnabled();
    static bool isThemeActive();
    enum Changes { resizeTop, movePosition, noChange } change;
    QPoint pressedPos;
    bool pressed;
    QRect rtTop;
    QRect rtTitle;
    QWizard *wizard;
    QVistaBackButton *backButton_;

    int titleBarOffset;  // Extra spacing above the text
    int iconSpacing;    // Space between button and icon
    int textSpacing;    // Space between icon and text
    static int m_devicePixelRatio;
};


QT_END_NAMESPACE

#endif // style_windowsvista
#endif // QWIZARD_WIN_P_H
