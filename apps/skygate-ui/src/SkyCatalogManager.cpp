#include "SkyCatalogManager.hpp"

#include "CatalogCoordinator.hpp"
#include "SkyContextControllerSupport.hpp"

#include <QLocale>
#include <QNetworkAccessManager>

#include "skygate/ephemeris/CatalogPayloadParser.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"
#include "skygate/ephemeris/StellariumConstellationParser.hpp"

#include <cstddef>
#include <string_view>
#include <utility>

using namespace skygate::ui::internal;

namespace {

QString stripSavedSuffixes(const QString& sourceLabel)
{
    QString normalizedSourceLabel = sourceLabel.trimmed();
    const QString spacedSavedSuffix = QStringLiteral(" (saved)");
    const QString compactSavedSuffix = QStringLiteral("(saved)");
    while (
        normalizedSourceLabel.endsWith(spacedSavedSuffix, Qt::CaseInsensitive)
        || normalizedSourceLabel.endsWith(compactSavedSuffix, Qt::CaseInsensitive)
    ) {
        if (normalizedSourceLabel.endsWith(spacedSavedSuffix, Qt::CaseInsensitive)) {
            normalizedSourceLabel.chop(spacedSavedSuffix.size());
        } else {
            normalizedSourceLabel.chop(compactSavedSuffix.size());
        }
        normalizedSourceLabel = normalizedSourceLabel.trimmed();
    }
    return normalizedSourceLabel;
}

}  // namespace

SkyCatalogManager::SkyCatalogManager(
    SkySettingsStore* settingsStore,
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    QObject* parent
)
    : QObject(parent)
    , m_settingsStore(settingsStore)
    , m_starCatalog(std::move(starCatalog))
    , m_ephemerisEngine(std::move(ephemerisEngine))
{
    m_networkAccessManager = new QNetworkAccessManager(this);
    m_catalogCoordinator = std::make_unique<CatalogCoordinator>(m_networkAccessManager);
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

    m_catalogUrlText = QString::fromUtf8(SkyContextControllerConstants::kHygCatalogPrimaryUrl);
}

SkyCatalogManager::~SkyCatalogManager() = default;

QString SkyCatalogManager::statusText() const
{
    return m_statusText;
}

