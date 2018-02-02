/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#ifndef QHTTP2PROTOCOLHANDLER_P_H
#define QHTTP2PROTOCOLHANDLER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the Network Access API.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <private/qhttpnetworkconnectionchannel_p.h>
#include <private/qabstractprotocolhandler_p.h>
#include <private/qhttpnetworkrequest_p.h>

#if !defined(QT_NO_HTTP)

#include <private/http2protocol_p.h>
#include <private/http2streams_p.h>
#include <private/http2frames_p.h>
#include <private/hpacktable_p.h>
#include <private/hpack_p.h>

#include <QtCore/qnamespace.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qflags.h>
#include <QtCore/qhash.h>

#include <vector>
#include <limits>
#include <deque>
#include <set>

QT_BEGIN_NAMESPACE

class QHttp2ProtocolHandler : public QObject, public QAbstractProtocolHandler
{
    Q_OBJECT

public:
    QHttp2ProtocolHandler(QHttpNetworkConnectionChannel *channel);

    QHttp2ProtocolHandler(const QHttp2ProtocolHandler &rhs) = delete;
    QHttp2ProtocolHandler(QHttp2ProtocolHandler &&rhs) = delete;

    QHttp2ProtocolHandler &operator = (const QHttp2ProtocolHandler &rhs) = delete;
    QHttp2ProtocolHandler &operator = (QHttp2ProtocolHandler &&rhs) = delete;

private slots:
    void _q_uploadDataReadyRead();
    void _q_replyDestroyed(QObject* reply);

private:
    using Stream = Http2::Stream;

    void _q_readyRead() override;
    void _q_receiveReply() override;
    Q_INVOKABLE bool sendRequest() override;

    bool sendClientPreface();
    bool sendSETTINGS_ACK();
    bool sendHEADERS(Stream &stream);
    bool sendDATA(Stream &stream);
    Q_INVOKABLE bool sendWINDOW_UPDATE(quint32 streamID, quint32 delta);
    bool sendRST_STREAM(quint32 streamID, quint32 errorCoder);
    bool sendGOAWAY(quint32 errorCode);

    void handleDATA();
    void handleHEADERS();
    void handlePRIORITY();
    void handleRST_STREAM();
    void handleSETTINGS();
    void handlePUSH_PROMISE();
    void handlePING();
    void handleGOAWAY();
    void handleWINDOW_UPDATE();
    void handleCONTINUATION();

    void handleContinuedHEADERS();

    bool acceptSetting(Http2::Settings identifier, quint32 newValue);

    void updateStream(Stream &stream, const HPack::HttpHeader &headers,
                      Qt::ConnectionType connectionType = Qt::DirectConnection);
    void updateStream(Stream &stream, const Http2::Frame &dataFrame,
                      Qt::ConnectionType connectionType = Qt::DirectConnection);
    void finishStream(Stream &stream, Qt::ConnectionType connectionType = Qt::DirectConnection);
    // Error code send by a peer (GOAWAY/RST_STREAM):
    void finishStreamWithError(Stream &stream, quint32 errorCode);
    // Locally encountered error:
    void finishStreamWithError(Stream &stream, QNetworkReply::NetworkError error,
                               const QString &message);

    // Stream's lifecycle management:
    quint32 createNewStream(const HttpMessagePair &message);
    void addToSuspended(Stream &stream);
    void markAsReset(quint32 streamID);
    quint32 popStreamToResume();
    void removeFromSuspended(quint32 streamID);
    void deleteActiveStream(quint32 streamID);
    bool streamWasReset(quint32 streamID) const;

    bool prefaceSent = false;
    // In the current implementation we send
    // SETTINGS only once, immediately after
    // the client's preface 24-byte message.
    bool waitingForSettingsACK = false;

    static const quint32 maxAcceptableTableSize = 16 * HPack::FieldLookupTable::DefaultSize;
    // HTTP/2 4.3: Header compression is stateful. One compression context and
    // one decompression context are used for the entire connection.
    HPack::Decoder decoder;
    HPack::Encoder encoder;

    QHash<quint32, Stream> activeStreams;
    std::deque<quint32> suspendedStreams[3]; // 3 for priorities: High, Normal, Low.
    static const std::deque<quint32>::size_type maxRecycledStreams;
    std::deque<quint32> recycledStreams;

    // Peer's max frame size.
    quint32 maxFrameSize = Http2::maxFrameSize;

    Http2::FrameReader frameReader;
    Http2::Frame inboundFrame;
    Http2::FrameWriter frameWriter;
    // Temporary storage to assemble HEADERS' block
    // from several CONTINUATION frames ...
    bool continuationExpected = false;
    std::vector<Http2::Frame> continuedFrames;

    // Peer's max number of streams ...
    quint32 maxConcurrentStreams = Http2::maxConcurrentStreams;

    // Control flow:
    static const qint32 sessionMaxRecvWindowSize = Http2::defaultSessionWindowSize * 10;
    // Signed integer, it can become negative (it's still a valid window size):
    qint32 sessionRecvWindowSize = sessionMaxRecvWindowSize;

    // We do not negotiate this window size
    // We have to send WINDOW_UPDATE frames to our peer also.
    static const qint32 streamInitialRecvWindowSize = Http2::defaultSessionWindowSize;

    // Updated by SETTINGS and WINDOW_UPDATE.
    qint32 sessionSendWindowSize = Http2::defaultSessionWindowSize;
    qint32 streamInitialSendWindowSize = Http2::defaultSessionWindowSize;

    // It's unlimited by default, but can be changed via SETTINGS.
    quint32 maxHeaderListSize = (std::numeric_limits<quint32>::max)();

    Q_INVOKABLE void resumeSuspendedStreams();
    // Our stream IDs (all odd), the first valid will be 1.
    quint32 nextID = 1;
    quint32 allocateStreamID();
    bool validPeerStreamID() const;
    bool goingAway = false;
    bool pushPromiseEnabled = false;
    quint32 lastPromisedID = Http2::connectionStreamID;
    QHash<QString, Http2::PushPromise> promisedData;
    bool tryReserveStream(const Http2::Frame &pushPromiseFrame,
                          const HPack::HttpHeader &requestHeader);
    void resetPromisedStream(const Http2::Frame &pushPromiseFrame,
                             Http2::Http2Error reason);
    void initReplyFromPushPromise(const HttpMessagePair &message,
                                  const QString &cacheKey);
    // Errors:
    void connectionError(Http2::Http2Error errorCode,
                         const char *message);
    void closeSession();
};

QT_END_NAMESPACE

#endif // !defined(QT_NO_HTTP)

#endif
