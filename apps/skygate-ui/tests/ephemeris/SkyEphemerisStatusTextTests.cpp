#include "ephemeris/SkyEphemerisStatusText.hpp"

#include <QString>
#include <QStringList>
#include <QtTest/QtTest>

#include <string_view>
#include <vector>

using skygate::ui::internal::SkyEphemerisStatusText;

namespace {

struct StatusTextSample final {
    const char* label;
    QString text;
};

QString statusPrefix()
{
    const std::string_view prefix = SkyEphemerisStatusText::kStatusPrefix;
    return QString::fromLatin1(prefix.data(), static_cast<qsizetype>(prefix.size()));
}

std::vector<StatusTextSample> allStatusTexts()
{
    return {
        {"bundledFallback", SkyEphemerisStatusText::bundledFallback()},
        {"installedDataMissing", SkyEphemerisStatusText::installedDataMissing()},
        {"installedDataActive", SkyEphemerisStatusText::installedDataActive()},
        {"updateAlreadyInProgress", SkyEphemerisStatusText::updateAlreadyInProgress()},
        {"updateManifestUnavailable", SkyEphemerisStatusText::updateManifestUnavailable()},
        {"updateManifestUnreadable", SkyEphemerisStatusText::updateManifestUnreadable()},
        {"updateManifestTooLarge", SkyEphemerisStatusText::updateManifestTooLarge()},
        {"updateManagerUnavailable", SkyEphemerisStatusText::updateManagerUnavailable()},
        {"updateProfileUnavailable", SkyEphemerisStatusText::updateProfileUnavailable()},
        {"updateProfileMissingAsset", SkyEphemerisStatusText::updateProfileMissingAsset()},
        {"writableCacheUnavailable", SkyEphemerisStatusText::writableCacheUnavailable()},
        {"stagingAreaUnclearable", SkyEphemerisStatusText::stagingAreaUnclearable()},
        {"stagingAreaUncreatable", SkyEphemerisStatusText::stagingAreaUncreatable()},
        {"checkingUpdateManifest", SkyEphemerisStatusText::checkingUpdateManifest()},
        {"downloading", SkyEphemerisStatusText::downloading(QStringLiteral("fresh"))},
        {"verifying", SkyEphemerisStatusText::verifying(QStringLiteral("fresh"))},
        {"cancelingDownload", SkyEphemerisStatusText::cancelingDownload()},
        {"checkingSupportData", SkyEphemerisStatusText::checkingSupportData()},
        {"supportDataUpdatesAvailable", SkyEphemerisStatusText::supportDataUpdatesAvailable()},
        {"supportDataUpToDate", SkyEphemerisStatusText::supportDataUpToDate()},
        {"failure", SkyEphemerisStatusText::failure(QStringLiteral("manifest download failed"))},
    };
}

}  // namespace

class SkyEphemerisStatusTextTests final : public QObject {
    Q_OBJECT

private slots:
    void statusPrefixMatchesPresentedPrefix();
    void dataSourceStatusTextsArePinned();
    void updateGuardStatusTextsArePinned();
    void progressStatusTextsArePinned();
    void progressNameIsInterpolatedVerbatim_data();
    void progressNameIsInterpolatedVerbatim();
    void failureAppendsTheDiagnosticVerbatim();
    void everyStatusTextCarriesThePrefixOnce();
    void noTwoStatusesShareTheSameText();
};

void SkyEphemerisStatusTextTests::statusPrefixMatchesPresentedPrefix()
{
    // Presentation code strips this exact prefix before rendering a status.
    QCOMPARE(statusPrefix(), QStringLiteral("Ephemeris data: "));
}

void SkyEphemerisStatusTextTests::dataSourceStatusTextsArePinned()
{
    QCOMPARE(SkyEphemerisStatusText::bundledFallback(), QStringLiteral("Ephemeris data: Bundled fallback"));
    QCOMPARE(
        SkyEphemerisStatusText::installedDataMissing(),
        QStringLiteral("Ephemeris data: Installed data missing; bundled fallback active")
    );
    QCOMPARE(SkyEphemerisStatusText::installedDataActive(), QStringLiteral("Ephemeris data: Installed data active"));
}

void SkyEphemerisStatusTextTests::updateGuardStatusTextsArePinned()
{
    QCOMPARE(
        SkyEphemerisStatusText::updateAlreadyInProgress(), QStringLiteral("Ephemeris data: Update already in progress")
    );
    QCOMPARE(
        SkyEphemerisStatusText::updateManifestUnavailable(),
        QStringLiteral("Ephemeris data: Update manifest unavailable")
    );
    QCOMPARE(
        SkyEphemerisStatusText::updateManifestUnreadable(),
        QStringLiteral("Ephemeris data: Unable to open update manifest")
    );
    QCOMPARE(
        SkyEphemerisStatusText::updateManifestTooLarge(), QStringLiteral("Ephemeris data: Update manifest is too large")
    );
    QCOMPARE(
        SkyEphemerisStatusText::updateManagerUnavailable(), QStringLiteral("Ephemeris data: Update manager unavailable")
    );
    QCOMPARE(
        SkyEphemerisStatusText::updateProfileUnavailable(), QStringLiteral("Ephemeris data: Update profile unavailable")
    );
    QCOMPARE(
        SkyEphemerisStatusText::updateProfileMissingAsset(),
        QStringLiteral("Ephemeris data: Update profile references a missing asset")
    );
    QCOMPARE(
        SkyEphemerisStatusText::writableCacheUnavailable(), QStringLiteral("Ephemeris data: Writable cache unavailable")
    );
    QCOMPARE(
        SkyEphemerisStatusText::stagingAreaUnclearable(),
        QStringLiteral("Ephemeris data: Unable to clear update staging area")
    );
    QCOMPARE(
        SkyEphemerisStatusText::stagingAreaUncreatable(),
        QStringLiteral("Ephemeris data: Unable to create update staging area")
    );
}

