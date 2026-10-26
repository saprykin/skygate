#include "catalog/SkyCatalogConstellationStore.hpp"
#include "catalog/SkyCatalogImportStatus.hpp"
#include "catalog/SkyCatalogLegacySlot.hpp"
#include "catalog/SkyCatalogPresets.hpp"
#include "catalog/SkyCatalogText.hpp"

#include <QStringList>
#include <QtTest/QtTest>

#include <cstdint>
#include <string>
#include <vector>

class SkyCatalogInternalsTests final : public QObject {
    Q_OBJECT

private slots:
    void catalogTextFormatsStableStatusMessages();
    void catalogTextComposesSummaryDetails();
    void catalogTextDropsStatusPrefix();
    void catalogImportStatusKeepsPhaseIndependentOfText();
    void constellationStoreKeepsCustomRefsAndCounts();
    void constellationStoreClearDropsRefsAndCounts();
    void legacySlotNormalizesPresetIndexAndUrl();
};

void SkyCatalogInternalsTests::catalogTextFormatsStableStatusMessages()
{
    using skygate::ui::internal::SkyCatalogText;

    QCOMPARE(SkyCatalogText::datasetInfo(12U, 3U), QString("Objects: 12 | Constellations: 3"));
    QCOMPARE(SkyCatalogText::deepSkyCatalogInfo(9U), QString("Objects: 9"));
    QCOMPARE(SkyCatalogText::cacheClearBlocked(), QString("Catalog: Cannot clear cache while download is in progress"));
    QCOMPARE(SkyCatalogText::starCacheClearResult(true), QString("Catalog: Star catalog cache cleared"));
    QCOMPARE(SkyCatalogText::starCacheClearResult(false), QString("Catalog: Star catalog cache clear failed"));
    QCOMPARE(SkyCatalogText::deepSkyCacheClearResult(true), QString("Catalog: Deep-sky catalog cache cleared"));
    QCOMPARE(SkyCatalogText::deepSkyCacheClearResult(false), QString("Catalog: Deep-sky catalog cache clear failed"));
}

void SkyCatalogInternalsTests::catalogTextDropsStatusPrefix()
{
    using skygate::ui::internal::SkyCatalogText;

    QCOMPARE(QString::fromLatin1(SkyCatalogText::kStatusPrefix), QString("Catalog: "));
    QCOMPARE(SkyCatalogText::withoutStatusPrefix("Catalog: Loaded"), QString("Loaded"));
    QCOMPARE(SkyCatalogText::withoutStatusPrefix("Catalog: "), QString(""));
    QCOMPARE(SkyCatalogText::withoutStatusPrefix("Imported 4 line refs"), QString("Imported 4 line refs"));
    QCOMPARE(SkyCatalogText::withoutStatusPrefix(""), QString(""));

    // Only a leading prefix is a status prefix.
    QCOMPARE(SkyCatalogText::withoutStatusPrefix("Loaded | Catalog: tail"), QString("Loaded | Catalog: tail"));
}

