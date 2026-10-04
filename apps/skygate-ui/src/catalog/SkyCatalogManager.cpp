#include "SkyCatalogManager.hpp"
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

QString stableSourceInstanceId(const SkyCatalogSourceInstance& instance)
{
    if (!instance.descriptorId.isEmpty()) {
        return QStringLiteral("preset:") + instance.descriptorId;
    }
    if (!instance.urls.isEmpty()) {
        return SkyCatalogSourceInstance::createCustom(instance.urls.first()).instanceId;
    }
    return instance.instanceId;
}

SkyCatalogRuntimeResult& operator|=(SkyCatalogRuntimeResult& target, const SkyCatalogRuntimeResult& source)
{
    if (source.statusTextChanged) {
        target.statusText = source.statusText;
        target.statusTextChanged = true;
    }
    target.datasetInfoChanged = target.datasetInfoChanged || source.datasetInfoChanged;
    target.deepSkyCatalogInfoChanged = target.deepSkyCatalogInfoChanged || source.deepSkyCatalogInfoChanged;
    target.catalogChanged = target.catalogChanged || source.catalogChanged;
    return target;
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

QStringList SkyCatalogManager::sourceLabels() const
{
    return m_runtime->sourceLabels();
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

std::span<const std::uint8_t> SkyCatalogManager::sourceIds() const noexcept
{
    return m_runtime->sourceIds();
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

void SkyCatalogManager::downloadCatalogFromUrl(const QString& urlText)
{
    setCatalogPresetIndex(2);
    setCatalogUrlText(urlText);
    auto source = SkyCatalogSourceInstance::createCustom(urlText);
    source.instanceId = QString::fromLatin1(kPrimarySlotId);
    loadSourceInstance(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
}

void SkyCatalogManager::downloadDeepSkyCatalogFromUrl(const QString& urlText)
{
    setDeepSkyCatalogPresetIndex(2);
    setDeepSkyCatalogUrlText(urlText);
    auto source = SkyCatalogSourceInstance::createCustom(urlText);
    source.instanceId = QString::fromLatin1(kDeepSkySlotId);
    loadSourceInstance(source, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
}

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

    invalidatePendingSourceWork();
    removeOperation(instanceId);
    const SkyCatalogRuntimeResult result = m_runtime->removeSource(instanceId, runtimeBuildOptions());
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
    persistCatalogCache();
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
    if (!m_downloadingCatalog && !m_constellationDownloadPending) {
        return;
    }

    invalidatePendingSourceWork();
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
            const SourceOperation* operation = findOperation(source.instanceId);
            const QString persistedId =
                operation != nullptr ? stableSourceInstanceId(operation->instance) : source.instanceId;
            cacheCleared = m_cacheController->clearSourceCache(persistedId) && cacheCleared;
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
            const SourceOperation* operation = findOperation(source.instanceId);
            const QString persistedId =
                operation != nullptr ? stableSourceInstanceId(operation->instance) : source.instanceId;
            cacheCleared = m_cacheController->clearSourceCache(persistedId) && cacheCleared;
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

    const SourceOperation* operation = findOperation(instanceId);
    const QString persistedId = operation != nullptr ? stableSourceInstanceId(operation->instance) : instanceId;
    const bool cacheCleared = m_cacheController != nullptr && m_cacheController->clearSourceCache(persistedId);
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
    if (restoreResult.sources.empty() && restoreResult.constellationLineRefs.empty()
        && !restoreResult.resetConstellationLineRefs) {
        return false;
    }

    // Apply every restored source into a single runtime replacement and emit
    // the change signals once, so the controller does not rebuild the
    // ephemeris engine and search model per restored source.
    std::vector<SkyCatalogSourceRecord> restoredSources;
    restoredSources.reserve(restoreResult.sources.size());
    bool requiresPersist = restoreResult.migratedLegacy;
    for (SkyCatalogSourceRestoreEntry& entry : restoreResult.sources) {
        requiresPersist = requiresPersist || entry.requiresBinaryUpgrade;

        SourceOperation* operation = upsertOperation(entry.instance, entry.record.policy);
        operation->payload = entry.payload;

        SkyCatalogSourceRecord record = std::move(entry.record);
        record.instanceId = stableSourceInstanceId(entry.instance);
        restoredSources.push_back(std::move(record));
    }

    SkyCatalogRuntimeResult mergedResult = m_runtime->replaceSources(std::move(restoredSources), runtimeBuildOptions());

    if (!restoreResult.constellationLineRefs.empty()) {
        mergedResult |= m_runtime->restoreConstellationRefs(
            std::move(restoreResult.constellationLineRefs),
            std::move(restoreResult.constellationAnchorGroups),
            restoreResult.constellationCount
        );
    }

    if (restoreResult.resetConstellationLineRefs) {
        mergedResult |= m_runtime->resetConstellationLineRefs();
    }

    applyRuntimeResult(mergedResult);

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

    SourceOperation* operation = upsertOperation(source, policy);
    invalidatePendingSourceWork();
    operation->busy = true;
    operation->hasError = false;
    operation->statusText = source.urls.isEmpty() ? QStringLiteral("Loading...") : QStringLiteral("Downloading...");
    emit sourcesChanged();

    if (source.urls.isEmpty()) {
        operation->payload.clear();
        if (policy == skygate::ephemeris::CatalogCompositionPolicy::Merge) {
            resetConstellationLineRefs();
        }
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
    m_activeDownloadInstanceId = source.instanceId;
    setCatalogProcessing(false);
    setDownloadingCatalog(true);

    m_importWorkflow->downloadSource(
        source,
        policy,
        this,
        [this, instanceId = source.instanceId, revision](const QString& statusText) {
            if (isOperationCurrent(instanceId, revision)) {
                handleCatalogImportStatus(instanceId, statusText);
            }
        },
        [this, instanceId = source.instanceId, revision, policy, relatedDatasetUrls = source.relatedDatasetUrls](
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

    const SkyCatalogRuntimeResult result = m_runtime->applySource(std::move(record), runtimeBuildOptions());
    operation.busy = false;
    operation.hasError = false;
    operation.statusText = QStringLiteral("Active");
    persistCatalogCache();
    applyRuntimeResult(result);
}

void SkyCatalogManager::applySourceResult(
    SkyCatalogSourceImportResult result, const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    SkyCatalogSourceRecord record;
    record.instanceId = result.sourceId;
    record.title = result.sourceLabel;
    record.version = result.sourceVersion;
    record.policy = policy;
    record.enabled = true;
    record.catalog = std::move(result.catalog);
    record.foundObjectCount = result.foundObjectCount;

    const SkyCatalogRuntimeResult runtimeResult = m_runtime->applySource(std::move(record), runtimeBuildOptions());
    persistCatalogCache();
    applyRuntimeResult(runtimeResult);
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

    operation->payload = result.payload;
    operation->busy = false;
    operation->hasError = false;
    operation->statusText = QStringLiteral("Active");
    const std::size_t foundObjectCount = result.foundObjectCount;
    const auto diagnostics = result.diagnostics;
    const QString sourceLabel = result.sourceLabel;

    applySourceResult(std::move(result), policy);

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
        resetConstellationLineRefs();
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
    m_constellationDownloadPending = true;
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
            m_constellationDownloadPending = false;
            handleConstellationLineImportFinished(catalogSummaryText, std::move(lineResult));
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
    const QString& catalogSummaryText, SkyConstellationLineImportResult lineResult
)
{
    if (lineResult.hasCustomLines()) {
        const SkyCatalogRuntimeResult result = m_runtime->restoreConstellationRefs(
            std::move(lineResult.lineRefs),
            std::move(lineResult.anchorGroups),
            lineResult.constellationCount > 0U ? std::optional<std::size_t>(lineResult.constellationCount)
                                               : std::nullopt
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

void SkyCatalogManager::invalidatePendingSourceWork()
{
    for (SourceOperation& operation : m_sourceOperations) {
        ++operation.revision;
        operation.constellationPending = false;
    }
    m_constellationDownloadPending = false;
}

void SkyCatalogManager::setSourceEnabled(const QString& instanceId, const bool enabled)
{
    if (m_downloadingCatalog) {
        return;
    }

    const SkyCatalogRuntimeResult result = m_runtime->setSourceEnabled(instanceId, enabled, runtimeBuildOptions());
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
    return SkyCatalogRuntimeBuildOptions{.useBundledDeepSkyCatalog = m_deepSkyCatalogPresetIndex == 0};
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

void SkyCatalogManager::resetConstellationLineRefs()
{
    static_cast<void>(m_runtime->resetConstellationLineRefs());
}

void SkyCatalogManager::persistCatalogCache() const
{
    if (m_cacheController == nullptr) {
        return;
    }

    SkyCatalogCollectionPersistRequest request;
    for (const SkyCatalogSourceRecord& source : m_runtime->sources()) {
        const SourceOperation* operation = findOperation(source.instanceId);
        if (operation == nullptr || operation->instance.urls.isEmpty()) {
            // Bundled/synthesized sources are reconstructed at startup; only
            // configured sources with their own data need persistence.
            continue;
        }

        SkyCatalogSourcePersistEntry entry;
        entry.instanceId = stableSourceInstanceId(operation->instance);
        entry.descriptorId = operation->instance.descriptorId;
        entry.title = operation->instance.title;
        entry.version = operation->instance.version;
        entry.urls = operation->instance.urls;
        entry.relatedDatasetUrls = operation->instance.relatedDatasetUrls;
        entry.archiveSelector = operation->instance.archiveSelector;
        entry.policy = source.policy;
        entry.enabled = source.enabled;
        entry.catalog = source.catalog.get();
        entry.payload = operation->payload;

        if (source.policy == skygate::ephemeris::CatalogCompositionPolicy::Merge
            && !operation->instance.relatedDatasetUrls.isEmpty()) {
            std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs(
                m_runtime->constellationLineRefs().begin(), m_runtime->constellationLineRefs().end()
            );
            std::vector<skygate::ephemeris::ConstellationAnchorGroup> anchorGroups(
                m_runtime->constellationAnchorGroups().begin(), m_runtime->constellationAnchorGroups().end()
            );
            entry.constellationLineRows = SkyContextCatalogCodec::serializeConstellationLineRows(lineRefs);
            entry.constellationAnchorGroupRows =
                SkyContextCatalogCodec::serializeConstellationAnchorGroupRows(anchorGroups);
            entry.constellationLineSchemaVersion = SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
            entry.constellationCount = m_runtime->constellationCount();
        }
        request.sources.push_back(std::move(entry));
    }
    m_cacheController->persistCollection(request);
}

SkyCatalogManager::SourceOperation* SkyCatalogManager::upsertOperation(
    const SkyCatalogSourceInstance& source, const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    SourceOperation* existing = findOperation(source.instanceId);
    if (existing != nullptr) {
        existing->instance = source;
        existing->policy = policy;
        return existing;
    }

    SourceOperation operation;
    operation.instance = source;
    operation.policy = policy;
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

bool SkyCatalogManager::isOperationCurrent(const QString& instanceId, const std::uint64_t revision) const
{
    const SourceOperation* operation = findOperation(instanceId);
    return operation != nullptr && operation->revision == revision;
}
