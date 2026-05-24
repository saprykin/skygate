#include "UtcTimeCodec.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr double kSecondsPerDay = 86'400.0;
constexpr double kUnixEpochJulianDay = 2'440'587.5;

skygate::ephemeris::AstronomicalEpoch epochFromUtc(const skygate::core::UtcTimePoint& utcTime)
{
    const double julianDay =
        skygate::core::UtcTimeCodec::secondsSinceEpochDouble(utcTime) / kSecondsPerDay + kUnixEpochJulianDay;
    const double julianDatePart1 = std::floor(julianDay);
    return {
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDay - julianDatePart1,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    };
}

skygate::ephemeris::CelestialBody makeTargetBody()
{
    return {
        .id = "target",
        .displayName = "Target",
        .type = skygate::ephemeris::CelestialBodyType::Star,
        .visualMagnitude = 1.0,
        .fixedEquatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = 3.0,
            .declinationDeg = 4.0,
        },
    };
}

skygate::ephemeris::CelestialBodyState
makeState(const skygate::ephemeris::EphemerisRequest& request, const std::uint32_t bodyIndex)
{
    skygate::ephemeris::CelestialBodyState state;
    state.bodyIndex = bodyIndex;
    state.equatorial = {
        .rightAscensionHours = request.epoch.julianDatePart2,
        .declinationDeg = static_cast<double>(bodyIndex),
    };
    state.horizontal = {
        .altitudeDeg = request.context.observer.latitudeDeg,
        .azimuthDeg = request.context.observer.longitudeDeg,
    };
    state.metadata.appliedCorrections = request.options.correctionFlags;
    return state;
}

class MetadataDefaultEngine final : public skygate::ephemeris::IEphemerisEngine {
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
        if (bodyIndex > std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }
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

class RequestAwareTestEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        skygate::ephemeris::EphemerisEngineOptions engineOptions;
        engineOptions.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
        engineOptions.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections;
        engineOptions.enableAtmosphericRefraction = false;
        return engineOptions;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        ++m_requestComputeCount;
        m_lastRequestOptions = request.options;
        m_lastRequestEpoch = request.epoch;

        auto bodies = std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
            std::vector<skygate::ephemeris::CelestialBody>{makeTargetBody()}
        );

        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = request.context;
        snapshot.catalogBodies = std::move(bodies);
        snapshot.states.push_back(makeState(request, 0U));
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view bodyId) const override
    {
        ++m_requestBodyLookupCount;
        m_lastRequestOptions = request.options;
        m_lastRequestEpoch = request.epoch;
        if (bodyId != "target") {
            return std::nullopt;
        }
        return makeState(request, 0U);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        ++m_requestBodyLookupCount;
        m_lastRequestOptions = request.options;
        m_lastRequestEpoch = request.epoch;
        if (bodyIndex != 0U || bodyIndex > std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }
        return makeState(request, static_cast<std::uint32_t>(bodyIndex));
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        return compute(makeCompatibilityRequest(context));
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::string_view bodyId) const override
    {
        return computeBodyState(makeCompatibilityRequest(context), bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::uint32_t bodyIndex) const override
    {
        return computeBodyState(makeCompatibilityRequest(context), static_cast<std::size_t>(bodyIndex));
    }

    [[nodiscard]] int requestComputeCount() const noexcept
    {
        return m_requestComputeCount;
    }

    [[nodiscard]] int requestBodyLookupCount() const noexcept
    {
        return m_requestBodyLookupCount;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions lastRequestOptions() const noexcept
    {
        return m_lastRequestOptions;
    }

    [[nodiscard]] skygate::ephemeris::AstronomicalEpoch lastRequestEpoch() const noexcept
    {
        return m_lastRequestEpoch;
    }

private:
    [[nodiscard]] skygate::ephemeris::EphemerisRequest
    makeCompatibilityRequest(const skygate::core::SkyContext& context) const noexcept
    {
        skygate::ephemeris::EphemerisRequest request;
        request.context = context;
        request.epoch = epochFromUtc(context.utcTime);
        request.options = options();
        return request;
    }

    mutable int m_requestComputeCount = 0;
    mutable int m_requestBodyLookupCount = 0;
    mutable skygate::ephemeris::EphemerisEngineOptions m_lastRequestOptions;
    mutable skygate::ephemeris::AstronomicalEpoch m_lastRequestEpoch;
};

skygate::core::SkyContext makeContext()
{
    skygate::core::SkyContext context;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 47.3769,
        .longitudeDeg = 8.5417,
        .elevationMeters = 408.0,
    };
    return context;
}

}  // namespace

