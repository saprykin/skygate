#include "ApparentPlaceTestFixture.hpp"
#include "CelestialBodyCatalog.hpp"
#include "EphemerisRequest.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PassThroughFrameTransformer.hpp"
#include "RecordingFrameTransformer.hpp"
#include "ShortBatchFrameTransformer.hpp"
#include "ValidUtcTimeScaleService.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "engine/TimeScaleConversionStatus.hpp"
#include "engine/TimeScaleConversionWarningCode.hpp"
#include "math/Vector3d.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"
#include "time/TimeScale.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/AtmosphericRefractionCalculator.hpp"
#include "engine/highprecision/CelestialReferenceFrame.hpp"
#include "engine/highprecision/ErfaFrameTransformer.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/IFrameTransformer.hpp"
#include "engine/highprecision/StarAstrometryBatchResult.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using skygate::ephemeris::tests::ApparentPlaceTestFixture;
using skygate::ephemeris::tests::PassThroughFrameTransformer;
using skygate::ephemeris::tests::RecordingFrameTransformer;
using skygate::ephemeris::tests::ShortBatchFrameTransformer;
using skygate::ephemeris::tests::ValidUtcTimeScaleService;
using namespace skygate::core;

class DegradedTtTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const skygate::core::AstronomicalEpoch& epoch, const skygate::core::TimeScale targetScale) const override
    {
        ++m_convertCallCount;
        m_lastTargetScale = targetScale;
        if (targetScale != skygate::core::TimeScale::Tt) {
            TimeScaleConversionResult result;
            result.epoch = epoch;
            result.status = TimeScaleConversionStatus::Failed;
            result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
            return result;
        }

        TimeScaleConversionResult result;
        result.epoch =
            skygate::core::AstronomicalEpoch{
                .julianDatePart1 = epoch.julianDatePart1,
                .julianDatePart2 = epoch.julianDatePart2,
                .timeScale = skygate::core::TimeScale::Tt,
            }
                .normalized();
        result.status = TimeScaleConversionStatus::Degraded;
        result.diagnosticText = "unit test degraded TT conversion";
        result.addWarning(TimeScaleConversionWarningCode::LeapSecondTableMissing);
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult convertCivilDateTime(
        const skygate::core::CivilDateTime& dateTime, const skygate::core::TimeScale targetScale
    ) const override
    {
        static_cast<void>(dateTime);
        static_cast<void>(targetScale);
        return {};
    }

    [[nodiscard]] int convertCallCount() const noexcept
    {
        return m_convertCallCount;
    }

    [[nodiscard]] skygate::core::TimeScale lastTargetScale() const noexcept
    {
        return m_lastTargetScale;
    }

private:
    mutable int m_convertCallCount = 0;
    mutable skygate::core::TimeScale m_lastTargetScale = skygate::core::TimeScale::Utc;
};

void compareEpochs(const skygate::core::AstronomicalEpoch& actual, const skygate::core::AstronomicalEpoch& expected)
{
    QCOMPARE(actual.julianDatePart1, expected.julianDatePart1);
    QCOMPARE(actual.julianDatePart2, expected.julianDatePart2);
    QCOMPARE(static_cast<std::uint8_t>(actual.timeScale), static_cast<std::uint8_t>(expected.timeScale));
}

void compareDateRanges(const EphemerisDateRange& actual, const EphemerisDateRange& expected)
{
    QCOMPARE(QString::fromStdString(actual.id), QString::fromStdString(expected.id));
    QCOMPARE(QString::fromStdString(actual.displayName), QString::fromStdString(expected.displayName));
    compareEpochs(actual.start, expected.start);
    compareEpochs(actual.end, expected.end);
}

void compareOptionalDateRanges(
    const std::optional<EphemerisDateRange>& actual, const std::optional<EphemerisDateRange>& expected
)
{
    QCOMPARE(actual.has_value(), expected.has_value());
    if (actual.has_value() && expected.has_value()) {
        compareDateRanges(*actual, *expected);
    }
}

