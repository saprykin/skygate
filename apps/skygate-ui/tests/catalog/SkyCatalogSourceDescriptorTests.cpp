#include "SkyCatalogPresets.hpp"
#include "SkyCatalogSourceDescriptor.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourcePresetModel.hpp"

#include <QtTest/QtTest>

class SkyCatalogSourceDescriptorTests final : public QObject {
    Q_OBJECT

private slots:
    void starPresetsCarryStableIdsAndMetadata();
    void deepSkyPresetsCarryStableIdsAndMetadata();
    void legacyPresetIndexConversionPreserved();
    void presetAliasesResolveToStableIds();
    void renamingInstanceKeepsInstanceId();
    void identicalLabelsDoNotCollapseCustomInstances();
    void customInstanceCarriesHonestMetadata();
    void descriptorCarriesArchiveAndRelatedDatasetHints();
    void presetModelExposesTitlesUrlsAndCustomRow();
    void presetModelKeepsSameTitledSourcesDistinct();
};

void SkyCatalogSourceDescriptorTests::starPresetsCarryStableIdsAndMetadata()
{
    using skygate::ui::internal::SkyCatalogPresets;

    const auto bundled = SkyCatalogPresets::starSourceDescriptor(QStringLiteral("bundled"));
    QVERIFY(bundled.has_value());
    QCOMPARE(bundled->sourceId, QString("bundled"));
    QCOMPARE(bundled->title, QString("Bundled"));
    QVERIFY(bundled->bundled);
    QCOMPARE(bundled->legacyPresetIndex, 0);
    QVERIFY(bundled->urls.isEmpty());
    QCOMPARE(bundled->schemaHint, skygate::ephemeris::CatalogSourceType::Bundled);

    const auto hyg = SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v42"));
    QVERIFY(hyg.has_value());
    QCOMPARE(hyg->sourceId, QString("hyg_v42"));
    QCOMPARE(hyg->title, QString("HYG v4.2"));
    QCOMPARE(hyg->version, QString("v4.2"));
    QCOMPARE(hyg->legacyPresetIndex, 1);
    QVERIFY(!hyg->bundled);
    QCOMPARE(hyg->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(hyg->urls.size(), 1);
    QVERIFY(hyg->defaultUrl().startsWith(QStringLiteral("https://www.astronexus.com")));
    QCOMPARE(hyg->relatedDatasetUrls.size(), 3);
    QVERIFY(!hyg->attribution.isEmpty());
}

void SkyCatalogSourceDescriptorTests::deepSkyPresetsCarryStableIdsAndMetadata()
{
    using skygate::ui::internal::SkyCatalogPresets;

    const auto bundled = SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("bundled_messier"));
    QVERIFY(bundled.has_value());
    QCOMPARE(bundled->sourceId, QString("bundled_messier"));
    QCOMPARE(bundled->title, QString("Bundled Messier"));
    QVERIFY(bundled->bundled);
    QCOMPARE(bundled->legacyPresetIndex, 0);
    QVERIFY(bundled->urls.isEmpty());
    QCOMPARE(bundled->schemaHint, skygate::ephemeris::CatalogSourceType::Bundled);

    const auto openNgc = SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("open_ngc"));
    QVERIFY(openNgc.has_value());
    QCOMPARE(openNgc->sourceId, QString("open_ngc"));
    QCOMPARE(openNgc->title, QString("OpenNGC"));
    QCOMPARE(openNgc->version, QString("v20260307"));
    QCOMPARE(openNgc->legacyPresetIndex, 1);
    QVERIFY(!openNgc->bundled);
    QCOMPARE(openNgc->schemaHint, skygate::ephemeris::CatalogSourceType::OpenNgcCsv);
    QCOMPARE(openNgc->urls.size(), 2);
    QVERIFY(openNgc->defaultUrl().contains(QStringLiteral("OpenNGC")));
    QVERIFY(!openNgc->attribution.isEmpty());
}

