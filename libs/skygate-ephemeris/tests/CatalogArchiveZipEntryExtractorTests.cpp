#include "CatalogArchiveTestSupport.hpp"

#include "catalog/ZipDirectoryReader.hpp"
#include "catalog/ZipEntryExtractor.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <string>

class CatalogArchiveZipEntryExtractorTests final : public QObject {
    Q_OBJECT

private slots:
    void extractorRejectsSizeMismatches();
    void extractorRejectsBoundaryMetadata();
};

void CatalogArchiveZipEntryExtractorTests::extractorRejectsSizeMismatches()
{
    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
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

void CatalogArchiveZipEntryExtractorTests::extractorRejectsBoundaryMetadata()
{
    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n12,1,2,3\n",
        },
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "empty.csv",
            .data = {},
        },
    });
    const auto entries = skygate::ephemeris::ZipDirectoryReader::readEntries(zipData);
    QVERIFY(entries.has_value());
    QCOMPARE(entries->size(), std::size_t(2));

    auto entryPastEnd = entries->front();
    entryPastEnd.localHeaderOffset = zipData.size();
    QVERIFY(!skygate::ephemeris::ZipEntryExtractor::extract(zipData, entryPastEnd).has_value());

    auto entryOversized = entries->front();
    entryOversized.compressedSize = zipData.size();
    QVERIFY(!skygate::ephemeris::ZipEntryExtractor::extract(zipData, entryOversized).has_value());

    QVERIFY(!skygate::ephemeris::ZipEntryExtractor::extract(zipData, entries->at(1)).has_value());
}

QTEST_APPLESS_MAIN(CatalogArchiveZipEntryExtractorTests)

#include "CatalogArchiveZipEntryExtractorTests.moc"