void compareOptionalDoubles(const std::optional<double>& actual, const std::optional<double>& expected)
{
    QCOMPARE(actual.has_value(), expected.has_value());
    if (actual.has_value() && expected.has_value()) {
        QCOMPARE(*actual, *expected);
    }
}

void compareOptionalVectors(const std::optional<Vector3d>& actual, const std::optional<Vector3d>& expected)
{
    QCOMPARE(actual.has_value(), expected.has_value());
    if (actual.has_value() && expected.has_value()) {
        QCOMPARE(actual->x, expected->x);
        QCOMPARE(actual->y, expected->y);
        QCOMPARE(actual->z, expected->z);
    }
}

void compareMetadata(const EphemerisEngineQueryResult& actual, const EphemerisEngineQueryResult& expected)
{
    QCOMPARE(static_cast<std::uint8_t>(actual.status), static_cast<std::uint8_t>(expected.status));
    QCOMPARE(actual.warningCodeMask, expected.warningCodeMask);
    QCOMPARE(
        QString::fromStdString(actual.dataSourceProvenance), QString::fromStdString(expected.dataSourceProvenance)
    );
    compareOptionalDateRanges(actual.effectiveDataValidityRange, expected.effectiveDataValidityRange);
    compareOptionalDoubles(actual.estimatedAngularUncertaintyArcsec, expected.estimatedAngularUncertaintyArcsec);
    QCOMPARE(
        static_cast<std::uint32_t>(actual.requestedCorrections),
        static_cast<std::uint32_t>(expected.requestedCorrections)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(actual.appliedCorrections), static_cast<std::uint32_t>(expected.appliedCorrections)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(actual.skippedCorrections), static_cast<std::uint32_t>(expected.skippedCorrections)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(actual.unavailableCorrections),
        static_cast<std::uint32_t>(expected.unavailableCorrections)
    );
}

void compareCalculatorResults(
    const HighPrecisionCalculatorResult& actual, const HighPrecisionCalculatorResult& expected
)
{
    QCOMPARE(actual.equatorial.has_value(), expected.equatorial.has_value());
    if (actual.equatorial.has_value() && expected.equatorial.has_value()) {
        QCOMPARE(actual.equatorial->rightAscensionHours, expected.equatorial->rightAscensionHours);
        QCOMPARE(actual.equatorial->declinationDeg, expected.equatorial->declinationDeg);
    }

    QCOMPARE(actual.horizontal.has_value(), expected.horizontal.has_value());
    if (actual.horizontal.has_value() && expected.horizontal.has_value()) {
        QCOMPARE(actual.horizontal->altitudeDeg, expected.horizontal->altitudeDeg);
        QCOMPARE(actual.horizontal->azimuthDeg, expected.horizontal->azimuthDeg);
    }

    compareOptionalVectors(actual.observerRelativePositionAu, expected.observerRelativePositionAu);
    compareMetadata(actual.metadata, expected.metadata);
}

}  // namespace

class ApparentPlaceCorrectionRoutingTests final : public QObject {
    Q_OBJECT

private slots:
    void routesGeometricRequestsToGcrs();
    void routesRequestsWithoutPrecessionNutationToGcrs();
    void routesAstrometricToGcrsWhenRefractionUnsupported();
    void routesApparentRequestsToTrueEquatorAndEquinox();
    void appliesPrecessionNutationWhenRequested();
    void singleApplyAndBatchApplyProduceSameResults();
    void propagatesDegradedMetadataForPrecessionNutation();
    void propagatesDegradedRealFrameMetadataForNutation();
    void failsMissingApparentTransformResults();
};

