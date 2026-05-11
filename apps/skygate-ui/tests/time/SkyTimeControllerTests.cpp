#include <QtTest>

#include "SkyTimeController.hpp"

#include "skygate/core/ITimeSource.hpp"

#include <QSignalSpy>
#include <QTimeZone>

#include <chrono>

namespace {

constexpr qint64 kFixedUtcEpochSeconds = 1'778'064'600;

bool isExpectedZurichLabel(const QString& label, const QString& abbreviation, const QString& offset)
{
    return label == abbreviation || label == offset;
}

class FixedTimeSource final : public skygate::core::ITimeSource {
public:
    [[nodiscard]] skygate::core::UtcTimePoint nowUtc() const noexcept override
    {
        return skygate::core::UtcTimePoint(std::chrono::seconds(kFixedUtcEpochSeconds));
    }
};

}  // namespace

class SkyTimeControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void usesInjectedTimeSourceForInitialUtc();
    void defaultsToSystemTimeZoneOrUtc();
    void rejectsInvalidTimeZoneIds();
    void changingTimeZonePreservesUtcInstant();
    void formatsSeasonalDstLabels();
    void parsesSelectedZoneWallTimeToUtc();
    void validatesBceInput();
    void emitsSignalsOnlyWhenValuesChange();
};

void SkyTimeControllerTests::usesInjectedTimeSourceForInitialUtc()
{
    FixedTimeSource timeSource;
    SkyTimeController controller(timeSource);

    QCOMPARE(
        controller.utcDateTime(),
        QDateTime::fromSecsSinceEpoch(kFixedUtcEpochSeconds, QTimeZone::UTC)
    );
}

void SkyTimeControllerTests::defaultsToSystemTimeZoneOrUtc()
{
    SkyTimeController controller;
    const QByteArray systemId = QTimeZone::systemTimeZoneId();
    const QString expectedId = !systemId.isEmpty() && QTimeZone::isTimeZoneIdAvailable(systemId)
        ? QString::fromUtf8(systemId)
        : QStringLiteral("UTC");
    QCOMPARE(controller.timeZoneId(), expectedId);
    QVERIFY(!controller.dateText().isEmpty());
    QVERIFY(!controller.timeText().isEmpty());
}

void SkyTimeControllerTests::rejectsInvalidTimeZoneIds()
{
    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("Europe/Zurich")));
    const QString previousId = controller.timeZoneId();
    QSignalSpy timeZoneSpy(&controller, &SkyTimeController::timeZoneChanged);

    QVERIFY(!controller.setTimeZoneId(QStringLiteral("Skygate/InvalidZone")));

    QCOMPARE(controller.timeZoneId(), previousId);
    QCOMPARE(timeZoneSpy.count(), 0);
}

void SkyTimeControllerTests::changingTimeZonePreservesUtcInstant()
{
    SkyTimeController controller;
    controller.setUtcDateTime(QDateTime::fromSecsSinceEpoch(1'778'064'600, QTimeZone::UTC));
    const QDateTime previousUtc = controller.utcDateTime();

    QVERIFY(controller.setTimeZoneId(QStringLiteral("Asia/Bishkek")));

    QCOMPARE(controller.utcDateTime(), previousUtc);
    QCOMPARE(controller.timeZoneId(), QStringLiteral("Asia/Bishkek"));
}

void SkyTimeControllerTests::formatsSeasonalDstLabels()
{
    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("Europe/Zurich")));

    controller.setUtcDateTime(QDateTime(QDate(2026, 1, 15), QTime(12, 0, 0), QTimeZone::UTC));
    QVERIFY(isExpectedZurichLabel(
        controller.timeZoneLabel(),
        QStringLiteral("CET"),
        QStringLiteral("UTC+01:00")
    ));
    QCOMPARE(controller.dateText(), QStringLiteral("2026-01-15"));
    QCOMPARE(controller.timeText(), QStringLiteral("13:00:00"));

    controller.setUtcDateTime(QDateTime(QDate(2026, 7, 15), QTime(12, 0, 0), QTimeZone::UTC));
    QVERIFY(isExpectedZurichLabel(
        controller.timeZoneLabel(),
        QStringLiteral("CEST"),
        QStringLiteral("UTC+02:00")
    ));
    QCOMPARE(controller.timeText(), QStringLiteral("14:00:00"));
}

