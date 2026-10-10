#include "HighPrecisionEngineFixture.hpp"

#include "UtcTimePoint.hpp"
#include "engine/EarthOrientationDataLoader.hpp"
#include "engine/EphemerisDatasetInfo.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "engine/EphemerisTextDataAsset.hpp"
#include "engine/highprecision/ICalcephKernel.hpp"

#include <QtGlobal>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::tests {

using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

namespace {

[[nodiscard]] EphemerisTextDataAsset makeEarthOrientationAsset()
{
    return {
        .id = "earth-orientation",
        .version = "test-eop",
        .provenance = "unit test",
        .content =
            "#@ version test-eop\n"
            "#@ source unit test\n"
            "#@ expires 2026-07-01\n"
            "#@ prediction_start 2026-05-01\n"
            "#@ prediction_end 2026-06-30\n"
            "effective_utc_date,ut1_minus_utc_seconds,polar_motion_x_arcseconds,polar_motion_y_arcseconds,predicted\n"
            "2026-04-01,0.10,0.0,0.0,false\n"
            "2026-04-11,0.30,0.0,0.0,false\n"
            "2026-05-01,0.50,0.0,0.0,true\n",
    };
}

}  // namespace

OwnGalaxyCelestialBody HighPrecisionEngineFixture::sunBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "sun";
    body.displayName = "Sun";
    body.kind = BaseCelestialBody::Kind::Sun;
    return body;
}

OwnGalaxyCelestialBody HighPrecisionEngineFixture::starBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "vega";
    body.displayName = "Vega";
    body.kind = BaseCelestialBody::Kind::Star;
    return body;
}

OwnGalaxyCelestialBody
HighPrecisionEngineFixture::fixedStarBody(std::string id, const double rightAscensionHours, const double declinationDeg)
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = body.id;
    body.kind = BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = declinationDeg,
    };
    return body;
}

CelestialBodyCatalog HighPrecisionEngineFixture::catalog(std::span<const OwnGalaxyCelestialBody> bodies)
{
    return CelestialBodyCatalog(bodies);
}

skygate::core::ObservationContext HighPrecisionEngineFixture::context()
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

EphemerisRequest HighPrecisionEngineFixture::request()
{
    EphemerisRequest request;
    request.context = context();
    request.epoch = {
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = skygate::core::TimeScale::Tdb,
    };
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::apparent());
    return request;
}

HighPrecisionEphemerisEngine::Dependencies HighPrecisionEngineFixture::dependencies(
    std::shared_ptr<ISolarSystemStateCalculator> solarSystemCalculator,
    std::shared_ptr<IStarAstrometryCalculator> starAstrometryCalculator,
    std::shared_ptr<IApparentPlaceCalculator> apparentPlaceCalculator,
    std::shared_ptr<IEphemerisResultBuilder> resultBuilder,
    std::shared_ptr<IEphemerisComputationCache> computationCache
)
{
    HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.solarSystemStateCalculator = std::move(solarSystemCalculator);
    dependencies.starAstrometryCalculator = std::move(starAstrometryCalculator);
    dependencies.apparentPlaceCalculator = std::move(apparentPlaceCalculator);
    dependencies.resultBuilder = std::move(resultBuilder);
    dependencies.computationCache = std::move(computationCache);
    dependencies.dataSetInfo.id = "test-data";
    dependencies.dataSetInfo.displayName = "Test data";
    dependencies.dataSetInfo.version = "fixture";
    dependencies.dataSetInfo.provenance = "unit test";
    dependencies.dataSetInfo.dateRanges.push_back(
        EphemerisDateRange{
            .id = "modern",
            .displayName = "Modern",
            .start =
                {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
            .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
        }
    );
    return dependencies;
}

HighPrecisionCalculatorResult HighPrecisionEngineFixture::calculatorResult(
    const HighPrecisionComputationInput& input,
    const double rightAscensionHours,
    const double declinationDeg,
    const std::string_view provenance
)
{
    HighPrecisionCalculatorResult result;
    result.equatorial = EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = declinationDeg,
    };
    result.metadata.dataSourceProvenance = provenance;
    result.metadata.appliedCorrections = input.request.options.correctionFlags();
    return result;
}

std::shared_ptr<const IEarthOrientationProvider> HighPrecisionEngineFixture::earthOrientationProvider(
    const std::optional<skygate::core::AstronomicalEpoch>& referenceEpoch
)
{
    EarthOrientationDataLoader::Options options;
    options.referenceEpoch = referenceEpoch;
    const EarthOrientationDataLoader::Result result =
        EarthOrientationDataLoader::loadFromTextAsset(makeEarthOrientationAsset(), options);
    Q_ASSERT(result.isSuccess());
    return result.provider;
}

std::shared_ptr<TestCalcephKernel> HighPrecisionEngineFixture::longRangeFallbackKernel()
{
    ICalcephKernel::StateResult result;
    result.positionAu = Vector3d{.x = 0.5, .y = 1.0, .z = 0.25};
    result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange;
    result.metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    result.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
    result.metadata.dataSourceProvenance = "missing DE441 long-range kernel; modern kernel fallback";
    result.metadata.effectiveDataValidityRange = EphemerisDateRange{
        .id = "de440-modern",
        .displayName = "DE440 modern range",
        .start = {.julianDatePart1 = 2'300'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'700'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
    };
    result.metadata.estimatedAngularUncertaintyArcsec = 3600.0;

    auto kernel = std::make_shared<TestCalcephKernel>();
    kernel->setDefaultResult(std::move(result));
    return kernel;
}

}  // namespace skygate::ephemeris::tests
