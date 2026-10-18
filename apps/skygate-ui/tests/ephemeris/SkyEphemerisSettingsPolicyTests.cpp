#include "ephemeris/SkyEphemerisSettingsPolicy.hpp"

#include <QString>
#include <QtTest>

#include <cstdint>
#include <optional>

using namespace skygate::ui::internal;

namespace {

std::optional<double> parseAtmosphereText(const QString& field, const QString& text)
{
    if (field == QStringLiteral("pressure")) {
        return SkyEphemerisSettingsPolicy::atmosphericPressureHpaFromText(text);
    }
    if (field == QStringLiteral("temperature")) {
        return SkyEphemerisSettingsPolicy::atmosphericTemperatureCFromText(text);
    }
    if (field == QStringLiteral("humidity")) {
        return SkyEphemerisSettingsPolicy::relativeHumidityFromText(text);
    }
    if (field == QStringLiteral("wavelength")) {
        return SkyEphemerisSettingsPolicy::observingWavelengthMicrometersFromText(text);
    }
    return std::nullopt;
}

}  // namespace

class SkyEphemerisSettingsPolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void presetFlagsMatchPresetIndex_data();
    void presetFlagsMatchPresetIndex();
    void presetIdsMatchPresetIndex_data();
    void presetIdsMatchPresetIndex();
    void flagsMapToPresetIndex_data();
    void flagsMapToPresetIndex();
    void refractionTogglesAtmosphericFlag();
    void atmosphereTextEnforcesFieldBounds_data();
    void atmosphereTextEnforcesFieldBounds();
    void userSettingsFromEngineCopiesEngineFields();
    void normalizedUpdateManifestUrlFillsDefaultWhenEmpty();
    void engineOptionsFromSettingsCopiesAllFields();
    void settingsForEngineSwitchAppliesDefaults();
    void defaultWritableCacheRootEndsWithEphemerisData();
};

void SkyEphemerisSettingsPolicyTests::presetFlagsMatchPresetIndex_data()
{
    QTest::addColumn<int>("presetIndex");
    QTest::addColumn<uint>("expectedFlags");

    QTest::newRow("geometric") << 0 << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    QTest::newRow("astrometric") << 1 << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    QTest::newRow("apparent") << 2 << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::apparent());
    QTest::newRow("topocentric") << 3 << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::topocentric());
    QTest::newRow("negative") << -1 << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::topocentric());
    QTest::newRow("out-of-range") << 4
                                  << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::topocentric());
}

void SkyEphemerisSettingsPolicyTests::presetFlagsMatchPresetIndex()
{
    QFETCH(int, presetIndex);
    QFETCH(uint, expectedFlags);

    QCOMPARE(SkyEphemerisSettingsPolicy::correctionFlagsForPreset(presetIndex).bits(), expectedFlags);
}

void SkyEphemerisSettingsPolicyTests::presetIdsMatchPresetIndex_data()
{
    QTest::addColumn<int>("presetIndex");
    QTest::addColumn<QString>("expectedId");

    QTest::newRow("geometric") << 0 << QStringLiteral("geometric");
    QTest::newRow("astrometric") << 1 << QStringLiteral("astrometric");
    QTest::newRow("apparent") << 2 << QStringLiteral("apparent");
    QTest::newRow("topocentric") << 3 << QStringLiteral("apparent-topocentric");
    QTest::newRow("negative") << -1 << QStringLiteral("apparent-topocentric");
    QTest::newRow("out-of-range") << 4 << QStringLiteral("apparent-topocentric");
}

void SkyEphemerisSettingsPolicyTests::presetIdsMatchPresetIndex()
{
    QFETCH(int, presetIndex);
    QFETCH(QString, expectedId);

    QCOMPARE(SkyEphemerisSettingsPolicy::correctionPresetId(presetIndex), expectedId);
}

