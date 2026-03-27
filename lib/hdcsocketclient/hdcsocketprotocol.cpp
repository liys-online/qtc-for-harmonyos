// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "hdcsocketprotocol.h"

#include <QtEndian>

#include <cstring>

namespace Ohos::Internal::HdcSocketProtocol {

QByteArray buildHeadPacket(const QString &serial)
{
    QByteArray pkt(handshakeSize, '\0');
    pkt[3] = char(handshakeSize - 4);
    std::memcpy(pkt.data() + 4, "OHOS HDC", 8);
    const QByteArray s = serial.toUtf8();
    const int n = qMin<int>(s.size(), handshakeSize - 16);
    if (n > 0)
        std::memcpy(pkt.data() + 16, s.constData(), n);
    return pkt;
}

QByteArray buildCommandPacket(const QString &command)
{
    const QByteArray cmd = command.toUtf8();
    const quint32 payload = quint32(cmd.size() + 1);
    QByteArray pkt(int(frameHeaderSize + payload), '\0');
    const quint32 be = qToBigEndian(payload);
    std::memcpy(pkt.data(), &be, frameHeaderSize);
    std::memcpy(pkt.data() + frameHeaderSize, cmd.constData(), cmd.size());
    return pkt;
}

bool verifyHandshakeResponse(const QByteArray &r)
{
    if (r.size() < handshakeSize)
        return false;
    return r[4] == 'O' && r[5] == 'H' && r[6] == 'O' && r[7] == 'S'
        && r[9] == 'H' && r[10] == 'D' && r[11] == 'C';
}

} // namespace Ohos::Internal::HdcSocketProtocol
