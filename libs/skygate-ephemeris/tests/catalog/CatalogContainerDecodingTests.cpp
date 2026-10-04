#include "CatalogArchiveTestSupport.hpp"
#include "TestHelpers.hpp"
#include "catalog/CatalogPayloadParser.hpp"

#include <QtTest/QtTest>

#include <array>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kHygCsv = "id,hip,proper,ra,dec,mag\n"
                                     "7,42,TestStar,1.25,-2.5,3.2\n";

constexpr std::string_view kOpenNgcCsv = "Name;Type;RA;Dec\n"
                                         "NGC0224;G;00:42:44.35;+41:16:08.6\n";

constexpr std::array<unsigned char, 73> kHygGzip{
    0x1f, 0x8b, 0x08, 0x00, 0x77, 0xa9, 0x86, 0x69, 0x00, 0x03, 0xcb, 0x4c, 0xd1, 0xc9, 0xc8, 0x2c, 0xd0, 0x29, 0x28,
    0xca, 0x2f, 0x48, 0x2d, 0xd2, 0x29, 0x4a, 0xd4, 0x49, 0x49, 0x4d, 0xd6, 0xc9, 0x4d, 0x4c, 0xe7, 0x32, 0xd7, 0x31,
    0x31, 0xd2, 0x09, 0x49, 0x2d, 0x2e, 0x09, 0x2e, 0x49, 0x2c, 0xd2, 0x31, 0xd4, 0x33, 0x32, 0xd5, 0xd1, 0x35, 0xd2,
    0x33, 0xd5, 0x31, 0xd6, 0x33, 0xe2, 0x02, 0x00, 0xb9, 0xd5, 0xee, 0x71, 0x35, 0x00, 0x00, 0x00
};

constexpr std::array<unsigned char, 87> kOpenNgcGzip{
    0x1f, 0x8b, 0x08, 0x08, 0x12, 0x4c, 0xc2, 0x6a, 0x00, 0x03, 0x6f, 0x70, 0x65, 0x6e, 0x6e, 0x67, 0x63, 0x5f,
    0x6d, 0x69, 0x6e, 0x2e, 0x63, 0x73, 0x76, 0x00, 0xf3, 0x4b, 0xcc, 0x4d, 0xb5, 0x0e, 0xa9, 0x2c, 0x48, 0xb5,
    0x0e, 0x72, 0xb4, 0x76, 0x49, 0x4d, 0xe6, 0xf2, 0x73, 0x77, 0x36, 0x30, 0x32, 0x32, 0xb1, 0x76, 0xb7, 0x36,
    0x30, 0xb0, 0x32, 0x31, 0xb2, 0x32, 0x31, 0xd1, 0x33, 0x36, 0xb5, 0xd6, 0x36, 0x31, 0xb4, 0x32, 0x34, 0xb3,
    0x32, 0xb0, 0xd0, 0x33, 0xe3, 0x02, 0x00, 0x79, 0x89, 0xb1, 0x6a, 0x33, 0x00, 0x00, 0x00
};

// A gzip payload whose decoded content is the text "not a catalog".
constexpr std::array<unsigned char, 51> kUnknownInnerGzip{
    0x1f, 0x8b, 0x08, 0x08, 0x12, 0x4c, 0xc2, 0x6a, 0x00, 0x03, 0x75, 0x6e, 0x6b, 0x6e, 0x6f, 0x77, 0x6e,
    0x5f, 0x69, 0x6e, 0x6e, 0x65, 0x72, 0x2e, 0x74, 0x78, 0x74, 0x00, 0xcb, 0xcb, 0x2f, 0x51, 0x48, 0x54,
    0x48, 0x4e, 0x2c, 0x49, 0xcc, 0xc9, 0x4f, 0x07, 0x00, 0x3c, 0x5b, 0x2f, 0x94, 0x0d, 0x00, 0x00, 0x00
};

