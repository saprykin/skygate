#pragma once

#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourceRecord.hpp"
#include "SkySettingsStore.hpp"

#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

class QNetworkAccessManager;

namespace skygate::ui::internal {
class SkyCatalogCacheController;
class SkyCatalogImportWorkflow;
class SkyCatalogRuntime;
struct SkyCatalogRuntimeBuildOptions;
struct SkyCatalogRuntimeResult;
struct SkyCatalogSourceImportResult;
struct SkyConstellationLineImportResult;
struct SkyCatalogSourceInstance;
}  // namespace skygate::ui::internal

// QML-facing owner of the configured catalog source collection.
//
// One operation record per source instance carries a monotonic operation
// revision, so a response captured for a superseded incarnation can never
// apply to its successor while unrelated instances keep their pending work.
// Active composition and related-data ownership live in SkyCatalogRuntime;
// persistence and legacy migration live in SkyCatalogCacheController. Only an
// accepted transition persists state and emits catalogChanged.
//
// A successful source (re)load publishes its catalog and its accepted
// related-data state in one step: the owner's previous related dataset is
// cleared together with the replacement, before any consumer is notified or
// the reload is persisted, and the replacement dataset becomes owned only
// when its own download completes successfully. A successful update whose
// accepted related declaration is removed retires the owner's obsolete
// dataset in the same accepted transition, so no declaration means no active
// owned related data. An observer that reads the related view during
// catalogChanged
// therefore sees the state the runtime holds, and no later silent clear moves
// the runtime away from it.
class SkyCatalogManager final : public QObject {
    Q_OBJECT

public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationAnchorGroup = skygate::ephemeris::ConstellationAnchorGroup;

    struct SourceViewEntry final {
        QString instanceId;
        QString title;
        QString version;
        skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
        bool enabled = true;
        bool bundled = false;
        bool busy = false;
        bool hasError = false;
        QString statusText;
        std::size_t objectCount = 0;
    };

    explicit SkyCatalogManager(
        SkySettingsStore* settingsStore,
        std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog = nullptr,
        QObject* parent = nullptr,
        QNetworkAccessManager* networkAccessManager = nullptr
    );
    ~SkyCatalogManager() override;

    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString datasetInfoText() const;
    [[nodiscard]] QString deepSkyCatalogInfoText() const;
    [[nodiscard]] bool downloadingCatalog() const noexcept;
    [[nodiscard]] bool catalogProcessing() const noexcept;
    [[nodiscard]] int catalogPresetIndex() const noexcept;
    [[nodiscard]] QString catalogUrlText() const;
    [[nodiscard]] int deepSkyCatalogPresetIndex() const noexcept;
    [[nodiscard]] QString deepSkyCatalogUrlText() const;
    [[nodiscard]] QString sourceLabel() const;
    // Presentation summary of the active collection's participation, derived
    // from the enabled sources and the provenance of the active snapshot.
    [[nodiscard]] QString participationSummary() const;
    [[nodiscard]] std::size_t bodyCount() const noexcept;
    [[nodiscard]] std::size_t constellationCount() const noexcept;
    [[nodiscard]] std::uint64_t catalogRevision() const noexcept;
    [[nodiscard]] const skygate::ephemeris::IStarCatalog* starCatalog() const noexcept;
    [[nodiscard]] std::size_t sourceCount() const noexcept;
    [[nodiscard]] QStringList sourceInstanceIds() const;
    [[nodiscard]] bool isSourceEnabled(const QString& instanceId) const;
    [[nodiscard]] QHash<QString, QString> sourceTitles() const;
    [[nodiscard]] QString sourceTitle(const QString& instanceId) const;
    [[nodiscard]] QVector<SourceViewEntry> sourceViewEntries() const;
    [[nodiscard]] std::span<const QString> sourceIds() const noexcept;
    [[nodiscard]] const std::vector<QStringList>& contributorSourceIds() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> constellationLineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationAnchorGroup> constellationAnchorGroups() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> resolvedConstellationLineRefs() const;
    [[nodiscard]] std::span<const ConstellationAnchorGroup> resolvedConstellationAnchorGroups() const;

