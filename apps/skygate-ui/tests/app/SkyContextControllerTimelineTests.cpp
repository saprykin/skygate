#include "SkyContextControllerTestSupport.hpp"

namespace {

class ThrottledLiveTestEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    ThrottledLiveTestEngine()
    {
        m_options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return m_options.engineKind();
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Throttled live test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisCapabilities capabilities() const noexcept override
    {
        skygate::ephemeris::EphemerisCapabilities capabilities =
            skygate::ephemeris::EphemerisCapabilities::noCapabilities();

        return capabilities;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        return m_options;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        return snapshotFor(request.context);
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        return snapshotFor(context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, std::string_view bodyId) const override
    {
        Q_UNUSED(context);
        Q_UNUSED(bodyId);
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, std::uint32_t bodyIndex) const override
    {
        Q_UNUSED(context);
        Q_UNUSED(bodyIndex);
        return std::nullopt;
    }

private:
    [[nodiscard]] static skygate::ephemeris::SkySnapshot snapshotFor(const skygate::core::SkyContext& context)
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        return snapshot;
    }

private:
    skygate::ephemeris::EphemerisEngineOptions m_options;
};

std::unique_ptr<SkyContextController>
createControllerWithThrottledLiveEngine(const skygate::core::ITimeSource& timeSource)
{
    auto initializationOptions = controllerInitializationOptions(false, &timeSource);
    initializationOptions.rebuildEphemerisEngineOnStartup = false;

    return std::make_unique<SkyContextController>(
        createTestCatalog(), std::make_unique<ThrottledLiveTestEngine>(), initializationOptions, nullptr
    );
}

}  // namespace

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
    void throttledLivePlaybackAdvancesByElapsedWallTime();
    void throttledLivePlaybackKeepsDisplayedTimeTicking();
    void throttledCatchUpPlaybackRecomputesForLargeTimelineSteps();
    void fallbackSimpleEngineLivePlaybackUsesOneSecondTicks();
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

    QCOMPARE(controllerUtcTime(*bcController).toSecsSinceEpoch(), controllerUtcTime(*bceController).toSecsSinceEpoch());
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
    QVERIFY(controller->setUtcDateTimeText(startUtc.toString("yyyy-MM-dd"), startUtc.toString("HH:mm:ss")));

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->setLive(true);

    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toSecsSinceEpoch() > beforeSeconds + 60, 15000);

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
    QVERIFY(controller->setUtcDateTimeText(startUtc.toString("yyyy-MM-dd"), startUtc.toString("HH:mm:ss")));

    controller->setLive(true);
    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toSecsSinceEpoch() > startUtc.toSecsSinceEpoch(), 8000);

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
    QVERIFY(controller->setUtcDateTimeText(startUtc.toString("yyyy-MM-dd"), startUtc.toString("HH:mm:ss")));

    controller->setLive(true);
    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toMSecsSinceEpoch() > startUtc.toMSecsSinceEpoch(), 8000);

    const qint64 afterCatchUpMillis = controllerUtcTime(*controller).toMSecsSinceEpoch();

    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toMSecsSinceEpoch() > afterCatchUpMillis, 8000);

    const qint64 afterLiveTickMillis = controllerUtcTime(*controller).toMSecsSinceEpoch();
    QVERIFY(afterLiveTickMillis > afterCatchUpMillis);

    controller->setLive(false);
}

void SkyContextControllerTimelineTests::throttledLivePlaybackAdvancesByElapsedWallTime()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithThrottledLiveEngine(timeSource);
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2026-05-06", "09:30:00"));

    const qint64 startSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->setLive(true);

    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toSecsSinceEpoch() > startSeconds, 8000);
    const qint64 afterFirstTickSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(afterFirstTickSeconds >= startSeconds + 1);

    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toSecsSinceEpoch() > afterFirstTickSeconds, 25000);

    const qint64 afterSecondTickSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(afterSecondTickSeconds - afterFirstTickSeconds >= 9);

    controller->setLive(false);
}

void SkyContextControllerTimelineTests::throttledLivePlaybackKeepsDisplayedTimeTicking()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithThrottledLiveEngine(timeSource);
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2026-05-06", "09:30:00"));

    QSignalSpy displayedTimeSpy(controller->timeController(), &SkyTimeController::timeTextChanged);
    displayedTimeSpy.clear();

    const qint64 startContextSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->setLive(true);

    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toSecsSinceEpoch() > startContextSeconds, 8000);
    const qint64 firstContextSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(firstContextSeconds >= startContextSeconds + 1);

    displayedTimeSpy.clear();

    QTRY_VERIFY_WITH_TIMEOUT(displayedTimeSpy.count() >= 1, 8000);
    QCOMPARE(controllerUtcTime(*controller).toSecsSinceEpoch(), firstContextSeconds);
    QVERIFY(controller->timeController()->utcDateTime().toSecsSinceEpoch() > firstContextSeconds);

    controller->setLive(false);
}

void SkyContextControllerTimelineTests::throttledCatchUpPlaybackRecomputesForLargeTimelineSteps()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithThrottledLiveEngine(timeSource);
    controller->setLive(false);
    controller->setStepSeconds(5 * 60);

    const QDateTime startUtc = fixedNowUtc().addSecs(-60 * 60);
    QVERIFY(controller->setUtcDateTimeText(startUtc.toString("yyyy-MM-dd"), startUtc.toString("HH:mm:ss")));

    const qint64 startCatchUpSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->setLive(true);

    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toSecsSinceEpoch() > startCatchUpSeconds, 8000);

    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toSecsSinceEpoch() > startCatchUpSeconds + 60, 25000);

    controller->setLive(false);
}

void SkyContextControllerTimelineTests::fallbackSimpleEngineLivePlaybackUsesOneSecondTicks()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithTimeSource(timeSource);
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2026-05-06", "09:30:00"));

    controller->setEphemerisEngineKindIndex(1);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 1);
    QVERIFY(controller->ephemerisEngine() != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(controller->ephemerisEngine()->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );

    const qint64 startSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->setLive(true);

    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toSecsSinceEpoch() > startSeconds, 8000);
    const qint64 afterFirstTickSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(afterFirstTickSeconds >= startSeconds + 1);

    QTRY_VERIFY_WITH_TIMEOUT(controllerUtcTime(*controller).toSecsSinceEpoch() > afterFirstTickSeconds, 8000);

    const qint64 afterSecondTickSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(afterSecondTickSeconds > afterFirstTickSeconds);

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
    snapshot.utcEpochMicros = QDateTime::fromString("2000-01-01T00:00:00Z", Qt::ISODate).toMSecsSinceEpoch() * 1000;
    QVERIFY(store.saveState(snapshot));

    const auto controller = createControllerWithTimeSource(timeSource, true);

    QVERIFY(controller->live());
    const qint64 timelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(timelineSeconds, fixedNowUtc().toSecsSinceEpoch());
    QVERIFY(timelineSeconds != snapshot.utcEpochMicros / 1'000'000);
}

void SkyContextControllerTimelineTests::restoresPausedSettingsAtSavedUtc()
{
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.live = false;
    snapshot.utcEpochMicros = QDateTime::fromString("2000-01-01T00:00:00Z", Qt::ISODate).toMSecsSinceEpoch() * 1000;
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);

    QVERIFY(!controller->live());
    QCOMPARE(controllerUtcTime(*controller).toMSecsSinceEpoch() * 1000, snapshot.utcEpochMicros);
}

QTEST_GUILESS_MAIN(SkyContextControllerTimelineTests)

#include "SkyContextControllerTimelineTests.moc"
