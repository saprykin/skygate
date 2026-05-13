#include "engine/highprecision/FrameTransformer.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;

constexpr double kTolerance = 1.0e-14;

[[nodiscard]] AstronomicalEpoch sofaReferenceTtEpoch() noexcept
{
    return {
        .julianDatePart1 = 2'400'000.5,
        .julianDatePart2 = 53'736.0,
        .timeScale = TimeScale::Tt,
    };
}

[[nodiscard]] AstronomicalEpoch utcEpoch() noexcept
{
    return {
        .julianDatePart1 = 2'400'000.5,
        .julianDatePart2 = 53'735.9992,
        .timeScale = TimeScale::Utc,
    };
}

void compareVector(const CelestialFrameVector& actual, const CelestialFrameVector& expected, const double tolerance)
{
    QVERIFY(std::abs(actual.x - expected.x) <= tolerance);
    QVERIFY(std::abs(actual.y - expected.y) <= tolerance);
    QVERIFY(std::abs(actual.z - expected.z) <= tolerance);
}

class RecordingTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        ++convertCallCount;
        lastEpoch = epoch;
        lastTargetScale = targetScale;
        return nextResult;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);
        static_cast<void>(targetScale);
        return {};
    }

    mutable int convertCallCount = 0;
    mutable AstronomicalEpoch lastEpoch;
    mutable TimeScale lastTargetScale = TimeScale::Utc;
    TimeScaleConversionResult nextResult;
};

[[nodiscard]] TimeScaleConversionResult validTtConversionResult()
{
    return {
        .epoch = sofaReferenceTtEpoch(),
        .status = TimeScaleConversionStatus::Valid,
        .diagnosticText = "unit test conversion",
    };
}

}  // namespace

class FrameTransformerTests final : public QObject {
    Q_OBJECT

private slots:
    void transformsGcrsToCirsAgainstSofaReference();
    void roundTripsGcrsAndCirsVectors();
    void treatsIcrsAndGcrsAsIdentityCelestialAxes();
    void usesTimeScaleServiceForNonTtEpochs();
    void reportsMissingTimeScaleServiceForNonTtCirsTransforms();
};

void FrameTransformerTests::transformsGcrsToCirsAgainstSofaReference()
{
    const ErfaFrameTransformer transformer(nullptr);
    const CelestialFrameTransformResult result = transformer.transformCelestialVector(CelestialFrameTransformRequest{
        .sourceFrame = CelestialReferenceFrame::Gcrs,
        .targetFrame = CelestialReferenceFrame::Cirs,
        .epoch = sofaReferenceTtEpoch(),
        .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
    });

    QVERIFY(result.vector.has_value());
    compareVector(
        *result.vector,
        {
            .x = 0.9999998323037166,
            .y = -0.23842662278707525e-7,
            .z = 0.5791308472168153e-3,
        },
        kTolerance
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::PrecessionNutation)
    );
}

void FrameTransformerTests::roundTripsGcrsAndCirsVectors()
{
    const ErfaFrameTransformer transformer(nullptr);
    const CelestialFrameTransformRequest gcrsToCirsRequest{
        .sourceFrame = CelestialReferenceFrame::Gcrs,
        .targetFrame = CelestialReferenceFrame::Cirs,
        .epoch = sofaReferenceTtEpoch(),
        .vector = {.x = 0.4, .y = -0.2, .z = 0.9},
    };

    const CelestialFrameTransformResult cirs = transformer.transformCelestialVector(gcrsToCirsRequest);
    QVERIFY(cirs.vector.has_value());

    const CelestialFrameTransformResult gcrs = transformer.transformCelestialVector(CelestialFrameTransformRequest{
        .sourceFrame = CelestialReferenceFrame::Cirs,
        .targetFrame = CelestialReferenceFrame::Gcrs,
        .epoch = sofaReferenceTtEpoch(),
        .vector = *cirs.vector,
    });

    QVERIFY(gcrs.vector.has_value());
    compareVector(*gcrs.vector, gcrsToCirsRequest.vector, 2.0e-14);
}

void FrameTransformerTests::treatsIcrsAndGcrsAsIdentityCelestialAxes()
{
    const ErfaFrameTransformer transformer(nullptr);
    const CelestialFrameTransformResult result = transformer.transformCelestialVector(CelestialFrameTransformRequest{
        .sourceFrame = CelestialReferenceFrame::Icrs,
        .targetFrame = CelestialReferenceFrame::Gcrs,
        .epoch =
            {
                .julianDatePart1 = std::numeric_limits<double>::quiet_NaN(),
                .julianDatePart2 = std::numeric_limits<double>::quiet_NaN(),
                .timeScale = TimeScale::Utc,
            },
        .vector = {.x = 0.25, .y = 0.5, .z = -0.75},
    });

    QVERIFY(result.vector.has_value());
    compareVector(*result.vector, {.x = 0.25, .y = 0.5, .z = -0.75}, 0.0);
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::NoCorrections)
    );
}

void FrameTransformerTests::usesTimeScaleServiceForNonTtEpochs()
{
    auto service = std::make_shared<RecordingTimeScaleService>();
    service->nextResult = validTtConversionResult();
    const ErfaFrameTransformer transformer(service);
    const AstronomicalEpoch epoch = utcEpoch();

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(CelestialFrameTransformRequest{
        .sourceFrame = CelestialReferenceFrame::Gcrs,
        .targetFrame = CelestialReferenceFrame::Cirs,
        .epoch = epoch,
        .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
    });

    QVERIFY(result.vector.has_value());
    QCOMPARE(service->convertCallCount, 1);
    QCOMPARE(static_cast<std::uint8_t>(service->lastTargetScale), static_cast<std::uint8_t>(TimeScale::Tt));
    QCOMPARE(service->lastEpoch.julianDatePart1, epoch.julianDatePart1);
    QCOMPARE(service->lastEpoch.julianDatePart2, epoch.julianDatePart2);
}

void FrameTransformerTests::reportsMissingTimeScaleServiceForNonTtCirsTransforms()
{
    const ErfaFrameTransformer transformer(nullptr);

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(CelestialFrameTransformRequest{
        .sourceFrame = CelestialReferenceFrame::Gcrs,
        .targetFrame = CelestialReferenceFrame::Cirs,
        .epoch = utcEpoch(),
        .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
    });

    QVERIFY(!result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
}

QTEST_MAIN(FrameTransformerTests)

#include "FrameTransformerTests.moc"
