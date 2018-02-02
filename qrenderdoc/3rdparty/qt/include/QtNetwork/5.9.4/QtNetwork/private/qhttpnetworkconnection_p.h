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

#ifndef QHTTPNETWORKCONNECTION_H
#define QHTTPNETWORKCONNECTION_H

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

#include <QtNetwork/private/qtnetworkglobal_p.h>
#include <QtNetwork/qnetworkrequest.h>
#include <QtNetwork/qnetworkreply.h>
#include <QtNetwork/qabstractsocket.h>
#include <QtNetwork/qnetworksession.h>

#include <private/qobject_p.h>
#include <qauthenticator.h>
#include <qnetworkproxy.h>
#include <qbuffer.h>
#include <qtimer.h>
#include <qsharedpointer.h>

#include <private/qhttpnetworkheader_p.h>
#include <private/qhttpnetworkrequest_p.h>
#include <private/qhttpnetworkreply_p.h>

#include <private/qhttpnetworkconnectionchannel_p.h>

#ifndef QT_NO_HTTP

QT_BEGIN_NAMESPACE

class QHttpNetworkRequest;
class QHttpNetworkReply;
class QHttpThreadDelegate;
class QByteArray;
class QHostInfo;
#ifndef QT_NO_SSL
class QSslConfiguration;
class QSslContext;
#endif // !QT_NO_SSL

class QHttpNetworkConnectionPrivate;
class Q_AUTOTEST_EXPORT QHttpNetworkConnection : public QObject
{
    Q_OBJECT
public:

    enum ConnectionType {
        ConnectionTypeHTTP,
        ConnectionTypeSPDY,
        ConnectionTypeHTTP2
    };

#ifndef QT_NO_BEARERMANAGEMENT
    explicit QHttpNetworkConnection(const QString &hostName, quint16 port = 80, bool encrypt = false,
                                    ConnectionType connectionType = ConnectionTypeHTTP,
                                    QObject *parent = 0, QSharedPointer<QNetworkSession> networkSession
                                    = QSharedPointer<QNetworkSession>());
    QHttpNetworkConnection(quint16 channelCount, const QString &hostName, quint16 port = 80,
                           bool encrypt = false, QObject *parent = 0,
                           QSharedPointer<QNetworkSession> networkSession = QSharedPointer<QNetworkSession>(),
                           ConnectionType connectionType = ConnectionTypeHTTP);
#else
    explicit QHttpNetworkConnection(const QString &hostName, quint16 port = 80, bool encrypt = false,
                                    ConnectionType connectionType = ConnectionTypeHTTP,
                                    QObject *parent = 0);
    QHttpNetworkConnection(quint16 channelCount, const QString &hostName, quint16 port = 80,
                           bool encrypt = false, QObject *parent = 0,
                           ConnectionType connectionType = ConnectionTypeHTTP);
#endif
    ~QHttpNetworkConnection();

    //The hostname to which this is connected to.
    QString hostName() const;
    //The HTTP port in use.
    quint16 port() const;

    //add a new HTTP request through this connection
    QHttpNetworkReply* sendRequest(const QHttpNetworkRequest &request);

#ifndef QT_NO_NETWORKPROXY
    //set the proxy for this connection
    void setCacheProxy(const QNetworkProxy &networkProxy);
    QNetworkProxy cacheProxy() const;
    void setTransparentProxy(const QNetworkProxy &networkProxy);
    QNetworkProxy transparentProxy() const;
#endif

    bool isSsl() const;

    QHttpNetworkConnectionChannel *channels() const;

    ConnectionType connectionType();
    void setConnectionType(ConnectionType type);

#ifndef QT_NO_SSL
    void setSslConfiguration(const QSslConfiguration &config);
    void ignoreSslErrors(int channel = -1);
    void ignoreSslErrors(const QList<QSslError> &errors, int channel = -1);
    QSharedPointer<QSslContext> sslContext();
    void setSslContext(QSharedPointer<QSslContext> context);
#endif

    void preConnectFinished();

private:
    Q_DECLARE_PRIVATE(QHttpNetworkConnection)
    Q_DISABLE_COPY(QHttpNetworkConnection)
    friend class QHttpThreadDelegate;
    friend class QHttpNetworkReply;
    friend class QHttpNetworkReplyPrivate;
    friend class QHttpNetworkConnectionChannel;
    friend class QHttp2ProtocolHandler;
    friend class QHttpProtocolHandler;
    friend class QSpdyProtocolHandler;