// A gzip payload whose decoded content is another gzip stream.
constexpr std::array<unsigned char, 72> kNestedGzip{
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x93, 0xef, 0xe6, 0x60, 0x00, 0x03, 0xe6, 0xd3,
    0x3e, 0x17, 0x35, 0xbd, 0xae, 0x78, 0x7a, 0xfa, 0x5e, 0x3b, 0xe9, 0xeb, 0xf3, 0xdc, 0xe8, 0x8a, 0xe1, 0x65,
    0x63, 0x73, 0x53, 0xa3, 0xab, 0x17, 0x4d, 0x59, 0x8c, 0x78, 0xce, 0xf2, 0x34, 0xf2, 0xd8, 0x0a, 0x1b, 0xeb,
    0x31, 0x28, 0xea, 0xe8, 0xa5, 0xaa, 0x01, 0x55, 0x03, 0x00, 0x21, 0x03, 0x23, 0x4a, 0x37, 0x00, 0x00, 0x00
};

[[nodiscard]] std::string_view view(const auto& bytes)
{
    return std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

[[nodiscard]] std::string verifyHygResult(const skygate::ephemeris::CatalogLoadResult& result)
{
    if (!result.isSuccess()) {
        return "expected successful HYG parse, got: " + result.errorDetail;
    }
    if (result.catalog == nullptr) {
        return "expected a non-null HYG catalog";
    }
    const auto bodies = result.catalog->bodies();
    if (bodies.size() != 1U) {
        return "expected one HYG body, got " + std::to_string(bodies.size());
    }
    const skygate::ephemeris::BaseCelestialBody* const body = bodies[0];
    if (body == nullptr || body->id != "hip_42") {
        return "expected HYG body hip_42";
    }
    if (!body->fixedEquatorialValue().has_value()) {
        return "expected HYG fixed equatorial coordinates";
    }
    const auto& equatorial = *body->fixedEquatorialValue();
    if (!skygate::ephemeris::tests::isNear(equatorial.rightAscensionHours, 1.25, 1e-8)
        || !skygate::ephemeris::tests::isNear(equatorial.declinationDeg, -2.5, 1e-8)) {
        return "unexpected HYG coordinates";
    }
    return {};
}

[[nodiscard]] std::string verifyOpenNgcResult(const skygate::ephemeris::CatalogLoadResult& result)
{
    if (!result.isSuccess()) {
        return "expected successful OpenNGC parse, got: " + result.errorDetail;
    }
    if (result.catalog == nullptr) {
        return "expected a non-null OpenNGC catalog";
    }
    const auto bodies = result.catalog->bodies();
    if (bodies.size() != 1U) {
        return "expected one OpenNGC body, got " + std::to_string(bodies.size());
    }
    const skygate::ephemeris::BaseCelestialBody* const body = bodies[0];
    if (body == nullptr || body->id != "ngc_224") {
        return "expected OpenNGC body ngc_224";
    }
    if (body->kind != skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject) {
        return "expected OpenNGC body to be a deep-sky object";
    }
    return {};
}

}  // namespace

class CatalogContainerDecodingTests final : public QObject {
    Q_OBJECT

private slots:
    void hygProducesEquivalentBodiesAcrossContainers();
    void openNgcProducesEquivalentBodiesAcrossContainers();
    void reportsUnknownInnerSchemaAfterSuccessfulContainerDecode();
    void rejectsNestedArchivesWithoutRecursiveDecoding();
    void rejectsArchiveEntriesBeyondDecompressionLimits();
    void unrelatedCsvBeforeCatalogIsIgnored();
    void selectsExplicitArchiveMember();
    void reportsAmbiguousArchiveMembers();
    void reportsMissingArchiveMember();
};

