#include "engine/highprecision/CalcephKernelProvider.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view kPayload = "SkyGate ephemeris bundled asset\nline two\n";
constexpr std::string_view kPayloadSha256 = "46886c2ebd5fefef66b3283a6f45f10cd4e1520fa1bc47816bfa3bb6b39eed98";

class TestEphemerisDataSnapshot final : public skygate::ephemeris::IEphemerisDataSnapshot {
public:
    explicit TestEphemerisDataSnapshot(std::optional<skygate::ephemeris::EphemerisKernelDataAsset> kernelAsset)
        : m_kernelAsset(std::move(kernelAsset))
    {
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisKernelDataAsset>
    solarSystemKernelAsset(std::string_view assetId) const override
    {
        if (!m_kernelAsset.has_value() || m_kernelAsset->id != assetId) {
            return std::nullopt;
        }

        return m_kernelAsset;
    }

private:
    std::optional<skygate::ephemeris::EphemerisKernelDataAsset> m_kernelAsset;
};

class FakeCalcephKernelHandle final : public skygate::ephemeris::highprecision::ICalcephKernelHandle {
public:
    explicit FakeCalcephKernelHandle(std::shared_ptr<int> closeCount) : m_closeCount(std::move(closeCount)) {}

    ~FakeCalcephKernelHandle() override
    {
        ++*m_closeCount;
    }

private:
    std::shared_ptr<int> m_closeCount;
};

class FakeCalcephKernelRuntime final : public skygate::ephemeris::highprecision::ICalcephKernelRuntime {
public:
    explicit FakeCalcephKernelRuntime(bool openSucceeds = true, bool available = true)
        : m_openSucceeds(openSucceeds), m_available(available)
    {
    }

    [[nodiscard]] bool isAvailable() const noexcept override
    {
        return m_available;
    }

    [[nodiscard]] skygate::ephemeris::highprecision::CalcephKernelOpenResult
    openKernel(const std::filesystem::path& path) const override
    {
        lastOpenedPath = path;
        ++openCount;
        if (!m_openSucceeds) {
            return {.diagnostic = "fake open failure"};
        }

        return {.handle = std::make_unique<FakeCalcephKernelHandle>(closeCount)};
    }

    mutable int openCount = 0;
    mutable std::filesystem::path lastOpenedPath;
    std::shared_ptr<int> closeCount = std::make_shared<int>(0);

private:
    bool m_openSucceeds = true;
    bool m_available = true;
};

[[nodiscard]] skygate::ephemeris::AstronomicalEpoch epochForDate(const int year, const int month, const int day)
{
    const auto epoch = skygate::ephemeris::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{.astronomicalYear = year, .month = month, .day = day}
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

[[nodiscard]] skygate::ephemeris::EphemerisDateRange makeRange(std::string id, const int startYear, const int endYear)
{
    return skygate::ephemeris::EphemerisDateRange{
        .id = std::move(id),
        .displayName = "Kernel range",
        .start = epochForDate(startYear, 1, 1),
        .end = epochForDate(endYear, 12, 31),
    };
}

[[nodiscard]] skygate::ephemeris::EphemerisDataManifestAsset
makeKernelAsset(std::string id, std::string profileId, const int startYear, const int endYear)
{
    skygate::ephemeris::EphemerisDataManifestAsset asset;
    asset.id = std::move(id);
    asset.kind = skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel;
    asset.profileId = std::move(profileId);
    asset.version = "2026a";
    asset.sourceUrl = "https://ssd.jpl.nasa.gov/ftp/eph/planets/bsp/test.bsp";
    asset.relativePath = "kernels/test.bsp";
    asset.checksum.algorithm = "sha256";
    asset.checksum.value = std::string{kPayloadSha256};
    asset.compression.kind = skygate::ephemeris::EphemerisDataManifestCompressionKind::None;
    asset.compression.uncompressedSizeBytes = kPayload.size();
    asset.validityRange = makeRange(asset.id + "-range", startYear, endYear);
    return asset;
}

[[nodiscard]] skygate::ephemeris::EphemerisDataManifest makeManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "unit-test";
    manifest.dataSetInfo.dateRanges.push_back(makeRange("product", -13200, 13200));
    manifest.profiles.push_back(skygate::ephemeris::EphemerisDataManifestProfile{
        .id = "modern",
        .displayName = "Modern",
        .bundled = true,
        .longRange = false,
        .assetIds = {"de440s-kernel"},
    });
    manifest.profiles.push_back(skygate::ephemeris::EphemerisDataManifestProfile{
        .id = "long-range",
        .displayName = "Long range",
        .bundled = false,
        .longRange = true,
        .assetIds = {"de441-kernel"},
    });
    manifest.assets.push_back(makeKernelAsset("de440s-kernel", "modern", 1550, 2650));
    skygate::ephemeris::EphemerisDataManifestAsset longRange =
        makeKernelAsset("de441-kernel", "long-range", -13200, 13200);
    longRange.optional = true;
    manifest.assets.push_back(std::move(longRange));
    return manifest;
}

void writeFile(const QString& path, const QByteArray& payload)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(payload), static_cast<qint64>(payload.size()));
}

