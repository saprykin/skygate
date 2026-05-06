#include "catalog/SkyCatalogConstellationStore.hpp"
#include "catalog/SkyCatalogText.hpp"

#include <QtTest/QtTest>

#include <string>
#include <vector>

class SkyCatalogInternalsTests final : public QObject {
    Q_OBJECT

private slots:
    void catalogTextFormatsStableStatusMessages();
    void catalogTextComposesSummaryDetails();
    void constellationStoreKeepsCustomRefsAndCounts();
    void constellationStoreFallsBackToBundledLinesWhenCustomLinesAreEmpty();
};

void SkyCatalogInternalsTests::catalogTextFormatsStableStatusMessages()
{
    using skygate::ui::internal::SkyCatalogText;

    QCOMPARE(SkyCatalogText::datasetInfo(12U, 3U), QString("Objects: 12 | Constellations: 3"));
    QCOMPARE(SkyCatalogText::deepSkyCatalogInfo(9U), QString("Objects: 9"));
    QCOMPARE(
        SkyCatalogText::unknownCatalogPreset("demo"),
        QString("Catalog: Unknown preset 'demo'")
    );
    QCOMPARE(
        SkyCatalogText::unknownDeepSkyPreset("deep"),
        QString("Catalog: Unknown deep-sky preset 'deep'")
    );
    QCOMPARE(
        SkyCatalogText::cacheClearBlocked(),
        QString("Catalog: Cannot clear cache while download is in progress")
    );
    QCOMPARE(
        SkyCatalogText::starCacheClearResult(true),
        QString("Catalog: Star catalog cache cleared")
    );
    QCOMPARE(
        SkyCatalogText::starCacheClearResult(false),
        QString("Catalog: Star catalog cache clear failed")
    );
    QCOMPARE(
        SkyCatalogText::deepSkyCacheClearResult(true),
        QString("Catalog: Deep-sky catalog cache cleared")
    );
    QCOMPARE(
        SkyCatalogText::deepSkyCacheClearResult(false),
        QString("Catalog: Deep-sky catalog cache clear failed")
    );
    QVERIFY(SkyCatalogText::isProcessingStatus("catalog: processing stars"));
    QVERIFY(!SkyCatalogText::isProcessingStatus("Catalog: Ready"));
}

void SkyCatalogInternalsTests::catalogTextComposesSummaryDetails()
{
    using skygate::ui::internal::SkyCatalogText;

    QCOMPARE(
        SkyCatalogText::brightnessFilterSummary("Catalog: Loaded", 5U, 12U),
        QString("Catalog: Loaded | Brightness filter kept 5 of 12 objects")
    );
    QCOMPARE(
        SkyCatalogText::constellationLineSummary("Objects: 12", "Catalog: Loaded 4 line refs"),
        QString("Objects: 12 | Constellation lines: Loaded 4 line refs")
    );
    QCOMPARE(
        SkyCatalogText::constellationLineSummary("Objects: 12", "Imported 4 line refs"),
        QString("Objects: 12 | Constellation lines: Imported 4 line refs")
    );
    QCOMPARE(
        SkyCatalogText::deepSkyFoundSummary("Catalog: Loaded", "NGC", 8U),
        QString("Catalog: Loaded | NGC found 8 objects")
    );
}

void SkyCatalogInternalsTests::constellationStoreKeepsCustomRefsAndCounts()
{
    skygate::ui::internal::SkyCatalogConstellationStore store;
    const std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs {
        {"hip_1", "hip_2"},
        {"hip_2", "hip_3"},
    };
    const std::vector<skygate::ephemeris::ConstellationLabelRef> labelRefs {
        {"Demo", {"hip_1", "hip_2"}},
    };

    store.setLineRefs(lineRefs);
    store.setLabelRefs(labelRefs);
    store.setCount(42U);

    QCOMPARE(store.count(), 42U);
    QCOMPARE(store.lineRefs().size(), 2U);
    QCOMPARE(store.labelRefs().size(), 1U);
    QCOMPARE(store.lineRefVector().front().first, std::string("hip_1"));
    QCOMPARE(store.lineRefVector().front().second, std::string("hip_2"));
    QCOMPARE(store.labelRefVector().front().first, std::string("Demo"));
}

void SkyCatalogInternalsTests::constellationStoreFallsBackToBundledLinesWhenCustomLinesAreEmpty()
{
    skygate::ui::internal::SkyCatalogConstellationStore store;
    store.setLineRefs({{"custom_a", "custom_b"}});
    store.setLabelRefs({{"Custom", {"custom_a", "custom_b"}}});
    store.setCount(1U);

    store.setLineRefs({});

    QVERIFY(!store.lineRefs().empty());
    QVERIFY(!store.labelRefs().empty());
    QCOMPARE(store.count(), store.labelRefs().size());
    QVERIFY(store.lineRefVector().front().first != std::string("custom_a"));
}

QTEST_APPLESS_MAIN(SkyCatalogInternalsTests)

#include "SkyCatalogInternalsTests.moc"
