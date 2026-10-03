#include "composition/EphemerisBackendResourceCache.hpp"
#include "StaticCalcephKernelProvider.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/EphemerisKernelDataAsset.hpp"
#include "engine/EphemerisTextDataAsset.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "engine/highprecision/ICalcephKernel.hpp"
#include "factory/EphemerisEngineFactoryRequest.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {

class TestEphemerisDataSnapshot final : public skygate::ephemeris::IEphemerisDataSnapshot {
public:
    TestEphemerisDataSnapshot(
        std::optional<skygate::ephemeris::EphemerisTextDataAsset> leapSecondTable = std::nullopt,
        std::optional<skygate::ephemeris::EphemerisTextDataAsset> earthOrientationData = std::nullopt,
        std::optional<skygate::ephemeris::EphemerisTextDataAsset> deltaTData = std::nullopt,
        std::optional<skygate::ephemeris::EphemerisKernelDataAsset> kernelAsset = std::nullopt
    )
        : m_leapSecondTable(std::move(leapSecondTable)), m_earthOrientationData(std::move(earthOrientationData)),
          m_deltaTData(std::move(deltaTData)), m_kernelAsset(std::move(kernelAsset))
    {
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        return m_leapSecondTable;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> earthOrientationDataAsset() const override
    {
        return m_earthOrientationData;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> deltaTDataAsset() const override
    {
        return m_deltaTData;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisKernelDataAsset>
    solarSystemKernelAsset(std::string_view assetId) const override
    {
        if (!m_kernelAsset.has_value() || m_kernelAsset->id != std::string{assetId}) {
            return std::nullopt;
        }

        return m_kernelAsset;
    }

private:
    std::optional<skygate::ephemeris::EphemerisTextDataAsset> m_leapSecondTable;
    std::optional<skygate::ephemeris::EphemerisTextDataAsset> m_earthOrientationData;
    std::optional<skygate::ephemeris::EphemerisTextDataAsset> m_deltaTData;
    std::optional<skygate::ephemeris::EphemerisKernelDataAsset> m_kernelAsset;
};

class FakeTimeScaleService final : public skygate::ephemeris::ITimeScaleService {
public:
    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult
    convert(const skygate::core::AstronomicalEpoch&, skygate::core::TimeScale) const override
    {
        return {};
    }

    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult
    convertCivilDateTime(const skygate::core::CivilDateTime&, skygate::core::TimeScale) const override
    {
        return {};
    }
};

class FakeEarthOrientationProvider final : public skygate::ephemeris::IEarthOrientationProvider {
public:
    [[nodiscard]] const DataInfo& dataInfo() const noexcept override
    {
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const TableEntry> entries() const noexcept override
    {
        return {};
    }

private:
    DataInfo m_dataInfo;
};

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeAsset(std::string id, std::string content)
{
    return {
        .id = std::move(id),
        .version = "test-version",
        .provenance = "test",
        .content = std::move(content),
    };
}

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeValidLeapSecondTable()
{
    return makeAsset(
        "leap-seconds",
        "# IANA leap-second file sample\n"
        "# File expires on 28 December 2026\n"
        "#@ 4007404800\n"
        "#NTP Time      DTAI    Day Month Year\n"
        "2272060800     10      # 1 Jan 1972\n"
        "3692217600     37      # 1 Jan 2017\n"
    );
}

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeValidEarthOrientationData()
{
    return makeAsset(
        "earth-orientation",
        "EARTH ORIENTATION PARAMETER (EOP) PRODUCT CENTER CENTER (PARIS OBSERVATORY)\n"
        "Date      MJD      x          y        UT1-UTC       LOD\n"
        "(0h UTC)\n"
        "2026  5  1  60431   0.112300   0.218700   0.0314200   0.001723\n"
        "2026  5  2  60432   0.118000   0.221000   0.0345000   0.001669\n"
    );
}

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeValidDeltaTData()
{
    return makeAsset(
        "delta-t",
        "1900  1  1  -2.7200\n"
        "2000  1  1  63.8300\n"
        "2026  1  1  69.2000\n"
    );
}

[[nodiscard]] std::shared_ptr<const TestEphemerisDataSnapshot> makeSnapshot()
{
    return std::make_shared<const TestEphemerisDataSnapshot>(
        makeValidLeapSecondTable(), makeValidEarthOrientationData(), makeValidDeltaTData()
    );
}

[[nodiscard]] skygate::ephemeris::EphemerisDataManifest makeKernelManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "unit-test";
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "modern",
            .displayName = "Modern",
            .bundled = true,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );
    skygate::ephemeris::EphemerisDataManifest::Asset asset;
    asset.id = "de440s-kernel";
    asset.kind = skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel;
    asset.profileId = "modern";
    asset.version = "installed-2026a";
    asset.relativePath = "kernels/test.bsp";
    asset.checksum.algorithm = "sha256";
    asset.checksum.value = std::string(64U, '0');
    asset.compression.kind = skygate::ephemeris::EphemerisDataManifest::CompressionKind::None;
    asset.compression.uncompressedSizeBytes = 1U;
    manifest.assets.push_back(std::move(asset));
    return manifest;
}

[[nodiscard]] std::shared_ptr<const TestEphemerisDataSnapshot> makeKernelSnapshot(const QString& kernelPath)
{
    skygate::ephemeris::EphemerisKernelDataAsset asset;
    asset.id = "de440s-kernel";
    asset.profileId = "modern";
    asset.version = "installed-2026a";
    asset.provenance = "Installed test data";
    asset.activePath = kernelPath.toStdString();
    return std::make_shared<const TestEphemerisDataSnapshot>(
        std::nullopt, std::nullopt, std::nullopt, std::move(asset)
    );
}

}  // namespace

class EphemerisBackendResourceCacheTests final : public QObject {
    Q_OBJECT

private slots:
    void reusesProvidersWhenRevisionAndManifestAreUnchanged();
    void invalidatesAndReloadsProvidersWhenRevisionChanges();
    void preservesInjectedProvidersWhenNoManagedProviderWasLoaded();
    void assemblesProviderFieldsIntoFactoryRequest();
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    void cachedKernelProviderSkipsChecksumVerification();
#endif
};

void EphemerisBackendResourceCacheTests::reusesProvidersWhenRevisionAndManifestAreUnchanged()
{
    const std::shared_ptr<const TestEphemerisDataSnapshot> snapshot = makeSnapshot();
    skygate::ephemeris::EphemerisBackendResourceCache cache;
    cache.refresh(snapshot, nullptr, 7U);

    const auto firstTimeScaleService = cache.timeScaleService();
    const auto firstEarthOrientationProvider = cache.earthOrientationProvider();
    QVERIFY(firstTimeScaleService != nullptr);
    QVERIFY(firstEarthOrientationProvider != nullptr);
    QVERIFY(cache.calcephKernelProvider() == nullptr);

    cache.refresh(snapshot, nullptr, 7U);
    QVERIFY(cache.timeScaleService() == firstTimeScaleService);
    QVERIFY(cache.earthOrientationProvider() == firstEarthOrientationProvider);
}

void EphemerisBackendResourceCacheTests::invalidatesAndReloadsProvidersWhenRevisionChanges()
{
    const std::shared_ptr<const TestEphemerisDataSnapshot> snapshot = makeSnapshot();
    skygate::ephemeris::EphemerisBackendResourceCache cache;
    cache.refresh(snapshot, nullptr, 7U);

    const auto firstTimeScaleService = cache.timeScaleService();
    const auto firstEarthOrientationProvider = cache.earthOrientationProvider();
    QVERIFY(firstTimeScaleService != nullptr);
    QVERIFY(firstEarthOrientationProvider != nullptr);

    cache.refresh(snapshot, nullptr, 8U);
    QVERIFY(cache.timeScaleService() != nullptr);
    QVERIFY(cache.earthOrientationProvider() != nullptr);
    QVERIFY(cache.timeScaleService() != firstTimeScaleService);
    QVERIFY(cache.earthOrientationProvider() != firstEarthOrientationProvider);
}

void EphemerisBackendResourceCacheTests::preservesInjectedProvidersWhenNoManagedProviderWasLoaded()
{
    const std::shared_ptr<const TestEphemerisDataSnapshot> snapshot = makeSnapshot();
    const auto injectedTimeScaleService = std::make_shared<FakeTimeScaleService>();
    const auto injectedEarthOrientationProvider = std::make_shared<FakeEarthOrientationProvider>();
    const auto injectedCalcephKernelProvider =
        std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>();

    skygate::ephemeris::EphemerisBackendResourceCache cache(
        skygate::ephemeris::EphemerisBackendResourceCache::InitialProviders{
            .timeScaleService = injectedTimeScaleService,
            .earthOrientationProvider = injectedEarthOrientationProvider,
            .calcephKernelProvider = injectedCalcephKernelProvider,
        }
    );
    cache.refresh(snapshot, nullptr, 1U);
    QVERIFY(cache.timeScaleService() == injectedTimeScaleService);
    QVERIFY(cache.earthOrientationProvider() == injectedEarthOrientationProvider);
    QVERIFY(cache.calcephKernelProvider() == injectedCalcephKernelProvider);

    cache.refresh(snapshot, nullptr, 2U);
    QVERIFY(cache.timeScaleService() == injectedTimeScaleService);
    QVERIFY(cache.earthOrientationProvider() == injectedEarthOrientationProvider);
    QVERIFY(cache.calcephKernelProvider() == injectedCalcephKernelProvider);
}

void EphemerisBackendResourceCacheTests::assemblesProviderFieldsIntoFactoryRequest()
{
    const std::shared_ptr<const TestEphemerisDataSnapshot> snapshot = makeSnapshot();
    const auto injectedCalcephKernelProvider =
        std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>();

    skygate::ephemeris::EphemerisBackendResourceCache cache(
        skygate::ephemeris::EphemerisBackendResourceCache::InitialProviders{
            .timeScaleService = nullptr,
            .earthOrientationProvider = nullptr,
            .calcephKernelProvider = injectedCalcephKernelProvider,
        }
    );
    cache.refresh(snapshot, nullptr, 3U);

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    cache.applyProviderFields(request);

    QVERIFY(request.timeScaleService == cache.timeScaleService());
    QVERIFY(request.earthOrientationProvider == cache.earthOrientationProvider());
    QVERIFY(request.calcephKernelProvider == injectedCalcephKernelProvider);
}

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
void EphemerisBackendResourceCacheTests::cachedKernelProviderSkipsChecksumVerification()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = root.path() + QStringLiteral("/kernels/test.bsp");
    QVERIFY(QDir(root.path()).mkpath(QStringLiteral("kernels")));
    QFile kernelFile(kernelPath);
    QVERIFY(kernelFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray payload = QByteArrayLiteral("SkyGate ephemeris bundled asset\nline two\n");
    QCOMPARE(kernelFile.write(payload), static_cast<qint64>(payload.size()));
    kernelFile.close();

    const std::shared_ptr<const TestEphemerisDataSnapshot> snapshot = makeKernelSnapshot(kernelPath);
    const skygate::ephemeris::EphemerisDataManifest manifest = makeKernelManifest();
    skygate::ephemeris::EphemerisBackendResourceCache cache(
        skygate::ephemeris::EphemerisBackendResourceCache::InitialProviders{
            .timeScaleService = std::make_shared<FakeTimeScaleService>(),
            .earthOrientationProvider = std::make_shared<FakeEarthOrientationProvider>(),
            .calcephKernelProvider = nullptr,
        }
    );
    cache.refresh(snapshot, &manifest, 5U);

    const auto kernelProvider = cache.calcephKernelProvider();
    QVERIFY(kernelProvider != nullptr);
    const auto kernel = kernelProvider->openKernel();
    QVERIFY(kernel != nullptr);
    QVERIFY(kernel->status() != skygate::ephemeris::highprecision::ICalcephKernel::Status::ChecksumMismatch);
}
#endif

QTEST_APPLESS_MAIN(EphemerisBackendResourceCacheTests)
#include "EphemerisBackendResourceCacheTests.moc"
