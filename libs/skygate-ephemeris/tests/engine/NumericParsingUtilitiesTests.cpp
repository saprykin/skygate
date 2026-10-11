#include "NumericParsingUtilities.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

using namespace skygate::ephemeris;

class NumericParsingUtilitiesTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesFiniteDoublesStrictly();
    void parsesIntegersStrictly();
    void parsesUnsignedIntegersStrictly();
};

void NumericParsingUtilitiesTests::parsesFiniteDoublesStrictly()
{
    const std::optional<double> value = NumericParsingUtilities::parseFiniteDouble(" \t-1.25e2\r\n");
    QVERIFY(value.has_value());
    QCOMPARE(*value, -125.0);
    for (const std::string_view text : {"", " ", "+1.0", "1.0x", "1 2", "nan", "inf", "-inf", "1e9999"}) {
        QVERIFY(!NumericParsingUtilities::parseFiniteDouble(text).has_value());
    }
}

void NumericParsingUtilitiesTests::parsesIntegersStrictly()
{
    const std::optional<int> value = NumericParsingUtilities::parseInt(" \t-42\n");
    QVERIFY(value.has_value());
    QCOMPARE(*value, -42);
    QVERIFY(NumericParsingUtilities::parseInt("0") == 0);
    QVERIFY(
        NumericParsingUtilities::parseInt(std::to_string(std::numeric_limits<int>::max()))
        == std::numeric_limits<int>::max()
    );
    QVERIFY(
        NumericParsingUtilities::parseInt(std::to_string(std::numeric_limits<int>::min()))
        == std::numeric_limits<int>::min()
    );
    for (const std::string_view text : {"", " ", "+1", "1.0", "1x", "1 2", "999999999999999999999999"}) {
        QVERIFY(!NumericParsingUtilities::parseInt(text).has_value());
    }
}

void NumericParsingUtilitiesTests::parsesUnsignedIntegersStrictly()
{
    QVERIFY(NumericParsingUtilities::parseUint64("0") == 0U);
    QVERIFY(
        NumericParsingUtilities::parseUint64(" 18446744073709551615 ") == std::numeric_limits<std::uint64_t>::max()
    );
    for (const std::string_view text : {"", " ", "-1", "+1", "12x", "1 2", "18446744073709551616"}) {
        QVERIFY(!NumericParsingUtilities::parseUint64(text).has_value());
    }
}

QTEST_APPLESS_MAIN(NumericParsingUtilitiesTests)

#include "NumericParsingUtilitiesTests.moc"
