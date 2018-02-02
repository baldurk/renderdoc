/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2013 Aleix Pol Gonzalez <aleixpol@kde.org>
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

#ifndef QCOLLATOR_P_H
#define QCOLLATOR_P_H

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
#include "qcollator.h"
#include <QVector>
#if QT_CONFIG(icu)
#include <unicode/ucol.h>
#elif defined(Q_OS_OSX)
#include <CoreServices/CoreServices.h>
#elif defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

QT_BEGIN_NAMESPACE

#if QT_CONFIG(icu)
typedef UCollator *CollatorType;
typedef QByteArray CollatorKeyType;

#elif defined(Q_OS_OSX)
typedef CollatorRef CollatorType;
typedef QVector<UCCollationValue> CollatorKeyType;

#elif defined(Q_OS_WIN)
typedef QString CollatorKeyType;
typedef int CollatorType;
#  ifdef Q_OS_WINRT
#    define USE_COMPARESTRINGEX
#  endif

#else //posix
typedef QVector<wchar_t> CollatorKeyType;
typedef int CollatorType;
#endif

class QCollatorPrivate
{
public:
    QAtomicInt ref;
    QLocale locale;
#if defined(Q_OS_WIN) && !QT_CONFIG(icu)
#ifdef USE_COMPARESTRINGEX
    QString localeName;
#else
    LCID localeID;
#endif
#endif
    Qt::CaseSensitivity caseSensitivity;
    bool numericMode;
    bool ignorePunctuation;
    bool dirty;

    CollatorType collator;

    void clear() {
        cleanup();
        collator = 0;
    }

    void init();
    void cleanup();

    QCollatorPrivate()
        : ref(1),
          caseSensitivity(Qt::CaseSensitive),
          numericMode(false),
          ignorePunctuation(false),
          dirty(true),
          collator(0)
    { cleanup(); }

    ~QCollatorPrivate() { cleanup(); }

private:
    Q_DISABLE_COPY(QCollatorPrivate)
};

class QCollatorSortKeyPrivate : public QSharedData
{
    friend class QCollator;
public:
    template <typename...T>
    explicit QCollatorSortKeyPrivate(T &&...args)
        : QSharedData()
        , m_key(std::forward<T>(args)...)
    {
    }

    CollatorKeyType m_key;

private:
    Q_DISABLE_COPY(QCollatorSortKeyPrivate)
};


QT_END_NAMESPACE

#endif // QCOLLATOR_P_H
