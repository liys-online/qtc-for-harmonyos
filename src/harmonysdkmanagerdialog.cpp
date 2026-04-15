// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonysdkmanagerdialog.h"
#include "harmonysdkarchiveutils.h"
#include "harmonysdkdownloader.h"
#include "harmonyconfigurations.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <coreplugin/icore.h>

#include <utils/id.h>
#include <utils/filepath.h>
#include <utils/layoutbuilder.h>
#include <utils/qtcassert.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTreeWidget>

#include <QCryptographicHash>

using namespace Utils;
using namespace Ohos;

namespace Ohos::Internal {

namespace {

QString formatSizeBytes(qint64 bytes)
{
    if (bytes <= 0)
        return QStringLiteral("—");
    const double mib = bytes / (1024.0 * 1024.0);
    return QString::number(mib, 'f', 2) + QLatin1String(" MiB");
}

QString fileNameForEntry(const HarmonySdkPackageEntry &e)
{
    const QString fromUrl = e.archive.url.fileName();
    if (!fromUrl.isEmpty())
        return fromUrl.split(QLatin1Char('?')).constFirst();
    return QStringLiteral("ohos-%1-%2.zip")
        .arg(e.apiVersion, e.componentPath);
}

/** Sort API version keys for tree: numeric descending (newer first), else lexicographic descending. */
bool apiVersionTreeOrder(const QString &a, const QString &b)
{
    bool okA = false;
    bool okB = false;
    const int ia = a.toInt(&okA);
    const int ib = b.toInt(&okB);
    if (okA && okB)
        return ia > ib;
    return QString::localeAwareCompare(a, b) > 0;
}

void collectCheckedEntryIndices(QTreeWidgetItem *item, QVector<int> *out)
{
    if (!item)
        return;
    if (item->childCount() == 0) {
        const QVariant v = item->data(0, Qt::UserRole);
        if (v.isValid() && item->checkState(0) == Qt::Checked)
            out->append(v.toInt());
        return;
    }
    for (int i = 0; i < item->childCount(); ++i)
        collectCheckedEntryIndices(item->child(i), out);
}

} // namespace

class HarmonySdkManagerDialog final : public QDialog
{
public:
    explicit HarmonySdkManagerDialog(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void appendLog(const QString &line);
    void setBusy(bool busy);
    void onRefreshList();
    void fillTree(const QVector<HarmonySdkPackageEntry> &entries);
    void onDownloadSelected();
    void startNextDownload();
    void flushAndCloseDownloadFile(QNetworkReply *reply);
    bool verifyDownloadChecksum(const FilePath &destPath, const HarmonySdkPackageEntry &e);
    void extractDownloadedEntry(const FilePath &destPath, const HarmonySdkPackageEntry &e);
    void onSingleDownloadFinished(const FilePath &destPath, const HarmonySdkPackageEntry &e);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void finishDownloadBatch();
    FilePaths collectValidSdkRoots(const FilePath &configRoot) const;
    void handleSingleSdkRoot(const FilePath &sdkRoot);
    void handleMultipleSdkRoots(const FilePaths &validRoots);
    void handleNoSdkRootsFound(const FilePath &configRoot);
    void refreshPathsHint();

    QLabel *m_pathsHintLabel = nullptr;
    QTreeWidget *m_tree = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_downloadBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
    HarmonySdkDownloader *m_listDownloader = nullptr;
    QNetworkAccessManager m_downloadNam;

    QVector<HarmonySdkPackageEntry> m_entries;
    QVector<int> m_downloadRows;
    int m_downloadQueueIndex = 0;
    QNetworkReply *m_activeDownload = nullptr;
    std::unique_ptr<QFile> m_downloadFile;
    std::unique_ptr<QCryptographicHash> m_downloadHash;

    FilePath m_batchSdkRoot;
    FilePath m_batchTempDir;
    QSet<QString> m_batchApiVersions;
};

HarmonySdkManagerDialog::HarmonySdkManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(Tr::tr("OpenHarmony SDK Manager"));
    resize(900, 600);

