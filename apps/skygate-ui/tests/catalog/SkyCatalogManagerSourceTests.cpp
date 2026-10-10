#include "BaseCelestialBody.hpp"
#include "CatalogBodyQuery.hpp"
#include "CatalogTestPayloadFile.hpp"
#include "CatalogTestPayloads.hpp"
#include "ConstellationTestSupport.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "RelatedDatasetFixture.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogPresets.hpp"
#include "SkyCatalogSourceDescriptor.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

#include <QDir>
#include <QLocale>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

#include <algorithm>
#include <optional>
#include <span>

using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogTestPayloadFile;
using skygate::ui::tests::RelatedDatasetFixture;

namespace {

const skygate::ui::internal::SkyCatalogSourceDescriptor kHygPreset =
    skygate::ui::internal::SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v42")).value();

}  // namespace

class SkyCatalogManagerSourceTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void unknownPresetsUpdateStatus();
    void bundledPresetResetsCatalogAndConstellationRefs();
    void bundledReplacementDropsOwnerDataAndReportsFreshCounts();
    void bundledDeepSkyPresetRebuildsActiveCatalog();
    void clearCacheReportsStatusAndSignals();
    void collectionSourcesLoadEnableDisableAndRemoveIndependently();
    void addSourceUrlAddsSameCategorySourcesWithStableIdentity();
    void addSourcePresetUsesDescriptorPolicy();
    void moveSourceReordersActiveSources();
    void sameDescriptorInstancesCoexistIndependently();
    void sameUrlDifferentVersionsStayDistinct();
    void editingSourceAsUpdatePreservesInstanceId();
    void missingConfigurationKeepsFirstUseDefaults();
    void removingSoleSourceLeavesEmptyCollection();
    void presentationSummarizesEnabledCollectionParticipation();
    void bundledFallbackPresentationFollowsParticipationAndRestart();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerSourceTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerSourceTests")));
}

void SkyCatalogManagerSourceTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void SkyCatalogManagerSourceTests::unknownPresetsUpdateStatus()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.loadCatalogPreset("unknown");
    QCOMPARE(manager.statusText(), QString("Catalog: Unknown preset 'unknown'"));
    QCOMPARE(statusSpy.count(), 1);

    manager.loadDeepSkyCatalogPreset("unknown_dso");
    QCOMPARE(manager.statusText(), QString("Catalog: Unknown deep-sky preset 'unknown_dso'"));
    QCOMPARE(statusSpy.count(), 2);
}

void SkyCatalogManagerSourceTests::bundledPresetResetsCatalogAndConstellationRefs()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const auto originalRevision = manager.catalogRevision();
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.setCatalogPresetIndex(99);
    manager.loadCatalogPreset("bundled");

    QCOMPARE(manager.catalogPresetIndex(), 0);
    QCOMPARE(manager.sourceLabel(), QString("Bundled"));
    QVERIFY(manager.catalogRevision() > originalRevision);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QVERIFY(catalogSpy.count() >= 1);
}

