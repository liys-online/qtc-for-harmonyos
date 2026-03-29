// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QAbstractSocket>
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringDecoder>
#include <QStringList>

#include <functional>

QT_BEGIN_NAMESPACE
class QTcpSocket;
QT_END_NAMESPACE

namespace Ohos::Internal {

/*
** 单次阻塞设备 shell 调用的返回结果（hdc 守护进程，P2-15 阶段一）。
*/
struct HdcShellSyncResult {
    enum class Code {
        Ok,
        Timeout,
        ConnectionFailed,
        HandshakeFailed,
        BadFrame,
        SocketError,
        /* ** hdc.exe 因 socket 跳过/失败/被拒而运行，且退出错误或 I/O 失败。 */
        CliFailed
    };
    Code code = Code::SocketError;
    QString errorMessage;
    /* ** 所有已接收帧的 UTF-8 解码负载（超时时可能不完整）。 */
    QString standardOutput;

    bool isOk() const { return code == Code::Ok; }
};

/*
** 通过 TCP（默认端口 8710）直接连接本地 hdc 守护进程，
** 并使用 hdc 二进制协议流式传输命令输出。
**
** 协议（逆向工程自 DevEco Studio hdclib）：
**   1. 连接 → 读取 48 字节握手包（验证“OHOS HDC” magic 字符串）
**   2. 发送包含设备序列号的 48 字节头部包
**   3. 发送带长度前缀的命令（如 "shell hilog -P <PID>"）
**   4. 读取连续数据帧：[4 字节大端 size][UTF-8 负载]
**
** 使用带 TCP_NODELAY 的直接 socket 可消除派生 hdc.exe 子进程时
** 存在的主机端管道缓冲延迟。
*/
class HdcSocketClient : public QObject
{
    Q_OBJECT
public:
    explicit HdcSocketClient(QObject *parent = nullptr);
    ~HdcSocketClient() override;

    void start(const QString &serial, const QString &command);
    void stop();
    bool isRunning() const;

    /*
    ** 单次阻塞请求/响应，使用新 TCP 连接。运行本地 QEventLoop，
    ** 直到守护进程关闭 socket（shell 正常结束）、超时或错误。
    ** daemonCommand 为完整的 hdc 线吐命令，同需传入 start()，如 "shell param get…"。
    ** 不得用于长运行流（如 hilog）；那种情况请使用 start()。
    */
    static HdcShellSyncResult runShellSync(const QString &deviceSerial,
                                           const QString &daemonCommand,
                                           int timeoutMs = 30000);

    /* ** QTC_HARMONY_HDC_USE_CLI — 语义同插件中的 harmonyHdcShellPreferCli()。 */
    static bool preferCliFromEnvironment();

    /**
     * Unless preferCliFromEnvironment(): run TCP @c runShellSync; if that fails, or @p rejectSocketOk
     * returns @c true for an otherwise OK socket result, fall back to @c hdcProgram with @p cliArgs.
     * If preferCliFromEnvironment(): run @c hdc.exe only.
     * @p socketFallbackNotifier is invoked (e.g. UI log) when falling back after socket failure or
     * rejection; not called when CLI-only mode.
     */
    static HdcShellSyncResult runSyncWithCliFallback(
        const QString &deviceSerial,
        const QString &daemonCommand,
        const QString &hdcProgram,
        const QStringList &cliArgs,
        int timeoutMs,
        const std::function<void(const QString &message)> &socketFallbackNotifier = {},
        const std::function<bool(const HdcShellSyncResult &socketResult)> &rejectSocketOk = {});

signals:
    void lineReceived(const QString &line);
    void started();
    void errorOccurred(const QString &message);
    void finished();

private:
    /* ** enum class：Windows 头文件中定义了名为 Idle 的宏（winbase.h）。 */
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
