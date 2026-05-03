#include "catalog/CompressedDataInflater.hpp"
#include "catalog/CatalogZipEntrySelector.hpp"
#include "catalog/ZipCodec.hpp"
#include "catalog/ZipDirectoryReader.hpp"
#include "catalog/ZipEntryExtractor.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct ZipEntrySpec final {
    std::string path;
    std::string data;
    std::uint16_t compressionMethod = 0;
    std::uint16_t generalPurposeFlag = 0;
};

void appendLe16(std::string& data, const std::uint16_t value)
{
    data.push_back(static_cast<char>(value & 0xffU));
    data.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void appendLe32(std::string& data, const std::uint32_t value)
{
    appendLe16(data, static_cast<std::uint16_t>(value & 0xffffU));
    appendLe16(data, static_cast<std::uint16_t>((value >> 16U) & 0xffffU));
}

std::string makeZip(
    const std::vector<ZipEntrySpec>& entries,
    const std::string_view comment = {}
)
{
    std::string zipData;
    std::vector<std::uint32_t> localHeaderOffsets;
    localHeaderOffsets.reserve(entries.size());

    for (const ZipEntrySpec& entry : entries) {
        localHeaderOffsets.push_back(static_cast<std::uint32_t>(zipData.size()));
        appendLe32(zipData, 0x04034b50U);
        appendLe16(zipData, 20U);
        appendLe16(zipData, entry.generalPurposeFlag);
        appendLe16(zipData, entry.compressionMethod);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe32(zipData, 0U);
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe16(zipData, static_cast<std::uint16_t>(entry.path.size()));
        appendLe16(zipData, 0U);
        zipData += entry.path;
        zipData += entry.data;
    }

    const std::uint32_t centralDirectoryOffset = static_cast<std::uint32_t>(zipData.size());
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const ZipEntrySpec& entry = entries[index];
        appendLe32(zipData, 0x02014b50U);
        appendLe16(zipData, 20U);
        appendLe16(zipData, 20U);
        appendLe16(zipData, entry.generalPurposeFlag);
        appendLe16(zipData, entry.compressionMethod);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe32(zipData, 0U);
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe16(zipData, static_cast<std::uint16_t>(entry.path.size()));
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe32(zipData, 0U);
        appendLe32(zipData, localHeaderOffsets[index]);
        zipData += entry.path;
    }

    const std::uint32_t centralDirectorySize =
        static_cast<std::uint32_t>(zipData.size()) - centralDirectoryOffset;
    appendLe32(zipData, 0x06054b50U);
    appendLe16(zipData, 0U);
    appendLe16(zipData, 0U);
    appendLe16(zipData, static_cast<std::uint16_t>(entries.size()));
    appendLe16(zipData, static_cast<std::uint16_t>(entries.size()));
    appendLe32(zipData, centralDirectorySize);
    appendLe32(zipData, centralDirectoryOffset);
    appendLe16(zipData, static_cast<std::uint16_t>(comment.size()));
    zipData.append(comment.data(), comment.size());
    return zipData;
}

}  // namespace

class CatalogArchiveCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void inflatesGzipCsvPayloads();
    void extractsFirstCsvEntryFromZipPayloads();
    void extractsStoredCsvEntryFromZipPayloads();
    void readsCentralDirectoryEntryMetadata();
    void selectsCsvEntriesAndFallsBackToFirstUsableEntry();
    void extractorRejectsSizeMismatches();
    void choosesCsvEntryOverEarlierNonCsvEntry();
    void ignoresDirectoriesAndMatchesCsvExtensionCaseInsensitively();
    void rejectsUnsupportedCompressionAndCorruptLocalHeaders();
    void findsEndOfCentralDirectoryWithComment();
};

void CatalogArchiveCodecTests::inflatesGzipCsvPayloads()
{
    using namespace skygate::ephemeris;

    constexpr std::array<unsigned char, 73> kCompressedCsv {
        0x1f, 0x8b, 0x08, 0x00, 0x77, 0xa9, 0x86, 0x69, 0x00, 0x03, 0xcb, 0x4c,
        0xd1, 0xc9, 0xc8, 0x2c, 0xd0, 0x29, 0x28, 0xca, 0x2f, 0x48, 0x2d, 0xd2,
        0x29, 0x4a, 0xd4, 0x49, 0x49, 0x4d, 0xd6, 0xc9, 0x4d, 0x4c, 0xe7, 0x32,
        0xd7, 0x31, 0x31, 0xd2, 0x09, 0x49, 0x2d, 0x2e, 0x09, 0x2e, 0x49, 0x2c,
        0xd2, 0x31, 0xd4, 0x33, 0x32, 0xd5, 0xd1, 0x35, 0xd2, 0x33, 0xd5, 0x31,
        0xd6, 0x33, 0xe2, 0x02, 0x00, 0xb9, 0xd5, 0xee, 0x71, 0x35, 0x00, 0x00,
        0x00
    };
    const std::string_view compressedData(
        reinterpret_cast<const char*>(kCompressedCsv.data()),
        kCompressedCsv.size()
    );

    const auto inflated = CompressedDataInflater::inflate(
        compressedData,
        CompressedDataFormat::Gzip
    );

    QVERIFY(inflated.has_value());
    QVERIFY(inflated->find("hip") != std::string::npos);
}