void SkyCatalogManagerSourceTests::bundledReplacementDropsOwnerDataAndReportsFreshCounts()
{
    const QString catalogUrl = QStringLiteral("https://example.test/bundled-replacement-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/bundled-replacement-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    // The legacy primary slot owns a related dataset before the bundled preset
    // replaces its catalog.
    skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    source.instanceId = QStringLiteral("primary");
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(!manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationCount() > 0U);

    manager.loadCatalogPreset("bundled");

    // The bundled replacement drops the owned related dataset, and the status
    // text of that committed replacement reports the post-clear counts instead
    // of the pre-clear ones.
    QVERIFY(manager.constellationLineRefs().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
    const QLocale locale = QLocale::system();
    QVERIFY(manager.statusText().endsWith(
        QStringLiteral("%1 constellations)").arg(locale.toString(static_cast<qulonglong>(manager.constellationCount())))
    ));
}

void SkyCatalogManagerSourceTests::bundledDeepSkyPresetRebuildsActiveCatalog()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const auto originalRevision = manager.catalogRevision();
    QSignalSpy infoSpy(&manager, &SkyCatalogManager::deepSkyCatalogInfoTextChanged);

    manager.setDeepSkyCatalogPresetIndex(99);
    manager.loadDeepSkyCatalogPreset("bundled_messier");

    QCOMPARE(manager.deepSkyCatalogPresetIndex(), 0);
    QVERIFY(manager.catalogRevision() > originalRevision);
    QVERIFY(manager.deepSkyCatalogInfoText().contains("Objects:"));
    QVERIFY(infoSpy.count() >= 1);
}

void SkyCatalogManagerSourceTests::clearCacheReportsStatusAndSignals()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    QVERIFY(manager.clearCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Star catalog cache cleared"));
    QVERIFY(manager.clearDeepSkyCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Deep-sky catalog cache cleared"));
    QCOMPARE(statusSpy.count(), 2);
}

void SkyCatalogManagerSourceTests::collectionSourcesLoadEnableDisableAndRemoveIndependently()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName, const int hip) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!CatalogTestPayloadFile::write(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = hip, .hip = hip, .properName = QByteArray(properName), .mag = QByteArray("1.0")}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QCOMPARE(manager.sourceCount(), std::size_t{1});

    const auto loadLocalSource = [&](const QString& url) {
        manager.loadSource(
            skygate::ui::internal::SkyCatalogSourceInstance::createCustom(url),
            skygate::ephemeris::CatalogCompositionPolicy::Merge
        );
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    const QString sourceAUrl = writeSourceFile(QStringLiteral("collection-a.csv"), "Collection Star A", 901001);
    const QString sourceBUrl = writeSourceFile(QStringLiteral("collection-b.csv"), "Collection Star B", 901002);
    const QString sourceCUrl = writeSourceFile(QStringLiteral("collection-c.csv"), "Collection Star C", 901003);
    QVERIFY(!sourceAUrl.isEmpty());
    QVERIFY(!sourceBUrl.isEmpty());
    QVERIFY(!sourceCUrl.isEmpty());

    loadLocalSource(sourceAUrl);
    const QString sourceAId = manager.sourceInstanceIds().last();
    loadLocalSource(sourceBUrl);
    loadLocalSource(sourceCUrl);

    QCOMPARE(manager.sourceCount(), std::size_t{4});
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star B")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star C")));

    manager.disableSource(sourceAId);
    QVERIFY(!manager.isSourceEnabled(sourceAId));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star B")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star C")));

    manager.enableSource(sourceAId);
    QVERIFY(manager.isSourceEnabled(sourceAId));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));

    manager.removeSource(sourceAId);
    QCOMPARE(manager.sourceCount(), std::size_t{3});
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star B")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star C")));
}

