#include "SkyCatalogManager.hpp"
#include "SkyCatalogConstellationStore.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourceRecord.hpp"
#include "SkyContextControllerSupport.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/SkyCatalogCacheController.hpp"
#include "catalog/SkyCatalogImportWorkflow.hpp"
#include "catalog/SkyCatalogPresets.hpp"
#include "catalog/SkyCatalogRuntime.hpp"
#include "catalog/SkyCatalogText.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <optional>
#include <utility>
#include <vector>

using namespace skygate::ui::internal;

namespace {

constexpr const char* kPrimarySlotId = "primary";
constexpr const char* kDeepSkySlotId = "deep-sky";

// Source-row status text for a rejected runtime transition: the row reports the
// operation error without the statusText's own "Catalog: " prefix, matching
// the other per-source failure rows.
QString operationErrorText(const QString& statusText)
{
    const QString prefix = QStringLiteral("Catalog: ");
    return statusText.startsWith(prefix) ? statusText.mid(prefix.size()) : statusText;
}

}  // namespace

SkyCatalogManager::SkyCatalogManager(
    SkySettingsStore* settingsStore,
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    QObject* parent,
    QNetworkAccessManager* networkAccessManager
)
    : QObject(parent), m_runtime(std::make_unique<SkyCatalogRuntime>(std::move(starCatalog)))
{
    m_networkAccessManager = networkAccessManager != nullptr ? networkAccessManager : new QNetworkAccessManager(this);
    m_cacheController = std::make_unique<SkyCatalogCacheController>(settingsStore);
    m_importWorkflow = std::make_unique<SkyCatalogImportWorkflow>(m_networkAccessManager);
    applyRuntimeResult(m_runtime->initialize(runtimeBuildOptions()));

    m_catalogUrlText = SkyCatalogPresets::defaultCatalogUrlText();
    m_deepSkyCatalogUrlText = SkyCatalogPresets::defaultDeepSkyCatalogUrlText();
}

SkyCatalogManager::~SkyCatalogManager() = default;

QString SkyCatalogManager::statusText() const
{
    return m_statusText;
}

QString SkyCatalogManager::datasetInfoText() const
{
    return SkyCatalogText::datasetInfo(m_runtime->bodyCount(), m_runtime->constellationCount());
}

QString SkyCatalogManager::deepSkyCatalogInfoText() const
{
    return SkyCatalogText::deepSkyCatalogInfo(m_runtime->deepSkyCatalogFoundObjectCount());
}

bool SkyCatalogManager::downloadingCatalog() const noexcept
{
    return m_downloadingCatalog;
}

bool SkyCatalogManager::catalogProcessing() const noexcept
{
    return m_catalogProcessing;
}

int SkyCatalogManager::catalogPresetIndex() const noexcept
{
    return m_catalogPresetIndex;
}

QString SkyCatalogManager::catalogUrlText() const
{
    return m_catalogUrlText;
}

int SkyCatalogManager::deepSkyCatalogPresetIndex() const noexcept
{
    return m_deepSkyCatalogPresetIndex;
}

QString SkyCatalogManager::deepSkyCatalogUrlText() const
{
    return m_deepSkyCatalogUrlText;
}

QString SkyCatalogManager::sourceLabel() const
{
    return m_runtime->sourceLabel();
}

QString SkyCatalogManager::participationSummary() const
{
    return m_runtime->participationSummary();
}

std::size_t SkyCatalogManager::bodyCount() const noexcept
{
    return m_runtime->bodyCount();
}

std::size_t SkyCatalogManager::constellationCount() const noexcept
{
    return m_runtime->constellationCount();
}

std::uint64_t SkyCatalogManager::catalogRevision() const noexcept
{
    return m_runtime->catalogRevision();
}

const skygate::ephemeris::IStarCatalog* SkyCatalogManager::starCatalog() const noexcept
{
    return m_runtime->starCatalog();
}

std::size_t SkyCatalogManager::sourceCount() const noexcept
{
    return m_runtime->sourceCount();
}

QStringList SkyCatalogManager::sourceInstanceIds() const
{
    return m_runtime->sourceInstanceIds();
}

bool SkyCatalogManager::isSourceEnabled(const QString& instanceId) const
{
    return m_runtime->isSourceEnabled(instanceId);
}

QHash<QString, QString> SkyCatalogManager::sourceTitles() const
{
    return m_runtime->sourceTitles();
}

QString SkyCatalogManager::sourceTitle(const QString& instanceId) const
{
    return m_runtime->sourceTitle(instanceId);
}

QVector<SkyCatalogManager::SourceViewEntry> SkyCatalogManager::sourceViewEntries() const
{
    QVector<SourceViewEntry> entries;
    entries.reserve(static_cast<int>(m_runtime->sourceCount()) + m_sourceOperations.size());

    for (const SkyCatalogSourceRecord& source : m_runtime->sources()) {
        SourceViewEntry entry;
        entry.instanceId = source.instanceId;
        entry.title = source.title;
        entry.version = source.version;
        entry.policy = source.policy;
        entry.enabled = source.enabled;
        entry.bundled = source.bundled;
        entry.objectCount = source.catalog != nullptr ? source.catalog->bodies().size() : 0U;

        const SourceOperation* operation = findOperation(source.instanceId);
        if (operation != nullptr && !operation->statusText.isEmpty()) {
            entry.busy = operation->busy;
            entry.hasError = operation->hasError;
            entry.statusText = operation->statusText;
        } else {
            entry.statusText = source.enabled ? QStringLiteral("Active") : QStringLiteral("Disabled");
        }
        entries.push_back(std::move(entry));
    }

    for (const SourceOperation& operation : m_sourceOperations) {
        if (m_runtime->hasSource(operation.instance.instanceId)) {
            continue;
        }

        SourceViewEntry entry;
        entry.instanceId = operation.instance.instanceId;
        entry.title = operation.instance.title;
        entry.version = operation.instance.version;
        entry.policy = operation.policy;
        entry.enabled = false;
        entry.bundled = false;
        entry.busy = operation.busy;
        entry.hasError = operation.hasError;
        entry.statusText = operation.statusText.isEmpty() ? QStringLiteral("Pending") : operation.statusText;
        entries.push_back(std::move(entry));
    }

    return entries;
}

