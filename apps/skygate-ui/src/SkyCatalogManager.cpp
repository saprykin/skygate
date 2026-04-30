#include "SkyCatalogManager.hpp"

#include "catalog/SkyActiveCatalogBuilder.hpp"
#include "catalog/SkyCatalogCacheController.hpp"
#include "catalog/SkyCatalogImportWorkflow.hpp"
#include "catalog/SkyCatalogPresets.hpp"

#include <QLocale>
#include <QNetworkAccessManager>

#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <utility>

using namespace skygate::ui::internal;

SkyCatalogManager::SkyCatalogManager(
    SkySettingsStore* settingsStore,
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    QObject* parent
)
    : QObject(parent)
    , m_starCatalog(std::move(starCatalog))
    , m_ephemerisEngine(std::move(ephemerisEngine))
{
    m_networkAccessManager = new QNetworkAccessManager(this);
    m_cacheController = std::make_unique<SkyCatalogCacheController>(settingsStore);
    m_importWorkflow = std::make_unique<SkyCatalogImportWorkflow>(m_networkAccessManager);
    resetConstellationLineRefs();

    if (m_starCatalog == nullptr) {
        m_starCatalog = skygate::ephemeris::createBundledStarCatalog();
    }
    if (m_ephemerisEngine == nullptr && m_starCatalog != nullptr) {
        m_ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*m_starCatalog);
    }

    if (m_starCatalog != nullptr) {
        applyCatalog(std::move(m_starCatalog), "Bundled", false);
    } else {
        m_statusText = "Catalog: Unavailable";
    }

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
    const QLocale locale = QLocale::system();
    return QString("Objects: %1 | Constellations: %2").arg(
        locale.toString(static_cast<qulonglong>(m_bodyCount)),
        locale.toString(static_cast<qulonglong>(m_constellationCount))
    );
}

QString SkyCatalogManager::deepSkyCatalogInfoText() const
{
    const QLocale locale = QLocale::system();
    return QString("Objects: %1").arg(
        locale.toString(static_cast<qulonglong>(m_deepSkyCatalogFoundObjectCount))
    );
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
    return m_sourceLabel;
}

std::size_t SkyCatalogManager::bodyCount() const noexcept
{
    return m_bodyCount;
}

std::size_t SkyCatalogManager::constellationCount() const noexcept
{
    return m_constellationCount;
}

std::uint64_t SkyCatalogManager::catalogRevision() const noexcept
{
    return m_catalogRevision;
}

const skygate::ephemeris::IStarCatalog* SkyCatalogManager::starCatalog() const noexcept
{
    return m_starCatalog.get();
}

const skygate::ephemeris::IEphemerisEngine* SkyCatalogManager::ephemerisEngine() const noexcept
{
    return m_ephemerisEngine.get();
}

std::span<const SkyCatalogManager::ConstellationLineRef>
SkyCatalogManager::constellationLineRefs() const noexcept
{
    return std::span<const ConstellationLineRef>(m_constellationLineRefs);
}

std::span<const SkyCatalogManager::ConstellationLabelRef>
SkyCatalogManager::constellationLabelRefs() const noexcept
{
    return std::span<const ConstellationLabelRef>(m_constellationLabelRefs);
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
        m_statusText = QString("Catalog: Unknown preset '%1'").arg(presetId);
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
        m_statusText = QString("Catalog: Unknown deep-sky preset '%1'").arg(presetId);
        emit statusTextChanged();
        return;
    }

    setDeepSkyCatalogPresetIndex(preset.presetIndex);
    if (preset.bundled) {
        m_cachedDeepSkyCatalogPayload.clear();
        m_deepSkyCatalog.reset();
        m_deepSkyCatalogFoundObjectCount = 0;
        m_deepSkySourceLabel = preset.sourceLabel;
        rebuildActiveCatalog(true);
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
        m_statusText = "Catalog: Cannot clear cache while download is in progress";
        emit statusTextChanged();
        return false;
    }

    const bool cacheCleared =
        m_cacheController != nullptr && m_cacheController->clearCatalogCache();
    if (cacheCleared) {
        m_cachedCatalogPayload.clear();
    }
    m_statusText = cacheCleared
        ? "Catalog: Star catalog cache cleared"
        : "Catalog: Star catalog cache clear failed";
    emit statusTextChanged();
    return cacheCleared;
}

