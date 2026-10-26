#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "catalog/SkyCatalogSourceCachePaths.hpp"
#include "catalog/SkyCatalogSourceCacheRecord.hpp"
#include "catalog/SkyCatalogSourceRecordCodec.hpp"
#include <skygate/testsupport/LogCapture.hpp>
#include "support/SettingsTestFixture.hpp"

#include <QByteArray>
#include <QFile>
#include <QSettings>
#include <QStringList>
#include <QtTest/QtTest>

#include <cstddef>
#include <optional>

using skygate::ui::internal::SkyCatalogSourceCachePaths;
using skygate::ui::internal::SkyCatalogSourceRecordCodec;

namespace {

SkyCatalogSourceCacheRecord configuredRecord()
{
    SkyCatalogSourceCacheRecord record;
    record.instanceId = QStringLiteral("custom:alpha");
    record.descriptorId = QStringLiteral("alpha");
    record.title = QStringLiteral("Alpha");
    record.version = QStringLiteral("v1");
    record.urls =
        QStringList{QStringLiteral("https://example.test/alpha.csv"), QStringLiteral("https://example.test/a2.csv")};
    record.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/lines.json")};
    record.archiveSelector = QStringLiteral("alpha/*.csv");
    record.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    record.attribution = QStringLiteral("Example project");
    record.policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly;
    record.enabled = false;
    record.bundled = true;
    record.order = 3;
    record.constellationLineRows = QByteArray("hip_1|hip_2\n");
    record.constellationAnchorGroupRows = QByteArray("Demo|hip_1,hip_2\n");
    record.constellationLineSchemaVersion = 4;
    record.constellationCount = 2;
    return record;
}

bool writePayloadFile(const QString& path, const QByteArray& payload)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(payload) == payload.size();
}

void compareConfigurationFields(const SkyCatalogSourceCacheRecord& loaded, const SkyCatalogSourceCacheRecord& expected)
{
    QCOMPARE(loaded.instanceId, expected.instanceId);
    QCOMPARE(loaded.descriptorId, expected.descriptorId);
    QCOMPARE(loaded.title, expected.title);
    QCOMPARE(loaded.version, expected.version);
    QCOMPARE(loaded.urls, expected.urls);
    QCOMPARE(loaded.relatedDatasetUrls, expected.relatedDatasetUrls);
    QCOMPARE(loaded.archiveSelector, expected.archiveSelector);
    QCOMPARE(loaded.schemaHint, expected.schemaHint);
    QCOMPARE(loaded.attribution, expected.attribution);
    QCOMPARE(loaded.policy, expected.policy);
    QCOMPARE(loaded.enabled, expected.enabled);
    QCOMPARE(loaded.bundled, expected.bundled);
    QCOMPARE(loaded.order, expected.order);
}

}  // namespace

class SkyCatalogSourceRecordCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void namesRecordGroupsUnderTheSourcesRoot();
    void roundTripsEveryRecordField();
    void loadsDefinedDefaultsForMissingFields();
    void unknownPolicyFallsBackToMerge();
    void unknownSchemaHintFallsBackToNoHint();
    void missingPayloadSidecarLoadsWithoutPayload();
    void emptyPayloadKeepsNoSidecarReference();
    void hasFlatRecordsDistinguishesLayouts();
    void evictPayloadKeepsConfigurationFields();

private:
    [[nodiscard]] QString recordGroup() const;

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogSourceRecordCodecTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogSourceRecordCodecTests")));
}

void SkyCatalogSourceRecordCodecTests::init()
{
    m_settings.resetForCurrentTest();
}

QString SkyCatalogSourceRecordCodecTests::recordGroup() const
{
    return SkyCatalogSourceRecordCodec::sourceGroup(QStringLiteral("catalogSources/4"), QStringLiteral("custom:alpha"));
}

void SkyCatalogSourceRecordCodecTests::namesRecordGroupsUnderTheSourcesRoot()
{
    const QString instanceId = QStringLiteral("custom:alpha");
    const QString sourceGroup = QStringLiteral("catalogSources/") + SkyCatalogSourceCachePaths::fileStem(instanceId);

    QCOMPARE(SkyCatalogSourceRecordCodec::sourcesGroup(), QString("catalogSources"));
    QCOMPARE(SkyCatalogSourceRecordCodec::sourceGroup(QStringLiteral("catalogSources"), instanceId), sourceGroup);
    // A committed generation nests the same record group one level deeper.
    QCOMPARE(
        SkyCatalogSourceRecordCodec::sourceGroup(QStringLiteral("catalogSources/4"), instanceId),
        QStringLiteral("catalogSources/4/") + SkyCatalogSourceCachePaths::fileStem(instanceId)
    );
}

