#include "catalog/CatalogLoadResult.hpp"

#include <QtTest/QtTest>

#include <array>
#include <string_view>
#include <utility>

namespace {

[[nodiscard]] QString textOf(const std::string_view description)
{
    return QString::fromLatin1(description.data(), static_cast<qsizetype>(description.size()));
}

}  // namespace

class CatalogLoadResultTests final : public QObject {
    Q_OBJECT

private slots:
    void describeErrorCodeNamesEveryCode();
    void describeErrorCodeFallsBackToUnknownParseFailure();
};

void CatalogLoadResultTests::describeErrorCodeNamesEveryCode()
{
    using skygate::ephemeris::CatalogLoadResult;

    const std::array<std::pair<CatalogLoadResult::ErrorCode, QString>, 12> expectedTexts{{
        {CatalogLoadResult::ErrorCode::NoError, QStringLiteral("Unknown parse failure")},
        {CatalogLoadResult::ErrorCode::EmptyInput, QStringLiteral("empty payload")},
        {CatalogLoadResult::ErrorCode::UnsupportedFormat, QStringLiteral("unsupported format")},
        {CatalogLoadResult::ErrorCode::MissingRequiredColumns, QStringLiteral("missing required columns")},
        {CatalogLoadResult::ErrorCode::InvalidHygCsv, QStringLiteral("invalid HYG CSV payload")},
        {CatalogLoadResult::ErrorCode::InvalidOpenNgcCsv, QStringLiteral("invalid OpenNGC CSV payload")},
        {CatalogLoadResult::ErrorCode::InvalidGzipData, QStringLiteral("invalid gzip payload")},
        {CatalogLoadResult::ErrorCode::InvalidZipData, QStringLiteral("invalid ZIP payload")},
        {CatalogLoadResult::ErrorCode::NoBodies, QStringLiteral("catalog contains no usable bodies")},
        {CatalogLoadResult::ErrorCode::ArchiveMemberNotFound, QStringLiteral("requested archive member not found")},
        {CatalogLoadResult::ErrorCode::AmbiguousArchiveMember, QStringLiteral("multiple supported archive members")},
        {CatalogLoadResult::ErrorCode::SchemaHintMismatch, QStringLiteral("schema hint mismatch")},
    }};

    for (const auto& [errorCode, expectedText] : expectedTexts) {
        QCOMPARE(textOf(CatalogLoadResult::describeErrorCode(errorCode)), expectedText);
    }
}

void CatalogLoadResultTests::describeErrorCodeFallsBackToUnknownParseFailure()
{
    using skygate::ephemeris::CatalogLoadResult;

    const auto outOfRangeCode = static_cast<CatalogLoadResult::ErrorCode>(200);
    QCOMPARE(CatalogLoadResult::describeErrorCode(outOfRangeCode), std::string_view("Unknown parse failure"));
    QCOMPARE(
        CatalogLoadResult::describeErrorCode(outOfRangeCode),
        CatalogLoadResult::describeErrorCode(CatalogLoadResult::ErrorCode::NoError)
    );
}

QTEST_APPLESS_MAIN(CatalogLoadResultTests)

#include "CatalogLoadResultTests.moc"
