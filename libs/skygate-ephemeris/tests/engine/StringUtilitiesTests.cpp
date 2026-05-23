#include "StringUtilities.hpp"

#include <QtTest/QtTest>

#include <string>
#include <string_view>
#include <vector>

class StringUtilitiesTests final : public QObject {
    Q_OBJECT

private slots:
    void normalizesAsciiStrings();
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