bool SkyCatalogManager::clearDeepSkyCatalogCache()
{
    if (m_downloadingCatalog || m_catalogProcessing) {
        m_statusText = "Catalog: Cannot clear cache while download is in progress";
        emit statusTextChanged();
        return false;
    }

    const bool cacheCleared =
        m_cacheController != nullptr && m_cacheController->clearDeepSkyCatalogCache();
    if (cacheCleared) {
        m_cachedDeepSkyCatalogPayload.clear();
    }
    m_statusText = cacheCleared
        ? "Catalog: Deep-sky catalog cache cleared"
        : "Catalog: Deep-sky catalog cache clear failed";
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
        m_deepSkyCatalogFoundObjectCount = restoreResult.deepSkyObjectCount;
        applyDeepSkyCatalog(
            std::move(restoreResult.deepSkyCatalog),
            restoreResult.deepSkySourceLabel,
            false
        );
    }

    if (!restoreResult.constellationLineRefs.empty()) {
        setConstellationLineRefs(std::move(restoreResult.constellationLineRefs));
        setConstellationLabelRefs(std::move(restoreResult.constellationLabelRefs));

        if (
            restoreResult.constellationCount.has_value()
            && restoreResult.constellationCount.value() != m_constellationCount
        ) {
            m_constellationCount = restoreResult.constellationCount.value();
            emit datasetInfoTextChanged();
        }
        emit catalogChanged();
        return true;
    }

    if (restoreResult.resetConstellationLineRefs) {
        resetConstellationLineRefs();
        emit catalogChanged();
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
        m_statusText = "Catalog: Network unavailable";
        emit statusTextChanged();
        return;
    }

    if (m_catalogProcessing) {
        m_catalogProcessing = false;
        emit catalogProcessingChanged();
    }

    m_downloadingCatalog = true;
    emit downloadingCatalogChanged();

    m_importWorkflow->downloadCatalog(
        urlTexts,
        sourceLabel,
        this,
        [this](const QString& statusText) {
            m_statusText = statusText;
            emit statusTextChanged();

            const bool isProcessing = statusText.startsWith(
                "Catalog: Processing",
                Qt::CaseInsensitive
            );
            if (m_catalogProcessing != isProcessing) {
                m_catalogProcessing = isProcessing;
                emit catalogProcessingChanged();
            }
        },
        [this, constellationLineUrlTexts](SkyCatalogImportResult result) {
            if (m_catalogProcessing) {
                m_catalogProcessing = false;
                emit catalogProcessingChanged();
            }

            if (result.catalog == nullptr) {
                m_downloadingCatalog = false;
                emit downloadingCatalogChanged();
                m_statusText = result.errorText;
                emit statusTextChanged();
                return;
            }

            m_cachedCatalogPayload = std::move(result.payload);
            applyCatalog(std::move(result.catalog), result.sourceLabel);
            if (result.diagnostics.truncatedBodyCount > 0U) {
                const QLocale locale = QLocale::system();
                m_statusText = QString("%1 | Brightness filter kept %2 of %3 objects").arg(
                    m_statusText,
                    locale.toString(
                        static_cast<qulonglong>(result.diagnostics.selectedBodyCount)
                    ),
                    locale.toString(
                        static_cast<qulonglong>(result.diagnostics.parsedBodyCount)
                    )
                );
                emit statusTextChanged();
            }
            m_downloadingCatalog = false;
            emit downloadingCatalogChanged();

            if (constellationLineUrlTexts.isEmpty()) {
                return;
            }

            resetConstellationLineRefs();
            const QString catalogSummaryText = m_statusText;
            m_importWorkflow->downloadConstellationLines(
                constellationLineUrlTexts,
                this,
                [this, catalogSummaryText](const QString& statusText) {
                    QString normalizedStatusText = statusText;
                    if (normalizedStatusText.startsWith("Catalog: ")) {
                        normalizedStatusText.remove(0, 9);
                    }
                    m_statusText = QString("%1 | Constellation lines: %2").arg(
                        catalogSummaryText,
                        normalizedStatusText
                    );
                    emit statusTextChanged();
                },
                [this, catalogSummaryText](SkyConstellationLineImportResult lineResult) {
                    if (lineResult.hasCustomLines()) {
                        setConstellationLineRefs(std::move(lineResult.lineRefs));
                        setConstellationLabelRefs(std::move(lineResult.labelRefs));
                        if (lineResult.constellationCount > 0U) {
                            m_constellationCount = lineResult.constellationCount;
                            emit datasetInfoTextChanged();
                        }
                    }
                    m_statusText = QString("%1 | Constellation lines: %2").arg(
                        catalogSummaryText,
                        lineResult.statusSuffix
                    );
                    persistCatalogCache();
                    emit statusTextChanged();
                    emit catalogChanged();
                }
            );
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
        m_statusText = "Catalog: Network unavailable";
        emit statusTextChanged();
        return;
    }

    if (m_catalogProcessing) {
        m_catalogProcessing = false;
        emit catalogProcessingChanged();
    }

    m_downloadingCatalog = true;
    emit downloadingCatalogChanged();

    m_importWorkflow->downloadDeepSkyCatalog(
        urlTexts,
        sourceLabel,
        this,
        [this](const QString& statusText) {
            m_statusText = statusText;
            emit statusTextChanged();

            const bool isProcessing = statusText.startsWith(
                "Catalog: Processing",
                Qt::CaseInsensitive
            );
            if (m_catalogProcessing != isProcessing) {
                m_catalogProcessing = isProcessing;
                emit catalogProcessingChanged();
            }
        },
        [this](SkyDeepSkyCatalogImportResult result) {
            if (m_catalogProcessing) {
                m_catalogProcessing = false;
                emit catalogProcessingChanged();
            }

            m_downloadingCatalog = false;
            emit downloadingCatalogChanged();

            if (result.catalog == nullptr) {
                m_statusText = result.errorText;
                emit statusTextChanged();
                return;
            }

            m_cachedDeepSkyCatalogPayload = std::move(result.payload);
            m_deepSkyCatalogFoundObjectCount = result.foundObjectCount;
            applyDeepSkyCatalog(std::move(result.catalog), result.sourceLabel);
            if (result.foundObjectCount > 0U) {
                const QLocale locale = QLocale::system();
                m_statusText = QString("%1 | %2 found %3 objects").arg(
                    m_statusText,
                    result.sourceLabel,
                    locale.toString(static_cast<qulonglong>(result.foundObjectCount))
                );
                emit statusTextChanged();
            }
        }
    );
}

