#include "CatalogArchiveTestSupport.hpp"

#include "catalog/CatalogLoadResult.hpp"
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

using skygate::ephemeris::CatalogLoadResult;
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
    void ignoresDirectoriesAndEncryptedEntries();
    void reportsMalformedArchive();
    void retainsSingleCatalogCompatibility();
    void reportsNoSupportedMember();
    void reportsNoReadableEntry();
    void mapsStatusToCatalogErrorCodeAndDetail();
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

void CatalogZipEntrySelectorTests::ignoresDirectoriesAndEncryptedEntries()
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

void CatalogZipEntrySelectorTests::mapsStatusToCatalogErrorCodeAndDetail()
{
    const CatalogZipEntrySelection selected{
        .status = CatalogZipEntrySelection::Status::Selected,
        .selectedPath = "catalog/hyg.csv",
    };
    QCOMPARE(selected.errorCode(), CatalogLoadResult::ErrorCode::NoError);
    QVERIFY(selected.errorDetail().empty());

    const CatalogZipEntrySelection invalidArchive{.status = CatalogZipEntrySelection::Status::InvalidArchive};
    QCOMPARE(invalidArchive.errorCode(), CatalogLoadResult::ErrorCode::InvalidZipData);
    QCOMPARE(invalidArchive.errorDetail(), std::string("ZIP catalog payload could not be parsed."));

    const CatalogZipEntrySelection missingMember{
        .status = CatalogZipEntrySelection::Status::MissingMember,
        .selectedPath = "catalog/missing.csv",
    };
    QCOMPARE(missingMember.errorCode(), CatalogLoadResult::ErrorCode::ArchiveMemberNotFound);
    QCOMPARE(
        missingMember.errorDetail(), std::string("ZIP catalog payload does not contain member 'catalog/missing.csv'.")
    );

    const CatalogZipEntrySelection unusableMember{
        .status = CatalogZipEntrySelection::Status::UnusableMember,
        .selectedPath = "catalog/archive/",
    };
    QCOMPARE(unusableMember.errorCode(), CatalogLoadResult::ErrorCode::InvalidZipData);
    QCOMPARE(
        unusableMember.errorDetail(),
        std::string("ZIP catalog member 'catalog/archive/' is not a readable catalog entry.")
    );

    const CatalogZipEntrySelection ambiguousMember{.status = CatalogZipEntrySelection::Status::AmbiguousMember};
    QCOMPARE(ambiguousMember.errorCode(), CatalogLoadResult::ErrorCode::AmbiguousArchiveMember);
    QCOMPARE(
        ambiguousMember.errorDetail(), std::string("ZIP catalog payload contains multiple supported catalog members.")
    );

    const CatalogZipEntrySelection noSupportedMember{.status = CatalogZipEntrySelection::Status::NoSupportedMember};
    QCOMPARE(noSupportedMember.errorCode(), CatalogLoadResult::ErrorCode::InvalidZipData);
    QCOMPARE(
        noSupportedMember.errorDetail(), std::string("ZIP catalog payload does not contain a supported catalog member.")
    );

    const CatalogZipEntrySelection noReadableEntry{.status = CatalogZipEntrySelection::Status::NoReadableEntry};
    QCOMPARE(noReadableEntry.errorCode(), CatalogLoadResult::ErrorCode::InvalidZipData);
    QCOMPARE(noReadableEntry.errorDetail(), std::string("ZIP catalog payload does not contain a readable CSV entry."));
}

QTEST_APPLESS_MAIN(CatalogZipEntrySelectorTests)

#include "CatalogZipEntrySelectorTests.moc"
