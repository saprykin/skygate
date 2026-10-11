#include "CatalogDownloadCancellation.hpp"

#include <QNetworkReply>

void CatalogDownloadCancellation::cancel()
{
    m_canceled = true;
    if (m_activeReply != nullptr && m_activeReply->isRunning()) {
        m_activeReply->abort();
    }
}

bool CatalogDownloadCancellation::isCanceled() const noexcept
{
    return m_canceled;
}

void CatalogDownloadCancellation::setActiveReply(QNetworkReply* reply)
{
    m_activeReply = reply;
}
