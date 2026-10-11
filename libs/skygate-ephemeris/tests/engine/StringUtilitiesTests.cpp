#include "StringUtilities.hpp"

#include <QtTest/QtTest>

#include <string>
#include <string_view>
#include <vector>

class StringUtilitiesTests final : public QObject {
    Q_OBJECT

private slots:
    void normalizesAsciiStrings();
    void recognizesAsciiWhitespaceCharacters();
    void consumesLinesPreservingWhitespace();
    void appendsUniqueValuesIgnoringAsciiCase();
};

void StringUtilitiesTests::normalizesAsciiStrings()
{
    using namespace skygate::ephemeris;

    QCOMPARE(StringUtilities::toLowerAscii("AbC_123"), std::string("abc_123"));
    QCOMPARE(StringUtilities::trimAsciiWhitespace(" \tOrion\n "), std::string_view("Orion"));
    QVERIFY(StringUtilities::equalsIgnoreAsciiCase("HIP_42", "hip_42"));
    QVERIFY(StringUtilities::containsIgnoreAsciiCase("Name;Type;RA;Dec", "type"));
    QCOMPARE(StringUtilities::normalizedLookupKey("  ORION  "), std::string("orion"));
    QCOMPARE(StringUtilities::normalizedAlnumKey("NGC 0224"), std::string("ngc0224"));
}

void StringUtilitiesTests::recognizesAsciiWhitespaceCharacters()
{
    using namespace skygate::ephemeris;

    QVERIFY(StringUtilities::isAsciiWhitespace(' '));
    QVERIFY(StringUtilities::isAsciiWhitespace('\t'));
    QVERIFY(StringUtilities::isAsciiWhitespace('\n'));
    QVERIFY(StringUtilities::isAsciiWhitespace('\r'));
    QVERIFY(StringUtilities::isAsciiWhitespace('\v'));
    QVERIFY(StringUtilities::isAsciiWhitespace('\f'));
    QVERIFY(!StringUtilities::isAsciiWhitespace('A'));
    QVERIFY(!StringUtilities::isAsciiWhitespace('\0'));
}

void StringUtilitiesTests::consumesLinesPreservingWhitespace()
{
    using namespace skygate::ephemeris;

    std::string_view remaining = "  first \t\r\n\nsecond\nlast\r";
    QCOMPARE(StringUtilities::takeLine(remaining), std::string_view("  first \t"));
    QCOMPARE(remaining, std::string_view("\nsecond\nlast\r"));
    QVERIFY(StringUtilities::takeLine(remaining).empty());
    QCOMPARE(StringUtilities::takeLine(remaining), std::string_view("second"));
    QCOMPARE(StringUtilities::takeLine(remaining), std::string_view("last"));
    QVERIFY(remaining.empty());
    QVERIFY(StringUtilities::takeLine(remaining).empty());
    remaining = "a\rb\n";
    QCOMPARE(StringUtilities::takeLine(remaining), std::string_view("a\rb"));
    QVERIFY(remaining.empty());
}

void StringUtilitiesTests::appendsUniqueValuesIgnoringAsciiCase()
{
    using namespace skygate::ephemeris;

    std::vector<std::string> values{"M 31"};
    QVERIFY(!StringUtilities::appendUniqueIgnoreAsciiCase(values, "m 31"));
    QVERIFY(StringUtilities::appendUniqueIgnoreAsciiCase(values, "NGC 224"));
    QCOMPARE(values.size(), 2U);
}

QTEST_APPLESS_MAIN(StringUtilitiesTests)

#include "StringUtilitiesTests.moc"
