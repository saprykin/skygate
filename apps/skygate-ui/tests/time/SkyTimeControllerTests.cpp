#include "ITimeSource.hpp"
#include "UtcTimeCodec.hpp"
#include "app/SkyContextUtcDateTimeTextCodec.hpp"
#include "time/SkyLiveClock.hpp"
#include "time/SkyQtTimeCodec.hpp"
#include "time/SkyTimeController.hpp"

#include <QSignalSpy>
#include <QTimeZone>
#include <QtTest>

#include <chrono>

namespace {

constexpr qint64 kFixedUtcEpochSeconds = 1'778'064'600;
constexpr qint64 kFixedUtcEpochMicros = kFixedUtcEpochSeconds * 1'000'000 + 123'000;

bool isExpectedZurichLabel(const QString& label, const QString& abbreviation, const QString& offset)
{
    return label == abbreviation || label == offset;
}

class FixedTimeSource final : public skygate::core::ITimeSource {
public:
    [[nodiscard]] skygate::core::UtcTimePoint nowUtc() const noexcept override
    {
        return skygate::core::UtcTimeCodec::fromEpochMicros(kFixedUtcEpochMicros);
    }
};

}  // namespace

class SkyTimeControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void usesInjectedTimeSourceForInitialUtc();
    void preservesInjectedMicrosecondTimePoint();
    void liveClockAdvancesFromMonotonicElapsedTime();
    void defaultsToSystemTimeZoneOrUtc();
    void initialTimeZoneIdFallsBackForUnavailableIds();
    void rejectsInvalidTimeZoneIds();
    void acceptsPaddedTimeZoneIds();
    void changingTimeZonePreservesUtcInstant();
    void formatsSeasonalDstLabels();
    void parsesSelectedZoneWallTimeToUtc();
    void parsesWallTimeAcrossDstGapAndOverlap();
    void validatesBceInput();
    void dateTimeTextCodecsReportExactMessages_data();
    void dateTimeTextCodecsReportExactMessages();
    void acceptsTrimmedDateTimeText();
    void formatsBceDateTimesWithSuffix();
    void dateTextCodecsAgreeOnValidDates();
    void invalidDateTimeKeepsZeroedDateText();
    void emitsSignalsOnlyWhenValuesChange();
};

void SkyTimeControllerTests::usesInjectedTimeSourceForInitialUtc()
{
    FixedTimeSource timeSource;
    SkyTimeController controller(timeSource);

    QCOMPARE(controller.utcDateTime(), QDateTime::fromMSecsSinceEpoch(kFixedUtcEpochMicros / 1000, QTimeZone::UTC));
}

void SkyTimeControllerTests::preservesInjectedMicrosecondTimePoint()
{
    FixedTimeSource timeSource;
    SkyTimeController controller(timeSource);

    QCOMPARE(skygate::core::UtcTimeCodec::toEpochMicros(controller.utcTimePoint()), kFixedUtcEpochMicros);
}