void SkyTimeControllerTests::parsesSelectedZoneWallTimeToUtc()
{
    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("Europe/Zurich")));
    connect(
        &controller,
        &SkyTimeController::utcDateTimeChangeRequested,
        &controller,
        &SkyTimeController::setUtcDateTime
    );

    QVERIFY(controller.setDateTimeText(QStringLiteral("2026-05-06"), QStringLiteral("09:30:30")));

    QCOMPARE(
        controller.utcDateTime(),
        QDateTime(QDate(2026, 5, 6), QTime(7, 30, 30), QTimeZone::UTC)
    );
    QCOMPARE(controller.dateText(), QStringLiteral("2026-05-06"));
    QCOMPARE(controller.timeText(), QStringLiteral("09:30:30"));
}

void SkyTimeControllerTests::validatesBceInput()
{
    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("UTC")));
    connect(
        &controller,
        &SkyTimeController::utcDateTimeChangeRequested,
        &controller,
        &SkyTimeController::setUtcDateTime
    );

    QCOMPARE(
        controller.validateDateTimeText(QStringLiteral("0000-01-01"), QStringLiteral("00:00:00")),
        QStringLiteral("Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE.")
    );
    QVERIFY(!controller.setDateTimeText(QStringLiteral("0000-01-01"), QStringLiteral("00:00:00")));

    QVERIFY(controller.setDateTimeText(QStringLiteral("0044-03-15 BCE"), QStringLiteral("12:00:00")));
    QCOMPARE(controller.dateText(), QStringLiteral("0044-03-15 BCE"));
    QCOMPARE(controller.timeText(), QStringLiteral("12:00:00"));

    QVERIFY(controller.setDateTimeText(QStringLiteral("0044-03-15 BC"), QStringLiteral("12:00:00")));
    QCOMPARE(controller.dateText(), QStringLiteral("0044-03-15 BCE"));
}

void SkyTimeControllerTests::emitsSignalsOnlyWhenValuesChange()
{
    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("UTC")));
    controller.setUtcDateTime(QDateTime(QDate(2026, 5, 6), QTime(9, 30, 30), QTimeZone::UTC));

    QSignalSpy dateSpy(&controller, &SkyTimeController::dateTextChanged);
    QSignalSpy timeSpy(&controller, &SkyTimeController::timeTextChanged);
    QSignalSpy zoneSpy(&controller, &SkyTimeController::timeZoneChanged);

    controller.setUtcDateTime(QDateTime(QDate(2026, 5, 6), QTime(9, 30, 30), QTimeZone::UTC));
    QVERIFY(controller.setTimeZoneId(QStringLiteral("UTC")));
    QVERIFY(!controller.setTimeZoneId(QStringLiteral("Invalid/Zone")));
    QCOMPARE(dateSpy.count(), 0);
    QCOMPARE(timeSpy.count(), 0);
    QCOMPARE(zoneSpy.count(), 0);

    controller.setUtcDateTime(QDateTime(QDate(2026, 5, 6), QTime(9, 30, 31), QTimeZone::UTC));
    QCOMPARE(dateSpy.count(), 0);
    QCOMPARE(timeSpy.count(), 1);

    QVERIFY(controller.setTimeZoneId(QStringLiteral("Europe/Zurich")));
    QCOMPARE(zoneSpy.count(), 1);
    QVERIFY(dateSpy.count() <= 1);
}

QTEST_APPLESS_MAIN(SkyTimeControllerTests)

#include "SkyTimeControllerTests.moc"