void SkyCatalogManagerSourceTests::addSourceUrlAddsSameCategorySourcesWithStableIdentity()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName, const int hip) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!CatalogTestPayloadFile::write(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = hip, .hip = hip, .properName = QByteArray(properName), .mag = QByteArray("1.0")}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    const QString firstUrl = writeSourceFile(QStringLiteral("same-category-a.csv"), "Same Category A", 902001);
    const QString secondUrl = writeSourceFile(QStringLiteral("same-category-b.csv"), "Same Category B", 902002);
    QVERIFY(!firstUrl.isEmpty());
    QVERIFY(!secondUrl.isEmpty());

    manager.addSourceUrl(firstUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString firstId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(secondUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString secondId = manager.sourceInstanceIds().last();

    QCOMPARE(manager.sourceCount(), std::size_t{3});
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Same Category A")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Same Category B")));

    QVERIFY(firstId != secondId);
    QVERIFY(manager.sourceInstanceIds().contains(firstId));
    QVERIFY(manager.sourceInstanceIds().contains(secondId));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    QCOMPARE(view.size(), 3);
    QCOMPARE(view[1].instanceId, firstId);
    QCOMPARE(view[2].instanceId, secondId);
    QCOMPARE(view[1].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCOMPARE(view[2].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
}

void SkyCatalogManagerSourceTests::addSourcePresetUsesDescriptorPolicy()
{
    const QString hygUrl = kHygPreset.urls.value(0);
    const std::optional<skygate::ui::internal::SkyCatalogSourceDescriptor> openNgcPreset =
        skygate::ui::internal::SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("open_ngc"));
    QVERIFY(openNgcPreset.has_value());
    const QString openNgcUrl = openNgcPreset->urls.value(0);

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        hygUrl,
        {.payload =
             skygate::ui::tests::sampleHygCsvPayload({.hip = 902010, .properName = "Preset HYG Star", .mag = "1.0"})}
    );
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0998", .ngc = "0998", .identifiers = "PGC 9998", .commonName = "Preset Galaxy"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    manager.addSourcePreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString hygInstanceId = manager.sourceInstanceIds().last();
    QVERIFY(manager.sourceInstanceIds().contains(hygInstanceId));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Preset HYG Star")));

    manager.addSourcePreset(QStringLiteral("open_ngc"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString openNgcInstanceId = manager.sourceInstanceIds().last();
    QVERIFY(manager.sourceInstanceIds().contains(openNgcInstanceId));
    QVERIFY(hygInstanceId != openNgcInstanceId);

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto findViewEntry = [&view](const QString& instanceId) {
        return std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    };
    const auto hygView = findViewEntry(hygInstanceId);
    const auto openNgcView = findViewEntry(openNgcInstanceId);
    QVERIFY(hygView != view.end());
    QVERIFY(openNgcView != view.end());
    QCOMPARE(hygView->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCOMPARE(openNgcView->policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
}

void SkyCatalogManagerSourceTests::moveSourceReordersActiveSources()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName, const int hip) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!CatalogTestPayloadFile::write(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = hip, .hip = hip, .properName = QByteArray(properName), .mag = QByteArray("1.0")}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    const QString aUrl = writeSourceFile(QStringLiteral("move-a.csv"), "Move Star A", 902020);
    const QString bUrl = writeSourceFile(QStringLiteral("move-b.csv"), "Move Star B", 902021);
    QVERIFY(!aUrl.isEmpty());
    QVERIFY(!bUrl.isEmpty());

    manager.addSourceUrl(aUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString aId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(bUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString bId = manager.sourceInstanceIds().last();
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), aId, bId}));

    manager.moveSource(bId, 1);
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), bId, aId}));
}

void SkyCatalogManagerSourceTests::sameDescriptorInstancesCoexistIndependently()
{
    const std::optional<skygate::ui::internal::SkyCatalogSourceDescriptor> openNgcPreset =
        skygate::ui::internal::SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("open_ngc"));
    QVERIFY(openNgcPreset.has_value());
    const QString openNgcUrl = openNgcPreset->urls.value(0);

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0991",
              .type = "G",
              .ra = "00:20:00.00",
              .dec = "+10:00:00.0",
              .messier = "",
              .ngc = "0991",
              .identifiers = "PGC 9991",
              .commonName = "First Galaxy"}
         )}
    );
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0992",
              .type = "G",
              .ra = "00:30:00.00",
              .dec = "+11:00:00.0",
              .messier = "",
              .ngc = "0992",
              .identifiers = "PGC 9992",
              .commonName = "Second Galaxy"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    manager.addSourcePreset(QStringLiteral("open_ngc"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString firstId = manager.sourceInstanceIds().last();
    manager.addSourcePreset(QStringLiteral("open_ngc"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString secondId = manager.sourceInstanceIds().last();

    QVERIFY(firstId != secondId);
    QCOMPARE(manager.sourceCount(), std::size_t{3});
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 992")));

    // Each instance participates independently.
    manager.disableSource(firstId);
    QVERIFY(!manager.isSourceEnabled(firstId));
    QVERIFY(manager.isSourceEnabled(secondId));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 992")));
    manager.enableSource(firstId);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));

    // Each instance reloads independently.
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0993",
              .type = "G",
              .ra = "00:35:00.00",
              .dec = "+11:30:00.0",
              .messier = "",
              .ngc = "0993",
              .identifiers = "PGC 9993",
              .commonName = "Reloaded Galaxy"}
         )}
    );
    manager.retrySource(firstId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), firstId, secondId}));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 993")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 992")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));

    // Each instance reorders independently.
    manager.moveSource(secondId, 1);
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), secondId, firstId}));

    // Restart restores the bundled source and both instances with their
    // durable IDs and order.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), secondId, firstId}));
    QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 993")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 992")));

    // Each instance removes independently.
    restoredManager.removeSource(firstId);
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), secondId}));
    QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 992")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 993")));
}

