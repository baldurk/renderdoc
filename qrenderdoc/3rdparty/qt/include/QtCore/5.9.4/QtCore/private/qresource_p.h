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

#ifndef QRESOURCE_P_H
#define QRESOURCE_P_H

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

#include "qabstractfileengine_p.h"

QT_BEGIN_NAMESPACE

class QResourceFileEnginePrivate;
class QResourceFileEngine : public QAbstractFileEngine
{
private:
    Q_DECLARE_PRIVATE(QResourceFileEngine)
public:
    explicit QResourceFileEngine(const QString &path);
    ~QResourceFileEngine();

    virtual void setFileName(const QString &file) Q_DECL_OVERRIDE;

    virtual bool open(QIODevice::OpenMode flags) Q_DECL_OVERRIDE ;
    virtual bool close() Q_DECL_OVERRIDE;
    virtual bool flush() Q_DECL_OVERRIDE;
    virtual qint64 size() const Q_DECL_OVERRIDE;
    virtual qint64 pos() const Q_DECL_OVERRIDE;
    virtual bool atEnd() const;
    virtual bool seek(qint64) Q_DECL_OVERRIDE;
    virtual qint64 read(char *data, qint64 maxlen) Q_DECL_OVERRIDE;
    virtual qint64 write(const char *data, qint64 len) Q_DECL_OVERRIDE;

    virtual bool remove() Q_DECL_OVERRIDE;
    virtual bool copy(const QString &newName) Q_DECL_OVERRIDE;
    virtual bool rename(const QString &newName) Q_DECL_OVERRIDE;
    virtual bool link(const QString &newName) Q_DECL_OVERRIDE;

    virtual bool isSequential() const Q_DECL_OVERRIDE;

    virtual bool isRelativePath() const Q_DECL_OVERRIDE;

    virtual bool mkdir(const QString &dirName, bool createParentDirectories) const Q_DECL_OVERRIDE;
    virtual bool rmdir(const QString &dirName, bool recurseParentDirectories) const Q_DECL_OVERRIDE;

    virtual bool setSize(qint64 size) Q_DECL_OVERRIDE;

    virtual QStringList entryList(QDir::Filters filters, const QStringList &filterNames) const Q_DECL_OVERRIDE;

    virtual bool caseSensitive() const Q_DECL_OVERRIDE;

    virtual FileFlags fileFlags(FileFlags type) const Q_DECL_OVERRIDE;

    virtual bool setPermissions(uint perms) Q_DECL_OVERRIDE;

    virtual QString fileName(QAbstractFileEngine::FileName file) const Q_DECL_OVERRIDE;

    virtual uint ownerId(FileOwner) const Q_DECL_OVERRIDE;
    virtual QString owner(FileOwner) const Q_DECL_OVERRIDE;

    virtual QDateTime fileTime(FileTime time) const Q_DECL_OVERRIDE;

    virtual Iterator *beginEntryList(QDir::Filters filters, const QStringList &filterNames) Q_DECL_OVERRIDE;
    virtual Iterator *endEntryList() Q_DECL_OVERRIDE;

    bool extension(Extension extension, const ExtensionOption *option = 0, ExtensionReturn *output = 0) Q_DECL_OVERRIDE;
    bool supportsExtension(Extension extension) const Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif // QRESOURCE_P_H