void SkyCatalogSourceRecordCodecTests::roundTripsEveryRecordField()
{
    const QString rawPath = m_settings.currentTestFilePath(QStringLiteral("raw-sidecar.txt"));
    const QString binaryPath = m_settings.currentTestFilePath(QStringLiteral("binary-sidecar.bin"));
    QVERIFY(writePayloadFile(rawPath, QByteArray("raw payload bytes")));
    QVERIFY(writePayloadFile(binaryPath, QByteArray("binary payload bytes")));

    SkyCatalogSourceCacheRecord record = configuredRecord();
    record.payload = QByteArray("raw payload bytes");
    record.binaryPayload = QByteArray("binary payload bytes");

    QSettings settings;
    SkyCatalogSourceRecordCodec::saveRecord(settings, recordGroup(), record, rawPath, binaryPath);
    settings.sync();
    QCOMPARE(settings.status(), QSettings::NoError);

    // The payload stays in the sidecar file; the record only keeps the
    // reference, which is what keeps the settings file small.
    QCOMPARE(settings.value(recordGroup() + QStringLiteral("/payloadPath")).toString(), rawPath);
    QCOMPARE(settings.value(recordGroup() + QStringLiteral("/binaryPayloadPath")).toString(), binaryPath);
    QCOMPARE(settings.value(recordGroup() + QStringLiteral("/payload")).toString(), QString());

    const SkyCatalogSourceCacheRecord loaded = SkyCatalogSourceRecordCodec::loadRecord(settings, recordGroup());

    compareConfigurationFields(loaded, record);
    QCOMPARE(loaded.payload, record.payload);
    QCOMPARE(loaded.binaryPayload, record.binaryPayload);
    QCOMPARE(loaded.constellationLineRows, record.constellationLineRows);
    QCOMPARE(loaded.constellationAnchorGroupRows, record.constellationAnchorGroupRows);
    QCOMPARE(loaded.constellationLineSchemaVersion, record.constellationLineSchemaVersion);
    QCOMPARE(loaded.constellationCount, record.constellationCount);
}

