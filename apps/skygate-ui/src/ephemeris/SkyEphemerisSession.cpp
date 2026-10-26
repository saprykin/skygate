#include "SkyEphemerisSession.hpp"
#include "composition/EphemerisBackendResourceCache.hpp"
#include "composition/EphemerisGuidanceComposition.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/EphemerisEngineDescriptorRegistry.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "factory/EphemerisEngineFactoryRequest.hpp"
#include "factory/EphemerisEngineReplacementPolicy.hpp"
#include "hash/Fnv1a64Hasher.hpp"
#include "hash/HashConstants.hpp"
#include "time/EpochCodec.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

using skygate::core::Fnv1a64Hasher;
using skygate::core::HashConstants;

[[nodiscard]] std::uint64_t textDataAssetRevision(
    const std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot>& snapshot,
    const std::uint64_t dataRevision,
    const bool earthOrientation
) noexcept
{
    if (snapshot == nullptr) {
        return 0U;
    }

    const std::optional<skygate::ephemeris::EphemerisTextDataAsset> asset =
        earthOrientation ? snapshot->earthOrientationDataAsset() : snapshot->leapSecondTableAsset();
    if (!asset.has_value()) {
        return dataRevision;
    }

    std::uint64_t revision = Fnv1a64Hasher::combine(HashConstants::kFnv1a64OffsetBasis, dataRevision);
    revision = Fnv1a64Hasher::appendComponent(revision, asset->id);
    revision = Fnv1a64Hasher::appendComponent(revision, asset->version);
    revision = Fnv1a64Hasher::appendComponent(revision, asset->provenance);
    return revision;
}

[[nodiscard]] std::optional<skygate::ephemeris::EphemerisDateRange> activeTextAssetRange(
    const skygate::ephemeris::EphemerisDataManifest* manifest,
    const skygate::ephemeris::EphemerisDataManifest::AssetKind kind,
    const QString& activeVersion
)
{
    if (manifest == nullptr || activeVersion.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const std::string version = activeVersion.trimmed().toStdString();
    for (const skygate::ephemeris::EphemerisDataManifest::Asset& asset : manifest->assets) {
        if (asset.kind == kind && asset.version == version) {
            return asset.validityRange;
        }
    }

    return std::nullopt;
}

}  // namespace

SkyEphemerisSession::SkyEphemerisSession(
    std::shared_ptr<const skygate::ephemeris::IEphemerisEngine> initialEngine, FactoryInputs factoryInputs
)
    : m_engine(std::move(initialEngine)),
      m_guidanceStrategy(skygate::ephemeris::EphemerisGuidanceComposition::defaultGuidanceStrategy()),
      m_datasetManifest(factoryInputs.datasetManifest),
      m_backendResourceCache(
          std::make_unique<skygate::ephemeris::EphemerisBackendResourceCache>(
              skygate::ephemeris::EphemerisBackendResourceCache::InitialProviders{
                  .timeScaleService = std::move(factoryInputs.timeScaleService),
                  .earthOrientationProvider = std::move(factoryInputs.earthOrientationProvider),
                  .calcephKernelProvider = std::move(factoryInputs.calcephKernelProvider),
              }
          )
      ),
      m_diagnosticsSink(factoryInputs.diagnosticsSink)
{
    if (m_engine != nullptr) {
        m_engineKind = m_engine->kind();
        m_engineOptions = m_engine->options();
    }
}

SkyEphemerisSession::~SkyEphemerisSession() = default;

const skygate::ephemeris::IEphemerisEngine* SkyEphemerisSession::engine() const noexcept
{
    return m_engine.get();
}

std::shared_ptr<const skygate::ephemeris::IEphemerisEngine> SkyEphemerisSession::engineHandle() const
{
    const std::lock_guard<std::mutex> lock(m_engineMutex);
    return m_engine;
}

std::shared_ptr<skygate::ephemeris::IEphemerisGuidanceStrategy> SkyEphemerisSession::guidanceStrategy() const noexcept
{
    return m_guidanceStrategy;
}

