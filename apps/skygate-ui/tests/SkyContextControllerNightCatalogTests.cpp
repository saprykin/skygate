#include "SkyContextControllerTestSupport.hpp"

class SkyContextControllerNightCatalogTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void nightConditionsPopulateAndRefreshForValidObserver();
    void failedDeepSkyCatalogDownloadKeepsCountLabel();
    void restoresCachedCatalogConstellationCount();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyContextControllerNightCatalogTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyContextControllerNightCatalogTests")));
}

void SkyContextControllerNightCatalogTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyContextControllerNightCatalogTests::nightConditionsPopulateAndRefreshForValidObserver()
{
    const auto controller = createController();
    configureFocusTestContext(*controller);
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("Asia/Bishkek")));
    QVERIFY(controller->setUtcDateTimeText("2024-03-21", "12:00:00"));

    controller->refreshNightConditions();
    const QVariantMap initialConditions = controller->nightConditions();
    QVERIFY(initialConditions.value("valid").toBool());
    QCOMPARE(initialConditions.value("sunRows").toList().size(), 6);
    QVERIFY(initialConditions.value("locationText").toString().contains("UTC+06:00"));
    QVERIFY(initialConditions.value("moonPhaseText").toString().contains("%"));
    QVERIFY(initialConditions.value("moonRiseText").toString() != "--");
    QVERIFY(initialConditions.value("moonSetText").toString() != "--");
    QVERIFY(
        QStringList({"sun", "twilight", "moon"}).contains(controller->nightConditionsIconKind())
    );

    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("UTC")));
    const QVariantMap utcConditions = controller->nightConditions();
    QVERIFY(utcConditions.value("valid").toBool());
    QVERIFY(utcConditions.value("locationText").toString().contains("UTC"));
    QVERIFY(!utcConditions.value("locationText").toString().contains("UTC+06:00"));
    QVERIFY(utcConditions != initialConditions);

    QVERIFY(controller->setUtcDateTimeText("2024-03-22", "12:00:00"));
    controller->refreshNightConditions();
    const QVariantMap refreshedConditions = controller->nightConditions();
    QVERIFY(refreshedConditions.value("valid").toBool());
    QVERIFY(refreshedConditions != initialConditions);
}

void SkyContextControllerNightCatalogTests::failedDeepSkyCatalogDownloadKeepsCountLabel()
{
    const auto controller = createController();
    QSignalSpy infoSpy(
        controller.get(),
        &SkyContextController::deepSkyCatalogInfoTextChanged
    );

    QCOMPARE(controller->deepSkyCatalogInfoText(), QString("Objects: 110"));
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog download aborted: no valid source URLs"
    );
    controller->downloadDeepSkyCatalogFromUrl(QString());

    QCOMPARE(infoSpy.count(), 0);
    QCOMPARE(controller->deepSkyCatalogInfoText(), QString("Objects: 110"));
    QCOMPARE(controller->catalogStatusText(), QString("Catalog: Invalid URL"));
}

void SkyContextControllerNightCatalogTests::restoresCachedCatalogConstellationCount()
{
    QSettings settings;
    settings.setValue(
        "skyContext/catalogCachePath",
        m_settings.filePath(QStringLiteral("cached-hyg-catalog.csv"))
    );

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot stateSnapshot;
    stateSnapshot.catalogPresetIndex = 1;
    QVERIFY(store.saveState(stateSnapshot));

    SkySettingsStore::CatalogCacheSnapshot cacheSnapshot;
    cacheSnapshot.sourceLabel = "HYG v4.2";
    cacheSnapshot.catalogPayload =
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Demo Star,6.7525,-16.7161,-1.46\n";
    cacheSnapshot.constellationLineRows = "hyg_1|hyg_1\n";
    cacheSnapshot.constellationLabelRows = "Demo|hyg_1\n";
    cacheSnapshot.constellationLineSchemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
    cacheSnapshot.constellationCount = 88;
    QVERIFY(store.saveCatalogCache(cacheSnapshot));

    const auto controller = createController(true);

    QCOMPARE(controller->catalogPresetIndex(), 1);
    QVERIFY(controller->catalogDatasetInfoText().contains("Constellations: 88"));
}

QTEST_GUILESS_MAIN(SkyContextControllerNightCatalogTests)

#include "SkyContextControllerNightCatalogTests.moc"
