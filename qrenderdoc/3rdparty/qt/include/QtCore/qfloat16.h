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

#ifndef QFLOAT16_H
#define QFLOAT16_H

#include <QtCore/qglobal.h>
#include <QtCore/qmetatype.h>
#include <string.h>

#if defined(QT_COMPILER_SUPPORTS_F16C) && defined(__AVX2__) && !defined(__F16C__)
// All processors that support AVX2 do support F16C too. That doesn't mean
// we're allowed to use the intrinsics directly, so we'll do it only for
// the Intel and Microsoft's compilers.
#  if defined(Q_CC_INTEL) || defined(Q_CC_MSVC)
#    define __F16C__        1
# endif
#endif

#if defined(QT_COMPILER_SUPPORTS_F16C) && defined(__F16C__)
#include <immintrin.h>
#endif

QT_BEGIN_NAMESPACE

#if 0
#pragma qt_class(QFloat16)
#pragma qt_no_master_include
#endif

class qfloat16
{
public:
#ifndef Q_QDOC
    Q_DECL_CONSTEXPR inline qfloat16() Q_DECL_NOTHROW : b16(0) { }
    inline qfloat16(float f) Q_DECL_NOTHROW;
    inline operator float() const Q_DECL_NOTHROW;
#endif

private:
    quint16 b16;

    Q_CORE_EXPORT static const quint32 mantissatable[];
    Q_CORE_EXPORT static const quint32 exponenttable[];
    Q_CORE_EXPORT static const quint32 offsettable[];
    Q_CORE_EXPORT static const quint32 basetable[];
    Q_CORE_EXPORT static const quint32 shifttable[];

    friend bool qIsNull(qfloat16 f) Q_DECL_NOTHROW;
    friend qfloat16 operator-(qfloat16 a) Q_DECL_NOTHROW;
};

Q_DECLARE_TYPEINFO(qfloat16, Q_PRIMITIVE_TYPE);

Q_REQUIRED_RESULT Q_CORE_EXPORT bool qIsInf(qfloat16 f) Q_DECL_NOTHROW;    // complements qnumeric.h
Q_REQUIRED_RESULT Q_CORE_EXPORT bool qIsNaN(qfloat16 f) Q_DECL_NOTHROW;    // complements qnumeric.h
Q_REQUIRED_RESULT Q_CORE_EXPORT bool qIsFinite(qfloat16 f) Q_DECL_NOTHROW; // complements qnumeric.h

// The remainder of these utility functions complement qglobal.h
Q_REQUIRED_RESULT inline int qRound(qfloat16 d) Q_DECL_NOTHROW
{ return qRound(static_cast<float>(d)); }

Q_REQUIRED_RESULT inline qint64 qRound64(qfloat16 d) Q_DECL_NOTHROW
{ return qRound64(static_cast<float>(d)); }

Q_REQUIRED_RESULT inline bool qFuzzyCompare(qfloat16 p1, qfloat16 p2) Q_DECL_NOTHROW
{
    float f1 = static_cast<float>(p1);
    float f2 = static_cast<float>(p2);
    // The significand precision for IEEE754 half precision is
    // 11 bits (10 explicitly stored), or approximately 3 decimal
    // digits.  In selecting the fuzzy comparison factor of 102.5f
    // (that is, (2^10+1)/10) below, we effectively select a
    // window of about 1 (least significant) decimal digit about
    // which the two operands can vary and still return true.
    return (qAbs(f1 - f2) * 102.5f <= qMin(qAbs(f1), qAbs(f2)));
}

Q_REQUIRED_RESULT inline bool qIsNull(qfloat16 f) Q_DECL_NOTHROW
{
    return (f.b16 & static_cast<quint16>(0x7fff)) == 0;
}

inline int qIntCast(qfloat16 f) Q_DECL_NOTHROW
{ return int(static_cast<float>(f)); }

QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wc99-extensions")
QT_WARNING_DISABLE_GCC("-Wold-style-cast")
inline qfloat16::qfloat16(float f) Q_DECL_NOTHROW
{
#if defined(QT_COMPILER_SUPPORTS_F16C) && defined(__F16C__)
    __m128 packsingle = _mm_set_ss(f);
    __m128i packhalf = _mm_cvtps_ph(packsingle, 0);
    b16 = _mm_extract_epi16(packhalf, 0);
#elif defined (__ARM_FP16_FORMAT_IEEE)
    __fp16 f16 = f;
    memcpy(&b16, &f16, sizeof(quint16));
#else
    quint32 u;
    memcpy(&u, &f, sizeof(quint32));
    b16 = basetable[(u >> 23) & 0x1ff]
          + ((u & 0x007fffff) >> shifttable[(u >> 23) & 0x1ff]);
#endif
}
QT_WARNING_POP

inline qfloat16::operator float() const Q_DECL_NOTHROW
{
#if defined(QT_COMPILER_SUPPORTS_F16C) && defined(__F16C__)
    __m128i packhalf = _mm_cvtsi32_si128(b16);
    __m128 packsingle = _mm_cvtph_ps(packhalf);
    return _mm_cvtss_f32(packsingle);
#elif defined (__ARM_FP16_FORMAT_IEEE)
    __fp16 f16;
    memcpy(&f16, &b16, sizeof(quint16));
    return float(f16);
#else
    quint32 u = mantissatable[offsettable[b16 >> 10] + (b16 & 0x3ff)]
                + exponenttable[b16 >> 10];
    float f;
    memcpy(&f, &u, sizeof(quint32));
    return f;
#endif
}

inline qfloat16 operator-(qfloat16 a) Q_DECL_NOTHROW
{
    qfloat16 f;
    f.b16 = a.b16 ^ quint16(0x8000);
    return f;
}

inline qfloat16 operator+(qfloat16 a, qfloat16 b) Q_DECL_NOTHROW { return qfloat16(static_cast<float>(a) + static_cast<float>(b)); }
inline qfloat16 operator-(qfloat16 a, qfloat16 b) Q_DECL_NOTHROW { return qfloat16(static_cast<float>(a) - static_cast<float>(b)); }
inline qfloat16 operator*(qfloat16 a, qfloat16 b) Q_DECL_NOTHROW { return qfloat16(static_cast<float>(a) * static_cast<float>(b)); }
inline qfloat16 operator/(qfloat16 a, qfloat16 b) Q_DECL_NOTHROW { return qfloat16(static_cast<float>(a) / static_cast<float>(b)); }

#define QF16_MAKE_ARITH_OP_FP(FP, OP) \
    inline FP operator OP(qfloat16 lhs, FP rhs) Q_DECL_NOTHROW { return static_cast<FP>(lhs) OP rhs; } \
    inline FP operator OP(FP lhs, qfloat16 rhs) Q_DECL_NOTHROW { return lhs OP static_cast<FP>(rhs); }
#define QF16_MAKE_ARITH_OP_EQ_FP(FP, OP_EQ, OP) \
    inline qfloat16& operator OP_EQ(qfloat16& lhs, FP rhs) Q_DECL_NOTHROW { lhs = qfloat16(static_cast<FP>(lhs) OP rhs); return lhs; }
#define QF16_MAKE_ARITH_OP(FP) \
    QF16_MAKE_ARITH_OP_FP(FP, +) \
    QF16_MAKE_ARITH_OP_FP(FP, -) \
    QF16_MAKE_ARITH_OP_FP(FP, *) \
    QF16_MAKE_ARITH_OP_FP(FP, /) \
    QF16_MAKE_ARITH_OP_EQ_FP(FP, +=, +) \
    QF16_MAKE_ARITH_OP_EQ_FP(FP, -=, -) \
    QF16_MAKE_ARITH_OP_EQ_FP(FP, *=, *) \
    QF16_MAKE_ARITH_OP_EQ_FP(FP, /=, /)
QF16_MAKE_ARITH_OP(long double)
QF16_MAKE_ARITH_OP(double)
QF16_MAKE_ARITH_OP(float)
#undef QF16_MAKE_ARITH_OP
#undef QF16_MAKE_ARITH_OP_FP