void SkyEphemerisStatusTextTests::progressStatusTextsArePinned()
{
    QCOMPARE(
        SkyEphemerisStatusText::checkingUpdateManifest(), QStringLiteral("Ephemeris data: Checking update manifest...")
    );
    QCOMPARE(SkyEphemerisStatusText::cancelingDownload(), QStringLiteral("Ephemeris data: Canceling download..."));
    QCOMPARE(
        SkyEphemerisStatusText::checkingSupportData(), QStringLiteral("Ephemeris data: Checking time and Earth data...")
    );
    QCOMPARE(
        SkyEphemerisStatusText::supportDataUpdatesAvailable(),
        QStringLiteral("Ephemeris data: Updates available for time and Earth data")
    );
    QCOMPARE(
        SkyEphemerisStatusText::supportDataUpToDate(),
        QStringLiteral("Ephemeris data: Time and Earth data is up to date")
    );
}

void SkyEphemerisStatusTextTests::progressNameIsInterpolatedVerbatim_data()
{
    QTest::addColumn<QString>("progressName");
    QTest::addColumn<QString>("downloadText");
    QTest::addColumn<QString>("verifyText");

    QTest::newRow("single word") << QStringLiteral("test") << QStringLiteral("Ephemeris data: Downloading test...")
                                 << QStringLiteral("Ephemeris data: Verifying test...");
    QTest::newRow("dashed asset") << QStringLiteral("test-asset")
                                  << QStringLiteral("Ephemeris data: Downloading test-asset...")
                                  << QStringLiteral("Ephemeris data: Verifying test-asset...");
    QTest::newRow("spaces kept") << QStringLiteral("Earth orientation data")
                                 << QStringLiteral("Ephemeris data: Downloading Earth orientation data...")
                                 << QStringLiteral("Ephemeris data: Verifying Earth orientation data...");
    QTest::newRow("bracketed version") << QStringLiteral("DE441 (2.0)")
                                       << QStringLiteral("Ephemeris data: Downloading DE441 (2.0)...")
                                       << QStringLiteral("Ephemeris data: Verifying DE441 (2.0)...");
}

void SkyEphemerisStatusTextTests::progressNameIsInterpolatedVerbatim()
{
    QFETCH(QString, progressName);
    QFETCH(QString, downloadText);
    QFETCH(QString, verifyText);

    QCOMPARE(SkyEphemerisStatusText::downloading(progressName), downloadText);
    QCOMPARE(SkyEphemerisStatusText::verifying(progressName), verifyText);
}

void SkyEphemerisStatusTextTests::failureAppendsTheDiagnosticVerbatim()
{
    QCOMPARE(
        SkyEphemerisStatusText::failure(QStringLiteral("manifest download failed")),
        QStringLiteral("Ephemeris data: manifest download failed")
    );
    QCOMPARE(
        SkyEphemerisStatusText::failure(QStringLiteral("stage failed: checksum mismatch")),
        QStringLiteral("Ephemeris data: stage failed: checksum mismatch")
    );
    QCOMPARE(SkyEphemerisStatusText::failure({}), statusPrefix());
}

void SkyEphemerisStatusTextTests::everyStatusTextCarriesThePrefixOnce()
{
    const QString prefix = statusPrefix();
    QVERIFY(!prefix.isEmpty());

    for (const StatusTextSample& sample : allStatusTexts()) {
        const QString label = QString::fromLatin1(sample.label);
        QVERIFY2(
            sample.text.startsWith(prefix),
            qPrintable(QStringLiteral("%1 lacks the status prefix: %2").arg(label, sample.text))
        );
        const QString remainder = sample.text.mid(prefix.size());
        QVERIFY2(!remainder.isEmpty(), qPrintable(QStringLiteral("%1 has an empty status body").arg(label)));
        QVERIFY2(
            !remainder.startsWith(prefix),
            qPrintable(QStringLiteral("%1 repeats the status prefix: %2").arg(label, sample.text))
        );
    }
}

void SkyEphemerisStatusTextTests::noTwoStatusesShareTheSameText()
{
    QStringList seenTexts;
    for (const StatusTextSample& sample : allStatusTexts()) {
        const QString label = QString::fromLatin1(sample.label);
        QVERIFY2(
            !seenTexts.contains(sample.text),
            qPrintable(QStringLiteral("%1 reuses an earlier status text: %2").arg(label, sample.text))
        );
        seenTexts.push_back(sample.text);
    }
}

QTEST_GUILESS_MAIN(SkyEphemerisStatusTextTests)

#include "SkyEphemerisStatusTextTests.moc"
