// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonyqttsdkmanagerdialog.h"
#include "harmonyqttreleasesdownloader.h"
#include "harmonyconfigurations.h"
#include "harmonysdkarchiveutils.h"
#include "ohostr.h"
#include "ohosconstants.h"

#include <coreplugin/icore.h>

#include <utils/filepath.h>
#include <utils/hostosinfo.h>
#include <utils/id.h>
#include <utils/layoutbuilder.h>
#include <utils/pathchooser.h>
#include <utils/qtcassert.h>

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QUrl>

using namespace Utils;

namespace Ohos::Internal {

namespace {

QString catalogPlatformForHost()
{
    if (HostOsInfo::isMacHost())
        return QStringLiteral("darwin");
    if (HostOsInfo::isWindowsHost())
        return QStringLiteral("windows");
    if (HostOsInfo::isLinuxHost())
        return QStringLiteral("linux");
    return QStringLiteral("linux");
}

bool shouldShowCatalogAssetForHost(const QtForOhReleaseAsset &asset)
{
    const QString kind = asset.assetKind.toLower();
    if (kind == QStringLiteral("metadata"))
        return false;

    const QString hostPlat = catalogPlatformForHost();

    if (asset.catalogPlatform.isEmpty())
        return false;

    if (asset.catalogPlatform != hostPlat)
        return false;

    // catalog `arch` is OpenHarmony *target* ABI (e.g. arm64 device vs x86_64 emulator), not host CPU.
    return true;
}

void collectCheckedAssetIndices(QTreeWidgetItem *item, QList<QPair<int, int>> *out)
{
    if (!item || !out)
        return;
    if (item->childCount() == 0) {
        if (item->checkState(0) != Qt::Checked)
            return;
        const QVariant vr = item->data(0, Qt::UserRole);
        const QVariant va = item->data(0, Qt::UserRole + 1);
        if (vr.isValid() && va.isValid())
            out->append({vr.toInt(), va.toInt()});
        return;
    }
    for (int i = 0; i < item->childCount(); ++i)
        collectCheckedAssetIndices(item->child(i), out);
}

QString safeFileName(const QString &n)
{
    QString s = n;
    s.replace(QLatin1Char('/'), QLatin1Char('_'));
    s.replace(QLatin1Char('\\'), QLatin1Char('_'));
    return s;
}

} // namespace

class HarmonyQtOhSdkManagerDialog final : public QDialog
{
public:
    explicit HarmonyQtOhSdkManagerDialog(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *e) override;

private:
    void appendLog(const QString &line);
    void setBusy(bool busy);
    void onRefreshList();
    void fillTree(const QVector<QtForOhRelease> &releases);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void onDownloadSelected();
    void startNextDownload();
    void finishDownloadBatch();
    void tryRegisterExtractedQtOhQmake(const FilePath &extractRoot);
    FilePath defaultInstallRoot() const;

    QLabel *m_hintLabel = nullptr;
    QPushButton *m_openGitCodeBtn = nullptr;
    PathChooser *m_installRootChooser = nullptr;
    QTreeWidget *m_tree = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_downloadBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QCheckBox *m_autoExtractCheck = nullptr;

    HarmonyQtOhReleasesDownloader *m_listDownloader = nullptr;
    QNetworkAccessManager m_downloadNam;

    QVector<QtForOhRelease> m_releases;
    QList<QPair<int, int>> m_downloadQueue;
    int m_downloadIndex = 0;
    QNetworkReply *m_activeDownload = nullptr;
    std::unique_ptr<QFile> m_downloadFile;
    FilePath m_batchTempDir;
    FilePath m_batchInstallRoot;
    int m_qmakesAddedInBatch = 0;
};

FilePath HarmonyQtOhSdkManagerDialog::defaultInstallRoot() const
{
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return FilePath::fromUserInput(docs + QStringLiteral("/QtForOpenHarmony")).cleanPath();
}

HarmonyQtOhSdkManagerDialog::HarmonyQtOhSdkManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(Tr::tr("Qt for OpenHarmony SDK Manager"));
    resize(920, 620);

    m_hintLabel = new QLabel;
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setTextFormat(Qt::RichText);
    m_hintLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_hintLabel->setOpenExternalLinks(true);

