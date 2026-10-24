#include "ApparentPlaceTestFixture.hpp"
#include "CelestialBodyCatalog.hpp"
#include "EphemerisRequest.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PassThroughFrameTransformer.hpp"
#include "ShortBatchFrameTransformer.hpp"
#include "ValidUtcTimeScaleService.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "math/Vector3d.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/AtmosphericRefractionCalculator.hpp"
#include "engine/highprecision/CelestialFrameTransformResult.hpp"
#include "engine/highprecision/CelestialReferenceFrame.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/IAtmosphericRefractionCalculator.hpp"
#include "engine/highprecision/IFrameTransformer.hpp"
#include "engine/highprecision/StarAstrometryBatchResult.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using skygate::ephemeris::tests::ApparentPlaceTestFixture;
using skygate::ephemeris::tests::PassThroughFrameTransformer;
using skygate::ephemeris::tests::ShortBatchFrameTransformer;
using skygate::ephemeris::tests::ValidUtcTimeScaleService;
using namespace skygate::core;

class FailingTopocentricFrameTransformer final : public IFrameTransformer {
public:
    FailingTopocentricFrameTransformer(
        const CelestialReferenceFrame::Type failingSourceFrame, const CelestialReferenceFrame::Type failingTargetFrame
    ) noexcept
        : m_failingSourceFrame(failingSourceFrame), m_failingTargetFrame(failingTargetFrame)
    {
    }

    [[nodiscard]] std::vector<CelestialFrameTransformResult>
    transform(const CelestialFrameTransformRequest& request) const override
    {
        std::vector<CelestialFrameTransformResult> results;
        results.reserve(request.vectors.size());
        for (const Vector3d& vector : request.vectors) {
            ++m_callCount;
            if (request.sourceFrame == m_failingSourceFrame && request.targetFrame == m_failingTargetFrame) {
                EphemerisEngineQueryResult metadata;
                metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded;
                metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
                results.push_back(
                    CelestialFrameTransformResult{
                        .vector = std::nullopt,
                        .metadata = metadata,
                    }
                );
                continue;
            }

            results.push_back(
                ApparentPlaceTestFixture::frameTransformResult(request.sourceFrame, request.targetFrame, vector)
            );
        }
        return results;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount = 0;
    CelestialReferenceFrame::Type m_failingSourceFrame = CelestialReferenceFrame::Type::Gcrs;
    CelestialReferenceFrame::Type m_failingTargetFrame = CelestialReferenceFrame::Type::Gcrs;
};

class ZeroVectorFrameTransformer final : public IFrameTransformer {
public:
    [[nodiscard]] std::vector<CelestialFrameTransformResult>
    transform(const CelestialFrameTransformRequest& request) const override
    {
        ++m_callCount;
        std::vector<CelestialFrameTransformResult> results;
        results.reserve(request.vectors.size());
        for (const Vector3d& vector : request.vectors) {
            static_cast<void>(vector);
            results.push_back(
                CelestialFrameTransformResult{
                    .vector = Vector3d{},
                    .metadata = EphemerisEngineQueryResult{
                        .status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid,
                    },
                }
            );
        }
        return results;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount = 0;
};

class CountingAtmosphericRefractionCalculator final : public IAtmosphericRefractionCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult apply(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override
    {
        static_cast<void>(input);
        ++m_callCount;
        return calculatorResult;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount = 0;
};

}  // namespace

class ApparentPlaceTopocentricTests final : public QObject {
    Q_OBJECT

private slots:
    void appliesTopocentricParallaxAndHorizontal();
    void reportsInvalidObserverForTopocentricRequest();
    void missingEarthOrientationForTopocentricRequest();
    void unavailableCorrectionsWhenPrimaryTransformFails();
    void clearsCoordinatesWhenBatchTransformFails();
    void failsMissingBatchResultsAndClearsCoordinates();
    void clearsEquatorialWhenEquatorialTransformFails();
    void clearsEquatorialWhenBatchTransformFails();
    void unavailableParallaxWhenDistanceVectorMissing();
    void leavesApparentRequestGeocentricWithoutParallax();
    void changesTopocentricPositionWithObserverElevation();
    void appliesRefractionToTopocentricHorizontal();
    void skipsRefractionWhenEquatorialConversionFails();
};

void ApparentPlaceTopocentricTests::appliesTopocentricParallaxAndHorizontal()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());
    request.context.observer = {
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };

    const HighPrecisionCalculatorResult result = calculator.apply(
        ApparentPlaceTestFixture::input(request),
        ApparentPlaceTestFixture::solarSystemCalculatorResult(
            Vector3d{
                .x = 0.0,
                .y = 1.0,
                .z = 0.0,
            }
        )
    );

