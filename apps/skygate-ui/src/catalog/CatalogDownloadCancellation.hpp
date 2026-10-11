#pragma once

#include <QPointer>

class QNetworkReply;

// Cancels one catalog download operation.
//
// The owner marks the operation terminal before calling `cancel()`, so the
// abort cannot be taken for an ordinary transfer failure: the download never
// advances to another candidate URL, starts parsing, or delivers a
// completion. `cancel()` aborts only the reply this operation currently
// holds, which leaves requests owned by others on the same network manager
// running.
class CatalogDownloadCancellation final {
public:
    void cancel();
    [[nodiscard]] bool isCanceled() const noexcept;
    void setActiveReply(QNetworkReply* reply);

private:
    QPointer<QNetworkReply> m_activeReply;
    bool m_canceled = false;
};