    m_pathsHintLabel = new QLabel(this);    // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_pathsHintLabel->setWordWrap(true);
    m_pathsHintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_tree = new QTreeWidget(this); // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({
        Tr::tr("Get"),
        Tr::tr("API"),
        Tr::tr("Component"),
        Tr::tr("Size"),
    });
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setRootIsDecorated(true);

    m_log = new QPlainTextEdit(this);   // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);

    m_progress = new QProgressBar(this);    // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_progress->setRange(0, 100);
    m_progress->setValue(0);

    m_refreshBtn = new QPushButton(Tr::tr("Refresh List"), this);   // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_downloadBtn = new QPushButton(Tr::tr("Download Selected"), this); // NOSONAR (cpp:S5025)
    m_closeBtn = new QPushButton(Tr::tr("Close"), this);    // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_refreshBtn->setDefault(true);

    m_listDownloader = new HarmonySdkDownloader(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
    connect(m_listDownloader, &HarmonySdkDownloader::packageListFetched, this,
            &HarmonySdkManagerDialog::fillTree);
    connect(m_listDownloader, &HarmonySdkDownloader::fetchFailed, this, [this](const QString &err) {
        setBusy(false);
        appendLog(err);
        QMessageBox::warning(this, Tr::tr("SDK List"), err);
    });

    connect(m_refreshBtn, &QPushButton::clicked, this, &HarmonySdkManagerDialog::onRefreshList);
    connect(m_downloadBtn, &QPushButton::clicked, this, &HarmonySdkManagerDialog::onDownloadSelected);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_tree, &QTreeWidget::itemChanged, this, &HarmonySdkManagerDialog::onTreeItemChanged);

    using namespace Layouting;

    Column {
        m_pathsHintLabel,
        m_tree,
        m_progress,
        Row { m_refreshBtn, st, m_downloadBtn, st, m_closeBtn },
        Tr::tr("Log:"),
        m_log,
    }.attachTo(this);

    refreshPathsHint();
    appendLog(Tr::tr("Click \"Refresh List\" to load packages."));
}

void HarmonySdkManagerDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    refreshPathsHint();
}

void HarmonySdkManagerDialog::refreshPathsHint()
{
    const FilePath root = HarmonyConfig::effectiveOhosSdkRoot().cleanPath();
    const FilePath tempDir = root.pathAppended(QStringLiteral(".temp"));
    if (HarmonyConfig::ohosSdkRoot().cleanPath().isEmpty()) {
        m_pathsHintLabel->setText(
            Tr::tr("No custom path saved yet — using default.\n"
                   "SDK root: %1\n• Downloads: %2\n• Unpack: %3/<api_version>/\n\n"
                   "Override under Harmony preferences if needed.")
                .arg(root.path(), tempDir.path(), root.path()));
    } else {
        m_pathsHintLabel->setText(
            Tr::tr("SDK root: %1\n• Downloads: %2\n• Unpack: %3/<api_version>/")
                .arg(root.path(), tempDir.path(), root.path()));
    }
}

void HarmonySdkManagerDialog::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void HarmonySdkManagerDialog::setBusy(bool busy)
{
    m_refreshBtn->setEnabled(!busy);
    m_downloadBtn->setEnabled(!busy);
    if (m_tree)
        m_tree->setEnabled(!busy);
    if (m_closeBtn)
        m_closeBtn->setEnabled(!busy);
}

void HarmonySdkManagerDialog::onRefreshList()
{
    const HarmonySdkDownloader::ListRequest req = HarmonySdkDownloader::defaultListRequest(HarmonySdkBackupMirror::GitCode);
    appendLog(Tr::tr("Requesting package list (POST %1)…").arg(req.primaryListPostUrl.toDisplayString()));
    setBusy(true);
    m_tree->clear();
    m_entries.clear();
    m_listDownloader->fetchPackageList(req);
}