    void setCatalogPresetIndex(int catalogPresetIndex);
    void setCatalogUrlText(const QString& catalogUrlText);
    void setDeepSkyCatalogPresetIndex(int deepSkyCatalogPresetIndex);
    void setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText);
    void loadCatalogPreset(const QString& presetId);
    void downloadCatalogFromUrl(const QString& urlText);
    void loadDeepSkyCatalogPreset(const QString& presetId);
    void downloadDeepSkyCatalogFromUrl(const QString& urlText);
    // Adds the instance when its durable instance ID is not active yet. When
    // the ID is already active this is the explicit reload/update action for
    // that instance, and the target instance ID is preserved.
    void loadSource(
        const skygate::ui::internal::SkyCatalogSourceInstance& source,
        skygate::ephemeris::CatalogCompositionPolicy policy
    );
    void addSourcePreset(const QString& presetId);
    void addSourceUrl(const QString& urlText, const QString& category);
    void enableSource(const QString& instanceId);
    // Disabling a source supersedes its own in-flight related request: a
    // response completing after the disable is discarded. Data that completed
    // before the disable stays owned and inactive, and re-enabling restores
    // only that retained data.
    void disableSource(const QString& instanceId);
    void removeSource(const QString& instanceId);
    void moveSource(const QString& instanceId, int targetIndex);
    // Reloads the active instance identified by instanceId from its last
    // requested configuration, which may be a rejected or canceled attempt;
    // the instance ID is never re-allocated by a reload.
    void retrySource(const QString& instanceId);
    void cancelCatalogDownload();
    bool clearCatalogCache();
    bool clearDeepSkyCatalogCache();
    // Evicts the source's disposable payload files while its configured record
    // stays durable. The accepted in-memory snapshot remains active until the
    // source is reloaded or the application restarts; after a restart the
    // source is configured and reports an unavailable payload with retry
    // state. Ordinary metadata persistence never writes the retained payload
    // back; only a later successful load repopulates it. Removing the source
    // stays the separate configuration-deletion operation.
    bool clearSourceCache(const QString& instanceId);
    // Applies a stored collection when one was committed. An accepted restore
    // replaces the runtime collection and the operation/accepted-facts state in
    // one step: work captured for a superseded incarnation is rejected, so a
    // late reply can neither repopulate an omitted source nor mutate a restored
    // instance that reuses its ID.
    bool restoreCatalogCache();

signals:
    void statusTextChanged();
    void datasetInfoTextChanged();
    void deepSkyCatalogInfoTextChanged();
    void downloadingCatalogChanged();
    void catalogProcessingChanged();
    void catalogChanged();
    void sourcesChanged();

