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

#ifndef QACCESSIBLEWIDGET_H
#define QACCESSIBLEWIDGET_H

#include <QtWidgets/qtwidgetsglobal.h>
#include <QtGui/qaccessibleobject.h>

QT_BEGIN_NAMESPACE


#ifndef QT_NO_ACCESSIBILITY

class QAccessibleWidgetPrivate;

class Q_WIDGETS_EXPORT QAccessibleWidget : public QAccessibleObject, public QAccessibleActionInterface
{
public:
    explicit QAccessibleWidget(QWidget *o, QAccessible::Role r = QAccessible::Client, const QString& name = QString());
    bool isValid() const Q_DECL_OVERRIDE;

    QWindow *window() const Q_DECL_OVERRIDE;
    int childCount() const Q_DECL_OVERRIDE;
    int indexOfChild(const QAccessibleInterface *child) const Q_DECL_OVERRIDE;
    QVector<QPair<QAccessibleInterface*, QAccessible::Relation> > relations(QAccessible::Relation match = QAccessible::AllRelations) const Q_DECL_OVERRIDE;
    QAccessibleInterface *focusChild() const Q_DECL_OVERRIDE;

    QRect rect() const Q_DECL_OVERRIDE;

    QAccessibleInterface *parent() const Q_DECL_OVERRIDE;
    QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;

    QString text(QAccessible::Text t) const Q_DECL_OVERRIDE;
    QAccessible::Role role() const Q_DECL_OVERRIDE;
    QAccessible::State state() const Q_DECL_OVERRIDE;

    QColor foregroundColor() const Q_DECL_OVERRIDE;
    QColor backgroundColor() const Q_DECL_OVERRIDE;

    void *interface_cast(QAccessible::InterfaceType t) Q_DECL_OVERRIDE;

    // QAccessibleActionInterface
    QStringList actionNames() const Q_DECL_OVERRIDE;
    void doAction(const QString &actionName) Q_DECL_OVERRIDE;
    QStringList keyBindingsForAction(const QString &actionName) const Q_DECL_OVERRIDE;
protected:
    ~QAccessibleWidget();
    QWidget *widget() const;
    QObject *parentObject() const;

    void addControllingSignal(const QString &signal);

private:
    QAccessibleWidgetPrivate *d;
    Q_DISABLE_COPY(QAccessibleWidget)
};


#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // QACCESSIBLEWIDGET_H
