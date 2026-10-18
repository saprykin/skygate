#include "engine/EphemerisDataManifest.hpp"
#include "ephemeris/SkyEphemerisDataManager.hpp"
#include "ephemeris/SkyEphemerisPathSegmentPolicy.hpp"
#include "ephemeris/SkyEphemerisUpdateWorkflow.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/EphemerisDataCacheSnapshotFixture.hpp"
#include "support/FakeNetworkAccessManager.hpp"
#include "support/SettingsTestFixture.hpp"
#include "support/StagedEphemerisAssetFixture.hpp"

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
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

using skygate::ui::tests::EphemerisDataCacheSnapshotFixture;
using skygate::ui::tests::FakeNetworkAccessManager;
using skygate::ui::tests::FakeNetworkReply;
using skygate::ui::tests::FakeNetworkResponse;
using skygate::ui::tests::SettingsTestFixture;
using skygate::ui::tests::StagedEphemerisAssetFixture;
using namespace skygate::ui::internal;

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

QByteArray stagedPayload()
{
    return QByteArray(
        StagedEphemerisAssetFixture::kPayload.data(),
        static_cast<qsizetype>(StagedEphemerisAssetFixture::kPayload.size())
    );
}

skygate::ephemeris::EphemerisDataManifest twoAssetUpdateManifest(const bool includeSizes = true)
{
    using AssetKind = skygate::ephemeris::EphemerisDataManifest::AssetKind;

    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "workflow test";
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = false,
            .longRange = false,
            .assetIds = {"de440s-kernel", "leap-seconds"},
        }
    );

    skygate::ephemeris::EphemerisDataManifest::Asset kernel =
        StagedEphemerisAssetFixture::stagedAsset("de440s-kernel", AssetKind::SolarSystemKernel, "kernels/de440s.bsp");
    kernel.sourceUrl = "https://example.test/kernels/de440s.bsp";
    if (!includeSizes) {
        kernel.compression.uncompressedSizeBytes.reset();
    }
    manifest.assets.push_back(std::move(kernel));

    skygate::ephemeris::EphemerisDataManifest::Asset leapSeconds =
        StagedEphemerisAssetFixture::stagedAsset("leap-seconds", AssetKind::LeapSecondTable, "time/leap-seconds.list");
    leapSeconds.sourceUrl = "https://example.test/time/leap-seconds.list";
    if (!includeSizes) {
        leapSeconds.compression.uncompressedSizeBytes.reset();
    }
    manifest.assets.push_back(std::move(leapSeconds));
    return manifest;
}

FakeNetworkResponse manualPayloadResponse()
{
    return FakeNetworkResponse{.payload = stagedPayload(), .manualFinish = true};
}

class UpdateWorkflowFixture final {
public:
    UpdateWorkflowFixture(
        SettingsTestFixture& settings, skygate::ephemeris::EphemerisDataManifest manifest, const QString& cacheName
    )
        : m_manifest(std::move(manifest)), m_cacheRoot(settings.path() + QLatin1Char('/') + cacheName),
          m_manager(&m_store, nullptr, &m_network),
          m_workflow(
              std::make_unique<SkyEphemerisUpdateWorkflow>(
                  &m_manager, &m_manifest, settings.path(), m_cacheRoot, nullptr
              )
          )
    {
    }

    [[nodiscard]] bool start(const QString& profileId = QStringLiteral("de440s-short-range"))
    {
        return m_workflow->updateProfile(profileId, QString());
    }

    [[nodiscard]] SkyEphemerisUpdateWorkflow& workflow() const noexcept
    {
        return *m_workflow;
    }

    [[nodiscard]] SkyEphemerisDataManager& manager() noexcept
    {
        return m_manager;
    }

    [[nodiscard]] FakeNetworkAccessManager& network() noexcept
    {
        return m_network;
    }

    [[nodiscard]] SkySettingsStore& store() noexcept
    {
        return m_store;
    }

    [[nodiscard]] QString stagingRoot(const QString& profileId = QStringLiteral("de440s-short-range")) const
    {
        return m_cacheRoot + QStringLiteral("/staging/")
               + SkyEphemerisPathSegmentPolicy::spellPathSegment(profileId, QStringLiteral("profile"));
    }