    m_openGitCodeBtn = new QPushButton(Tr::tr("Open GitCode Releases Page…"));
    connect(m_openGitCodeBtn, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(
            "https://gitcode.com/openharmony-sig/qt/releases")));
    });

    m_installRootChooser = new PathChooser;
    m_installRootChooser->setExpectedKind(PathChooser::Directory);
    m_installRootChooser->setPromptDialogTitle(Tr::tr("Select Qt for OpenHarmony Install Root"));
    m_installRootChooser->setFilePath(defaultInstallRoot());

    m_autoExtractCheck = new QCheckBox(
        Tr::tr("After each download, extract with the system \"tar\" when the format is supported "
               "(.zip / .tar / .tar.gz, same as OpenHarmony SDK Manager), then search for "
               "<code>bin/qmake</code> and add it to <i>Qt for Harmony qmake list</i> when found. "
               "Split .7z parts must be merged with 7-Zip manually."));
    m_autoExtractCheck->setChecked(true);

    m_tree = new QTreeWidget;
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({
        Tr::tr("Get"),
        Tr::tr("Release"),
        Tr::tr("Package"),
    });
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(400);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);

    m_refreshBtn = new QPushButton(Tr::tr("Refresh List"));
    m_downloadBtn = new QPushButton(Tr::tr("Download Selected"));
    m_closeBtn = new QPushButton(Tr::tr("Close"));
    m_refreshBtn->setDefault(true);

    m_listDownloader = new HarmonyQtOhReleasesDownloader(this);
    connect(m_listDownloader, &HarmonyQtOhReleasesDownloader::releasesFetched, this,
            &HarmonyQtOhSdkManagerDialog::fillTree);
    connect(m_listDownloader, &HarmonyQtOhReleasesDownloader::fetchFailed, this,
            [this](const QString &err) {
                setBusy(false);
                appendLog(err);
                QMessageBox::warning(this, Tr::tr("Releases"), err);
            });

    connect(m_refreshBtn, &QPushButton::clicked, this, &HarmonyQtOhSdkManagerDialog::onRefreshList);
    connect(m_downloadBtn, &QPushButton::clicked, this, &HarmonyQtOhSdkManagerDialog::onDownloadSelected);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_tree, &QTreeWidget::itemChanged, this, &HarmonyQtOhSdkManagerDialog::onTreeItemChanged);

    m_hintLabel->setText(
        Tr::tr("Release list is loaded from the online catalog. Only packages for this platform are shown."));

    using namespace Layouting;

    Column {
        m_hintLabel,
        Row { m_openGitCodeBtn, st },
        Tr::tr("Install / download root:"),
        m_installRootChooser,
        m_autoExtractCheck,
        m_tree,
        m_progress,
        Row { m_refreshBtn, st, m_downloadBtn, st, m_closeBtn },
        Tr::tr("Log:"),
        m_log,
    }.attachTo(this);

    appendLog(Tr::tr("Click \"Refresh List\" to load releases (GitCode catalog, GitHub fallback)."));
}

void HarmonyQtOhSdkManagerDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
}

void HarmonyQtOhSdkManagerDialog::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void HarmonyQtOhSdkManagerDialog::setBusy(bool busy)
{
    m_refreshBtn->setEnabled(!busy);
    m_downloadBtn->setEnabled(!busy);
    m_closeBtn->setEnabled(!busy);
    m_tree->setEnabled(!busy);
    m_installRootChooser->setEnabled(!busy);
    if (m_autoExtractCheck)
        m_autoExtractCheck->setEnabled(!busy);
    if (m_openGitCodeBtn)
        m_openGitCodeBtn->setEnabled(!busy);
}

void HarmonyQtOhSdkManagerDialog::onRefreshList()
{
    appendLog(Tr::tr("Requesting catalog…"));
    setBusy(true);
    m_tree->clear();
    m_releases.clear();
    m_listDownloader->fetchAllReleases();
}

void HarmonyQtOhSdkManagerDialog::fillTree(const QVector<QtForOhRelease> &releases)
{
    setBusy(false);
    m_releases = releases;

    const QSignalBlocker blocker(m_tree);
    m_tree->clear();

    int releasesShown = 0;
    for (int ri = 0; ri < m_releases.size(); ++ri) {
        const QtForOhRelease &rel = m_releases.at(ri);
        int visibleAssets = 0;
        for (int ai = 0; ai < rel.assets.size(); ++ai) {
            if (shouldShowCatalogAssetForHost(rel.assets.at(ai)))
                ++visibleAssets;
        }
        if (visibleAssets == 0)
            continue;

        ++releasesShown;
        auto *parent = new QTreeWidgetItem(m_tree);
        const QString relLabel = rel.title.isEmpty() ? rel.tagName : rel.title;
        parent->setText(1, relLabel);
        QString tagCol = rel.tagName;
        if (!rel.qtVersion.isEmpty())
            tagCol = QStringLiteral("%1 (%2)").arg(rel.tagName, rel.qtVersion);
        parent->setText(2, QStringLiteral("%1 · %2 %3")
                                .arg(tagCol,
                                     QString::number(visibleAssets),
                                     Tr::tr("file(s)")));
        parent->setFlags(parent->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate
                         | Qt::ItemIsEnabled);
        parent->setCheckState(0, Qt::Unchecked);

        for (int ai = 0; ai < rel.assets.size(); ++ai) {
            const QtForOhReleaseAsset &as = rel.assets.at(ai);
            if (!shouldShowCatalogAssetForHost(as))
                continue;
            auto *ch = new QTreeWidgetItem(parent);
            ch->setData(0, Qt::UserRole, ri);
            ch->setData(0, Qt::UserRole + 1, ai);
            ch->setFlags(ch->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            ch->setCheckState(0, Qt::Unchecked);
            ch->setText(1, relLabel);
            ch->setText(2, as.name);
        }
    }

    m_tree->expandAll();
    for (int c = 0; c < m_tree->columnCount(); ++c)
        m_tree->resizeColumnToContents(c);

    appendLog(Tr::tr("Loaded %1 release(s) with packages for this host OS (catalog platform: %2).")
                  .arg(QString::number(releasesShown), catalogPlatformForHost()));
}

void HarmonyQtOhSdkManagerDialog::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0 || !item)
        return;
    if (item->parent() == nullptr && item->childCount() > 0) {
        const Qt::CheckState st = item->checkState(0);
        if (st == Qt::PartiallyChecked)
            return;
        const QSignalBlocker blocker(m_tree);
        for (int i = 0; i < item->childCount(); ++i)
            item->child(i)->setCheckState(0, st);
    }
}