void HarmonySdkManagerDialog::fillTree(const QVector<HarmonySdkPackageEntry> &entries)
{
    setBusy(false);
    m_entries = entries;

    QMap<QString, QVector<int>> byApi;
    for (int i = 0; i < entries.size(); ++i) {
        const QString v = entries.at(i).apiVersion.isEmpty()
                              ? Tr::tr("(unknown API)")
                              : entries.at(i).apiVersion;
        byApi[v].append(i);
    }

    QStringList apiKeys = byApi.keys();
    std::sort(apiKeys.begin(), apiKeys.end(), apiVersionTreeOrder);

    const QSignalBlocker blocker(m_tree);
    m_tree->clear();

    for (const QString &apiKey : std::as_const(apiKeys)) {
        const QVector<int> indices = byApi.value(apiKey);
        auto *parent = new QTreeWidgetItem(m_tree); // NOSONAR (cpp:S5025) - parented, will auto-delete
        parent->setText(1, apiKey);
        parent->setText(2, Tr::tr("%1 component(s)").arg(indices.size()));
        parent->setFlags(parent->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsAutoTristate);
        parent->setCheckState(0, Qt::Unchecked);
        parent->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

        for (int idx : indices) {
            const HarmonySdkPackageEntry &e = entries.at(idx);
            auto *child = new QTreeWidgetItem(parent);  // NOSONAR (cpp:S5025) - parented, will auto-delete
            child->setData(0, Qt::UserRole, idx);
            child->setFlags(child->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            child->setCheckState(0, Qt::Unchecked);
            child->setText(1, e.apiVersion);
            child->setText(2, e.componentPath);
            child->setText(3, formatSizeBytes(e.archive.size));
        }
    }

    m_tree->expandAll();
    for (int c = 0; c < m_tree->columnCount(); ++c)
        m_tree->resizeColumnToContents(c);

    appendLog(Tr::tr("Loaded %1 package(s) in %2 API group(s).")
                  .arg(entries.size())
                  .arg(apiKeys.size()));
}

void HarmonySdkManagerDialog::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0 || !item)
        return;
    /* ** 顶层 API 行：仅在完全选中/取消选中时同步（不处理子项混合状态）。 */
    if (item->parent() == nullptr && item->childCount() > 0) {
        const Qt::CheckState st = item->checkState(0);
        if (st == Qt::PartiallyChecked)
            return;
        const QSignalBlocker blocker(m_tree);
        for (int i = 0; i < item->childCount(); ++i)
            item->child(i)->setCheckState(0, st);
    }
}

void HarmonySdkManagerDialog::onDownloadSelected()
{
    if (m_activeDownload) {
        appendLog(Tr::tr("A download is already in progress."));
        return;
    }
    const FilePath sdkRoot = HarmonyConfig::effectiveOhosSdkRoot().cleanPath();
    if (!sdkRoot.exists() && !sdkRoot.createDir()) {
        QMessageBox::warning(this, Tr::tr("Download"),
                             Tr::tr("Cannot create HarmonyOS SDK root:\n%1")
                                 .arg(sdkRoot.toUserOutput()));
        return;
    }
    if (!sdkRoot.isWritableDir()) {
        QMessageBox::warning(this, Tr::tr("Download"),
                             Tr::tr("HarmonyOS SDK root is not writable:\n%1")
                                 .arg(sdkRoot.toUserOutput()));
        return;
    }

    const FilePath tempDir = sdkRoot.pathAppended(QStringLiteral(".temp"));
    if (!tempDir.exists() && !tempDir.createDir()) {
        QMessageBox::warning(this, Tr::tr("Download"),
                             Tr::tr("Cannot create download folder:\n%1").arg(tempDir.toUserOutput()));
        return;
    }

    QVector<int> rows;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        collectCheckedEntryIndices(m_tree->topLevelItem(i), &rows);

    if (rows.isEmpty()) {
        QMessageBox::information(this, Tr::tr("Download"),
                                 Tr::tr("Check one or more packages (under an API version) in the "
                                        "first column."));
        return;
    }

    m_batchSdkRoot = sdkRoot;
    m_batchTempDir = tempDir;
    m_batchApiVersions.clear();
    appendLog(Tr::tr("Download folder: %1").arg(m_batchTempDir.toUserOutput()));

    m_downloadRows = rows;
    m_downloadQueueIndex = 0;
    m_progress->setValue(0);
    appendLog(Tr::tr("Starting %1 download(s)…").arg(rows.size()));
    setBusy(true);
    startNextDownload();
}

