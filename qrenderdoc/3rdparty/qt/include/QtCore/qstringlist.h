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

#include <QtCore/qlist.h>

#ifndef QSTRINGLIST_H
#define QSTRINGLIST_H

#include <QtCore/qalgorithms.h>
#include <QtCore/qregexp.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringmatcher.h>

QT_BEGIN_NAMESPACE

class QRegExp;
class QRegularExpression;

typedef QListIterator<QString> QStringListIterator;
typedef QMutableListIterator<QString> QMutableStringListIterator;

class QStringList;

#ifdef Q_QDOC
class QStringList : public QList<QString>
#else
template <> struct QListSpecialMethods<QString>
#endif
{
#ifndef Q_QDOC
protected:
    ~QListSpecialMethods() {}
#endif
public:
    inline void sort(Qt::CaseSensitivity cs = Qt::CaseSensitive);
    inline int removeDuplicates();

    inline QString join(const QString &sep) const;
    inline QString join(QLatin1String sep) const;
    inline QString join(QChar sep) const;

    inline QStringList filter(const QString &str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    inline QStringList &replaceInStrings(const QString &before, const QString &after, Qt::CaseSensitivity cs = Qt::CaseSensitive);

#ifndef QT_NO_REGEXP
    inline QStringList filter(const QRegExp &rx) const;
    inline QStringList &replaceInStrings(const QRegExp &rx, const QString &after);
#endif

#ifndef QT_BOOTSTRAPPED
#ifndef QT_NO_REGULAREXPRESSION
    inline QStringList filter(const QRegularExpression &re) const;
    inline QStringList &replaceInStrings(const QRegularExpression &re, const QString &after);
#endif // QT_NO_REGULAREXPRESSION
#endif // QT_BOOTSTRAPPED

#ifndef Q_QDOC
private:
    inline QStringList *self();
    inline const QStringList *self() const;
};

// ### Qt6: check if there's a better way
class QStringList : public QList<QString>
{
#endif
public:
    inline QStringList() Q_DECL_NOTHROW { }
    inline explicit QStringList(const QString &i) { append(i); }
    inline QStringList(const QList<QString> &l) : QList<QString>(l) { }
#ifdef Q_COMPILER_RVALUE_REFS
    inline QStringList(QList<QString> &&l) Q_DECL_NOTHROW : QList<QString>(std::move(l)) { }
#endif
#ifdef Q_COMPILER_INITIALIZER_LISTS
    inline QStringList(std::initializer_list<QString> args) : QList<QString>(args) { }
#endif

    QStringList &operator=(const QList<QString> &other)
    { QList<QString>::operator=(other); return *this; }
#ifdef Q_COMPILER_RVALUE_REFS
    QStringList &operator=(QList<QString> &&other) Q_DECL_NOTHROW
    { QList<QString>::operator=(std::move(other)); return *this; }
#endif

    inline bool contains(const QString &str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;

    inline QStringList operator+(const QStringList &other) const
    { QStringList n = *this; n += other; return n; }
    inline QStringList &operator<<(const QString &str)
    { append(str); return *this; }
    inline QStringList &operator<<(const QStringList &l)
    { *this += l; return *this; }
    inline QStringList &operator<<(const QList<QString> &l)
    { *this += l; return *this; }

#ifndef QT_NO_REGEXP
    inline int indexOf(const QRegExp &rx, int from = 0) const;
    inline int lastIndexOf(const QRegExp &rx, int from = -1) const;
    inline int indexOf(QRegExp &rx, int from = 0) const;
    inline int lastIndexOf(QRegExp &rx, int from = -1) const;
#endif

#ifndef QT_BOOTSTRAPPED
#ifndef QT_NO_REGULAREXPRESSION
    inline int indexOf(const QRegularExpression &re, int from = 0) const;
    inline int lastIndexOf(const QRegularExpression &re, int from = -1) const;
#endif // QT_NO_REGULAREXPRESSION
#endif // QT_BOOTSTRAPPED

    using QList<QString>::indexOf;
    using QList<QString>::lastIndexOf;
};

Q_DECLARE_TYPEINFO(QStringList, Q_MOVABLE_TYPE);

#ifndef Q_QDOC
inline QStringList *QListSpecialMethods<QString>::self()
{ return static_cast<QStringList *>(this); }
inline const QStringList *QListSpecialMethods<QString>::self() const
{ return static_cast<const QStringList *>(this); }

namespace QtPrivate {
    void Q_CORE_EXPORT QStringList_sort(QStringList *that, Qt::CaseSensitivity cs);
    int Q_CORE_EXPORT QStringList_removeDuplicates(QStringList *that);
    QString Q_CORE_EXPORT QStringList_join(const QStringList *that, const QChar *sep, int seplen);
    Q_CORE_EXPORT QString QStringList_join(const QStringList &list, QLatin1String sep);
    QStringList Q_CORE_EXPORT QStringList_filter(const QStringList *that, const QString &str,
                                               Qt::CaseSensitivity cs);

    bool Q_CORE_EXPORT QStringList_contains(const QStringList *that, const QString &str, Qt::CaseSensitivity cs);
    void Q_CORE_EXPORT QStringList_replaceInStrings(QStringList *that, const QString &before, const QString &after,
                                      Qt::CaseSensitivity cs);

#ifndef QT_NO_REGEXP
    void Q_CORE_EXPORT QStringList_replaceInStrings(QStringList *that, const QRegExp &rx, const QString &after);
    QStringList Q_CORE_EXPORT QStringList_filter(const QStringList *that, const QRegExp &re);
    int Q_CORE_EXPORT QStringList_indexOf(const QStringList *that, const QRegExp &rx, int from);
    int Q_CORE_EXPORT QStringList_lastIndexOf(const QStringList *that, const QRegExp &rx, int from);
    int Q_CORE_EXPORT QStringList_indexOf(const QStringList *that, QRegExp &rx, int from);
    int Q_CORE_EXPORT QStringList_lastIndexOf(const QStringList *that, QRegExp &rx, int from);
#endif

#ifndef QT_BOOTSTRAPPED
#ifndef QT_NO_REGULAREXPRESSION
    void Q_CORE_EXPORT QStringList_replaceInStrings(QStringList *that, const QRegularExpression &rx, const QString &after);
    QStringList Q_CORE_EXPORT QStringList_filter(const QStringList *that, const QRegularExpression &re);
    int Q_CORE_EXPORT QStringList_indexOf(const QStringList *that, const QRegularExpression &re, int from);
    int Q_CORE_EXPORT QStringList_lastIndexOf(const QStringList *that, const QRegularExpression &re, int from);
#endif // QT_NO_REGULAREXPRESSION
#endif // QT_BOOTSTRAPPED
}

inline void QListSpecialMethods<QString>::sort(Qt::CaseSensitivity cs)
{
    QtPrivate::QStringList_sort(self(), cs);
}

inline int QListSpecialMethods<QString>::removeDuplicates()
{
    return QtPrivate::QStringList_removeDuplicates(self());
}

inline QString QListSpecialMethods<QString>::join(const QString &sep) const
{
    return QtPrivate::QStringList_join(self(), sep.constData(), sep.length());
}

QString QListSpecialMethods<QString>::join(QLatin1String sep) const
{
    return QtPrivate::QStringList_join(*self(), sep);
}

inline QString QListSpecialMethods<QString>::join(QChar sep) const
{
    return QtPrivate::QStringList_join(self(), &sep, 1);
}

inline QStringList QListSpecialMethods<QString>::filter(const QString &str, Qt::CaseSensitivity cs) const
{
    return QtPrivate::QStringList_filter(self(), str, cs);
}

inline bool QStringList::contains(const QString &str, Qt::CaseSensitivity cs) const
{
    return QtPrivate::QStringList_contains(this, str, cs);
}

inline QStringList &QListSpecialMethods<QString>::replaceInStrings(const QString &before, const QString &after, Qt::CaseSensitivity cs)
{
    QtPrivate::QStringList_replaceInStrings(self(), before, after, cs);
    return *self();
}

inline QStringList operator+(const QList<QString> &one, const QStringList &other)
{
    QStringList n = one;
    n += other;
    return n;
}

#ifndef QT_NO_REGEXP
inline QStringList &QListSpecialMethods<QString>::replaceInStrings(const QRegExp &rx, const QString &after)
{
    QtPrivate::QStringList_replaceInStrings(self(), rx, after);
    return *self();
}

inline QStringList QListSpecialMethods<QString>::filter(const QRegExp &rx) const
{
    return QtPrivate::QStringList_filter(self(), rx);
}

inline int QStringList::indexOf(const QRegExp &rx, int from) const
{
    return QtPrivate::QStringList_indexOf(this, rx, from);
}

inline int QStringList::lastIndexOf(const QRegExp &rx, int from) const
{
    return QtPrivate::QStringList_lastIndexOf(this, rx, from);
}

inline int QStringList::indexOf(QRegExp &rx, int from) const
{
    return QtPrivate::QStringList_indexOf(this, rx, from);
}

inline int QStringList::lastIndexOf(QRegExp &rx, int from) const
{
    return QtPrivate::QStringList_lastIndexOf(this, rx, from);
}
#endif

#ifndef QT_BOOTSTRAPPED
#ifndef QT_NO_REGULAREXPRESSION
inline QStringList &QListSpecialMethods<QString>::replaceInStrings(const QRegularExpression &rx, const QString &after)
{
    QtPrivate::QStringList_replaceInStrings(self(), rx, after);
    return *self();
}

inline QStringList QListSpecialMethods<QString>::filter(const QRegularExpression &rx) const
{
    return QtPrivate::QStringList_filter(self(), rx);
}

inline int QStringList::indexOf(const QRegularExpression &rx, int from) const
{
    return QtPrivate::QStringList_indexOf(this, rx, from);
}

inline int QStringList::lastIndexOf(const QRegularExpression &rx, int from) const
{
    return QtPrivate::QStringList_lastIndexOf(this, rx, from);
}
#endif // QT_NO_REGULAREXPRESSION
#endif // QT_BOOTSTRAPPED
#endif // Q_QDOC

QT_END_NAMESPACE

#endif // QSTRINGLIST_H