void HarmonyQtOhSdkManagerDialog::onDownloadSelected()
{
    if (m_activeDownload) {
        appendLog(Tr::tr("A download is already in progress."));
        return;
    }

    const FilePath root = m_installRootChooser->filePath().cleanPath();
    if (root.isEmpty()) {
        QMessageBox::warning(this, Tr::tr("Download"), Tr::tr("Choose an install root directory."));
        return;
    }
    if (!root.exists() && !root.createDir()) {
        QMessageBox::warning(this, Tr::tr("Download"),
                             Tr::tr("Cannot create directory:\n%1").arg(root.toUserOutput()));
        return;
    }
    if (!root.isWritableDir()) {
        QMessageBox::warning(this, Tr::tr("Download"),
                             Tr::tr("Directory is not writable:\n%1").arg(root.toUserOutput()));
        return;
    }

    const FilePath tempDir = root.pathAppended(QStringLiteral(".temp"));
    if (!tempDir.exists() && !tempDir.createDir()) {
        QMessageBox::warning(this, Tr::tr("Download"),
                             Tr::tr("Cannot create download folder:\n%1").arg(tempDir.toUserOutput()));
        return;
    }

    QList<QPair<int, int>> checked;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        collectCheckedAssetIndices(m_tree->topLevelItem(i), &checked);

    if (checked.isEmpty()) {
        QMessageBox::information(this, Tr::tr("Download"),
                                 Tr::tr("Select one or more assets in the first column."));
        return;
    }

    m_batchInstallRoot = root;
    m_batchTempDir = tempDir;
    m_qmakesAddedInBatch = 0;
    m_downloadQueue = checked;
    m_downloadIndex = 0;
    m_progress->setValue(0);
    appendLog(Tr::tr("Download folder: %1").arg(tempDir.toUserOutput()));
    appendLog(Tr::tr("Starting %1 download(s)…").arg(checked.size()));
    setBusy(true);
    startNextDownload();
}