void HarmonySdkManagerDialog::flushAndCloseDownloadFile(QNetworkReply *reply)
{
    if (!m_downloadFile)
        return;
    if (reply) {
        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty())
            m_downloadFile->write(tail);
    }
    m_downloadFile->flush();
    m_downloadFile->close();
}

bool HarmonySdkManagerDialog::verifyDownloadChecksum(const FilePath &destPath,
                                                      const HarmonySdkPackageEntry &e)
{
    const QByteArray digest = m_downloadHash ? m_downloadHash->result() : QByteArray();
    const QString hex = QString::fromLatin1(digest.toHex());
    const QString expected = e.archive.sha256Checksum.trimmed();
    if (expected.isEmpty()) {
        appendLog(Tr::tr("No checksum in index; skipped verification."));
        return true;
    }
    if (hex.compare(expected, Qt::CaseInsensitive) == 0) {
        appendLog(Tr::tr("SHA-256 OK."));
        return true;
    }
    appendLog(Tr::tr("SHA-256 mismatch: expected %1, got %2").arg(expected, hex));
    m_downloadFile.reset();
    QFile::remove(destPath.toFSPathString());
    return false;
}

void HarmonySdkManagerDialog::extractDownloadedEntry(const FilePath &destPath,
                                                      const HarmonySdkPackageEntry &e)
{
    if (m_batchSdkRoot.isEmpty())
        return;
    QString apiKey = e.apiVersion.trimmed();
    if (apiKey.isEmpty())
        apiKey = QStringLiteral("unknown");
    const FilePath extractRoot = m_batchSdkRoot.pathAppended(apiKey);
    if (!extractRoot.exists() && !extractRoot.createDir()) {
        appendLog(Tr::tr("Cannot create unpack directory: %1").arg(extractRoot.toUserOutput()));
        return;
    }
    QString exErr;
    if (extractHarmonySdkArchive(destPath, extractRoot, &exErr)) {
        appendLog(Tr::tr("Extracted with tar into %1").arg(extractRoot.toUserOutput()));
        m_batchApiVersions.insert(apiKey);
    } else {
        appendLog(Tr::tr("Extract failed for %1: %2").arg(destPath.fileName(), exErr));
    }
}

void HarmonySdkManagerDialog::onSingleDownloadFinished(const FilePath &destPath,
                                                        const HarmonySdkPackageEntry &e)
{
    QNetworkReply *reply = m_activeDownload;
    m_activeDownload = nullptr;
    flushAndCloseDownloadFile(reply);

    if (!reply || reply->error() != QNetworkReply::NoError) {
        appendLog(Tr::tr("Network error: %1").arg(reply ? reply->errorString() : QString()));
        if (reply)
            reply->deleteLater();
        m_downloadFile.reset();
        m_downloadHash.reset();
        m_downloadQueueIndex++;
        startNextDownload();
        return;
    }

    if (!verifyDownloadChecksum(destPath, e)) {
        reply->deleteLater();
        m_downloadHash.reset();
        m_downloadQueueIndex++;
        startNextDownload();
        return;
    }

    extractDownloadedEntry(destPath, e);
    reply->deleteLater();
    m_downloadFile.reset();
    m_downloadHash.reset();
    m_downloadQueueIndex++;
    startNextDownload();
}

