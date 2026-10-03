#include "EphemerisEngineTestDoubles.hpp"
#include "factory/EphemerisEngineFactoryResult.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <memory>
#include <utility>

namespace {

using skygate::ephemeris::EphemerisEngineFactoryResult;
using skygate::ephemeris::EphemerisEngineKind;
using skygate::ephemeris::EphemerisFactoryCreationStatus;
using skygate::ephemeris::tests::FixedAltitudeEngine;
using skygate::ephemeris::tests::highPrecisionLightTimeOptions;
using skygate::ephemeris::tests::RequestCountingEphemerisEngine;

[[nodiscard]] std::uint8_t kindValue(const EphemerisEngineKind::Type kind)
{
    return static_cast<std::uint8_t>(kind);
}

}  // namespace

class EphemerisEngineFactoryResultTests final : public QObject {
    Q_OBJECT

private slots:
    void fallbackEffectiveKindSurvivesEngineMove();
    void requestedEffectiveKindSurvivesEngineMove();
    void failureEffectiveKindUsesRequestedKindWithoutEngine();
};

void EphemerisEngineFactoryResultTests::fallbackEffectiveKindSurvivesEngineMove()
{
    EphemerisEngineFactoryResult result = EphemerisEngineFactoryResult::success(
        std::make_unique<FixedAltitudeEngine>(0.0),
        EphemerisFactoryCreationStatus::CreatedSimpleFallback,
        {},
        EphemerisEngineKind::Type::HighPrecision
    );

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    QCOMPARE(kindValue(result.effectiveKind()), kindValue(EphemerisEngineKind::Type::Simple));
    QCOMPARE(kindValue(result.engine->kind()), kindValue(EphemerisEngineKind::Type::Simple));

    auto engine = std::move(result.engine);

    QVERIFY(engine != nullptr);
    QCOMPARE(kindValue(result.effectiveKind()), kindValue(EphemerisEngineKind::Type::Simple));
    QCOMPARE(kindValue(engine->kind()), kindValue(EphemerisEngineKind::Type::Simple));
}

void EphemerisEngineFactoryResultTests::requestedEffectiveKindSurvivesEngineMove()
{
    auto highPrecisionEngine = std::make_unique<RequestCountingEphemerisEngine>(
        std::make_unique<FixedAltitudeEngine>(0.0), highPrecisionLightTimeOptions()
    );

    EphemerisEngineFactoryResult result = EphemerisEngineFactoryResult::success(
        std::move(highPrecisionEngine),
        EphemerisFactoryCreationStatus::CreatedRequestedEngine,
        {},
        EphemerisEngineKind::Type::HighPrecision
    );

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    QCOMPARE(kindValue(result.effectiveKind()), kindValue(EphemerisEngineKind::Type::HighPrecision));
    QCOMPARE(kindValue(result.engine->kind()), kindValue(EphemerisEngineKind::Type::HighPrecision));

    auto engine = std::move(result.engine);

    QVERIFY(engine != nullptr);
    QCOMPARE(kindValue(result.effectiveKind()), kindValue(EphemerisEngineKind::Type::HighPrecision));
    QCOMPARE(kindValue(engine->kind()), kindValue(EphemerisEngineKind::Type::HighPrecision));
}

void EphemerisEngineFactoryResultTests::failureEffectiveKindUsesRequestedKindWithoutEngine()
{
    const EphemerisEngineFactoryResult result = EphemerisEngineFactoryResult::failure(
        EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable,
        {},
        EphemerisEngineKind::Type::HighPrecision
    );

    QVERIFY(result.isFailure());
    QVERIFY(result.engine == nullptr);
    QCOMPARE(kindValue(result.effectiveKind()), kindValue(EphemerisEngineKind::Type::HighPrecision));
}

QTEST_APPLESS_MAIN(EphemerisEngineFactoryResultTests)

#include "EphemerisEngineFactoryResultTests.moc"
