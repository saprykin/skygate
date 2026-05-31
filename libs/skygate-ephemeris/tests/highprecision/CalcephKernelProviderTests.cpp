#include "time/CalendarTime.hpp"
#include "engine/highprecision/CalcephKernel.hpp"
#include "engine/highprecision/CalcephKernelProvider.hpp"
#include "engine/highprecision/EphemerisDataManifest.hpp"
#include "engine/highprecision/EphemerisDataSnapshot.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

constexpr std::string_view kPayload = "SkyGate ephemeris bundled asset\nline two\n";
constexpr std::string_view kPayloadSha256 = "46886c2ebd5fefef66b3283a6f45f10cd4e1520fa1bc47816bfa3bb6b39eed98";

static_assert(!std::is_move_constructible_v<skygate::ephemeris::highprecision::CalcephKernel>);
static_assert(!std::is_move_assignable_v<skygate::ephemeris::highprecision::CalcephKernel>);

[[nodiscard]] skygate::ephemeris::highprecision::ICalcephKernel::Status expectedSelectedKernelOpenStatus() noexcept
{
    return skygate::ephemeris::highprecision::ICalcephKernel::Status::OpenFailed;
}

class TestEphemerisDataSnapshot final : public skygate::ephemeris::IEphemerisDataSnapshot {
public:
    explicit TestEphemerisDataSnapshot(
        std::optional<skygate::ephemeris::EphemerisKernelDataAsset> kernelAsset,
        const bool requireRequestedAssetId = true
    )
        : m_kernelAsset(std::move(kernelAsset)), m_requireRequestedAssetId(requireRequestedAssetId)
    {
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisKernelDataAsset>
    solarSystemKernelAsset(std::string_view assetId) const override
    {
        if (!m_kernelAsset.has_value() || (m_requireRequestedAssetId && m_kernelAsset->id != std::string{assetId})) {
            return std::nullopt;
        }

        return m_kernelAsset;
    }

private:
    std::optional<skygate::ephemeris::EphemerisKernelDataAsset> m_kernelAsset;
    bool m_requireRequestedAssetId = true;
};

[[nodiscard]] skygate::ephemeris::AstronomicalEpoch epochForDate(const int year, const int month, const int day)
{
    const auto epoch = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
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
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifestProfile{
            .id = "modern",
            .displayName = "Modern",
            .bundled = true,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifestProfile{
            .id = "long-range",
            .displayName = "Long range",
            .bundled = false,
            .longRange = true,
            .assetIds = {"de441-kernel"},
        }
    );
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
    if (!QDir(root.path()).mkpath(QStringLiteral("kernels"))) {
        qFatal("Unable to create temporary kernel test directory.");
    }
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
    asset.profileId = asset.id == "de441-kernel" ? "long-range" : "modern";
    asset.version = "installed-2026a";
    asset.provenance = "Installed test data";
    asset.activePath = path.toStdString();
    return TestEphemerisDataSnapshot(std::move(asset));
}

[[nodiscard]] std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel>
openProvidedKernel(const skygate::ephemeris::highprecision::CalcephKernelProvider& provider)
{
    return provider.openKernel();
}

}  // namespace

class CalcephKernelProviderTests final : public QObject {
    Q_OBJECT

private slots:
    void opensSelectedModernKernel();
    void defersKernelFileChecksUntilOpenKernel();
    void selectsOptionalLongRangeProfileWhenRequested();
    void reportsMissingKernelAsset();
    void reportsMissingKernelFile();
    void reportsMissingKernelWhenSnapshotDoesNotContainSelectedAsset();
    void reportsMissingKernelWhenPreferredProfileAssetIsAbsent();
    void usesActiveSnapshotKernelWhenDefaultProfileAssetIsAbsent();
    void reportsMissingKernelWhenExplicitProfileSnapshotMismatches();
    void rejectsChecksumMismatchBeforeOpening();
    void reportsSelectedKernelOpenFailure();
    void rejectsNonTdbEpochsBeforeCallingKernel();
    void keepsSelectedMetadataWhenOpenFails();
};

