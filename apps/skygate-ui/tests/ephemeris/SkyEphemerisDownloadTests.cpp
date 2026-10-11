#include "EphemerisDataCacheSnapshotFixture.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkyEphemerisDownloadService.hpp"
#include "SkySettingsStore.hpp"
#include "StagedEphemerisAssetFixture.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest/QtTest>

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

using skygate::ui::tests::EphemerisDataCacheSnapshotFixture;
using skygate::ui::tests::SettingsTestFixture;
using skygate::ui::tests::StagedEphemerisAssetFixture;

namespace {

constexpr std::string_view kPayload = StagedEphemerisAssetFixture::kPayload;

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

class SkyEphemerisDownloadTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void stagesAssetFromSourceUrl();
    void asyncNetworkDownloadStagesWithoutBlocking();
    void rejectsPlainHttpSourceUrls();
    void rejectsOversizedNetworkStaging();
    void cancelDuringDownloadKeepsStagingAndActiveData();

private:
    SettingsTestFixture m_settings;
};

void SkyEphemerisDownloadTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyEphemerisDownloadTests")));
}

void SkyEphemerisDownloadTests::init()
{
    m_settings.clearSettings();
}

void SkyEphemerisDownloadTests::stagesAssetFromSourceUrl()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);

    QTemporaryDir sourceRoot;
    QTemporaryDir stagedRoot;
    QVERIFY(sourceRoot.isValid());
    QVERIFY(stagedRoot.isValid());
    const QString sourcePath = sourceRoot.path() + QStringLiteral("/source-kernel.bsp");
    const QByteArray payload(kPayload.data(), static_cast<qsizetype>(kPayload.size()));
    QVERIFY(writeFile(sourcePath, payload));

    skygate::ephemeris::EphemerisDataManifest::Asset asset = StagedEphemerisAssetFixture::stagedAsset(
        "download-kernel", skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel, "kernels/de440s.bsp"
    );
    asset.sourceUrl = QUrl::fromLocalFile(sourcePath).toString().toStdString();

    SkyEphemerisDataManager::StagedUpdateDownloadRequest request;
    request.asset = &asset;
    request.stagedResourceRoot = stagedRoot.path();
    std::uint64_t progressBytes = 0U;
    std::optional<std::uint64_t> progressTotal;
    request.progressHandler = [&progressBytes, &progressTotal](
                                  const std::uint64_t stagedBytes, const std::optional<std::uint64_t> totalBytes
                              ) {
        progressBytes = stagedBytes;
        progressTotal = totalBytes;
    };

    const SkyEphemerisDataManager::StagedUpdateDownloadResult result = manager.stageEphemerisUpdateAsset(request);

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QCOMPARE(result.stagedPath, stagedRoot.path() + QStringLiteral("/kernels/de440s.bsp"));
    QCOMPARE(result.stagedBytes, static_cast<std::uint64_t>(payload.size()));
    QCOMPARE(progressBytes, static_cast<std::uint64_t>(payload.size()));
    QVERIFY(progressTotal.has_value());
    QCOMPARE(*progressTotal, static_cast<std::uint64_t>(payload.size()));

    QFile stagedFile(result.stagedPath);
    QVERIFY(stagedFile.open(QIODevice::ReadOnly));
    QCOMPARE(stagedFile.readAll(), payload);
}

void SkyEphemerisDownloadTests::asyncNetworkDownloadStagesWithoutBlocking()
{
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    SkyEphemerisDownloadService service(&networkAccessManager);

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const QByteArray payload(kPayload.data(), static_cast<qsizetype>(kPayload.size()));

    skygate::ephemeris::EphemerisDataManifest::Asset asset = StagedEphemerisAssetFixture::stagedAsset(
        "download-kernel", skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel, "kernels/de440s.bsp"
    );
    const QString sourceUrl = QStringLiteral("https://example.test/kernels/de440s.bsp");
    asset.sourceUrl = sourceUrl.toStdString();
    networkAccessManager.enqueueResponse(sourceUrl, {.payload = payload});

    SkyEphemerisDownloadService::StagedUpdateDownloadRequest request;
    request.asset = &asset;
    request.sourceUrl = sourceUrl;
    request.stagedResourceRoot = stagedRoot.path();

    bool completed = false;
    SkyEphemerisDownloadService::StagedUpdateDownloadResult result;
    service.stageAsync(
        request, [&completed, &result](SkyEphemerisDownloadService::StagedUpdateDownloadResult stagedResult) {
            completed = true;
            result = std::move(stagedResult);
        }
    );

    QTRY_VERIFY_WITH_TIMEOUT(completed, 5000);
    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QCOMPARE(result.stagedBytes, static_cast<std::uint64_t>(payload.size()));
    QCOMPARE(networkAccessManager.requestedUrls().size(), 1);
    QCOMPARE(networkAccessManager.requestedUrls().constFirst(), sourceUrl);

    QFile stagedFile(result.stagedPath);
    QVERIFY(stagedFile.open(QIODevice::ReadOnly));
    QCOMPARE(stagedFile.readAll(), payload);
}

