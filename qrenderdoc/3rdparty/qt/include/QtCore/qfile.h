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

#ifndef QFILE_H
#define QFILE_H

#include <QtCore/qfiledevice.h>
#include <QtCore/qstring.h>
#include <stdio.h>

#ifdef open
#error qfile.h must be included before any header file that defines open
#endif

QT_BEGIN_NAMESPACE

class QTemporaryFile;
class QFilePrivate;

class Q_CORE_EXPORT QFile : public QFileDevice
{
#ifndef QT_NO_QOBJECT
    Q_OBJECT
#endif
    Q_DECLARE_PRIVATE(QFile)

public:
    QFile();
    QFile(const QString &name);
#ifndef QT_NO_QOBJECT
    explicit QFile(QObject *parent);
    QFile(const QString &name, QObject *parent);
#endif
    ~QFile();

    QString fileName() const Q_DECL_OVERRIDE;
    void setFileName(const QString &name);

#if defined(Q_OS_DARWIN)
    // Mac always expects filenames in UTF-8... and decomposed...
    static inline QByteArray encodeName(const QString &fileName)
    {
        return fileName.normalized(QString::NormalizationForm_D).toUtf8();
    }
    static QString decodeName(const QByteArray &localFileName)
    {
        return QString::fromUtf8(localFileName).normalized(QString::NormalizationForm_C);
    }
#else
    static inline QByteArray encodeName(const QString &fileName)
    {
        return fileName.toLocal8Bit();
    }
    static QString decodeName(const QByteArray &localFileName)
    {
        return QString::fromLocal8Bit(localFileName);
    }
#endif
    inline static QString decodeName(const char *localFileName)
        { return decodeName(QByteArray(localFileName)); }

#if QT_DEPRECATED_SINCE(5,0)
    typedef QByteArray (*EncoderFn)(const QString &fileName);
    typedef QString (*DecoderFn)(const QByteArray &localfileName);
    QT_DEPRECATED static void setEncodingFunction(EncoderFn) {}
    QT_DEPRECATED static void setDecodingFunction(DecoderFn) {}
#endif

    bool exists() const;
    static bool exists(const QString &fileName);

    QString readLink() const;
    static QString readLink(const QString &fileName);
    inline QString symLinkTarget() const { return readLink(); }
    inline static QString symLinkTarget(const QString &fileName) { return readLink(fileName); }

    bool remove();
    static bool remove(const QString &fileName);

    bool rename(const QString &newName);
    static bool rename(const QString &oldName, const QString &newName);

    bool link(const QString &newName);
    static bool link(const QString &oldname, const QString &newName);

    bool copy(const QString &newName);
    static bool copy(const QString &fileName, const QString &newName);

    bool open(OpenMode flags) Q_DECL_OVERRIDE;
    bool open(FILE *f, OpenMode ioFlags, FileHandleFlags handleFlags=DontCloseHandle);
    bool open(int fd, OpenMode ioFlags, FileHandleFlags handleFlags=DontCloseHandle);

    qint64 size() const Q_DECL_OVERRIDE;

    bool resize(qint64 sz) Q_DECL_OVERRIDE;
    static bool resize(const QString &filename, qint64 sz);

    Permissions permissions() const Q_DECL_OVERRIDE;
    static Permissions permissions(const QString &filename);
    bool setPermissions(Permissions permissionSpec) Q_DECL_OVERRIDE;
    static bool setPermissions(const QString &filename, Permissions permissionSpec);

protected:
#ifdef QT_NO_QOBJECT
    QFile(QFilePrivate &dd);
#else
    QFile(QFilePrivate &dd, QObject *parent = Q_NULLPTR);
#endif

private:
    friend class QTemporaryFile;
    Q_DISABLE_COPY(QFile)
};

QT_END_NAMESPACE

#endif // QFILE_H
