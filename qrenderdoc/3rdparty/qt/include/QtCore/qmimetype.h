/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Klaralvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author David Faure <david.faure@kdab.com>
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

#ifndef QMIMETYPE_H
#define QMIMETYPE_H

#include <QtCore/qglobal.h>

#ifndef QT_NO_MIMETYPE

#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

QT_BEGIN_NAMESPACE

class QMimeTypePrivate;
class QStringList;
class QMimeType;

Q_CORE_EXPORT uint qHash(const QMimeType &key, uint seed = 0) Q_DECL_NOTHROW;

class Q_CORE_EXPORT QMimeType
{
public:
    QMimeType();
    QMimeType(const QMimeType &other);
    QMimeType &operator=(const QMimeType &other);
#ifdef Q_COMPILER_RVALUE_REFS
    QMimeType &operator=(QMimeType &&other) Q_DECL_NOTHROW { swap(other); return *this; }
#endif
    void swap(QMimeType &other) Q_DECL_NOTHROW
    {
        qSwap(d, other.d);
    }
    explicit QMimeType(const QMimeTypePrivate &dd);
    ~QMimeType();

    bool operator==(const QMimeType &other) const;

    inline bool operator!=(const QMimeType &other) const
    {
        return !operator==(other);
    }

    bool isValid() const;

    bool isDefault() const;

    QString name() const;
    QString comment() const;
    QString genericIconName() const;
    QString iconName() const;
    QStringList globPatterns() const;
    QStringList parentMimeTypes() const;
    QStringList allAncestors() const;
    QStringList aliases() const;
    QStringList suffixes() const;
    QString preferredSuffix() const;

    bool inherits(const QString &mimeTypeName) const;

    QString filterString() const;

protected:
    friend class QMimeTypeParserBase;
    friend class MimeTypeMapEntry;
    friend class QMimeDatabasePrivate;
    friend class QMimeXMLProvider;
    friend class QMimeBinaryProvider;
    friend class QMimeTypePrivate;
    friend Q_CORE_EXPORT uint qHash(const QMimeType &key, uint seed) Q_DECL_NOTHROW;

    QExplicitlySharedDataPointer<QMimeTypePrivate> d;
};

Q_DECLARE_SHARED(QMimeType)

#ifndef QT_NO_DEBUG_STREAM
class QDebug;
Q_CORE_EXPORT QDebug operator<<(QDebug debug, const QMimeType &mime);
#endif

QT_END_NAMESPACE

#endif // QT_NO_MIMETYPE
#endif // QMIMETYPE_H
