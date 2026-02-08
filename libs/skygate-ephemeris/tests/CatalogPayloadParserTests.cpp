#include "TestHelpers.hpp"
#include "skygate/ephemeris/CatalogPayloadParser.hpp"

#include <QtTest/QtTest>

#include <array>

class CatalogPayloadParserTests final : public QObject {
    Q_OBJECT

private slots:
    void detectsPayloadFormats();
    void parsesPipeRowsPayload();
    void parsesHygGzipPayload();
};

void CatalogPayloadParserTests::detectsPayloadFormats()
{
    const skygate::ephemeris::CatalogPayloadParser parser;

    QVERIFY(
        parser.detectFormat("alpha|Alpha|Star|1.0\n")
        == skygate::ephemeris::CatalogPayloadFormat::PipeRows
    );
    QVERIFY(
        parser.detectFormat("id,hip,proper,ra,dec,mag\n")
        == skygate::ephemeris::CatalogPayloadFormat::HygCsv
    );

    constexpr std::array<unsigned char, 2> kGzipPrefix {{0x1f, 0x8b}};
    const std::string_view gzipPrefix(
        reinterpret_cast<const char*>(kGzipPrefix.data()),
        kGzipPrefix.size()
    );
    QVERIFY(
        parser.detectFormat(gzipPrefix)
        == skygate::ephemeris::CatalogPayloadFormat::HygCsvGzip
    );

    QVERIFY(
        parser.detectFormat("just some plain text")
        == skygate::ephemeris::CatalogPayloadFormat::Unknown
    );
}

void CatalogPayloadParserTests::parsesPipeRowsPayload()
{
    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto catalog = parser.parse(
        "demo_star|Demo Star|Star|1.0|12.5|-30.0\n"
        "demo_constellation|Demo Constellation|Constellation|2.0\n"
    );
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 2U);
    QVERIFY(skygate::ephemeris::tests::findBodyById(bodies, "demo_star") != nullptr);
    QVERIFY(skygate::ephemeris::tests::findBodyById(bodies, "demo_constellation") != nullptr);
}

void CatalogPayloadParserTests::parsesHygGzipPayload()
{
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

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto catalog = parser.parse(compressedData);
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 1U);
    QVERIFY(bodies[0].id == "hip_42");
}

QTEST_APPLESS_MAIN(CatalogPayloadParserTests)

#include "CatalogPayloadParserTests.moc"
