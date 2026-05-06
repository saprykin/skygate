#include "SkyCatalogRuntime.hpp"

#include "SkyActiveCatalogBuilder.hpp"

#include "skygate/ephemeris/CatalogFactory.hpp"

#include <utility>

namespace skygate::ui::internal {

SkyCatalogRuntime::SkyCatalogRuntime(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> sourceCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine
)
    : m_sourceCatalog(std::move(sourceCatalog))
    , m_ephemerisEngine(std::move(ephemerisEngine))
{
    static_cast<void>(resetConstellationLineRefs());
}

const skygate::ephemeris::IStarCatalog* SkyCatalogRuntime::starCatalog() const noexcept
{
    return m_starCatalog.get();
}

const skygate::ephemeris::IEphemerisEngine* SkyCatalogRuntime::ephemerisEngine() const noexcept
{
    return m_ephemerisEngine.get();
}

QString SkyCatalogRuntime::sourceLabel() const
{
    return m_sourceLabel;
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

QStringList SkyCatalogRuntime::sourceLabels() const
{
    return m_sourceLabels;
}

std::span<const std::uint8_t> SkyCatalogRuntime::sourceIds() const noexcept
{
    return std::span<const std::uint8_t>(m_sourceIds);
}

std::span<const SkyCatalogRuntime::ConstellationLineRef>
SkyCatalogRuntime::constellationLineRefs() const noexcept
{
    return m_constellationRefs.lineRefs();
}

std::span<const SkyCatalogRuntime::ConstellationLabelRef>
SkyCatalogRuntime::constellationLabelRefs() const noexcept
{
    return m_constellationRefs.labelRefs();
}

SkyCatalogRuntimeResult SkyCatalogRuntime::initialize(
    const SkyCatalogRuntimeBuildOptions& options
)
{
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

    m_sourceCatalog = std::move(catalog);
    m_sourceLabel = sourceLabel;
    return rebuildActiveCatalog(options);
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

    m_deepSkyCatalog = std::move(catalog);
    m_deepSkySourceLabel = sourceLabel;
    m_deepSkyCatalogFoundObjectCount = foundObjectCount;
    return rebuildActiveCatalog(options);
}

SkyCatalogRuntimeResult SkyCatalogRuntime::clearDeepSkyCatalog(
    const QString& sourceLabel,
    const SkyCatalogRuntimeBuildOptions& options
)
{
    m_deepSkyCatalog.reset();
    m_deepSkyCatalogFoundObjectCount = 0;
    m_deepSkySourceLabel = sourceLabel;
    return rebuildActiveCatalog(options);
}

SkyCatalogRuntimeResult SkyCatalogRuntime::rebuildActiveCatalog(
    const SkyCatalogRuntimeBuildOptions& options
)
{
    if (m_sourceCatalog == nullptr) {
        m_sourceCatalog = skygate::ephemeris::createBundledStarCatalog();
        m_sourceLabel = QStringLiteral("Bundled");
    }

    if (m_sourceCatalog == nullptr) {
        return failedCatalogResult(QStringLiteral("Catalog: Failed to load"));
    }

    const SkyActiveCatalogBuildRequest request {
        .sourceCatalog = *m_sourceCatalog,
        .deepSkyCatalog = m_deepSkyCatalog.get(),
        .useBundledDeepSkyCatalog = options.useBundledDeepSkyCatalog,
        .currentConstellationCount = m_constellationRefs.count(),
        .knownDeepSkyObjectCount = m_deepSkyCatalogFoundObjectCount,
        .sourceLabel = m_sourceLabel,
        .deepSkySourceLabel = m_deepSkySourceLabel
    };
    auto buildResult = SkyActiveCatalogBuilder::build(request);
    if (!buildResult.isSuccess()) {
        m_bodyCount = 0;
        m_constellationRefs.setCount(0);
        m_deepSkyObjectCount = 0;
        return SkyCatalogRuntimeResult {
            .statusText = buildResult.errorText,
            .statusTextChanged = true,
            .datasetInfoChanged = true
        };
    }

    m_starCatalog = std::move(buildResult.catalog);
    m_ephemerisEngine = std::move(buildResult.ephemerisEngine);
    ++m_catalogRevision;
    m_bodyCount = buildResult.bodyCount;
    m_constellationRefs.setCount(buildResult.constellationCount);
    m_deepSkyObjectCount = buildResult.deepSkyObjectCount;
    m_deepSkyCatalogFoundObjectCount = buildResult.foundDeepSkyObjectCount;
    m_sourceLabels = buildResult.sourceLabels;
    m_sourceIds = std::move(buildResult.sourceIds);
    return SkyCatalogRuntimeResult {
        .statusText = buildResult.statusText,
        .statusTextChanged = true,
        .datasetInfoChanged = true,
        .deepSkyCatalogInfoChanged = true,
        .catalogChanged = true
    };
}

SkyCatalogRuntimeResult SkyCatalogRuntime::resetConstellationLineRefs()
{
    m_constellationRefs.resetToBundled();
    ++m_catalogRevision;
    return SkyCatalogRuntimeResult {
        .catalogChanged = true
    };
}

SkyCatalogRuntimeResult SkyCatalogRuntime::setConstellationLineRefs(
    std::vector<ConstellationLineRef> lineRefs
)
{
    if (lineRefs.empty()) {
        return resetConstellationLineRefs();
    }

    m_constellationRefs.setLineRefs(std::move(lineRefs));
    ++m_catalogRevision;
    return SkyCatalogRuntimeResult {
        .catalogChanged = true
    };
}

SkyCatalogRuntimeResult SkyCatalogRuntime::setConstellationLabelRefs(
    std::vector<ConstellationLabelRef> labelRefs
)
{
    m_constellationRefs.setLabelRefs(std::move(labelRefs));
    ++m_catalogRevision;
    return SkyCatalogRuntimeResult {
        .catalogChanged = true
    };
}

SkyCatalogRuntimeResult SkyCatalogRuntime::restoreConstellationRefs(
    std::vector<ConstellationLineRef> lineRefs,
    std::vector<ConstellationLabelRef> labelRefs,
    const std::optional<std::size_t> constellationCount
)
{
    SkyCatalogRuntimeResult result = setConstellationLineRefs(std::move(lineRefs));
    const SkyCatalogRuntimeResult labelResult = setConstellationLabelRefs(std::move(labelRefs));
    result.catalogChanged = result.catalogChanged || labelResult.catalogChanged;

    if (
        constellationCount.has_value()
        && constellationCount.value() != m_constellationRefs.count()
    ) {
        m_constellationRefs.setCount(constellationCount.value());
        result.datasetInfoChanged = true;
    }
    return result;
}

std::optional<SkyCatalogCachePersistRequest> SkyCatalogRuntime::cachePersistRequest(
    const QByteArray& catalogPayload,
    const QByteArray& deepSkyCatalogPayload
) const
{
    if (
        m_starCatalog == nullptr
        || (catalogPayload.isEmpty() && deepSkyCatalogPayload.isEmpty())
    ) {
        return std::nullopt;
    }

    const auto bodies = m_starCatalog->bodies();
    if (bodies.empty()) {
        return std::nullopt;
    }

    SkyCatalogCachePersistRequest request;
    request.sourceLabel = m_sourceLabel;
    request.deepSkySourceLabel = m_deepSkySourceLabel;
    request.catalogPayload = catalogPayload;
    request.deepSkyCatalogPayload = deepSkyCatalogPayload;
    request.constellationLineRefs = m_constellationRefs.lineRefVector();
    request.constellationLabelRefs = m_constellationRefs.labelRefVector();
    request.constellationCount = m_constellationRefs.count();
    return request;
}

SkyCatalogRuntimeResult SkyCatalogRuntime::failedCatalogResult(const QString& statusText)
{
    m_bodyCount = 0;
    m_constellationRefs.setCount(0);
    m_deepSkyObjectCount = 0;
    m_sourceLabels.clear();
    m_sourceIds.clear();
    return SkyCatalogRuntimeResult {
        .statusText = statusText,
        .statusTextChanged = true,
        .datasetInfoChanged = true
    };
}

SkyCatalogRuntimeResult SkyCatalogRuntime::failedDeepSkyCatalogResult(const QString& statusText)
{
    return SkyCatalogRuntimeResult {
        .statusText = statusText,
        .statusTextChanged = true
    };
}

}  // namespace skygate::ui::internal