void CatalogContainerDecodingTests::hygProducesEquivalentBodiesAcrossContainers()
{
    using namespace skygate::ephemeris;

    const CatalogPayloadParser parser;
    const std::string zipData = tests::makeZip({
        tests::ZipEntrySpec{.path = "hyg.csv", .data = std::string(kHygCsv)},
    });

    const auto plainResult = parser.parseResult(kHygCsv);
    const auto gzipResult = parser.parseResult(view(kHygGzip));
    const auto zipResult = parser.parseResult(zipData);

    QCOMPARE(plainResult.detectedFormat, CatalogSourceType::HygCsv);
    QCOMPARE(gzipResult.detectedFormat, CatalogSourceType::HygCsv);
    QCOMPARE(zipResult.detectedFormat, CatalogSourceType::HygCsv);

    const std::string plainError = verifyHygResult(plainResult);
    const std::string gzipError = verifyHygResult(gzipResult);
    const std::string zipError = verifyHygResult(zipResult);
    QVERIFY2(plainError.empty(), plainError.c_str());
    QVERIFY2(gzipError.empty(), gzipError.c_str());
    QVERIFY2(zipError.empty(), zipError.c_str());
}

void CatalogContainerDecodingTests::openNgcProducesEquivalentBodiesAcrossContainers()
{
    using namespace skygate::ephemeris;

    const CatalogPayloadParser parser;
    const std::string zipData = tests::makeZip({
        tests::ZipEntrySpec{.path = "ngc.csv", .data = std::string(kOpenNgcCsv)},
    });

    const auto plainResult = parser.parseResult(kOpenNgcCsv);
    const auto gzipResult = parser.parseResult(view(kOpenNgcGzip));
    const auto zipResult = parser.parseResult(zipData);

    QCOMPARE(plainResult.detectedFormat, CatalogSourceType::OpenNgcCsv);
    QCOMPARE(gzipResult.detectedFormat, CatalogSourceType::OpenNgcCsv);
    QCOMPARE(zipResult.detectedFormat, CatalogSourceType::OpenNgcCsv);

    const std::string plainError = verifyOpenNgcResult(plainResult);
    const std::string gzipError = verifyOpenNgcResult(gzipResult);
    const std::string zipError = verifyOpenNgcResult(zipResult);
    QVERIFY2(plainError.empty(), plainError.c_str());
    QVERIFY2(gzipError.empty(), gzipError.c_str());
    QVERIFY2(zipError.empty(), zipError.c_str());
}

void CatalogContainerDecodingTests::reportsUnknownInnerSchemaAfterSuccessfulContainerDecode()
{
    using namespace skygate::ephemeris;

    const CatalogPayloadParser parser;
    const std::string zipData = tests::makeZip({
        tests::ZipEntrySpec{.path = "data.csv", .data = "not a catalog"},
    });

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog container decoded, but the inner payload is not a recognized catalog "
        "format."
    );
    const auto gzipResult = parser.parseResult(view(kUnknownInnerGzip));
    QVERIFY(!gzipResult.isSuccess());
    QCOMPARE(gzipResult.errorCode, CatalogLoadResult::ErrorCode::UnsupportedFormat);
    QCOMPARE(gzipResult.detectedFormat, CatalogSourceType::Unknown);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog container decoded, but the inner payload is not a recognized catalog "
        "format."
    );
    const auto zipResult = parser.parseResult(zipData);
    QVERIFY(!zipResult.isSuccess());
    QCOMPARE(zipResult.errorCode, CatalogLoadResult::ErrorCode::UnsupportedFormat);
    QCOMPARE(zipResult.detectedFormat, CatalogSourceType::Unknown);
}

void CatalogContainerDecodingTests::rejectsNestedArchivesWithoutRecursiveDecoding()
{
    using namespace skygate::ephemeris;

    const CatalogPayloadParser parser;
    const std::string zipWithGzipMember = tests::makeZip({
        tests::ZipEntrySpec{
            .path = "inner.gz",
            .data = std::string(view(kHygGzip)),
        },
    });

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog container decoded, but the inner payload is not a recognized catalog "
        "format."
    );
    const auto nestedGzipResult = parser.parseResult(view(kNestedGzip));
    QVERIFY(!nestedGzipResult.isSuccess());
    QCOMPARE(nestedGzipResult.errorCode, CatalogLoadResult::ErrorCode::UnsupportedFormat);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog container decoded, but the inner payload is not a recognized catalog "
        "format."
    );
    const auto nestedZipResult = parser.parseResult(zipWithGzipMember);
    QVERIFY(!nestedZipResult.isSuccess());
    QCOMPARE(nestedZipResult.errorCode, CatalogLoadResult::ErrorCode::UnsupportedFormat);
}

