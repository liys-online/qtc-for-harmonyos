// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "arktsdebugbridge.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

Q_LOGGING_CATEGORY(arkLog, "qtc.harmony.arktsdebugbridge", QtWarningMsg)

/*
** 常量定义
*/

static constexpr int kL1MaxAttempts   = 60;
static constexpr int kL1RetryMs       = 500;
static constexpr int kL2MaxAttempts   = 20;
static constexpr int kL2RetryMs       = 300;
static constexpr int kSignalPollMs    = 100;
static constexpr int kSignalTimeoutMs = 120'000;
static constexpr int kAddInstanceWaitMs = 30'000;
static constexpr int kCdtIntervalMs   = 100;

/*
** 构造函数 / 析构函数
*/

ArkTSDebugBridge::ArkTSDebugBridge(QObject *parent)
    : QObject(parent)
    , m_l1Socket(std::make_unique<QWebSocket>())
    , m_l2Socket(std::make_unique<QWebSocket>())
    , m_retryTimer(std::make_unique<QTimer>())
    , m_signalTimer(std::make_unique<QTimer>())
    , m_deadlineTimer(std::make_unique<QTimer>())
    , m_cdtTimer(std::make_unique<QTimer>())
{
    m_retryTimer->setSingleShot(true);
    m_signalTimer->setInterval(kSignalPollMs);
    m_deadlineTimer->setSingleShot(true);
}

ArkTSDebugBridge::~ArkTSDebugBridge()
{
    cleanup(false);
}

/*
** 公共接口
*/

void ArkTSDebugBridge::start(quint16 connectPort, quint16 pandaPort, const QString &signalFile)
{
    if (m_state != State::Idle) {
        qCWarning(arkLog) << "ArkTSDebugBridge::start called while already running";
        return;
    }

    m_connectPort = connectPort;
    m_pandaPort   = pandaPort;
    m_signalFile  = signalFile;
    m_l1Attempts  = 0;
    m_l2Attempts  = 0;
    m_cdtMsgId    = 0;

    log(QStringLiteral("[BRIDGE] Starting: connectPort=%1 pandaPort=%2 signalFile=%3")
            .arg(connectPort).arg(pandaPort).arg(signalFile));

    m_state = State::ConnectingL1;
    attemptL1Connect();
}

void ArkTSDebugBridge::stop()
{
    if (m_state == State::Idle)
        return;
    log(QStringLiteral("[BRIDGE] stop() called"));
    cleanup(true);
}

bool ArkTSDebugBridge::isRunning() const
{
    return m_state != State::Idle;
}

/*
** 第一级协议：ConnectServer
*/

void ArkTSDebugBridge::attemptL1Connect()
{
    Q_ASSERT(m_state == State::ConnectingL1);

    if (m_l1Attempts >= kL1MaxAttempts) {
        fatalError(QStringLiteral("[BRIDGE] ERROR: could not connect to ConnectServer after %1 attempts")
                       .arg(kL1MaxAttempts));
        return;
    }

    ++m_l1Attempts;

    m_l1Socket->disconnect(this);
    m_l1Socket->abort();

    connect(m_l1Socket.get(), &QWebSocket::connected,
            this, &ArkTSDebugBridge::onL1Connected);
    connect(m_l1Socket.get(), &QWebSocket::textMessageReceived,
            this, &ArkTSDebugBridge::onL1TextMessageReceived);
    connect(m_l1Socket.get(), &QWebSocket::disconnected,
            this, &ArkTSDebugBridge::onL1Disconnected);
    connect(m_l1Socket.get(), &QWebSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError) { onL1Error(); });

    const auto url = QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(m_connectPort));
    qCDebug(arkLog) << "L1 connect attempt" << m_l1Attempts << url;
    m_l1Socket->open(url);
}

void ArkTSDebugBridge::onL1Connected()
{
    if (m_state != State::ConnectingL1)
        return;

    /* ** 停止重试定时器 */
    m_retryTimer->stop();
    m_retryTimer->disconnect(this);

    log(QStringLiteral("[BRIDGE] WebSocket connected, port=%1").arg(m_connectPort));
    m_state = State::WaitingSignal;
    startSignalWatch();
}

