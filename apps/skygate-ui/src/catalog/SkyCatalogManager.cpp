#include "SkyCatalogManager.hpp"

#include "catalog/SkyCatalogCacheController.hpp"
#include "catalog/SkyCatalogImportWorkflow.hpp"
#include "catalog/SkyCatalogPresets.hpp"
#include "catalog/SkyCatalogRuntime.hpp"
#include "catalog/SkyCatalogText.hpp"

#include <QNetworkAccessManager>

#include "skygate/ephemeris/CatalogFactory.hpp"

#include <optional>
#include <utility>

using namespace skygate::ui::internal;

SkyCatalogManager::SkyCatalogManager(
    SkySettingsStore* settingsStore,
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    QObject* parent
)
    : QObject(parent)
    , m_runtime(std::make_unique<SkyCatalogRuntime>(
          std::move(starCatalog),
          std::move(ephemerisEngine)
      ))
{
    m_networkAccessManager = new QNetworkAccessManager(this);
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

const skygate::ephemeris::IEphemerisEngine* SkyCatalogManager::ephemerisEngine() const noexcept
{
    return m_runtime->ephemerisEngine();
}

QStringList SkyCatalogManager::sourceLabels() const
{
    return m_runtime->sourceLabels();
}

std::span<const std::uint8_t> SkyCatalogManager::sourceIds() const noexcept
{
    return m_runtime->sourceIds();
}

std::span<const SkyCatalogManager::ConstellationLineRef>
SkyCatalogManager::constellationLineRefs() const noexcept
{
    return m_runtime->constellationLineRefs();
}

std::span<const SkyCatalogManager::ConstellationLabelRef>
SkyCatalogManager::constellationLabelRefs() const noexcept
{
    return m_runtime->constellationLabelRefs();
}

void SkyCatalogManager::setCatalogPresetIndex(const int catalogPresetIndex)
{
    m_catalogPresetIndex = SkyCatalogPresets::normalizeCatalogPresetIndex(catalogPresetIndex);
}

void SkyCatalogManager::setDeepSkyCatalogPresetIndex(const int deepSkyCatalogPresetIndex)
{
    m_deepSkyCatalogPresetIndex =
        SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(deepSkyCatalogPresetIndex);
}

void SkyCatalogManager::setCatalogUrlText(const QString& catalogUrlText)
{
    const QString normalizedCatalogUrlText = catalogUrlText.trimmed();
    m_catalogUrlText = normalizedCatalogUrlText.isEmpty()
        ? SkyCatalogPresets::defaultCatalogUrlText()
        : normalizedCatalogUrlText;
}

void SkyCatalogManager::setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText)
{
    const QString normalizedUrlText = deepSkyCatalogUrlText.trimmed();
    m_deepSkyCatalogUrlText = normalizedUrlText.isEmpty()
        ? SkyCatalogPresets::defaultDeepSkyCatalogUrlText()
        : normalizedUrlText;
}

void SkyCatalogManager::loadCatalogPreset(const QString& presetId)
{
    if (m_downloadingCatalog) {
        return;
    }

    const SkyCatalogPreset preset = SkyCatalogPresets::catalogPreset(presetId);
    if (!preset.known) {
        m_statusText = SkyCatalogText::unknownCatalogPreset(presetId);
        emit statusTextChanged();
        return;
    }

    setCatalogPresetIndex(preset.presetIndex);
    if (preset.bundled) {
        m_cachedCatalogPayload.clear();
        resetConstellationLineRefs();
        applyCatalog(skygate::ephemeris::createBundledStarCatalog(), preset.sourceLabel);
        return;
    }

    setCatalogUrlText(preset.defaultUrlText);
    downloadCatalogFromUrls(
        preset.catalogUrls,
        preset.sourceLabel,
        preset.constellationLineUrls
    );
}

void SkyCatalogManager::loadDeepSkyCatalogPreset(const QString& presetId)
{
    if (m_downloadingCatalog) {
        return;
    }

    const SkyDeepSkyCatalogPreset preset = SkyCatalogPresets::deepSkyCatalogPreset(presetId);
    if (!preset.known) {
        m_statusText = SkyCatalogText::unknownDeepSkyPreset(presetId);
        emit statusTextChanged();
        return;
    }

    setDeepSkyCatalogPresetIndex(preset.presetIndex);
    if (preset.bundled) {
        m_cachedDeepSkyCatalogPayload.clear();
        const SkyCatalogRuntimeResult result = m_runtime->clearDeepSkyCatalog(
            preset.sourceLabel,
            runtimeBuildOptions()
        );
        persistCatalogCache();
        applyRuntimeResult(result);
        return;
    }

    setDeepSkyCatalogUrlText(preset.defaultUrlText);
    downloadDeepSkyCatalogFromUrls(preset.catalogUrls, preset.sourceLabel);
}