void CatalogArchiveCodecTests::extractsFirstCsvEntryFromZipPayloads()
{
    using namespace skygate::ephemeris;

    constexpr std::array<unsigned char, 213> kZippedCsv {
        0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0xc8, 0xa0,
        0x5c, 0x5c, 0x76, 0xd9, 0x48, 0x5a, 0x31, 0x00, 0x00, 0x00, 0x32, 0x00,
        0x00, 0x00, 0x07, 0x00, 0x1c, 0x00, 0x68, 0x79, 0x67, 0x2e, 0x63, 0x73,
        0x76, 0x55, 0x54, 0x09, 0x00, 0x03, 0xa8, 0x3c, 0xa3, 0x69, 0xa8, 0x3c,
        0xa3, 0x69, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0xf5, 0x01, 0x00, 0x00,
        0x04, 0x14, 0x00, 0x00, 0x00, 0xcb, 0x4c, 0xd1, 0xc9, 0xc8, 0x2c, 0xd0,
        0x29, 0x28, 0xca, 0x2f, 0x48, 0x2d, 0xd2, 0x29, 0x4a, 0xd4, 0x49, 0x49,
        0x4d, 0xd6, 0xc9, 0x4d, 0x4c, 0xe7, 0x32, 0x31, 0xd2, 0x01, 0x22, 0x97,
        0xd4, 0xdc, 0x7c, 0x1d, 0x43, 0x3d, 0x23, 0x53, 0x1d, 0x5d, 0x23, 0x3d,
        0x53, 0x20, 0xcb, 0x80, 0x0b, 0x00, 0x50, 0x4b, 0x01, 0x02, 0x1e, 0x03,
        0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0xc8, 0xa0, 0x5c, 0x5c, 0x76, 0xd9,
        0x48, 0x5a, 0x31, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x07, 0x00,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xa4, 0x81,
        0x00, 0x00, 0x00, 0x00, 0x68, 0x79, 0x67, 0x2e, 0x63, 0x73, 0x76, 0x55,
        0x54, 0x05, 0x00, 0x03, 0xa8, 0x3c, 0xa3, 0x69, 0x75, 0x78, 0x0b, 0x00,
        0x01, 0x04, 0xf5, 0x01, 0x00, 0x00, 0x04, 0x14, 0x00, 0x00, 0x00, 0x50,
        0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x4d,
        0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    const auto zipEntry = ZipCodec {}.extractFirstCsvEntry(std::string_view(
        reinterpret_cast<const char*>(kZippedCsv.data()),
        kZippedCsv.size()
    ));

    QVERIFY(zipEntry.has_value());
    QVERIFY(zipEntry->find("hip") != std::string::npos);
}

void CatalogArchiveCodecTests::extractsStoredCsvEntryFromZipPayloads()
{
    const std::string zipData = makeZip({
        ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n42,1,2,3\n",
        },
    });

    const auto zipEntry = skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(zipData);

    QVERIFY(zipEntry.has_value());
    QCOMPARE(*zipEntry, std::string("hip,ra,dec,mag\n42,1,2,3\n"));
}

void CatalogArchiveCodecTests::readsCentralDirectoryEntryMetadata()
{
    const std::string zipData = makeZip({
        ZipEntrySpec {
            .path = "catalog/",
            .data = {},
        },
        ZipEntrySpec {
            .path = "catalog/hyg.csv",
            .data = "hip,ra,dec,mag\n5,1,2,3\n",
        },
        ZipEntrySpec {
            .path = "secret.csv",
            .data = "hidden",
            .generalPurposeFlag = 0x1U,
        },
    });

    const auto entries = skygate::ephemeris::ZipDirectoryReader::readEntries(zipData);

    QVERIFY(entries.has_value());
    QCOMPARE(entries->size(), std::size_t(3));
    QCOMPARE(entries->at(0).path, std::string("catalog/"));
    QVERIFY(entries->at(0).isDirectory());
    QCOMPARE(entries->at(1).path, std::string("catalog/hyg.csv"));
    QCOMPARE(entries->at(1).compressionMethod, static_cast<std::uint16_t>(0U));
    QCOMPARE(entries->at(1).uncompressedSize, std::size_t(23));
    QVERIFY(!entries->at(1).isEncrypted());
    QCOMPARE(entries->at(2).path, std::string("secret.csv"));
    QVERIFY(entries->at(2).isEncrypted());
}