[[nodiscard]] QString writeKernel(QTemporaryDir& root, QByteArray payload = {})
{
    Q_ASSERT(QDir(root.path()).mkpath(QStringLiteral("kernels")));
    const QString path = root.path() + QStringLiteral("/kernels/test.bsp");
    if (payload.isEmpty()) {
        payload = QByteArray(kPayload.data(), static_cast<qsizetype>(kPayload.size()));
    }
    writeFile(path, payload);
    return path;
}

[[nodiscard]] TestEphemerisDataSnapshot makeSnapshot(const QString& path, std::string assetId = "de440s-kernel")
{
    skygate::ephemeris::EphemerisKernelDataAsset asset;
    asset.id = std::move(assetId);
    asset.version = "installed-2026a";
    asset.provenance = "Installed test data";
    asset.activePath = path.toStdString();
    return TestEphemerisDataSnapshot(std::move(asset));
}

}  // namespace

class CalcephKernelProviderTests final : public QObject {
    Q_OBJECT

private slots:
    void opensSelectedModernKernelAndClosesIt();
    void selectsOptionalLongRangeProfileWhenRequested();
    void reportsUnavailableRuntime();
    void reportsMissingKernelAsset();
    void reportsMissingKernelFile();
    void rejectsChecksumMismatchBeforeOpening();
    void reportsOpenFailureForWrongKernelFile();
    void reportsOutOfRangeEpochs();
};

void CalcephKernelProviderTests::opensSelectedModernKernelAndClosesIt()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    const TestEphemerisDataSnapshot snapshot = makeSnapshot(kernelPath);
    const skygate::ephemeris::EphemerisDataManifest manifest = makeManifest();
    const auto runtime = std::make_shared<FakeCalcephKernelRuntime>();

    {
        const skygate::ephemeris::highprecision::CalcephKernelProvider provider(snapshot, manifest, {}, runtime);

        QCOMPARE(
            static_cast<std::uint8_t>(provider.status()),
            static_cast<std::uint8_t>(skygate::ephemeris::highprecision::CalcephKernelProviderStatus::Ready)
        );
        QVERIFY(provider.isReady());
        QVERIFY(provider.diagnostics().empty());
        QVERIFY(provider.kernelInfo().has_value());
        QVERIFY(provider.kernelInfo()->id == std::string{"de440s-kernel"});
        QVERIFY(provider.kernelInfo()->profileId == std::string{"modern"});
        QVERIFY(provider.kernelInfo()->version == std::string{"installed-2026a"});
        QVERIFY(provider.kernelInfo()->provenance == std::string{"Installed test data"});
        QCOMPARE(runtime->openCount, 1);
        QCOMPARE(QString::fromStdString(runtime->lastOpenedPath.generic_string()), kernelPath);
        QCOMPARE(*runtime->closeCount, 0);
    }

    QCOMPARE(*runtime->closeCount, 1);
}

