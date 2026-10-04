#include "CatalogArchiveTestSupport.hpp"

#include "catalog/io/CatalogZipEntrySelector.hpp"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kHygCsv = "id,hip,proper,ra,dec,mag\n"
                                     "7,42,TestStar,1.25,-2.5,3.2\n";

constexpr std::string_view kOpenNgcCsv = "Name;Type;RA;Dec\n"
                                         "NGC0224;G;00:42:44.35;+41:16:08.6\n";

constexpr std::string_view kUnrelatedCsv = "foo,bar,baz\n"
                                           "1,2,3\n";

using skygate::ephemeris::CatalogZipEntrySelection;
using skygate::ephemeris::CatalogZipEntrySelector;
using skygate::ephemeris::tests::makeZip;
using skygate::ephemeris::tests::ZipEntrySpec;

}  // namespace

class CatalogZipEntrySelectorTests final : public QObject {
    Q_OBJECT

private slots:
    void selectsUniqueSupportedCsvOverUnrelatedFirstCsv();
    void selectsExplicitNonFirstMember();
    void reportsAmbiguityForMultipleSupportedMembers();
    void reportsAmbiguityAcrossDifferentSchemas();
    void reportsMissingMemberExplicitly();
    void rejectsDirectoryAndEncryptedMembersWhenRequested();
    void ignoresDirectoriesAndEncryptedEntriesWithoutSelector();
    void reportsMalformedArchive();
    void retainsSingleCatalogCompatibility();
    void reportsNoSupportedMember();
    void reportsNoReadableEntry();
};

void CatalogZipEntrySelectorTests::selectsUniqueSupportedCsvOverUnrelatedFirstCsv()
{
    const std::string zipData = makeZip({
        ZipEntrySpec{.path = "readme.csv", .data = std::string(kUnrelatedCsv)},
        ZipEntrySpec{.path = "catalog/hyg.csv", .data = std::string(kHygCsv)},
    });

    const CatalogZipEntrySelection selection = CatalogZipEntrySelector::select(zipData, std::nullopt);
    QCOMPARE(selection.status, CatalogZipEntrySelection::Status::Selected);
    QCOMPARE(selection.selectedPath, std::string("catalog/hyg.csv"));
    QCOMPARE(selection.payload, std::string(kHygCsv));
}

void CatalogZipEntrySelectorTests::selectsExplicitNonFirstMember()
{
    const std::string zipData = makeZip({
        ZipEntrySpec{.path = "first.csv", .data = std::string(kUnrelatedCsv)},
        ZipEntrySpec{.path = "second.csv", .data = std::string(kHygCsv)},
    });

    const CatalogZipEntrySelection selection =
        CatalogZipEntrySelector::select(zipData, std::optional<std::string>("second.csv"));
    QCOMPARE(selection.status, CatalogZipEntrySelection::Status::Selected);
    QCOMPARE(selection.selectedPath, std::string("second.csv"));
    QCOMPARE(selection.payload, std::string(kHygCsv));
}

void CatalogZipEntrySelectorTests::reportsAmbiguityForMultipleSupportedMembers()
{
    const std::string zipData = makeZip({
        ZipEntrySpec{.path = "a.csv", .data = std::string(kHygCsv)},
        ZipEntrySpec{.path = "b.csv", .data = std::string(kHygCsv)},
    });

    const CatalogZipEntrySelection selection = CatalogZipEntrySelector::select(zipData, std::nullopt);
    QCOMPARE(selection.status, CatalogZipEntrySelection::Status::AmbiguousMember);
}

void CatalogZipEntrySelectorTests::reportsAmbiguityAcrossDifferentSchemas()
{
    const std::string zipData = makeZip({
        ZipEntrySpec{.path = "stars.csv", .data = std::string(kHygCsv)},
        ZipEntrySpec{.path = "deepsky.csv", .data = std::string(kOpenNgcCsv)},
    });

    const CatalogZipEntrySelection selection = CatalogZipEntrySelector::select(zipData, std::nullopt);
    QCOMPARE(selection.status, CatalogZipEntrySelection::Status::AmbiguousMember);
}

void CatalogZipEntrySelectorTests::reportsMissingMemberExplicitly()
{
    const std::string zipData = makeZip({
        ZipEntrySpec{.path = "hyg.csv", .data = std::string(kHygCsv)},
    });

    const CatalogZipEntrySelection selection =
        CatalogZipEntrySelector::select(zipData, std::optional<std::string>("nope.csv"));
    QCOMPARE(selection.status, CatalogZipEntrySelection::Status::MissingMember);
    QCOMPARE(selection.selectedPath, std::string("nope.csv"));
    QVERIFY(selection.payload.empty());
}

