#include "SkyCatalogPresets.hpp"
#include "StringUtilities.hpp"

#include <QString>

#include <string>
#include <string_view>
#include <utility>

using skygate::ephemeris::StringUtilities;

namespace skygate::ui::internal {
namespace {

constexpr const char* kHygCatalogPrimaryUrl = "https://www.astronexus.com/downloads/catalogs/hygdata_v42.csv.gz";
constexpr const char* kOpenNgcCatalogPrimaryUrl =
    "https://raw.githubusercontent.com/mattiaverga/OpenNGC/refs/tags/v20260307/database_files/NGC.csv";
constexpr const char* kOpenNgcCatalogMirrorUrl =
    "https://raw.githubusercontent.com/mattiaverga/OpenNGC/master/database_files/NGC.csv";
constexpr const char* kStellariumConstellationLinesPrimaryUrl =
    "https://raw.githubusercontent.com/Stellarium/stellarium-skycultures/master/western/index.json";
constexpr const char* kStellariumConstellationLinesMirrorUrl =
    "https://raw.githubusercontent.com/Stellarium/stellarium-skycultures/main/western/index.json";
constexpr const char* kStellariumConstellationLinesCdnUrl =
    "https://cdn.jsdelivr.net/gh/Stellarium/stellarium-skycultures@master/western/index.json";

QVector<SkyCatalogSourceDescriptor> buildStarSourceDescriptors()
{
    QVector<SkyCatalogSourceDescriptor> descriptors;
    descriptors.reserve(2);

    SkyCatalogSourceDescriptor bundled;
    bundled.sourceId = QStringLiteral("bundled");
    bundled.title = QStringLiteral("Bundled");
    bundled.schemaHint = skygate::ephemeris::CatalogSourceType::Bundled;
    bundled.attribution = QStringLiteral("Built-in bright star catalog");
    bundled.category = SkyCatalogPresets::starCategory();
    bundled.legacyPresetIndex = 0;
    bundled.bundled = true;
    descriptors.push_back(std::move(bundled));

    SkyCatalogSourceDescriptor hyg;
    hyg.sourceId = QStringLiteral("hyg_v42");
    hyg.title = QStringLiteral("HYG v4.2");
    hyg.version = QStringLiteral("v4.2");
    hyg.urls = QStringList{QString::fromUtf8(kHygCatalogPrimaryUrl)};
    hyg.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    hyg.relatedDatasetUrls = QStringList{
        QString::fromUtf8(kStellariumConstellationLinesPrimaryUrl),
        QString::fromUtf8(kStellariumConstellationLinesMirrorUrl),
        QString::fromUtf8(kStellariumConstellationLinesCdnUrl)
    };
    hyg.attribution = QStringLiteral("HYG Database v4.2 (astronexus.com)");
    hyg.category = SkyCatalogPresets::starCategory();
    hyg.legacyPresetIndex = 1;
    descriptors.push_back(std::move(hyg));

    return descriptors;
}

QVector<SkyCatalogSourceDescriptor> buildDeepSkySourceDescriptors()
{
    QVector<SkyCatalogSourceDescriptor> descriptors;
    descriptors.reserve(2);

    SkyCatalogSourceDescriptor bundledMessier;
    bundledMessier.sourceId = QStringLiteral("bundled_messier");
    bundledMessier.title = QStringLiteral("Bundled Messier");
    bundledMessier.schemaHint = skygate::ephemeris::CatalogSourceType::Bundled;
    bundledMessier.attribution = QStringLiteral("Built-in Messier catalog");
    bundledMessier.category = SkyCatalogPresets::deepSkyCategory();
    bundledMessier.legacyPresetIndex = 0;
    bundledMessier.bundled = true;
    descriptors.push_back(std::move(bundledMessier));

    SkyCatalogSourceDescriptor openNgc;
    openNgc.sourceId = QStringLiteral("open_ngc");
    openNgc.title = QStringLiteral("OpenNGC");
    openNgc.version = QStringLiteral("v20260307");
    openNgc.urls =
        QStringList{QString::fromUtf8(kOpenNgcCatalogPrimaryUrl), QString::fromUtf8(kOpenNgcCatalogMirrorUrl)};
    openNgc.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    openNgc.attribution = QStringLiteral("OpenNGC (github.com/mattiaverga/OpenNGC)");
    openNgc.category = SkyCatalogPresets::deepSkyCategory();
    openNgc.legacyPresetIndex = 1;
    descriptors.push_back(std::move(openNgc));

    return descriptors;
}

const QVector<SkyCatalogSourceDescriptor>& starDescriptors()
{
    static const QVector<SkyCatalogSourceDescriptor> descriptors = buildStarSourceDescriptors();
    return descriptors;
}

const QVector<SkyCatalogSourceDescriptor>& deepSkyDescriptors()
{
    static const QVector<SkyCatalogSourceDescriptor> descriptors = buildDeepSkySourceDescriptors();
    return descriptors;
}

// The retired preset IDs are aliases of the IDs the descriptors carry, so a
// stored preset selection keeps resolving after its preset was replaced.
std::string canonicalStarPresetId(std::string normalizedId)
{
    if (normalizedId == "hyg_v3") {
        return "hyg_v42";
    }
    return normalizedId;
}

std::string canonicalDeepSkyPresetId(std::string normalizedId)
{
    if (normalizedId == "bundled") {
        return "bundled_messier";
    }
    return normalizedId;
}

}  // namespace

std::string SkyCatalogPresets::normalizedPresetId(const std::string_view presetId)
{
    return StringUtilities::normalizedLookupKey(presetId);
}

bool SkyCatalogPresets::presetIdMatches(const std::string_view registeredId, const std::string_view requestedId)
{
    return StringUtilities::equalsIgnoreAsciiCase(registeredId, requestedId);
}

int SkyCatalogPresets::normalizeCatalogPresetIndex(const int presetIndex) noexcept
{
    if (presetIndex <= 0) {
        return 0;
    }
    if (presetIndex == 1 || presetIndex == 3) {
        return 1;
    }
    return kCustomLegacyPresetIndex;
}

int SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(const int presetIndex) noexcept
{
    if (presetIndex <= 0) {
        return 0;
    }
    if (presetIndex == 1) {
        return 1;
    }
    return kCustomLegacyPresetIndex;
}

QString SkyCatalogPresets::defaultCatalogUrlText()
{
    const std::optional<SkyCatalogSourceDescriptor> descriptor = starSourceDescriptor(QStringLiteral("hyg_v42"));
    return descriptor.has_value() ? descriptor->defaultUrl() : QString();
}

QString SkyCatalogPresets::defaultDeepSkyCatalogUrlText()
{
    const std::optional<SkyCatalogSourceDescriptor> descriptor = deepSkySourceDescriptor(QStringLiteral("open_ngc"));
    return descriptor.has_value() ? descriptor->defaultUrl() : QString();
}

QString SkyCatalogPresets::starCategory()
{
    return QStringLiteral("Star");
}

QString SkyCatalogPresets::deepSkyCategory()
{
    return QStringLiteral("Deep sky");
}

skygate::ephemeris::CatalogCompositionPolicy SkyCatalogPresets::policyForCategory(const QString& category)
{
    return category == deepSkyCategory() ? skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly
                                         : skygate::ephemeris::CatalogCompositionPolicy::Merge;
}

QString SkyCatalogPresets::categoryForPolicy(const skygate::ephemeris::CatalogCompositionPolicy policy)
{
    return policy == skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly ? deepSkyCategory() : starCategory();
}

std::optional<SkyCatalogSourceDescriptor> SkyCatalogPresets::starSourceDescriptor(const QString& presetId)
{
    const std::string canonicalId = canonicalStarPresetId(normalizedPresetId(presetId.toStdString()));
    for (const SkyCatalogSourceDescriptor& descriptor : starDescriptors()) {
        if (presetIdMatches(descriptor.sourceId.toStdString(), canonicalId)) {
            return descriptor;
        }
    }
    return std::nullopt;
}

std::optional<SkyCatalogSourceDescriptor> SkyCatalogPresets::deepSkySourceDescriptor(const QString& presetId)
{
    const std::string canonicalId = canonicalDeepSkyPresetId(normalizedPresetId(presetId.toStdString()));
    for (const SkyCatalogSourceDescriptor& descriptor : deepSkyDescriptors()) {
        if (presetIdMatches(descriptor.sourceId.toStdString(), canonicalId)) {
            return descriptor;
        }
    }
    return std::nullopt;
}

QVector<SkyCatalogSourceDescriptor> SkyCatalogPresets::starSourceDescriptors()
{
    return starDescriptors();
}

QVector<SkyCatalogSourceDescriptor> SkyCatalogPresets::deepSkySourceDescriptors()
{
    return deepSkyDescriptors();
}

}  // namespace skygate::ui::internal
