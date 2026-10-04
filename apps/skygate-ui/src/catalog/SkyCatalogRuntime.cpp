#include "SkyCatalogRuntime.hpp"

#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/constellation/ConstellationReferenceResolver.hpp"

#include <QHash>
#include <QLocale>

#include <algorithm>
#include <utility>

namespace skygate::ui::internal {
namespace {

constexpr const char* kPrimarySourceId = "primary";
constexpr const char* kDeepSkySourceId = "deep-sky";
constexpr const char* kBuiltInSourceId = "built-in-ephemeris";
constexpr const char* kBundledDeepSkySourceId = "bundled-deep-sky";

QString normalizedTitle(const QString& title, const QString& fallback)
{
    const QString normalized = title.trimmed();
    return normalized.isEmpty() ? fallback : normalized;
}

}  // namespace

SkyCatalogRuntime::SkyCatalogRuntime(std::unique_ptr<skygate::ephemeris::IStarCatalog> sourceCatalog)
{
    if (sourceCatalog != nullptr) {
        m_sources.push_back(
            SkyCatalogSourceRecord{
                .instanceId = QString::fromLatin1(kPrimarySourceId),
                .title = QStringLiteral("Bundled"),
                .version = QString(),
                .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                .enabled = true,
                .bundled = true,
                .catalog = std::move(sourceCatalog),
                .foundObjectCount = 0,
            }
        );
    }
    static_cast<void>(resetConstellationLineRefs());
}

const skygate::ephemeris::IStarCatalog* SkyCatalogRuntime::starCatalog() const noexcept
{
    return m_starCatalog.get();
}

QString SkyCatalogRuntime::sourceLabel() const
{
    const SkyCatalogSourceRecord* primary =
        firstSourceWithPolicy(skygate::ephemeris::CatalogCompositionPolicy::Merge, false);
    if (primary != nullptr) {
        return normalizedTitle(primary->title, QStringLiteral("Bundled"));
    }
    if (!m_sources.empty()) {
        return normalizedTitle(m_sources.front().title, QStringLiteral("Bundled"));
    }
    return QStringLiteral("Bundled");
}

std::size_t SkyCatalogRuntime::bodyCount() const noexcept
{
    return m_bodyCount;
}

std::size_t SkyCatalogRuntime::constellationCount() const noexcept
{
    return m_constellationRefs.count();
}

std::size_t SkyCatalogRuntime::deepSkyObjectCount() const noexcept
{
    return m_deepSkyObjectCount;
}

std::size_t SkyCatalogRuntime::deepSkyCatalogFoundObjectCount() const noexcept
{
    return m_deepSkyCatalogFoundObjectCount;
}

std::uint64_t SkyCatalogRuntime::catalogRevision() const noexcept
{
    return m_catalogRevision;
}

std::size_t SkyCatalogRuntime::sourceCount() const noexcept
{
    return m_sources.size();
}

QStringList SkyCatalogRuntime::sourceInstanceIds() const
{
    QStringList instanceIds;
    instanceIds.reserve(static_cast<int>(m_sources.size()));
    for (const SkyCatalogSourceRecord& source : m_sources) {
        instanceIds.push_back(source.instanceId);
    }
    return instanceIds;
}

bool SkyCatalogRuntime::isSourceEnabled(const QString& instanceId) const
{
    const SkyCatalogSourceRecord* source = findSource(instanceId);
    return source != nullptr && source->enabled;
}

bool SkyCatalogRuntime::hasSource(const QString& instanceId) const
{
    return findSource(instanceId) != nullptr;
}

QStringList SkyCatalogRuntime::sourceLabels() const
{
    return m_sourceLabels;
}

std::span<const SkyCatalogSourceRecord> SkyCatalogRuntime::sources() const noexcept
{
    return std::span<const SkyCatalogSourceRecord>(m_sources);
}

std::span<const std::uint8_t> SkyCatalogRuntime::sourceIds() const noexcept
{
    return std::span<const std::uint8_t>(m_sourceIds);
}

std::span<const SkyCatalogRuntime::ConstellationLineRef> SkyCatalogRuntime::constellationLineRefs() const noexcept
{
    return m_constellationRefs.lineRefs();
}

std::span<const SkyCatalogRuntime::ConstellationAnchorGroup>
SkyCatalogRuntime::constellationAnchorGroups() const noexcept
{
    return m_constellationRefs.anchorGroups();
}

std::span<const SkyCatalogRuntime::ConstellationLineRef> SkyCatalogRuntime::resolvedConstellationLineRefs() const
{
    refreshResolvedConstellationRefs();
    return std::span<const ConstellationLineRef>(m_resolvedLineRefs);
}

std::span<const SkyCatalogRuntime::ConstellationAnchorGroup>
SkyCatalogRuntime::resolvedConstellationAnchorGroups() const
{
    refreshResolvedConstellationRefs();
    return std::span<const ConstellationAnchorGroup>(m_resolvedAnchorGroups);
}

SkyCatalogRuntimeResult SkyCatalogRuntime::initialize(const SkyCatalogRuntimeBuildOptions& options)
{
    return rebuildActiveCatalog(options);
}

SkyCatalogRuntimeResult
SkyCatalogRuntime::applySource(SkyCatalogSourceRecord source, const SkyCatalogRuntimeBuildOptions& options)
{
    if (source.catalog == nullptr) {
        return failedCatalogResult(QStringLiteral("Catalog: Failed to load"));
    }

    const auto existing =
        std::find_if(m_sources.begin(), m_sources.end(), [&source](const SkyCatalogSourceRecord& candidate) {
            return candidate.instanceId == source.instanceId;
        });
    if (existing != m_sources.end()) {
        *existing = std::move(source);
    } else {
        m_sources.push_back(std::move(source));
    }
    return rebuildActiveCatalog(options);
}

SkyCatalogRuntimeResult SkyCatalogRuntime::setSourceEnabled(
    const QString& instanceId, const bool enabled, const SkyCatalogRuntimeBuildOptions& options
)
{
    const auto existing =
        std::find_if(m_sources.begin(), m_sources.end(), [&instanceId](const SkyCatalogSourceRecord& candidate) {
            return candidate.instanceId == instanceId;
        });
    if (existing == m_sources.end() || existing->enabled == enabled) {
        return SkyCatalogRuntimeResult{};
    }

    existing->enabled = enabled;
    return rebuildActiveCatalog(options);
}

SkyCatalogRuntimeResult
SkyCatalogRuntime::removeSource(const QString& instanceId, const SkyCatalogRuntimeBuildOptions& options)
{
    const auto existing =
        std::find_if(m_sources.begin(), m_sources.end(), [&instanceId](const SkyCatalogSourceRecord& candidate) {
            return candidate.instanceId == instanceId;
        });
    if (existing == m_sources.end()) {
        return SkyCatalogRuntimeResult{};
    }

    m_sources.erase(existing);
    return rebuildActiveCatalog(options);
}

SkyCatalogRuntimeResult SkyCatalogRuntime::moveSource(
    const QString& instanceId, const std::size_t targetIndex, const SkyCatalogRuntimeBuildOptions& options
)
{
    const auto existing =
        std::find_if(m_sources.begin(), m_sources.end(), [&instanceId](const SkyCatalogSourceRecord& candidate) {
            return candidate.instanceId == instanceId;
        });
    if (existing == m_sources.end()) {
        return SkyCatalogRuntimeResult{};
    }

    const auto currentIndex = static_cast<std::size_t>(std::distance(m_sources.begin(), existing));
    const std::size_t clampedTarget = std::min(targetIndex, m_sources.size() - 1);
    if (currentIndex == clampedTarget) {
        return SkyCatalogRuntimeResult{};
    }

    SkyCatalogSourceRecord moved = std::move(*existing);
    m_sources.erase(existing);
    m_sources.insert(m_sources.begin() + static_cast<std::ptrdiff_t>(clampedTarget), std::move(moved));
    return rebuildActiveCatalog(options);
}

SkyCatalogRuntimeResult SkyCatalogRuntime::replaceSources(
    std::vector<SkyCatalogSourceRecord> sources, const SkyCatalogRuntimeBuildOptions& options
)
{
    m_sources = std::move(sources);
    return rebuildActiveCatalog(options);
}

SkyCatalogRuntimeResult SkyCatalogRuntime::applyCatalog(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
    const QString& sourceLabel,
    const SkyCatalogRuntimeBuildOptions& options
)
{
    if (catalog == nullptr) {
        return failedCatalogResult(QStringLiteral("Catalog: Failed to load"));
    }

    return applySource(
        SkyCatalogSourceRecord{
            .instanceId = QString::fromLatin1(kPrimarySourceId),
            .title = sourceLabel,
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = std::move(catalog),
            .foundObjectCount = 0,
        },
        options
    );
}

SkyCatalogRuntimeResult SkyCatalogRuntime::applyDeepSkyCatalog(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
    const QString& sourceLabel,
    const std::size_t foundObjectCount,
    const SkyCatalogRuntimeBuildOptions& options
)
{
    if (catalog == nullptr) {
        return failedDeepSkyCatalogResult(QStringLiteral("Catalog: Failed to load deep-sky catalog"));
    }

    return applySource(
        SkyCatalogSourceRecord{
            .instanceId = QString::fromLatin1(kDeepSkySourceId),
            .title = sourceLabel,
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
            .enabled = true,
            .catalog = std::move(catalog),
            .foundObjectCount = foundObjectCount,
        },
        options
    );
}

SkyCatalogRuntimeResult
SkyCatalogRuntime::clearDeepSkyCatalog(const QString& sourceLabel, const SkyCatalogRuntimeBuildOptions& options)
{
    static_cast<void>(sourceLabel);
    return removeSource(QString::fromLatin1(kDeepSkySourceId), options);
}

SkyCatalogRuntimeResult SkyCatalogRuntime::rebuildActiveCatalog(const SkyCatalogRuntimeBuildOptions& options)
{
    if (m_sources.empty()) {
        auto bundledCatalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
        if (bundledCatalog == nullptr) {
            return failedCatalogResult(QStringLiteral("Catalog: Failed to load"));
        }
        m_sources.push_back(
            SkyCatalogSourceRecord{
                .instanceId = QString::fromLatin1(kPrimarySourceId),
                .title = QStringLiteral("Bundled"),
                .version = QString(),
                .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                .enabled = true,
                .bundled = true,
                .catalog = std::move(bundledCatalog),
                .foundObjectCount = 0,
            }
        );
    }

    skygate::ephemeris::CatalogCompositionRequest request;
    request.currentConstellationCount = m_constellationRefs.count();

    std::size_t knownDeepSkyObjectCount = 0;
    bool hasDeepSkyOnlySource = false;
    for (const SkyCatalogSourceRecord& source : m_sources) {
        if (!source.enabled || source.catalog == nullptr) {
            continue;
        }
        if (source.policy == skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly) {
            hasDeepSkyOnlySource = true;
            knownDeepSkyObjectCount += source.foundObjectCount;
        }
        request.sources.push_back(
            skygate::ephemeris::CatalogCompositionSourceEntry{
                .sourceId = source.instanceId.toStdString(),
                .enabled = true,
                .catalog = source.catalog.get(),
                .policy = source.policy,
            }
        );
    }
    request.knownDeepSkyObjectCount = knownDeepSkyObjectCount;

    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledCore =
        skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    if (bundledCore != nullptr) {
        request.sources.push_back(
            skygate::ephemeris::CatalogCompositionSourceEntry{
                .sourceId = std::string(kBuiltInSourceId),
                .enabled = true,
                .catalog = bundledCore.get(),
                .policy = skygate::ephemeris::CatalogCompositionPolicy::AugmentCore,
            }
        );
        if (options.useBundledDeepSkyCatalog && !hasDeepSkyOnlySource) {
            request.sources.push_back(
                skygate::ephemeris::CatalogCompositionSourceEntry{
                    .sourceId = std::string(kBundledDeepSkySourceId),
                    .enabled = true,
                    .catalog = bundledCore.get(),
                    .policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
                }
            );
        }
    }

    skygate::ephemeris::CatalogCompositionResult composed =
        skygate::ephemeris::CatalogComposer::composeCollection(request);
    if (!composed.isSuccess()) {
        return failedCatalogResult(QStringLiteral("Catalog: Failed to load"));
    }

    m_starCatalog = std::move(composed.catalog);
    ++m_catalogRevision;
    m_bodyCount = composed.bodyCount;
    m_constellationRefs.setCount(composed.constellationCount);
    m_deepSkyObjectCount = composed.deepSkyObjectCount;
    m_deepSkyCatalogFoundObjectCount = composed.foundDeepSkyObjectCount;
    rebuildSourceProvenance(composed.sourceIds);

    return SkyCatalogRuntimeResult{
        .statusText = buildStatusText(),
        .statusTextChanged = true,
        .datasetInfoChanged = true,
        .deepSkyCatalogInfoChanged = true,
        .catalogChanged = true
    };
}

SkyCatalogRuntimeResult SkyCatalogRuntime::resetConstellationLineRefs()
{
    m_constellationRefs.clear();
    ++m_catalogRevision;
    return SkyCatalogRuntimeResult{.catalogChanged = true};
}

SkyCatalogRuntimeResult SkyCatalogRuntime::setConstellationLineRefs(std::vector<ConstellationLineRef> lineRefs)
{
    if (lineRefs.empty()) {
        return resetConstellationLineRefs();
    }

    m_constellationRefs.setLineRefs(std::move(lineRefs));
    ++m_catalogRevision;
    return SkyCatalogRuntimeResult{.catalogChanged = true};
}

SkyCatalogRuntimeResult
SkyCatalogRuntime::setConstellationAnchorGroups(std::vector<ConstellationAnchorGroup> anchorGroups)
{
    m_constellationRefs.setAnchorGroups(std::move(anchorGroups));
    ++m_catalogRevision;
    return SkyCatalogRuntimeResult{.catalogChanged = true};
}

SkyCatalogRuntimeResult SkyCatalogRuntime::restoreConstellationRefs(
    std::vector<ConstellationLineRef> lineRefs,
    std::vector<ConstellationAnchorGroup> anchorGroups,
    const std::optional<std::size_t> constellationCount
)
{
    SkyCatalogRuntimeResult result = setConstellationLineRefs(std::move(lineRefs));
    const SkyCatalogRuntimeResult labelResult = setConstellationAnchorGroups(std::move(anchorGroups));
    result.catalogChanged = result.catalogChanged || labelResult.catalogChanged;

    if (constellationCount.has_value() && constellationCount.value() != m_constellationRefs.count()) {
        m_constellationRefs.setCount(constellationCount.value());
        result.datasetInfoChanged = true;
    }
    return result;
}

SkyCatalogRuntimeResult SkyCatalogRuntime::failedCatalogResult(const QString& statusText)
{
    m_bodyCount = 0;
    m_constellationRefs.setCount(0);
    m_deepSkyObjectCount = 0;
    m_sourceLabels.clear();
    m_sourceIds.clear();
    return SkyCatalogRuntimeResult{.statusText = statusText, .statusTextChanged = true, .datasetInfoChanged = true};
}

SkyCatalogRuntimeResult SkyCatalogRuntime::failedDeepSkyCatalogResult(const QString& statusText)
{
    return SkyCatalogRuntimeResult{.statusText = statusText, .statusTextChanged = true};
}

QString SkyCatalogRuntime::buildStatusText() const
{
    const QLocale locale = QLocale::system();
    return QStringLiteral("Catalog: %1 + %2 (%3 objects, %4 deep sky, %5 constellations)")
        .arg(
            sourceLabel(),
            deepSkySourceLabel(),
            locale.toString(static_cast<qulonglong>(m_bodyCount)),
            locale.toString(static_cast<qulonglong>(m_deepSkyObjectCount)),
            locale.toString(static_cast<qulonglong>(m_constellationRefs.count()))
        );
}

QString SkyCatalogRuntime::deepSkySourceLabel() const
{
    const SkyCatalogSourceRecord* deepSky =
        firstSourceWithPolicy(skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly, false);
    if (deepSky != nullptr) {
        return normalizedTitle(deepSky->title, QStringLiteral("Bundled Messier"));
    }
    return QStringLiteral("Bundled Messier");
}

const SkyCatalogSourceRecord* SkyCatalogRuntime::findSource(const QString& instanceId) const
{
    const auto it =
        std::find_if(m_sources.begin(), m_sources.end(), [&instanceId](const SkyCatalogSourceRecord& candidate) {
            return candidate.instanceId == instanceId;
        });
    return it != m_sources.end() ? &*it : nullptr;
}

const SkyCatalogSourceRecord* SkyCatalogRuntime::firstSourceWithPolicy(
    const skygate::ephemeris::CatalogCompositionPolicy policy, const bool requireEnabled
) const
{
    for (const SkyCatalogSourceRecord& source : m_sources) {
        if (source.policy != policy) {
            continue;
        }
        if (requireEnabled && !source.enabled) {
            continue;
        }
        return &source;
    }
    return nullptr;
}

void SkyCatalogRuntime::rebuildSourceProvenance(const std::vector<std::string>& composedSourceIds)
{
    QStringList labels;
    QHash<QString, std::uint8_t> indexBySourceId;
    labels.reserve(static_cast<int>(m_sources.size()) + 1);

    const auto labelForSourceId = [this](const QString& instanceId) {
        const SkyCatalogSourceRecord* source = findSource(instanceId);
        if (source != nullptr) {
            return normalizedTitle(source->title, QStringLiteral("Catalog"));
        }
        if (instanceId == QString::fromLatin1(kBuiltInSourceId)) {
            return QStringLiteral("Built-in ephemeris");
        }
        if (instanceId == QString::fromLatin1(kBundledDeepSkySourceId)) {
            return QStringLiteral("Bundled Messier");
        }
        return QStringLiteral("Catalog");
    };

    const auto indexForSourceId = [&](const QString& instanceId) {
        const auto existing = indexBySourceId.constFind(instanceId);
        if (existing != indexBySourceId.constEnd()) {
            return existing.value();
        }
        const auto index = static_cast<std::uint8_t>(labels.size());
        labels.push_back(labelForSourceId(instanceId));
        indexBySourceId.insert(instanceId, index);
        return index;
    };

    m_sourceIds.clear();
    m_sourceIds.reserve(composedSourceIds.size());
    for (const std::string& sourceId : composedSourceIds) {
        m_sourceIds.push_back(indexForSourceId(QString::fromStdString(sourceId)));
    }
    m_sourceLabels = std::move(labels);
}

void SkyCatalogRuntime::refreshResolvedConstellationRefs() const
{
    if (m_resolvedRevision == m_catalogRevision) {
        return;
    }

    m_resolvedLineRefs.clear();
    m_resolvedAnchorGroups.clear();
    if (m_starCatalog != nullptr) {
        const skygate::ephemeris::ConstellationReferenceResolver resolver(m_starCatalog->bodies());
        m_resolvedLineRefs = resolver.resolveLines(m_constellationRefs.lineRefs());
        m_resolvedAnchorGroups = resolver.resolveAnchors(m_constellationRefs.anchorGroups());
    }
    m_resolvedRevision = m_catalogRevision;
}

}  // namespace skygate::ui::internal
