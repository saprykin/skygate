#include "SkyContextControllerTestSupport.hpp"

namespace {

SkySettingsStore::EphemerisUserSettingsSnapshot customEphemerisUserSettings()
{
    SkySettingsStore::EphemerisUserSettingsSnapshot settings;
    settings.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    settings.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime
                               | skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration;
    settings.correctionPresetId = QStringLiteral("astrometric");
    settings.fallbackToSimpleEngine = false;
    settings.refractionEnabled = false;
    settings.atmosphericPressureHpa = 802.5;
    settings.atmosphericTemperatureC = -6.0;
    settings.relativeHumidity = 0.42;
    settings.observingWavelengthMicrometers = 0.68;
    settings.preferredDataProfileId = QStringLiteral("de441-long-range");
    settings.onlineUpdatesEnabled = false;
    settings.updatePresetId = QStringLiteral("custom");
    settings.updateManifestUrl = QStringLiteral("https://example.com/skygate-ephemeris.json");
    return settings;
}

void verifyEphemerisUserSettings(
    const SkySettingsStore::EphemerisUserSettingsSnapshot& actual,
    const SkySettingsStore::EphemerisUserSettingsSnapshot& expected
)
{
    QCOMPARE(static_cast<std::uint8_t>(actual.engineKind), static_cast<std::uint8_t>(expected.engineKind));
    QCOMPARE(static_cast<std::uint32_t>(actual.correctionFlags), static_cast<std::uint32_t>(expected.correctionFlags));
    QCOMPARE(actual.correctionPresetId, expected.correctionPresetId);
    QCOMPARE(actual.fallbackToSimpleEngine, expected.fallbackToSimpleEngine);
    QCOMPARE(actual.refractionEnabled, expected.refractionEnabled);
    QCOMPARE(actual.atmosphericPressureHpa, expected.atmosphericPressureHpa);
    QCOMPARE(actual.atmosphericTemperatureC, expected.atmosphericTemperatureC);
    QCOMPARE(actual.relativeHumidity, expected.relativeHumidity);
    QCOMPARE(actual.observingWavelengthMicrometers, expected.observingWavelengthMicrometers);
    QCOMPARE(actual.preferredDataProfileId, expected.preferredDataProfileId);
    QCOMPARE(actual.onlineUpdatesEnabled, expected.onlineUpdatesEnabled);
    QCOMPARE(actual.updatePresetId, expected.updatePresetId);
    QCOMPARE(actual.updateManifestUrl, expected.updateManifestUrl);
}

}  // namespace

class SkyContextControllerEphemerisSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void loadSavePreservesAllEphemerisUserSettings();
    void loadSettingsWithEphemerisSettingsNotifiesSceneConsumers();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyContextControllerEphemerisSettingsTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyContextControllerEphemerisSettingsTests")));
}

void SkyContextControllerEphemerisSettingsTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyContextControllerEphemerisSettingsTests::loadSavePreservesAllEphemerisUserSettings()
{
    const SkySettingsStore::EphemerisUserSettingsSnapshot expectedSettings = customEphemerisUserSettings();

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.ephemeris = expectedSettings;
    snapshot.ephemerisSettingsPresent = true;
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);
    QVERIFY(controller->saveSettings());

    const auto savedSnapshot = store.loadState();
    QVERIFY(savedSnapshot.has_value());
    QVERIFY(savedSnapshot->ephemerisSettingsPresent);
    verifyEphemerisUserSettings(savedSnapshot->ephemeris, expectedSettings);
}

void SkyContextControllerEphemerisSettingsTests::loadSettingsWithEphemerisSettingsNotifiesSceneConsumers()
{
    const auto controller = createController(false);
    SkySettingsStore store;
    QVERIFY(controller->saveSettings());
    auto snapshot = store.loadState();
    QVERIFY(snapshot.has_value());
    snapshot->ephemeris = customEphemerisUserSettings();
    snapshot->ephemerisSettingsPresent = true;
    QVERIFY(store.saveState(*snapshot));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);

    QVERIFY(controller->loadSettings());

    QCOMPARE(skyContextChangedSpy.count(), 1);
    QVERIFY(controller->ephemerisEngine() != nullptr);
    const auto restoredOptions = controller->ephemerisEngine()->options();
    QCOMPARE(
        static_cast<std::uint32_t>(restoredOptions.correctionFlags),
        static_cast<std::uint32_t>(snapshot->ephemeris.correctionFlags)
    );
    QCOMPARE(restoredOptions.fallbackToSimpleEngine, snapshot->ephemeris.fallbackToSimpleEngine);
    QCOMPARE(restoredOptions.enableAtmosphericRefraction, snapshot->ephemeris.refractionEnabled);
}

QTEST_GUILESS_MAIN(SkyContextControllerEphemerisSettingsTests)

#include "SkyContextControllerEphemerisSettingsTests.moc"
