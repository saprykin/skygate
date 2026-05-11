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

    QCOMPARE(strings::toLowerAscii("AbC_123"), std::string("abc_123"));
    QCOMPARE(strings::trimAsciiWhitespace(" \tOrion\n "), std::string_view("Orion"));
    QVERIFY(strings::equalsIgnoreAsciiCase("HIP_42", "hip_42"));
    QVERIFY(strings::containsIgnoreAsciiCase("Name;Type;RA;Dec", "type"));
    QCOMPARE(strings::normalizedLookupKey("  ORION  "), std::string("orion"));
    QCOMPARE(strings::normalizedAlnumKey("NGC 0224"), std::string("ngc0224"));
}

void StringUtilitiesTests::appendsUniqueValuesIgnoringAsciiCase()
{
    using namespace skygate::ephemeris;

    std::vector<std::string> values {"M 31"};
    QVERIFY(!strings::appendUniqueIgnoreAsciiCase(values, "m 31"));
    QVERIFY(strings::appendUniqueIgnoreAsciiCase(values, "NGC 224"));
    QCOMPARE(values.size(), 2U);
}

QTEST_APPLESS_MAIN(StringUtilitiesTests)

#include "StringUtilitiesTests.moc"
