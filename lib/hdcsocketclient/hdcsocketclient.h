// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QAbstractSocket>
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringDecoder>

QT_BEGIN_NAMESPACE
class QTcpSocket;
QT_END_NAMESPACE

namespace Ohos::Internal {

// Result of a blocking one-shot shell over the hdc daemon (P2-15 phase 1).
struct HdcShellSyncResult {
    enum class Code {
        Ok,
        Timeout,
        ConnectionFailed,
        HandshakeFailed,
        BadFrame,
        SocketError
    };
    Code code = Code::SocketError;
    QString errorMessage;
    /// Decoded UTF-8 payload from all received frames (may be partial if Timeout).
    QString standardOutput;

    bool isOk() const { return code == Code::Ok; }
};

// Connects directly to the local hdc daemon via TCP (default port 8710)
// and streams command output using the hdc binary protocol.
//
// Protocol (reverse-engineered from DevEco Studio's hdclib):
//   1. Connect → read 48-byte handshake (verify "OHOS HDC" magic)
//   2. Send 48-byte head containing the device serial
//   3. Send length-prefixed command (e.g. "shell hilog -P <PID>")
//   4. Read continuous data frames: [4-byte big-endian size][UTF-8 payload]
//
// Using a direct socket with TCP_NODELAY eliminates host-side pipe
// buffering that occurs when spawning hdc.exe as a subprocess.
class HdcSocketClient : public QObject
{
    Q_OBJECT
public:
    explicit HdcSocketClient(QObject *parent = nullptr);
    ~HdcSocketClient() override;

    void start(const QString &serial, const QString &command);
    void stop();
    bool isRunning() const;

    // Blocking request/response over a fresh TCP connection. Runs a local QEventLoop until
    // the daemon closes the socket (normal end of shell), timeout, or an error.
    // @p daemonCommand is the full hdc wire command, same as @c start(), e.g. @c "shell param get …".
    // Do not use for long-running streams (e.g. hilog); use @c start() instead.
    static HdcShellSyncResult runShellSync(const QString &deviceSerial,
                                           const QString &daemonCommand,
                                           int timeoutMs = 30000);

signals:
    void lineReceived(const QString &line);
    void started();
    void errorOccurred(const QString &message);
    void finished();

private:
    // enum class: Windows headers define a macro named Idle (winbase.h).
    enum class Phase { Idle, Connecting, WaitHandshake, Streaming };

    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

    bool processHandshake();
    void sendHeadAndCommand();
    void processStreamFrames();
    void emitLines(const QByteArray &payload);

    static int serverPort();
    static QByteArray buildHeadPacket(const QString &serial);
    static QByteArray buildCommandPacket(const QString &command);
    static bool verifyHandshakeResponse(const QByteArray &response);

    static constexpr int kHandshakeSize = 48;
    static constexpr int kFrameHeaderSize = 4;
    static constexpr int kMaxFramePayload = 524288; // 512 KB

    QTcpSocket *m_socket = nullptr;
    Phase m_phase = Phase::Idle;
    QString m_serial;
    QString m_command;
    QByteArray m_readBuf;
    QStringDecoder m_utf8{QStringDecoder::Utf8};
    QString m_lineRemainder;
    qint32 m_pendingPayload = -1;
};

} // namespace Ohos::Internal
