// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "hdcsocketclient.h"

#include <QEventLoop>
#include <QLoggingCategory>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>

#include <cstring>

Q_LOGGING_CATEGORY(hdcSocketLog, "qtc.harmony.hdcsocket", QtWarningMsg)

namespace Ohos::Internal {

static constexpr int kDefaultPort = 8710;

HdcSocketClient::HdcSocketClient(QObject *parent)
    : QObject(parent)
{}

HdcSocketClient::~HdcSocketClient()
{
    stop();
}

// ---------------------------------------------------------------------------
// Port
// ---------------------------------------------------------------------------
int HdcSocketClient::serverPort()
{
    bool ok = false;
    const int port = qEnvironmentVariableIntValue("OHOS_HDC_SERVER_PORT", &ok);
    return (ok && port > 0 && port <= 65535) ? port : kDefaultPort;
}

// ---------------------------------------------------------------------------
// Packet builders  (match ChannelHandShakeHelp from DevEco hdclib)
// ---------------------------------------------------------------------------

// 48-byte head: [0-3] length byte at [3], [4-11] "OHOS HDC", [16+] serial
QByteArray HdcSocketClient::buildHeadPacket(const QString &serial)
{
    QByteArray pkt(kHandshakeSize, '\0');
    pkt[3] = char(kHandshakeSize - 4);
    std::memcpy(pkt.data() + 4, "OHOS HDC", 8);
    const QByteArray s = serial.toUtf8();
    const int n = qMin<int>(s.size(), kHandshakeSize - 16);
    if (n > 0)
        std::memcpy(pkt.data() + 16, s.constData(), n);
    return pkt;
}

// [4-byte BE size = len+1] [command UTF-8] [0x00]
QByteArray HdcSocketClient::buildCommandPacket(const QString &command)
{
    const QByteArray cmd = command.toUtf8();
    const quint32 payload = quint32(cmd.size() + 1);
    QByteArray pkt(int(kFrameHeaderSize + payload), '\0');
    const quint32 be = qToBigEndian(payload);
    std::memcpy(pkt.data(), &be, kFrameHeaderSize);
    std::memcpy(pkt.data() + kFrameHeaderSize, cmd.constData(), cmd.size());
    return pkt;
}

// "OHOS" at [4..7], "HDC" at [9..11]
bool HdcSocketClient::verifyHandshakeResponse(const QByteArray &r)
{
    if (r.size() < kHandshakeSize)
        return false;
    return r[4] == 'O' && r[5] == 'H' && r[6] == 'O' && r[7] == 'S'
        && r[9] == 'H' && r[10] == 'D' && r[11] == 'C';
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void HdcSocketClient::start(const QString &serial, const QString &command)
{
    if (m_phase != Phase::Idle) {
        qCWarning(hdcSocketLog) << "HdcSocketClient::start called while already active";
        return;
    }

    m_serial = serial;
    m_command = command;
    m_readBuf.clear();
    m_lineRemainder.clear();
    m_pendingPayload = -1;
    m_utf8 = QStringDecoder(QStringDecoder::Utf8);

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected,    this, &HdcSocketClient::onConnected);
    connect(m_socket, &QTcpSocket::readyRead,    this, &HdcSocketClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &HdcSocketClient::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,this, &HdcSocketClient::onSocketError);

    m_phase = Phase::Connecting;
    m_socket->connectToHost(QStringLiteral("127.0.0.1"), quint16(serverPort()));
}

void HdcSocketClient::stop()
{
    if (m_phase == Phase::Idle || !m_socket)
        return;
    m_phase = Phase::Idle;
    m_socket->disconnect(this);
    m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
    emit finished();
}

bool HdcSocketClient::isRunning() const
{
    return m_phase == Phase::Streaming;
}

// ---------------------------------------------------------------------------
// Socket callbacks
// ---------------------------------------------------------------------------
void HdcSocketClient::onConnected()
{
    qCDebug(hdcSocketLog) << "HdcSocketClient: connected to hdc daemon on port" << serverPort();
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_phase = Phase::WaitHandshake;
}

void HdcSocketClient::onReadyRead()
{
    if (!m_socket)
        return;
    m_readBuf.append(m_socket->readAll());

    if (m_phase == Phase::WaitHandshake && !processHandshake())
        return;
    if (m_phase == Phase::Streaming)
        processStreamFrames();
}

void HdcSocketClient::onDisconnected()
{
    qCDebug(hdcSocketLog) << "HdcSocketClient: disconnected";
    if (!m_lineRemainder.isEmpty()) {
        emit lineReceived(m_lineRemainder);
        m_lineRemainder.clear();
    }
    if (m_phase != Phase::Idle) {
        m_phase = Phase::Idle;
        emit finished();
    }
}

void HdcSocketClient::onSocketError(QAbstractSocket::SocketError error)
{
    if (m_phase == Phase::Idle)
        return;
    const QString msg = m_socket ? m_socket->errorString() : tr("Unknown socket error");
    qCWarning(hdcSocketLog) << "HdcSocketClient: socket error" << error << msg;
    emit errorOccurred(msg);
    if (error != QAbstractSocket::RemoteHostClosedError)
        stop();
}

// ---------------------------------------------------------------------------
// Protocol state machine
// ---------------------------------------------------------------------------
bool HdcSocketClient::processHandshake()
{
    if (m_readBuf.size() < kHandshakeSize)
        return false;

    const QByteArray hs = m_readBuf.left(kHandshakeSize);
    m_readBuf.remove(0, kHandshakeSize);

    if (!verifyHandshakeResponse(hs)) {
        const QString msg = tr("hdc daemon handshake verification failed (expected OHOS HDC).");
        qCWarning(hdcSocketLog) << msg;
        emit errorOccurred(msg);
        stop();
        return false;
    }

    sendHeadAndCommand();
    m_phase = Phase::Streaming;
    emit started();
    return true;
}

void HdcSocketClient::sendHeadAndCommand()
{
    Q_ASSERT(m_socket);
    m_socket->write(buildHeadPacket(m_serial));
    m_socket->write(buildCommandPacket(m_command));
    m_socket->flush();
}

void HdcSocketClient::processStreamFrames()
{
    for (;;) {
        if (m_pendingPayload < 0) {
            if (m_readBuf.size() < kFrameHeaderSize)
                break;
            quint32 raw;
            std::memcpy(&raw, m_readBuf.constData(), kFrameHeaderSize);
            m_pendingPayload = qint32(qFromBigEndian(raw));
            m_readBuf.remove(0, kFrameHeaderSize);

            if (m_pendingPayload == 0) {
                m_pendingPayload = -1;
                continue;
            }
            if (m_pendingPayload < 0 || m_pendingPayload > kMaxFramePayload) {
                qCWarning(hdcSocketLog) << "HdcSocketClient: bad frame size" << m_pendingPayload;
                m_pendingPayload = -1;
                stop();
                return;
            }
        }

        if (m_readBuf.size() < m_pendingPayload)
            break;

        const QByteArray payload = m_readBuf.left(m_pendingPayload);
        m_readBuf.remove(0, m_pendingPayload);
        m_pendingPayload = -1;

        emitLines(payload);
    }
}

void HdcSocketClient::emitLines(const QByteArray &payload)
{
    m_lineRemainder.append(m_utf8(payload));

    int start = 0;
    for (;;) {
        const int nl = m_lineRemainder.indexOf(u'\n', start);
        if (nl < 0)
            break;
        const QStringView line = QStringView(m_lineRemainder).mid(start, nl - start);
        if (!line.isEmpty())
            emit lineReceived(line.toString());
        start = nl + 1;
    }
    if (start > 0)
        m_lineRemainder.remove(0, start);
}

// ---------------------------------------------------------------------------
// Blocking sync shell (P2-15 phase 1) — separate connection from streaming @c start().
// ---------------------------------------------------------------------------
HdcShellSyncResult HdcSocketClient::runShellSync(const QString &serial,
                                                 const QString &command,
                                                 int timeoutMs)
{
    HdcShellSyncResult result;
    if (timeoutMs <= 0) {
        result.code = HdcShellSyncResult::Code::SocketError;
        result.errorMessage = tr("hdc shell sync: invalid timeout");
        return result;
    }

    QEventLoop loop;
    QTcpSocket socket;
    QTimer timer;
    timer.setSingleShot(true);

    QByteArray readBuf;
    QByteArray outputUtf8;
    qint32 pendingPayload = -1;
    bool commandSent = false;
    bool quitScheduled = false;

    auto quitOnce = [&quitScheduled, &loop] {
        if (quitScheduled)
            return;
        quitScheduled = true;
        loop.quit();
    };

    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        if (quitScheduled)
            return;
        result.code = HdcShellSyncResult::Code::Timeout;
        result.errorMessage = tr("hdc shell sync: timeout after %1 ms").arg(timeoutMs);
        result.standardOutput = QString::fromUtf8(outputUtf8);
        socket.abort();
        quitOnce();
    });

    QObject::connect(&socket, &QTcpSocket::connected, &loop, [&socket] {
        socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    });

    QObject::connect(&socket, &QTcpSocket::readyRead, &loop, [&] {
        readBuf.append(socket.readAll());

        if (!commandSent) {
            if (readBuf.size() < kHandshakeSize)
                return;
            const QByteArray hs = readBuf.left(kHandshakeSize);
            readBuf.remove(0, kHandshakeSize);
            if (!verifyHandshakeResponse(hs)) {
                result.code = HdcShellSyncResult::Code::HandshakeFailed;
                result.errorMessage =
                    tr("hdc daemon handshake verification failed (expected OHOS HDC).");
                qCWarning(hdcSocketLog) << result.errorMessage;
                socket.abort();
                quitOnce();
                return;
            }
            socket.write(buildHeadPacket(serial));
            socket.write(buildCommandPacket(command));
            socket.flush();
            commandSent = true;
        }

        for (;;) {
            if (pendingPayload < 0) {
                if (readBuf.size() < kFrameHeaderSize)
                    return;
                quint32 raw = 0;
                std::memcpy(&raw, readBuf.constData(), kFrameHeaderSize);
                pendingPayload = qint32(qFromBigEndian(raw));
                readBuf.remove(0, kFrameHeaderSize);
                if (pendingPayload == 0) {
                    pendingPayload = -1;
                    continue;
                }
                if (pendingPayload < 0 || pendingPayload > kMaxFramePayload) {
                    result.code = HdcShellSyncResult::Code::BadFrame;
                    result.errorMessage = tr("hdc shell sync: invalid frame size (%1).").arg(pendingPayload);
                    result.standardOutput = QString::fromUtf8(outputUtf8);
                    qCWarning(hdcSocketLog) << result.errorMessage;
                    socket.abort();
                    quitOnce();
                    return;
                }
            }
            if (readBuf.size() < pendingPayload)
                return;
            const QByteArray payload = readBuf.left(pendingPayload);
            readBuf.remove(0, pendingPayload);
            pendingPayload = -1;
            outputUtf8 += payload;
        }
    });

    QObject::connect(&socket, &QTcpSocket::disconnected, &loop, [&] {
        if (quitScheduled)
            return;
        if (!commandSent) {
            result.code = HdcShellSyncResult::Code::ConnectionFailed;
            if (result.errorMessage.isEmpty()) {
                result.errorMessage =
                    tr("Connection closed before hdc handshake completed (%1).").arg(socket.errorString());
            }
            quitOnce();
            return;
        }
        result.code = HdcShellSyncResult::Code::Ok;
        result.standardOutput = QString::fromUtf8(outputUtf8);
        quitOnce();
    });

    QObject::connect(&socket, &QTcpSocket::errorOccurred, &loop,
                     [&](QAbstractSocket::SocketError err) {
                         if (quitScheduled)
                             return;
                         if (err == QAbstractSocket::RemoteHostClosedError) {
                             if (commandSent) {
                                 result.code = HdcShellSyncResult::Code::Ok;
                                 result.standardOutput = QString::fromUtf8(outputUtf8);
                             } else {
                                 result.code = HdcShellSyncResult::Code::ConnectionFailed;
                                 result.errorMessage =
                                     tr("hdc daemon closed the connection before handshake completed.");
                             }
                             quitOnce();
                             return;
                         }
                         if (err == QAbstractSocket::ConnectionRefusedError)
                             result.code = HdcShellSyncResult::Code::ConnectionFailed;
                         else
                             result.code = HdcShellSyncResult::Code::SocketError;
                         result.errorMessage = socket.errorString();
                         result.standardOutput = QString::fromUtf8(outputUtf8);
                         qCWarning(hdcSocketLog)
                             << "HdcSocketClient::runShellSync socket error" << err << result.errorMessage;
                         quitOnce();
                     });

    timer.start(timeoutMs);
    socket.connectToHost(QStringLiteral("127.0.0.1"), quint16(serverPort()));
    loop.exec();
    timer.stop();

    if (!quitScheduled) {
        result.code = HdcShellSyncResult::Code::SocketError;
        result.errorMessage = tr("hdc shell sync: event loop exited unexpectedly.");
        result.standardOutput = QString::fromUtf8(outputUtf8);
    }

    return result;
}

} // namespace Ohos::Internal
