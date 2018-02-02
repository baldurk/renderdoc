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

#ifndef QCOMMONSTYLE_H
#define QCOMMONSTYLE_H

#include <QtWidgets/qtwidgetsglobal.h>
#include <QtWidgets/qstyle.h>

QT_BEGIN_NAMESPACE

class QCommonStylePrivate;

class Q_WIDGETS_EXPORT QCommonStyle: public QStyle
{
    Q_OBJECT

public:
    QCommonStyle();
    ~QCommonStyle();

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p,
                       const QWidget *w = Q_NULLPTR) const Q_DECL_OVERRIDE;
    void drawControl(ControlElement element, const QStyleOption *opt, QPainter *p,
                     const QWidget *w = Q_NULLPTR) const Q_DECL_OVERRIDE;
    QRect subElementRect(SubElement r, const QStyleOption *opt, const QWidget *widget = Q_NULLPTR) const Q_DECL_OVERRIDE;
    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt, QPainter *p,
                            const QWidget *w = Q_NULLPTR) const Q_DECL_OVERRIDE;
    SubControl hitTestComplexControl(ComplexControl cc, const QStyleOptionComplex *opt,
                                     const QPoint &pt, const QWidget *w = Q_NULLPTR) const Q_DECL_OVERRIDE;
    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc,
                         const QWidget *w = Q_NULLPTR) const Q_DECL_OVERRIDE;
    QSize sizeFromContents(ContentsType ct, const QStyleOption *opt,
                           const QSize &contentsSize, const QWidget *widget = Q_NULLPTR) const Q_DECL_OVERRIDE;

    int pixelMetric(PixelMetric m, const QStyleOption *opt = Q_NULLPTR, const QWidget *widget = Q_NULLPTR) const Q_DECL_OVERRIDE;

    int styleHint(StyleHint sh, const QStyleOption *opt = Q_NULLPTR, const QWidget *w = Q_NULLPTR,
                  QStyleHintReturn *shret = Q_NULLPTR) const Q_DECL_OVERRIDE;

    QIcon standardIcon(StandardPixmap standardIcon, const QStyleOption *opt = Q_NULLPTR,
                       const QWidget *widget = Q_NULLPTR) const Q_DECL_OVERRIDE;
    QPixmap standardPixmap(StandardPixmap sp, const QStyleOption *opt = Q_NULLPTR,
                           const QWidget *widget = Q_NULLPTR) const Q_DECL_OVERRIDE;

    QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap,
                                const QStyleOption *opt) const Q_DECL_OVERRIDE;
    int layoutSpacing(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2,
                      Qt::Orientation orientation, const QStyleOption *option = Q_NULLPTR,
                      const QWidget *widget = Q_NULLPTR) const Q_DECL_OVERRIDE;

    void polish(QPalette &) Q_DECL_OVERRIDE;
    void polish(QApplication *app) Q_DECL_OVERRIDE;
    void polish(QWidget *widget) Q_DECL_OVERRIDE;
    void unpolish(QWidget *widget) Q_DECL_OVERRIDE;
    void unpolish(QApplication *application) Q_DECL_OVERRIDE;

protected:
    QCommonStyle(QCommonStylePrivate &dd);

private:
    Q_DECLARE_PRIVATE(QCommonStyle)
    Q_DISABLE_COPY(QCommonStyle)
#ifndef QT_NO_ANIMATION
    Q_PRIVATE_SLOT(d_func(), void _q_removeAnimation())
#endif
};

QT_END_NAMESPACE

#endif // QCOMMONSTYLE_H