void HarmonySdkManagerDialog::startNextDownload()
{
    if (m_activeDownload) {
        m_activeDownload->disconnect();
        m_activeDownload->abort();
        m_activeDownload->deleteLater();
        m_activeDownload = nullptr;
    }
    m_downloadFile.reset();
    m_downloadHash.reset();

    if (m_downloadQueueIndex >= m_downloadRows.size()) {
        finishDownloadBatch();
        return;
    }

    const int row = m_downloadRows.at(m_downloadQueueIndex);
    QTC_ASSERT(row >= 0 && row < m_entries.size(), setBusy(false); return );
    const HarmonySdkPackageEntry &e = m_entries.at(row);
    const QString fname = fileNameForEntry(e);
    const FilePath destPath = m_batchTempDir.pathAppended(fname);

    m_progress->setValue(0);
    appendLog(Tr::tr("Downloading [%1 / %2]: %3")
                  .arg(QString::number(m_downloadQueueIndex + 1),
                       QString::number(m_downloadRows.size()),
                       destPath.toUserOutput()));

    m_downloadFile = std::make_unique<QFile>(destPath.toFSPathString());
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        appendLog(Tr::tr("Failed to open file for writing: %1").arg(destPath.toUserOutput()));
        m_downloadQueueIndex++;
        startNextDownload();
        return;
    }

    m_downloadHash = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);

    QNetworkRequest request(e.archive.url);
    m_activeDownload = m_downloadNam.get(request);

    connect(m_activeDownload, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                m_progress->setValue(total > 0 ? int(received * 100 / total) : 0);
            });

    connect(m_activeDownload, &QNetworkReply::readyRead, this, [this] {
        if (!m_activeDownload || !m_downloadFile || !m_downloadHash)
            return;
        const QByteArray chunk = m_activeDownload->readAll();
        m_downloadFile->write(chunk);
        m_downloadHash->addData(chunk);
    });

    connect(m_activeDownload, &QNetworkReply::finished, this, [this, e, destPath] {
        if (QPointer<HarmonySdkManagerDialog>(this).isNull())
            return;
        onSingleDownloadFinished(destPath, e);
    });
}

static QString joinPaths(const FilePaths &paths)
{
    QStringList sl;
    sl.reserve(paths.size());
    for (const FilePath &p : paths)
        sl << p.toUserOutput();
    return sl.join(QLatin1Char('\n'));
}

FilePaths HarmonySdkManagerDialog::collectValidSdkRoots(const FilePath &configRoot) const
{
    FilePaths validRoots;
    if (configRoot.isEmpty())
        return validRoots;
    for (const QString &api : std::as_const(m_batchApiVersions)) {
        const FilePath apiDir = configRoot.pathAppended(api);
        FilePath detected = findOhosSdkRootUnder(apiDir);
        if (detected.isEmpty() && HarmonyConfig::isValidSdk(apiDir))
            detected = apiDir;
        if (!detected.isEmpty() && !validRoots.contains(detected))
            validRoots.append(detected);
    }
    return validRoots;
}

void HarmonySdkManagerDialog::handleSingleSdkRoot(const FilePath &sdkRoot)
{
    appendLog(Tr::tr("Detected SDK root: %1").arg(sdkRoot.toUserOutput()));
    if (!HarmonyConfig::isValidSdk(sdkRoot)) {
        QMessageBox::warning(this, Tr::tr("Download"),
                             Tr::tr("Downloads finished, but the path does not look like a complete SDK:\n%1")
                                 .arg(sdkRoot.toUserOutput()));
        return;
    }
    QMessageBox mbox(QMessageBox::Information,
                     Tr::tr("Download"),
                     Tr::tr("Downloads finished.\n\n"
                            "An OpenHarmony SDK was detected at:\n%1\n\n"
                            "Add it to the Harmony SDK list now?")
                         .arg(sdkRoot.toUserOutput()),
                     QMessageBox::NoButton, this);
    const auto *addBtn  = mbox.addButton(Tr::tr("Add to SDK List"),       QMessageBox::AcceptRole);
    const auto *openBtn = mbox.addButton(Tr::tr("Open Harmony Settings"), QMessageBox::ActionRole);
    mbox.addButton(QMessageBox::Close);
    mbox.exec();
    if (mbox.clickedButton() == addBtn) {
        HarmonyConfig::addSdk(sdkRoot.path());
        HarmonyConfigurations::applyConfig();
        appendLog(Tr::tr("SDK path registered: %1").arg(sdkRoot.toUserOutput()));
        QMessageBox::information(this, Tr::tr("Harmony SDK"),
                                 Tr::tr("The SDK was added. Kits and toolchains will refresh."));
    } else if (mbox.clickedButton() == openBtn) {
        Core::ICore::showSettings(Id(Constants::HARMONY_SETTINGS_ID));
    }
}