void SkyCatalogManager::downloadCatalogFromUrl(const QString& urlText)
{
    setCatalogPresetIndex(2);
    setCatalogUrlText(urlText);
    downloadCatalogFromUrls(QStringList {urlText}, "Downloaded");
}

void SkyCatalogManager::downloadDeepSkyCatalogFromUrl(const QString& urlText)
{
    setDeepSkyCatalogPresetIndex(2);
    setDeepSkyCatalogUrlText(urlText);
    downloadDeepSkyCatalogFromUrls(QStringList {urlText}, "OpenNGC");
}

bool SkyCatalogManager::clearCatalogCache()
{
    if (m_downloadingCatalog || m_catalogProcessing) {
        m_statusText = SkyCatalogText::cacheClearBlocked();
        emit statusTextChanged();
        return false;
    }

    const bool cacheCleared =
        m_cacheController != nullptr && m_cacheController->clearCatalogCache();
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

    const bool cacheCleared =
        m_cacheController != nullptr && m_cacheController->clearDeepSkyCatalogCache();
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

    auto restoreResult = m_cacheController->restore(
        m_catalogPresetIndex,
        m_deepSkyCatalogPresetIndex
    );
    if (restoreResult.savedCatalogUnreadable) {
        m_cachedCatalogPayload.clear();
        m_statusText = restoreResult.statusText;
        emit statusTextChanged();
        return false;
    }
    if (!restoreResult.restored) {
        return false;
    }

    if (restoreResult.catalog != nullptr) {
        m_cachedCatalogPayload = restoreResult.catalogPayload;
        applyCatalog(
            std::move(restoreResult.catalog),
            restoreResult.sourceLabel,
            false
        );
    }

    if (restoreResult.deepSkyCatalog != nullptr) {
        m_cachedDeepSkyCatalogPayload = restoreResult.deepSkyCatalogPayload;
        applyDeepSkyCatalog(
            std::move(restoreResult.deepSkyCatalog),
            restoreResult.deepSkySourceLabel,
            restoreResult.deepSkyObjectCount,
            false
        );
    }

    if (!restoreResult.constellationLineRefs.empty()) {
        applyRuntimeResult(m_runtime->restoreConstellationRefs(
            std::move(restoreResult.constellationLineRefs),
            std::move(restoreResult.constellationLabelRefs),
            restoreResult.constellationCount
        ));
        return true;
    }

    if (restoreResult.resetConstellationLineRefs) {
        const SkyCatalogRuntimeResult result = m_runtime->resetConstellationLineRefs();
        if (result.catalogChanged) {
            emit catalogChanged();
        }
        return true;
    }

    return true;
}

void SkyCatalogManager::downloadCatalogFromUrls(
    const QStringList& urlTexts,
    const QString& sourceLabel,
    const QStringList& constellationLineUrlTexts
)
{
    if (m_downloadingCatalog) {
        return;
    }

    if (m_importWorkflow == nullptr || !m_importWorkflow->isAvailable()) {
        setStatusText(QStringLiteral("Catalog: Network unavailable"));
        return;
    }

    setCatalogProcessing(false);
    setDownloadingCatalog(true);

    m_importWorkflow->downloadCatalog(
        urlTexts,
        sourceLabel,
        this,
        [this](const QString& statusText) {
            handleCatalogImportStatus(statusText);
        },
        [this, constellationLineUrlTexts](SkyCatalogImportResult result) {
            handleCatalogImportFinished(std::move(result), constellationLineUrlTexts);
        }
    );
}