void CalcephKernelProviderTests::opensSelectedModernKernel()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    const TestEphemerisDataSnapshot snapshot = makeSnapshot(kernelPath);
    const skygate::ephemeris::EphemerisDataManifest manifest = makeManifest();
    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(snapshot, manifest);
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()), static_cast<std::uint8_t>(expectedSelectedKernelOpenStatus())
    );
    QVERIFY(!kernel->diagnostics().empty());
    QVERIFY(kernel->kernelInfo().has_value());
    QVERIFY(kernel->kernelInfo()->id == std::string{"de440s-kernel"});
    QVERIFY(kernel->kernelInfo()->profileId == std::string{"modern"});
    QVERIFY(kernel->kernelInfo()->version == std::string{"installed-2026a"});
    QVERIFY(kernel->kernelInfo()->provenance == std::string{"Installed test data"});
    QCOMPARE(QString::fromStdString(kernel->kernelInfo()->activePath.generic_string()), kernelPath);
}

void CalcephKernelProviderTests::defersKernelFileChecksUntilOpenKernel()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = root.path() + QStringLiteral("/kernels/test.bsp");
    const TestEphemerisDataSnapshot snapshot = makeSnapshot(kernelPath);
    const skygate::ephemeris::EphemerisDataManifest manifest = makeManifest();

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(snapshot, manifest);
    const QString writtenKernelPath = writeKernel(root);
    QCOMPARE(writtenKernelPath, kernelPath);

    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()), static_cast<std::uint8_t>(expectedSelectedKernelOpenStatus())
    );
    QVERIFY(kernel->kernelInfo().has_value());
}

void CalcephKernelProviderTests::selectsOptionalLongRangeProfileWhenRequested()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    const TestEphemerisDataSnapshot snapshot = makeSnapshot(kernelPath, "de441-kernel");
    skygate::ephemeris::highprecision::CalcephKernelProvider::Options options;
    options.preferLongRange = true;

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(snapshot, makeManifest(), options);
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()), static_cast<std::uint8_t>(expectedSelectedKernelOpenStatus())
    );
    QVERIFY(kernel->kernelInfo().has_value());
    QVERIFY(kernel->kernelInfo()->id == std::string{"de441-kernel"});
    QVERIFY(kernel->kernelInfo()->longRange);
    QVERIFY(kernel->kernelInfo()->optional);
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

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(makeSnapshot(kernelPath), manifest);
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::ICalcephKernel::Status::MissingKernelAsset)
    );
    QVERIFY(!kernel->diagnostics().empty());
}

void CalcephKernelProviderTests::reportsMissingKernelFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = root.path() + QStringLiteral("/kernels/missing.bsp");

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(makeSnapshot(kernelPath), makeManifest());
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::ICalcephKernel::Status::MissingKernelFile)
    );
    QVERIFY(!kernel->diagnostics().empty());
}

void CalcephKernelProviderTests::reportsMissingKernelWhenSnapshotDoesNotContainSelectedAsset()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    skygate::ephemeris::highprecision::CalcephKernelProvider::Options options;
    options.preferLongRange = true;

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(
        makeSnapshot(kernelPath), makeManifest(), options
    );
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::ICalcephKernel::Status::MissingKernelFile)
    );
    QVERIFY(!kernel->diagnostics().empty());
}

void CalcephKernelProviderTests::reportsMissingKernelWhenPreferredProfileAssetIsAbsent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    skygate::ephemeris::highprecision::CalcephKernelProvider::Options options;
    options.preferredProfileId = "long-range";

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(
        makeSnapshot(kernelPath), makeManifest(), options
    );
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::ICalcephKernel::Status::MissingKernelFile)
    );
    QVERIFY(!kernel->diagnostics().empty());
}

