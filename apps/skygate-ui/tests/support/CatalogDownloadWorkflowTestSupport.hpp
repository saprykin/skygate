#pragma once

#include "FakeNetworkAccessManager.hpp"

#include <QtTest/QtTest>

#include <QNetworkReply>

namespace skygate::ui::tests {

inline FakeNetworkReply* onlyIssuedReply(FakeNetworkAccessManager& networkAccessManager)
{
    const QList<FakeNetworkReply*> replies = networkAccessManager.issuedReplies();
    if (replies.size() != 1) {
        QTest::qFail(
            qPrintable(QString("Expected 1 issued reply, got %1").arg(replies.size())),
            __FILE__,
            __LINE__
        );
        return nullptr;
    }
    return replies.front();
}

inline void waitForFakeReplyFinished(FakeNetworkReply* reply)
{
    QVERIFY(reply != nullptr);
    if (!reply->isFinished()) {
        QSignalSpy finishedSpy(reply, &QNetworkReply::finished);
        if (!reply->isFinished()) {
            QVERIFY(finishedSpy.wait(1000));
        }
    }
    QVERIFY(reply->isFinished());
}

}  // namespace skygate::ui::tests
