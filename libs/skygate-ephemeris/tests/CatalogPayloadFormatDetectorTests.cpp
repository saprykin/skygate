#include "catalog/io/CatalogPayloadFormatDetector.hpp"

#include <QtTest/QtTest>

#include <array>
#include <string_view>

class CatalogPayloadFormatDetectorTests final : public QObject {
    Q_OBJECT

private slots:
    void detectsTextPayloadFormats();
    void detectsCompressedPayloadFormats();
    void returnsUnknownForUnrecognizedPayloads();
};

void CatalogPayloadFormatDetectorTests::detectsTextPayloadFormats()
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
}

void CatalogPayloadFormatDetectorTests::detectsCompressedPayloadFormats()
{
    using namespace skygate::ephemeris;

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
}

void CatalogPayloadFormatDetectorTests::returnsUnknownForUnrecognizedPayloads()
{
    QCOMPARE(
        skygate::ephemeris::CatalogPayloadFormatDetector::detect("not a catalog"),
        skygate::ephemeris::CatalogPayloadFormat::Unknown
    );
}

QTEST_APPLESS_MAIN(CatalogPayloadFormatDetectorTests)

#include "CatalogPayloadFormatDetectorTests.moc"
