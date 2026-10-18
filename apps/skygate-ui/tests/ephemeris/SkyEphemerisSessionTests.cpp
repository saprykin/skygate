#include "UtcTimeCodec.hpp"
#include "ephemeris/SkyEphemerisSession.hpp"
#include "time/CalendarTime.hpp"
#include "time/EpochCodec.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <optional>

class SkyEphemerisSessionTests final : public QObject {
    Q_OBJECT

private slots:
    void requestContextDerivesEpochFromMicroseconds();
};

void SkyEphemerisSessionTests::requestContextDerivesEpochFromMicroseconds()
{
    SkyEphemerisSession session(nullptr, {});
    SkyEphemerisSession::RequestInputs inputs;
    inputs.observationContext.utcTime = skygate::core::UtcTimeCodec::fromEpochMicros(1'700'000'000'654'321LL);

    const SkyEphemerisSession::RequestContext context = session.requestContext(inputs);

    QCOMPARE(context.request.context.utcTime, inputs.observationContext.utcTime);
    QVERIFY(
        context.request.epoch == skygate::core::EpochCodec::epochFromUtcTimeExact(inputs.observationContext.utcTime)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(context.request.epoch.timeScale),
        static_cast<std::uint8_t>(skygate::core::TimeScale::Utc)
    );

    const std::optional<skygate::core::CivilDateTime> dateTime =
        skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(context.request.epoch);
    QVERIFY(dateTime.has_value());
    QCOMPARE(dateTime->astronomicalYear, 2023);
    QCOMPARE(dateTime->month, 11);
    QCOMPARE(dateTime->day, 14);
    QCOMPARE(dateTime->hour, 22);
    QCOMPARE(dateTime->minute, 13);
    QCOMPARE(dateTime->second, 20);
    const std::int64_t nanosecondDelta = static_cast<std::int64_t>(dateTime->nanosecond) - 654'321'000LL;
    QVERIFY(nanosecondDelta > -1'000 && nanosecondDelta < 1'000);
}

QTEST_GUILESS_MAIN(SkyEphemerisSessionTests)

#include "SkyEphemerisSessionTests.moc"
