// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonysdklicensedialog.h"
#include "harmonyconfigurations.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <coreplugin/icore.h>

#include <utils/layoutbuilder.h>

#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>

using namespace Utils;

namespace Ohos::Internal {

// ── Persisted license acceptance ────────────────────────────────────────────

static const QByteArrayView kLicenseAcceptedKey = "Harmony.SdkLicenseAccepted";

bool HarmonySdkLicenseDialog::isLicenseAccepted()
{
    return Core::ICore::settings()->value(kLicenseAcceptedKey, false).toBool();
}

void HarmonySdkLicenseDialog::setLicenseAccepted()
{
    Core::ICore::settings()->setValue(kLicenseAcceptedKey, true);
}

// ── Dialog ──────────────────────────────────────────────────────────────────

HarmonySdkLicenseDialog::HarmonySdkLicenseDialog(QWidget *parent)
    : QDialog(parent ? parent : Core::ICore::dialogParent())
{
    setWindowTitle(Tr::tr("OpenHarmony SDK License Agreement"));
    resize(700, 550);
    setModal(true);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);

    m_licenseText = new QPlainTextEdit(this);
    m_licenseText->setReadOnly(true);
    m_licenseText->setVisible(false);

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Cancel);

    using namespace Layouting;
    Column {
        m_statusLabel,
        m_licenseText,
        m_buttonBox,
    }.attachTo(this);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, [this] {
        m_accepted = true;
        setLicenseAccepted();
        accept();
    });
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, [this] {
        m_accepted = false;
        reject();
    });

    fetchLicenseUrl();
}

void HarmonySdkLicenseDialog::setBusy(bool busy)
{
    m_buttonBox->setEnabled(!busy);
}

void HarmonySdkLicenseDialog::fetchLicenseUrl()
{
    m_statusLabel->setText(Tr::tr("Fetching license information…"));
    setBusy(true);

    QJsonObject body;
    body.insert(QStringLiteral("licenseIds"), QJsonArray{QStringLiteral("OpenHarmony-SDK")});

    QNetworkRequest req(QUrl(QStringLiteral("https://repo.harmonyos.com/sdkmanager/v4/ohos/licenses")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(30000);

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        onLicenseUrlReceived();

        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(
                Tr::tr("Failed to fetch license URL: %1").arg(reply->errorString()));
            setBusy(false);
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isArray() || doc.array().isEmpty()) {
            m_statusLabel->setText(Tr::tr("Invalid license response from server."));
            setBusy(false);
            return;
        }

        const QJsonObject first = doc.array().first().toObject();
        const QString urlStr = first.value(QStringLiteral("url")).toString();
        if (urlStr.isEmpty()) {
            m_statusLabel->setText(Tr::tr("License URL not found in server response."));
            setBusy(false);
            return;
        }

        m_licenseTextUrl = QUrl(urlStr);
        fetchLicenseText(m_licenseTextUrl);
    });
}

void HarmonySdkLicenseDialog::onLicenseUrlReceived()
{
    // handled inline in the lambda above
}

void HarmonySdkLicenseDialog::fetchLicenseText(const QUrl &url)
{
    m_statusLabel->setText(Tr::tr("Loading license text…"));
    setBusy(true);

    QNetworkRequest req(url);
    req.setTransferTimeout(30000);

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        onLicenseTextReceived();

        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(
                Tr::tr("Failed to load license text: %1").arg(reply->errorString()));
            setBusy(false);
            return;
        }

        const QString text = QString::fromUtf8(reply->readAll());
        if (text.trimmed().isEmpty()) {
            m_statusLabel->setText(Tr::tr("License text is empty."));
            setBusy(false);
            return;
        }

        m_licenseText->setPlainText(text);
        m_licenseText->setVisible(true);
        // Scroll to top so the user sees the beginning
        m_licenseText->verticalScrollBar()->setValue(0);

        m_statusLabel->setText(
            Tr::tr("Please read the OpenHarmony SDK License Agreement above.\n"
                   "You must accept it to download and use the SDK packages."));

        m_buttonBox->clear();
        auto *acceptBtn = m_buttonBox->addButton(Tr::tr("Accept"), QDialogButtonBox::AcceptRole);
        auto *declineBtn = m_buttonBox->addButton(Tr::tr("Decline"), QDialogButtonBox::RejectRole);
        acceptBtn->setDefault(true);

        setBusy(false);
    });
}

void HarmonySdkLicenseDialog::onLicenseTextReceived()
{
    // handled inline in the lambda above
}

} // namespace Ohos::Internal
