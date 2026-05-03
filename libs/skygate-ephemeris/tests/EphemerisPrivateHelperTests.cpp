#include "catalog/CatalogParsingUtilities.hpp"
#include "catalog/CatalogPayloadFormatDetector.hpp"
#include "catalog/CompressedDataInflater.hpp"
#include "catalog/OpenNgcObjectMapper.hpp"
#include "catalog/StellariumHipParser.hpp"
#include "catalog/StellariumLabelRefExtractor.hpp"
#include "catalog/StellariumLineRefExtractor.hpp"
#include "catalog/ZipCodec.hpp"
#include "common/StringUtilities.hpp"
#include "skygate/ephemeris/CatalogLoadResult.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <string>

class EphemerisPrivateHelperTests final : public QObject {
    Q_OBJECT

private slots:
    void normalizesAsciiStrings();
    void parsesCatalogTextValues();
    void detectsCatalogPayloadFormats();
    void inflatesCompressedPayloadsAndExtractsZipCsv();
    void extractsStellariumRefs();
    void mapsOpenNgcObjects();
};

void EphemerisPrivateHelperTests::normalizesAsciiStrings()
{
    using namespace skygate::ephemeris;

    QCOMPARE(strings::toLowerAscii("AbC_123"), std::string("abc_123"));
    QCOMPARE(strings::trimAsciiWhitespace(" \tOrion\n "), std::string_view("Orion"));
    QVERIFY(strings::equalsIgnoreAsciiCase("HIP_42", "hip_42"));
    QVERIFY(strings::containsIgnoreAsciiCase("Name;Type;RA;Dec", "type"));
    QCOMPARE(strings::normalizedLookupKey("  ORION  "), std::string("orion"));
    QCOMPARE(strings::normalizedAlnumKey("NGC 0224"), std::string("ngc0224"));

    std::vector<std::string> values {"M 31"};
    QVERIFY(!strings::appendUniqueIgnoreAsciiCase(values, "m 31"));
    QVERIFY(strings::appendUniqueIgnoreAsciiCase(values, "NGC 224"));
    QCOMPARE(values.size(), 2U);
}

void EphemerisPrivateHelperTests::parsesCatalogTextValues()
{
    using namespace skygate::ephemeris;

    QCOMPARE(catalog_parsing::toUtf8String(QString::fromUtf8("M31")), std::string("M31"));
    QVERIFY(catalog_parsing::parseFiniteDouble(QStringLiteral(" 1.25 ")).has_value());
    QVERIFY(!catalog_parsing::parseFiniteDouble(QStringLiteral("nan")).has_value());
    QVERIFY(catalog_parsing::parsePositiveDouble(QStringLiteral("2.5")).has_value());
    QVERIFY(!catalog_parsing::parsePositiveDouble(QStringLiteral("-2.5")).has_value());

    const auto ra = catalog_parsing::parseRightAscensionHours(QStringLiteral("01:30:00"));
    QVERIFY(ra.has_value());
    QCOMPARE(*ra, 1.5);

    const auto dec = catalog_parsing::parseDeclinationDeg(QStringLiteral("-02:30:00"));
    QVERIFY(dec.has_value());
    QCOMPARE(*dec, -2.5);
    QVERIFY(!catalog_parsing::parseRightAscensionHours(QStringLiteral("24:00:00")).has_value());
    QVERIFY(!catalog_parsing::parseDeclinationDeg(QStringLiteral("+91:00:00")).has_value());
}

void EphemerisPrivateHelperTests::detectsCatalogPayloadFormats()
{
    using namespace skygate::ephemeris;

    QCOMPARE(
        CatalogPayloadFormatDetector::detect("# comment\nid,ra,dec,mag\n"),
        CatalogPayloadFormat::HygCsv
    );
    QCOMPARE(
        CatalogPayloadFormatDetector::detect("Name;Type;RA;Dec\n"),
        CatalogPayloadFormat::OpenNgcCsv
    );

    constexpr std::array<unsigned char, 2> kGzip {{0x1f, 0x8b}};
    QCOMPARE(
        CatalogPayloadFormatDetector::detect(std::string_view(
            reinterpret_cast<const char*>(kGzip.data()),
            kGzip.size()
        )),
        CatalogPayloadFormat::HygCsvGzip
    );

    constexpr std::array<unsigned char, 4> kZip {{0x50, 0x4b, 0x03, 0x04}};
    QCOMPARE(
        CatalogPayloadFormatDetector::detect(std::string_view(
            reinterpret_cast<const char*>(kZip.data()),
            kZip.size()
        )),
        CatalogPayloadFormat::HygCsvZip
    );
    QCOMPARE(CatalogPayloadFormatDetector::detect("not a catalog"), CatalogPayloadFormat::Unknown);
}

void EphemerisPrivateHelperTests::inflatesCompressedPayloadsAndExtractsZipCsv()
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
    const auto inflated = CompressedDataInflater::inflate(compressedData, CompressedDataFormat::Gzip);
    QVERIFY(inflated.has_value());
    QVERIFY(inflated->find("hip") != std::string::npos);

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

void EphemerisPrivateHelperTests::extractsStellariumRefs()
{
    constexpr std::string_view kJsonPayload = R"({
        "constellations": {
            "orion": {
                "common_name": { "english": "Orion" },
                "lines": [[{"hip": 24436}, "hip_25930", 26727], [26727, 24436]]
            },
            "ursa-major": {
                "line": [["hip 1", "hip_2"]]
            }
        }
    })";
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray(kJsonPayload.data(), static_cast<qsizetype>(kJsonPayload.size()))
    );
    const QJsonObject root = document.object();

    const auto lineRefs = skygate::ephemeris::StellariumLineRefExtractor::extract(root);
    QVERIFY(lineRefs.size() == 4U);

    const auto labelRefs = skygate::ephemeris::StellariumLabelRefExtractor::extract(root);
    QVERIFY(labelRefs.size() == 2U);
    QCOMPARE(labelRefs[0].first, std::string("Orion"));

    const auto hip = skygate::ephemeris::StellariumHipParser::parseStrictHipText("hip_24436");
    QVERIFY(hip.has_value());
    QCOMPARE(*hip, 24436);
}

void EphemerisPrivateHelperTests::mapsOpenNgcObjects()
{
    using namespace skygate::ephemeris;

    QVERIFY(OpenNgcObjectMapper::shouldSkipType(QStringLiteral("Star")));
    const auto mapping = OpenNgcObjectMapper::mapObject(
        QStringLiteral("G"),
        QStringLiteral("NGC0224"),
        OpenNgcObjectMapper::withoutLeadingZeros(QStringLiteral("031")),
        OpenNgcObjectMapper::withoutLeadingZeros(QStringLiteral("0224")),
        {},
        QStringLiteral("NGC0224,M 031"),
        QStringLiteral("Andromeda Galaxy")
    );

    QCOMPARE(mapping.id, std::string("messier_031"));
    QCOMPARE(mapping.displayName, std::string("M31"));
    QCOMPARE(mapping.kind, DeepSkyObjectKind::Galaxy);
    QVERIFY(std::find(mapping.aliases.begin(), mapping.aliases.end(), "M 31") != mapping.aliases.end());
    QVERIFY(std::find(mapping.aliases.begin(), mapping.aliases.end(), "NGC 224") != mapping.aliases.end());
    QVERIFY(
        std::find(mapping.aliases.begin(), mapping.aliases.end(), "Andromeda Galaxy")
        != mapping.aliases.end()
    );
}

QTEST_APPLESS_MAIN(EphemerisPrivateHelperTests)

#include "EphemerisPrivateHelperTests.moc"
