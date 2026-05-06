#include "CatalogArchiveTestSupport.hpp"

#include "catalog/ZipCodec.hpp"

#include <QtTest/QtTest>

#include <array>
#include <string>
#include <string_view>

class CatalogArchiveCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void extractsFirstCsvEntryFromZipPayloads();
    void extractsStoredCsvEntryFromZipPayloads();
    void choosesCsvEntryOverEarlierNonCsvEntry();
    void ignoresDirectoriesAndMatchesCsvExtensionCaseInsensitively();
    void rejectsUnsupportedCompressionAndCorruptLocalHeaders();
    void findsEndOfCentralDirectoryWithComment();
};

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
    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n42,1,2,3\n",
        },
    });

    const auto zipEntry = skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(zipData);

    QVERIFY(zipEntry.has_value());
    QCOMPARE(*zipEntry, std::string("hip,ra,dec,mag\n42,1,2,3\n"));
}

void CatalogArchiveCodecTests::choosesCsvEntryOverEarlierNonCsvEntry()
{
    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "README.txt",
            .data = "not a catalog",
        },
        skygate::ephemeris::tests::ZipEntrySpec {
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
    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "catalog/",
            .data = {},
        },
        skygate::ephemeris::tests::ZipEntrySpec {
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
    const std::string unsupportedZip = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n9,1,2,3\n",
            .compressionMethod = 12U,
        },
    });
    QVERIFY(!skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(unsupportedZip).has_value());

    std::string corruptZip = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n10,1,2,3\n",
        },
    });
    corruptZip[0] = '\0';
    QVERIFY(!skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(corruptZip).has_value());
}

void CatalogArchiveCodecTests::findsEndOfCentralDirectoryWithComment()
{
    const std::string zipData = skygate::ephemeris::tests::makeZip(
        {
            skygate::ephemeris::tests::ZipEntrySpec {
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