void SkyCatalogManagerSourceTests::sameUrlDifferentVersionsStayDistinct()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("versioned-star.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            catalogPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 903001, .hip = 903001, .properName = "Versioned Star", .mag = "1.0"}
            )
        )
    );
    const QString catalogUrl = QUrl::fromLocalFile(catalogPath).toString();

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    skygate::ui::internal::SkyCatalogSourceInstance first =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl, QStringLiteral("v1"));
    first.archiveSelector = QStringLiteral("members/catalog-v1.csv");
    skygate::ui::internal::SkyCatalogSourceInstance second =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl, QStringLiteral("v2"));
    second.archiveSelector = QStringLiteral("members/catalog-v2.csv");

    manager.loadSource(first, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(second, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QVERIFY(first.instanceId != second.instanceId);
    QCOMPARE(manager.sourceCount(), std::size_t{3});

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto findEntry = [&view](const QString& instanceId) {
        return std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    };
    const auto firstEntry = findEntry(first.instanceId);
    const auto secondEntry = findEntry(second.instanceId);
    QVERIFY(firstEntry != view.end());
    QVERIFY(secondEntry != view.end());
    QCOMPARE(firstEntry->version, QStringLiteral("v1"));
    QCOMPARE(secondEntry->version, QStringLiteral("v2"));

    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    QCOMPARE(snapshot.snapshot.sources.size(), 3);
    QCOMPARE(snapshot.snapshot.sources[0].instanceId, QStringLiteral("primary"));
    QVERIFY(snapshot.snapshot.sources[0].bundled);
    QCOMPARE(snapshot.snapshot.sources[1].instanceId, first.instanceId);
    QCOMPARE(snapshot.snapshot.sources[1].version, QStringLiteral("v1"));
    QCOMPARE(snapshot.snapshot.sources[1].archiveSelector, QStringLiteral("members/catalog-v1.csv"));
    QCOMPARE(snapshot.snapshot.sources[2].instanceId, second.instanceId);
    QCOMPARE(snapshot.snapshot.sources[2].version, QStringLiteral("v2"));
    QCOMPARE(snapshot.snapshot.sources[2].archiveSelector, QStringLiteral("members/catalog-v2.csv"));

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), first.instanceId, second.instanceId})
    );
}

