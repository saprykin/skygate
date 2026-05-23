#include "catalog/normalize/CatalogParsingUtilities.hpp"

#include <QtTest/QtTest>

#include <string>

class CatalogParsingUtilitiesTests final : public QObject {
    Q_OBJECT

private slots:
    void convertsQtTextToUtf8();
    void parsesFiniteAndPositiveDoubles();
    void parsesSexagesimalCoordinates();
    void validatesDecimalCoordinateRanges();
    void rejectsOutOfRangeSexagesimalCoordinates();
};

void CatalogParsingUtilitiesTests::convertsQtTextToUtf8()
{
    QCOMPARE(skygate::ephemeris::CatalogParsingUtilities::toUtf8String(QString::fromUtf8("M31")), std::string("M31"));
}

void CatalogParsingUtilitiesTests::parsesFiniteAndPositiveDoubles()
{
    using namespace skygate::ephemeris;

    QVERIFY(CatalogParsingUtilities::parseFiniteDouble(QStringLiteral(" 1.25 ")).has_value());
    QVERIFY(!CatalogParsingUtilities::parseFiniteDouble(QStringLiteral("nan")).has_value());
    QVERIFY(CatalogParsingUtilities::parsePositiveDouble(QStringLiteral("2.5")).has_value());
    QVERIFY(!CatalogParsingUtilities::parsePositiveDouble(QStringLiteral("-2.5")).has_value());
    QVERIFY(CatalogParsingUtilities::parseNonNegativeDouble(QStringLiteral("0")).has_value());
    QVERIFY(!CatalogParsingUtilities::parseNonNegativeDouble(QStringLiteral("-0.1")).has_value());
}

void CatalogParsingUtilitiesTests::parsesSexagesimalCoordinates()
{
    using namespace skygate::ephemeris;

    const auto ra = CatalogParsingUtilities::parseRightAscensionHours(QStringLiteral("01:30:00"));
    QVERIFY(ra.has_value());
    QCOMPARE(*ra, 1.5);

    const auto dec = CatalogParsingUtilities::parseDeclinationDeg(QStringLiteral("-02:30:00"));
    QVERIFY(dec.has_value());
    QCOMPARE(*dec, -2.5);
}

void CatalogParsingUtilitiesTests::validatesDecimalCoordinateRanges()
{
    using namespace skygate::ephemeris;

    QVERIFY(CatalogParsingUtilities::parseRightAscensionHours(QStringLiteral("23.999")).has_value());
    QVERIFY(!CatalogParsingUtilities::parseRightAscensionHours(QStringLiteral("24.0")).has_value());
    QVERIFY(!CatalogParsingUtilities::parseRightAscensionHours(QStringLiteral("-1.0")).has_value());
    QVERIFY(CatalogParsingUtilities::parseDeclinationDeg(QStringLiteral("90.0")).has_value());
    QVERIFY(CatalogParsingUtilities::parseDeclinationDeg(QStringLiteral("-90.0")).has_value());
    QVERIFY(!CatalogParsingUtilities::parseDeclinationDeg(QStringLiteral("91.0")).has_value());
    QVERIFY(!CatalogParsingUtilities::parseDeclinationDeg(QStringLiteral("-91.0")).has_value());
}

void CatalogParsingUtilitiesTests::rejectsOutOfRangeSexagesimalCoordinates()
{
    using namespace skygate::ephemeris;

    QVERIFY(!CatalogParsingUtilities::parseRightAscensionHours(QStringLiteral("24:00:00")).has_value());
    QVERIFY(!CatalogParsingUtilities::parseDeclinationDeg(QStringLiteral("+91:00:00")).has_value());
}

QTEST_APPLESS_MAIN(CatalogParsingUtilitiesTests)

#include "CatalogParsingUtilitiesTests.moc"
