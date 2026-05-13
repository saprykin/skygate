#include "skygate/ephemeris/EphemerisDataManifest.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <string>
#include <string_view>

class EphemerisDataManifestTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesValidManifest();
    void rejectsMalformedManifest();
    void rejectsMissingRequiredFields();
    void parsesChecksumAndCompressionMetadata();
    void parsesValidityRanges();
    void distinguishesModernAndOptionalDe441Profiles();
    void rejectsUnknownProfileAssetReferences();
};

namespace {

[[nodiscard]] std::string validManifestPayload()
{
    return R"({
        "schemaVersion": 1,
        "id": "skygate-high-precision-2026a",
        "displayName": "SkyGate high-precision ephemeris data",
        "version": "2026a",
        "provenance": "SkyGate bundled data manifest",
        "dateRanges": [
            {
                "id": "product-range",
                "displayName": "Product range",
                "start": "-13200-01-01",
                "end": "13200-12-31"
            }
        ],
        "profiles": [
            {
                "id": "modern",
                "displayName": "Bundled modern data",
                "bundled": true,
                "longRange": false,
                "assetIds": [
                    "de440s-kernel",
                    "leap-seconds",
                    "earth-orientation",
                    "delta-t"
                ]
            },
            {
                "id": "de441-long-range",
                "displayName": "Optional DE441 long-range data",
                "bundled": false,
                "longRange": true,
                "assetIds": ["de441-kernel"]
            }
        ],
        "assets": [
            {
                "id": "de440s-kernel",
                "kind": "solar-system-kernel",
                "profileId": "modern",
                "version": "DE440s",
                "sourceUrl": "https://ssd.jpl.nasa.gov/ftp/eph/planets/bsp/de440s.bsp",
                "relativePath": "kernels/de440s.bsp.zst",
                "checksum": {
                    "algorithm": "sha256",
                    "value": "0123456789abcdef"
                },
                "compression": {
                    "format": "zstd",
                    "compressedSizeBytes": 4096,
                    "uncompressedSizeBytes": 8192
                },
                "validityRange": {
                    "id": "de440s-range",
                    "displayName": "DE440s kernel range",
                    "start": "1849-12-26",
                    "end": "2150-01-22"
                }
            },
            {
                "id": "leap-seconds",
                "kind": "leap-second-table",
                "profileId": "modern",
                "version": "2026a",
                "sourceUrl": "https://data.iana.org/time-zones/data/leap-seconds.list",
                "relativePath": "time/leap-seconds.list",
                "checksum": {
                    "algorithm": "sha256",
                    "value": "abcdef0123456789"
                },
                "compression": {
                    "format": "none"
                },
                "validityRange": {
                    "id": "leap-second-range",
                    "displayName": "Leap-second table range",
                    "start": "1972-01-01",
                    "end": "2027-01-01"
                }
            },
            {
                "id": "earth-orientation",
                "kind": "earth-orientation-data",
                "profileId": "modern",
                "version": "2026-05-01",
                "sourceUrl": "https://datacenter.iers.org/products/eop",
                "relativePath": "time/eop.csv",
                "checksum": {
                    "algorithm": "sha256",
                    "value": "fedcba9876543210"
                },
                "compression": {
                    "format": "none"
                },
                "validityRange": {
                    "id": "eop-range",
                    "displayName": "EOP range",
                    "start": "1973-01-02",
                    "end": "2026-10-01"
                }
            },
            {
                "id": "delta-t",
                "kind": "delta-t-data",
                "profileId": "modern",
                "version": "2026a",
                "sourceUrl": "https://example.test/delta-t.csv",
                "relativePath": "time/delta-t.csv",
                "checksum": {
                    "algorithm": "sha256",
                    "value": "0011223344556677"
                },
                "compression": {
                    "format": "none"
                },
                "validityRange": {
                    "id": "delta-t-range",
                    "displayName": "Delta T range",
                    "start": "-13200-01-01",
                    "end": "13200-12-31"
                }
            },
            {
                "id": "de441-kernel",
                "kind": "solar-system-kernel",
                "profileId": "de441-long-range",
                "version": "DE441",
                "sourceUrl": "https://ssd.jpl.nasa.gov/ftp/eph/planets/bsp/de441.bsp",
                "relativePath": "kernels/de441.bsp.zst",
                "optional": true,
                "checksum": {
                    "algorithm": "sha256",
                    "value": "8899aabbccddeeff"
                },
                "compression": {
                    "format": "zstd",
                    "compressedSizeBytes": 4096000,
                    "uncompressedSizeBytes": 8192000
                },
                "validityRange": {
                    "id": "de441-range",
                    "displayName": "DE441 long range",
                    "start": "-13200-01-01",
                    "end": "17191-01-01"
                }
            }
        ]
    })";
}

