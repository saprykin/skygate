#include "SkySettingsSnapshotCodecs.hpp"
#include "SkySettingsValueCodecs.hpp"

#include <QtTest/QtTest>

#include "SkyLogging.hpp"

#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>

namespace {

void ignoreFallbackWarnings(const int count)
{
    for (int index = 0; index < count; ++index) {
        QTest::ignoreMessage(
            QtWarningMsg,
            QRegularExpression("Invalid .*setting .* - using fallback .*")
        );
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
    void primitiveReadersUseFallbacksForMissingMalformedAndNonFiniteValues();
    void splitAndMergeStateSnapshotPreservesTypedDomains();
    void stateSnapshotLoadRequiresVersionAndAppliesDefaults();
    void savingBlankLogPathStoresDefaultPath();
};

void SkySettingsCodecsTests::boolCodecAcceptsStrictBooleanSpellings()
{
    using skygate::ui::internal::boolFromSettingValue;

    QCOMPARE(boolFromSettingValue(QVariant(true)), std::optional<bool>(true));
    QCOMPARE(boolFromSettingValue(QVariant(" yes ")), std::optional<bool>(true));
    QCOMPARE(boolFromSettingValue(QVariant("ON")), std::optional<bool>(true));
    QCOMPARE(boolFromSettingValue(QVariant("0")), std::optional<bool>(false));
    QCOMPARE(boolFromSettingValue(QVariant("off")), std::optional<bool>(false));
    QVERIFY(!boolFromSettingValue(QVariant()).has_value());
    QVERIFY(!boolFromSettingValue(QVariant("maybe")).has_value());
}

void SkySettingsCodecsTests::primitiveReadersUseFallbacksForMissingMalformedAndNonFiniteValues()
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
    QCOMPARE(readBoolSetting(settings, "missingBool", false), false);
    QCOMPARE(readDoubleSetting(settings, "missingDouble", 9.0), 9.0);
    QCOMPARE(readDoubleSetting(settings, "goodDouble", 0.0), 42.5);

    ignoreFallbackWarnings(6);
    QCOMPARE(readBoolSetting(settings, "badBool", true), true);
    QCOMPARE(readDoubleSetting(settings, "badDouble", 12.0), 12.0);
    QCOMPARE(readIntSetting(settings, "badInt", 7), 7);
    QCOMPARE(readLongLongSetting(settings, "badLongLong", -3), -3);
    QCOMPARE(readULongLongSetting(settings, "badULongLong", 8), 8ULL);
    QCOMPARE(readNonBlankStringSetting(settings, "blankPath", "fallback"), QString("fallback"));
}

void SkySettingsCodecsTests::splitAndMergeStateSnapshotPreservesTypedDomains()
{
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.live = false;
    snapshot.timelineToolbarCollapsed = true;
    snapshot.searchToolbarCollapsed = true;
    snapshot.speedMultiplier = 2.5;
    snapshot.stepSeconds = 300;
    snapshot.utcEpochSeconds = -1234;
    snapshot.magnitudeCutoff = 7.25;
    snapshot.viewCenterAltitudeDeg = 12.0;
    snapshot.viewCenterAzimuthDeg = 220.0;
    snapshot.viewFieldOfViewDeg = 60.0;
    snapshot.projectionTypeText = "Perspective";
    snapshot.themeId = "paper";
    snapshot.latitudeDeg = 47.0;
    snapshot.longitudeDeg = 8.0;
    snapshot.elevationMeters = 420.0;
    snapshot.locationSourceText = "City";
    snapshot.selectedCityId = "ch-zurich";
    snapshot.displayTimeZoneId = "Europe/Zurich";
    snapshot.overlayLayers.horizon = false;
    snapshot.overlayLayers.ecliptic = true;
    snapshot.catalogPresetIndex = 3;
    snapshot.catalogUrlText = "https://example.invalid/stars.csv";
    snapshot.deepSkyCatalogPresetIndex = 2;
    snapshot.deepSkyCatalogUrlText = "https://example.invalid/dso.csv";
    snapshot.logToTerminal = false;
    snapshot.logToFile = true;
    snapshot.logFilePath = "/tmp/skygate.log";

    const auto domains = skygate::ui::internal::splitStateSnapshot(snapshot);
    QCOMPARE(domains.timeline.live, false);
    QCOMPARE(domains.search.toolbarCollapsed, true);
    QCOMPARE(domains.view.themeId, QString("paper"));
    QCOMPARE(domains.location.displayTimeZoneId, QString("Europe/Zurich"));
    QCOMPARE(domains.catalogSources.deepSkyPresetIndex, 2);
    QCOMPARE(domains.logging.logToFile, true);

    const auto merged = skygate::ui::internal::mergeStateSnapshot(domains);
    QCOMPARE(merged.live, snapshot.live);
    QCOMPARE(merged.timelineToolbarCollapsed, snapshot.timelineToolbarCollapsed);
    QCOMPARE(merged.searchToolbarCollapsed, snapshot.searchToolbarCollapsed);
    QCOMPARE(merged.speedMultiplier, snapshot.speedMultiplier);
    QCOMPARE(merged.stepSeconds, snapshot.stepSeconds);
    QCOMPARE(merged.utcEpochSeconds, snapshot.utcEpochSeconds);
    QCOMPARE(merged.projectionTypeText, snapshot.projectionTypeText);
    QVERIFY(merged.overlayLayers.equals(snapshot.overlayLayers));
    QCOMPARE(merged.deepSkyCatalogUrlText, snapshot.deepSkyCatalogUrlText);
    QCOMPARE(merged.logFilePath, snapshot.logFilePath);
}

void SkySettingsCodecsTests::stateSnapshotLoadRequiresVersionAndAppliesDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings = makeSettings(dir);

    QVERIFY(!skygate::ui::internal::loadStateSnapshot(settings).has_value());

    settings.setValue("skyContext/version", 3);
    settings.setValue("skyContext/searchToolbarCollapsed", true);
    settings.setValue("skyContext/overlayLayers/altAzGrid", false);

    const auto loaded = skygate::ui::internal::loadStateSnapshot(settings);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->live, true);
    QCOMPARE(loaded->searchToolbarCollapsed, true);
    QCOMPARE(loaded->timelineToolbarCollapsed, false);
    QCOMPARE(loaded->overlayLayers.horizon, true);
    QCOMPARE(loaded->overlayLayers.altAzGrid, false);
    QCOMPARE(loaded->logToTerminal, true);
    QCOMPARE(loaded->logToFile, false);
    QCOMPARE(loaded->logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
}

void SkySettingsCodecsTests::savingBlankLogPathStoresDefaultPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings = makeSettings(dir);

    SkySettingsStore::StateSnapshot snapshot;
    snapshot.logToTerminal = false;
    snapshot.logToFile = true;
    snapshot.logFilePath = "  ";

    skygate::ui::internal::saveStateSnapshot(settings, snapshot);
    const auto loaded = skygate::ui::internal::loadStateSnapshot(settings);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->logToTerminal, false);
    QCOMPARE(loaded->logToFile, true);
    QCOMPARE(loaded->logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
}

QTEST_APPLESS_MAIN(SkySettingsCodecsTests)

#include "SkySettingsCodecsTests.moc"