void ArkTSDebugBridge::onL1TextMessageReceived(const QString &message)
{
    log(QStringLiteral("[BRIDGE] ConnectServer: %1").arg(message.left(200)));

    if (m_state == State::WaitingAddInstance) {
        const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
        const QJsonObject   obj = doc.object();
        if (obj.value(QStringLiteral("type")).toString() == QStringLiteral("addInstance")) {
            const QString instanceId =
                obj.value(QStringLiteral("instanceId")).toString(QStringLiteral("0"));
            log(QStringLiteral("[BRIDGE] Got addInstance, instanceId=%1").arg(instanceId));

            /* ** 取消 addInstance 超时看门狗 */
            m_deadlineTimer->stop();
            m_deadlineTimer->disconnect(this);

            emit addInstanceReceived(instanceId);
            m_state = State::ConnectingL2;
            m_l2Attempts = 0;
            attemptL2Connect();
        }
    }
}

void ArkTSDebugBridge::onL1Disconnected()
{
    if (m_state == State::ConnectingL1) {
        /* ** 连接被拒绝，等待重试 */
        m_retryTimer->disconnect(this);
        connect(m_retryTimer.get(), &QTimer::timeout,
                this, &ArkTSDebugBridge::attemptL1Connect);
        if (m_l1Attempts == 1) {
            log(QStringLiteral("[BRIDGE] port=%1 connect attempt 1 failed, retrying...")
                    .arg(m_connectPort));
        }
        m_retryTimer->start(kL1RetryMs);
        return;
    }

    /* ** WaitingSignal / WaitingAddInstance 状态下 L1 断开 → 致命错误 */
    if (m_state == State::WaitingSignal || m_state == State::WaitingAddInstance) {
        fatalError(QStringLiteral("[BRIDGE] ERROR: L1 WebSocket disconnected while in state %1")
                       .arg(int(m_state)));
        return;
    }

    /* ** 非预期状态下的断开连接 */
    if (m_state != State::Idle) {
        qCWarning(arkLog) << "L1 WebSocket disconnected unexpectedly in state" << int(m_state);
    }
}

void ArkTSDebugBridge::onL1Error() const
{
    if (m_state == State::Idle)
        return;

    const QString errMsg = m_l1Socket->errorString();
    qCDebug(arkLog) << "L1 error:" << errMsg;

    if (m_state == State::ConnectingL1) {
        /*
        ** onL1Disconnected 将随后触发并安排重试
        */
        return;
    }
    qCWarning(arkLog) << "L1 unexpected error:" << errMsg;
}

/*
** 信号文件监控
*/

void ArkTSDebugBridge::startSignalWatch()
{
    log(QStringLiteral("[BRIDGE] Waiting for signal: %1").arg(m_signalFile));

    /* ** 每 100ms 轮询一次 */
    m_signalTimer->disconnect(this);
    connect(m_signalTimer.get(), &QTimer::timeout, this, &ArkTSDebugBridge::onSignalPollTick);
    m_signalTimer->start();

    /* ** 总超时 120s */
    m_deadlineTimer->disconnect(this);
    connect(m_deadlineTimer.get(), &QTimer::timeout, this, &ArkTSDebugBridge::onSignalTimeout);
    m_deadlineTimer->start(kSignalTimeoutMs);
}

