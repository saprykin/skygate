#include "CatalogCacheTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "CelestialBodyCatalog.hpp"
#include "LogCapture.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SkySettingsStore.hpp"
#include "SettingsTestFixture.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/SkyCatalogCacheController.hpp"

#include <QtTest/QtTest>

class SkyCatalogCacheControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void restoresSavedCatalogsAndConstellationLabels();
    void unreadableSavedStarCatalogFallsBack();
    void staleConstellationLineSchemaRequestsReset();
    void persistRoundTripsConstellationRows();
    void restoresBinaryCatalogPayloads();
    void corruptBinaryPayloadFallsBackToCsvParsing();
    void legacyBinarySchemaVersionFallsBackToCsvParsing();
    void logsCacheLifecycleSummariesAtInfoLevel();

private:
    SkySettingsStore::CatalogCacheSnapshot makeValidCacheSnapshot() const;

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogCacheControllerTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogCacheControllerTests")));
}

void SkyCatalogCacheControllerTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
}

SkySettingsStore::CatalogCacheSnapshot SkyCatalogCacheControllerTests::makeValidCacheSnapshot() const
{
    return skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Downloaded (saved) (saved)"),
         .deepSkySourceLabel = QStringLiteral("OpenNGC (saved)")}
    );
}

void SkyCatalogCacheControllerTests::restoresSavedCatalogsAndConstellationLabels()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(makeValidCacheSnapshot()));

    const skygate::ui::internal::SkyCatalogCacheController controller(&store);
    auto result = controller.restore(1, 1);

    QVERIFY(result.restored);
    QVERIFY(!result.savedCatalogUnreadable);
    QVERIFY(result.catalog != nullptr);
    QVERIFY(result.deepSkyCatalog != nullptr);
    QVERIFY(result.requiresBinaryUpgrade);
    QCOMPARE(result.sourceLabel, QString("Downloaded (saved)"));
    QCOMPARE(result.deepSkySourceLabel, QString("OpenNGC (saved)"));
    QCOMPARE(result.constellationLineRefs.size(), 1U);
    QCOMPARE(result.constellationLineRefs[0].first, std::string("hip_1"));
    QCOMPARE(result.constellationLineRefs[0].second, std::string("hip_2"));
    QCOMPARE(result.constellationAnchorGroups.size(), 1U);
    QCOMPARE(result.constellationAnchorGroups[0].first, std::string("Demo"));
    QCOMPARE(result.constellationAnchorGroups[0].second.size(), 2U);
    QVERIFY(result.constellationCount.has_value());
    QCOMPARE(*result.constellationCount, 1U);
}

void SkyCatalogCacheControllerTests::unreadableSavedStarCatalogFallsBack()
{
    auto snapshot = makeValidCacheSnapshot();
    snapshot.catalogPayload = "this is not a catalog";

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(snapshot));

    const skygate::ui::internal::SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(QtWarningMsg, "Catalog payload parse failed: Catalog payload format is not recognized.");
    QTest::ignoreMessage(
        QtWarningMsg,
        "Saved star catalog cache unreadable; using bundled catalog: Catalog payload format is not recognized."
    );
    const auto result = controller.restore(1, 0);

    QVERIFY(!result.restored);
    QVERIFY(result.savedCatalogUnreadable);
    QCOMPARE(result.statusText, QString("Catalog: Saved cache unreadable, using bundled"));
    QVERIFY(result.catalog == nullptr);
}

void SkyCatalogCacheControllerTests::staleConstellationLineSchemaRequestsReset()
{
    auto snapshot = makeValidCacheSnapshot();
    snapshot.constellationLineSchemaVersion = 1;

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(snapshot));

    const skygate::ui::internal::SkyCatalogCacheController controller(&store);
    const auto result = controller.restore(1, 0);

    QVERIFY(result.restored);
    QVERIFY(result.catalog != nullptr);
    QVERIFY(result.resetConstellationLineRefs);
    QVERIFY(result.constellationLineRefs.empty());
}

