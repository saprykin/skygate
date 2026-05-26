#include "SkyContextControllerTestSupport.hpp"
#include "UtcTimeCodec.hpp"

#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <string_view>
#include <vector>

namespace {

class RequestSensitiveNightEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit RequestSensitiveNightEngine(std::vector<skygate::ephemeris::CelestialBody> bodies)
        : m_bodies(std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(std::move(bodies)))
    {
        m_options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
        m_options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Request-sensitive night test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        return m_options;
    }

    void setOptions(const skygate::ephemeris::EphemerisEngineOptions& options) noexcept
    {
        m_options = options;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = request.context;
        snapshot.catalogBodies = m_bodies;
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            snapshot.states.push_back(*computeBodyState(request, bodyIndex));
        }
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view bodyId) const override
    {
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            if ((*m_bodies)[bodyIndex].id == bodyId) {
                return computeBodyState(request, bodyIndex);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        ++m_requestBodyStateCount;
        if (bodyIndex >= m_bodies->size()) {
            return std::nullopt;
        }

        m_lastRequestOptions = request.options;
        if (isSelectedNightRequest(request)) {
            ++m_selectedRequestCount;
        }

        return stateFor(bodyIndex, requestAltitude(bodyIndex, request), 150.0);
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = m_bodies;
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            snapshot.states.push_back(*computeBodyState(context, static_cast<std::uint32_t>(bodyIndex)));
        }
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::string_view bodyId) const override
    {
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            if ((*m_bodies)[bodyIndex].id == bodyId) {
                return computeBodyState(context, static_cast<std::uint32_t>(bodyIndex));
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, const std::uint32_t bodyIndex) const override
    {
        ++m_contextBodyStateCount;
        if (bodyIndex >= m_bodies->size()) {
            return std::nullopt;
        }

        return stateFor(bodyIndex, 42.0, 20.0);
    }

    [[nodiscard]] int requestBodyStateCount() const noexcept
    {
        return m_requestBodyStateCount;
    }

    [[nodiscard]] int contextBodyStateCount() const noexcept
    {
        return m_contextBodyStateCount;
    }

    [[nodiscard]] int selectedRequestCount() const noexcept
    {
        return m_selectedRequestCount;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisEngineOptions> lastRequestOptions() const
    {
        return m_lastRequestOptions;
    }

private:
    [[nodiscard]] skygate::ephemeris::CelestialBodyState
    stateFor(const std::size_t bodyIndex, const double altitudeDeg, const double azimuthDeg) const
    {
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .equatorial = {.rightAscensionHours = static_cast<double>(bodyIndex), .declinationDeg = altitudeDeg / 2.0},
            .horizontal = {.altitudeDeg = altitudeDeg, .azimuthDeg = azimuthDeg}
        };
    }

    [[nodiscard]] bool isSelectedNightRequest(const skygate::ephemeris::EphemerisRequest& request) const noexcept
    {
        return request.options.engineKind == skygate::ephemeris::EphemerisEngineKind::HighPrecision
               && skygate::ephemeris::hasCorrectionFlag(
                   request.options.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::LightTime
               );
    }

    [[nodiscard]] double
    requestAltitude(const std::size_t bodyIndex, const skygate::ephemeris::EphemerisRequest& request) const noexcept
    {
        if (!isSelectedNightRequest(request)) {
            return 42.0;
        }

        constexpr double kSecondsPerDay = 86'400.0;
        const double seconds = skygate::core::UtcTimeCodec::secondsSinceEpochDouble(request.context.utcTime);
        double dayFraction = std::fmod(seconds, kSecondsPerDay) / kSecondsPerDay;
        if (dayFraction < 0.0) {
            dayFraction += 1.0;
        }

        const double phase = bodyIndex == 0U ? -0.25 : 0.0;
        return 55.0 * std::sin(std::numbers::pi_v<double> * 2.0 * (dayFraction + phase));
    }

    std::shared_ptr<const std::vector<skygate::ephemeris::CelestialBody>> m_bodies;
    skygate::ephemeris::EphemerisEngineOptions m_options;
    mutable int m_requestBodyStateCount = 0;
    mutable int m_contextBodyStateCount = 0;
    mutable int m_selectedRequestCount = 0;
    mutable std::optional<skygate::ephemeris::EphemerisEngineOptions> m_lastRequestOptions;
};

std::unique_ptr<SkyContextController> createRequestSensitiveNightController(RequestSensitiveNightEngine*& engine)
{
    std::vector<skygate::ephemeris::CelestialBody> bodies{
        makeBody("sun", "Sun", skygate::ephemeris::CelestialBodyType::Sun, -26.7),
        makeBody("moon", "Moon", skygate::ephemeris::CelestialBodyType::Moon, -12.0),
    };
    auto starCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(bodies);
    Q_ASSERT(starCatalog != nullptr);

    auto ephemerisEngine = std::make_unique<RequestSensitiveNightEngine>(std::move(bodies));
    engine = ephemerisEngine.get();

    auto initializationOptions = controllerInitializationOptions(false);
    initializationOptions.rebuildEphemerisEngineOnStartup = false;
    auto controller = std::make_unique<SkyContextController>(
        std::move(starCatalog), std::move(ephemerisEngine), initializationOptions, nullptr
    );
    skygate::ephemeris::EphemerisEngineOptions contextAdapterOptions;
    contextAdapterOptions.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    contextAdapterOptions.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections;
    engine->setOptions(contextAdapterOptions);
    configureFocusTestContext(*controller);
    return controller;
}

}  // namespace

class SkyContextControllerNightCatalogTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void nightConditionsPopulateAndRefreshForValidObserver();
    void nightConditionsUseSelectedEngineRequest();
    void failedDeepSkyCatalogDownloadKeepsCountLabel();
    void restoresCachedCatalogConstellationCount();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyContextControllerNightCatalogTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyContextControllerNightCatalogTests")));
}

void SkyContextControllerNightCatalogTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyContextControllerNightCatalogTests::nightConditionsPopulateAndRefreshForValidObserver()
{
    const auto controller = createController();
    configureFocusTestContext(*controller);
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("Asia/Bishkek")));
    QVERIFY(controller->setUtcDateTimeText("2024-03-21", "12:00:00"));

    controller->refreshNightConditions();
    const QVariantMap initialConditions = controller->nightConditions();
    QVERIFY(initialConditions.value("valid").toBool());
    QCOMPARE(initialConditions.value("sunRows").toList().size(), 6);
    QVERIFY(initialConditions.value("locationText").toString().contains("UTC+06:00"));
    QVERIFY(initialConditions.value("moonPhaseText").toString().contains("%"));
    QVERIFY(initialConditions.value("moonRiseText").toString() != "--");
    QVERIFY(initialConditions.value("moonSetText").toString() != "--");
    QVERIFY(QStringList({"sun", "twilight", "moon"}).contains(controller->nightConditionsIconKind()));

    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("UTC")));
    const QVariantMap utcConditions = controller->nightConditions();
    QVERIFY(utcConditions.value("valid").toBool());
    QVERIFY(utcConditions.value("locationText").toString().contains("UTC"));
    QVERIFY(!utcConditions.value("locationText").toString().contains("UTC+06:00"));
    QVERIFY(utcConditions != initialConditions);

    QVERIFY(controller->setUtcDateTimeText("2024-03-22", "12:00:00"));
    controller->refreshNightConditions();
    const QVariantMap refreshedConditions = controller->nightConditions();
    QVERIFY(refreshedConditions.value("valid").toBool());
    QVERIFY(refreshedConditions != initialConditions);
}

void SkyContextControllerNightCatalogTests::nightConditionsUseSelectedEngineRequest()
{
    RequestSensitiveNightEngine* engine = nullptr;
    const auto controller = createRequestSensitiveNightController(engine);
    QVERIFY(engine != nullptr);
    QVERIFY(controller->setUtcDateTimeText("2024-03-21", "22:00:00"));

    QCOMPARE(controller->nightConditionsIconKind(), QString("moon"));
    controller->refreshNightConditions();

    const QVariantMap conditions = controller->nightConditions();
    QVERIFY(conditions.value("valid").toBool());
    const QVariantList sunRows = conditions.value("sunRows").toList();
    QCOMPARE(sunRows.size(), 6);
    bool hasTimedSunEvent = false;
    for (const QVariant& rowValue : sunRows) {
        hasTimedSunEvent = hasTimedSunEvent || rowValue.toMap().value("value").toString().contains(":");
    }
    QVERIFY(hasTimedSunEvent);
    QVERIFY(engine->requestBodyStateCount() > 0);
    QCOMPARE(engine->contextBodyStateCount(), 0);
    QVERIFY(engine->selectedRequestCount() > 0);
    const auto lastRequestOptions = engine->lastRequestOptions();
    QVERIFY(lastRequestOptions.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(lastRequestOptions->engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QVERIFY(
        skygate::ephemeris::hasCorrectionFlag(
            lastRequestOptions->correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::LightTime
        )
    );
}

void SkyContextControllerNightCatalogTests::failedDeepSkyCatalogDownloadKeepsCountLabel()
{
    const auto controller = createController();
    QSignalSpy infoSpy(controller.get(), &SkyContextController::deepSkyCatalogInfoTextChanged);

    QCOMPARE(controller->deepSkyCatalogInfoText(), QString("Objects: 110"));
    QTest::ignoreMessage(QtWarningMsg, "Catalog download aborted: no valid source URLs");
    controller->downloadDeepSkyCatalogFromUrl(QString());

    QCOMPARE(infoSpy.count(), 0);
    QCOMPARE(controller->deepSkyCatalogInfoText(), QString("Objects: 110"));
    QCOMPARE(controller->catalogStatusText(), QString("Catalog: Invalid URL"));
}

void SkyContextControllerNightCatalogTests::restoresCachedCatalogConstellationCount()
{
    QSettings settings;
    settings.setValue("skyContext/catalogCachePath", m_settings.filePath(QStringLiteral("cached-hyg-catalog.csv")));

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot stateSnapshot;
    stateSnapshot.catalogPresetIndex = 1;
    QVERIFY(store.saveState(stateSnapshot));

    SkySettingsStore::CatalogCacheSnapshot cacheSnapshot;
    cacheSnapshot.sourceLabel = "HYG v4.2";
    cacheSnapshot.catalogPayload = "id,hip,proper,ra,dec,mag\n"
                                   "1,42,Demo Star,6.7525,-16.7161,-1.46\n";
    cacheSnapshot.constellationLineRows = "hyg_1|hyg_1\n";
    cacheSnapshot.constellationAnchorGroupRows = "Demo|hyg_1\n";
    cacheSnapshot.constellationLineSchemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
    cacheSnapshot.constellationCount = 88;
    QVERIFY(store.saveCatalogCache(cacheSnapshot));

    const auto controller = createController(true);

    QCOMPARE(controller->catalogPresetIndex(), 1);
    QVERIFY(controller->catalogDatasetInfoText().contains("Constellations: 88"));
}

QTEST_GUILESS_MAIN(SkyContextControllerNightCatalogTests)

#include "SkyContextControllerNightCatalogTests.moc"