[[nodiscard]] skygate::ephemeris::AstronomicalEpoch epochForDate(const int year, const int month, const int day)
{
    const auto epoch = skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = year,
        .month = month,
        .day = day,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

}  // namespace

void EphemerisDataManifestTests::parsesValidManifest()
{
    const skygate::ephemeris::EphemerisDataManifestParseResult result =
        skygate::ephemeris::parseEphemerisDataManifest(validManifestPayload());

    QVERIFY(result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataManifestStatus::Valid)
    );
    QVERIFY(result.manifest.dataSetInfo.id == std::string{"skygate-high-precision-2026a"});
    QVERIFY(result.manifest.dataSetInfo.version == std::string{"2026a"});
    QCOMPARE(result.manifest.profiles.size(), std::size_t{2});
    QCOMPARE(result.manifest.assets.size(), std::size_t{5});
    QVERIFY(result.manifest.profile("modern") != nullptr);
    QVERIFY(result.manifest.asset("de440s-kernel") != nullptr);
}

void EphemerisDataManifestTests::rejectsMalformedManifest()
{
    for (const std::string_view payload : {"", "   \n\t", "[1, 2, 3]", "{ not-json"}) {
        const skygate::ephemeris::EphemerisDataManifestParseResult result =
            skygate::ephemeris::parseEphemerisDataManifest(payload);

        QVERIFY(!result.isSuccess());
        QCOMPARE(
            static_cast<std::uint8_t>(result.status),
            static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataManifestStatus::Malformed)
        );
        QVERIFY(!result.diagnostics.empty());
    }
}

void EphemerisDataManifestTests::rejectsMissingRequiredFields()
{
    constexpr std::string_view kMissingRequiredFields = R"({
        "schemaVersion": 1,
        "id": "missing-fields",
        "profiles": [],
        "assets": []
    })";

    const skygate::ephemeris::EphemerisDataManifestParseResult result =
        skygate::ephemeris::parseEphemerisDataManifest(kMissingRequiredFields);

    QVERIFY(!result.isSuccess());
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataManifestTests::parsesChecksumAndCompressionMetadata()
{
    const skygate::ephemeris::EphemerisDataManifestParseResult result =
        skygate::ephemeris::parseEphemerisDataManifest(validManifestPayload());

    QVERIFY(result.isSuccess());
    const skygate::ephemeris::EphemerisDataManifestAsset* kernel = result.manifest.asset("de440s-kernel");
    QVERIFY(kernel != nullptr);
    QVERIFY(kernel->checksum.algorithm == std::string{"sha256"});
    QVERIFY(kernel->checksum.value == std::string{"0123456789abcdef"});
    QCOMPARE(
        static_cast<std::uint8_t>(kernel->compression.kind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataManifestCompressionKind::Zstd)
    );
    QVERIFY(kernel->compression.compressedSizeBytes.has_value());
    QVERIFY(kernel->compression.uncompressedSizeBytes.has_value());
    QCOMPARE(*kernel->compression.compressedSizeBytes, std::uint64_t{4096});
    QCOMPARE(*kernel->compression.uncompressedSizeBytes, std::uint64_t{8192});

    const skygate::ephemeris::EphemerisDataManifestAsset* leapSeconds = result.manifest.asset("leap-seconds");
    QVERIFY(leapSeconds != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(leapSeconds->compression.kind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataManifestCompressionKind::None)
    );
    QVERIFY(!leapSeconds->compression.compressedSizeBytes.has_value());
}

