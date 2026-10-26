#include "AsciiText.hpp"

#include <QtTest/QtTest>

#include <string>
#include <string_view>

class AsciiTextTests final : public QObject {
    Q_OBJECT

private slots:
    void toLowerFoldsAsciiOnly();
    void normalizedLookupKeyTrimsAndFolds();
    void equalsIgnoreCaseComparesAsciiOnly();
};

void AsciiTextTests::toLowerFoldsAsciiOnly()
{
    using skygate::core::AsciiText;

    QCOMPARE(AsciiText::toLower(std::string_view("MiXeD_Case-42")), std::string("mixed_case-42"));
    QCOMPARE(AsciiText::toLower(std::string_view{}), std::string(""));
    QCOMPARE(AsciiText::toLower(std::string("\xC3\x89RI")), std::string("\xC3\x89ri"));
    QCOMPARE(AsciiText::toLower(std::string("\xD0\x97\xD0\x92")), std::string("\xD0\x97\xD0\x92"));
}

void AsciiTextTests::normalizedLookupKeyTrimsAndFolds()
{
    using skygate::core::AsciiText;

    QCOMPARE(AsciiText::normalizedLookupKey("  ORION  "), std::string("orion"));
    QCOMPARE(AsciiText::normalizedLookupKey(" \tM 31\n"), std::string("m 31"));
    QCOMPARE(AsciiText::normalizedLookupKey("America/Los_Angeles"), std::string("america/los_angeles"));
    // Only ASCII whitespace trims, so a no-break space stays part of the key.
    QCOMPARE(
        AsciiText::normalizedLookupKey(std::string("\xC2\xA0Orion\xC2\xA0")), std::string("\xC2\xA0orion\xC2\xA0")
    );
    QCOMPARE(AsciiText::normalizedLookupKey(std::string_view{}), std::string(""));
}

void AsciiTextTests::equalsIgnoreCaseComparesAsciiOnly()
{
    using skygate::core::AsciiText;

    QVERIFY(AsciiText::equalsIgnoreCase("ngc", "NGC"));
    QVERIFY(!AsciiText::equalsIgnoreCase("ngc", "ngc "));
    QVERIFY(AsciiText::equalsIgnoreCase(std::string("\xC3\x9C"), std::string("\xC3\x9C")));
    QVERIFY(!AsciiText::equalsIgnoreCase(std::string("\xC3\x9C"), std::string("\xC3\xBC")));
}

QTEST_APPLESS_MAIN(AsciiTextTests)

#include "AsciiTextTests.moc"
