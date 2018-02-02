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

#ifndef QTEMPORARYFILE_P_H
#define QTEMPORARYFILE_P_H

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

#include <QtCore/qglobal.h>

#ifndef QT_NO_TEMPORARYFILE

#include "private/qfsfileengine_p.h"
#include "private/qfilesystemengine_p.h"
#include "private/qfile_p.h"

QT_BEGIN_NAMESPACE

class QTemporaryFilePrivate : public QFilePrivate
{
    Q_DECLARE_PUBLIC(QTemporaryFile)

protected:
    QTemporaryFilePrivate();
    explicit QTemporaryFilePrivate(const QString &templateNameIn);
    ~QTemporaryFilePrivate();

    QAbstractFileEngine *engine() const override;
    void resetFileEngine() const;

    bool autoRemove = true;
    QString templateName = defaultTemplateName();

    static QString defaultTemplateName();

    friend class QLockFilePrivate;
};

class QTemporaryFileEngine : public QFSFileEngine
{
    Q_DECLARE_PRIVATE(QFSFileEngine)
public:
    void initialize(const QString &file, quint32 mode, bool nameIsTemplate = true)
    {
        Q_D(QFSFileEngine);
        Q_ASSERT(!isReallyOpen());
        fileMode = mode;
        filePathIsTemplate = filePathWasTemplate = nameIsTemplate;
        d->fileEntry = QFileSystemEntry(file);

        if (!filePathIsTemplate)
            QFSFileEngine::setFileName(file);
    }
    ~QTemporaryFileEngine();

    bool isReallyOpen() const;
    void setFileName(const QString &file) override;
    void setFileTemplate(const QString &fileTemplate);

    bool open(QIODevice::OpenMode flags) override;
    bool remove() override;
    bool rename(const QString &newName) override;
    bool renameOverwrite(const QString &newName) override;
    bool close() override;

    quint32 fileMode;
    bool filePathIsTemplate;
    bool filePathWasTemplate;
};

QT_END_NAMESPACE

#endif // QT_NO_TEMPORARYFILE

#endif /* QTEMPORARYFILE_P_H */