void HarmonyQtOhSdkManagerDialog::startNextDownload()
{
    if (m_activeDownload) {
        m_activeDownload->disconnect();
        m_activeDownload->abort();
        m_activeDownload->deleteLater();
        m_activeDownload = nullptr;
    }
    m_downloadFile.reset();

    if (m_downloadIndex >= m_downloadQueue.size()) {
        finishDownloadBatch();
        return;
    }

    const QPair<int, int> id = m_downloadQueue.at(m_downloadIndex);
    QTC_ASSERT(id.first >= 0 && id.first < m_releases.size(), setBusy(false); return );
    const QtForOhRelease &rel = m_releases.at(id.first);
    QTC_ASSERT(id.second >= 0 && id.second < rel.assets.size(), setBusy(false); return );
    const QtForOhReleaseAsset &as = rel.assets.at(id.second);

    const QString fname = safeFileName(as.name);
    const FilePath destPath = m_batchTempDir.pathAppended(fname);

    m_progress->setValue(0);
    appendLog(Tr::tr("Downloading [%1 / %2]: %3")
                  .arg(QString::number(m_downloadIndex + 1),
                       QString::number(m_downloadQueue.size()),
                       destPath.toUserOutput()));

    m_downloadFile = std::make_unique<QFile>(destPath.toFSPathString());
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        appendLog(Tr::tr("Failed to open file for writing: %1").arg(destPath.toUserOutput()));
        m_downloadIndex++;
        startNextDownload();
        return;
    }

    QNetworkRequest request(as.browserDownloadUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QtCreator-Harmony-Plugin"));
    m_activeDownload = m_downloadNam.get(request);

    connect(m_activeDownload, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total > 0)
                    m_progress->setValue(int(received * 100 / total));
                else
                    m_progress->setValue(0);
            });

    connect(m_activeDownload, &QNetworkReply::readyRead, this, [this] {
        if (!m_activeDownload || !m_downloadFile)
            return;
        m_downloadFile->write(m_activeDownload->readAll());
    });

    connect(m_activeDownload, &QNetworkReply::finished, this, [this, destPath, rel, as] {
        QPointer<HarmonyQtOhSdkManagerDialog> guard(this);
        if (!guard)
            return;

        QNetworkReply *reply = m_activeDownload;
        m_activeDownload = nullptr;

        if (m_downloadFile && reply) {
            const QByteArray tail = reply->readAll();
            if (!tail.isEmpty())
                m_downloadFile->write(tail);
            m_downloadFile->flush();
            m_downloadFile->close();
        } else if (m_downloadFile) {
            m_downloadFile->flush();
            m_downloadFile->close();
        }

        const bool netOk = reply->error() == QNetworkReply::NoError;
        if (!netOk) {
            appendLog(Tr::tr("Network error: %1").arg(reply->errorString()));
            reply->deleteLater();
            m_downloadFile.reset();
            QFile::remove(destPath.toFSPathString());
            m_downloadIndex++;
            startNextDownload();
            return;
        }
        reply->deleteLater();
        appendLog(Tr::tr("Saved: %1").arg(destPath.toUserOutput()));

        if (m_autoExtractCheck && m_autoExtractCheck->isChecked()) {
            const QString lower = destPath.fileName().toLower();
            const bool maybe7z = lower.contains(QLatin1String(".7z"));
            if (maybe7z) {
                appendLog(Tr::tr("Skipping automatic extract for 7z; merge parts with 7-Zip if needed, "
                                 "then extract manually."));
            } else {
                const FilePath extractRoot = m_batchInstallRoot.pathAppended(
                    safeFileName(rel.tagName));
                if (!extractRoot.exists() && !extractRoot.createDir()) {
                    appendLog(
                        Tr::tr("Cannot create unpack directory: %1").arg(extractRoot.toUserOutput()));
                } else {
                    QString exErr;
                    const bool extracted = extractHarmonySdkArchive(destPath, extractRoot, &exErr);
                    if (extracted) {
                        appendLog(Tr::tr("Extracted into %1").arg(extractRoot.toUserOutput()));
                        tryRegisterExtractedQtOhQmake(extractRoot);
                    } else {
                        appendLog(Tr::tr("Extract skipped or failed: %1").arg(exErr));
                    }
                }
            }
        }

        m_downloadFile.reset();
        m_downloadIndex++;
        startNextDownload();
    });
}

void HarmonyQtOhSdkManagerDialog::tryRegisterExtractedQtOhQmake(const FilePath &extractRoot)
{
    const FilePath qmakePath = findQtOhQmakeUnder(extractRoot);
    if (qmakePath.isEmpty()) {
        appendLog(Tr::tr("No bin/qmake found under %1. If this archive is not a full Qt prefix, "
                         "add qmake manually in Harmony settings.")
                      .arg(extractRoot.toUserOutput()));
        return;
    }

    const QString qs = qmakePath.toFSPathString();
    if (HarmonyConfig::getQmakeList().contains(qs)) {
        appendLog(Tr::tr("qmake path already in Qt for Harmony list: %1")
                      .arg(qmakePath.toUserOutput()));
        return;
    }

    HarmonyConfig::addQmake(qs);
    ++m_qmakesAddedInBatch;
    HarmonyConfigurations::applyConfig();
    appendLog(Tr::tr("Registered qmake for Qt for Harmony: %1").arg(qmakePath.toUserOutput()));
}

void HarmonyQtOhSdkManagerDialog::finishDownloadBatch()
{
    m_progress->setValue(100);
    setBusy(false);
    appendLog(Tr::tr("Batch finished."));
    if (m_qmakesAddedInBatch > 0) {
        QMessageBox::information(
            this,
            Tr::tr("Qt for OpenHarmony"),
            Tr::tr("Downloads finished.\n\nRegistered %1 new qmake path(s) in Qt for Harmony settings.")
                .arg(QString::number(m_qmakesAddedInBatch)));
    } else {
        QMessageBox::information(
            this,
            Tr::tr("Qt for OpenHarmony"),
            Tr::tr("Downloads finished.\n\nIf needed, add the kit’s <b>qmake</b> under "
                   "<i>Qt for Harmony qmake list</i> in Harmony settings."));
    }
}

void executeHarmonyQtOhSdkManagerDialog(QWidget *parent)
{
    HarmonyQtOhSdkManagerDialog dlg(parent);
    dlg.exec();
}

} // namespace Ohos::Internal
