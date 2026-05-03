#include "SkySettingsStore.hpp"
#include "catalog/SkyCatalogCacheController.hpp"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

int severityRank(const QtMsgType type) noexcept
{
    switch (type) {
    case QtDebugMsg:
        return 0;
    case QtInfoMsg:
        return 1;
    case QtWarningMsg:
        return 2;
    case QtCriticalMsg:
        return 3;
    case QtFatalMsg:
        return 4;
    }

    return 1;
}

class LogCapture final {
public:
    explicit LogCapture(const QtMsgType minimumType = QtWarningMsg)
        : m_minimumType(minimumType)
    {
        s_current = this;
        m_previousHandler = qInstallMessageHandler(&LogCapture::handler);
    }

    ~LogCapture()
    {
        qInstallMessageHandler(m_previousHandler);
        s_current = nullptr;
    }

    [[nodiscard]] QString joinedMessages() const
    {
        return m_messages.join('\n');
    }

private:
    static void handler(
        const QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
    )
    {
        if (
            s_current != nullptr
            && severityRank(type) >= severityRank(s_current->m_minimumType)
        ) {
            s_current->m_messages.push_back(QStringLiteral("%1 %2").arg(
                QString::fromUtf8(context.category != nullptr ? context.category : "default"),
                message
            ));
        }
    }

private:
    QtMessageHandler m_previousHandler = nullptr;
    QtMsgType m_minimumType = QtWarningMsg;
    QStringList m_messages;
    static inline LogCapture* s_current = nullptr;
};

}  // namespace

class SkyCatalogCacheControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void restoresSavedCatalogsAndConstellationLabels();
    void unreadableSavedStarCatalogFallsBack();
    void staleConstellationLineSchemaRequestsReset();
    void persistRoundTripsConstellationRows();
    void logsCacheLifecycleSummariesAtInfoLevel();

private:
    SkySettingsStore::CatalogCacheSnapshot makeValidCacheSnapshot() const;

private:
    QTemporaryDir m_settingsDir;
};

void SkyCatalogCacheControllerTests::initTestCase()
{
    QVERIFY(m_settingsDir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QCoreApplication::setOrganizationName("SkygateTests");
    QCoreApplication::setApplicationName("SkyCatalogCacheControllerTests");
}

void SkyCatalogCacheControllerTests::init()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/catalogCachePath", m_settingsDir.filePath("catalog-cache.txt"));
    settings.setValue(
        "skyContext/deepSkyCatalogCachePath",
        m_settingsDir.filePath("deep-sky-catalog-cache.txt")
    );
}

SkySettingsStore::CatalogCacheSnapshot SkyCatalogCacheControllerTests::makeValidCacheSnapshot() const
{
    SkySettingsStore::CatalogCacheSnapshot snapshot;
    snapshot.sourceLabel = "Downloaded (saved) (saved)";
    snapshot.catalogPayload =
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Sirius,6.7525,-16.7161,-1.46\n";
    snapshot.deepSkySourceLabel = "OpenNGC (saved)";
    snapshot.deepSkyCatalogPayload =
        "Name;Type;RA;Dec;M;NGC;IC\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;\n";
    snapshot.constellationLineRows = "hip_1|hip_2\n";
    snapshot.constellationLabelRows = "Demo|hip_1,hip_2\n";
    snapshot.constellationLineSchemaVersion = 4;
    snapshot.constellationCount = 1;
    return snapshot;
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
    QCOMPARE(result.sourceLabel, QString("Downloaded (saved)"));
    QCOMPARE(result.deepSkySourceLabel, QString("OpenNGC (saved)"));
    QCOMPARE(result.constellationLineRefs.size(), 1U);
    QCOMPARE(result.constellationLineRefs[0].first, std::string("hip_1"));
    QCOMPARE(result.constellationLineRefs[0].second, std::string("hip_2"));
    QCOMPARE(result.constellationLabelRefs.size(), 1U);
    QCOMPARE(result.constellationLabelRefs[0].first, std::string("Demo"));
    QCOMPARE(result.constellationLabelRefs[0].second.size(), 2U);
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
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog payload format is not recognized."
    );
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
    request.catalogPayload =
        "id,hip,proper,ra,dec,mag\n"
        "1,11,Alpha,1.0,2.0,3.0\n";
    request.constellationLineRefs = {{"hip_a", "hip_b"}};
    request.constellationLabelRefs = {{"Label", {"hip_a", "hip_b", "hip_c"}}};
    request.constellationCount = 12;
    controller.persist(request);

    const auto result = controller.restore(1, 0);
    QVERIFY(result.restored);
    QCOMPARE(result.sourceLabel, QString("Custom (saved)"));
    QCOMPARE(result.constellationLineRefs.size(), 1U);
    QCOMPARE(result.constellationLabelRefs.size(), 1U);
    QCOMPARE(result.constellationLabelRefs[0].second.size(), 3U);
    QVERIFY(result.constellationCount.has_value());
    QCOMPARE(*result.constellationCount, 12U);
}

void SkyCatalogCacheControllerTests::logsCacheLifecycleSummariesAtInfoLevel()
{
    SkySettingsStore store;
    const skygate::ui::internal::SkyCatalogCacheController controller(&store);
    LogCapture capture(QtInfoMsg);

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
