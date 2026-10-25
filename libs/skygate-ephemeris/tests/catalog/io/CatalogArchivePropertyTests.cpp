#include "CatalogArchiveTestSupport.hpp"
#include "catalog/io/CatalogZipEntrySelector.hpp"
#include "catalog/io/zip/ZipDirectoryReader.hpp"
#include "catalog/io/zip/ZipEntryExtractor.hpp"
#include "skygate/testsupport/DeterministicFuzz.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using skygate::ephemeris::tests::makeZip;
using skygate::ephemeris::tests::ZipEntrySpec;

[[nodiscard]] std::string csvPayload(const int value)
{
    return "hip,ra,dec,mag\n" + std::to_string(value) + ",1.0,2.0,3.0\n";
}

}  // namespace

class CatalogArchivePropertyTests final : public QObject {
    Q_OBJECT

private slots:
    void storedEntriesRoundTripThroughDirectoryAndCodec();
    void mutatedZipStructuresFailClosedOrRemainBounded();
    void deflatedEntriesRejectMismatchedUncompressedSize();
};

void CatalogArchivePropertyTests::storedEntriesRoundTripThroughDirectoryAndCodec()
{
    for (int scenario = 0; scenario < 48; ++scenario) {
        skygate::testsupport::DeterministicRng rng(0xa12c0000U + scenario);
        const int entryCount = rng.intInRange(2, 6);
        const int csvIndex = rng.intInRange(0, entryCount - 1);
        std::vector<ZipEntrySpec> specs;
        specs.reserve(static_cast<std::size_t>(entryCount));
        std::string firstCsvPayload;
        std::size_t csvEntryCount = 0;

        for (int index = 0; index < entryCount; ++index) {
            const bool isCsv = index == csvIndex || rng.chance(1U, 4U);
            const std::string path =
                isCsv ? "catalog/generated_" + std::to_string(scenario) + "_" + std::to_string(index) + ".CSV"
                      : "notes/readme_" + std::to_string(index) + ".txt";
            const std::string data = isCsv ? csvPayload((scenario * 100) + index)
                                           : rng.token(static_cast<std::size_t>(rng.intInRange(8, 40)), "abcXYZ0123 ");
            if (isCsv) {
                ++csvEntryCount;
                if (firstCsvPayload.empty()) {
                    firstCsvPayload = data;
                }
            }
            specs.push_back(
                ZipEntrySpec{
                    .path = path,
                    .data = data,
                }
            );
        }

        const std::string zipData = makeZip(specs, rng.token(8U, "comment012345"));
        const auto entries = skygate::ephemeris::ZipDirectoryReader::readEntries(zipData);
        QVERIFY(entries.has_value());
        QCOMPARE(entries->size(), specs.size());
        for (std::size_t index = 0; index < specs.size(); ++index) {
            QCOMPARE(entries->at(index).path, specs.at(index).path);
            QCOMPARE(entries->at(index).compressedSize, specs.at(index).data.size());
            QCOMPARE(entries->at(index).uncompressedSize, specs.at(index).data.size());
            const auto extracted = skygate::ephemeris::ZipEntryExtractor::extract(zipData, entries->at(index));
            QVERIFY(extracted.has_value());
            QCOMPARE(*extracted, specs.at(index).data);
        }

        const auto selection = skygate::ephemeris::CatalogZipEntrySelector::select(zipData, std::nullopt);
        if (csvEntryCount == 1U) {
            QCOMPARE(selection.status, skygate::ephemeris::CatalogZipEntrySelection::Status::Selected);
            QCOMPARE(selection.payload, firstCsvPayload);
        } else {
            QCOMPARE(selection.status, skygate::ephemeris::CatalogZipEntrySelection::Status::AmbiguousMember);
        }
    }
}

void CatalogArchivePropertyTests::mutatedZipStructuresFailClosedOrRemainBounded()
{
    const std::string seedZip = makeZip({
        ZipEntrySpec{
            .path = "README.txt",
            .data = "not a catalog",
        },
        ZipEntrySpec{
            .path = "catalog/hyg.csv",
            .data = csvPayload(42),
        },
    });
    skygate::testsupport::DeterministicRng rng(0x21f1a11U);

    for (std::size_t offset = 0; offset < seedZip.size(); offset += 5U) {
        std::string mutated = seedZip;
        mutated[offset] = static_cast<char>(
            static_cast<unsigned char>(mutated[offset]) ^ static_cast<unsigned char>(1U + (rng.nextU32() & 0x7fU))
        );

        const auto entries = skygate::ephemeris::ZipDirectoryReader::readEntries(mutated);
        if (entries.has_value()) {
            for (const auto& entry : *entries) {
                const auto extracted = skygate::ephemeris::ZipEntryExtractor::extract(mutated, entry);
                if (extracted.has_value()) {
                    QVERIFY(!extracted->empty());
                    QVERIFY(extracted->size() <= mutated.size());
                }
            }
        }

        const auto selection = skygate::ephemeris::CatalogZipEntrySelector::select(mutated, std::nullopt);
        if (selection.status == skygate::ephemeris::CatalogZipEntrySelection::Status::Selected) {
            QVERIFY(!selection.payload.empty());
            QVERIFY(selection.payload.size() <= mutated.size());
        }
    }
}

void CatalogArchivePropertyTests::deflatedEntriesRejectMismatchedUncompressedSize()
{
    constexpr std::array<unsigned char, 213> kZippedCsv{
        0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0xc8, 0xa0, 0x5c, 0x5c, 0x76, 0xd9, 0x48, 0x5a,
        0x31, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x07, 0x00, 0x1c, 0x00, 0x68, 0x79, 0x67, 0x2e, 0x63, 0x73,
        0x76, 0x55, 0x54, 0x09, 0x00, 0x03, 0xa8, 0x3c, 0xa3, 0x69, 0xa8, 0x3c, 0xa3, 0x69, 0x75, 0x78, 0x0b, 0x00,
        0x01, 0x04, 0xf5, 0x01, 0x00, 0x00, 0x04, 0x14, 0x00, 0x00, 0x00, 0xcb, 0x4c, 0xd1, 0xc9, 0xc8, 0x2c, 0xd0,
        0x29, 0x28, 0xca, 0x2f, 0x48, 0x2d, 0xd2, 0x29, 0x4a, 0xd4, 0x49, 0x49, 0x4d, 0xd6, 0xc9, 0x4d, 0x4c, 0xe7,
        0x32, 0x31, 0xd2, 0x01, 0x22, 0x97, 0xd4, 0xdc, 0x7c, 0x1d, 0x43, 0x3d, 0x23, 0x53, 0x1d, 0x5d, 0x23, 0x3d,
        0x53, 0x20, 0xcb, 0x80, 0x0b, 0x00, 0x50, 0x4b, 0x01, 0x02, 0x1e, 0x03, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00,
        0xc8, 0xa0, 0x5c, 0x5c, 0x76, 0xd9, 0x48, 0x5a, 0x31, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x07, 0x00,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xa4, 0x81, 0x00, 0x00, 0x00, 0x00, 0x68, 0x79,
        0x67, 0x2e, 0x63, 0x73, 0x76, 0x55, 0x54, 0x05, 0x00, 0x03, 0xa8, 0x3c, 0xa3, 0x69, 0x75, 0x78, 0x0b, 0x00,
        0x01, 0x04, 0xf5, 0x01, 0x00, 0x00, 0x04, 0x14, 0x00, 0x00, 0x00, 0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x01, 0x00, 0x4d, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    const std::string zipData(reinterpret_cast<const char*>(kZippedCsv.data()), kZippedCsv.size());

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
