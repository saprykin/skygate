#include <QtTest>

#include "TimeZoneAliasCatalog.hpp"
#include "TimeZoneCatalogModel.hpp"

#include <QTimeZone>

namespace {

bool containsTimeZoneId(const TimeZoneCatalogModel& model, const QString& timeZoneId)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (
            model.index(row, 0).data(TimeZoneCatalogModel::TimeZoneIdRole).toString()
            == timeZoneId
        ) {
            return true;
        }
    }
    return false;
}

} // namespace

class TimeZoneCatalogModelTests final : public QObject {
    Q_OBJECT

private slots:
    void aliasSourceCoversCommonUserTerms();
    void loadsQtTimeZonesAndUtc();
    void exposesRequiredRoles();
    void filtersByIanaIdLabelAndOffset();
    void filteredRowsFollowReferenceInstant();
    void detailReflectsReferenceInstant();
};

void TimeZoneCatalogModelTests::aliasSourceCoversCommonUserTerms()
{
    QVERIFY(TimeZoneAliasCatalog::aliasesForTimeZoneId(
        QStringLiteral("America/Los_Angeles")
    ).contains(QStringLiteral("PST")));
    QVERIFY(TimeZoneAliasCatalog::aliasesForTimeZoneId(
        QStringLiteral("America/New_York")
    ).contains(QStringLiteral("Eastern Time")));
    QVERIFY(TimeZoneAliasCatalog::aliasesForTimeZoneId(
        QStringLiteral("Asia/Kolkata")
    ).contains(QStringLiteral("Indian Standard Time")));
    QVERIFY(TimeZoneAliasCatalog::aliasesForTimeZoneId(
        QStringLiteral("Asia/Shanghai")
    ).contains(QStringLiteral("China Standard Time")));
}

void TimeZoneCatalogModelTests::loadsQtTimeZonesAndUtc()
{
    TimeZoneCatalogModel model;

    QVERIFY(model.rowCount() > 0);
    QVERIFY(model.hasTimeZoneId(QStringLiteral("UTC")));
    QVERIFY(model.hasTimeZoneId(QStringLiteral("Europe/Zurich")));
}

void TimeZoneCatalogModelTests::exposesRequiredRoles()
{
    TimeZoneCatalogModel model;
    model.setFilterText(QStringLiteral("UTC"));
    QVERIFY(model.rowCount() >= 1);

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(index.data(TimeZoneCatalogModel::RowKindRole).toString(), QStringLiteral("timeZone"));
    QCOMPARE(index.data(TimeZoneCatalogModel::DisplayTextRole).toString(), QStringLiteral("UTC"));
    QCOMPARE(index.data(TimeZoneCatalogModel::TimeZoneIdRole).toString(), QStringLiteral("UTC"));
    QVERIFY(index.data(TimeZoneCatalogModel::SelectableRole).toBool());
    QVERIFY(index.data(TimeZoneCatalogModel::DetailTextRole).toString().contains("UTC"));
}

void TimeZoneCatalogModelTests::filtersByIanaIdLabelAndOffset()
{
    TimeZoneCatalogModel model;
    model.setReferenceUtcDateTime(QDateTime(QDate(2026, 7, 15), QTime(12, 0, 0), QTimeZone::UTC));

    model.setFilterText(QStringLiteral("zurich"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(
        model.index(0, 0).data(TimeZoneCatalogModel::TimeZoneIdRole).toString(),
        QStringLiteral("Europe/Zurich")
    );

    model.setFilterText(QStringLiteral("CEST"));
    QVERIFY(model.rowCount() >= 1);

    model.setFilterText(QStringLiteral("UTC+02:00"));
    QVERIFY(model.rowCount() >= 1);

    model.setFilterText(QStringLiteral("Los Angeles"));
    QVERIFY(containsTimeZoneId(model, QStringLiteral("America/Los_Angeles")));

    model.setFilterText(QStringLiteral("Los-Angeles"));
    QVERIFY(containsTimeZoneId(model, QStringLiteral("America/Los_Angeles")));

    model.setFilterText(QStringLiteral("PDT"));
    QVERIFY(containsTimeZoneId(model, QStringLiteral("America/Los_Angeles")));

    model.setFilterText(QStringLiteral("PST"));
    QVERIFY(containsTimeZoneId(model, QStringLiteral("America/Los_Angeles")));

    model.setFilterText(QStringLiteral("Eastern Time"));
    QVERIFY(containsTimeZoneId(model, QStringLiteral("America/New_York")));

    model.setFilterText(QStringLiteral("IST"));
    const QString indiaTimeZoneId = model.hasTimeZoneId(QStringLiteral("Asia/Kolkata"))
        ? QStringLiteral("Asia/Kolkata")
        : QStringLiteral("Asia/Calcutta");
    QVERIFY(containsTimeZoneId(model, indiaTimeZoneId));

    model.setFilterText(QStringLiteral("CST"));
    QVERIFY(containsTimeZoneId(model, QStringLiteral("America/Chicago")));
    QVERIFY(containsTimeZoneId(model, QStringLiteral("Asia/Shanghai")));
}

void TimeZoneCatalogModelTests::filteredRowsFollowReferenceInstant()
{
    TimeZoneCatalogModel model;
    model.setReferenceUtcDateTime(QDateTime(QDate(2026, 1, 15), QTime(12, 0, 0), QTimeZone::UTC));
    model.setFilterText(QStringLiteral("UTC+02:00"));
    QVERIFY(!containsTimeZoneId(model, QStringLiteral("Europe/Zurich")));

    model.setReferenceUtcDateTime(QDateTime(QDate(2026, 7, 15), QTime(12, 0, 0), QTimeZone::UTC));
    QVERIFY(containsTimeZoneId(model, QStringLiteral("Europe/Zurich")));
}

void TimeZoneCatalogModelTests::detailReflectsReferenceInstant()
{
    TimeZoneCatalogModel model;

    model.setReferenceUtcDateTime(QDateTime(QDate(2026, 1, 15), QTime(12, 0, 0), QTimeZone::UTC));
    const QString winterDetail = model.detailTextForTimeZoneId(QStringLiteral("Europe/Zurich"));
    QVERIFY(winterDetail.contains(QStringLiteral("CET")));
    QVERIFY(winterDetail.contains(QStringLiteral("UTC+01:00")));

    model.setReferenceUtcDateTime(QDateTime(QDate(2026, 7, 15), QTime(12, 0, 0), QTimeZone::UTC));
    const QString summerDetail = model.detailTextForTimeZoneId(QStringLiteral("Europe/Zurich"));
    QVERIFY(summerDetail.contains(QStringLiteral("CEST")));
    QVERIFY(summerDetail.contains(QStringLiteral("UTC+02:00")));
}

QTEST_APPLESS_MAIN(TimeZoneCatalogModelTests)

#include "TimeZoneCatalogModelTests.moc"
