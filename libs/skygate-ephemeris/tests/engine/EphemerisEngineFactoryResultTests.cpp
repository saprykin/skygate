#include "EphemerisEngineTestDoubles.hpp"
#include "factory/EphemerisEngineFactoryResult.hpp"
#include "factory/EphemerisFactoryCreationStatus.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace {

using skygate::ephemeris::EphemerisFactoryCreationStatus;
using skygate::ephemeris::tests::FixedAltitudeEngine;

}  // namespace

class EphemerisEngineFactoryResultTests final : public QObject {
    Q_OBJECT

private slots:
    void classifiesCreationStatusesAndDiagnosticCodes();
    void resultHelpersDistinguishOutcomesAndDiagnostics();
};

void EphemerisEngineFactoryResultTests::classifiesCreationStatusesAndDiagnosticCodes()
{
    constexpr std::array factoryStatuses{
        skygate::ephemeris::EphemerisFactoryCreationStatus::Type::CreatedRequestedEngine,
        skygate::ephemeris::EphemerisFactoryCreationStatus::Type::CreatedSimpleFallback,
        skygate::ephemeris::EphemerisFactoryCreationStatus::Type::FailedStrictHighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationStatus::Type::FailedInvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationStatus::Type::FailedCreationError,
    };
    constexpr std::array statusSuccessFlags{true, true, false, false, false};

    for (std::size_t index = 0; index < factoryStatuses.size(); ++index) {
        QCOMPARE(EphemerisFactoryCreationStatus::isSuccess(factoryStatuses[index]), statusSuccessFlags[index]);
    }

    const std::array diagnosticCodes{
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::RequiredEphemerisDataUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::RequiredTimeScaleServiceUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::RequiredEarthOrientationProviderUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::InvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::EngineCreationFailed,
    };

    for (const skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code code : diagnosticCodes) {
        const skygate::ephemeris::EphemerisFactoryCreationDiagnostic diagnostic{
            code, skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Error
        };
        QVERIFY(!diagnostic.displayText().empty());
        QCOMPARE(diagnostic.displayText(), skygate::ephemeris::EphemerisFactoryCreationDiagnostic::text(code));
    }

    skygate::ephemeris::EphemerisFactoryCreationDiagnostic fallbackDiagnostic{
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Warning,
    };
    QVERIFY(!fallbackDiagnostic.isError());
    QVERIFY(!fallbackDiagnostic.displayText().empty());

    skygate::ephemeris::EphemerisFactoryCreationDiagnostic strictFailureDiagnostic{
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Error,
        "strict high precision requested but unavailable",
    };
    QVERIFY(strictFailureDiagnostic.isError());
    QVERIFY(
        strictFailureDiagnostic.displayText() == std::string_view{"strict high precision requested but unavailable"}
    );
}

void EphemerisEngineFactoryResultTests::resultHelpersDistinguishOutcomesAndDiagnostics()
{
    skygate::ephemeris::EphemerisFactoryCreationDiagnostic warning{
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Warning,
    };
    skygate::ephemeris::EphemerisFactoryCreationDiagnostic error{
        skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::InvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Error,
        "invalid engine kind",
    };

    auto success =
        skygate::ephemeris::EphemerisEngineFactoryResult::success(std::make_unique<FixedAltitudeEngine>(0.0));
    QVERIFY(EphemerisFactoryCreationStatus::isSuccess(success.status));
    QCOMPARE(
        static_cast<std::uint8_t>(success.status),
        static_cast<std::uint8_t>(EphemerisFactoryCreationStatus::Type::CreatedRequestedEngine)
    );
    QVERIFY(success.engine != nullptr);
    QVERIFY(!success.usedSimpleEngineFallback());
    QVERIFY(success.diagnostics.empty());

    auto fallback = skygate::ephemeris::EphemerisEngineFactoryResult::success(
        std::make_unique<FixedAltitudeEngine>(0.0),
        skygate::ephemeris::EphemerisFactoryCreationStatus::Type::CreatedSimpleFallback,
        {warning}
    );
    QVERIFY(EphemerisFactoryCreationStatus::isSuccess(fallback.status));
    QVERIFY(fallback.engine != nullptr);
    QVERIFY(fallback.usedSimpleEngineFallback());
    QCOMPARE(fallback.diagnostics.size(), std::size_t{1});
    QVERIFY(!fallback.diagnostics.front().isError());
    QVERIFY(!fallback.diagnostics.front().displayText().empty());

    auto failure = skygate::ephemeris::EphemerisEngineFactoryResult::failure(
        skygate::ephemeris::EphemerisFactoryCreationStatus::Type::FailedInvalidRequest, {error}
    );
    QVERIFY(!EphemerisFactoryCreationStatus::isSuccess(failure.status));
    QVERIFY(failure.engine == nullptr);
    QVERIFY(!failure.usedSimpleEngineFallback());
    QCOMPARE(failure.diagnostics.size(), std::size_t{1});
    QVERIFY(failure.diagnostics.front().isError());
    QVERIFY(failure.diagnostics.front().displayText() == std::string_view{"invalid engine kind"});
}

QTEST_APPLESS_MAIN(EphemerisEngineFactoryResultTests)

#include "EphemerisEngineFactoryResultTests.moc"