void ApparentPlaceCorrectionRoutingTests::routesGeometricRequestsToGcrs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::geometric());

    const HighPrecisionCalculatorResult result =
        calculator.apply(ApparentPlaceTestFixture::input(request), ApparentPlaceTestFixture::calculatorResult());

    QCOMPARE(frameTransformer->callCount(), 0);
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 0.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
}

void ApparentPlaceCorrectionRoutingTests::routesRequestsWithoutPrecessionNutationToGcrs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::lightTime());

    const HighPrecisionCalculatorResult result =
        calculator.apply(ApparentPlaceTestFixture::input(request), ApparentPlaceTestFixture::calculatorResult());

    QCOMPARE(frameTransformer->callCount(), 0);
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 0.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
}

void ApparentPlaceCorrectionRoutingTests::routesAstrometricToGcrsWhenRefractionUnsupported()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    EphemerisRequest request = ApparentPlaceTestFixture::request(
        EphemerisCorrectionFlags::lightTime() | EphemerisCorrectionFlags::atmosphericRefraction()
    );
    request.options.setEnableAtmosphericRefraction(true);

    const HighPrecisionCalculatorResult result =
        calculator.apply(ApparentPlaceTestFixture::input(request), ApparentPlaceTestFixture::calculatorResult());

    QCOMPARE(frameTransformer->callCount(), 0);
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 0.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
}

void ApparentPlaceCorrectionRoutingTests::routesApparentRequestsToTrueEquatorAndEquinox()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::apparent());

    const HighPrecisionCalculatorResult result =
        calculator.apply(ApparentPlaceTestFixture::input(request), ApparentPlaceTestFixture::calculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastSourceFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::TrueEquatorAndEquinox)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
}

void ApparentPlaceCorrectionRoutingTests::appliesPrecessionNutationWhenRequested()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    frameTransformer->setResultVector(
        Vector3d{
            .x = 0.0,
            .y = 1.0,
            .z = 0.0,
        }
    );
    EphemerisEngineQueryResult transformMetadata;
    transformMetadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
    transformMetadata.appliedCorrections = EphemerisCorrectionFlags::precessionNutation();
    frameTransformer->setResultMetadata(transformMetadata);

    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(
        EphemerisCorrectionFlags::lightTime() | EphemerisCorrectionFlags::precessionNutation()
    );

    const HighPrecisionCalculatorResult result =
        calculator.apply(ApparentPlaceTestFixture::input(request), ApparentPlaceTestFixture::calculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::TrueEquatorAndEquinox)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 6.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
}

