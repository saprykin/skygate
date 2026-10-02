#include "CelestialBodyCatalog.hpp"
#include "EphemerisFixtureSupport.hpp"
#include "EphemerisRequest.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/CalcephKernelProvider.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/EphemerisTextDataAsset.hpp"
#include "engine/highprecision/ErfaFrameTransformer.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/ICalcephKernel.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/LeapSecondTableLoader.hpp"
#include "engine/LeapSecondTimeScaleService.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using skygate::ephemeris::tests::angularSeparationDegrees;
using skygate::ephemeris::tests::EphemerisRaDecExpectation;

constexpr std::string_view kDe405sSha256 = "0e3793cca287b75ce33bf6155a8fef912d1114de63b7cf39eded66afc08e8f98";
constexpr double kHoursToDegrees = 15.0;
constexpr double kArcsecondsToDegrees = 1.0 / 3600.0;

struct HorizonsReferenceRow {
    std::string_view bodyId;
    double julianDateUtc = 0.0;
    double astrometricRaDeg = 0.0;
    double astrometricDecDeg = 0.0;
    double apparentRaDeg = 0.0;
    double apparentDecDeg = 0.0;
};

// Independent JPL Horizons observer rows (CENTER='500@399', QUANTITIES='1,2',
// airless apparent). The observer TLIST column is JD_UTC; the engine converts
// the UTC epoch to TDB/TT internally through LeapSecondTimeScaleService.
constexpr std::array kReferenceRows{
    HorizonsReferenceRow{
        .bodyId = "mars",
        .julianDateUtc = 2454833.5,
        .astrometricRaDeg = 274.545702086,
        .astrometricDecDeg = -24.081239344,
        .apparentRaDeg = 274.681188309,
        .apparentDecDeg = -24.078818451,
    },
    HorizonsReferenceRow{
        .bodyId = "mars",
        .julianDateUtc = 2454600.5,
        .astrometricRaDeg = 124.788182127,
        .astrometricDecDeg = 21.334389051,
        .apparentRaDeg = 124.911347246,
        .apparentDecDeg = 21.309460512,
    },
    HorizonsReferenceRow{
        .bodyId = "saturn",
        .julianDateUtc = 2454833.5,
        .astrometricRaDeg = 173.128432989,
        .astrometricDecDeg = 5.204602741,
        .apparentRaDeg = 173.249621757,
        .apparentDecDeg = 5.152593655,
    },
};

[[nodiscard]] OwnGalaxyCelestialBody makeBody(const std::string_view id)
{
    OwnGalaxyCelestialBody body;
    body.id = std::string{id};
    body.displayName = std::string{id};
    body.kind = BaseCelestialBody::Kind::Planet;
    return body;
}

[[nodiscard]] EphemerisEngineOptions makeEngineOptions()
{
    EphemerisEngineOptions options;
    options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    return options;
}

[[nodiscard]] EphemerisRequest
makeRequest(const EphemerisCorrectionFlags correctionFlags, const skygate::core::AstronomicalEpoch& epoch)
{
    EphemerisRequest request;
    request.epoch = epoch;
    request.options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(correctionFlags);
    request.options.setEnableAtmosphericRefraction(false);
    return request;
}