QString SkyCatalogManager::datasetInfoText() const
{
    const QLocale locale = QLocale::system();
    return QString("Objects: %1 | Deep sky: %2 | Constellations: %3").arg(
        locale.toString(static_cast<qulonglong>(m_bodyCount)),
        locale.toString(static_cast<qulonglong>(m_deepSkyObjectCount)),
        locale.toString(static_cast<qulonglong>(m_constellationCount))
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
    if (catalogPresetIndex <= 0) {
        m_catalogPresetIndex = 0;
        return;
    }
    if (catalogPresetIndex == 1) {
        m_catalogPresetIndex = 1;
        return;
    }
    if (catalogPresetIndex == 2) {
        m_catalogPresetIndex = 2;
        return;
    }

    if (catalogPresetIndex == 3) {
        m_catalogPresetIndex = 1;
        return;
    }
    m_catalogPresetIndex = 2;
}

void SkyCatalogManager::setCatalogUrlText(const QString& catalogUrlText)
{
    const QString normalizedCatalogUrlText = catalogUrlText.trimmed();
    m_catalogUrlText = normalizedCatalogUrlText.isEmpty()
        ? QString::fromUtf8(SkyContextControllerConstants::kHygCatalogPrimaryUrl)
        : normalizedCatalogUrlText;
}

void SkyCatalogManager::loadCatalogPreset(const QString& presetId)
{
    if (m_downloadingCatalog) {
        return;
    }

    const QString normalizedPresetId = presetId.trimmed().toLower();
    if (normalizedPresetId == "bundled") {
        setCatalogPresetIndex(0);
        m_cachedCatalogPayload.clear();
        resetConstellationLineRefs();
        applyCatalog(skygate::ephemeris::createBundledStarCatalog(), "Bundled");
        return;
    }

    if (normalizedPresetId == "hyg_v42" || normalizedPresetId == "hyg_v3") {
        setCatalogPresetIndex(1);
        setCatalogUrlText(QString::fromUtf8(SkyContextControllerConstants::kHygCatalogPrimaryUrl));
        downloadCatalogFromUrls(
            QStringList {
                QString::fromUtf8(SkyContextControllerConstants::kHygCatalogPrimaryUrl)
            },
            "HYG v4.2",
            QStringList {
                QString::fromUtf8(
                    SkyContextControllerConstants::kStellariumConstellationLinesPrimaryUrl
                ),
                QString::fromUtf8(
                    SkyContextControllerConstants::kStellariumConstellationLinesMirrorUrl
                ),
                QString::fromUtf8(
                    SkyContextControllerConstants::kStellariumConstellationLinesCdnUrl
                )
            }
        );
        return;
    }

    m_statusText = QString("Catalog: Unknown preset '%1'").arg(presetId);
    emit statusTextChanged();
}

void SkyCatalogManager::downloadCatalogFromUrl(const QString& urlText)
{
    setCatalogPresetIndex(2);
    setCatalogUrlText(urlText);
    downloadCatalogFromUrls(QStringList {urlText}, "Downloaded");
}

bool SkyCatalogManager::clearCatalogCache()
{
    if (m_downloadingCatalog || m_catalogProcessing) {
        m_statusText = "Catalog: Cannot clear cache while download is in progress";
        emit statusTextChanged();
        return false;
    }

    const bool cacheCleared = m_settingsStore != nullptr && m_settingsStore->clearCatalogCache();
    if (cacheCleared) {
        m_cachedCatalogPayload.clear();
    }
    m_statusText = cacheCleared
        ? "Catalog: Cache cleared"
        : "Catalog: Cache clear failed";
    emit statusTextChanged();
    return cacheCleared;
}

bool SkyCatalogManager::restoreCatalogCache()
{
    if (m_settingsStore == nullptr) {
        return false;
    }
    if (m_catalogPresetIndex == 0) {
        return false;
    }

    const auto cacheSnapshot = m_settingsStore->loadCatalogCache();
    if (!cacheSnapshot.has_value()) {
        return false;
    }

    const std::string_view payloadView(
        cacheSnapshot->catalogPayload.constData(),
        static_cast<std::size_t>(cacheSnapshot->catalogPayload.size())
    );
    const skygate::ephemeris::CatalogPayloadParser parser;
    auto restoredCatalogResult = parser.parseResult(payloadView);
    if (!restoredCatalogResult.isSuccess() || restoredCatalogResult.catalog == nullptr) {
        m_cachedCatalogPayload.clear();
        m_statusText = "Catalog: Saved cache unreadable, using bundled";
        emit statusTextChanged();
        return false;
    }

    m_cachedCatalogPayload = cacheSnapshot->catalogPayload;
    QString normalizedSourceLabel = stripSavedSuffixes(cacheSnapshot->sourceLabel);
    if (normalizedSourceLabel.isEmpty()) {
        normalizedSourceLabel = "Saved";
    }
    applyCatalog(
        std::move(restoredCatalogResult.catalog),
        QString("%1 (saved)").arg(normalizedSourceLabel),
        false
    );

    if (
        cacheSnapshot->constellationLineSchemaVersion
            >= SkyContextControllerConstants::kConstellationLineCacheSchemaVersion
        && !cacheSnapshot->constellationLineRows.isEmpty()
    ) {
        auto parsedLineRefs = SkyContextCatalogCodec::parseConstellationLineRows(
            std::string_view(
                cacheSnapshot->constellationLineRows.constData(),
                static_cast<std::size_t>(cacheSnapshot->constellationLineRows.size())
            )
        );
        if (!parsedLineRefs.empty()) {
            setConstellationLineRefs(std::move(parsedLineRefs));

            if (!cacheSnapshot->constellationLabelRows.isEmpty()) {
                auto parsedLabelRefs = SkyContextCatalogCodec::parseConstellationLabelRows(
                    std::string_view(
                        cacheSnapshot->constellationLabelRows.constData(),
                        static_cast<std::size_t>(cacheSnapshot->constellationLabelRows.size())
                    )
                );
                setConstellationLabelRefs(std::move(parsedLabelRefs));
            } else {
                setConstellationLabelRefs({});
            }

            const std::size_t restoredConstellationCount = cacheSnapshot->constellationCount;
            if (restoredConstellationCount != m_constellationCount) {
                m_constellationCount = restoredConstellationCount;
                emit datasetInfoTextChanged();
            }
            emit catalogChanged();
            return true;
        }
    } else if (cacheSnapshot->constellationLineSchemaVersion > 0) {
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

    if (m_catalogCoordinator == nullptr) {
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

    m_catalogCoordinator->downloadCatalogFromUrls(
        urlTexts,
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
        [this, sourceLabel, constellationLineUrlTexts](CatalogCoordinator::DownloadResult result) {
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
            applyCatalog(std::move(result.catalog), sourceLabel);
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
            m_catalogCoordinator->downloadRawDataFromUrls(
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
                [this, catalogSummaryText](CatalogCoordinator::RawDownloadResult lineResult) {
                    if (lineResult.payload.isEmpty()) {
                        const QString reason = lineResult.errorText.isEmpty()
                            ? QString("unavailable")
                            : lineResult.errorText;
                        m_statusText =
                            QString("%1 | Constellation lines: fallback default (%2)").arg(
                                catalogSummaryText,
                                reason
                            );
                        emit statusTextChanged();
                        persistCatalogCache();
                        emit catalogChanged();
                        return;
                    }

                    const std::string_view payloadView(
                        lineResult.payload.constData(),
                        static_cast<std::size_t>(lineResult.payload.size())
                    );
                    const skygate::ephemeris::StellariumConstellationParser parser;
                    auto parsedData = parser.parse(payloadView);
                    if (parsedData.lineRefs.empty()) {
                        QString payloadPreview = QString::fromUtf8(
                            lineResult.payload.left(120)
                        ).simplified();
                        if (payloadPreview.isEmpty()) {
                            payloadPreview = "<empty>";
                        }
                        m_statusText = QString(
                            "%1 | Constellation lines: fallback default (parse failed: %2)"
                        ).arg(catalogSummaryText, payloadPreview);
                        emit statusTextChanged();
                        persistCatalogCache();
                        emit catalogChanged();
                        return;
                    }

                    setConstellationLineRefs(std::move(parsedData.lineRefs));
                    setConstellationLabelRefs(std::move(parsedData.labelRefs));
                    if (parsedData.constellationCount > 0U) {
                        m_constellationCount = parsedData.constellationCount;
                        emit datasetInfoTextChanged();
                    }
                    persistCatalogCache();
                    m_statusText = QString("%1 | Constellation lines: %2 segments").arg(
                        catalogSummaryText,
                        QString::number(
                            static_cast<qulonglong>(m_constellationLineRefs.size())
                        )
                    );
                    emit statusTextChanged();
                    emit catalogChanged();
                }
            );
        }
    );
}

void SkyCatalogManager::resetConstellationLineRefs()
{
    const skygate::ephemeris::BundledConstellationData bundledConstellationData;
    m_constellationLineRefs = bundledConstellationData.lineRefs();
    m_constellationLabelRefs = bundledConstellationData.labelRefs();
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
        m_settingsStore == nullptr
        || m_starCatalog == nullptr
        || m_cachedCatalogPayload.isEmpty()
    ) {
        return;
    }

    const auto bodies = m_starCatalog->bodies();
    if (bodies.empty()) {
        return;
    }

    SkySettingsStore::CatalogCacheSnapshot snapshot;
    snapshot.sourceLabel = m_sourceLabel;
    snapshot.catalogPayload = m_cachedCatalogPayload;
    snapshot.constellationLineRows = SkyContextCatalogCodec::serializeConstellationLineRows(
        m_constellationLineRefs
    );
    snapshot.constellationLabelRows = SkyContextCatalogCodec::serializeConstellationLabelRows(
        m_constellationLabelRefs
    );
    snapshot.constellationLineSchemaVersion =
        SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
    snapshot.constellationCount = m_constellationCount;
    (void)m_settingsStore->saveCatalogCache(snapshot);
}

void SkyCatalogManager::applyCatalog(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
    const QString& sourceLabel,
    const bool persistCatalog
)
{
    if (catalog != nullptr) {
        catalog = skygate::ephemeris::createCatalogWithCoreSolarSystemBodies(*catalog);
    }
    if (catalog == nullptr) {
        m_bodyCount = 0;
        m_constellationCount = 0;
        m_deepSkyObjectCount = 0;
        m_statusText = "Catalog: Failed to load";
        emit statusTextChanged();
        emit datasetInfoTextChanged();
        return;
    }

    const auto bodies = catalog->bodies();
    std::size_t constellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    for (const auto& body : bodies) {
        if (body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
            ++constellationCount;
        }
        if (body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject) {
            ++deepSkyObjectCount;
        }
    }

    m_starCatalog = std::move(catalog);
    m_ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*m_starCatalog);
    ++m_catalogRevision;
    m_bodyCount = bodies.size();
    m_constellationCount = constellationCount;
    m_deepSkyObjectCount = deepSkyObjectCount;
    m_sourceLabel = sourceLabel;
    m_statusText = QString("Catalog: %1 (%2 objects, %3 deep sky, %4 constellations)").arg(
        sourceLabel,
        QString::number(static_cast<qulonglong>(bodies.size())),
        QString::number(static_cast<qulonglong>(deepSkyObjectCount)),
        QString::number(static_cast<qulonglong>(constellationCount))
    );
    if (persistCatalog) {
        persistCatalogCache();
    }
    emit statusTextChanged();
    emit datasetInfoTextChanged();
    emit catalogChanged();
}
