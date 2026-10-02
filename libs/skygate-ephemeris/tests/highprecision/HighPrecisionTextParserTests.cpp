#include "text/HighPrecisionTextParser.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace skygate::ephemeris;

class HighPrecisionTextParserTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesBooleansStrictly();
    void preservesEmptyCommaSeparatedFields();
    void clipsFixedColumns();
    void consumesLinesPreservingWhitespace();
    void parsesAstronomicalUtcDates();
    void parsesIntegersStrictly();
    void parsesFiniteDoublesStrictly();
};

void HighPrecisionTextParserTests::parsesBooleansStrictly()
{
    const HighPrecisionTextParser parser;
    bool value = false;
    QVERIFY(parser.parseBool(" \ttrue\r\n", value));
    QVERIFY(value);
    QVERIFY(parser.parseBool(" 0 ", value));
    QVERIFY(!value);
    QVERIFY(parser.parseBool("1", value));
    QVERIFY(value);
    QVERIFY(parser.parseBool("false", value));
    QVERIFY(!value);

    for (const std::string_view text : {"", " ", "True", "FALSE", "yes", "2", "true trailing"}) {
        value = true;
        QVERIFY(!parser.parseBool(text, value));
        QVERIFY(value);
        value = false;
        QVERIFY(!parser.parseBool(text, value));
        QVERIFY(!value);
    }
}

void HighPrecisionTextParserTests::preservesEmptyCommaSeparatedFields()
{
    const HighPrecisionTextParser parser;
    const std::vector<std::string_view> expected{"", "first", "", "last", ""};
    QCOMPARE(parser.splitCommaSeparated(", first , \t, last ,"), expected);
    QCOMPARE(parser.splitCommaSeparated(""), std::vector<std::string_view>{""});
    QCOMPARE(parser.splitCommaSeparated(" \t "), std::vector<std::string_view>{""});
    const std::vector<std::string_view> quoted{"\"a", "b\"", "c"};
    QCOMPARE(parser.splitCommaSeparated("\"a,b\",c"), quoted);
}

void HighPrecisionTextParserTests::clipsFixedColumns()
{
    const HighPrecisionTextParser parser;
    const std::string_view line = "abc  42  ";
    QCOMPARE(parser.fixedColumn(line, 3U, 6U), std::string_view("42"));
    QCOMPARE(parser.fixedColumn(line, 3U, std::numeric_limits<std::size_t>::max()), std::string_view("42"));
    QVERIFY(parser.fixedColumn(line, 3U, 0U).empty());
    QVERIFY(parser.fixedColumn(line, line.size(), 1U).empty());
    QVERIFY(parser.fixedColumn(line, std::numeric_limits<std::size_t>::max(), 1U).empty());
    QVERIFY(parser.fixedColumn("", 0U, 10U).empty());
}

void HighPrecisionTextParserTests::consumesLinesPreservingWhitespace()
{
    const HighPrecisionTextParser parser;
    std::string_view remaining = "  first \t\r\n\nsecond\nlast\r";
    QCOMPARE(parser.takeLine(remaining), std::string_view("  first \t"));
    QCOMPARE(remaining, std::string_view("\nsecond\nlast\r"));
    QVERIFY(parser.takeLine(remaining).empty());
    QCOMPARE(parser.takeLine(remaining), std::string_view("second"));
    QCOMPARE(parser.takeLine(remaining), std::string_view("last"));
    QVERIFY(remaining.empty());
    QVERIFY(parser.takeLine(remaining).empty());
    remaining = "a\rb\n";
    QCOMPARE(parser.takeLine(remaining), std::string_view("a\rb"));
    QVERIFY(remaining.empty());
}

void HighPrecisionTextParserTests::parsesAstronomicalUtcDates()
{
    const HighPrecisionTextParser parser;
    const auto negativeYear = parser.parseUtcDate(" -44-03-15 ");
    QVERIFY(negativeYear.has_value());
    QCOMPARE(negativeYear->astronomicalYear, -44);
    QCOMPARE(negativeYear->month, 3);
    QCOMPARE(negativeYear->day, 15);
    QCOMPARE(negativeYear->timeScale, skygate::core::TimeScale::Utc);
    const auto yearZero = parser.parseUtcDate("0-02-29");
    QVERIFY(yearZero.has_value());
    QCOMPARE(yearZero->astronomicalYear, 0);
    QVERIFY(parser.parseUtcDate("2000-02-29").has_value());
    for (const std::string_view text :
         {"1900-02-29", "2001-02-29", "2024-13-01", "2024-01-00", "2024-01-01x", "2024-01", ""}) {
        QVERIFY(!parser.parseUtcDate(text).has_value());
    }
}

void HighPrecisionTextParserTests::parsesIntegersStrictly()
{
    const HighPrecisionTextParser parser;
    int value = 0;
    QVERIFY(parser.parseInt(" \t-42\n", value));
    QCOMPARE(value, -42);
    QVERIFY(parser.parseInt(std::to_string(std::numeric_limits<int>::max()), value));
    QCOMPARE(value, std::numeric_limits<int>::max());
    QVERIFY(parser.parseInt(std::to_string(std::numeric_limits<int>::min()), value));
    QCOMPARE(value, std::numeric_limits<int>::min());
    for (const std::string_view text : {"", " ", "+1", "1.0", "1x", "1 2", "999999999999999999999999"}) {
        QVERIFY(!parser.parseInt(text, value));
    }
    std::uint64_t unsignedValue = 0;
    QVERIFY(parser.parseUint64(" 18446744073709551615 ", unsignedValue));
    QCOMPARE(unsignedValue, std::numeric_limits<std::uint64_t>::max());
    for (const std::string_view text : {"", "-1", "+1", "12x", "18446744073709551616"}) {
        QVERIFY(!parser.parseUint64(text, unsignedValue));
    }
}

void HighPrecisionTextParserTests::parsesFiniteDoublesStrictly()
{
    const HighPrecisionTextParser parser;
    double value = 0.0;
    QVERIFY(parser.parseFiniteDouble(" \t-1.25e2\r\n", value));
    QCOMPARE(value, -125.0);
    for (const std::string_view text : {"", " ", "+1.0", "1.0x", "1 2", "nan", "inf", "-inf", "1e9999"}) {
        QVERIFY(!parser.parseFiniteDouble(text, value));
    }
}

QTEST_APPLESS_MAIN(HighPrecisionTextParserTests)

#include "HighPrecisionTextParserTests.moc"
