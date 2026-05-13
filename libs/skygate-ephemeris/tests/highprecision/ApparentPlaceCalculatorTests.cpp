#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/FrameTransformer.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

[[nodiscard]] core::SkyContext makeContext()
{
    core::SkyContext context;
    context.utcTime = core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

[[nodiscard]] EphemerisRequest makeRequest(const EphemerisCorrectionFlags correctionFlags)
{
    EphemerisRequest request;
    request.context = makeContext();
    request.epoch = {
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tdb,
    };
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = correctionFlags;
    request.options.enableAtmosphericRefraction = false;
    return request;
}

[[nodiscard]] CelestialBody makeBody()
{
    return {
        .id = "sun",
        .displayName = "Sun",
        .type = CelestialBodyType::Sun,
        .ephemerisSource = CelestialBodyEphemerisSource::Sun,
    };
}

[[nodiscard]] HighPrecisionComputationInput makeInput(const EphemerisRequest& request)
{
    static const CelestialBody kBody = makeBody();
    return {
        .request = request,
        .body = kBody,
        .bodyIndex = 0U,
    };
}

[[nodiscard]] HighPrecisionCalculatorResult makeCalculatorResult()
{
    HighPrecisionCalculatorResult result;
    result.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 0.0,
        .declinationDeg = 0.0,
    };
    result.metadata.dataSourceProvenance = "unit-test source";
    return result;
}

class RecordingFrameTransformer final : public IFrameTransformer {
public:
    [[nodiscard]] CelestialFrameTransformResult transformCelestialVector(const CelestialFrameTransformRequest& request
    ) const override
    {
        ++m_callCount;
        m_lastSourceFrame = request.sourceFrame;
        m_lastTargetFrame = request.targetFrame;
        return {
            .vector = request.vector,
            .metadata = {.status = EphemerisResultStatus::Valid},
        };
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] CelestialReferenceFrame lastSourceFrame() const noexcept
    {
        return m_lastSourceFrame;
    }

    [[nodiscard]] CelestialReferenceFrame lastTargetFrame() const noexcept
    {
        return m_lastTargetFrame;
    }

private:
    mutable int m_callCount = 0;
    mutable CelestialReferenceFrame m_lastSourceFrame = CelestialReferenceFrame::Icrs;
    mutable CelestialReferenceFrame m_lastTargetFrame = CelestialReferenceFrame::Icrs;
};

}  // namespace

class ApparentPlaceCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void routesAstrometricRequestsToGcrs();
    void routesApparentRequestsToCirs();
    void reportsUnavailableRefractionMode();
};

void ApparentPlaceCalculatorTests::routesAstrometricRequestsToGcrs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Astrometric);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastSourceFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 0.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
}

void ApparentPlaceCalculatorTests::routesApparentRequestsToCirs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Apparent);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastSourceFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Cirs)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
}

void ApparentPlaceCalculatorTests::reportsUnavailableRefractionMode()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::AtmosphericRefraction);
    request.options.enableAtmosphericRefraction = true;

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
}

QTEST_APPLESS_MAIN(ApparentPlaceCalculatorTests)

#include "ApparentPlaceCalculatorTests.moc"
