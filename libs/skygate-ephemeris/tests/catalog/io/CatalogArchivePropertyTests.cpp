#include "catalog/io/CatalogZipEntrySelector.hpp"
#include "catalog/io/zip/ZipCodec.hpp"
#include "catalog/io/zip/ZipDirectoryReader.hpp"
#include "catalog/io/zip/ZipEntryExtractor.hpp"
#include "skygate/testsupport/DeterministicFuzz.hpp"

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

[[nodiscard]] std::string makeZip(
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

[[nodiscard]] std::string csvPayload(const int value)
{
    return "hip,ra,dec,mag\n" + std::to_string(value) + ",1.0,2.0,3.0\n";
}

}  // namespace

class CatalogArchivePropertyTests final : public QObject {
    Q_OBJECT

private slots:
    void generatedStoredZipEntriesRoundTripThroughDirectoryAndCodec();
    void mutatedZipStructuresFailClosedOrRemainBounded();
    void deflatedEntriesRejectMismatchedUncompressedSize();
};

void CatalogArchivePropertyTests::generatedStoredZipEntriesRoundTripThroughDirectoryAndCodec()
{
    for (int scenario = 0; scenario < 48; ++scenario) {
        skygate::testsupport::DeterministicRng rng(0xa12c0000U + scenario);
        const int entryCount = rng.intInRange(2, 6);
        const int csvIndex = rng.intInRange(0, entryCount - 1);
        std::vector<ZipEntrySpec> specs;
        specs.reserve(static_cast<std::size_t>(entryCount));
        std::string firstCsvPayload;

        for (int index = 0; index < entryCount; ++index) {
            const bool isCsv = index == csvIndex || rng.chance(1U, 4U);
            const std::string path = isCsv
                ? "catalog/generated_" + std::to_string(scenario) + "_" + std::to_string(index) + ".CSV"
                : "notes/readme_" + std::to_string(index) + ".txt";
            const std::string data = isCsv
                ? csvPayload((scenario * 100) + index)
                : rng.token(static_cast<std::size_t>(rng.intInRange(8, 40)), "abcXYZ0123 ");
            if (isCsv && firstCsvPayload.empty()) {
                firstCsvPayload = data;
            }
            specs.push_back(ZipEntrySpec {
                .path = path,
                .data = data,
            });
        }

        const std::string zipData = makeZip(specs, rng.token(8U, "comment012345"));
        const auto entries = skygate::ephemeris::ZipDirectoryReader::readEntries(zipData);
        QVERIFY(entries.has_value());
        QCOMPARE(entries->size(), specs.size());
        for (std::size_t index = 0; index < specs.size(); ++index) {
            QCOMPARE(entries->at(index).path, specs.at(index).path);
            QCOMPARE(entries->at(index).compressedSize, specs.at(index).data.size());
            QCOMPARE(entries->at(index).uncompressedSize, specs.at(index).data.size());
            const auto extracted =
                skygate::ephemeris::ZipEntryExtractor::extract(zipData, entries->at(index));
            QVERIFY(extracted.has_value());
            QCOMPARE(*extracted, specs.at(index).data);
        }

        const auto selected = skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(zipData);
        QVERIFY(selected.has_value());
        QCOMPARE(*selected, firstCsvPayload);
    }
}

void CatalogArchivePropertyTests::mutatedZipStructuresFailClosedOrRemainBounded()
{
    const std::string seedZip = makeZip({
        ZipEntrySpec {
            .path = "README.txt",
            .data = "not a catalog",
        },
        ZipEntrySpec {
            .path = "catalog/hyg.csv",
            .data = csvPayload(42),
        },
    });
    skygate::testsupport::DeterministicRng rng(0x21f1a11U);

    for (std::size_t offset = 0; offset < seedZip.size(); offset += 5U) {
        std::string mutated = seedZip;
        mutated[offset] = static_cast<char>(
            static_cast<unsigned char>(mutated[offset])
            ^ static_cast<unsigned char>(1U + (rng.nextU32() & 0x7fU))
        );

        const auto entries = skygate::ephemeris::ZipDirectoryReader::readEntries(mutated);
        if (entries.has_value()) {
            for (const auto& entry : *entries) {
                const auto extracted = skygate::ephemeris::ZipEntryExtractor::extract(
                    mutated,
                    entry
                );
                if (extracted.has_value()) {
                    QVERIFY(!extracted->empty());
                    QVERIFY(extracted->size() <= mutated.size());
                }
            }
        }

        const auto selected = skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(mutated);
        if (selected.has_value()) {
            QVERIFY(!selected->empty());
            QVERIFY(selected->size() <= mutated.size());
        }
    }
}

void CatalogArchivePropertyTests::deflatedEntriesRejectMismatchedUncompressedSize()
{
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
    const std::string zipData(
        reinterpret_cast<const char*>(kZippedCsv.data()),
        kZippedCsv.size()
    );

    const auto entries = skygate::ephemeris::ZipDirectoryReader::readEntries(zipData);
    QVERIFY(entries.has_value());
    QCOMPARE(entries->size(), std::size_t(1));

    auto entry = entries->front();
    QVERIFY(skygate::ephemeris::ZipEntryExtractor::extract(zipData, entry).has_value());

    ++entry.uncompressedSize;
    QVERIFY(!skygate::ephemeris::ZipEntryExtractor::extract(zipData, entry).has_value());
}

QTEST_APPLESS_MAIN(CatalogArchivePropertyTests)

#include "CatalogArchivePropertyTests.moc"
