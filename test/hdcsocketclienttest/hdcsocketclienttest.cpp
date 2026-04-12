// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "hdcsocketclient.h"
#include "hdcsocketprotocol.h"

#include <QTcpServer>
#include <QTcpSocket>

#include <QSignalSpy>
#include <QTest>

#include <cstring>
#include <memory>

using namespace Ohos::Internal;
using namespace Ohos::Internal::HdcSocketProtocol;

namespace {

class EnvHdcPort
{
public:
    explicit EnvHdcPort(quint16 port)
        : m_old(qgetenv("OHOS_HDC_SERVER_PORT"))
    {
        qputenv("OHOS_HDC_SERVER_PORT", QByteArray::number(port));
    }

    ~EnvHdcPort()
    {
        if (m_old.isNull())
            qunsetenv("OHOS_HDC_SERVER_PORT");
        else
            qputenv("OHOS_HDC_SERVER_PORT", m_old);
    }

    EnvHdcPort(const EnvHdcPort&) = delete;
    EnvHdcPort& operator=(const EnvHdcPort&) = delete;

    EnvHdcPort(EnvHdcPort&&) = default;
    EnvHdcPort& operator=(EnvHdcPort&&) = default;

private:
    QByteArray m_old;
};

class EnvVar
{
public:
    explicit EnvVar(const char *name, const QByteArray &value) : m_name(name)
    {
        m_old = qgetenv(name);
        qputenv(name, value);
    }
    ~EnvVar()
    {
        if (m_old.isNull())
            qunsetenv(m_name);
        else
            qputenv(m_name, m_old);
    }
    EnvVar(const EnvVar&) = delete;
    EnvVar& operator=(const EnvVar&) = delete;

private:
    const char *m_name;
    QByteArray m_old;
};

QByteArray makeValidHandshake()
{
    QByteArray hs(handshakeSize, '\0');
    hs[4] = 'O';
    hs[5] = 'H';
    hs[6] = 'O';
    hs[7] = 'S';
    hs[9] = 'H';
    hs[10] = 'D';
    hs[11] = 'C';
    return hs;
}

// 启动 mock 服务端：收到头包（>=handshakeSize 字节）后发送 payload，然后断开连接
QTcpServer *makeMockServer(QObject *parent, const QByteArray &payloadToSend,
                            bool disconnectAfter = true)
{
    auto *server = new QTcpServer(parent);
    QObject::connect(server, &QTcpServer::newConnection, server,
                     [server, payloadToSend, disconnectAfter]() {
                         QTcpSocket *peer = server->nextPendingConnection();
                         if (!peer)
                             return;
                         peer->write(makeValidHandshake());
                         auto acc = std::make_shared<QByteArray>();
                         auto replied = std::make_shared<bool>(false);
                         QObject::connect(peer, &QTcpSocket::readyRead, peer,
                                          [peer, acc, replied, payloadToSend, disconnectAfter]() {
                                              *acc += peer->readAll();
                                              if (*replied || acc->size() < handshakeSize)
                                                  return;
                                              *replied = true;
                                              if (!payloadToSend.isEmpty()) {
                                                  quint32 belen = qToBigEndian(quint32(payloadToSend.size()));
                                                  peer->write(reinterpret_cast<const char *>(&belen), 4);
                                                  peer->write(payloadToSend);
                                              }
                                              if (disconnectAfter)
                                                  peer->disconnectFromHost();
                                          });
                     });
    return server;
}

} // namespace

class HdcSocketClientTest : public QObject
{
    Q_OBJECT

private slots:
    // ── 协议层（无 socket）────────────────────────────────────────────
    void testVerifyHandshake_valid();
    void testVerifyHandshake_tooShort();
    void testVerifyHandshake_badMagic();
    void testBuildHeadPacket();
    void testBuildCommandPacket();

    // ── HdcSocketClient 流式接口 ──────────────────────────────────────
    void testMockStreaming_lineReceived();
    void testStreaming_alreadyRunning();
    void testStreaming_badHandshake();
    void testStreaming_badFrame();
    void testStreaming_multilineAndRemainder();

    // ── runShellSync ──────────────────────────────────────────────────
    void testRunShellSync_mockServer();
    void testRunShellSync_invalidTimeout();
    void testRunShellSync_timeout();
    void testRunShellSync_badHandshake();
    void testRunShellSync_badFrame();
    void testRunShellSync_connectionFailed();
    void testRunShellSync_earlyDisconnect();

