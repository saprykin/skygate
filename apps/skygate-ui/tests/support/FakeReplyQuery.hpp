#pragma once

#include "FakeNetworkAccessManager.hpp"

#include <QString>

namespace skygate::ui::tests {

// Queries issued fake replies by URL and completion state.
class FakeReplyQuery final {
public:
    [[nodiscard]] static FakeNetworkReply*
    findReplyForUrl(FakeNetworkAccessManager& networkAccessManager, const QString& url);
    [[nodiscard]] static FakeNetworkReply*
    findPendingReplyForUrl(FakeNetworkAccessManager& networkAccessManager, const QString& url);
    [[nodiscard]] static bool allIssuedRepliesFinished(const FakeNetworkAccessManager& networkAccessManager);
};

}  // namespace skygate::ui::tests
