// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonysdkarchiveutils.h"
#include "harmonyconfigurations.h"
#include "ohostr.h"

#include <utils/commandline.h>
#include <utils/qtcprocess.h>

#include <QDir>
#include <QFile>

#include <algorithm>
#include <chrono>

using namespace Utils;

namespace Ohos::Internal {

namespace {

bool isTarSupportedArchiveFileName(const QString &lowerName)
{
    return lowerName.endsWith(QLatin1String(".zip"))
           || lowerName.endsWith(QLatin1String(".tar"))
           || lowerName.endsWith(QLatin1String(".tar.gz"))
           || lowerName.endsWith(QLatin1String(".tgz"))
           || lowerName.endsWith(QLatin1String(".tar.bz2"))
           || lowerName.endsWith(QLatin1String(".tbz2"));
}

bool extractHarmonySdkArchiveOnce(const FilePath &archive, const FilePath &destDir, QString *errorMessage)
{
    if (!archive.isReadableFile()) {
        if (errorMessage) {
            *errorMessage = Ohos::Tr::tr("Archive is not readable: %1").arg(archive.toUserOutput());
        }
        return false;
    }

    const QString lower = archive.fileName().toLower();
    if (!isTarSupportedArchiveFileName(lower)) {
        if (errorMessage) {
            *errorMessage = Ohos::Tr::tr("Unsupported archive type: %1\n"
                                         "Extract manually, then add the SDK root in Harmony settings.")
                                .arg(archive.fileName());
        }
        return false;
    }

    Process proc;
    proc.setCommand(CommandLine(
        FilePath::fromUserInput(QStringLiteral("tar")),
        QStringList({QStringLiteral("-xf"),
                     archive.toFSPathString(),
                     QStringLiteral("-C"),
                     destDir.toFSPathString()})));
    /* ** Process::runBlocking() 默认超时为 10s —— Qt/OH SDK 归档文件需要更长时间。 */
    proc.runBlocking(std::chrono::hours(1));

    if (proc.result() != ProcessResult::FinishedWithSuccess) {
        if (errorMessage) {
            *errorMessage = Ohos::Tr::tr("tar failed: %1\n%2")
                                .arg(proc.exitMessage(), proc.allOutput());
        }
        return false;
    }

    return true;
}

static FilePaths collectShallowNestedArchives(const FilePath &destDir)
{
    FilePaths archives;
    if (!destDir.isReadableDir())
        return archives;

    const FilePaths topFiles = destDir.dirEntries(QDir::Files | QDir::NoSymLinks);
    for (const FilePath &f : topFiles) {
        if (isTarSupportedArchiveFileName(f.fileName().toLower()))
            archives.append(f);
    }

    const FilePaths subdirs = destDir.dirEntries(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const FilePath &d : subdirs) {
        if (d.fileName() == QStringLiteral(".temp"))
            continue;
        if (!d.isReadableDir())
            continue;
        const FilePaths childFiles = d.dirEntries(QDir::Files | QDir::NoSymLinks);
        for (const FilePath &f : childFiles) {
            if (isTarSupportedArchiveFileName(f.fileName().toLower()))
                archives.append(f);
        }
    }

    return archives;
}

static bool extractAndRemoveNestedArchive(const FilePath &arch, QString *errorMessage)
{
    if (!arch.isReadableFile())
        return true; // skip, not an error

    const FilePath unpackInto = arch.parentDir();
    if (!unpackInto.isReadableDir() && !unpackInto.createDir()) {
        if (errorMessage)
            *errorMessage = Ohos::Tr::tr("Cannot create folder for nested archive: %1")
                                .arg(unpackInto.toUserOutput());
        return false;
    }
    if (!extractHarmonySdkArchiveOnce(arch, unpackInto, errorMessage))
        return false;
    if (!QFile::remove(arch.toFSPathString())) {
        if (errorMessage)
            *errorMessage = Ohos::Tr::tr("Could not remove nested archive after extracting: %1")
                                .arg(arch.toUserOutput());
        return false;
    }
    return true;
}

/**
 * Unpack tar-supported archives found as files in @p destDir or as files in its immediate
 * subdirectories (common wrapper layout). Each archive is unpacked into its parent directory,
 * then removed. Largest archives first. Repeat until none left or @p maxRounds.
 */
bool extractNestedArchivesShallow(const FilePath &destDir, int maxRounds, QString *errorMessage)
{
    for (int round = 0; round < maxRounds; ++round) {
        FilePaths archives = collectShallowNestedArchives(destDir);
        if (archives.isEmpty())
            return true;

        std::sort(archives.begin(), archives.end(), [](const FilePath &a, const FilePath &b) {
            return a.fileSize() > b.fileSize();
        });

        for (const FilePath &arch : std::as_const(archives)) {
            if (!extractAndRemoveNestedArchive(arch, errorMessage))
                return false;
        }
    }

    if (errorMessage)
        *errorMessage = Ohos::Tr::tr("Nested unpacking stopped after %1 round(s); archives may still "
                                      "be present under %2.")
                            .arg(QString::number(maxRounds), destDir.toUserOutput());
    return false;
}

} // namespace

bool extractHarmonySdkArchive(const FilePath &archive, const FilePath &destDir, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    if (!destDir.exists()) {
        if (!destDir.createDir()) {
            if (errorMessage)
                *errorMessage = Ohos::Tr::tr("Cannot create folder: %1").arg(destDir.toUserOutput());
            return false;
        }
    }

    if (!extractHarmonySdkArchiveOnce(archive, destDir, errorMessage))
        return false;

    static const int kMaxNestedRounds = 16;
    return extractNestedArchivesShallow(destDir, kMaxNestedRounds, errorMessage);
}

static FilePath findOhosSdkRootUnderImpl(const FilePath &dir, int depth, int maxDepth)
{
    if (depth > maxDepth || !dir.isReadableDir())
        return {};

    if (HarmonyConfig::isValidSdk(dir))
        return dir;

    const FilePaths children = dir.dirEntries(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const FilePath &child : children) {
        const FilePath found = findOhosSdkRootUnderImpl(child, depth + 1, maxDepth);
        if (!found.isEmpty())
            return found;
    }
    return {};
}

FilePath findOhosSdkRootUnder(const FilePath &searchRoot, int maxDepth)
{
    if (searchRoot.isEmpty() || !searchRoot.isReadableDir())
        return {};
    return findOhosSdkRootUnderImpl(searchRoot, 0, maxDepth);
}

static FilePath qmakeInQtPrefix(const FilePath &prefix)
{
    const FilePath qmake = (prefix / "bin" / "qmake").withExecutableSuffix();
    return qmake.isExecutableFile() ? qmake : FilePath{};
}

static FilePath findQtOhQmakeUnderImpl(const FilePath &dir, int depth, int maxDepth)
{
    if (depth > maxDepth || !dir.isReadableDir())
        return {};

    const FilePath here = qmakeInQtPrefix(dir);
    if (!here.isEmpty())
        return here;

    const FilePaths children = dir.dirEntries(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const FilePath &child : children) {
        const FilePath found = findQtOhQmakeUnderImpl(child, depth + 1, maxDepth);
        if (!found.isEmpty())
            return found;
    }
    return {};
}

FilePath findQtOhQmakeUnder(const FilePath &searchRoot, int maxDepth)
{
    if (searchRoot.isEmpty() || !searchRoot.isReadableDir())
        return {};
    return findQtOhQmakeUnderImpl(searchRoot, 0, maxDepth);
}

} // namespace Ohos::Internal
