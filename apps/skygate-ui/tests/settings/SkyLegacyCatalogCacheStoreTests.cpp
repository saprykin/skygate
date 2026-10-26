#include "settings/SkyLegacyCatalogCacheStore.hpp"
#include "support/CatalogCacheTestSupport.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QSettings>
#include <QtTest/QtTest>

#include <cstddef>
#include <optional>

using skygate::ui::internal::SkyLegacyCatalogCacheStore;

class SkyLegacyCatalogCacheStoreTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsSeededCacheSlots();
    void reportsNoCacheWhenEverySlotIsEmpty();
    void clearsStarSlotAndKeepsDeepSkySlot();
    void clearsBothCacheSlots();

private:
    void resetStorage() const;

    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyLegacyCatalogCacheStoreTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkygateUiLegacyCatalogCacheStoreTests")));
}

void SkyLegacyCatalogCacheStoreTests::resetStorage() const
{
    QSettings settings;
    settings.clear();
    // The default binary cache slots live in the application data directory
    // shared by every test in this binary, so earlier runs are cleared here.
    QVERIFY(SkyLegacyCatalogCacheStore::clearStarCache());
    QVERIFY(SkyLegacyCatalogCacheStore::clearDeepSkyCache());
    m_settings.resetForCurrentTest();
}

void SkyLegacyCatalogCacheStoreTests::loadsSeededCacheSlots()
{
    resetStorage();

    SkyCatalogLegacyCacheSnapshot savedSnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot();
    savedSnapshot.catalogBinaryPayload = QByteArray("star-binary-payload");
    savedSnapshot.catalogBinarySchemaVersion = 7;
    savedSnapshot.deepSkyBinaryPayload = QByteArray("deep-sky-binary-payload");

    QVERIFY(skygate::ui::tests::seedLegacyCatalogCache(savedSnapshot));
    const std::optional<SkyCatalogLegacyCacheSnapshot> loadedSnapshot = SkyLegacyCatalogCacheStore::load();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->sourceLabel, savedSnapshot.sourceLabel);
    QCOMPARE(loadedSnapshot->deepSkySourceLabel, savedSnapshot.deepSkySourceLabel);
    QCOMPARE(loadedSnapshot->catalogPayload, savedSnapshot.catalogPayload);
    QCOMPARE(loadedSnapshot->deepSkyCatalogPayload, savedSnapshot.deepSkyCatalogPayload);
    QCOMPARE(loadedSnapshot->catalogBinaryPayload, savedSnapshot.catalogBinaryPayload);
    QCOMPARE(loadedSnapshot->deepSkyBinaryPayload, savedSnapshot.deepSkyBinaryPayload);
    QCOMPARE(loadedSnapshot->catalogBinarySchemaVersion, savedSnapshot.catalogBinarySchemaVersion);
    QCOMPARE(loadedSnapshot->constellationLineRows, savedSnapshot.constellationLineRows);
    QCOMPARE(loadedSnapshot->constellationAnchorGroupRows, savedSnapshot.constellationAnchorGroupRows);
    QCOMPARE(loadedSnapshot->constellationLineSchemaVersion, savedSnapshot.constellationLineSchemaVersion);
    QCOMPARE(loadedSnapshot->constellationCount, savedSnapshot.constellationCount);
}

void SkyLegacyCatalogCacheStoreTests::reportsNoCacheWhenEverySlotIsEmpty()
{
    resetStorage();

    QVERIFY(!SkyLegacyCatalogCacheStore::load().has_value());
}

void SkyLegacyCatalogCacheStoreTests::clearsStarSlotAndKeepsDeepSkySlot()
{
    resetStorage();

    const SkyCatalogLegacyCacheSnapshot savedSnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot();
    QVERIFY(skygate::ui::tests::seedLegacyCatalogCache(savedSnapshot));

    QVERIFY(SkyLegacyCatalogCacheStore::clearStarCache());
    const std::optional<SkyCatalogLegacyCacheSnapshot> starClearedSnapshot = SkyLegacyCatalogCacheStore::load();
    QVERIFY(starClearedSnapshot.has_value());
    QVERIFY(starClearedSnapshot->catalogPayload.isEmpty());
    QVERIFY(starClearedSnapshot->catalogBinaryPayload.isEmpty());
    QVERIFY(starClearedSnapshot->constellationLineRows.isEmpty());
    QVERIFY(starClearedSnapshot->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(starClearedSnapshot->constellationLineSchemaVersion, 0);
    QCOMPARE(starClearedSnapshot->constellationCount, std::size_t{0});
    QCOMPARE(starClearedSnapshot->deepSkySourceLabel, savedSnapshot.deepSkySourceLabel);
    QCOMPARE(starClearedSnapshot->deepSkyCatalogPayload, savedSnapshot.deepSkyCatalogPayload);
}

void SkyLegacyCatalogCacheStoreTests::clearsBothCacheSlots()
{
    resetStorage();

    QVERIFY(skygate::ui::tests::seedLegacyCatalogCache(skygate::ui::tests::sampleCatalogCacheSnapshot()));

    QVERIFY(SkyLegacyCatalogCacheStore::clearStarCache());
    QVERIFY(SkyLegacyCatalogCacheStore::clearDeepSkyCache());
    QVERIFY(!SkyLegacyCatalogCacheStore::load().has_value());

    QSettings settings;
    QVERIFY(!settings.contains("skyContext/catalogCachePath"));
    QVERIFY(!settings.contains("skyContext/deepSkyCatalogCachePath"));
    QVERIFY(!settings.contains("skyContext/catalogSourceLabel"));
    QVERIFY(!settings.contains("skyContext/deepSkyCatalogSourceLabel"));
}

QTEST_GUILESS_MAIN(SkyLegacyCatalogCacheStoreTests)

#include "SkyLegacyCatalogCacheStoreTests.moc"
