#include "SkyContextControllerTestSupport.hpp"

#include <cmath>

namespace {

void compareEpoch(
    const skygate::ephemeris::AstronomicalEpoch& actual, const skygate::ephemeris::AstronomicalEpoch& expected
)
{
    QCOMPARE(actual.timeScale, expected.timeScale);
    QVERIFY(std::abs(actual.julianDatePart1 - expected.julianDatePart1) < 1.0e-12);
    QVERIFY(std::abs(actual.julianDatePart2 - expected.julianDatePart2) < 1.0e-12);
}

skygate::ephemeris::AstronomicalEpoch expectedUtcEpoch(
    const int astronomicalYear, const int month, const int day, const int hour, const int minute, const int second
)
{
    const auto epoch = skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = astronomicalYear,
        .month = month,
        .day = day,
        .hour = hour,
        .minute = minute,
        .second = second,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

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
    void requestContextUsesSimpleEngineDefaults();
    void requestContextCombinesRestoredSettingsObserverTimeAndDataRevision();
    void requestContextConvertsBceUtcToAstronomicalEpoch();

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

void SkyContextControllerEphemerisSettingsTests::requestContextUsesSimpleEngineDefaults()
{
    const FakeTimeSource timeSource(QDateTime(QDate(2026, 5, 14), QTime(8, 45, 30), QTimeZone::UTC));
    const auto controller = createControllerWithTimeSource(timeSource, false);

    const auto requestContext = controller->ephemerisRequestContext();

    QCOMPARE(requestContext.request.context.utcTime, controller->skyContext().utcTime);
    QCOMPARE(requestContext.request.context.observer.latitudeDeg, controller->skyContext().observer.latitudeDeg);
    QCOMPARE(
        static_cast<std::uint8_t>(requestContext.request.options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(requestContext.request.options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections)
    );
    compareEpoch(requestContext.request.epoch, expectedUtcEpoch(2026, 5, 14, 8, 45, 30));
    QVERIFY(requestContext.activeDataSnapshot != nullptr);
    QCOMPARE(requestContext.ephemerisDataRevision, controller->ephemerisDataRevision());
    QCOMPARE(requestContext.catalogRevision, controller->catalogRevision());
}

void SkyContextControllerEphemerisSettingsTests::requestContextCombinesRestoredSettingsObserverTimeAndDataRevision()
{
    auto expectedSettings = customEphemerisUserSettings();
    expectedSettings.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    expectedSettings.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Apparent
                                       | skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction;
    expectedSettings.refractionEnabled = true;

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.ephemeris = expectedSettings;
    snapshot.ephemerisSettingsPresent = true;
    snapshot.utcEpochSeconds = QDateTime(QDate(2030, 7, 2), QTime(3, 4, 5), QTimeZone::UTC).toSecsSinceEpoch();
    snapshot.live = false;
    snapshot.latitudeDeg = 47.3769;
    snapshot.longitudeDeg = 8.5417;
    snapshot.elevationMeters = 408.0;
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);
    const auto requestContext = controller->ephemerisRequestContext();

    QCOMPARE(
        static_cast<std::uint8_t>(requestContext.request.options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(requestContext.request.options.correctionFlags),
        static_cast<std::uint32_t>(expectedSettings.correctionFlags)
    );
    QCOMPARE(requestContext.request.options.enableAtmosphericRefraction, expectedSettings.refractionEnabled);
    QCOMPARE(requestContext.request.options.atmosphericPressureHpa, expectedSettings.atmosphericPressureHpa);
    QCOMPARE(requestContext.request.options.atmosphericTemperatureC, expectedSettings.atmosphericTemperatureC);
    QCOMPARE(requestContext.request.options.relativeHumidity, expectedSettings.relativeHumidity);
    QCOMPARE(
        requestContext.request.options.observingWavelengthMicrometers, expectedSettings.observingWavelengthMicrometers
    );
    QCOMPARE(requestContext.request.context.observer.latitudeDeg, snapshot.latitudeDeg);
    QCOMPARE(requestContext.request.context.observer.longitudeDeg, snapshot.longitudeDeg);
    QCOMPARE(requestContext.request.context.observer.elevationMeters, snapshot.elevationMeters);
    compareEpoch(requestContext.request.epoch, expectedUtcEpoch(2030, 7, 2, 3, 4, 5));
    QVERIFY(requestContext.activeDataSnapshot != nullptr);
    QCOMPARE(requestContext.ephemerisDataRevision, controller->ephemerisDataRevision());
}

void SkyContextControllerEphemerisSettingsTests::requestContextConvertsBceUtcToAstronomicalEpoch()
{
    const auto controller = createController(false);
    QVERIFY(controller->setUtcDateTimeText("0044-03-15 BCE", "12:00:00"));

    const auto requestContext = controller->ephemerisRequestContext();

    compareEpoch(requestContext.request.epoch, expectedUtcEpoch(-43, 3, 15, 12, 0, 0));
}

QTEST_GUILESS_MAIN(SkyContextControllerEphemerisSettingsTests)

#include "SkyContextControllerEphemerisSettingsTests.moc"