void SkyCatalogCacheControllerTests::persistRoundTripsConstellationRows()
{
    SkySettingsStore store;
    const skygate::ui::internal::SkyCatalogCacheController controller(&store);

    skygate::ui::internal::SkyCatalogCachePersistRequest request;
    request.sourceLabel = "Custom";
    request.catalogPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.hip = 11, .properName = "Alpha", .ra = "1.0", .dec = "2.0", .mag = "3.0"}
    );
    request.constellationLineRefs = {{"hip_a", "hip_b"}};
    request.constellationAnchorGroups = {{"Label", {"hip_a", "hip_b", "hip_c"}}};
    request.constellationCount = 12;
    controller.persist(request);

    const auto result = controller.restore(1, 0);
    QVERIFY(result.restored);
    QCOMPARE(result.sourceLabel, QString("Custom (saved)"));
    QCOMPARE(result.constellationLineRefs.size(), 1U);
    QCOMPARE(result.constellationAnchorGroups.size(), 1U);
    QCOMPARE(result.constellationAnchorGroups[0].second.size(), 3U);
    QVERIFY(result.constellationCount.has_value());
    QCOMPARE(*result.constellationCount, 12U);
}

void SkyCatalogCacheControllerTests::restoresBinaryCatalogPayloads()
{
    SkySettingsStore store;
    const skygate::ui::internal::SkyCatalogCacheController controller(&store);

    skygate::ephemeris::OwnGalaxyCelestialBody star;
    star.id = "hip_11";
    star.displayName = "Alpha";
    star.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    star.visualMagnitude = 3.0;
    skygate::ephemeris::CelestialBodyCatalog catalog(std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{star});

    skygate::ui::internal::SkyCatalogCachePersistRequest request;
    request.sourceLabel = "Binary";
    request.catalogPayload = skygate::ui::tests::sampleHygCsvPayload();
    request.catalogBinaryPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(catalog);
    controller.persist(request);

    const auto result = controller.restore(1, 0);
    QVERIFY(result.restored);
    QVERIFY(result.catalog != nullptr);
    QCOMPARE(result.sourceLabel, QString("Binary (saved)"));
    QCOMPARE(result.catalog->bodies().size(), 1U);
    QCOMPARE(std::string{result.catalog->bodies()[0]->id}, std::string("hip_11"));
    QVERIFY(!result.requiresBinaryUpgrade);
}

void SkyCatalogCacheControllerTests::corruptBinaryPayloadFallsBackToCsvParsing()
{
    auto snapshot = makeValidCacheSnapshot();
    snapshot.catalogBinaryPayload = "corrupt binary payload";
    snapshot.deepSkyBinaryPayload = "also corrupt";
    snapshot.catalogBinarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(snapshot));

    const skygate::ui::internal::SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(QtWarningMsg, "Saved binary star catalog cache unreadable; falling back to payload parsing");
    QTest::ignoreMessage(
        QtWarningMsg, "Saved binary deep-sky catalog cache unreadable; falling back to payload parsing"
    );
    const auto result = controller.restore(1, 1);

    QVERIFY(result.restored);
    QVERIFY(result.catalog != nullptr);
    QVERIFY(result.deepSkyCatalog != nullptr);
    QVERIFY(result.requiresBinaryUpgrade);
}

void SkyCatalogCacheControllerTests::legacyBinarySchemaVersionFallsBackToCsvParsing()
{
    auto snapshot = makeValidCacheSnapshot();
    snapshot.catalogBinaryPayload = "legacy version 2 binary payload";
    snapshot.catalogBinarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion) - 1;

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(snapshot));

    const skygate::ui::internal::SkyCatalogCacheController controller(&store);
    const auto result = controller.restore(1, 0);

    QVERIFY(result.restored);
    QVERIFY(result.catalog != nullptr);
    QVERIFY(result.requiresBinaryUpgrade);
    QCOMPARE(result.catalog->bodies().size(), 1U);
}

void SkyCatalogCacheControllerTests::logsCacheLifecycleSummariesAtInfoLevel()
{
    SkySettingsStore store;
    const skygate::ui::internal::SkyCatalogCacheController controller(&store);
    skygate::ui::tests::LogCapture capture(QtInfoMsg);

    QVERIFY(store.saveCatalogCache(makeValidCacheSnapshot()));
    const auto result = controller.restore(1, 1);
    QVERIFY(result.restored);
    QVERIFY(controller.clearCatalogCache());

    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("Catalog cache saved: starBytes")));
    QVERIFY(messages.contains(QStringLiteral("Catalog cache loaded: starBytes")));
    QVERIFY(messages.contains(QStringLiteral("Saved star catalog cache restored: Downloaded (saved)")));
    QVERIFY(messages.contains(QStringLiteral("Saved deep-sky catalog cache restored: OpenNGC (saved)")));
    QVERIFY(messages.contains(QStringLiteral("Saved constellation line cache restored: segments 1 labels 1")));
    QVERIFY(messages.contains(QStringLiteral("Star catalog cache cleared: files")));
}

QTEST_GUILESS_MAIN(SkyCatalogCacheControllerTests)

#include "SkyCatalogCacheControllerTests.moc"
