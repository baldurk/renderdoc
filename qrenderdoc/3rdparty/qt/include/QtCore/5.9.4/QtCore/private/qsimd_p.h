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

#ifndef QSIMD_P_H
#define QSIMD_P_H

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

#include <QtCore/private/qglobal_p.h>
#include <qatomic.h>

/*
 * qt_module_config.prf defines the QT_COMPILER_SUPPORTS_XXX macros.
 * They mean the compiler supports the necessary flags and the headers
 * for the x86 and ARM intrinsics:
 *  - GCC: the -mXXX or march=YYY flag is necessary before #include
 *    up to 4.8; GCC >= 4.9 can include unconditionally
 *  - Intel CC: #include can happen unconditionally
 *  - MSVC: #include can happen unconditionally
 *  - RVCT: ???
 *
 * We will try to include all headers possible under this configuration.
 *
 * MSVC does not define __SSE2__ & family, so we will define them. MSVC 2013 &
 * up do define __AVX__ if the -arch:AVX option is passed on the command-line.
 *
 * Supported XXX are:
 *   Flag    | Arch |  GCC  | Intel CC |  MSVC  |
 *  ARM_NEON | ARM  | I & C | None     |   ?    |
 *  SSE2     | x86  | I & C | I & C    | I & C  |
 *  SSE3     | x86  | I & C | I & C    | I only |
 *  SSSE3    | x86  | I & C | I & C    | I only |
 *  SSE4_1   | x86  | I & C | I & C    | I only |
 *  SSE4_2   | x86  | I & C | I & C    | I only |
 *  AVX      | x86  | I & C | I & C    | I & C  |
 *  AVX2     | x86  | I & C | I & C    | I only |
 *  AVX512xx | x86  | I & C | I & C    | I only |
 * I = intrinsics; C = code generation
 *
 * Code can use the following constructs to determine compiler support & status:
 * - #ifdef __XXX__      (e.g: #ifdef __AVX__  or #ifdef __ARM_NEON__)
 *   If this test passes, then the compiler is already generating code for that
 *   given sub-architecture. The intrinsics for that sub-architecture are
 *   #included and can be used without restriction or runtime check.
 *
 * - #if QT_COMPILER_SUPPORTS(XXX)
 *   If this test passes, then the compiler is able to generate code for that
 *   given sub-architecture in another translation unit, given the right set of
 *   flags. Use of the intrinsics is not guaranteed. This is useful with
 *   runtime detection (see below).
 *
 * - #if QT_COMPILER_SUPPORTS_HERE(XXX)
 *   If this test passes, then the compiler is able to generate code for that
 *   given sub-architecture in this translation unit, even if it is not doing
 *   that now (it might be). Individual functions may be tagged with
 *   QT_FUNCTION_TARGET(XXX) to cause the compiler to generate code for that
 *   sub-arch. Only inside such functions is the use of the intrisics
 *   guaranteed to work. This is useful with runtime detection (see below).
 *
 * Runtime detection of a CPU sub-architecture can be done with the
 * qCpuHasFeature(XXX) function. There are two strategies for generating
 * optimized code like that:
 *
 * 1) place the optimized code in a different translation unit (C or assembly
 * sources) and pass the correct flags to the compiler to enable support. Those
 * sources must not include qglobal.h, which means they cannot include this
 * file either. The dispatcher function would look like this:
 *
 *      void foo()
 *      {
 *      #if QT_COMPILER_SUPPORTS(XXX)
 *          if (qCpuHasFeature(XXX)) {
 *              foo_optimized_xxx();
 *              return;
 *          }
 *      #endif
 *          foo_plain();
 *      }
 *
 * 2) place the optimized code in a function tagged with QT_FUNCTION_TARGET and
 * surrounded by #if QT_COMPILER_SUPPORTS_HERE(XXX). That code can freely use
 * other Qt code. The dispatcher function would look like this:
 *
 *      void foo()
 *      {
 *      #if QT_COMPILER_SUPPORTS_HERE(XXX)
 *          if (qCpuHasFeature(XXX)) {
 *              foo_optimized_xxx();
 *              return;
 *          }
 *      #endif
 *          foo_plain();
 *      }
 */

#if defined(__MINGW64_VERSION_MAJOR) || defined(Q_CC_MSVC)
#include <intrin.h>
#endif

#define QT_COMPILER_SUPPORTS(x)     (QT_COMPILER_SUPPORTS_ ## x - 0)

