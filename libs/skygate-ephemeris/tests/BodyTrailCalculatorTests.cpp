#include "skygate/ephemeris/BodyTrailCalculator.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <limits>
#include <optional>

namespace {

class FakeTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(
        const skygate::core::SkyContext& context
    ) const override
    {
        return skygate::ephemeris::SkySnapshot {.context = context};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState> computeBodyState(
        const skygate::core::SkyContext& context,
        const std::uint32_t bodyIndex
    ) const override
    {
        const auto offset = std::chrono::duration_cast<std::chrono::minutes>(
            context.utcTime.time_since_epoch()
        ).count();
        if (offset == -30) {
            return std::nullopt;
        }

        skygate::ephemeris::CelestialBodyState state {
            .bodyIndex = bodyIndex,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
            .horizontal = {
                .altitudeDeg = static_cast<double>(offset / 30),
                .azimuthDeg = 120.0
            }
        };
        if (offset == 30) {
            state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
        }
        return state;
    }
};

}  // namespace

class BodyTrailCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void samplesOffsetsAndPreservesInvalidGaps();
    void rejectsInvalidOptions();
};

void BodyTrailCalculatorTests::samplesOffsetsAndPreservesInvalidGaps()
{
    const FakeTrailEngine engine;
    const skygate::ephemeris::BodyTrailCalculator calculator;
    const auto samples = calculator.sample(
        engine,
        skygate::core::SkyContext {},
        7U,
        skygate::ephemeris::BodyTrailOptions {
            .pastHours = 1,
            .futureHours = 1,
            .sampleStepMinutes = 30
        }
    );

    QCOMPARE(samples.size(), 5U);
    QCOMPARE(samples[0].offsetMinutes, -60);
    QVERIFY(samples[0].horizontal.has_value());
    QCOMPARE(samples[1].offsetMinutes, -30);
    QVERIFY(!samples[1].horizontal.has_value());
    QCOMPARE(samples[2].offsetMinutes, 0);
    QVERIFY(samples[2].horizontal.has_value());
    QCOMPARE(samples[3].offsetMinutes, 30);
    QVERIFY(!samples[3].horizontal.has_value());
    QCOMPARE(samples[4].offsetMinutes, 60);
    QVERIFY(samples[4].horizontal.has_value());
}

void BodyTrailCalculatorTests::rejectsInvalidOptions()
{
    const FakeTrailEngine engine;
    const skygate::ephemeris::BodyTrailCalculator calculator;
    const auto samples = calculator.sample(
        engine,
        skygate::core::SkyContext {},
        7U,
        skygate::ephemeris::BodyTrailOptions {
            .pastHours = 1,
            .futureHours = 1,
            .sampleStepMinutes = 0
        }
    );

    QVERIFY(samples.empty());
}

QTEST_APPLESS_MAIN(BodyTrailCalculatorTests)

#include "BodyTrailCalculatorTests.moc"