void CatalogArchiveCodecTests::selectsCsvEntriesAndFallsBackToFirstUsableEntry()
{
    const std::vector<skygate::ephemeris::ZipEntryMetadata> entries {
        skygate::ephemeris::ZipEntryMetadata {
            .path = "catalog/",
        },
        skygate::ephemeris::ZipEntryMetadata {
            .path = "secret.csv",
            .generalPurposeFlag = 0x1U,
        },
        skygate::ephemeris::ZipEntryMetadata {
            .path = "README.txt",
        },
        skygate::ephemeris::ZipEntryMetadata {
            .path = "nested/HYG.CSV",
        },
    };

    const auto* selected =
        skygate::ephemeris::CatalogZipEntrySelector::selectFirstCsvEntry(entries);

    QVERIFY(selected != nullptr);
    QCOMPARE(selected->path, std::string("nested/HYG.CSV"));

    const std::vector<skygate::ephemeris::ZipEntryMetadata> fallbackEntries {
        skygate::ephemeris::ZipEntryMetadata {
            .path = "catalog/",
        },
        skygate::ephemeris::ZipEntryMetadata {
            .path = "README.txt",
        },
    };
    selected = skygate::ephemeris::CatalogZipEntrySelector::selectFirstCsvEntry(fallbackEntries);

    QVERIFY(selected != nullptr);
    QCOMPARE(selected->path, std::string("README.txt"));

    const std::vector<skygate::ephemeris::ZipEntryMetadata> unusableEntries {
        skygate::ephemeris::ZipEntryMetadata {
            .path = "catalog/",
        },
        skygate::ephemeris::ZipEntryMetadata {
            .path = "secret.csv",
            .generalPurposeFlag = 0x1U,
        },
    };

    QVERIFY(
        skygate::ephemeris::CatalogZipEntrySelector::selectFirstCsvEntry(unusableEntries)
        == nullptr
    );
}

void CatalogArchiveCodecTests::extractorRejectsSizeMismatches()
{
    const std::string zipData = makeZip({
        ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n6,1,2,3\n",
        },
    });
    const auto entries = skygate::ephemeris::ZipDirectoryReader::readEntries(zipData);
    QVERIFY(entries.has_value());
    QCOMPARE(entries->size(), std::size_t(1));

    auto entry = entries->front();
    const auto extracted = skygate::ephemeris::ZipEntryExtractor::extract(zipData, entry);

    QVERIFY(extracted.has_value());
    QVERIFY(extracted->find("6,1,2,3") != std::string::npos);

    ++entry.uncompressedSize;
    QVERIFY(!skygate::ephemeris::ZipEntryExtractor::extract(zipData, entry).has_value());
}

void CatalogArchiveCodecTests::choosesCsvEntryOverEarlierNonCsvEntry()
{
    const std::string zipData = makeZip({
        ZipEntrySpec {
            .path = "README.txt",
            .data = "not a catalog",
        },
        ZipEntrySpec {
            .path = "catalog/hyg.csv",
            .data = "hip,ra,dec,mag\n7,1,2,3\n",
        },
    });

    const auto zipEntry = skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(zipData);

    QVERIFY(zipEntry.has_value());
    QVERIFY(zipEntry->find("hip,ra,dec,mag") != std::string::npos);
}

void CatalogArchiveCodecTests::ignoresDirectoriesAndMatchesCsvExtensionCaseInsensitively()
{
    const std::string zipData = makeZip({
        ZipEntrySpec {
            .path = "catalog/",
            .data = {},
        },
        ZipEntrySpec {
            .path = "catalog/HYG.CSV",
            .data = "hip,ra,dec,mag\n8,1,2,3\n",
        },
    });

    const auto zipEntry = skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(zipData);

    QVERIFY(zipEntry.has_value());
    QVERIFY(zipEntry->find("8,1,2,3") != std::string::npos);
}

void CatalogArchiveCodecTests::rejectsUnsupportedCompressionAndCorruptLocalHeaders()
{
    const std::string unsupportedZip = makeZip({
        ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n9,1,2,3\n",
            .compressionMethod = 12U,
        },
    });
    QVERIFY(!skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(unsupportedZip).has_value());

    std::string corruptZip = makeZip({
        ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n10,1,2,3\n",
        },
    });
    corruptZip[0] = '\0';
    QVERIFY(!skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(corruptZip).has_value());
}

void CatalogArchiveCodecTests::findsEndOfCentralDirectoryWithComment()
{
    const std::string zipData = makeZip(
        {
            ZipEntrySpec {
                .path = "hyg.csv",
                .data = "hip,ra,dec,mag\n11,1,2,3\n",
            },
        },
        "archive comment"
    );

    const auto zipEntry = skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(zipData);

    QVERIFY(zipEntry.has_value());
    QVERIFY(zipEntry->find("11,1,2,3") != std::string::npos);
}

QTEST_APPLESS_MAIN(CatalogArchiveCodecTests)

#include "CatalogArchiveCodecTests.moc"