void SkyCatalogManager::resetConstellationLineRefs()
{
    const skygate::ephemeris::BundledConstellationData bundledConstellationData;
    m_constellationLineRefs = bundledConstellationData.lineRefs();
    m_constellationLabelRefs = bundledConstellationData.labelRefs();
    m_constellationCount = m_constellationLabelRefs.size();
    ++m_catalogRevision;
}

void SkyCatalogManager::setConstellationLineRefs(
    std::vector<ConstellationLineRef> lineRefs
)
{
    if (lineRefs.empty()) {
        resetConstellationLineRefs();
        return;
    }
    m_constellationLineRefs = std::move(lineRefs);
    ++m_catalogRevision;
}

void SkyCatalogManager::setConstellationLabelRefs(
    std::vector<ConstellationLabelRef> labelRefs
)
{
    m_constellationLabelRefs = std::move(labelRefs);
    ++m_catalogRevision;
}

void SkyCatalogManager::persistCatalogCache() const
{
    if (
        m_cacheController == nullptr
        || m_starCatalog == nullptr
        || (m_cachedCatalogPayload.isEmpty() && m_cachedDeepSkyCatalogPayload.isEmpty())
    ) {
        return;
    }

    const auto bodies = m_starCatalog->bodies();
    if (bodies.empty()) {
        return;
    }

    SkyCatalogCachePersistRequest request;
    request.sourceLabel = m_sourceLabel;
    request.deepSkySourceLabel = m_deepSkySourceLabel;
    request.catalogPayload = m_cachedCatalogPayload;
    request.deepSkyCatalogPayload = m_cachedDeepSkyCatalogPayload;
    request.constellationLineRefs = m_constellationLineRefs;
    request.constellationLabelRefs = m_constellationLabelRefs;
    request.constellationCount = m_constellationCount;
    m_cacheController->persist(request);
}