    [[nodiscard]] QString stagedAssetPath(
        const std::string_view assetId, const QString& profileId = QStringLiteral("de440s-short-range")
    ) const
    {
        const skygate::ephemeris::EphemerisDataManifest::Asset* stagedAsset = m_manifest.asset(assetId);
        Q_ASSERT(stagedAsset != nullptr);
        return stagingRoot(profileId) + QLatin1Char('/') + QString::fromStdString(stagedAsset->relativePath);
    }

    void destroyWorkflow()
    {
        m_workflow.reset();
    }

private:
    skygate::ephemeris::EphemerisDataManifest m_manifest;
    FakeNetworkAccessManager m_network;
    QString m_cacheRoot;
    SkySettingsStore m_store;
    SkyEphemerisDataManager m_manager;
    std::unique_ptr<SkyEphemerisUpdateWorkflow> m_workflow;
};

}  // namespace

class SkyEphemerisUpdateWorkflowTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void destroysCleanlyWhileUpdateIsPending();
    void destroysCleanlyWhileSecondReplyIsPending();
    void updateIsUnavailableWithoutManifest();
    void completesMultiAssetUpdateOnce();
    void completesUpdateWithUnknownAssetSizes();
    void stopsUpdateWhenSecondAssetFails();
    void cancelRetainsStagingUntilNextUpdate();
    void activationFailurePreservesInstalledData();
    void spellPathSegmentTrimsWhitespace();
    void spellPathSegmentReplacesPunctuation();
    void spellPathSegmentKeepsUnicodeLettersAndDigits();
    void spellPathSegmentCollapsesRepeatedDots();
    void spellPathSegmentFallsBackWhenEmpty();
    void spellPathSegmentFallsBackForSingleDot();

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

