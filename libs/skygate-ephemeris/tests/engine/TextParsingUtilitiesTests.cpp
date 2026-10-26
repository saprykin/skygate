#include "TextParsingUtilities.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace skygate::ephemeris;

class TextParsingUtilitiesTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesBooleansStrictly();
    void preservesEmptyCommaSeparatedFields();
    void clipsFixedColumns();
    void parsesAstronomicalUtcDates();
};

void TextParsingUtilitiesTests::parsesBooleansStrictly()
{
    bool value = false;
    QVERIFY(TextParsingUtilities::parseBool(" \ttrue\r\n", value));
    QVERIFY(value);
    QVERIFY(TextParsingUtilities::parseBool(" 0 ", value));
    QVERIFY(!value);
    QVERIFY(TextParsingUtilities::parseBool("1", value));
    QVERIFY(value);
    QVERIFY(TextParsingUtilities::parseBool("false", value));
    QVERIFY(!value);

    for (const std::string_view text : {"", " ", "True", "FALSE", "yes", "2", "true trailing"}) {
        value = true;
        QVERIFY(!TextParsingUtilities::parseBool(text, value));
        QVERIFY(value);
        value = false;
        QVERIFY(!TextParsingUtilities::parseBool(text, value));
        QVERIFY(!value);
    }
}

void TextParsingUtilitiesTests::preservesEmptyCommaSeparatedFields()
{
    const std::vector<std::string_view> expected{"", "first", "", "last", ""};
    QCOMPARE(TextParsingUtilities::splitCommaSeparated(", first , \t, last ,"), expected);
    QCOMPARE(TextParsingUtilities::splitCommaSeparated(""), std::vector<std::string_view>{""});
    QCOMPARE(TextParsingUtilities::splitCommaSeparated(" \t "), std::vector<std::string_view>{""});
    const std::vector<std::string_view> quoted{"\"a", "b\"", "c"};
    QCOMPARE(TextParsingUtilities::splitCommaSeparated("\"a,b\",c"), quoted);
}

void TextParsingUtilitiesTests::clipsFixedColumns()
{
    const std::string_view line = "abc  42  ";
    QCOMPARE(TextParsingUtilities::fixedColumn(line, 3U, 6U), std::string_view("42"));
    QCOMPARE(
        TextParsingUtilities::fixedColumn(line, 3U, std::numeric_limits<std::size_t>::max()), std::string_view("42")
    );
    QVERIFY(TextParsingUtilities::fixedColumn(line, 3U, 0U).empty());
    QVERIFY(TextParsingUtilities::fixedColumn(line, line.size(), 1U).empty());
    QVERIFY(TextParsingUtilities::fixedColumn(line, std::numeric_limits<std::size_t>::max(), 1U).empty());
    QVERIFY(TextParsingUtilities::fixedColumn("", 0U, 10U).empty());
}

void TextParsingUtilitiesTests::parsesAstronomicalUtcDates()
{
    const auto negativeYear = TextParsingUtilities::parseUtcDate(" -44-03-15 ");
    QVERIFY(negativeYear.has_value());
    QCOMPARE(negativeYear->astronomicalYear, -44);
    QCOMPARE(negativeYear->month, 3);
    QCOMPARE(negativeYear->day, 15);
    QCOMPARE(negativeYear->timeScale, skygate::core::TimeScale::Utc);
    const auto yearZero = TextParsingUtilities::parseUtcDate("0-02-29");
    QVERIFY(yearZero.has_value());
    QCOMPARE(yearZero->astronomicalYear, 0);
    QVERIFY(TextParsingUtilities::parseUtcDate("2000-02-29").has_value());
    for (const std::string_view text :
         {"1900-02-29", "2001-02-29", "2024-13-01", "2024-01-00", "2024-01-01x", "2024-01", ""}) {
        QVERIFY(!TextParsingUtilities::parseUtcDate(text).has_value());
    }
}

QTEST_APPLESS_MAIN(TextParsingUtilitiesTests)

#include "TextParsingUtilitiesTests.moc"
