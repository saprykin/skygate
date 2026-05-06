#include "SkyContextControllerTestSupport.hpp"

class SkyContextControllerTimelineTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void setUtcDateTimeTextAppliesAtomically();
    void setUtcDateTimeTextAcceptsBceInput();
    void bceAliasesResolveToTheSameInstant();
    void invalidUtcDateTimeInputLeavesTimelineUnchanged();
    void invalidYearZeroUtcDateTimeIsRejected();
    void manualUtcApplyPausesLivePlayback();
    void manualTimelineSteppingStillWorks();
    void manualTimelineSteppingCrossesBceBoundaryWithoutYearZero();
    void livePlaybackUsesManualStepWhileCatchingUp();
    void livePlaybackDoesNotOvershootCurrentUtcWhenCatchingUp();
    void livePlaybackFallsBackToOneSecondTicksAfterCatchUp();
    void goLiveNowJumpsToCurrentUtcAndEnablesLive();
    void restoresLiveSettingsAtCurrentUtc();
    void restoresPausedSettingsAtSavedUtc();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyContextControllerTimelineTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyContextControllerTimelineTests")));
}

void SkyContextControllerTimelineTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyContextControllerTimelineTests::setUtcDateTimeTextAppliesAtomically()
{
    const auto controller = createController();
    controller->setLive(false);

    QVERIFY(controller->setUtcDateTimeText("2026-01-02", "03:04:05"));
    QCOMPARE(controller->utcDateText(), QString("2026-01-02"));
    QCOMPARE(controller->utcTimeText(), QString("03:04:05"));
    QCOMPARE(
        controllerUtcTime(*controller).toSecsSinceEpoch(),
        QDateTime::fromString("2026-01-02T03:04:05Z", Qt::ISODate).toSecsSinceEpoch()
    );
}

void SkyContextControllerTimelineTests::setUtcDateTimeTextAcceptsBceInput()
{
    const auto controller = createController();
    controller->setLive(false);

    QVERIFY(controller->setUtcDateTimeText("0044-03-15 BCE", "12:00:00"));
    QCOMPARE(controller->utcDateText(), QString("0044-03-15 BCE"));
    QCOMPARE(controller->utcTimeText(), QString("12:00:00"));
    QCOMPARE(controllerUtcTime(*controller).date(), QDate(-44, 3, 15));
}

void SkyContextControllerTimelineTests::bceAliasesResolveToTheSameInstant()
{
    const auto bcController = createController();
    bcController->setLive(false);
    QVERIFY(bcController->setUtcDateTimeText("0044-03-15 BC", "12:00:00"));

    const auto bceController = createController();
    bceController->setLive(false);
    QVERIFY(bceController->setUtcDateTimeText("0044-03-15 BCE", "12:00:00"));

    QCOMPARE(
        controllerUtcTime(*bcController).toSecsSinceEpoch(),
        controllerUtcTime(*bceController).toSecsSinceEpoch()
    );
}

void SkyContextControllerTimelineTests::invalidUtcDateTimeInputLeavesTimelineUnchanged()
{
    const auto controller = createController();
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2026-01-02", "03:04:05"));

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(!controller->setUtcDateTimeText("2026-02-30", "09:08:07"));

    QCOMPARE(controller->utcDateText(), QString("2026-01-02"));
    QCOMPARE(controller->utcTimeText(), QString("03:04:05"));
    QCOMPARE(controllerUtcTime(*controller).toSecsSinceEpoch(), beforeSeconds);
}

void SkyContextControllerTimelineTests::invalidYearZeroUtcDateTimeIsRejected()
{
    const auto controller = createController();
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2026-01-02", "03:04:05"));

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(
        controller->validateUtcDateTimeText("0000-01-01", "00:00:00"),
        QString("Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE.")
    );
    QVERIFY(!controller->setUtcDateTimeText("0000-01-01", "00:00:00"));
    QCOMPARE(controllerUtcTime(*controller).toSecsSinceEpoch(), beforeSeconds);
}

void SkyContextControllerTimelineTests::manualUtcApplyPausesLivePlayback()
{
    const auto controller = createController();
    controller->setLive(true);

    QVERIFY(controller->setUtcDateTimeText("2026-01-09", "09:08:07"));
    QVERIFY(!controller->live());
    QCOMPARE(controller->utcDateText(), QString("2026-01-09"));
    QCOMPARE(controller->utcTimeText(), QString("09:08:07"));
}

