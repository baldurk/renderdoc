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

#ifndef QRINGBUFFER_P_H
#define QRINGBUFFER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/private/qglobal_p.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>

QT_BEGIN_NAMESPACE

#ifndef QRINGBUFFER_CHUNKSIZE
#define QRINGBUFFER_CHUNKSIZE 4096
#endif

class QRingBuffer
{
public:
    explicit inline QRingBuffer(int growth = QRINGBUFFER_CHUNKSIZE) :
        head(0), tail(0), tailBuffer(0), basicBlockSize(growth), bufferSize(0) { }

    inline void setChunkSize(int size) {
        basicBlockSize = size;
    }

    inline int chunkSize() const {
        return basicBlockSize;
    }

    inline qint64 nextDataBlockSize() const {
        return (tailBuffer == 0 ? tail : buffers.first().size()) - head;
    }

    inline const char *readPointer() const {
        return bufferSize == 0 ? Q_NULLPTR : (buffers.first().constData() + head);
    }

    Q_CORE_EXPORT const char *readPointerAtPosition(qint64 pos, qint64 &length) const;
    Q_CORE_EXPORT void free(qint64 bytes);
    Q_CORE_EXPORT char *reserve(qint64 bytes);
    Q_CORE_EXPORT char *reserveFront(qint64 bytes);

    inline void truncate(qint64 pos) {
        if (pos < size())
            chop(size() - pos);
    }

    Q_CORE_EXPORT void chop(qint64 bytes);

    inline bool isEmpty() const {
        return bufferSize == 0;
    }

    inline int getChar() {
        if (isEmpty())
            return -1;
        char c = *readPointer();
        free(1);
        return int(uchar(c));
    }

    inline void putChar(char c) {
        char *ptr = reserve(1);
        *ptr = c;
    }

    void ungetChar(char c)
    {
        if (head > 0) {
            --head;
            buffers.first()[head] = c;
            ++bufferSize;
        } else {
            char *ptr = reserveFront(1);
            *ptr = c;
        }
    }


    inline qint64 size() const {
        return bufferSize;
    }

    Q_CORE_EXPORT void clear();
    inline qint64 indexOf(char c) const { return indexOf(c, size()); }
    Q_CORE_EXPORT qint64 indexOf(char c, qint64 maxLength, qint64 pos = 0) const;
    Q_CORE_EXPORT qint64 read(char *data, qint64 maxLength);
    Q_CORE_EXPORT QByteArray read();
    Q_CORE_EXPORT qint64 peek(char *data, qint64 maxLength, qint64 pos = 0) const;
    Q_CORE_EXPORT void append(const char *data, qint64 size);
    Q_CORE_EXPORT void append(const QByteArray &qba);

    inline qint64 skip(qint64 length) {
        qint64 bytesToSkip = qMin(length, bufferSize);

        free(bytesToSkip);
        return bytesToSkip;
    }

    Q_CORE_EXPORT qint64 readLine(char *data, qint64 maxLength);

    inline bool canReadLine() const {
        return indexOf('\n') >= 0;
    }

private:
    QList<QByteArray> buffers;
    int head, tail;
    int tailBuffer; // always buffers.size() - 1
    int basicBlockSize;
    qint64 bufferSize;
};

QT_END_NAMESPACE

#endif // QRINGBUFFER_P_H
