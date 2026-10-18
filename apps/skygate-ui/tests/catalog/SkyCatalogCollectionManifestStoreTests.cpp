#include "catalog/SkyCatalogCollectionManifestStore.hpp"
#include "catalog/SkyCatalogSourceCachePaths.hpp"
#include "support/LogCapture.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStringList>
#include <QtTest/QtTest>

using skygate::ui::internal::SkyCatalogCollectionManifestStore;
using skygate::ui::internal::SkyCatalogSourceCachePaths;

class SkyCatalogCollectionManifestStoreTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void publishesAndReadsTheManifest();
    void reportsAbsentAndUnreadableManifests();
    void nextGenerationSkipsObservableNumbers();
    void recordsTheCommittedGeneration();
    void stageGenerationMarksRecordsStored();
    void detectsGenerationRecordsAndSidecars();
    void listsEveryCollectionCacheFile();
    void removeObsoleteKeepsTheCommittedGeneration();

private:
    [[nodiscard]] QString sidecarPath(const QString& instanceId, quint64 generation) const;

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
    QString m_directory;
};

void SkyCatalogCollectionManifestStoreTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogCollectionManifestStoreTests")));
}

void SkyCatalogCollectionManifestStoreTests::init()
{
    m_settings.resetForCurrentTest();
    m_directory = m_settings.currentTestFilePath(QStringLiteral("collection-cache"));
    QVERIFY(QDir(m_directory).removeRecursively() || !QDir(m_directory).exists());
    QVERIFY(QDir().mkpath(m_directory));
}

QString SkyCatalogCollectionManifestStoreTests::sidecarPath(const QString& instanceId, const quint64 generation) const
{
    return SkyCatalogSourceCachePaths::rawSidecarPath(m_directory, instanceId, generation);
}

void SkyCatalogCollectionManifestStoreTests::publishesAndReadsTheManifest()
{
    QCOMPARE(
        SkyCatalogCollectionManifestStore::path(m_directory),
        QDir(m_directory).filePath(QStringLiteral("catalog-collection.manifest"))
    );

    const SkyCatalogCollectionManifestStore::Manifest manifest{
        .schemaVersion = 3,
        .binarySchemaVersion = 9,
        .generation = 5,
    };
    QVERIFY(SkyCatalogCollectionManifestStore::publish(m_directory, manifest));
    QVERIFY(QFile::exists(SkyCatalogCollectionManifestStore::path(m_directory)));

    const SkyCatalogCollectionManifestStore::ReadResult read = SkyCatalogCollectionManifestStore::read(m_directory);
    QCOMPARE(read.state, SkyCatalogCollectionManifestStore::State::Readable);
    QCOMPARE(read.manifest.schemaVersion, 3);
    QCOMPARE(read.manifest.binarySchemaVersion, 9);
    QCOMPARE(read.manifest.generation, quint64{5});

    // Publishing replaces the previous generation instead of appending.
    QVERIFY(
        SkyCatalogCollectionManifestStore::publish(
            m_directory,
            SkyCatalogCollectionManifestStore::Manifest{
                .schemaVersion = 4,
                .binarySchemaVersion = 10,
                .generation = 6,
            }
        )
    );
    const SkyCatalogCollectionManifestStore::ReadResult replaced = SkyCatalogCollectionManifestStore::read(m_directory);
    QCOMPARE(replaced.state, SkyCatalogCollectionManifestStore::State::Readable);
    QCOMPARE(replaced.manifest.generation, quint64{6});
}

void SkyCatalogCollectionManifestStoreTests::reportsAbsentAndUnreadableManifests()
{
    QCOMPARE(
        SkyCatalogCollectionManifestStore::read(m_directory).state, SkyCatalogCollectionManifestStore::State::Absent
    );

    // A path occupied by something that is not a regular file cannot hold a
    // committed manifest.
    const QString manifestPath = SkyCatalogCollectionManifestStore::path(m_directory);
    QVERIFY(QDir().mkpath(manifestPath));
    skygate::ui::tests::LogCapture capture;
    QCOMPARE(
        SkyCatalogCollectionManifestStore::read(m_directory).state, SkyCatalogCollectionManifestStore::State::Absent
    );
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("not a regular file")));
    QVERIFY(QDir(manifestPath).removeRecursively());

    // A manifest missing one of its fields is committed configuration that
    // cannot be read, which is different from no manifest at all.
    const QByteArray truncatedContent = QByteArrayLiteral("schemaVersion=1\nbinarySchemaVersion=4\ngenera");
    QFile truncated(manifestPath);
    QVERIFY(truncated.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(truncated.write(truncatedContent), qint64(truncatedContent.size()));
    truncated.close();
    QCOMPARE(
        SkyCatalogCollectionManifestStore::read(m_directory).state, SkyCatalogCollectionManifestStore::State::Unreadable
    );
}

