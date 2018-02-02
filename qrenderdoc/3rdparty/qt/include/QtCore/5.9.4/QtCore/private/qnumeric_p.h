/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
** Contact: https://www.qt.io/licensing/
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

#ifndef QNUMERIC_P_H
#define QNUMERIC_P_H

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

#include "QtCore/private/qglobal_p.h"
#include <cmath>
#include <limits>

#if defined(Q_CC_MSVC)
#  include <intrin.h>
#elif defined(Q_CC_INTEL)
#  include <immintrin.h>    // for _addcarry_u<nn>
#endif

#if defined(Q_CC_MSVC)
#include <float.h>
#endif

#if !defined(Q_CC_MSVC) && (defined(Q_OS_QNX) || defined(Q_CC_INTEL))
#  include <math.h>
#  ifdef isnan
#    define QT_MATH_H_DEFINES_MACROS
QT_BEGIN_NAMESPACE
namespace qnumeric_std_wrapper {
// the 'using namespace std' below is cases where the stdlib already put the math.h functions in the std namespace and undefined the macros.
static inline bool math_h_isnan(double d) { using namespace std; return isnan(d); }
static inline bool math_h_isinf(double d) { using namespace std; return isinf(d); }
static inline bool math_h_isfinite(double d) { using namespace std; return isfinite(d); }
static inline bool math_h_isnan(float f) { using namespace std; return isnan(f); }
static inline bool math_h_isinf(float f) { using namespace std; return isinf(f); }
static inline bool math_h_isfinite(float f) { using namespace std; return isfinite(f); }
}
QT_END_NAMESPACE
// These macros from math.h conflict with the real functions in the std namespace.
#    undef signbit
#    undef isnan
#    undef isinf
#    undef isfinite
#  endif // defined(isnan)
#endif

QT_BEGIN_NAMESPACE

namespace qnumeric_std_wrapper {
#if defined(QT_MATH_H_DEFINES_MACROS)
#  undef QT_MATH_H_DEFINES_MACROS
static inline bool isnan(double d) { return math_h_isnan(d); }
static inline bool isinf(double d) { return math_h_isinf(d); }
static inline bool isfinite(double d) { return math_h_isfinite(d); }
static inline bool isnan(float f) { return math_h_isnan(f); }
static inline bool isinf(float f) { return math_h_isinf(f); }
static inline bool isfinite(float f) { return math_h_isfinite(f); }
#else
static inline bool isnan(double d) { return std::isnan(d); }
static inline bool isinf(double d) { return std::isinf(d); }
static inline bool isfinite(double d) { return std::isfinite(d); }
static inline bool isnan(float f) { return std::isnan(f); }
static inline bool isinf(float f) { return std::isinf(f); }
static inline bool isfinite(float f) { return std::isfinite(f); }
#endif
}

Q_DECL_CONSTEXPR static inline double qt_inf() Q_DECL_NOEXCEPT
{
    Q_STATIC_ASSERT_X(std::numeric_limits<double>::has_infinity,
                      "platform has no definition for infinity for type double");
    return std::numeric_limits<double>::infinity();
}

// Signaling NaN
Q_DECL_CONSTEXPR static inline double qt_snan() Q_DECL_NOEXCEPT
{
    Q_STATIC_ASSERT_X(std::numeric_limits<double>::has_signaling_NaN,
                      "platform has no definition for signaling NaN for type double");
    return std::numeric_limits<double>::signaling_NaN();
}

// Quiet NaN
Q_DECL_CONSTEXPR static inline double qt_qnan() Q_DECL_NOEXCEPT
{
    Q_STATIC_ASSERT_X(std::numeric_limits<double>::has_quiet_NaN,
                      "platform has no definition for quiet NaN for type double");
    return std::numeric_limits<double>::quiet_NaN();
}

static inline bool qt_is_inf(double d)
{
    return qnumeric_std_wrapper::isinf(d);
}

static inline bool qt_is_nan(double d)
{
    return qnumeric_std_wrapper::isnan(d);
}

static inline bool qt_is_finite(double d)
{
    return qnumeric_std_wrapper::isfinite(d);
}

static inline bool qt_is_inf(float f)
{
    return qnumeric_std_wrapper::isinf(f);
}

static inline bool qt_is_nan(float f)
{
    return qnumeric_std_wrapper::isnan(f);
}