void SkyCatalogManagerSourceTests::editingSourceAsUpdatePreservesInstanceId()
{
    const QString firstPath = m_settings.filePath(QStringLiteral("edit-source-v1.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            firstPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 903010, .hip = 903010, .properName = "Edit Star V1", .mag = "1.0"}
            )
        )
    );
    const QString secondPath = m_settings.filePath(QStringLiteral("edit-source-v2.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            secondPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 903011, .hip = 903011, .properName = "Edit Star V2", .mag = "2.0"}
            )
        )
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(
            QUrl::fromLocalFile(firstPath).toString(), QStringLiteral("v1")
        );
    source.title = QStringLiteral("Original Title");
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString instanceId = source.instanceId;
    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Edit Star V1")));

    // Editing title, URL, and version updates the same configured instance
    // instead of adding another one with a new ID.
    source.title = QStringLiteral("Updated Title");
    source.urls = QStringList{QUrl::fromLocalFile(secondPath).toString()};
    source.version = QStringLiteral("v2");
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QCOMPARE(manager.sourceInstanceIds().count(instanceId), 1);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Edit Star V2")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Edit Star V1")));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto entry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& candidate) {
            return candidate.instanceId == instanceId;
        });
    QVERIFY(entry != view.end());
    QCOMPARE(entry->title, QStringLiteral("Updated Title"));
    QCOMPARE(entry->version, QStringLiteral("v2"));

    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    QCOMPARE(snapshot.snapshot.sources.size(), 2);
    QCOMPARE(snapshot.snapshot.sources[0].instanceId, QStringLiteral("primary"));
    QVERIFY(snapshot.snapshot.sources[0].bundled);
    QCOMPARE(snapshot.snapshot.sources[1].instanceId, instanceId);
    QCOMPARE(snapshot.snapshot.sources[1].version, QStringLiteral("v2"));
}

void SkyCatalogManagerSourceTests::missingConfigurationKeepsFirstUseDefaults()
{
    // This scenario needs a settings state with no stored configuration at all,
    // so its cache paths are isolated from the shared per-suite cache files.
    m_settings.resetForCurrentTest();

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    // First use has no stored configuration, so the bundled default source is
    // configured and composes the initial snapshot.
    QCOMPARE(manager.sourceInstanceIds(), QStringList{QStringLiteral("primary")});
    QVERIFY(manager.isSourceEnabled(QStringLiteral("primary")));
    QVERIFY(manager.starCatalog() != nullptr);
    QVERIFY(manager.bodyCount() > 0U);
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());

    // Nothing was ever stored: restore reports that and keeps the first-use
    // default instead of clearing the collection.
    QVERIFY(!manager.restoreCatalogCache());
    QCOMPARE(manager.sourceInstanceIds(), QStringList{QStringLiteral("primary")});
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());

    // A fresh manager over the still empty configuration receives the same
    // first-use default.
    SkyCatalogManager restarted(&store);
    QVERIFY(!restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds(), QStringList{QStringLiteral("primary")});
    QVERIFY(restarted.isSourceEnabled(QStringLiteral("primary")));
    QVERIFY(restarted.starCatalog() != nullptr);
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());
}

void SkyCatalogManagerSourceTests::removingSoleSourceLeavesEmptyCollection()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QCOMPARE(manager.sourceCount(), std::size_t{1});
    QSignalSpy sourcesSpy(&manager, &SkyCatalogManager::sourcesChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy datasetSpy(&manager, &SkyCatalogManager::datasetInfoTextChanged);

    manager.removeSource(QStringLiteral("primary"));

    // The removed instance is neither recreated nor replaced by an implicit
    // preset row, and the removal is published to the UI.
    QCOMPARE(manager.sourceCount(), std::size_t{0});
    QVERIFY(manager.sourceInstanceIds().isEmpty());
    QVERIFY(manager.sourceViewEntries().isEmpty());
    QVERIFY(!manager.sourceTitles().contains(QStringLiteral("primary")));
    QVERIFY(catalogSpy.count() >= 1);
    QVERIFY(datasetSpy.count() >= 1);
    QVERIFY(sourcesSpy.count() >= 1);

    // The active snapshot is the documented bundled augmentation: the bundled
    // core and the bundled deep-sky fallback participate under their own
    // provenance without a configured source row.
    QVERIFY(manager.starCatalog() != nullptr);
    QVERIFY(manager.bodyCount() > 0U);
    const auto bodies = manager.starCatalog()->bodies();
    const std::size_t deepSkyBodyCount = static_cast<std::size_t>(
        std::count_if(bodies.begin(), bodies.end(), [](const skygate::ephemeris::BaseCelestialBody* body) {
            return body != nullptr && body->kind == skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
        })
    );
    QVERIFY(deepSkyBodyCount > 0U);
    QCOMPARE(manager.sourceLabel(), QStringLiteral("Bundled core"));
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled core + Bundled Messier"));
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: Bundled core + Bundled Messier")));
    const std::span<const QString> composedSourceIds = manager.sourceIds();
    QVERIFY(!composedSourceIds.empty());
    QVERIFY(std::all_of(composedSourceIds.begin(), composedSourceIds.end(), [](const QString& sourceId) {
        return sourceId == QStringLiteral("bundled-core") || sourceId == QStringLiteral("bundled-deep-sky");
    }));

    // Removing the last source commits the empty collection instead of
    // clearing the configuration back to first use.
    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QVERIFY(persisted.snapshot.sources.isEmpty());
}