void CatalogZipEntrySelectorTests::rejectsDirectoryAndEncryptedMembersWhenRequested()
{
    const std::string zipData = makeZip({
        ZipEntrySpec{.path = "catalog/", .data = {}},
        ZipEntrySpec{.path = "secret.csv", .data = std::string(kHygCsv), .generalPurposeFlag = 0x1U},
        ZipEntrySpec{.path = "hyg.csv", .data = std::string(kHygCsv)},
    });

    const CatalogZipEntrySelection directorySelection =
        CatalogZipEntrySelector::select(zipData, std::optional<std::string>("catalog/"));
    QCOMPARE(directorySelection.status, CatalogZipEntrySelection::Status::UnusableMember);
    QCOMPARE(directorySelection.selectedPath, std::string("catalog/"));

    const CatalogZipEntrySelection encryptedSelection =
        CatalogZipEntrySelector::select(zipData, std::optional<std::string>("secret.csv"));
    QCOMPARE(encryptedSelection.status, CatalogZipEntrySelection::Status::UnusableMember);
    QCOMPARE(encryptedSelection.selectedPath, std::string("secret.csv"));
}

void CatalogZipEntrySelectorTests::ignoresDirectoriesAndEncryptedEntriesWithoutSelector()
{
    const std::string zipData = makeZip({
        ZipEntrySpec{.path = "catalog/", .data = {}},
        ZipEntrySpec{.path = "secret.csv", .data = std::string(kHygCsv), .generalPurposeFlag = 0x1U},
        ZipEntrySpec{.path = "hyg.csv", .data = std::string(kHygCsv)},
    });

    const CatalogZipEntrySelection selection = CatalogZipEntrySelector::select(zipData, std::nullopt);
    QCOMPARE(selection.status, CatalogZipEntrySelection::Status::Selected);
    QCOMPARE(selection.selectedPath, std::string("hyg.csv"));
}

void CatalogZipEntrySelectorTests::reportsMalformedArchive()
{
    const CatalogZipEntrySelection selection = CatalogZipEntrySelector::select("not a zip", std::nullopt);
    QCOMPARE(selection.status, CatalogZipEntrySelection::Status::InvalidArchive);
}

void CatalogZipEntrySelectorTests::retainsSingleCatalogCompatibility()
{
    const std::string zipData = makeZip({
        ZipEntrySpec{.path = "hyg.csv", .data = std::string(kHygCsv)},
    });

    const CatalogZipEntrySelection selection = CatalogZipEntrySelector::select(zipData, std::nullopt);
    QCOMPARE(selection.status, CatalogZipEntrySelection::Status::Selected);
    QCOMPARE(selection.selectedPath, std::string("hyg.csv"));
    QCOMPARE(selection.payload, std::string(kHygCsv));
}

void CatalogZipEntrySelectorTests::reportsNoSupportedMember()
{
    const std::string zipData = makeZip({
        ZipEntrySpec{.path = "a.csv", .data = std::string(kUnrelatedCsv)},
        ZipEntrySpec{.path = "b.csv", .data = std::string(kUnrelatedCsv)},
    });

    const CatalogZipEntrySelection selection = CatalogZipEntrySelector::select(zipData, std::nullopt);
    QCOMPARE(selection.status, CatalogZipEntrySelection::Status::NoSupportedMember);
}

void CatalogZipEntrySelectorTests::reportsNoReadableEntry()
{
    const CatalogZipEntrySelection emptySelection = CatalogZipEntrySelector::select(makeZip({}), std::nullopt);
    QCOMPARE(emptySelection.status, CatalogZipEntrySelection::Status::NoReadableEntry);

    const std::string onlyDirectories = makeZip({
        ZipEntrySpec{.path = "catalog/", .data = {}},
    });
    QCOMPARE(
        CatalogZipEntrySelector::select(onlyDirectories, std::nullopt).status,
        CatalogZipEntrySelection::Status::NoReadableEntry
    );

    const std::string onlyEncrypted = makeZip({
        ZipEntrySpec{.path = "secret.csv", .data = std::string(kHygCsv), .generalPurposeFlag = 0x1U},
    });
    QCOMPARE(
        CatalogZipEntrySelector::select(onlyEncrypted, std::nullopt).status,
        CatalogZipEntrySelection::Status::NoReadableEntry
    );
}

QTEST_APPLESS_MAIN(CatalogZipEntrySelectorTests)

#include "CatalogZipEntrySelectorTests.moc"