void SkyEphemerisSettingsPolicyTests::flagsMapToPresetIndex_data()
{
    QTest::addColumn<uint>("flags");
    QTest::addColumn<int>("expectedPresetIndex");

    QTest::newRow("no-corrections") << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::noCorrections())
                                    << 0;
    QTest::newRow("geometric") << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::geometric()) << 0;
    QTest::newRow("astrometric") << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::astrometric()) << 1;
    QTest::newRow("apparent") << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::apparent()) << 2;
    QTest::newRow("topocentric") << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::topocentric()) << 3;
    QTest::newRow("apparent-topocentric")
        << static_cast<uint>(skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric()) << 3;
    QTest::newRow("astrometric-with-refraction") << static_cast<uint>(
        skygate::ephemeris::EphemerisCorrectionFlags::astrometric()
        | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
    ) << 1;
    QTest::newRow("geometric-with-refraction") << static_cast<uint>(
        skygate::ephemeris::EphemerisCorrectionFlags::geometric()
        | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
    ) << 0;
}

void SkyEphemerisSettingsPolicyTests::flagsMapToPresetIndex()
{
    QFETCH(uint, flags);
    QFETCH(int, expectedPresetIndex);

    QCOMPARE(
        SkyEphemerisSettingsPolicy::correctionPresetIndex(
            skygate::ephemeris::EphemerisCorrectionFlags(static_cast<std::uint32_t>(flags))
        ),
        expectedPresetIndex
    );
}

void SkyEphemerisSettingsPolicyTests::refractionTogglesAtmosphericFlag()
{
    const auto apparent = skygate::ephemeris::EphemerisCorrectionFlags::apparent();
    QCOMPARE(
        SkyEphemerisSettingsPolicy::withRefraction(apparent, true),
        apparent | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
    );
    QCOMPARE(SkyEphemerisSettingsPolicy::withRefraction(apparent, false), apparent);

    const auto topocentric = skygate::ephemeris::EphemerisCorrectionFlags::topocentric();
    QCOMPARE(
        SkyEphemerisSettingsPolicy::withRefraction(topocentric, true),
        skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric()
    );
    QCOMPARE(SkyEphemerisSettingsPolicy::withRefraction(topocentric, false), topocentric);
}

void SkyEphemerisSettingsPolicyTests::atmosphereTextEnforcesFieldBounds_data()
{
    QTest::addColumn<QString>("field");
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<double>("expectedValue");

    QTest::newRow("pressure-min") << QStringLiteral("pressure") << QStringLiteral("0") << true << 0.0;
    QTest::newRow("pressure-max") << QStringLiteral("pressure") << QStringLiteral("2000") << true << 2000.0;
    QTest::newRow("pressure-trimmed") << QStringLiteral("pressure") << QStringLiteral(" 900.5 ") << true << 900.5;
    QTest::newRow("pressure-below") << QStringLiteral("pressure") << QStringLiteral("-0.01") << false << 0.0;
    QTest::newRow("pressure-above") << QStringLiteral("pressure") << QStringLiteral("2000.01") << false << 0.0;
    QTest::newRow("pressure-text") << QStringLiteral("pressure") << QStringLiteral("thin") << false << 0.0;
    QTest::newRow("pressure-empty") << QStringLiteral("pressure") << QString() << false << 0.0;

    QTest::newRow("temperature-min") << QStringLiteral("temperature") << QStringLiteral("-100") << true << -100.0;
    QTest::newRow("temperature-max") << QStringLiteral("temperature") << QStringLiteral("80") << true << 80.0;
    QTest::newRow("temperature-below") << QStringLiteral("temperature") << QStringLiteral("-100.01") << false << 0.0;
    QTest::newRow("temperature-above") << QStringLiteral("temperature") << QStringLiteral("80.01") << false << 0.0;
    QTest::newRow("temperature-text") << QStringLiteral("temperature") << QStringLiteral("cold") << false << 0.0;

    QTest::newRow("humidity-min") << QStringLiteral("humidity") << QStringLiteral("0") << true << 0.0;
    QTest::newRow("humidity-max") << QStringLiteral("humidity") << QStringLiteral("100") << true << 1.0;
    QTest::newRow("humidity-percent") << QStringLiteral("humidity") << QStringLiteral("42.0") << true << 0.42;
    QTest::newRow("humidity-below") << QStringLiteral("humidity") << QStringLiteral("-0.01") << false << 0.0;
    QTest::newRow("humidity-above") << QStringLiteral("humidity") << QStringLiteral("100.01") << false << 0.0;
    QTest::newRow("humidity-text") << QStringLiteral("humidity") << QStringLiteral("damp") << false << 0.0;

    QTest::newRow("wavelength-min") << QStringLiteral("wavelength") << QStringLiteral("0.1") << true << 0.1;
    QTest::newRow("wavelength-max") << QStringLiteral("wavelength") << QStringLiteral("100") << true << 100.0;
    QTest::newRow("wavelength-below") << QStringLiteral("wavelength") << QStringLiteral("0.09") << false << 0.0;
    QTest::newRow("wavelength-above") << QStringLiteral("wavelength") << QStringLiteral("100.01") << false << 0.0;
    QTest::newRow("wavelength-text") << QStringLiteral("wavelength") << QStringLiteral("green") << false << 0.0;
}

