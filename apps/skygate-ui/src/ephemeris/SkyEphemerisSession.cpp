#include "SkyEphemerisSession.hpp"

#include "SkyQtTimeCodec.hpp"
#include "UtcTimeCodec.hpp"
#include "composition/EphemerisBackendResourceCache.hpp"
#include "composition/EphemerisGuidanceComposition.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/EphemerisEngineDescriptorRegistry.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "factory/EphemerisEngineFactoryRequest.hpp"
#include "factory/EphemerisEngineReplacementPolicy.hpp"
#include "time/CalendarTime.hpp"

#include <QDateTime>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

[[nodiscard]] qint64 floorMod(const qint64 numerator, const qint64 denominator) noexcept
{
    qint64 remainder = numerator % denominator;
    if (remainder < 0) {
        remainder += denominator;
    }
    return remainder;
}

[[nodiscard]] std::optional<skygate::core::AstronomicalEpoch>
astronomicalEpochFromUtcTime(const skygate::core::UtcTimePoint& utcTime) noexcept
{
    const QDateTime normalizedUtcDateTime = SkyQtTimeCodec::toQDateTimeUtc(utcTime);
    const QDate date = normalizedUtcDateTime.date();
    const QTime time = normalizedUtcDateTime.time();
    const auto astronomicalYear = skygate::core::CalendarTime::astronomicalYearFromHistoricalYear(date.year());
    if (!astronomicalYear.has_value()) {
        return std::nullopt;
    }

    return skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = *astronomicalYear,
            .month = date.month(),
            .day = date.day(),
            .hour = time.hour(),
            .minute = time.minute(),
            .second = time.second(),
            .nanosecond = static_cast<std::uint32_t>(floorMod(
                              static_cast<qint64>(skygate::core::UtcTimeCodec::toEpochMicros(utcTime)), 1'000'000
                          ))
                          * 1000U,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
}

void appendRevisionComponent(std::uint64_t& revision, const std::string_view value) noexcept
{
    constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ULL;
    for (const char character : value) {
        revision ^= static_cast<unsigned char>(character);
        revision *= kFnvPrime;
    }
    revision ^= 0xffU;
    revision *= kFnvPrime;
}

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

    constexpr std::uint64_t kFnvOffsetBasis = 14'695'981'039'346'656'037ULL;
    std::uint64_t revision = kFnvOffsetBasis;
    revision ^= dataRevision;
    revision *= 1'099'511'628'211ULL;
    appendRevisionComponent(revision, asset->id);
    appendRevisionComponent(revision, asset->version);
    appendRevisionComponent(revision, asset->provenance);
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

    if (const auto epoch = astronomicalEpochFromUtcTime(inputs.observationContext.utcTime); epoch.has_value()) {
        context.request.epoch = *epoch;
    }

    return context;
}
