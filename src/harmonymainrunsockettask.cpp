// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonymainrunsockettask.h"

#include "hdcsocketclient.h"
#include "ohostr.h"

#include <projectexplorer/runcontrol.h>

#include <utils/outputformat.h>

using namespace ProjectExplorer;
using namespace Utils;

namespace Ohos::Internal {

void HarmonyMainRunSocketTask::setContext(RunControl *runControl, QString serial,
                                          QString deviceShellScript)
{
    m_runControl = runControl;
    m_serial = std::move(serial);
    m_script = std::move(deviceShellScript);
}

void HarmonyMainRunSocketTask::start()
{
    m_sessionReady = false;
    m_emittedDone = false;

    if (!m_runControl || m_script.isEmpty()) {
        finalizeDone(QtTaskTree::DoneResult::Error);
        return;
    }

    m_client = new HdcSocketClient(this);   // NOSONAR (cpp:S5025) - parented, will auto-delete on stopped

    QObject::connect(m_client, &HdcSocketClient::started, this, [this] {
        m_sessionReady = true;
        emit sessionStarted();
    });
    QObject::connect(m_client, &HdcSocketClient::lineReceived, m_runControl,
                     [this](const QString &line) {
                         m_runControl->postMessage(line, StdOutFormat);
                     });
    QObject::connect(m_client, &HdcSocketClient::errorOccurred, m_runControl,
                     [this](const QString &msg) {
                         m_runControl->postMessage(
                             Tr::tr("Harmony main shell (socket): %1").arg(msg),
                             ErrorMessageFormat);
                     });
    QObject::connect(m_client, &HdcSocketClient::finished, this,
                     &HarmonyMainRunSocketTask::onClientFinished);

    m_client->start(m_serial, QStringLiteral("shell ") + m_script);
}

void HarmonyMainRunSocketTask::stopShell()
{
    if (m_client)
        m_client->stop();
}

void HarmonyMainRunSocketTask::onClientFinished()
{
    const QtTaskTree::DoneResult dr = m_sessionReady ? QtTaskTree::DoneResult::Success
                                                     : QtTaskTree::DoneResult::Error;
    finalizeDone(dr);
}

void HarmonyMainRunSocketTask::finalizeDone(QtTaskTree::DoneResult result)
{
    if (m_emittedDone)
        return;
    m_emittedDone = true;
    if (m_client) {
        m_client->disconnect(this);
        m_client->deleteLater();
        m_client = nullptr;
    }
    emit done(result);
}

} // namespace Ohos::Internal
