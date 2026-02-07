#include "SkyContextController.hpp"

#include "CatalogCoordinator.hpp"
#include "SkyContextControllerSupport.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

using namespace skygate::ui::internal;

void SkyContextController::loadCatalogPreset(const QString& presetId)
{
    if (m_downloadingCatalog) {
        return;
    }

    const QString normalizedPresetId = presetId.trimmed().toLower();
    if (normalizedPresetId == "bundled") {
        setCatalogPresetIndex(0);
        resetConstellationLineRefs();
        applyCatalog(skygate::ephemeris::createBundledStarCatalog(), "Bundled");
        return;
    }

    if (normalizedPresetId == "starter") {
        setCatalogPresetIndex(1);
        resetConstellationLineRefs();
        applyCatalog(
            skygate::ephemeris::createStarCatalogFromRows(kStarterCatalogRows),
            "Starter"
        );
        return;
    }

    if (normalizedPresetId == "constellations_major") {
        setCatalogPresetIndex(2);
        resetConstellationLineRefs();
        applyCatalog(
            skygate::ephemeris::createStarCatalogFromRows(kMajorConstellationsCatalogRows),
            "Major Constellations"
        );
        return;
    }

    if (normalizedPresetId == "hyg_v3") {
        setCatalogPresetIndex(3);
        setCatalogUrlText(QString::fromUtf8(kHygCatalogPrimaryUrl));
        downloadCatalogFromUrls(
            QStringList {
                QString::fromUtf8(kHygCatalogPrimaryUrl),
                QString::fromUtf8(kHygCatalogMirrorUrl),
                QString::fromUtf8(kHygCatalogMirror2Url)
            },
            "HYG v3",
            QStringList {
                QString::fromUtf8(kStellariumConstellationLinesPrimaryUrl),
                QString::fromUtf8(kStellariumConstellationLinesMirrorUrl),
                QString::fromUtf8(kStellariumConstellationLinesCdnUrl)
            }
        );
        return;
    }

    m_catalogStatusText = QString("Catalog: Unknown preset '%1'").arg(presetId);
    emit catalogStatusTextChanged();
}

void SkyContextController::downloadCatalogFromUrl(const QString& urlText)
{
    setCatalogUrlText(urlText);
    downloadCatalogFromUrls(QStringList {urlText}, "Downloaded");
}

