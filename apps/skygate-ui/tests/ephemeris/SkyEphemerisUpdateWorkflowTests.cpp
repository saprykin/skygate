#include "FakeNetworkAccessManager.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkyEphemerisUpdateWorkflow.hpp"
#include "SkySettingsStore.hpp"
#include "engine/EphemerisDataManifest.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QPointer>
#include <QSignalSpy>
#include <QString>
#include <QUrl>
#include <QtTest/QtTest>

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

using skygate::ui::tests::FakeNetworkAccessManager;
using skygate::ui::tests::FakeNetworkReply;
using skygate::ui::tests::SettingsTestFixture;

namespace {

constexpr std::string_view kEmptySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

QString pendingAssetManifestJson(const QString& assetUrl)
{
    return QStringLiteral(R"({
  "schemaVersion": 1,
  "id": "test-data",
  "displayName": "Test data",
  "version": "2026a",
  "provenance": "workflow test",
  "dateRanges": [
    {
      "id": "test-range",
      "displayName": "Test range",
      "start": "1900-01-01",
      "end": "2100-01-01"
    }
  ],
  "profiles": [
    {
      "id": "de440s-short-range",
      "displayName": "DE440sShortRange",
      "bundled": false,
      "longRange": false,
      "assetIds": ["de440s-kernel"]
    }
  ],
  "assets": [
    {
      "id": "de440s-kernel",
      "kind": "solar-system-kernel",
      "profileId": "de440s-short-range",
      "version": "test-asset",
      "sourceUrl": "%1",
      "relativePath": "kernels/de440s.bsp",
      "checksum": {
        "algorithm": "sha256",
        "value": "%2"
      },
      "compression": {
        "format": "none",
        "uncompressedSizeBytes": 1
      },
      "validityRange": {
        "id": "kernel-range",
        "displayName": "Kernel range",
        "start": "1900-01-01",
        "end": "2100-01-01"
      }
    }
  ]
})")
        .arg(assetUrl, QString::fromLatin1(kEmptySha256.data(), static_cast<qsizetype>(kEmptySha256.size())));
}

skygate::ephemeris::EphemerisDataManifest parseManifest(const QString& payload)
{
    const QByteArray bytes = payload.toUtf8();
    skygate::ephemeris::EphemerisDataManifest::ParseResult result = skygate::ephemeris::EphemerisDataManifest::parse(
        std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size()))
    );
    if (!result.isSuccess()) {
        QTest::qFail("Test manifest did not parse.", __FILE__, __LINE__);
        return {};
    }
    return std::move(result.manifest);
}

bool writeFile(const QString& path, const QByteArray& contents)
{
    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

}  // namespace

class SkyEphemerisUpdateWorkflowTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void destroysCleanlyWhileUpdateIsPending();
    void updateIsUnavailableWithoutManifest();

private:
    SettingsTestFixture m_settings;
};

void SkyEphemerisUpdateWorkflowTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyEphemerisUpdateWorkflowTests")));
}

void SkyEphemerisUpdateWorkflowTests::init()
{
    m_settings.clearSettings();
}

void SkyEphemerisUpdateWorkflowTests::destroysCleanlyWhileUpdateIsPending()
{
    const QString manifestPath = m_settings.filePath(QStringLiteral("pending-asset-manifest.json"));
    const QString assetUrl = QStringLiteral("https://example.test/kernels/de440s.bsp");
    QVERIFY(writeFile(manifestPath, pendingAssetManifestJson(assetUrl).toUtf8()));

    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(assetUrl, {.manualFinish = true});

    skygate::ephemeris::EphemerisDataManifest fallbackManifest = parseManifest(pendingAssetManifestJson(assetUrl));

    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store, nullptr, &networkAccessManager);
    auto workflow = std::make_unique<SkyEphemerisUpdateWorkflow>(
        &manager, &fallbackManifest, m_settings.path(), m_settings.path() + QStringLiteral("/pending-cache"), nullptr
    );

    QSignalSpy statusSpy(workflow.get(), &SkyEphemerisUpdateWorkflow::statusChanged);
    QVERIFY(
        workflow->updateProfile(QStringLiteral("de440s-short-range"), QUrl::fromLocalFile(manifestPath).toString())
    );

    QCOMPARE(workflow->isUpdateInProgress(), true);
    QCOMPARE(workflow->updateProgress(), 0.0);
    QCOMPARE(workflow->statusText(), QString("Ephemeris data: Downloading test-asset..."));
    QVERIFY(statusSpy.count() > 0);
    QCOMPARE(networkAccessManager.requestedUrls().size(), 1);
    QCOMPARE(networkAccessManager.requestedUrls().constFirst(), assetUrl);

    const QList<FakeNetworkReply*> issuedReplies = networkAccessManager.issuedReplies();
    QCOMPARE(issuedReplies.size(), 1);
    const QPointer<FakeNetworkReply> pendingReply = issuedReplies.constFirst();

    workflow.reset();

    QVERIFY(!pendingReply.isNull());
    pendingReply->finishNow();
    QCoreApplication::processEvents();

    QCOMPARE(networkAccessManager.finishedUrls().size(), 1);
    QCOMPARE(manager.statusText(), QString("Ephemeris data: Bundled fallback"));
}

void SkyEphemerisUpdateWorkflowTests::updateIsUnavailableWithoutManifest()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);
    SkyEphemerisUpdateWorkflow workflow(
        &manager, nullptr, m_settings.path(), m_settings.path() + QStringLiteral("/missing-manifest-cache"), nullptr
    );

    QVERIFY(!workflow.updateEnabled());
    QVERIFY(!workflow.updateProfile(QStringLiteral("de440s-short-range"), QString()));
    QVERIFY(!workflow.isUpdateInProgress());
    QCOMPARE(workflow.statusText(), QString("Ephemeris data: Update manifest unavailable"));
}

QTEST_GUILESS_MAIN(SkyEphemerisUpdateWorkflowTests)

#include "SkyEphemerisUpdateWorkflowTests.moc"
