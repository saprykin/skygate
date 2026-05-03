#include <QtTest>

#include "LocationCatalogModel.hpp"
#include "MacDockIcon.hpp"
#include "SkyContextController.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkySceneModel.hpp"
#include "SkyTheme.hpp"

#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include <QElapsedTimer>

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class PerformanceGuardTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsLargeSceneWithinGuardrail();
    void searchesLargeMixedCatalogWithinGuardrail();
    void packagedResourceDependenciesResolve();
    void platformNativeHooksTolerateEmptyInputs();
};

namespace {

constexpr qint64 kLargeSceneBuildBudgetMs = 15000;
constexpr qint64 kLargeSearchLoadBudgetMs = 10000;
constexpr qint64 kLargeSearchQueryBudgetMs = 3000;

void verifyElapsedBelow(
    const qint64 elapsedMs,
    const qint64 budgetMs,
    const char* operationName
)
{
    QVERIFY2(
        elapsedMs < budgetMs,
        qPrintable(QStringLiteral("%1 took %2 ms, budget is %3 ms")
            .arg(QString::fromUtf8(operationName))
            .arg(elapsedMs)
            .arg(budgetMs))
    );
}

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const double visualMagnitude,
    const double rightAscensionHours,
    const double declinationDeg
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate {
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = declinationDeg
    };
    if (type == skygate::ephemeris::CelestialBodyType::DeepSkyObject) {
        body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
            .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
            .aliases = {
                "Guard Alias " + body.id,
                "Shared Collision Alias " + std::to_string(
                    static_cast<int>(std::fmod(rightAscensionHours * 1000.0, 25.0))
                )
            },
            .majorAxisArcmin = 5.0,
            .minorAxisArcmin = 3.0,
            .positionAngleDeg = 0.0
        };
    }
    return body;
}

std::vector<skygate::ephemeris::CelestialBody> makeLargeMixedCatalog()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies;
    bodies.reserve(9000U);
    for (int index = 0; index < 7000; ++index) {
        const double rightAscensionHours = std::fmod(index * 0.017, 24.0);
        const double declinationDeg = -72.0 + static_cast<double>(index % 145);
        const double magnitude = 2.0 + static_cast<double>(index % 80) / 10.0;
        bodies.push_back(makeBody(
            "guard_star_" + std::to_string(index),
            "Guard Star " + std::to_string(index),
            skygate::ephemeris::CelestialBodyType::Star,
            magnitude,
            rightAscensionHours,
            declinationDeg
        ));
    }

    for (int index = 0; index < 2000; ++index) {
        const double rightAscensionHours = std::fmod(index * 0.071, 24.0);
        const double declinationDeg = -55.0 + static_cast<double>(index % 110);
        const double magnitude = 4.0 + static_cast<double>(index % 90) / 10.0;
        bodies.push_back(makeBody(
            "guard_dso_" + std::to_string(index),
            "Guard Galaxy " + std::to_string(index),
            skygate::ephemeris::CelestialBodyType::DeepSkyObject,
            magnitude,
            rightAscensionHours,
            declinationDeg
        ));
    }
    return bodies;
}

SkyContextController::InitializationOptions testInitializationOptions()
{
    return SkyContextController::InitializationOptions {
        .loadSettings = false,
        .initializeLocation = false
    };
}

}  // namespace

void PerformanceGuardTests::buildsLargeSceneWithinGuardrail()
{
    auto catalog = skygate::ephemeris::createStarCatalogFromBodies(makeLargeMixedCatalog());
    QVERIFY(catalog != nullptr);
    auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    SkyContextController controller(
        std::move(catalog),
        std::move(engine),
        testInitializationOptions(),
        nullptr
    );
    controller.setLatitudeText(QStringLiteral("47.4"));
    controller.setLongitudeText(QStringLiteral("8.5"));
    QVERIFY(controller.setUtcDateTimeText(
        QStringLiteral("2026-05-03"),
        QStringLiteral("21:00:00")
    ));
    controller.setMagnitudeCutoff(12.0);
    controller.setViewCenter(45.0, 180.0);

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);

    QElapsedTimer timer;
    timer.start();
    sceneModel.setViewportSize(1280.0, 800.0);
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(sceneModel.snapshotGeneration() > 0U);
    QVERIFY(
        !sceneModel.renderPointSpan().empty()
        || !sceneModel.renderGlyphSpan().empty()
        || !sceneModel.renderLineSpan().empty()
    );
    verifyElapsedBelow(elapsedMs, kLargeSceneBuildBudgetMs, "large scene build");
}

void PerformanceGuardTests::searchesLargeMixedCatalogWithinGuardrail()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies = makeLargeMixedCatalog();
    bodies.push_back(makeBody(
        "guard_exact_target",
        "Guard Exact Target",
        skygate::ephemeris::CelestialBodyType::Star,
        -1.0,
        6.0,
        -16.0
    ));

    SkyObjectSearchModel model;
    const std::vector<skygate::ephemeris::ConstellationLabelRef> labelRefs {
        {"Guard Constellation", {"guard_star_1", "guard_star_2"}}
    };
    QElapsedTimer timer;
    timer.start();
    model.setCatalogData(bodies, labelRefs);
    const qint64 loadElapsedMs = timer.elapsed();
    verifyElapsedBelow(loadElapsedMs, kLargeSearchLoadBudgetMs, "large search catalog load");

    timer.restart();
    model.setFilterText(QStringLiteral("guard exact target"));
    const qint64 exactQueryElapsedMs = timer.elapsed();
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(
        model.index(0, 0).data(SkyObjectSearchModel::TargetIdRole).toString(),
        QStringLiteral("guard_exact_target")
    );
    verifyElapsedBelow(exactQueryElapsedMs, kLargeSearchQueryBudgetMs, "large exact search");

    timer.restart();
    model.setFilterText(QStringLiteral("shared collision alias 7"));
    const qint64 aliasQueryElapsedMs = timer.elapsed();
    QVERIFY(model.rowCount() > 0);
    verifyElapsedBelow(aliasQueryElapsedMs, kLargeSearchQueryBudgetMs, "large alias search");
}

void PerformanceGuardTests::packagedResourceDependenciesResolve()
{
    const skygate::ui::internal::SkyThemeRepository themeRepository;
    QVERIFY(themeRepository.themeOptions().size() >= 2);
    QVERIFY(themeRepository.defaultTheme().ui.windowBackground.isValid());

    LocationCatalogModel locations;
    QVERIFY(locations.rowCount() > 0);
    QVERIFY(!locations.index(0, 0).data(LocationCatalogModel::DisplayTextRole).toString().isEmpty());
}

void PerformanceGuardTests::platformNativeHooksTolerateEmptyInputs()
{
#ifdef Q_OS_MACOS
    skygate::ui::setMacDockIcon(QString());
#else
    QSKIP("Mac dock icon hook is only built on macOS.");
#endif
}

QTEST_GUILESS_MAIN(PerformanceGuardTests)

#include "PerformanceGuardTests.moc"
