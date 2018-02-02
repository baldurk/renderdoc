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

#ifndef QPUSHBUTTON_H
#define QPUSHBUTTON_H

#include <QtWidgets/qtwidgetsglobal.h>
#include <QtWidgets/qabstractbutton.h>

QT_REQUIRE_CONFIG(pushbutton);

QT_BEGIN_NAMESPACE


class QPushButtonPrivate;
class QMenu;
class QStyleOptionButton;

class Q_WIDGETS_EXPORT QPushButton : public QAbstractButton
{
    Q_OBJECT

    Q_PROPERTY(bool autoDefault READ autoDefault WRITE setAutoDefault)
    Q_PROPERTY(bool default READ isDefault WRITE setDefault)
    Q_PROPERTY(bool flat READ isFlat WRITE setFlat)

public:
    explicit QPushButton(QWidget *parent = Q_NULLPTR);
    explicit QPushButton(const QString &text, QWidget *parent = Q_NULLPTR);
    QPushButton(const QIcon& icon, const QString &text, QWidget *parent = Q_NULLPTR);
    ~QPushButton();

    QSize sizeHint() const Q_DECL_OVERRIDE;
    QSize minimumSizeHint() const Q_DECL_OVERRIDE;

    bool autoDefault() const;
    void setAutoDefault(bool);
    bool isDefault() const;
    void setDefault(bool);

#if QT_CONFIG(menu)
    void setMenu(QMenu* menu);
    QMenu* menu() const;
#endif

    void setFlat(bool);
    bool isFlat() const;

public Q_SLOTS:
#if QT_CONFIG(menu)
    void showMenu();
#endif

protected:
    bool event(QEvent *e) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *) Q_DECL_OVERRIDE;
    void focusInEvent(QFocusEvent *) Q_DECL_OVERRIDE;
    void focusOutEvent(QFocusEvent *) Q_DECL_OVERRIDE;
    void initStyleOption(QStyleOptionButton *option) const;
    QPushButton(QPushButtonPrivate &dd, QWidget* parent = Q_NULLPTR);

public:

private:
    Q_DISABLE_COPY(QPushButton)
    Q_DECLARE_PRIVATE(QPushButton)
#if QT_CONFIG(menu)
    Q_PRIVATE_SLOT(d_func(), void _q_popupPressed())
#endif
};

QT_END_NAMESPACE

#endif // QPUSHBUTTON_H