std::span<const QString> SkyCatalogManager::sourceIds() const noexcept
{
    return m_runtime->sourceIds();
}

const std::vector<QStringList>& SkyCatalogManager::contributorSourceIds() const noexcept
{
    return m_runtime->contributorSourceIds();
}

std::span<const SkyCatalogManager::ConstellationLineRef> SkyCatalogManager::constellationLineRefs() const noexcept
{
    return m_runtime->constellationLineRefs();
}

std::span<const SkyCatalogManager::ConstellationAnchorGroup>
SkyCatalogManager::constellationAnchorGroups() const noexcept
{
    return m_runtime->constellationAnchorGroups();
}

std::span<const SkyCatalogManager::ConstellationLineRef> SkyCatalogManager::resolvedConstellationLineRefs() const
{
    return m_runtime->resolvedConstellationLineRefs();
}

std::span<const SkyCatalogManager::ConstellationAnchorGroup>
SkyCatalogManager::resolvedConstellationAnchorGroups() const
{
    return m_runtime->resolvedConstellationAnchorGroups();
}

void SkyCatalogManager::setCatalogPresetIndex(const int catalogPresetIndex)
{
    m_catalogPresetIndex = SkyCatalogPresets::normalizeCatalogPresetIndex(catalogPresetIndex);
}

void SkyCatalogManager::setDeepSkyCatalogPresetIndex(const int deepSkyCatalogPresetIndex)
{
    m_deepSkyCatalogPresetIndex = SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(deepSkyCatalogPresetIndex);
}

void SkyCatalogManager::setCatalogUrlText(const QString& catalogUrlText)
{
    const QString normalizedCatalogUrlText = catalogUrlText.trimmed();
    m_catalogUrlText =
        normalizedCatalogUrlText.isEmpty() ? SkyCatalogPresets::defaultCatalogUrlText() : normalizedCatalogUrlText;
}

void SkyCatalogManager::setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText)
{
    const QString normalizedUrlText = deepSkyCatalogUrlText.trimmed();
    m_deepSkyCatalogUrlText =
        normalizedUrlText.isEmpty() ? SkyCatalogPresets::defaultDeepSkyCatalogUrlText() : normalizedUrlText;
}

