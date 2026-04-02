// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QVector>

#include <memory>

namespace Ohos::Internal {

/**
 * Backup JSON list host (mirrors of getSdkList), same as build_qt `configure.json`
 * `dependencies.ohos_sdk.gc_url` / `gh_url` without the trailing "-{osType}".
 */
enum class HarmonySdkBackupMirror {
    GitCode,
    GitHub,
};

struct HarmonySdkArchiveInfo {
    QUrl url;
    qint64 size = -1;
    QString sha256Checksum;
    QString osArch;
};

struct HarmonySdkPackageEntry {
    QString apiVersion;
    QString componentPath;
    HarmonySdkArchiveInfo archive;
};

/**
 * Fetches OpenHarmony SDK component index (JSON list) over HTTP.
 *
 * Aligns with the protocol used by build_qt/ohos_sdk_downloader.py:
 * - Primary: POST JSON { osArch, osType, supportVersion }
 * - Backup: GET backup URL (caller supplies full URL, e.g. base + "-" + osType)
 * - Merge: primary entries override backup for the same (apiVersion, componentPath).
 */
class HarmonySdkDownloader final : public QObject
{
    Q_OBJECT

public:
    struct ListRequest {
        QUrl primaryListPostUrl;
        QString osType;
        QString osArch;
        QString supportVersion;
    };

    explicit HarmonySdkDownloader(QObject *parent = nullptr);

    void fetchPackageList(const ListRequest &request);

    /** Primary POST URL, backup GET URL, osType/osArch/supportVersion — same defaults as build_qt. */
    static ListRequest defaultListRequest(HarmonySdkBackupMirror mirror = HarmonySdkBackupMirror::GitCode);

    static QVector<HarmonySdkPackageEntry> parsePackageListJson(const QByteArray &json,
                                                                QString *errorMessage = nullptr);

signals:
    void packageListFetched(const QVector<HarmonySdkPackageEntry> &entries);
    void fetchFailed(const QString &errorMessage);

private:
    std::unique_ptr<QNetworkAccessManager> m_nam;
};

} // namespace Ohos::Internal
