#include "EphemerisEngineTestDoubles.hpp"
#include "EphemerisRequestFactory.hpp"
#include "MarkerGuidanceEngine.hpp"
#include "MarkerGuidanceStrategy.hpp"
#include "ObservationEventCalculator.hpp"
#include "ObservationEventTestSupport.hpp"
#include "UtcTimeCodec.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace {

using skygate::ephemeris::tests::kPi;
using skygate::ephemeris::tests::makeContext;
using skygate::ephemeris::tests::makeFixedBody;
using skygate::ephemeris::tests::makeRequest;
using skygate::ephemeris::tests::MarkerGuidanceEngine;
using skygate::ephemeris::tests::MarkerGuidanceStrategy;
using skygate::ephemeris::tests::TrustOptOutPrimaryEngine;
using SearchMode = skygate::ephemeris::ObservationEventCalculator::SearchMode;

void verifyPrimaryCrossingAltitude(
    const skygate::ephemeris::IEphemerisEngine& engine,
    const skygate::ephemeris::EphemerisRequest& request,
    const skygate::core::UtcTimePoint& utcTime
)
{
    const auto verificationRequest = skygate::ephemeris::EphemerisRequestFactory::atUtcTime(request, utcTime);
    const auto state = engine.computeBodyState(verificationRequest, std::size_t{0U});
    QVERIFY(state.has_value());
    QVERIFY(std::abs(state->horizontal.altitudeDeg - 0.0) < 1.0);
}

class OffsetGuidanceEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit OffsetGuidanceEngine(const double offsetDeg) : m_offsetDeg(offsetDeg) {}

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Offset guidance test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return compute(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view) const override
    {
        return computeBodyState(request, std::size_t{0U});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        const double seconds = skygate::core::UtcTimeCodec::secondsSinceEpochDouble(request.context.utcTime);
        const double phase = std::fmod(seconds, 86400.0) / 86400.0;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
            .horizontal = {
                .altitudeDeg = 35.0 * std::sin(2.0 * kPi * (phase - 0.25)) + m_offsetDeg, .azimuthDeg = 180.0
            }
        };
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot compute(const skygate::core::ObservationContext&) const override
    {
        return {};
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, const std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::size_t) const override
    {
        return std::nullopt;
    }

private:
    double m_offsetDeg;
};

class OffsetGuidanceStrategy final : public skygate::ephemeris::IEphemerisGuidanceStrategy {
public:
    explicit OffsetGuidanceStrategy(const double offsetDeg) : m_offsetDeg(offsetDeg) {}

    [[nodiscard]] std::unique_ptr<skygate::ephemeris::IEphemerisEngine>
    createGuidanceEngine(const skygate::ephemeris::CelestialBodyCatalog&) const override
    {
        return std::make_unique<OffsetGuidanceEngine>(m_offsetDeg);
    }

private:
    double m_offsetDeg;
};

}  // namespace

class ObservationEventPrimaryEngineTests final : public QObject {
    Q_OBJECT

private slots:
    void untrustedFixedStarGuidedSearchVerifiesPrimary();
    void untrustedGuidedApproximateVerifiesPrimary();
    void trustedFixedStarGuidedSkipsPrimaryVerification();
    void untrustedGuidedAlwaysAboveFallsBackToPrimary();
    void untrustedGuidedApproximateAlwaysAboveFallback();
    void untrustedGuidedAlwaysBelowFallsBackToPrimary();
    void untrustedGuidedApproximateWindowMissFallback();
    void trustedGuidedApproximateFixedBodySkipsPrimary();
};

void ObservationEventPrimaryEngineTests::untrustedFixedStarGuidedSearchVerifiesPrimary()
{
    const auto strategy = std::make_shared<MarkerGuidanceStrategy>();
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0});
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, false);

    const auto request = makeRequest(makeContext(), engine);
    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(*primaryCall > 0);

    const auto verificationRequest =
        skygate::ephemeris::EphemerisRequestFactory::atUtcTime(request, *summary.nextRise.utcTime);
    const auto state = engine.computeBodyState(verificationRequest, std::size_t{0U});
    QVERIFY(state.has_value());
    QVERIFY(std::abs(state->horizontal.altitudeDeg - 0.0) < 1.0);
}