void HarmonySdkManagerDialog::handleMultipleSdkRoots(const FilePaths &validRoots)
{
    appendLog(Tr::tr("Multiple SDK roots detected."));
    QMessageBox mbox(QMessageBox::Information,
                     Tr::tr("Download"),
                     Tr::tr("Downloads finished.\n\n"
                            "Multiple SDK roots were detected:\n%1\n\n"
                            "Add each path under Harmony settings if needed.")
                         .arg(joinPaths(validRoots)),
                     QMessageBox::NoButton, this);
    const auto *openBtn = mbox.addButton(Tr::tr("Open Harmony Settings"), QMessageBox::AcceptRole);
    mbox.addButton(QMessageBox::Close);
    mbox.exec();
    if (mbox.clickedButton() == openBtn)
        Core::ICore::showSettings(Id(Constants::HARMONY_SETTINGS_ID));
}

void HarmonySdkManagerDialog::handleNoSdkRootsFound(const FilePath &configRoot)
{
    const QString msg = !configRoot.isEmpty()
        ? Tr::tr("Downloads finished.\n\n"
                 "No complete SDK root was found under:\n%1/<API version>/\n\n"
                 "If archives use another layout, merge manually, "
                 "then use Harmony settings \u2192 Add\u2026").arg(configRoot.toUserOutput())
        : Tr::tr("Downloads finished.\n\n"
                 "Enable extraction or add the SDK path under Harmony settings.");
    QMessageBox mbox(QMessageBox::Information, Tr::tr("Download"), msg,
                     QMessageBox::NoButton, this);
    const auto *openBtn = mbox.addButton(Tr::tr("Open Harmony Settings"), QMessageBox::AcceptRole);
    mbox.addButton(QMessageBox::Close);
    mbox.exec();
    if (mbox.clickedButton() == openBtn)
        Core::ICore::showSettings(Id(Constants::HARMONY_SETTINGS_ID));
}

void HarmonySdkManagerDialog::finishDownloadBatch()
{
    m_progress->setValue(100);
    setBusy(false);

    const FilePath configRoot = m_batchSdkRoot.isEmpty()
                                    ? HarmonyConfig::effectiveOhosSdkRoot().cleanPath()
                                    : m_batchSdkRoot;

    if (!configRoot.isEmpty() && HarmonyConfig::registerDownloadedSdksUnder(configRoot) > 0)
        HarmonyConfigurations::applyConfig();

    const FilePaths validRoots = collectValidSdkRoots(configRoot);

    if (validRoots.size() == 1)
        handleSingleSdkRoot(validRoots.first());
    else if (validRoots.size() > 1)
        handleMultipleSdkRoots(validRoots);
    else
        handleNoSdkRootsFound(configRoot);

    m_batchSdkRoot = {};
    m_batchTempDir = {};
    m_batchApiVersions.clear();
}

void executeHarmonySdkManagerDialog(QWidget *parent)
{
    QWidget *p = parent ? parent : Core::ICore::dialogParent();
    HarmonySdkManagerDialog dlg(p);
    dlg.exec();
}

} // namespace Ohos::Internal
