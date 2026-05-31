#include "EphemerisFixtureSupport.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "time/CalendarTime.hpp"
#include "engine/highprecision/CalcephKernelProvider.hpp"
#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/EphemerisDataManifest.hpp"
#include "engine/highprecision/EphemerisDataSnapshot.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/IApparentPlaceCalculator.hpp"
#include "engine/highprecision/ICalcephKernel.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"
#include "engine/highprecision/TimeScaleService.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

constexpr int kNaifEarth = 399;
constexpr int kNaifMars = 499;
constexpr int kNaifSun = 10;
constexpr int kNaifSolarSystemBarycenter = 0;
constexpr double kCoordinateTolerance = 1.0e-9;
constexpr std::string_view kDe405sSha256 = "0e3793cca287b75ce33bf6155a8fef912d1114de63b7cf39eded66afc08e8f98";

[[nodiscard]] OwnGalaxyCelestialBody makeMarsBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "mars";
    body.displayName = "Mars";
    body.kind = BaseCelestialBody::Kind::Planet;
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makeJupiterBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "jupiter";
    body.displayName = "Jupiter";
    body.kind = BaseCelestialBody::Kind::Planet;
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makeUnsupportedBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "unsupported-test";
    body.displayName = "Unsupported Test";
    body.kind = BaseCelestialBody::Kind::Constellation;
    return body;
}

template <typename BodyRange>
[[nodiscard]] std::shared_ptr<const CelestialBodyCatalog> makeCatalogHandle(const BodyRange& bodies)
{
    return std::make_shared<const CelestialBodyCatalog>(std::span<const OwnGalaxyCelestialBody>{bodies});
}