#define QF16_MAKE_ARITH_OP_INT(OP) \
    inline double operator OP(qfloat16 lhs, int rhs) Q_DECL_NOTHROW { return static_cast<double>(lhs) OP rhs; } \
    inline double operator OP(int lhs, qfloat16 rhs) Q_DECL_NOTHROW { return lhs OP static_cast<double>(rhs); }
QF16_MAKE_ARITH_OP_INT(+)
QF16_MAKE_ARITH_OP_INT(-)
QF16_MAKE_ARITH_OP_INT(*)
QF16_MAKE_ARITH_OP_INT(/)
#undef QF16_MAKE_ARITH_OP_INT

QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wfloat-equal")
QT_WARNING_DISABLE_GCC("-Wfloat-equal")

inline bool operator>(qfloat16 a, qfloat16 b)  Q_DECL_NOTHROW { return static_cast<float>(a) >  static_cast<float>(b); }
inline bool operator<(qfloat16 a, qfloat16 b)  Q_DECL_NOTHROW { return static_cast<float>(a) <  static_cast<float>(b); }
inline bool operator>=(qfloat16 a, qfloat16 b) Q_DECL_NOTHROW { return static_cast<float>(a) >= static_cast<float>(b); }
inline bool operator<=(qfloat16 a, qfloat16 b) Q_DECL_NOTHROW { return static_cast<float>(a) <= static_cast<float>(b); }
inline bool operator==(qfloat16 a, qfloat16 b) Q_DECL_NOTHROW { return static_cast<float>(a) == static_cast<float>(b); }
inline bool operator!=(qfloat16 a, qfloat16 b) Q_DECL_NOTHROW { return static_cast<float>(a) != static_cast<float>(b); }

#define QF16_MAKE_BOOL_OP_FP(FP, OP) \
    inline bool operator OP(qfloat16 lhs, FP rhs) Q_DECL_NOTHROW { return static_cast<FP>(lhs) OP rhs; } \
    inline bool operator OP(FP lhs, qfloat16 rhs) Q_DECL_NOTHROW { return lhs OP static_cast<FP>(rhs); }
#define QF16_MAKE_BOOL_OP(FP) \
    QF16_MAKE_BOOL_OP_FP(FP, <) \
    QF16_MAKE_BOOL_OP_FP(FP, >) \
    QF16_MAKE_BOOL_OP_FP(FP, >=) \
    QF16_MAKE_BOOL_OP_FP(FP, <=) \
    QF16_MAKE_BOOL_OP_FP(FP, ==) \
    QF16_MAKE_BOOL_OP_FP(FP, !=)
QF16_MAKE_BOOL_OP(long double)
QF16_MAKE_BOOL_OP(double)
QF16_MAKE_BOOL_OP(float)
#undef QF16_MAKE_BOOL_OP
#undef QF16_MAKE_BOOL_OP_FP

#define QF16_MAKE_BOOL_OP_INT(OP) \
    inline bool operator OP(qfloat16 a, int b) Q_DECL_NOTHROW { return static_cast<float>(a) OP b; } \
    inline bool operator OP(int a, qfloat16 b) Q_DECL_NOTHROW { return a OP static_cast<float>(b); }
QF16_MAKE_BOOL_OP_INT(>)
QF16_MAKE_BOOL_OP_INT(<)
QF16_MAKE_BOOL_OP_INT(>=)
QF16_MAKE_BOOL_OP_INT(<=)
QF16_MAKE_BOOL_OP_INT(==)
QF16_MAKE_BOOL_OP_INT(!=)
#undef QF16_MAKE_BOOL_OP_INT

QT_WARNING_POP

/*!
  \internal
*/
Q_REQUIRED_RESULT inline bool qFuzzyIsNull(qfloat16 f) Q_DECL_NOTHROW
{
    return qAbs(static_cast<float>(f)) <= 0.001f;
}

QT_END_NAMESPACE

Q_DECLARE_METATYPE(qfloat16)

#endif // QFLOAT16_H