void EphemerisDataManifestTests::parsesValidityRanges()
{
    const skygate::ephemeris::EphemerisDataManifestParseResult result =
        skygate::ephemeris::parseEphemerisDataManifest(validManifestPayload());

    QVERIFY(result.isSuccess());
    QVERIFY(result.manifest.dataSetInfo.dateRanges.size() == 1U);
    QCOMPARE(
        result.manifest.dataSetInfo.dateRanges[0].start.julianDatePart1, epochForDate(-13200, 1, 1).julianDatePart1
    );
    QCOMPARE(
        result.manifest.dataSetInfo.dateRanges[0].end.julianDatePart1, epochForDate(13200, 12, 31).julianDatePart1
    );

    const skygate::ephemeris::EphemerisDataManifestAsset* de441 = result.manifest.asset("de441-kernel");
    QVERIFY(de441 != nullptr);
    QCOMPARE(de441->validityRange.start.julianDatePart1, epochForDate(-13200, 1, 1).julianDatePart1);
    QCOMPARE(de441->validityRange.end.julianDatePart1, epochForDate(17191, 1, 1).julianDatePart1);
}

void EphemerisDataManifestTests::distinguishesModernAndOptionalDe441Profiles()
{
    const skygate::ephemeris::EphemerisDataManifestParseResult result =
        skygate::ephemeris::parseEphemerisDataManifest(validManifestPayload());

    QVERIFY(result.isSuccess());
    const skygate::ephemeris::EphemerisDataManifestProfile* modern = result.manifest.profile("modern");
    QVERIFY(modern != nullptr);
    QVERIFY(modern->bundled);
    QVERIFY(!modern->longRange);
    QVERIFY(std::ranges::find(modern->assetIds, "de440s-kernel") != modern->assetIds.end());

    const skygate::ephemeris::EphemerisDataManifestProfile* de441 = result.manifest.profile("de441-long-range");
    QVERIFY(de441 != nullptr);
    QVERIFY(!de441->bundled);
    QVERIFY(de441->longRange);
    QCOMPARE(de441->assetIds.size(), std::size_t{1});

    const skygate::ephemeris::EphemerisDataManifestAsset* de441Kernel = result.manifest.asset(de441->assetIds[0]);
    QVERIFY(de441Kernel != nullptr);
    QVERIFY(de441Kernel->optional);
    QCOMPARE(
        static_cast<std::uint8_t>(de441Kernel->kind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel)
    );
}

void EphemerisDataManifestTests::rejectsUnknownProfileAssetReferences()
{
    constexpr std::string_view kUnknownReference = R"({
        "schemaVersion": 1,
        "id": "bad-reference",
        "displayName": "Bad reference",
        "version": "2026a",
        "provenance": "test",
        "profiles": [
            {
                "id": "modern",
                "displayName": "Modern",
                "bundled": true,
                "longRange": false,
                "assetIds": ["missing-kernel"]
            }
        ],
        "assets": [
            {
                "id": "leap-seconds",
                "kind": "leap-second-table",
                "profileId": "modern",
                "version": "2026a",
                "sourceUrl": "https://example.test/leap-seconds.list",
                "checksum": {
                    "algorithm": "sha256",
                    "value": "abcdef"
                },
                "compression": {
                    "format": "none"
                },
                "validityRange": {
                    "id": "leap-range",
                    "displayName": "Leap range",
                    "start": "1972-01-01",
                    "end": "2027-01-01"
                }
            }
        ]
    })";

    const skygate::ephemeris::EphemerisDataManifestParseResult result =
        skygate::ephemeris::parseEphemerisDataManifest(kUnknownReference);

    QVERIFY(!result.isSuccess());
    QVERIFY(!result.diagnostics.empty());
}

QTEST_APPLESS_MAIN(EphemerisDataManifestTests)

#include "EphemerisDataManifestTests.moc"
