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

#ifndef QCOLOR_H
#define QCOLOR_H

#include <QtGui/qtguiglobal.h>
#include <QtGui/qrgb.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qrgba64.h>

QT_BEGIN_NAMESPACE


class QColor;
class QColormap;
class QVariant;

#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QColor &);
#endif
#ifndef QT_NO_DATASTREAM
Q_GUI_EXPORT QDataStream &operator<<(QDataStream &, const QColor &);
Q_GUI_EXPORT QDataStream &operator>>(QDataStream &, QColor &);
#endif

class Q_GUI_EXPORT QColor
{
public:
    enum Spec { Invalid, Rgb, Hsv, Cmyk, Hsl };
    enum NameFormat { HexRgb, HexArgb };

    inline QColor() Q_DECL_NOTHROW;
    QColor(Qt::GlobalColor color) Q_DECL_NOTHROW;
    inline QColor(int r, int g, int b, int a = 255);
    QColor(QRgb rgb) Q_DECL_NOTHROW;
    QColor(QRgba64 rgba64) Q_DECL_NOTHROW;
    inline QColor(const QString& name);
    inline QColor(const char *aname) : QColor(QLatin1String(aname)) {}
    inline QColor(QLatin1String name);
    QColor(Spec spec) Q_DECL_NOTHROW;

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    inline QColor(const QColor &color) Q_DECL_NOTHROW; // ### Qt 6: remove all of these, the trivial ones are fine.
# ifdef Q_COMPILER_RVALUE_REFS
    QColor(QColor &&other) Q_DECL_NOTHROW : cspec(other.cspec), ct(other.ct) {}
    QColor &operator=(QColor &&other) Q_DECL_NOTHROW
    { cspec = other.cspec; ct = other.ct; return *this; }
# endif
    QColor &operator=(const QColor &) Q_DECL_NOTHROW;
#endif // Qt < 6

    QColor &operator=(Qt::GlobalColor color) Q_DECL_NOTHROW;

    bool isValid() const Q_DECL_NOTHROW;

    // ### Qt 6: merge overloads
    QString name() const;
    QString name(NameFormat format) const;

    void setNamedColor(const QString& name);
    void setNamedColor(QLatin1String name);

    static QStringList colorNames();

    inline Spec spec() const Q_DECL_NOTHROW
    { return cspec; }

    int alpha() const Q_DECL_NOTHROW;
    void setAlpha(int alpha);

    qreal alphaF() const Q_DECL_NOTHROW;
    void setAlphaF(qreal alpha);

    int red() const Q_DECL_NOTHROW;
    int green() const Q_DECL_NOTHROW;
    int blue() const Q_DECL_NOTHROW;
    void setRed(int red);
    void setGreen(int green);
    void setBlue(int blue);

    qreal redF() const Q_DECL_NOTHROW;
    qreal greenF() const Q_DECL_NOTHROW;
    qreal blueF() const Q_DECL_NOTHROW;
    void setRedF(qreal red);
    void setGreenF(qreal green);
    void setBlueF(qreal blue);

    void getRgb(int *r, int *g, int *b, int *a = Q_NULLPTR) const;
    void setRgb(int r, int g, int b, int a = 255);

    void getRgbF(qreal *r, qreal *g, qreal *b, qreal *a = Q_NULLPTR) const;
    void setRgbF(qreal r, qreal g, qreal b, qreal a = 1.0);

    QRgba64 rgba64() const Q_DECL_NOTHROW;
    void setRgba64(QRgba64 rgba) Q_DECL_NOTHROW;

    QRgb rgba() const Q_DECL_NOTHROW;
    void setRgba(QRgb rgba) Q_DECL_NOTHROW;

    QRgb rgb() const Q_DECL_NOTHROW;
    void setRgb(QRgb rgb) Q_DECL_NOTHROW;

    int hue() const Q_DECL_NOTHROW; // 0 <= hue < 360
    int saturation() const Q_DECL_NOTHROW;
    int hsvHue() const Q_DECL_NOTHROW; // 0 <= hue < 360
    int hsvSaturation() const Q_DECL_NOTHROW;
    int value() const Q_DECL_NOTHROW;

    qreal hueF() const Q_DECL_NOTHROW; // 0.0 <= hueF < 360.0
    qreal saturationF() const Q_DECL_NOTHROW;
    qreal hsvHueF() const Q_DECL_NOTHROW; // 0.0 <= hueF < 360.0
    qreal hsvSaturationF() const Q_DECL_NOTHROW;
    qreal valueF() const Q_DECL_NOTHROW;

    void getHsv(int *h, int *s, int *v, int *a = Q_NULLPTR) const;
    void setHsv(int h, int s, int v, int a = 255);

    void getHsvF(qreal *h, qreal *s, qreal *v, qreal *a = Q_NULLPTR) const;
    void setHsvF(qreal h, qreal s, qreal v, qreal a = 1.0);

    int cyan() const Q_DECL_NOTHROW;
    int magenta() const Q_DECL_NOTHROW;
    int yellow() const Q_DECL_NOTHROW;
    int black() const Q_DECL_NOTHROW;

    qreal cyanF() const Q_DECL_NOTHROW;
    qreal magentaF() const Q_DECL_NOTHROW;
    qreal yellowF() const Q_DECL_NOTHROW;
    qreal blackF() const Q_DECL_NOTHROW;