    // ── preferCliFromEnvironment ──────────────────────────────────────
    void testPreferCliFromEnvironment();

    // ── runSyncWithCliFallback ────────────────────────────────────────
    void testRunSyncWithCliFallback_invalidTimeout();
    void testRunSyncWithCliFallback_socketOk();
    void testRunSyncWithCliFallback_socketFails_noHdc();
    void testRunSyncWithCliFallback_hdcNotFound();
    void testRunSyncWithCliFallback_envCli_noHdc();
    void testRunSyncWithCliFallback_rejectOk();
};

// ════════════════════════════════════════════════════════════════════════════
// 协议层
// ════════════════════════════════════════════════════════════════════════════

void HdcSocketClientTest::testVerifyHandshake_valid()
{
    QVERIFY(verifyHandshakeResponse(makeValidHandshake()));
}

void HdcSocketClientTest::testVerifyHandshake_tooShort()
{
    QByteArray hs(10, '\0');
    QVERIFY(!verifyHandshakeResponse(hs));
    QVERIFY(!verifyHandshakeResponse(QByteArray()));
}

void HdcSocketClientTest::testVerifyHandshake_badMagic()
{
    QByteArray hs = makeValidHandshake();
    hs[4] = 'X';
    QVERIFY(!verifyHandshakeResponse(hs));
}

void HdcSocketClientTest::testBuildHeadPacket()
{
    const QByteArray p = buildHeadPacket(QStringLiteral("ABC123"));
    QCOMPARE(p.size(), handshakeSize);
    QCOMPARE(int(static_cast<unsigned char>(p[3])), handshakeSize - 4);
    QVERIFY(std::memcmp(p.constData() + 4, "OHOS HDC", 8) == 0);
    QCOMPARE(p.mid(16, 6), QByteArray("ABC123"));
}

void HdcSocketClientTest::testBuildCommandPacket()
{
    const QString cmd = QStringLiteral("shell echo");
    const QByteArray p = buildCommandPacket(cmd);
    const QByteArray utf8 = cmd.toUtf8();
    const quint32 expectPayload = quint32(utf8.size() + 1);
    QCOMPARE(p.size(), int(frameHeaderSize + expectPayload));

    quint32 be = 0;
    std::memcpy(&be, p.constData(), 4);
    QCOMPARE(qFromBigEndian(be), expectPayload);
    QCOMPARE(p.mid(frameHeaderSize, utf8.size()), utf8);
    QCOMPARE(char(p[int(frameHeaderSize + utf8.size())]), '\0');
}

// ════════════════════════════════════════════════════════════════════════════
// HdcSocketClient 流式接口
// ════════════════════════════════════════════════════════════════════════════

void HdcSocketClientTest::testMockStreaming_lineReceived()
{
    // 修复：服务端条件从 <64 改为 <handshakeSize（48），
    // 确保 "shell true"（head=48 字节）能触发回复
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));

    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        QVERIFY(peer);
        peer->write(makeValidHandshake());

        auto acc = std::make_shared<QByteArray>();
        auto replied = std::make_shared<bool>(false);
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer, acc, replied]() {
            *acc += peer->readAll();
            if (*replied || acc->size() < handshakeSize)
                return;
            *replied = true;
            const QByteArray pay = "line1\n";
            quint32 belen = qToBigEndian(quint32(pay.size()));
            peer->write(reinterpret_cast<const char *>(&belen), 4);
            peer->write(pay);
        });
    });

    HdcSocketClient client;
    QSignalSpy startedSpy(&client, &HdcSocketClient::started);
    QSignalSpy lineSpy(&client, &HdcSocketClient::lineReceived);

    client.start(QStringLiteral("dev1"), QStringLiteral("shell true"));

    QVERIFY(QTest::qWaitFor([&startedSpy] { return startedSpy.size() >= 1; }, 8000));
    QVERIFY(QTest::qWaitFor([&lineSpy] { return lineSpy.size() >= 1; }, 8000));
    QCOMPARE(lineSpy.first().first().toString(), QStringLiteral("line1"));

    client.stop();
}

void HdcSocketClientTest::testStreaming_alreadyRunning()
{
    // start() 在 phase != Idle 时应直接返回（覆盖第二次调用的早返回分支）
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server,
                     [&server]() { server.nextPendingConnection(); });

    HdcSocketClient client;
    client.start(QStringLiteral("s1"), QStringLiteral("shell cmd"));
    QTest::qWait(50); // 让连接建立
    // 第二次 start() 应被忽略（不崩溃，发出警告日志）
    client.start(QStringLiteral("s2"), QStringLiteral("shell cmd2"));
    client.stop();
}