void SkyEphemerisUpdateWorkflowTests::destroysCleanlyWhileSecondReplyIsPending()
{
    UpdateWorkflowFixture fixture(m_settings, twoAssetUpdateManifest(), QStringLiteral("second-pending-cache"));
    fixture.network().enqueueResponse(
        QStringLiteral("https://example.test/kernels/de440s.bsp"), manualPayloadResponse()
    );
    fixture.network().enqueueResponse(
        QStringLiteral("https://example.test/time/leap-seconds.list"), manualPayloadResponse()
    );

    QVERIFY(fixture.start());
    QCOMPARE(fixture.network().requestedUrls().size(), 1);

    QList<FakeNetworkReply*> replies = fixture.network().issuedReplies();
    QCOMPARE(replies.size(), 1);
    replies.constFirst()->finishNow();
    QCoreApplication::processEvents();

    QCOMPARE(fixture.network().requestedUrls().size(), 2);
    replies = fixture.network().issuedReplies();
    QVERIFY(!replies.isEmpty());
    const QPointer<FakeNetworkReply> pendingReply = replies.constLast();

    fixture.destroyWorkflow();

    QVERIFY(!pendingReply.isNull());
    pendingReply->finishNow();
    QCoreApplication::processEvents();

    QCOMPARE(fixture.network().finishedUrls().size(), 2);
    QCOMPARE(fixture.manager().statusText(), QString("Ephemeris data: Bundled fallback"));
    QVERIFY(!fixture.manager().usingInstalledData());
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

void SkyEphemerisUpdateWorkflowTests::completesMultiAssetUpdateOnce()
{
    UpdateWorkflowFixture fixture(m_settings, twoAssetUpdateManifest(), QStringLiteral("multi-asset-cache"));
    fixture.network().enqueueResponse(
        QStringLiteral("https://example.test/kernels/de440s.bsp"), manualPayloadResponse()
    );
    fixture.network().enqueueResponse(
        QStringLiteral("https://example.test/time/leap-seconds.list"), manualPayloadResponse()
    );
    QSignalSpy preferredSpy(&fixture.workflow(), &SkyEphemerisUpdateWorkflow::preferredProfileSelected);
    const std::uint64_t originalRevision = fixture.manager().dataRevision();

    QVERIFY(fixture.start());
    QCOMPARE(fixture.workflow().isUpdateInProgress(), true);
    QCOMPARE(fixture.workflow().updateProgress(), 0.0);
    QCOMPARE(fixture.workflow().statusText(), QString("Ephemeris data: Downloading test..."));
    QCOMPARE(fixture.network().requestedUrls().size(), 1);

    QList<FakeNetworkReply*> replies = fixture.network().issuedReplies();
    QCOMPARE(replies.size(), 1);
    replies.constFirst()->finishNow();
    QCoreApplication::processEvents();

    QCOMPARE(fixture.network().requestedUrls().size(), 2);
    QCOMPARE(fixture.network().requestedUrls().constLast(), QString("https://example.test/time/leap-seconds.list"));
    QCOMPARE(fixture.workflow().isUpdateInProgress(), true);
    QCOMPARE(fixture.workflow().updateProgress(), 0.5);
    QCOMPARE(preferredSpy.count(), 0);

    replies = fixture.network().issuedReplies();
    QVERIFY(!replies.isEmpty());
    replies.constLast()->finishNow();
    QCoreApplication::processEvents();

    QCOMPARE(fixture.workflow().isUpdateInProgress(), false);
    QCOMPARE(fixture.workflow().updateProgress(), 1.0);
    QCOMPARE(fixture.workflow().statusText(), QString("Ephemeris data: Installed data active"));
    QVERIFY(fixture.workflow().updateEnabled());
    QCOMPARE(preferredSpy.count(), 1);
    QCOMPARE(preferredSpy.constFirst().constFirst().toString(), QString("de440s-short-range"));
    QVERIFY(fixture.manager().usingInstalledData());
    QVERIFY(fixture.manager().dataRevision() > originalRevision);
    QCOMPARE(fixture.store().loadEphemerisDataCache().installedKernelAssetId, QString("de440s-kernel"));
    QCOMPARE(fixture.store().loadEphemerisDataCache().installedLeapSecondTableVersion, QString("test"));
}

void SkyEphemerisUpdateWorkflowTests::completesUpdateWithUnknownAssetSizes()
{
    UpdateWorkflowFixture fixture(
        m_settings, twoAssetUpdateManifest(false), QStringLiteral("unknown-asset-size-cache")
    );
    fixture.network().enqueueResponse(
        QStringLiteral("https://example.test/kernels/de440s.bsp"), manualPayloadResponse()
    );
    fixture.network().enqueueResponse(
        QStringLiteral("https://example.test/time/leap-seconds.list"), manualPayloadResponse()
    );
    QSignalSpy preferredSpy(&fixture.workflow(), &SkyEphemerisUpdateWorkflow::preferredProfileSelected);

    QVERIFY(fixture.start());
    QCOMPARE(fixture.workflow().updateProgress(), 0.0);

    QList<FakeNetworkReply*> replies = fixture.network().issuedReplies();
    QCOMPARE(replies.size(), 1);
    replies.constFirst()->finishNow();
    QCoreApplication::processEvents();

    QCOMPARE(fixture.network().requestedUrls().size(), 2);
    QCOMPARE(fixture.workflow().isUpdateInProgress(), true);
    QCOMPARE(fixture.workflow().updateProgress(), 1.0);

    replies = fixture.network().issuedReplies();
    QVERIFY(!replies.isEmpty());
    replies.constLast()->finishNow();
    QCoreApplication::processEvents();

    QCOMPARE(fixture.workflow().isUpdateInProgress(), false);
    QCOMPARE(fixture.workflow().updateProgress(), 1.0);
    QCOMPARE(preferredSpy.count(), 1);
    QVERIFY(fixture.manager().usingInstalledData());
}

void SkyEphemerisUpdateWorkflowTests::stopsUpdateWhenSecondAssetFails()
{
    skygate::ephemeris::EphemerisDataManifest manifest = twoAssetUpdateManifest();
    manifest.assets.back().sourceUrl.clear();

    UpdateWorkflowFixture fixture(m_settings, std::move(manifest), QStringLiteral("mid-failure-cache"));
    fixture.network().enqueueResponse(
        QStringLiteral("https://example.test/kernels/de440s.bsp"), manualPayloadResponse()
    );
    QSignalSpy preferredSpy(&fixture.workflow(), &SkyEphemerisUpdateWorkflow::preferredProfileSelected);
    QSignalSpy statusSpy(&fixture.workflow(), &SkyEphemerisUpdateWorkflow::statusChanged);
    const std::uint64_t originalRevision = fixture.manager().dataRevision();

    QVERIFY(fixture.start());
    const int statusCountBeforeFailure = statusSpy.count();

    QList<FakeNetworkReply*> replies = fixture.network().issuedReplies();
    QCOMPARE(replies.size(), 1);
    const QPointer<FakeNetworkReply> completedReply = replies.constFirst();
    completedReply->finishNow();
    QCoreApplication::processEvents();

    QCOMPARE(fixture.network().requestedUrls().size(), 1);
    QCOMPARE(fixture.workflow().isUpdateInProgress(), false);
    QCOMPARE(fixture.workflow().updateProgress(), 0.0);
    QCOMPARE(
        fixture.workflow().statusText(), QString("Ephemeris data: Ephemeris update download source file is missing.")
    );
    QCOMPARE(preferredSpy.count(), 0);
    QCOMPARE(fixture.manager().dataRevision(), originalRevision);
    QVERIFY(!fixture.manager().usingInstalledData());
    QVERIFY(statusSpy.count() > statusCountBeforeFailure);

    const int terminalStatusCount = statusSpy.count();
    completedReply->finishNow();
    QCoreApplication::processEvents();
    QCOMPARE(statusSpy.count(), terminalStatusCount);
}

void SkyEphemerisUpdateWorkflowTests::cancelRetainsStagingUntilNextUpdate()
{
    UpdateWorkflowFixture fixture(m_settings, twoAssetUpdateManifest(), QStringLiteral("cancel-cache"));
    const QString kernelUrl = QStringLiteral("https://example.test/kernels/de440s.bsp");
    const QString leapSecondsUrl = QStringLiteral("https://example.test/time/leap-seconds.list");
    fixture.network().enqueueResponse(kernelUrl, manualPayloadResponse());
    fixture.network().enqueueResponse(leapSecondsUrl, manualPayloadResponse());
    QSignalSpy preferredSpy(&fixture.workflow(), &SkyEphemerisUpdateWorkflow::preferredProfileSelected);
    const std::uint64_t originalRevision = fixture.manager().dataRevision();

    QVERIFY(fixture.start());
    QList<FakeNetworkReply*> replies = fixture.network().issuedReplies();
    QCOMPARE(replies.size(), 1);
    replies.constFirst()->finishNow();
    QCoreApplication::processEvents();
    QCOMPARE(fixture.network().requestedUrls().size(), 2);

    fixture.workflow().cancelActiveUpdate();
    QCOMPARE(fixture.workflow().statusText(), QString("Ephemeris data: Canceling download..."));
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.workflow().isUpdateInProgress(), 5000);

    QCOMPARE(
        fixture.workflow().statusText(),
        QString("Ephemeris data: Ephemeris update download was canceled during transfer.")
    );
    QCOMPARE(fixture.workflow().updateProgress(), 0.0);
    QCOMPARE(preferredSpy.count(), 0);
    QCOMPARE(fixture.manager().dataRevision(), originalRevision);
    QVERIFY(!fixture.manager().usingInstalledData());

    const QString kernelPath = fixture.stagedAssetPath("de440s-kernel");
    const QString leapSecondsPath = fixture.stagedAssetPath("leap-seconds");
    QVERIFY(QFileInfo::exists(kernelPath));
    QCOMPARE(QFileInfo(kernelPath).size(), static_cast<qint64>(StagedEphemerisAssetFixture::kPayload.size()));
    QVERIFY(QFileInfo::exists(leapSecondsPath));
    QCOMPARE(QFileInfo(leapSecondsPath).size(), qint64{0});

    fixture.network().enqueueResponse(kernelUrl, manualPayloadResponse());
    fixture.network().enqueueResponse(leapSecondsUrl, manualPayloadResponse());
    QVERIFY(fixture.start());
    QCOMPARE(fixture.network().requestedUrls().size(), 3);
    QVERIFY(!QFileInfo::exists(leapSecondsPath));
    QVERIFY(QFileInfo::exists(kernelPath));
    QCOMPARE(QFileInfo(kernelPath).size(), qint64{0});

    fixture.destroyWorkflow();
}

