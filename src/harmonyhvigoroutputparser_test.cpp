// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonyhvigoroutputparser_test.h"
#include "harmonyhvigoroutputparser.h"

#include <QTest>

using namespace Utils;

namespace Ohos::Internal {

class HarmonyHvigorOutputParserTest : public QObject
{
    Q_OBJECT

private slots:
    // ── handleLine：能识别的行 ───────────────────────────────────────────
    void handleLine_pathLineCol_returnsStatus_data();
    void handleLine_pathLineCol_returnsStatus();
    void handleLine_pathLineNoCol_returnsStatus_data();
    void handleLine_pathLineNoCol_returnsStatus();
    void handleLine_pathParenLineCol_returnsStatus();
    void handleLine_atFile_returnsStatus();
    void handleLine_npmErrPath_returnsStatus();
    void handleLine_hvigorError_returnsStatus_data();
    void handleLine_hvigorError_returnsStatus();
    void handleLine_ohpmError_returnsStatus();

    // ── handleLine：不能识别的行 → NotHandled ───────────────────────────
    void handleLine_emptyLine_notHandled();
    void handleLine_whitespaceOnly_notHandled();
    void handleLine_plainMessage_notHandled();
    void handleLine_partialMatch_notHandled();
    void handleLine_unknownExtension_notHandled();
    void handleLine_headerLine_notHandled();

    // ── 文件扩展名边界 ──────────────────────────────────────────────────
    void handleLine_supportedExtensions_data();
    void handleLine_supportedExtensions_done();
};

// ── helpers ─────────────────────────────────────────────────────────────────

static OutputLineParser::Status callHandleLine(HarmonyHvigorOhpmOutputParser &p, const QString &line)
{
    return p.handleLine(line, OutputFormat::StdOutFormat).status;
}

// ── path:line:col: message ───────────────────────────────────────────────────

void HarmonyHvigorOutputParserTest::handleLine_pathLineCol_returnsStatus_data()
{
    QTest::addColumn<QString>("line");

    QTest::newRow("ets_error")
        << QStringLiteral("/project/src/index.ets:10:5: error TS9001: bad syntax");
    QTest::newRow("ts_error")
        << QStringLiteral("src/pages/Main.ts:42:1: error TS2304: Cannot find name");
    QTest::newRow("tsx_error")
        << QStringLiteral("src/App.tsx:1:1: error TS1005: ';' expected.");
    QTest::newRow("js_warning")
        << QStringLiteral("src/utils.js:7:3: warning: use strict");
    QTest::newRow("jsx_error")
        << QStringLiteral("src/Comp.jsx:3:9: error TS6133: unused");
    QTest::newRow("json_error")
        << QStringLiteral("config.json:2:4: error: unexpected token");
    QTest::newRow("json5_error")
        << QStringLiteral("build.json5:5:1: error: expected comma");
    QTest::newRow("mjs_error")
        << QStringLiteral("src/mod.mjs:12:3: error: module error");
    QTest::newRow("cjs_error")
        << QStringLiteral("src/mod.cjs:8:2: error: require error");
    QTest::newRow("hml_error")
        << QStringLiteral("src/comp.hml:3:1: error: attribute error");
    QTest::newRow("css_error")
        << QStringLiteral("src/style.css:10:1: error: unexpected token");
    QTest::newRow("java_error")
        << QStringLiteral("src/Main.java:5:1: error: class not found");
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineCol_returnsStatus()
{
    QFETCH(QString, line);
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, line), OutputLineParser::Status::Done);
}

// ── path:line: message (无列号) ──────────────────────────────────────────────

void HarmonyHvigorOutputParserTest::handleLine_pathLineNoCol_returnsStatus_data()
{
    QTest::addColumn<QString>("line");

    QTest::newRow("ets_no_col")
        << QStringLiteral("/build/src/Main.ets:15: warning TS2339: property missing");
    QTest::newRow("ts_no_col")
        << QStringLiteral("src/helper.ts:1: error: parse error");
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineNoCol_returnsStatus()
{
    QFETCH(QString, line);
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, line), OutputLineParser::Status::Done);
}

// ── path(line,col): message ──────────────────────────────────────────────────

void HarmonyHvigorOutputParserTest::handleLine_pathParenLineCol_returnsStatus()
{
    HarmonyHvigorOhpmOutputParser p;
    // 圆括号风格 (行,列)
    QCOMPARE(callHandleLine(p, QStringLiteral("src/mod.ets(10,5): error: bad syntax")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(p, QStringLiteral("src/App.ts(1,1): error TS1005: delimiter expected.")),
             OutputLineParser::Status::Done);
}

// ── At file: path ───────────────────────────────────────────────────────────

void HarmonyHvigorOutputParserTest::handleLine_atFile_returnsStatus()
{
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, QStringLiteral("At file: /project/entry/src/Main.ets")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(p, QStringLiteral("  At file: src/index.ts")),
             OutputLineParser::Status::Done);
}