static inline bool qt_is_finite(float f)
{
    return qnumeric_std_wrapper::isfinite(f);
}

//
// Unsigned overflow math
//
namespace {
template <typename T> inline typename std::enable_if<std::is_unsigned<T>::value, bool>::type
add_overflow(T v1, T v2, T *r)
{
    // unsigned additions are well-defined
    *r = v1 + v2;
    return v1 > T(v1 + v2);
}

template <typename T> inline typename std::enable_if<std::is_unsigned<T>::value, bool>::type
mul_overflow(T v1, T v2, T *r)
{
    // use the next biggest type
    // Note: for 64-bit systems where __int128 isn't supported, this will cause an error.
    // A fallback is present below.
    typedef typename QIntegerForSize<sizeof(T) * 2>::Unsigned Larger;
    Larger lr = Larger(v1) * Larger(v2);
    *r = T(lr);
    return lr > std::numeric_limits<T>::max();
}

#if defined(__SIZEOF_INT128__)
#  define HAVE_MUL64_OVERFLOW
#endif

// GCC 5 and Clang have builtins to detect overflows
#if (defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 500) || QT_HAS_BUILTIN(__builtin_uadd_overflow)
template <> inline bool add_overflow(unsigned v1, unsigned v2, unsigned *r)
{ return __builtin_uadd_overflow(v1, v2, r); }
#endif
#if (defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 500) || QT_HAS_BUILTIN(__builtin_uaddl_overflow)
template <> inline bool add_overflow(unsigned long v1, unsigned long v2, unsigned long *r)
{ return __builtin_uaddl_overflow(v1, v2, r); }
#endif
#if (defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 500) || QT_HAS_BUILTIN(__builtin_uaddll_overflow)
template <> inline bool add_overflow(unsigned long long v1, unsigned long long v2, unsigned long long *r)
{ return __builtin_uaddll_overflow(v1, v2, r); }
#endif

#if (defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 500) || QT_HAS_BUILTIN(__builtin_umul_overflow)
template <> inline bool mul_overflow(unsigned v1, unsigned v2, unsigned *r)
{ return __builtin_umul_overflow(v1, v2, r); }
#endif
#if (defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 500) || QT_HAS_BUILTIN(__builtin_umull_overflow)
template <> inline bool mul_overflow(unsigned long v1, unsigned long v2, unsigned long *r)
{ return __builtin_umull_overflow(v1, v2, r); }
#endif
#if (defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 500) || QT_HAS_BUILTIN(__builtin_umulll_overflow)
template <> inline bool mul_overflow(unsigned long long v1, unsigned long long v2, unsigned long long *r)
{ return __builtin_umulll_overflow(v1, v2, r); }
#  define HAVE_MUL64_OVERFLOW
#endif

#if ((defined(Q_CC_MSVC) && _MSC_VER >= 1800) || defined(Q_CC_INTEL)) && defined(Q_PROCESSOR_X86) && !QT_HAS_BUILTIN(__builtin_uadd_overflow)
template <> inline bool add_overflow(unsigned v1, unsigned v2, unsigned *r)
{ return _addcarry_u32(0, v1, v2, r); }
#  ifdef Q_CC_MSVC      // longs are 32-bit
template <> inline bool add_overflow(unsigned long v1, unsigned long v2, unsigned long *r)
{ return _addcarry_u32(0, v1, v2, reinterpret_cast<unsigned *>(r)); }
#  endif
#endif
#if ((defined(Q_CC_MSVC) && _MSC_VER >= 1800) || defined(Q_CC_INTEL)) && defined(Q_PROCESSOR_X86_64) && !QT_HAS_BUILTIN(__builtin_uadd_overflow)
template <> inline bool add_overflow(quint64 v1, quint64 v2, quint64 *r)
{ return _addcarry_u64(0, v1, v2, reinterpret_cast<unsigned __int64 *>(r)); }
#  ifndef Q_CC_MSVC      // longs are 64-bit
template <> inline bool add_overflow(unsigned long v1, unsigned long v2, unsigned long *r)
{ return _addcarry_u64(0, v1, v2, reinterpret_cast<unsigned __int64 *>(r)); }
#  endif
#endif

#if defined(Q_CC_MSVC) && (defined(Q_PROCESSOR_X86_64) || defined(Q_PROCESSOR_IA64)) && !QT_HAS_BUILTIN(__builtin_uadd_overflow)
#pragma intrinsic(_umul128)
template <> inline bool mul_overflow(quint64 v1, quint64 v2, quint64 *r)
{
    // use 128-bit multiplication with the _umul128 intrinsic
    // https://msdn.microsoft.com/en-us/library/3dayytw9.aspx
    quint64 high;
    *r = _umul128(v1, v2, &high);
    return high;
}
#  define HAVE_MUL64_OVERFLOW
#endif

#if !defined(HAVE_MUL64_OVERFLOW) && defined(__LP64__)
// no 128-bit multiplication, we need to figure out with a slow division
template <> inline bool mul_overflow(quint64 v1, quint64 v2, quint64 *r)
{
    if (v2 && v1 > std::numeric_limits<quint64>::max() / v2)
        return true;
    *r = v1 * v2;
    return false;
}
template <> inline bool mul_overflow(unsigned long v1, unsigned long v2, unsigned long *r)
{
    return mul_overflow<quint64>(v1, v2, reinterpret_cast<quint64 *>(r));
}
#else
#  undef HAVE_MUL64_OVERFLOW
#endif

//
// Signed overflow math
//
// In C++, signed overflow math is Undefined Behavior. However, many CPUs do implement some way to
// check for overflow. Some compilers expose intrinsics to use this functionality. If the no
// intrinsic is exposed, overflow checking can be done by widening the result type and "manually"
// checking for overflow. Or, alternatively, by using inline assembly to use the CPU features.
//
// Only int overflow checking is implemented, because it's the only one used.
#if (defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 500) || QT_HAS_BUILTIN(__builtin_sadd_overflow)
inline bool add_overflow(int v1, int v2, int *r)
{ return __builtin_sadd_overflow(v1, v2, r); }
#elif defined(Q_CC_GNU) && defined(Q_PROCESSOR_X86)
inline bool add_overflow(int v1, int v2, int *r)
{
    quint8 overflow = 0;
    int res = v1;

    asm ("addl %2, %1\n"
         "seto %0"
         : "=q" (overflow), "=r" (res)
         : "r" (v2), "1" (res)
         : "cc"
    );
    *r = res;
    return overflow;
}
#else
inline bool add_overflow(int v1, int v2, int *r)
{
    qint64 t = qint64(v1) + v2;
    *r = static_cast<int>(t);
    return t > std::numeric_limits<int>::max() || t < std::numeric_limits<int>::min();
}
#endif

#if (defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 500) || QT_HAS_BUILTIN(__builtin_ssub_overflow)
inline bool sub_overflow(int v1, int v2, int *r)
{ return __builtin_ssub_overflow(v1, v2, r); }
#elif defined(Q_CC_GNU) && defined(Q_PROCESSOR_X86)
inline bool sub_overflow(int v1, int v2, int *r)
{
    quint8 overflow = 0;
    int res = v1;

    asm ("subl %2, %1\n"
         "seto %0"
         : "=q" (overflow), "=r" (res)
         : "r" (v2), "1" (res)
         : "cc"
    );
    *r = res;
    return overflow;
}
#else
inline bool sub_overflow(int v1, int v2, int *r)
{
    qint64 t = qint64(v1) - v2;
    *r = static_cast<int>(t);
    return t > std::numeric_limits<int>::max() || t < std::numeric_limits<int>::min();
}
#endif

#if (defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 500) || QT_HAS_BUILTIN(__builtin_smul_overflow)
inline bool mul_overflow(int v1, int v2, int *r)
{ return __builtin_smul_overflow(v1, v2, r); }
#elif defined(Q_CC_GNU) && defined(Q_PROCESSOR_X86)
inline bool mul_overflow(int v1, int v2, int *r)
{
    quint8 overflow = 0;
    int res = v1;

    asm ("imul %2, %1\n"
         "seto %0"
         : "=q" (overflow), "=r" (res)
         : "r" (v2), "1" (res)
         : "cc"
    );
    *r = res;
    return overflow;
}
#else
inline bool mul_overflow(int v1, int v2, int *r)
{
    qint64 t = qint64(v1) * v2;
    *r = static_cast<int>(t);
    return t > std::numeric_limits<int>::max() || t < std::numeric_limits<int>::min();
}
#endif

}

QT_END_NAMESPACE

#endif // QNUMERIC_P_H
