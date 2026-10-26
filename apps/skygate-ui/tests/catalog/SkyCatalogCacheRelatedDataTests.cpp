#include "catalog/SkyCatalogCacheController.hpp"
#include "catalog/SkyCatalogCollectionStore.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/CatalogCacheFixture.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using skygate::ui::internal::SkyCatalogCacheController;
using skygate::ui::tests::CatalogCacheFixture;

namespace {

constexpr int kCurrentCollectionSchemaVersion =
    skygate::ui::internal::SkyCatalogCollectionStore::kCollectionSchemaVersion;

}  // namespace

class SkyCatalogCacheRelatedDataTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void restoresCollectionSourcesAndConstellationLabels();
    void dropsPriorDuplicatedRelatedPayloads();

private:
    void resetSettings();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogCacheRelatedDataTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogCacheRelatedDataTests")));
}

void SkyCatalogCacheRelatedDataTests::init()
{
    resetSettings();
}

void SkyCatalogCacheRelatedDataTests::resetSettings()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), m_settings.filePath(QStringLiteral("collection-cache"))
    );
    QDir(m_settings.filePath(QStringLiteral("collection-cache"))).removeRecursively();
}

void SkyCatalogCacheRelatedDataTests::restoresCollectionSourcesAndConstellationLabels()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(CatalogCacheFixture::makeCollectionSnapshot()));

    const SkyCatalogCacheController controller(&store);
    auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QVERIFY(!result.migratedLegacy);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QCOMPARE(result.sources[0].record.instanceId, QString("preset:hyg_v42"));
    QCOMPARE(result.sources[0].record.title, QString("HYG v4.2 (saved)"));
    QVERIFY(result.sources[0].requiresBinaryUpgrade);
    QCOMPARE(result.sources[1].record.instanceId, QString("preset:open_ngc"));
    QCOMPARE(result.sources[1].record.title, QString("OpenNGC (saved)"));
    QVERIFY(result.sources[1].record.catalog != nullptr);

    // Related constellation data is owned by the record that stored it.
    const skygate::ui::internal::SkyCatalogConstellationStore& starConstellationData =
        result.sources[0].record.constellationData;
    QCOMPARE(starConstellationData.lineRefs().size(), 1U);
    QCOMPARE(starConstellationData.lineRefs()[0].first, std::string("hip_1"));
    QCOMPARE(starConstellationData.lineRefs()[0].second, std::string("hip_2"));
    QCOMPARE(starConstellationData.anchorGroups().size(), 1U);
    QCOMPARE(starConstellationData.count(), 1U);
    QVERIFY(result.sources[1].record.constellationData.lineRefs().empty());
}

void SkyCatalogCacheRelatedDataTests::dropsPriorDuplicatedRelatedPayloads()
{
    // The prior writer serialized the same composed collection-wide view into
    // every eligible source record. Identical copies in several records cannot
    // establish an owner, so none of them is attributed to a record.
    auto snapshot = CatalogCacheFixture::makeCollectionSnapshot();
    snapshot.schemaVersion = kCurrentCollectionSchemaVersion - 1;
    snapshot.sources[1].constellationLineRows = snapshot.sources[0].constellationLineRows;
    snapshot.sources[1].constellationAnchorGroupRows = snapshot.sources[0].constellationAnchorGroupRows;
    snapshot.sources[1].constellationLineSchemaVersion = snapshot.sources[0].constellationLineSchemaVersion;
    snapshot.sources[1].constellationCount = snapshot.sources[0].constellationCount;

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Duplicated pre-migration related constellation data"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Duplicated pre-migration related constellation data"));
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QVERIFY(result.requiresRecordUpgrade);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QVERIFY(result.sources[0].record.constellationData.lineRefs().empty());
    QVERIFY(result.sources[0].record.constellationData.anchorGroups().empty());
    QCOMPARE(result.sources[0].record.constellationData.count(), std::size_t{0});
    QVERIFY(result.sources[1].record.constellationData.lineRefs().empty());
    QVERIFY(result.sources[1].record.constellationData.anchorGroups().empty());
    QCOMPARE(result.sources[1].record.constellationData.count(), std::size_t{0});
}

QTEST_GUILESS_MAIN(SkyCatalogCacheRelatedDataTests)

#include "SkyCatalogCacheRelatedDataTests.moc"