void SkyEphemerisSettingsPolicyTests::atmosphereTextEnforcesFieldBounds()
{
    QFETCH(QString, field);
    QFETCH(QString, text);
    QFETCH(bool, valid);
    QFETCH(double, expectedValue);

    const std::optional<double> value = parseAtmosphereText(field, text);
    QCOMPARE(value.has_value(), valid);
    if (valid) {
        QCOMPARE(*value, expectedValue);
    }
}

void SkyEphemerisSettingsPolicyTests::userSettingsFromEngineCopiesEngineFields()
{
    SkyEphemerisUserSettingsSnapshot settings;
    settings.correctionPresetId = QStringLiteral("custom-preset");
    settings.preferredDataProfileId = QStringLiteral("custom-profile");
    settings.onlineUpdatesEnabled = false;
    settings.updatePresetId = QStringLiteral("custom");
    settings.updateManifestUrl = QStringLiteral("https://example.test/ephemeris.json");

    skygate::ephemeris::EphemerisEngineOptions engineOptions;
    engineOptions.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    engineOptions.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    engineOptions.setFallbackToSimpleEngine(false);
    engineOptions.setEnableAtmosphericRefraction(true);
    engineOptions.setAtmosphericPressureHpa(802.5);
    engineOptions.setAtmosphericTemperatureC(-6.0);
    engineOptions.setRelativeHumidity(0.42);
    engineOptions.setObservingWavelengthMicrometers(0.68);

    const SkyEphemerisUserSettingsSnapshot result = SkyEphemerisSettingsPolicy::userSettingsFromEngine(
        settings, skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision, engineOptions
    );

    QCOMPARE(
        static_cast<std::uint8_t>(result.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(result.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::astrometric());
    QVERIFY(!result.fallbackToSimpleEngine);
    QVERIFY(result.refractionEnabled);
    QCOMPARE(result.atmosphericPressureHpa, 802.5);
    QCOMPARE(result.atmosphericTemperatureC, -6.0);
    QCOMPARE(result.relativeHumidity, 0.42);
    QCOMPARE(result.observingWavelengthMicrometers, 0.68);
    QCOMPARE(result.correctionPresetId, QStringLiteral("custom-preset"));
    QCOMPARE(result.preferredDataProfileId, QStringLiteral("custom-profile"));
    QVERIFY(!result.onlineUpdatesEnabled);
    QCOMPARE(result.updatePresetId, QStringLiteral("custom"));
    QCOMPARE(result.updateManifestUrl, QStringLiteral("https://example.test/ephemeris.json"));
}

void SkyEphemerisSettingsPolicyTests::normalizedUpdateManifestUrlFillsDefaultWhenEmpty()
{
    SkyEphemerisUserSettingsSnapshot emptySettings;
    QVERIFY(emptySettings.updateManifestUrl.isEmpty());

    const SkyEphemerisUserSettingsSnapshot normalized =
        SkyEphemerisSettingsPolicy::normalizedUpdateManifestUrl(emptySettings);
    QCOMPARE(normalized.updateManifestUrl, SkyEphemerisSettingsPolicy::kDefaultUpdateManifestUrl.toString());

    SkyEphemerisUserSettingsSnapshot blankSettings;
    blankSettings.updateManifestUrl = QStringLiteral("  \t ");
    const SkyEphemerisUserSettingsSnapshot blankNormalized =
        SkyEphemerisSettingsPolicy::normalizedUpdateManifestUrl(blankSettings);
    QCOMPARE(blankNormalized.updateManifestUrl, SkyEphemerisSettingsPolicy::kDefaultUpdateManifestUrl.toString());

    SkyEphemerisUserSettingsSnapshot customSettings;
    customSettings.updateManifestUrl = QStringLiteral("https://example.test/ephemeris.json");
    const SkyEphemerisUserSettingsSnapshot preserved =
        SkyEphemerisSettingsPolicy::normalizedUpdateManifestUrl(customSettings);
    QCOMPARE(preserved.updateManifestUrl, QStringLiteral("https://example.test/ephemeris.json"));
}

void SkyEphemerisSettingsPolicyTests::engineOptionsFromSettingsCopiesAllFields()
{
    SkyEphemerisUserSettingsSnapshot settings;
    settings.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    settings.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::apparent();
    settings.fallbackToSimpleEngine = false;
    settings.refractionEnabled = true;
    settings.atmosphericPressureHpa = 802.5;
    settings.atmosphericTemperatureC = -6.0;
    settings.relativeHumidity = 0.42;
    settings.observingWavelengthMicrometers = 0.68;

    const skygate::ephemeris::EphemerisEngineOptions baseOptions;
    const skygate::ephemeris::EphemerisEngineOptions result =
        SkyEphemerisSettingsPolicy::engineOptionsFromSettings(baseOptions, settings);

    QCOMPARE(
        static_cast<std::uint8_t>(result.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(result.correctionFlags(), skygate::ephemeris::EphemerisCorrectionFlags::apparent());
    QVERIFY(!result.fallbackToSimpleEngine());
    QVERIFY(result.enableAtmosphericRefraction());
    QCOMPARE(result.atmosphericPressureHpa(), 802.5);
    QCOMPARE(result.atmosphericTemperatureC(), -6.0);
    QCOMPARE(result.relativeHumidity(), 0.42);
    QCOMPARE(result.observingWavelengthMicrometers(), 0.68);
}

void SkyEphemerisSettingsPolicyTests::settingsForEngineSwitchAppliesDefaults()
{
    SkyEphemerisUserSettingsSnapshot settings;
    settings.correctionPresetId = QStringLiteral("custom-preset");
    settings.fallbackToSimpleEngine = false;
    settings.atmosphericPressureHpa = 802.5;
    settings.atmosphericTemperatureC = -6.0;
    settings.relativeHumidity = 0.42;
    settings.observingWavelengthMicrometers = 0.68;
    settings.preferredDataProfileId = QStringLiteral("custom-profile");

    const auto highPrecision = SkyEphemerisSettingsPolicy::settingsForEngineSwitch(
        settings, skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision
    );
    QCOMPARE(
        static_cast<std::uint8_t>(highPrecision.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(highPrecision.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric());
    QVERIFY(highPrecision.refractionEnabled);
    QCOMPARE(highPrecision.correctionPresetId, QStringLiteral("apparent-topocentric"));

    const auto simple = SkyEphemerisSettingsPolicy::settingsForEngineSwitch(
        settings, skygate::ephemeris::EphemerisEngineKind::Type::Simple
    );
    QCOMPARE(
        static_cast<std::uint8_t>(simple.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(simple.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::noCorrections());
    QVERIFY(!simple.refractionEnabled);
    QCOMPARE(simple.correctionPresetId, QStringLiteral("geometric"));

    for (const SkyEphemerisUserSettingsSnapshot* result : {&highPrecision, &simple}) {
        QVERIFY(!result->fallbackToSimpleEngine);
        QCOMPARE(result->atmosphericPressureHpa, 802.5);
        QCOMPARE(result->atmosphericTemperatureC, -6.0);
        QCOMPARE(result->relativeHumidity, 0.42);
        QCOMPARE(result->observingWavelengthMicrometers, 0.68);
        QCOMPARE(result->preferredDataProfileId, QStringLiteral("custom-profile"));
    }
}

void SkyEphemerisSettingsPolicyTests::defaultWritableCacheRootEndsWithEphemerisData()
{
    const QString writableCacheRoot = SkyEphemerisSettingsPolicy::defaultWritableCacheRoot();
    QVERIFY(!writableCacheRoot.isEmpty());
    QVERIFY(writableCacheRoot.endsWith(QStringLiteral("/ephemeris-data")));
}

QTEST_GUILESS_MAIN(SkyEphemerisSettingsPolicyTests)

#include "SkyEphemerisSettingsPolicyTests.moc"