    QCOMPARE(timeScaleService->convertCallCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(timeScaleService->lastTargetScale()),
        static_cast<std::uint8_t>(skygate::core::TimeScale::Utc)
    );
    QCOMPARE(frameTransformer->callCount(), 3);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::TrueEquatorAndEquinox)
    );
    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.horizontal.has_value());
    QVERIFY(result.equatorial->rightAscensionHours > 6.0);
    QVERIFY(std::abs(result.equatorial->declinationDeg) < 1.0e-12);
    QVERIFY(result.horizontal->altitudeDeg < 0.0);
    QVERIFY(std::abs(result.horizontal->azimuthDeg - 90.0) < 1.0e-9);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
        )
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
}

void ApparentPlaceTopocentricTests::reportsInvalidObserverForTopocentricRequest()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());
    request.context.observer.latitudeDeg = 120.0;

    const HighPrecisionCalculatorResult result = calculator.apply(
        ApparentPlaceTestFixture::input(request),
        ApparentPlaceTestFixture::solarSystemCalculatorResult(
            Vector3d{
                .x = 0.0,
                .y = 1.0,
                .z = 0.0,
            }
        )
    );

    QVERIFY(result.equatorial.has_value());
    QVERIFY(!result.horizontal.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::MissingObserver));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
    ));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::diurnalParallax()
        )
    );
}

void ApparentPlaceTopocentricTests::missingEarthOrientationForTopocentricRequest()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());

    const HighPrecisionCalculatorResult result = calculator.apply(
        ApparentPlaceTestFixture::input(request),
        ApparentPlaceTestFixture::solarSystemCalculatorResult(
            Vector3d{
                .x = 0.0,
                .y = 1.0,
                .z = 0.0,
            }
        )
    );

    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.horizontal.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
        )
    );
}

void ApparentPlaceTopocentricTests::unavailableCorrectionsWhenPrimaryTransformFails()
{
    auto frameTransformer = std::make_shared<FailingTopocentricFrameTransformer>(
        CelestialReferenceFrame::Type::Gcrs, CelestialReferenceFrame::Type::Itrs
    );
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());

    const HighPrecisionCalculatorResult result = calculator.apply(
        ApparentPlaceTestFixture::input(request),
        ApparentPlaceTestFixture::solarSystemCalculatorResult(
            Vector3d{
                .x = 0.0,
                .y = 1.0,
                .z = 0.0,
            }
        )
    );

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );
    QVERIFY(!result.equatorial.has_value());
    QVERIFY(!result.horizontal.has_value());
}

