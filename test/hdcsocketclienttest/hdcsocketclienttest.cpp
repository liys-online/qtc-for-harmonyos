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

} // namespace

class HdcSocketClientTest : public QObject
{
    Q_OBJECT

private slots:
    void testVerifyHandshake_valid();
    void testVerifyHandshake_tooShort();
    void testVerifyHandshake_badMagic();

    void testBuildHeadPacket();
    void testBuildCommandPacket();

    void testMockStreaming_lineReceived();
    void testRunShellSync_mockServer();
};

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

void HdcSocketClientTest::testMockStreaming_lineReceived()
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
            if (*replied || acc->size() < 64)
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
            if (*replied || acc->size() < 64)
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

QTEST_MAIN(HdcSocketClientTest)

#include "hdcsocketclienttest.moc"