void ArkTSDebugBridge::onSignalPollTick()
{
    if (m_state != State::WaitingSignal)
        return;

    if (!QFile::exists(m_signalFile))
        return;

    /* ** 信号文件已出现，停止轮询和看门狗定时器 */
    m_signalTimer->stop();
    m_signalTimer->disconnect(this);

    m_deadlineTimer->stop(); /* ** 下方复用此定时器作为 addInstance 超时 */

    /* ** 发送 {"type":"connected"}（第一级解锁）*/
    const auto connected = QStringLiteral("{\"type\":\"connected\"}");
    if (m_l1Socket->state() != QAbstractSocket::ConnectedState) {
        fatalError(QStringLiteral("[BRIDGE] ERROR sending connected: socket not ready"));
        return;
    }

    m_l1Socket->sendTextMessage(connected);
    log(QStringLiteral("[BRIDGE] Sent {\"type\":\"connected\"} successfully"));

    /* ** 切换状态并启动 addInstance 超时（30s）*/
    m_state = State::WaitingAddInstance;
    log(QStringLiteral("[BRIDGE] Waiting for addInstance response..."));

    m_deadlineTimer->setInterval(kAddInstanceWaitMs);
    m_deadlineTimer->setSingleShot(true);
    /* ** 复用同一定时器，重新绑定 addInstance 超时槽函数 */
    disconnect(m_deadlineTimer.get(), &QTimer::timeout, this, &ArkTSDebugBridge::onSignalTimeout);
    connect(m_deadlineTimer.get(), &QTimer::timeout, this, [this] {
        log(QStringLiteral("[BRIDGE] WARNING: no addInstance — trying Panda CDT anyway"));
        m_state = State::ConnectingL2;
        m_l2Attempts = 0;
        attemptL2Connect();
    });
    m_deadlineTimer->start();
}

void ArkTSDebugBridge::onSignalTimeout()
{
    fatalError(QStringLiteral("[BRIDGE] TIMEOUT: signal file never appeared: %1").arg(m_signalFile));
}

/*
** 第二级协议：Panda CDT
*/

void ArkTSDebugBridge::attemptL2Connect()
{
    Q_ASSERT(m_state == State::ConnectingL2);

    if (m_l2Attempts >= kL2MaxAttempts) {
        fatalError(QStringLiteral("[BRIDGE] ERROR: could not connect to Panda CDT after %1 attempts")
                       .arg(kL2MaxAttempts));
        return;
    }

    ++m_l2Attempts;

    m_l2Socket->disconnect(this);
    m_l2Socket->abort();

    log(QStringLiteral("[BRIDGE] Connecting to Panda CDT at port=%1... (attempt %2)")
            .arg(m_pandaPort).arg(m_l2Attempts));

    connect(m_l2Socket.get(), &QWebSocket::connected,
            this, &ArkTSDebugBridge::onL2Connected);
    connect(m_l2Socket.get(), &QWebSocket::textMessageReceived,
            this, &ArkTSDebugBridge::onL2TextMessageReceived);
    connect(m_l2Socket.get(), &QWebSocket::disconnected,
            this, &ArkTSDebugBridge::onL2Disconnected);
    connect(m_l2Socket.get(), &QWebSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError) { onL2Error(); });

    const auto url = QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(m_pandaPort));
    m_l2Socket->open(url);
}

void ArkTSDebugBridge::onL2Connected()
{
    if (m_state != State::ConnectingL2)
        return;

    m_retryTimer->stop();
    m_retryTimer->disconnect(this);

    m_state = State::Running;
    sendCDTMessages();
    log(QStringLiteral("[BRIDGE] Running — will stay alive until session ends or connection drops"));
}

void ArkTSDebugBridge::sendCDTMessages()
{
    m_cdtMsgIdx = 0;
    m_cdtTimer->disconnect(this);
    m_cdtTimer->setInterval(kCdtIntervalMs);
    connect(m_cdtTimer.get(), &QTimer::timeout, this, &ArkTSDebugBridge::onCdtTimerTick);
    m_cdtTimer->start();
    onCdtTimerTick(); /* ** 立即发送第一条，timer 负责后续两条 */
}

void ArkTSDebugBridge::onCdtTimerTick()
{
    static constexpr std::array<const char *, 3> kCdtMethods = {
        "Runtime.enable",
        "Debugger.enable",
        "Runtime.runIfWaitingForDebugger",
    };

    if (m_state != State::Running || m_cdtMsgIdx >= static_cast<int>(kCdtMethods.size())) {
        m_cdtTimer->stop();
        m_cdtTimer->disconnect(this);
        return;
    }

    ++m_cdtMsgId;
    const QString msg =
        QStringLiteral("{\"id\":%1,\"method\":\"%2\",\"params\":{}}")
            .arg(m_cdtMsgId)
            .arg(QString::fromLatin1(kCdtMethods[m_cdtMsgIdx]));
    m_l2Socket->sendTextMessage(msg);
    log(QStringLiteral("[BRIDGE] Sent CDT: %1").arg(msg));
    ++m_cdtMsgIdx;

    if (m_cdtMsgIdx >= static_cast<int>(kCdtMethods.size())) {
        m_cdtTimer->stop();
        m_cdtTimer->disconnect(this);
        log(QStringLiteral("[BRIDGE] Listening for Panda CDT events..."));
    }
}