void HdcSocketClientTest::testStreaming_badHandshake()
{
    // 服务端发送无效握手 → processHandshake 失败 → errorOccurred
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        peer->setParent(&server);
        QByteArray bad(handshakeSize, 'X'); // magic 字节全错
        peer->write(bad);
        // 不主动断开：让客户端检测到握手失败后自己 abort()
    });

    HdcSocketClient client;
    QSignalSpy errorSpy(&client, &HdcSocketClient::errorOccurred);
    client.start(QStringLiteral("s1"), QStringLiteral("shell test"));
    QVERIFY(QTest::qWaitFor([&errorSpy] { return errorSpy.size() >= 1; }, 5000));
}

void HdcSocketClientTest::testStreaming_badFrame()
{
    // 服务端发送有效握手 + 超大帧头 → processStreamFrames 检测到非法帧 → stop()
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        peer->write(makeValidHandshake());
        auto acc = std::make_shared<QByteArray>();
        auto replied = std::make_shared<bool>(false);
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer, acc, replied]() {
            *acc += peer->readAll();
            if (*replied || acc->size() < handshakeSize)
                return;
            *replied = true;
            quint32 badSize = qToBigEndian(quint32(maxFramePayload + 1));
            peer->write(reinterpret_cast<const char *>(&badSize), 4);
        });
    });

    HdcSocketClient client;
    QSignalSpy startedSpy(&client, &HdcSocketClient::started);
    QSignalSpy finishedSpy(&client, &HdcSocketClient::finished);
    client.start(QStringLiteral("s1"), QStringLiteral("shell test bad frame"));
    QVERIFY(QTest::qWaitFor([&startedSpy] { return startedSpy.size() >= 1; }, 5000));
    QVERIFY(QTest::qWaitFor([&finishedSpy] { return finishedSpy.size() >= 1; }, 5000));
}

void HdcSocketClientTest::testStreaming_multilineAndRemainder()
{
    // 多行 payload + 服务端断开 → lineRemainder 在 onDisconnected 中 emit
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        peer->write(makeValidHandshake());
        auto acc = std::make_shared<QByteArray>();
        auto replied = std::make_shared<bool>(false);
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer, acc, replied]() {
            *acc += peer->readAll();
            if (*replied || acc->size() < handshakeSize)
                return;
            *replied = true;
            const QByteArray pay = "alpha\nbeta\ngamma"; // gamma 无换行 → remainder
            quint32 belen = qToBigEndian(quint32(pay.size()));
            peer->write(reinterpret_cast<const char *>(&belen), 4);
            peer->write(pay);
            peer->disconnectFromHost();
        });
    });

    HdcSocketClient client;
    QSignalSpy lineSpy(&client, &HdcSocketClient::lineReceived);
    QSignalSpy finishedSpy(&client, &HdcSocketClient::finished);
    client.start(QStringLiteral("s1"), QStringLiteral("shell multiline test"));
    QVERIFY(QTest::qWaitFor([&finishedSpy] { return finishedSpy.size() >= 1; }, 5000));
    // alpha, beta (from newlines) + gamma (from remainder on disconnect) = 3
    QCOMPARE(lineSpy.size(), 3);
    QCOMPARE(lineSpy.at(0).first().toString(), QStringLiteral("alpha"));
    QCOMPARE(lineSpy.at(1).first().toString(), QStringLiteral("beta"));
    QCOMPARE(lineSpy.at(2).first().toString(), QStringLiteral("gamma"));
}

// ════════════════════════════════════════════════════════════════════════════
// runShellSync
// ════════════════════════════════════════════════════════════════════════════

void HdcSocketClientTest::testRunShellSync_mockServer()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));

    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        QVERIFY(peer);
        peer->write(makeValidHandshake());

        auto acc = std::make_shared<QByteArray>();
        auto replied = std::make_shared<bool>(false);
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer, acc, replied]() {
            *acc += peer->readAll();
            if (*replied || acc->size() < handshakeSize)
                return;
            *replied = true;
            const QByteArray pay = "sync-ok";
            quint32 belen = qToBigEndian(quint32(pay.size()));
            peer->write(reinterpret_cast<const char *>(&belen), 4);
            peer->write(pay);
            peer->disconnectFromHost();
        });
    });

    const HdcShellSyncResult r = HdcSocketClient::runShellSync(
        QStringLiteral("d1"), QStringLiteral("shell echo x"), 8000);
    QVERIFY(r.isOk());
    QCOMPARE(r.standardOutput, QStringLiteral("sync-ok"));
}

