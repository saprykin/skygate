#include "CatalogArchiveTestSupport.hpp"

#include "catalog/io/CatalogZipEntrySelector.hpp"
#include "catalog/io/zip/ZipDirectoryReader.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class CatalogArchiveZipDirectoryTests final : public QObject {
    Q_OBJECT

private slots:
    void readsCentralDirectoryEntryMetadata();
    void selectsCsvEntriesAndFallsBackToFirstUsableEntry();
    void directoryReaderRejectsMalformedStructures();
};

void CatalogArchiveZipDirectoryTests::readsCentralDirectoryEntryMetadata()
{
    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "catalog/",
            .data = {},
        },
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "catalog/hyg.csv",
            .data = "hip,ra,dec,mag\n5,1,2,3\n",
        },
        skygate::ephemeris::tests::ZipEntrySpec {
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

void CatalogArchiveZipDirectoryTests::selectsCsvEntriesAndFallsBackToFirstUsableEntry()
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

void CatalogArchiveZipDirectoryTests::directoryReaderRejectsMalformedStructures()
{
    QVERIFY(!skygate::ephemeris::ZipDirectoryReader::readEntries("not a zip").has_value());

    std::string truncatedCentralDirectory = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n13,1,2,3\n",
        },
    });
    truncatedCentralDirectory.resize(truncatedCentralDirectory.size() - 10U);
    QVERIFY(!skygate::ephemeris::ZipDirectoryReader::readEntries(
        truncatedCentralDirectory
    ).has_value());

    std::string invalidCentralSignature = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec {
            .path = "hyg.csv",
            .data = "hip,ra,dec,mag\n14,1,2,3\n",
        },
    });
    const auto centralOffset = invalidCentralSignature.find("PK\x01\x02", 0, 4);
    QVERIFY(centralOffset != std::string::npos);
    invalidCentralSignature[centralOffset] = '\0';
    QVERIFY(!skygate::ephemeris::ZipDirectoryReader::readEntries(
        invalidCentralSignature
    ).has_value());
}

QTEST_APPLESS_MAIN(CatalogArchiveZipDirectoryTests)

#include "CatalogArchiveZipDirectoryTests.moc"
