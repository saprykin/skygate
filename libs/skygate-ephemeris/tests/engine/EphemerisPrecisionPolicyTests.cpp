#include "skygate/ephemeris/EphemerisPrecisionPolicy.hpp"

#include <QtTest/QtTest>

class EphemerisPrecisionPolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void sceneRenderAndTrailsUseLeanTopocentricCorrections();
    void detailAndEventPoliciesPreserveRequestedCorrections();
};

void EphemerisPrecisionPolicyTests::sceneRenderAndTrailsUseLeanTopocentricCorrections()
{
    skygate::ephemeris::EphemerisRequest request;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric;
    request.options.enableAtmosphericRefraction = true;

    const auto sceneRequest = skygate::ephemeris::ephemerisRequestForPrecisionPolicy(
        request, skygate::ephemeris::EphemerisPrecisionPolicy::SceneRender
    );
    const auto trailRequest = skygate::ephemeris::ephemerisRequestForPrecisionPolicy(
        request, skygate::ephemeris::EphemerisPrecisionPolicy::Trail
    );

    const auto expectedCorrections = skygate::ephemeris::EphemerisCorrectionFlags::PrecessionNutation
                                     | skygate::ephemeris::EphemerisCorrectionFlags::EarthOrientation
                                     | skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax
                                     | skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction;
    QCOMPARE(
        static_cast<std::uint32_t>(sceneRequest.options.correctionFlags),
        static_cast<std::uint32_t>(expectedCorrections)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(trailRequest.options.correctionFlags),
        static_cast<std::uint32_t>(expectedCorrections)
    );
}

void EphemerisPrecisionPolicyTests::detailAndEventPoliciesPreserveRequestedCorrections()
{
    skygate::ephemeris::EphemerisRequest request;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Astrometric;
    request.options.enableAtmosphericRefraction = false;

    for (const auto policy : {
             skygate::ephemeris::EphemerisPrecisionPolicy::SelectionDetail,
             skygate::ephemeris::EphemerisPrecisionPolicy::EventSearch,
             skygate::ephemeris::EphemerisPrecisionPolicy::NightConditionsApproximate,
             skygate::ephemeris::EphemerisPrecisionPolicy::NightConditionsVerified,
         }) {
        const auto policyRequest = skygate::ephemeris::ephemerisRequestForPrecisionPolicy(request, policy);
        QCOMPARE(
            static_cast<std::uint32_t>(policyRequest.options.correctionFlags),
            static_cast<std::uint32_t>(request.options.correctionFlags)
        );
        QVERIFY(!policyRequest.options.enableAtmosphericRefraction);
    }
}

QTEST_APPLESS_MAIN(EphemerisPrecisionPolicyTests)

#include "EphemerisPrecisionPolicyTests.moc"