void ApparentPlaceCorrectionRoutingTests::singleApplyAndBatchApplyProduceSameResults()
{
    const OwnGalaxyCelestialBody body = ApparentPlaceTestFixture::sunBody();
    const CelestialBodyCatalog catalog(std::vector<OwnGalaxyCelestialBody>{body});
    EphemerisRequest refractionRequest =
        ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::apparentTopocentric());
    refractionRequest.options.setEnableAtmosphericRefraction(true);
    refractionRequest.context.observer = {
        .latitudeDeg = 45.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };
    struct RequestCase {
        EphemerisRequest request;
        std::optional<int> expectedFrameTransformCount;
        std::optional<CelestialReferenceFrame::Type> expectedLastTargetFrame;
        bool expectedAtmosphericRefractionApplied = false;
    };
    const std::vector<RequestCase> requestCases = {
        {
            .request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::geometric()),
            .expectedFrameTransformCount = 0,
        },
        {
            .request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::lightTime()),
            .expectedFrameTransformCount = 0,
        },
        {
            .request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::apparent()),
            .expectedFrameTransformCount = 1,
            .expectedLastTargetFrame = CelestialReferenceFrame::Type::TrueEquatorAndEquinox,
        },
        {
            .request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric()),
            .expectedLastTargetFrame = CelestialReferenceFrame::Type::TrueEquatorAndEquinox,
        },
        {
            .request = refractionRequest,
            .expectedLastTargetFrame = CelestialReferenceFrame::Type::TrueEquatorAndEquinox,
            .expectedAtmosphericRefractionApplied = true,
        },
    };

    for (const RequestCase& requestCase : requestCases) {
        auto scalarFrameTransformer = std::make_shared<PassThroughFrameTransformer>();
        auto batchFrameTransformer = std::make_shared<PassThroughFrameTransformer>();
        auto scalarTimeScaleService = std::make_shared<ValidUtcTimeScaleService>();
        auto batchTimeScaleService = std::make_shared<ValidUtcTimeScaleService>();
        const auto scalarRefractionCalculator = std::make_shared<AtmosphericRefractionCalculator>();
        const auto batchRefractionCalculator = std::make_shared<AtmosphericRefractionCalculator>();
        const ApparentPlaceCalculator scalarCalculator(
            scalarFrameTransformer, scalarTimeScaleService, nullptr, scalarRefractionCalculator
        );
        const ApparentPlaceCalculator batchCalculator(
            batchFrameTransformer, batchTimeScaleService, nullptr, batchRefractionCalculator
        );
        const HighPrecisionCalculatorResult calculatorResult =
            skygate::ephemeris::EphemerisCorrectionFlags::has(
                requestCase.request.options.correctionFlags(), EphemerisCorrectionFlags::diurnalParallax()
            )
                ? ApparentPlaceTestFixture::solarSystemCalculatorResult(Vector3d{.x = 1.0, .y = 0.0, .z = 1.0})
                : ApparentPlaceTestFixture::calculatorResult();
        const std::vector<StarAstrometryBatchResult> calculatorResults = {
            {
                .bodyIndex = 0U,
                .result = calculatorResult,
            },
        };

        const HighPrecisionCalculatorResult scalarResult =
            scalarCalculator.apply(ApparentPlaceTestFixture::input(requestCase.request, body), calculatorResult);
        const std::vector<StarAstrometryBatchResult> batchResults =
            batchCalculator.applyBatch(requestCase.request, catalog.bodies(), calculatorResults, nullptr);

        if (requestCase.expectedFrameTransformCount.has_value()) {
            QCOMPARE(scalarFrameTransformer->callCount(), *requestCase.expectedFrameTransformCount);
            QCOMPARE(batchFrameTransformer->callCount(), *requestCase.expectedFrameTransformCount);
        }
        if (requestCase.expectedLastTargetFrame.has_value()) {
            QCOMPARE(
                static_cast<std::uint8_t>(scalarFrameTransformer->lastTargetFrame()),
                static_cast<std::uint8_t>(*requestCase.expectedLastTargetFrame)
            );
            QCOMPARE(
                static_cast<std::uint8_t>(batchFrameTransformer->lastTargetFrame()),
                static_cast<std::uint8_t>(*requestCase.expectedLastTargetFrame)
            );
        }
        QCOMPARE(batchResults.size(), 1U);
        QCOMPARE(
            skygate::ephemeris::EphemerisCorrectionFlags::has(
                scalarResult.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
            ),
            requestCase.expectedAtmosphericRefractionApplied
        );
        QCOMPARE(
            skygate::ephemeris::EphemerisCorrectionFlags::has(
                batchResults[0].result.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
            ),
            requestCase.expectedAtmosphericRefractionApplied
        );
        compareCalculatorResults(batchResults[0].result, scalarResult);
    }
}

void ApparentPlaceCorrectionRoutingTests::propagatesDegradedMetadataForPrecessionNutation()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    EphemerisEngineQueryResult transformMetadata;
    transformMetadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded;
    transformMetadata.addWarning(EphemerisEngineWarning::Code::AccuracyDegraded);
    transformMetadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
    transformMetadata.appliedCorrections = EphemerisCorrectionFlags::precessionNutation();
    frameTransformer->setResultMetadata(transformMetadata);

    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::precessionNutation());

    const HighPrecisionCalculatorResult result =
        calculator.apply(ApparentPlaceTestFixture::input(request), ApparentPlaceTestFixture::calculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::TrueEquatorAndEquinox)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
}

