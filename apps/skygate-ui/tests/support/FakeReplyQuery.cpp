#include "FakeReplyQuery.hpp"

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

}  // namespace skygate::ui::tests