void SkyContextController::downloadCatalogFromUrls(
    const QStringList& urlTexts,
    const QString& sourceLabel,
    const QStringList& constellationLineUrlTexts
)
{
    if (m_downloadingCatalog) {
        return;
    }

    if (m_catalogCoordinator == nullptr) {
        m_catalogStatusText = "Catalog: Network unavailable";
        emit catalogStatusTextChanged();
        return;
    }

    m_downloadingCatalog = true;
    emit downloadingCatalogChanged();

    m_catalogCoordinator->downloadCatalogFromUrls(
        urlTexts,
        this,
        [this](const QString& statusText) {
            m_catalogStatusText = statusText;
            emit catalogStatusTextChanged();
        },
        [this, sourceLabel, constellationLineUrlTexts](CatalogCoordinator::DownloadResult result) {
            if (result.catalog == nullptr) {
                m_downloadingCatalog = false;
                emit downloadingCatalogChanged();
                m_catalogStatusText = result.errorText;
                emit catalogStatusTextChanged();
                return;
            }

            applyCatalog(std::move(result.catalog), sourceLabel);
            m_downloadingCatalog = false;
            emit downloadingCatalogChanged();

            if (constellationLineUrlTexts.isEmpty()) {
                return;
            }

            resetConstellationLineRefs();
            const QString catalogSummaryText = m_catalogStatusText;
            m_catalogCoordinator->downloadRawDataFromUrls(
                constellationLineUrlTexts,
                this,
                [this, catalogSummaryText](const QString& statusText) {
                    QString normalizedStatusText = statusText;
                    if (normalizedStatusText.startsWith("Catalog: ")) {
                        normalizedStatusText.remove(0, 9);
                    }
                    m_catalogStatusText = QString("%1 | Constellation lines: %2").arg(
                        catalogSummaryText,
                        normalizedStatusText
                    );
                    emit catalogStatusTextChanged();
                },
                [this, catalogSummaryText](CatalogCoordinator::RawDownloadResult lineResult) {
                    if (lineResult.payload.isEmpty()) {
                        const QString reason = lineResult.errorText.isEmpty()
                            ? QString("unavailable")
                            : lineResult.errorText;
                        m_catalogStatusText = QString("%1 | Constellation lines: fallback default (%2)").arg(
                            catalogSummaryText,
                            reason
                        );
                        emit catalogStatusTextChanged();
                        if (m_starCatalog != nullptr) {
                            persistCatalogCache(m_starCatalog->bodies(), m_catalogSourceLabel);
                        }
                        emit skyContextChanged();
                        return;
                    }

                    const std::string_view rows(
                        lineResult.payload.constData(),
                        static_cast<std::size_t>(lineResult.payload.size())
                    );
                    auto parsedLineRefs = parseStellariumConstellationLineRefs(rows);
                    if (parsedLineRefs.empty()) {
                        parsedLineRefs = parseStellariumIndexJsonConstellationLineRefs(lineResult.payload);
                    }
                    if (parsedLineRefs.empty()) {
                        QString payloadPreview = QString::fromUtf8(lineResult.payload.left(120)).simplified();
                        if (payloadPreview.isEmpty()) {
                            payloadPreview = "<empty>";
                        }
                        m_catalogStatusText = QString(
                            "%1 | Constellation lines: fallback default (parse failed: %2)"
                        ).arg(catalogSummaryText, payloadPreview);
                        emit catalogStatusTextChanged();
                        if (m_starCatalog != nullptr) {
                            persistCatalogCache(m_starCatalog->bodies(), m_catalogSourceLabel);
                        }
                        emit skyContextChanged();
                        return;
                    }

                    setConstellationLineRefs(std::move(parsedLineRefs));
                    if (m_starCatalog != nullptr) {
                        persistCatalogCache(m_starCatalog->bodies(), m_catalogSourceLabel);
                    }
                    m_catalogStatusText =
                        QString("%1 | Constellation lines: %2 segments").arg(
                            catalogSummaryText,
                            QString::number(static_cast<qulonglong>(m_constellationLineRefs.size()))
                        );
                    emit catalogStatusTextChanged();
                    emit skyContextChanged();
                }
            );
        }
    );
}

void SkyContextController::resetConstellationLineRefs()
{
    m_constellationLineRefs = defaultConstellationLineRefs();
}

void SkyContextController::setConstellationLineRefs(
    std::vector<std::pair<std::string, std::string>> lineRefs
)
{
    if (lineRefs.empty()) {
        resetConstellationLineRefs();
        return;
    }
    m_constellationLineRefs = std::move(lineRefs);
}

void SkyContextController::applyCatalog(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
    const QString& sourceLabel,
    const bool persistCatalog
)
{
    catalog = CatalogCoordinator::ensureCoreSolarSystemBodies(std::move(catalog));
    if (catalog == nullptr) {
        m_catalogStatusText = "Catalog: Failed to load";
        emit catalogStatusTextChanged();
        return;
    }

    const auto bodies = catalog->bodies();
    std::size_t constellationCount = 0;
    for (const auto& body : bodies) {
        if (body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
            ++constellationCount;
        }
    }

    m_starCatalog = std::move(catalog);
    m_ephemerisEngine = skygate::ephemeris::createEphemerisEngineStub(m_starCatalog.get());
    m_catalogSourceLabel = sourceLabel;
    m_catalogStatusText = QString("Catalog: %1 (%2 objects, %3 constellations)").arg(
        sourceLabel,
        QString::number(static_cast<qulonglong>(bodies.size())),
        QString::number(static_cast<qulonglong>(constellationCount))
    );
    if (persistCatalog) {
        persistCatalogCache(bodies, sourceLabel);
    }
    emit catalogStatusTextChanged();
    emit skyContextChanged();
}