void ObservationEventPrimaryEngineTests::untrustedGuidedApproximateVerifiesPrimary()
{
    const auto strategy = std::make_shared<MarkerGuidanceStrategy>();
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0});
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, false);

    const auto request = makeRequest(makeContext(), engine);
    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::GuidedApproximate);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(*primaryCall > 0);

    const auto verificationRequest =
        skygate::ephemeris::EphemerisRequestFactory::atUtcTime(request, *summary.nextRise.utcTime);
    const auto state = engine.computeBodyState(verificationRequest, std::size_t{0U});
    QVERIFY(state.has_value());
    QVERIFY(std::abs(state->horizontal.altitudeDeg - 0.0) < 1.0);
}

void ObservationEventPrimaryEngineTests::trustedFixedStarGuidedSkipsPrimaryVerification()
{
    const auto strategy = std::make_shared<MarkerGuidanceStrategy>();
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0});
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, true);

    const auto request = makeRequest(makeContext(), engine);
    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QCOMPARE(*primaryCall, 0);

    const MarkerGuidanceEngine guidance(std::make_shared<int>(0));
    const auto guidanceRequest =
        skygate::ephemeris::EphemerisRequestFactory::atUtcTime(request, *summary.nextRise.utcTime);
    const auto state = guidance.computeBodyState(guidanceRequest, std::size_t{0U});
    QVERIFY(state.has_value());
    QVERIFY(std::abs(state->horizontal.altitudeDeg - 0.0) < 0.5);
}

void ObservationEventPrimaryEngineTests::untrustedGuidedAlwaysAboveFallsBackToPrimary()
{
    const auto strategy = std::make_shared<OffsetGuidanceStrategy>(40.0);
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 42.7});
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, false);

    const auto request = makeRequest(makeContext(47.3769), engine);
    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(summary.nextSet.utcTime.has_value());
    QVERIFY(*primaryCall > 0);
    verifyPrimaryCrossingAltitude(engine, request, *summary.nextRise.utcTime);
    verifyPrimaryCrossingAltitude(engine, request, *summary.nextSet.utcTime);
}

void ObservationEventPrimaryEngineTests::untrustedGuidedApproximateAlwaysAboveFallback()
{
    const auto strategy = std::make_shared<OffsetGuidanceStrategy>(40.0);
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 42.7});
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, false);

    const auto request = makeRequest(makeContext(47.3769), engine);
    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::GuidedApproximate);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(summary.nextSet.utcTime.has_value());
    QVERIFY(*primaryCall > 0);
    verifyPrimaryCrossingAltitude(engine, request, *summary.nextRise.utcTime);
    verifyPrimaryCrossingAltitude(engine, request, *summary.nextSet.utcTime);
}

void ObservationEventPrimaryEngineTests::untrustedGuidedAlwaysBelowFallsBackToPrimary()
{
    const auto strategy = std::make_shared<OffsetGuidanceStrategy>(-40.0);
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = -42.7});
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, false);

    const auto request = makeRequest(makeContext(47.3769), engine);
    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::Guided);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(summary.nextSet.utcTime.has_value());
    QVERIFY(*primaryCall > 0);
    verifyPrimaryCrossingAltitude(engine, request, *summary.nextRise.utcTime);
    verifyPrimaryCrossingAltitude(engine, request, *summary.nextSet.utcTime);
}

void ObservationEventPrimaryEngineTests::untrustedGuidedApproximateWindowMissFallback()
{
    const auto strategy = std::make_shared<OffsetGuidanceStrategy>(40.0);
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 20.0});
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, false);

    const auto request = makeRequest(makeContext(), engine);
    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::GuidedApproximate);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::Available);
    QVERIFY(summary.nextRise.utcTime.has_value());
    QVERIFY(summary.nextSet.utcTime.has_value());
    QVERIFY(*primaryCall > 0);
    verifyPrimaryCrossingAltitude(engine, request, *summary.nextRise.utcTime);
    verifyPrimaryCrossingAltitude(engine, request, *summary.nextSet.utcTime);
}

void ObservationEventPrimaryEngineTests::trustedGuidedApproximateFixedBodySkipsPrimary()
{
    const auto strategy = std::make_shared<OffsetGuidanceStrategy>(40.0);
    const skygate::ephemeris::ObservationEventCalculator calculator(strategy);
    const auto body = makeFixedBody({.rightAscensionHours = 8.0, .declinationDeg = 42.7});
    auto primaryCall = std::make_shared<int>(0);
    const TrustOptOutPrimaryEngine engine(primaryCall, true);

    const auto request = makeRequest(makeContext(47.3769), engine);
    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::GuidedApproximate);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::AlwaysAbove);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::AlwaysAbove);
    QCOMPARE(*primaryCall, 0);
}

QTEST_APPLESS_MAIN(ObservationEventPrimaryEngineTests)

#include "ObservationEventPrimaryEngineTests.moc"