void SkyEphemerisUpdateWorkflowTests::activationFailurePreservesInstalledData()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore seedStore;
    QVERIFY(seedStore.saveEphemerisDataCache(EphemerisDataCacheSnapshotFixture::installedSnapshot(oldKernelPath, {})));

    UpdateWorkflowFixture fixture(m_settings, twoAssetUpdateManifest(), QStringLiteral("activation-failure-cache"));
    QVERIFY(fixture.manager().usingInstalledData());
    const std::uint64_t originalRevision = fixture.manager().dataRevision();

    const QByteArray wrongPayload(stagedPayload().size(), 'x');
    fixture.network().enqueueResponse(
        QStringLiteral("https://example.test/kernels/de440s.bsp"),
        FakeNetworkResponse{.payload = wrongPayload, .manualFinish = true}
    );
    fixture.network().enqueueResponse(
        QStringLiteral("https://example.test/time/leap-seconds.list"),
        FakeNetworkResponse{.payload = wrongPayload, .manualFinish = true}
    );
    QSignalSpy preferredSpy(&fixture.workflow(), &SkyEphemerisUpdateWorkflow::preferredProfileSelected);

    QVERIFY(fixture.start());
    QList<FakeNetworkReply*> replies = fixture.network().issuedReplies();
    QCOMPARE(replies.size(), 1);
    replies.constFirst()->finishNow();
    QCoreApplication::processEvents();
    QCOMPARE(fixture.network().requestedUrls().size(), 2);

    replies = fixture.network().issuedReplies();
    QVERIFY(!replies.isEmpty());
    replies.constLast()->finishNow();
    QCoreApplication::processEvents();

    QCOMPARE(fixture.workflow().isUpdateInProgress(), false);
    QCOMPARE(fixture.workflow().updateProgress(), 0.0);
    QCOMPARE(
        fixture.workflow().statusText(),
        QString("Ephemeris data: Staged ephemeris asset checksum does not match manifest metadata.")
    );
    QCOMPARE(preferredSpy.count(), 0);
    QCOMPARE(fixture.manager().dataRevision(), originalRevision);
    QVERIFY(fixture.manager().usingInstalledData());
    QCOMPARE(fixture.manager().activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(fixture.store().loadEphemerisDataCache().installedKernelPath, oldKernelPath);
}