void SkyCatalogManagerSourceTests::presentationSummarizesEnabledCollectionParticipation()
{
    const QString starAPath = m_settings.filePath(QStringLiteral("presentation-star-a.csv"));
    const QString starBPath = m_settings.filePath(QStringLiteral("presentation-star-b.csv"));
    const QString deepSkyPath = m_settings.filePath(QStringLiteral("presentation-dso.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            starAPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 900701, .hip = 900701, .properName = "Presentation Star A", .mag = "1.0"}
            )
        )
    );
    QVERIFY(
        CatalogTestPayloadFile::write(
            starBPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 900702, .hip = 900702, .properName = "Presentation Star B", .mag = "2.0"}
            )
        )
    );
    QVERIFY(
        CatalogTestPayloadFile::write(
            deepSkyPath,
            skygate::ui::tests::sampleOpenNgcCsvPayload(
                {.name = "NGC0702",
                 .type = "G",
                 .ra = "00:42:44.35",
                 .dec = "+41:16:08.6",
                 .messier = "",
                 .ngc = "0702",
                 .identifiers = "PGC 7002",
                 .commonName = "Presentation Galaxy"}
            )
        )
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    // The bundled star catalog already supplies the bundled deep-sky objects,
    // so the enabled fallback contributes nothing and is not named.
    QVERIFY(manager.participationSummary().startsWith(QStringLiteral("Bundled")));
    QVERIFY(!manager.participationSummary().contains(QStringLiteral("Bundled Messier")));

    // Three mixed configured sources: two star sources and one deep-sky source.
    skygate::ui::internal::SkyCatalogSourceInstance starA =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starAPath).toString());
    starA.title = QStringLiteral("Alpha");
    skygate::ui::internal::SkyCatalogSourceInstance starB =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starBPath).toString());
    starB.title = QStringLiteral("Beta");
    skygate::ui::internal::SkyCatalogSourceInstance deepSky =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(deepSkyPath).toString());
    deepSky.title = QStringLiteral("Gamma");

    manager.loadSource(starA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(starB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(deepSky, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The summary names every enabled source in visible collection order, and
    // the status text pairs it with the active snapshot counts.
    QCOMPARE(manager.sourceCount(), std::size_t{4});
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Alpha + Beta + Gamma"));
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: %1 (").arg(manager.participationSummary())));
    const QLocale locale = QLocale::system();
    QVERIFY(manager.statusText().contains(
        QStringLiteral("(%1 objects,").arg(locale.toString(static_cast<qulonglong>(manager.bodyCount())))
    ));

    // Disabling one source removes it from the summary and keeps its own state
    // row, so a configured but disabled source is never presented as active.
    const int statusChangesBeforeDisable = statusSpy.count();
    manager.disableSource(starB.instanceId);
    QVERIFY(statusSpy.count() > statusChangesBeforeDisable);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Alpha + Gamma"));
    QVERIFY(!manager.statusText().contains(QStringLiteral("Beta")));

    const auto findViewEntry = [](const QVector<SkyCatalogManager::SourceViewEntry>& entries,
                                  const QString& instanceId) {
        return std::find_if(
            entries.begin(), entries.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
                return entry.instanceId == instanceId;
            }
        );
    };
    const QVector<SkyCatalogManager::SourceViewEntry> disabledView = manager.sourceViewEntries();
    QCOMPARE(disabledView.size(), 4);
    const auto disabledEntry = findViewEntry(disabledView, starB.instanceId);
    QVERIFY(disabledEntry != disabledView.end());
    QVERIFY(!disabledEntry->enabled);
    QCOMPARE(disabledEntry->statusText, QStringLiteral("Disabled"));
    const auto enabledEntry = findViewEntry(disabledView, starA.instanceId);
    QVERIFY(enabledEntry != disabledView.end());
    QVERIFY(enabledEntry->enabled);
    QCOMPARE(enabledEntry->statusText, QStringLiteral("Active"));
    const auto deepSkyEntry = findViewEntry(disabledView, deepSky.instanceId);
    QVERIFY(deepSkyEntry != disabledView.end());
    QCOMPARE(deepSkyEntry->title, QStringLiteral("Gamma"));
    QCOMPARE(deepSkyEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);

    // Re-enabling restores the source in its collection position.
    manager.enableSource(starB.instanceId);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Alpha + Beta + Gamma"));

    // Reordering the collection reorders the summary.
    manager.moveSource(deepSky.instanceId, 1);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Gamma + Alpha + Beta"));

    // Removing a source drops its row, its title, and its objects.
    manager.removeSource(starA.instanceId);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Gamma + Beta"));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Presentation Star A")));
    QCOMPARE(manager.sourceViewEntries().size(), 3);
}

