/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QMUTEX_H
#define QMUTEX_H

#include <QtCore/qglobal.h>
#include <QtCore/qatomic.h>
#include <new>

#if QT_HAS_INCLUDE(<chrono>)
#  include <chrono>
#  include <limits>
#endif

class tst_QMutex;

QT_BEGIN_NAMESPACE


#if !defined(QT_NO_THREAD) || defined(Q_CLANG_QDOC)

#ifdef Q_OS_LINUX
# define QT_MUTEX_LOCK_NOEXCEPT Q_DECL_NOTHROW
#else
# define QT_MUTEX_LOCK_NOEXCEPT
#endif

class QMutexData;

class Q_CORE_EXPORT QBasicMutex
{
public:
    // BasicLockable concept
    inline void lock() QT_MUTEX_LOCK_NOEXCEPT {
        if (!fastTryLock())
            lockInternal();
    }

    // BasicLockable concept
    inline void unlock() Q_DECL_NOTHROW {
        Q_ASSERT(d_ptr.load()); //mutex must be locked
        if (!fastTryUnlock())
            unlockInternal();
    }

    bool tryLock() Q_DECL_NOTHROW {
        return fastTryLock();
    }

    // Lockable concept
    bool try_lock() Q_DECL_NOTHROW { return tryLock(); }

    bool isRecursive() Q_DECL_NOTHROW; //### Qt6: remove me
    bool isRecursive() const Q_DECL_NOTHROW;

private:
    inline bool fastTryLock() Q_DECL_NOTHROW {
        return d_ptr.testAndSetAcquire(Q_NULLPTR, dummyLocked());
    }
    inline bool fastTryUnlock() Q_DECL_NOTHROW {
        return d_ptr.testAndSetRelease(dummyLocked(), Q_NULLPTR);
    }
    inline bool fastTryLock(QMutexData *&current) Q_DECL_NOTHROW {
        return d_ptr.testAndSetAcquire(Q_NULLPTR, dummyLocked(), current);
    }
    inline bool fastTryUnlock(QMutexData *&current) Q_DECL_NOTHROW {
        return d_ptr.testAndSetRelease(dummyLocked(), Q_NULLPTR, current);
    }

    void lockInternal() QT_MUTEX_LOCK_NOEXCEPT;
    bool lockInternal(int timeout) QT_MUTEX_LOCK_NOEXCEPT;
    void unlockInternal() Q_DECL_NOTHROW;

    QBasicAtomicPointer<QMutexData> d_ptr;
    static inline QMutexData *dummyLocked() {
        return reinterpret_cast<QMutexData *>(quintptr(1));
    }

    friend class QMutex;
    friend class QMutexData;
};

class Q_CORE_EXPORT QMutex : public QBasicMutex
{
public:
    enum RecursionMode { NonRecursive, Recursive };
    explicit QMutex(RecursionMode mode = NonRecursive);
    ~QMutex();

    // BasicLockable concept
    void lock() QT_MUTEX_LOCK_NOEXCEPT;
    bool tryLock(int timeout = 0) QT_MUTEX_LOCK_NOEXCEPT;
    // BasicLockable concept
    void unlock() Q_DECL_NOTHROW;

    // Lockable concept
    bool try_lock() QT_MUTEX_LOCK_NOEXCEPT { return tryLock(); }

#if QT_HAS_INCLUDE(<chrono>)
    // TimedLockable concept
    template <class Rep, class Period>
    bool try_lock_for(std::chrono::duration<Rep, Period> duration)
    {
        return tryLock(convertToMilliseconds(duration));
    }

    // TimedLockable concept
    template<class Clock, class Duration>
    bool try_lock_until(std::chrono::time_point<Clock, Duration> timePoint)
    {
        // Implemented in terms of try_lock_for to honor the similar
        // requirement in N4606 § 30.4.1.3 [thread.timedmutex.requirements]/12.

        return try_lock_for(timePoint - Clock::now());
    }
#endif

