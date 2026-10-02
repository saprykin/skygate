#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CivilDateTime.hpp"
#include "time/TimeScale.hpp"
#include "engine/highprecision/ITimeScaleService.hpp"
#include "engine/highprecision/PreparedEphemerisRequestState.hpp"
#include "engine/highprecision/PreparedRequestStateBuilder.hpp"
#include "engine/highprecision/TimeScaleConversionWarningCode.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <memory>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;

[[nodiscard]] EphemerisRequest makeTopocentricRequest()
{
    EphemerisRequest request;
    request.epoch = AstronomicalEpoch{
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Utc,
    };
    request.options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::topocentric());
    return request;
}

class FailingTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        ++m_convertCallCount;
        TimeScaleConversionResult result;
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Failed;
        result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);
        TimeScaleConversionResult result;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Failed;
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

QTEST_APPLESS_MAIN(PreparedRequestStateBuilderTests)

#include "PreparedRequestStateBuilderTests.moc"