    Q_PRIVATE_SLOT(d_func(), void _q_startNextRequest())
    Q_PRIVATE_SLOT(d_func(), void _q_hostLookupFinished(QHostInfo))
    Q_PRIVATE_SLOT(d_func(), void _q_connectDelayedChannel())
};


// private classes
typedef QPair<QHttpNetworkRequest, QHttpNetworkReply*> HttpMessagePair;


class QHttpNetworkConnectionPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QHttpNetworkConnection)
public:
    static const int defaultHttpChannelCount;
    static const int defaultPipelineLength;
    static const int defaultRePipelineLength;

    enum ConnectionState {
        RunningState = 0,
        PausedState = 1
    };

    enum NetworkLayerPreferenceState {
        Unknown,
        HostLookupPending,
        IPv4,
        IPv6,
        IPv4or6
    };

    QHttpNetworkConnectionPrivate(const QString &hostName, quint16 port, bool encrypt,
                                  QHttpNetworkConnection::ConnectionType type);
    QHttpNetworkConnectionPrivate(quint16 channelCount, const QString &hostName, quint16 port, bool encrypt,
                                  QHttpNetworkConnection::ConnectionType type);
    ~QHttpNetworkConnectionPrivate();
    void init();

    void pauseConnection();
    void resumeConnection();
    ConnectionState state;
    NetworkLayerPreferenceState networkLayerState;

    enum { ChunkSize = 4096 };

    int indexOf(QAbstractSocket *socket) const;

    QHttpNetworkReply *queueRequest(const QHttpNetworkRequest &request);
    void requeueRequest(const HttpMessagePair &pair); // e.g. after pipeline broke
    bool dequeueRequest(QAbstractSocket *socket);
    void prepareRequest(HttpMessagePair &request);
    void updateChannel(int i, const HttpMessagePair &messagePair);
    QHttpNetworkRequest predictNextRequest() const;

    void fillPipeline(QAbstractSocket *socket);
    bool fillPipeline(QList<HttpMessagePair> &queue, QHttpNetworkConnectionChannel &channel);

    // read more HTTP body after the next event loop spin
    void readMoreLater(QHttpNetworkReply *reply);

    void copyCredentials(int fromChannel, QAuthenticator *auth, bool isProxy);

    void startHostInfoLookup();
    void startNetworkLayerStateLookup();
    void networkLayerDetected(QAbstractSocket::NetworkLayerProtocol protocol);

    // private slots
    void _q_startNextRequest(); // send the next request from the queue

    void _q_hostLookupFinished(const QHostInfo &info);
    void _q_connectDelayedChannel();

    void createAuthorization(QAbstractSocket *socket, QHttpNetworkRequest &request);

    QString errorDetail(QNetworkReply::NetworkError errorCode, QAbstractSocket *socket,
                        const QString &extraDetail = QString());

    void removeReply(QHttpNetworkReply *reply);

    QString hostName;
    quint16 port;
    bool encrypt;
    bool delayIpv4;

    // Number of channels we are trying to use at the moment:
    int activeChannelCount;
    // The total number of channels we reserved:
    const int channelCount;
    QTimer delayedConnectionTimer;
    QHttpNetworkConnectionChannel *channels; // parallel connections to the server
    bool shouldEmitChannelError(QAbstractSocket *socket);

    qint64 uncompressedBytesAvailable(const QHttpNetworkReply &reply) const;
    qint64 uncompressedBytesAvailableNextBlock(const QHttpNetworkReply &reply) const;


    void emitReplyError(QAbstractSocket *socket, QHttpNetworkReply *reply, QNetworkReply::NetworkError errorCode);
    bool handleAuthenticateChallenge(QAbstractSocket *socket, QHttpNetworkReply *reply, bool isProxy, bool &resend);
    QUrl parseRedirectResponse(QAbstractSocket *socket, QHttpNetworkReply *reply);

#ifndef QT_NO_NETWORKPROXY
    QNetworkProxy networkProxy;
    void emitProxyAuthenticationRequired(const QHttpNetworkConnectionChannel *chan, const QNetworkProxy &proxy, QAuthenticator* auth);
#endif

    //The request queues
    QList<HttpMessagePair> highPriorityQueue;
    QList<HttpMessagePair> lowPriorityQueue;

    int preConnectRequests;

    QHttpNetworkConnection::ConnectionType connectionType;

#ifndef QT_NO_SSL
    QSharedPointer<QSslContext> sslContext;
#endif

#ifndef QT_NO_BEARERMANAGEMENT
    QSharedPointer<QNetworkSession> networkSession;
#endif

    friend class QHttpNetworkConnectionChannel;
};



QT_END_NAMESPACE

#endif // QT_NO_HTTP

#endif
