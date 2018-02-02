/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QACCESSIBLEMENU_H
#define QACCESSIBLEMENU_H

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
#include <QtWidgets/qaccessiblewidget.h>
#include <QtCore/qpointer.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

#if QT_CONFIG(menu)
class QMenu;
class QMenuBar;
class QAction;

class QAccessibleMenu : public QAccessibleWidget
{
public:
    explicit QAccessibleMenu(QWidget *w);

    int childCount() const Q_DECL_OVERRIDE;
    QAccessibleInterface *childAt(int x, int y) const Q_DECL_OVERRIDE;

    QString text(QAccessible::Text t) const Q_DECL_OVERRIDE;
    QAccessible::Role role() const Q_DECL_OVERRIDE;
    QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;
    QAccessibleInterface *parent() const Q_DECL_OVERRIDE;
    int indexOfChild( const QAccessibleInterface *child ) const Q_DECL_OVERRIDE;

protected:
    QMenu *menu() const;
};

#if QT_CONFIG(menubar)
class QAccessibleMenuBar : public QAccessibleWidget
{
public:
    explicit QAccessibleMenuBar(QWidget *w);

    QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;
    int childCount() const Q_DECL_OVERRIDE;

    int indexOfChild(const QAccessibleInterface *child) const Q_DECL_OVERRIDE;

protected:
    QMenuBar *menuBar() const;
};
#endif // QT_CONFIG(menubar)


class QAccessibleMenuItem : public QAccessibleInterface, public QAccessibleActionInterface
{
public:
    explicit QAccessibleMenuItem(QWidget *owner, QAction *w);

    ~QAccessibleMenuItem();
    void *interface_cast(QAccessible::InterfaceType t) Q_DECL_OVERRIDE;

    int childCount() const Q_DECL_OVERRIDE;
    QAccessibleInterface *childAt(int x, int y) const Q_DECL_OVERRIDE;
    bool isValid() const Q_DECL_OVERRIDE;
    int indexOfChild(const QAccessibleInterface * child) const Q_DECL_OVERRIDE;

    QAccessibleInterface *parent() const Q_DECL_OVERRIDE;
    QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;
    QObject * object() const Q_DECL_OVERRIDE;
    QWindow *window() const Q_DECL_OVERRIDE;

    QRect rect() const Q_DECL_OVERRIDE;
    QAccessible::Role role() const Q_DECL_OVERRIDE;
    void setText(QAccessible::Text t, const QString & text) Q_DECL_OVERRIDE;
    QAccessible::State state() const Q_DECL_OVERRIDE;
    QString text(QAccessible::Text t) const Q_DECL_OVERRIDE;

    // QAccessibleActionInterface
    QStringList actionNames() const Q_DECL_OVERRIDE;
    void doAction(const QString &actionName) Q_DECL_OVERRIDE;
    QStringList keyBindingsForAction(const QString &actionName) const Q_DECL_OVERRIDE;

    QWidget *owner() const;
protected:
    QAction *action() const;
private:
    QAction *m_action;
    QPointer<QWidget> m_owner; // can hold either QMenu or the QMenuBar that contains the action
};

#endif // QT_CONFIG(menu)

QT_END_NAMESPACE
#endif // QT_NO_ACCESSIBILITY
#endif // QACCESSIBLEMENU_H