class EphemerisEngineInterfaceMigrationTests final : public QObject {
    Q_OBJECT

private slots:
    void metadataDefaultsRemainAvailableForTestEngines();
    void requestComputeReceivesFullRequest();
    void requestBodyLookupSupportsIdAndIndex();
    void skyContextCompatibilityUsesEngineDefaultOptions();
};

void EphemerisEngineInterfaceMigrationTests::metadataDefaultsRemainAvailableForTestEngines()
{
    const MetadataDefaultEngine engine;

    QCOMPARE(
        static_cast<std::uint8_t>(engine.kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QVERIFY(engine.name() == std::string_view{"Ephemeris engine"});

    const auto capabilities = engine.capabilities();
    QCOMPARE(
        static_cast<std::uint8_t>(capabilities.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QVERIFY(!capabilities.supportsSolarSystemBodies);
    QVERIFY(engine.supportedDateRanges().empty());
    QVERIFY(engine.dataSetInfo().id.empty());

    const auto options = engine.options();
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
}

void EphemerisEngineInterfaceMigrationTests::requestComputeReceivesFullRequest()
{
    RequestAwareTestEngine engine;

    skygate::ephemeris::EphemerisRequest request;
    request.context = makeContext();
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1234));
    request.epoch = {.julianDatePart1 = 2'460'000.0, .julianDatePart2 = 0.75};
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;

    const auto snapshot = engine.compute(request);

    QCOMPARE(engine.requestComputeCount(), 1);
    QCOMPARE(skygate::core::UtcTimeCodec::toEpochSecondsFloor(snapshot.context.utcTime), 1234);
    QCOMPARE(snapshot.states.size(), std::size_t{1});
    QCOMPARE(
        static_cast<std::uint32_t>(snapshot.states.front().metadata.appliedCorrections),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::LightTime)
    );
    QCOMPARE(engine.lastRequestEpoch().julianDatePart1, 2'460'000.0);
    QCOMPARE(engine.lastRequestEpoch().julianDatePart2, 0.75);
}

void EphemerisEngineInterfaceMigrationTests::requestBodyLookupSupportsIdAndIndex()
{
    RequestAwareTestEngine engine;

    skygate::ephemeris::EphemerisRequest request;
    request.context = makeContext();
    request.epoch = {.julianDatePart1 = 2'460'000.0, .julianDatePart2 = 0.25};
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax;

    const auto byId = engine.computeBodyState(request, "target");
    QVERIFY(byId.has_value());
    QCOMPARE(byId->bodyIndex, 0U);
    QCOMPARE(
        static_cast<std::uint32_t>(byId->metadata.appliedCorrections),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax)
    );

    const auto byIndex = engine.computeBodyState(request, std::size_t{0});
    QVERIFY(byIndex.has_value());
    QCOMPARE(byIndex->bodyIndex, 0U);
    QVERIFY(!engine.computeBodyState(request, "missing").has_value());
    QVERIFY(!engine.computeBodyState(request, std::size_t{1}).has_value());
    QCOMPARE(engine.requestBodyLookupCount(), 4);
}

void EphemerisEngineInterfaceMigrationTests::skyContextCompatibilityUsesEngineDefaultOptions()
{
    RequestAwareTestEngine engine;
    const auto context = makeContext();

    const auto snapshot = engine.compute(context);
    QCOMPARE(engine.requestComputeCount(), 1);
    QCOMPARE(snapshot.states.size(), std::size_t{1});
    QCOMPARE(
        static_cast<std::uint32_t>(snapshot.states.front().metadata.appliedCorrections),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(engine.lastRequestOptions().correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(engine.lastRequestEpoch().timeScale),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Utc)
    );

    const auto bodyState = engine.computeBodyState(context, "target");
    QVERIFY(bodyState.has_value());
    QCOMPARE(
        static_cast<std::uint32_t>(bodyState->metadata.appliedCorrections),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections)
    );
}

QTEST_APPLESS_MAIN(EphemerisEngineInterfaceMigrationTests)

#include "EphemerisEngineInterfaceMigrationTests.moc"