void SkyCatalogSourceDescriptorTests::legacyPresetIndexConversionPreserved()
{
    using skygate::ui::internal::SkyCatalogPresets;

    QCOMPARE(SkyCatalogPresets::normalizeCatalogPresetIndex(-1), 0);
    QCOMPARE(SkyCatalogPresets::normalizeCatalogPresetIndex(0), 0);
    QCOMPARE(SkyCatalogPresets::normalizeCatalogPresetIndex(1), 1);
    QCOMPARE(SkyCatalogPresets::normalizeCatalogPresetIndex(2), 2);
    QCOMPARE(SkyCatalogPresets::normalizeCatalogPresetIndex(3), 1);
    QCOMPARE(SkyCatalogPresets::normalizeCatalogPresetIndex(99), 2);

    QCOMPARE(SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(-1), 0);
    QCOMPARE(SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(0), 0);
    QCOMPARE(SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(1), 1);
    QCOMPARE(SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(2), 2);
    QCOMPARE(SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(99), 2);
}

void SkyCatalogSourceDescriptorTests::presetAliasesResolveToStableIds()
{
    using skygate::ui::internal::SkyCatalogPresets;

    const auto legacyHyg = SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v3"));
    QVERIFY(legacyHyg.has_value());
    QCOMPARE(legacyHyg->sourceId, QString("hyg_v42"));

    const auto legacyBundledDeepSky = SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("bundled"));
    QVERIFY(legacyBundledDeepSky.has_value());
    QCOMPARE(legacyBundledDeepSky->sourceId, QString("bundled_messier"));

    QVERIFY(!SkyCatalogPresets::starSourceDescriptor(QStringLiteral("unknown")).has_value());
    QVERIFY(!SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("unknown")).has_value());
}

void SkyCatalogSourceDescriptorTests::renamingInstanceKeepsInstanceId()
{
    using skygate::ui::internal::SkyCatalogSourceDescriptor;
    using skygate::ui::internal::SkyCatalogSourceInstance;

    SkyCatalogSourceDescriptor descriptor;
    descriptor.sourceId = QStringLiteral("hyg_v42");
    descriptor.title = QStringLiteral("HYG v4.2");
    descriptor.version = QStringLiteral("v4.2");
    descriptor.urls = QStringList{QStringLiteral("https://example.test/hyg.csv.gz")};

    SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::fromDescriptor(descriptor);
    QCOMPARE(instance.instanceId, QString("preset:hyg_v42"));

    instance.title = QStringLiteral("Renamed HYG Source");
    QCOMPARE(instance.instanceId, QString("preset:hyg_v42"));
    QCOMPARE(instance.title, QString("Renamed HYG Source"));
}

void SkyCatalogSourceDescriptorTests::identicalLabelsDoNotCollapseCustomInstances()
{
    using skygate::ui::internal::SkyCatalogSourceInstance;

    const SkyCatalogSourceInstance first =
        SkyCatalogSourceInstance::createCustom(QStringLiteral("https://example.test/first.csv"));
    const SkyCatalogSourceInstance second =
        SkyCatalogSourceInstance::createCustom(QStringLiteral("https://example.test/second.csv"));

    QCOMPARE(first.title, QString("Downloaded"));
    QCOMPARE(second.title, QString("Downloaded"));
    QVERIFY(first.instanceId != second.instanceId);
    QVERIFY(first.isCustom());
    QVERIFY(second.isCustom());
}

void SkyCatalogSourceDescriptorTests::customInstanceCarriesHonestMetadata()
{
    using skygate::ui::internal::SkyCatalogSourceInstance;

    const SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::createCustom(
        QStringLiteral("https://example.test/custom.csv.gz"), QStringLiteral("v9")
    );

    QVERIFY(instance.instanceId.startsWith(QStringLiteral("custom:")));
    QVERIFY(instance.descriptorId.isEmpty());
    QCOMPARE(instance.version, QString("v9"));
    QCOMPARE(instance.urls, QStringList{QStringLiteral("https://example.test/custom.csv.gz")});
    QCOMPARE(instance.schemaHint, skygate::ephemeris::CatalogSourceType::Unknown);
}

