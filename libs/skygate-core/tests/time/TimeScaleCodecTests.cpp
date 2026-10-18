#include "time/TimeScale.hpp"
#include "time/TimeScaleCodec.hpp"

#include <QtTest/QtTest>

#include <array>
#include <optional>

namespace {

constexpr std::array<skygate::core::TimeScale, 5> kKnownScales{
    skygate::core::TimeScale::Utc,
    skygate::core::TimeScale::Tai,
    skygate::core::TimeScale::Tt,
    skygate::core::TimeScale::Tdb,
    skygate::core::TimeScale::Ut1,
};

}  // namespace

class TimeScaleCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void knownScalesAreRecognized();
    void rawValuesRoundTripEveryKnownScale();
    void fromRawRejectsUnknownValues();
    void toRawRejectsUnknownScales();
};

void TimeScaleCodecTests::knownScalesAreRecognized()
{
    for (const skygate::core::TimeScale timeScale : kKnownScales) {
        QVERIFY(skygate::core::TimeScaleCodec::isKnown(timeScale));
    }
    QVERIFY(!skygate::core::TimeScaleCodec::isKnown(static_cast<skygate::core::TimeScale>(5)));
    QVERIFY(!skygate::core::TimeScaleCodec::isKnown(static_cast<skygate::core::TimeScale>(200)));
}

void TimeScaleCodecTests::rawValuesRoundTripEveryKnownScale()
{
    for (const skygate::core::TimeScale timeScale : kKnownScales) {
        const int rawValue = skygate::core::TimeScaleCodec::toRaw(timeScale);
        QVERIFY(rawValue >= 0);

        const std::optional<skygate::core::TimeScale> parsed = skygate::core::TimeScaleCodec::fromRaw(rawValue);
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, timeScale);
    }
}

void TimeScaleCodecTests::fromRawRejectsUnknownValues()
{
    QVERIFY(!skygate::core::TimeScaleCodec::fromRaw(-1).has_value());
    QVERIFY(!skygate::core::TimeScaleCodec::fromRaw(5).has_value());
    QVERIFY(!skygate::core::TimeScaleCodec::fromRaw(256).has_value());
    QVERIFY(!skygate::core::TimeScaleCodec::fromRaw(1000).has_value());
}

void TimeScaleCodecTests::toRawRejectsUnknownScales()
{
    QCOMPARE(skygate::core::TimeScaleCodec::toRaw(static_cast<skygate::core::TimeScale>(5)), -1);
    QCOMPARE(skygate::core::TimeScaleCodec::toRaw(static_cast<skygate::core::TimeScale>(200)), -1);
}

QTEST_APPLESS_MAIN(TimeScaleCodecTests)

#include "TimeScaleCodecTests.moc"
