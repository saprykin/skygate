#include "SkyCatalogRuntime.hpp"

#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentity.hpp"
#include "catalog/constellation/ConstellationReferenceResolver.hpp"

#include <QHash>
#include <QLocale>

#include <algorithm>
#include <optional>
#include <unordered_set>
#include <utility>

namespace skygate::ui::internal {
namespace {

constexpr const char* kPrimarySourceId = "primary";
constexpr const char* kBundledCoreSourceId = "bundled-core";
constexpr const char* kBundledDeepSkySourceId = "bundled-deep-sky";

QString normalizedTitle(const QString& title, const QString& fallback)
{
    const QString normalized = title.trimmed();
    return normalized.isEmpty() ? fallback : normalized;
}

bool containsSourceId(const std::vector<QString>& sourceIds, const QString& instanceId)
{
    return std::find(sourceIds.begin(), sourceIds.end(), instanceId) != sourceIds.end();
}

std::size_t
countConstellationBodies(const std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies) noexcept
{
    return static_cast<std::size_t>(
        std::count_if(bodies.begin(), bodies.end(), [](const skygate::ephemeris::BaseCelestialBody* body) {
            return body != nullptr && body->kind == skygate::ephemeris::BaseCelestialBody::Kind::Constellation;
        })
    );
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
}

const skygate::ephemeris::IStarCatalog* SkyCatalogRuntime::starCatalog() const noexcept
{
    return m_starCatalog.get();
}

QString SkyCatalogRuntime::statusText() const
{
    return buildStatusText();
}

QString SkyCatalogRuntime::sourceLabel() const
{
    for (const SkyCatalogSourceRecord& source : m_sources) {
        if (source.enabled) {
            return normalizedTitle(source.title, source.instanceId);
        }
    }

    // No configured source participates: the implicit bundled contributions are
    // the only sources left, so name the leading one instead of naming a
    // configured source the user disabled.
    if (!m_contributingSourceIds.empty()) {
        return sourceTitle(m_contributingSourceIds.front());
    }
    return QStringLiteral("Bundled");
}

QString SkyCatalogRuntime::participationSummary() const
{
    QStringList titles;
    titles.reserve(static_cast<int>(m_sources.size()) + 2);
    for (const SkyCatalogSourceRecord& source : m_sources) {
        if (!source.enabled) {
            continue;
        }
        titles.push_back(normalizedTitle(source.title, source.instanceId));
    }

    // The bundled core augmentation and the bundled deep-sky fallback are not
    // configured sources, so they are named only when they supplied objects to
    // the active snapshot, in the precedence order the composition used.
    for (const char* implicitSourceId : {kBundledCoreSourceId, kBundledDeepSkySourceId}) {
        const QString instanceId = QString::fromLatin1(implicitSourceId);
        if (containsSourceId(m_contributingSourceIds, instanceId)) {
            titles.push_back(sourceTitle(instanceId));
        }
    }

    if (titles.isEmpty()) {
        return QStringLiteral("No active sources");
    }
    return titles.join(QStringLiteral(" + "));
}

std::size_t SkyCatalogRuntime::bodyCount() const noexcept
{
    return m_bodyCount;
}

std::size_t SkyCatalogRuntime::constellationCount() const noexcept
{
    return std::max(m_catalogConstellationCount, m_constellationRefs.count());
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

QHash<QString, QString> SkyCatalogRuntime::sourceTitles() const
{
    return m_sourceTitles;
}

QString SkyCatalogRuntime::sourceTitle(const QString& instanceId) const
{
    const auto it = m_sourceTitles.constFind(instanceId);
    if (it == m_sourceTitles.cend()) {
        return normalizedTitle(instanceId, QStringLiteral("Catalog"));
    }
    return it.value();
}

std::span<const SkyCatalogSourceRecord> SkyCatalogRuntime::sources() const noexcept
{
    return std::span<const SkyCatalogSourceRecord>(m_sources);
}

std::span<const QString> SkyCatalogRuntime::sourceIds() const noexcept
{
    return std::span<const QString>(m_sourceIds);
}

const std::vector<QStringList>& SkyCatalogRuntime::contributorSourceIds() const noexcept
{
    return m_contributorSourceIds;
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
        // A source without a catalog is never installed: the last accepted
        // collection stays active and the operation reports the error.
        return activationFailureResult(QStringLiteral("Catalog: Failed to load"));
    }

    const auto existing =
        std::find_if(m_sources.begin(), m_sources.end(), [&source](const SkyCatalogSourceRecord& candidate) {
            return candidate.instanceId == source.instanceId;
        });
    if (existing == m_sources.end()) {
        m_sources.push_back(std::move(source));
        const SkyCatalogRuntimeResult result = rebuildActiveCatalog(options);
        if (!result.succeeded) {
            // The rejected addition is not kept configured.
            m_sources.pop_back();
        }
        return result;
    }

    const std::size_t existingIndex = static_cast<std::size_t>(std::distance(m_sources.begin(), existing));
    SkyCatalogSourceRecord previous = std::move(m_sources[existingIndex]);
    // The related dataset is owned by the instance and changes only through
    // the explicit related-data operations, so replacing the catalog keeps it
    // until its owner replaces or clears the dataset.
    source.constellationData = std::move(previous.constellationData);
    m_sources[existingIndex] = std::move(source);

    const SkyCatalogRuntimeResult result = rebuildActiveCatalog(options);
    if (!result.succeeded) {
        // The rejected replacement is not kept configured: the previous
        // record, including its owned related dataset, stays in place.
        previous.constellationData = std::move(m_sources[existingIndex].constellationData);
        m_sources[existingIndex] = std::move(previous);
    }
    return result;
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

    const std::size_t existingIndex = static_cast<std::size_t>(std::distance(m_sources.begin(), existing));
    m_sources[existingIndex].enabled = enabled;
    const SkyCatalogRuntimeResult result = rebuildActiveCatalog(options);
    if (!result.succeeded) {
        // The rejected participation change is not kept configured.
        m_sources[existingIndex].enabled = !enabled;
    }
    return result;
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

    const std::size_t existingIndex = static_cast<std::size_t>(std::distance(m_sources.begin(), existing));
    SkyCatalogSourceRecord removed = std::move(m_sources[existingIndex]);
    m_sources.erase(m_sources.begin() + static_cast<std::ptrdiff_t>(existingIndex));
    const SkyCatalogRuntimeResult result = rebuildActiveCatalog(options);
    if (!result.succeeded) {
        // The rejected removal restores the removed record at its position.
        m_sources.insert(m_sources.begin() + static_cast<std::ptrdiff_t>(existingIndex), std::move(removed));
    }
    return result;
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

    SkyCatalogSourceRecord moved = std::move(m_sources[currentIndex]);
    m_sources.erase(m_sources.begin() + static_cast<std::ptrdiff_t>(currentIndex));
    m_sources.insert(m_sources.begin() + static_cast<std::ptrdiff_t>(clampedTarget), std::move(moved));
    const SkyCatalogRuntimeResult result = rebuildActiveCatalog(options);
    if (!result.succeeded) {
        // The rejected reorder restores the previous visible order.
        SkyCatalogSourceRecord staged = std::move(m_sources[clampedTarget]);
        m_sources.erase(m_sources.begin() + static_cast<std::ptrdiff_t>(clampedTarget));
        m_sources.insert(m_sources.begin() + static_cast<std::ptrdiff_t>(currentIndex), std::move(staged));
    }
    return result;
}

SkyCatalogRuntimeResult SkyCatalogRuntime::replaceSources(
    std::vector<SkyCatalogSourceRecord> sources, const SkyCatalogRuntimeBuildOptions& options
)
{
    std::vector<SkyCatalogSourceRecord> previousSources = std::move(m_sources);
    m_sources = std::move(sources);
    const SkyCatalogRuntimeResult result = rebuildActiveCatalog(options);
    if (!result.succeeded) {
        // The rejected collection is not installed; the previous configuration
        // keeps feeding the published snapshot.
        m_sources = std::move(previousSources);
    }
    return result;
}

SkyCatalogRuntimeResult SkyCatalogRuntime::rebuildActiveCatalog(const SkyCatalogRuntimeBuildOptions& options)
{
    // The whole transition is staged: the implicit bundled source, the related
    // view, the composition, and every derived count are built locally, and
    // only an accepted composition commits them. A rejected composition
    // reports the operation error and leaves the published state untouched.
    std::optional<SkyCatalogSourceRecord> implicitBundledSource;
    if (m_sources.empty()) {
        auto bundledCatalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
        if (bundledCatalog == nullptr) {
            return activationFailureResult(QStringLiteral("Catalog: Failed to load"));
        }
        implicitBundledSource = SkyCatalogSourceRecord{
            .instanceId = QString::fromLatin1(kPrimarySourceId),
            .title = QStringLiteral("Bundled"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .bundled = true,
            .catalog = std::move(bundledCatalog),
            .foundObjectCount = 0,
        };
    }

    std::vector<const SkyCatalogSourceRecord*> candidateSources;
    candidateSources.reserve(m_sources.size() + (implicitBundledSource.has_value() ? 1U : 0U));
    for (const SkyCatalogSourceRecord& source : m_sources) {
        candidateSources.push_back(&source);
    }
    if (implicitBundledSource.has_value()) {
        candidateSources.push_back(&*implicitBundledSource);
    }

    skygate::ephemeris::CatalogCompositionRequest request;
    std::vector<ConstellationLineRef> activeLineRefs;
    std::vector<ConstellationAnchorGroup> activeAnchorGroups;
    // The active related view is recomposed from the owned datasets of the
    // enabled sources before composition, so the reported constellation count
    // reflects the same collection state as the composed snapshot.
    request.currentConstellationCount = buildActiveConstellationView(activeLineRefs, activeAnchorGroups);

    std::size_t knownDeepSkyObjectCount = 0;
    for (const SkyCatalogSourceRecord* source : candidateSources) {
        if (!source->enabled || source->catalog == nullptr) {
            continue;
        }
        if (source->policy == skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly) {
            knownDeepSkyObjectCount += source->foundObjectCount;
        }
        request.sources.push_back(
            skygate::ephemeris::CatalogCompositionSourceEntry{
                .sourceId = source->instanceId.toStdString(),
                .enabled = true,
                .catalog = source->catalog.get(),
                .policy = source->policy,
            }
        );
    }

    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledCore =
        skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    if (bundledCore != nullptr) {
        // Bundled core augmentation is always required: it supplies the
        // Sun/Moon/planet bodies, and the bundled bright-star fallback when no
        // configured source supplies a star.
        request.sources.push_back(
            skygate::ephemeris::CatalogCompositionSourceEntry{
                .sourceId = std::string(kBundledCoreSourceId),
                .enabled = true,
                .catalog = bundledCore.get(),
                .policy = skygate::ephemeris::CatalogCompositionPolicy::AugmentCore,
            }
        );

        // The bundled deep-sky fallback participates only when the composition
        // configuration enables it, and it only fills deep-sky identities no
        // configured source supplies.
        if (options.bundledDeepSkyParticipation
            == SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation::Fallback) {
            request.sources.push_back(
                skygate::ephemeris::CatalogCompositionSourceEntry{
                    .sourceId = std::string(kBundledDeepSkySourceId),
                    .enabled = true,
                    .catalog = bundledCore.get(),
                    .policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyFallback,
                }
            );
            knownDeepSkyObjectCount += skygate::ephemeris::CatalogIdentity::countDeepSkyObjects(bundledCore->bodies());
        }
    }
    request.knownDeepSkyObjectCount = knownDeepSkyObjectCount;

    skygate::ephemeris::CatalogCompositionResult composed =
        skygate::ephemeris::CatalogComposer::composeCollection(request);
    if (!composed.isSuccess()) {
        const QString detail = QString::fromStdString(composed.errorDetail).trimmed();
        return activationFailureResult(
            detail.isEmpty() ? QStringLiteral("Catalog: Failed to load")
                             : QStringLiteral("Catalog: Collection rejected: %1").arg(detail)
        );
    }

    // The composition is accepted: commit the candidate configuration and
    // publish every derived value together.
    if (implicitBundledSource.has_value()) {
        m_sources.push_back(std::move(*implicitBundledSource));
    }
    m_starCatalog = std::move(composed.catalog);
    ++m_catalogRevision;
    m_bodyCount = composed.bodyCount;
    m_catalogConstellationCount = countConstellationBodies(m_starCatalog->bodies());
    static_cast<void>(m_constellationRefs.setDataset(
        std::move(activeLineRefs), std::move(activeAnchorGroups), request.currentConstellationCount
    ));
    m_deepSkyObjectCount = composed.deepSkyObjectCount;
    m_deepSkyCatalogFoundObjectCount = composed.foundDeepSkyObjectCount;
    rebuildSourceProvenance(composed.sourceIds, composed.contributorSourceIds);
    rebuildSourceParticipation(composed.sourceOrder);

    return SkyCatalogRuntimeResult{
        .statusText = buildStatusText(),
        .statusTextChanged = true,
        .datasetInfoChanged = true,
        .deepSkyCatalogInfoChanged = true,
        .catalogChanged = true
    };
}

SkyCatalogRuntimeResult SkyCatalogRuntime::setSourceConstellationRefs(
    const QString& instanceId,
    std::vector<ConstellationLineRef> lineRefs,
    std::vector<ConstellationAnchorGroup> anchorGroups,
    const std::size_t constellationCount
)
{
    SkyCatalogSourceRecord* source = findSource(instanceId);
    if (source == nullptr
        || !source->constellationData.setDataset(std::move(lineRefs), std::move(anchorGroups), constellationCount)) {
        return SkyCatalogRuntimeResult{};
    }

    return refreshActiveConstellationView();
}

SkyCatalogRuntimeResult SkyCatalogRuntime::clearSourceConstellationRefs(const QString& instanceId)
{
    SkyCatalogSourceRecord* source = findSource(instanceId);
    if (source == nullptr || !source->constellationData.clear()) {
        return SkyCatalogRuntimeResult{};
    }

    return refreshActiveConstellationView();
}

SkyCatalogRuntimeResult SkyCatalogRuntime::activationFailureResult(const QString& statusText)
{
    // A rejected transition publishes no state: the caller receives the
    // operation error and the last accepted configuration, snapshot, counts,
    // provenance, related view, and revision stay exactly as they were.
    return SkyCatalogRuntimeResult{.statusText = statusText, .statusTextChanged = true, .succeeded = false};
}

QString SkyCatalogRuntime::buildStatusText() const
{
    const QLocale locale = QLocale::system();
    return QStringLiteral("Catalog: %1 (%2 objects, %3 deep sky, %4 constellations)")
        .arg(
            participationSummary(),
            locale.toString(static_cast<qulonglong>(m_bodyCount)),
            locale.toString(static_cast<qulonglong>(m_deepSkyObjectCount)),
            locale.toString(static_cast<qulonglong>(constellationCount()))
        );
}

SkyCatalogSourceRecord* SkyCatalogRuntime::findSource(const QString& instanceId)
{
    const auto it =
        std::find_if(m_sources.begin(), m_sources.end(), [&instanceId](const SkyCatalogSourceRecord& candidate) {
            return candidate.instanceId == instanceId;
        });
    return it != m_sources.end() ? &*it : nullptr;
}

const SkyCatalogSourceRecord* SkyCatalogRuntime::findSource(const QString& instanceId) const
{
    const auto it =
        std::find_if(m_sources.begin(), m_sources.end(), [&instanceId](const SkyCatalogSourceRecord& candidate) {
            return candidate.instanceId == instanceId;
        });
    return it != m_sources.end() ? &*it : nullptr;
}

void SkyCatalogRuntime::rebuildSourceParticipation(const std::vector<std::string>& contributingSourceIds)
{
    m_contributingSourceIds.clear();
    m_contributingSourceIds.reserve(contributingSourceIds.size());
    for (const std::string& sourceId : contributingSourceIds) {
        m_contributingSourceIds.push_back(QString::fromStdString(sourceId));
    }
}

void SkyCatalogRuntime::rebuildSourceProvenance(
    const std::vector<std::string>& composedSourceIds, const std::vector<std::vector<std::string>>& contributorSourceIds
)
{
    m_sourceTitles.clear();

    const auto titleForSourceId = [this](const QString& instanceId) {
        const SkyCatalogSourceRecord* source = findSource(instanceId);
        if (source != nullptr) {
            return normalizedTitle(source->title, QStringLiteral("Catalog"));
        }
        if (instanceId == QString::fromLatin1(kBundledCoreSourceId)) {
            return QStringLiteral("Bundled core");
        }
        if (instanceId == QString::fromLatin1(kBundledDeepSkySourceId)) {
            return QStringLiteral("Bundled Messier");
        }
        return QStringLiteral("Catalog");
    };

    const auto recordTitle = [&](const QString& instanceId) {
        if (!m_sourceTitles.contains(instanceId)) {
            m_sourceTitles.insert(instanceId, titleForSourceId(instanceId));
        }
    };

    m_sourceIds.clear();
    m_sourceIds.reserve(composedSourceIds.size());
    for (const std::string& sourceId : composedSourceIds) {
        const QString instanceId = QString::fromStdString(sourceId);
        recordTitle(instanceId);
        m_sourceIds.push_back(instanceId);
    }

    m_contributorSourceIds.clear();
    m_contributorSourceIds.reserve(contributorSourceIds.size());
    for (const std::vector<std::string>& contributors : contributorSourceIds) {
        QStringList instanceIds;
        instanceIds.reserve(static_cast<int>(contributors.size()));
        for (const std::string& sourceId : contributors) {
            const QString instanceId = QString::fromStdString(sourceId);
            recordTitle(instanceId);
            instanceIds.push_back(instanceId);
        }
        m_contributorSourceIds.push_back(std::move(instanceIds));
    }
}

std::size_t SkyCatalogRuntime::buildActiveConstellationView(
    std::vector<ConstellationLineRef>& lineRefs, std::vector<ConstellationAnchorGroup>& anchorGroups
) const
{
    lineRefs.clear();
    anchorGroups.clear();

    std::unordered_set<std::string> seenSegments;
    std::size_t declaredCount = 0U;
    for (const SkyCatalogSourceRecord& source : m_sources) {
        if (!source.enabled) {
            continue;
        }

        const SkyCatalogConstellationStore& owned = source.constellationData;
        if (owned.lineRefVector().empty() && owned.anchorGroupVector().empty() && owned.count() == 0U) {
            continue;
        }
        if (owned.count() > 0U) {
            declaredCount = owned.count();
        }

        for (const ConstellationLineRef& lineRef : owned.lineRefVector()) {
            std::string segmentKey = lineRef.first;
            segmentKey += '\n';
            segmentKey += lineRef.second;
            if (seenSegments.insert(segmentKey).second) {
                lineRefs.push_back(lineRef);
            }
        }

        for (const ConstellationAnchorGroup& anchorGroup : owned.anchorGroupVector()) {
            if (anchorGroup.first.empty()) {
                anchorGroups.push_back(anchorGroup);
                continue;
            }
            const auto existing =
                std::find_if(anchorGroups.begin(), anchorGroups.end(), [&anchorGroup](const auto& candidate) {
                    return candidate.first == anchorGroup.first;
                });
            if (existing != anchorGroups.end()) {
                existing->second = anchorGroup.second;
            } else {
                anchorGroups.push_back(anchorGroup);
            }
        }
    }

    const auto namedAnchorGroupCount = static_cast<std::size_t>(
        std::count_if(anchorGroups.begin(), anchorGroups.end(), [](const ConstellationAnchorGroup& anchorGroup) {
            return !anchorGroup.first.empty();
        })
    );
    return std::max(declaredCount, namedAnchorGroupCount);
}

SkyCatalogRuntimeResult SkyCatalogRuntime::refreshActiveConstellationView()
{
    std::vector<ConstellationLineRef> lineRefs;
    std::vector<ConstellationAnchorGroup> anchorGroups;
    const std::size_t declaredCount = buildActiveConstellationView(lineRefs, anchorGroups);

    const bool referencesChanged =
        lineRefs != m_constellationRefs.lineRefVector() || anchorGroups != m_constellationRefs.anchorGroupVector();
    const std::size_t previousCount = constellationCount();
    if (!referencesChanged && declaredCount == m_constellationRefs.count()) {
        return SkyCatalogRuntimeResult{};
    }

    static_cast<void>(m_constellationRefs.setDataset(std::move(lineRefs), std::move(anchorGroups), declaredCount));
    SkyCatalogRuntimeResult result;
    result.datasetInfoChanged = previousCount != constellationCount();
    if (referencesChanged) {
        ++m_catalogRevision;
        result.catalogChanged = true;
    }
    return result;
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