#if defined(Q_PROCESSOR_ARM)
#  define QT_COMPILER_SUPPORTS_HERE(x)    (__ARM_FEATURE_ ## x)
#  if defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 600
     /* GCC requires attributes for a function */
#    define QT_FUNCTION_TARGET(x)  __attribute__((__target__(QT_FUNCTION_TARGET_STRING_ ## x)))
#  else
#    define QT_FUNCTION_TARGET(x)
#  endif
#  if !defined(__ARM_FEATURE_NEON) && defined(__ARM_NEON__)
#    define __ARM_FEATURE_NEON           // also support QT_COMPILER_SUPPORTS_HERE(NEON)
#  endif
#elif defined(Q_PROCESSOR_MIPS)
#  define QT_COMPILER_SUPPORTS_HERE(x)    (__ ## x ## __)
#  define QT_FUNCTION_TARGET(x)
#  if !defined(__MIPS_DSP__) && defined(__mips_dsp) && defined(Q_PROCESSOR_MIPS_32)
#    define __MIPS_DSP__
#  endif
#  if !defined(__MIPS_DSPR2__) && defined(__mips_dspr2) && defined(Q_PROCESSOR_MIPS_32)
#    define __MIPS_DSPR2__
#  endif
#elif (defined(Q_CC_INTEL) || defined(Q_CC_MSVC) \
    || (defined(Q_CC_GNU) && !defined(Q_CC_CLANG) && Q_CC_GNU >= 409) \
    || (defined(Q_CC_CLANG) && Q_CC_CLANG >= 308)) \
    && !defined(QT_BOOTSTRAPPED)
#  define QT_COMPILER_SUPPORTS_SIMD_ALWAYS
#  define QT_COMPILER_SUPPORTS_HERE(x)    ((__ ## x ## __) || QT_COMPILER_SUPPORTS(x))
#  if defined(Q_CC_GNU) && !defined(Q_CC_INTEL)
     /* GCC requires attributes for a function */
#    define QT_FUNCTION_TARGET(x)  __attribute__((__target__(QT_FUNCTION_TARGET_STRING_ ## x)))
#  else
#    define QT_FUNCTION_TARGET(x)
#  endif
#else
#  define QT_COMPILER_SUPPORTS_HERE(x)    (__ ## x ## __)
#  define QT_FUNCTION_TARGET(x)
#endif

#if defined(Q_CC_MSVC) && (defined(_M_AVX) || defined(__AVX__))
// Visual Studio defines __AVX__ when /arch:AVX is passed, but not the earlier macros
// See: https://msdn.microsoft.com/en-us/library/b0084kay.aspx
// SSE2 is handled by _M_IX86_FP below
#  define __SSE3__ 1
#  define __SSSE3__ 1
// no Intel CPU supports SSE4a, so don't define it
#  define __SSE4_1__ 1
#  define __SSE4_2__ 1
#  ifndef __AVX__
#    define __AVX__ 1
#  endif
#endif

// SSE intrinsics
#if defined(__SSE2__) || (defined(QT_COMPILER_SUPPORTS_SSE2) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS))
#if defined(QT_LINUXBASE)
/// this is an evil hack - the posix_memalign declaration in LSB
/// is wrong - see http://bugs.linuxbase.org/show_bug.cgi?id=2431
#  define posix_memalign _lsb_hack_posix_memalign
#  include <emmintrin.h>
#  undef posix_memalign
#else
#  include <emmintrin.h>
#endif
#if defined(Q_CC_MSVC) && (defined(_M_X64) || _M_IX86_FP >= 2)
#  define __SSE__ 1
#  define __SSE2__ 1
#endif
#endif

// SSE3 intrinsics
#if defined(__SSE3__) || (defined(QT_COMPILER_SUPPORTS_SSE3) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS))
#include <pmmintrin.h>
#endif

// SSSE3 intrinsics
#if defined(__SSSE3__) || (defined(QT_COMPILER_SUPPORTS_SSSE3) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS))
#include <tmmintrin.h>
#endif

// SSE4.1 intrinsics
#if defined(__SSE4_1__) || (defined(QT_COMPILER_SUPPORTS_SSE4_1) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS))
#include <smmintrin.h>
#endif

// SSE4.2 intrinsics
#if defined(__SSE4_2__) || (defined(QT_COMPILER_SUPPORTS_SSE4_2) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS))
#include <nmmintrin.h>

#  if defined(__SSE4_2__) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS) && (defined(Q_CC_INTEL) || defined(Q_CC_MSVC))
// POPCNT instructions:
// All processors that support SSE4.2 support POPCNT
// (but neither MSVC nor the Intel compiler define this macro)
#    define __POPCNT__                      1
#  endif
#endif

