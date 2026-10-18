#include "settings/SkyCacheFiles.hpp"
#include "support/LogCapture.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtTest/QtTest>

using skygate::ui::internal::SkyCacheFiles;

class SkyCacheFilesTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void appDataFilePathJoinsAppDataLocation();
    void appendsUniqueNonEmptyPaths();
    void writeFileAtomicallyReplacesExistingContents();
    void reportsMissingAndUnreadableFiles();
    void removesExistingFilesAndReportsFailures();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCacheFilesTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCacheFilesTests")));
}

void SkyCacheFilesTests::appDataFilePathJoinsAppDataLocation()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(!appDataPath.isEmpty());
    QCOMPARE(
        SkyCacheFiles::appDataFilePath(QStringLiteral("catalog-cache-v2.txt")),
        QDir(appDataPath).filePath(QStringLiteral("catalog-cache-v2.txt"))
    );
}

void SkyCacheFilesTests::appendsUniqueNonEmptyPaths()
{
    QStringList paths;
    SkyCacheFiles::appendUniquePath(paths, QStringLiteral("first.txt"));
    SkyCacheFiles::appendUniquePath(paths, QString());
    SkyCacheFiles::appendUniquePath(paths, QStringLiteral("first.txt"));
    SkyCacheFiles::appendUniquePath(paths, QStringLiteral("second.txt"));

    QCOMPARE(paths, QStringList({QStringLiteral("first.txt"), QStringLiteral("second.txt")}));
}

void SkyCacheFilesTests::writeFileAtomicallyReplacesExistingContents()
{
    const QString path = m_settings.currentTestFilePath(QStringLiteral("nested/cache.bin"));
    const QByteArray longPayload(4096, 'x');

    QVERIFY(SkyCacheFiles::writeFileAtomically(path, longPayload, QStringLiteral("test cache")));
    const SkyCacheFiles::ReadResult written = SkyCacheFiles::readFile(path);
    QCOMPARE(written.state, SkyCacheFiles::ReadState::Ok);
    QCOMPARE(written.payload, longPayload);

    const QByteArray shortPayload("replacement");
    QVERIFY(SkyCacheFiles::writeFileAtomically(path, shortPayload, QStringLiteral("test cache")));
    const SkyCacheFiles::ReadResult replaced = SkyCacheFiles::readFile(path);
    QCOMPARE(replaced.state, SkyCacheFiles::ReadState::Ok);
    QCOMPARE(replaced.payload, shortPayload);
}

void SkyCacheFilesTests::reportsMissingAndUnreadableFiles()
{
    const SkyCacheFiles::ReadResult missing =
        SkyCacheFiles::readFile(m_settings.currentTestFilePath(QStringLiteral("missing-cache.bin")));
    QCOMPARE(missing.state, SkyCacheFiles::ReadState::Absent);
    QVERIFY(missing.payload.isEmpty());
    QVERIFY(missing.errorString.isEmpty());
    QCOMPARE(SkyCacheFiles::readFile(QString()).state, SkyCacheFiles::ReadState::Absent);

    const QString directoryPath = m_settings.currentTestFilePath(QStringLiteral("unreadable-cache.bin"));
    QVERIFY(QDir().mkpath(directoryPath));
    const SkyCacheFiles::ReadResult unreadable = SkyCacheFiles::readFile(directoryPath);
    QCOMPARE(unreadable.state, SkyCacheFiles::ReadState::Unreadable);
    QVERIFY(unreadable.payload.isEmpty());
    QVERIFY(!unreadable.errorString.isEmpty());
}

void SkyCacheFilesTests::removesExistingFilesAndReportsFailures()
{
    const QString firstPath = m_settings.currentTestFilePath(QStringLiteral("first-cache.bin"));
    const QString secondPath = m_settings.currentTestFilePath(QStringLiteral("second-cache.bin"));
    QVERIFY(SkyCacheFiles::writeFileAtomically(firstPath, QByteArray("first"), QStringLiteral("test cache")));
    QVERIFY(SkyCacheFiles::writeFileAtomically(secondPath, QByteArray("second"), QStringLiteral("test cache")));

    QVERIFY(
        SkyCacheFiles::removeFiles(
            {firstPath, secondPath, m_settings.currentTestFilePath(QStringLiteral("absent-cache.bin"))},
            QStringLiteral("test cache")
        )
    );
    QVERIFY(!QFile::exists(firstPath));
    QVERIFY(!QFile::exists(secondPath));

    const QString directoryPath = m_settings.currentTestFilePath(QStringLiteral("remove-directory"));
    QVERIFY(QDir().mkpath(directoryPath));
    skygate::ui::tests::LogCapture capture(QtWarningMsg);
    QVERIFY(!SkyCacheFiles::removeFiles({directoryPath}, QStringLiteral("test cache")));
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Failed to remove test cache file")));
}

QTEST_GUILESS_MAIN(SkyCacheFilesTests)

#include "SkyCacheFilesTests.moc"