    bool isRecursive() const Q_DECL_NOTHROW
    { return QBasicMutex::isRecursive(); }

private:
    Q_DISABLE_COPY(QMutex)
    friend class QMutexLocker;
    friend class ::tst_QMutex;

#if QT_HAS_INCLUDE(<chrono>)
    template<class Rep, class Period>
    static int convertToMilliseconds(std::chrono::duration<Rep, Period> duration)
    {
        // N4606 § 30.4.1.3.5 [thread.timedmutex.requirements] specifies that a
        // duration less than or equal to duration.zero() shall result in a
        // try_lock, unlike QMutex's tryLock with a negative duration which
        // results in a lock.

        if (duration <= duration.zero())
            return 0;

        // when converting from 'duration' to milliseconds, make sure that
        // the result is not shorter than 'duration':
        std::chrono::milliseconds wait = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        if (wait < duration)
            wait += std::chrono::milliseconds(1);
        Q_ASSERT(wait >= duration);
        const auto ms = wait.count();
        const auto maxInt = (std::numeric_limits<int>::max)();

        return ms < maxInt ? int(ms) : maxInt;
    }
#endif
};

class Q_CORE_EXPORT QMutexLocker
{
public:
#ifndef Q_CLANG_QDOC
    inline explicit QMutexLocker(QBasicMutex *m) QT_MUTEX_LOCK_NOEXCEPT
    {
        Q_ASSERT_X((reinterpret_cast<quintptr>(m) & quintptr(1u)) == quintptr(0),
                   "QMutexLocker", "QMutex pointer is misaligned");
        val = quintptr(m);
        if (Q_LIKELY(m)) {
            // call QMutex::lock() instead of QBasicMutex::lock()
            static_cast<QMutex *>(m)->lock();
            val |= 1;
        }
    }
#else
    QMutexLocker(QMutex *) { }
#endif
    inline ~QMutexLocker() { unlock(); }

    inline void unlock() Q_DECL_NOTHROW
    {
        if ((val & quintptr(1u)) == quintptr(1u)) {
            val &= ~quintptr(1u);
            mutex()->unlock();
        }
    }

    inline void relock() QT_MUTEX_LOCK_NOEXCEPT
    {
        if (val) {
            if ((val & quintptr(1u)) == quintptr(0u)) {
                mutex()->lock();
                val |= quintptr(1u);
            }
        }
    }

#if defined(Q_CC_MSVC)
#pragma warning( push )
#pragma warning( disable : 4312 ) // ignoring the warning from /Wp64
#endif

    inline QMutex *mutex() const
    {
        return reinterpret_cast<QMutex *>(val & ~quintptr(1u));
    }

#if defined(Q_CC_MSVC)
#pragma warning( pop )
#endif

private:
    Q_DISABLE_COPY(QMutexLocker)

    quintptr val;
};

#else // QT_NO_THREAD && !Q_CLANG_QDOC

class Q_CORE_EXPORT QMutex
{
public:
    enum RecursionMode { NonRecursive, Recursive };

    inline explicit QMutex(RecursionMode mode = NonRecursive) Q_DECL_NOTHROW { Q_UNUSED(mode); }

    inline void lock() Q_DECL_NOTHROW {}
    inline bool tryLock(int timeout = 0) Q_DECL_NOTHROW { Q_UNUSED(timeout); return true; }
    inline bool try_lock() Q_DECL_NOTHROW { return true; }
    inline void unlock() Q_DECL_NOTHROW {}
    inline bool isRecursive() const Q_DECL_NOTHROW { return true; }

#if QT_HAS_INCLUDE(<chrono>)
    template <class Rep, class Period>
    inline bool try_lock_for(std::chrono::duration<Rep, Period> duration) Q_DECL_NOTHROW
    {
        Q_UNUSED(duration);
        return true;
    }

    template<class Clock, class Duration>
    inline bool try_lock_until(std::chrono::time_point<Clock, Duration> timePoint) Q_DECL_NOTHROW
    {
        Q_UNUSED(timePoint);
        return true;
    }
#endif

private:
    Q_DISABLE_COPY(QMutex)
};

class Q_CORE_EXPORT QMutexLocker
{
public:
    inline explicit QMutexLocker(QMutex *) Q_DECL_NOTHROW {}
    inline ~QMutexLocker() Q_DECL_NOTHROW {}

    inline void unlock() Q_DECL_NOTHROW {}
    void relock() Q_DECL_NOTHROW {}
    inline QMutex *mutex() const Q_DECL_NOTHROW { return Q_NULLPTR; }

private:
    Q_DISABLE_COPY(QMutexLocker)
};

typedef QMutex QBasicMutex;

#endif // QT_NO_THREAD && !Q_CLANG_QDOC

QT_END_NAMESPACE

#endif // QMUTEX_H
