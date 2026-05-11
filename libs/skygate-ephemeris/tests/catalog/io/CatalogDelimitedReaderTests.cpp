#include "catalog/io/CsvRowTokenizer.hpp"
#include "catalog/io/DelimitedCatalogReader.hpp"

#include <QtTest/QtTest>

#include <string>
#include <string_view>
#include <vector>

class CatalogDelimitedReaderTests final : public QObject {
    Q_OBJECT

private slots:
    void tokenizesQuotedRowsAndDecodesEscapes();
    void tokenizesSemicolonRowsWithoutSplittingQuotedSeparators();
    void decodesUnterminatedQuotesAsBestEffortField();
    void readerHandlesBomBlankRowsCrlfAndFinalLineWithoutNewline();
    void readerRejectsMissingRequiredColumns();
    void readerStopsOnHandlerFailureAndPreservesPartialResult();
};

void CatalogDelimitedReaderTests::tokenizesQuotedRowsAndDecodesEscapes()
{
    const QString row = QStringLiteral(R"(alpha,"beta, gamma","quoted ""value""",delta)");

    const QVector<QStringView> columns =
        skygate::ephemeris::CsvRowTokenizer::splitColumns(QStringView {row});

    QCOMPARE(columns.size(), 4);
    QCOMPARE(skygate::ephemeris::CsvRowTokenizer::decodeField(columns.at(0)), QString("alpha"));
    QCOMPARE(
        skygate::ephemeris::CsvRowTokenizer::decodeField(columns.at(1)),
        QString("beta, gamma")
    );
    QCOMPARE(
        skygate::ephemeris::CsvRowTokenizer::decodeField(columns.at(2)),
        QString("quoted \"value\"")
    );
    QCOMPARE(skygate::ephemeris::CsvRowTokenizer::decodeField(columns.at(3)), QString("delta"));
}

void CatalogDelimitedReaderTests::tokenizesSemicolonRowsWithoutSplittingQuotedSeparators()
{
    const QString row = QStringLiteral(R"(name;"M 42; NGC 1976";"Great ""Nebula""")");

    const QVector<QStringView> columns =
        skygate::ephemeris::CsvRowTokenizer::splitColumns(QStringView {row}, ';');

    QCOMPARE(columns.size(), 3);
    QCOMPARE(
        skygate::ephemeris::CsvRowTokenizer::decodeField(columns.at(1)),
        QString("M 42; NGC 1976")
    );
    QCOMPARE(
        skygate::ephemeris::CsvRowTokenizer::decodeField(columns.at(2)),
        QString("Great \"Nebula\"")
    );
}

void CatalogDelimitedReaderTests::decodesUnterminatedQuotesAsBestEffortField()
{
    const QString row = QStringLiteral(R"(alpha,"unterminated,beta)");

    const QVector<QStringView> columns =
        skygate::ephemeris::CsvRowTokenizer::splitColumns(QStringView {row});

    QCOMPARE(columns.size(), 2);
    QCOMPARE(
        skygate::ephemeris::CsvRowTokenizer::decodeField(columns.at(1)),
        QString("\"unterminated,beta")
    );
}

void CatalogDelimitedReaderTests::readerHandlesBomBlankRowsCrlfAndFinalLineWithoutNewline()
{
    skygate::ephemeris::DelimitedCatalogReaderOptions options;
    options.separator = ',';
    options.requiredColumns = {QStringLiteral("name"), QStringLiteral("value")};
    options.emptyInputDetail = "empty";
    options.missingColumnsDetail = "missing";
    options.rowLimitDetail = "too many";

    std::vector<QString> names;
    const std::string payload =
        "\xef\xbb\xbfname,value\r\n"
        "\r\n"
        "\"Alpha\",1\r\n"
        "\"Beta, Two\",2";
    const auto result = skygate::ephemeris::DelimitedCatalogReader::read(
        std::string_view(payload),
        options,
        [&names](
            const skygate::ephemeris::DelimitedCatalogReader::Row& row,
            skygate::ephemeris::CatalogBodyParseResult&
        ) {
            names.push_back(row.decodeColumn(QStringLiteral("NAME")));
            return true;
        }
    );

    QVERIFY(result.isSuccess());
    QCOMPARE(result.diagnostics.processedRowCount, 2U);
    QCOMPARE(names.size(), 2U);
    QCOMPARE(names.at(0), QString("Alpha"));
    QCOMPARE(names.at(1), QString("Beta, Two"));
}

void CatalogDelimitedReaderTests::readerRejectsMissingRequiredColumns()
{
    skygate::ephemeris::DelimitedCatalogReaderOptions options;
    options.requiredColumns = {QStringLiteral("name"), QStringLiteral("value")};
    options.emptyInputDetail = "empty";
    options.missingColumnsDetail = "missing value";

    const auto result = skygate::ephemeris::DelimitedCatalogReader::read(
        std::string_view("name\nAlpha\n"),
        options,
        {}
    );

    QCOMPARE(
        result.errorCode,
        skygate::ephemeris::CatalogLoadErrorCode::MissingRequiredColumns
    );
    QCOMPARE(result.errorDetail, std::string("missing value"));
}

void CatalogDelimitedReaderTests::readerStopsOnHandlerFailureAndPreservesPartialResult()
{
    skygate::ephemeris::DelimitedCatalogReaderOptions options;
    options.requiredColumns = {QStringLiteral("name"), QStringLiteral("value")};
    options.emptyInputDetail = "empty";
    options.missingColumnsDetail = "missing";
    int handledRows = 0;

    const auto result = skygate::ephemeris::DelimitedCatalogReader::read(
        std::string_view("name,value\nAlpha,1\nBeta,2\n"),
        options,
        [&handledRows](
            const skygate::ephemeris::DelimitedCatalogReader::Row&,
            skygate::ephemeris::CatalogBodyParseResult& parseResult
        ) {
            ++handledRows;
            parseResult.errorCode = skygate::ephemeris::CatalogLoadErrorCode::InvalidHygCsv;
            parseResult.errorDetail = "handler stopped";
            return false;
        }
    );

    QCOMPARE(handledRows, 1);
    QCOMPARE(result.errorCode, skygate::ephemeris::CatalogLoadErrorCode::InvalidHygCsv);
    QCOMPARE(result.errorDetail, std::string("handler stopped"));
}

QTEST_APPLESS_MAIN(CatalogDelimitedReaderTests)

#include "CatalogDelimitedReaderTests.moc"
