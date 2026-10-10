#include "EphemerisEngineTestDoubles.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "factory/EphemerisEngineFactoryResult.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
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
    void constructsFactoryResultAndCreationDiagnostics();
    void resultHelpersDistinguishSuccessFallbackFailureAndDiagnostics();
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

void EphemerisEngineFactoryResultTests::constructsFactoryResultAndCreationDiagnostics()
{
    constexpr std::array factoryStatuses{
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedRequestedEngine,
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback,
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedInvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedCreationError,
    };

    for (const skygate::ephemeris::EphemerisFactoryCreationStatus status : factoryStatuses) {
        QVERIFY(!skygate::ephemeris::EphemerisEngineFactory::displayName(status).empty());
    }

    QVERIFY(
        skygate::ephemeris::EphemerisEngineFactory::isCreationSuccess(
            skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedRequestedEngine
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisEngineFactory::isCreationSuccess(
            skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisEngineFactory::isCreationSuccess(
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable
    ));

    const std::array diagnosticCodes{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::RequiredTimeScaleServiceUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::RequiredEarthOrientationProviderUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::InvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::EngineCreationFailed,
    };

    for (const skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode code : diagnosticCodes) {
        QVERIFY(!skygate::ephemeris::EphemerisEngineFactory::diagnosticText(code).empty());
    }

    skygate::ephemeris::EphemerisFactoryCreationDiagnostic fallbackDiagnostic{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Warning,
    };
    QVERIFY(!fallbackDiagnostic.isError());
    QVERIFY(!fallbackDiagnostic.displayText().empty());

    skygate::ephemeris::EphemerisFactoryCreationDiagnostic strictFailureDiagnostic{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Error,
        "strict high precision requested but unavailable",
    };
    QVERIFY(strictFailureDiagnostic.isError());
    QVERIFY(
        strictFailureDiagnostic.displayText() == std::string_view{"strict high precision requested but unavailable"}
    );

    auto successEngine = skygate::ephemeris::EphemerisEngineFactory::create();
    auto successResult = skygate::ephemeris::EphemerisEngineFactoryResult::success(std::move(successEngine.engine));
    QVERIFY(successResult.isSuccess());
    QVERIFY(!successResult.isFailure());
    QVERIFY(successResult.engine != nullptr);
    QVERIFY(!successResult.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(successResult.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(successResult.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(!successResult.hasDiagnostics());
    QVERIFY(!successResult.hasErrors());

    auto fallbackEngine = skygate::ephemeris::EphemerisEngineFactory::create();
    auto fallbackResult = skygate::ephemeris::EphemerisEngineFactoryResult::success(
        std::move(fallbackEngine.engine),
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback,
        {fallbackDiagnostic},
        skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision
    );
    QVERIFY(fallbackResult.isSuccess());
    QVERIFY(fallbackResult.engine != nullptr);
    QVERIFY(fallbackResult.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(fallbackResult.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(fallbackResult.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(fallbackResult.hasDiagnostics());
    QVERIFY(!fallbackResult.hasErrors());
    QCOMPARE(fallbackResult.diagnostics.size(), std::size_t{1});

    auto strictFailureResult = skygate::ephemeris::EphemerisEngineFactoryResult::failure(
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable,
        {strictFailureDiagnostic},
        skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision
    );
    QVERIFY(!strictFailureResult.isSuccess());
    QVERIFY(strictFailureResult.isFailure());
    QVERIFY(strictFailureResult.engine == nullptr);
    QVERIFY(!strictFailureResult.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(strictFailureResult.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(strictFailureResult.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QVERIFY(strictFailureResult.hasDiagnostics());
    QVERIFY(strictFailureResult.hasErrors());
    QCOMPARE(strictFailureResult.diagnostics.size(), std::size_t{1});
}

void EphemerisEngineFactoryResultTests::resultHelpersDistinguishSuccessFallbackFailureAndDiagnostics()
{
    skygate::ephemeris::EphemerisFactoryCreationDiagnostic warning{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Warning,
    };
    skygate::ephemeris::EphemerisFactoryCreationDiagnostic error{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::InvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Error,
        "invalid engine kind",
    };

    auto successEngine = skygate::ephemeris::EphemerisEngineFactory::create();
    auto success = skygate::ephemeris::EphemerisEngineFactoryResult::success(std::move(successEngine.engine));
    QVERIFY(success.isSuccess());
    QVERIFY(!success.isFailure());
    QVERIFY(!success.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(success.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(success.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(!success.hasDiagnostics());
    QVERIFY(!success.hasErrors());

    auto fallbackEngine = skygate::ephemeris::EphemerisEngineFactory::create();
    auto fallback = skygate::ephemeris::EphemerisEngineFactoryResult::success(
        std::move(fallbackEngine.engine),
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback,
        {warning},
        skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision
    );
    QVERIFY(fallback.isSuccess());
    QVERIFY(fallback.usedSimpleEngineFallback());
    QCOMPARE(
        static_cast<std::uint8_t>(fallback.requestedKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(fallback.effectiveKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(fallback.hasDiagnostics());
    QVERIFY(!fallback.hasErrors());
    QVERIFY(!fallback.diagnostics.front().displayText().empty());

    constexpr auto invalidRequestedKind = static_cast<skygate::ephemeris::EphemerisEngineKind::Type>(std::uint8_t{255});
    auto failure = skygate::ephemeris::EphemerisEngineFactoryResult::failure(
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedInvalidRequest, {error}, invalidRequestedKind
    );
    QVERIFY(!failure.isSuccess());
    QVERIFY(failure.isFailure());
    QVERIFY(!failure.usedSimpleEngineFallback());
    QCOMPARE(static_cast<std::uint8_t>(failure.requestedKind()), static_cast<std::uint8_t>(invalidRequestedKind));
    QCOMPARE(static_cast<std::uint8_t>(failure.effectiveKind()), static_cast<std::uint8_t>(invalidRequestedKind));
    QVERIFY(failure.hasDiagnostics());
    QVERIFY(failure.hasErrors());
    QVERIFY(failure.diagnostics.front().displayText() == std::string_view{"invalid engine kind"});
}

QTEST_APPLESS_MAIN(EphemerisEngineFactoryResultTests)

#include "EphemerisEngineFactoryResultTests.moc"