void SkyCatalogManagerSourceTests::bundledFallbackPresentationFollowsParticipationAndRestart()
{
    const QString starsPath = m_settings.filePath(QStringLiteral("fallback-stars.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            starsPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 900703, .hip = 900703, .properName = "Fallback Star", .mag = "1.0"}
            )
        )
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    // The bundled star source supplies the bundled deep-sky objects, so the
    // enabled fallback is not presented as the active deep-sky source.
    QVERIFY(!manager.participationSummary().contains(QStringLiteral("Bundled Messier")));

    skygate::ui::internal::SkyCatalogSourceInstance stars =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starsPath).toString());
    stars.title = QStringLiteral("Stars");
    manager.loadSource(stars, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // Without the bundled star source the explicit fallback supplies the
    // deep-sky identities and the summary names it through its provenance.
    manager.removeSource(QStringLiteral("primary"));
    QCOMPARE(manager.participationSummary(), QStringLiteral("Stars + Bundled core + Bundled Messier"));
    QVERIFY(manager.statusText().contains(QStringLiteral("Bundled Messier")));

    // Turning the fallback participation off drops its name and its objects.
    manager.setDeepSkyCatalogPresetIndex(1);
    manager.retrySource(stars.instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(manager.participationSummary(), QStringLiteral("Stars + Bundled core"));
    QVERIFY(!manager.statusText().contains(QStringLiteral("Bundled Messier")));

    // A restart restores the configured collection, the source titles, and the
    // fallback participation of the restarted configuration.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds().size(), 1);
    QVERIFY(restoredManager.participationSummary().startsWith(QStringLiteral("Stars (saved)")));
    QVERIFY(restoredManager.participationSummary().contains(QStringLiteral("Bundled Messier")));
    QVERIFY(restoredManager.statusText().startsWith(QStringLiteral("Catalog: Stars (saved)")));
}

QTEST_GUILESS_MAIN(SkyCatalogManagerSourceTests)

#include "SkyCatalogManagerSourceTests.moc"
