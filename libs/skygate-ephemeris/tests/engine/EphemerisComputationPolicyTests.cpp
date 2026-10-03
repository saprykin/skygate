#include "engine/EphemerisComputationPolicy.hpp"
#include "EphemerisEngineTestDoubles.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <QtTest/QtTest>

#include <optional>
#include <string_view>

namespace {

[[nodiscard]] skygate::ephemeris::EphemerisRequest makeRequest(
    const skygate::ephemeris::EphemerisEngineKind::Type kind,
    const skygate::ephemeris::EphemerisCorrectionFlags correctionFlags =
        skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric()
)
{
    skygate::ephemeris::EphemerisRequest request;
    request.options.setEngineKind(kind);
    request.options.setCorrectionFlags(correctionFlags);
    request.options.setEnableAtmosphericRefraction(true);
    return request;
}

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeFixedBody()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "fixed";
    body.displayName = "Fixed";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    return body;
}

class ConfigurableTraitsEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit ConfigurableTraitsEngine(skygate::ephemeris::EphemerisEngineTraits traits) : m_traits(traits) {}

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return static_cast<skygate::ephemeris::EphemerisEngineKind::Type>(42);
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Configurable traits test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineTraits traits() const noexcept override
    {
        return m_traits;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        skygate::ephemeris::EphemerisSnapshot snapshot;
        snapshot.context = request.context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest&, std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest&, std::size_t) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::core::ObservationContext& context) const override
    {
        skygate::ephemeris::EphemerisSnapshot snapshot;
        snapshot.context = context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::string_view) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext&, std::size_t) const override
    {
        return std::nullopt;
    }

private:
    skygate::ephemeris::EphemerisEngineTraits m_traits;
};

}  // namespace

class EphemerisComputationPolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void correctionFlagsForAppliesScenePolicyOnlyForSupportedEngines();
    void inspectorDetailRecomputeFollowsTraits();
    void inspectorEventSearchModeFollowsTraitsAndBody();
    void trailSamplingAndGuidanceFollowTraits();
    void liveRecomputeThrottleAppliesFollowsTraits();
};

void EphemerisComputationPolicyTests::correctionFlagsForAppliesScenePolicyOnlyForSupportedEngines()
{
    const skygate::ephemeris::tests::FixedAltitudeEngine noTraitsEngine(0.0);
    const skygate::ephemeris::tests::NonEnumeratedHighPrecisionTraitsEngine highPrecisionTraitsEngine;

    const auto unsupportedRequest = makeRequest(skygate::ephemeris::EphemerisEngineKind::Type::Simple);
    const auto sceneRequest = skygate::ephemeris::EphemerisComputationPolicy::correctionFlagsFor(
        noTraitsEngine, unsupportedRequest, skygate::ephemeris::EphemerisPrecisionPolicy::SceneRender
    );
    QCOMPARE(
        static_cast<std::uint32_t>(sceneRequest.options.correctionFlags()),
        static_cast<std::uint32_t>(unsupportedRequest.options.correctionFlags())
    );

    const auto supportedRequest = makeRequest(highPrecisionTraitsEngine.kind());
    const auto supportedSceneRequest = skygate::ephemeris::EphemerisComputationPolicy::correctionFlagsFor(
        highPrecisionTraitsEngine, supportedRequest, skygate::ephemeris::EphemerisPrecisionPolicy::SceneRender
    );
    const auto expectedCorrections = skygate::ephemeris::EphemerisCorrectionFlags::precessionNutation()
                                     | skygate::ephemeris::EphemerisCorrectionFlags::earthOrientation()
                                     | skygate::ephemeris::EphemerisCorrectionFlags::diurnalParallax()
                                     | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction();
    QCOMPARE(
        static_cast<std::uint32_t>(supportedSceneRequest.options.correctionFlags()),
        static_cast<std::uint32_t>(expectedCorrections)
    );

    const auto detailRequest = skygate::ephemeris::EphemerisComputationPolicy::correctionFlagsFor(
        highPrecisionTraitsEngine, supportedRequest, skygate::ephemeris::EphemerisPrecisionPolicy::SelectionDetail
    );
    QCOMPARE(
        static_cast<std::uint32_t>(detailRequest.options.correctionFlags()),
        static_cast<std::uint32_t>(supportedRequest.options.correctionFlags())
    );
}

void EphemerisComputationPolicyTests::inspectorDetailRecomputeFollowsTraits()
{
    const skygate::ephemeris::tests::FixedAltitudeEngine noTraitsEngine(0.0);
    const skygate::ephemeris::tests::NonEnumeratedHighPrecisionTraitsEngine highPrecisionTraitsEngine;
    const auto request = makeRequest(highPrecisionTraitsEngine.kind());

    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::shouldRecomputeInspectorDetail(noTraitsEngine, request));
    QVERIFY(
        skygate::ephemeris::EphemerisComputationPolicy::shouldRecomputeInspectorDetail(
            highPrecisionTraitsEngine, request
        )
    );
}