private:
    struct SourceOperation final {
        // The configuration most recently requested for the instance,
        // including an attempt the runtime rejected. A retry repeats it, and
        // it is never serialized while it is only an attempt.
        skygate::ui::internal::SkyCatalogSourceInstance instance;
        skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
        // The source facts committed together when the runtime accepted the
        // requested configuration: the descriptor and the bytes its catalog
        // was built from. Ordinary persistence serializes these instead of the
        // attempted configuration.
        skygate::ui::internal::SkyCatalogSourceInstance acceptedInstance;
        QByteArray acceptedPayload;
        bool hasAcceptedInstance = false;
        // A payload eviction dropped this instance's disposable payload files
        // from its configured record. The accepted runtime snapshot stays
        // active for the session, but ordinary persistence must not write the
        // retained bytes back; only a later accepted load repopulates them.
        bool payloadEvicted = false;
        std::uint64_t revision = 0;
        bool constellationPending = false;
        bool busy = false;
        bool hasError = false;
        QString statusText;
    };

    void loadSourceInstance(
        const skygate::ui::internal::SkyCatalogSourceInstance& source,
        skygate::ephemeris::CatalogCompositionPolicy policy
    );
    void applyBundledSource(SourceOperation& operation, skygate::ephemeris::CatalogCompositionPolicy policy);
    // Applies the accepted related-declaration transition of an activation to
    // the instance's owned dataset and folds the published changes into the
    // activation result. A declaration that still selects related data
    // supersedes the dataset owned by the replaced catalog, because the
    // replacement becomes owned only when its own download completes; a
    // declaration that no longer selects related data retires the obsolete
    // dataset. Only the accepted declarations and the instance's own ownership
    // decide, so a downloaded and a bundled activation and every composition
    // policy share one rule, and no other instance's dataset is touched. Called
    // only after the activation was accepted and before it is persisted or
    // published, so a rejected update keeps the previously accepted dataset.
    void applyRelatedDeclarationTransition(
        const SourceOperation& operation,
        const QStringList& acceptedRelatedDatasetUrls,
        skygate::ui::internal::SkyCatalogRuntimeResult& activationResult
    );
    [[nodiscard]] skygate::ui::internal::SkyCatalogRuntimeResult applySourceResult(
        skygate::ui::internal::SkyCatalogSourceImportResult result, skygate::ephemeris::CatalogCompositionPolicy policy
    );
    void handleSourceImportFinished(
        const QString& instanceId,
        std::uint64_t revision,
        skygate::ui::internal::SkyCatalogSourceImportResult result,
        skygate::ephemeris::CatalogCompositionPolicy policy,
        const QStringList& relatedDatasetUrls
    );
    [[nodiscard]] SourceOperation* upsertOperation(
        const skygate::ui::internal::SkyCatalogSourceInstance& source,
        skygate::ephemeris::CatalogCompositionPolicy policy
    );
    [[nodiscard]] SourceOperation* findOperation(const QString& instanceId);
    [[nodiscard]] const SourceOperation* findOperation(const QString& instanceId) const;
    void removeOperation(const QString& instanceId);
    // Marks the instance's retained in-memory payload as evicted so ordinary
    // persistence keeps its configured record without writing its bytes back.
    void markSourcePayloadEvicted(const QString& instanceId);
    [[nodiscard]] bool isOperationCurrent(const QString& instanceId, std::uint64_t revision) const;
    // Commits the operation's requested configuration and the payload that was
    // activated with it as the instance's accepted facts. Only a successful
    // runtime transition may call this, so persistence never pairs attempted
    // options with previously accepted bytes.
    static void commitAcceptedSourceFacts(SourceOperation& operation, QByteArray payload);
    // Supersedes the pending work of one instance: callbacks already captured
    // for its current operation revision are rejected and its pending related
    // download is no longer tracked. Other instances keep their revisions and
    // their in-flight work.
    void invalidatePendingSourceWork(const QString& instanceId);
    // Supersedes every instance's pending work; only the global cancel does.
    void invalidateAllPendingSourceWork();
    [[nodiscard]] bool hasPendingConstellationWork() const;
    void setSourceEnabled(const QString& instanceId, bool enabled);
    void setStatusText(const QString& statusText);
    void setDownloadingCatalog(bool downloadingCatalog);
    void setCatalogProcessing(bool catalogProcessing);
    void handleCatalogImportStatus(const QString& instanceId, const QString& statusText);
    void downloadConstellationLinesAfterCatalog(
        const QString& instanceId,
        std::uint64_t revision,
        const QStringList& constellationLineUrlTexts,
        const QString& catalogSummaryText
    );
    void handleConstellationLineImportStatus(const QString& catalogSummaryText, const QString& statusText);
    void handleConstellationLineImportFinished(
        const QString& instanceId,
        const QString& catalogSummaryText,
        skygate::ui::internal::SkyConstellationLineImportResult lineResult
    );
    [[nodiscard]] skygate::ui::internal::SkyCatalogRuntimeBuildOptions runtimeBuildOptions() const;
    void applyRuntimeResult(const skygate::ui::internal::SkyCatalogRuntimeResult& result);
    void persistCatalogCache() const;

private:
    std::unique_ptr<skygate::ui::internal::SkyCatalogRuntime> m_runtime;
    std::unique_ptr<skygate::ui::internal::SkyCatalogCacheController> m_cacheController;
    std::unique_ptr<skygate::ui::internal::SkyCatalogImportWorkflow> m_importWorkflow;
    QNetworkAccessManager* m_networkAccessManager = nullptr;
    QString m_statusText;
    int m_catalogPresetIndex = 0;
    int m_deepSkyCatalogPresetIndex = 0;
    QString m_catalogUrlText;
    QString m_deepSkyCatalogUrlText;
    bool m_downloadingCatalog = false;
    bool m_catalogProcessing = false;
    QString m_activeDownloadInstanceId;
    QVector<SourceOperation> m_sourceOperations;
    // Monotonic source of operation revisions. A revision is never reused, so
    // a callback captured for one incarnation of an instance can never match
    // the operation that supersedes it, not even when the same instance ID is
    // removed and added again.
    std::uint64_t m_nextOperationRevision = 0;
};
