// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

/**
 * Fetches GitCode releases for openharmony-sig/qt and writes
 * qt-for-openharmony.binary-catalog (schema v1) JSON.
 *
 * Authentication: env GITCODE_PRIVATE_TOKEN or --token (see GitCode OpenAPI docs).
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrlQuery>
#include <QVector>

#include <algorithm>
#include <cstdlib>

static constexpr char kDefaultRepo[] = "openharmony-sig/qt";
static constexpr char kApiHost[] = "https://api.gitcode.com";

static int readTotalPages(QNetworkReply *reply)
{
    if (!reply)
        return 1;
    bool ok = false;
    const int n = reply->rawHeader(QByteArrayLiteral("total_page")).trimmed().toInt(&ok);
    return ok && n > 0 ? n : 1;
}

static QJsonArray fetchAllReleases(const QString &repo, const QByteArray &token, QString *error)
{
    QNetworkAccessManager nam;
    QJsonArray merged;

    int totalPages = -1;
    for (int page = 1;; ++page) {
        if (totalPages > 0 && page > totalPages)
            break;

        QUrl url(QString::fromLatin1("%1/api/v5/repos/%2/releases").arg(QLatin1String(kApiHost), repo));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("page"), QString::number(page));
        q.addQueryItem(QStringLiteral("per_page"), QStringLiteral("100"));
        url.setQuery(q);

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("qt-oh-binary-catalog-generator"));
        if (!token.isEmpty())
            req.setRawHeader(QByteArrayLiteral("PRIVATE-TOKEN"), token);

        QNetworkReply *reply = nam.get(req);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            *error = reply->errorString();
            reply->deleteLater();
            return {};
        }

        if (totalPages < 0)
            totalPages = readTotalPages(reply);

        const QByteArray body = reply->readAll();
        reply->deleteLater();

        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isArray()) {
            *error = QStringLiteral("Expected JSON array on page %1").arg(page);
            return {};
        }

        const QJsonArray arr = doc.array();
        if (arr.isEmpty() && page > 1)
            break;

        for (const QJsonValue &v : arr)
            merged.append(v);

        if (page >= totalPages)
            break;
    }

    return merged;
}

static int alphaVersionFromTag(const QString &tag)
{
    static const QRegularExpression re(QStringLiteral("^Alpha_v(\\d+)$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(tag);
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}

static QString qtVersionFromAssetName(const QString &name)
{
    static const QRegularExpression re(QStringLiteral(R"(Qt(\d+\.\d+\.\d+))"));
    const QRegularExpressionMatch m = re.match(name);
    return m.hasMatch() ? m.captured(1) : QString();
}

static void platformArchFromName(const QString &name, QString *platform, QString *arch)
{
    const QString n = name.toLower();
    if (n.contains(u"_linux.") || n.contains(u"_linux.tar.gz") || n.contains(u"_linux.zip"))
        *platform = QStringLiteral("linux");
    else if (n.contains(u"_macos.") || n.contains(u"_macos.zip") || n.contains(u"_macos.tar.gz"))
        *platform = QStringLiteral("darwin");
    else if (n.contains(u"_win.") || n.contains(u"_win.zip"))
        *platform = QStringLiteral("windows");

    if (n.contains(u"arm64-v8a"))
        *arch = QStringLiteral("arm64-v8a");
    else if (n.contains(u"x86-64") || n.contains(u"x86_64"))
        *arch = QStringLiteral("x86_64");
}

static QString toUtcIsoZ(const QString &gitcodeIso)
{
    QDateTime dt = QDateTime::fromString(gitcodeIso, Qt::ISODate);
    if (!dt.isValid())
        return gitcodeIso;
    return dt.toUTC().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss")) + u'Z';
}

static QString contentTypeForFileName(const QString &name)
{
    const QString n = name.toLower();
    if (n.endsWith(u".zip"))
        return QStringLiteral("application/zip");
    if (n.endsWith(u".tar.gz"))
        return QStringLiteral("application/gzip");
    if (n.endsWith(u".tar.bz2"))
        return QStringLiteral("application/x-bzip2");
    if (n.endsWith(u".tar"))
        return QStringLiteral("application/x-tar");
    return {};
}

static QJsonObject buildCatalog(const QJsonArray &releasesRaw, const QString &repo, int minAlpha,
                                QString *error)
{
    Q_UNUSED(error);
    QJsonArray outReleases;

    for (const QJsonValue &rv : releasesRaw) {
        if (!rv.isObject())
            continue;
        const QJsonObject r = rv.toObject();
        const QString tag = r.value(QStringLiteral("tag_name")).toString();
        const int av = alphaVersionFromTag(tag);
        if (av < 0 || av < minAlpha)
            continue;

        QString qtVer;
        QJsonArray outAssets;
        const QJsonArray assets = r.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue &av2 : assets) {
            if (!av2.isObject())
                continue;
            const QJsonObject a = av2.toObject();
            const QString nm = a.value(QStringLiteral("name")).toString();
            const QString url = a.value(QStringLiteral("browser_download_url")).toString();
            if (nm.isEmpty() || url.isEmpty())
                continue;

            if (qtVer.isEmpty())
                qtVer = qtVersionFromAssetName(nm);

            const QString atype = a.value(QStringLiteral("type")).toString().toLower();
            const QString kind = atype == u"source" ? QStringLiteral("metadata")
                                                    : QStringLiteral("archive");

            QJsonObject o{
                {QStringLiteral("name"), nm},
                {QStringLiteral("downloadUrl"), url},
                {QStringLiteral("kind"), kind},
            };

            QString plat, arch;
            if (kind == QLatin1String("archive")) {
                platformArchFromName(nm, &plat, &arch);
                if (!plat.isEmpty() && !arch.isEmpty()) {
                    o.insert(QStringLiteral("platform"), plat);
                    o.insert(QStringLiteral("arch"), arch);
                    const QString ct = contentTypeForFileName(nm);
                    if (!ct.isEmpty())
                        o.insert(QStringLiteral("contentType"), ct);

                    outAssets.append(o);
                }
            }
        }

        QJsonObject rel{
            {QStringLiteral("id"), tag},
            {QStringLiteral("tag"), tag},
            {QStringLiteral("title"), r.value(QStringLiteral("name")).toString(tag)},
            {QStringLiteral("createdAt"), toUtcIsoZ(r.value(QStringLiteral("created_at")).toString())},
            {QStringLiteral("channel"), QStringLiteral("snapshot")},
            {QStringLiteral("summary"), r.value(QStringLiteral("body")).toString()},
            {QStringLiteral("notesUrl"),
             QStringLiteral("https://gitcode.com/%1/releases/%2").arg(repo, tag)},
            {QStringLiteral("assets"), outAssets},
        };
        if (!qtVer.isEmpty())
            rel.insert(QStringLiteral("qtVersion"), qtVer);

        outReleases.append(rel);
    }

    /* ** 按 createdAt 降序排序（ISO Z 格式可直接进行字符串比较）*/
    QVector<QJsonValue> rows;
    rows.reserve(outReleases.size());
    for (const QJsonValue &v : std::as_const(outReleases))
        rows.append(v);
    std::sort(rows.begin(), rows.end(), [](const QJsonValue &a, const QJsonValue &b) {
        const QString ca = a.toObject().value(QStringLiteral("createdAt")).toString();
        const QString cb = b.toObject().value(QStringLiteral("createdAt")).toString();
        return ca > cb;
    });
    QJsonArray sorted;
    for (const QJsonValue &v : rows)
        sorted.append(v);

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QJsonObject catalog{
        {QStringLiteral("schema"), QStringLiteral("qt-for-openharmony.binary-catalog")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("catalogId"), repo},
        {QStringLiteral("title"), QStringLiteral("Qt for OpenHarmony — GitCode releases (Alpha v%1+)")
                                      .arg(minAlpha)},
        {QStringLiteral("updatedAt"),
         now.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss")) + u'Z'},
        {QStringLiteral("homepage"),
         QStringLiteral("https://gitcode.com/%1/releases").arg(repo)},
        {QStringLiteral("releases"), sorted},
    };

    return catalog;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qt-oh-binary-catalog-generator"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser p;
    p.setApplicationDescription(
        QStringLiteral("Generate qt-for-openharmony.binary-catalog v1 JSON from GitCode API."));
    p.addHelpOption();
    p.addVersionOption();
    p.addOption(QCommandLineOption(
        QStringList() << "o" << "output",
        "Write catalog to <file> (UTF-8).",
        "file"));

    p.addOption(QCommandLineOption(
        QStringList() << "t" << "token",
        "GitCode private token (otherwise GITCODE_PRIVATE_TOKEN env).",
        "token"));

    p.addOption(QCommandLineOption(
        QStringList() << "repo",
        "repo path (owner/name), default openharmony-sig/qt.",
        "path"));

    p.addOption(QCommandLineOption(
        QStringList() << "min-alpha",
        "Minimum Alpha_vN tag to include (default 7).",
        "n"));

    p.process(app);

    QString token = p.value(QStringLiteral("token"));
    if (token.isEmpty()) {
        const QByteArray env = qgetenv("GITCODE_PRIVATE_TOKEN");
        token = QString::fromUtf8(env);
    }
    if (token.isEmpty()) {
        fputs(qPrintable(QStringLiteral("Error: set GITCODE_PRIVATE_TOKEN or pass --token.\n")),
              stderr);
        return 2;
    }

    QString repo = p.value(QStringLiteral("repo"));
    if (repo.isEmpty())
        repo = QString::fromLatin1(kDefaultRepo);

    int minAlpha = 7;
    if (p.isSet(QStringLiteral("min-alpha"))) {
        bool ok = false;
        minAlpha = p.value(QStringLiteral("min-alpha")).toInt(&ok);
        if (!ok || minAlpha < 0) {
            fputs("Error: --min-alpha must be a non-negative integer.\n", stderr);
            return 2;
        }
    }

    QString err;
    const QJsonArray raw = fetchAllReleases(repo, token.toUtf8(), &err);
    if (!err.isEmpty()) {
        fputs(qPrintable(QStringLiteral("Network/API error: %1\n").arg(err)), stderr);
        return 1;
    }

    const QJsonObject catalog = buildCatalog(raw, repo, minAlpha, &err);
    const QJsonDocument doc(catalog);
    const QByteArray json = doc.toJson(QJsonDocument::Indented);

    if (p.isSet(QStringLiteral("output"))) {
        const QString path = p.value(QStringLiteral("output"));
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            fputs(qPrintable(QStringLiteral("Cannot open %1 for write.\n").arg(path)), stderr);
            return 1;
        }
        f.write(json);
        if (!f.commit()) {
            fputs(qPrintable(QStringLiteral("Failed to save %1\n").arg(path)), stderr);
            return 1;
        }
    } else {
        fputs(json.constData(), stdout);
    }

    return 0;
}