void SkyTimeControllerTests::liveClockAdvancesFromMonotonicElapsedTime()
{
    SkyLiveClock liveClock;
    const auto anchorUtc = skygate::core::UtcTimeCodec::fromEpochMicros(1'717'276'800'000'000LL);
    liveClock.start(anchorUtc);

    QTest::qWait(20);

    const auto elapsedMicros = skygate::core::UtcTimeCodec::toEpochMicros(liveClock.currentUtc())
                               - skygate::core::UtcTimeCodec::toEpochMicros(anchorUtc);
    QVERIFY(elapsedMicros >= 10'000);
    QVERIFY(elapsedMicros < 500'000);
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

void SkyTimeControllerTests::initialTimeZoneIdFallsBackForUnavailableIds()
{
    SkyTimeController controller;

    QVERIFY(controller.setInitialTimeZoneId(QStringLiteral(" Asia/Bishkek ")));
    QCOMPARE(controller.timeZoneId(), QStringLiteral("Asia/Bishkek"));

    // A present but unavailable id falls back to UTC rather than the system zone.
    QVERIFY(controller.setInitialTimeZoneId(QStringLiteral("Skygate/InvalidZone")));
    QCOMPARE(controller.timeZoneId(), QStringLiteral("UTC"));

    // A missing id follows the system zone, matching the constructor default.
    QVERIFY(controller.setInitialTimeZoneId(QString()));
    const QByteArray systemId = QTimeZone::systemTimeZoneId();
    const QString expectedId = !systemId.isEmpty() && QTimeZone::isTimeZoneIdAvailable(systemId)
                                   ? QString::fromUtf8(systemId)
                                   : QStringLiteral("UTC");
    QCOMPARE(controller.timeZoneId(), expectedId);
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

void SkyTimeControllerTests::acceptsPaddedTimeZoneIds()
{
    SkyTimeController controller;

    QVERIFY(controller.setTimeZoneId(QStringLiteral("  Europe/Zurich  ")));
    QCOMPARE(controller.timeZoneId(), QStringLiteral("Europe/Zurich"));
}

void SkyTimeControllerTests::changingTimeZonePreservesUtcInstant()
{
    SkyTimeController controller;
    controller.setUtcDateTime(QDateTime::fromSecsSinceEpoch(kFixedUtcEpochSeconds, QTimeZone::UTC));
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
    QVERIFY(isExpectedZurichLabel(controller.timeZoneLabel(), QStringLiteral("CET"), QStringLiteral("UTC+01:00")));
    QCOMPARE(controller.dateText(), QStringLiteral("2026-01-15"));
    QCOMPARE(controller.timeText(), QStringLiteral("13:00:00"));

    controller.setUtcDateTime(QDateTime(QDate(2026, 7, 15), QTime(12, 0, 0), QTimeZone::UTC));
    QVERIFY(isExpectedZurichLabel(controller.timeZoneLabel(), QStringLiteral("CEST"), QStringLiteral("UTC+02:00")));
    QCOMPARE(controller.timeText(), QStringLiteral("14:00:00"));
}

void SkyTimeControllerTests::parsesSelectedZoneWallTimeToUtc()
{
    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("Europe/Zurich")));
    connect(
        &controller, &SkyTimeController::utcDateTimeChangeRequested, &controller, &SkyTimeController::setUtcDateTime
    );

    QVERIFY(controller.setDateTimeText(QStringLiteral("2026-05-06"), QStringLiteral("09:30:30")));

    QCOMPARE(controller.utcDateTime(), QDateTime(QDate(2026, 5, 6), QTime(7, 30, 30), QTimeZone::UTC));
    QCOMPARE(controller.dateText(), QStringLiteral("2026-05-06"));
    QCOMPARE(controller.timeText(), QStringLiteral("09:30:30"));
}

void SkyTimeControllerTests::validatesBceInput()
{
    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("UTC")));
    connect(
        &controller, &SkyTimeController::utcDateTimeChangeRequested, &controller, &SkyTimeController::setUtcDateTime
    );

    QVERIFY(!controller.setDateTimeText(QStringLiteral("0000-01-01"), QStringLiteral("00:00:00")));

    QVERIFY(controller.setDateTimeText(QStringLiteral("0044-03-15 BCE"), QStringLiteral("12:00:00")));
    QCOMPARE(controller.dateText(), QStringLiteral("0044-03-15 BCE"));
    QCOMPARE(controller.timeText(), QStringLiteral("12:00:00"));

    QVERIFY(controller.setDateTimeText(QStringLiteral("0044-03-15 BC"), QStringLiteral("12:00:00")));
    QCOMPARE(controller.dateText(), QStringLiteral("0044-03-15 BCE"));
}

void SkyTimeControllerTests::parsesWallTimeAcrossDstGapAndOverlap()
{
    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("Europe/Zurich")));
    connect(
        &controller, &SkyTimeController::utcDateTimeChangeRequested, &controller, &SkyTimeController::setUtcDateTime
    );

    // 02:30 does not exist on the spring-forward day; Qt resolves it with the pre-transition offset.
    QVERIFY(controller.setDateTimeText(QStringLiteral("2026-03-29"), QStringLiteral("02:30:00")));
    QCOMPARE(controller.utcDateTime(), QDateTime(QDate(2026, 3, 29), QTime(1, 30, 0), QTimeZone::UTC));

    // 02:30 happens twice on the fall-back day; Qt resolves it to the first occurrence.
    QVERIFY(controller.setDateTimeText(QStringLiteral("2026-10-25"), QStringLiteral("02:30:00")));
    QCOMPARE(controller.utcDateTime(), QDateTime(QDate(2026, 10, 25), QTime(0, 30, 0), QTimeZone::UTC));

    QVERIFY(controller.setDateTimeText(QStringLiteral("2026-10-25"), QStringLiteral("03:00:00")));
    QCOMPARE(controller.utcDateTime(), QDateTime(QDate(2026, 10, 25), QTime(2, 0, 0), QTimeZone::UTC));
}

void SkyTimeControllerTests::dateTimeTextCodecsReportExactMessages_data()
{
    QTest::addColumn<QString>("dateText");
    QTest::addColumn<QString>("timeText");
    QTest::addColumn<QString>("controllerErrorText");
    QTest::addColumn<QString>("utcCodecErrorText");

    QTest::newRow("date syntax") << QStringLiteral("2026-5-6") << QStringLiteral("09:30:30")
                                 << QStringLiteral("Use YYYY-MM-DD for CE or YYYY-MM-DD BCE for ancient dates.")
                                 << QStringLiteral("Use YYYY-MM-DD for CE or YYYY-MM-DD BCE for ancient dates.");
    QTest::newRow("year zero") << QStringLiteral("0000-01-01") << QStringLiteral("00:00:00")
                               << QStringLiteral("Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE.")
                               << QStringLiteral("Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE.");
    QTest::newRow("calendar date") << QStringLiteral("2026-02-30") << QStringLiteral("09:30:30")
                                   << QStringLiteral("Enter a valid date.")
                                   << QStringLiteral("Enter a valid UTC date.");
    QTest::newRow("time syntax") << QStringLiteral("2026-05-06") << QStringLiteral("09:30")
                                 << QStringLiteral("Use HH:mm:ss for the time.")
                                 << QStringLiteral("Use HH:mm:ss for the UTC time.");
}

