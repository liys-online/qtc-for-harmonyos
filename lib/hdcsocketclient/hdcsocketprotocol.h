// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QByteArray>
#include <QString>

namespace Ohos::Internal::HdcSocketProtocol {

inline constexpr int handshakeSize = 48;
inline constexpr int frameHeaderSize = 4;
inline constexpr int maxFramePayload = 524288; /* ** 512 KB */

/*
** 守护进程握手包验证：[4..7] 为 "OHOS"，[9..11] 为 "HDC"（参见 DevEco hdclib）。
*/
bool verifyHandshakeResponse(const QByteArray &response);

/*
** 构建 48 字节头部包：[3] 为长度字节，[4..11] 为 "OHOS HDC"，[16] 起为 UTF-8 序列号。
*/
QByteArray buildHeadPacket(const QString &serial);

/*
** 构建命令包：[4 字节大端 size = utf8Len(command)+1] [command UTF-8] [0x00]
*/
QByteArray buildCommandPacket(const QString &command);

} // namespace Ohos::Internal::HdcSocketProtocol
