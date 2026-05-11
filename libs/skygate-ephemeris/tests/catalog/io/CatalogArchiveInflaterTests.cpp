#include "catalog/io/CompressedDataInflater.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

class CatalogArchiveInflaterTests final : public QObject {
    Q_OBJECT

private slots:
    void inflatesGzipCsvPayloads();
    void inflaterRejectsInvalidInputsAndHonorsOptions();
};

void CatalogArchiveInflaterTests::inflatesGzipCsvPayloads()
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

void CatalogArchiveInflaterTests::inflaterRejectsInvalidInputsAndHonorsOptions()
{
    using namespace skygate::ephemeris;

    constexpr std::array<unsigned char, 17> kRawDeflate {
        0x4b, 0xcc, 0x29, 0xc8, 0x48, 0xd4, 0x49, 0x4a, 0x2d, 0x49, 0xe4, 0x32,
        0xd4, 0x31, 0xe2, 0x02, 0x00
    };
    const std::string_view rawDeflate(
        reinterpret_cast<const char*>(kRawDeflate.data()),
        kRawDeflate.size()
    );

    const auto inflated = CompressedDataInflater::inflate(
        rawDeflate,
        CompressedDataFormat::RawDeflate,
        CompressedDataInflateOptions {
            .expectedOutputBytes = 15U,
            .allowEmptyOutput = false
        }
    );
    QVERIFY(inflated.has_value());
    QCOMPARE(*inflated, std::string("alpha,beta\n1,2\n"));

    QVERIFY(!CompressedDataInflater::inflate("", CompressedDataFormat::RawDeflate).has_value());
    QVERIFY(!CompressedDataInflater::inflate("not compressed", CompressedDataFormat::RawDeflate).has_value());
    QVERIFY(!CompressedDataInflater::inflate(
        rawDeflate,
        CompressedDataFormat::RawDeflate,
        CompressedDataInflateOptions {
            .expectedOutputBytes = 16U,
            .allowEmptyOutput = false
        }
    ).has_value());
    QVERIFY(!CompressedDataInflater::inflate(
        rawDeflate,
        CompressedDataFormat::RawDeflate,
        CompressedDataInflateOptions {
            .expectedOutputBytes = std::numeric_limits<std::size_t>::max(),
            .allowEmptyOutput = false
        }
    ).has_value());

    constexpr std::array<unsigned char, 2> kEmptyRawDeflate {0x03, 0x00};
    const std::string_view emptyRawDeflate(
        reinterpret_cast<const char*>(kEmptyRawDeflate.data()),
        kEmptyRawDeflate.size()
    );
    QVERIFY(!CompressedDataInflater::inflate(
        emptyRawDeflate,
        CompressedDataFormat::RawDeflate
    ).has_value());
    const auto emptyInflated = CompressedDataInflater::inflate(
        emptyRawDeflate,
        CompressedDataFormat::RawDeflate,
        CompressedDataInflateOptions {
            .expectedOutputBytes = 0U,
            .allowEmptyOutput = true
        }
    );
    QVERIFY(emptyInflated.has_value());
    QVERIFY(emptyInflated->empty());
}

QTEST_APPLESS_MAIN(CatalogArchiveInflaterTests)

#include "CatalogArchiveInflaterTests.moc"
