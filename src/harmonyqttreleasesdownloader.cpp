// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonyqttreleasesdownloader.h"

#include "harmonyconfigurations.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <algorithm>

namespace Ohos::Internal {

namespace {

static constexpr char kExpectedSchema[] = "qt-for-openharmony.binary-catalog";

static QVector<QtForOhRelease> catalogError(QString *errorOut, const QString &msg)
{
    if (errorOut)
        *errorOut = msg;
    return {};
}

static QVector<QtForOhReleaseAsset> parseReleaseAssets(const QJsonArray &assets)
{
    QVector<QtForOhReleaseAsset> result;
    for (const QJsonValue &av : assets) {
        if (!av.isObject())
            continue;
        const QJsonObject ao = av.toObject();
        const QString name = ao.value(QStringLiteral("name")).toString();
        const QUrl dl(ao.value(QStringLiteral("downloadUrl")).toString());
        if (name.isEmpty() || !dl.isValid())
            continue;
        QtForOhReleaseAsset a;
        a.name = name;
        a.browserDownloadUrl = dl;
        a.catalogPlatform = ao.value(QStringLiteral("platform")).toString();
        a.catalogArch = ao.value(QStringLiteral("arch")).toString();
        a.assetKind = ao.value(QStringLiteral("kind")).toString();
        result.append(a);
    }
    return result;
}

QVector<QtForOhRelease> parseBinaryCatalogV1(const QByteArray &json, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError)
        return catalogError(errorMessage, pe.errorString());

    if (!doc.isObject())
        return catalogError(errorMessage, QStringLiteral("Catalog must be a JSON object."));

    const QJsonObject root = doc.object();
    const QString schema = root.value(QStringLiteral("schema")).toString();
    if (schema != QLatin1String(kExpectedSchema))
        return catalogError(errorMessage,
                            QStringLiteral("Unknown catalog schema: %1 (expected %2).")
                                .arg(schema, QString::fromLatin1(kExpectedSchema)));

    const int ver = root.value(QStringLiteral("schemaVersion")).toInt();
    if (ver != 1)
        return catalogError(errorMessage,
                            QStringLiteral("Unsupported schemaVersion %1 (this plugin supports v1 only).").arg(ver));

    const QJsonValue relVal = root.value(QStringLiteral("releases"));
    if (!relVal.isArray())
        return catalogError(errorMessage, QStringLiteral("Missing or invalid \"releases\" array."));

    QVector<QtForOhRelease> out;
    const QJsonArray releases = relVal.toArray();
    out.reserve(releases.size());

    for (const QJsonValue &rv : releases) {
        if (!rv.isObject())
            continue;
        const QJsonObject ro = rv.toObject();
        const QString tag = ro.value(QStringLiteral("tag")).toString();
        if (tag.isEmpty())
            continue;

        QtForOhRelease r;
        r.tagName = tag;
        r.title = ro.value(QStringLiteral("title")).toString();
        r.createdAt = ro.value(QStringLiteral("createdAt")).toString();
        r.body = ro.value(QStringLiteral("summary")).toString();
        r.qtVersion = ro.value(QStringLiteral("qtVersion")).toString();
        r.assets = parseReleaseAssets(ro.value(QStringLiteral("assets")).toArray());
        out.append(r);
    }

    std::sort(out.begin(), out.end(), [](const QtForOhRelease &a, const QtForOhRelease &b) {
        return QString::localeAwareCompare(a.createdAt, b.createdAt) > 0;
    });

    return out;
}

} // namespace

HarmonyQtOhReleasesDownloader::HarmonyQtOhReleasesDownloader(QObject *parent)
    : QObject(parent)
    , m_nam(std::make_unique<QNetworkAccessManager>())
{}

void HarmonyQtOhReleasesDownloader::fetchAllReleases()
{
    if (m_reply) {
        m_reply->disconnect();
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    const QString primaryString = HarmonyConfig::effectiveQtOhBinaryCatalogUrl();
    const QUrl primary = QUrl::fromUserInput(primaryString);
    // m_fallbackUrl = QUrl(QString::fromLatin1(Constants::QtOhBinaryCatalogDefaultGithubUrl));

    if (!primary.isValid() || primaryString.trimmed().isEmpty()) {
        emit fetchFailed(Ohos::Tr::tr("Invalid Qt for OpenHarmony catalog URL."));
        return;
    }

    if (primary.scheme() != QStringLiteral("https") && primary.scheme() != QStringLiteral("http")) {
        emit fetchFailed(Ohos::Tr::tr("Catalog URL must use http or https: %1")
                             .arg(primary.toDisplayString()));
        return;
    }

    startCatalogFetch(primary, true);
}

void HarmonyQtOhReleasesDownloader::startCatalogFetch(const QUrl &url, bool isPrimaryAttempt)
{
    m_currentFetchIsPrimary = isPrimaryAttempt;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QtCreator-Harmony-Plugin"));
    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &HarmonyQtOhReleasesDownloader::onCatalogReplyFinished);
}

void HarmonyQtOhReleasesDownloader::onCatalogReplyFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        emit fetchFailed(Ohos::Tr::tr("Internal error: no reply."));
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit fetchFailed(reply->errorString());
        reply->deleteLater();
        return;
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();
    QString err;
    const QVector<QtForOhRelease> items = parseBinaryCatalogV1(body, &err);
    if (!err.isEmpty()) {
        emit fetchFailed(err);
        return;
    }

    emit releasesFetched(items);
}

} // namespace Ohos::Internal
