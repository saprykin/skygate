#include "factory/EphemerisEngineReplacementPolicy.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "factory/EphemerisEngineFactoryRequest.hpp"
#include "factory/EphemerisEngineFactoryResult.hpp"

#include <QtTest/QtTest>

#include <memory>
#include <utility>

namespace {

using skygate::ephemeris::EphemerisEngineFactory;
using skygate::ephemeris::EphemerisEngineFactoryRequest;
using skygate::ephemeris::EphemerisEngineFactoryResult;
using skygate::ephemeris::EphemerisEngineKind;
using skygate::ephemeris::EphemerisEngineReplacementPolicy;
using skygate::ephemeris::EphemerisFactoryCreationStatus;

constexpr EphemerisEngineKind::Type kNonEnumeratedEngineKind = static_cast<EphemerisEngineKind::Type>(42);

[[nodiscard]] EphemerisEngineFactoryResult makeSimpleFallbackResult()
{
    auto created = EphemerisEngineFactory::create(EphemerisEngineFactoryRequest{});
    Q_ASSERT(EphemerisFactoryCreationStatus::isSuccess(created.status));
    return EphemerisEngineFactoryResult::success(
        std::move(created.engine), EphemerisFactoryCreationStatus::Type::CreatedSimpleFallback
    );
}

[[nodiscard]] EphemerisEngineFactoryResult makeRequestedEngineResult()
{
    auto created = EphemerisEngineFactory::create(EphemerisEngineFactoryRequest{});
    Q_ASSERT(EphemerisFactoryCreationStatus::isSuccess(created.status));
    return EphemerisEngineFactoryResult::success(
        std::move(created.engine), EphemerisFactoryCreationStatus::Type::CreatedRequestedEngine
    );
}

}  // namespace

class EphemerisEngineReplacementPolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void keepsCurrentHighPrecisionWhenRebuildFallsBack();
    void replacesCurrentEngineForNonFallbackResults();
    void replacesCurrentEngineForSimpleRequestedKind();
    void replacesCurrentEngineForSimpleCurrentKind();
    void keepsCurrentNonEnumeratedWhenRebuildFallsBack();
    void replacesCurrentEngineForDifferentFallbackKind();
    void replacesNonEnumeratedEngineForNonFallbackResult();
};

void EphemerisEngineReplacementPolicyTests::keepsCurrentHighPrecisionWhenRebuildFallsBack()
{
    const EphemerisEngineFactoryResult fallbackResult = makeSimpleFallbackResult();
    QVERIFY(fallbackResult.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(fallbackResult.engine->kind()),
        static_cast<std::uint8_t>(EphemerisEngineKind::Type::Simple)
    );

    QVERIFY(
        EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
            EphemerisEngineKind::Type::HighPrecision, fallbackResult, EphemerisEngineKind::Type::HighPrecision
        )
    );
}

void EphemerisEngineReplacementPolicyTests::replacesCurrentEngineForNonFallbackResults()
{
    const EphemerisEngineFactoryResult requestedResult = makeRequestedEngineResult();

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        EphemerisEngineKind::Type::HighPrecision, requestedResult, EphemerisEngineKind::Type::HighPrecision
    ));
}

void EphemerisEngineReplacementPolicyTests::replacesCurrentEngineForSimpleRequestedKind()
{
    const EphemerisEngineFactoryResult fallbackResult = makeSimpleFallbackResult();

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        EphemerisEngineKind::Type::Simple, fallbackResult, EphemerisEngineKind::Type::HighPrecision
    ));
}

void EphemerisEngineReplacementPolicyTests::replacesCurrentEngineForSimpleCurrentKind()
{
    const EphemerisEngineFactoryResult fallbackResult = makeSimpleFallbackResult();

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        EphemerisEngineKind::Type::HighPrecision, fallbackResult, EphemerisEngineKind::Type::Simple
    ));
}

void EphemerisEngineReplacementPolicyTests::keepsCurrentNonEnumeratedWhenRebuildFallsBack()
{
    const EphemerisEngineFactoryResult fallbackResult = makeSimpleFallbackResult();
    QVERIFY(fallbackResult.usedSimpleEngineFallback());

    QVERIFY(
        EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
            kNonEnumeratedEngineKind, fallbackResult, kNonEnumeratedEngineKind
        )
    );
}

void EphemerisEngineReplacementPolicyTests::replacesCurrentEngineForDifferentFallbackKind()
{
    const EphemerisEngineFactoryResult fallbackResult = makeSimpleFallbackResult();

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        kNonEnumeratedEngineKind, fallbackResult, EphemerisEngineKind::Type::Simple
    ));
}

void EphemerisEngineReplacementPolicyTests::replacesNonEnumeratedEngineForNonFallbackResult()
{
    const EphemerisEngineFactoryResult requestedResult = makeRequestedEngineResult();

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        kNonEnumeratedEngineKind, requestedResult, kNonEnumeratedEngineKind
    ));
}

QTEST_APPLESS_MAIN(EphemerisEngineReplacementPolicyTests)
#include "EphemerisEngineReplacementPolicyTests.moc"