void SkyCatalogCollectionManifestStoreTests::nextGenerationSkipsObservableNumbers()
{
    QSettings settings;
    QCOMPARE(SkyCatalogCollectionManifestStore::nextGeneration(m_directory, settings, 0U), quint64{1});
    // The committed generation named by the manifest is skipped as well.
    QCOMPARE(SkyCatalogCollectionManifestStore::nextGeneration(m_directory, settings, 3U), quint64{4});

    // A save interrupted between its staging steps leaves a records group and a
    // sidecar the manifest does not name yet; their numbers must not be reused.
    settings.setValue(QStringLiteral("catalogSources/2/recordsStored"), true);
    settings.setValue(QStringLiteral("catalogSources/4/recordsStored"), true);
    QFile stagedSidecar(sidecarPath(QStringLiteral("staged:source"), 7U));
    QVERIFY(stagedSidecar.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(stagedSidecar.write("staged raw payload"), qint64(18));
    stagedSidecar.close();

    QCOMPARE(SkyCatalogCollectionManifestStore::nextGeneration(m_directory, settings, 3U), quint64{8});
}

void SkyCatalogCollectionManifestStoreTests::recordsTheCommittedGeneration()
{
    QSettings settings;
    QVERIFY(!SkyCatalogCollectionManifestStore::committedGeneration(settings).has_value());

    SkyCatalogCollectionManifestStore::recordCommittedGeneration(settings, 5U);
    QCOMPARE(SkyCatalogCollectionManifestStore::committedGeneration(settings).value(), quint64{5});
    QCOMPARE(
        settings.value(QStringLiteral("skyContext/catalogCollectionCommittedGeneration")).toULongLong(), quint64{5}
    );

    // Recording the same generation again changes nothing.
    SkyCatalogCollectionManifestStore::recordCommittedGeneration(settings, 5U);
    QCOMPARE(SkyCatalogCollectionManifestStore::committedGeneration(settings).value(), quint64{5});

    SkyCatalogCollectionManifestStore::forgetCommittedGeneration(settings);
    QVERIFY(!SkyCatalogCollectionManifestStore::committedGeneration(settings).has_value());

    // Zero is not a generation a manifest can commit.
    settings.setValue(QStringLiteral("skyContext/catalogCollectionCommittedGeneration"), 0);
    QVERIFY(!SkyCatalogCollectionManifestStore::committedGeneration(settings).has_value());
}

void SkyCatalogCollectionManifestStoreTests::stageGenerationMarksRecordsStored()
{
    QSettings settings;
    settings.setValue(QStringLiteral("catalogSources/6/stale/instanceId"), QStringLiteral("stale:source"));
    QVERIFY(!SkyCatalogCollectionManifestStore::hasRecordsStored(settings, 6U));

    SkyCatalogCollectionManifestStore::stageGeneration(settings, 6U);
    QVERIFY(SkyCatalogCollectionManifestStore::hasRecordsStored(settings, 6U));
    // Staging starts from an empty namespace so leftover data of an interrupted
    // save cannot mix into the records being written.
    QVERIFY(!settings.contains(QStringLiteral("catalogSources/6/stale/instanceId")));
    QVERIFY(!SkyCatalogCollectionManifestStore::hasRecordsStored(settings, 7U));

    SkyCatalogCollectionManifestStore::discardGeneration(settings, 6U);
    QVERIFY(!SkyCatalogCollectionManifestStore::hasRecordsStored(settings, 6U));
    QVERIFY(!settings.contains(QStringLiteral("catalogSources/6")));
}

void SkyCatalogCollectionManifestStoreTests::detectsGenerationRecordsAndSidecars()
{
    QSettings settings;
    QVERIFY(!SkyCatalogCollectionManifestStore::hasGenerationRecords(settings));
    QVERIFY(!SkyCatalogCollectionManifestStore::hasGenerationArtifacts(m_directory, settings));

    // Records in a numeric group mark the settings file as generation-format.
    settings.setValue(QStringLiteral("catalogSources/3/recordsStored"), true);
    QVERIFY(SkyCatalogCollectionManifestStore::hasGenerationRecords(settings));
    QVERIFY(SkyCatalogCollectionManifestStore::hasGenerationArtifacts(m_directory, settings));

    // A settings reset can remove the records while the sidecars survive; the
    // leftover files are still generation-format data the caller reports.
    settings.remove(QStringLiteral("catalogSources"));
    QVERIFY(!SkyCatalogCollectionManifestStore::hasGenerationArtifacts(m_directory, settings));
    QFile stagedSidecar(sidecarPath(QStringLiteral("staged:source"), 4U));
    QVERIFY(stagedSidecar.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(stagedSidecar.write("staged raw payload"), qint64(18));
    stagedSidecar.close();
    QVERIFY(!SkyCatalogCollectionManifestStore::hasGenerationRecords(settings));
    QVERIFY(SkyCatalogCollectionManifestStore::hasGenerationArtifacts(m_directory, settings));

    // A sidecar-named flat record is the pre-generation layout, not a
    // generation group.
    settings.setValue(QStringLiteral("catalogSources/catalog-source-flat/instanceId"), QStringLiteral("flat:source"));
    QVERIFY(!SkyCatalogCollectionManifestStore::hasGenerationRecords(settings));
}

void SkyCatalogCollectionManifestStoreTests::listsEveryCollectionCacheFile()
{
    QVERIFY(SkyCatalogCollectionManifestStore::cacheFilePaths(QString()).isEmpty());

    const QString firstSidecar = sidecarPath(QStringLiteral("custom:alpha"), 1U);
    const QString secondSidecar = sidecarPath(QStringLiteral("custom:beta"), 2U);
    for (const QString& path : QStringList{firstSidecar, secondSidecar}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("payload"), qint64(7));
    }
    QVERIFY(
        SkyCatalogCollectionManifestStore::publish(
            m_directory,
            SkyCatalogCollectionManifestStore::Manifest{
                .schemaVersion = 3,
                .binarySchemaVersion = 4,
                .generation = 2,
            }
        )
    );

    const QStringList cachePaths = SkyCatalogCollectionManifestStore::cacheFilePaths(m_directory);
    QCOMPARE(cachePaths.size(), 3);
    QVERIFY(cachePaths.contains(firstSidecar));
    QVERIFY(cachePaths.contains(secondSidecar));
    QVERIFY(cachePaths.contains(SkyCatalogCollectionManifestStore::path(m_directory)));
}

void SkyCatalogCollectionManifestStoreTests::removeObsoleteKeepsTheCommittedGeneration()
{
    const QString referencedSidecar = sidecarPath(QStringLiteral("kept:source"), 3U);
    const QString obsoleteSidecar = sidecarPath(QStringLiteral("obsolete:source"), 2U);
    const QString obsoleteBinarySidecar =
        SkyCatalogSourceCachePaths::binarySidecarPath(m_directory, QStringLiteral("kept:source"), 2U);
    for (const QString& path : QStringList{referencedSidecar, obsoleteSidecar, obsoleteBinarySidecar}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("payload"), qint64(7));
    }

    QSettings settings;
    const QString manifestPath = SkyCatalogCollectionManifestStore::path(m_directory);
    QVERIFY(
        SkyCatalogCollectionManifestStore::publish(
            m_directory,
            SkyCatalogCollectionManifestStore::Manifest{
                .schemaVersion = 3,
                .binarySchemaVersion = 4,
                .generation = 3,
            }
        )
    );
    settings.setValue(QStringLiteral("catalogSources/3/kept/instanceId"), QStringLiteral("kept:source"));
    settings.setValue(QStringLiteral("catalogSources/2/obsolete/instanceId"), QStringLiteral("obsolete:source"));
    settings.setValue(QStringLiteral("catalogSources/catalog-source-flat/instanceId"), QStringLiteral("flat:source"));
    settings.sync();

    SkyCatalogCollectionManifestStore::removeObsolete(settings, m_directory, 3U, QStringList{referencedSidecar});

    // The referenced sidecar and the committed generation's records stay; the
    // unreferenced files, the older generation's records, and the flat records
    // the committed generation replaced are removed. The manifest is a file of
    // its own and file cleanup leaves it alone.
    QVERIFY(QFile::exists(referencedSidecar));
    QVERIFY(!QFile::exists(obsoleteSidecar));
    QVERIFY(!QFile::exists(obsoleteBinarySidecar));
    QVERIFY(QFile::exists(manifestPath));
    QVERIFY(settings.contains(QStringLiteral("catalogSources/3/kept/instanceId")));
    QVERIFY(!settings.contains(QStringLiteral("catalogSources/2/obsolete/instanceId")));
    QVERIFY(!settings.contains(QStringLiteral("catalogSources/catalog-source-flat/instanceId")));
}

QTEST_GUILESS_MAIN(SkyCatalogCollectionManifestStoreTests)

#include "SkyCatalogCollectionManifestStoreTests.moc"
