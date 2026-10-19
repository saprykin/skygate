#pragma once

#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourceOperation.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

#include <QString>
#include <QVector>

#include <cstdint>

namespace skygate::ui::internal {

// The operation records of one catalog collection: one record per source
// instance plus the revision source that names their incarnations.
//
// A revision is never reused, so a callback captured for one incarnation of an
// instance can never match the operation that supersedes it, not even when the
// instance ID is removed and added again. Retiring an incarnation advances its
// revision before any transfer is canceled, so a completion that runs during
// the cancellation already finds no current operation. The registry also names
// the instance that owns the published catalog download, so completion,
// cancellation, and restore resolve that identity through the same collection
// the rows come from.
class SkyCatalogSourceOperationRegistry final {
public:
    // Records the instance's requested configuration. An active incarnation
    // takes the attempt in place and keeps its revision and accepted facts; a
    // new instance receives a revision that was never handed out.
    [[nodiscard]] SkyCatalogSourceOperation*
    upsert(const SkyCatalogSourceInstance& attempt, skygate::ephemeris::CatalogCompositionPolicy policy);

    [[nodiscard]] SkyCatalogSourceOperation* find(const QString& instanceId);
    [[nodiscard]] const SkyCatalogSourceOperation* find(const QString& instanceId) const;

    // Whether the instance's current incarnation is the one that handed out
    // this revision.
    [[nodiscard]] bool isCurrent(const QString& instanceId, std::uint64_t revision) const;

    // Retires the instance's pending work before its record is erased, so a
    // later instance with the same ID starts from a fresh revision instead of
    // inheriting the removed one.
    void remove(const QString& instanceId);

    // Marks one instance's pending work terminal: the revision advances first
    // so a completion that runs during the abort is already stale, then both
    // owned transfers are canceled and released, and pending related state is
    // reset. The record stays, so a rejected transition keeps its reported
    // status. Other instances keep their revisions and their in-flight work.
    void retire(const QString& instanceId);

    // Retires every instance's pending work; only the global cancel does this.
    void retireAll();

    [[nodiscard]] bool hasPendingRelatedWork() const noexcept;

    // Marks the instance's retained in-memory payload as evicted so ordinary
    // persistence keeps its configured record without writing its bytes back.
    void markPayloadEvicted(const QString& instanceId);

    // Hands out a revision that was never used before. A caller that stages
    // operations outside the registry takes its revisions from here so the
    // guarantee holds for them too.
    [[nodiscard]] std::uint64_t allocateRevision() noexcept
    {
        return ++m_nextRevision;
    }

    // Replaces the whole collection. The previous incarnations are retired
    // before they are dropped, so the transfers they still own are aborted
    // instead of running on with results the replacement can no longer accept.
    void replace(QVector<SkyCatalogSourceOperation> operations);

    // The instance that owns the published catalog download. The identity is
    // published before the first row notification, so a receiver of that
    // notification can cancel the startup and leave its row without a request.
    [[nodiscard]] const QString& activeDownloadInstanceId() const noexcept
    {
        return m_activeDownloadInstanceId;
    }

    [[nodiscard]] bool hasActiveDownload() const noexcept
    {
        return !m_activeDownloadInstanceId.isEmpty();
    }

    void setActiveDownloadInstanceId(const QString& instanceId)
    {
        m_activeDownloadInstanceId = instanceId;
    }

    // Drops the identity while it still names this instance; a completion of a
    // superseded instance leaves the successor's identity in place.
    void endActiveDownload(const QString& instanceId)
    {
        if (m_activeDownloadInstanceId == instanceId) {
            m_activeDownloadInstanceId.clear();
        }
    }

    void clearActiveDownload()
    {
        m_activeDownloadInstanceId.clear();
    }

    [[nodiscard]] const QVector<SkyCatalogSourceOperation>& operations() const noexcept
    {
        return m_operations;
    }

private:
    void retireOperation(SkyCatalogSourceOperation& operation);

    QVector<SkyCatalogSourceOperation> m_operations;
    QString m_activeDownloadInstanceId;
    // Monotonic source of operation revisions. A revision is never reused, so a
    // callback captured for one incarnation of an instance can never match the
    // operation that supersedes it.
    std::uint64_t m_nextRevision = 0;
};

}  // namespace skygate::ui::internal
