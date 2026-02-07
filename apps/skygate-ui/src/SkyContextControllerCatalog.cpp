#include "SkyContextController.hpp"

#include "CatalogCoordinator.hpp"
#include "SkyContextControllerSupport.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <charconv>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

using namespace skygate::ui::internal;

namespace {
std::optional<int> parsePositiveInteger(std::string_view value)
{
    if (value.empty()) {
        return std::nullopt;
    }

    int parsedValue = 0;
    const auto [parseEnd, errorCode] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsedValue
    );
    if (errorCode != std::errc() || parseEnd != value.data() + value.size() || parsedValue <= 0) {
        return std::nullopt;
    }

    return parsedValue;
}

std::size_t inferConstellationCountFromRows(const std::string_view rows)
{
    std::unordered_set<std::string> constellationIds;

    std::size_t cursor = 0;
    while (cursor < rows.size()) {
        const std::size_t newline = rows.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? rows.size() : newline;
        std::string_view line = rows.substr(cursor, lineEnd - cursor);

        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
            line.remove_prefix(1);
        }
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
            line.remove_suffix(1);
        }

        if (!line.empty() && line.front() != '#') {
            std::size_t firstSplit = 0;
            while (firstSplit < line.size() && !std::isspace(static_cast<unsigned char>(line[firstSplit]))) {
                ++firstSplit;
            }
            std::string_view constellationId = line.substr(0, firstSplit);

            while (
                firstSplit < line.size()
                && std::isspace(static_cast<unsigned char>(line[firstSplit]))
            ) {
                ++firstSplit;
            }
            std::size_t secondSplit = firstSplit;
            while (
                secondSplit < line.size()
                && !std::isspace(static_cast<unsigned char>(line[secondSplit]))
            ) {
                ++secondSplit;
            }
            const std::string_view segmentCountText = line.substr(firstSplit, secondSplit - firstSplit);

            if (
                !constellationId.empty()
                && parsePositiveInteger(segmentCountText).has_value()
            ) {
                constellationIds.insert(std::string(constellationId));
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1;
    }

    return constellationIds.size();
}

std::size_t inferConstellationCountFromPayload(const QByteArray& payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        const QJsonValue constellationsValue = document.object().value("constellations");
        if (constellationsValue.isArray()) {
            return static_cast<std::size_t>(constellationsValue.toArray().size());
        }
    }

    const std::string_view rows(payload.constData(), static_cast<std::size_t>(payload.size()));
    return inferConstellationCountFromRows(rows);
}
} // namespace

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

    if (normalizedPresetId == "hyg_v42" || normalizedPresetId == "hyg_v3") {
        setCatalogPresetIndex(1);
        setCatalogUrlText(QString::fromUtf8(kHygCatalogPrimaryUrl));
        downloadCatalogFromUrls(
            QStringList {
                QString::fromUtf8(kHygCatalogPrimaryUrl)
            },
            "HYG v4.2",
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
            m_catalogStatusText = statusText;
            emit catalogStatusTextChanged();

            const bool isProcessing = statusText.startsWith("Catalog: Processing", Qt::CaseInsensitive);
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
                    const std::size_t inferredConstellationCount =
                        inferConstellationCountFromPayload(lineResult.payload);
                    if (inferredConstellationCount > 0) {
                        m_catalogConstellationCount = inferredConstellationCount;
                        emit catalogDatasetInfoTextChanged();
                    }
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
        m_catalogBodyCount = 0;
        m_catalogConstellationCount = 0;
        m_catalogStatusText = "Catalog: Failed to load";
        emit catalogStatusTextChanged();
        emit catalogDatasetInfoTextChanged();
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
    m_catalogBodyCount = bodies.size();
    m_catalogConstellationCount = constellationCount;
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
    emit catalogDatasetInfoTextChanged();
    emit skyContextChanged();
}
