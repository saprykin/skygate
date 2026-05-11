#include "catalog/SkyCatalogRuntime.hpp"

#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QtTest/QtTest>

#include <string>
#include <utility>

namespace {

skygate::ephemeris::CelestialBody makeFixedBody(
    std::string id,
    std::string displayName,
    const double magnitude = 1.0
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = skygate::ephemeris::CelestialBodyType::Star;
    body.ephemerisSource = skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial;
    body.visualMagnitude = magnitude;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate {
        .rightAscensionHours = 1.0,
        .declinationDeg = 2.0
    };
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> makeCatalog()
{
    return skygate::ephemeris::createStarCatalogFromBodies({
        makeFixedBody("hip_1", "HIP 1"),
        makeFixedBody("hip_2", "HIP 2", 2.0)
    });
}

}  // namespace

class SkyCatalogRuntimeTests final : public QObject {
    Q_OBJECT

private slots:
    void initializeBuildsActiveCatalogAndCacheRequest();
    void restoreConstellationRefsUpdatesRevisionAndCount();
    void nullCatalogReportsFailureWithoutCatalogChange();
};

void SkyCatalogRuntimeTests::initializeBuildsActiveCatalogAndCacheRequest()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog(), nullptr);

    const auto result = runtime.initialize({
        .useBundledDeepSkyCatalog = false
    });

    QVERIFY(result.catalogChanged);
    QVERIFY(result.datasetInfoChanged);
    QVERIFY(runtime.starCatalog() != nullptr);
    QVERIFY(runtime.ephemerisEngine() != nullptr);
    QCOMPARE(runtime.sourceLabel(), QString("Bundled"));
    QVERIFY(runtime.bodyCount() >= 2U);
    QCOMPARE(runtime.sourceIds().size(), runtime.bodyCount());

    const auto request = runtime.cachePersistRequest("stars", {});
    QVERIFY(request.has_value());
    QCOMPARE(request->sourceLabel, QString("Bundled"));
    QCOMPARE(request->catalogPayload, QByteArray("stars"));
}

void SkyCatalogRuntimeTests::restoreConstellationRefsUpdatesRevisionAndCount()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog(), nullptr);
    static_cast<void>(runtime.initialize({
        .useBundledDeepSkyCatalog = false
    }));
    const auto originalRevision = runtime.catalogRevision();

    const auto result = runtime.restoreConstellationRefs(
        {{"orion", "hip_1"}},
        {{"orion", {"hip_1", "hip_2"}}},
        1U
    );

    QVERIFY(result.catalogChanged);
    QVERIFY(result.datasetInfoChanged);
    QVERIFY(runtime.catalogRevision() > originalRevision);
    QCOMPARE(runtime.constellationCount(), 1U);
    QCOMPARE(runtime.constellationLineRefs().size(), 1U);
    QCOMPARE(runtime.constellationLabelRefs().size(), 1U);
}

void SkyCatalogRuntimeTests::nullCatalogReportsFailureWithoutCatalogChange()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog(), nullptr);

    const auto result = runtime.applyCatalog(
        nullptr,
        QStringLiteral("Broken"),
        {.useBundledDeepSkyCatalog = false}
    );

    QVERIFY(result.statusTextChanged);
    QVERIFY(result.datasetInfoChanged);
    QVERIFY(!result.catalogChanged);
    QCOMPARE(result.statusText, QString("Catalog: Failed to load"));
    QCOMPARE(runtime.bodyCount(), 0U);
}

QTEST_APPLESS_MAIN(SkyCatalogRuntimeTests)

#include "SkyCatalogRuntimeTests.moc"
