#include "EphemerisRequest.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"

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
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric());
    request.options.setEnableAtmosphericRefraction(true);

    const auto sceneRequest = skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(
        request, skygate::ephemeris::EphemerisPrecisionPolicy::SceneRender
    );
    const auto trailRequest = skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(
        request, skygate::ephemeris::EphemerisPrecisionPolicy::Trail
    );

    const auto expectedCorrections = skygate::ephemeris::EphemerisCorrectionFlags::precessionNutation()
                                     | skygate::ephemeris::EphemerisCorrectionFlags::earthOrientation()
                                     | skygate::ephemeris::EphemerisCorrectionFlags::diurnalParallax()
                                     | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction();
    QCOMPARE(
        static_cast<std::uint32_t>(sceneRequest.options.correctionFlags()),
        static_cast<std::uint32_t>(expectedCorrections)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(trailRequest.options.correctionFlags()),
        static_cast<std::uint32_t>(expectedCorrections)
    );
}

void EphemerisPrecisionPolicyTests::detailAndEventPoliciesPreserveRequestedCorrections()
{
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
        const auto policyRequest = skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(request, policy);
        QCOMPARE(
            static_cast<std::uint32_t>(policyRequest.options.correctionFlags()),
            static_cast<std::uint32_t>(request.options.correctionFlags())
        );
        QVERIFY(!policyRequest.options.enableAtmosphericRefraction());
    }
}

QTEST_APPLESS_MAIN(EphemerisPrecisionPolicyTests)

#include "EphemerisPrecisionPolicyTests.moc"