void SkyCatalogSourceDescriptorTests::descriptorCarriesArchiveAndRelatedDatasetHints()
{
    using skygate::ui::internal::SkyCatalogSourceDescriptor;

    SkyCatalogSourceDescriptor descriptor;
    descriptor.sourceId = QStringLiteral("demo_zip");
    descriptor.title = QStringLiteral("Demo ZIP");
    descriptor.urls = QStringList{QStringLiteral("https://example.test/demo.zip")};
    descriptor.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    descriptor.archiveSelector = QStringLiteral("catalog/hyg.csv");
    descriptor.relatedDatasetUrls = QStringList{
        QStringLiteral("https://example.test/lines.json"),
    };
    descriptor.attribution = QStringLiteral("Demo data");

    QCOMPARE(descriptor.sourceId, QString("demo_zip"));
    QCOMPARE(descriptor.archiveSelector, QString("catalog/hyg.csv"));
    QCOMPARE(descriptor.relatedDatasetUrls.size(), 1);
    QCOMPARE(descriptor.relatedDatasetUrls.first(), QString("https://example.test/lines.json"));
    QCOMPARE(descriptor.attribution, QString("Demo data"));
    QVERIFY(descriptor.isKnown());
}

void SkyCatalogSourceDescriptorTests::presetModelExposesTitlesUrlsAndCustomRow()
{
    using skygate::ui::internal::SkyCatalogPresets;
    using skygate::ui::internal::SkyCatalogSourcePresetModel;

    SkyCatalogSourcePresetModel model(SkyCatalogPresets::starSourceDescriptors(), true);
    QCOMPARE(model.rowCount(), 3);

    QCOMPARE(model.titleAt(0), QString("Bundled"));
    QCOMPARE(model.titleAt(1), QString("HYG v4.2"));
    QCOMPARE(model.titleAt(2), QString("Custom URL"));

    QCOMPARE(model.sourceIdAt(0), QString("bundled"));
    QCOMPARE(model.sourceIdAt(1), QString("hyg_v42"));
    QCOMPARE(model.sourceIdAt(2), QString());

    QVERIFY(model.defaultUrlAt(0).isEmpty());
    QVERIFY(model.defaultUrlAt(1).startsWith(QStringLiteral("https://www.astronexus.com")));
    QVERIFY(model.defaultUrlAt(2).isEmpty());

    QCOMPARE(model.legacyPresetIndexAt(0), 0);
    QCOMPARE(model.legacyPresetIndexAt(1), 1);
    QCOMPARE(model.legacyPresetIndexAt(2), 2);

    QVERIFY(!model.isCustomAt(0));
    QVERIFY(!model.isCustomAt(1));
    QVERIFY(model.isCustomAt(2));

    QCOMPARE(model.data(model.index(1, 0), SkyCatalogSourcePresetModel::TitleRole).toString(), QString("HYG v4.2"));
    QCOMPARE(
        model.data(model.index(1, 0), SkyCatalogSourcePresetModel::DefaultUrlRole).toString(), model.defaultUrlAt(1)
    );
    QVERIFY(model.roleNames().values().contains("title"));
    QVERIFY(model.roleNames().values().contains("sourceId"));
    QVERIFY(model.roleNames().values().contains("defaultUrl"));
}

void SkyCatalogSourceDescriptorTests::presetModelKeepsSameTitledSourcesDistinct()
{
    using skygate::ui::internal::SkyCatalogSourceDescriptor;
    using skygate::ui::internal::SkyCatalogSourcePresetModel;

    SkyCatalogSourceDescriptor first;
    first.sourceId = QStringLiteral("first");
    first.title = QStringLiteral("Same Title");
    first.urls = QStringList{QStringLiteral("https://example.test/first.csv")};

    SkyCatalogSourceDescriptor second;
    second.sourceId = QStringLiteral("second");
    second.title = QStringLiteral("Same Title");
    second.urls = QStringList{QStringLiteral("https://example.test/second.csv")};

    SkyCatalogSourcePresetModel model({first, second}, false);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.titleAt(0), QString("Same Title"));
    QCOMPARE(model.titleAt(1), QString("Same Title"));
    QCOMPARE(model.sourceIdAt(0), QString("first"));
    QCOMPARE(model.sourceIdAt(1), QString("second"));
    QVERIFY(model.defaultUrlAt(0) != model.defaultUrlAt(1));
}

QTEST_GUILESS_MAIN(SkyCatalogSourceDescriptorTests)

#include "SkyCatalogSourceDescriptorTests.moc"
