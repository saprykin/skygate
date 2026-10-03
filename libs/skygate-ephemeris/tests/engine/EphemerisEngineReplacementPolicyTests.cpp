#include "factory/EphemerisEngineReplacementPolicy.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "factory/EphemerisEngineFactoryResult.hpp"

#include <QtTest/QtTest>

#include <memory>
#include <utility>

namespace {

using skygate::ephemeris::EphemerisEngineFactory;
using skygate::ephemeris::EphemerisEngineFactoryResult;
using skygate::ephemeris::EphemerisEngineKind;
using skygate::ephemeris::EphemerisEngineReplacementPolicy;
using skygate::ephemeris::EphemerisFactoryCreationStatus;

constexpr EphemerisEngineKind::Type kNonEnumeratedEngineKind = static_cast<EphemerisEngineKind::Type>(42);

[[nodiscard]] EphemerisEngineFactoryResult makeSimpleFallbackResult(const EphemerisEngineKind::Type requestedKind)
{
    auto created = EphemerisEngineFactory::create();
    Q_ASSERT(created.isSuccess());
    return EphemerisEngineFactoryResult::success(
        std::move(created.engine), EphemerisFactoryCreationStatus::CreatedSimpleFallback, {}, requestedKind
    );
}

[[nodiscard]] EphemerisEngineFactoryResult makeRequestedEngineResult(const EphemerisEngineKind::Type requestedKind)
{
    auto created = EphemerisEngineFactory::create();
    Q_ASSERT(created.isSuccess());
    return EphemerisEngineFactoryResult::success(
        std::move(created.engine), EphemerisFactoryCreationStatus::CreatedRequestedEngine, {}, requestedKind
    );
}

}  // namespace

class EphemerisEngineReplacementPolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void keepsCurrentHighPrecisionEngineWhenRebuildFallsBackToSimple();
    void replacesCurrentEngineForNonFallbackResults();
    void replacesCurrentEngineWhenRequestedKindIsNotHighPrecision();
    void replacesCurrentEngineWhenCurrentKindIsNotHighPrecision();
    void keepsCurrentNonEnumeratedEngineWhenRebuildFallsBack();
    void replacesCurrentEngineWhenFallbackRequestedKindDiffersFromCurrent();
    void replacesCurrentNonEnumeratedEngineForNonFallbackResult();
};

void EphemerisEngineReplacementPolicyTests::keepsCurrentHighPrecisionEngineWhenRebuildFallsBackToSimple()
{
    const EphemerisEngineFactoryResult fallbackResult =
        makeSimpleFallbackResult(EphemerisEngineKind::Type::HighPrecision);
    QVERIFY(fallbackResult.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(fallbackResult.requestedKind()),
        static_cast<std::uint8_t>(EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(fallbackResult.effectiveKind()),
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
    const EphemerisEngineFactoryResult requestedResult =
        makeRequestedEngineResult(EphemerisEngineKind::Type::HighPrecision);

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        EphemerisEngineKind::Type::HighPrecision, requestedResult, EphemerisEngineKind::Type::HighPrecision
    ));
}

void EphemerisEngineReplacementPolicyTests::replacesCurrentEngineWhenRequestedKindIsNotHighPrecision()
{
    const EphemerisEngineFactoryResult fallbackResult = makeSimpleFallbackResult(EphemerisEngineKind::Type::Simple);

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        EphemerisEngineKind::Type::Simple, fallbackResult, EphemerisEngineKind::Type::HighPrecision
    ));
}

void EphemerisEngineReplacementPolicyTests::replacesCurrentEngineWhenCurrentKindIsNotHighPrecision()
{
    const EphemerisEngineFactoryResult fallbackResult =
        makeSimpleFallbackResult(EphemerisEngineKind::Type::HighPrecision);

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        EphemerisEngineKind::Type::HighPrecision, fallbackResult, EphemerisEngineKind::Type::Simple
    ));
}

void EphemerisEngineReplacementPolicyTests::keepsCurrentNonEnumeratedEngineWhenRebuildFallsBack()
{
    const EphemerisEngineFactoryResult fallbackResult = makeSimpleFallbackResult(kNonEnumeratedEngineKind);
    QVERIFY(fallbackResult.usedSimpleEngineFallback());

    QVERIFY(
        EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
            kNonEnumeratedEngineKind, fallbackResult, kNonEnumeratedEngineKind
        )
    );
}

void EphemerisEngineReplacementPolicyTests::replacesCurrentEngineWhenFallbackRequestedKindDiffersFromCurrent()
{
    const EphemerisEngineFactoryResult fallbackResult = makeSimpleFallbackResult(kNonEnumeratedEngineKind);

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        kNonEnumeratedEngineKind, fallbackResult, EphemerisEngineKind::Type::Simple
    ));
}

void EphemerisEngineReplacementPolicyTests::replacesCurrentNonEnumeratedEngineForNonFallbackResult()
{
    const EphemerisEngineFactoryResult requestedResult = makeRequestedEngineResult(kNonEnumeratedEngineKind);

    QVERIFY(!EphemerisEngineReplacementPolicy::shouldKeepCurrentEngine(
        kNonEnumeratedEngineKind, requestedResult, kNonEnumeratedEngineKind
    ));
}

QTEST_APPLESS_MAIN(EphemerisEngineReplacementPolicyTests)
#include "EphemerisEngineReplacementPolicyTests.moc"
