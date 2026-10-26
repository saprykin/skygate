#pragma once

#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogDownloadCancellation.hpp"
#include "catalog/SkyCatalogSourceInstance.hpp"

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <memory>

namespace skygate::ui::internal {

// One catalog source operation: what was last requested for an instance, what
// the runtime accepted for it, the transfers it still owns, and the row state
// reported while it is not settled.
//
// The requested configuration stays an attempt until accept() records it as
// the accepted facts together with the payload that was activated with it. A
// retry repeats the attempt, and persistence reads only accepted facts, so an
// attempt the runtime rejected cannot reach the stored collection. The revision
// is allocated by the manager and names one incarnation of an instance, so work
// captured for an older revision cannot apply to its successor.
class SkyCatalogSourceOperation final {
public:
    // Creates the record of a fresh incarnation. The caller owns the revision
    // source and hands out a value that was never used before.
    SkyCatalogSourceOperation(
        const SkyCatalogSourceInstance& attempt,
        skygate::ephemeris::CatalogCompositionPolicy policy,
        std::uint64_t revision
    )
        : m_attempt(attempt), m_policy(policy), m_revision(revision)
    {
    }

    [[nodiscard]] const SkyCatalogSourceInstance& attempt() const noexcept
    {
        return m_attempt;
    }

    [[nodiscard]] skygate::ephemeris::CatalogCompositionPolicy policy() const noexcept
    {
        return m_policy;
    }

    [[nodiscard]] std::uint64_t revision() const noexcept
    {
        return m_revision;
    }

    [[nodiscard]] bool isBusy() const noexcept
    {
        return m_busy;
    }

    [[nodiscard]] bool hasError() const noexcept
    {
        return m_hasError;
    }

    // Empty once the operation settled: the row then shows the collection's own
    // state instead of an operation status.
    [[nodiscard]] const QString& statusText() const noexcept
    {
        return m_statusText;
    }

    [[nodiscard]] bool hasAcceptedFacts() const noexcept
    {
        return m_hasAcceptedFacts;
    }

    [[nodiscard]] const SkyCatalogSourceInstance& acceptedInstance() const noexcept
    {
        return m_acceptedInstance;
    }

    [[nodiscard]] const QByteArray& acceptedPayload() const noexcept
    {
        return m_acceptedPayload;
    }

    [[nodiscard]] bool payloadEvicted() const noexcept
    {
        return m_payloadEvicted;
    }

    [[nodiscard]] bool hasPendingRelatedWork() const noexcept
    {
        return m_constellationPending;
    }

    // Records a new attempt for the same instance without touching the accepted
    // facts.
    void beginAttempt(const SkyCatalogSourceInstance& attempt, skygate::ephemeris::CatalogCompositionPolicy policy)
    {
        m_attempt = attempt;
        m_policy = policy;
    }

    // Enters the pending state and takes ownership of the attempt's catalog
    // transfer. A null handle records an attempt that issues no transfer.
    void beginWork(const QString& statusText, std::shared_ptr<CatalogDownloadCancellation> catalogCancellation)
    {
        m_catalogCancellation = std::move(catalogCancellation);
        m_busy = true;
        m_hasError = false;
        m_statusText = statusText;
    }

    // Reports progress of the pending catalog transfer.
    void reportProgress(const QString& statusText)
    {
        m_busy = true;
        m_hasError = false;
        m_statusText = statusText;
    }

    // Ends the attempt as accepted: the requested configuration becomes the
    // accepted facts together with the payload it was activated with, and the
    // row settles without an error.
    void accept(QByteArray payload)
    {
        m_busy = false;
        m_hasError = false;
        m_statusText.clear();
        m_acceptedInstance = m_attempt;
        m_acceptedPayload = std::move(payload);
        m_hasAcceptedFacts = true;
        // The accepted payload can be stored again, so an earlier eviction is
        // superseded.
        m_payloadEvicted = false;
        m_catalogCancellation.reset();
    }

    // Ends the attempt without accepting it: the row reports the error and the
    // previously accepted facts stay in place.
    void fail(const QString& statusText)
    {
        m_busy = false;
        m_hasError = true;
        m_statusText = statusText;
        m_catalogCancellation.reset();
    }

    // Ends the attempt as canceled: the row reports the cancellation without an
    // error and the accepted facts stay in place.
    void markCanceled(const QString& statusText)
    {
        m_busy = false;
        m_hasError = false;
        m_statusText = statusText;
    }

    // Supersedes every transfer of this incarnation. The caller advances the
    // revision before any abort, so a completion that runs during the
    // cancellation is already stale.
    void retire(std::uint64_t revision)
    {
        m_revision = revision;
        cancelCatalogWork();
        cancelRelatedWork();
        m_constellationPending = false;
    }

    // Drops the catalog transfer handle of an attempt that issues no request.
    void releaseCatalogWork()
    {
        m_catalogCancellation.reset();
    }

    // Marks the payload files of the accepted facts as removed. The in-memory
    // snapshot stays active, but ordinary persistence must not write the
    // retained bytes back; only a later accepted load repopulates them.
    void markPayloadEvicted()
    {
        m_payloadEvicted = true;
    }

    // Takes ownership of the attempt's related transfer and supersedes the
    // previous one.
    void beginRelatedWork(std::shared_ptr<CatalogDownloadCancellation> relatedCancellation)
    {
        cancelRelatedWork();
        m_relatedCancellation = std::move(relatedCancellation);
        m_constellationPending = true;
    }

    // Ends the pending related transfer of this incarnation.
    void finishRelatedWork()
    {
        m_constellationPending = false;
        m_relatedCancellation.reset();
    }

private:
    static void cancelTransfer(std::shared_ptr<CatalogDownloadCancellation>& cancellation)
    {
        if (cancellation != nullptr) {
            cancellation->cancel();
            cancellation.reset();
        }
    }

    void cancelCatalogWork()
    {
        cancelTransfer(m_catalogCancellation);
    }

    void cancelRelatedWork()
    {
        cancelTransfer(m_relatedCancellation);
    }

    // The configuration most recently requested for the instance, including an
    // attempt the runtime rejected. A retry repeats it, and it is never
    // serialized while it is only an attempt.
    SkyCatalogSourceInstance m_attempt;
    skygate::ephemeris::CatalogCompositionPolicy m_policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    // The facts committed together when the runtime accepted the requested
    // configuration: the descriptor and the bytes its catalog was built from.
    SkyCatalogSourceInstance m_acceptedInstance;
    QByteArray m_acceptedPayload;
    bool m_hasAcceptedFacts = false;
    bool m_payloadEvicted = false;
    std::uint64_t m_revision = 0;
    bool m_constellationPending = false;
    bool m_busy = false;
    bool m_hasError = false;
    QString m_statusText;
    // The transfers this instance started, retained so cancellation aborts
    // exactly them instead of every request on the network manager. A new
    // transfer of the same kind replaces its handle; a completed transfer drops
    // it.
    std::shared_ptr<CatalogDownloadCancellation> m_catalogCancellation;
    std::shared_ptr<CatalogDownloadCancellation> m_relatedCancellation;
};

}  // namespace skygate::ui::internal