void SkyCatalogSourceRecordCodecTests::loadsDefinedDefaultsForMissingFields()
{
    QSettings settings;
    settings.beginGroup(recordGroup());
    settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:alpha"));
    settings.endGroup();

    // A record written before the parse contract and the payload references
    // existed loads as a configured, enabled, unparsed source.
    const SkyCatalogSourceCacheRecord loaded = SkyCatalogSourceRecordCodec::loadRecord(settings, recordGroup());

    QCOMPARE(loaded.instanceId, QString("custom:alpha"));
    QCOMPARE(loaded.schemaHint, skygate::ephemeris::CatalogSourceType::Unknown);
    QCOMPARE(loaded.policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(loaded.enabled);
    QVERIFY(!loaded.bundled);
    QCOMPARE(loaded.order, 0);
    QVERIFY(loaded.payload.isEmpty());
    QVERIFY(loaded.binaryPayload.isEmpty());
    QVERIFY(loaded.constellationLineRows.isEmpty());
    QCOMPARE(loaded.constellationLineSchemaVersion, 0);
    QCOMPARE(loaded.constellationCount, std::size_t{0});
}

void SkyCatalogSourceRecordCodecTests::unknownPolicyFallsBackToMerge()
{
    QSettings settings;
    settings.beginGroup(recordGroup());
    settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:alpha"));
    settings.setValue(QStringLiteral("policy"), 200);
    settings.endGroup();

    skygate::testsupport::LogCapture capture;
    const SkyCatalogSourceCacheRecord loaded = SkyCatalogSourceRecordCodec::loadRecord(settings, recordGroup());

    // A policy this build does not know must not reach composition, which
    // could not classify it.
    QCOMPARE(loaded.policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Invalid catalog composition policy")));
}

void SkyCatalogSourceRecordCodecTests::unknownSchemaHintFallsBackToNoHint()
{
    QSettings settings;
    settings.beginGroup(recordGroup());
    settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:alpha"));
    settings.setValue(QStringLiteral("schemaHint"), 200);
    settings.endGroup();

    skygate::testsupport::LogCapture capture;
    const SkyCatalogSourceCacheRecord loaded = SkyCatalogSourceRecordCodec::loadRecord(settings, recordGroup());

    // Detection decides again instead of the source failing on a hint that no
    // schema registry entry backs.
    QCOMPARE(loaded.schemaHint, skygate::ephemeris::CatalogSourceType::Unknown);
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Invalid catalog source schema hint")));
}

void SkyCatalogSourceRecordCodecTests::missingPayloadSidecarLoadsWithoutPayload()
{
    QSettings settings;
    settings.beginGroup(recordGroup());
    settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:alpha"));
    settings.setValue(
        QStringLiteral("payloadPath"), m_settings.currentTestFilePath(QStringLiteral("missing-sidecar.txt"))
    );
    settings.endGroup();

    skygate::testsupport::LogCapture capture;
    const SkyCatalogSourceCacheRecord loaded = SkyCatalogSourceRecordCodec::loadRecord(settings, recordGroup());

    // A referenced payload that is gone is corrupt committed data: the source
    // still loads as configured, keeps its identity, and reports the loss.
    QCOMPARE(loaded.instanceId, QString("custom:alpha"));
    QVERIFY(loaded.payload.isEmpty());
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Referenced catalog source payload is missing")));
}

void SkyCatalogSourceRecordCodecTests::emptyPayloadKeepsNoSidecarReference()
{
    SkyCatalogSourceCacheRecord record = configuredRecord();
    QVERIFY(record.payload.isEmpty());

    QSettings settings;
    SkyCatalogSourceRecordCodec::saveRecord(
        settings, recordGroup(), record, m_settings.currentTestFilePath(QStringLiteral("raw.txt")), QString()
    );
    settings.sync();

    // An evicted source is configuration only; storing a path without payload
    // would report the missing file on every later load.
    QCOMPARE(settings.value(recordGroup() + QStringLiteral("/payloadPath")).toString(), QString());
    QCOMPARE(settings.value(recordGroup() + QStringLiteral("/binaryPayloadPath")).toString(), QString());

    const SkyCatalogSourceCacheRecord loaded = SkyCatalogSourceRecordCodec::loadRecord(settings, recordGroup());
    QVERIFY(loaded.payload.isEmpty());
    QVERIFY(loaded.binaryPayload.isEmpty());
}

void SkyCatalogSourceRecordCodecTests::hasFlatRecordsDistinguishesLayouts()
{
    QSettings settings;
    QVERIFY(!SkyCatalogSourceRecordCodec::hasFlatRecords(settings));
    QVERIFY(SkyCatalogSourceRecordCodec::recordGroups(settings).isEmpty());

    // Committed generations nest their records in a numeric group.
    settings.setValue(QStringLiteral("catalogSources/4/recordsStored"), true);
    QVERIFY(!SkyCatalogSourceRecordCodec::hasFlatRecords(settings));
    QVERIFY(SkyCatalogSourceRecordCodec::isGenerationGroup(QStringLiteral("4")));
    const std::optional<quint64> generation = SkyCatalogSourceRecordCodec::generationOfGroup(QStringLiteral("4"));
    QVERIFY(generation.has_value());
    QCOMPARE(generation.value(), quint64{4});
    QCOMPARE(SkyCatalogSourceRecordCodec::recordGroups(settings), QStringList{QStringLiteral("4")});

    // A sidecar-named group is a record written before generations were
    // published.
    settings.setValue(QStringLiteral("catalogSources/catalog-source-flat/instanceId"), QStringLiteral("flat:source"));
    QVERIFY(SkyCatalogSourceRecordCodec::hasFlatRecords(settings));
    QVERIFY(!SkyCatalogSourceRecordCodec::isGenerationGroup(QStringLiteral("catalog-source-flat")));
    QVERIFY(!SkyCatalogSourceRecordCodec::generationOfGroup(QStringLiteral("catalog-source-flat")).has_value());
    QCOMPARE(
        SkyCatalogSourceRecordCodec::recordGroups(settings),
        QStringList({QStringLiteral("4"), QStringLiteral("catalog-source-flat")})
    );
}

void SkyCatalogSourceRecordCodecTests::evictPayloadKeepsConfigurationFields()
{
    const QString rawPath = m_settings.currentTestFilePath(QStringLiteral("raw-sidecar.txt"));
    const QString binaryPath = m_settings.currentTestFilePath(QStringLiteral("binary-sidecar.bin"));
    QVERIFY(writePayloadFile(rawPath, QByteArray("raw payload bytes")));
    QVERIFY(writePayloadFile(binaryPath, QByteArray("binary payload bytes")));

    SkyCatalogSourceCacheRecord record = configuredRecord();
    record.payload = QByteArray("raw payload bytes");
    record.binaryPayload = QByteArray("binary payload bytes");

    QSettings settings;
    SkyCatalogSourceRecordCodec::saveRecord(settings, recordGroup(), record, rawPath, binaryPath);
    SkyCatalogSourceRecordCodec::evictPayload(settings, recordGroup());
    settings.sync();

    for (const QString& key : QStringList{
             QStringLiteral("payloadPath"),
             QStringLiteral("binaryPayloadPath"),
             QStringLiteral("constellationLineRows"),
             QStringLiteral("constellationAnchorGroupRows"),
             QStringLiteral("constellationLineSchemaVersion"),
             QStringLiteral("constellationCount"),
         }) {
        QVERIFY2(!settings.contains(recordGroup() + QLatin1Char('/') + key), qPrintable(key));
    }

    const SkyCatalogSourceCacheRecord loaded = SkyCatalogSourceRecordCodec::loadRecord(settings, recordGroup());
    compareConfigurationFields(loaded, record);
    QVERIFY(loaded.payload.isEmpty());
    QVERIFY(loaded.binaryPayload.isEmpty());
    QVERIFY(loaded.constellationLineRows.isEmpty());
    QVERIFY(loaded.constellationAnchorGroupRows.isEmpty());
    QCOMPARE(loaded.constellationLineSchemaVersion, 0);
    QCOMPARE(loaded.constellationCount, std::size_t{0});
}

QTEST_GUILESS_MAIN(SkyCatalogSourceRecordCodecTests)

#include "SkyCatalogSourceRecordCodecTests.moc"