void CalcephKernelProviderTests::selectsOptionalLongRangeProfileWhenRequested()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    const TestEphemerisDataSnapshot snapshot = makeSnapshot(kernelPath, "de441-kernel");
    skygate::ephemeris::highprecision::CalcephKernelSelectionOptions options;
    options.preferLongRange = true;

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(
        snapshot, makeManifest(), options, std::make_shared<FakeCalcephKernelRuntime>()
    );

    QVERIFY(provider.isReady());
    QVERIFY(provider.kernelInfo().has_value());
    QVERIFY(provider.kernelInfo()->id == std::string{"de441-kernel"});
    QVERIFY(provider.kernelInfo()->longRange);
    QVERIFY(provider.kernelInfo()->optional);
}

void CalcephKernelProviderTests::reportsUnavailableRuntime()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(
        makeSnapshot(kernelPath), makeManifest(), {}, std::make_shared<FakeCalcephKernelRuntime>(true, false)
    );

    QCOMPARE(
        static_cast<std::uint8_t>(provider.status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::CalcephKernelProviderStatus::CalcephUnavailable)
    );
    QVERIFY(!provider.diagnostics().empty());
}

void CalcephKernelProviderTests::reportsMissingKernelAsset()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    skygate::ephemeris::EphemerisDataManifest manifest = makeManifest();
    manifest.profiles.front().assetIds = {"leap-seconds"};
    skygate::ephemeris::EphemerisDataManifestAsset leapSecondAsset = manifest.assets.front();
    leapSecondAsset.id = "leap-seconds";
    leapSecondAsset.kind = skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable;
    manifest.assets.push_back(std::move(leapSecondAsset));

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(
        makeSnapshot(kernelPath), manifest, {}, std::make_shared<FakeCalcephKernelRuntime>()
    );

    QCOMPARE(
        static_cast<std::uint8_t>(provider.status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::CalcephKernelProviderStatus::MissingKernelAsset)
    );
    QVERIFY(!provider.diagnostics().empty());
}

void CalcephKernelProviderTests::reportsMissingKernelFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = root.path() + QStringLiteral("/kernels/missing.bsp");

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(
        makeSnapshot(kernelPath), makeManifest(), {}, std::make_shared<FakeCalcephKernelRuntime>()
    );

    QCOMPARE(
        static_cast<std::uint8_t>(provider.status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::CalcephKernelProviderStatus::MissingKernelFile)
    );
    QVERIFY(!provider.diagnostics().empty());
}

void CalcephKernelProviderTests::rejectsChecksumMismatchBeforeOpening()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root, QByteArray(static_cast<qsizetype>(kPayload.size()), 'x'));
    const auto runtime = std::make_shared<FakeCalcephKernelRuntime>();

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(
        makeSnapshot(kernelPath), makeManifest(), {}, runtime
    );

    QCOMPARE(
        static_cast<std::uint8_t>(provider.status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::CalcephKernelProviderStatus::ChecksumMismatch)
    );
    QCOMPARE(runtime->openCount, 0);
    QVERIFY(!provider.diagnostics().empty());
}

void CalcephKernelProviderTests::reportsOpenFailureForWrongKernelFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(
        makeSnapshot(kernelPath), makeManifest(), {}, std::make_shared<FakeCalcephKernelRuntime>(false)
    );

    QCOMPARE(
        static_cast<std::uint8_t>(provider.status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::CalcephKernelProviderStatus::OpenFailed)
    );
    QVERIFY(!provider.diagnostics().empty());
}

void CalcephKernelProviderTests::reportsOutOfRangeEpochs()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(
        makeSnapshot(kernelPath), makeManifest(), {}, std::make_shared<FakeCalcephKernelRuntime>()
    );

    QCOMPARE(
        static_cast<std::uint8_t>(provider.statusForEpoch(epochForDate(2000, 1, 1))),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::CalcephKernelProviderStatus::Ready)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(provider.statusForEpoch(epochForDate(3000, 1, 1))),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::CalcephKernelProviderStatus::OutOfRange)
    );
}

QTEST_APPLESS_MAIN(CalcephKernelProviderTests)

#include "CalcephKernelProviderTests.moc"