void EphemerisComputationPolicyTests::inspectorEventSearchModeFollowsTraitsAndBody()
{
    const skygate::ephemeris::tests::FixedAltitudeEngine noTraitsEngine(0.0);
    const skygate::ephemeris::tests::NonEnumeratedHighPrecisionTraitsEngine highPrecisionTraitsEngine;
    const auto request = makeRequest(highPrecisionTraitsEngine.kind());

    skygate::ephemeris::OwnGalaxyCelestialBody nonFixedBody;
    nonFixedBody.id = "non-fixed";
    nonFixedBody.displayName = "Non-fixed";
    nonFixedBody.kind = skygate::ephemeris::BaseCelestialBody::Kind::Planet;

    const skygate::ephemeris::OwnGalaxyCelestialBody fixedBody = makeFixedBody();

    skygate::ephemeris::EphemerisEngineTraits supportedUntrusted =
        skygate::ephemeris::EphemerisEngineTraits::noTraits();
    supportedUntrusted.supportsGuidedEventSearch = true;
    const ConfigurableTraitsEngine supportedUntrustedEngine(supportedUntrusted);

    skygate::ephemeris::EphemerisEngineTraits trustedUnsupported =
        skygate::ephemeris::EphemerisEngineTraits::noTraits();
    trustedUnsupported.trustsGuidedSearchResult = true;
    const ConfigurableTraitsEngine trustedUnsupportedEngine(trustedUnsupported);

    QCOMPARE(
        skygate::ephemeris::EphemerisComputationPolicy::inspectorEventSearchMode(
            noTraitsEngine, request, &nonFixedBody
        ),
        skygate::ephemeris::ObservationEventCalculator::SearchMode::Guided
    );
    QCOMPARE(
        skygate::ephemeris::EphemerisComputationPolicy::inspectorEventSearchMode(
            highPrecisionTraitsEngine, request, &nonFixedBody
        ),
        skygate::ephemeris::ObservationEventCalculator::SearchMode::GuidedApproximate
    );
    QCOMPARE(
        skygate::ephemeris::EphemerisComputationPolicy::inspectorEventSearchMode(
            highPrecisionTraitsEngine, request, &fixedBody
        ),
        skygate::ephemeris::ObservationEventCalculator::SearchMode::Guided
    );
    QCOMPARE(
        skygate::ephemeris::EphemerisComputationPolicy::inspectorEventSearchMode(
            supportedUntrustedEngine, request, &nonFixedBody
        ),
        skygate::ephemeris::ObservationEventCalculator::SearchMode::Guided
    );
    QCOMPARE(
        skygate::ephemeris::EphemerisComputationPolicy::inspectorEventSearchMode(
            trustedUnsupportedEngine, request, &nonFixedBody
        ),
        skygate::ephemeris::ObservationEventCalculator::SearchMode::Guided
    );
}

void EphemerisComputationPolicyTests::trailSamplingAndGuidanceFollowTraits()
{
    const skygate::ephemeris::tests::FixedAltitudeEngine noTraitsEngine(0.0);
    const skygate::ephemeris::tests::NonEnumeratedHighPrecisionTraitsEngine highPrecisionTraitsEngine;
    const auto request = makeRequest(highPrecisionTraitsEngine.kind());
    const skygate::ephemeris::OwnGalaxyCelestialBody fixedBody = makeFixedBody();

    skygate::ephemeris::EphemerisEngineTraits adaptiveWithoutPermission =
        skygate::ephemeris::EphemerisEngineTraits::noTraits();
    adaptiveWithoutPermission.prefersAdaptiveTrailSampling = true;
    const ConfigurableTraitsEngine adaptiveWithoutPermissionEngine(adaptiveWithoutPermission);

    skygate::ephemeris::EphemerisEngineTraits permissionWithoutAdaptive =
        skygate::ephemeris::EphemerisEngineTraits::noTraits();
    permissionWithoutAdaptive.allowsTrailGuidanceApproximation = true;
    const ConfigurableTraitsEngine permissionWithoutAdaptiveEngine(permissionWithoutAdaptive);

    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::trailSamplingIsAdaptive(noTraitsEngine, request));
    QVERIFY(
        skygate::ephemeris::EphemerisComputationPolicy::trailSamplingIsAdaptive(highPrecisionTraitsEngine, request)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisComputationPolicy::trailSamplingIsAdaptive(
            adaptiveWithoutPermissionEngine, request
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::trailSamplingIsAdaptive(
        permissionWithoutAdaptiveEngine, request
    ));

    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(noTraitsEngine, request, &fixedBody));
    QVERIFY(
        skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(
            highPrecisionTraitsEngine, request, &fixedBody
        )
    );
    QVERIFY(
        !skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(highPrecisionTraitsEngine, request, nullptr)
    );
    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(
        adaptiveWithoutPermissionEngine, request, &fixedBody
    ));
    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(
        permissionWithoutAdaptiveEngine, request, &fixedBody
    ));

    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::trailAllowsGuidanceApproximation(noTraitsEngine));
    QVERIFY(
        skygate::ephemeris::EphemerisComputationPolicy::trailAllowsGuidanceApproximation(highPrecisionTraitsEngine)
    );
    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::trailAllowsGuidanceApproximation(
        adaptiveWithoutPermissionEngine
    ));
    QVERIFY(
        skygate::ephemeris::EphemerisComputationPolicy::trailAllowsGuidanceApproximation(
            permissionWithoutAdaptiveEngine
        )
    );
}

void EphemerisComputationPolicyTests::liveRecomputeThrottleAppliesFollowsTraits()
{
    const skygate::ephemeris::tests::FixedAltitudeEngine noTraitsEngine(0.0);
    const skygate::ephemeris::tests::NonEnumeratedHighPrecisionTraitsEngine highPrecisionTraitsEngine;

    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::liveRecomputeThrottleApplies(noTraitsEngine));
    QVERIFY(skygate::ephemeris::EphemerisComputationPolicy::liveRecomputeThrottleApplies(highPrecisionTraitsEngine));
}

QTEST_APPLESS_MAIN(EphemerisComputationPolicyTests)

#include "EphemerisComputationPolicyTests.moc"