void SkyTimeControllerTests::dateTimeTextCodecsReportExactMessages()
{
    QFETCH(QString, dateText);
    QFETCH(QString, timeText);
    QFETCH(QString, controllerErrorText);
    QFETCH(QString, utcCodecErrorText);

    using skygate::ui::internal::SkyContextUtcDateTimeTextCodec;

    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("Europe/Zurich")));

    QCOMPARE(controller.validateDateTimeText(dateText, timeText), controllerErrorText);
    QCOMPARE(SkyContextUtcDateTimeTextCodec::parse(dateText, timeText).errorText, utcCodecErrorText);
}

void SkyTimeControllerTests::acceptsTrimmedDateTimeText()
{
    using skygate::ui::internal::SkyContextUtcDateTimeTextCodec;

    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("Europe/Zurich")));

    QVERIFY(controller.validateDateTimeText(QStringLiteral(" 2026-05-06 "), QStringLiteral(" 09:30:30 ")).isEmpty());
    QVERIFY(controller.validateDateTimeText(QStringLiteral("0044-03-15 BC"), QStringLiteral("12:00:00")).isEmpty());

    const auto parsed =
        SkyContextUtcDateTimeTextCodec::parse(QStringLiteral(" 0044-03-15 bc "), QStringLiteral(" 12:00:00 "));
    QVERIFY(parsed.isValid());
    QCOMPARE(parsed.utcDateTime, QDateTime(QDate(-44, 3, 15), QTime(12, 0, 0), QTimeZone::UTC));
}

void SkyTimeControllerTests::formatsBceDateTimesWithSuffix()
{
    SkyTimeController controller;
    QVERIFY(controller.setTimeZoneId(QStringLiteral("UTC")));
    controller.setUtcDateTime(QDateTime(QDate(-44, 3, 15), QTime(12, 0, 0), QTimeZone::UTC));

    QCOMPARE(controller.dateText(), QStringLiteral("0044-03-15 BCE"));
    QCOMPARE(controller.timeText(), QStringLiteral("12:00:00"));
    QCOMPARE(controller.formatUtcTime(controller.utcTimePoint()), QStringLiteral("0044-03-15 BCE 12:00:00 UTC"));

    const auto oneHourLater =
        SkyQtTimeCodec::toUtcTimePoint(QDateTime(QDate(-44, 3, 15), QTime(13, 0, 0), QTimeZone::UTC));
    QCOMPARE(
        controller.formatUtcTimeRange(controller.utcTimePoint(), oneHourLater),
        QStringLiteral("0044-03-15 BCE 12:00:00 / 13:00:00 UTC")
    );
}

void SkyTimeControllerTests::dateTextCodecsAgreeOnValidDates()
{
    using skygate::ui::internal::SkyContextUtcDateTimeTextCodec;

    const QList<QPair<QDate, QString>> cases = {
        {QDate(2026, 1, 2), QStringLiteral("2026-01-02")},
        {QDate(5, 1, 1), QStringLiteral("0005-01-01")},
        {QDate(44, 3, 15), QStringLiteral("0044-03-15")},
        {QDate(999, 12, 31), QStringLiteral("0999-12-31")},
        {QDate(-1, 12, 31), QStringLiteral("0001-12-31 BCE")},
        {QDate(-44, 3, 15), QStringLiteral("0044-03-15 BCE")},
        {QDate(10000, 1, 1), QStringLiteral("10000-01-01")},
    };

    for (const auto& [date, expectedText] : cases) {
        QCOMPARE(SkyQtTimeCodec::formatDateText(date), expectedText);
        QCOMPARE(
            SkyContextUtcDateTimeTextCodec::formatDate(QDateTime(date, QTime(12, 0, 0), QTimeZone::UTC)), expectedText
        );
    }
}

void SkyTimeControllerTests::invalidDateTimeKeepsZeroedDateText()
{
    // The support codec formats an invalid instant as the zeroed placeholder;
    // the Qt codec reports an invalid date as empty text.
    using skygate::ui::internal::SkyContextUtcDateTimeTextCodec;

    QCOMPARE(SkyContextUtcDateTimeTextCodec::formatDate(QDateTime()), QStringLiteral("0000-00-00"));
    QVERIFY(SkyQtTimeCodec::formatDateText(QDate()).isEmpty());
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