// Legacy two-slot QML entry point retained for compatibility. Delegates to
// loadSourceInstance with the fixed primary slot ID; new callers use
// addSourcePreset.
void SkyCatalogManager::loadCatalogPreset(const QString& presetId)
{
    if (m_downloadingCatalog) {
        return;
    }

    const std::optional<SkyCatalogSourceDescriptor> descriptor = SkyCatalogPresets::starSourceDescriptor(presetId);
    if (!descriptor.has_value()) {
        m_statusText = SkyCatalogText::unknownCatalogPreset(presetId);
        emit statusTextChanged();
        return;
    }

    setCatalogPresetIndex(descriptor->legacyPresetIndex);
    auto source = SkyCatalogSourceInstance::fromDescriptor(*descriptor);
    source.instanceId = QString::fromLatin1(kPrimarySlotId);
    if (!descriptor->bundled) {
        setCatalogUrlText(descriptor->defaultUrl());
    }
    loadSourceInstance(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
}

// Legacy two-slot QML entry point retained for compatibility. Delegates to
// loadSourceInstance with the fixed deep-sky slot ID; new callers use
// addSourcePreset.
void SkyCatalogManager::loadDeepSkyCatalogPreset(const QString& presetId)
{
    if (m_downloadingCatalog) {
        return;
    }

    const std::optional<SkyCatalogSourceDescriptor> descriptor = SkyCatalogPresets::deepSkySourceDescriptor(presetId);
    if (!descriptor.has_value()) {
        m_statusText = SkyCatalogText::unknownDeepSkyPreset(presetId);
        emit statusTextChanged();
        return;
    }

    setDeepSkyCatalogPresetIndex(descriptor->legacyPresetIndex);
    auto source = SkyCatalogSourceInstance::fromDescriptor(*descriptor);
    source.instanceId = QString::fromLatin1(kDeepSkySlotId);
    if (!descriptor->bundled) {
        setDeepSkyCatalogUrlText(descriptor->defaultUrl());
    }
    loadSourceInstance(source, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
}

// Legacy two-slot QML entry point retained for compatibility. Delegates to
// loadSourceInstance with the fixed primary slot ID; new callers use
// addSourceUrl.
void SkyCatalogManager::downloadCatalogFromUrl(const QString& urlText)
{
    setCatalogPresetIndex(2);
    setCatalogUrlText(urlText);
    auto source = SkyCatalogSourceInstance::createCustom(urlText);
    source.instanceId = QString::fromLatin1(kPrimarySlotId);
    loadSourceInstance(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
}

// Legacy two-slot QML entry point retained for compatibility. Delegates to
// loadSourceInstance with the fixed deep-sky slot ID; new callers use
// addSourceUrl.
void SkyCatalogManager::downloadDeepSkyCatalogFromUrl(const QString& urlText)
{
    setDeepSkyCatalogPresetIndex(2);
    setDeepSkyCatalogUrlText(urlText);
    auto source = SkyCatalogSourceInstance::createCustom(urlText);
    source.instanceId = QString::fromLatin1(kDeepSkySlotId);
    loadSourceInstance(source, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
}

// Adds the instance when its durable instance ID is not active yet and
// reloads that instance in place when it is, so an explicit update targets the
// stored instance ID instead of relying on preset/URL ID collisions.
void SkyCatalogManager::loadSource(
    const SkyCatalogSourceInstance& source, const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    loadSourceInstance(source, policy);
}

void SkyCatalogManager::addSourcePreset(const QString& presetId)
{
    if (m_downloadingCatalog) {
        return;
    }

    std::optional<SkyCatalogSourceDescriptor> descriptor = SkyCatalogPresets::starSourceDescriptor(presetId);
    auto policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    if (!descriptor.has_value()) {
        descriptor = SkyCatalogPresets::deepSkySourceDescriptor(presetId);
        policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly;
    }
    if (!descriptor.has_value()) {
        m_statusText = SkyCatalogText::unknownSourcePreset(presetId);
        emit statusTextChanged();
        return;
    }

    loadSourceInstance(SkyCatalogSourceInstance::fromDescriptor(*descriptor), policy);
}

void SkyCatalogManager::addSourceUrl(const QString& urlText, const QString& category)
{
    if (m_downloadingCatalog) {
        return;
    }

    const auto policy = category == SkyCatalogPresets::deepSkyCategory()
                            ? skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly
                            : skygate::ephemeris::CatalogCompositionPolicy::Merge;
    loadSourceInstance(SkyCatalogSourceInstance::createCustom(urlText), policy);
}

void SkyCatalogManager::enableSource(const QString& instanceId)
{
    setSourceEnabled(instanceId, true);
}

void SkyCatalogManager::disableSource(const QString& instanceId)
{
    setSourceEnabled(instanceId, false);
}

void SkyCatalogManager::removeSource(const QString& instanceId)
{
    if (m_downloadingCatalog) {
        return;
    }

    const SkyCatalogRuntimeResult result = m_runtime->removeSource(instanceId, runtimeBuildOptions());
    if (!result.succeeded) {
        applyRuntimeResult(result);
        return;
    }

    // Dropping the operation rejects every callback already captured for this
    // incarnation; a later instance with the same ID receives a fresh revision
    // instead of inheriting the removed one. Completed references are removed
    // together with the runtime record that owns them. The operation is
    // dropped only after the runtime committed the removal, so a rejected
    // removal keeps its pending work and its reported status.
    removeOperation(instanceId);
    persistCatalogCache();
    applyRuntimeResult(result);
}

void SkyCatalogManager::moveSource(const QString& instanceId, const int targetIndex)
{
    if (m_downloadingCatalog || targetIndex < 0) {
        return;
    }

    const SkyCatalogRuntimeResult result =
        m_runtime->moveSource(instanceId, static_cast<std::size_t>(targetIndex), runtimeBuildOptions());
    if (result.succeeded) {
        persistCatalogCache();
    }
    applyRuntimeResult(result);
}

void SkyCatalogManager::retrySource(const QString& instanceId)
{
    if (m_downloadingCatalog) {
        return;
    }

    const SourceOperation* operation = findOperation(instanceId);
    if (operation == nullptr) {
        return;
    }
    loadSourceInstance(operation->instance, operation->policy);
}

void SkyCatalogManager::cancelCatalogDownload()
{
    if (!m_downloadingCatalog && !hasPendingConstellationWork()) {
        return;
    }

    invalidateAllPendingSourceWork();
    const QString activeInstanceId = m_activeDownloadInstanceId;
    m_activeDownloadInstanceId.clear();
    if (m_networkAccessManager != nullptr) {
        const QList<QNetworkReply*> replies = m_networkAccessManager->findChildren<QNetworkReply*>();
        for (QNetworkReply* reply : replies) {
            if (reply != nullptr && reply->isRunning()) {
                reply->abort();
            }
        }
    }
    setDownloadingCatalog(false);
    setCatalogProcessing(false);
    setStatusText(QStringLiteral("Catalog: Download canceled."));
    if (SourceOperation* operation = findOperation(activeInstanceId)) {
        operation->busy = false;
        operation->hasError = false;
        operation->statusText = QStringLiteral("Canceled");
    }
    emit sourcesChanged();
}

bool SkyCatalogManager::clearCatalogCache()
{
    if (m_downloadingCatalog || m_catalogProcessing) {
        m_statusText = SkyCatalogText::cacheClearBlocked();
        emit statusTextChanged();
        return false;
    }

    bool cacheCleared = true;
    if (m_cacheController != nullptr) {
        for (const SkyCatalogSourceRecord& source : m_runtime->sources()) {
            if (source.policy != skygate::ephemeris::CatalogCompositionPolicy::Merge) {
                continue;
            }
            const bool sourceCleared = m_cacheController->clearSourceCache(source.instanceId);
            if (sourceCleared) {
                markSourcePayloadEvicted(source.instanceId);
            }
            cacheCleared = sourceCleared && cacheCleared;
        }
        cacheCleared = m_cacheController->clearCatalogCache() && cacheCleared;
    }
    m_statusText = SkyCatalogText::starCacheClearResult(cacheCleared);
    emit statusTextChanged();
    return cacheCleared;
}

bool SkyCatalogManager::clearDeepSkyCatalogCache()
{
    if (m_downloadingCatalog || m_catalogProcessing) {
        m_statusText = SkyCatalogText::cacheClearBlocked();
        emit statusTextChanged();
        return false;
    }

    bool cacheCleared = true;
    if (m_cacheController != nullptr) {
        for (const SkyCatalogSourceRecord& source : m_runtime->sources()) {
            if (source.policy != skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly) {
                continue;
            }
            const bool sourceCleared = m_cacheController->clearSourceCache(source.instanceId);
            if (sourceCleared) {
                markSourcePayloadEvicted(source.instanceId);
            }
            cacheCleared = sourceCleared && cacheCleared;
        }
        cacheCleared = m_cacheController->clearDeepSkyCatalogCache() && cacheCleared;
    }
    m_statusText = SkyCatalogText::deepSkyCacheClearResult(cacheCleared);
    emit statusTextChanged();
    return cacheCleared;
}

bool SkyCatalogManager::clearSourceCache(const QString& instanceId)
{
    if (m_downloadingCatalog || m_catalogProcessing) {
        m_statusText = SkyCatalogText::cacheClearBlocked();
        emit statusTextChanged();
        return false;
    }

    const bool cacheCleared = m_cacheController != nullptr && m_cacheController->clearSourceCache(instanceId);
    if (cacheCleared) {
        // The runtime keeps displaying the accepted snapshot, but its bytes
        // must not return to the configured record through a later unrelated
        // persistence; only an accepted reload may write them again.
        markSourcePayloadEvicted(instanceId);
    }
    m_statusText = SkyCatalogText::sourceCacheClearResult(cacheCleared);
    emit statusTextChanged();
    return cacheCleared;
}

bool SkyCatalogManager::restoreCatalogCache()
{
    if (m_cacheController == nullptr) {
        return false;
    }

    auto restoreResult = m_cacheController->restoreCollection(
        m_catalogPresetIndex, m_deepSkyCatalogPresetIndex, m_catalogUrlText, m_deepSkyCatalogUrlText
    );
    // A stored snapshot is a restored configuration even when it holds no
    // source: the cache controller distinguishes an intentionally empty
    // collection from "no collection was ever stored". First-use defaults
    // therefore remain only when nothing was stored, and an empty collection
    // is installed through the same staged replacement as a populated one, so
    // a rejected composition leaves the previous configuration untouched.
    if (!restoreResult.restored) {
        return false;
    }

    // Apply every restored source into a single runtime replacement and emit
    // the change signals once, so the controller does not rebuild the
    // ephemeris engine and search model per restored source. The records are
    // staged before the runtime transition; the owner-bound operations, their
    // payloads, and the cache are committed only when the transition is
    // accepted, so a rejected collection leaves the previous configuration,
    // operations, and cache untouched.
    std::vector<SkyCatalogSourceRecord> restoredSources;
    restoredSources.reserve(restoreResult.sources.size());
    // Facts the operation commit needs after the records are moved into the
    // runtime, kept in restore order.
    std::vector<skygate::ephemeris::CatalogCompositionPolicy> restoredPolicies;
    restoredPolicies.reserve(restoreResult.sources.size());
    QStringList unavailableInstanceIds;
    bool requiresPersist = restoreResult.migratedLegacy || restoreResult.requiresRecordUpgrade;
    for (SkyCatalogSourceRestoreEntry& entry : restoreResult.sources) {
        requiresPersist = requiresPersist || entry.requiresBinaryUpgrade;
        restoredPolicies.push_back(entry.record.policy);
        if (entry.record.catalog == nullptr) {
            // The configured source keeps its identity, order, policy, and
            // participation state; only its payload is unavailable, so it is
            // restored as an explicit error instead of being omitted.
            unavailableInstanceIds.push_back(entry.instance.instanceId);
        }

        // The restored record, operation, cache entry, and provenance all stay
        // keyed by the stored instance ID; restore never re-derives it.
        restoredSources.push_back(std::move(entry.record));
    }

    // Each restored source record carries its own related constellation
    // dataset, so replacing the collection installs the owned data in the same
    // step as the catalogs and the active view is composed from it.
    const SkyCatalogRuntimeResult mergedResult =
        m_runtime->replaceSources(std::move(restoredSources), runtimeBuildOptions());
    if (!mergedResult.succeeded) {
        applyRuntimeResult(mergedResult);
        return false;
    }

    // The accepted restore replaces the whole operation collection together
    // with the runtime: every restored instance starts an operation revision
    // that was never handed out, and an operation omitted from the restored
    // collection is dropped. A reply captured for a superseded incarnation
    // therefore finds no current operation and can no longer apply its source
    // to the replacement, not even when a restored instance reuses its ID or
    // when the reply belongs to a pending related download.
    QVector<SourceOperation> restoredOperations;
    restoredOperations.reserve(static_cast<qsizetype>(restoreResult.sources.size()));
    for (std::size_t index = 0; index < restoreResult.sources.size(); ++index) {
        const SkyCatalogSourceRestoreEntry& entry = restoreResult.sources[index];
        SourceOperation operation;
        operation.instance = entry.instance;
        operation.policy = restoredPolicies[index];
        operation.revision = ++m_nextOperationRevision;
        commitAcceptedSourceFacts(operation, entry.payload);
        if (unavailableInstanceIds.contains(entry.instance.instanceId)) {
            operation.hasError = true;
            operation.statusText = SkyCatalogText::sourcePayloadUnavailable();
        }
        restoredOperations.push_back(std::move(operation));
    }
    m_sourceOperations = std::move(restoredOperations);

    // The replaced collection owns no in-flight work: import or related replies
    // of a superseded source are discarded by the revision guards above, so the
    // download state is reset with the collection instead of continuing to
    // report work that can no longer be accepted.
    m_activeDownloadInstanceId.clear();
    setDownloadingCatalog(false);
    setCatalogProcessing(false);

    const QString runtimeStatusText = mergedResult.statusText;
    applyRuntimeResult(mergedResult);

    const std::size_t unavailableSourceCount = static_cast<std::size_t>(unavailableInstanceIds.size());
    if (unavailableSourceCount > 0U) {
        setStatusText(SkyCatalogText::unavailableSourceSummary(runtimeStatusText, unavailableSourceCount));
    }

    // First restore from a legacy or raw-only cache upgrades it into the
    // versioned binary collection so subsequent startups skip the CSV re-parse.
    if (requiresPersist) {
        persistCatalogCache();
    }
    return true;
}

void SkyCatalogManager::loadSourceInstance(
    const SkyCatalogSourceInstance& source, const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    if (m_downloadingCatalog) {
        return;
    }

    // Adding an instance allocates its durable ID. Callers that already hold an
    // ID (retry, restore, or an explicit update) keep it unchanged.
    SkyCatalogSourceInstance instance = source;
    if (instance.instanceId.isEmpty()) {
        instance.instanceId = SkyCatalogSourceInstance::allocateInstanceId();
    }

    SourceOperation* operation = upsertOperation(instance, policy);
    // Loading or updating this instance supersedes the work of its previous
    // incarnation only: every other source keeps its pending requests, its
    // revisions, and its reported status.
    invalidatePendingSourceWork(instance.instanceId);
    operation->busy = true;
    operation->hasError = false;
    operation->statusText = instance.urls.isEmpty() ? QStringLiteral("Loading...") : QStringLiteral("Downloading...");
    emit sourcesChanged();

    if (instance.urls.isEmpty()) {
        applyBundledSource(*operation, policy);
        return;
    }

    if (m_importWorkflow == nullptr || !m_importWorkflow->isAvailable()) {
        operation->busy = false;
        operation->hasError = true;
        operation->statusText = QStringLiteral("Network unavailable");
        setStatusText(QStringLiteral("Catalog: Network unavailable"));
        emit sourcesChanged();
        return;
    }

    const std::uint64_t revision = operation->revision;
    m_activeDownloadInstanceId = instance.instanceId;
    setCatalogProcessing(false);
    setDownloadingCatalog(true);

    m_importWorkflow->downloadSource(
        instance,
        policy,
        this,
        [this, instanceId = instance.instanceId, revision](const QString& statusText) {
            if (isOperationCurrent(instanceId, revision)) {
                handleCatalogImportStatus(instanceId, statusText);
            }
        },
        [this, instanceId = instance.instanceId, revision, policy, relatedDatasetUrls = instance.relatedDatasetUrls](
            SkyCatalogSourceImportResult result
        ) {
            if (!isOperationCurrent(instanceId, revision)) {
                return;
            }
            if (m_activeDownloadInstanceId == instanceId) {
                m_activeDownloadInstanceId.clear();
            }
            handleSourceImportFinished(instanceId, revision, std::move(result), policy, relatedDatasetUrls);
        }
    );
}

void SkyCatalogManager::applyBundledSource(
    SourceOperation& operation, const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    auto bundledCatalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    if (bundledCatalog == nullptr) {
        operation.busy = false;
        operation.hasError = true;
        operation.statusText = QStringLiteral("Failed to load");
        setStatusText(QStringLiteral("Catalog: Failed to load"));
        emit sourcesChanged();
        return;
    }

    SkyCatalogSourceRecord record;
    record.instanceId = operation.instance.instanceId;
    record.title = operation.instance.title;
    record.version = operation.instance.version;
    record.policy = policy;
    record.enabled = true;
    record.bundled = true;
    record.catalog = std::move(bundledCatalog);
    record.foundObjectCount = 0;

    SkyCatalogRuntimeResult result = m_runtime->applySource(std::move(record), runtimeBuildOptions());
    operation.busy = false;
    if (!result.succeeded) {
        // The runtime refused the transition: the row reports the operation
        // error and the committed collection stays active.
        operation.hasError = true;
        operation.statusText = operationErrorText(result.statusText);
        applyRuntimeResult(result);
        return;
    }

    operation.hasError = false;
    applyRelatedDeclarationTransition(operation, operation.instance.relatedDatasetUrls, result);
    // A completed load leaves no operation status behind: the settled row state
    // is the collection's own state, so disabling the source afterwards is
    // presented as disabled instead of as the active source it once loaded.
    operation.statusText.clear();
    // The bundled source has no payload, so its accepted facts are the
    // configuration that was just activated.
    commitAcceptedSourceFacts(operation, QByteArray());
    persistCatalogCache();
    applyRuntimeResult(result);
}

void SkyCatalogManager::applyRelatedDeclarationTransition(
    const SourceOperation& operation,
    const QStringList& acceptedRelatedDatasetUrls,
    SkyCatalogRuntimeResult& activationResult
)
{
    const bool relatedDeclarationRemoved = acceptedRelatedDatasetUrls.isEmpty() && operation.hasAcceptedInstance
                                           && !operation.acceptedInstance.relatedDatasetUrls.isEmpty();
    if (acceptedRelatedDatasetUrls.isEmpty() && !relatedDeclarationRemoved) {
        return;
    }

    const SkyCatalogRuntimeResult clearResult = m_runtime->clearSourceConstellationRefs(operation.instance.instanceId);
    activationResult.catalogChanged = activationResult.catalogChanged || clearResult.catalogChanged;
    activationResult.datasetInfoChanged = activationResult.datasetInfoChanged || clearResult.datasetInfoChanged;
    // The clear is part of the committed replacement, so every published summary
    // is re-read instead of keeping the pre-clear counts.
    activationResult.statusText = m_runtime->statusText();
}

// Applies the parsed result to the runtime. Only a committed transition is
// persisted; a rejected collection is not configuration and must not replace
// the stored one. The caller commits the accepted descriptor and payload before
// persisting, so neither an attempt nor a mixture of attempted options and
// accepted bytes is ever serialized.
SkyCatalogRuntimeResult SkyCatalogManager::applySourceResult(
    SkyCatalogSourceImportResult result, const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    SkyCatalogSourceRecord record;
    record.instanceId = result.sourceId;
    record.title = result.sourceLabel;
    record.version = result.sourceVersion;
    record.url = result.sourceUrl;
    record.policy = policy;
    record.enabled = true;
    record.catalog = std::move(result.catalog);
    record.foundObjectCount = result.foundObjectCount;

    const SkyCatalogRuntimeResult runtimeResult = m_runtime->applySource(std::move(record), runtimeBuildOptions());
    return runtimeResult;
}

void SkyCatalogManager::handleSourceImportFinished(
    const QString& instanceId,
    const std::uint64_t revision,
    SkyCatalogSourceImportResult result,
    const skygate::ephemeris::CatalogCompositionPolicy policy,
    const QStringList& relatedDatasetUrls
)
{
    setCatalogProcessing(false);
    SourceOperation* operation = findOperation(instanceId);
    if (operation == nullptr || operation->revision != revision) {
        return;
    }

    if (result.catalog == nullptr) {
        operation->busy = false;
        operation->hasError = true;
        operation->statusText = result.errorText;
        setDownloadingCatalog(false);
        setStatusText(result.errorText);
        emit sourcesChanged();
        return;
    }

    const QByteArray acceptedPayload = result.payload;
    const std::size_t foundObjectCount = result.foundObjectCount;
    const auto diagnostics = result.diagnostics;
    const QString sourceLabel = result.sourceLabel;

    SkyCatalogRuntimeResult runtimeResult = applySourceResult(std::move(result), policy);
    if (!runtimeResult.succeeded) {
        // The catalog was parsed but the runtime refused the collection
        // transition: the source is not installed (or the previous record
        // stays) and the pending related dataset is left untouched, so the
        // operation reports the error instead of proceeding as if the source
        // were active. Nothing was accepted, so the previously accepted
        // descriptor and payload stay in place together.
        operation->busy = false;
        operation->hasError = true;
        operation->statusText = operationErrorText(runtimeResult.statusText);
        setDownloadingCatalog(false);
        applyRuntimeResult(runtimeResult);
        return;
    }

    applyRelatedDeclarationTransition(*operation, relatedDatasetUrls, runtimeResult);

    operation->busy = false;
    operation->hasError = false;
    // A completed load leaves no operation status behind: the settled row state
    // is the collection's own state, so disabling the source afterwards is
    // presented as disabled instead of as the active source it once loaded.
    operation->statusText.clear();
    commitAcceptedSourceFacts(*operation, acceptedPayload);
    persistCatalogCache();

    applyRuntimeResult(runtimeResult);

    if (policy == skygate::ephemeris::CatalogCompositionPolicy::Merge && diagnostics.truncatedBodyCount > 0U) {
        setStatusText(
            SkyCatalogText::brightnessFilterSummary(
                m_statusText, diagnostics.selectedBodyCount, diagnostics.parsedBodyCount
            )
        );
    } else if (policy == skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly && foundObjectCount > 0U) {
        setStatusText(SkyCatalogText::deepSkyFoundSummary(m_statusText, sourceLabel, foundObjectCount));
    }

    setDownloadingCatalog(false);

    if (!relatedDatasetUrls.isEmpty()) {
        // The replacement dataset is requested only after the accepted
        // related-data state has been published and persisted, so its pending
        // interval starts from the state observers already saw.
        downloadConstellationLinesAfterCatalog(instanceId, revision, relatedDatasetUrls, m_statusText);
    }
}

void SkyCatalogManager::downloadConstellationLinesAfterCatalog(
    const QString& instanceId,
    const std::uint64_t revision,
    const QStringList& constellationLineUrlTexts,
    const QString& catalogSummaryText
)
{
    if (m_importWorkflow == nullptr || constellationLineUrlTexts.isEmpty()) {
        return;
    }

    SourceOperation* operation = findOperation(instanceId);
    if (operation == nullptr || operation->revision != revision) {
        return;
    }

    operation->constellationPending = true;
    m_importWorkflow->downloadConstellationLines(
        constellationLineUrlTexts,
        this,
        [this, catalogSummaryText, instanceId, revision](const QString& statusText) {
            if (isOperationCurrent(instanceId, revision)) {
                handleConstellationLineImportStatus(catalogSummaryText, statusText);
            }
        },
        [this, catalogSummaryText, instanceId, revision](SkyConstellationLineImportResult lineResult) {
            if (!isOperationCurrent(instanceId, revision)) {
                return;
            }
            if (SourceOperation* operation = findOperation(instanceId)) {
                operation->constellationPending = false;
            }
            handleConstellationLineImportFinished(instanceId, catalogSummaryText, std::move(lineResult));
        }
    );
}

void SkyCatalogManager::handleConstellationLineImportStatus(
    const QString& catalogSummaryText, const QString& statusText
)
{
    setStatusText(SkyCatalogText::constellationLineSummary(catalogSummaryText, statusText));
}

void SkyCatalogManager::handleConstellationLineImportFinished(
    const QString& instanceId, const QString& catalogSummaryText, SkyConstellationLineImportResult lineResult
)
{
    if (lineResult.hasCustomLines()) {
        const SkyCatalogRuntimeResult result = m_runtime->setSourceConstellationRefs(
            instanceId,
            std::move(lineResult.lineRefs),
            std::move(lineResult.anchorGroups),
            lineResult.constellationCount
        );
        if (result.datasetInfoChanged) {
            emit datasetInfoTextChanged();
        }
        setStatusText(SkyCatalogText::constellationLineSummary(catalogSummaryText, lineResult.statusSuffix));
        persistCatalogCache();
        if (result.catalogChanged) {
            emit catalogChanged();
        }
        return;
    }
    setStatusText(SkyCatalogText::constellationLineSummary(catalogSummaryText, lineResult.statusSuffix));
    persistCatalogCache();
}

void SkyCatalogManager::invalidatePendingSourceWork(const QString& instanceId)
{
    SourceOperation* operation = findOperation(instanceId);
    if (operation == nullptr) {
        return;
    }

    operation->revision = ++m_nextOperationRevision;
    operation->constellationPending = false;
}

void SkyCatalogManager::invalidateAllPendingSourceWork()
{
    for (SourceOperation& operation : m_sourceOperations) {
        operation.revision = ++m_nextOperationRevision;
        operation.constellationPending = false;
    }
}

bool SkyCatalogManager::hasPendingConstellationWork() const
{
    for (const SourceOperation& operation : m_sourceOperations) {
        if (operation.constellationPending) {
            return true;
        }
    }
    return false;
}

void SkyCatalogManager::setSourceEnabled(const QString& instanceId, const bool enabled)
{
    if (m_downloadingCatalog) {
        return;
    }

    const SkyCatalogRuntimeResult result = m_runtime->setSourceEnabled(instanceId, enabled, runtimeBuildOptions());
    if (!result.succeeded) {
        applyRuntimeResult(result);
        return;
    }

    if (!enabled) {
        // Disabling an owner supersedes its own in-flight related work: a
        // response completing after the disable is discarded instead of
        // replacing the owner's dataset. Data that completed before the
        // disable stays owned and inactive, and re-enabling the owner restores
        // only that retained data. Other sources are unaffected. The pending
        // work is superseded only after the runtime committed the disable, so
        // a rejected transition does not discard a response for an owner that
        // is still enabled.
        invalidatePendingSourceWork(instanceId);
    }

    persistCatalogCache();
    applyRuntimeResult(result);
}

void SkyCatalogManager::setStatusText(const QString& statusText)
{
    m_statusText = statusText;
    emit statusTextChanged();
}

void SkyCatalogManager::setDownloadingCatalog(const bool downloadingCatalog)
{
    if (m_downloadingCatalog == downloadingCatalog) {
        return;
    }

    m_downloadingCatalog = downloadingCatalog;
    emit downloadingCatalogChanged();
}

void SkyCatalogManager::setCatalogProcessing(const bool catalogProcessing)
{
    if (m_catalogProcessing == catalogProcessing) {
        return;
    }

    m_catalogProcessing = catalogProcessing;
    emit catalogProcessingChanged();
}

void SkyCatalogManager::handleCatalogImportStatus(const QString& instanceId, const QString& statusText)
{
    setStatusText(statusText);
    setCatalogProcessing(SkyCatalogText::isProcessingStatus(statusText));
    if (SourceOperation* operation = findOperation(instanceId)) {
        operation->busy = true;
        operation->hasError = false;
        operation->statusText = statusText;
    }
    emit sourcesChanged();
}

SkyCatalogRuntimeBuildOptions SkyCatalogManager::runtimeBuildOptions() const
{
    // The legacy deep-sky preset selects bundled Messier. That control maps to
    // the same explicit bundled participation the composition configuration
    // carries, instead of the runtime independently deciding a hidden source
    // insertion from the collection's current contents.
    const auto bundledDeepSkyParticipation = m_deepSkyCatalogPresetIndex == 0
                                                 ? SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation::Fallback
                                                 : SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation::Disabled;
    return SkyCatalogRuntimeBuildOptions{.bundledDeepSkyParticipation = bundledDeepSkyParticipation};
}

void SkyCatalogManager::applyRuntimeResult(const SkyCatalogRuntimeResult& result)
{
    if (result.statusTextChanged) {
        setStatusText(result.statusText);
    }
    if (result.datasetInfoChanged) {
        emit datasetInfoTextChanged();
    }
    if (result.deepSkyCatalogInfoChanged) {
        emit deepSkyCatalogInfoTextChanged();
    }
    if (result.catalogChanged) {
        emit catalogChanged();
    }
    emit sourcesChanged();
}

void SkyCatalogManager::persistCatalogCache() const
{
    if (m_cacheController == nullptr) {
        return;
    }

    SkyCatalogCollectionPersistRequest request;
    for (const SkyCatalogSourceRecord& source : m_runtime->sources()) {
        SkyCatalogSourcePersistEntry entry;
        entry.instanceId = source.instanceId;
        entry.title = source.title;
        entry.version = source.version;
        entry.url = source.url;
        entry.policy = source.policy;
        entry.enabled = source.enabled;
        entry.bundled = source.bundled;

        // A source with its own download keeps its accepted descriptor and the
        // payload that was activated with it, so an attempted update that never
        // became active cannot leak its options into the stored record. A
        // source that needs no payload (bundled or synthesized) is persisted by
        // configuration alone and reconstructed from the bundled factory on
        // restore, so its identity, order, policy, and enabled state survive
        // even when it has no operation record. An evicted payload stays out
        // of the record even though the accepted in-memory snapshot still
        // carries it: only an accepted load repopulates the payload.
        const SourceOperation* operation = findOperation(source.instanceId);
        const bool payloadEvicted = operation != nullptr && operation->payloadEvicted;
        if (operation != nullptr && operation->hasAcceptedInstance) {
            entry.descriptorId = operation->acceptedInstance.descriptorId;
            entry.title = operation->acceptedInstance.title;
            entry.version = operation->acceptedInstance.version;
            entry.urls = operation->acceptedInstance.urls;
            entry.relatedDatasetUrls = operation->acceptedInstance.relatedDatasetUrls;
            entry.archiveSelector = operation->acceptedInstance.archiveSelector;
            entry.schemaHint = operation->acceptedInstance.schemaHint;
            entry.attribution = operation->acceptedInstance.attribution;
            if (!source.bundled && !payloadEvicted) {
                entry.catalog = source.catalog.get();
                entry.payload = operation->acceptedPayload;
            }
        }

        // Each source's related dataset is serialized from the dataset that
        // source itself owns, so a restart attributes every line, anchor, and
        // count to the source whose download produced it. A source that has
        // not completed a related download keeps no related payload, and a
        // disabled owner keeps its own payload even though it contributes
        // nothing to the active view. The related dataset is payload of the
        // same source, so it stays out of an evicted record until a later
        // accepted load commits a payload again.
        const SkyCatalogConstellationStore& ownedRelatedData = source.constellationData;
        if (!source.bundled && !payloadEvicted
            && (!ownedRelatedData.lineRefVector().empty() || !ownedRelatedData.anchorGroupVector().empty()
                || ownedRelatedData.count() > 0U)) {
            entry.constellationLineRows =
                SkyContextCatalogCodec::serializeConstellationLineRows(ownedRelatedData.lineRefVector());
            entry.constellationAnchorGroupRows =
                SkyContextCatalogCodec::serializeConstellationAnchorGroupRows(ownedRelatedData.anchorGroupVector());
            entry.constellationLineSchemaVersion = SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
            entry.constellationCount = ownedRelatedData.count();
        }
        request.sources.push_back(std::move(entry));
    }
    m_cacheController->persistCollection(request);
}

SkyCatalogManager::SourceOperation* SkyCatalogManager::upsertOperation(
    const SkyCatalogSourceInstance& source, const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    // Requesting a configuration records it as the instance's attempt without
    // touching the accepted facts: an attempt that is never accepted must not
    // replace what persistence and consumers still see as the source.
    SourceOperation* existing = findOperation(source.instanceId);
    if (existing != nullptr) {
        existing->instance = source;
        existing->policy = policy;
        return existing;
    }

    SourceOperation operation;
    operation.instance = source;
    operation.policy = policy;
    operation.revision = ++m_nextOperationRevision;
    m_sourceOperations.push_back(std::move(operation));
    return &m_sourceOperations.back();
}

SkyCatalogManager::SourceOperation* SkyCatalogManager::findOperation(const QString& instanceId)
{
    for (SourceOperation& operation : m_sourceOperations) {
        if (operation.instance.instanceId == instanceId) {
            return &operation;
        }
    }
    return nullptr;
}

const SkyCatalogManager::SourceOperation* SkyCatalogManager::findOperation(const QString& instanceId) const
{
    for (const SourceOperation& operation : m_sourceOperations) {
        if (operation.instance.instanceId == instanceId) {
            return &operation;
        }
    }
    return nullptr;
}

void SkyCatalogManager::removeOperation(const QString& instanceId)
{
    for (int index = 0; index < m_sourceOperations.size(); ++index) {
        if (m_sourceOperations[index].instance.instanceId == instanceId) {
            m_sourceOperations.removeAt(index);
            return;
        }
    }
}

void SkyCatalogManager::markSourcePayloadEvicted(const QString& instanceId)
{
    if (SourceOperation* operation = findOperation(instanceId)) {
        operation->payloadEvicted = true;
    }
}

bool SkyCatalogManager::isOperationCurrent(const QString& instanceId, const std::uint64_t revision) const
{
    const SourceOperation* operation = findOperation(instanceId);
    return operation != nullptr && operation->revision == revision;
}

void SkyCatalogManager::commitAcceptedSourceFacts(SourceOperation& operation, QByteArray payload)
{
    operation.acceptedInstance = operation.instance;
    operation.acceptedPayload = std::move(payload);
    operation.hasAcceptedInstance = true;
    // A successful activation supersedes an earlier payload eviction: the
    // accepted facts now describe a payload that may be persisted again.
    operation.payloadEvicted = false;
}
