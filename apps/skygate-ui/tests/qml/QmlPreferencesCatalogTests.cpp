#include "QmlPreferencesTestSupport.hpp"
#include "SkyCatalogSourceCollectionModel.hpp"
#include "SkySettingsStore.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "time/CalendarTime.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view kEmptySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

skygate::ephemeris::EphemerisDateRange testValidityRange()
{
    const auto start = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 1900,
            .month = 1,
            .day = 1,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    const auto end = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2100,
            .month = 1,
            .day = 1,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    Q_ASSERT(start.has_value());
    Q_ASSERT(end.has_value());
    return skygate::ephemeris::EphemerisDateRange{
        .id = "test-range",
        .displayName = "Test range",
        .start = *start,
        .end = *end,
    };
}

skygate::ephemeris::EphemerisDataManifest::Asset
emptyKernelAsset(std::string id, std::string profileId, std::string version, std::string relativePath)
{
    skygate::ephemeris::EphemerisDataManifest::Asset asset;
    asset.id = std::move(id);
    asset.kind = skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel;
    asset.profileId = std::move(profileId);
    asset.version = std::move(version);
    asset.relativePath = std::move(relativePath);
    asset.checksum.algorithm = "sha256";
    asset.checksum.value = std::string{kEmptySha256};
    asset.compression.kind = skygate::ephemeris::EphemerisDataManifest::CompressionKind::None;
    asset.compression.uncompressedSizeBytes = 0U;
    asset.validityRange = testValidityRange();
    return asset;
}

skygate::ephemeris::EphemerisDataManifest::Asset emptyDataAsset(
    std::string id,
    const skygate::ephemeris::EphemerisDataManifest::AssetKind kind,
    std::string version,
    std::string relativePath
)
{
    skygate::ephemeris::EphemerisDataManifest::Asset asset =
        emptyKernelAsset(std::move(id), "support-data", std::move(version), std::move(relativePath));
    asset.kind = kind;
    return asset;
}

void writeInstalledEphemerisSettings(
    const QString& kernelPath,
    const QString& earthOrientationPath,
    const QString& leapSecondPath,
    const QString& deltaTPath
)
{
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/ephemerisData/installedKernelAssetId"), QStringLiteral("de441-kernel")
    );
    settings.setValue(
        QStringLiteral("skyContext/ephemerisData/installedKernelProfileId"), QStringLiteral("de441-long-range")
    );
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedKernelPath"), kernelPath);
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedKernelVersion"), QStringLiteral("DE441-test"));
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedEarthOrientationPath"), earthOrientationPath);
    settings.setValue(
        QStringLiteral("skyContext/ephemerisData/installedEarthOrientationVersion"), QStringLiteral("2026-05-07")
    );
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedLeapSecondTablePath"), leapSecondPath);
    settings.setValue(
        QStringLiteral("skyContext/ephemerisData/installedLeapSecondTableVersion"), QStringLiteral("2026a")
    );
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedDeltaTDataPath"), deltaTPath);
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedDeltaTDataVersion"), QStringLiteral("2026a"));
    settings.setValue(QStringLiteral("skyContext/ephemerisData/dataRevisionToken"), QStringLiteral("de441-test"));
    settings.setValue(QStringLiteral("skyContext/ephemerisData/lastUpdateResult"), QStringLiteral("Installed DE441"));
}