void CalcephKernelProviderTests::usesActiveSnapshotKernelWhenDefaultProfileAssetIsAbsent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    skygate::ephemeris::EphemerisKernelDataAsset asset;
    asset.id = "de441-kernel";
    asset.profileId = "long-range";
    asset.version = "installed-2026a";
    asset.provenance = "Installed test data";
    asset.activePath = kernelPath.toStdString();
    const TestEphemerisDataSnapshot snapshot(std::move(asset), false);

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(snapshot, makeManifest());
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()), static_cast<std::uint8_t>(expectedSelectedKernelOpenStatus())
    );
    QVERIFY(kernel->kernelInfo().has_value());
    QVERIFY(kernel->kernelInfo()->id == std::string{"de441-kernel"});
    QVERIFY(kernel->kernelInfo()->profileId == std::string{"long-range"});
    QVERIFY(kernel->kernelInfo()->longRange);
}

void CalcephKernelProviderTests::reportsMissingKernelWhenExplicitProfileSnapshotMismatches()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    skygate::ephemeris::EphemerisKernelDataAsset asset;
    asset.id = "de440s-kernel";
    asset.profileId = "long-range";
    asset.version = "installed-2026a";
    asset.provenance = "Installed test data";
    asset.activePath = kernelPath.toStdString();
    const TestEphemerisDataSnapshot snapshot(std::move(asset), false);
    skygate::ephemeris::highprecision::CalcephKernelProvider::Options options;
    options.preferredProfileId = "modern";

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(snapshot, makeManifest(), options);
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::ICalcephKernel::Status::MissingKernelFile)
    );
    QVERIFY(!kernel->diagnostics().empty());
}

void CalcephKernelProviderTests::rejectsChecksumMismatchBeforeOpening()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root, QByteArray(static_cast<qsizetype>(kPayload.size()), 'x'));

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(makeSnapshot(kernelPath), makeManifest());
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()),
        static_cast<std::uint8_t>(skygate::ephemeris::highprecision::ICalcephKernel::Status::ChecksumMismatch)
    );
    QVERIFY(!kernel->diagnostics().empty());
}

void CalcephKernelProviderTests::reportsSelectedKernelOpenFailure()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);

    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(makeSnapshot(kernelPath), makeManifest());
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    QCOMPARE(
        static_cast<std::uint8_t>(kernel->status()), static_cast<std::uint8_t>(expectedSelectedKernelOpenStatus())
    );
    QVERIFY(!kernel->diagnostics().empty());
}

void CalcephKernelProviderTests::rejectsNonTdbEpochsBeforeCallingKernel()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);
    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(makeSnapshot(kernelPath), makeManifest());
    const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
        openProvidedKernel(provider);

    const skygate::ephemeris::AstronomicalEpoch utcEpoch = epochForDate(2000, 1, 1);
    const skygate::ephemeris::highprecision::SolarSystemKernelStateResult result = kernel->compute(utcEpoch, 499, 399);

    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(result.metadata.hasWarning(skygate::ephemeris::EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(!result.positionAu.has_value());
}

void CalcephKernelProviderTests::keepsSelectedMetadataWhenOpenFails()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString kernelPath = writeKernel(root);

    std::optional<skygate::ephemeris::highprecision::ICalcephKernel::Info> kernelInfo;
    const skygate::ephemeris::highprecision::CalcephKernelProvider provider(makeSnapshot(kernelPath), makeManifest());
    {
        const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
            openProvidedKernel(provider);
        kernelInfo = kernel->kernelInfo();
    }

    QVERIFY(kernelInfo.has_value());
    QVERIFY(kernelInfo->provenance == std::string{"Installed test data"});
    QVERIFY(kernelInfo->validityRange.id == std::string{"de440s-kernel-range"});
}

QTEST_APPLESS_MAIN(CalcephKernelProviderTests)

#include "CalcephKernelProviderTests.moc"
