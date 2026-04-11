// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonyhvigoroutputparser.h"

#include <projectexplorer/task.h>

#include <QRegularExpression>

#include <optional>

using namespace ProjectExplorer;
using namespace Utils;

namespace Ohos::Internal {
namespace {

/* ** ArkTS/TS 风格：path.ext:行:列: 消息 */
const QRegularExpression kPathLineColMessage(
    QStringLiteral(R"(^(.+\.(?:ets|ts|tsx|js|jsx|json5?|mjs|cjs|hml|css|java)):(\d+):(\d+):\s*(.+)$)"));

/* ** path.ext:行: 消息（无列号） */
const QRegularExpression kPathLineMessage(
    QStringLiteral(R"(^(.+\.(?:ets|ts|tsx|js|jsx|json5?|mjs|cjs|hml|css|java)):(\d+):\s*(.+)$)"));

/* ** path.ext(行,列): 消息 */
const QRegularExpression kPathParenLineCol(
    QStringLiteral(R"(^(.+\.(?:ets|ts|tsx|js|jsx|json5?|mjs|cjs|hml|css|java))\((\d+),(\d+)\):\s*(.+)$)"));

/* ** hvigor / 堆栈跟踪 */
const QRegularExpression kAtFile(QStringLiteral(R"(.*\bAt file:\s*(.+)$)"));

/* ** ohpm / npm 路径提示 */
const QRegularExpression kNpmErrPath(QStringLiteral(R"(^npm ERR!\s+.*\b(?:path|file)\s+([^\s].+)$)"));

/* ** 摘要行（无文件）— 仍会在 Issues 中显示 */
const QRegularExpression kHvigorErrorPrefix(QStringLiteral(R"(^(?:> )?hvigor\s+ERROR:\s*(.+)$)"));
const QRegularExpression kOhpmErrorLine(QStringLiteral(R"(^ohpm\s+ERROR:?\s*(.+)$)"));

} // namespace

HarmonyHvigorOhpmOutputParser::HarmonyHvigorOhpmOutputParser()
{
    setOrigin(QStringLiteral("hvigor"));
}

bool HarmonyHvigorOhpmOutputParser::looksLikeWarning(const QString &message)
{
    const QString m = message.toLower();
    return m.contains(QLatin1String("warning"))
           || m.contains(QLatin1String("warn "))
           || m.contains(QLatin1String("ts("))
           || m.startsWith(QLatin1String("warn"));
}

std::optional<OutputLineParser::Result> HarmonyHvigorOhpmOutputParser::tryCompilePattern(
    const QString &trimmed,
    const QRegularExpression &re,
    int capFile, int capLine, int capCol,
    int capDesc, bool hasColumn)
{
    const QRegularExpressionMatch match = re.match(trimmed);
    if (!match.hasMatch())
        return std::nullopt;

    bool okLine = false;
    const int lineNo = match.capturedView(capLine).toInt(&okLine);
    if (!okLine)
        return std::nullopt;

    int colNo = 0;
    if (hasColumn) {
        bool okCol = false;
        colNo = match.capturedView(capCol).toInt(&okCol);
        if (!okCol)
            colNo = 0;
    }

    const QString desc = match.captured(capDesc).trimmed();
    if (desc.isEmpty())
        return std::nullopt;

    const Task::TaskType taskType = looksLikeWarning(desc) ? Task::Warning : Task::Error;
    const FilePath file = absoluteFilePath(FilePath::fromUserInput(match.captured(capFile)));
    CompileTask task(taskType, desc, file, lineNo, hasColumn ? colNo : 0);
    LinkSpecs linkSpecs;
    if (file.isAbsolutePath())
        addLinkSpecForAbsoluteFilePath(linkSpecs, file, lineNo, task.column(), match, capFile);
    scheduleTask(task, 1);
    return Result{Status::Done, linkSpecs};
}

std::optional<OutputLineParser::Result> HarmonyHvigorOhpmOutputParser::tryAtFileLine(
    const QString &trimmed)
{
    const QRegularExpressionMatch m = kAtFile.match(trimmed);
    if (!m.hasMatch())
        return std::nullopt;
    const QString rawPath = m.captured(1).trimmed();
    if (rawPath.isEmpty())
        return std::nullopt;
    const FilePath file = absoluteFilePath(FilePath::fromUserInput(rawPath));
    CompileTask task(Task::Error, trimmed, file, -1, 0);
    LinkSpecs linkSpecs;
    if (file.isAbsolutePath())
        addLinkSpecForAbsoluteFilePath(linkSpecs, file, -1, 0, m, 1);
    scheduleTask(task, 1);
    return Result{Status::Done, linkSpecs};
}

std::optional<OutputLineParser::Result> HarmonyHvigorOhpmOutputParser::tryNpmErrLine(
    const QString &trimmed)
{
    const QRegularExpressionMatch m = kNpmErrPath.match(trimmed);
    if (!m.hasMatch())
        return std::nullopt;
    const QString rawPath = m.captured(1).trimmed();
    if (rawPath.isEmpty() || rawPath.startsWith(QLatin1Char('<')))
        return std::nullopt;
    const FilePath file = absoluteFilePath(FilePath::fromUserInput(rawPath));
    CompileTask task(Task::Error, trimmed, file, -1, 0);
    LinkSpecs linkSpecs;
    if (file.isAbsolutePath())
        addLinkSpecForAbsoluteFilePath(linkSpecs, file, -1, 0, m, 1);
    scheduleTask(task, 1);
    return Result{Status::Done, linkSpecs};
}

std::optional<OutputLineParser::Result> HarmonyHvigorOhpmOutputParser::trySummaryErrorLine(
    const QString &trimmed)
{
    QRegularExpressionMatch m = kHvigorErrorPrefix.match(trimmed);
    if (!m.hasMatch())
        m = kOhpmErrorLine.match(trimmed);
    if (!m.hasMatch())
        return std::nullopt;
    const QString desc = m.captured(1).trimmed();
    if (desc.isEmpty())
        return std::nullopt;
    scheduleTask(BuildSystemTask(Task::Error, desc), 1);
    return Result{Status::Done, {}};
}

OutputLineParser::Result HarmonyHvigorOhpmOutputParser::handleLine(const QString &line,
                                                                   OutputFormat type)
{
    Q_UNUSED(type)
    const QString trimmed = rightTrimmed(line);
    if (trimmed.isEmpty())
        return Status::NotHandled;

    /* ** 分组：文件、行、列、描述 */
    if (auto r = tryCompilePattern(trimmed, kPathLineColMessage, 1, 2, 3, 4, true))
        return *r;
    /* ** 分组：文件、行、描述 */
    if (auto r = tryCompilePattern(trimmed, kPathLineMessage, 1, 2, -1, 3, false))
        return *r;
    /* ** 分组：文件、行、列、描述 */
    if (auto r = tryCompilePattern(trimmed, kPathParenLineCol, 1, 2, 3, 4, true))
        return *r;

    if (auto r = tryAtFileLine(trimmed))
        return *r;
    if (auto r = tryNpmErrLine(trimmed))
        return *r;
    if (auto r = trySummaryErrorLine(trimmed))
        return *r;

    return Status::NotHandled;
}

} // namespace Ohos::Internal