// AVX intrinsics
#if defined(__AVX__) || (defined(QT_COMPILER_SUPPORTS_AVX) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS))
// immintrin.h is the ultimate header, we don't need anything else after this
#include <immintrin.h>

#  if defined(__AVX__) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS) && (defined(Q_CC_INTEL) || defined(Q_CC_MSVC))
// AES, PCLMULQDQ instructions:
// All processors that support AVX support AES, PCLMULQDQ
// (but neither MSVC nor the Intel compiler define these macros)
#    define __AES__                         1
#    define __PCLMUL__                      1
#  endif

#  if defined(__AVX2__) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS) && (defined(Q_CC_INTEL) || defined(Q_CC_MSVC))
// F16C & RDRAND instructions:
// All processors that support AVX2 support F16C & RDRAND:
// (but neither MSVC nor the Intel compiler define these macros)
#    define __F16C__                        1
#    define __RDRND__                       1
#  endif
#endif

#if defined(__AES__) || defined(__PCLMUL__)
#  include <wmmintrin.h>
#endif

#define QT_FUNCTION_TARGET_STRING_SSE2      "sse2"
#define QT_FUNCTION_TARGET_STRING_SSE3      "sse3"
#define QT_FUNCTION_TARGET_STRING_SSSE3     "ssse3"
#define QT_FUNCTION_TARGET_STRING_SSE4_1    "sse4.1"
#define QT_FUNCTION_TARGET_STRING_SSE4_2    "sse4.2"
#define QT_FUNCTION_TARGET_STRING_AVX       "avx"
#define QT_FUNCTION_TARGET_STRING_AVX2      "avx2"
#define QT_FUNCTION_TARGET_STRING_AVX512F       "avx512f"
#define QT_FUNCTION_TARGET_STRING_AVX512CD      "avx512cd"
#define QT_FUNCTION_TARGET_STRING_AVX512ER      "avx512er"
#define QT_FUNCTION_TARGET_STRING_AVX512PF      "avx512pf"
#define QT_FUNCTION_TARGET_STRING_AVX512BW      "avx512bw"
#define QT_FUNCTION_TARGET_STRING_AVX512DQ      "avx512dq"
#define QT_FUNCTION_TARGET_STRING_AVX512VL      "avx512vl"
#define QT_FUNCTION_TARGET_STRING_AVX512IFMA    "avx512ifma"
#define QT_FUNCTION_TARGET_STRING_AVX512VBMI    "avx512vbmi"

#define QT_FUNCTION_TARGET_STRING_AES           "aes,sse4.2"
#define QT_FUNCTION_TARGET_STRING_PCLMUL        "pclmul,sse4.2"
#define QT_FUNCTION_TARGET_STRING_POPCNT        "popcnt"
#define QT_FUNCTION_TARGET_STRING_F16C          "f16c,avx"
#define QT_FUNCTION_TARGET_STRING_RDRAND        "rdrnd"
#define QT_FUNCTION_TARGET_STRING_BMI           "bmi"
#define QT_FUNCTION_TARGET_STRING_BMI2          "bmi2"
#define QT_FUNCTION_TARGET_STRING_RDSEED        "rdseed"
#define QT_FUNCTION_TARGET_STRING_SHA           "sha"

// other x86 intrinsics
#if defined(Q_PROCESSOR_X86) && ((defined(Q_CC_GNU) && (Q_CC_GNU >= 404)) \
    || (defined(Q_CC_CLANG) && (Q_CC_CLANG >= 208)) \
    || defined(Q_CC_INTEL))
#  define QT_COMPILER_SUPPORTS_X86INTRIN
#  ifdef Q_CC_INTEL
// The Intel compiler has no <x86intrin.h> -- all intrinsics are in <immintrin.h>;
#    include <immintrin.h>
#  else
// GCC 4.4 and Clang 2.8 added a few more intrinsics there
#    include <x86intrin.h>
#  endif
#endif

// Clang compiler fix, see http://lists.llvm.org/pipermail/cfe-commits/Week-of-Mon-20160222/151168.html
// This should be tweaked with an "upper version" of clang once we know which release fixes the
// issue. At that point we can rely on __ARM_FEATURE_CRC32 again.
#if defined(Q_CC_CLANG) && defined(Q_OS_DARWIN) && defined (__ARM_FEATURE_CRC32)
#  undef __ARM_FEATURE_CRC32
#endif

