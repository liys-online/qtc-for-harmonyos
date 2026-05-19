// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include <QDialog>
#include <QNetworkAccessManager>
#include <QUrl>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QLabel;
class QDialogButtonBox;
class QNetworkReply;
QT_END_NAMESPACE

namespace Ohos::Internal {

/**
 * Dialog that fetches and displays the OpenHarmony SDK license agreement.
 * The user must accept the license before downloading SDK packages.
 *
 * Flow:
 *   1. POST to license API to get the license text URL
 *   2. GET the license text from that URL
 *   3. Display the text with Accept / Decline buttons
 */
class HarmonySdkLicenseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HarmonySdkLicenseDialog(QWidget *parent = nullptr);

    bool isAccepted() const { return m_accepted; }

    /** Check whether the user has previously accepted the license (persisted). */
    static bool isLicenseAccepted();

    /** Persist that the user has accepted the license. */
    static void setLicenseAccepted();

private:
    void fetchLicenseUrl();
    void onLicenseUrlReceived();
    void fetchLicenseText(const QUrl &url);
    void onLicenseTextReceived();
    void setBusy(bool busy);

    QPlainTextEdit *m_licenseText = nullptr;
    QLabel *m_statusLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    QNetworkAccessManager m_nam;
    bool m_accepted = false;
    QUrl m_licenseTextUrl;
};

} // namespace Ohos::Internal