// ── npm ERR! path <path> ─────────────────────────────────────────────────────

void HarmonyHvigorOutputParserTest::handleLine_npmErrPath_returnsStatus()
{
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, QStringLiteral("npm ERR! code ENOENT path /home/user/.npm/pkg")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(p, QStringLiteral("npm ERR! syscall open file /etc/missing")),
             OutputLineParser::Status::Done);
}

// ── hvigor ERROR: ────────────────────────────────────────────────────────────

void HarmonyHvigorOutputParserTest::handleLine_hvigorError_returnsStatus_data()
{
    QTest::addColumn<QString>("line");

    QTest::newRow("plain")
        << QStringLiteral("hvigor  ERROR: BUILD FAILED in 3 s");
    QTest::newRow("piped")
        << QStringLiteral("> hvigor  ERROR: BUILD FAILED");
    QTest::newRow("single_space")
        << QStringLiteral("hvigor ERROR: compilation failed");
}

void HarmonyHvigorOutputParserTest::handleLine_hvigorError_returnsStatus()
{
    QFETCH(QString, line);
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, line), OutputLineParser::Status::Done);
}

// ── ohpm ERROR: ─────────────────────────────────────────────────────────────

void HarmonyHvigorOutputParserTest::handleLine_ohpmError_returnsStatus()
{
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, QStringLiteral("ohpm ERROR: package not found")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(p, QStringLiteral("ohpm ERROR pkg install failed")),
             OutputLineParser::Status::Done);
}

// ── 不识别的行 ───────────────────────────────────────────────────────────────

void HarmonyHvigorOutputParserTest::handleLine_emptyLine_notHandled()
{
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, QString()), OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_whitespaceOnly_notHandled()
{
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, QStringLiteral("   \t  ")), OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_plainMessage_notHandled()
{
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, QStringLiteral("BUILD SUCCESSFUL in 12 s")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(p, QStringLiteral("Compiling sources...")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(p, QStringLiteral("    at HvigorEntry.ts:0:0")),
             OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_partialMatch_notHandled()
{
    HarmonyHvigorOhpmOutputParser p;
    // 缺少行号 → 不能匹配 kPathLineColMessage / kPathLineMessage
    QCOMPARE(callHandleLine(p, QStringLiteral("src/index.ets: error: no line number")),
             OutputLineParser::Status::NotHandled);
    // 文件名无扩展名 → 不匹配
    QCOMPARE(callHandleLine(p, QStringLiteral("Makefile:10:1: error something")),
             OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_unknownExtension_notHandled()
{
    HarmonyHvigorOhpmOutputParser p;
    // .cpp / .h 不在支持列表中
    QCOMPARE(callHandleLine(p, QStringLiteral("src/main.cpp:1:1: error: undefined")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(p, QStringLiteral("include/a.h:5:1: error: bad include")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(p, QStringLiteral("module.py:10:1: SyntaxError: invalid")),
             OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_headerLine_notHandled()
{
    HarmonyHvigorOhpmOutputParser p;
    QCOMPARE(callHandleLine(p, QStringLiteral("Scanning sources")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(p, QStringLiteral("> Task :default@CompileArkTS")),
             OutputLineParser::Status::NotHandled);
}

// ── 支持的扩展名覆盖测试 ─────────────────────────────────────────────────────

void HarmonyHvigorOutputParserTest::handleLine_supportedExtensions_data()
{
    QTest::addColumn<QString>("ext");

    for (const char *e : {"ets", "ts", "tsx", "js", "jsx", "json", "json5", "mjs", "cjs",
                          "hml", "css", "java"}) {
        QTest::newRow(e) << QString::fromLatin1(e);
    }
}

void HarmonyHvigorOutputParserTest::handleLine_supportedExtensions_done()
{
    QFETCH(QString, ext);
    HarmonyHvigorOhpmOutputParser p;
    const QString line = QStringLiteral("path/to/file.%1:1:1: error: test").arg(ext);
    QCOMPARE(callHandleLine(p, line), OutputLineParser::Status::Done);
}

QObject *createHarmonyHvigorOutputParserTest()
{
    return new HarmonyHvigorOutputParserTest;
}

} // namespace Ohos::Internal

#include "harmonyhvigoroutputparser_test.moc"