void ApparentPlaceTopocentricTests::clearsCoordinatesWhenBatchTransformFails()
{
    auto frameTransformer = std::make_shared<FailingTopocentricFrameTransformer>(
        CelestialReferenceFrame::Type::Gcrs, CelestialReferenceFrame::Type::Itrs
    );
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());
    const std::vector<OwnGalaxyCelestialBody> bodies = {ApparentPlaceTestFixture::sunBody()};
    const CelestialBodyCatalog catalog(bodies);
    const std::vector<StarAstrometryBatchResult> calculatorResults = {
        {
            .bodyIndex = 0U,
            .result = ApparentPlaceTestFixture::solarSystemCalculatorResult(
                Vector3d{
                    .x = 0.0,
                    .y = 1.0,
                    .z = 0.0,
                }
            ),
        },
    };

    const std::vector<StarAstrometryBatchResult> results =
        calculator.applyBatch(request, catalog.bodies(), calculatorResults, nullptr);

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(results.size(), 1U);
    QCOMPARE(
        static_cast<std::uint8_t>(results[0].result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(results[0].result.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(results[0].result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            results[0].result.metadata.unavailableCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            results[0].result.metadata.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );
    QVERIFY(!results[0].result.equatorial.has_value());
    QVERIFY(!results[0].result.horizontal.has_value());
}

void ApparentPlaceTopocentricTests::failsMissingBatchResultsAndClearsCoordinates()
{
    auto frameTransformer = std::make_shared<ShortBatchFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());
    const std::vector<OwnGalaxyCelestialBody> bodies = {ApparentPlaceTestFixture::sunBody()};
    const CelestialBodyCatalog catalog(bodies);
    const std::vector<StarAstrometryBatchResult> calculatorResults = {
        {
            .bodyIndex = 0U,
            .result = ApparentPlaceTestFixture::solarSystemCalculatorResult(
                Vector3d{
                    .x = 0.0,
                    .y = 1.0,
                    .z = 0.0,
                }
            ),
        },
    };

    const std::vector<StarAstrometryBatchResult> results =
        calculator.applyBatch(request, catalog.bodies(), calculatorResults, nullptr);

    QCOMPARE(frameTransformer->batchCallCount(), 1);
    QCOMPARE(results.size(), 1U);
    QCOMPARE(
        static_cast<std::uint8_t>(results[0].result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(results[0].result.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(results[0].result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            results[0].result.metadata.unavailableCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            results[0].result.metadata.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );
    QVERIFY(!results[0].result.equatorial.has_value());
    QVERIFY(!results[0].result.horizontal.has_value());
}

void ApparentPlaceTopocentricTests::clearsEquatorialWhenEquatorialTransformFails()
{
    auto frameTransformer = std::make_shared<FailingTopocentricFrameTransformer>(
        CelestialReferenceFrame::Type::Itrs, CelestialReferenceFrame::Type::Gcrs
    );
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());

    const HighPrecisionCalculatorResult result = calculator.apply(
        ApparentPlaceTestFixture::input(request),
        ApparentPlaceTestFixture::solarSystemCalculatorResult(
            Vector3d{
                .x = 0.0,
                .y = 1.0,
                .z = 0.0,
            }
        )
    );

    QCOMPARE(frameTransformer->callCount(), 2);
    QVERIFY(!result.equatorial.has_value());
    QVERIFY(result.horizontal.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );
}

void ApparentPlaceTopocentricTests::clearsEquatorialWhenBatchTransformFails()
{
    auto frameTransformer = std::make_shared<FailingTopocentricFrameTransformer>(
        CelestialReferenceFrame::Type::Itrs, CelestialReferenceFrame::Type::Gcrs
    );
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());
    const std::vector<OwnGalaxyCelestialBody> bodies = {ApparentPlaceTestFixture::sunBody()};
    const CelestialBodyCatalog catalog(bodies);
    const std::vector<StarAstrometryBatchResult> calculatorResults = {
        {
            .bodyIndex = 0U,
            .result = ApparentPlaceTestFixture::solarSystemCalculatorResult(
                Vector3d{
                    .x = 0.0,
                    .y = 1.0,
                    .z = 0.0,
                }
            ),
        },
    };

    const std::vector<StarAstrometryBatchResult> results =
        calculator.applyBatch(request, catalog.bodies(), calculatorResults, nullptr);

    QCOMPARE(frameTransformer->callCount(), 2);
    QCOMPARE(results.size(), 1U);
    QVERIFY(!results[0].result.equatorial.has_value());
    QVERIFY(results[0].result.horizontal.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(results[0].result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(results[0].result.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(results[0].result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            results[0].result.metadata.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );
}

void ApparentPlaceTopocentricTests::unavailableParallaxWhenDistanceVectorMissing()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());

    const HighPrecisionCalculatorResult result =
        calculator.apply(ApparentPlaceTestFixture::input(request), ApparentPlaceTestFixture::calculatorResult());

    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.horizontal.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
    ));
}

void ApparentPlaceTopocentricTests::leavesApparentRequestGeocentricWithoutParallax()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest apparentRequest = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::apparent());
    EphemerisRequest topocentricRequest = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());
    topocentricRequest.context.observer = {
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };
    const HighPrecisionCalculatorResult calculatorResult =
        ApparentPlaceTestFixture::solarSystemCalculatorResult(Vector3d{.x = 0.0, .y = 1.0, .z = 0.0});

    const HighPrecisionCalculatorResult apparentResult =
        calculator.apply(ApparentPlaceTestFixture::input(apparentRequest), calculatorResult);
    const HighPrecisionCalculatorResult topocentricResult =
        calculator.apply(ApparentPlaceTestFixture::input(topocentricRequest), calculatorResult);

    QVERIFY(apparentResult.equatorial.has_value());
    QVERIFY(topocentricResult.equatorial.has_value());
    QVERIFY(!apparentResult.horizontal.has_value());
    QVERIFY(topocentricResult.horizontal.has_value());
    QVERIFY(std::abs(apparentResult.equatorial->rightAscensionHours - 6.0) < 1.0e-12);
    QVERIFY(topocentricResult.equatorial->rightAscensionHours > apparentResult.equatorial->rightAscensionHours);
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        apparentResult.metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
    ));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            topocentricResult.metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
        )
    );
}

