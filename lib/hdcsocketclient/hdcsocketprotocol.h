// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QByteArray>
#include <QString>

namespace Ohos::Internal::HdcSocketProtocol {

inline constexpr int handshakeSize = 48;
inline constexpr int frameHeaderSize = 4;
inline constexpr int maxFramePayload = 524288; // 512 KB

/// Daemon handshake: "OHOS" at [4..7], "HDC" at [9..11] (see DevEco hdclib).
bool verifyHandshakeResponse(const QByteArray &response);

/// 48-byte head: length byte at [3], "OHOS HDC" at [4..11], UTF-8 serial from [16].
QByteArray buildHeadPacket(const QString &serial);

/// [4-byte BE size = utf8Len(command)+1] [command UTF-8] [0x00]
QByteArray buildCommandPacket(const QString &command);

} // namespace Ohos::Internal::HdcSocketProtocol