void CatalogContainerDecodingTests::rejectsArchiveEntriesBeyondDecompressionLimits()
{
    using namespace skygate::ephemeris;

    const std::string oversizedZip = tests::makeZip({
        tests::ZipEntrySpec{
            .path = "hyg.csv",
            .data = "xx",
            .compressionMethod = 8U,
            .uncompressedSize = std::size_t{1} << 30,
        },
    });

    const CatalogPayloadParser parser;
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog ZIP parse failed: ZIP catalog payload does not contain a readable CSV entry."
    );
    const auto result = parser.parseResult(oversizedZip);
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.errorCode, CatalogLoadResult::ErrorCode::InvalidZipData);
}

void CatalogContainerDecodingTests::unrelatedCsvBeforeCatalogIsIgnored()
{
    using namespace skygate::ephemeris;

    const std::string zipData = tests::makeZip({
        tests::ZipEntrySpec{.path = "readme.csv", .data = "not,a,catalog\n1,2,3\n"},
        tests::ZipEntrySpec{.path = "catalog/hyg.csv", .data = std::string(kHygCsv)},
    });

    const CatalogPayloadParser parser;
    const auto result = parser.parseResult(zipData);
    QCOMPARE(result.detectedFormat, CatalogSourceType::HygCsv);
    const std::string error = verifyHygResult(result);
    QVERIFY2(error.empty(), error.c_str());
}

void CatalogContainerDecodingTests::selectsExplicitArchiveMember()
{
    using namespace skygate::ephemeris;

    const std::string zipData = tests::makeZip({
        tests::ZipEntrySpec{.path = "readme.csv", .data = "not,a,catalog\n1,2,3\n"},
        tests::ZipEntrySpec{.path = "catalog/hyg.csv", .data = std::string(kHygCsv)},
    });

    const CatalogPayloadParser parser;
    const auto result = parser.parseResult(
        CatalogParseRequest{
            .payload = zipData,
            .memberSelector = std::string("catalog/hyg.csv"),
        }
    );
    QCOMPARE(result.detectedFormat, CatalogSourceType::HygCsv);
    const std::string error = verifyHygResult(result);
    QVERIFY2(error.empty(), error.c_str());
}

void CatalogContainerDecodingTests::reportsAmbiguousArchiveMembers()
{
    using namespace skygate::ephemeris;

    const std::string zipData = tests::makeZip({
        tests::ZipEntrySpec{.path = "a.csv", .data = std::string(kHygCsv)},
        tests::ZipEntrySpec{.path = "b.csv", .data = std::string(kHygCsv)},
    });

    const CatalogPayloadParser parser;
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog ZIP parse failed: ZIP catalog payload contains multiple supported catalog members."
    );
    const auto result = parser.parseResult(zipData);
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.errorCode, CatalogLoadResult::ErrorCode::AmbiguousArchiveMember);
}

void CatalogContainerDecodingTests::reportsMissingArchiveMember()
{
    using namespace skygate::ephemeris;

    const std::string zipData = tests::makeZip({
        tests::ZipEntrySpec{.path = "hyg.csv", .data = std::string(kHygCsv)},
    });

    const CatalogPayloadParser parser;
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog ZIP parse failed: ZIP catalog payload does not contain member 'nope.csv'."
    );
    const auto result = parser.parseResult(
        CatalogParseRequest{
            .payload = zipData,
            .memberSelector = std::string("nope.csv"),
        }
    );
    QVERIFY(!result.isSuccess());
    QCOMPARE(result.errorCode, CatalogLoadResult::ErrorCode::ArchiveMemberNotFound);
}

QTEST_APPLESS_MAIN(CatalogContainerDecodingTests)

#include "CatalogContainerDecodingTests.moc"