void SkyEphemerisUpdateWorkflowTests::spellPathSegmentTrimsWhitespace()
{
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(
            QStringLiteral(" \t de440s-short-range \n"), QStringLiteral("profile")
        ),
        QStringLiteral("de440s-short-range")
    );
}

void SkyEphemerisUpdateWorkflowTests::spellPathSegmentReplacesPunctuation()
{
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QStringLiteral("a/b:c d\\e-f_g.h"), QStringLiteral("profile")),
        QStringLiteral("a_b_c_d_e-f_g.h")
    );
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QStringLiteral("/"), QStringLiteral("profile")),
        QStringLiteral("_")
    );
}

void SkyEphemerisUpdateWorkflowTests::spellPathSegmentKeepsUnicodeLettersAndDigits()
{
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QString::fromUtf8("δ-Kerné ٢٠٢٦"), QStringLiteral("profile")),
        QString::fromUtf8("δ-Kerné_٢٠٢٦")
    );
}

void SkyEphemerisUpdateWorkflowTests::spellPathSegmentCollapsesRepeatedDots()
{
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QStringLiteral("a..b....c"), QStringLiteral("profile")),
        QStringLiteral("a.b.c")
    );
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QStringLiteral("a/..b"), QStringLiteral("profile")),
        QStringLiteral("a_.b")
    );
}

void SkyEphemerisUpdateWorkflowTests::spellPathSegmentFallsBackWhenEmpty()
{
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QString(), QStringLiteral("profile")), QStringLiteral("profile")
    );
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QStringLiteral(" \t\n "), QStringLiteral("update")),
        QStringLiteral("update")
    );
}

void SkyEphemerisUpdateWorkflowTests::spellPathSegmentFallsBackForSingleDot()
{
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QStringLiteral("."), QStringLiteral("profile")),
        QStringLiteral("profile")
    );
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QStringLiteral(" . "), QStringLiteral("update")),
        QStringLiteral("update")
    );
    QCOMPARE(
        SkyEphemerisPathSegmentPolicy::spellPathSegment(QStringLiteral("..."), QStringLiteral("update")),
        QStringLiteral("update")
    );
}

QTEST_GUILESS_MAIN(SkyEphemerisUpdateWorkflowTests)

#include "SkyEphemerisUpdateWorkflowTests.moc"
