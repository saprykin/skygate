#include "EphemerisRequest.hpp"
#include "EphemerisEngineTestDoubles.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"

#include <QtTest/QtTest>

class EphemerisPrecisionPolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void sceneRenderAndTrailsUseLeanCorrections();
    void detailAndEventPoliciesPreserveCorrections();
};

void EphemerisPrecisionPolicyTests::sceneRenderAndTrailsUseLeanCorrections()
{
    const skygate::ephemeris::tests::NonEnumeratedHighPrecisionTraitsEngine engine;
    skygate::ephemeris::EphemerisRequest request;
    request.options.setEngineKind(engine.kind());
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric());
    request.options.setEnableAtmosphericRefraction(true);

    const auto sceneRequest = skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(
        request, skygate::ephemeris::EphemerisPrecisionPolicy::SceneRender, engine
    );
    const auto trailRequest = skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(
        request, skygate::ephemeris::EphemerisPrecisionPolicy::Trail, engine
    );

    const auto leanCorrections = skygate::ephemeris::EphemerisCorrectionFlags::precessionNutation()
                                 | skygate::ephemeris::EphemerisCorrectionFlags::earthOrientation()
                                 | skygate::ephemeris::EphemerisCorrectionFlags::diurnalParallax();
    QCOMPARE(
        static_cast<std::uint32_t>(sceneRequest.options.correctionFlags()),
        static_cast<std::uint32_t>(
            leanCorrections | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
    QVERIFY(sceneRequest.options.requestsAtmosphericRefraction());
    QCOMPARE(
        static_cast<std::uint32_t>(trailRequest.options.correctionFlags()), static_cast<std::uint32_t>(leanCorrections)
    );
    QVERIFY(!trailRequest.options.requestsAtmosphericRefraction());
}

void EphemerisPrecisionPolicyTests::detailAndEventPoliciesPreserveCorrections()
{
    const skygate::ephemeris::tests::FixedAltitudeEngine engine(0.0);
    skygate::ephemeris::EphemerisRequest request;
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    request.options.setEnableAtmosphericRefraction(false);

    for (const auto policy : {
             skygate::ephemeris::EphemerisPrecisionPolicy::SelectionDetail,
             skygate::ephemeris::EphemerisPrecisionPolicy::EventSearch,
             skygate::ephemeris::EphemerisPrecisionPolicy::NightConditionsApproximate,
             skygate::ephemeris::EphemerisPrecisionPolicy::NightConditionsVerified,
         }) {
        const auto policyRequest = skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(request, policy, engine);
        QCOMPARE(
            static_cast<std::uint32_t>(policyRequest.options.correctionFlags()),
            static_cast<std::uint32_t>(request.options.correctionFlags())
        );
        QVERIFY(!policyRequest.options.enableAtmosphericRefraction());
    }
}

QTEST_APPLESS_MAIN(EphemerisPrecisionPolicyTests)

#include "EphemerisPrecisionPolicyTests.moc"
