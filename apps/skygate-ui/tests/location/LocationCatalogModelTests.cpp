#include "location/LocationCatalogModel.hpp"

#include <QtTest>

class LocationCatalogModelTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsBundledCatalog();
    void filtersByCityAndCountry();
    void groupsFilteredRowsByCountry();
    void resolvesCityByStableId();
    void includesRussiaAndMoscow();
    void skipsMalformedCatalogRows();
    void rejectsCatalogWithoutRequiredColumns();
};

void LocationCatalogModelTests::loadsBundledCatalog()
{
    LocationCatalogModel model;

    QVERIFY(model.rowCount() > 0);
    QVERIFY(model.hasCityId("ch-zurich"));
}

void LocationCatalogModelTests::filtersByCityAndCountry()
{
    LocationCatalogModel model;

    model.setFilterText("zurich");
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.index(1, 0).data(LocationCatalogModel::CityIdRole).toString(), QString("ch-zurich"));

    model.setFilterText("switzerland");
    QVERIFY(model.rowCount() >= 3);
    QCOMPARE(model.index(0, 0).data(LocationCatalogModel::DisplayTextRole).toString(), QString("Switzerland"));
}

void LocationCatalogModelTests::groupsFilteredRowsByCountry()
{
    LocationCatalogModel model;

    model.setFilterText("switzerland");
    QCOMPARE(model.index(0, 0).data(LocationCatalogModel::RowKindRole).toString(), QString("countryHeader"));
    QCOMPARE(model.index(1, 0).data(LocationCatalogModel::RowKindRole).toString(), QString("city"));
    QVERIFY(model.index(0, 0).data(LocationCatalogModel::SelectableRole).toBool() == false);
    QVERIFY(model.index(1, 0).data(LocationCatalogModel::SelectableRole).toBool());
}

void LocationCatalogModelTests::resolvesCityByStableId()
{
    LocationCatalogModel model;

    const auto cityEntry = model.entryForCityId("ch-zurich");
    QVERIFY(cityEntry.has_value());
    QCOMPARE(cityEntry->countryName, QString("Switzerland"));
    QCOMPARE(cityEntry->cityName, QString("Zurich"));
    QCOMPARE(cityEntry->latitudeDeg, 47.3769);
    QCOMPARE(cityEntry->longitudeDeg, 8.5417);
}

void LocationCatalogModelTests::includesRussiaAndMoscow()
{
    LocationCatalogModel model;

    QVERIFY(model.hasCityId("ru-moscow"));
    QVERIFY(model.hasCityId("ru-saint-petersburg"));

    model.setFilterText("russia");
    QVERIFY(model.rowCount() >= 4);
    QCOMPARE(model.index(0, 0).data(LocationCatalogModel::DisplayTextRole).toString(), QString("Russia"));

    const auto cityEntry = model.entryForCityId("ru-moscow");
    QVERIFY(cityEntry.has_value());
    QCOMPARE(cityEntry->countryName, QString("Russia"));
    QCOMPARE(cityEntry->cityName, QString("Moscow"));
}

void LocationCatalogModelTests::skipsMalformedCatalogRows()
{
    const QString csv = QStringLiteral(
        "id,countryName,cityName,latitudeDeg,longitudeDeg\n"
        "ch-zurich,Switzerland,Zurich,47.3769,8.5417\n"
        "ch-basel,Switzerland,Basel\n"
        "ch-bern,Switzerland,Bern,nan,7.4474\n"
        "ch-geneva,Switzerland,,46.2044,6.1432\n"
        "ch-lugano,Switzerland,Lugano,1e400,8.9511\n"
    );

    const auto entries = LocationCatalogModel::parseCatalog(csv);

    QVERIFY(entries.has_value());
    QCOMPARE(entries->size(), 1);
    QCOMPARE(entries->constFirst().id, QStringLiteral("ch-zurich"));
    QCOMPARE(entries->constFirst().latitudeDeg, 47.3769);
}

void LocationCatalogModelTests::rejectsCatalogWithoutRequiredColumns()
{
    const auto entries = LocationCatalogModel::parseCatalog(QStringLiteral("id,countryName\nch-zurich,Switzerland\n"));

    QVERIFY(!entries.has_value());
}

QTEST_GUILESS_MAIN(LocationCatalogModelTests)

#include "LocationCatalogModelTests.moc"