void ArkTSDebugBridge::onL2TextMessageReceived(const QString &message)
{
    log(QStringLiteral("[BRIDGE] Panda CDT: %1").arg(message.left(300)));

    if (m_state != State::Running)
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    const QJsonObject   obj = doc.object();
    const QString method    = obj.value(QStringLiteral("method")).toString();

    if (method == QStringLiteral("Debugger.paused")) {
        const QString reason =
            obj.value(QStringLiteral("params"))
                .toObject()
                .value(QStringLiteral("reason"))
                .toString();

        if (reason.contains(QStringLiteral("start"), Qt::CaseInsensitive)
            || reason == QStringLiteral("other")) {
            log(QStringLiteral("[BRIDGE] Sending Debugger.resume (reason=%1)").arg(reason));
            ++m_cdtMsgId;
            const QString resume =
                QStringLiteral("{\"id\":%1,\"method\":\"Debugger.resume\",\"params\":{}}")
                    .arg(m_cdtMsgId);
            m_l2Socket->sendTextMessage(resume);
        }
    }
}

void ArkTSDebugBridge::onL2Disconnected()
{
    if (m_state == State::ConnectingL2) {
        /* ** 等待重试 */
        m_retryTimer->disconnect(this);
        connect(m_retryTimer.get(), &QTimer::timeout,
                this, &ArkTSDebugBridge::attemptL2Connect);
        m_retryTimer->start(kL2RetryMs);
        return;
    }

    if (m_state == State::Running) {
        log(QStringLiteral("[BRIDGE] Panda CDT disconnected in Running state; shutting down"));
        cleanup(true);
    }
}

void ArkTSDebugBridge::onL2Error() const
{
    if (m_state == State::Idle)
        return;

    qCDebug(arkLog) << "L2 error:" << m_l2Socket->errorString();

    if (m_state == State::ConnectingL2) {
        /*
        ** onL2Disconnected 将随后安排重试
        */
        return;
    }
}

/*
** 辅助函数
*/

void ArkTSDebugBridge::log(const QString &msg)
{
    qCDebug(arkLog) << msg;
    emit logMessage(msg);
}

void ArkTSDebugBridge::fatalError(const QString &msg)
{
    qCWarning(arkLog) << msg;
    emit errorOccurred(msg);
    cleanup(true);
}

void ArkTSDebugBridge::cleanupTimers() const
{
    for (const auto &t : {m_retryTimer.get(),
                          m_signalTimer.get(),
                          m_deadlineTimer.get(),
                          m_cdtTimer.get()}) {
        t->stop();
        t->disconnect(this);
    }
}

void ArkTSDebugBridge::teardownL2Socket(State prev)
{
    if (prev == State::Running
        && m_l2Socket->state() == QAbstractSocket::ConnectedState) {
        ++m_cdtMsgId;
        m_l2Socket->sendTextMessage(
            QStringLiteral("{\"id\":%1,\"method\":\"Debugger.disable\",\"params\":{}}")
                .arg(m_cdtMsgId));
        m_l2Socket->close();
    } else {
        m_l2Socket->abort();
    }
    m_l2Socket->disconnect(this);
}

void ArkTSDebugBridge::teardownL1Socket()
{
    m_l1Socket->disconnect(this);
    if (m_l1Socket->state() == QAbstractSocket::ConnectedState)
        m_l1Socket->close();
    else
        m_l1Socket->abort();
}

void ArkTSDebugBridge::cleanup(bool emitFinished)
{
    const State prev = m_state;
    m_state = State::Idle;

    cleanupTimers();
    teardownL2Socket(prev);
    teardownL1Socket();

    if (emitFinished)
        emit finished();
}
