#include "skygate/ephemeris/BodyTrailCalculator.hpp"
#include "skygate/core/UtcTimeCodec.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace {

class FakeTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return compute(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::string_view bodyId) const override
    {
        return computeBodyState(request.context, bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        return computeBodyState(request.context, static_cast<std::uint32_t>(bodyIndex));
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        return skygate::ephemeris::SkySnapshot{.context = context};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::uint32_t bodyIndex) const override
    {
        const auto offset =
            std::chrono::duration_cast<std::chrono::minutes>(context.utcTime.time_since_epoch()).count();
        if (offset == -30) {
            return std::nullopt;
        }

        skygate::ephemeris::CelestialBodyState state{
            .bodyIndex = bodyIndex,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
            .horizontal = {.altitudeDeg = static_cast<double>(offset / 30), .azimuthDeg = 120.0}
        };
        if (offset == 30) {
            state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
        }
        return state;
    }
};

class RequestAwareTrailEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    struct RequestSample final {
        std::int64_t utcSeconds = 0;
        skygate::ephemeris::AstronomicalEpoch epoch;
        skygate::ephemeris::EphemerisEngineOptions options;
        std::size_t bodyIndex = 0U;
    };

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return skygate::ephemeris::SkySnapshot{.context = request.context};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::string_view) const override
    {
        return computeBodyState(request, std::size_t{0});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        m_requestSamples.push_back(
            RequestSample{
                .utcSeconds = skygate::core::UtcTimeCodec::toEpochSecondsFloor(request.context.utcTime),
                .epoch = request.epoch,
                .options = request.options,
                .bodyIndex = bodyIndex
            }
        );
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex), .horizontal = {.altitudeDeg = 42.0, .azimuthDeg = 120.0}
        };
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        return skygate::ephemeris::SkySnapshot{.context = context};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
    {
        m_usedContextPath = true;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::uint32_t) const override
    {
        m_usedContextPath = true;
        return std::nullopt;
    }

    [[nodiscard]] const std::vector<RequestSample>& requestSamples() const noexcept
    {
        return m_requestSamples;
    }

    [[nodiscard]] bool usedContextPath() const noexcept
    {
        return m_usedContextPath;
    }

private:
    mutable std::vector<RequestSample> m_requestSamples;
    mutable bool m_usedContextPath = false;
};

}  // namespace

class BodyTrailCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void samplesOffsetsAndPreservesInvalidGaps();
    void samplesCurrentInstantForZeroWindow();
    void preservesMissingBodySamplesAsGaps();
    void stopsBeforeEndWhenStepDoesNotDivideWindow();
    void requestSamplingPreservesOptionsAndUpdatesEpochs();
    void rejectsInvalidOptions();
};