void SkyEphemerisDownloadTests::rejectsPlainHttpSourceUrls()
{
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    SkyEphemerisDownloadService service(&networkAccessManager);

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());

    skygate::ephemeris::EphemerisDataManifest::Asset asset = StagedEphemerisAssetFixture::stagedAsset(
        "download-kernel", skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel, "kernels/de440s.bsp"
    );
    const QString sourceUrl = QStringLiteral("http://example.test/kernels/de440s.bsp");
    asset.sourceUrl = sourceUrl.toStdString();

    SkyEphemerisDownloadService::StagedUpdateDownloadRequest request;
    request.asset = &asset;
    request.sourceUrl = sourceUrl;
    request.stagedResourceRoot = stagedRoot.path();

    bool completed = false;
    SkyEphemerisDownloadService::StagedUpdateDownloadResult result;
    service.stageAsync(
        request, [&completed, &result](SkyEphemerisDownloadService::StagedUpdateDownloadResult stagedResult) {
            completed = true;
            result = std::move(stagedResult);
        }
    );

    QTRY_VERIFY_WITH_TIMEOUT(completed, 5000);
    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDownloadService::StagedUpdateDownloadStatus::MissingSource)
    );
    QVERIFY(!result.diagnostics.empty());
    QVERIFY(result.diagnostics.front().contains(QStringLiteral("HTTPS")));
    QVERIFY(networkAccessManager.requestedUrls().isEmpty());
}

void SkyEphemerisDownloadTests::rejectsOversizedNetworkStaging()
{
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    SkyEphemerisDownloadService service(&networkAccessManager);

    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());

    skygate::ephemeris::EphemerisDataManifest::Asset asset = StagedEphemerisAssetFixture::stagedAsset(
        "download-kernel", skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel, "kernels/de440s.bsp"
    );
    const QString sourceUrl = QStringLiteral("https://example.test/kernels/de440s.bsp");
    asset.sourceUrl = sourceUrl.toStdString();

    const std::uint64_t declaredBytes = static_cast<std::uint64_t>(kPayload.size());
    const QByteArray oversizedPayload(static_cast<qsizetype>(declaredBytes + (2ULL << 20U)), 'x');
    networkAccessManager.enqueueResponse(sourceUrl, {.payload = oversizedPayload});

    SkyEphemerisDownloadService::StagedUpdateDownloadRequest request;
    request.asset = &asset;
    request.sourceUrl = sourceUrl;
    request.stagedResourceRoot = stagedRoot.path();

    bool completed = false;
    SkyEphemerisDownloadService::StagedUpdateDownloadResult result;
    service.stageAsync(
        request, [&completed, &result](SkyEphemerisDownloadService::StagedUpdateDownloadResult stagedResult) {
            completed = true;
            result = std::move(stagedResult);
        }
    );

    QTRY_VERIFY_WITH_TIMEOUT(completed, 5000);
    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDownloadService::StagedUpdateDownloadStatus::IoError)
    );
    QVERIFY(!result.diagnostics.empty());
    QVERIFY(result.diagnostics.front().contains(QStringLiteral("declared staging size")));
    QCOMPARE(result.stagedBytes, std::uint64_t{0});
    QVERIFY(QFileInfo::exists(result.stagedPath));
    QCOMPARE(QFileInfo(result.stagedPath).size(), qint64{0});
}

void SkyEphemerisDownloadTests::cancelDuringDownloadKeepsStagingAndActiveData()
{
    const QString oldKernelPath = m_settings.filePath(QStringLiteral("download-cancel-old-kernel.bsp"));
    QVERIFY(writeFile(oldKernelPath, QByteArrayLiteral("old kernel")));

    SkySettingsStore store;
    const SkySettingsStore::EphemerisDataCacheSnapshot oldSnapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(oldKernelPath, QString());
    QVERIFY(store.saveEphemerisDataCache(oldSnapshot));
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();

    QTemporaryDir sourceRoot;
    QTemporaryDir stagedRoot;
    QVERIFY(sourceRoot.isValid());
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest::Asset asset = StagedEphemerisAssetFixture::stagedAsset(
        "download-kernel", skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel, "kernels/de440s.bsp"
    );
    const QByteArray payload(200000, 'x');
    const QString sourcePath = sourceRoot.path() + QStringLiteral("/kernels/de440s.bsp");
    QVERIFY(writeFile(sourcePath, payload));

    int cancellationChecks = 0;
    SkyEphemerisDataManager::StagedUpdateDownloadRequest request;
    request.asset = &asset;
    request.sourceResourceRoot = sourceRoot.path();
    request.stagedResourceRoot = stagedRoot.path();
    request.cancellationRequested = [&cancellationChecks] {
        ++cancellationChecks;
        return cancellationChecks >= 3;
    };

    const SkyEphemerisDataManager::StagedUpdateDownloadResult result = manager.stageEphemerisUpdateAsset(request);

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(SkyEphemerisDataManager::StagedUpdateDownloadStatus::Canceled)
    );
    QCOMPARE(manager.dataRevision(), originalRevision);
    QCOMPARE(manager.activeCacheSnapshot().installedKernelPath, oldKernelPath);
    QCOMPARE(store.loadEphemerisDataCache().installedKernelPath, oldKernelPath);
    QVERIFY(QFileInfo::exists(result.stagedPath));
    const qint64 stagedSize = QFileInfo(result.stagedPath).size();
    QVERIFY(stagedSize > 0);
    QVERIFY(stagedSize < payload.size());
}

QTEST_GUILESS_MAIN(SkyEphemerisDownloadTests)

#include "SkyEphemerisDownloadTests.moc"
