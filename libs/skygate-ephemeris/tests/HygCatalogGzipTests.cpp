#include "TestHelpers.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QtTest/QtTest>

#include <array>

class HygCatalogGzipTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesValidGzipCatalog();
    void rejectsInvalidGzipCatalog();
};

void HygCatalogGzipTests::parsesValidGzipCatalog()
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

    const auto catalog = skygate::ephemeris::createStarCatalogFromHygCsvGzip(compressedData);
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 1U);
    QVERIFY(bodies[0].id == "hip_42");
    QVERIFY(bodies[0].fixedEquatorial.has_value());
    if (bodies[0].fixedEquatorial.has_value()) {
        QVERIFY(skygate::ephemeris::tests::isNear(
            bodies[0].fixedEquatorial->rightAscensionHours,
            1.25,
            1e-8
        ));
        QVERIFY(skygate::ephemeris::tests::isNear(
            bodies[0].fixedEquatorial->declinationDeg,
            -2.5,
            1e-8
        ));
    }
}

void HygCatalogGzipTests::rejectsInvalidGzipCatalog()
{
    const auto emptyCatalog = skygate::ephemeris::createStarCatalogFromHygCsvGzip("");
    QVERIFY(emptyCatalog == nullptr);

    const auto malformedCatalog = skygate::ephemeris::createStarCatalogFromHygCsvGzip(
        "not-a-gzip-stream"
    );
    QVERIFY(malformedCatalog == nullptr);
}

QTEST_APPLESS_MAIN(HygCatalogGzipTests)

#include "HygCatalogGzipTests.moc"
