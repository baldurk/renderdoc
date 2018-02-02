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

#ifndef QCOLORPROFILE_P_H
#define QCOLORPROFILE_P_H

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
#include <QtGui/qrgb.h>
#include <QtGui/qrgba64.h>

QT_BEGIN_NAMESPACE

class Q_GUI_EXPORT QColorProfile
{
public:
    static QColorProfile *fromGamma(qreal gamma);
    static QColorProfile *fromSRgb();

    // The following methods all convert opaque or unpremultiplied colors:

    QRgba64 toLinear64(QRgb rgb32) const
    {
        ushort r = m_toLinear[qRed(rgb32) << 4];
        ushort g = m_toLinear[qGreen(rgb32) << 4];
        ushort b = m_toLinear[qBlue(rgb32) << 4];
        r = r + (r >> 8);
        g = g + (g >> 8);
        b = b + (b >> 8);
        return QRgba64::fromRgba64(r, g, b, qAlpha(rgb32) * 257);
    }

    QRgb toLinear(QRgb rgb32) const
    {
        uchar r = (m_toLinear[qRed(rgb32) << 4] + 0x80) >> 8;
        uchar g = (m_toLinear[qGreen(rgb32) << 4] + 0x80) >> 8;
        uchar b = (m_toLinear[qBlue(rgb32) << 4] + 0x80) >> 8;
        return qRgba(r, g, b, qAlpha(rgb32));
    }

    QRgba64 toLinear(QRgba64 rgb64) const
    {
        ushort r = rgb64.red();
        ushort g = rgb64.green();
        ushort b = rgb64.blue();
        r = r - (r >> 8);
        g = g - (g >> 8);
        b = b - (b >> 8);
        r = m_toLinear[r >> 4];
        g = m_toLinear[g >> 4];
        b = m_toLinear[b >> 4];
        r = r + (r >> 8);
        g = g + (g >> 8);
        b = b + (b >> 8);
        return QRgba64::fromRgba64(r, g, b, rgb64.alpha());
    }

    QRgb fromLinear64(QRgba64 rgb64) const
    {
        ushort r = rgb64.red();
        ushort g = rgb64.green();
        ushort b = rgb64.blue();
        r = r - (r >> 8);
        g = g - (g >> 8);
        b = b - (b >> 8);
        r = (m_fromLinear[r >> 4] + 0x80) >> 8;
        g = (m_fromLinear[g >> 4] + 0x80) >> 8;
        b = (m_fromLinear[b >> 4] + 0x80) >> 8;
        return qRgba(r, g, b, rgb64.alpha8());
    }

    QRgb fromLinear(QRgb rgb32) const
    {
        uchar r = (m_fromLinear[qRed(rgb32) << 4] + 0x80) >> 8;
        uchar g = (m_fromLinear[qGreen(rgb32) << 4] + 0x80) >> 8;
        uchar b = (m_fromLinear[qBlue(rgb32) << 4] + 0x80) >> 8;
        return qRgba(r, g, b, qAlpha(rgb32));
    }

    QRgba64 fromLinear(QRgba64 rgb64) const
    {
        ushort r = rgb64.red();
        ushort g = rgb64.green();
        ushort b = rgb64.blue();
        r = r - (r >> 8);
        g = g - (g >> 8);
        b = b - (b >> 8);
        r = m_fromLinear[r >> 4];
        g = m_fromLinear[g >> 4];
        b = m_fromLinear[b >> 4];
        r = r + (r >> 8);
        g = g + (g >> 8);
        b = b + (b >> 8);
        return QRgba64::fromRgba64(r, g, b, rgb64.alpha());
    }

private:
    QColorProfile() { }

    // We translate to 0-65280 (255*256) instead to 0-65535 to make simple
    // shifting an accurate conversion.
    // We translate from 0-4080 (255*16) for the same speed up, and to keep
    // the tables small enough to fit in most inner caches.
    ushort m_toLinear[(255 * 16) + 1]; // [0-4080] -> [0-65280]
    ushort m_fromLinear[(255 * 16) + 1]; // [0-4080] -> [0-65280]

};

QT_END_NAMESPACE

#endif // QCOLORPROFILE_P_H