void ApparentPlaceCorrectionRoutingTests::propagatesDegradedRealFrameMetadataForNutation()
{
    const ErfaFrameTransformer availabilityTransformer(nullptr);
    const CelestialFrameTransformResult availabilityResult = ApparentPlaceTestFixture::transformSingleVector(
        availabilityTransformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Cirs,
        {
            .julianDatePart1 = 2'400'000.5,
            .julianDatePart2 = 53'736.0,
            .timeScale = skygate::core::TimeScale::Tt,
        },
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );
    if (!availabilityResult.vector.has_value()) {
        QSKIP("ERFA-backed frame transforms are not available in this build.");
    }

    auto timeScaleService = std::make_shared<DegradedTtTimeScaleService>();
    const auto frameTransformer = std::make_shared<ErfaFrameTransformer>(timeScaleService);
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::precessionNutation());
    request.epoch = {
        .julianDatePart1 = 2'400'000.5,
        .julianDatePart2 = 53'736.0,
        .timeScale = skygate::core::TimeScale::Utc,
    };

    const HighPrecisionCalculatorResult result =
        calculator.apply(ApparentPlaceTestFixture::input(request), ApparentPlaceTestFixture::calculatorResult());

    QCOMPARE(timeScaleService->convertCallCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(timeScaleService->lastTargetScale()),
        static_cast<std::uint8_t>(skygate::core::TimeScale::Tt)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
}

void ApparentPlaceCorrectionRoutingTests::failsMissingApparentTransformResults()
{
    auto scalarFrameTransformer = std::make_shared<ShortBatchFrameTransformer>();
    auto batchFrameTransformer = std::make_shared<ShortBatchFrameTransformer>();
    const ApparentPlaceCalculator scalarCalculator(scalarFrameTransformer, nullptr, nullptr);
    const ApparentPlaceCalculator batchCalculator(batchFrameTransformer, nullptr, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::precessionNutation());
    const std::vector<OwnGalaxyCelestialBody> bodies = {
        ApparentPlaceTestFixture::sunBody(), ApparentPlaceTestFixture::sunBody()
    };
    const CelestialBodyCatalog catalog(bodies);
    const std::vector<StarAstrometryBatchResult> calculatorResults = {
        {
            .bodyIndex = 0U,
            .result = ApparentPlaceTestFixture::calculatorResult(),
        },
        {
            .bodyIndex = 1U,
            .result = ApparentPlaceTestFixture::calculatorResult(),
        },
    };

    const HighPrecisionCalculatorResult scalarResult =
        scalarCalculator.apply(ApparentPlaceTestFixture::input(request), ApparentPlaceTestFixture::calculatorResult());
    const std::vector<StarAstrometryBatchResult> results =
        batchCalculator.applyBatch(request, catalog.bodies(), calculatorResults, nullptr);

    QCOMPARE(scalarFrameTransformer->batchCallCount(), 1);
    QCOMPARE(batchFrameTransformer->batchCallCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(scalarResult.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(scalarResult.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(scalarResult.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(!scalarResult.equatorial.has_value());
    QVERIFY(!scalarResult.horizontal.has_value());
    QCOMPARE(results.size(), 2U);
    QCOMPARE(
        static_cast<std::uint8_t>(results[0].result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            results[0].result.metadata.appliedCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
    QCOMPARE(
        static_cast<std::uint8_t>(results[1].result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(results[1].result.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(results[1].result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(!results[1].result.equatorial.has_value());
    QVERIFY(!results[1].result.horizontal.has_value());
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            results[1].result.metadata.unavailableCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
}

QTEST_APPLESS_MAIN(ApparentPlaceCorrectionRoutingTests)

#include "ApparentPlaceCorrectionRoutingTests.moc"