// NEON intrinsics
// note: as of GCC 4.9, does not support function targets for ARM
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define QT_FUNCTION_TARGET_STRING_NEON      "+neon" // unused: gcc doesn't support function targets on non-aarch64, and on Aarch64 NEON is always available.
#ifndef __ARM_NEON__
// __ARM_NEON__ is not defined on AArch64, but we need it in our NEON detection.
#define __ARM_NEON__
#endif
#endif
// AArch64/ARM64
#if defined(Q_PROCESSOR_ARM_V8) && defined(__ARM_FEATURE_CRC32)
#if defined(Q_PROCESSOR_ARM_64)
// only available on aarch64
#define QT_FUNCTION_TARGET_STRING_CRC32      "+crc"
#endif
#  include <arm_acle.h>
#endif

#undef QT_COMPILER_SUPPORTS_SIMD_ALWAYS

QT_BEGIN_NAMESPACE


enum CPUFeatures {
#if defined(Q_PROCESSOR_ARM)
    CpuFeatureNEON          = 0,
    CpuFeatureARM_NEON      = CpuFeatureNEON,
    CpuFeatureCRC32         = 1,
#elif defined(Q_PROCESSOR_MIPS)
    CpuFeatureDSP           = 0,
    CpuFeatureDSPR2         = 1,
#elif defined(Q_PROCESSOR_X86)
    // The order of the flags is jumbled so it matches most closely the bits in CPUID
    // Out of order:
    CpuFeatureSSE2          = 1,                       // uses the bit for PCLMULQDQ
    // in level 1, ECX
    CpuFeatureSSE3          = (0 + 0),
    CpuFeatureSSSE3         = (0 + 9),
    CpuFeatureSSE4_1        = (0 + 19),
    CpuFeatureSSE4_2        = (0 + 20),
    CpuFeatureMOVBE         = (0 + 22),
    CpuFeaturePOPCNT        = (0 + 23),
    CpuFeatureAES           = (0 + 25),
    CpuFeatureAVX           = (0 + 28),
    CpuFeatureF16C          = (0 + 29),
    CpuFeatureRDRAND        = (0 + 30),
    // 31 is always zero and we've used it for the QSimdInitialized

    // in level 7, leaf 0, EBX
    CpuFeatureBMI           = (32 + 3),
    CpuFeatureHLE           = (32 + 4),
    CpuFeatureAVX2          = (32 + 5),
    CpuFeatureBMI2          = (32 + 8),
    CpuFeatureRTM           = (32 + 11),
    CpuFeatureAVX512F       = (32 + 16),
    CpuFeatureAVX512DQ      = (32 + 17),
    CpuFeatureRDSEED        = (32 + 18),
    CpuFeatureAVX512IFMA    = (32 + 21),
    CpuFeatureAVX512PF      = (32 + 26),
    CpuFeatureAVX512ER      = (32 + 27),
    CpuFeatureAVX512CD      = (32 + 28),
    CpuFeatureSHA           = (32 + 29),
    CpuFeatureAVX512BW      = (32 + 30),
    CpuFeatureAVX512VL      = (32 + 31),

    // in level 7, leaf 0, ECX (out of order, for now)
    CpuFeatureAVX512VBMI    = 2,                       // uses the bit for DTES64
#endif

    // used only to indicate that the CPU detection was initialised
    QSimdInitialized = 0x80000000
};

static const quint64 qCompilerCpuFeatures = 0
#if defined __SHA__
        | (Q_UINT64_C(1) << CpuFeatureSHA)
#endif
#if defined __AES__
        | (Q_UINT64_C(1) << CpuFeatureAES)
#endif
#if defined __RTM__
        | (Q_UINT64_C(1) << CpuFeatureRTM)
#endif
#ifdef __RDRND__
        | (Q_UINT64_C(1) << CpuFeatureRDRAND)
#endif
#ifdef __RDSEED__
        | (Q_UINT64_C(1) << CpuFeatureRDSEED)
#endif
#if defined __BMI__
        | (Q_UINT64_C(1) << CpuFeatureBMI)
#endif
#if defined __BMI2__
        | (Q_UINT64_C(1) << CpuFeatureBMI2)
#endif
#if defined __F16C__
        | (Q_UINT64_C(1) << CpuFeatureF16C)
#endif
#if defined __POPCNT__
        | (Q_UINT64_C(1) << CpuFeaturePOPCNT)
