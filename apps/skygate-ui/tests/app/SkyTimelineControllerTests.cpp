#include "UtcTimeCodec.hpp"
#include "UtcTimePoint.hpp"
#include "app/SkyTimelineController.hpp"

#include <QtTest>

#include <chrono>

namespace {

using skygate::core::UtcTimePoint;

// 0001-01-01T00:00:00 UTC expressed in Unix seconds.
constexpr qint64 kFirstCeInstantEpochSeconds = -62'135'596'800;
constexpr double kUnitSpeedMultiplier = 1.0;
constexpr qint64 kCatchUpStepSeconds = 60;

UtcTimePoint utcAtSeconds(const qint64 epochSeconds)
{
    return UtcTimePoint{std::chrono::duration_cast<UtcTimePoint::duration>(std::chrono::seconds(epochSeconds))};
}

qint64 epochMicros(const UtcTimePoint& utc)
{
    return skygate::core::UtcTimeCodec::toEpochMicros(utc);
}

skygate::ui::internal::SkyTimelineController timelineController(const double speedMultiplier, const int stepSeconds)
{
    skygate::ui::internal::SkyTimelineController controller;
    static_cast<void>(controller.setSpeedMultiplier(speedMultiplier));
    static_cast<void>(controller.setStepSeconds(stepSeconds));
    controller.setCatchingUpToCurrentUtc(true);
    return controller;
}

}  // namespace

class SkyTimelineControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void passesClockTimeThroughWhenNotCatchingUp();
    void passesClockTimeThroughAtWallTime();
    void advancesOneStepAtUnitSpeed();
    void honorsFractionalSpeedMultiplier();
    void advancesByElapsedGapWhileCatchingUp();
    void appliesLargeStepWhileCatchingUp();
    void clampsAdvancementToWallTime();
    void usesMinimumOneSecondOfElapsedTime();
    void crossesBceBoundaryWhileCatchingUp();
    void returnsNoAdvancementWhenSpeedIsZero();
    void returnsNoAdvancementWhenStepIsZero();
};

void SkyTimelineControllerTests::passesClockTimeThroughWhenNotCatchingUp()
{
    skygate::ui::internal::SkyTimelineController controller;
    static_cast<void>(controller.setSpeedMultiplier(2.0));
    static_cast<void>(controller.setStepSeconds(600));

    const UtcTimePoint timelineUtc = utcAtSeconds(1000);
    const UtcTimePoint clockUtc = utcAtSeconds(1010);
    const auto result = controller.computeCatchUpAdvancement(timelineUtc, clockUtc, utcAtSeconds(900));

    QVERIFY(result.has_value());
    QCOMPARE(epochMicros(result->nextUtc), epochMicros(clockUtc));
    QVERIFY(!result->advancedWhileCatchingUp);
    QVERIFY(!result->catchingUpToCurrentUtc);
}

void SkyTimelineControllerTests::passesClockTimeThroughAtWallTime()
{
    auto controller = timelineController(1.0, 60);

    const UtcTimePoint timelineUtc = utcAtSeconds(1000);
    const UtcTimePoint clockUtc = utcAtSeconds(1005);
    const auto result = controller.computeCatchUpAdvancement(timelineUtc, clockUtc, utcAtSeconds(1000));

    QVERIFY(result.has_value());
    QCOMPARE(epochMicros(result->nextUtc), epochMicros(clockUtc));
    QVERIFY(!result->advancedWhileCatchingUp);
    QVERIFY(!result->catchingUpToCurrentUtc);
}

void SkyTimelineControllerTests::advancesOneStepAtUnitSpeed()
{
    auto controller = timelineController(1.0, 60);

    const UtcTimePoint timelineUtc = utcAtSeconds(1000);
    const auto result = controller.computeCatchUpAdvancement(timelineUtc, utcAtSeconds(1001), utcAtSeconds(2000));

    QVERIFY(result.has_value());
    QCOMPARE(epochMicros(result->nextUtc), epochMicros(timelineUtc) + 60'000'000);
    QVERIFY(result->advancedWhileCatchingUp);
    QVERIFY(result->catchingUpToCurrentUtc);
}

void SkyTimelineControllerTests::honorsFractionalSpeedMultiplier()
{
    auto controller = timelineController(0.5, 60);

    const UtcTimePoint timelineUtc = utcAtSeconds(1000);
    const auto result = controller.computeCatchUpAdvancement(timelineUtc, utcAtSeconds(1002), utcAtSeconds(5000));

    QVERIFY(result.has_value());
    QCOMPARE(epochMicros(result->nextUtc), epochMicros(timelineUtc) + 60'000'000);
    QVERIFY(result->advancedWhileCatchingUp);
    QVERIFY(result->catchingUpToCurrentUtc);
}

