#include "catalog/CatalogParsingUtilities.hpp"

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
    QCOMPARE(
        skygate::ephemeris::catalog_parsing::toUtf8String(QString::fromUtf8("M31")),
        std::string("M31")
    );
}

void CatalogParsingUtilitiesTests::parsesFiniteAndPositiveDoubles()
{
    using namespace skygate::ephemeris;

    QVERIFY(catalog_parsing::parseFiniteDouble(QStringLiteral(" 1.25 ")).has_value());
    QVERIFY(!catalog_parsing::parseFiniteDouble(QStringLiteral("nan")).has_value());
    QVERIFY(catalog_parsing::parsePositiveDouble(QStringLiteral("2.5")).has_value());
    QVERIFY(!catalog_parsing::parsePositiveDouble(QStringLiteral("-2.5")).has_value());
    QVERIFY(catalog_parsing::parseNonNegativeDouble(QStringLiteral("0")).has_value());
    QVERIFY(!catalog_parsing::parseNonNegativeDouble(QStringLiteral("-0.1")).has_value());
}

void CatalogParsingUtilitiesTests::parsesSexagesimalCoordinates()
{
    using namespace skygate::ephemeris;

    const auto ra = catalog_parsing::parseRightAscensionHours(QStringLiteral("01:30:00"));
    QVERIFY(ra.has_value());
    QCOMPARE(*ra, 1.5);

    const auto dec = catalog_parsing::parseDeclinationDeg(QStringLiteral("-02:30:00"));
    QVERIFY(dec.has_value());
    QCOMPARE(*dec, -2.5);
}

void CatalogParsingUtilitiesTests::validatesDecimalCoordinateRanges()
{
    using namespace skygate::ephemeris;

    QVERIFY(catalog_parsing::parseRightAscensionHours(QStringLiteral("23.999")).has_value());
    QVERIFY(!catalog_parsing::parseRightAscensionHours(QStringLiteral("24.0")).has_value());
    QVERIFY(!catalog_parsing::parseRightAscensionHours(QStringLiteral("-1.0")).has_value());
    QVERIFY(catalog_parsing::parseDeclinationDeg(QStringLiteral("90.0")).has_value());
    QVERIFY(catalog_parsing::parseDeclinationDeg(QStringLiteral("-90.0")).has_value());
    QVERIFY(!catalog_parsing::parseDeclinationDeg(QStringLiteral("91.0")).has_value());
    QVERIFY(!catalog_parsing::parseDeclinationDeg(QStringLiteral("-91.0")).has_value());
}

void CatalogParsingUtilitiesTests::rejectsOutOfRangeSexagesimalCoordinates()
{
    using namespace skygate::ephemeris;

    QVERIFY(!catalog_parsing::parseRightAscensionHours(QStringLiteral("24:00:00")).has_value());
    QVERIFY(!catalog_parsing::parseDeclinationDeg(QStringLiteral("+91:00:00")).has_value());
}

QTEST_APPLESS_MAIN(CatalogParsingUtilitiesTests)

#include "CatalogParsingUtilitiesTests.moc"