[[nodiscard]] EphemerisDateRange
makeRange(std::string id, std::string displayName, const double startJd, const double endJd)
{
    return {
        .id = std::move(id),
        .displayName = std::move(displayName),
        .start = {.julianDatePart1 = startJd, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
        .end = {.julianDatePart1 = endJd, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
    };
}

[[nodiscard]] EphemerisDataManifest makeManifest()
{
    EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "real-kernel-horizons";
    manifest.dataSetInfo.displayName = "Real kernel Horizons acceptance";
    manifest.dataSetInfo.version = "DE405s";
    manifest.dataSetInfo.provenance = "NAIF DE405s kernel";
    manifest.dataSetInfo.dateRanges.push_back(
        makeRange("de405s-modern-range", "DE405s modern range", 2'451'544.5, 2'455'197.5)
    );
    manifest.profiles.push_back(
        EphemerisDataManifest::Profile{
            .id = "de405s-modern",
            .displayName = "DE405s modern",
            .bundled = true,
            .longRange = false,
            .assetIds = {"de405s-kernel"},
        }
    );
    manifest.assets.push_back(
        EphemerisDataManifest::Asset{
            .id = "de405s-kernel",
            .kind = EphemerisDataManifest::AssetKind::SolarSystemKernel,
            .profileId = "de405s-modern",
            .version = "DE405s",
            .sourceUrl = "https://naif.jpl.nasa.gov/pub/naif/M01/kernels/spk/de405s.bsp",
            .relativePath = "ephemeris/kernels/de405s.bsp",
            .checksum = {.algorithm = "sha256", .value = std::string{kDe405sSha256}},
            .compression = {.kind = EphemerisDataManifest::CompressionKind::None, .uncompressedSizeBytes = 1'426'432U},
            .validityRange = makeRange("de405s-modern-range", "DE405s modern range", 2'451'544.5, 2'455'197.5),
            .optional = false,
        }
    );
    return manifest;
}

class KernelSnapshot final : public IEphemerisDataSnapshot {
public:
    explicit KernelSnapshot(std::string activePath) : m_activePath(std::move(activePath)) {}

    [[nodiscard]] std::optional<EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<EphemerisKernelDataAsset>
    solarSystemKernelAsset(const std::string_view assetId) const override
    {
        if (assetId != "de405s-kernel") {
            return std::nullopt;
        }

        return EphemerisKernelDataAsset{
            .id = "de405s-kernel",
            .profileId = "de405s-modern",
            .version = "DE405s",
            .provenance = "NAIF DE405s acceptance fixture",
            .activePath = m_activePath,
        };
    }

private:
    std::string m_activePath;
};

[[nodiscard]] std::shared_ptr<const ILeapSecondProvider> makeLeapSecondProvider()
{
    const EphemerisTextDataAsset asset{
        .id = "leap-seconds",
        .version = "test",
        .provenance = "IERS Bulletin C",
        .content = "#@ version test\n"
                   "#@ source IERS Bulletin C\n"
                   "#@ expires 2027-01-01\n"
                   "effective_utc_date,tai_minus_utc\n"
                   "1972-01-01,10\n"
                   "1972-07-01,11\n"
                   "2006-01-01,33\n"
                   "2009-01-01,34\n"
                   "2017-01-01,37\n",
    };

    const LeapSecondTableLoader::Result result = LeapSecondTableLoader::loadFromTextAsset(asset);
    return result.provider;
}

[[nodiscard]] EphemerisRaDecExpectation toExpectation(const double raDeg, const double decDeg)
{
    return {
        .rightAscensionHours = raDeg / kHoursToDegrees,
        .declinationDegrees = decDeg,
    };
}

}  // namespace

class RealKernelHorizonsAcceptanceTests final : public QObject {
    Q_OBJECT

private slots:
    void matchesIndependentJplHorizonsReferenceRows();
};

void RealKernelHorizonsAcceptanceTests::matchesIndependentJplHorizonsReferenceRows()
{
    const std::string kernelPath = std::string{SKYGATE_EPHEMERIS_TESTDATA_DIR} + "/ephemeris/kernels/de405s.bsp";
    const EphemerisDataManifest manifest = makeManifest();
    KernelSnapshot snapshot(kernelPath);
    auto provider = std::make_shared<CalcephKernelProvider>(snapshot, manifest);
    const std::shared_ptr<const ICalcephKernel> kernel = provider->openKernel();

    if (kernel->status() == ICalcephKernel::Status::CalcephUnavailable) {
        QSKIP("Real-kernel Horizons acceptance requires SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON.");
    }
    if (kernel->status() == ICalcephKernel::Status::MissingKernelFile) {
        QSKIP("Real-kernel Horizons acceptance requires the de405s.bsp kernel fixture.");
    }
    if (kernel->status() == ICalcephKernel::Status::OpenFailed) {
        QSKIP("Real-kernel Horizons acceptance requires a kernel format supported by the linked CALCEPH.");
    }
    if (kernel->status() != ICalcephKernel::Status::Ready) {
        QSKIP("Real-kernel Horizons acceptance is unavailable in this build/environment.");
    }

    const std::shared_ptr<const ILeapSecondProvider> leapSecondProvider = makeLeapSecondProvider();
    QVERIFY2(leapSecondProvider != nullptr, "Leap-second table for the real-kernel acceptance row failed to load.");
    std::shared_ptr<const ITimeScaleService> timeScaleService =
        std::make_shared<LeapSecondTimeScaleService>(leapSecondProvider);
    std::shared_ptr<const IFrameTransformer> frameTransformer =
        std::make_shared<ErfaFrameTransformer>(timeScaleService);
    auto apparentPlaceCalculator = std::make_shared<ApparentPlaceCalculator>(
        frameTransformer, timeScaleService, std::shared_ptr<const IEarthOrientationProvider>{}
    );

    HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.calcephKernel = kernel;
    dependencies.solarSystemStateCalculator = std::make_shared<SolarSystemStateCalculator>(kernel);
    dependencies.timeScaleService = timeScaleService;
    dependencies.apparentPlaceCalculator = apparentPlaceCalculator;
    dependencies.dataSetInfo = manifest.dataSetInfo;

    std::vector<OwnGalaxyCelestialBody> bodies;
    bodies.push_back(makeBody("mars"));
    bodies.push_back(makeBody("saturn"));
    const HighPrecisionEphemerisEngine engine(
        CelestialBodyCatalog(std::move(bodies)), makeEngineOptions(), std::move(dependencies)
    );

    constexpr double kAstrometricToleranceDeg = 0.3 * kArcsecondsToDegrees;
    constexpr double kApparentToleranceDeg = 2.0 * kArcsecondsToDegrees;

    for (const HorizonsReferenceRow& row : kReferenceRows) {
        const skygate::core::AstronomicalEpoch epoch{
            .julianDatePart1 = row.julianDateUtc,
            .julianDatePart2 = 0.0,
            .timeScale = skygate::core::TimeScale::Utc,
        };

        const auto astrometricState =
            engine.computeBodyState(makeRequest(EphemerisCorrectionFlags::lightTime(), epoch), row.bodyId);
        QVERIFY2(astrometricState.has_value(), qPrintable(QString::fromStdString(std::string{row.bodyId})));
        QVERIFY2(
            astrometricState->metadata.isSuccessful(), qPrintable(QString::fromStdString(std::string{row.bodyId}))
        );
        QVERIFY2(
            std::isfinite(astrometricState->equatorial.rightAscensionHours)
                && std::isfinite(astrometricState->equatorial.declinationDeg),
            qPrintable(QString::fromStdString(std::string{row.bodyId}))
        );
        const EphemerisRaDecExpectation actualAstrometric{
            .rightAscensionHours = astrometricState->equatorial.rightAscensionHours,
            .declinationDegrees = astrometricState->equatorial.declinationDeg,
        };
        const double astrometricErrorDeg =
            angularSeparationDegrees(actualAstrometric, toExpectation(row.astrometricRaDeg, row.astrometricDecDeg));
        QVERIFY2(
            astrometricErrorDeg <= kAstrometricToleranceDeg,
            qPrintable(QStringLiteral("astrometric angular error %1 deg exceeds tolerance %2 deg")
                           .arg(astrometricErrorDeg, 0, 'g', 12)
                           .arg(kAstrometricToleranceDeg, 0, 'g', 12))
        );

        const auto apparentState =
            engine.computeBodyState(makeRequest(EphemerisCorrectionFlags::apparent(), epoch), row.bodyId);
        QVERIFY2(apparentState.has_value(), qPrintable(QString::fromStdString(std::string{row.bodyId})));
        QVERIFY2(apparentState->metadata.isSuccessful(), qPrintable(QString::fromStdString(std::string{row.bodyId})));
        QVERIFY2(
            std::isfinite(apparentState->equatorial.rightAscensionHours)
                && std::isfinite(apparentState->equatorial.declinationDeg),
            qPrintable(QString::fromStdString(std::string{row.bodyId}))
        );
        const EphemerisRaDecExpectation actualApparent{
            .rightAscensionHours = apparentState->equatorial.rightAscensionHours,
            .declinationDegrees = apparentState->equatorial.declinationDeg,
        };
        const double apparentErrorDeg =
            angularSeparationDegrees(actualApparent, toExpectation(row.apparentRaDeg, row.apparentDecDeg));
        QVERIFY2(
            apparentErrorDeg <= kApparentToleranceDeg,
            qPrintable(QStringLiteral("apparent angular error %1 deg exceeds tolerance %2 deg")
                           .arg(apparentErrorDeg, 0, 'g', 12)
                           .arg(kApparentToleranceDeg, 0, 'g', 12))
        );
    }
}

QTEST_APPLESS_MAIN(RealKernelHorizonsAcceptanceTests)

#include "RealKernelHorizonsAcceptanceTests.moc"