void SkyTimelineControllerTests::advancesByElapsedGapWhileCatchingUp()
{
    auto controller = timelineController(1.0, 1);

    const UtcTimePoint timelineUtc = utcAtSeconds(0);
    const auto result =
        controller.computeCatchUpAdvancement(timelineUtc, utcAtSeconds(250'000), utcAtSeconds(1'000'000));

    QVERIFY(result.has_value());
    QCOMPARE(epochMicros(result->nextUtc), 250'000'000'000);
    QVERIFY(result->advancedWhileCatchingUp);
    QVERIFY(result->catchingUpToCurrentUtc);
}

void SkyTimelineControllerTests::appliesLargeStepWhileCatchingUp()
{
    auto controller = timelineController(1.0, 3600);

    const UtcTimePoint timelineUtc = utcAtSeconds(0);
    const auto result = controller.computeCatchUpAdvancement(timelineUtc, utcAtSeconds(1), utcAtSeconds(1'000'000));

    QVERIFY(result.has_value());
    QCOMPARE(epochMicros(result->nextUtc), 3'600'000'000);
    QVERIFY(result->advancedWhileCatchingUp);
    QVERIFY(result->catchingUpToCurrentUtc);
}

void SkyTimelineControllerTests::clampsAdvancementToWallTime()
{
    auto controller = timelineController(1.0, 3600);

    const UtcTimePoint timelineUtc = utcAtSeconds(1000);
    const UtcTimePoint wallUtc = utcAtSeconds(1060);
    const auto result = controller.computeCatchUpAdvancement(timelineUtc, utcAtSeconds(1001), wallUtc);

    QVERIFY(result.has_value());
    QCOMPARE(epochMicros(result->nextUtc), epochMicros(wallUtc));
    QVERIFY(result->advancedWhileCatchingUp);
    QVERIFY(!result->catchingUpToCurrentUtc);
}

void SkyTimelineControllerTests::usesMinimumOneSecondOfElapsedTime()
{
    auto controller = timelineController(kUnitSpeedMultiplier, kCatchUpStepSeconds);

    const UtcTimePoint timelineUtc = utcAtSeconds(1000);
    const auto result = controller.computeCatchUpAdvancement(timelineUtc, timelineUtc, utcAtSeconds(2000));

    QVERIFY(result.has_value());
    QCOMPARE(epochMicros(result->nextUtc), epochMicros(timelineUtc) + 60'000'000);
    QVERIFY(result->advancedWhileCatchingUp);
    QVERIFY(result->catchingUpToCurrentUtc);
}

void SkyTimelineControllerTests::crossesBceBoundaryWhileCatchingUp()
{
    auto controller = timelineController(kUnitSpeedMultiplier, kCatchUpStepSeconds);

    const UtcTimePoint timelineUtc = utcAtSeconds(kFirstCeInstantEpochSeconds - 30);
    const auto result = controller.computeCatchUpAdvancement(
        timelineUtc, utcAtSeconds(kFirstCeInstantEpochSeconds - 29), utcAtSeconds(kFirstCeInstantEpochSeconds + 1000)
    );

    QVERIFY(result.has_value());
    QCOMPARE(epochMicros(result->nextUtc), (kFirstCeInstantEpochSeconds + 30) * 1'000'000);
    QVERIFY(result->advancedWhileCatchingUp);
    QVERIFY(result->catchingUpToCurrentUtc);
}

void SkyTimelineControllerTests::returnsNoAdvancementWhenSpeedIsZero()
{
    auto controller = timelineController(0.0, kCatchUpStepSeconds);

    const auto result =
        controller.computeCatchUpAdvancement(utcAtSeconds(1000), utcAtSeconds(1001), utcAtSeconds(2000));

    QVERIFY(!result.has_value());
}

void SkyTimelineControllerTests::returnsNoAdvancementWhenStepIsZero()
{
    auto controller = timelineController(kUnitSpeedMultiplier, 0);

    const auto result =
        controller.computeCatchUpAdvancement(utcAtSeconds(1000), utcAtSeconds(1001), utcAtSeconds(2000));

    QVERIFY(!result.has_value());
}

QTEST_APPLESS_MAIN(SkyTimelineControllerTests)

#include "SkyTimelineControllerTests.moc"