void ApparentPlaceTopocentricTests::changesTopocentricPositionWithObserverElevation()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    EphemerisRequest seaLevelRequest = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::topocentric());
    seaLevelRequest.context.observer = {
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };
    EphemerisRequest elevatedRequest = seaLevelRequest;
    elevatedRequest.context.observer.elevationMeters = 4'200.0;
    const HighPrecisionCalculatorResult calculatorResult =
        ApparentPlaceTestFixture::solarSystemCalculatorResult(Vector3d{.x = 0.0, .y = 1.0, .z = 0.0});

    const HighPrecisionCalculatorResult seaLevelResult =
        calculator.apply(ApparentPlaceTestFixture::input(seaLevelRequest), calculatorResult);
    const HighPrecisionCalculatorResult elevatedResult =
        calculator.apply(ApparentPlaceTestFixture::input(elevatedRequest), calculatorResult);

    QVERIFY(seaLevelResult.equatorial.has_value());
    QVERIFY(elevatedResult.equatorial.has_value());
    QVERIFY(seaLevelResult.horizontal.has_value());
    QVERIFY(elevatedResult.horizontal.has_value());
    QVERIFY(
        elevatedResult.equatorial->rightAscensionHours > seaLevelResult.equatorial->rightAscensionHours
        || elevatedResult.horizontal->altitudeDeg != seaLevelResult.horizontal->altitudeDeg
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            elevatedResult.metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
        )
    );
}

void ApparentPlaceTopocentricTests::appliesRefractionToTopocentricHorizontal()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const auto refractionCalculator = std::make_shared<AtmosphericRefractionCalculator>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr, refractionCalculator);
    EphemerisRequest request = ApparentPlaceTestFixture::request(EphemerisCorrectionFlags::apparentTopocentric());
    request.options.setEnableAtmosphericRefraction(true);
    request.context.observer = {
        .latitudeDeg = 45.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };

    const HighPrecisionCalculatorResult result = calculator.apply(
        ApparentPlaceTestFixture::input(request),
        ApparentPlaceTestFixture::solarSystemCalculatorResult(
            Vector3d{
                .x = 1.0,
                .y = 0.0,
                .z = 1.0,
            }
        )
    );

    QVERIFY(result.horizontal.has_value());
    QVERIFY(result.horizontal->altitudeDeg > 0.0);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
}

void ApparentPlaceTopocentricTests::skipsRefractionWhenEquatorialConversionFails()
{
    auto scalarFrameTransformer = std::make_shared<ZeroVectorFrameTransformer>();
    auto batchFrameTransformer = std::make_shared<ZeroVectorFrameTransformer>();
    const auto scalarRefractionCalculator = std::make_shared<CountingAtmosphericRefractionCalculator>();
    const auto batchRefractionCalculator = std::make_shared<CountingAtmosphericRefractionCalculator>();
    const ApparentPlaceCalculator scalarCalculator(
        scalarFrameTransformer, nullptr, nullptr, scalarRefractionCalculator
    );
    const ApparentPlaceCalculator batchCalculator(batchFrameTransformer, nullptr, nullptr, batchRefractionCalculator);
    EphemerisRequest request = ApparentPlaceTestFixture::request(
        EphemerisCorrectionFlags::precessionNutation() | EphemerisCorrectionFlags::atmosphericRefraction()
    );
    request.options.setEnableAtmosphericRefraction(true);
    const std::vector<OwnGalaxyCelestialBody> bodies = {ApparentPlaceTestFixture::sunBody()};
    const CelestialBodyCatalog catalog(bodies);
    const HighPrecisionCalculatorResult calculatorResult = ApparentPlaceTestFixture::calculatorResult();
    const std::vector<StarAstrometryBatchResult> calculatorResults = {
        {
            .bodyIndex = 0U,
            .result = calculatorResult,
        },
    };

    const HighPrecisionCalculatorResult scalarResult =
        scalarCalculator.apply(ApparentPlaceTestFixture::input(request), calculatorResult);
    const std::vector<StarAstrometryBatchResult> batchResults =
        batchCalculator.applyBatch(request, catalog.bodies(), calculatorResults, nullptr);

    QCOMPARE(scalarFrameTransformer->callCount(), 1);
    QCOMPARE(batchFrameTransformer->callCount(), 1);
    QCOMPARE(scalarRefractionCalculator->callCount(), 0);
    QCOMPARE(batchRefractionCalculator->callCount(), 0);
    QCOMPARE(batchResults.size(), 1U);
    QCOMPARE(
        static_cast<std::uint8_t>(scalarResult.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(batchResults[0].result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
}

QTEST_APPLESS_MAIN(ApparentPlaceTopocentricTests)

#include "ApparentPlaceTopocentricTests.moc"