void SkyCatalogInternalsTests::catalogImportStatusKeepsPhaseIndependentOfText()
{
    using skygate::ui::internal::SkyCatalogImportStatus;

    const SkyCatalogImportStatus renamedProcessing{
        .text = QStringLiteral("Renamed processing wording"),
        .phase = SkyCatalogImportStatus::Phase::Processing,
    };
    const SkyCatalogImportStatus originalProcessing{
        .text = QStringLiteral("Catalog: Processing... 4 objects parsed"),
        .phase = SkyCatalogImportStatus::Phase::Processing,
    };

    QCOMPARE(renamedProcessing.phase, originalProcessing.phase);
    QVERIFY(renamedProcessing.isProcessing());
    QVERIFY(originalProcessing.isProcessing());

    // Processing-looking text does not make a download-phase event busy.
    const SkyCatalogImportStatus downloadWithProcessingText{
        .text = QStringLiteral("Catalog: Processing... still downloading"),
        .phase = SkyCatalogImportStatus::Phase::Downloading,
    };
    QVERIFY(!downloadWithProcessingText.isProcessing());
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
    const std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs{
        {"hip_1", "hip_2"},
        {"hip_2", "hip_3"},
    };
    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> anchorGroups{
        {"Demo", {"hip_1", "hip_2"}},
    };

    QVERIFY(store.setDataset(lineRefs, anchorGroups, 42U));
    QCOMPARE(store.revision(), std::uint64_t{1});
    QCOMPARE(store.count(), 42U);
    QCOMPARE(store.lineRefs().size(), 2U);
    QCOMPARE(store.anchorGroups().size(), 1U);
    QCOMPARE(store.lineRefVector().front().first, std::string("hip_1"));
    QCOMPARE(store.lineRefVector().front().second, std::string("hip_2"));
    QCOMPARE(store.anchorGroupVector().front().first, std::string("Demo"));

    // Repeating the same content is not a data change.
    QVERIFY(!store.setDataset(lineRefs, anchorGroups, 42U));
    QCOMPARE(store.revision(), std::uint64_t{1});
}

void SkyCatalogInternalsTests::constellationStoreClearDropsRefsAndCounts()
{
    skygate::ui::internal::SkyCatalogConstellationStore store;
    QVERIFY(store.setDataset({{"custom_a", "custom_b"}}, {{"Custom", {"custom_a", "custom_b"}}}, 1U));

    QVERIFY(store.clear());

    QVERIFY(store.lineRefs().empty());
    QVERIFY(store.anchorGroups().empty());
    QCOMPARE(store.count(), 0U);
    QCOMPARE(store.revision(), std::uint64_t{2});

    // Clearing an already empty store is not a data change.
    QVERIFY(!store.clear());
    QCOMPARE(store.revision(), std::uint64_t{2});
}

void SkyCatalogInternalsTests::legacySlotNormalizesPresetIndexAndUrl()
{
    using skygate::ui::internal::SkyCatalogLegacySlot;
    using skygate::ui::internal::SkyCatalogPresets;

    SkyCatalogLegacySlot slot;
    QCOMPARE(slot.catalogPresetIndex(), 0);
    QCOMPARE(slot.deepSkyCatalogPresetIndex(), 0);
    QCOMPARE(slot.catalogUrlText(), SkyCatalogPresets::defaultCatalogUrlText());
    QCOMPARE(slot.deepSkyCatalogUrlText(), SkyCatalogPresets::defaultDeepSkyCatalogUrlText());

    slot.setCatalogPresetIndex(3);
    QCOMPARE(slot.catalogPresetIndex(), 1);
    slot.setCatalogPresetIndex(-4);
    QCOMPARE(slot.catalogPresetIndex(), 0);
    slot.setDeepSkyCatalogPresetIndex(7);
    QCOMPARE(slot.deepSkyCatalogPresetIndex(), SkyCatalogPresets::kCustomLegacyPresetIndex);

    slot.setCatalogUrlText(QStringLiteral("  https://example.test/stars.csv  "));
    QCOMPARE(slot.catalogUrlText(), QStringLiteral("https://example.test/stars.csv"));
    slot.setCatalogUrlText(QString());
    QCOMPARE(slot.catalogUrlText(), SkyCatalogPresets::defaultCatalogUrlText());
    slot.setDeepSkyCatalogUrlText(QStringLiteral(" file:///tmp/deep.csv "));
    QCOMPARE(slot.deepSkyCatalogUrlText(), QStringLiteral("file:///tmp/deep.csv"));
    slot.setDeepSkyCatalogUrlText(QStringLiteral("   "));
    QCOMPARE(slot.deepSkyCatalogUrlText(), SkyCatalogPresets::defaultDeepSkyCatalogUrlText());
}

QTEST_APPLESS_MAIN(SkyCatalogInternalsTests)

#include "SkyCatalogInternalsTests.moc"