void SkyCatalogManager::applyCatalog(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
    const QString& sourceLabel,
    const bool persistCatalog
)
{
    if (catalog == nullptr) {
        m_bodyCount = 0;
        m_constellationCount = 0;
        m_deepSkyObjectCount = 0;
        m_statusText = "Catalog: Failed to load";
        emit statusTextChanged();
        emit datasetInfoTextChanged();
        return;
    }

    m_sourceCatalog = std::move(catalog);
    m_sourceLabel = sourceLabel;
    rebuildActiveCatalog(persistCatalog);
}

void SkyCatalogManager::applyDeepSkyCatalog(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
    const QString& sourceLabel,
    const bool persistCatalog
)
{
    if (catalog == nullptr) {
        m_statusText = "Catalog: Failed to load deep-sky catalog";
        emit statusTextChanged();
        return;
    }

    m_deepSkyCatalog = std::move(catalog);
    m_deepSkySourceLabel = sourceLabel;
    rebuildActiveCatalog(persistCatalog);
}

void SkyCatalogManager::rebuildActiveCatalog(const bool persistCatalog)
{
    if (m_sourceCatalog == nullptr) {
        m_sourceCatalog = skygate::ephemeris::createBundledStarCatalog();
        m_sourceLabel = "Bundled";
    }

    if (m_sourceCatalog == nullptr) {
        m_bodyCount = 0;
        m_constellationCount = 0;
        m_deepSkyObjectCount = 0;
        m_statusText = "Catalog: Failed to load";
        emit statusTextChanged();
        emit datasetInfoTextChanged();
        return;
    }

    const SkyActiveCatalogBuildRequest request {
        .sourceCatalog = *m_sourceCatalog,
        .deepSkyCatalog = m_deepSkyCatalog.get(),
        .useBundledDeepSkyCatalog = m_deepSkyCatalogPresetIndex == 0,
        .currentConstellationCount = m_constellationCount,
        .knownDeepSkyObjectCount = m_deepSkyCatalogFoundObjectCount,
        .sourceLabel = m_sourceLabel,
        .deepSkySourceLabel = m_deepSkySourceLabel
    };
    auto buildResult = SkyActiveCatalogBuilder::build(request);
    if (!buildResult.isSuccess()) {
        m_bodyCount = 0;
        m_constellationCount = 0;
        m_deepSkyObjectCount = 0;
        m_statusText = buildResult.errorText;
        emit statusTextChanged();
        emit datasetInfoTextChanged();
        return;
    }

    m_starCatalog = std::move(buildResult.catalog);
    m_ephemerisEngine = std::move(buildResult.ephemerisEngine);
    ++m_catalogRevision;
    m_bodyCount = buildResult.bodyCount;
    m_constellationCount = buildResult.constellationCount;
    m_deepSkyObjectCount = buildResult.deepSkyObjectCount;
    m_deepSkyCatalogFoundObjectCount = buildResult.foundDeepSkyObjectCount;
    m_statusText = buildResult.statusText;
    if (persistCatalog) {
        persistCatalogCache();
    }
    emit statusTextChanged();
    emit datasetInfoTextChanged();
    emit deepSkyCatalogInfoTextChanged();
    emit catalogChanged();
}
