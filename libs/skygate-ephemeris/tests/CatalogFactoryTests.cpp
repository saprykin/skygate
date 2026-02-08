#include "TestHelpers.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QtTest/QtTest>

#include <cstddef>

class CatalogFactoryTests final : public QObject {
    Q_OBJECT

private slots:
    void createsBundledCatalogBySourceType();
    void createsRowsCatalogBySourceRequest();
    void createsHygCatalogBySourceTypeWithProgress();
};

void CatalogFactoryTests::createsBundledCatalogBySourceType()
{
    const auto catalog = skygate::ephemeris::createStarCatalog(
        skygate::ephemeris::CatalogSourceType::Bundled
    );
    QVERIFY(catalog != nullptr);
    QVERIFY(catalog->bodies().size() == 27U);
}

void CatalogFactoryTests::createsRowsCatalogBySourceRequest()
{
    const auto catalog = skygate::ephemeris::createStarCatalog(
        skygate::ephemeris::CatalogSourceRequest {
            .type = skygate::ephemeris::CatalogSourceType::Rows,
            .data = "demo_star|Demo Star|Star|4.0|12.5|-30.0\n"
        }
    );
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 1U);
    QVERIFY(bodies[0].id == "demo_star");
    QVERIFY(bodies[0].fixedEquatorial.has_value());
}

void CatalogFactoryTests::createsHygCatalogBySourceTypeWithProgress()
{
    std::size_t callbackLastCount = 0;
    const auto catalog = skygate::ephemeris::createStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,hip,proper,ra,dec,mag\n"
        "1,11,Alpha,1.0,2.0,3.0\n"
        "2,22,Beta,4.0,5.0,6.0\n",
        [&callbackLastCount](const std::size_t parsedObjectCount) {
            callbackLastCount = parsedObjectCount;
        }
    );
    QVERIFY(catalog != nullptr);
    QVERIFY(catalog->bodies().size() == 2U);
    QVERIFY(callbackLastCount == 2U);
}

QTEST_APPLESS_MAIN(CatalogFactoryTests)

#include "CatalogFactoryTests.moc"
