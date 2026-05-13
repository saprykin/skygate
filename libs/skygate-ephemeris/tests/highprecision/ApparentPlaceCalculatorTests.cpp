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
            .vector = m_resultVector.value_or(request.vector),
            .metadata = m_resultMetadata,
        };
    }

    void setResultVector(const CelestialFrameVector& vector) noexcept
    {
        m_resultVector = vector;
    }

    void setResultMetadata(const EphemerisResultMetadata& metadata)
    {
        m_resultMetadata = metadata;
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
    std::optional<CelestialFrameVector> m_resultVector;
    EphemerisResultMetadata m_resultMetadata = {.status = EphemerisResultStatus::Valid};
};

class DegradedTtTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        ++m_convertCallCount;
        m_lastTargetScale = targetScale;
        if (targetScale != TimeScale::Tt) {
            TimeScaleConversionResult result;
            result.epoch = epoch;
            result.status = TimeScaleConversionStatus::Failed;
            result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
            return result;
        }

        TimeScaleConversionResult result;
        result.epoch = normalizedAstronomicalEpoch(AstronomicalEpoch{
            .julianDatePart1 = epoch.julianDatePart1,
            .julianDatePart2 = epoch.julianDatePart2,
            .timeScale = TimeScale::Tt,
        });
        result.status = TimeScaleConversionStatus::Degraded;
        result.diagnosticText = "unit test degraded TT conversion";
        result.addWarning(TimeScaleConversionWarningCode::LeapSecondTableMissing);
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);
        static_cast<void>(targetScale);
        return {};
    }

    [[nodiscard]] int convertCallCount() const noexcept
    {
        return m_convertCallCount;
    }

    [[nodiscard]] TimeScale lastTargetScale() const noexcept
    {
        return m_lastTargetScale;
    }

private:
    mutable int m_convertCallCount = 0;
    mutable TimeScale m_lastTargetScale = TimeScale::Utc;
};

}  // namespace

class ApparentPlaceCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void routesGeometricRequestsToGcrs();
    void routesRequestsWithoutPrecessionNutationToGcrs();
    void routesAstrometricRequestsWithUnsupportedRefractionToGcrs();
    void routesApparentRequestsToCirs();
    void appliesPrecessionNutationWhenRequested();
    void propagatesDegradedTransformMetadataForPrecessionNutation();
    void propagatesDegradedRealFrameTransformMetadataForPrecessionNutation();
    void reportsUnavailableRefractionMode();
};

void ApparentPlaceCalculatorTests::routesGeometricRequestsToGcrs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Geometric);

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

void ApparentPlaceCalculatorTests::routesRequestsWithoutPrecessionNutationToGcrs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::LightTime);

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

void ApparentPlaceCalculatorTests::routesAstrometricRequestsWithUnsupportedRefractionToGcrs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    EphemerisRequest request =
        makeRequest(EphemerisCorrectionFlags::LightTime | EphemerisCorrectionFlags::AtmosphericRefraction);
    request.options.enableAtmosphericRefraction = true;

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
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
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

void ApparentPlaceCalculatorTests::appliesPrecessionNutationWhenRequested()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    frameTransformer->setResultVector(CelestialFrameVector{
        .x = 0.0,
        .y = 1.0,
        .z = 0.0,
    });
    EphemerisResultMetadata transformMetadata;
    transformMetadata.status = EphemerisResultStatus::Valid;
    transformMetadata.appliedCorrections = EphemerisCorrectionFlags::PrecessionNutation;
    frameTransformer->setResultMetadata(transformMetadata);

    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request =
        makeRequest(EphemerisCorrectionFlags::LightTime | EphemerisCorrectionFlags::PrecessionNutation);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Cirs)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 6.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::PrecessionNutation));
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
}

void ApparentPlaceCalculatorTests::propagatesDegradedTransformMetadataForPrecessionNutation()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    EphemerisResultMetadata transformMetadata;
    transformMetadata.status = EphemerisResultStatus::Degraded;
    transformMetadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    transformMetadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
    transformMetadata.appliedCorrections = EphemerisCorrectionFlags::PrecessionNutation;
    frameTransformer->setResultMetadata(transformMetadata);

    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::PrecessionNutation);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Cirs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::PrecessionNutation));
}

void ApparentPlaceCalculatorTests::propagatesDegradedRealFrameTransformMetadataForPrecessionNutation()
{
    const ErfaFrameTransformer availabilityTransformer(nullptr);
    const CelestialFrameTransformResult availabilityResult =
        availabilityTransformer.transformCelestialVector(CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Cirs,
            .epoch =
                {
                    .julianDatePart1 = 2'400'000.5,
                    .julianDatePart2 = 53'736.0,
                    .timeScale = TimeScale::Tt,
                },
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        });
    if (!availabilityResult.vector.has_value()) {
        QSKIP("ERFA-backed frame transforms are not available in this build.");
    }

    auto timeScaleService = std::make_shared<DegradedTtTimeScaleService>();
    const auto frameTransformer = std::make_shared<ErfaFrameTransformer>(timeScaleService);
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::PrecessionNutation);
    request.epoch = {
        .julianDatePart1 = 2'400'000.5,
        .julianDatePart2 = 53'736.0,
        .timeScale = TimeScale::Utc,
    };

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(timeScaleService->convertCallCount(), 1);
    QCOMPARE(static_cast<std::uint8_t>(timeScaleService->lastTargetScale()), static_cast<std::uint8_t>(TimeScale::Tt));
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::PrecessionNutation));
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
