#include "diagnostics/SkyLogging.hpp"
#include "settings/SkySettingsSnapshotCodecs.hpp"
#include "settings/SkySettingsValueCodecs.hpp"

#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstdint>

namespace {

void ignoreFallbackWarnings(const int count)
{
    for (int index = 0; index < count; ++index) {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Invalid .*setting .* - using fallback .*"));
    }
}

QSettings makeSettings(QTemporaryDir& dir)
{
    return QSettings(dir.filePath("settings.ini"), QSettings::IniFormat);
}

}  // namespace

class SkySettingsCodecsTests final : public QObject {
    Q_OBJECT

private slots:
    void boolCodecAcceptsStrictBooleanSpellings();
    void primitiveReadersFallBackForMissingInvalidValues();
    void stateSnapshotMigratesLegacyShortRangeProfileId();
    void savingBlankLogPathStoresDefaultPath();
    void ephemerisEngineKindRoundTripsBothStableIds();
    void engineKindParsesLegacyAliasesAndCaseInsensitive();
    void engineKindUnknownIdFallsBackToDefaultDescriptor();
};

void SkySettingsCodecsTests::boolCodecAcceptsStrictBooleanSpellings()
{
    using skygate::ui::internal::SkySettingsValueCodecs;

    QCOMPARE(SkySettingsValueCodecs::boolFromSettingValue(QVariant(true)), std::optional<bool>(true));
    QCOMPARE(SkySettingsValueCodecs::boolFromSettingValue(QVariant(" yes ")), std::optional<bool>(true));
    QCOMPARE(SkySettingsValueCodecs::boolFromSettingValue(QVariant("ON")), std::optional<bool>(true));
    QCOMPARE(SkySettingsValueCodecs::boolFromSettingValue(QVariant("0")), std::optional<bool>(false));
    QCOMPARE(SkySettingsValueCodecs::boolFromSettingValue(QVariant("off")), std::optional<bool>(false));
    QVERIFY(!SkySettingsValueCodecs::boolFromSettingValue(QVariant()).has_value());
    QVERIFY(!SkySettingsValueCodecs::boolFromSettingValue(QVariant("maybe")).has_value());
}

void SkySettingsCodecsTests::primitiveReadersFallBackForMissingInvalidValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings = makeSettings(dir);
    settings.setValue("badBool", "sometimes");
    settings.setValue("badDouble", "nan");
    settings.setValue("badInt", "soon");
    settings.setValue("badLongLong", "later");
    settings.setValue("badULongLong", "-5");
    settings.setValue("blankPath", "  ");
    settings.setValue("goodDouble", "42.5");

    using namespace skygate::ui::internal;
    QCOMPARE(SkySettingsValueCodecs::readBoolSetting(settings, "missingBool", false), false);
    QCOMPARE(SkySettingsValueCodecs::readDoubleSetting(settings, "missingDouble", 9.0), 9.0);
    QCOMPARE(SkySettingsValueCodecs::readDoubleSetting(settings, "goodDouble", 0.0), 42.5);

    ignoreFallbackWarnings(6);
    QCOMPARE(SkySettingsValueCodecs::readBoolSetting(settings, "badBool", true), true);
    QCOMPARE(SkySettingsValueCodecs::readDoubleSetting(settings, "badDouble", 12.0), 12.0);
    QCOMPARE(SkySettingsValueCodecs::readIntSetting(settings, "badInt", 7), 7);
    QCOMPARE(SkySettingsValueCodecs::readLongLongSetting(settings, "badLongLong", -3), -3);
    QCOMPARE(SkySettingsValueCodecs::readULongLongSetting(settings, "badULongLong", 8), 8ULL);
    QCOMPARE(SkySettingsValueCodecs::readNonBlankStringSetting(settings, "blankPath", "fallback"), QString("fallback"));
}

void SkySettingsCodecsTests::stateSnapshotMigratesLegacyShortRangeProfileId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings = makeSettings(dir);
    settings.setValue("skyContext/version", 3);
    settings.setValue("skyContext/ephemeris/preferredDataProfileId", "modern");

    const auto loaded = skygate::ui::internal::SkySettingsSnapshotCodecs::loadStateSnapshot(settings);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->ephemeris.preferredDataProfileId, QString("de440s-short-range"));
}