skygate::ephemeris::EphemerisDataManifest minimalUpdateManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = true,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "de441-long-range",
            .displayName = "DE441 long range",
            .bundled = false,
            .longRange = true,
            .assetIds = {"de441-kernel"},
        }
    );
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "support-data",
            .displayName = "Time and Earth data",
            .bundled = false,
            .longRange = false,
            .assetIds = {"support-earth-orientation", "support-leap-seconds", "support-delta-t"},
        }
    );
    manifest.assets.push_back(
        emptyKernelAsset("de440s-kernel", "de440s-short-range", "DE440s-test", "de440s-short-range/kernels/de440s.bsp")
    );
    manifest.assets.push_back(
        emptyKernelAsset("de441-kernel", "de441-long-range", "DE441-test", "de441/kernels/de441.bsp")
    );
    manifest.assets.push_back(emptyDataAsset(
        "support-earth-orientation",
        skygate::ephemeris::EphemerisDataManifest::AssetKind::EarthOrientationData,
        "2026-05-07",
        "time/eop.txt"
    ));
    manifest.assets.push_back(emptyDataAsset(
        "support-leap-seconds",
        skygate::ephemeris::EphemerisDataManifest::AssetKind::LeapSecondTable,
        "2026a",
        "time/leap-seconds.list"
    ));
    manifest.assets.push_back(emptyDataAsset(
        "support-delta-t",
        skygate::ephemeris::EphemerisDataManifest::AssetKind::DeltaTData,
        "2026a",
        "time/delta-t.data"
    ));
    return manifest;
}

skygate::ephemeris::EphemerisDataManifest supportDataUpdateManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest = minimalUpdateManifest();
    for (skygate::ephemeris::EphemerisDataManifest::Asset& asset : manifest.assets) {
        if (asset.id == "support-earth-orientation") {
            asset.version = "2026-06-01";
        } else if (asset.id == "support-leap-seconds" || asset.id == "support-delta-t") {
            asset.version = "2026b";
        }
    }
    return manifest;
}

bool writeStagedEphemerisAssets(const QString& root, const skygate::ephemeris::EphemerisDataManifest& manifest)
{
    for (const skygate::ephemeris::EphemerisDataManifest::Asset& asset : manifest.assets) {
        if (!writeFile(root + QLatin1Char('/') + QString::fromStdString(asset.relativePath), QByteArray{})) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<SkyContextController> makeControllerWithManifest(
    const skygate::ephemeris::EphemerisDataManifest& manifest,
    const QString& updateResourceRoot,
    const QString& writableCacheRoot
)
{
    auto starCatalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    if (starCatalog == nullptr) {
        return {};
    }
    auto ephemerisEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(*starCatalog);
    if (!ephemerisEngineResult.isSuccess() || ephemerisEngineResult.engine == nullptr) {
        return {};
    }
    auto ephemerisEngine = std::move(ephemerisEngineResult.engine);

    SkyContextController::InitializationOptions options;
    options.ephemerisFactoryInputs.dataManifest = &manifest;
    options.ephemerisFactoryInputs.updateResourceRoot = updateResourceRoot;
    options.ephemerisFactoryInputs.writableCacheRoot = writableCacheRoot;
    return std::make_unique<SkyContextController>(std::move(starCatalog), std::move(ephemerisEngine), options, nullptr);
}

skygate::ui::internal::SkyCatalogSourceCollectionModel* sourceCollectionModel(SkyContextController& controller)
{
    return qobject_cast<skygate::ui::internal::SkyCatalogSourceCollectionModel*>(
        controller.catalogSourceCollectionModel()
    );
}

QStringList sourceModelInstanceIds(SkyContextController& controller)
{
    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(controller);
    if (model == nullptr) {
        return {};
    }

    QStringList instanceIds;
    for (int row = 0; row < model->rowCount(); ++row) {
        instanceIds.push_back(
            model->data(model->index(row, 0), skygate::ui::internal::SkyCatalogSourceCollectionModel::InstanceIdRole)
                .toString()
        );
    }
    return instanceIds;
}

int sourceModelRowWithError(SkyContextController& controller)
{
    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(controller);
    if (model == nullptr) {
        return -1;
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        const bool hasError =
            model->data(model->index(row, 0), skygate::ui::internal::SkyCatalogSourceCollectionModel::HasErrorRole)
                .toBool();
        if (hasError) {
            return row;
        }
    }
    return -1;
}

void addCustomCatalogSource(
    QQuickItem* root,
    QQuickWindow* window,
    SkyContextController& controller,
    QObject* presetCombo,
    const int comboIndex,
    const QString& url
)
{
    presetCombo->setProperty("currentIndex", comboIndex);
    QCoreApplication::processEvents();

    QQuickItem* urlInput = firstQuickItemWithObjectName(root, QStringLiteral("catalogSourceUrlInput"));
    QVERIFY(urlInput != nullptr);
    QTRY_VERIFY(urlInput->isVisible());
    replaceText(window, urlInput, url);

    QObject* addButton = firstObjectWithObjectName(root, QStringLiteral("catalogAddSourceButton"));
    QVERIFY(addButton != nullptr);
    QTRY_VERIFY(addButton->property("enabled").toBool());
    QVERIFY(activateControl(addButton));
    QTRY_VERIFY(!controller.downloadingCatalog() && !controller.catalogProcessing());
}

}  // namespace

class QmlPreferencesCatalogTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void catalogSectionShowsPresetsAndAddsSources();
    void catalogSectionManagesSourcesOrderingAndRemoval();
    void catalogSectionReportsErrorsAndRetries();
    void catalogSectionRestoresCollectionAndPreservesIdentity();
    void ephemerisEngineControlsBindDraftAndVisibility();
    void ephemerisDataControlsShowFallbackAndUpdateMode();
    void ephemerisDataControlsShowInstalledStateAndClearCache();

private:
    QmlSettingsFixture m_settings;
};

void QmlPreferencesCatalogTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlPreferencesCatalogTests")));
}

void QmlPreferencesCatalogTests::init()
{
    m_settings.resetForCurrentTest();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"),
        m_settings.cachePath(QStringLiteral("catalog-collection-cache"))
    );
    QDir(m_settings.cachePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void QmlPreferencesCatalogTests::catalogSectionShowsPresetsAndAddsSources()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 620
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionSourcesTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    // The preset picker is data-driven: two preset descriptors plus two
    // generic custom rows with an explicit category. Adding another preset
    // therefore only requires a descriptor entry, not another QML branch.
    QObject* presetCombo = firstObjectWithObjectName(root, QStringLiteral("catalogSourcePresetCombo"));
    QVERIFY(presetCombo != nullptr);
    QCOMPARE(presetCombo->property("count").toInt(), 4);
    QCOMPARE(controller->catalogSourcePresetId(0), QStringLiteral("hyg_v42"));
    QCOMPARE(controller->catalogSourcePresetId(1), QStringLiteral("open_ngc"));
    QVERIFY(!controller->catalogSourcePresetIsCustom(0));
    QVERIFY(!controller->catalogSourcePresetIsCustom(1));
    QVERIFY(controller->catalogSourcePresetIsCustom(2));
    QVERIFY(controller->catalogSourcePresetIsCustom(3));
    QCOMPARE(controller->catalogSourcePresetCategory(0), QStringLiteral("Star"));
    QCOMPARE(controller->catalogSourcePresetCategory(1), QStringLiteral("Deep sky"));
    QCOMPARE(controller->catalogSourcePresetCategory(2), QStringLiteral("Star"));
    QCOMPARE(controller->catalogSourcePresetCategory(3), QStringLiteral("Deep sky"));

    // The default bundled source is present, enabled, and not removable.
    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(*controller);
    QVERIFY(model != nullptr);
    QTRY_COMPARE(model->rowCount(), 1);
    QCOMPARE(
        model->data(model->index(0, 0), skygate::ui::internal::SkyCatalogSourceCollectionModel::BundledRole).toBool(),
        true
    );

    const QString starPath = m_settings.cachePath(QStringLiteral("add-source-star.csv"));
    QVERIFY(writeFile(
        starPath,
        sampleHygCsvPayload(
            {.id = 1, .hip = 900101, .properName = "Added Star", .ra = "6.0", .dec = "-16.0", .mag = "1.0"}
        )
    ));
    const QString starUrl = QUrl::fromLocalFile(starPath).toString();
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 2, starUrl);

    QTRY_VERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Added Star")));
    QTRY_COMPARE(model->rowCount(), 2);
    QCOMPARE(sourceModelInstanceIds(*controller).size(), 2);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::catalogSectionManagesSourcesOrderingAndRemoval()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 720
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionOrderingTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    QObject* presetCombo = firstObjectWithObjectName(root, QStringLiteral("catalogSourcePresetCombo"));
    QVERIFY(presetCombo != nullptr);

    const QString starAPath = m_settings.cachePath(QStringLiteral("order-star-a.csv"));
    const QString starBPath = m_settings.cachePath(QStringLiteral("order-star-b.csv"));
    const QString deepSkyPath = m_settings.cachePath(QStringLiteral("order-dso.csv"));
    QVERIFY(writeFile(
        starAPath,
        sampleHygCsvPayload(
            {.id = 1, .hip = 900201, .properName = "Ordered Star A", .ra = "6.0", .dec = "-16.0", .mag = "1.0"}
        )
    ));
    QVERIFY(writeFile(
        starBPath,
        sampleHygCsvPayload(
            {.id = 2, .hip = 900202, .properName = "Ordered Star B", .ra = "6.1", .dec = "-16.1", .mag = "2.0"}
        )
    ));
    QVERIFY(writeFile(
        deepSkyPath,
        sampleOpenNgcCsvPayload(
            {.name = "NGC0999",
             .type = "G",
             .ra = "00:42:44.35",
             .dec = "+41:16:08.6",
             .messier = "",
             .ngc = "0999",
             .identifiers = "PGC 9999",
             .commonName = "Ordered Galaxy C"}
        )
    ));

    addCustomCatalogSource(
        root, exposed.window(), *controller, presetCombo, 2, QUrl::fromLocalFile(starAPath).toString()
    );
    addCustomCatalogSource(
        root, exposed.window(), *controller, presetCombo, 2, QUrl::fromLocalFile(starBPath).toString()
    );
    addCustomCatalogSource(
        root, exposed.window(), *controller, presetCombo, 3, QUrl::fromLocalFile(deepSkyPath).toString()
    );

    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(*controller);
    QVERIFY(model != nullptr);
    QTRY_COMPARE(model->rowCount(), 4);
    const QStringList addedIds = sourceModelInstanceIds(*controller);
    QCOMPARE(addedIds.size(), 4);
    const QString bundledId = addedIds[0];
    const QString starAId = addedIds[1];
    const QString starBId = addedIds[2];
    const QString deepSkyId = addedIds[3];
    QVERIFY(starAId != starBId);
    QVERIFY(starBId != deepSkyId);
    QVERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star A")));
    QVERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star B")));

    // Move the deep-sky source up: [bundled, A, C, B].
    QObject* upButton = firstObjectWithObjectName(root, QStringLiteral("catalogSourceUpButton_3"));
    QVERIFY(upButton != nullptr);
    QVERIFY(activateControl(upButton));
    QTRY_COMPARE(sourceModelInstanceIds(*controller), QStringList({bundledId, starAId, deepSkyId, starBId}));

    // Disabling A removes only its contribution.
    QObject* enableCheckBox = firstObjectWithObjectName(root, QStringLiteral("catalogSourceEnableCheckBox_1"));
    QVERIFY(enableCheckBox != nullptr);
    QVERIFY(activateControl(enableCheckBox));
    QTRY_VERIFY(!catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star A")));
    QVERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star B")));

    enableCheckBox = firstObjectWithObjectName(root, QStringLiteral("catalogSourceEnableCheckBox_1"));
    QVERIFY(enableCheckBox != nullptr);
    QVERIFY(activateControl(enableCheckBox));
    QTRY_VERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star A")));

    // Removing A leaves the other two sources intact.
    QObject* removeButton = firstObjectWithObjectName(root, QStringLiteral("catalogSourceRemoveButton_1"));
    QVERIFY(removeButton != nullptr);
    QVERIFY(activateControl(removeButton));
    QTRY_COMPARE(sourceModelInstanceIds(*controller), QStringList({bundledId, deepSkyId, starBId}));
    QTRY_VERIFY(!catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star A")));
    QVERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star B")));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::catalogSectionReportsErrorsAndRetries()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 620
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionErrorTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    QObject* presetCombo = firstObjectWithObjectName(root, QStringLiteral("catalogSourcePresetCombo"));
    QVERIFY(presetCombo != nullptr);

    const QString retryPath = m_settings.cachePath(QStringLiteral("retry-star.csv"));
    const QString retryUrl = QUrl::fromLocalFile(retryPath).toString();
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 2, retryUrl);

    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(*controller);
    QVERIFY(model != nullptr);
    QTRY_COMPARE(model->rowCount(), 2);
    const int errorRow = sourceModelRowWithError(*controller);
    QVERIFY(errorRow >= 0);

    QObject* retryButton =
        firstObjectWithObjectName(root, QStringLiteral("catalogSourceRetryButton_") + QString::number(errorRow));
    QVERIFY(retryButton != nullptr);
    QVERIFY(retryButton->property("visible").toBool());

    QVERIFY(writeFile(
        retryPath,
        sampleHygCsvPayload(
            {.id = 1, .hip = 900301, .properName = "Retried Star", .ra = "6.0", .dec = "-16.0", .mag = "1.0"}
        )
    ));
    QVERIFY(activateControl(retryButton));
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Retried Star")));
    QTRY_VERIFY(sourceModelRowWithError(*controller) < 0);

    QStringList unexpectedWarnings;
    for (const QString& message : warnings.messages()) {
        if (!message.startsWith(QStringLiteral("Catalog source failed"))) {
            unexpectedWarnings.push_back(message);
        }
    }
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
}

