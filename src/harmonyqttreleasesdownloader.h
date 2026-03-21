// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QVector>

#include <memory>

namespace Ohos::Internal {

struct QtForOhReleaseAsset {
    QString name;
    QUrl browserDownloadUrl;
    /** Catalog \c platform: \c linux / \c darwin / \c windows (empty if omitted). */
    QString catalogPlatform;
    /** Catalog \c arch: OpenHarmony target ABI (\c x86_64 / \c aarch64 / …), not host CPU (empty if omitted). */
    QString catalogArch;
    /** Catalog \c kind: \c archive / \c metadata / … (empty treated like archive). */
    QString assetKind;
};

struct QtForOhRelease {
    QString tagName;
    QString title;
    QString createdAt;
    QString body;
    /** From catalog field \c qtVersion when present (e.g. \c 5.15.12). */
    QString qtVersion;
    QVector<QtForOhReleaseAsset> assets;
};

/**
 * Fetches \c qt-for-openharmony.binary-catalog (v1) JSON from a configurable HTTPS URL
 * (Harmony settings and/or \c QT_OH_BINARY_CATALOG_URL), with an optional fallback URL.
 */
class HarmonyQtOhReleasesDownloader final : public QObject
{
    Q_OBJECT

public:
    explicit HarmonyQtOhReleasesDownloader(QObject *parent = nullptr);

    /** GET primary catalog URL, then optional fallback on network/parse failure. */
    void fetchAllReleases();

signals:
    void releasesFetched(const QVector<QtForOhRelease> &releases);
    void fetchFailed(const QString &errorMessage);

private:
    void startCatalogFetch(const QUrl &url, bool isPrimaryAttempt);
    void onCatalogReplyFinished();

    std::unique_ptr<QNetworkAccessManager> m_nam;
    QNetworkReply *m_reply = nullptr;

    QUrl m_fallbackUrl;
    bool m_currentFetchIsPrimary = true;
};

} // namespace Ohos::Internal