void HdcSocketClientTest::testRunShellSync_invalidTimeout()
{
    // timeoutMs <= 0 → 立即返回 SocketError
    const HdcShellSyncResult r = HdcSocketClient::runShellSync("s1", "cmd", 0);
    QVERIFY(!r.isOk());
    QCOMPARE(r.code, HdcShellSyncResult::Code::SocketError);

    const HdcShellSyncResult r2 = HdcSocketClient::runShellSync("s1", "cmd", -1);
    QVERIFY(!r2.isOk());
    QCOMPARE(r2.code, HdcShellSyncResult::Code::SocketError);
}

void HdcSocketClientTest::testRunShellSync_timeout()
{
    // 服务端接受连接但什么都不发 → 超时
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        // 将 socket 挂在 server 上，避免立即析构；不发送任何数据以触发超时
        QTcpSocket *peer = server.nextPendingConnection();
        peer->setParent(&server);
    });

    const HdcShellSyncResult r = HdcSocketClient::runShellSync("s1", "cmd", 300);
    QCOMPARE(r.code, HdcShellSyncResult::Code::Timeout);
}

void HdcSocketClientTest::testRunShellSync_badHandshake()
{
    // 服务端发送无效握手 → HandshakeFailed
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        QByteArray bad(handshakeSize, 'X');
        peer->write(bad);
        peer->disconnectFromHost();
    });

    const HdcShellSyncResult r = HdcSocketClient::runShellSync("s1", "cmd", 5000);
    QCOMPARE(r.code, HdcShellSyncResult::Code::HandshakeFailed);
}

void HdcSocketClientTest::testRunShellSync_badFrame()
{
    // 服务端发送有效握手 + 超大帧 → BadFrame
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        peer->setParent(&server);
        peer->write(makeValidHandshake());
        auto acc = std::make_shared<QByteArray>();
        auto replied = std::make_shared<bool>(false);
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer, acc, replied]() {
            *acc += peer->readAll();
            if (*replied || acc->size() < handshakeSize)
                return;
            *replied = true;
            quint32 badSize = qToBigEndian(quint32(maxFramePayload + 1));
            peer->write(reinterpret_cast<const char *>(&badSize), 4);
        });
    });

    const HdcShellSyncResult r = HdcSocketClient::runShellSync("s1", "shell bad frame test here", 5000);
    QCOMPARE(r.code, HdcShellSyncResult::Code::BadFrame);
}

void HdcSocketClientTest::testRunShellSync_connectionFailed()
{
    // 端口无服务端 → ConnectionFailed（ConnectionRefusedError）
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    const quint16 port = quint16(server.serverPort());
    server.close(); // 立即关闭，让连接被拒绝
    EnvHdcPort env(port);

    const HdcShellSyncResult r = HdcSocketClient::runShellSync("s1", "cmd", 5000);
    QCOMPARE(r.code, HdcShellSyncResult::Code::ConnectionFailed);
}

void HdcSocketClientTest::testRunShellSync_earlyDisconnect()
{
    // 服务端接受后立即断开（未发握手）→ commandSent=false → ConnectionFailed
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        peer->disconnectFromHost();
    });

    const HdcShellSyncResult r = HdcSocketClient::runShellSync("s1", "cmd", 5000);
    QCOMPARE(r.code, HdcShellSyncResult::Code::ConnectionFailed);
}

// ════════════════════════════════════════════════════════════════════════════
// preferCliFromEnvironment
// ════════════════════════════════════════════════════════════════════════════

void HdcSocketClientTest::testPreferCliFromEnvironment()
{
    { EnvVar e("QTC_HARMONY_HDC_USE_CLI", ""); QVERIFY(!HdcSocketClient::preferCliFromEnvironment()); }
    { EnvVar e("QTC_HARMONY_HDC_USE_CLI", "1"); QVERIFY(HdcSocketClient::preferCliFromEnvironment()); }
    { EnvVar e("QTC_HARMONY_HDC_USE_CLI", "true"); QVERIFY(HdcSocketClient::preferCliFromEnvironment()); }
    { EnvVar e("QTC_HARMONY_HDC_USE_CLI", "yes"); QVERIFY(HdcSocketClient::preferCliFromEnvironment()); }
    { EnvVar e("QTC_HARMONY_HDC_USE_CLI", "0"); QVERIFY(!HdcSocketClient::preferCliFromEnvironment()); }
    { EnvVar e("QTC_HARMONY_HDC_USE_CLI", "no"); QVERIFY(!HdcSocketClient::preferCliFromEnvironment()); }
    qunsetenv("QTC_HARMONY_HDC_USE_CLI");
    QVERIFY(!HdcSocketClient::preferCliFromEnvironment());
}