skygate::ephemeris::EphemerisEngineKind::Type SkyEphemerisSession::engineKind() const noexcept
{
    return m_engineKind;
}

const skygate::ephemeris::EphemerisEngineOptions& SkyEphemerisSession::engineOptions() const noexcept
{
    return m_engineOptions;
}

bool SkyEphemerisSession::applyOptions(
    const skygate::ephemeris::EphemerisEngineKind::Type engineKind,
    const skygate::ephemeris::EphemerisEngineOptions& engineOptions
)
{
    if (m_engineKind == engineKind && m_engineOptions == engineOptions) {
        return false;
    }

    m_engineKind = engineKind;
    m_engineOptions = engineOptions;
    ++m_engineOptionsRevision;
    return true;
}

void SkyEphemerisSession::rebuild(const RebuildInputs& inputs, const CreationDiagnosticsHandler& diagnosticsHandler)
{
    const auto* selectedDescriptor = skygate::ephemeris::EphemerisEngineDescriptorRegistry::findByKind(m_engineKind);
    if (selectedDescriptor != nullptr && selectedDescriptor->requiresBackendResources) {
        m_backendResourceCache->refresh(inputs.activeDataSnapshot, inputs.dataManifest, inputs.ephemerisDataRevision);
    }

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = m_engineKind;
    if (inputs.starCatalog != nullptr) {
        request.catalog =
            std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(inputs.starCatalog->bodies());
    }
    request.options = m_engineOptions;
    request.options.setEngineKind(m_engineKind);
    request.datasetManifest = inputs.dataManifest != nullptr ? &inputs.dataManifest->dataSetInfo : m_datasetManifest;
    request.dataManifest = inputs.dataManifest;
    request.activeDataSnapshot = inputs.activeDataSnapshot;
    m_backendResourceCache->applyProviderFields(request);
    request.diagnosticsSink = m_diagnosticsSink;

    auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);
    diagnosticsHandler(result.diagnostics);

    const std::shared_ptr<const skygate::ephemeris::IEphemerisEngine> currentEngine = engineHandle();
    if (currentEngine != nullptr
        && skygate::ephemeris::EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
            m_engineKind, result, currentEngine->kind()
        )) {
        return;
    }
    if (result.engine != nullptr) {
        const std::lock_guard<std::mutex> lock(m_engineMutex);
        m_engine = std::move(result.engine);
    }
}

SkyEphemerisSession::RequestContext SkyEphemerisSession::requestContext(const RequestInputs& inputs) const
{
    RequestContext context;
    context.request.context = inputs.observationContext;
    context.request.options = m_engineOptions;
    context.request.options.setEngineKind(m_engineKind);
    context.activeDataSnapshot = inputs.activeDataSnapshot;
    context.engineOptionsRevision = m_engineOptionsRevision;
    context.ephemerisDataRevision = inputs.ephemerisDataRevision;
    context.earthOrientationDataRevision =
        textDataAssetRevision(context.activeDataSnapshot, context.ephemerisDataRevision, true);
    context.leapSecondDataRevision =
        textDataAssetRevision(context.activeDataSnapshot, context.ephemerisDataRevision, false);
    context.catalogRevision = inputs.catalogRevision;
    context.earthOrientationDataRange = activeTextAssetRange(
        inputs.dataManifest,
        skygate::ephemeris::EphemerisDataManifest::AssetKind::EarthOrientationData,
        inputs.installedEarthOrientationVersion
    );
    context.leapSecondTableRange = activeTextAssetRange(
        inputs.dataManifest,
        skygate::ephemeris::EphemerisDataManifest::AssetKind::LeapSecondTable,
        inputs.installedLeapSecondTableVersion
    );
    context.deltaTDataRange = activeTextAssetRange(
        inputs.dataManifest,
        skygate::ephemeris::EphemerisDataManifest::AssetKind::DeltaTData,
        inputs.installedDeltaTDataVersion
    );

    context.request.epoch = skygate::core::EpochCodec::epochFromUtcTimeExact(inputs.observationContext.utcTime);

    return context;
}