void SkyContextControllerTimelineTests::manualTimelineSteppingStillWorks()
{
    const auto controller = createController();
    controller->setLive(false);
    controller->setStepSeconds(60);

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->stepForward();
    const qint64 afterStepForwardSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->stepBackward();
    const qint64 afterStepBackwardSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();

    QCOMPARE(afterStepForwardSeconds - beforeSeconds, 60);
    QCOMPARE(afterStepBackwardSeconds, beforeSeconds);
}

void SkyContextControllerTimelineTests::manualTimelineSteppingCrossesBceBoundaryWithoutYearZero()
{
    const auto controller = createController();
    controller->setLive(false);
    controller->setStepSeconds(60);
    QVERIFY(controller->setUtcDateTimeText("0001-12-31 BCE", "23:59:30"));

    controller->stepForward();
    QCOMPARE(controller->utcDateText(), QString("0001-01-01"));
    QCOMPARE(controller->utcTimeText(), QString("00:00:30"));

    controller->stepBackward();
    QCOMPARE(controller->utcDateText(), QString("0001-12-31 BCE"));
    QCOMPARE(controller->utcTimeText(), QString("23:59:30"));
}

void SkyContextControllerTimelineTests::livePlaybackUsesManualStepWhileCatchingUp()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithTimeSource(timeSource);
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(2.0);

    const QDateTime startUtc = fixedNowUtc().addSecs(-5 * 60);
    QVERIFY(controller->setUtcDateTimeText(
        startUtc.toString("yyyy-MM-dd"),
        startUtc.toString("HH:mm:ss")
    ));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    skyContextChangedSpy.clear();

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->setLive(true);

    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 afterSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(afterSeconds - beforeSeconds >= 120);

    controller->setLive(false);
}

void SkyContextControllerTimelineTests::livePlaybackDoesNotOvershootCurrentUtcWhenCatchingUp()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithTimeSource(timeSource);
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(2.0);

    const QDateTime startUtc = fixedNowUtc().addSecs(-30);
    QVERIFY(controller->setUtcDateTimeText(
        startUtc.toString("yyyy-MM-dd"),
        startUtc.toString("HH:mm:ss")
    ));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    skyContextChangedSpy.clear();

    controller->setLive(true);
    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 currentTimelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    const qint64 currentUtcSeconds = fixedNowUtc().toSecsSinceEpoch();
    QVERIFY(currentTimelineSeconds <= currentUtcSeconds);

    controller->setLive(false);
}

void SkyContextControllerTimelineTests::livePlaybackFallsBackToOneSecondTicksAfterCatchUp()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithTimeSource(timeSource);
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(4.0);

    const QDateTime startUtc = fixedNowUtc().addSecs(-2);
    QVERIFY(controller->setUtcDateTimeText(
        startUtc.toString("yyyy-MM-dd"),
        startUtc.toString("HH:mm:ss")
    ));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    skyContextChangedSpy.clear();

    controller->setLive(true);
    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 afterCatchUpSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    skyContextChangedSpy.clear();

    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 afterLiveTickSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(afterLiveTickSeconds - afterCatchUpSeconds, 1);

    controller->setLive(false);
}

void SkyContextControllerTimelineTests::goLiveNowJumpsToCurrentUtcAndEnablesLive()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithTimeSource(timeSource);
    controller->setLive(false);
    controller->setStepSeconds(60);

    for (int index = 0; index < 5; ++index) {
        controller->stepBackward();
    }

    controller->goLiveNow();

    QVERIFY(controller->live());

    const qint64 timelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(timelineSeconds, fixedNowUtc().toSecsSinceEpoch());
}

void SkyContextControllerTimelineTests::restoresLiveSettingsAtCurrentUtc()
{
    FakeTimeSource timeSource;
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.live = true;
    snapshot.utcEpochSeconds =
        QDateTime::fromString("2000-01-01T00:00:00Z", Qt::ISODate).toSecsSinceEpoch();
    QVERIFY(store.saveState(snapshot));

    const auto controller = createControllerWithTimeSource(timeSource, true);

    QVERIFY(controller->live());
    const qint64 timelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(timelineSeconds, fixedNowUtc().toSecsSinceEpoch());
    QVERIFY(timelineSeconds != snapshot.utcEpochSeconds);
}

void SkyContextControllerTimelineTests::restoresPausedSettingsAtSavedUtc()
{
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.live = false;
    snapshot.utcEpochSeconds =
        QDateTime::fromString("2000-01-01T00:00:00Z", Qt::ISODate).toSecsSinceEpoch();
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);

    QVERIFY(!controller->live());
    QCOMPARE(controllerUtcTime(*controller).toSecsSinceEpoch(), snapshot.utcEpochSeconds);
}

QTEST_GUILESS_MAIN(SkyContextControllerTimelineTests)

#include "SkyContextControllerTimelineTests.moc"