#endif
#if defined __MOVBE__           // GCC and Clang don't seem to define this
        | (Q_UINT64_C(1) << CpuFeatureMOVBE)
#endif
#if defined __AVX512F__
        | (Q_UINT64_C(1) << CpuFeatureAVX512F)
#endif
#if defined __AVX512CD__
        | (Q_UINT64_C(1) << CpuFeatureAVX512CD)
#endif
#if defined __AVX512ER__
        | (Q_UINT64_C(1) << CpuFeatureAVX512ER)
#endif
#if defined __AVX512PF__
        | (Q_UINT64_C(1) << CpuFeatureAVX512PF)
#endif
#if defined __AVX512BW__
        | (Q_UINT64_C(1) << CpuFeatureAVX512BW)
#endif
#if defined __AVX512DQ__
        | (Q_UINT64_C(1) << CpuFeatureAVX512DQ)
#endif
#if defined __AVX512VL__
        | (Q_UINT64_C(1) << CpuFeatureAVX512VL)
#endif
#if defined __AVX512IFMA__
        | (Q_UINT64_C(1) << CpuFeatureAVX512IFMA)
#endif
#if defined __AVX512VBMI__
        | (Q_UINT64_C(1) << CpuFeatureAVX512VBMI)
#endif
#if defined __AVX2__
        | (Q_UINT64_C(1) << CpuFeatureAVX2)
#endif
#if defined __AVX__
        | (Q_UINT64_C(1) << CpuFeatureAVX)
#endif
#if defined __SSE4_2__
        | (Q_UINT64_C(1) << CpuFeatureSSE4_2)
#endif
#if defined __SSE4_1__
        | (Q_UINT64_C(1) << CpuFeatureSSE4_1)
#endif
#if defined __SSSE3__
        | (Q_UINT64_C(1) << CpuFeatureSSSE3)
#endif
#if defined __SSE3__
        | (Q_UINT64_C(1) << CpuFeatureSSE3)
#endif
#if defined __SSE2__
        | (Q_UINT64_C(1) << CpuFeatureSSE2)
#endif
#if defined __ARM_NEON__
        | (Q_UINT64_C(1) << CpuFeatureNEON)
#endif
#if defined __ARM_FEATURE_CRC32
        | (Q_UINT64_C(1) << CpuFeatureCRC32)
#endif
#if defined __mips_dsp
        | (Q_UINT64_C(1) << CpuFeatureDSP)
#endif
#if defined __mips_dspr2
        | (Q_UINT64_C(1) << CpuFeatureDSPR2)
#endif
        ;

#ifdef Q_ATOMIC_INT64_IS_SUPPORTED
extern Q_CORE_EXPORT QBasicAtomicInteger<quint64> qt_cpu_features[1];
#else
extern Q_CORE_EXPORT QBasicAtomicInteger<unsigned> qt_cpu_features[2];
#endif
Q_CORE_EXPORT void qDetectCpuFeatures();

static inline quint64 qCpuFeatures()
{
    quint64 features = qt_cpu_features[0].load();
#ifndef Q_ATOMIC_INT64_IS_SUPPORTED
    features |= quint64(qt_cpu_features[1].load()) << 32;
#endif
    if (Q_UNLIKELY(features == 0)) {
        qDetectCpuFeatures();
        features = qt_cpu_features[0].load();
#ifndef Q_ATOMIC_INT64_IS_SUPPORTED
        features |= quint64(qt_cpu_features[1].load()) << 32;
#endif
        Q_ASSUME(features != 0);
    }
    return features;
}

#define qCpuHasFeature(feature)     ((qCompilerCpuFeatures & (Q_UINT64_C(1) << CpuFeature ## feature)) \
                                     || (qCpuFeatures() & (Q_UINT64_C(1) << CpuFeature ## feature)))

#define ALIGNMENT_PROLOGUE_16BYTES(ptr, i, length) \
    for (; i < static_cast<int>(qMin(static_cast<quintptr>(length), ((4 - ((reinterpret_cast<quintptr>(ptr) >> 2) & 0x3)) & 0x3))); ++i)

#define ALIGNMENT_PROLOGUE_32BYTES(ptr, i, length) \
    for (; i < static_cast<int>(qMin(static_cast<quintptr>(length), ((8 - ((reinterpret_cast<quintptr>(ptr) >> 2) & 0x7)) & 0x7))); ++i)

#define SIMD_EPILOGUE(i, length, max) \
    for (int _i = 0; _i < max && i < length; ++i, ++_i)

QT_END_NAMESPACE

#endif // QSIMD_P_H
