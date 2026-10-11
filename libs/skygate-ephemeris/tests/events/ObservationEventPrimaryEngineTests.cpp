#include "EphemerisRequestFactory.hpp"
#include "MarkerGuidanceEngine.hpp"
#include "MarkerGuidanceStrategy.hpp"
#include "ObservationEventCalculator.hpp"
#include "UtcTimeCodec.hpp"
#include "engine/IEphemerisEngine.hpp"

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

constexpr double kPi = 3.14159265358979323846;

skygate::core::ObservationContext makeContext(const double latitudeDeg = 47.0, const double longitudeDeg = 8.0)
{
    skygate::core::ObservationContext context;
    context.observer = {.latitudeDeg = latitudeDeg, .longitudeDeg = longitudeDeg, .elevationMeters = 400.0};
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'717'276'800));
    return context;
}

skygate::ephemeris::OwnGalaxyCelestialBody makeFixedBody(const skygate::core::EquatorialCoordinate& equatorial)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "target";
    body.displayName = "Target";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.visualMagnitude = 1.0;
    body.fixedEquatorial = equatorial;
    return body;
}

skygate::ephemeris::EphemerisRequest
makeRequest(const skygate::core::ObservationContext& context, const skygate::ephemeris::IEphemerisEngine& engine)
{
    return skygate::ephemeris::EphemerisRequestFactory::requestFromContext(context, engine.options());
}

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

class TrustOptOutEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    TrustOptOutEngine(std::shared_ptr<int> primaryCall, const bool trustGuidance)
        : m_primaryCall(std::move(primaryCall)), m_trustGuidance(trustGuidance)
    {
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Trust opt-out primary test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineTraits traits() const noexcept override
    {
        skygate::ephemeris::EphemerisEngineTraits traits = skygate::ephemeris::EphemerisEngineTraits::noTraits();
        traits.supportsGuidedEventSearch = true;
        traits.trustsGuidedSearchResult = m_trustGuidance;
        return traits;
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
        if (bodyIndex != 0U) {
            return std::nullopt;
        }

        ++*m_primaryCall;
        return altitudeState(static_cast<std::uint32_t>(bodyIndex), request.context.utcTime);
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::core::ObservationContext& context) const override
    {
        skygate::ephemeris::EphemerisSnapshot snapshot;
        snapshot.context = context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, const std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, const std::size_t) const override
    {
        return std::nullopt;
    }

private:
    [[nodiscard]] static skygate::ephemeris::CelestialBodyState
    altitudeState(const std::uint32_t bodyIndex, const skygate::core::UtcTimePoint& utcTime) noexcept
    {
        const double seconds = skygate::core::UtcTimeCodec::secondsSinceEpochDouble(utcTime);
        const double phase = std::fmod(seconds, 86400.0) / 86400.0;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = bodyIndex,
            .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
            .horizontal = {.altitudeDeg = 35.0 * std::sin(2.0 * kPi * (phase - 0.25)) + 5.0, .azimuthDeg = 180.0}
        };
    }

    std::shared_ptr<int> m_primaryCall;
    bool m_trustGuidance;
};

using skygate::ephemeris::tests::MarkerGuidanceEngine;
using skygate::ephemeris::tests::MarkerGuidanceStrategy;
using SearchMode = skygate::ephemeris::ObservationEventCalculator::SearchMode;

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
    const TrustOptOutEngine engine(primaryCall, false);

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
    const TrustOptOutEngine engine(primaryCall, false);

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
    const TrustOptOutEngine engine(primaryCall, true);

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
    const TrustOptOutEngine engine(primaryCall, false);

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
    const TrustOptOutEngine engine(primaryCall, false);

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
    const TrustOptOutEngine engine(primaryCall, false);

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
    const TrustOptOutEngine engine(primaryCall, false);

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
    const TrustOptOutEngine engine(primaryCall, true);

    const auto request = makeRequest(makeContext(47.3769), engine);
    const auto summary = calculator.compute(engine, request, 0U, &body, 0.0, SearchMode::GuidedApproximate);

    QCOMPARE(summary.nextRise.status, skygate::ephemeris::ObservationEventStatus::Type::AlwaysAbove);
    QCOMPARE(summary.nextSet.status, skygate::ephemeris::ObservationEventStatus::Type::AlwaysAbove);
    QCOMPARE(*primaryCall, 0);
}

QTEST_APPLESS_MAIN(ObservationEventPrimaryEngineTests)

#include "ObservationEventPrimaryEngineTests.moc"
