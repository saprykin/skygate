#pragma once

#include "FakeNetworkAccessManager.hpp"

#include <QString>

namespace skygate::ui::tests {

// Looks up issued fake replies by URL.
class FakeReplyQuery final {
public:
    [[nodiscard]] static FakeNetworkReply*
    findReplyForUrl(FakeNetworkAccessManager& networkAccessManager, const QString& url);
    [[nodiscard]] static FakeNetworkReply*
    findPendingReplyForUrl(FakeNetworkAccessManager& networkAccessManager, const QString& url);
};

}  // namespace skygate::ui::tests
