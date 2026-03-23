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

// ArkTS / TS-like: path.ext:line:col: message
const QRegularExpression kPathLineColMessage(
    QStringLiteral(R"(^(.+\.(?:ets|ts|tsx|js|jsx|json5?|mjs|cjs|hml|css|java)):(\d+):(\d+):\s*(.+)$)"));

// path.ext:line: message (no column)
const QRegularExpression kPathLineMessage(
    QStringLiteral(R"(^(.+\.(?:ets|ts|tsx|js|jsx|json5?|mjs|cjs|hml|css|java)):(\d+):\s*(.+)$)"));

// path.ext(line,col): message
const QRegularExpression kPathParenLineCol(
    QStringLiteral(R"(^(.+\.(?:ets|ts|tsx|js|jsx|json5?|mjs|cjs|hml|css|java))\((\d+),(\d+)\):\s*(.+)$)"));

// hvigor / stack traces
const QRegularExpression kAtFile(QStringLiteral(R"(.*\bAt file:\s*(.+)$)"));

// ohpm / npm style path hint
const QRegularExpression kNpmErrPath(QStringLiteral(R"(^npm ERR!\s+.*\b(?:path|file)\s+([^\s].+)$)"));

// Summary lines (no file) — still surface in Issues
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

OutputLineParser::Result HarmonyHvigorOhpmOutputParser::handleLine(const QString &line,
                                                                   OutputFormat type)
{
    Q_UNUSED(type)
    const QString trimmed = rightTrimmed(line);
    if (trimmed.isEmpty())
        return Status::NotHandled;

    auto tryCompilePattern = [&](const QRegularExpression &re, int capFile, int capLine, int capCol,
                                 int capDesc, bool hasColumn) -> std::optional<OutputLineParser::Result> {
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

        const QString rawPath = match.captured(capFile);
        const QString desc = match.captured(capDesc).trimmed();
        if (desc.isEmpty())
            return std::nullopt;

        const Task::TaskType taskType = looksLikeWarning(desc) ? Task::Warning : Task::Error;
        const FilePath file = absoluteFilePath(FilePath::fromUserInput(rawPath));
        CompileTask task(taskType, desc, file, lineNo, hasColumn ? colNo : 0);
        LinkSpecs linkSpecs;
        if (file.isAbsolutePath())
            addLinkSpecForAbsoluteFilePath(linkSpecs, file, lineNo, task.column(), match, capFile);
        scheduleTask(task, 1);
        return OutputLineParser::Result{Status::Done, linkSpecs};
    };

    // Groups: file, line, column, description
    if (auto r = tryCompilePattern(kPathLineColMessage, 1, 2, 3, 4, true))
        return *r;
    // Groups: file, line, description
    if (auto r = tryCompilePattern(kPathLineMessage, 1, 2, -1, 3, false))
        return *r;
    // Groups: file, line, column, description
    if (auto r = tryCompilePattern(kPathParenLineCol, 1, 2, 3, 4, true))
        return *r;

    {
        const QRegularExpressionMatch atMatch = kAtFile.match(trimmed);
        if (atMatch.hasMatch()) {
            const QString rawPath = atMatch.captured(1).trimmed();
            if (!rawPath.isEmpty()) {
                const FilePath file = absoluteFilePath(FilePath::fromUserInput(rawPath));
                const QString desc = trimmed;
                CompileTask task(Task::Error, desc, file, -1, 0);
                LinkSpecs linkSpecs;
                if (file.isAbsolutePath())
                    addLinkSpecForAbsoluteFilePath(linkSpecs, file, -1, 0, atMatch, 1);
                scheduleTask(task, 1);
                return {Status::Done, linkSpecs};
            }
        }
    }

    {
        const QRegularExpressionMatch npmMatch = kNpmErrPath.match(trimmed);
        if (npmMatch.hasMatch()) {
            const QString rawPath = npmMatch.captured(1).trimmed();
            if (!rawPath.isEmpty() && !rawPath.startsWith(QLatin1Char('<'))) {
                const FilePath file = absoluteFilePath(FilePath::fromUserInput(rawPath));
                const QString desc = trimmed;
                CompileTask task(Task::Error, desc, file, -1, 0);
                LinkSpecs linkSpecs;
                if (file.isAbsolutePath())
                    addLinkSpecForAbsoluteFilePath(linkSpecs, file, -1, 0, npmMatch, 1);
                scheduleTask(task, 1);
                return {Status::Done, linkSpecs};
            }
        }
    }

    {
        QRegularExpressionMatch m = kHvigorErrorPrefix.match(trimmed);
        if (!m.hasMatch())
            m = kOhpmErrorLine.match(trimmed);
        if (m.hasMatch()) {
            const QString desc = m.captured(1).trimmed();
            if (!desc.isEmpty()) {
                scheduleTask(BuildSystemTask(Task::Error, desc), 1);
                return {Status::Done, {}};
            }
        }
    }

    return Status::NotHandled;
}

} // namespace Ohos::Internal