[[nodiscard]] ObservationContext makeContext()
{
    ObservationContext context;
    context.utcTime = UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

[[nodiscard]] AstronomicalEpoch makeEpoch(const double julianDatePart1 = 2'460'310.0)
{
    return {
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tdb,
    };
}

[[nodiscard]] AstronomicalEpoch makeCivilEpoch(const int year, const int month, const int day)
{
    const std::optional<AstronomicalEpoch> epoch = CalendarTime::astronomicalEpochFromCivilDateTime(
        CivilDateTime{.astronomicalYear = year, .month = month, .day = day}
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

[[nodiscard]] EphemerisRequest makeRequest(const EphemerisCorrectionFlags correctionFlags)
{
    EphemerisRequest request;
    request.context = makeContext();
    request.epoch = makeEpoch();
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(correctionFlags);
    request.options.setEnableAtmosphericRefraction(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            correctionFlags, EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
    return request;
}

[[nodiscard]] EphemerisRequest
makeRequestForEpoch(const EphemerisCorrectionFlags correctionFlags, const AstronomicalEpoch epoch)
{
    EphemerisRequest request = makeRequest(correctionFlags);
    request.epoch = epoch;
    return request;
}

[[nodiscard]] EphemerisDateRange
makeRange(std::string id, std::string displayName, const double startJd, const double endJd)
{
    return {
        .id = std::move(id),
        .displayName = std::move(displayName),
        .start = {.julianDatePart1 = startJd, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = endJd, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    };
}

[[nodiscard]] EphemerisDatasetInfo makeDataSetInfo(const bool includeLongRange)
{
    EphemerisDatasetInfo info;
    info.id = includeLongRange ? "acceptance-with-de441" : "acceptance-modern";
    info.displayName = includeLongRange ? "Acceptance modern and DE441" : "Acceptance bundled modern";
    info.version = "2026a";
    info.provenance = "acceptance test data";
    info.dateRanges.push_back(makeRange("de440-modern", "Bundled modern range", 2'300'000.5, 2'700'000.5));
    if (includeLongRange) {
        info.dateRanges.push_back(
            makeRange("de441-long-range", "Optional DE441 long range", -3'100'000.5, 8'000'000.5)
        );
    }
    return info;
}

[[nodiscard]] EphemerisDataManifest makeFixtureDataManifest()
{
    EphemerisDataManifest manifest;
    manifest.dataSetInfo = makeDataSetInfo(true);
    manifest.profiles.push_back(
        EphemerisDataManifestProfile{
            .id = "de405s-modern",
            .displayName = "DE405s acceptance fixture",
            .bundled = true,
            .longRange = false,
            .assetIds = {"de405s-kernel"},
        }
    );
    manifest.profiles.push_back(
        EphemerisDataManifestProfile{
            .id = "de441-long-range",
            .displayName = "DE441 long-range acceptance profile",
            .bundled = false,
            .longRange = true,
            .assetIds = {"de441-kernel"},
        }
    );
    manifest.assets.push_back(
        EphemerisDataManifestAsset{
            .id = "de405s-kernel",
            .kind = EphemerisDataManifestAssetKind::SolarSystemKernel,
            .profileId = "de405s-modern",
            .version = "DE405s",
            .sourceUrl = "https://naif.jpl.nasa.gov/pub/naif/M01/kernels/spk/de405s.bsp",
            .relativePath = "ephemeris/kernels/de405s.bsp",
            .checksum = {.algorithm = "sha256", .value = std::string{kDe405sSha256}},
            .compression = {.kind = EphemerisDataManifestCompressionKind::None, .uncompressedSizeBytes = 1'426'432U},
            .validityRange = makeRange("de405s-modern-range", "DE405s fixture range", 2'451'544.5, 2'455'197.5),
            .optional = false,
        }
    );
    manifest.assets.push_back(
        EphemerisDataManifestAsset{
            .id = "de441-kernel",
            .kind = EphemerisDataManifestAssetKind::SolarSystemKernel,
            .profileId = "de441-long-range",
            .version = "DE441 fixture substitute",
            .sourceUrl = "https://ssd.jpl.nasa.gov/ftp/eph/planets/bsp/de441.bsp",
            .relativePath = "ephemeris/kernels/de405s.bsp",
            .checksum = {.algorithm = "sha256", .value = std::string{kDe405sSha256}},
            .compression = {.kind = EphemerisDataManifestCompressionKind::None, .uncompressedSizeBytes = 1'426'432U},
            .validityRange = makeRange("de441-long-range", "Optional DE441 long range", -3'100'000.5, 8'000'000.5),
            .optional = true,
        }
    );
    return manifest;
}

[[nodiscard]] EphemerisEngineOptions makeOptions(const EphemerisCorrectionFlags correctionFlags)
{
    EphemerisEngineOptions options;
    options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(correctionFlags);
    options.setEnableAtmosphericRefraction(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            correctionFlags, EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
    return options;
}

[[nodiscard]] bool nearlyEqual(const double lhs, const double rhs) noexcept
{
    return std::abs(lhs - rhs) <= kCoordinateTolerance;
}

class AcceptanceKernel final : public ICalcephKernel {
public:
    [[nodiscard]] Status status() const noexcept override
    {
        return Status::Ready;
    }

    [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept override
    {
        return m_diagnostics;
    }

    [[nodiscard]] const std::optional<Info>& kernelInfo() const noexcept override
    {
        return m_info;
    }

    [[nodiscard]] Status statusForEpoch(const AstronomicalEpoch&) const noexcept override
    {
        return Status::Ready;
    }

    [[nodiscard]] SolarSystemKernelStateResult
    compute(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const override
    {
        ++m_callCount;

        SolarSystemKernelStateResult result;
        result.metadata.dataSourceProvenance = "CALCEPH acceptance kernel";
        result.metadata.effectiveDataValidityRange =
            makeRange("de440-modern", "Bundled modern range", 2'300'000.5, 2'700'000.5);

        if (epoch.julianDatePart1 < 2'300'000.5 || epoch.julianDatePart1 > 2'700'000.5) {
            result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange;
            result.metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
            result.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
            result.positionAu = Vector3d{.x = 1.0, .y = 0.0, .z = 0.0};
            return result;
        }

        if (targetNaifId == kNaifMars && centerNaifId == kNaifEarth) {
            result.positionAu = Vector3d{.x = 1.0, .y = 1.0, .z = 0.1};
            return result;
        }

        if (targetNaifId == kNaifEarth && centerNaifId == kNaifSolarSystemBarycenter) {
            result.positionAu = Vector3d{.x = 0.5, .y = 0.0, .z = 0.0};
            result.velocityAuPerDay = Vector3d{.x = 0.0, .y = 0.01, .z = 0.0};
            return result;
        }

        if (targetNaifId == kNaifMars && centerNaifId == kNaifSolarSystemBarycenter) {
            result.positionAu = Vector3d{.x = 1.25, .y = 1.0, .z = 0.1};
            return result;
        }

        if (targetNaifId == kNaifSun && centerNaifId == kNaifEarth) {
            result.positionAu = Vector3d{.x = -1.0, .y = 0.0, .z = 0.0};
            return result;
        }

        result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Unsupported;
        result.metadata.addWarning(EphemerisEngineWarning::Code::UnsupportedBody);
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount = 0;
    std::vector<std::string> m_diagnostics;
    std::optional<Info> m_info;
};

class AcceptanceDataSnapshot final : public IEphemerisDataSnapshot {
public:
    AcceptanceDataSnapshot(std::string assetId, std::string profileId, std::string activePath)
        : m_assetId(std::move(assetId)), m_profileId(std::move(profileId)), m_activePath(std::move(activePath))
    {
    }

    [[nodiscard]] std::optional<EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<EphemerisKernelDataAsset>
    solarSystemKernelAsset(const std::string_view assetId) const override
    {
        if (assetId != m_assetId) {
            return std::nullopt;
        }

        return EphemerisKernelDataAsset{
            .id = m_assetId,
            .profileId = m_profileId,
            .version = "acceptance-fixture",
            .provenance = "NAIF DE405s acceptance fixture",
            .activePath = m_activePath,
        };
    }

private:
    std::string m_assetId;
    std::string m_profileId;
    std::string m_activePath;
};

class CountingSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;

        HighPrecisionCalculatorResult result;
        result.equatorial = EquatorialCoordinate{
            .rightAscensionHours = 3.0 + static_cast<double>(input.bodyIndex),
            .declinationDeg = -2.0 + static_cast<double>(input.bodyIndex),
        };
        result.metadata.dataSourceProvenance = "acceptance counting calculator";
        result.metadata.appliedCorrections = EphemerisCorrectionFlags::geometric();
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount = 0;
};

class AcceptanceTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        TimeScaleConversionResult result;
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        TimeScaleConversionResult result;
        const std::optional<AstronomicalEpoch> epoch = CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
        result.epoch = epoch.value_or(AstronomicalEpoch{});
        result.epoch.timeScale = targetScale;
        result.status = epoch.has_value() ? TimeScaleConversionStatus::Valid : TimeScaleConversionStatus::Failed;
        return result;
    }
};

class AcceptanceEarthOrientationProvider final : public IEarthOrientationProvider {
public:
    AcceptanceEarthOrientationProvider()
    {
        m_dataInfo.status = EarthOrientationDataStatus::Available;
        m_dataInfo.version = "acceptance-eop";
        m_dataInfo.provenance = "acceptance test";
        m_entries.push_back(
            EarthOrientationTableEntry{
                .effectiveUtcDate = {.astronomicalYear = 2024, .month = 1, .day = 1, .timeScale = TimeScale::Utc},
                .effectiveUtcEpoch = makeEpoch(),
                .ut1MinusUtcSeconds = 0.05,
                .polarMotionXArcseconds = 0.01,
                .polarMotionYArcseconds = -0.02,
            }
        );
    }

    [[nodiscard]] const EarthOrientationDataInfo& dataInfo() const noexcept override
    {
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const EarthOrientationTableEntry> entries() const noexcept override
    {
        return m_entries;
    }

private:
    EarthOrientationDataInfo m_dataInfo;
    std::vector<EarthOrientationTableEntry> m_entries;
};

class RecordingApparentPlaceCalculator final : public IApparentPlaceCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult apply(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override
    {
        ++m_callCount;
        m_lastRequestedCorrections = input.request.options.correctionFlags();
        m_sawTopocentricState =
            input.preparedRequestState != nullptr && input.preparedRequestState->topocentricStatePrepared;

        HighPrecisionCalculatorResult result = calculatorResult;
        result.metadata.appliedCorrections |= input.request.options.correctionFlags();
        if (skygate::ephemeris::EphemerisCorrectionFlags::has(
                input.request.options.correctionFlags(), EphemerisCorrectionFlags::diurnalParallax()
            )) {
            result.horizontal = HorizontalCoordinate{.altitudeDeg = 42.0, .azimuthDeg = 128.0};
        }
        return result;
    }

    [[nodiscard]] std::vector<StarAstrometryBatchResult> applyBatch(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> bodies,
        std::span<const StarAstrometryBatchResult> calculatorResults,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const override
    {
        std::vector<StarAstrometryBatchResult> results;
        results.reserve(calculatorResults.size());
        for (const StarAstrometryBatchResult& calculatorResult : calculatorResults) {
            if (calculatorResult.bodyIndex >= bodies.size() || bodies[calculatorResult.bodyIndex] == nullptr) {
                continue;
            }

            const HighPrecisionComputationInput input{
                .request = request,
                .body = *bodies[calculatorResult.bodyIndex],
                .preparedRequestState = preparedRequestState,
                .bodyIndex = calculatorResult.bodyIndex,
            };
            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = calculatorResult.bodyIndex,
                    .result = apply(input, calculatorResult.result),
                }
            );
        }
        return results;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastRequestedCorrections() const noexcept
    {
        return m_lastRequestedCorrections;
    }

    [[nodiscard]] bool sawTopocentricState() const noexcept
    {
        return m_sawTopocentricState;
    }

private:
    mutable int m_callCount = 0;
    mutable EphemerisCorrectionFlags m_lastRequestedCorrections = EphemerisCorrectionFlags::noCorrections();
    mutable bool m_sawTopocentricState = false;
};

class DegradedSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput&) const override
    {
        HighPrecisionCalculatorResult result;
        result.equatorial = EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
        result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange;
        result.metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
        result.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
        result.metadata.dataSourceProvenance = "missing DE441 long-range kernel; bundled modern fallback";
        result.metadata.effectiveDataValidityRange =
            makeRange("de440-modern", "Bundled modern range", 2'300'000.5, 2'700'000.5);
        return result;
    }
};

[[nodiscard]] HighPrecisionEphemerisEngine makeHighPrecisionEngine(
    std::vector<OwnGalaxyCelestialBody> bodies,
    EphemerisEngineOptions options,
    HighPrecisionEphemerisEngineDependencies dependencies
)
{
    if (dependencies.dataSetInfo.id.empty()) {
        dependencies.dataSetInfo = makeDataSetInfo(false);
    }
    return HighPrecisionEphemerisEngine(CelestialBodyCatalog(std::move(bodies)), options, std::move(dependencies));
}

}  // namespace

class EphemerisAcceptanceMatrixTests final : public QObject {
    Q_OBJECT

private slots:
    void factorySelectionStrictFailureAndFallbackRemainExplicit();
    void calcephProviderBackedSolarSystemRaDecSupportsCorrectionOptions();
    void realCalcephRuntimeComputesFixtureSolarSystemRaDecWhenAvailable();
    void correctionMatrixRoutesAstrometricApparentAndTopocentricRequests();
    void absentLongRangeKernelProducesDegradedFallbackMetadata();
    void bundledAndOptionalLongRangeDataSetProfilesDriveProviderSelection();
    void deterministicHorizonsFixturesRemainReadable();
    void fullFrameComputationCacheAvoidsPerObjectRecompute();
};

void EphemerisAcceptanceMatrixTests::factorySelectionStrictFailureAndFallbackRemainExplicit()
{
    const std::array bodies{makeMarsBody()};

    EphemerisEngineFactoryRequest simpleRequest;
    simpleRequest.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    simpleRequest.catalog = makeCatalogHandle(bodies);
    simpleRequest.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::Simple);
    const EphemerisEngineFactoryResult simpleResult = skygate::ephemeris::EphemerisEngineFactory::create(simpleRequest);
    QVERIFY(simpleResult.isSuccess());
    QVERIFY(simpleResult.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(simpleResult.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );

    EphemerisEngineFactoryRequest strictRequest;
    strictRequest.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    strictRequest.catalog = makeCatalogHandle(bodies);
    strictRequest.options = makeOptions(EphemerisCorrectionFlags::apparentTopocentric());
    strictRequest.fallbackPolicy = EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    const EphemerisEngineFactoryResult strictResult = skygate::ephemeris::EphemerisEngineFactory::create(strictRequest);
    QVERIFY(strictResult.isFailure());
    QVERIFY(strictResult.engine == nullptr);
    QVERIFY(strictResult.hasErrors());
    QVERIFY(!strictResult.diagnostics.empty());
    QVERIFY(!strictResult.diagnostics.front().displayText().empty());

    EphemerisEngineFactoryRequest fallbackRequest = strictRequest;
    fallbackRequest.fallbackPolicy = EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;
    const EphemerisEngineFactoryResult fallbackResult =
        skygate::ephemeris::EphemerisEngineFactory::create(fallbackRequest);
    QVERIFY(fallbackResult.isSuccess());
    QVERIFY(fallbackResult.usedSimpleEngineFallback());
    QVERIFY(fallbackResult.engine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(fallbackResult.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(fallbackResult.hasDiagnostics());
    QVERIFY(!fallbackResult.hasErrors());
}

void EphemerisAcceptanceMatrixTests::calcephProviderBackedSolarSystemRaDecSupportsCorrectionOptions()
{
    auto kernel = std::make_shared<AcceptanceKernel>();
    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.calcephKernel = kernel;
    dependencies.solarSystemStateCalculator = std::make_shared<SolarSystemStateCalculator>(kernel);
    dependencies.apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    dependencies.dataSetInfo = makeDataSetInfo(false);

    const HighPrecisionEphemerisEngine engine =
        makeHighPrecisionEngine({makeMarsBody()}, makeOptions(EphemerisCorrectionFlags::lightTime()), dependencies);

    const auto geometricState = engine.computeBodyState(makeRequest(EphemerisCorrectionFlags::geometric()), "mars");
    QVERIFY(geometricState.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(geometricState->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(geometricState->metadata.dataSourceProvenance, std::string{"CALCEPH acceptance kernel"});

    const auto lightTimeState = engine.computeBodyState(makeRequest(EphemerisCorrectionFlags::lightTime()), "mars");
    QVERIFY(lightTimeState.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(lightTimeState->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            lightTimeState->metadata.requestedCorrections, EphemerisCorrectionFlags::lightTime()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            lightTimeState->metadata.appliedCorrections, EphemerisCorrectionFlags::lightTime()
        )
    );
    QVERIFY(
        !nearlyEqual(geometricState->equatorial.rightAscensionHours, lightTimeState->equatorial.rightAscensionHours)
    );
    QVERIFY(kernel->callCount() > 1);
}

void EphemerisAcceptanceMatrixTests::realCalcephRuntimeComputesFixtureSolarSystemRaDecWhenAvailable()
{
    const std::string kernelPath = std::string{SKYGATE_EPHEMERIS_TESTDATA_DIR} + "/ephemeris/kernels/de405s.bsp";
    const EphemerisDataManifest manifest = makeFixtureDataManifest();
    AcceptanceDataSnapshot snapshot("de405s-kernel", "de405s-modern", kernelPath);

    auto provider = std::make_shared<CalcephKernelProvider>(snapshot, manifest);
    const std::shared_ptr<const ICalcephKernel> kernel = provider->openKernel();
    if (kernel->status() == ICalcephKernel::Status::CalcephUnavailable) {
        QSKIP("Real CALCEPH provider acceptance row requires SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON.");
    }
    const QByteArray diagnostics =
        kernel->diagnostics().empty() ? QByteArray{} : QByteArray(kernel->diagnostics().front().c_str());
    if (kernel->status() == ICalcephKernel::Status::OpenFailed && diagnostics.contains("CALCEPH failed to open")) {
        QSKIP("Real CALCEPH provider acceptance row requires a kernel format supported by the linked CALCEPH.");
    }
    QVERIFY2(kernel->status() == ICalcephKernel::Status::Ready, diagnostics.constData());

    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.calcephKernel = kernel;
    dependencies.solarSystemStateCalculator = std::make_shared<SolarSystemStateCalculator>(kernel);
    dependencies.dataSetInfo = manifest.dataSetInfo;
    const HighPrecisionEphemerisEngine engine =
        makeHighPrecisionEngine({makeMarsBody()}, makeOptions(EphemerisCorrectionFlags::geometric()), dependencies);

    AstronomicalEpoch epoch = makeCivilEpoch(2004, 1, 1);
    epoch.timeScale = TimeScale::Tdb;
    const auto state =
        engine.computeBodyState(makeRequestForEpoch(EphemerisCorrectionFlags::geometric(), epoch), "mars");

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(state->metadata.dataSourceProvenance, std::string{"NAIF DE405s acceptance fixture"});
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
    QCOMPARE(
        static_cast<std::uint32_t>(state->metadata.requestedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::geometric())
    );
}

void EphemerisAcceptanceMatrixTests::correctionMatrixRoutesAstrometricApparentAndTopocentricRequests()
{
    const std::array matrix{
        EphemerisCorrectionFlags::geometric(),
        EphemerisCorrectionFlags::astrometric(),
        EphemerisCorrectionFlags::apparent(),
        EphemerisCorrectionFlags::apparentTopocentric(),
    };

    for (const EphemerisCorrectionFlags correctionFlags : matrix) {
        auto kernel = std::make_shared<AcceptanceKernel>();
        auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
        HighPrecisionEphemerisEngineDependencies dependencies;
        dependencies.calcephKernel = kernel;
        dependencies.solarSystemStateCalculator = std::make_shared<SolarSystemStateCalculator>(kernel);
        dependencies.timeScaleService = std::make_shared<AcceptanceTimeScaleService>();
        dependencies.earthOrientationProvider = std::make_shared<AcceptanceEarthOrientationProvider>();
        dependencies.apparentPlaceCalculator = apparentPlaceCalculator;
        dependencies.dataSetInfo = makeDataSetInfo(false);
        const HighPrecisionEphemerisEngine engine =
            makeHighPrecisionEngine({makeMarsBody()}, makeOptions(correctionFlags), dependencies);

        const auto state = engine.computeBodyState(makeRequest(correctionFlags), "mars");
        QVERIFY(state.has_value());
        QCOMPARE(
            static_cast<std::uint32_t>(state->metadata.requestedCorrections),
            static_cast<std::uint32_t>(correctionFlags)
        );
        QVERIFY(
            static_cast<std::uint32_t>(state->metadata.appliedCorrections)
            == static_cast<std::uint32_t>(correctionFlags)
        );

        if (correctionFlags == EphemerisCorrectionFlags::geometric()) {
            QCOMPARE(apparentPlaceCalculator->callCount(), 0);
        } else {
            QCOMPARE(apparentPlaceCalculator->callCount(), 1);
            QCOMPARE(
                static_cast<std::uint32_t>(apparentPlaceCalculator->lastRequestedCorrections()),
                static_cast<std::uint32_t>(correctionFlags)
            );
        }

        if (correctionFlags == EphemerisCorrectionFlags::apparentTopocentric()) {
            QVERIFY(apparentPlaceCalculator->sawTopocentricState());
            QVERIFY(std::isfinite(state->horizontal.altitudeDeg));
            QVERIFY(std::isfinite(state->horizontal.azimuthDeg));
        }
    }
}

void EphemerisAcceptanceMatrixTests::absentLongRangeKernelProducesDegradedFallbackMetadata()
{
    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.solarSystemStateCalculator = std::make_shared<DegradedSolarSystemCalculator>();
    dependencies.dataSetInfo = makeDataSetInfo(false);

    const HighPrecisionEphemerisEngine engine =
        makeHighPrecisionEngine({makeMarsBody()}, makeOptions(EphemerisCorrectionFlags::geometric()), dependencies);

    EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::geometric());
    request.epoch = makeEpoch(-1'000'000.5);

    const auto state = engine.computeBodyState(request, "mars");
    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::MissingEphemerisData));
    QCOMPARE(
        state->metadata.dataSourceProvenance, std::string{"missing DE441 long-range kernel; bundled modern fallback"}
    );
    QVERIFY(state->metadata.effectiveDataValidityRange.has_value());
    QCOMPARE(state->metadata.effectiveDataValidityRange->id, std::string{"de440-modern"});
}

void EphemerisAcceptanceMatrixTests::bundledAndOptionalLongRangeDataSetProfilesDriveProviderSelection()
{
    const std::string kernelPath = std::string{SKYGATE_EPHEMERIS_TESTDATA_DIR} + "/ephemeris/kernels/de405s.bsp";
    const EphemerisDataManifest manifest = makeFixtureDataManifest();
    AcceptanceDataSnapshot bundledSnapshot("de405s-kernel", "de405s-modern", kernelPath);
    auto bundledKernelProvider = std::make_shared<CalcephKernelProvider>(bundledSnapshot, manifest);
    const std::shared_ptr<const ICalcephKernel> bundledKernel = bundledKernelProvider->openKernel();
    if (bundledKernel->status() == ICalcephKernel::Status::CalcephUnavailable) {
        QSKIP("Provider-selection acceptance row requires SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON.");
    }
    const QByteArray bundledDiagnostics =
        bundledKernel->diagnostics().empty() ? QByteArray{} : QByteArray(bundledKernel->diagnostics().front().c_str());
    if (bundledKernel->status() == ICalcephKernel::Status::OpenFailed
        && bundledDiagnostics.contains("CALCEPH failed to open")) {
        QSKIP("Provider-selection acceptance row requires a kernel format supported by the linked CALCEPH.");
    }

    HighPrecisionEphemerisEngineDependencies bundledDependencies;
    bundledDependencies.calcephKernel = bundledKernel;
    bundledDependencies.solarSystemStateCalculator = std::make_shared<SolarSystemStateCalculator>(bundledKernel);
    bundledDependencies.dataSetInfo = manifest.dataSetInfo;
    const HighPrecisionEphemerisEngine bundledEngine = makeHighPrecisionEngine(
        {makeMarsBody()}, makeOptions(EphemerisCorrectionFlags::geometric()), bundledDependencies
    );

    QVERIFY(bundledKernel->status() == ICalcephKernel::Status::Ready);
    QVERIFY(bundledKernel->kernelInfo().has_value());
    QCOMPARE(bundledKernel->kernelInfo()->id, std::string{"de405s-kernel"});
    QCOMPARE(bundledKernel->kernelInfo()->profileId, std::string{"de405s-modern"});
    QVERIFY(!bundledKernel->kernelInfo()->longRange);

    AcceptanceDataSnapshot longRangeSnapshot("de441-kernel", "de441-long-range", kernelPath);
    CalcephKernelProvider::Options selectionOptions;
    selectionOptions.preferLongRange = true;
    auto longRangeKernelProvider =
        std::make_shared<CalcephKernelProvider>(longRangeSnapshot, manifest, selectionOptions);
    const std::shared_ptr<const ICalcephKernel> longRangeKernel = longRangeKernelProvider->openKernel();
    HighPrecisionEphemerisEngineDependencies longRangeDependencies;
    longRangeDependencies.calcephKernel = longRangeKernel;
    longRangeDependencies.solarSystemStateCalculator = std::make_shared<SolarSystemStateCalculator>(longRangeKernel);
    longRangeDependencies.dataSetInfo = manifest.dataSetInfo;
    const HighPrecisionEphemerisEngine longRangeEngine = makeHighPrecisionEngine(
        {makeMarsBody()}, makeOptions(EphemerisCorrectionFlags::geometric()), longRangeDependencies
    );

    QVERIFY(longRangeKernel->status() == ICalcephKernel::Status::Ready);
    QVERIFY(longRangeKernel->kernelInfo().has_value());
    QCOMPARE(longRangeKernel->kernelInfo()->id, std::string{"de441-kernel"});
    QCOMPARE(longRangeKernel->kernelInfo()->profileId, std::string{"de441-long-range"});
    QVERIFY(longRangeKernel->kernelInfo()->longRange);
    QVERIFY(longRangeKernel->kernelInfo()->optional);

    QCOMPARE(longRangeEngine.dataSetInfo().id, std::string{"acceptance-with-de441"});
    QCOMPARE(longRangeEngine.supportedDateRanges().size(), std::size_t{2});
    QCOMPARE(longRangeEngine.supportedDateRanges()[1].id, std::string{"de441-long-range"});
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            longRangeEngine.capabilities(), skygate::ephemeris::EphemerisCapabilities::extendedHistoricalRange()
        )
    );

    AstronomicalEpoch epoch = makeCivilEpoch(2004, 1, 1);
    epoch.timeScale = TimeScale::Tdb;
    const auto state =
        longRangeEngine.computeBodyState(makeRequestForEpoch(EphemerisCorrectionFlags::geometric(), epoch), "mars");
    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
}

void EphemerisAcceptanceMatrixTests::deterministicHorizonsFixturesRemainReadable()
{
    QString errorText;
    const auto fixture = skygate::ephemeris::tests::loadRaDecFixture(
        QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR)
            + QStringLiteral("/ephemeris/apparent_solar_system_mars_smoke.json"),
        &errorText
    );

    QVERIFY2(fixture.has_value(), qPrintable(errorText));
    QCOMPARE(fixture->metadata.source, QStringLiteral("JPL Horizons"));
    QVERIFY(fixture->metadata.apiParameters.contains(QStringLiteral("COMMAND='499'")));
    QVERIFY(fixture->metadata.target.contains(QStringLiteral("Mars")));
    QVERIFY(skygate::ephemeris::tests::hasFiniteCoordinates(fixture->expected));
    QVERIFY(fixture->toleranceDegrees > 0.0);
}

void EphemerisAcceptanceMatrixTests::fullFrameComputationCacheAvoidsPerObjectRecompute()
{
    auto calculator = std::make_shared<CountingSolarSystemCalculator>();
    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.solarSystemStateCalculator = calculator;
    dependencies.computationCache = std::make_shared<EphemerisComputationCache>();
    dependencies.dataSetInfo = makeDataSetInfo(false);

    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(
        {makeMarsBody(), makeJupiterBody(), makeUnsupportedBody()},
        makeOptions(EphemerisCorrectionFlags::geometric()),
        dependencies
    );

    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::geometric());
    const EphemerisSnapshot firstSnapshot = engine.compute(request);
    QCOMPARE(firstSnapshot.states.size(), std::size_t{3});
    QCOMPARE(calculator->callCount(), 2);

    const EphemerisSnapshot cachedSnapshot = engine.compute(request);
    QCOMPARE(cachedSnapshot.states.size(), std::size_t{3});
    QCOMPARE(calculator->callCount(), 2);

    EphemerisRequest changedOptionsRequest = request;
    changedOptionsRequest.options.setCorrectionFlags(EphemerisCorrectionFlags::lightTime());
    const EphemerisSnapshot changedOptionsSnapshot = engine.compute(changedOptionsRequest);
    QCOMPARE(changedOptionsSnapshot.states.size(), std::size_t{3});
    QCOMPARE(calculator->callCount(), 4);
}

QTEST_APPLESS_MAIN(EphemerisAcceptanceMatrixTests)

#include "EphemerisAcceptanceMatrixTests.moc"
