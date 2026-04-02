// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonysdkdownloader.h"
#include "ohostr.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>

#include <algorithm>

using namespace Ohos;

namespace Ohos::Internal {
namespace {

/*
** 与 sibling 仓库 `build_qt/configure.json` → dependencies.ohos_sdk 及
** `build_qt/build_qt/config.py` 中 OhosSdkDownloader(...) 参数一致。
*/

QString detectOsTypeForSdkList()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("darwin");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return QStringLiteral("linux");
#endif
}

QString detectOsArchForSdkList()
{
    const QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == QLatin1String("x86_64") || arch == QLatin1String("amd64"))
        return QStringLiteral("x64");
    if (arch == QLatin1String("arm64") || arch == QLatin1String("aarch64"))
        return QStringLiteral("arm64");
    return arch;
}

QString entryKey(const HarmonySdkPackageEntry &e)
{
    return e.apiVersion + QLatin1Char('\n') + e.componentPath;
}

HarmonySdkPackageEntry entryFromJsonObject(const QJsonObject &o)
{
    HarmonySdkPackageEntry e;
    e.apiVersion = o.value(QStringLiteral("apiVersion")).toVariant().toString();
    e.componentPath = o.value(QStringLiteral("path")).toString();

    const QJsonObject archive = o.value(QStringLiteral("archive")).toObject();
    const QString urlStr = archive.value(QStringLiteral("url")).toString();
    if (!urlStr.isEmpty())
        e.archive.url = QUrl::fromUserInput(urlStr);

    const QJsonValue sizeVal = archive.value(QStringLiteral("size"));
    if (sizeVal.isDouble())
        e.archive.size = static_cast<qint64>(sizeVal.toDouble());
    else if (sizeVal.isString())
        e.archive.size = sizeVal.toString().toLongLong();

    e.archive.sha256Checksum = archive.value(QStringLiteral("checksum")).toString();
    e.archive.osArch = archive.value(QStringLiteral("osArch")).toString();
    return e;
}

QVector<HarmonySdkPackageEntry> mergeLists(const QVector<HarmonySdkPackageEntry> &backup,
                                           const QVector<HarmonySdkPackageEntry> &primary)
{
    QHash<QString, HarmonySdkPackageEntry> map;
    for (const HarmonySdkPackageEntry &e : backup)
        map.insert(entryKey(e), e);
    for (const HarmonySdkPackageEntry &e : primary)
        map.insert(entryKey(e), e);

    QList<QString> keys = map.keys();
    std::sort(keys.begin(), keys.end());

    QVector<HarmonySdkPackageEntry> out;
    out.reserve(keys.size());
    for (const QString &k : std::as_const(keys))
        out.append(map.value(k));
    return out;
}

} // namespace

HarmonySdkDownloader::HarmonySdkDownloader(QObject *parent)
    : QObject(parent)
    , m_nam(std::make_unique<QNetworkAccessManager>(this))
{
}

HarmonySdkDownloader::ListRequest HarmonySdkDownloader::defaultListRequest(HarmonySdkBackupMirror mirror)
{
    ListRequest r;
    r.primaryListPostUrl
        = QUrl(QStringLiteral("https://repo.harmonyos.com/sdkmanager/v5/ohos/getSdkList"));
    const QString osType = detectOsTypeForSdkList();
    r.osType = osType;
    r.osArch = detectOsArchForSdkList();
    r.supportVersion = QStringLiteral("6.0-ohos-single-3");
    return r;
}

QVector<HarmonySdkPackageEntry> HarmonySdkDownloader::parsePackageListJson(const QByteArray &json,
                                                                           QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage)
            *errorMessage = Tr::tr("Invalid JSON: %1").arg(parseError.errorString());
        return {};
    }

    if (!doc.isArray()) {
        if (errorMessage)
            *errorMessage = Tr::tr("Unexpected response format: expected a JSON array.");
        return {};
    }

    QVector<HarmonySdkPackageEntry> out;
    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        const HarmonySdkPackageEntry e = entryFromJsonObject(v.toObject());
        if (e.componentPath.isEmpty() || !e.archive.url.isValid())
            continue;
        out.append(e);
    }
    return out;
}

void HarmonySdkDownloader::fetchPackageList(const ListRequest &request)
{
    if (!request.primaryListPostUrl.isValid()) {
        emit fetchFailed(Tr::tr("SDK list endpoints are not configured."));
        return;
    }

    QJsonObject body;
    body.insert(QStringLiteral("osArch"), request.osArch);
    body.insert(QStringLiteral("osType"), request.osType);
    body.insert(QStringLiteral("supportVersion"), request.supportVersion);

    QNetworkRequest req(request.primaryListPostUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, request, reply] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QString parseErr;
            QVector<HarmonySdkPackageEntry> primary = parsePackageListJson(reply->readAll(), &parseErr);
            if (!parseErr.isEmpty()) {
                emit fetchFailed(parseErr);
                return;
            }
            emit packageListFetched(primary);
        }
    });
}

} // namespace Ohos::Internal