void QmlPreferencesCatalogTests::catalogSectionRestoresCollectionAndPreservesIdentity()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 620
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionRestoreTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    QObject* presetCombo = firstObjectWithObjectName(root, QStringLiteral("catalogSourcePresetCombo"));
    QVERIFY(presetCombo != nullptr);

    const QString starAPath = m_settings.cachePath(QStringLiteral("restore-star-a.csv"));
    const QString starBPath = m_settings.cachePath(QStringLiteral("restore-star-b.csv"));
    const QString deepSkyPath = m_settings.cachePath(QStringLiteral("restore-dso.csv"));
    QVERIFY(writeFile(
        starAPath,
        sampleHygCsvPayload(
            {.id = 1, .hip = 900401, .properName = "Restored Star A", .ra = "6.0", .dec = "-16.0", .mag = "1.0"}
        )
    ));
    QVERIFY(writeFile(
        starBPath,
        sampleHygCsvPayload(
            {.id = 2, .hip = 900402, .properName = "Restored Star B", .ra = "6.1", .dec = "-16.1", .mag = "2.0"}
        )
    ));
    QVERIFY(writeFile(
        deepSkyPath,
        sampleOpenNgcCsvPayload(
            {.name = "NGC0998",
             .type = "G",
             .ra = "00:42:44.35",
             .dec = "+41:16:08.6",
             .messier = "",
             .ngc = "0998",
             .identifiers = "PGC 9998",
             .commonName = "Restored Galaxy C"}
        )
    ));

    const QString starAUrl = QUrl::fromLocalFile(starAPath).toString();
    const QString starBUrl = QUrl::fromLocalFile(starBPath).toString();
    const QString deepSkyUrl = QUrl::fromLocalFile(deepSkyPath).toString();
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 2, starAUrl);
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 2, starBUrl);
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 3, deepSkyUrl);

    QTRY_COMPARE(sourceModelInstanceIds(*controller).size(), 4);
    const QStringList addedIds = sourceModelInstanceIds(*controller);
    const QString bundledId = addedIds[0];
    const QString starAId = addedIds[1];
    const QString starBId = addedIds[2];
    const QString deepSkyId = addedIds[3];
    QVERIFY(starAId != starBId);
    QVERIFY(starBId != deepSkyId);
    QVERIFY(sourceModelInstanceIds(*controller).contains(starAId));
    QVERIFY(sourceModelInstanceIds(*controller).contains(starBId));
    QVERIFY(sourceModelInstanceIds(*controller).contains(deepSkyId));

    QVERIFY(controller->saveSettings());
    auto restoredController = makeController();
    QVERIFY(restoredController != nullptr);
    QTRY_VERIFY(catalogContainsDisplayName(restoredController->catalogBodies(), QStringLiteral("Restored Star A")));
    QTRY_VERIFY(catalogContainsDisplayName(restoredController->catalogBodies(), QStringLiteral("Restored Star B")));

    skygate::ui::internal::SkyCatalogSourceCollectionModel* restoredModel = sourceCollectionModel(*restoredController);
    QVERIFY(restoredModel != nullptr);
    // The bundled source is restored with its position and participation next
    // to the three downloaded sources.
    QTRY_COMPARE(restoredModel->rowCount(), 4);
    const QStringList restoredIds = sourceModelInstanceIds(*restoredController);
    QCOMPARE(restoredIds[0], bundledId);
    QVERIFY(restoredIds.contains(starAId));
    QVERIFY(restoredIds.contains(starBId));
    QVERIFY(restoredIds.contains(deepSkyId));

    // A restored title gains a "(saved)" suffix, but the source identity is
    // unchanged, so QML bindings keyed on the model identity remain stable.
    const int restoredRow = restoredModel->indexOfInstanceId(starAId);
    QVERIFY(restoredRow >= 0);
    const QString restoredTitle =
        restoredModel
            ->data(
                restoredModel->index(restoredRow, 0), skygate::ui::internal::SkyCatalogSourceCollectionModel::TitleRole
            )
            .toString();
    QVERIFY(!restoredTitle.isEmpty());
    QVERIFY(restoredTitle.contains(QStringLiteral("saved")));
    QCOMPARE(
        restoredModel
            ->data(
                restoredModel->index(restoredRow, 0),
                skygate::ui::internal::SkyCatalogSourceCollectionModel::InstanceIdRole
            )
            .toString(),
        starAId
    );
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::ephemerisEngineControlsBindDraftAndVisibility()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 620
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesEngineSection {
                anchors.fill: parent
                skyContextController: skyContext
                preferencesDraft: draft
            }
        }
    )"),
        QStringLiteral("PreferencesEphemerisEngineSectionTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    QObject* engineCombo = firstObjectWithObjectName(root, QStringLiteral("ephemerisEngineSelectorCombo"));
    QVERIFY(engineCombo != nullptr);
    QCOMPARE(engineCombo->property("count").toInt(), 2);
    QCOMPARE(engineCombo->property("currentIndex").toInt(), 0);

    QObject* correctionCombo = firstObjectWithObjectName(root, QStringLiteral("ephemerisCorrectionPresetCombo"));
    QVERIFY(correctionCombo != nullptr);
    QVERIFY(!correctionCombo->property("visible").toBool());

    QObject* refractionCheckBox = firstObjectWithObjectName(root, QStringLiteral("ephemerisRefractionCheckBox"));
    QVERIFY(refractionCheckBox != nullptr);
    QVERIFY(!refractionCheckBox->property("visible").toBool());

    draft->setProperty("ephemerisEngineKindIndex", 1);
    draft->setProperty("ephemerisCorrectionPresetIndex", 3);
    draft->setProperty("ephemerisRefractionEnabled", true);
    QCoreApplication::processEvents();
    QTRY_COMPARE(engineCombo->property("currentIndex").toInt(), 1);
    QTRY_VERIFY(correctionCombo->property("visible").toBool());
    QCOMPARE(correctionCombo->property("count").toInt(), 4);
    QCOMPARE(correctionCombo->property("currentIndex").toInt(), 3);
    QVERIFY(refractionCheckBox->property("visible").toBool());
    QVERIFY(refractionCheckBox->property("checked").toBool());

    draft->setProperty("ephemerisCorrectionPresetIndex", 1);
    draft->setProperty("ephemerisRefractionEnabled", false);
    QCoreApplication::processEvents();
    QTRY_COMPARE(correctionCombo->property("currentIndex").toInt(), 1);
    QTRY_VERIFY(!refractionCheckBox->property("checked").toBool());

    auto* pressureInput = firstQuickItemWithObjectName(root, QStringLiteral("ephemerisPressureInput"));
    QVERIFY(pressureInput != nullptr);
    QVERIFY(!pressureInput->isVisible());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::ephemerisDataControlsShowFallbackAndUpdateMode()
{
    const skygate::ephemeris::EphemerisDataManifest manifest = minimalUpdateManifest();
    const QString updateResourceRoot = m_settings.cachePath(QStringLiteral("ephemeris-source"));
    QVERIFY(writeStagedEphemerisAssets(updateResourceRoot, manifest));
    auto controller = makeControllerWithManifest(
        manifest, updateResourceRoot, m_settings.cachePath(QStringLiteral("ephemeris-cache"))
    );
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 720
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesEngineSection {
                anchors.fill: parent
                skyContextController: skyContext
                preferencesDraft: draft
            }
        }
    )"),
        QStringLiteral("PreferencesEphemerisDataFallbackTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    QObject* modernStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisShortRangeKernelStatusLabel"));
    QObject* eopStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisEarthOrientationStatusLabel"));
    QObject* leapSecondStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLeapSecondStatusLabel"));
    QObject* deltaTStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisDeltaTStatusLabel"));
    QObject* kernelDownloadCombo = firstObjectWithObjectName(root, QStringLiteral("ephemerisKernelDownloadCombo"));
    QObject* supportDownloadCombo =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisSupportDataDownloadCombo"));
    QVERIFY(modernStatus != nullptr);
    QVERIFY(eopStatus != nullptr);
    QVERIFY(leapSecondStatus != nullptr);
    QVERIFY(deltaTStatus != nullptr);
    QVERIFY(kernelDownloadCombo != nullptr);
    QVERIFY(supportDownloadCombo != nullptr);

    QCOMPARE(modernStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(eopStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(leapSecondStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(deltaTStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(kernelDownloadCombo->property("displayText").toString(), QString("Select kernel..."));
    QCOMPARE(supportDownloadCombo->property("displayText").toString(), QString("Select data..."));
    QVERIFY(kernelDownloadCombo->property("enabled").toBool());
    QVERIFY(supportDownloadCombo->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(kernelDownloadCombo, "activated", Q_ARG(int, 2)));
    QTRY_COMPARE(modernStatus->property("text").toString(), QString("DE441"));
    QTRY_COMPARE(eopStatus->property("text").toString(), QString("Bundled"));
    QTRY_COMPARE(leapSecondStatus->property("text").toString(), QString("Bundled"));
    QTRY_COMPARE(deltaTStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(controller->ephemerisDataStatusText(), QString("Ephemeris data: Installed data active"));

    controller->setEphemerisDataOnlineUpdatesEnabled(false);
    QTRY_VERIFY(supportDownloadCombo->property("enabled").toBool());
    QTRY_VERIFY(kernelDownloadCombo->property("enabled").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::ephemerisDataControlsShowInstalledStateAndClearCache()
{
    const QString kernelPath = m_settings.cachePath(QStringLiteral("de441.bsp"));
    const QString earthOrientationPath = m_settings.cachePath(QStringLiteral("eop.csv"));
    const QString leapSecondPath = m_settings.cachePath(QStringLiteral("leap-seconds.list"));
    const QString deltaTPath = m_settings.cachePath(QStringLiteral("delta-t.csv"));
    QVERIFY(writeFile(kernelPath, QByteArray(2 * 1024 * 1024, 'k')));
    QVERIFY(writeFile(earthOrientationPath, QByteArray(1024 * 1024, 'e')));
    QVERIFY(writeFile(leapSecondPath, QByteArray(1024 * 1024, 'l')));
    QVERIFY(writeFile(deltaTPath, QByteArray(1024 * 1024, 'd')));
    writeInstalledEphemerisSettings(kernelPath, earthOrientationPath, leapSecondPath, deltaTPath);

    const skygate::ephemeris::EphemerisDataManifest updateManifest = supportDataUpdateManifest();
    auto controller = makeControllerWithManifest(
        updateManifest,
        m_settings.cachePath(QStringLiteral("ephemeris-source")),
        m_settings.cachePath(QStringLiteral("ephemeris-cache"))
    );
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 720
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesEngineSection {
                anchors.fill: parent
                skyContextController: skyContext
                preferencesDraft: draft
            }
        }
    )"),
        QStringLiteral("PreferencesEphemerisDataInstalledTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    QObject* modernStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisShortRangeKernelStatusLabel"));
    QObject* eopStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisEarthOrientationStatusLabel"));
    QObject* leapSecondStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLeapSecondStatusLabel"));
    QObject* deltaTStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisDeltaTStatusLabel"));
    QObject* kernelCacheSize =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisPlanetaryKernelCacheSizeLabel"));
    QObject* kernelClearButton =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisPlanetaryKernelClearCacheButton"));
    QObject* supportCacheSize = firstObjectWithObjectName(root, QStringLiteral("ephemerisSupportDataCacheSizeLabel"));
    QObject* supportDownloadCombo =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisSupportDataDownloadCombo"));
    QObject* supportClearButton =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisSupportDataClearCacheButton"));
    QVERIFY(modernStatus != nullptr);
    QVERIFY(eopStatus != nullptr);
    QVERIFY(leapSecondStatus != nullptr);
    QVERIFY(deltaTStatus != nullptr);
    QVERIFY(kernelCacheSize != nullptr);
    QVERIFY(kernelClearButton != nullptr);
    QVERIFY(supportCacheSize != nullptr);
    QVERIFY(supportDownloadCombo != nullptr);
    QVERIFY(supportClearButton != nullptr);

    QCOMPARE(modernStatus->property("text").toString(), QString("DE441"));
    QCOMPARE(kernelCacheSize->property("text").toString(), QString("2.0 MB"));
    QCOMPARE(supportCacheSize->property("text").toString(), QString("3.0 MB"));
    QCOMPARE(eopStatus->property("text").toString(), QString("2026-05-07"));
    QCOMPARE(leapSecondStatus->property("text").toString(), QString("2026a"));
    QCOMPARE(deltaTStatus->property("text").toString(), QString("2026a"));
    QVERIFY(supportDownloadCombo->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(supportDownloadCombo, "activated", Q_ARG(int, 2)));
    QTRY_COMPARE(eopStatus->property("text").toString(), QString("2026-05-07 (available: 2026-06-01)"));
    QTRY_COMPARE(leapSecondStatus->property("text").toString(), QString("2026a (available: 2026b)"));
    QTRY_COMPARE(deltaTStatus->property("text").toString(), QString("2026a (available: 2026b)"));

    QVERIFY(activateControl(kernelClearButton));
    QTRY_COMPARE(modernStatus->property("text").toString(), QString("Bundled"));
    QTRY_COMPARE(kernelCacheSize->property("text").toString(), QString("0 MB"));
    QCOMPARE(eopStatus->property("text").toString(), QString("2026-05-07 (available: 2026-06-01)"));
    QVERIFY(!QFileInfo::exists(kernelPath));
    QVERIFY(QFileInfo::exists(earthOrientationPath));

    QVERIFY(activateControl(supportClearButton));
    QTRY_COMPARE(modernStatus->property("text").toString(), QString("Bundled"));
    QTRY_COMPARE(supportCacheSize->property("text").toString(), QString("0 MB"));
    QCOMPARE(controller->ephemerisDataStatusText(), QString("Ephemeris data: Bundled fallback"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesCatalogTests)

#include "QmlPreferencesCatalogTests.moc"