void BodyTrailCalculatorTests::samplesOffsetsAndPreservesInvalidGaps()
{
    const FakeTrailEngine engine;
    const skygate::ephemeris::BodyTrailCalculator calculator;
    const auto samples = calculator.sample(
        engine,
        skygate::core::SkyContext{},
        7U,
        skygate::ephemeris::BodyTrailOptions{.pastHours = 1, .futureHours = 1, .sampleStepMinutes = 30}
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

void BodyTrailCalculatorTests::samplesCurrentInstantForZeroWindow()
{
    const FakeTrailEngine engine;
    const skygate::ephemeris::BodyTrailCalculator calculator;
    const auto samples = calculator.sample(
        engine,
        skygate::core::SkyContext{},
        7U,
        skygate::ephemeris::BodyTrailOptions{.pastHours = 0, .futureHours = 0, .sampleStepMinutes = 30}
    );

    QCOMPARE(samples.size(), 1U);
    QCOMPARE(samples[0].offsetMinutes, 0);
    QVERIFY(samples[0].horizontal.has_value());
}

void BodyTrailCalculatorTests::preservesMissingBodySamplesAsGaps()
{
    class MissingBodyEngine final : public skygate::ephemeris::IEphemerisEngine {
    public:
        [[nodiscard]] skygate::ephemeris::SkySnapshot
        compute(const skygate::ephemeris::EphemerisRequest& request) const override
        {
            return compute(request.context);
        }

        [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
        computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::string_view bodyId) const override
        {
            return computeBodyState(request.context, bodyId);
        }

        [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState> computeBodyState(
            const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex
        ) const override
        {
            return computeBodyState(request.context, static_cast<std::uint32_t>(bodyIndex));
        }

        [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
        {
            return skygate::ephemeris::SkySnapshot{.context = context};
        }

        [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
        computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
        {
            return std::nullopt;
        }

        [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
        computeBodyState(const skygate::core::SkyContext&, std::uint32_t) const override
        {
            return std::nullopt;
        }
    };

    const MissingBodyEngine engine;
    const skygate::ephemeris::BodyTrailCalculator calculator;
    const auto samples = calculator.sample(
        engine,
        skygate::core::SkyContext{},
        99U,
        skygate::ephemeris::BodyTrailOptions{.pastHours = 1, .futureHours = 0, .sampleStepMinutes = 30}
    );

    QCOMPARE(samples.size(), 3U);
    QCOMPARE(samples[0].offsetMinutes, -60);
    QCOMPARE(samples[1].offsetMinutes, -30);
    QCOMPARE(samples[2].offsetMinutes, 0);
    QVERIFY(!samples[0].horizontal.has_value());
    QVERIFY(!samples[1].horizontal.has_value());
    QVERIFY(!samples[2].horizontal.has_value());
}

void BodyTrailCalculatorTests::stopsBeforeEndWhenStepDoesNotDivideWindow()
{
    const FakeTrailEngine engine;
    const skygate::ephemeris::BodyTrailCalculator calculator;
    const auto samples = calculator.sample(
        engine,
        skygate::core::SkyContext{},
        7U,
        skygate::ephemeris::BodyTrailOptions{.pastHours = 1, .futureHours = 0, .sampleStepMinutes = 40}
    );

    QCOMPARE(samples.size(), 2U);
    QCOMPARE(samples[0].offsetMinutes, -60);
    QCOMPARE(samples[1].offsetMinutes, -20);
}

void BodyTrailCalculatorTests::requestSamplingPreservesOptionsAndUpdatesEpochs()
{
    const RequestAwareTrailEngine engine;
    const skygate::ephemeris::BodyTrailCalculator calculator;

    skygate::ephemeris::EphemerisRequest request;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(600));
    request.epoch = skygate::ephemeris::AstronomicalEpoch{
        .julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25, .timeScale = skygate::ephemeris::TimeScale::Utc
    };
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric;
    request.options.enableAtmosphericRefraction = false;

    const auto samples = calculator.sample(
        engine,
        request,
        11U,
        skygate::ephemeris::BodyTrailOptions{.pastHours = 1, .futureHours = 0, .sampleStepMinutes = 30}
    );

    QCOMPARE(samples.size(), 3U);
    QVERIFY(!engine.usedContextPath());
    QCOMPARE(engine.requestSamples().size(), 3U);
    QCOMPARE(engine.requestSamples()[0].bodyIndex, std::size_t{11});
    QCOMPARE(engine.requestSamples()[0].utcSeconds, std::int64_t{-3000});
    QCOMPARE(engine.requestSamples()[1].utcSeconds, std::int64_t{-1200});
    QCOMPARE(engine.requestSamples()[2].utcSeconds, std::int64_t{600});
    QCOMPARE(
        static_cast<std::uint8_t>(engine.requestSamples()[0].options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(engine.requestSamples()[0].options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric)
    );
    QVERIFY(!engine.requestSamples()[0].options.enableAtmosphericRefraction);
    QCOMPARE(engine.requestSamples()[0].epoch.julianDatePart1, 2'451'545.0);
    QVERIFY(std::abs(engine.requestSamples()[0].epoch.julianDatePart2 - (0.25 - (1.0 / 24.0))) < 1e-12);
    QVERIFY(std::abs(engine.requestSamples()[1].epoch.julianDatePart2 - (0.25 - (0.5 / 24.0))) < 1e-12);
    QCOMPARE(engine.requestSamples()[2].epoch.julianDatePart2, 0.25);
}

void BodyTrailCalculatorTests::rejectsInvalidOptions()
{
    const FakeTrailEngine engine;
    const skygate::ephemeris::BodyTrailCalculator calculator;
    const auto samples = calculator.sample(
        engine,
        skygate::core::SkyContext{},
        7U,
        skygate::ephemeris::BodyTrailOptions{.pastHours = 1, .futureHours = 1, .sampleStepMinutes = 0}
    );

    QVERIFY(samples.empty());
}

QTEST_APPLESS_MAIN(BodyTrailCalculatorTests)

#include "BodyTrailCalculatorTests.moc"
