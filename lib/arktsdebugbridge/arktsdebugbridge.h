// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef ARKTSDEBUGBRIDGE_H
#define ARKTSDEBUGBRIDGE_H

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QWebSocket;
class QTimer;
QT_END_NAMESPACE

/*
** ArkTSDebugBridge
**
** 实现 --startup-break 模式下的两级 ArkTS 解锁协议。
** 所有操作均为全异步，对象可在任意运行 QEventLoop 的线程上创建。
**
** 两级解锁协议（参考 OhCppXDebugProcess.java 分析）：
**
**   第一级 — ConnectServer（ark fport → ws://127.0.0.1:connectPort）：
**     在执行 'process attach' 之前，于 ArkTS 线程仍在运行时建立 WebSocket 连接。
**     LLDB attach 冻结线程后，调用方创建信号文件。
**     检测到信号文件时，将 {"type":"connected"} 写入 TCP 缓冲区。
**     ArkTS 读取缓冲消息后回复 {"type":"addInstance"}。
**
**   第二级 — Panda CDT（ark fport → ws://127.0.0.1:pandaPort）：
**     收到 "addInstance" 后，连接 Panda CDT WebSocket 并依次发送：
**       Runtime.enable / Debugger.enable / Runtime.runIfWaitingForDebugger
**     此操作解锁 ArkTS JS 虚拟机，使其加载原生 .so 并执行 main()。
**     处理 Debugger.paused/BREAK_ON_START 事件时自动发送 Debugger.resume，
**     防止 LLDB solib 停止事件永久冻结虚拟机。
**
** 典型用法：
**   auto bridge = new ArkTSDebugBridge;
**   connect(bridge, &ArkTSDebugBridge::logMessage, ...);
**   bridge->start(connectPort, pandaPort, signalFilePath);
**   // LLDB 运行期间会在 'process continue' 之前创建 signalFilePath
**   // bridge 随后异步驱动后续流程
*/
class ArkTSDebugBridge : public QObject
{
    Q_OBJECT
public:
    explicit ArkTSDebugBridge(QObject *parent = nullptr);
    ~ArkTSDebugBridge() override;

    /*
    ** 启动 bridge，立即发起第一级 WebSocket 连接。
    ** signalFile  ：LLDB 通过 'script' 命令创建的信号文件路径，
    **              用于通知 bridge 发送 {"type":"connected"}。
    ** connectPort ：本地 TCP 端口，已由 fport 转发到 ark:<pid>@<bundle>
    ** pandaPort   ：本地 TCP 端口，已由 fport 转发到 ark:<pid>@Debugger
    */
    void start(quint16 connectPort, quint16 pandaPort, const QString &signalFile);

    void stop();

    bool isRunning() const;

signals:
    /*
    ** 日志输出信号（对应 Python print 语句）。
    */
    void logMessage(const QString &message);

    /*
    ** 发生致命错误，bridge 已停止。
    */
    void errorOccurred(const QString &message);

    /*
    ** 第一级 addInstance 回复已接收。
    */
    void addInstanceReceived(const QString &instanceId);

    /*
    ** bridge 已停止（stop() 调用、连接断开或错误均可触发）。
    */
    void finished();

private:
    enum class State {
        Idle,
        ConnectingL1,       /* ** 正在连接 ConnectServer WebSocket */
        WaitingSignal,      /* ** 第一级已连接，等待信号文件出现 */
        WaitingAddInstance, /* ** 已发送 {"type":"connected"}，等待 addInstance 回复 */
        ConnectingL2,       /* ** 正在连接 Panda CDT WebSocket */
        Running,            /* ** 发送 CDT 命令并监听 Debugger.paused 事件 */
    };

    /*
    ** 第一级协议：ConnectServer
    */
    void attemptL1Connect();
    void onL1Connected();
    void onL1TextMessageReceived(const QString &message);
    void onL1Disconnected();
    void onL1Error();

    /*
    ** 信号文件监控
    */
    void startSignalWatch();
    void onSignalPollTick();
    void onSignalTimeout();

    /*
    ** 第二级协议：Panda CDT
    */
    void attemptL2Connect();
    void onL2Connected();
    void onL2TextMessageReceived(const QString &message);
    void onL2Disconnected();
    void onL2Error();

    void sendCDTMessages();

    /*
    ** 辅助工具
    */
    void log(const QString &msg);
    void fatalError(const QString &msg);
    void cleanup(bool emitFinished);

    /*
    ** 成员状态
    */
    State       m_state       = State::Idle;
    quint16     m_connectPort = 0;
    quint16     m_pandaPort   = 0;
    QString     m_signalFile;

    QWebSocket *m_l1Socket    = nullptr;
    QWebSocket *m_l2Socket    = nullptr;

    QTimer *m_retryTimer    = nullptr; /* ** L1 / L2 重试时钟 */
    QTimer *m_signalTimer   = nullptr; /* ** 100ms 信号文件轮询定时器 */
    QTimer *m_deadlineTimer = nullptr; /* ** 分阶段超时看门狗 */

    int m_l1Attempts = 0; /* ** 最多 60 次，间隔 500ms */
    int m_l2Attempts = 0; /* ** 最多 20 次，间隔 300ms */
    int m_cdtMsgId   = 0;
};

#endif // ARKTSDEBUGBRIDGE_H
