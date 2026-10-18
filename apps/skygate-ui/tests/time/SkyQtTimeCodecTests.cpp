#include "time/EphemerisDateRange.hpp"
#include "time/SkyQtTimeCodec.hpp"

#include <QtTest>

#include <limits>

namespace {

[[nodiscard]] skygate::core::AstronomicalEpoch epoch(const double julianDate)
{
    return skygate::core::AstronomicalEpoch{.julianDatePart1 = julianDate, .julianDatePart2 = 0.0};
}

}  // namespace

class SkyQtTimeCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void formatsBceEpochDateText();
    void formatsDateRangeWithoutDisplayName();
    void formatsDateRangeWithDisplayName();
    void formatsNonFiniteEpochDateAsPlaceholder();
    void formatsUtcOffsetText();
};

void SkyQtTimeCodecTests::formatsBceEpochDateText()
{
    // Julian date 1721060.0 is noon on 1 January of astronomical year 0, which
    // historical dating calls 1 BCE.
    QCOMPARE(SkyQtTimeCodec::formatAstronomicalEpochDate(epoch(1721060.0)), QStringLiteral("0001-01-01 BCE"));
}

void SkyQtTimeCodecTests::formatsDateRangeWithoutDisplayName()
{
    const skygate::ephemeris::EphemerisDateRange range{
        .id = "earth-orientation",
        .displayName = {},
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QCOMPARE(SkyQtTimeCodec::formatDateRangeText(range), QStringLiteral("2000-01-01 to 2000-01-02"));
}

void SkyQtTimeCodecTests::formatsDateRangeWithDisplayName()
{
    const skygate::ephemeris::EphemerisDateRange range{
        .id = "planetary-kernel",
        .displayName = "DE441",
        .start = epoch(1721060.0),
        .end = epoch(2451545.0),
    };

    QCOMPARE(SkyQtTimeCodec::formatDateRangeText(range), QStringLiteral("DE441: 0001-01-01 BCE to 2000-01-01"));
}

void SkyQtTimeCodecTests::formatsNonFiniteEpochDateAsPlaceholder()
{
    QCOMPARE(
        SkyQtTimeCodec::formatAstronomicalEpochDate(epoch(std::numeric_limits<double>::quiet_NaN())),
        QStringLiteral("--")
    );
}

void SkyQtTimeCodecTests::formatsUtcOffsetText()
{
    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetText(0), QStringLiteral("+00:00"));
    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetText(5 * 3600 + 30 * 60), QStringLiteral("+05:30"));
    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetText(-(3 * 3600 + 30 * 60)), QStringLiteral("-03:30"));

    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetLabel(0), QStringLiteral("UTC+00:00"));
    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetLabel(-(3 * 3600 + 30 * 60)), QStringLiteral("UTC-03:30"));
}

QTEST_APPLESS_MAIN(SkyQtTimeCodecTests)

#include "SkyQtTimeCodecTests.moc"
