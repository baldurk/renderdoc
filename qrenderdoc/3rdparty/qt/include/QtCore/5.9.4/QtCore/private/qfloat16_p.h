/****************************************************************************
**
** Copyright (C) 2016 by Southwest Research Institute (R)
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QFLOAT16_P_H
#define QFLOAT16_P_H

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

#include <QtCore/qfloat16.h>
#include <QtCore/qsysinfo.h>

QT_BEGIN_NAMESPACE

static inline bool qt_is_inf(qfloat16 d) Q_DECL_NOTHROW
{
    bool is_inf;
    uchar *ch = (uchar *)&d;
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
        is_inf = (ch[0] & 0x7c) == 0x7c && (ch[0] & 0x02) == 0;
    else
        is_inf = (ch[1] & 0x7c) == 0x7c && (ch[1] & 0x02) == 0;
    return is_inf;
}

static inline bool qt_is_nan(qfloat16 d) Q_DECL_NOTHROW
{
    bool is_nan;
    uchar *ch = (uchar *)&d;
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
        is_nan = (ch[0] & 0x7c) == 0x7c && (ch[0] & 0x02) != 0;
    else
        is_nan = (ch[1] & 0x7c) == 0x7c && (ch[1] & 0x02) != 0;
    return is_nan;
}

static inline bool qt_is_finite(qfloat16 d) Q_DECL_NOTHROW
{
    bool is_finite;
    uchar *ch = (uchar *)&d;
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
        is_finite = (ch[0] & 0x7c) != 0x7c;
    else
        is_finite = (ch[1] & 0x7c) != 0x7c;
    return is_finite;
}


QT_END_NAMESPACE

#endif // QFLOAT16_P_H
