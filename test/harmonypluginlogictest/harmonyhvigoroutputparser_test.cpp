// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifdef WITH_TESTS

#include "harmonyhvigoroutputparser_test.h"
#include "harmonyhvigoroutputparser.h"

#include <utils/outputformatter.h>
#include <QTest>

using namespace Utils;

namespace Ohos::Internal {

// 每次调用创建独立的 OutputFormatter + parser，确保 scheduleTask 不会因
// null formatter 指针崩溃（standalone 或插件测试中均无构建步骤上下文）。
static OutputLineParser::Status callHandleLine(const QString &line)
{
    Utils::OutputFormatter fmt;
    auto *p = new HarmonyHvigorOhpmOutputParser; // fmt 接管所有权
    fmt.addLineParser(p);
    return p->handleLine(line, OutputFormat::StdOutFormat).status;
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineCol_returnsStatus_data()
{
    QTest::addColumn<QString>("line");
    QTest::newRow("ets_error")   << QStringLiteral("/project/src/index.ets:10:5: error TS9001: bad syntax");
    QTest::newRow("ts_error")    << QStringLiteral("/tmp/src/pages/Main.ts:42:1: error TS2304: Cannot find name");
    QTest::newRow("tsx_error")   << QStringLiteral("/tmp/src/App.tsx:1:1: error TS1005: ';' expected.");
    QTest::newRow("js_warning")  << QStringLiteral("/tmp/src/utils.js:7:3: warning: use strict");
    QTest::newRow("jsx_error")   << QStringLiteral("/tmp/src/Comp.jsx:3:9: error TS6133: unused");
    QTest::newRow("json_error")  << QStringLiteral("/tmp/config.json:2:4: error: unexpected token");
    QTest::newRow("json5_error") << QStringLiteral("/tmp/build.json5:5:1: error: expected comma");
    QTest::newRow("mjs_error")   << QStringLiteral("/tmp/src/mod.mjs:12:3: error: module error");
    QTest::newRow("cjs_error")   << QStringLiteral("/tmp/src/mod.cjs:8:2: error: require error");
    QTest::newRow("hml_error")   << QStringLiteral("/tmp/src/comp.hml:3:1: error: attribute error");
    QTest::newRow("css_error")   << QStringLiteral("/tmp/src/style.css:10:1: error: unexpected token");
    QTest::newRow("java_error")  << QStringLiteral("/tmp/src/Main.java:5:1: error: class not found");
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineCol_returnsStatus()
{
    QFETCH(QString, line);
    QCOMPARE(callHandleLine(line), OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineNoCol_returnsStatus_data()
{
    QTest::addColumn<QString>("line");
    QTest::newRow("ets_no_col") << QStringLiteral("/build/src/Main.ets:15: warning TS2339: property missing");
    QTest::newRow("ts_no_col")  << QStringLiteral("/tmp/src/helper.ts:1: error: parse error");
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineNoCol_returnsStatus()
{
    QFETCH(QString, line);
    QCOMPARE(callHandleLine(line), OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_pathParenLineCol_returnsStatus()
{
    QCOMPARE(callHandleLine(QStringLiteral("/tmp/src/mod.ets(10,5): error: bad syntax")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(QStringLiteral("/tmp/src/App.ts(1,1): error TS1005: delimiter expected.")),
             OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_atFile_returnsStatus()
{
    QCOMPARE(callHandleLine(QStringLiteral("At file: /project/entry/src/Main.ets")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(QStringLiteral("  At file: /tmp/src/index.ts")),
             OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_npmErrPath_returnsStatus()
{
    QCOMPARE(callHandleLine(QStringLiteral("npm ERR! code ENOENT path /home/user/.npm/pkg")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(QStringLiteral("npm ERR! syscall open file /etc/missing")),
             OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_hvigorError_returnsStatus_data()
{
    QTest::addColumn<QString>("line");
    QTest::newRow("plain")        << QStringLiteral("hvigor  ERROR: BUILD FAILED in 3 s");
    QTest::newRow("piped")        << QStringLiteral("> hvigor  ERROR: BUILD FAILED");
    QTest::newRow("single_space") << QStringLiteral("hvigor ERROR: compilation failed");
}

void HarmonyHvigorOutputParserTest::handleLine_hvigorError_returnsStatus()
{
    QFETCH(QString, line);
    QCOMPARE(callHandleLine(line), OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_ohpmError_returnsStatus()
{
    QCOMPARE(callHandleLine(QStringLiteral("ohpm ERROR: package not found")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(QStringLiteral("ohpm ERROR pkg install failed")),
             OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_emptyLine_notHandled()
{
    QCOMPARE(callHandleLine(QString()), OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_whitespaceOnly_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("   \t  ")), OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_plainMessage_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("BUILD SUCCESSFUL in 12 s")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("Compiling sources...")),
             OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_partialMatch_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("src/index.ets: error: no line number")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("Makefile:10:1: error something")),
             OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_unknownExtension_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("src/main.cpp:1:1: error: undefined")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("include/a.h:5:1: error: bad include")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("module.py:10:1: SyntaxError: invalid")),
             OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_headerLine_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("Scanning sources")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("> Task :default@CompileArkTS")),
             OutputLineParser::Status::NotHandled);
}

} // namespace Ohos::Internal

#include "harmonyhvigoroutputparser_test.moc"

#endif // WITH_TESTS
