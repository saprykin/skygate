#include "TdbApproximationTimeScaleService.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CivilDateTime.hpp"
#include "time/TimeScale.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/highprecision/PreparedEphemerisRequestState.hpp"
#include "engine/highprecision/PreparedRequestStateBuilder.hpp"
#include "engine/TimeScaleConversionWarningCode.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <memory>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using skygate::ephemeris::tests::TdbApproximationTimeScaleService;

[[nodiscard]] EphemerisRequest makeKernelEpochRequest()
{
    EphemerisRequest request;
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'460'310.5,
        .julianDatePart2 = 0.25,
        .timeScale = skygate::core::TimeScale::Tt,
    };
    request.options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::noCorrections());
    return request;
}

[[nodiscard]] EphemerisRequest makeTopocentricRequest()
{
    EphemerisRequest request;
    request.epoch = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = skygate::core::TimeScale::Utc,
    };
    request.options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::topocentric());
    return request;
}

class FailingTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const skygate::core::AstronomicalEpoch& epoch, const skygate::core::TimeScale targetScale) const override
    {
        ++m_convertCallCount;
        TimeScaleConversionResult result;
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Type::Failed;
        result.addWarning(TimeScaleConversionWarningCode::Type::UnsupportedConversion);
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult convertCivilDateTime(
        const skygate::core::CivilDateTime& dateTime, const skygate::core::TimeScale targetScale
    ) const override
    {
        static_cast<void>(dateTime);
        TimeScaleConversionResult result;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Type::Failed;
        return result;
    }

    [[nodiscard]] int convertCallCount() const noexcept
    {
        return m_convertCallCount;
    }

private:
    mutable int m_convertCallCount = 0;
};

}  // namespace

class PreparedRequestStateBuilderTests final : public QObject {
    Q_OBJECT

private slots:
    void marksTopocentricConversionFailureAsFailed();
    void degradesKernelEpochWithoutTimeScaleService();
    void keepsTdbApproximationConversionSilent();
    void reportsFailedKernelEpochConversion();
};

void PreparedRequestStateBuilderTests::marksTopocentricConversionFailureAsFailed()
{
    auto timeScaleService = std::make_shared<FailingTimeScaleService>();
    PreparedRequestStateBuilder builder(
        PreparedRequestStateBuilder::Dependencies{
            .calcephKernel = nullptr,
            .timeScaleService = timeScaleService,
            .earthOrientationProvider = nullptr,
        }
    );

    const std::shared_ptr<const PreparedEphemerisRequestState> preparedState = builder.build(makeTopocentricRequest());

    QVERIFY(preparedState != nullptr);
    QVERIFY(preparedState->topocentricStatePrepared);
    QVERIFY(!preparedState->topocentricStateAvailable);
    QCOMPARE(
        static_cast<std::uint8_t>(preparedState->topocentricMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(preparedState->topocentricMetadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(preparedState->topocentricMetadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        EphemerisCorrectionFlags::has(
            preparedState->topocentricMetadata.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );
    QCOMPARE(timeScaleService->convertCallCount(), 2);
}

void PreparedRequestStateBuilderTests::degradesKernelEpochWithoutTimeScaleService()
{
    const PreparedRequestStateBuilder builder(
        PreparedRequestStateBuilder::Dependencies{
            .calcephKernel = nullptr,
            .timeScaleService = nullptr,
            .earthOrientationProvider = nullptr,
        }
    );

    const std::shared_ptr<const PreparedEphemerisRequestState> preparedState = builder.build(makeKernelEpochRequest());

    QVERIFY(preparedState != nullptr);
    QVERIFY(!preparedState->tdbKernelEpoch.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(preparedState->tdbKernelEpochMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(preparedState->tdbKernelEpochMetadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
}

void PreparedRequestStateBuilderTests::keepsTdbApproximationConversionSilent()
{
    auto timeScaleService = std::make_shared<TdbApproximationTimeScaleService>();
    const PreparedRequestStateBuilder builder(
        PreparedRequestStateBuilder::Dependencies{
            .calcephKernel = nullptr,
            .timeScaleService = timeScaleService,
            .earthOrientationProvider = nullptr,
        }
    );

    const std::shared_ptr<const PreparedEphemerisRequestState> preparedState = builder.build(makeKernelEpochRequest());

    QVERIFY(preparedState != nullptr);
    QCOMPARE(timeScaleService->convertCallCount(), 1);
    QVERIFY(preparedState->tdbKernelEpoch.has_value());
    const skygate::core::AstronomicalEpoch expectedTdbEpoch{
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.75,
        .timeScale = skygate::core::TimeScale::Tdb,
    };
    QCOMPARE(*preparedState->tdbKernelEpoch, expectedTdbEpoch);
    QCOMPARE(
        static_cast<std::uint8_t>(preparedState->tdbKernelEpochMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(!preparedState->tdbKernelEpochMetadata.warningCodeMask.hasAny());
}

void PreparedRequestStateBuilderTests::reportsFailedKernelEpochConversion()
{
    auto timeScaleService = std::make_shared<FailingTimeScaleService>();
    const PreparedRequestStateBuilder builder(
        PreparedRequestStateBuilder::Dependencies{
            .calcephKernel = nullptr,
            .timeScaleService = timeScaleService,
            .earthOrientationProvider = nullptr,
        }
    );

    const std::shared_ptr<const PreparedEphemerisRequestState> preparedState = builder.build(makeKernelEpochRequest());

    QVERIFY(preparedState != nullptr);
    QCOMPARE(timeScaleService->convertCallCount(), 1);
    QVERIFY(!preparedState->tdbKernelEpoch.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(preparedState->tdbKernelEpochMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(preparedState->tdbKernelEpochMetadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
}

QTEST_APPLESS_MAIN(PreparedRequestStateBuilderTests)

#include "PreparedRequestStateBuilderTests.moc"