    void getCmyk(int *c, int *m, int *y, int *k, int *a = Q_NULLPTR);
    void setCmyk(int c, int m, int y, int k, int a = 255);

    void getCmykF(qreal *c, qreal *m, qreal *y, qreal *k, qreal *a = Q_NULLPTR);
    void setCmykF(qreal c, qreal m, qreal y, qreal k, qreal a = 1.0);

    int hslHue() const Q_DECL_NOTHROW; // 0 <= hue < 360
    int hslSaturation() const Q_DECL_NOTHROW;
    int lightness() const Q_DECL_NOTHROW;

    qreal hslHueF() const Q_DECL_NOTHROW; // 0.0 <= hueF < 360.0
    qreal hslSaturationF() const Q_DECL_NOTHROW;
    qreal lightnessF() const Q_DECL_NOTHROW;

    void getHsl(int *h, int *s, int *l, int *a = Q_NULLPTR) const;
    void setHsl(int h, int s, int l, int a = 255);

    void getHslF(qreal *h, qreal *s, qreal *l, qreal *a = Q_NULLPTR) const;
    void setHslF(qreal h, qreal s, qreal l, qreal a = 1.0);

    QColor toRgb() const Q_DECL_NOTHROW;
    QColor toHsv() const Q_DECL_NOTHROW;
    QColor toCmyk() const Q_DECL_NOTHROW;
    QColor toHsl() const Q_DECL_NOTHROW;

    Q_REQUIRED_RESULT QColor convertTo(Spec colorSpec) const Q_DECL_NOTHROW;

    static QColor fromRgb(QRgb rgb) Q_DECL_NOTHROW;
    static QColor fromRgba(QRgb rgba) Q_DECL_NOTHROW;

    static QColor fromRgb(int r, int g, int b, int a = 255);
    static QColor fromRgbF(qreal r, qreal g, qreal b, qreal a = 1.0);

    static QColor fromRgba64(ushort r, ushort g, ushort b, ushort a = USHRT_MAX) Q_DECL_NOTHROW;
    static QColor fromRgba64(QRgba64 rgba) Q_DECL_NOTHROW;

    static QColor fromHsv(int h, int s, int v, int a = 255);
    static QColor fromHsvF(qreal h, qreal s, qreal v, qreal a = 1.0);

    static QColor fromCmyk(int c, int m, int y, int k, int a = 255);
    static QColor fromCmykF(qreal c, qreal m, qreal y, qreal k, qreal a = 1.0);

    static QColor fromHsl(int h, int s, int l, int a = 255);
    static QColor fromHslF(qreal h, qreal s, qreal l, qreal a = 1.0);

    Q_REQUIRED_RESULT QColor light(int f = 150) const Q_DECL_NOTHROW;
    Q_REQUIRED_RESULT QColor lighter(int f = 150) const Q_DECL_NOTHROW;
    Q_REQUIRED_RESULT QColor dark(int f = 200) const Q_DECL_NOTHROW;
    Q_REQUIRED_RESULT QColor darker(int f = 200) const Q_DECL_NOTHROW;

    bool operator==(const QColor &c) const Q_DECL_NOTHROW;
    bool operator!=(const QColor &c) const Q_DECL_NOTHROW;

    operator QVariant() const;

    static bool isValidColor(const QString &name);
    static bool isValidColor(QLatin1String) Q_DECL_NOTHROW;

private:

    void invalidate() Q_DECL_NOTHROW;
    template <typename String>
    bool setColorFromString(const String &name);

    Spec cspec;
    union {
        struct {
            ushort alpha;
            ushort red;
            ushort green;
            ushort blue;
            ushort pad;
        } argb;
        struct {
            ushort alpha;
            ushort hue;
            ushort saturation;
            ushort value;
            ushort pad;
        } ahsv;
        struct {
            ushort alpha;
            ushort cyan;
            ushort magenta;
            ushort yellow;
            ushort black;
        } acmyk;
        struct {
            ushort alpha;
            ushort hue;
            ushort saturation;
            ushort lightness;
            ushort pad;
        } ahsl;
        ushort array[5];
    } ct;

    friend class QColormap;
#ifndef QT_NO_DATASTREAM
    friend Q_GUI_EXPORT QDataStream &operator<<(QDataStream &, const QColor &);
    friend Q_GUI_EXPORT QDataStream &operator>>(QDataStream &, QColor &);
#endif
};
Q_DECLARE_TYPEINFO(QColor, QT_VERSION >= QT_VERSION_CHECK(6,0,0) ? Q_MOVABLE_TYPE : Q_RELOCATABLE_TYPE);

inline QColor::QColor() Q_DECL_NOTHROW
{ invalidate(); }

inline QColor::QColor(int r, int g, int b, int a)
{ setRgb(r, g, b, a); }

inline QColor::QColor(QLatin1String aname)
{ setNamedColor(aname); }

inline QColor::QColor(const QString& aname)
{ setNamedColor(aname); }

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
inline QColor::QColor(const QColor &acolor) Q_DECL_NOTHROW
    : cspec(acolor.cspec)
{ ct.argb = acolor.ct.argb; }
#endif

inline bool QColor::isValid() const Q_DECL_NOTHROW
{ return cspec != Invalid; }

inline QColor QColor::lighter(int f) const Q_DECL_NOTHROW
{ return light(f); }

inline QColor QColor::darker(int f) const Q_DECL_NOTHROW
{ return dark(f); }

QT_END_NAMESPACE

#endif // QCOLOR_H