// ════════════════════════════════════════════════════════════════════════════
// runSyncWithCliFallback
// ════════════════════════════════════════════════════════════════════════════

void HdcSocketClientTest::testRunSyncWithCliFallback_invalidTimeout()
{
    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        "d1", "cmd", "", {}, 0);
    QVERIFY(!r.isOk());
    QCOMPARE(r.code, HdcShellSyncResult::Code::CliFailed);
}

void HdcSocketClientTest::testRunSyncWithCliFallback_socketOk()
{
    // socket 成功 → 直接返回，不走 CLI
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        peer->write(makeValidHandshake());
        auto acc = std::make_shared<QByteArray>();
        auto replied = std::make_shared<bool>(false);
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer, acc, replied]() {
            *acc += peer->readAll();
            if (*replied || acc->size() < handshakeSize)
                return;
            *replied = true;
            const QByteArray pay = "fallback-ok";
            quint32 belen = qToBigEndian(quint32(pay.size()));
            peer->write(reinterpret_cast<const char *>(&belen), 4);
            peer->write(pay);
            peer->disconnectFromHost();
        });
    });

    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        "d1", "shell echo fallback ok test", "", {}, 8000);
    QVERIFY(r.isOk());
    QCOMPARE(r.standardOutput, QStringLiteral("fallback-ok"));
}

void HdcSocketClientTest::testRunSyncWithCliFallback_socketFails_noHdc()
{
    // socket 失败（无服务端），hdc 路径为空 → CliFailed；应调用 notifier
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    const quint16 port = quint16(server.serverPort());
    server.close();
    EnvHdcPort env(port);

    bool notified = false;
    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        "d1", "cmd", "", {}, 5000,
        [&notified](const QString &) { notified = true; });
    QVERIFY(notified);
    QCOMPARE(r.code, HdcShellSyncResult::Code::CliFailed);
}

void HdcSocketClientTest::testRunSyncWithCliFallback_hdcNotFound()
{
    // socket 失败，hdc 路径不存在 → CliFailed
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    const quint16 port = quint16(server.serverPort());
    server.close();
    EnvHdcPort env(port);

    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        "d1", "cmd", "/nonexistent/path/hdc", {}, 5000);
    QCOMPARE(r.code, HdcShellSyncResult::Code::CliFailed);
}

void HdcSocketClientTest::testRunSyncWithCliFallback_envCli_noHdc()
{
    // QTC_HARMONY_HDC_USE_CLI=1 → 跳过 socket，直接走 CLI；hdc 为空 → CliFailed
    EnvVar cliEnv("QTC_HARMONY_HDC_USE_CLI", "1");

    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        "d1", "cmd", "", {}, 5000);
    QCOMPARE(r.code, HdcShellSyncResult::Code::CliFailed);
}

void HdcSocketClientTest::testRunSyncWithCliFallback_rejectOk()
{
    // socket 成功但 rejectSocketOk 返回 true → fallback；hdc 为空 → CliFailed
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    EnvHdcPort env(quint16(server.serverPort()));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *peer = server.nextPendingConnection();
        peer->write(makeValidHandshake());
        auto acc = std::make_shared<QByteArray>();
        auto replied = std::make_shared<bool>(false);
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer, acc, replied]() {
            *acc += peer->readAll();
            if (*replied || acc->size() < handshakeSize)
                return;
            *replied = true;
            const QByteArray pay = "reject-me";
            quint32 belen = qToBigEndian(quint32(pay.size()));
            peer->write(reinterpret_cast<const char *>(&belen), 4);
            peer->write(pay);
            peer->disconnectFromHost();
        });
    });

    bool notified = false;
    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        "d1", "shell echo reject me please test", "", {}, 8000,
        [&notified](const QString &) { notified = true; },
        [](const HdcShellSyncResult &) { return true; }); // 始终拒绝 socket 结果
    QVERIFY(notified);
    QCOMPARE(r.code, HdcShellSyncResult::Code::CliFailed);
}

QTEST_MAIN(HdcSocketClientTest)

#include "hdcsocketclienttest.moc"
