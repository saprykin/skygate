#include "FakeReplyQuery.hpp"

#include <QList>

#include <algorithm>

namespace skygate::ui::tests {

FakeNetworkReply* FakeReplyQuery::findReplyForUrl(FakeNetworkAccessManager& networkAccessManager, const QString& url)
{
    for (FakeNetworkReply* reply : networkAccessManager.issuedReplies()) {
        if (reply != nullptr && reply->url().toString() == url) {
            return reply;
        }
    }
    return nullptr;
}

FakeNetworkReply*
FakeReplyQuery::findPendingReplyForUrl(FakeNetworkAccessManager& networkAccessManager, const QString& url)
{
    for (FakeNetworkReply* reply : networkAccessManager.issuedReplies()) {
        if (reply != nullptr && !reply->isFinished() && reply->url().toString() == url) {
            return reply;
        }
    }
    return nullptr;
}

bool FakeReplyQuery::allIssuedRepliesFinished(const FakeNetworkAccessManager& networkAccessManager)
{
    const QList<FakeNetworkReply*> replies = networkAccessManager.issuedReplies();
    return std::all_of(replies.begin(), replies.end(), [](const FakeNetworkReply* reply) {
        return reply == nullptr || reply->isFinished();
    });
}

}  // namespace skygate::ui::tests
