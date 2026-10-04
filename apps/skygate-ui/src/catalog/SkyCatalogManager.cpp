#include "SkyCatalogManager.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourceRecord.hpp"
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

using namespace skygate::ui::internal;

namespace {

constexpr const char* kPrimarySlotId = "primary";
constexpr const char* kDeepSkySlotId = "deep-sky";

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
}

bool SkyCatalogManager::clearCatalogCache()
{
    if (m_downloadingCatalog || m_catalogProcessing) {
        m_statusText = SkyCatalogText::cacheClearBlocked();
        emit statusTextChanged();
        return false;
    }

    const bool cacheCleared = m_cacheController != nullptr && m_cacheController->clearCatalogCache();
    if (cacheCleared) {
        m_cachedCatalogPayload.clear();
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

    const bool cacheCleared = m_cacheController != nullptr && m_cacheController->clearDeepSkyCatalogCache();
    if (cacheCleared) {
        m_cachedDeepSkyCatalogPayload.clear();
    }
    m_statusText = SkyCatalogText::deepSkyCacheClearResult(cacheCleared);
    emit statusTextChanged();
    return cacheCleared;
}

bool SkyCatalogManager::restoreCatalogCache()
{
    if (m_cacheController == nullptr) {
        return false;
    }

    auto restoreResult = m_cacheController->restore(m_catalogPresetIndex, m_deepSkyCatalogPresetIndex);
    if (restoreResult.savedCatalogUnreadable) {
        m_cachedCatalogPayload.clear();
        m_statusText = restoreResult.statusText;
        emit statusTextChanged();
        return false;
    }
    if (!restoreResult.restored) {
        return false;
    }

    // Apply every restored piece into a merged runtime result and emit the
    // change signals once, so the controller does not rebuild the ephemeris
    // engine and search model for each restored catalog component.
    SkyCatalogRuntimeResult mergedResult;
    if (restoreResult.catalog != nullptr) {
        m_cachedCatalogPayload = restoreResult.catalogPayload;
        mergedResult |=
            m_runtime->applyCatalog(std::move(restoreResult.catalog), restoreResult.sourceLabel, runtimeBuildOptions());
    }

    if (restoreResult.deepSkyCatalog != nullptr) {
        m_cachedDeepSkyCatalogPayload = restoreResult.deepSkyCatalogPayload;
        mergedResult |= m_runtime->applyDeepSkyCatalog(
            std::move(restoreResult.deepSkyCatalog),
            restoreResult.deepSkySourceLabel,
            restoreResult.deepSkyObjectCount,
            runtimeBuildOptions()
        );
    }

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

    // First restore from a legacy (gzip-only) cache upgrades it with binary
    // payloads so subsequent startups skip the CSV re-parse.
    if (restoreResult.requiresBinaryUpgrade) {
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

    if (source.urls.isEmpty()) {
        if (policy == skygate::ephemeris::CatalogCompositionPolicy::Merge) {
            m_cachedCatalogPayload.clear();
            resetConstellationLineRefs();
        } else {
            m_cachedDeepSkyCatalogPayload.clear();
        }
        applyBundledSource(*operation, policy);
        return;
    }

    if (m_importWorkflow == nullptr || !m_importWorkflow->isAvailable()) {
        setStatusText(QStringLiteral("Catalog: Network unavailable"));
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
                handleCatalogImportStatus(statusText);
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
    const SourceOperation& operation, const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    auto bundledCatalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    if (bundledCatalog == nullptr) {
        setStatusText(QStringLiteral("Catalog: Failed to load"));
        return;
    }

    SkyCatalogSourceRecord record;
    record.instanceId = operation.instance.instanceId;
    record.title = operation.instance.title;
    record.version = operation.instance.version;
    record.policy = policy;
    record.enabled = true;
    record.catalog = std::move(bundledCatalog);
    record.foundObjectCount = 0;

    const SkyCatalogRuntimeResult result = m_runtime->applySource(std::move(record), runtimeBuildOptions());
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
        setDownloadingCatalog(false);
        setStatusText(result.errorText);
        return;
    }

    operation->payload = result.payload;
    const std::size_t foundObjectCount = result.foundObjectCount;
    const auto diagnostics = result.diagnostics;
    const QString sourceLabel = result.sourceLabel;
    if (policy == skygate::ephemeris::CatalogCompositionPolicy::Merge) {
        m_cachedCatalogPayload = result.payload;
    } else {
        m_cachedDeepSkyCatalogPayload = result.payload;
    }

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

void SkyCatalogManager::handleCatalogImportStatus(const QString& statusText)
{
    setStatusText(statusText);
    setCatalogProcessing(SkyCatalogText::isProcessingStatus(statusText));
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

    const auto request = m_runtime->cachePersistRequest(m_cachedCatalogPayload, m_cachedDeepSkyCatalogPayload);
    if (request.has_value()) {
        m_cacheController->persist(request.value());
    }
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