void SkySettingsCodecsTests::savingBlankLogPathStoresDefaultPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings = makeSettings(dir);

    SkyPersistedStateSnapshot snapshot;
    snapshot.logToTerminal = false;
    snapshot.logToFile = true;
    snapshot.logFilePath = "  ";
    snapshot.ephemeris.correctionPresetId = "  ";
    snapshot.ephemeris.preferredDataProfileId = "";

    skygate::ui::internal::SkySettingsSnapshotCodecs::saveStateSnapshot(settings, snapshot);
    ignoreFallbackWarnings(2);
    const auto loaded = skygate::ui::internal::SkySettingsSnapshotCodecs::loadStateSnapshot(settings);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->logToTerminal, false);
    QCOMPARE(loaded->logToFile, true);
    QCOMPARE(loaded->logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
    QCOMPARE(loaded->ephemeris.correctionPresetId, QString("apparent-topocentric"));
    QCOMPARE(loaded->ephemeris.preferredDataProfileId, QString("de440s-short-range"));
    QCOMPARE(loaded->ephemerisSettingsPresent, true);
}

void SkySettingsCodecsTests::ephemerisEngineKindRoundTripsBothStableIds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings = makeSettings(dir);

    SkyPersistedStateSnapshot snapshot;
    snapshot.ephemeris.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    skygate::ui::internal::SkySettingsSnapshotCodecs::saveStateSnapshot(settings, snapshot);
    auto loaded = skygate::ui::internal::SkySettingsSnapshotCodecs::loadStateSnapshot(settings);
    QVERIFY(loaded.has_value());
    QCOMPARE(settings.value("skyContext/ephemeris/engineKind").toString(), QString("simple"));
    QCOMPARE(
        static_cast<std::uint8_t>(loaded->ephemeris.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );

    snapshot.ephemeris.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    skygate::ui::internal::SkySettingsSnapshotCodecs::saveStateSnapshot(settings, snapshot);
    loaded = skygate::ui::internal::SkySettingsSnapshotCodecs::loadStateSnapshot(settings);
    QVERIFY(loaded.has_value());
    QCOMPARE(settings.value("skyContext/ephemeris/engineKind").toString(), QString("highPrecision"));
    QCOMPARE(
        static_cast<std::uint8_t>(loaded->ephemeris.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
}

void SkySettingsCodecsTests::engineKindParsesLegacyAliasesAndCaseInsensitive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings = makeSettings(dir);
    settings.setValue("skyContext/version", 3);

    const QStringList highPrecisionAliases{
        QStringLiteral("highPrecision"),
        QStringLiteral("HighPrecision"),
        QStringLiteral("HIGHPRECISION"),
        QStringLiteral("highprecision"),
        QStringLiteral("high-precision"),
    };
    for (const QString& alias : highPrecisionAliases) {
        settings.setValue("skyContext/ephemeris/engineKind", alias);
        const auto loaded = skygate::ui::internal::SkySettingsSnapshotCodecs::loadStateSnapshot(settings);
        QVERIFY(loaded.has_value());
        QCOMPARE(
            static_cast<std::uint8_t>(loaded->ephemeris.engineKind),
            static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
        );
    }

    settings.setValue("skyContext/ephemeris/engineKind", QStringLiteral("SIMPLE"));
    const auto simpleLoaded = skygate::ui::internal::SkySettingsSnapshotCodecs::loadStateSnapshot(settings);
    QVERIFY(simpleLoaded.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(simpleLoaded->ephemeris.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
}

void SkySettingsCodecsTests::engineKindUnknownIdFallsBackToDefaultDescriptor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings = makeSettings(dir);
    settings.setValue("skyContext/version", 3);
    settings.setValue("skyContext/ephemeris/engineKind", QStringLiteral("experimental"));

    const auto loaded = skygate::ui::internal::SkySettingsSnapshotCodecs::loadStateSnapshot(settings);
    QVERIFY(loaded.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(loaded->ephemeris.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
}

QTEST_APPLESS_MAIN(SkySettingsCodecsTests)

#include "SkySettingsCodecsTests.moc"
