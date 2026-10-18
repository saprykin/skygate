#include "settings/SkyEphemerisDataCacheSettings.hpp"
#include "settings/SkyPersistedStateSnapshot.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QRegularExpression>
#include <QSettings>
#include <QtTest/QtTest>

#include <cstddef>
#include <optional>

using skygate::ui::internal::SkyEphemerisDataCacheSettings;

class SkyEphemerisDataCacheSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void savesLoadsAndClearsMetadata();
    void kernelRangePersistsAndFallsBackToMetadata();
    void blankUpdateTextFallsBackToDefaults();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyEphemerisDataCacheSettingsTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkygateUiEphemerisDataCacheSettingsTests")));
}

void SkyEphemerisDataCacheSettingsTests::savesLoadsAndClearsMetadata()
{
    QSettings settings;
    settings.clear();

    SkyEphemerisDataCacheSnapshot savedSnapshot;
    savedSnapshot.installedKernelAssetId = QStringLiteral("de440s-kernel");
    savedSnapshot.installedKernelProfileId = QStringLiteral("de440s-short-range");
    savedSnapshot.installedKernelPath = m_settings.filePath(QStringLiteral("de440s.bsp"));
    savedSnapshot.installedKernelVersion = QStringLiteral("DE440s-2026a");
    savedSnapshot.installedKernelLongRange = false;
    savedSnapshot.installedEarthOrientationPath = m_settings.filePath(QStringLiteral("eop.csv"));
    savedSnapshot.installedEarthOrientationVersion = QStringLiteral("IERS-2026-05");
    savedSnapshot.installedLeapSecondTablePath = m_settings.filePath(QStringLiteral("leap-seconds.list"));
    savedSnapshot.installedLeapSecondTableVersion = QStringLiteral("leap-seconds-2025");
    savedSnapshot.installedDeltaTDataPath = m_settings.filePath(QStringLiteral("delta-t.csv"));
    savedSnapshot.installedDeltaTDataVersion = QStringLiteral("delta-t-2026");
    savedSnapshot.dataRevisionToken = QStringLiteral("ephemeris-rev-42");
    savedSnapshot.lastUpdateResult = QStringLiteral("Updated");

    QVERIFY(SkyEphemerisDataCacheSettings::save(savedSnapshot));
    QCOMPARE(settings.value("skyContext/version").toInt(), SkyPersistedStateSnapshot::kSettingsVersion);
    const SkyEphemerisDataCacheSnapshot loadedSnapshot = SkyEphemerisDataCacheSettings::load();
    QCOMPARE(loadedSnapshot.installedKernelAssetId, savedSnapshot.installedKernelAssetId);
    QCOMPARE(loadedSnapshot.installedKernelProfileId, savedSnapshot.installedKernelProfileId);
    QCOMPARE(loadedSnapshot.installedKernelPath, savedSnapshot.installedKernelPath);
    QCOMPARE(loadedSnapshot.installedKernelVersion, savedSnapshot.installedKernelVersion);
    QCOMPARE(loadedSnapshot.installedKernelLongRange, savedSnapshot.installedKernelLongRange);
    QCOMPARE(loadedSnapshot.installedEarthOrientationPath, savedSnapshot.installedEarthOrientationPath);
    QCOMPARE(loadedSnapshot.installedEarthOrientationVersion, savedSnapshot.installedEarthOrientationVersion);
    QCOMPARE(loadedSnapshot.installedLeapSecondTablePath, savedSnapshot.installedLeapSecondTablePath);
    QCOMPARE(loadedSnapshot.installedLeapSecondTableVersion, savedSnapshot.installedLeapSecondTableVersion);
    QCOMPARE(loadedSnapshot.installedDeltaTDataPath, savedSnapshot.installedDeltaTDataPath);
    QCOMPARE(loadedSnapshot.installedDeltaTDataVersion, savedSnapshot.installedDeltaTDataVersion);
    QCOMPARE(loadedSnapshot.dataRevisionToken, savedSnapshot.dataRevisionToken);
    QCOMPARE(loadedSnapshot.lastUpdateResult, savedSnapshot.lastUpdateResult);

    QVERIFY(SkyEphemerisDataCacheSettings::clear());
    QVERIFY(!settings.contains("skyContext/ephemerisData"));
    const SkyEphemerisDataCacheSnapshot clearedSnapshot = SkyEphemerisDataCacheSettings::load();
    QVERIFY(clearedSnapshot.installedKernelAssetId.isEmpty());
    QVERIFY(clearedSnapshot.installedKernelPath.isEmpty());
    QVERIFY(!clearedSnapshot.installedKernelLongRange.has_value());
    QCOMPARE(clearedSnapshot.dataRevisionToken, QString("bundled"));
    QCOMPARE(clearedSnapshot.lastUpdateResult, QString("Bundled fallback"));
}

void SkyEphemerisDataCacheSettingsTests::kernelRangePersistsAndFallsBackToMetadata()
{
    QSettings settings;
    settings.clear();

    SkyEphemerisDataCacheSnapshot savedSnapshot;
    savedSnapshot.installedKernelAssetId = QStringLiteral("de441-kernel");
    savedSnapshot.installedKernelLongRange = std::nullopt;
    QVERIFY(SkyEphemerisDataCacheSettings::save(savedSnapshot));

    // No stored range leaves the decision to the kernel metadata.
    QCOMPARE(SkyEphemerisDataCacheSettings::load().installedKernelLongRange, std::optional<bool>(true));

    settings.setValue("skyContext/ephemerisData/installedKernelLongRange", "wide");
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Invalid boolean setting skyContext/ephemerisData/installedKernelLongRange .*")
    );
    QCOMPARE(SkyEphemerisDataCacheSettings::load().installedKernelLongRange, std::optional<bool>(true));
}

void SkyEphemerisDataCacheSettingsTests::blankUpdateTextFallsBackToDefaults()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/ephemerisData/installedKernelVersion", "DE440s-2026a");
    settings.setValue("skyContext/ephemerisData/dataRevisionToken", "   ");
    settings.setValue("skyContext/ephemerisData/lastUpdateResult", "");

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Invalid blank setting .* - using fallback .*"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Invalid blank setting .* - using fallback .*"));
    const SkyEphemerisDataCacheSnapshot loadedSnapshot = SkyEphemerisDataCacheSettings::load();
    QCOMPARE(loadedSnapshot.installedKernelVersion, QString("DE440s-2026a"));
    QCOMPARE(loadedSnapshot.installedKernelLongRange, std::optional<bool>(false));
    QCOMPARE(loadedSnapshot.dataRevisionToken, QString("bundled"));
    QCOMPARE(loadedSnapshot.lastUpdateResult, QString("Bundled fallback"));
}

QTEST_GUILESS_MAIN(SkyEphemerisDataCacheSettingsTests)

#include "SkyEphemerisDataCacheSettingsTests.moc"