void SkyCatalogManager::downloadDeepSkyCatalogFromUrls(
    const QStringList& urlTexts,
    const QString& sourceLabel
)
{
    if (m_downloadingCatalog) {
        return;
    }

    if (m_importWorkflow == nullptr || !m_importWorkflow->isAvailable()) {
        setStatusText(QStringLiteral("Catalog: Network unavailable"));
        return;
    }

    setCatalogProcessing(false);
    setDownloadingCatalog(true);

    m_importWorkflow->downloadDeepSkyCatalog(
        urlTexts,
        sourceLabel,
        this,
        [this](const QString& statusText) {
            handleCatalogImportStatus(statusText);
        },
        [this](SkyDeepSkyCatalogImportResult result) {
            handleDeepSkyImportFinished(std::move(result));
        }
    );
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

void SkyCatalogManager::handleCatalogImportFinished(
    SkyCatalogImportResult result,
    const QStringList& constellationLineUrlTexts
)
{
    setCatalogProcessing(false);

    if (result.catalog == nullptr) {
        setDownloadingCatalog(false);
        setStatusText(result.errorText);
        return;
    }

    m_cachedCatalogPayload = std::move(result.payload);
    applyCatalog(std::move(result.catalog), result.sourceLabel);
    if (result.diagnostics.truncatedBodyCount > 0U) {
        setStatusText(SkyCatalogText::brightnessFilterSummary(
            m_statusText,
            result.diagnostics.selectedBodyCount,
            result.diagnostics.parsedBodyCount
        ));
    }
    setDownloadingCatalog(false);

    if (!constellationLineUrlTexts.isEmpty()) {
        resetConstellationLineRefs();
        downloadConstellationLinesAfterCatalog(constellationLineUrlTexts, m_statusText);
    }
}

void SkyCatalogManager::downloadConstellationLinesAfterCatalog(
    const QStringList& constellationLineUrlTexts,
    const QString& catalogSummaryText
)
{
    if (m_importWorkflow == nullptr || constellationLineUrlTexts.isEmpty()) {
        return;
    }

    m_importWorkflow->downloadConstellationLines(
        constellationLineUrlTexts,
        this,
        [this, catalogSummaryText](const QString& statusText) {
            handleConstellationLineImportStatus(catalogSummaryText, statusText);
        },
        [this, catalogSummaryText](SkyConstellationLineImportResult lineResult) {
            handleConstellationLineImportFinished(catalogSummaryText, std::move(lineResult));
        }
    );
}

void SkyCatalogManager::handleConstellationLineImportStatus(
    const QString& catalogSummaryText,
    const QString& statusText
)
{
    setStatusText(SkyCatalogText::constellationLineSummary(catalogSummaryText, statusText));
}

void SkyCatalogManager::handleConstellationLineImportFinished(
    const QString& catalogSummaryText,
    SkyConstellationLineImportResult lineResult
)
{
    if (lineResult.hasCustomLines()) {
        const SkyCatalogRuntimeResult result = m_runtime->restoreConstellationRefs(
            std::move(lineResult.lineRefs),
            std::move(lineResult.labelRefs),
            lineResult.constellationCount > 0U
                ? std::optional<std::size_t>(lineResult.constellationCount)
                : std::nullopt
        );
        if (result.datasetInfoChanged) {
            emit datasetInfoTextChanged();
        }
        setStatusText(SkyCatalogText::constellationLineSummary(
            catalogSummaryText,
            lineResult.statusSuffix
        ));
        persistCatalogCache();
        if (result.catalogChanged) {
            emit catalogChanged();
        }
        return;
    }
    setStatusText(SkyCatalogText::constellationLineSummary(
        catalogSummaryText,
        lineResult.statusSuffix
    ));
    persistCatalogCache();
}

void SkyCatalogManager::handleDeepSkyImportFinished(SkyDeepSkyCatalogImportResult result)
{
    setCatalogProcessing(false);
    setDownloadingCatalog(false);

    if (result.catalog == nullptr) {
        setStatusText(result.errorText);
        return;
    }

    m_cachedDeepSkyCatalogPayload = std::move(result.payload);
    applyDeepSkyCatalog(
        std::move(result.catalog),
        result.sourceLabel,
        result.foundObjectCount
    );
    if (result.foundObjectCount > 0U) {
        setStatusText(SkyCatalogText::deepSkyFoundSummary(
            m_statusText,
            result.sourceLabel,
            result.foundObjectCount
        ));
    }
}

SkyCatalogRuntimeBuildOptions SkyCatalogManager::runtimeBuildOptions() const
{
    return SkyCatalogRuntimeBuildOptions {
        .useBundledDeepSkyCatalog = m_deepSkyCatalogPresetIndex == 0
    };
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

    const auto request = m_runtime->cachePersistRequest(
        m_cachedCatalogPayload,
        m_cachedDeepSkyCatalogPayload
    );
    if (request.has_value()) {
        m_cacheController->persist(request.value());
    }
}

void SkyCatalogManager::applyCatalog(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
    const QString& sourceLabel,
    const bool persistCatalog
)
{
    const SkyCatalogRuntimeResult result = m_runtime->applyCatalog(
        std::move(catalog),
        sourceLabel,
        runtimeBuildOptions()
    );
    if (persistCatalog) {
        persistCatalogCache();
    }
    applyRuntimeResult(result);
}

void SkyCatalogManager::applyDeepSkyCatalog(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
    const QString& sourceLabel,
    const std::size_t foundObjectCount,
    const bool persistCatalog
)
{
    const SkyCatalogRuntimeResult result = m_runtime->applyDeepSkyCatalog(
        std::move(catalog),
        sourceLabel,
        foundObjectCount,
        runtimeBuildOptions()
    );
    if (persistCatalog) {
        persistCatalogCache();
    }
    applyRuntimeResult(result);
}
