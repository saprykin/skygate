#include "engine/EphemerisComputationPolicy.hpp"
#include "EphemerisEngineTestDoubles.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <QtTest/QtTest>

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
}

void EphemerisComputationPolicyTests::trailSamplingAndGuidanceFollowTraits()
{
    const skygate::ephemeris::tests::FixedAltitudeEngine noTraitsEngine(0.0);
    const skygate::ephemeris::tests::NonEnumeratedHighPrecisionTraitsEngine highPrecisionTraitsEngine;
    const auto request = makeRequest(highPrecisionTraitsEngine.kind());
    const skygate::ephemeris::OwnGalaxyCelestialBody fixedBody = makeFixedBody();

    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::trailSamplingIsAdaptive(noTraitsEngine, request));
    QVERIFY(
        skygate::ephemeris::EphemerisComputationPolicy::trailSamplingIsAdaptive(highPrecisionTraitsEngine, request)
    );

    QVERIFY(!skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(noTraitsEngine, request, &fixedBody));
    QVERIFY(
        skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(
            highPrecisionTraitsEngine, request, &fixedBody
        )
    );
    QVERIFY(
        !skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(highPrecisionTraitsEngine, request, nullptr)
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
